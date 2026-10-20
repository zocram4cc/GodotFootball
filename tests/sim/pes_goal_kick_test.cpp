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
#include "onthepitch/pessetpiecetaker.hpp"
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

// The install's goal-kick constants (PES2021.exe), or false when no install is mounted.
bool InstalledGoalKickConstants(pes::GoalKickConstants* out) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return false;
  std::string err;
  pes::PesVfs vfs;
  if (!vfs.Mount(root, &err)) return false;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) return false;
  auto set = pes::LoadSetPieceTakerConstants(vfs, *exe, &err);
  if (!set) return false;
  *out = pes::GoalKickConstantsFrom(*set);
  return true;
}

constexpr float kPlacementTolerance_m = 1e-4f;
// The longest ladder ends by its forced kick at frame 109 (no receiver) or 271 (special); this window covers both.
constexpr unsigned long kLadderWindow_frames = 400;

// The first frame the ladder releases on, counted from its first tick (frame 0), and the order it gives.
struct Release {
  long frame = -1;
  pes::GoalKickOrder order;
};

template <typename InputsAt>
Release FirstRelease(const pes::GoalKickConstants& k, InputsAt inputsAt) {
  pes::GoalKickLadder ladder;
  for (unsigned long f = 0; f < kLadderWindow_frames; ++f) {
    const pes::GoalKickOrder order = pes::StepGoalKickLadder(ladder, f, k, inputsAt(f));
    if (order.released) return {static_cast<long>(f), order};
  }
  return {};
}

pes::GoalKickLadderInputs WithReceiver(int index) {
  pes::GoalKickLadderInputs in;
  in.lofted = index;
  return in;
}

}  // namespace

TEST(PesGoalKick, TheDefaultLadderPassesToTheMateOnTheFrameItsLoftedWindowOpens) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the ladder's timers are PES's";
  // The 1 s wait ends on frame 54, the 0.5 s short window on frame 81, and the lofted window is first
  // evaluated on the frame after its stamp: the transition ticks emit nothing.
  const Release r = FirstRelease(k, [](unsigned long) { return WithReceiver(2); });
  EXPECT_EQ(r.frame, 82);
  EXPECT_EQ(r.order.command, pes::GoalKickCommand::kLoftedToReceiver);
  EXPECT_EQ(r.order.receiver, 2);
}

TEST(PesGoalKick, WithoutAReceiverTheKickGoesIntoSpaceWhenTheWindowsHavePassed) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the ladder's timers are PES's";
  // The lofted window runs its own length, and the forced kick comes on the frame after it closes.
  const Release r = FirstRelease(k, [](unsigned long) { return WithReceiver(-1); });
  EXPECT_EQ(r.frame, 109);
  EXPECT_EQ(r.order.command, pes::GoalKickCommand::kLoftedIntoSpace);
  EXPECT_EQ(r.order.receiver, -1);
}

TEST(PesGoalKick, AReceiverFoundLateIsPassedToOnTheFrameItAppears) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the ladder's timers are PES's";
  const Release r = FirstRelease(k, [](unsigned long f) { return WithReceiver(f >= 90 ? 4 : -1); });
  EXPECT_EQ(r.frame, 90);
  EXPECT_EQ(r.order.receiver, 4);
}

TEST(PesGoalKick, ASpecialRestartBelowTheTacticsGateWaitsThreeSecondsBeforeItsWindow) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the ladder's timers are PES's";
  const Release r = FirstRelease(k, [&k](unsigned long) {
    pes::GoalKickLadderInputs in = WithReceiver(2);
    in.specialRestart = true;
    in.tactics = k.tacticsGate * 0.5f;
    return in;
  });
  EXPECT_EQ(r.frame, 244);
}

TEST(PesGoalKick, ASpecialRestartAtTheTacticsGateTakesTheDefaultRoute) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the ladder's timers are PES's";
  const Release r = FirstRelease(k, [&k](unsigned long) {
    pes::GoalKickLadderInputs in = WithReceiver(2);
    in.specialRestart = true;
    in.tactics = k.tacticsGate;  // not below the gate: the three-second wait is skipped
    return in;
  });
  EXPECT_EQ(r.frame, 82);
}

TEST(PesGoalKick, AValidSpecialObjectSkipsTheThreeSecondWait) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the ladder's timers are PES's";
  const Release r = FirstRelease(k, [&k](unsigned long) {
    pes::GoalKickLadderInputs in = WithReceiver(2);
    in.specialRestart = true;
    in.tactics = k.tacticsGate * 0.5f;
    in.specialObjectValid = true;
    return in;
  });
  EXPECT_EQ(r.frame, 83);
}

TEST(PesGoalKick, TheStandPointIsTheSpotLessTheClipsOffsetInXAndZWithItsHeightKept) {
  // Heading 0: SetKick negates the offset's x and z, and the placer keeps its height.
  const pes::Vec3 stand =
      pes::GoalKickStandPoint(pes::Vec3(47.8f, -2.6f, 0.0f), 0.0f, pes::Vec3(1.0f, 0.5f, 2.0f));
  EXPECT_NEAR(stand.x, 46.8f, kPlacementTolerance_m);
  EXPECT_NEAR(stand.y, -0.6f, kPlacementTolerance_m);
  EXPECT_NEAR(stand.z, 0.5f, kPlacementTolerance_m);
}

TEST(PesGoalKick, TheStandPointTurnsWithTheSpotsHeading) {
  // A quarter turn carries the offset's z into the pitch's x.
  const pes::Vec3 stand =
      pes::GoalKickStandPoint(pes::Vec3(47.8f, -2.6f, 0.0f), 90.0f, pes::Vec3(1.0f, 0.5f, 2.0f));
  EXPECT_NEAR(stand.x, 45.8f, kPlacementTolerance_m);
  EXPECT_NEAR(stand.y, -3.6f, kPlacementTolerance_m);
  EXPECT_NEAR(stand.z, 0.5f, kPlacementTolerance_m);
}

TEST(PesGoalKick, TheMirrorFlipsTheClipOffsetInX) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the centimetre is PES's";
  // A hit 100 cm across (x), 50 cm up and 200 cm forward (z): the other foot flips only the across component.
  const pes::Vec3 hit(100.0f, 50.0f, 200.0f);
  const pes::Vec3 right = pes::ClipOffsetMetres(hit, 1.0f, false, k);
  const pes::Vec3 left = pes::ClipOffsetMetres(hit, 1.0f, true, k);
  EXPECT_NEAR(right.x, 1.0f, kPlacementTolerance_m);
  EXPECT_NEAR(left.x, -1.0f, kPlacementTolerance_m);
  EXPECT_NEAR(left.y, right.y, kPlacementTolerance_m);
  EXPECT_NEAR(left.z, right.z, kPlacementTolerance_m);
}

TEST(PesGoalKick, TheContactTickScalesTheThirtyHertzHitFrameToTheLogicRate) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the tick base is PES's";
  // A hit at frame 10 of a 30 Hz clip lands 10 / 30 s in: 18 of PES's 54 logic frames.
  EXPECT_EQ(pes::GoalKickContactTicks(10, k), 18UL);
}

TEST(PesGoalKick, TheClipsRootCarriesTheKeeperFromTheStandPointByTheHitRootLength) {
  pes::GoalKickConstants k;
  if (!InstalledGoalKickConstants(&k)) GTEST_SKIP() << "PES21_ROOT not set: the centimetre is PES's";
  const pes::Vec3 stand(51.6f, -0.1f, 0.0f);
  // The hit record's root is 388 cm along and 252 cm across: the root moves that far in the pitch's plane.
  const pes::Vec3 contact = pes::GoalKickContactRoot(stand, 0.0f, pes::Vec3(388.0f, 0.0f, 252.0f), false, k);
  const float dx = contact.x - stand.x;
  const float dy = contact.y - stand.y;
  EXPECT_NEAR(std::sqrt(dx * dx + dy * dy), std::sqrt(3.88f * 3.88f + 2.52f * 2.52f), 0.01f);
}

TEST(PesGoalKick, TheStrikeWarpReachesTheContactRootOnTheContactTick) {
  const pes::Vec3 from(40.0f, 0.0f, 0.0f);
  const pes::Vec3 to(50.0f, 4.0f, 0.0f);
  const pes::Vec3 halfway = pes::GoalKickStrikeRoot(from, to, 5, 10);
  EXPECT_NEAR(halfway.x, 45.0f, kPlacementTolerance_m);
  EXPECT_NEAR(halfway.y, 2.0f, kPlacementTolerance_m);
  const pes::Vec3 onTheTick = pes::GoalKickStrikeRoot(from, to, 10, 10);
  EXPECT_NEAR(onTheTick.x, 50.0f, kPlacementTolerance_m);
  EXPECT_NEAR(onTheTick.y, 4.0f, kPlacementTolerance_m);
  // No ticks to play: the contact root at once.
  EXPECT_NEAR(pes::GoalKickStrikeRoot(from, to, 0, 0).x, 50.0f, kPlacementTolerance_m);
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
