#include "onthepitch/pesmarktable.hpp"

#include <cmath>
#include <cstring>

#include "pes/exe.hpp"

namespace pes {
namespace marktable {
namespace {

// Global slots are 0..21 (0x1404F621C CMP R13D,0x15; JA exit).
constexpr int kLastSlot = 0x15;
// The commit index is id % 11 (0x1404F65AA MUL 0xBA2E8BA3; SHR 3; IMUL 11; SUB).
constexpr int kFold = 11;

bool ReadFloatVa(const PeImage& exe, uint64_t va, float* out) {
  uint8_t bytes[sizeof(float)];
  if (!exe.ReadVa(va, sizeof bytes, bytes)) return false;
  float v = 0.0f;
  std::memcpy(&v, bytes, sizeof v);
  if (!std::isfinite(v)) return false;
  *out = v;
  return true;
}

constexpr uint64_t kVaRampBase = 0x14259BDF0;
constexpr uint64_t kVaRampTop = 0x14258C264;
constexpr uint64_t kVaConeOffset = 0x14259BE0C;
constexpr uint64_t kVaCeilD2 = 0x1425C48E4;
constexpr uint64_t kVaWrapCap = 0x1425C48D0;
constexpr uint64_t kVaWrapSide = 0x142596E6C;
constexpr uint64_t kVaConeDeg = 0x14259BE5C;
constexpr uint64_t kVaConeLen = 0x14259BE04;
constexpr uint64_t kVaGateFrac = 0x14259BE44;
constexpr uint64_t kVaGateRadius = 0x14259BE04;
constexpr uint64_t kVaGateWidth = 0x142565604;
constexpr uint64_t kVaHalf = 0x142565600;

// Wrap to (-180, 180]: FUN_14043A0A0's single-fold diff (matches GF WrapDeg).
float Wrap180(float d) {
  if (d > 180.0f) d -= 360.0f;
  if (d <= -180.0f) d += 360.0f;
  return d;
}

// The cone ramp of 0x1404F6354-0x1404F638F: lo = F[5] - 7.0, top = F[5] + 3.0,
// (sign*holderX - lo) / (top - lo) through the COMISS ladder, then cone = ramp - 15.0.
// FieldInfo F[5] is the goal-area half-width (9.16 m); GF carries it (pesGoalAreaHalfWidth).
float ConeHead(float sign, float holderX, float goalAreaHalfWidth_m, const Constants& k) {
  const float lo = goalAreaHalfWidth_m - k.rampBase;
  const float top = goalAreaHalfWidth_m + k.rampTop;
  float ramp = 0.0f;
  if (lo < sign * holderX) ramp = (sign * holderX >= top) ? 1.0f : (sign * holderX - lo) / (top - lo);
  return ramp - k.coneOffset;
}

// Bearing (a -> b) in [0, 360): FUN_140439FE0's rule over FUN_140A92D40. Coincident reads 0.0.
// ponytail: std::atan2 stands in for PES's atan2 polynomial 0x140A92D40 (lane-core O3).
float PairBearing(float ax, float az, float bx, float bz) {
  if (ax == bx && az == bz) return 0.0f;
  float r = std::atan2(bx - ax, bz - az) * 57.295776f;
  if (r < 0.0f) r += 360.0f;
  return r;
}
}  // namespace

bool ConePasses(float candX, float candZ, float holderX, float holderZ, float headArg,
                float inner, const Constants& k) {
  const float dx = holderX - candX;
  const float dz = holderZ - candZ;
  const float d2 = dx * dx + dz * dz;
  if (d2 >= k.coneLen * k.coneLen) return false;  // 0x140A53E20/26/2D JBE
  if (d2 <= inner * inner) return false;          // 0x140A53E2F/38/3F JBE
  const float live = PairBearing(candX, candZ, holderX, holderZ);
  return std::fabs(Wrap180(live - headArg)) < k.half * k.coneDeg;  // 0x140A53E81 CMOVA
}

int CountGate(float holderX, float holderZ, float u, const Facts& facts, const Constants& k) {
  const float radius2 = k.gateRadius * k.gateRadius;
  const float band = k.gateFrac / k.gateWidth;
  int count = 0;
  for (int i = 0; i < kSlots; ++i) {
    const float dx = facts.gateMates[i][0] - holderX;
    const float dz = facts.gateMates[i][1] - holderZ;
    if (dx * dx + dz * dz >= radius2) continue;  // 0x140A4E9D5/9D8 JNC: touching fails
    const float b = PairBearing(holderX, holderZ, facts.gateMates[i][0], facts.gateMates[i][1]);
    if (std::fabs(Wrap180(b - u)) <= band) ++count;  // 0x140A4EA42/45 JA: touching counts
  }
  return count;
}

std::optional<Constants> LoadMarkTableConstants(const PeImage& exe, std::string* error) {
  Constants c;
  const struct {
    uint64_t va;
    float* field;
  } table[] = {
      {kVaRampBase, &c.rampBase},
      {kVaRampTop, &c.rampTop},
      {kVaConeOffset, &c.coneOffset},
      {kVaCeilD2, &c.ceilD2},
      {kVaWrapCap, &c.wrapCap},
      {kVaWrapSide, &c.wrapSide},
      {kVaConeDeg, &c.coneDeg},
      {kVaConeLen, &c.coneLen},
      {kVaGateFrac, &c.gateFrac},
      {kVaGateRadius, &c.gateRadius},
      {kVaGateWidth, &c.gateWidth},
      {kVaHalf, &c.half},
  };
  for (const auto& entry : table) {
    if (!ReadFloatVa(exe, entry.va, entry.field)) {
      if (error != nullptr) {
        *error = "mark table constant unreadable at VA 0x" + std::to_string(entry.va);
      }
      return std::nullopt;
    }
  }
  return c;
}

void BuildMarkTable(MarkState* state, const Facts& facts, const Constants& k) {
  state->mark.fill(0);  // 0x1404F61F7/1FE/206: zero T+0xB7EC..B7F6 on entry
  // tgt = side*11 + markSlot (0x1404F620D-215); exit unless < 22 (0x1404F621C/220).
  if (facts.side < 0 || facts.side > 1) return;
  if (state->markSlot < 0 || state->markSlot > 10) return;
  const int tgt = facts.side * kFold + state->markSlot;
  if (tgt > kLastSlot) return;
  // Holder gates (0x1404F6242-255, 0x1404F626D-27D): < 22, not a keeper, not the target.
  if (facts.holder < 0 || facts.holder > kLastSlot) return;
  if (facts.holderIsKeeper) return;
  if (facts.holder == tgt) return;
  const float cone = ConeHead(facts.sign, facts.holderX, facts.goalAreaHalfWidth_m, k);
  int marks = 0;
  for (const Candidate& cand : facts.candidates) {
    if (cand.id < 0 || cand.id > kLastSlot) continue;  // 0x1404F6400 JA skip
    if (cand.id == facts.holder) continue;             // 0x1404F6409 JZ skip
    if (cand.id == tgt) continue;                      // 0x1404F6411 JZ skip
    if (cand.d2 > k.ceilD2) return;  // 0x1404F6439/43D JA: hard exit, not continue
    // FUN_140A4C720(holderX, candX, sign, cone): (sign*cone + holderX)*sign < sign*candX.
    if ((facts.sign * cone + facts.holderX) * facts.sign >= facts.sign * cand.posX) continue;
    if (cand.actionWord != 1 && cand.actionWord != 0x3F) continue;  // 0x1404F6484-492
    // Wrap tests (0x1404F64D9 cap 105, 0x1404F650D side 80, 0x1404F6532 floor 80).
    // The subtrahend is the facing word: FUN_14043A0A0(a, b) = wrap180(b - a) with a = facing (0x1404F64C5, 0x1404F64F9).
    if (std::fabs(Wrap180(cand.pairToTarget - cand.head)) > k.wrapCap) continue;    // 0x1404F64D9 JA
    if (std::fabs(Wrap180(cand.pairToHolder - cand.head)) > k.wrapSide) continue;   // 0x1404F650D JA
    if (k.wrapSide > std::fabs(Wrap180(cand.pairToHolder - cone))) continue;
    const bool coneHit = ConePasses(cand.posX, cand.posZ, facts.holderX, facts.holderZ,
                                    cand.pairToHolder, InnerQuirk(), k);  // 0x1404F6567 TEST AL,AL
    // 0x1404F6595 CMOVNZ + 0x1404F659D CMP AL,BL: count <= (cone != 0).
    if (CountGate(facts.holderX, facts.holderZ, cand.pairToHolder, facts, k) >
        (coneHit ? 1 : 0))
      continue;
    state->mark[cand.id % kFold] = 1;  // 0x1404F65CF
    if (++marks >= kMaxMarks) return;  // 0x1404F65D7/DB
  }
}

}  // namespace marktable
}  // namespace pes
