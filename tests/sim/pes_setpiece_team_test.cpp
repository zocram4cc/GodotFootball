// PES 2021's set-piece team AI: the taker waterfall, the jog budget, the wall and
// the corner attacker count. docs/re/pes-re-setpiece-kicker.md 1,
// docs/re/pes-re-setpiece-choreography.md 5.2, 11.3, 11.5, 11.6 and 11.8.
//
// The logic tests run on synthetic constants shaped like the install's; the
// install's own are read and checked in the one gated test at the end.

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>

#include "gametypes.hpp"
#include "onthepitch/pessetpieceteam.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace {

// The measured set: 16 km/h jog, 20 s corner, 7/10/13 s own side by role class,
// 8/12 s other side, wall at 9.15 m with 0.5 m spacings.
pes::SetPieceTeamConstants Measured() {
  pes::SetPieceTeamConstants k;
  k.jogKmh = 16000.0f;
  k.secondsPerHour = 3600.0f;
  k.budgetCorner_s = 20.0f;
  k.budgetPostMan_s = 300.0f;
  k.budgetOwnBack_s = 7.0f;
  k.budgetOwnMid_s = 10.0f;
  k.budgetOwnFront_s = 13.0f;
  k.budgetOtherMarking_s = 8.0f;
  k.budgetOther_s = 12.0f;
  k.budgetThrowInSupport_s = 3.0f;
  k.budgetThrowIn_s = 2.0f;
  k.takerSlotBudgetFactor = 1.5f;
  k.fkDesignatedNear_m = 32.0f;
  k.fkDesignatedFar_m = 60.0f;
  k.fkKeeperOwnGoal_m = 28.0f;
  k.fkNearest_m = 65.0f;
  k.fkSignFlip = -1.0f;
  k.wallDistance_m = 9.15f;
  k.wallSpacing_m = 0.5f;
  k.wallNearPost_m = 1.3f;
  k.wallGoalLineBand_m = 7.5f;
  k.wallNarrowDist_m = 28.0f;
  k.wallNarrowBand_m = 5.0f;
  k.wallCentralGap_m = 3.0f;
  k.wallCentralHalfZ_m = 10.0f;
  k.wallD25_m = 25.0f;
  k.wallD29_m = 29.0f;
  k.wallD32_m = 32.0f;
  k.wallD36_m = 36.0f;
  k.wallD40_m = 40.0f;
  k.wallAngle20_deg = 20.0f;
  k.wallAngle30_deg = 30.0f;
  k.wallAngle40_deg = 40.0f;
  k.wallAngle45_deg = 45.0f;
  k.wallAngle50_deg = 50.0f;
  k.wallSpan20_deg = 20.0f;
  k.wallSpan15_deg = 15.0f;
  k.wallSpan10_deg = 10.0f;
  return k;
}

const pes::SetPieceTeamConstants k = Measured();
const pes::SetPieceTeamCounts kCounts;

// PES's FieldInfo, which every placement rule reads: half-length, goal line,
// penalty line and half-width. GF's pitch is the 105 x 68 standard these were
// authored on (gametypes.hpp), so the numbers stand in for it.
constexpr float kPitchHalfW = 52.5f;
constexpr float kGoalHalfWidth = pesGoalInnerHalfWidth;
constexpr float kPenaltyX = 36.0f;
constexpr float kPenaltyHalfZ = 20.15f;

// A player on the pitch with his slot, role, rating and slot target (x, z).
pes::SetPieceCandidate Candidate(int index, e_PlayerRole role, float suitability, float x,
                                 float z) {
  pes::SetPieceCandidate c;
  c.index = index;
  c.role = role;
  c.suitability = suitability;
  c.targetX_m = x;
  c.targetZ_m = z;
  c.onPitch = true;
  return c;
}

// A restart at (x, z) for a side attacking towards +x, with no designated kickers.
pes::FreeKickInput Restart(float x, float z) {
  pes::FreeKickInput in;
  in.x_m = x;
  in.z_m = z;
  in.dir = 1.0f;
  in.halfLength_m = kPitchHalfW;
  in.penaltyX_m = kPenaltyX;
  in.penaltyHalfZ_m = kPenaltyHalfZ;
  return in;
}

}  // namespace

// --- the free-kick chooser (FUN_1404a8ad0, docs/re/pes-re-taker-map.md 4) ---------

// Step 1: the designated kicker of the kind the distance picks. Kind 1 inside 32 m of the
// attacked goal, kind 0 inside 60 m, none beyond. Exactly 32 m and exactly 60 m fall to the
// larger kind.
TEST(SetPieceTaker, DesignatedKickerIsChosenByDistanceToTheAttackedGoal) {
  std::vector<pes::SetPieceCandidate> squad{Candidate(0, e_PlayerRole_GK, 0.1f, -50.0f, 0.0f),
                                            Candidate(3, e_PlayerRole_CM, 0.2f, 20.0f, 0.0f),
                                            Candidate(4, e_PlayerRole_CF, 0.9f, 40.0f, 0.0f),
                                            Candidate(5, e_PlayerRole_CB, 0.4f, -30.0f, 0.0f)};
  auto at = [](float x) {
    pes::FreeKickInput in = Restart(x, 0.0f);
    in.designated = {3, 4, -1};
    return in;
  };
  // Exactly 32 m out is kind 0 (slot 3); 12.5 m out is kind 1 (slot 4).
  EXPECT_EQ(pes::FreeKickTaker(squad, at(20.5f), k), 3);
  EXPECT_EQ(pes::FreeKickTaker(squad, at(40.0f), k), 4);
  // Exactly 60 m out designates nobody, so slot 3 is not the kicker there.
  EXPECT_NE(pes::FreeKickTaker(squad, at(-7.5f), k), 3);
  // A designated man who is off the pitch is passed over.
  squad[2].onPitch = false;
  EXPECT_NE(pes::FreeKickTaker(squad, at(40.0f), k), 4);
}

// Step 2: the published kicker is reused only in mode three, and never for a second kicker.
TEST(SetPieceTaker, PublishedKickerIsReusedOnlyInModeThree) {
  std::vector<pes::SetPieceCandidate> squad{Candidate(3, e_PlayerRole_CM, 0.2f, 20.0f, 0.0f),
                                            Candidate(4, e_PlayerRole_CF, 0.9f, 40.0f, 0.0f),
                                            Candidate(5, e_PlayerRole_CB, 0.4f, -30.0f, 0.0f)};
  pes::FreeKickInput in = Restart(40.0f, 0.0f);
  in.published = 3;
  in.reuseMode = true;
  EXPECT_EQ(pes::FreeKickTaker(squad, in, k), 3);
  // Outside mode three the published kicker is not reused.
  in.reuseMode = false;
  EXPECT_NE(pes::FreeKickTaker(squad, in, k), 3);
  // The second kicker never reuses the first: the same mode, excluding slot 5.
  in.reuseMode = true;
  in.excluded = 5;
  EXPECT_NE(pes::FreeKickTaker(squad, in, k), 3);
}

// Step 3: a restart near the own goal, inside the own box's width, is the keeper's, ahead of
// the loops.
TEST(SetPieceTaker, KeeperTakesAFreeKickNearHisOwnGoalAheadOfTheLoops) {
  std::vector<pes::SetPieceCandidate> squad{Candidate(0, e_PlayerRole_GK, 0.1f, -50.0f, 0.0f),
                                            Candidate(5, e_PlayerRole_CB, 0.4f, -30.0f, 0.0f),
                                            Candidate(9, e_PlayerRole_CF, 0.9f, 30.0f, 0.0f)};
  // 12.5 m from his own goal, a centre back stands 10 m away; the keeper still takes it.
  EXPECT_EQ(pes::FreeKickTaker(squad, Restart(-40.0f, 0.0f), k), 0);
  // Exactly 28 m from the own goal is outside the 28 m test, so the keeper stays out.
  EXPECT_NE(pes::FreeKickTaker(squad, Restart(-24.5f, 0.0f), k), 0);
  // Beyond the penalty-area half width (|z| >= F[7]) the keeper stays out.
  EXPECT_NE(pes::FreeKickTaker(squad, Restart(-40.0f, 25.0f), k), 0);
}

// Step 4: the own-half loop takes the nearest centre or full back within 65 m. No other role
// counts, and a back 65 m or more away does not.
TEST(SetPieceTaker, OwnHalfLoopTakesTheNearestBackWithinSixtyFiveMetres) {
  std::vector<pes::SetPieceCandidate> squad{Candidate(0, e_PlayerRole_GK, 0.1f, -50.0f, -60.0f),
                                            Candidate(5, e_PlayerRole_CB, 0.5f, -20.0f, 10.0f),
                                            Candidate(6, e_PlayerRole_CM, 0.3f, -20.0f, 25.0f),
                                            Candidate(9, e_PlayerRole_CF, 0.9f, -20.0f, 30.0f)};
  // The midfielder is 5 m away but is not a back; the centre back at 20 m is the loop's pick.
  EXPECT_EQ(pes::FreeKickTaker(squad, Restart(-20.0f, 30.0f), k), 5);
  // A back 70 m from the restart is outside the 65 m bound, so the loop does not take him.
  std::vector<pes::SetPieceCandidate> far{Candidate(5, e_PlayerRole_CB, 0.5f, -20.0f, -40.0f),
                                          Candidate(9, e_PlayerRole_CF, 0.9f, -20.0f, 30.0f)};
  EXPECT_NE(pes::FreeKickTaker(far, Restart(-20.0f, 30.0f), k), 5);
}

// Step 5: more than 60 m from the attacked goal, the nearest outfielder who is not a
// striker, within 65 m. The keeper is in this loop too.
TEST(SetPieceTaker, FarLoopTakesTheNearestNonStrikerFromFarOut) {
  std::vector<pes::SetPieceCandidate> squad{Candidate(0, e_PlayerRole_GK, 0.1f, -50.0f, 0.0f),
                                            Candidate(8, e_PlayerRole_AM, 0.3f, 10.0f, 30.0f),
                                            Candidate(9, e_PlayerRole_CF, 0.9f, 5.0f, 40.0f)};
  // 66 m from the attacked goal. The striker is 5 m away but excluded; the attacking mid is 14 m.
  EXPECT_EQ(pes::FreeKickTaker(squad, Restart(0.0f, 40.0f), k), 8);
}

// Step 6: the rating fallback takes the highest rating, the first slot on a tie. The keeper is
// in it only for kind 0, and the first kicker is never in it.
TEST(SetPieceTaker, RatingFallbackKeepsTheKeeperForKindZeroAndTheFirstOnATie) {
  std::vector<pes::SetPieceCandidate> squad{Candidate(0, e_PlayerRole_GK, 1.0f, -50.0f, -60.0f),
                                            Candidate(9, e_PlayerRole_CF, 0.9f, 20.0f, 0.0f),
                                            Candidate(10, e_PlayerRole_CF, 0.9f, 20.0f, 0.0f)};
  // More than 60 m from the goal runs the fallback at kind 0, where the keeper's 1.0 wins.
  EXPECT_EQ(pes::FreeKickTaker(squad, Restart(-20.0f, 30.0f), k), 0);
  // Within 32 m (kind 1) the keeper is out; the two strikers tie at 0.9 and the first wins.
  EXPECT_EQ(pes::FreeKickTaker(squad, Restart(40.0f, 0.0f), k), 9);
  // As the second kicker the first kicker (slot 9) is out, and the other striker takes it.
  pes::FreeKickInput second = Restart(40.0f, 0.0f);
  second.excluded = 9;
  EXPECT_EQ(pes::FreeKickTaker(squad, second, k), 10);
}

// The second kicker: designated by kind 2 whatever the distance, and never the first kicker.
TEST(SetPieceTaker, SecondKickerIsDesignatedWhateverTheDistanceAndNeverTheFirst) {
  std::vector<pes::SetPieceCandidate> squad{Candidate(0, e_PlayerRole_GK, 0.1f, -50.0f, 0.0f),
                                            Candidate(3, e_PlayerRole_CM, 0.5f, 20.0f, 0.0f),
                                            Candidate(5, e_PlayerRole_CB, 0.4f, -30.0f, 0.0f),
                                            Candidate(9, e_PlayerRole_CF, 0.9f, 40.0f, 0.0f)};
  // The first kicker is slot 9, and the designated second names him: he is passed over.
  pes::FreeKickInput in = Restart(40.0f, 0.0f);
  in.designated = {-1, -1, 9};
  in.excluded = 9;
  EXPECT_NE(pes::FreeKickTaker(squad, in, k), 9);
  // Kind 2 ignores distance: 92.5 m from the goal, the designated second is still slot 3.
  in = Restart(-40.0f, 0.0f);
  in.designated = {-1, -1, 3};
  in.excluded = 9;
  EXPECT_EQ(pes::FreeKickTaker(squad, in, k), 3);
}

// FUN_1404a7d60 case 5: the first kicker is picked again when the published one is invalid;
// the second only for FK types 1..3, and then when either kicker is invalid. Type 0 leaves
// the second at none; types past 3 leave it alone.
TEST(SetPieceTaker, RepickGateFollowsTheSelectorsCaseFive) {
  pes::FreeKickRepick g = pes::FreeKickRepickGate(true, true, 1);
  EXPECT_FALSE(g.first);
  EXPECT_FALSE(g.second);
  g = pes::FreeKickRepickGate(true, false, 2);
  EXPECT_FALSE(g.first);
  EXPECT_TRUE(g.second);
  g = pes::FreeKickRepickGate(false, true, 3);
  EXPECT_TRUE(g.first);
  EXPECT_TRUE(g.second);
  g = pes::FreeKickRepickGate(true, true, 0);
  EXPECT_TRUE(g.secondNone);
  EXPECT_FALSE(g.second);
  g = pes::FreeKickRepickGate(true, false, 4);
  EXPECT_FALSE(g.second);
  EXPECT_FALSE(g.secondNone);
}


// --- the jog budget (pes-re-setpiece-choreography.md 11.3) -------------------

TEST(SetPieceJog, ACloseMarkIsReachedExactly) {
  // 10 m at 16 km/h is 2.25 s, so a 7 s budget covers it and the clamp is 1.
  EXPECT_FLOAT_EQ(pes::JogFraction(k.budgetOwnBack_s, 10.0f, k), 1.0f);
  // 31.1 m at 16 km/h is exactly 7 s: the boundary.
  EXPECT_NEAR(pes::JogFraction(k.budgetOwnBack_s, 31.1f, k), 1.0f, 0.02f);
}

TEST(SetPieceJog, AFarMarkIsOnlyPartiallyCovered) {
  // 60 m in a 12 s budget is 53.3 m: 88.9% of the way.
  const float fraction = pes::JogFraction(k.budgetOther_s, 60.0f, k);
  EXPECT_NEAR(fraction * 60.0f, 4.444f * k.budgetOther_s, 1.0f);
  EXPECT_GT(fraction, 0.85f);
  EXPECT_LT(fraction, 0.95f);
  // A 2 s throw-in budget covers 8.9 m of a 40 m reform.
  EXPECT_NEAR(pes::JogFraction(k.budgetThrowIn_s, 40.0f, k) * 40.0f, 8.89f, 0.05f);
}

TEST(SetPieceJog, NoBudgetMeansNoLimit) {
  EXPECT_FLOAT_EQ(pes::JogFraction(-1.0f, 90.0f, k), 1.0f);
  EXPECT_FLOAT_EQ(pes::JogFraction(-1.0f, 0.0f, k), 1.0f);
}

TEST(SetPieceJog, BudgetsFollowTheKindAndTheRole) {
  // The kicker, a wall man, the own keeper and a post man are unlimited.
  EXPECT_LT(pes::PlacementBudget_s(k, (int)e_SetPiece_FreeKick, true, e_PlayerRole_CF, true, false,
                                   false, false, false, false),
            0.0f);
  EXPECT_LT(pes::PlacementBudget_s(k, (int)e_SetPiece_FreeKick, false, e_PlayerRole_CM, false, false,
                                   true, false, false, false),
            0.0f);
  EXPECT_LT(pes::PlacementBudget_s(k, (int)e_SetPiece_Corner, true, e_PlayerRole_CM, false, false,
                                   false, false, true, false),
            0.0f);
  // A corner gives everybody the same 20 s.
  EXPECT_FLOAT_EQ(pes::PlacementBudget_s(k, (int)e_SetPiece_Corner, true, e_PlayerRole_CM, false,
                                         false, false, false, false, false),
                  k.budgetCorner_s);
  EXPECT_FLOAT_EQ(pes::PlacementBudget_s(k, (int)e_SetPiece_Corner, false, e_PlayerRole_CF, false,
                                         false, false, false, false, false),
                  k.budgetCorner_s);
  // A goal kick: back class 7 s, mid 10 s, front 13 s, the other side 12 s.
  EXPECT_FLOAT_EQ(pes::PlacementBudget_s(k, (int)e_SetPiece_GoalKick, true, e_PlayerRole_CB, false,
                                         false, false, false, false, false),
                  k.budgetOwnBack_s);
  EXPECT_FLOAT_EQ(pes::PlacementBudget_s(k, (int)e_SetPiece_GoalKick, true, e_PlayerRole_CM, false,
                                         false, false, false, false, false),
                  k.budgetOwnMid_s);
  EXPECT_FLOAT_EQ(pes::PlacementBudget_s(k, (int)e_SetPiece_GoalKick, true, e_PlayerRole_CF, false,
                                         false, false, false, false, false),
                  k.budgetOwnFront_s);
  EXPECT_FLOAT_EQ(pes::PlacementBudget_s(k, (int)e_SetPiece_GoalKick, false, e_PlayerRole_CF, false,
                                         false, false, false, false, false),
                  k.budgetOther_s);
  // A kick-off and a penalty are re-formed, not jogged to.
  EXPECT_LT(pes::PlacementBudget_s(k, (int)e_SetPiece_KickOff, true, e_PlayerRole_CM, false, false,
                                   false, false, false, false),
            0.0f);
}

// --- the wall (pes-re-setpiece-choreography.md 5.2, 5.3, 11.4, 11.8) ----------

TEST(SetPieceWall, SizeGrowsTowardsTheGoalAndNarrowsWithTheAngle) {
  // The live free kick: 35.3 m out, 60 degrees off the centre line -> 1 man.
  EXPECT_EQ(pes::WallSize(35.3f, 60.4f, 35.1f, -30.7f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ,
                          k, kCounts),
            1);
  // Straight in front of the box, close: the maximum.
  EXPECT_EQ(pes::WallSize(20.0f, 5.0f, 30.0f, 1.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            kCounts.wallCentralCount);
  // 22 m out and 10 degrees off centre: 4 + int(2*1) = 6.
  EXPECT_EQ(pes::WallSize(22.0f, 10.0f, 30.0f, 3.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            6);
  // 22 m out and 30 degrees off centre: 4 + int(2*0.5) = 5.
  EXPECT_EQ(pes::WallSize(22.0f, 30.0f, 30.0f, 12.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            5);
  // 22 m out and 45 degrees off centre: the ramp is empty, 4 men.
  EXPECT_EQ(pes::WallSize(22.0f, 45.0f, 30.0f, 17.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            4);
  // 35.3 m out and 40 degrees off centre: 1 + int(1) = 2. The live free kick was
  // 35.3 m out but 60 degrees off, which is past the 50-degree ramp and gives one.
  EXPECT_EQ(pes::WallSize(35.3f, 40.0f, 30.0f, 23.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            2);
  EXPECT_EQ(pes::WallSize(35.3f, 55.0f, 30.0f, 27.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            1);
  // Beyond 40 m nothing is built.
  EXPECT_EQ(pes::WallSize(41.0f, 5.0f, 30.0f, 3.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            0);
  // Within 7.5 m of the goal line: one man, whatever the angle.
  EXPECT_EQ(pes::WallSize(6.0f, 50.0f, 48.0f, 2.0f, 1.0f, kPitchHalfW, kPenaltyX, kPenaltyHalfZ, k,
                          kCounts),
            1);
}

TEST(SetPieceWall, MenStandNineFifteenMetresOutAndHalfAMetreApart) {
  // PES writes the centre 9.15 m from the ball toward the aim point and then
  // spreads the members 0.5 m apart along the wall line, the first on the centre.
  EXPECT_FLOAT_EQ(k.wallDistance_m, 9.15f);
  EXPECT_FLOAT_EQ(k.wallSpacing_m, 0.5f);
  // The centre for a one-man wall is the goal centre, so the wall man stands
  // exactly the ring distance out along the ball's bearing to it.
  const float ballX = 30.0f, ballZ = -20.0f, goalX = kPitchHalfW;
  const float dx = goalX - ballX, dz = 0.0f - ballZ;
  const float len = std::sqrt(dx * dx + dz * dz);
  const float centreX = ballX + dx / len * k.wallDistance_m;
  const float centreZ = ballZ + dz / len * k.wallDistance_m;
  EXPECT_NEAR(std::sqrt((centreX - ballX) * (centreX - ballX) + (centreZ - ballZ) *
                                                                     (centreZ - ballZ)),
              k.wallDistance_m, 1e-4f);
  // The second man is exactly one spacing further along the wall line, which is
  // perpendicular to the ball's bearing: the wall is 0.5 m apart, not 1 m and
  // not a line of men on the ring.
  const float perpX = -dz / len * k.wallSpacing_m, perpZ = dx / len * k.wallSpacing_m;
  const float secondX = centreX + perpX, secondZ = centreZ + perpZ;
  const float toSecond = std::sqrt((secondX - centreX) * (secondX - centreX) +
                                   (secondZ - centreZ) * (secondZ - centreZ));
  EXPECT_NEAR(toSecond, k.wallSpacing_m, 1e-4f);
  // ...and he is a chord out from the ring, because the wall is a straight line
  // through the centre, not an arc around the ball.
  EXPECT_NEAR(std::sqrt((secondX - ballX) * (secondX - ballX) +
                        (secondZ - ballZ) * (secondZ - ballZ)),
              std::sqrt(k.wallDistance_m * k.wallDistance_m + k.wallSpacing_m * k.wallSpacing_m),
              1e-4f);
}

TEST(SetPieceWall, JumpRuleIsDeterministicFrameArithmetic) {
  // FUN_140525ec0. No RNG draw anywhere: the same frames give the same decision.
  const int wallSize = 3;
  pes::WallJumpState a = pes::WallJumpStateStart(wallSize);
  pes::WallJumpState b = pes::WallJumpStateStart(wallSize);
  EXPECT_EQ(pes::WallAction(wallSize, 2, 11, 40, a), pes::WallAction(wallSize, 2, 11, 40, b));
  // The not-jump slots start at (n-1)/2 = 1 for a three-man wall.
  EXPECT_EQ(pes::WallJumpStateStart(wallSize).notJumpLeft, 1);
  EXPECT_EQ(pes::WallJumpStateStart(1).notJumpLeft, 0);
  EXPECT_EQ(pes::WallJumpStateStart(6).notJumpLeft, 2);
  // Once they run out, everyone jumps.
  pes::WallJumpState s = pes::WallJumpStateStart(wallSize);
  s.notJumpLeft = 0;
  EXPECT_EQ(pes::WallAction(wallSize, 1, 10, 40, s), 0);
  // A one-man wall jumps at once: (1-1)/2 = 0 slots.
  pes::WallJumpState one = pes::WallJumpStateStart(1);
  EXPECT_EQ(pes::WallAction(1, 9, 9, 40, one), 0);
  // A whole wall on one frame, threaded in slot order exactly as PES walks it:
  // slot 0 rolls 81 and jumps, slot 1 rolls 20 and does not, slot 2 has no
  // not-jump slot left and jumps. The arithmetic is the frame counter's, so the
  // same frame always produces this triple.
  pes::WallJumpState wall = pes::WallJumpStateStart(3);
  EXPECT_EQ(pes::WallAction(3, 0, 11, 41, wall), 0);  // r = 81  > 24
  EXPECT_EQ(pes::WallAction(3, 1, 12, 41, wall), 1);  // r = 20 <= 24
  EXPECT_EQ(pes::WallAction(3, 2, 13, 41, wall), 0);  // no slots left
  // The roll mixes the slot and the player index, so the same slot on a
  // different index is a different roll: nothing about it is a fixed order.
  pes::WallJumpState other = pes::WallJumpStateStart(3);
  EXPECT_EQ(pes::WallAction(3, 1, 1, 41, other), 0);  // r = 80 > 24
}

TEST(SetPieceWall, AtMostOneManPresses) {
  // Scan the wall's slot order over a window of frames and count the presses:
  // the rule allows exactly one, whatever the frames do.
  const int wallSize = 5;
  for (int frame = 0; frame < 300; frame++) {
    pes::WallJumpState state = pes::WallJumpStateStart(wallSize);
    int presses = 0, jumpers = 0;
    for (int slot = 0; slot < wallSize; slot++) {
      const int action = pes::WallAction(wallSize, slot, 11 + slot, frame, state);
      if (action == 2) presses++;
      if (action == 0) jumpers++;
    }
    EXPECT_LE(presses, 1) << "frame " << frame;
    EXPECT_GE(jumpers, 1) << "frame " << frame;
  }
}

// --- the corner (pes-re-setpiece-choreography.md 11.5, 11.6) ----------------

TEST(SetPieceCorner, BoxAttackerCountFollowsTheAttackLevel) {
  EXPECT_EQ(pes::BoxAttackerCount(pes::AttackLevel(0), kCounts), 4);
  EXPECT_EQ(pes::BoxAttackerCount(pes::AttackLevel(1), kCounts), 5);
  EXPECT_EQ(pes::BoxAttackerCount(pes::AttackLevel(2), kCounts), 6);
  EXPECT_EQ(pes::BoxAttackerCount(pes::AttackLevel(3), kCounts), 6);
  EXPECT_EQ(pes::BoxAttackerCount(pes::AttackLevel(4), kCounts), 6);
  // GF's mentality rungs map one-to-one onto PES's five levels.
  EXPECT_EQ(pes::AttackLevel(4), 4);
  EXPECT_EQ(pes::AttackLevel(0), 0);
  // And the count is clamped: a rung outside the five is not a bigger box.
  EXPECT_EQ(pes::BoxAttackerCount(pes::AttackLevel(9), kCounts), 6);
  EXPECT_EQ(pes::BoxAttackerCount(pes::AttackLevel(-3), kCounts), 4);
}

// The counts come out of the install's exe, not transcribed into the source.
TEST(SetPieceTeamConstants, ReadFromTheExe) {
  if (const char* root = std::getenv("PES21_ROOT")) {
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    if (!exe) GTEST_SKIP() << "PES2021.exe unreadable: " << err;
    auto c = pes::LoadSetPieceTeamConstants(*exe, &err);
    ASSERT_TRUE(c.has_value()) << err;
    // The measured values: 16 km/h, 20 s corner, 7/10/13 and 8/12, 9.15 m, 0.5 m.
    EXPECT_FLOAT_EQ(c->jogKmh, 16000.0f);
    EXPECT_FLOAT_EQ(c->secondsPerHour, 3600.0f);
    EXPECT_FLOAT_EQ(c->budgetCorner_s, 20.0f);
    EXPECT_FLOAT_EQ(c->budgetOwnBack_s, 7.0f);
    EXPECT_FLOAT_EQ(c->budgetOwnMid_s, 10.0f);
    EXPECT_FLOAT_EQ(c->budgetOwnFront_s, 13.0f);
    EXPECT_FLOAT_EQ(c->budgetOtherMarking_s, 8.0f);
    EXPECT_FLOAT_EQ(c->budgetOther_s, 12.0f);
    EXPECT_FLOAT_EQ(c->wallDistance_m, 9.15f);
    EXPECT_FLOAT_EQ(c->wallSpacing_m, 0.5f);
    EXPECT_FLOAT_EQ(c->wallNearPost_m, 1.3f);
    // The free-kick chooser's distances and the keeper's sign flip (FUN_1404a8ad0).
    EXPECT_FLOAT_EQ(c->fkDesignatedNear_m, 32.0f);
    EXPECT_FLOAT_EQ(c->fkDesignatedFar_m, 60.0f);
    EXPECT_FLOAT_EQ(c->fkKeeperOwnGoal_m, 28.0f);
    EXPECT_FLOAT_EQ(c->fkNearest_m, 65.0f);
    EXPECT_FLOAT_EQ(c->fkSignFlip, -1.0f);
  } else {
    GTEST_SKIP() << "PES21_ROOT not set: the constants are read from the install's exe";
  }
}

// The seamless throw-in taker (FUN_14044e230, docs/re/pes-re-throwin-choreography.md 2.2,
// EV-TI-0006). The ranks are the AI's ball-arrival order, which is not sorted by arrival.
namespace {
constexpr float kFps = 54.0f;  // the logic rate every frame count below is in
}  // namespace

TEST(SeamlessThrowInTaker, TheFastestOfTheFirstFourRanksTakesIt) {
  // Rank 2 (slot 7) arrives 30 frames before rank 0. Rank 4 (slot 1) is faster still, but it is
  // not among the first four and cannot take it.
  const std::vector<pes::ArrivalRank> ranked{{3, false, 40.0f},
                                             {5, false, 35.0f},
                                             {7, false, 10.0f},
                                             {9, false, 25.0f},
                                             {1, false, 2.0f}};
  EXPECT_EQ(pes::SeamlessThrowInTaker(ranked, kFps, pes::SetPieceTeamCounts()), 7);
}

TEST(SeamlessThrowInTaker, ALaterRankMustBeatTheBiasToTakeIt) {
  // The bias is fps / 3 frames. A rank that arrives within the bias of the candidate keeps him.
  const float bias = kFps / static_cast<float>(pes::SetPieceTeamCounts().seamlessBiasDivisor);
  const std::vector<pes::ArrivalRank> held{
      {3, false, 30.0f}, {5, false, 60.0f}, {7, false, 30.0f - bias + 1.0f}};
  EXPECT_EQ(pes::SeamlessThrowInTaker(held, kFps, pes::SetPieceTeamCounts()), 3);
  const std::vector<pes::ArrivalRank> beaten{
      {3, false, 30.0f}, {5, false, 60.0f}, {7, false, 30.0f - bias - 1.0f}};
  EXPECT_EQ(pes::SeamlessThrowInTaker(beaten, kFps, pes::SetPieceTeamCounts()), 7);
}

TEST(SeamlessThrowInTaker, AKeeperAtRankZeroGivesWayToRankOne) {
  const std::vector<pes::ArrivalRank> ranked{
      {3, true, 5.0f}, {5, false, 35.0f}, {7, false, 40.0f}};
  EXPECT_EQ(pes::SeamlessThrowInTaker(ranked, kFps, pes::SetPieceTeamCounts()), 5);
}

// PES's seamless candidate must reach the ball (FUN_140444f00, docs/re/pes-re-throwin-choreography.md
// 3.1, EV-TI-0007): a man more than 20 m from the ball is not eligible, however his arrival time
// reads. The arrival time alone cannot tell a near man from a far one when the stat is clamped.
TEST(SeamlessThrowInTaker, AManBeyondTheReachIsNotTakenWhateverHisArrival) {
  // Rank 0 looks fastest on time but stands 100 m from the ball. The man at 5 m is the taker.
  const std::vector<pes::ArrivalRank> ranked{{1, false, 54.0f, 116.0f},
                                             {2, false, 54.0f, 5.0f}};
  EXPECT_EQ(pes::SeamlessThrowInTaker(ranked, kFps, pes::SetPieceTeamCounts()), 2);
}
