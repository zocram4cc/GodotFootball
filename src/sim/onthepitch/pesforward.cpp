#include "onthepitch/pesforward.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include "pes/exe.hpp"

namespace pes::forward {
namespace {

constexpr uint64_t kImageBase = 0x140000000;
// The pick loop's jump table (sub_140682770, JMP RCX at 0x140683025): one RVA per class, indexed c - 1.
constexpr uint64_t kPickTableVa = 0x140683D60;
// The arm that classes 7-9 jump to (0x140683027: the protected-scan body).
constexpr uint64_t kForwardArmVa = 0x140683027;
// Role 9, CF: the lineup bump in the protected scan (CMP ESI,0x9 at 0x14068305A).
constexpr int kClassCf = 9;

struct ConstantSite {
  uint64_t va;
  float Constants::*field;
};

constexpr std::array<ConstantSite, 8> kConstantSites{{
    {0x142592228, &Constants::depthPerLevel},
    {0x1425656C0, &Constants::levelCeiling},
    {0x14256AE1C, &Constants::fullHigh},
    {0x142598578, &Constants::rankFirstScale},
    {0x14259D5D0, &Constants::rankOtherScale},
    {0x142591140, &Constants::cfScoreBonus},
    {0x14259BE84, &Constants::countScoreBump},
    {0x1425C6614, &Constants::scanGuard},
}};

bool IsForwardSlot(const ForwardClasses& classes, const Input& in, int s) {
  return in.slots[s].onPitch && classes.IsForward(in.slots[s].cls);
}

// FUN_1406405C0, mode 0: the candidate is the first or the last of his line.
bool FirstOrLast(const Input& in, int s) {
  const int rank = in.family[s].rank;
  return rank == 0 || rank == in.forwardCount - 1;
}

// FUN_1406405C0, mode 1 out of possession: the ball side picks the line's last man when
// dir * ballY > 0, else its first.
bool PickedByBallSide(const Input& in, int s) {
  const int rank = in.family[s].rank;
  return in.ballSide > 0.0f ? rank == in.forwardCount - 1 : rank == 0;
}

// FUN_1406405C0, mode 1 in possession: the line's members (a nonzero B class on the same line
// in the family) sorted by Y ascending. FUN_140A79050 is a stable insertion sort on the Y key,
// so ties keep slot order. The ball side picks the highest (> 0) or the lowest.
bool PickedByY(const Input& in, int s) {
  std::array<int, kSlots> members{};
  int n = 0;
  for (int t = 0; t < kSlots; ++t)
    if (in.slots[t].cls != 0 && in.family[t].line == in.family[s].line) members[n++] = t;
  std::stable_sort(members.begin(), members.begin() + n,
                   [&in](int a, int b) { return in.slots[a].y < in.slots[b].y; });
  return (in.ballSide > 0.0f ? members[n - 1] : members[0]) == s;
}

bool PickedInMode1(const Input& in, int s) {
  return in.possession ? PickedByY(in, s) : PickedByBallSide(in, s);
}

// The protected forward: the pick loop at 0x140682FA2..0x1406830B3. A Counter Target holder, or
// a line of one, takes the protection and ends the scan; otherwise every eligible forward is
// kept in turn, so the last one in slot order wins.
int ProtectedSlot(const Constants& k, const ForwardClasses& classes, const Input& in) {
  int protectedSlot = -1;
  for (int s = 0; s < kSlots; ++s) {
    if (!IsForwardSlot(classes, in, s)) continue;
    if (s == in.counterSlot || in.forwardCount <= 1) return s;
    float score = in.slots[s].lineupScore;
    if (in.slots[s].cls == kClassCf) score += k.cfScoreBonus;
    if (in.forwardCount > 2 && FirstOrLast(in, s)) score += k.countScoreBump;
    // COMISS at 0x140683091 and CMOVNC at 0x140683095: keep unless below the guard (NaN is below).
    if (k.scanGuard <= score) protectedSlot = s;
  }
  return protectedSlot;
}

}  // namespace

float DepthFactor(const Constants& k, int level) {
  if (level <= 0) return 0.0f;
  if (static_cast<float>(level) >= k.levelCeiling) return k.fullHigh;
  return static_cast<float>(level) * k.depthPerLevel;
}

float LineTarget(float factor, float deep, float high) { return deep + (high - deep) * factor; }

float BlendTarget(float deep, float highest, float v108) { return deep + (highest - deep) * v108; }

Result ForwardLine(const Constants& k, const ForwardClasses& classes, const Input& in) {
  Result r;
  r.protectedSlot = ProtectedSlot(k, classes, in);
  const float factor = DepthFactor(k, in.level);
  const bool rankBlock = in.forwardCount > 2 && static_cast<float>(in.level) < k.levelCeiling;
  for (int s = 0; s < kSlots; ++s) {
    if (!IsForwardSlot(classes, in, s)) continue;
    float f = factor;
    if (rankBlock && s != r.protectedSlot && FirstOrLast(in, s))
      f *= PickedInMode1(in, s) ? k.rankFirstScale : k.rankOtherScale;
    if (s == in.counterSlot) f = k.fullHigh;
    float target = LineTarget(f, in.deep, in.high);
    // MAXSS at 0x140683363: the protected forward is floored at -ref, unless the level is 0.
    if (s == r.protectedSlot && in.level != 0) {
      const float floor = -in.floorRef;
      if (floor > target) target = floor;
    }
    r.target[s] = target;
  }
  return r;
}

std::optional<Constants> LoadConstants(const PeImage& exe, std::string* error) {
  Constants k{};
  for (const ConstantSite& site : kConstantSites) {
    if (!exe.ReadVa(site.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*site.field)))) {
      *error = "forward line: a constant of PES2021.exe is unmapped";
      return std::nullopt;
    }
  }
  return k;
}

std::optional<ForwardClasses> LoadForwardClasses(const PeImage& exe, std::string* error) {
  ForwardClasses c{};
  for (int cls = 1; cls <= 9; ++cls) {
    uint8_t raw[sizeof(uint32_t)];
    if (!exe.ReadVa(kPickTableVa + sizeof(uint32_t) * uint64_t(cls - 1), sizeof raw, raw)) {
      *error = "forward line: the pick jump table at 0x140683D60 is unmapped";
      return std::nullopt;
    }
    uint32_t rva;
    std::memcpy(&rva, raw, sizeof rva);
    c.arm[cls] = kImageBase + rva == kForwardArmVa;
  }
  return c;
}

}  // namespace pes::forward
