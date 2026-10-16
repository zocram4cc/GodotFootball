#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <string>

#include "onthepitch/pesball.hpp"
#include "onthepitch/pesmover.hpp"
#include "pes/exe.hpp"

// PES's player-velocity accessors (FUN_140a6c6d0, FUN_140a6c990, FUN_140458740),
// read from the exe's control flow: a still player (movement state 1) and a slow
// walker (state 5 below the speed floor) are floored along their facing. The
// floor is a parameter here; the install relations test checks it against the exe.
namespace {

using pes::Vec3;
using pesmover::MoverRecord;

constexpr float kFloor = 0.5f;  // synthetic floor, not the install's

MoverRecord Mover(int state, float facingDeg, Vec3 velocity) {
  return MoverRecord{state, facingDeg, velocity};
}

TEST(PesMover, AMovingPlayerKeepsHisVelocity) {
  const Vec3 v = pesmover::FloorVelocity(Mover(2, 0.0f, Vec3(3.0f, 0.0f, 4.0f)), kFloor);
  EXPECT_FLOAT_EQ(v.x, 3.0f);
  EXPECT_FLOAT_EQ(v.y, 0.0f);
  EXPECT_FLOAT_EQ(v.z, 4.0f);
}

TEST(PesMover, ASlowWalkerBelowTheFloorMovesAtTheFloorAlongHisFacing) {
  // |v| = 0.14 is below the floor of 0.5; facing 90 degrees is +x, so the floored
  // velocity is (floor, 0, 0).
  const Vec3 v = pesmover::FloorVelocity(Mover(5, 90.0f, Vec3(0.1f, 0.0f, 0.1f)), kFloor);
  EXPECT_NEAR(v.x, kFloor, 1e-5f);
  EXPECT_NEAR(v.y, 0.0f, 1e-6f);
  EXPECT_NEAR(v.z, 0.0f, 1e-5f);
}

TEST(PesMover, AWalkerExactlyAtTheFloorKeepsHisVelocity) {
  // FUN_140a6c6d0 floors only when floor^2 > |v|^2 (strict).
  const Vec3 v = pesmover::FloorVelocity(Mover(5, 90.0f, Vec3(kFloor, 0.0f, 0.0f)), kFloor);
  EXPECT_FLOAT_EQ(v.x, kFloor);
  EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(PesMover, AStillPlayerMovesAtTheFloorAlongHisFacingWhateverHisVelocity) {
  // Facing 0 degrees is +z.
  const Vec3 v = pesmover::FloorVelocity(Mover(1, 0.0f, Vec3(3.0f, 0.0f, 4.0f)), kFloor);
  EXPECT_NEAR(v.x, 0.0f, 1e-6f);
  EXPECT_NEAR(v.z, kFloor, 1e-5f);
}

TEST(PesMover, MoverVelocityIsZeroWhenStillOrSlowAndRawOtherwise) {
  const Vec3 fast(3.0f, 0.0f, 4.0f);
  const Vec3 slow(0.1f, 0.0f, 0.1f);
  const auto zero = [](Vec3 v) { return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f; };
  EXPECT_TRUE(zero(pesmover::MoverVelocity(Mover(1, 0.0f, fast), kFloor)));
  EXPECT_TRUE(zero(pesmover::MoverVelocity(Mover(5, 0.0f, slow), kFloor)));
  const Vec3 walking = pesmover::MoverVelocity(Mover(5, 0.0f, fast), kFloor);
  EXPECT_FLOAT_EQ(walking.x, 3.0f);
  EXPECT_FLOAT_EQ(walking.z, 4.0f);
  const Vec3 moving = pesmover::MoverVelocity(Mover(2, 0.0f, slow), kFloor);
  EXPECT_FLOAT_EQ(moving.x, 0.1f);
  EXPECT_FLOAT_EQ(moving.z, 0.1f);
}

TEST(PesMover, BearingIsAtan2OfXAndZInDegreesFoldedIntoZeroToThreeSixty) {
  // FUN_140458740 calls atan2(x, z): +z is 0 degrees, +x is 90, and a negative
  // heading gains a full turn.
  const auto bearing = [](Vec3 v) { return pesmover::FloorBearingDeg(Mover(2, 0.0f, v), kFloor); };
  EXPECT_NEAR(bearing(Vec3(0.0f, 0.0f, 1.0f)), 0.0f, 1e-3f);
  EXPECT_NEAR(bearing(Vec3(1.0f, 0.0f, 0.0f)), 90.0f, 1e-3f);
  EXPECT_NEAR(bearing(Vec3(0.0f, 0.0f, -1.0f)), 180.0f, 1e-3f);
  EXPECT_NEAR(bearing(Vec3(-1.0f, 0.0f, 0.0f)), 270.0f, 1e-3f);
}

TEST(PesMover, BearingOfANoVelocityIsZero) {
  // A still player on a zero floor has no velocity, and FUN_140458740 returns 0.
  EXPECT_FLOAT_EQ(pesmover::FloorBearingDeg(Mover(1, 45.0f, Vec3(0, 0, 0)), 0.0f), 0.0f);
}

TEST(PesMoverInstall, TheExeHoldsTheFloorAndAngleConstantsTheMoverReads) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  float floor = 0.0f, degPerRad = 0.0f, twoPi = 0.0f;
  ASSERT_TRUE(exe->ReadVa(pesmover::kSpeedFloorVa, 4, reinterpret_cast<uint8_t*>(&floor)));
  ASSERT_TRUE(exe->ReadVa(pes::DEGREES_PER_RADIAN_VA, 4,
                          reinterpret_cast<uint8_t*>(&degPerRad)));
  ASSERT_TRUE(exe->ReadVa(pesmover::kTwoPiVa, 4, reinterpret_cast<uint8_t*>(&twoPi)));
  // The report's numbers: 0.6944444 is 2.5 km/h, 57.2957764 is 180/pi, 6.2831855 is 2*pi.
  EXPECT_NEAR(floor, 0.6944444f, 1e-7f);
  EXPECT_NEAR(degPerRad, 180.0f / std::acos(-1.0f), 1e-4f);
  EXPECT_NEAR(twoPi, 2.0f * std::acos(-1.0f), 1e-6f);
}

}  // namespace
