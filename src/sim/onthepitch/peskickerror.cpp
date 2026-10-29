#include "peskickerror.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <utility>

namespace pes {
namespace {

float Clamp01(float x) { return std::min(1.0f, std::max(0.0f, x)); }

// `lo >= x -> 0, x >= hi -> 1, else (x - lo) / span`: the ramp idiom every
// input of FUN_14083cc30 is built from.
float Ramp(float x, float lo, float hi, float span) {
  if (lo >= x) return 0.0f;
  if (x >= hi) return 1.0f;
  return (x - lo) / span;
}

// 0x1406ebf30: Range(slot 2) mod 100 < threshold, threshold = int(pSign*100)
// (FUN_1408398d5 passes the record's pSign that way). +1 on success.
constexpr int kPercent = 100;
float SignRoll(Random& rng, float pSign) {
  const int threshold = static_cast<int>(pSign * static_cast<float>(kPercent));
  return rng.Range(kKickRngSlot) % kPercent < threshold ? 1.0f : -1.0f;
}

// Normal's 0..99 back to the |N - 50| / 50 every caller computes.
float HalfNormal(const KickErrorConstants& k, int n) {
  return std::fabs(static_cast<float>(n) - k.normalCentre) / k.normalCentre;
}

// F1/F2 (scatter 6, the motion-descriptor factors, 0x14083e749..0x14083e76f) key on the kicker's motion id through
// FUN_1406b0250 over the motion table, a runtime singleton at 0x1436f23c0.
// ponytail: GF has neither PES's motion id nor that table, so F stays 1.0, the value PES stores (0x14083e749) when the
// descriptor byte is not 1. Remove when GF kicks carry a PES motion id and the table is read from the install.
constexpr float kMotionFactorNotPorted = 1.0f;

constexpr uint64_t kImageBase = 0x140000000;
constexpr int kLastKind = 0x95;                    // cmp [rcx-1], 0x95 in the kind-row getters
constexpr uint16_t kMotionNoGate = 0x9F1;           // 0x14083E03B: the gate does not apply while this motion plays
constexpr uint64_t kKindTableVa = 0x14344E380;      // 16 bytes per row, row = kind
constexpr uint64_t kKindOutRowVa = 0x14344E620;     // the row the getters return for a kind outside 1..0x96
constexpr int kRowSubclass = 12;                    // getter 0x14071E520 (movsx byte [rsp+0xc])
constexpr int kRowClass = 13;                       // getter 0x1407205A0 (movsx byte [rsp+0xd])
constexpr uint64_t kWindowStoreVa = 0x1406EB6FC;    // W1: mov qword [rcx+0x2764], imm32 (720.0f)

// Motion ids the kick-angle bucket is overridden for (jump table 0x14083FF68 / bytes 0x14083FF78, 0x14083CD6E..CDCD).
constexpr uint16_t kMotionBucketF = 0x9BE;          // -> 0xF
constexpr uint16_t kMotionBucketD1 = 0xA20, kMotionBucketD2 = 0xA7D;  // -> 0xD
constexpr uint16_t kMotionKindA[] = {0x9B0, 0x9B4, 0xA41, 0xA42, 0xA45};  // -> 9 for kinds 0x2B and 0x3A
constexpr int kKindMotionA = 0x2B, kKindMotionB = 0x3A;

// The bytes at `va` must equal `want` before an immediate beside them is trusted.
bool Bytes(const PeImage& exe, uint64_t va, std::initializer_list<uint8_t> want) {
  uint8_t b[16];
  if (want.size() > sizeof b || !exe.ReadVa(va, want.size(), b)) return false;
  return std::equal(want.begin(), want.end(), b);
}
bool U8(const PeImage& exe, uint64_t va, uint8_t* v) { return exe.ReadVa(va, 1, v); }
bool U32(const PeImage& exe, uint64_t va, uint32_t* v) {
  return exe.ReadVa(va, sizeof *v, reinterpret_cast<uint8_t*>(v));
}

// `lea eax,[rcx-lo]; cmp eax,span` (8D 41 d8 83 F8 s8, or 8D 81 d32 83 F8 s8): the unsigned range test lo..lo+span.
bool LeaRange(const PeImage& exe, uint64_t va, uint32_t* lo, uint32_t* span) {
  uint8_t b[9];
  if (!exe.ReadVa(va, sizeof b, b)) return false;
  if (b[0] == 0x8D && b[1] == 0x41 && b[3] == 0x83 && b[4] == 0xF8) {
    *lo = static_cast<uint32_t>(-static_cast<int32_t>(static_cast<int8_t>(b[2])));
    *span = b[5];
    return true;
  }
  if (b[0] == 0x8D && b[1] == 0x81 && b[6] == 0x83 && b[7] == 0xF8) {
    int32_t disp;
    std::memcpy(&disp, b + 2, sizeof disp);
    *lo = static_cast<uint32_t>(-disp);
    *span = b[8];
    return true;
  }
  return false;
}
bool InRange(uint32_t kind, uint32_t lo, uint32_t span) { return kind - lo <= span; }

// The four predicates of Q's gate, evaluated for kinds 0..0x96 from their own code and tables:
// 0x1407240F0 (a jump table over kinds 0x1D..0x92), 0x140724590 (two ranges), 0x140726610 (three), 0x140726640 (a bit
// mask over 0..0x1A and a range).
bool LoadGatePredicates(const PeImage& exe, SituationConstants* c) {
  uint32_t r[8][2];  // 24590: 0, 1. 26610: 2, 3, 4. 26640: 5.
  bool ok = LeaRange(exe, 0x140724590, r[0], r[0] + 1) && LeaRange(exe, 0x14072459B, r[1], r[1] + 1) &&
            LeaRange(exe, 0x140726610, r[2], r[2] + 1) && LeaRange(exe, 0x14072661B, r[3], r[3] + 1) &&
            LeaRange(exe, 0x140726626, r[4], r[4] + 1) && LeaRange(exe, 0x140726652, r[5], r[5] + 1);
  uint8_t limit = 0;
  uint32_t mask = 0, byteTab = 0, dwordTab = 0;
  uint8_t addHead[6];
  ok = ok && Bytes(exe, 0x140726640, {0x83, 0xF9}) && U8(exe, 0x140726642, &limit) &&
       Bytes(exe, 0x140726645, {0xB8}) && U32(exe, 0x140726646, &mask);
  // 0x1407240F0: add ecx,-lo; cmp ecx,span; ... movzx eax,byte[rdx+rax+byteTab]; mov ecx,[rdx+rax*4+dwordTab]
  ok = ok && exe.ReadVa(0x1407240F0, sizeof addHead, addHead) && addHead[0] == 0x83 && addHead[1] == 0xC1 &&
       addHead[3] == 0x83 && addHead[4] == 0xF9 && Bytes(exe, 0x140724102, {0x0F, 0xB6, 0x84, 0x02}) &&
       U32(exe, 0x140724106, &byteTab) && Bytes(exe, 0x14072410A, {0x8B, 0x8C, 0x82}) &&
       U32(exe, 0x14072410D, &dwordTab) && Bytes(exe, 0x140724116, {0xB0, 0x01, 0xC3});  // mov al,1; ret
  if (!ok) return false;
  const uint32_t lo240F0 = static_cast<uint32_t>(-static_cast<int32_t>(static_cast<int8_t>(addHead[2])));
  const uint32_t span240F0 = addHead[5];
  constexpr uint64_t kTrueTarget = 0x140724116;
  for (uint32_t k = 0; k < SituationConstants::kKinds; ++k) {
    uint8_t g = 0;
    if (InRange(k, lo240F0, span240F0)) {
      uint8_t idx = 0;
      uint32_t rel = 0;
      if (!U8(exe, kImageBase + byteTab + (k - lo240F0), &idx) || !U32(exe, kImageBase + dwordTab + 4u * idx, &rel))
        return false;
      if (kImageBase + rel == kTrueTarget) g |= 1;
    }
    if (InRange(k, r[0][0], r[0][1]) || InRange(k, r[1][0], r[1][1])) g |= 2;
    if (InRange(k, r[2][0], r[2][1]) || InRange(k, r[3][0], r[3][1]) || InRange(k, r[4][0], r[4][1])) g |= 4;
    if ((k <= limit && ((mask >> k) & 1)) || InRange(k, r[5][0], r[5][1])) g |= 8;
    c->gateKinds[k] = g;
  }
  return true;
}

bool LoadSituationConstants(const PeImage& exe, SituationConstants* c) {
  const std::pair<uint64_t, float*> rdata[] = {
      {0x14258c260, &c->p90},       {0x1425ddf70, &c->p88},      {0x1425eb850, &c->p86},
      {0x14256ae1c, &c->one},       {0x1425ab124, &c->p95},      {0x142572348, &c->p80},
      {0x1425ab120, &c->p85},       {0x14257a194, &c->p70},      {0x1425ef9dc, &c->p925},
      {0x14259d624, &c->tickRate},  {0x14259a764, &c->sixty},    {0x14256ae34, &c->thousand},
      {0x14259bdfc, &c->stopLow},   {0x14253ee30, &c->hundred},  {0x14259be34, &c->fifty},
      {0x14259bde4, &c->c0Gain},    {0x14259222c, &c->c13Gain},  {0x1425a2004, &c->speedTop},
      {0x14256ae18, &c->staminaK},  {0x14259be30, &c->turnLo},   {0x14259be60, &c->turnHi},
      {0x14259be5c, &c->turnDiv},   {0x1425c48c0, &c->lowKnee},  {0x1425c48b4, &c->lowGain},
      {0x14259bdec, &c->topGain},   {0x14257a190, &c->effK},     {0x142588200, &c->tenth},
      {0x142590d98, &c->turnSq},    {0x1425a8a28, &c->c2Base},   {0x1425c48a0, &c->c2Gain},
      {0x14259858c, &c->windowLimit}, {0x142598580, &c->halfTurn}, {0x142588218, &c->negOne},
      {0x1425cca3c, &c->turn157},   {0x1425a9bf0, &c->turn130},  {0x1425c5950, &c->turn125},
      {0x14256ae30, &c->turn120},   {0x1425c48d4, &c->turn115},  {0x14259d628, &c->turn110}};
  bool ok = true;
  for (auto [va, out] : rdata) ok &= exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out));
  const std::pair<uint64_t, double*> doubles[] = {
      {0x14259d5e0, &c->aGain}, {0x14259d5e0, &c->aPlusFirst}, {0x1425ef868, &c->aPlusLater}};
  for (auto [va, out] : doubles) ok &= exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out));
  // W1 stores the no-window angle with `mov qword [rcx+0x2764], imm32`.
  uint32_t window = 0;
  ok = ok && Bytes(exe, kWindowStoreVa, {0x48, 0xC7, 0x81, 0x64, 0x27, 0x00, 0x00}) &&
       U32(exe, kWindowStoreVa + 7, &window);
  std::memcpy(&c->noWindow, &window, sizeof window);
  ok = ok && exe.ReadVa(kKindTableVa, c->kindTable.size(), c->kindTable.data()) &&
       exe.ReadVa(kKindOutRowVa, c->outOfRangeRow.size(), c->outOfRangeRow.data());
  return ok && LoadGatePredicates(exe, c);
}


// The install's quarter-wave sine table (513 floats at 0x142655950), read at load and mapped into the translated sine.
constexpr uint64_t kSinTableVa = 0x142655950;

}  // namespace

std::optional<KickErrorConstants> LoadKickErrorConstants(const PeImage& exe, std::string* error) {
  KickErrorConstants k{};
  bool ok = true;
  const std::pair<uint64_t, float*> rdata[] = {
      {0x14259be2c, &k.qFloorStat},     {0x14259a764, &k.qMidStat},
      {0x1425c594c, &k.qTopStat},       {0x142588200, &k.qLowGain},
      {0x14259be1c, &k.qLowSpan},       {0x1425ab120, &k.qMidGain},
      {0x14259a760, &k.qMidSpan},       {0x1425a8a28, &k.qTopGain},
      {0x142592228, &k.qTopQuarter},    {0x1425ab124, &k.qTopBase},
      {0x14259222c, &k.skillBonus},     {0x14259d618, &k.angleScale_deg},
      {0x142590d98, &k.speedScale},     {0x1425ef9f8, &k.ampGain},
      {0x14259bdd4, &k.ampBase},        {0x142565600, &k.half},
      {0x142592228, &k.aBase},          {0x14259a764, &k.minute_s},
      {0x14256ae34, &k.km_m},           {0x142588210, &k.rampPerQuality},
      {0x14259a764, &k.rampLo_kmh},     {0x14259be58, &k.groundHi_kmh},
      {0x142596e6c, &k.otherHi_kmh},    {0x1425ab124, &k.wQuality},
      {0x142590d98, &k.wGround},        {0x1425e4a30, &k.wOther},
      {0x14259be1c, &k.pitchLo_deg},    {0x142591140, &k.pitchHi_deg},
      {0x142598578, &k.powerLo},        {0x142572348, &k.powerSpan},
      {0x142592228, &k.shootPowerWeight}, {0x14259b89c, &k.rWeight},
      {0x142572348, &k.dBase},          {0x1425a946c, &k.contactLo},
      {0x14259be2c, &k.contactSpan},    {0x14259be1c, &k.contactHi},
      {0x14256ae1c, &k.nearOpp_m},      {0x142565604, &k.farOpp_m},
      {0x14259bddc, &k.dWeight},        {0x1426550e4, &k.normalDiv},
      {0x14259be34, &k.normalCentre},   {0x1426550dc, &k.normalTabScale},
      {0x14259a768, &k.normalNegTwo},   {0x1426550e8, &k.normalTabSteps},
      {0x14256ae34, &k.normalDraw},     {0x14259858c, &k.fullTurn_deg},
      {0x14259be5c, &k.quarterTurn_deg}};
  for (auto [va, out] : rdata)
    ok &= exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out));
  constexpr uint64_t LOG_TABLE_VA = 0x1426548c0;
  ok &= exe.ReadVa(LOG_TABLE_VA, sizeof k.logTable, reinterpret_cast<uint8_t*>(k.logTable.data()));
  ok &= exe.ReadVa(kSinTableVa, sizeof k.sinTable, reinterpret_cast<uint8_t*>(k.sinTable.data()));
  if (!ok) {
    *error = "kick error constant: .rdata unreadable";
    return std::nullopt;
  }
  auto penalty = LoadPenaltyScatterConstants(exe, error);
  if (!penalty) return std::nullopt;
  k.penaltyScatter = *penalty;
  if (!LoadSituationConstants(exe, &k.situation)) {
    *error = "kick situation constant: .rdata or gate predicate unreadable";
    return std::nullopt;
  }
  return k;
}

float KickQuality(const KickErrorConstants& k, float s) {
  float q;
  if (k.qMidStat > s)
    q = (s - k.qFloorStat) * k.qLowGain / k.qLowSpan;
  else if (k.qTopStat > s)
    q = (s - k.qMidStat) * k.qMidGain / k.qMidSpan + k.qLowGain;
  else
    q = (s - k.qTopStat) * k.qTopGain * k.qTopQuarter + k.qTopBase;
  return std::max(0.0f, q);
}

float SoftMin4(float a, float b, float c, float d) {
  // FUN_1408413c0: qsort descending (comparator 0x14083cc10 is -1 when a > b),
  // then fold from x = 1.0 once per factor: x = (max*min + min) * 0.5 with
  // min/max of (x, v). The 1.0 is 0x14256ae1c, the 0.5 is 0x142565600.
  constexpr float kFoldStart = 1.0f;
  constexpr float kFoldHalf = 0.5f;
  float v[4] = {a, b, c, d};
  std::sort(v, v + 4, std::greater<float>());
  float x = kFoldStart;
  for (const float f : v) {
    const float lo = std::min(x, f);
    const float hi = std::max(x, f);
    x = (hi * lo + lo) * kFoldHalf;
  }
  return x;
}

// 0x140a92c80: sin of `deg` in degrees, times `amp`. Runs PES's own translated function
// (ball/pes_fn_140A92C80.cpp: xmm0 = deg, xmm1 = amp, result in xmm0). The translated code reads
// the quarter-wave table through its memory view, so the install's table is mapped as that window.
float SinTableDeg(const KickErrorConstants& k, float deg, float amp) {
  PesState s;
  s.mem.window = {kSinTableVa, sizeof k.sinTable, reinterpret_cast<const uint8_t*>(k.sinTable.data()),
                  nullptr};
  s.set_x_f32(0, deg);
  s.set_x_f32(1, amp);
  ::pes_fn_140A92C80(s);
  return s.x_f32(0);
}

int Normal(Random& rng, const KickErrorConstants& k, float amp) {
  constexpr uint32_t kDrawModulus = 1000;  // 0x10624dd3 >> 6 and imul 0x3e8
  constexpr int kLast = 99;                // cmp eax, 0x63 ; jbe
  // Both offered angles rejected: PES jumps back to redraw the pair
  // (0x140a9275f), so this loops until one lands.
  for (;;) {
    const uint32_t s1 = rng.Step(kKickRngSlot);
    const uint32_t s2 = rng.Step(kKickRngSlot);
    const float angle = static_cast<float>(s2 % kDrawModulus + 1) / k.normalDraw * k.fullTurn_deg;
    const float t = static_cast<float>(s1 % kDrawModulus + 1) / k.normalDraw * k.normalTabSteps;
    const int idx = static_cast<int>(t);
    const float v = idx >= kNormalTableSize - 1
                        ? 0.0f
                        : k.logTable[idx] + (k.logTable[idx + 1] - k.logTable[idx]) *
                                                (t - static_cast<float>(idx));
    // sqrt(-2 ln u) * amp / sqrt(-2 ln (1/512)): Box-Muller scaled so the
    // table's far end is `amp` (scatter 2).
    const float g = std::sqrt(v * k.normalTabScale * k.normalNegTwo) * amp / k.normalDiv;
    for (int attempt = 0; attempt < 2; ++attempt) {
      const float a = SinTableDeg(k, angle + (attempt != 1 ? k.quarterTurn_deg : 0.0f), 1.0f);
      const float x = (1.0f + a * g) * k.normalCentre;
      const int res = static_cast<int>((x < 0.0f ? -k.half : k.half) + x);
      if (res >= 0 && res <= kLast) return res;
    }
  }
}

float PassLaunchPenalty(const KickErrorConstants& k, float a, float speed_mps, float pitch_deg,
                        bool groundPass) {
  const float kmh = speed_mps * k.minute_s * k.minute_s / k.km_m;
  const float lo = a * k.rampPerQuality + k.rampLo_kmh;
  const float hi = a * k.rampPerQuality + (groundPass ? k.groundHi_kmh : k.otherHi_kmh);
  float ramp = Ramp(kmh, lo, hi, hi - lo);
  const float w = std::max(0.0f, 1.0f - a * k.wQuality) * (groundPass ? k.wGround : k.wOther);
  // The elevation ramp (ap+0x2754 == 0 branch).
  ramp = std::max(ramp, Ramp(pitch_deg, k.pitchLo_deg, k.pitchHi_deg, k.rampPerQuality));
  return 1.0f - ramp * w;
}

float ShotLaunchPenalty(const KickErrorConstants& k, float power) {
  // ponytail: the -0.02*g pitch term (0x14083dd17..dd27; 0.02 at 0x1425c4894, 0.25 at 0x142592228)
  // is left out. g = SinTableDeg(90*x, xmm12) with x = 0.25*(xmm14-xmm1)
  // when xmm14 < xmm7, else xmm12. xmm14, xmm7, xmm1 are set before 0x14083da40 and are
  // not traced. Moves I by at most 0.02.
  return 1.0f - k.shootPowerWeight * Ramp(power, k.powerLo, 1.0f, k.powerSpan);
}

float PressureFactor(const KickErrorConstants& k, float contactSelf, float contactOpp,
                     float oppDistance_m) {
  if (oppDistance_m >= k.farOpp_m) return 1.0f;
  // 0x14043b8b0's distance ramp: 1 inside a metre, 0 at two.
  const float distRamp = oppDistance_m <= k.nearOpp_m ? 1.0f : k.farOpp_m - oppDistance_m;
  const float edge = Ramp(contactSelf - contactOpp, k.contactLo, k.contactHi, k.contactSpan);
  // ponytail: xmm8 (contact-byte factor, 0x14083ecc9..0x14083ed32; KickDecode (d)): obj = 0x140a48910(FUN_1406ea340(kicker));
  // byte[obj+1]==0x48 gives 1.0 (0x14083ecd4); else B = byte[obj+0x2c] maps to t = 1 for B <= 9, 0 for B >= 54, else
  // (54-B)/45 (9 = 54/6 by the magic divide, 54 = int 0x14351AE50 set to 0x36 by 0x141e532f0). The angRamp is xmm7:
  // clamp((err+20)/40)*0.4, err = byte[obj2+0x28] - byte[r13+0x28] (0x14083ed40..ed87). GF has no anime or player
  // record for either: xmm8 is 1.0 here and the xmm7 angRamp is omitted.
  const float d = (k.dBase - k.rWeight * edge) * distRamp;
  // ponytail: FUN_140723320 (run term, 3-pass running max of 0.1*sum; weights 0x14344ef78..0x14344ef94 and
  // 0x14344ef98/9c/a0 = -0.0029828, 2.7572925, 8.7184591; case affine from byte[r15+1]-8 via table 0x14072394c)
  // needs the kicker's contact vector and the state id of PES's player record. GF has neither: run = 0.
  return 1.0f - k.rWeight * (k.dWeight * d);
}

float PesHeadingDeg(float x, float z) {
  // FUN_14046F230(vec, out): out = FUN_14046F260's bearing of (vec[0], vec[2]), PES's own translated code, over a
  // scratch window that holds the vector, the out slot and the stack.
  constexpr uint64_t kBase = 0x20000, kVec = kBase, kVecZ = kBase + 8, kOut = kBase + 0x100;
  constexpr uint64_t kStack = kBase + 0x1000;
  std::array<uint8_t, 0x2000> scratch{};
  PesState s;
  s.mem.window = {kBase, scratch.size(), nullptr, scratch.data()};
  s.gpr[4] = kStack;
  s.gpr[1] = kVec;
  s.gpr[2] = kOut;
  s.wrf(kVec, x);
  s.wrf(kVecZ, z);
  ::pes_fn_14046F230(s);
  return s.rdf(kOut);
}

namespace {

const uint8_t* KindRow(const SituationConstants& c, int kind) {
  return (kind - 1 >= 0 && kind - 1 <= kLastKind) ? &c.kindTable[static_cast<size_t>(kind) * 16]
                                                  : c.outOfRangeRow.data();
}
int RowClass(const uint8_t* row) { return static_cast<int8_t>(row[kRowClass]); }
int RowSubclass(const uint8_t* row) { return static_cast<int8_t>(row[kRowSubclass]); }

// `to - from`, `from` wrapped to [0, 360), folded into [-180, 180]: 0x14083D195..0x14083D1F7 (kick angle),
// 0x14083E0AD..0x14083E11C (body turn) and FUN_1404B6390 (first-time window), all one routine.
float AngleDelta(const SituationConstants& c, float from, float to) {
  if (0.0f > from)
    from += c.windowLimit;
  else if (from >= c.windowLimit)
    from -= c.windowLimit;
  float d = to - from;
  if (static_cast<float>(std::fabs(static_cast<double>(d))) > c.halfTurn)
    d -= (0.0f > d ? c.negOne : c.one) * c.windowLimit;
  return d;
}

float AbsF(float x) { return static_cast<float>(std::fabs(static_cast<double>(x))); }

// The bucket byte `uVar14` after the motion override (0x14083CD6E..0x14083CDCD) and the first-time window
// (0x14083CDCD..0x14083CF07).
int EffectiveBucket(const SituationConstants& c, const KickSituation& s, int cls, float launchBearing) {
  int u = s.entry2755;
  if (s.motion == kMotionBucketF) {
    u = 0xF;
  } else if (s.motion == kMotionBucketD1 || s.motion == kMotionBucketD2) {
    u = 0xD;
  } else if (s.kind == kKindMotionA || s.kind == kKindMotionB) {
    for (const uint16_t m : kMotionKindA)
      if (s.motion == m) u = 9;
  }
  if (!(s.window < c.windowLimit && (s.firstTime == 1 || cls == 2))) return u;
  float d = AngleDelta(c, s.window, launchBearing);  // FUN_1404B6390(window, &bearing)
  if (s.touch) d *= c.negOne;
  const bool later = s.firstTime != 0;
  float limit;
  switch (cls) {
    case 0: limit = later ? c.turn125 : c.turn130; break;
    case 1: limit = later ? c.turn115 : c.turn120; break;
    case 2: limit = later ? c.turn110 : c.turn115; break;
    default: return u;  // classes 3 and up skip the window (0x14083CE4F)
  }
  if (d > c.turn157) return 0x11;
  return d > limit ? 0xF : u;
}

}  // namespace

float KickBase(const SituationConstants& c, const KickSituation& s) {
  const uint8_t* row = KindRow(c, s.kind);
  const int cls = RowClass(row);
  const int sub = RowSubclass(row);
  float q;
  switch (cls) {
    case 0: q = c.p90; break;
    case 1: q = c.p88; break;
    case 2:
    case 3: q = c.p86; break;
    default: q = c.one; break;
  }
  switch (sub) {  // jump table 0x1408400A0
    case 1: q *= c.p95; break;
    case 5: q *= c.p80; break;
    case 6: q *= c.p85; break;
    case 7:
    case 0xB: q *= c.p90; break;
    case 8: q *= c.p70; break;
    default: break;
  }
  const uint8_t g = (s.kind >= 0 && s.kind < SituationConstants::kKinds) ? c.gateKinds[s.kind] : 0;
  const bool excluded = (g & 1) != 0;   // 0x1407240F0
  const bool listed = (g & 14) != 0;    // 0x140724590 | 0x140726610 | 0x140726640
  if (!excluded && !s.aiBit && listed && s.motion != kMotionNoGate) {
    if (static_cast<unsigned>(cls) < 2)
      q *= c.p95;
    else if (cls == 3)
      q *= c.p925;
    else
      q *= c.p90;
  }
  return q;
}

float SituationFactor(const SituationConstants& c, const KickSituation& s) {
  float q = KickBase(c, s);
  const uint8_t* row = KindRow(c, s.kind);
  const int cls = RowClass(row);
  const int sub = RowSubclass(row);
  const float launchBearing = PesHeadingDeg(s.launchX, s.launchZ);
  const int bucket = EffectiveBucket(c, s, cls, launchBearing);
  const float a = s.quality;
  const float aLeft = c.one - a;  // 1 - a ([rbp-0x60])

  // Speed (km/h) of the kicker: ap+0x2480 / +0x2488 are metres per tick.
  const float kmhPerTickUnit = ((c.tickRate * c.sixty) * c.sixty) / c.thousand;
  float speed = kmhPerTickUnit * std::sqrt(s.velZ * s.velZ + s.velX * s.velX);
  // Kick angle against the kicker's heading picks the speed at which the strike starts to suffer.
  const float kickAngle = AbsF(AngleDelta(c, s.heading_deg, launchBearing));
  float limit = c.stopLow;
  if (cls >= 0 && cls <= 3) {
    const float gain = cls == 0 ? c.c0Gain : (cls == 2 ? c.stopLow : c.c13Gain);
    const float t = Ramp(kickAngle, c.fifty, c.hundred, c.fifty);
    limit = t * gain + c.stopLow;
  }
  if (speed > limit) speed = ((c.speedTop - limit) * s.stamina) * c.staminaK + limit;

  // The body turn (ap+0xBE4 against the heading) widens the speed window.
  const float turn = AbsF(AngleDelta(c, s.heading_deg, s.body_deg));
  const float turnRamp = Ramp(turn, c.turnLo, c.turnHi, c.turnDiv);
  const float turn100 = turnRamp * c.hundred;
  const float low = c.lowKnee - (turn100 * c.lowGain) / c.hundred;
  const float high = c.speedTop - (turn100 * c.topGain) / c.hundred;
  const float speedRamp = Ramp(speed, low, high, high - low);
  q *= c.one - ((aLeft * c.effK) + c.tenth) * speedRamp;

  switch (sub) {  // jump table 0x1408400D4
    case 0: case 1: case 9: case 10:
      q *= c.one - (turnRamp * turnRamp) * c.turnSq;
      break;
    case 2: case 3: case 4: case 5: case 6: case 7: case 0xC:
      if (bucket == 0 || bucket == 2 || bucket == 0xF || bucket == 0x11) {
        q *= static_cast<float>(static_cast<double>(a) * c.aGain +
                                (s.firstTime == 0 ? c.aPlusFirst : c.aPlusLater));
      } else if (bucket == 4 || bucket == 0xD) {
        float x;
        if (cls == 2)
          x = s.firstTime == 0 ? aLeft * c.tenth + c.c2Base : aLeft * c.c2Gain + c.c2Base;
        else
          x = s.firstTime == 0 ? aLeft * c.c2Base : aLeft * c.tenth;
        q *= c.one - x;
      }
      break;
    default: break;
  }
  // Subclass 0 and 1 with the b2754 byte at 9 (0x14083E691).
  constexpr int kTailByte = 9;
  if (sub == 0 || sub == 1) {
    if (s.b2754 == kTailByte) q *= s.firstTime == 0 ? c.p90 : c.p85;
  }
  // ponytail: subclass 2..12 with the motion-descriptor byte 1 (FUN_1406b0250 over the runtime motion table, 0x14083E42E)
  // multiplies Q again from the target entity's geometry (0x14083E437..0x14083E607). GF has no motion table: skipped.
  // The oracle (tools/dev/rendezvous/kick_replay.py --q) records the descriptor byte; rows with 1 are not compared.
  return q;
}

KickScatter BuildScatter(const KickErrorConstants& k, float a, float launchPenalty,
                         float pressure, float situation) {
  const float q = situation;
  const float qk = k.half * q + (1.0f - k.half);       // Qk, k = 0.5 by default
  const float qs = k.half * q + k.half;                // 0.5Q + 0.5
  const float stat = a * k.ampBase + k.aBase;          // A = 0.75a + 0.25
  const float f = kMotionFactorNotPorted;
  auto soft = [&](float x) { return SoftMin4(launchPenalty, x, f, pressure); };
  auto record = [&](float scale, float qScale, float qAmp) {
    const float m = 1.0f - qAmp;
    return ScatterRecord{(1.0f - qScale) * scale, k.half, m * m * k.ampGain + k.ampBase};
  };
  return {record(k.angleScale_deg, soft(q), soft(stat * q)),
          record(k.angleScale_deg, soft(qk), soft(stat * qk)),
          record(k.speedScale, soft(qs), soft(stat * qs))};
}

KickError DrawKickError(const KickErrorConstants& k, Random& rng, const KickScatter& s,
                        bool halveNegativePitch) {
  KickError e;
  // ponytail: the yaw redraw loop (0x14083a550, <=6 draws) is skipped; it
  // only re-rolls a yaw that matches a sign test on untraced fields.
  float h = HalfNormal(k, Normal(rng, k, s.yaw.amp));
  e.yaw_deg = SignRoll(rng, s.yaw.pSign) * s.yaw.scale * h;
  h = HalfNormal(k, Normal(rng, k, s.pitch.amp));
  const float pitchSign = SignRoll(rng, s.pitch.pSign);
  e.pitch_deg = pitchSign * s.pitch.scale * h;
  if (pitchSign < 0.0f && halveNegativePitch) e.pitch_deg *= k.half;
  h = HalfNormal(k, Normal(rng, k, s.speed.amp));
  e.speedFactor = 1.0f + SignRoll(rng, s.speed.pSign) * s.speed.scale * h;
  return e;
}

void ApplyKickError(const KickError& e, float v[3]) {
  constexpr float kDegToRad = 3.14159265f / 180.0f;
  const float horizontal = std::sqrt(v[0] * v[0] + v[1] * v[1]);
  const float speed = std::sqrt(horizontal * horizontal + v[2] * v[2]) * e.speedFactor;
  const float heading = std::atan2(v[1], v[0]) + e.yaw_deg * kDegToRad;
  const float pitch = std::atan2(v[2], horizontal) + e.pitch_deg * kDegToRad;
  v[0] = speed * std::cos(pitch) * std::cos(heading);
  v[1] = speed * std::cos(pitch) * std::sin(heading);
  v[2] = speed * std::sin(pitch);
}

}  // namespace pes
