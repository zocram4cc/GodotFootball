// PES 2021's goal kick on the pes_loop path (docs/re/pes-re-goal-kick.md).
//
// The taker is the keeper (ThinkUnitSetPlay MODE 3). After the set-play wait his command is a
// lofted pass to the mate the lofted picker names, or a lofted kick with no receiver. The flight
// is PES's own ball integrator; the launch speed is the one that brings the ball down through the
// receiving height at the receiver's distance.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "onthepitch/pesgoalkick.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace {

// Test-only launch elevation (degrees above the ground). The solver is checked for any direction;
// this one is only a fixed point to fly from.
constexpr float kTestElevation_deg = 20.0f;
constexpr float kTestSpeedTolerance_m = 0.25f;
constexpr float kDegreesPerRadian = 57.29578f;

// Ball start on the spot (GF space, z up), about one ball radius above the pitch.
pes::Vec3 Spot() { return pes::Vec3(0.0f, 0.0f, 0.11f); }

// Unit launch direction in GF space at the test elevation, heading down +x.
pes::Vec3 TestDirection() {
  const float e = kTestElevation_deg / kDegreesPerRadian;
  return pes::Vec3(std::cos(e), 0.0f, std::sin(e));
}

// The install's ball constants, or skip: the flight is PES's and has no GF stand-in.
bool LoadBallConstants(pes::PesBallConstants* out) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return false;
  std::string err;
  pes::PesVfs vfs;
  if (!vfs.Mount(root, &err)) return false;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) return false;
  auto constants = pes::LoadPesBallConstants(*exe, vfs, &err);
  if (!constants) return false;
  *out = *constants;
  return true;
}

}  // namespace

TEST(PesGoalKick, TheKickIsLoftedWhetherOrNotAReceiverIsFound) {
  EXPECT_EQ(pes::GoalKickAfterWait(true), pes::GoalKickCommand::kLoftedToReceiver);
  EXPECT_EQ(pes::GoalKickAfterWait(false), pes::GoalKickCommand::kLoftedIntoSpace);
}

TEST(PesGoalKick, ThePickerTakesTheBestRatedMateOtherThanTheTaker) {
  // Taker at index 2 has the top rating; he must not be picked.
  EXPECT_EQ(pes::PickGoalKickReceiver({0.5f, 0.9f, 0.95f}, 2), 1);
  // A worse-rated mate loses to a better one wherever it stands in the list.
  EXPECT_EQ(pes::PickGoalKickReceiver({0.9f, 0.5f, 0.2f}, 2), 0);
}

TEST(PesGoalKick, ThePickerFindsNobodyWhenTheTakerIsAlone) {
  EXPECT_EQ(pes::PickGoalKickReceiver({0.7f}, 0), -1);
  EXPECT_EQ(pes::PickGoalKickReceiver({}, 0), -1);
}

TEST(PesGoalKick, TheTouchTurnsThePowerIntoTheLaunchSpeedTheKickAsksFor) {
  // humanoid.cpp: touchVec = ballDirection * 36 * (ballPower + 0.3).
  EXPECT_NEAR(36.0f * (pes::PowerForLaunchSpeed(26.0f) + 0.3f), 26.0f, 1e-3f);
  EXPECT_NEAR(36.0f * (pes::PowerForLaunchSpeed(10.0f) + 0.3f), 10.0f, 1e-3f);
}

TEST(PesGoalKick, TheSolvedSpeedLandsTheBallOnTheReceiverDistance) {
  pes::PesBallConstants c;
  if (!LoadBallConstants(&c)) GTEST_SKIP() << "PES21_ROOT not set: the flight is PES's";
  for (float distance_m : {20.0f, 30.0f, 45.0f}) {
    const float speed = pes::LoftedKickSpeed(c, Spot(), TestDirection(), distance_m,
                                             pes::kLoftedReceiveHeight_m);
    const float landed = pes::LoftedFlightDistance(c, Spot(), TestDirection(), speed,
                                                   pes::kLoftedReceiveHeight_m);
    EXPECT_NEAR(landed, distance_m, kTestSpeedTolerance_m) << "distance " << distance_m;
  }
}

TEST(PesGoalKick, AFartherReceiverNeedsAFasterKick) {
  pes::PesBallConstants c;
  if (!LoadBallConstants(&c)) GTEST_SKIP() << "PES21_ROOT not set: the flight is PES's";
  const float near = pes::LoftedKickSpeed(c, Spot(), TestDirection(), 20.0f,
                                          pes::kLoftedReceiveHeight_m);
  const float mid = pes::LoftedKickSpeed(c, Spot(), TestDirection(), 30.0f,
                                         pes::kLoftedReceiveHeight_m);
  const float far = pes::LoftedKickSpeed(c, Spot(), TestDirection(), 45.0f,
                                         pes::kLoftedReceiveHeight_m);
  EXPECT_LT(near, mid);
  EXPECT_LT(mid, far);
}

// A lower receiving height is met later on the way down, so the ball has travelled further.
TEST(PesGoalKick, TheBallCrossesALowerReceivingHeightFurtherAlong) {
  pes::PesBallConstants c;
  if (!LoadBallConstants(&c)) GTEST_SKIP() << "PES21_ROOT not set: the flight is PES's";
  constexpr float kSpeed_mps = 25.0f;
  const float high = pes::LoftedFlightDistance(c, Spot(), TestDirection(), kSpeed_mps, 3.0f);
  const float mid = pes::LoftedFlightDistance(c, Spot(), TestDirection(), kSpeed_mps,
                                              pes::kLoftedReceiveHeight_m);
  const float low = pes::LoftedFlightDistance(c, Spot(), TestDirection(), kSpeed_mps, 0.5f);
  EXPECT_LT(high, mid);
  EXPECT_LT(mid, low);
}

// Drag only ever takes range away: the ball reaches the receiving height nearer than the
// drag-free parabola would.
TEST(PesGoalKick, DragKeepsTheFlightShorterThanTheVacuumParabola) {
  pes::PesBallConstants c;
  if (!LoadBallConstants(&c)) GTEST_SKIP() << "PES21_ROOT not set: the flight is PES's";
  constexpr float kSpeed_mps = 25.0f;
  const float e = kTestElevation_deg / kDegreesPerRadian;
  const float vx = kSpeed_mps * std::cos(e);
  const float vy = kSpeed_mps * std::sin(e);
  const float h = pes::kLoftedReceiveHeight_m;
  const float y0 = c.image.ballRadius;
  const float fall = std::sqrt(vy * vy + 2.0f * c.image.gravity * (h - y0));
  const float vacuum = vx * (vy + fall) / c.image.gravity;
  const float drag = pes::LoftedFlightDistance(c, Spot(), TestDirection(), kSpeed_mps, h);
  EXPECT_GT(drag, 0.0f);
  EXPECT_LT(drag, vacuum);
}
