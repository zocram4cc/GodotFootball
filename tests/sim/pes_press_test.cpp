// Pure tests of the press command arithmetic (src/sim/onthepitch/pespress.hpp) against synthetic constants, plus one install
// relation test. The synthetic values are arbitrary on purpose: the tests check the arithmetic, not PES's numbers.
// WHO presses is PES's translated writer/coordinator/eval: see pes_press_world_test.cpp.

#include <gtest/gtest.h>

#include <cstdlib>

#include "onthepitch/pespress.hpp"

namespace {

pes::PressConstants Synthetic() {
  pes::PressConstants c{};
  c.framesPerSec = 54.0f;
  c.speedCut0_kmh = 0.05f;
  c.speedCut1_kmh = 8.0f;
  c.speedCut2_kmh = 13.5f;
  c.speedCut3_kmh = 22.0f;
  c.targetTickRate = 0.1f;
  c.targetTickBias = 0.5f;
  return c;
}

constexpr float kEps = 1e-3f;

TEST(PressConstantsLoader, ImageRelationsHold) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto c = pes::LoadPressConstants(*exe, &err);
  ASSERT_TRUE(c.has_value()) << err;
  EXPECT_FLOAT_EQ(c->framesPerSec, 54.0f);
  // Ordered cuts and the k = 5 tick rule.
  EXPECT_LT(c->speedCut0_kmh, c->speedCut1_kmh);
  EXPECT_LT(c->speedCut1_kmh, c->speedCut2_kmh);
  EXPECT_LT(c->speedCut2_kmh, c->speedCut3_kmh);
  EXPECT_EQ(pes::PressTargetTick(*c), 5);
}

TEST(PressTarget, BallNowOrOwnBase) {
  // Sample nearer the ball now than the base: the ball.
  EXPECT_TRUE(pes::PressTargetIsBall({1.0f, 0.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}));
  // Sample nearer the base: the base.
  EXPECT_FALSE(pes::PressTargetIsBall({9.0f, 0.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}));
  // Tie goes to the ball (<=).
  EXPECT_TRUE(pes::PressTargetIsBall({5.0f, 0.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}));
}

TEST(PressTargetTick, FpsTimesRatePlusBias) {
  // int(54 * 0.1 + 0.5) = int(5.9) = 5.
  EXPECT_EQ(pes::PressTargetTick(Synthetic()), 5);
}

TEST(PressSpeedClass, CutLegs) {
  pes::PressConstants c = Synthetic();
  EXPECT_EQ(pes::PressSpeedClass(c, 0.0f), 0);
  EXPECT_EQ(pes::PressSpeedClass(c, 0.05f), 0);
  EXPECT_EQ(pes::PressSpeedClass(c, 0.06f), 1);
  EXPECT_EQ(pes::PressSpeedClass(c, 8.0f), 1);
  EXPECT_EQ(pes::PressSpeedClass(c, 8.01f), 2);
  EXPECT_EQ(pes::PressSpeedClass(c, 13.5f), 2);
  EXPECT_EQ(pes::PressSpeedClass(c, 13.51f), 3);
  EXPECT_EQ(pes::PressSpeedClass(c, 22.0f), 3);
  EXPECT_EQ(pes::PressSpeedClass(c, 22.01f), 4);
}

TEST(PressCommandSpeed, DistanceSlopeCappedOneBandAbove) {
  pes::PressConstants c = Synthetic();
  // Standing (class 0): capped at the class-1 top, 8 km/h = 2.22 m/s.
  EXPECT_NEAR(pes::PressCommandSpeed_ms(c, 11.0f, 0.0f, 2.6f), 8.0f / 3.6f, 0.01f);
  // Near the ball: the slope wins over the cap.
  EXPECT_NEAR(pes::PressCommandSpeed_ms(c, 0.5f, 0.0f, 2.6f), 0.5f * 2.6f, kEps);
  // Sprinting (class 4): no cap below the slope.
  EXPECT_NEAR(pes::PressCommandSpeed_ms(c, 11.0f, 25.0f, 2.6f), 11.0f * 2.6f, kEps);
}

}  // namespace
