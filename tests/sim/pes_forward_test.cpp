// PES 2021's forward line in the defensive block (sub_140682770; docs/re/pes-re-attack-level-pull.md
// §2, §5, §8). Synthetic constants everywhere except the install tests, which read PES2021.exe
// through PES21_ROOT and skip when it is unset.

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>

#include "onthepitch/pesforward.hpp"
#include "pes/exe.hpp"

namespace {

using namespace pes::forward;

// The install's values, as LoadConstants reads them. The logic tests use them as given.
Constants InstallNumbers() {
  Constants k{};
  k.depthPerLevel = 0.25f;
  k.levelCeiling = 4.0f;
  k.fullHigh = 1.0f;
  k.rankFirstScale = 0.2f;
  k.rankOtherScale = 0.65f;
  k.cfScoreBonus = 30.0f;
  k.countScoreBump = -10.0f;
  k.scanGuard = -100000.0f;
  return k;
}

ForwardClasses SevenEightNine() {
  ForwardClasses c{};
  c.arm[7] = c.arm[8] = c.arm[9] = true;
  return c;
}

// The line stands between deep -40 and high 0; the floor sits at -20, so the floor bites
// on a protected forward whose target is deeper than -20.
Input Line(int level, float ballSide) {
  Input in;
  in.level = level;
  in.ballSide = ballSide;
  in.deep = -40.0f;
  in.high = 0.0f;
  in.floorRef = 20.0f;
  return in;
}

// Puts a forward on `slot`. Calls must go in slot order: a forward's rank is the number of
// forwards before him, which is what FUN_140682320 writes.
void AddForward(Input& in, int slot, int cls, float lineupScore = 0.0f) {
  in.slots[slot].cls = cls;
  in.slots[slot].onPitch = true;
  in.slots[slot].lineupScore = lineupScore;
  in.family[slot].line = 2;
  in.family[slot].rank = in.forwardCount;
  ++in.forwardCount;
}

TEST(ForwardFactor, LevelQuarterWithTheTopRungFull) {
  const Constants k = InstallNumbers();
  EXPECT_FLOAT_EQ(DepthFactor(k, 0), 0.0f);
  EXPECT_FLOAT_EQ(DepthFactor(k, 1), 0.25f);  // -1: 75% of the way back to deep
  EXPECT_FLOAT_EQ(DepthFactor(k, 4), 1.0f);   // +2: the high line
}

TEST(ForwardLine, LineTargetIsDeepPlusFactorSpan) {
  EXPECT_FLOAT_EQ(LineTarget(0.25f, -20.0f, 0.0f), -15.0f);
}

TEST(ForwardLine, BlendMovesTowardTheHighestByV108) {
  EXPECT_FLOAT_EQ(BlendTarget(-40.0f, 0.0f, 0.0f), -40.0f);
  EXPECT_FLOAT_EQ(BlendTarget(-40.0f, 0.0f, 0.5f), -20.0f);
  EXPECT_FLOAT_EQ(BlendTarget(-40.0f, 0.0f, 1.0f), 0.0f);
}

TEST(ForwardLine, TwoForwardsShareTheLineAtMinusOne) {
  Input in = Line(1, 1.0f);
  in.floorRef = 50.0f;  // the floor lies beyond the line, so it does not bite
  AddForward(in, 2, 9);
  AddForward(in, 5, 7);
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  EXPECT_FLOAT_EQ(r.target[2], -30.0f);
  EXPECT_FLOAT_EQ(r.target[5], -30.0f);
}

TEST(ForwardLine, ProtectedIsTheLastEligibleForward) {
  Input in = Line(1, -1.0f);
  AddForward(in, 1, 9);
  AddForward(in, 4, 7);
  EXPECT_EQ(ForwardLine(InstallNumbers(), SevenEightNine(), in).protectedSlot, 4);
}

TEST(ForwardLine, CounterTargetBreaksTheScan) {
  Input in = Line(1, -1.0f);
  AddForward(in, 1, 7);
  AddForward(in, 4, 9);
  in.counterSlot = 1;
  EXPECT_EQ(ForwardLine(InstallNumbers(), SevenEightNine(), in).protectedSlot, 1);
}

TEST(ForwardLine, ASingleForwardIsProtectedEvenBelowTheGuard) {
  Input in = Line(1, -1.0f);
  AddForward(in, 3, 8, -200000.0f);  // below -100000: only the one-forward break protects him
  EXPECT_EQ(ForwardLine(InstallNumbers(), SevenEightNine(), in).protectedSlot, 3);
}

// Three forwards at -1, no Counter Target, ball on the far side: the first-listed drops to
// 0.2 x 0.25, the middle keeps 0.25 unfloored, the last is the protected forward, floored.
TEST(ForwardLine, ThreeAtMinusOneFirstDropsToFivePercentWhenBallIsNotOnHisSide) {
  Input in = Line(1, -1.0f);
  AddForward(in, 1, 7);
  AddForward(in, 4, 8);
  AddForward(in, 7, 9);
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  EXPECT_EQ(r.protectedSlot, 7);
  EXPECT_FLOAT_EQ(r.target[1], -38.0f);  // -40 + 40 * 0.05
  EXPECT_FLOAT_EQ(r.target[4], -30.0f);  // -40 + 40 * 0.25, not floored
  EXPECT_FLOAT_EQ(r.target[7], -20.0f);  // -40 + 40 * 0.25 = -30, floored at -20
}

TEST(ForwardLine, ThreeAtMinusOneFirstDropsToSixteenPercentWhenBallIsOnHisSide) {
  Input in = Line(1, 1.0f);
  AddForward(in, 1, 7);
  AddForward(in, 4, 8);
  AddForward(in, 7, 9);
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  EXPECT_FLOAT_EQ(r.target[1], -33.5f);  // -40 + 40 * 0.1625
  EXPECT_FLOAT_EQ(r.target[4], -30.0f);
  EXPECT_FLOAT_EQ(r.target[7], -20.0f);
}

TEST(ForwardLine, CounterTargetOverridesTheSheet) {
  Input in = Line(1, -1.0f);
  AddForward(in, 1, 7);
  AddForward(in, 4, 8);
  AddForward(in, 7, 9);
  in.counterSlot = 4;
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  EXPECT_EQ(r.protectedSlot, 4);
  EXPECT_FLOAT_EQ(r.target[4], 0.0f);  // full factor: the high line, floored at -20 (no effect)
}

TEST(ForwardLine, LevelZeroHasNoFloor) {
  Input in = Line(0, -1.0f);
  AddForward(in, 1, 7);
  AddForward(in, 4, 8);
  AddForward(in, 7, 9);
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  EXPECT_EQ(r.protectedSlot, 7);
  EXPECT_FLOAT_EQ(r.target[7], -40.0f);  // factor 0, no floor: every forward is at deep
}

// In possession, mode 1 is the Y-sort: the line's members sorted by Y ascending, ties in slot
// order. The ball side picks the highest (> 0) or the lowest. Out of possession it is the rank.
TEST(ForwardLine, InPossessionTheLowestYIsPickedNotTheFirstListed) {
  Input in = Line(1, -1.0f);
  in.possession = true;
  AddForward(in, 1, 7);
  AddForward(in, 4, 8);
  AddForward(in, 7, 9);
  in.slots[1].y = 5.0f;
  in.slots[4].y = -3.0f;
  in.slots[7].y = 2.0f;
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  // Sorted by Y: slot 4, slot 7, slot 1. The ball side <= 0 picks the lowest, slot 4, so slot 1
  // (listed first) misses mode 1 and takes the 0.65 scale.
  EXPECT_FLOAT_EQ(r.target[1], -33.5f);  // -40 + 40 * 0.1625
  EXPECT_FLOAT_EQ(r.target[4], -30.0f);
  EXPECT_FLOAT_EQ(r.target[7], -20.0f);
}

TEST(ForwardLine, InPossessionTheHighestYIsPickedWhenBallIsOnHisSide) {
  Input in = Line(1, 1.0f);
  in.possession = true;
  AddForward(in, 1, 7);
  AddForward(in, 4, 8);
  AddForward(in, 7, 9);
  in.slots[1].y = 5.0f;
  in.slots[4].y = -3.0f;
  in.slots[7].y = 2.0f;
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  // The highest Y is slot 1, which passes mode 1: 0.2 x 0.25. Out of possession the rank says slot 7.
  EXPECT_FLOAT_EQ(r.target[1], -38.0f);  // -40 + 40 * 0.05
  EXPECT_FLOAT_EQ(r.target[4], -30.0f);
  EXPECT_FLOAT_EQ(r.target[7], -20.0f);
}

// Equal keys keep slot order (the sort is stable), so slot 1 is the lowest and passes mode 1.
TEST(ForwardLine, InPossessionTiesKeepSlotOrder) {
  Input in = Line(1, -1.0f);
  in.possession = true;
  AddForward(in, 1, 7);
  AddForward(in, 4, 8);
  AddForward(in, 7, 9);
  in.slots[1].y = 0.0f;
  in.slots[4].y = 0.0f;
  in.slots[7].y = 0.0f;
  const Result r = ForwardLine(InstallNumbers(), SevenEightNine(), in);
  EXPECT_FLOAT_EQ(r.target[1], -38.0f);
}

// The install: each constant is read at the VA of the instruction that loads it. Skipped
// without PES21_ROOT.
std::optional<pes::PeImage> OpenInstall(std::string* error) {
  const char* root = std::getenv("PES21_ROOT");
  if (root == nullptr) return std::nullopt;
  return pes::PeImage::Open(std::string(root) + "/PES2021.exe", error);
}

TEST(ForwardInstall, ConstantsAreTheExeFloats) {
  std::string error;
  auto exe = OpenInstall(&error);
  if (!exe) GTEST_SKIP() << "PES21_ROOT unset or unreadable: " << error;
  const auto k = LoadConstants(*exe, &error);
  ASSERT_TRUE(k.has_value()) << error;
  EXPECT_FLOAT_EQ(k->depthPerLevel, 0.25f);
  EXPECT_FLOAT_EQ(k->levelCeiling, 4.0f);
  EXPECT_FLOAT_EQ(k->fullHigh, 1.0f);
  EXPECT_FLOAT_EQ(k->rankFirstScale, 0.2f);
  EXPECT_FLOAT_EQ(k->rankOtherScale, 0.65f);
  EXPECT_FLOAT_EQ(k->cfScoreBonus, 30.0f);
  EXPECT_FLOAT_EQ(k->countScoreBump, -10.0f);
  EXPECT_FLOAT_EQ(k->scanGuard, -100000.0f);
}

TEST(ForwardInstall, ArmsAreSevenEightNine) {
  std::string error;
  auto exe = OpenInstall(&error);
  if (!exe) GTEST_SKIP() << "PES21_ROOT unset or unreadable: " << error;
  const auto c = LoadForwardClasses(*exe, &error);
  ASSERT_TRUE(c.has_value()) << error;
  for (int cls = 0; cls < 10; ++cls) EXPECT_EQ(c->IsForward(cls), cls >= 7) << "class " << cls;
}

}  // namespace
