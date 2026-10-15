// PES 2021's FieldInfo table (gametypes.hpp; docs/re/pes-re-fieldinfo.md §1).
// The record is VM-built at match start and no install VA holds it, so these
// tests check the table's own relations: the PES layout of the pitch and goal,
// which the 18 values must satisfy.
#include <gtest/gtest.h>

#include "gametypes.hpp"

namespace {

constexpr float kTolerance_m = 1e-5f;
// Law 1 dimensions the PES layout is built from (metres).
constexpr float kGoalAreaDepth_m = 5.5f;             // goal area: past the goal line and the posts
constexpr float kPenaltySpotFromGoalLine_m = 11.0f;  // the penalty mark
constexpr float kCentreCircleRadius_m = 9.15f;       // centre circle and the penalty arc

}  // namespace

TEST(FieldInfo, OuterEdgesAreTheLineCentresPlusHalfALine) {
  EXPECT_NEAR(pesFieldHalfLength, pitchHalfW + lineHalfW, kTolerance_m);
  EXPECT_NEAR(pesFieldHalfWidth, pitchHalfH + lineHalfW, kTolerance_m);
}

TEST(FieldInfo, GoalAreaLinesLieFiveAndAHalfMetresBeyondTheGoalLineAndPosts) {
  EXPECT_NEAR(pesGoalAreaLineX, pesFieldHalfLength - kGoalAreaDepth_m, kTolerance_m);
  EXPECT_NEAR(pesGoalAreaHalfWidth, pesGoalInnerHalfWidth + kGoalAreaDepth_m, kTolerance_m);
}

TEST(FieldInfo, PenaltySpotIsElevenMetresFromTheGoalLine) {
  EXPECT_NEAR(pesPenaltySpotX, pesFieldHalfLength - kPenaltySpotFromGoalLine_m, kTolerance_m);
}

TEST(FieldInfo, NetHeightIsTheCrossbarTop) {
  EXPECT_NEAR(pesNetHeight, pesGoalInnerHeight + pesCrossbarThickness * 0.5f, kTolerance_m);
}

TEST(FieldInfo, CentreCircleRadiusIsLawOne) {
  EXPECT_NEAR(pesCircleRadius, kCentreCircleRadius_m, kTolerance_m);
}

TEST(FieldInfo, NetBacksLieBehindTheGoalLineInOrder) {
  EXPECT_LT(pesFieldHalfLength, pesNetRoofBackX);
  EXPECT_LT(pesNetRoofBackX, pesNetFloorBackX);
}

TEST(FieldInfo, PitchMarksNestFromTheGoalLineInwards) {
  EXPECT_GT(pesFieldHalfLength, pesGoalAreaLineX);
  EXPECT_GT(pesGoalAreaLineX, pesPenaltySpotX);
  EXPECT_GT(pesPenaltySpotX, pesPenaltyAreaLineX);
  EXPECT_GT(pesFieldHalfWidth, pesPenaltyAreaHalfWidth);
  EXPECT_GT(pesPenaltyAreaHalfWidth, pesGoalAreaHalfWidth);
  EXPECT_GT(pesGoalAreaHalfWidth, pesGoalInnerHalfWidth);
}
