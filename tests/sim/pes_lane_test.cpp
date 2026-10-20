// PES's pass-lane blocker test (onthepitch/peslane.hpp): hand-placed players against the lane
// geometry, with constants read from the install. Lane values are the one-touch G11 lane (spec
// docs/re/pes-re-lane-core.md 6.2): length 15 m, cone 60 deg, radius offset 2 m, origin (0, 0).
// Slots: home 0..10 (side 0), away 11..21 (side 1).

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "gametypes.hpp"
#include "onthepitch/peslane.hpp"
#include "pes/exe.hpp"

namespace lane = pes::lane;

namespace {

using Players = std::array<lane::LanePlayer, lane::kSlots>;

lane::LaneRequest TestLane() { return {{0.0f, 0.0f}, 180.0f, 15.0f, 60.0f, 2.0f}; }

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

TEST_F(LaneCoreTest, PathAlongTheLaneLineIsNotACrossing) {
  // The path runs 2e-9 m across the lane line, so the crossing's denominator is under the floor.
  Players p{};
  Put(p, 11, -1e-9f, -10.0f, 2e-9f, 30.0f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), false, 1).count, 0);
}

TEST_F(LaneCoreTest, AnnulusReachesRadPlusLengthFromTheApex) {
  // The apex is 2 m behind the origin, so a man at 14.5 m is 16.5 m from it: inside rad + len = 17.
  Players p{};
  Put(p, 11, 0.0f, 14.5f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), false, 0).count, 1);
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

TEST_F(LaneCoreTest, LaneAngleWrapsAcrossNorth) {
  // Heading 350 deg and a man 10 m out at PES bearing 10 deg (GF y is -PES z): 20 deg apart, so he is in the cone.
  // Without the fold into [-180, 180] the difference is 340 deg and he is refused.
  lane::LaneRequest north = TestLane();
  north.heading_deg = 350.0f;
  Players p{};
  Put(p, 11, 1.7364818f, -9.8480775f);  // 10 m at PES bearing 10 deg: (sin, -cos) in GF
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, north, false, 0).count, 1);
}

TEST_F(LaneCoreTest, PathThatOnlyTouchesTheLaneLineIsNotACrossing) {
  // Slot 11 starts on the lane line (x = 0) and runs away from it: the segment touches, not crosses.
  Players p{};
  Put(p, 11, 0.0f, 8.0f, 20.0f, 0.0f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), false, 1).count, 0);
}

TEST_F(LaneCoreTest, NearestIsMeasuredFromThePredictedPosition) {
  // Now slot 12 (9 m) is nearer than slot 11 (12 m). After one frame slot 11 is at 8 m.
  Players p{};
  Put(p, 11, 0.0f, 12.0f, 0.0f, -4.0f);
  Put(p, 12, 0.0f, 9.0f);
  EXPECT_EQ(lane::ScanLane(k_, p, 3, 1, TestLane(), false, 1).nearest, 11);
}

TEST_F(LaneCoreTest, AnnulusBoundsAreExclusive) {
  // The apex sits at (0, -2). A man exactly 2 m from it (the origin) and exactly 17 m from it
  // (0, 15) is outside the annulus: both bounds are strict.
  Players inner{};
  Put(inner, 11, 0.0f, 0.0f);
  EXPECT_EQ(lane::ScanLane(k_, inner, 3, 1, TestLane(), false, 0).count, 0);
  Players outer{};
  Put(outer, 11, 0.0f, 15.0f);
  EXPECT_EQ(lane::ScanLane(k_, outer, 3, 1, TestLane(), false, 0).count, 0);
}

TEST_F(LaneCoreTest, HeadingAddWrapsIntoOneTurn) {
  EXPECT_FLOAT_EQ(lane::HeadingAdd(k_, 350.0f, 180.0f), 170.0f);
  EXPECT_FLOAT_EQ(lane::HeadingAdd(k_, -10.0f, 0.0f), 350.0f);
}

// The Cross unit's lane (0x142129590, the core call at 0x142129804): the aim is the spot on the
// pitch's centre line at the attacked goal's goal-area line, the heading is the bearing from the
// ball to that spot, and the origin is the ball advanced one metre along it.
TEST_F(LaneCoreTest, CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine) {
  // The ball on the centre line 20 m from the centre, attacking +x: the aim is at
  // (pesGoalAreaLineX, 0), so the bearing is due +x (90 deg) and the origin is one metre along it.
  const lane::LaneRequest l = lane::CrossLane(k_, {0.0f, 0.0f}, pesGoalAreaLineX);
  EXPECT_FLOAT_EQ(l.heading_deg, 90.0f);
  EXPECT_FLOAT_EQ(l.origin.x, 1.0f);
  EXPECT_FLOAT_EQ(l.origin.z, 0.0f);
  EXPECT_FLOAT_EQ(l.len_m, 5.0f);
  EXPECT_FLOAT_EQ(l.cone_deg, 30.0f);
  EXPECT_FLOAT_EQ(l.rad_m, 2.0f);
}

TEST_F(LaneCoreTest, CrossLaneHeadingPointsAtTheAimAndAdvancesOneMetre) {
  // The ball at (-40, 0) attacking -x: the aim is at (-pesGoalAreaLineX, 0), the bearing is due
  // -x (270 deg) and the origin sits one metre towards it.
  const lane::LaneRequest l = lane::CrossLane(k_, {-40.0f, 0.0f}, -pesGoalAreaLineX);
  EXPECT_FLOAT_EQ(l.heading_deg, 270.0f);
  EXPECT_FLOAT_EQ(l.origin.x, -41.0f);
  EXPECT_FLOAT_EQ(l.origin.z, 0.0f);
}

TEST_F(LaneCoreTest, CrossLaneScansTheOpponentsAndCountsThem) {
  // The carrier is the home side (slot 3), so the Cross lane scans the away side (11..21).
  // A ball on the centre line 20 m out attacking +x, and one opponent standing on the lane.
  const lane::LaneRequest l = lane::CrossLane(k_, {20.0f, 0.0f}, pesGoalAreaLineX);
  Players clear{};
  EXPECT_TRUE(lane::LaneFree(k_, clear, 3, l, /*keepKeeper*/ false, /*horizonFrames*/ 0));

  Players blocked{};
  // The lane runs from (21, 0) towards +x with a 30 deg cone and a 2 m apex offset, so its
  // annulus reaches 2..7 m from (23, 0). An opponent at (25, 0) is inside it.
  Put(blocked, 11, 25.0f, 0.0f);
  const lane::LaneScan s = lane::ScanLane(k_, blocked, 3, 1, l, /*keepKeeper*/ false, 0);
  EXPECT_EQ(s.count, 1);
  EXPECT_EQ(s.nearest, 11);
  EXPECT_FALSE(lane::LaneFree(k_, blocked, 3, l, false, 0));
}

}  // namespace
