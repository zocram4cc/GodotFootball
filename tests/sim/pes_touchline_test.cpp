// The touchline judge on the PES path (referee.cpp). PES judges the outer edge of the touchline,
// F1 = pesFieldHalfWidth (docs/re/pes-re-fieldinfo.md, F1). A ball is out when it crosses that edge
// from inside. A throw-in's ball is placed over the line at its spot, so it is not out at the spot:
// it is out again once it has moved one ball radius beyond that spot. A ball that stays at rest past
// the touchline, while play is live, must be out on the next tick.

#include <gtest/gtest.h>

#include "gametypes.hpp"
#include "onthepitch/setpiecelaws.hpp"

namespace {
// A stand-in radius: the judge takes the ball radius as an input, the PES value comes from the install.
constexpr float kBallRadius_m = 0.11f;
// How far past the edge a crossing ball is, for the cases below.
constexpr float kPastTheEdge_m = 0.1f;
}  // namespace

// The stall, seed 7: a throw-in's spot at y -34.31 over the line, then the ball at rest at y -35.30.
TEST(TouchlineJudge, ABallAtRestPastTheTouchlineIsOutOnTheNextTick) {
  EXPECT_TRUE(SetPieceLaws::TouchlineOut(-35.30f, pesFieldHalfWidth, /*wasOver=*/true,
                                         /*spotY=*/-34.31f, kBallRadius_m));
}

TEST(TouchlineJudge, ABallInsideTheTouchlineIsNotOut) {
  EXPECT_FALSE(SetPieceLaws::TouchlineOut(33.0f, pesFieldHalfWidth, false, 0.0f, kBallRadius_m));
}

TEST(TouchlineJudge, ACrossingFromInsideIsOut) {
  EXPECT_TRUE(SetPieceLaws::TouchlineOut(pesFieldHalfWidth + kPastTheEdge_m, pesFieldHalfWidth,
                                         /*wasOver=*/false, pesFieldHalfWidth, kBallRadius_m));
}

TEST(TouchlineJudge, AThrowInBallLyingAtItsSpotIsNotOut) {
  const float spotY = -(pesFieldHalfWidth + kPastTheEdge_m);
  EXPECT_FALSE(SetPieceLaws::TouchlineOut(spotY, pesFieldHalfWidth, /*wasOver=*/true, spotY,
                                          kBallRadius_m));
}

TEST(TouchlineJudge, ABallLessThanARadiusBeyondTheSpotIsStillAtTheSpot) {
  const float spotY = -(pesFieldHalfWidth + kPastTheEdge_m);
  EXPECT_FALSE(SetPieceLaws::TouchlineOut(spotY - 0.5f * kBallRadius_m, pesFieldHalfWidth,
                                          /*wasOver=*/true, spotY, kBallRadius_m));
}
