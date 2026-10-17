// PES's pass-lane blocker test (onthepitch/peslane.hpp): hand-placed players against the lane
// geometry, with constants read from the install. Lane values are the one-touch G11 lane (spec
// docs/re/pes-re-lane-core.md 6.2): length 15 m, cone 60 deg, radius offset 2 m, origin (0, 0).
// Slots: home 0..10 (side 0), away 11..21 (side 1).

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "onthepitch/peslane.hpp"
#include "pes/exe.hpp"

namespace lane = pes::lane;

namespace {

using Players = std::array<lane::LanePlayer, lane::kSlots>;

lane::LaneRequest TestLane() { return {{0.0f, 0.0f}, 0.0f, 15.0f, 60.0f, 2.0f}; }

void Put(Players& p, int slot, float x, float z, float vx = 0.0f, float vz = 0.0f,
         int category = -1) {
  p[slot].pos = {x, z};
  p[slot].vel = {vx, vz};
  p[slot].active = true;
  p[slot].positionCategory = category;
}

class LaneCoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) GTEST_SKIP() << "PES21_ROOT unset: lane constants come from the install";
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    auto k = lane::LoadLaneConstants(*exe, &err);
    ASSERT_TRUE(k.has_value()) << err;
    k_ = *k;
  }

  lane::LaneConstants k_{};
};

TEST_F(LaneCoreTest, ClearLaneRecordsNothing) {
  Players p{};
  Put(p, 11, 0.0f, 50.0f);
  const lane::LaneScan s = lane::ScanLane(k_, p, 3, 1, TestLane(), false, 0);
  EXPECT_EQ(s.count, 0);
  EXPECT_EQ(s.nearest, lane::kNoSlot);
  EXPECT_TRUE(lane::LaneFree(k_, p, 3, TestLane(), false, 0));
}

TEST_F(LaneCoreTest, OpponentInsideConeIsRecorded) {
  Players p{};
  Put(p, 11, 0.0f, 10.0f);
  const lane::LaneScan s = lane::ScanLane(k_, p, 3, 1, TestLane(), false, 0);
  EXPECT_EQ(s.count, 1);
  EXPECT_EQ(s.nearest, 11);
  EXPECT_FALSE(lane::LaneFree(k_, p, 3, TestLane(), false, 0));
}

TEST_F(LaneCoreTest, OpponentOutsideConeAngleIsNotRecorded) {
  // Inside the annulus but past the half-cone seen from the apex.
  Players p{};
  Put(p, 11, 10.0f, 10.0f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), false, 0).count, 0);
}

TEST_F(LaneCoreTest, OpponentCrossingTheSegmentIsRecordedAndBeyondItsEndIsNot) {
  // Slot 11 crosses x = 0 at z = 8, inside the segment [0, 15]. It lies outside the annulus, so
  // only the crossing test records it. Slot 12 crosses at z = 40, past the segment's end.
  Players p{};
  Put(p, 11, -20.0f, 8.0f, 40.0f, 0.0f);
  Put(p, 12, -20.0f, 40.0f, 40.0f, 0.0f);
  const lane::LaneScan s = lane::ScanLane(k_, p, 3, 1, TestLane(), false, 1);
  EXPECT_EQ(s.count, 1);
  EXPECT_EQ(s.nearest, 11);
}

TEST_F(LaneCoreTest, PredictionUsesVelocityOverTheHorizon) {
  // Slot 11 is 40 m away and runs 2.5 m per frame straight at the lane: 12 frames bring it to 10 m.
  Players p{};
  Put(p, 11, 0.0f, 40.0f, 0.0f, -2.5f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), false, 0).count, 0);
  const lane::LaneScan s = lane::ScanLane(k_, p, 3, 1, TestLane(), false, 12);
  EXPECT_EQ(s.count, 1);
  EXPECT_EQ(s.nearest, 11);
}

TEST_F(LaneCoreTest, NearestOfSeveralIsTheClosestPredictedPlayer) {
  Players p{};
  Put(p, 11, 0.0f, 12.0f);
  Put(p, 12, 0.0f, 6.0f);
  Put(p, 13, 0.0f, 9.0f);
  const lane::LaneScan s = lane::ScanLane(k_, p, 3, 1, TestLane(), false, 0);
  EXPECT_EQ(s.count, 3);
  EXPECT_EQ(s.nearest, 12);
}

TEST_F(LaneCoreTest, EqualDistancesKeepTheLowestScannedSlot) {
  Players p{};
  Put(p, 12, 0.0f, 6.0f);
  Put(p, 11, 0.0f, 6.0f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), false, 0).nearest, 11);
}

TEST_F(LaneCoreTest, TeammateBranchSkipsTheOriginConeAndExcludesTheReference) {
  // Slot 11 sits at (1, -0.2): inside the annulus and within 30 deg of the apex bearing, but
  // 101 deg from the origin. An opponent is refused by that origin bearing; a teammate is not.
  Players teammates{};
  Put(teammates, 11, 1.0f, -0.2f);
  Put(teammates, 14, 0.0f, 10.0f);  // the reference itself: never scanned
  const lane::LaneScan t = lane::ScanLane(k_, teammates, 14, 1, TestLane(), false, 0);
  EXPECT_EQ(t.count, 1);
  EXPECT_EQ(t.nearest, 11);

  Players opponents{};
  Put(opponents, 11, 1.0f, -0.2f);
  EXPECT_EQ(lane::ScanLane(k_, opponents, 3, 1, TestLane(), false, 0).count, 0);
}

TEST_F(LaneCoreTest, LaneFreeScansTheSideOppositeTheReference) {
  Players p{};
  Put(p, 11, 0.0f, 10.0f);  // away
  EXPECT_FALSE(lane::LaneFree(k_, p, 3, TestLane(), false, 0));    // home reference: away scanned
  EXPECT_TRUE(lane::LaneFree(k_, p, 14, TestLane(), false, 0));    // away reference: away is teammates

  Players home{};
  Put(home, 4, 0.0f, 10.0f);  // home teammate of reference 3
  EXPECT_TRUE(lane::LaneFree(k_, home, 3, TestLane(), false, 0));
  EXPECT_FALSE(lane::LaneFree(k_, home, 14, TestLane(), false, 0));  // away reference: home scanned
}

TEST(PesLane, IsAwaySlotIsElevenThroughTwentyOne) {
  EXPECT_FALSE(lane::IsAwaySlot(10));
  EXPECT_TRUE(lane::IsAwaySlot(11));
  EXPECT_TRUE(lane::IsAwaySlot(21));
  EXPECT_FALSE(lane::IsAwaySlot(22));
}

TEST_F(LaneCoreTest, KeeperIsExcludedUnlessKept) {
  Players p{};
  Put(p, 11, 0.0f, 10.0f, 0.0f, 0.0f, /*category*/ 0);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), /*keepKeeper*/ false, 0).count, 0);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), /*keepKeeper*/ true, 0).count, 1);
}

TEST_F(LaneCoreTest, KeeperSlotIsTheFirstActiveCategoryZeroOfTheSide) {
  Players p{};
  Put(p, 11, 0.0f, 0.0f, 0.0f, 0.0f, 3);
  Put(p, 12, 0.0f, 0.0f, 0.0f, 0.0f, 0);
  p[12].active = false;  // inactive keeper-category man is not the keeper
  Put(p, 15, 0.0f, 0.0f, 0.0f, 0.0f, 0);
  Put(p, 17, 0.0f, 0.0f, 0.0f, 0.0f, 0);
  EXPECT_EQ(lane::KeeperSlot(1, p), 15);
  EXPECT_EQ(lane::KeeperSlot(0, Players{}), lane::kNoSlot);
}

TEST_F(LaneCoreTest, LaneFollowsItsHeadingTowardPlusX) {
  // Heading 90 deg runs the lane along +x.
  lane::LaneRequest east = TestLane();
  east.heading_deg = 90.0f;
  Players along{};
  Put(along, 11, 10.0f, 0.0f);
  EXPECT_EQ(lane::ScanLane(k_, along, 3, 1, east, false, 0).count, 1);

  Players across{};
  Put(across, 11, 0.0f, 10.0f);
  EXPECT_EQ(lane::ScanLane(k_, across, 3, 1, east, false, 0).count, 0);
}

TEST_F(LaneCoreTest, WideConeUsesTheOffsetFormula) {
  // Cone 170 deg: the opponent bound is max(0.5*170 + 10, 90) = 95 deg. The player is 92.9 deg off
  // the apex bearing, so the cone floor of 90 alone would refuse him.
  lane::LaneRequest wide = TestLane();
  wide.cone_deg = 170.0f;
  Players p{};
  Put(p, 11, 10.0f, -0.5f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, wide, false, 0).count, 1);
}

TEST_F(LaneCoreTest, HeadingAddWrapsIntoOneTurn) {
  EXPECT_FLOAT_EQ(lane::HeadingAdd(k_, 350.0f, 180.0f), 170.0f);
  EXPECT_FLOAT_EQ(lane::HeadingAdd(k_, -10.0f, 0.0f), 350.0f);
}

}  // namespace
