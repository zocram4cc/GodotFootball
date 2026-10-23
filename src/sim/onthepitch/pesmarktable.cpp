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
constexpr uint64_t kVaGateRadius = 0x14259BE44;
constexpr uint64_t kVaHalf = 0x142565600;

// Wrap to (-180, 180]: FUN_14043A0A0's single-fold diff (matches GF WrapDeg).
float Wrap180(float d) {
  if (d > 180.0f) d -= 360.0f;
  if (d <= -180.0f) d += 360.0f;
  return d;
}

// The cone ramp of 0x1404F6354-0x1404F638A: clamp((sign*holderX - (wind - base)) / top)
// in [0, 1], then cone = ramp - offset. Zero when the wind is missing (no GF carrier).
float ConeHead(float sign, float holderX, float wind, const Constants& k) {
  const float lo = wind - k.rampBase;
  float ramp = (sign * holderX - lo) / k.rampTop;
  if (ramp < 0.0f) ramp = 0.0f;
  if (ramp > 1.0f) ramp = 1.0f;
  return ramp - k.coneOffset;
}

}  // namespace

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
      {kVaGateRadius, &c.gateRadius},
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
  const float cone = ConeHead(facts.sign, facts.holderX, facts.rampWind, k);
  int marks = 0;
  for (const Candidate& cand : facts.candidates) {
    if (cand.id < 0 || cand.id > kLastSlot) continue;  // 0x1404F6400 JA skip
    if (cand.id == facts.holder) continue;             // 0x1404F6409 JZ skip
    if (cand.id == tgt) continue;                      // 0x1404F6411 JZ skip
    if (cand.d2 > k.ceilD2) return;  // 0x1404F6439/43D JA: hard exit, not continue
    if (!facts.sideOk) continue;     // 0x1404F647C JZ skip (FUN_140A4C720)
    if (!facts.actionOk) continue;   // 0x1404F648D/492 (pl+0x5D0 in {1, 0x3F})
    // Wrap tests (0x1404F64D9 cap 105, 0x1404F650D side 80, 0x1404F6532 floor 80).
    if (std::fabs(Wrap180(cand.pairToTarget - cand.head)) > k.wrapCap) continue;
    if (std::fabs(Wrap180(cand.pairToHolder - cand.head)) > k.wrapSide) continue;
    if (k.wrapSide > std::fabs(Wrap180(cand.pairToHolder - cone))) continue;
    if (!facts.coneOk) continue;     // 0x1404F6574-59F (FUN_140A53DA0)
    if (!facts.partnerOk) continue;  // 0x1404F659D-59F (FUN_140A4EAF0 <= cone)
    state->mark[cand.id % kFold] = 1;  // 0x1404F65CF
    if (++marks >= kMaxMarks) return;  // 0x1404F65D7/DB
  }
}

}  // namespace marktable
}  // namespace pes
