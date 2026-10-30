#include "onthepitch/peskickmotion.hpp"

#include <cmath>
#include <cstring>

namespace pes {
namespace {

constexpr const char* kIdletablePath = "common/anime/AnimeTable/bin/Idletable.bin";  // 0x1425DB9F8
constexpr const char* kMovetablePath = "common/anime/AnimeTable/bin/Movetable.bin";  // 0x1425DBA30
constexpr size_t kClipRecordBytes = 20;  // five u32 words per kind (FUN_1406AA590 sub-types 1 and 2)

// Row ids the clip table and FUN_14071E6F0 name as immediates (opcode-less: they are the decompile's literals).
constexpr int kRowRunShot = 0x7A4, kRowRunShotOdd = 0x876;       // 0x14071E7xx: request 10, even tick
constexpr int kRowRunA = 0x7A6, kRowRunB = 0x7A7;                // unconditional
constexpr int kRowBall = 0x809, kRowBallFar = 0x874, kRowBallSlow = 0x875, kRowBallFast = 0x877;
constexpr int kRowPassA = 0x863, kRowPassB = 0x864, kRowShotA = 0x865, kRowShotB = 0x866;
constexpr int kRowAction = 0x86E, kRowActionB = 0x86F;
constexpr int kRequestPass = 9, kRequestShot = 10;
constexpr int kMotionFixA = 0xA5E, kMotionFixA2 = 0xA7B, kMotionFixB = 0xABE, kMotionFixB2 = 0xADC;  // FUN_14071E570's tail
// FUN_1406ABBB0's angle-keyed kinds, each a (side 0, side 1) row pair per angle sector.
constexpr int kKindAngleA = 0x77, kKindAngleB = 0x78;
constexpr int kRowsA[5][2] = {{0x892, 0x898}, {0x891, 0x897}, {0x88E, 0x894}, {0x88F, 0x895}, {0x890, 0x896}};
constexpr int kRowsAMid[2] = {0x88D, 0x893};
constexpr int kRowsB[2][2] = {{0x89A, 0x89C}, {0x899, 0x89B}};
constexpr int kDescriptorE42 = 0x22, kDescriptorF7C = 0x18;  // FUN_1406B0250's two motions with no record
constexpr int kMotionE42 = 0xE42, kMotionF7C = 0xF7C;
constexpr int kMotionSlots = 0x1125;

constexpr uint64_t kSwapOddVa = 0x14344EEB8, kSwapLateralVa = 0x14344EEE0, kSwapFirstSideVa = 0x14344EF50;
constexpr uint64_t kSwapEndVa = 0x14344EF78;

bool ReadF(const PeImage& exe, uint64_t va, float* out) { return exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out)); }

bool ReadPairs(const PeImage& exe, uint64_t from, uint64_t to, std::vector<std::pair<int, int>>* out) {
  for (uint64_t va = from; va < to; va += 8) {
    int32_t w[2];
    if (!exe.ReadVa(va, sizeof w, reinterpret_cast<uint8_t*>(w))) return false;
    out->push_back({w[0], w[1]});
  }
  return true;
}

uint32_t Word(const std::vector<uint8_t>& b, size_t record, int word) {
  uint32_t v;
  std::memcpy(&v, &b[record * kClipRecordBytes + 4 * size_t(word)], sizeof v);
  return v;
}
// One 12-bit field, minus one: the clip table's s16 (FUN_1406AA590).
int16_t Field12(uint32_t w, int shift) { return int16_t(int(w >> shift & 0xFFF) - 1); }
int16_t Field11(uint32_t w, int shift) { return int16_t(int(w >> shift & 0x7FF) - 1); }
// A one-bit field: 0 when set, -1 when clear.
int16_t Bit(uint32_t w, int bit) { return int16_t(int(w >> bit & 1) - 1); }

void Put(KickMotionConstants* k, int kind, int side, const int16_t (&band)[kClipBands]) {
  for (int b = 0; b < kClipBands; ++b) k->clip[size_t((kind * kClipBands + b) * kClipSides + side)] = band[b];
}

// The Movetable.bin record (the table's side 1; FUN_1406AA590's stores at +2, +6, ..): bands 0..11 from the record's five words.
void MovetableRecord(KickMotionConstants* k, const std::vector<uint8_t>& f, int kind) {
  const uint32_t w0 = Word(f, kind, 0), w1 = Word(f, kind, 1), w2 = Word(f, kind, 2), w3 = Word(f, kind, 3), w4 = Word(f, kind, 4);
  const int16_t band[kClipBands] = {Field12(w1, 0),  Field12(w0, 12), Field12(w2, 0),  Field12(w1, 12), Field12(w0, 0), Field11(w3, 0),
                                    Field12(w2, 12), Field11(w4, 0),  Bit(w1, 24),      Field11(w3, 11), Bit(w1, 26),    Bit(w1, 25)};
  Put(k, kind, 1, band);
}

// The Idletable.bin record (side 0; the stores at +0, +4, ..).
void IdletableRecord(KickMotionConstants* k, const std::vector<uint8_t>& f, int kind) {
  const uint32_t w0 = Word(f, kind, 0), w1 = Word(f, kind, 1), w2 = Word(f, kind, 2), w3 = Word(f, kind, 3), w4 = Word(f, kind, 4);
  const int16_t band[kClipBands] = {Field12(w2, 12), Field12(w2, 0), Field12(w3, 12), Field12(w3, 0), Field12(w0, 12), Field12(w0, 0),
                                    Field12(w4, 12), Field12(w4, 0), Field12(w1, 12), Field12(w1, 0), Bit(w1, 25),     Bit(w1, 24)};
  Put(k, kind, 0, band);
}

}  // namespace

std::optional<KickMotionConstants> LoadKickMotionConstants(const PeImage& exe, const PesVfs& vfs, std::string* error) {
  KickMotionConstants k;
  k.clip.fill(-1);  // FUN_1406A9F20 initialises the table to 0xFFFF
  const auto idle = vfs.Read(kIdletablePath, error);
  if (!idle) return std::nullopt;
  const auto move = vfs.Read(kMovetablePath, error);
  if (!move) return std::nullopt;
  for (const auto* f : {&*idle, &*move}) {
    if (f->size() < kClipKinds * kClipRecordBytes) {
      *error = "kick motion: the clip table file holds fewer than " + std::to_string(kClipKinds) + " records";
      return std::nullopt;
    }
  }
  for (int kind = 0; kind < kClipKinds; ++kind) IdletableRecord(&k, *idle, kind), MovetableRecord(&k, *move, kind);
  const std::pair<uint64_t, float*> floats[] = {
      {0x1425DBDC0, &k.n157}, {0x1425DBDBC, &k.n112}, {0x1425DBDB8, &k.n67},   {0x1425CFA50, &k.n22},       {0x14259D618, &k.p22},
      {0x1425CCA34, &k.p67},  {0x1425CB30C, &k.p112}, {0x1425E5B58, &k.cutE5B58}, {0x1425C65E8, &k.lateralKmh},
      {0x1425C48C0, &k.runKmh}, {0x1425CD078, &k.ballRange}, {0x142598590, &k.rangeDiv}};
  bool ok = true;
  for (auto [va, out] : floats) ok = ok && ReadF(exe, va, out);
  ok = ok && ReadPairs(exe, kSwapOddVa, kSwapLateralVa, &k.swapOddTick) && ReadPairs(exe, kSwapLateralVa, kSwapFirstSideVa, &k.swapLateral) &&
       ReadPairs(exe, kSwapFirstSideVa, kSwapEndVa, &k.swapFirstSide);
  // FUN_1406AE940 fills the default record with one entry per kind: `inc byte [rbx+1]` after the cap compare for kind 1.
  constexpr uint64_t kDefaultHitIncVa = 0x1406AE9A4;
  uint8_t inc[3];
  if (ok && exe.ReadVa(kDefaultHitIncVa, sizeof inc, inc) && inc[0] == 0xFE && inc[1] == 0x43 && inc[2] == 0x01) k.defaultRecordHits = 1;
  ok = ok && k.defaultRecordHits == 1;
  if (!ok) {
    *error = "kick motion: the angle cuts or swap tables are not in the exe";
    return std::nullopt;
  }
  return k;
}

int KickClipRow(const KickMotionConstants& k, int kind, int band, int side, float a) {
  if (unsigned(kind - 1) >= 0x96u || unsigned(band) >= unsigned(kClipBands) || unsigned(side) >= unsigned(kClipSides)) return kNoClipRow;
  if (kind == kKindAngleA) {
    if (a < k.n157 || a < k.n112) return kRowsA[0][side];
    if (a < k.n67) return kRowsA[1][side];
    if (k.n22 <= a && k.p22 <= a) return a < k.p67 ? kRowsA[2][side] : a < k.p112 ? kRowsA[3][side] : kRowsA[4][side];
    return kRowsAMid[side];
  }
  if (kind == kKindAngleB) {
    if (a < k.n112) return kRowsB[0][side];
    if (k.p112 < a) return kRowsB[1][side];
    return kNoClipRow;
  }
  const int16_t row = k.clip[size_t((kind * kClipBands + band) * kClipSides + side)];
  return row >= 0 ? row : kNoClipRow;
}

int KickRow(const KickMotionConstants& k, const SituationConstants& sc, int request, int kind, int band, int side, float scale_deg,
            bool flagA, bool flagB, const KickWorld* w) {
  int row = KickClipRow(k, kind, band, side, (flagA ? -1.0f : 1.0f) * scale_deg);
  if (!w || row == kNoClipRow || flagB) return row;
  const auto swap = [&row](const std::vector<std::pair<int, int>>& pairs, bool taken) {
    for (const auto& [from, to] : pairs)
      if (row == from) {
        if (taken) row = to;
        return;  // PES stops at the first matching row
      }
  };
  swap(k.swapOddTick, w->tickOdd);
  // The lateral table: the displacement turned into the player's heading frame (FUN_140A92C80 sines), as km/h.
  const float rad = float(M_PI) / 180.0f;
  const float s90 = std::sin((w->heading_deg + 90.0f) * rad), s0 = std::sin(w->heading_deg * rad);
  const float lateral = s90 * w->dispZ - s0 * w->dispX;
  swap(k.swapLateral, k.lateralKmh <= ((sc.tickRate * sc.sixty) * sc.sixty) / sc.thousand * lateral);
  swap(k.swapFirstSide, w->firstSide);
  switch (row) {
    case kRowRunShot:
      if (request == kRequestShot && !w->tickOdd) row = kRowRunShotOdd;
      break;
    case kRowRunA:
      row = kRowRunB;
      break;
    case kRowBall: {
      const float reach = k.ballRange / (sc.tickRate * k.rangeDiv);
      const float dx = w->ballX - w->dispX, dz = w->ballZ - w->dispZ;
      if (dx * dx + dz * dz <= reach * reach) {
        const float speed_kmh = std::sqrt(w->dispX * w->dispX + w->dispZ * w->dispZ) * sc.tickRate * 3.6f;  // FUN_1406EA110
        if (speed_kmh < k.runKmh) {
          if (!w->tickOdd) row = kRowBallSlow;
        } else {
          row = kRowBallFast;
        }
      } else {
        row = kRowBallFar;
      }
      break;
    }
    case kRowPassA:
      if (request == kRequestPass) row = kRowPassB;
      break;
    case kRowShotA:
      if (request == kRequestShot) row = kRowShotB;
      break;
    case kRowAction:
      if (w->actionBit) row = kRowActionB;
      break;
    default:
      break;
  }
  return row;
}

int KickMotion(const Animetable& table, int row, int column) {
  if (row < 0 || row >= kNoClipRow || column < 0 || column >= table.Layout().cols) return -1;
  int motion = table.Cell(row, column).motionNo;
  if (motion < 0) motion = 0;  // FUN_1406ABB30 with flag 0 answers 0 for an empty cell
  return motion == kMotionFixA ? kMotionFixA2 : motion == kMotionFixB ? kMotionFixB2 : motion;
}

int KickBucket(const KickMotionConstants& k, const SituationConstants& sc, float a, int kind, float req) {
  constexpr int kNone = 9;  // the answer when no cut applies
  const float gate = sc.flightHi;
  const auto wrap = [&](float v) { return v >= 0.0f ? (v >= 360.0f ? v - 360.0f : v) : v + 360.0f; };  // FUN_14043A0A0's first step
  const auto turnTo = [&] {  // FUN_14043A0A0(req, angle): the signed turn from req to angle
    const float d = wrap(a) - wrap(req);
    if (std::fabs(d) > 180.0f) return d < 0.0f ? d + 360.0f : d - 360.0f;
    return d;
  };
  const auto lab = [&](float v) { return v < sc.turn157 ? 0xD : 0x11; };  // LAB_140723FE2: 0xD under 112.5 and under 157.5
  switch (kind) {
    case 7: case 8: case 9: case 0xC: case 0xD: case 0x18: case 0x1B: {
      if (std::fabs(a) < gate) return kNone;
      const float d = turnTo();
      if (0.0f < req) {
        if (0.0f <= d) {
          if (a < k.n157 || a < k.n112) return 0;
          if (k.cutE5B58 <= a) return a < gate ? 9 : lab(a);
        } else {
          if (a < k.n157) return 0;
          if (k.n112 <= a && k.cutE5B58 <= a) return 9;
        }
        return 4;
      }
      if (d <= 0.0f) {
        if (a < k.n157) return 0;
        if (a < k.n112) return 4;
        if (a < k.cutE5B58) return 4;
        if (gate <= a) return a < k.p112 ? 0xD : 0x11;
      } else if (k.n157 <= a && k.n112 <= a && k.cutE5B58 <= a) {
        return a < gate ? 9 : lab(a);
      }
      return kNone;
    }
    default: {
      const bool header = kind >= 1 && kind < SituationConstants::kKinds ? sc.kindTable[size_t(kind) * 16 + 12] < 2
                                                                         : sc.outOfRangeRow[12] < 2;
      if (header) {
        if (a < k.n157) return 0;
        if (a < k.n112) return 4;
        if (a < k.cutE5B58) return 4;
        if (gate <= a) return lab(a);
        return kNone;
      }
      if (a < k.n157) return 0;
      if (a < k.n112) return 2;
      if (a < k.n67) return 4;
      if (a < k.n22) return 6;
      if (k.p22 <= a) return a < k.p67 ? 0xB : a < k.p112 ? 0xD : a < sc.turn157 ? 0xF : 0x11;
      return kNone;
    }
  }
}

int KickMotionDescriptor(const KickMotionConstants& k, const Mbinfo& mbinfo, int motion) {
  if (motion == kMotionE42) return kDescriptorE42;
  if (motion == kMotionF7C) return kDescriptorF7C;
  if (motion >= kMotionSlots) return k.defaultRecordHits;  // the table's default record
  return mbinfo.HitCount(motion);
}

}  // namespace pes
