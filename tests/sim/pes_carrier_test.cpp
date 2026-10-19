// PES 2021's ball-carrier decision layer (docs/re/pes-re-carrier-ai.md,
// docs/re/pes-re-attack-decisions.md 2). Synthetic constants exercise the
// shoot zone and its gates, the openness formula's class ladder, PassForward's
// three tiers and the plan-order roll; one install test checks the loader
// against the user's own PES2021.exe through relations, not pinned numbers.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "gametypes.hpp"
#include "onthepitch/pescarrier.hpp"

namespace {

// The values the user's install carries for these fields, used both as the
// synthetic set and as the shape the install test checks. PES's own: report 4.1
// for the zone ranges, 4.3 for the openness ladder, 4.4 for the plan roll.
pes::CarrierConstants Synthetic() {
  pes::CarrierConstants k{};
  k.chooserGate_deg = 30.0f;
  k.chooserTurn_deg = 60.0f;
  k.chooserSideAngle_deg = 40.0f;
  k.chooserPressureScale_deg = 20.0f;
  k.chooserSpeedLow_kmh = 10.0f;
  k.chooserSpeedMid_kmh = 15.0f;
  k.chooserRoom_m = 12.0f;
  k.shootR0_m = 11.0f;
  k.shootR1_m = 12.0f;
  k.nearCentral_m = 18.0f;
  k.farCentral_m = 24.0f;
  k.nearSide_m = 16.0f;
  k.farSide_m = 22.0f;
  k.centralHalfAngle_deg = 15.0f;
  k.sideAngle_deg = 40.0f;
  k.finishingBoost_m = 3.0f;
  k.longShotBoost_m = 3.0f;
  k.finishingFloor = 70.0f;
  k.finishingTop = 99.0f;
  k.finishingSpan = 29.0f;
  k.justShootDist_m = 20.0f;
  k.justShootAngle_deg = 40.0f;
  k.firstTimeDistSq = 400.0f;
  k.firstTimeNearDistSq = 225.0f;
  k.firstTimeIncoming_deg = 120.0f;
  k.firstTimeBody_deg = 70.0f;
  k.firstTimePathGate_m = 7.0f;
  k.aboveHeadFactor = 0.8f;
  k.statDivisor = 100.0f;
  k.movingBallFloor_kmh = 10.0f;
  k.movingBallSpan_kmh = 50.0f;
  k.kmhPerMs = 3.6f;
  k.distFloor_m = 10.0f;
  k.distSpan_m = 20.0f;
  k.ballAngleHigh_deg = 150.0f;
  k.ballAngleSpan_deg = 60.0f;
  k.oneTouchGain = 0.5f;
  k.runFloor_kmh = 16.0f;
  k.runSpan_kmh = 12.0f;
  k.turnFloor_deg = 60.0f;
  k.turnSpan_deg = 70.0f;
  k.turnTop_deg = 130.0f;
  k.highFloor_m = 0.108685933f;
  k.highSpan_m = 0.891314089f;
  k.diffMovingBallWeight = 2.0f;
  k.diffRunWeight = 1.5f;
  k.diffTurnWeight = 1.5f;
  k.lowPassFloor = 80.0f;
  k.lowPassTop = 99.0f;
  k.lowPassSpan = 19.0f;
  k.lowPassBase = 0.7f;
  k.lowPassGain = 0.3f;
  k.forwardAngle_deg = 60.0f;
  k.squareAngle_deg = 110.0f;
  k.openNear_m = 1.0f;
  k.openSpan_m = 29.0f;
  k.openInFront_deg = 90.0f;
  k.openBlockMargin_m = 1.5f;
  k.openLaneMargin_m = 1.0f;
  k.openAlongMin_m = 0.2f;
  k.openAlongGain = 0.6f;
  k.openAlongPenalty = 0.3f;
  k.openMarginScale = 100.0f;
  k.openKLowAngle_deg = 45.0f;
  k.openKHighAngle_deg = 90.0f;
  k.openClass1 = 10.0f;
  k.openClass2 = 40.0f;
  k.openClass3 = 60.0f;
  k.openClass4 = 100.0f;
  k.openClass5 = 130.0f;
  k.fwdPlanAngleGate_deg = 60.0f;
  k.fwdTeamFlagGate = 36.0f;
  k.fwdCloseRange_deg = 120.0f;
  k.fwdGoalCentreDist_m = 30.0f;
  k.fwdLaneWidthGain = 0.25f;
  k.fwdLaneWidthMin_m = 1.5f;
  k.fwdLaneWidthMax_m = 6.0f;
  k.fwdLaneBudget = 220.0f;
  k.fwdLineValueSq = 1225.0f;
  k.fwdLineValueFarSq = 1600.0f;
  k.fwdNearestOppFar_m = 8.0f;
  k.fwdNearestOppNear_m = 5.0f;
  k.fwdNearestOppMin_m = 1.5f;
  k.fwdTargetAdvance_m = 35.0f;
  k.fwdOppNearDist_m = 7.0f;
  k.fwdOppNearFreeDist_m = 3.0f;
  k.fwdTurnFacing_deg = 80.0f;
  k.fwdFacingBack_deg = 135.0f;
  k.fwdMateSpeedCap_kmh = 20.0f;
  k.loftMinDist_m = 15.0f;
  k.loftMaxDist_m = 25.0f;
  k.loftDistBase_m = 10.0f;
  k.loftDistGain_m = 5.0f;
  k.loftKeeperNearSq = 100.0f;
  k.loftKeeperFarSq = 225.0f;
  k.throughMinDist_m = 5.0f;
  k.throughCap_m = 20.0f;
  k.throughOpenCap_m = 15.0f;
  k.throughOpennessOpen = 100.0f;
  k.throughClassOpen = 4.0f;
  k.throughMateSpeedCap_kmh = 60.0f;
  k.throughFacing_deg = 135.0f;
  k.breakthroughSeedK = 1100.0f;
  k.middleShotSeedK = 1200.0f;
  k.curveShotSeedK = 1300.0f;
  k.crossSeedK = 1500.0f;
  k.longSeedK = 1600.0f;
  k.planRollModulus = 100;
  k.planRollSlot = 2;
  k.statFloor = 70.0f;
  k.statSpan = 29.0f;
  k.breakthroughBase = 70.0f;
  k.breakthroughComBase = 20.0f;
  k.breakthroughStatSpan = 20.0f;
  k.breakthroughStatGain = 15.0f;
  k.breakthroughCap = 70.0f;
  k.middleShotBase = 70.0f;
  k.middleShotStateGain = 40.0f;
  k.middleShotStatGain = 15.0f;
  k.middleShotFloor = 5.0f;
  k.middleShotCap = 80.0f;
  k.crossBase = 50.0f;
  k.crossStatGain = 20.0f;
  k.crossFloor = 40.0f;
  k.crossCap = 80.0f;
  k.longBasePressed = 30.0f;
  k.longBaseMid = 60.0f;
  k.longBaseFree = 80.0f;
  k.longStatGain = 10.0f;
  k.longComBonus = 50.0f;
  k.longSkillBonus = 50.0f;
  k.longLevelStep = 20.0f;
  k.longPathBSkillBonus = 10.0f;
  // The dribble search (report 1) and its threat term (report 1.1).
  k.dribbleEdgeCost = 100.0f;
  k.dribbleEdgeMargin_m = 0.5f;
  k.dribblePrefGain = 0.6f;
  k.dribbleRange_m = 10.0f;
  k.dribbleStep_m = 2.0f / 3.0f;
  k.dribbleDirections = 72.0f;
  k.threatReachFloor_m = 5.0f;
  k.threatReachScale = 2.0f;
  k.threatKeeperScale = 2.0f;
  k.threatFullDist_m = 5.0f;
  k.threatFalloff = -0.25f;
  k.threatPushSq_m2 = 0.444444f;
  return k;
}

// --- the shoot zone (report 4.1) ---

// The zone builder's bands: inside the posts, the central range Rn..Rf; outside
// them, the side range. A Finishing of 99 with the long-shot skills puts the
// two edges at 21 and 27 (OBS live: Rn 19.03 at 71, Rn 20.90 at 89, Rf 27.0).
TEST(PesCarrierZone, CentralBandFollowsTheRanges) {
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.5f, GOAL_HALF = 3.7f;
  // d = 18 - 3*((70-70)/29) = 18 for a 70 Finishing: close.
  pes::ShootZone close = pes::ComputeShootZone(35.0f, 0.0f, 1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_FLOAT_EQ(close.d_m, 17.5f);
  EXPECT_TRUE(pes::IsClose(close));
  // d = 17.5 is inside Rn..Rf: mid.
  pes::ShootZone mid = pes::ComputeShootZone(34.0f, 0.0f, 1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_TRUE(pes::IsMid(mid));
  EXPECT_FALSE(pes::IsClose(mid));
  // d = 25 is past Rf = 24: far.
  pes::ShootZone far = pes::ComputeShootZone(27.5f, 0.0f, 1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_EQ(far.band, pes::kBandFar);
  EXPECT_TRUE(pes::InShootRange(far));
  // The Finishing ramp: 99 Finishing moves Rn to 21 and Rf to 27 with the
  // long-shot skills, so d = 20.5 is close where it was mid.
  pes::ShootZone ramped = pes::ComputeShootZone(32.0f, 0.0f, 1.0f, 99.0f, true, HALF_LEN, GOAL_HALF, k);
  EXPECT_FLOAT_EQ(ramped.d_m, 20.5f);
  EXPECT_TRUE(pes::IsClose(ramped));
}

// Behind the goal line the band and sector both go wide: nothing is in range.
TEST(PesCarrierZone, BehindTheGoalLineIsOut) {
  const pes::CarrierConstants k = Synthetic();
  pes::ShootZone behind = pes::ComputeShootZone(-40.0f, 0.0f, 1.0f, 90.0f, false, 52.5f, 3.7f, k);
  EXPECT_EQ(behind.band, pes::kBandOut);
  EXPECT_EQ(behind.sector, pes::kSectorWide);
  EXPECT_FALSE(pes::InShootRange(behind));
  EXPECT_FALSE(pes::IsCentral(behind));
}

// Outside the posts the distance is to the near post and the bearing decides
// the sector: central inside 15 degrees, side inside 40.
TEST(PesCarrierZone, OutsideThePostsMeasuresToThePost) {
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.5f, GOAL_HALF = 3.7f;
  // Level with the goal line and 6 m wide of the post: the distance is to the
  // near post 6 m away, and the bearing is straight across, so the sector is
  // wide and nothing is in range.
  pes::ShootZone across =
      pes::ComputeShootZone(HALF_LEN, 9.7f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_NEAR(across.d_m, 6.0f, 0.01f);
  EXPECT_EQ(across.sector, pes::kSectorWide);
  EXPECT_FALSE(pes::IsCentral(across));
  EXPECT_TRUE(pes::IsClose(across));  // the box bit makes it in range, band 0
  // The ball is exactly level with the goal line, on the box cone's boundary:
  // the box bit is set (PES's `setbe` includes the boundary), so it is in range
  // and the box rule fires.
  EXPECT_TRUE(pes::InBox(across));
  EXPECT_TRUE(pes::InShootRange(across));
  // Pushed wider past the cone's radius it leaves the box.
  pes::ShootZone wider =
      pes::ComputeShootZone(HALF_LEN, 12.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_FALSE(pes::InBox(wider));
  EXPECT_FALSE(pes::InShootRange(wider));
  // Level with the near post and 8 m short of it: the bearing is nearly along
  // the attack direction, so the sector is central and the side range applies.
  pes::ShootZone side =
      pes::ComputeShootZone(HALF_LEN - 8.0f, 3.7f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_NEAR(side.d_m, 8.0f, 0.01f);
  EXPECT_TRUE(pes::IsClose(side));
}

// The box predicate: inside the 11 m half-disc in front of the goal centre.
TEST(PesCarrierZone, BoxIsTheElevenMetreHalfDisc) {
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.5f, GOAL_HALF = 3.7f;
  EXPECT_TRUE(pes::InBox(pes::ComputeShootZone(45.0f, 0.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k)));
  EXPECT_FALSE(pes::InBox(pes::ComputeShootZone(45.0f, 10.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k)));
  EXPECT_TRUE(pes::InBox(pes::ComputeShootZone(45.0f, 6.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k)));
  EXPECT_FALSE(pes::InBox(pes::ComputeShootZone(40.0f, 0.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k)));
  EXPECT_FALSE(pes::InBox(pes::ComputeShootZone(45.0f, 12.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k)));
  // Behind the goal line there is no front to be in front of.
  EXPECT_FALSE(pes::InBox(pes::ComputeShootZone(-45.0f, 0.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k)));
}

// --- the shoot decision (report 5.1) ---

TEST(PesCarrierShoot, JustShootInsideTwentyAndFortyDegrees) {
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.5f, GOAL_HALF = 3.7f;
  // d = 12, level with the goal: inside 20 m and inside 40 degrees.
  pes::ShootInput in;
  in.zone = pes::ComputeShootZone(40.5f, 0.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  in.owned = true;
  in.keeperPresent = true;
  in.keeperSettled = true;
  EXPECT_EQ(pes::ShootSubKind(in, k), 1);
  // Outside 20 m the rule does not fire: a wide ball with no other rule left
  // to answer it.
  pes::ShootInput farIn = in;
  farIn.zone = pes::ComputeShootZone(31.0f, 12.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  if (!pes::InBox(farIn.zone)) EXPECT_EQ(pes::ShootSubKind(farIn, k), 0);
}

TEST(PesCarrierShoot, NoShotFirstTimeBeyondTwentyMetres) {
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.5f, GOAL_HALF = 3.7f;
  pes::ShootInput in;
  in.owned = false;  // a first-time shot
  in.keeperPresent = true;
  in.keeperSettled = true;
  in.incomingVsFacing_deg = 30.0f;
  in.aimVsFacing_deg = 10.0f;
  in.zone = pes::ComputeShootZone(31.0f, 0.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  // 21.5 m from the goal, squared = 462.25 > 400: no shot.
  in.ballToGoalDistSq = 21.5f * 21.5f;
  EXPECT_EQ(pes::ShootSubKind(in, k), 0);
  in.ballToGoalDistSq = 19.0f * 19.0f;
  EXPECT_EQ(pes::ShootSubKind(in, k), 1);
}

TEST(PesCarrierShoot, TheBoxAndTheHeader) {
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.5f, GOAL_HALF = 3.7f;
  pes::ShootInput in;
  in.owned = true;
  in.height_m = 180.0f;
  // 11 m out and 4 m wide: inside the box, so a shot, but past the just-shoot
  // distance band's angle is 0 so this would fire anyway; use a wide ball.
  in.zone = pes::ComputeShootZone(41.5f, 4.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_EQ(pes::ShootSubKind(in, k), 1);
  // 13 m out and 1 m wide: outside the box, past the just-shoot distance, so
  // the plain-shot branch only fires when the goal is in front of him.
  in.zone = pes::ComputeShootZone(39.5f, 1.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  in.aimVsFacing_deg = 95.0f;
  EXPECT_EQ(pes::ShootSubKind(in, k), 0);
  in.aimVsFacing_deg = 20.0f;
  EXPECT_EQ(pes::ShootSubKind(in, k), 1);
  // Above the head (0.8 * 1.8 m = 1.44) and inside the header range (12 m).
  in.ballHeight_m = 2.0f;
  in.zone = pes::ComputeShootZone(40.6f, 0.0f, 1.0f, 90.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_EQ(pes::ShootSubKind(in, k), 1);
}

// --- the openness formula's class ladder (report 4.3) ---

TEST(PesCarrierOpenness, ClassBoundariesAtTenFortySixtyHundredThirty) {
  const pes::CarrierConstants k = Synthetic();
  EXPECT_EQ(pes::OpennessClassFromMargin(0.0f, k), 0);
  EXPECT_EQ(pes::OpennessClassFromMargin(9.99f, k), 0);
  EXPECT_EQ(pes::OpennessClassFromMargin(10.0f, k), 1);
  EXPECT_EQ(pes::OpennessClassFromMargin(40.0f, k), 2);
  EXPECT_EQ(pes::OpennessClassFromMargin(60.0f, k), 3);
  EXPECT_EQ(pes::OpennessClassFromMargin(100.0f, k), 4);
  EXPECT_EQ(pes::OpennessClassFromMargin(130.0f, k), 4);
  EXPECT_EQ(pes::OpennessClassFromMargin(130.01f, k), 5);
  EXPECT_EQ(pes::OpennessClassFromMargin(1000.0f, k), 5);
}

// An opponent right in the lane has no perpendicular margin, so his
// contribution is a margin of 0 (class 0); the same opponent 4 m off it has
// 100 * 3 / T.
TEST(PesCarrierOpenness, MarginScalesWithThePerpendicularDistance) {
  const pes::CarrierConstants k = Synthetic();
  // along = 10, perp = 1.0: T = min(0.6*10, 0.3*81) = 6, margin = 100*0/6 = 0.
  EXPECT_FLOAT_EQ(pes::LaneOpennessMargin(0, 0, 10.0f, 1.0f, 1.0f, 0.0f, k), 0.0f);
  // perp = 5: margin = 100 * 4 / 6.
  EXPECT_FLOAT_EQ(pes::LaneOpennessMargin(0, 0, 10.0f, 5.0f, 1.0f, 0.0f, k), 400.0f / 6.0f);
  // A harder pass closes the lane: difficulty 2 multiplies T by 3
  // (`T *= diff + 1`), so the margin falls by that factor.
  EXPECT_FLOAT_EQ(pes::LaneOpennessMargin(0, 0, 10.0f, 5.0f, 1.0f, 2.0f, k), 400.0f / 18.0f);
  // Behind the ball (along < 0.2) the opponent is not in the lane at all.
  EXPECT_FLOAT_EQ(pes::LaneOpennessMargin(0, 0, 0.1f, 5.0f, 1.0f, 0.0f, k), 0.0f);
}

// --- the receiver difficulty term (report 4.3) ---

TEST(PesCarrierDifficulty, ComponentsAndTheLowPassMultiplier) {
  const pes::CarrierConstants k = Synthetic();
  // A settled ball at ground level, no turn and a good passer: only the High
  // term of a ground ball (0) and the Low Pass multiplier (1.0 at 99).
  EXPECT_NEAR(pes::ReceiverDifficulty(0.0f, 5.0f, 0.0f, false, 0.0f, false, 0.0f, 0.0f, 99.0f, k),
              0.0f, 1e-4f);
  // The turn term: 190 degrees is past the 130 top, so it is the full 1.5,
  // times the Low Pass multiplier (0.7 + 0.3*g, g = 1 at or below 80).
  EXPECT_NEAR(pes::ReceiverDifficulty(0.0f, 5.0f, 0.0f, false, 0.0f, false, 190.0f, 0.0f, 80.0f, k),
              1.5f, 1e-3f);
  // A ball at head height adds the High term's full 1.0.
  EXPECT_NEAR(pes::ReceiverDifficulty(0.0f, 5.0f, 0.0f, false, 0.0f, false, 0.0f, 2.0f, 80.0f, k),
              1.0f, 1e-3f);
  // A poor passer pays the multiplier: at 99 Low Pass the multiplier is 0.7
  // alone (report 4.3's ramp bottoms out there; live it never did, which is
  // why the sampled multipliers stayed in [0.76, 1.0]).
  EXPECT_NEAR(pes::ReceiverDifficulty(0.0f, 5.0f, 0.0f, false, 0.0f, false, 190.0f, 0.0f, 99.0f, k),
              1.5f * 0.7f, 1e-3f);
  // The moving-ball term: a fast, far, square ball is expensive, and One-touch
  // Pass halves that term inside the sum.
  const float plain = pes::ReceiverDifficulty(30.0f, 30.0f, 90.0f, false, 0.0f, false, 0.0f, 0.0f,
                                              80.0f, k);
  const float oneTouch = pes::ReceiverDifficulty(30.0f, 30.0f, 90.0f, true, 0.0f, false, 0.0f, 0.0f,
                                                 80.0f, k);
  EXPECT_GT(plain, 1.0f);
  EXPECT_LT(oneTouch, plain);
}

// --- the direction and turn classes (report 4.3) ---

TEST(PesCarrierClasses, ForwardSquareBack) {
  const pes::CarrierConstants k = Synthetic();
  EXPECT_EQ(pes::DirectionClass(0.0f, k), 0);
  EXPECT_EQ(pes::DirectionClass(60.0f, k), 0);
  EXPECT_EQ(pes::DirectionClass(110.0f, k), 2);
  EXPECT_EQ(pes::DirectionClass(111.0f, k), 1);
  EXPECT_EQ(pes::DirectionClass(-170.0f, k), 1);
  EXPECT_EQ(pes::TurnClass(0.0f, k), 3);
  EXPECT_EQ(pes::TurnClass(60.0f, k), 3);
  EXPECT_EQ(pes::TurnClass(110.0f, k), 4);
  EXPECT_EQ(pes::TurnClass(180.0f, k), 5);
}

// --- PassForward's three tiers (report 5.2) ---

namespace {

pes::ReceiverInput Mate(float targetX, float targetZ, float passAngle, float fwdAngle,
                        float turnAngle, int opennessClass, float dist, float difficulty) {
  pes::ReceiverInput r;
  r.target = {targetX, targetZ};
  r.passAngle_deg = passAngle;
  r.fwdAngle_deg = fwdAngle;
  r.turnAngle_deg = turnAngle;
  r.opennessClass = opennessClass;
  r.openness = opennessClass == 0 ? 5.0f : (opennessClass == 3 ? 80.0f : 150.0f);
  r.difficulty = difficulty;
  r.ballToTarget_m = dist;
  r.dirClass = pes::DirectionClass(fwdAngle, Synthetic());
  r.turnClass = pes::TurnClass(turnAngle, Synthetic());
  return r;
}

}  // namespace

TEST(PesCarrierPassForward, ShortForwardWantsTheMostAdvancedOpenMate) {
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.5f;
  pes::PassForwardInput in;
  in.owned = true;
  in.close = false;
  in.carrierPressureClass = 0;
  in.attackSign = 1.0f;
  in.planAngle_deg = 0.0f;
  in.imageState = 0;
  // Two open forward mates, both passes longer than 30 m so the short-pass
  // tightening (openness 4) does not bite: the more advanced one wins.
  std::vector<pes::PassCandidate> mates(2);
  mates[0].index = 3;
  mates[0].record = Mate(30.0f, 10.0f, 0.0f, 90.0f, 10.0f, 4, 32.0f, 0.5f);
  mates[1].index = 7;
  mates[1].record = Mate(40.0f, -10.0f, 0.0f, 90.0f, 10.0f, 4, 32.0f, 0.5f);
  mates[0].nearestOppToMate_m = 12.0f;
  mates[1].nearestOppToMate_m = 12.0f;
  const pes::PassAnswer answer = pes::PassForward(mates, {}, in, k);
  EXPECT_EQ(answer.index, 7);
  EXPECT_EQ(answer.kind, pes::kPassShort);
}

TEST(PesCarrierPassForward, ClosedLaneForcesTheThroughBall) {
  const pes::CarrierConstants k = Synthetic();
  pes::PassForwardInput in;
  in.owned = true;
  in.close = false;
  in.carrierPressureClass = 0;
  in.attackSign = 1.0f;
  in.planAngle_deg = 0.0f;
  in.imageState = 1;
  in.freeSpace = 40.0f;  // over the 36 threshold
  // One mate, far upfield, only 8 m away: the ground lane is closed
  // (openness class 0), so tier 1 rejects and tier 2's through ball answers.
  std::vector<pes::PassCandidate> mates(1);
  mates[0].index = 5;
  mates[0].record = Mate(42.0f, 5.0f, 0.0f, 0.0f, 10.0f, 0, 8.0f, 1.5f);
  mates[0].nearestOppToMate_m = 12.0f;
  const pes::PassAnswer answer = pes::PassForward(mates, {}, in, k);
  EXPECT_EQ(answer.index, 5);
  EXPECT_EQ(answer.kind, pes::kPassThrough);
}

TEST(PesCarrierPassForward, NoFreeSpaceNoThroughBall) {
  const pes::CarrierConstants k = Synthetic();
  pes::PassForwardInput in;
  in.owned = true;
  in.close = false;
  in.carrierPressureClass = 0;
  in.attackSign = 1.0f;
  in.imageState = 1;
  in.freeSpace = 30.0f;  // below the 36 threshold
  std::vector<pes::PassCandidate> mates(1);
  mates[0].index = 5;
  mates[0].record = Mate(42.0f, 5.0f, 0.0f, 0.0f, 10.0f, 0, 8.0f, 1.5f);
  mates[0].nearestOppToMate_m = 12.0f;
  const pes::PassAnswer answer = pes::PassForward(mates, {}, in, k);
  EXPECT_EQ(answer.index, -1);
}

TEST(PesCarrierPassForward, LoftedForwardInStateTwoOnly) {
  const pes::CarrierConstants k = Synthetic();
  pes::PassForwardInput in;
  in.owned = true;
  in.close = false;
  in.carrierPressureClass = 0;
  in.attackSign = 1.0f;
  in.imageState = 2;
  in.freeSpace = 0.0f;
  // A mate 20 m away, facing forward, with a closed ground lane and the
  // keeper far off: the lofted forward answers a long ball.
  std::vector<pes::PassCandidate> mates(1);
  mates[0].index = 9;
  mates[0].record = Mate(38.0f, 20.0f, 0.0f, 0.0f, 30.0f, 0, 20.0f, 2.0f);
  mates[0].pressureClass = 3;
  mates[0].pressureDistOpp0_m = 400.0f;  // 20 m, over the 15 m squared cap
  mates[0].nearestOppToMate_m = 12.0f;
  const pes::PassAnswer answer = pes::PassForward(mates, {}, in, k);
  EXPECT_EQ(answer.index, 9);
  EXPECT_EQ(answer.kind, pes::kPassLong);
  // In image state 0 the lofted branch is not tried.
  in.imageState = 0;
  EXPECT_EQ(pes::PassForward(mates, {}, in, k).index, -1);
}

// The unit's head gate: pressed and not owned the unit stays silent.
// A mate that satisfies both tier 1 and tier 3 must be answered by tier 1:
// the tiers are attempted in order, and the short forward wins.
TEST(PesCarrierPassForward, TheShortForwardBeatsTheLoftedOne) {
  const pes::CarrierConstants k = Synthetic();
  pes::PassForwardInput in;
  in.owned = true;
  in.close = false;
  in.carrierPressureClass = 0;
  in.attackSign = 1.0f;
  in.imageState = 2;
  // Two mates: one only qualifies for the lofted forward (closed ground lane,
  // so tier 1 rejects it), and one only for the short forward. Tier 1 is asked
  // first, so the short ball wins over the lofted one however far upfield the
  // lofted target stands.
  std::vector<pes::PassCandidate> mates(2);
  mates[0].index = 8;
  mates[0].record = Mate(48.0f, 0.0f, 0.0f, 90.0f, 30.0f, 0, 20.0f, 0.5f);
  mates[0].pressureClass = 3;
  mates[0].pressureDistOpp0_m = 400.0f;
  mates[0].nearestOppToMate_m = 12.0f;
  mates[1].index = 2;
  mates[1].record = Mate(20.0f, 0.0f, 0.0f, 90.0f, 30.0f, 4, 32.0f, 0.5f);
  mates[1].nearestOppToMate_m = 12.0f;
  const pes::PassAnswer answer = pes::PassForward(mates, {}, in, k);
  EXPECT_EQ(answer.index, 2);
  EXPECT_EQ(answer.kind, pes::kPassShort);
}

TEST(PesCarrierPassForward, GateOnPressureAndTheCloseBand) {
  const pes::CarrierConstants k = Synthetic();
  pes::PassForwardInput in;
  in.owned = false;
  in.carrierPressureClass = 2;  // pressed
  in.close = false;
  in.attackSign = 1.0f;
  std::vector<pes::PassCandidate> mates(1);
  mates[0].index = 4;
  mates[0].record = Mate(40.0f, 0.0f, 0.0f, 90.0f, 10.0f, 4, 32.0f, 0.5f);
  mates[0].nearestOppToMate_m = 12.0f;
  EXPECT_EQ(pes::PassForward(mates, {}, in, k).index, -1);
  in.carrierPressureClass = 1;
  EXPECT_EQ(pes::PassForward(mates, {}, in, k).index, 4);
  // In the close band the unit is off entirely.
  in.close = true;
  EXPECT_EQ(pes::PassForward(mates, {}, in, k).index, -1);
}

// --- the plan order and its roll (report 4.4) ---

TEST(PesCarrierPlan, PriorityOrderIsNineEightTwoThreeOneFourFiveSixSeven) {
  constexpr int EXPECTED[] = {9, 8, 2, 3, 1, 4, 5, 6, 7};
  static_assert(sizeof(pes::kPlanPriorityOrder) / sizeof(int) == 9);
  for (int i = 0; i < 9; ++i) EXPECT_EQ(pes::kPlanPriorityOrder[i], EXPECTED[i]);
}

TEST(PesCarrierPlan, StartProbabilitiesFollowThePTable) {
  const pes::CarrierConstants k = Synthetic();
  // Breakthrough: base 70 with the skill, capped at 70, plus the stat term.
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanBreakthrough, k, 99.0f, true, false, false),
                  k.breakthroughCap);
  // Without the skill a COM run style gives the 20 base plus the stat term.
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanBreakthrough, k, 70.0f, false, true, false),
                  k.breakthroughComBase);
  EXPECT_GT(pes::PlanStartProbability(pes::kPlanBreakthrough, k, 99.0f, false, true, false),
            k.breakthroughComBase);
  // MiddleShot: 70 with the skill, +40 in image state 2, capped at 80, and
  // floored at 5 even with nothing.
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanMiddleShot, k, 70.0f, false, false, false),
                  k.middleShotFloor);
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanMiddleShot, k, 70.0f, true, false, true),
                  k.middleShotCap);
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanMiddleShot, k, 70.0f, true, false, false),
                  k.middleShotBase);
  // Cross: base 50 with the style or the skill, floored at 40, capped at 80.
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanCross, k, 70.0f, true, false, false),
                  k.crossBase);
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanCross, k, 70.0f, false, false, false),
                  k.crossFloor);
  // Plans that are not rolled have no probability at all.
  EXPECT_LT(pes::PlanStartProbability(pes::kPlanPassThrough, k, 90.0f, false, false, false), 0.0f);
}

// --- the Long plan (report 6.6: veto 0x14211a570, body 0x14211b1d0) ---

pes::LongInput LongCarrier() {
  pes::LongInput in;
  in.intention = 1;
  in.engagement = 1;  // base 30
  in.lofted = 70.0f;  // no ramp yet
  return in;
}

TEST(PesCarrierLong, TheVetoRejectsIntentionThreeAndFourAndAPressedCarrier) {
  pes::LongInput in = LongCarrier();
  EXPECT_FALSE(pes::LongVetoed(in));
  in.intention = 3;
  EXPECT_TRUE(pes::LongVetoed(in));
  in.intention = 4;
  EXPECT_TRUE(pes::LongVetoed(in));
  in = LongCarrier();
  in.engagement = 0;  // engagement (+16) below 1
  EXPECT_TRUE(pes::LongVetoed(in));
}

TEST(PesCarrierLong, AnAcceptBypassSkipsTheRollButNotTheVeto) {
  const pes::CarrierConstants k = Synthetic();
  // Score 30 against a draw of 99: the roll would reject.
  pes::LongInput in = LongCarrier();
  EXPECT_FALSE(pes::LongAdmitted(in, 99, k));
  in.teamModeThree = true;
  EXPECT_TRUE(pes::LongAdmitted(in, 99, k));
  in = LongCarrier();
  in.designatedSelf = true;
  EXPECT_TRUE(pes::LongAdmitted(in, 99, k));
  in = LongCarrier();
  in.fullAttack = true;
  EXPECT_TRUE(pes::LongAdmitted(in, 99, k));
  in.intention = 3;  // the veto is checked first
  EXPECT_FALSE(pes::LongAdmitted(in, 99, k));
}

TEST(PesCarrierLong, TheRollAdmitsAtOrBelowTheScore) {
  const pes::CarrierConstants k = Synthetic();
  const pes::LongInput in = LongCarrier();  // score 30
  EXPECT_TRUE(pes::LongAdmitted(in, 30, k));
  EXPECT_FALSE(pes::LongAdmitted(in, 31, k));
}

TEST(PesCarrierLong, TheBaseFollowsTheCarrierPressureClass) {
  const pes::CarrierConstants k = Synthetic();
  pes::LongInput in = LongCarrier();
  in.engagement = 0;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBasePressed);
  in.engagement = 2;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBaseMid);
  in.engagement = 3;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBaseFree);
}

TEST(PesCarrierLong, COMAndEachSkillAddTheirOwnBonus) {
  const pes::CarrierConstants k = Synthetic();
  pes::LongInput in = LongCarrier();
  in.engagement = 3;
  in.comLongBallExpert = true;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBaseFree + k.longComBonus);
  in.comLongBallExpert = false;
  in.skillWeightedPass = true;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBaseFree + k.longSkillBonus);
  in.skillLowLoftedPass = true;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBaseFree + 2.0f * k.longSkillBonus);
}

TEST(PesCarrierLong, ATeamLevelAddsTwentyPerStepAndCapsAtForty) {
  const pes::CarrierConstants k = Synthetic();
  pes::LongInput in = LongCarrier();
  in.level = 1;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBasePressed + k.longLevelStep);
  in.level = 2;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBasePressed + 2.0f * k.longLevelStep);
  in.level = 3;  // the binary's compares stop at two steps
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBasePressed + 2.0f * k.longLevelStep);
}

TEST(PesCarrierLong, TheLoftedRampAddsItsGainAboveSeventyAndSaturates) {
  const pes::CarrierConstants k = Synthetic();
  pes::LongInput in = LongCarrier();
  in.lofted = k.statFloor;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBasePressed);
  in.lofted = k.statFloor + 0.5f * k.statSpan;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBasePressed + 0.5f * k.longStatGain);
  in.lofted = k.statFloor + 2.0f * k.statSpan;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longBasePressed + k.longStatGain);
}

TEST(PesCarrierLong, PathBTakesOnlyTheTwoSkillsAndTheRamp) {
  const pes::CarrierConstants k = Synthetic();
  pes::LongInput in = LongCarrier();
  in.teamSetting = true;
  in.engagement = 3;
  in.comLongBallExpert = true;
  in.level = 2;
  in.lofted = k.statFloor + k.statSpan;  // full ramp
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), k.longStatGain);
  in.skillWeightedPass = true;
  in.skillLowLoftedPass = true;
  EXPECT_FLOAT_EQ(pes::LongScore(in, k), 2.0f * k.longPathBSkillBonus + k.longStatGain);
}

pes::CarrierSeed SeedAt(float snapshot) {
  pes::CarrierSeed seed;
  seed.snapshot = snapshot;
  return seed;
}

pes::RngConstants PlanRollRng() {
  pes::RngConstants rng{};
  rng.lcgMul = 0x2d22b2e3u;
  rng.lcgSub = 0x4513u;
  rng.modulus = {100, 5000, 32768, 32768};
  rng.scale = 4.656612873077393e-10f;
  rng.defaultSeeds = {0, 1, 2, 3};
  return rng;
}

TEST(PesCarrierPlan, TheRollIsSlotTwoModulusOneHundred) {
  pes::CarrierConstants k = Synthetic();
  const pes::RngConstants rng = PlanRollRng();
  // The report's roll: a throw-away generator seeded with int(snapshot * K) on
  // slot 2, drawn once through RangeRaw(2, 100).
  const int roll = pes::PlanRoll(SeedAt(0.5f), k.breakthroughSeedK, rng, k);
  pes::Random manual(rng);
  manual.SetSeed(k.planRollSlot,
                 static_cast<uint32_t>(static_cast<int>(0.5f * k.breakthroughSeedK)));
  EXPECT_EQ(roll, manual.RangeRaw(k.planRollSlot, k.planRollModulus));
  EXPECT_GE(roll, 0);
  EXPECT_LT(roll, 100);
  // The modulus spans the whole 0..99 range: over many snapshots both a low and a
  // high draw turn up, so the start test `P >= roll` really is a percentage gate.
  int low = 100, high = -1;
  for (int i = 0; i < 200; ++i) {
    const int r =
        pes::PlanRoll(SeedAt(static_cast<float>(i) / 200.0f), k.breakthroughSeedK, rng, k);
    ASSERT_GE(r, 0);
    ASSERT_LT(r, 100);
    low = std::min(low, r);
    high = std::max(high, r);
  }
  EXPECT_LE(low, 10);
  EXPECT_GE(high, 90);
  // The same snapshot and K agree, and a different snapshot moves the seed.
  EXPECT_EQ(roll, pes::PlanRoll(SeedAt(0.5f), k.breakthroughSeedK, rng, k));
  EXPECT_NE(pes::PlanRoll(SeedAt(0.6f), k.breakthroughSeedK, rng, k), roll);
}

TEST(PesCarrierSeed, ARunCopiesRandomInfoOnItsFirstFrameAndKeepsTheCopy) {
  // report 3.9-3.13: the member starts at -1.0. The first frame of a run copies
  // RandomInfo+8; the later frames of the same run keep that copy.
  pes::CarrierSeed seed;
  EXPECT_FLOAT_EQ(seed.snapshot, -1.0f);
  pes::CarrierSeedTick(seed, true, 0.25f);
  EXPECT_FLOAT_EQ(seed.snapshot, 0.25f);
  pes::CarrierSeedTick(seed, true, 0.75f);
  EXPECT_FLOAT_EQ(seed.snapshot, 0.25f);
  EXPECT_EQ(seed.updates, 2);
}

TEST(PesCarrierSeed, AFrameWithoutTheCarrierPredicateResetsTheMember) {
  // Driver 0x142110c90: a frame the predicate 0x14210e200 fails runs the reset
  // 0x14210e5f0 -> 0x14063a3b0. The next run then takes a fresh copy of that frame's
  // RandomInfo, not the copy of the run before.
  pes::CarrierSeed seed;
  pes::CarrierSeedTick(seed, true, 0.25f);
  pes::CarrierSeedTick(seed, false, 0.5f);
  EXPECT_FLOAT_EQ(seed.snapshot, -1.0f);
  EXPECT_EQ(seed.updates, 0);
  pes::CarrierSeedTick(seed, true, 0.75f);
  EXPECT_FLOAT_EQ(seed.snapshot, 0.75f);
}

TEST(PesCarrierSeed, ARollRepeatsWhileTheRunHoldsAndFollowsTheNextRun) {
  // The roll reads the snapshot and K alone: the frames after the copy do not reach it,
  // however many frames the run lasts.
  const pes::CarrierConstants k = Synthetic();
  const pes::RngConstants rng = PlanRollRng();
  pes::CarrierSeed seed;
  pes::CarrierSeedTick(seed, true, 0.4f);
  const int first = pes::PlanRoll(seed, k.breakthroughSeedK, rng, k);
  for (int frame = 0; frame < 50; ++frame) {
    pes::CarrierSeedTick(seed, true, 0.9f - 0.01f * static_cast<float>(frame));
    EXPECT_EQ(pes::PlanRoll(seed, k.breakthroughSeedK, rng, k), first) << "frame " << frame;
  }
  pes::CarrierSeedTick(seed, false, 0.9f);
  pes::CarrierSeedTick(seed, true, 0.9f);
  EXPECT_NE(pes::PlanRoll(seed, k.breakthroughSeedK, rng, k), first)
      << "a new run copies the new frame's RandomInfo, and the roll follows it";
}

// --- the think cadence (report 2) ---

TEST(PesCarrierCadence, TheRethinkGateIsFivePesFrames) {
  // Milestone A1: this engine's tick IS PES's logic frame, so the 5-frame gate
  // (92.6 ms) is 5 ticks, not 9.26 of GF's old 10 ms ones.
  // `frame - busyStamp > 5`: on the 6th frame after leaving a committed action.
  EXPECT_FALSE(pes::RethinkDue(5, 0));
  EXPECT_TRUE(pes::RethinkDue(6, 0));
  EXPECT_FALSE(pes::RethinkDue(15, 10));
  EXPECT_TRUE(pes::RethinkDue(16, 10));
}

// --- the dribble direction search (report 1 and 1.1) ---

constexpr float kRadPerDeg = 3.14159265f / 180.0f;

// A carrier at the centre of a 105 x 68 pitch, wanting heading `desired`.
pes::DribbleInput OpenPitch(float desired) {
  pes::DribbleInput in;
  in.ball = {0.0f, 0.0f};
  in.desired_deg = desired;
  in.weight = 1.0f;
  in.halfLen_m = 52.5f;
  in.halfWid_m = 34.0f;
  return in;
}

// Unsigned angle between two headings, folded to [0, 180].
float OffAxis(float deg, float desired) {
  return std::fabs(std::remainder(deg - desired, 360.0f));
}

TEST(PesCarrierDribble, WithNoOpponentsItFollowsTheDesiredHeading) {
  const pes::CarrierConstants k = Synthetic();
  EXPECT_NEAR(pes::DribbleDirection(OpenPitch(90.0f), k), 90.0f, 1e-3f);
}

TEST(PesCarrierDribble, ATouchlineBandKeepsHimOffTheLine) {
  // 0.8 m inside the +z touchline and wanting to run into it. Any heading that
  // runs at the line pays the edge cost; running along the line does not.
  const pes::CarrierConstants k = Synthetic();
  pes::DribbleInput in = OpenPitch(90.0f);
  in.ball = {0.0f, 33.2f};
  const float ans = pes::DribbleDirection(in, k);
  EXPECT_LT(std::sin(ans * kRadPerDeg), 0.45f);
}

TEST(PesCarrierDribble, AManAheadTurnsHimAway) {
  // A man 2 m dead ahead: the cheapest heading leaves the desired line by more
  // than 90 degrees (the reference model picks 160).
  const pes::CarrierConstants k = Synthetic();
  pes::DribbleInput in = OpenPitch(0.0f);
  in.opponents.push_back({{2.0f, 0.0f}, {2.0f, 0.0f}, k.dribbleRange_m, false});
  EXPECT_GT(OffAxis(pes::DribbleDirection(in, k), 0.0f), 90.0f);
}

TEST(PesCarrierThreat, ReachGateAndKeeperDoubling) {
  const pes::CarrierConstants k = Synthetic();
  const pes::CarrierPoint ball{0.0f, 0.0f};
  // Reach is max(5, 2 R_o) = 20 m for R_o 10. At 4 m the term is (10 - 4)/10
  // raised to 1 + w, with w = clamp01((5 - 4)/4) = 0.25.
  const float near = pes::DribbleThreat(ball, ball, {{{4.0f, 0.0f}, {4.0f, 0.0f}, 10.0f, false}}, k);
  EXPECT_NEAR(near, std::pow(0.6f, 1.25f), 1e-4f);
  const float keeper = pes::DribbleThreat(ball, ball, {{{4.0f, 0.0f}, {4.0f, 0.0f}, 10.0f, true}}, k);
  EXPECT_NEAR(keeper, 2.0f * near, 1e-4f);
  const float far = pes::DribbleThreat(ball, ball, {{{25.0f, 0.0f}, {25.0f, 0.0f}, 10.0f, false}}, k);
  EXPECT_EQ(far, 0.0f);
}

// --- the install: read the constants out of the user's own exe ---

TEST(PesCarrierInstall, TheZonesAndLaddersAreTheGamesOwn) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadCarrierConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // The chooser's bands widen in the decoded order: 30 (feint), 60 (turn), 40 (side), 10 and 15 (speed).
  EXPECT_GT(k->chooserTurn_deg, k->chooserGate_deg);
  EXPECT_GT(k->chooserSideAngle_deg, 0.0f);
  EXPECT_GT(k->chooserSpeedMid_kmh, k->chooserSpeedLow_kmh);
  EXPECT_GT(k->chooserRoom_m, 0.0f);
  // Relations, not the game's numbers (no PES tables in tests): the loader
  // must have read a sane shoot zone, Finishing ramp, openness ladder and plan
  // seeds from the user's own image.
  EXPECT_GT(k->nearCentral_m, 0.0f);
  EXPECT_LT(k->nearCentral_m, k->farCentral_m);
  EXPECT_GT(k->finishingBoost_m, 0.0f);
  EXPECT_LT(k->finishingFloor, k->finishingTop);
  EXPECT_FLOAT_EQ(k->finishingFloor + k->finishingSpan, k->finishingTop);
  // A top-rated finisher's near range stays inside the far range.
  EXPECT_LT(k->nearCentral_m + k->finishingBoost_m, k->farCentral_m);
  EXPECT_GT(k->openClass1, 0.0f);
  EXPECT_LT(k->openClass1, k->openClass5);
  EXPECT_LT(k->breakthroughSeedK, k->middleShotSeedK);
  EXPECT_LT(k->middleShotSeedK, k->crossSeedK);
  EXPECT_LT(k->crossSeedK, k->longSeedK);
  EXPECT_GE(k->planRollSlot, 0);
  // The dribble search: a 72-heading circle, a positive edge band and range.
  EXPECT_GT(k->dribbleEdgeCost, 0.0f);
  EXPECT_GT(k->dribbleEdgeMargin_m, 0.0f);
  EXPECT_GT(k->dribbleRange_m, k->dribbleStep_m);
  EXPECT_FLOAT_EQ(std::fmod(360.0f, k->dribbleDirections), 0.0f);
  EXPECT_LT(k->threatFalloff, 0.0f);
  EXPECT_GT(k->threatReachFloor_m, 0.0f);
  // The plan P table's stat gains are read from the image, not left at zero:
  // the synthetic set assigns them, so only this loader test can catch a
  // missing load. At stat = floor + span the ramp is saturated, so each plan's
  // score is its base plus exactly its own gain.
  EXPECT_GT(k->breakthroughStatGain, 0.0f);
  EXPECT_GT(k->crossStatGain, 0.0f);
  EXPECT_GT(k->longComBonus, 0.0f);
  EXPECT_GT(k->longSkillBonus, 0.0f);
  EXPECT_GT(k->longLevelStep, 0.0f);
  EXPECT_GT(k->longPathBSkillBonus, 0.0f);
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanBreakthrough, *k, k->statFloor + k->statSpan,
                                            false, true, false),
                  k->breakthroughComBase + k->breakthroughStatGain);
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanCross, *k, k->statFloor + k->statSpan, true,
                                            false, false),
                  k->crossBase + k->crossStatGain);
  // Breakthrough's ramp divisor is its own (0x14259be1c, where the shared
  // span is 0x1425a9460): its term saturates at stat 90, the shared span's at
  // 99, so only Breakthrough is at full gain by stat 90.
  EXPECT_LT(k->breakthroughStatSpan, k->statSpan);
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanBreakthrough, *k,
                                            k->statFloor + k->breakthroughStatSpan, false, true,
                                            false),
                  k->breakthroughComBase + k->breakthroughStatGain);
  // The lane-gated units (0x14212ABA0's lane request, 0x1421265C0's gates) and the lane core's own
  // table the bundle carries: relations only, no pinned numbers.
  EXPECT_GT(k->unitLaneLen_m, 0.0f);
  EXPECT_GT(k->unitLaneCone_deg, 0.0f);
  EXPECT_GT(k->unitLaneRad_m, 0.0f);
  EXPECT_GT(k->gfDifficultyMax, 0.0f);
  EXPECT_GT(k->gfAngle_deg, 0.0f);
  EXPECT_LT(k->gfAngle_deg, k->gfStatAngle_deg);
  EXPECT_LT(k->gfStatAngle_deg, k->gfFlagAngle_deg);
  EXPECT_LT(k->gfDistMinBand_m, k->gfDistMin_m);
  EXPECT_LT(k->gfDistMin_m, k->gfClass2Dist_m);
  EXPECT_LT(k->gfClass2Dist_m, k->gfDistMax_m);
  EXPECT_GT(k->gfStatFloor, 0.0f);
  EXPECT_GT(k->gfStatBonus, 0.0f);
  EXPECT_GT(k->gfFlagDistSq_m2, 0.0f);
  EXPECT_GT(k->gfPredictGain, 0.0f);
  EXPECT_FLOAT_EQ(k->gfZoneDist_m, k->gfClass2Dist_m);  // one load site (0x14259BE0C)
  // BackSpace (0x142129110's lane branch and block 2, 0x1421226C0's gates, 0x142125140's picker):
  // relations only, no pinned numbers.
  EXPECT_GT(k->bsOppDistSq_m2, 0.0f);
  EXPECT_GT(k->bsBallHeight_m, 0.0f);
  EXPECT_GT(k->bsCornerDistSq_m2, k->bsOppDistSq_m2);
  EXPECT_GT(k->bsPredictGain, 0.0f);
  EXPECT_GT(k->bsBoxSkipCount, 0.0f);
  EXPECT_GT(k->bsAngleLimitGap_m, 0.0f);
  EXPECT_FLOAT_EQ(k->bsAlignDist_m, k->bsAngleLimitGap_m);  // one load site (0x14259BDEC)
  EXPECT_GT(k->bsAlignNeed_m, k->bsAlignDist_m);
  EXPECT_GT(k->bsAlignAngle_deg, 0.0f);
  EXPECT_LT(k->bsAlignAngle_deg, k->bsFacingLimit_deg);
  EXPECT_LT(k->bsFacingLimit_deg, k->bsHumanFacingLimit_deg);
  EXPECT_LT(k->bsNearBox_m, k->bsNearSum_m);
  EXPECT_FLOAT_EQ(k->bsFarDist_m, k->bsNearBox_m);  // one load site (0x14259BE1C)
  EXPECT_GT(k->bsAngleClass, 0.0f);
  EXPECT_GT(k->bsLane1Len_m, 0.0f);
  EXPECT_GT(k->bsLane1Cone_deg, 0.0f);
  EXPECT_GT(k->bsLane1Rad_m, 0.0f);
  EXPECT_GT(k->bsLane2Cone_deg, k->bsFacingLimit_deg);
  EXPECT_LT(k->bsLane2Floor_deg, 0.0f);
  EXPECT_FLOAT_EQ(k->bsLane2Floor_deg, -k->bsLane2Cap_deg);  // -20.0 vs 20.0
  EXPECT_GT(k->bsLane2Gain, 0.0f);
  EXPECT_LT(k->bsLane2XNear_m, k->bsLane2XFar_m);
  EXPECT_FLOAT_EQ(k->bsLane2XFar_m - k->bsLane2XNear_m, k->bsLane2XSpan_m);  // 15 - 3 = 12
  EXPECT_GT(k->bsLane2ZGain, 0.0f);
  EXPECT_FLOAT_EQ(k->bsLane2Base_m, k->bsAlignDist_m);  // one load site (0x14259BDEC)
  EXPECT_GT(k->bsLane2Pad_m, 0.0f);
  EXPECT_FLOAT_EQ(k->bsLane2LenMin_m, k->bsLane2XNear_m);  // one load site (0x14258C264)
  EXPECT_GT(k->bsTailDistSq_m2, 0.0f);
  EXPECT_GT(k->bsTailHalf, 0.0f);
  EXPECT_FLOAT_EQ(k->bsTailFacing_deg, k->bsFacingLimit_deg);  // one load site (0x14259BE5C)
  EXPECT_GT(k->bsTailHeading_deg, 0.0f);
  EXPECT_FLOAT_EQ(k->bsTailPad_m, k->bsBallHeight_m);  // one load site (0x14256AE1C)
  EXPECT_GT(k->cbDist_m, 0.0f);
  EXPECT_GT(k->cbSpeedBranch_kmh, 0.0f);
  EXPECT_GT(k->cbAlign_deg, 0.0f);
  EXPECT_LT(k->cbSpeedBranch_kmh, k->cbFastAlign_kmh);
  EXPECT_FLOAT_EQ(k->cbCrowd_m, k->bsLane2LenMin_m);  // one load site (0x14258C264)
  EXPECT_FLOAT_EQ(k->cbBoxAngle_deg, k->bsTailDistSq_m2);  // one load site (0x14253EE30)
  EXPECT_GT(k->cbSpeedGate_kmh, 0.0f);
  EXPECT_FLOAT_EQ(k->cbAdvance_m, k->bsFarDist_m);  // one load site (0x14259BE1C)
  EXPECT_GT(k->lane.radToDeg, 0.0f);
  EXPECT_GT(k->lane.coneFloor_deg, 0.0f);
  EXPECT_GT(k->lane.trig.tabScale, 0.0f);
}

}  // namespace

TEST(PassForward, MarksDefaultToTheFieldInfoTable) {
  const pes::PassForwardInput in;
  EXPECT_FLOAT_EQ(in.halfLen_m, pesFieldHalfLength);
  EXPECT_FLOAT_EQ(in.goalHalfWidth_m, pesGoalInnerHalfWidth);
  EXPECT_FLOAT_EQ(in.goalAreaHalfWidth_m, pesGoalAreaHalfWidth);
}

// --- the MiddleShot start skill (report 4.4) and the pass command (report 9.1) ---

TEST(PesCarrierPlan, MiddleShotStartsOnCom5Alone) {
  const pes::CarrierConstants k = Synthetic();
  EXPECT_FLOAT_EQ(pes::PlanStartProbability(pes::kPlanMiddleShot, k, 70.0f, false, true, false),
                  k.middleShotBase);
}

TEST(PesCarrierPlan, MiddleShotSkillCountsRisingShotsOnItsOwn) {
  EXPECT_TRUE(pes::MiddleShotSkill(false, false, false, true));
  EXPECT_FALSE(pes::MiddleShotSkill(false, false, false, false));
}

TEST(PesCarrierPass, ThroughIsGFsLongPassOnThePesPath) {
  EXPECT_EQ(pes::PesPassFunctionType(pes::kPassThrough), e_FunctionType_LongPass);
  EXPECT_EQ(pes::PesPassFunctionType(pes::kPassLong), e_FunctionType_HighPass);
  EXPECT_EQ(pes::PesPassFunctionType(pes::kPassShort), e_FunctionType_ShortPass);
}

// --- the unit table: the list builder (0x1421112c0) ---

namespace {
pes::UnitListInput ListInput(int plan) {
  pes::UnitListInput in;
  in.plan = plan;
  return in;
}
}  // namespace

TEST(PesCarrierList, BasicListsTheDecodedKindsInTheDecodedOrder) {
  EXPECT_EQ(pes::BuildUnitList(ListInput(pes::kPlanBasic)),
            (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 2, 3, 10, 4}));
}

TEST(PesCarrierList, ACloseCarrierBasicListLosesTheDefaultPassKinds) {
  pes::UnitListInput in = ListInput(pes::kPlanBasic);
  in.close = true;
  EXPECT_EQ(pes::BuildUnitList(in), (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12}));
}

TEST(PesCarrierList, PlanOneListsBackSpaceAndTheDefaultPassKinds) {
  EXPECT_EQ(pes::BuildUnitList(ListInput(pes::kPlanBreakthrough)),
            (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 2, 3, 4, 10}));
}

TEST(PesCarrierList, PlanOneInTheCentralCloseBandListsNoPassKinds) {
  pes::UnitListInput in = ListInput(pes::kPlanBreakthrough);
  in.central = true;
  in.close = true;
  EXPECT_EQ(pes::BuildUnitList(in), (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12}));
}

TEST(PesCarrierList, TheWideSideTailAddsSevenAndSixOnlyWhenThePlanAsksForIt) {
  pes::UnitListInput in = ListInput(pes::kPlanLong);
  EXPECT_EQ(pes::BuildUnitList(in), (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 9}));
  in.planMode = 3;
  EXPECT_EQ(pes::BuildUnitList(in), (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 9, 7, 6}));
  in.planMode = 0;
  in.wideSideLatch = true;
  EXPECT_EQ(pes::BuildUnitList(in), (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 9, 7, 6}));
}

TEST(PesCarrierList, TheStopLatchCutsPlansOneToThreeAfterGoalFront) {
  // plan+0xD9: plans 1-3 stop after kind 2.
  pes::UnitListInput in = ListInput(pes::kPlanBreakthrough);
  in.stopAfterFirst = true;
  EXPECT_EQ(pes::BuildUnitList(in), (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 2}));
}

TEST(PesCarrierList, PassThroughLosesFourAndTenOnTheOneTwoTarget) {
  // plan+0xD9 with FUN_14211CB00(plan, 10) valid: plan 4 lists only 3, 2.
  pes::UnitListInput in = ListInput(pes::kPlanPassThrough);
  in.stopAfterFirst = true;
  in.oneTwoTarget = true;
  EXPECT_EQ(pes::BuildUnitList(in), (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 3, 2}));
}

TEST(PesCarrierList, TheWideSectorListsTheCrossForPlansOneToThree) {
  // imageA+0xB44 == 3: plan 1 lists 7 between 3 and 4.
  pes::UnitListInput in = ListInput(pes::kPlanBreakthrough);
  in.wideSector = true;
  EXPECT_EQ(pes::BuildUnitList(in),
            (std::vector<int>{23, 22, 0, 1, 17, 14, 11, 12, 2, 3, 7, 4, 10}));
}

TEST(PesCarrierTable, PlanSixWithALongAnswerAdoptsTheLongPass) {
  // Plan 6 lists kind 9 and no PassForward: a Long answer in slot 9 is
  // adopted.
  pes::UnitListInput list;
  list.plan = pes::kPlanLong;
  const std::vector<int> kinds = pes::BuildUnitList(list);
  ASSERT_TRUE(std::find(kinds.begin(), kinds.end(), 4) == kinds.end());
  pes::PassAnswer fwd, lng, cross;
  lng.index = 5;
  lng.kind = pes::kPassLong;
  const std::vector<pes::UnitSlot> table = pes::FillUnitTable(kinds, false, fwd, lng, cross, {});
  const pes::UnitSlot won = pes::WalkCarrierUnits(table, pes::WalkContext{});
  EXPECT_EQ(won.kind, 9);
  EXPECT_EQ(won.answer.sub, pes::kPassLong);
}

TEST(PesCarrierTable, PlanFiveWithACrossAnswerAdoptsTheCross) {
  // Plan 5 lists kind 7 and no PassForward: a Cross answer in slot 7 is
  // adopted.
  pes::UnitListInput list;
  list.plan = pes::kPlanCross;
  const std::vector<int> kinds = pes::BuildUnitList(list);
  ASSERT_TRUE(std::find(kinds.begin(), kinds.end(), 4) == kinds.end());
  pes::PassAnswer fwd, lng, cross;
  cross.index = 3;
  cross.kind = pes::kPassCentering;
  const std::vector<pes::UnitSlot> table = pes::FillUnitTable(kinds, false, fwd, lng, cross, {});
  EXPECT_EQ(pes::WalkCarrierUnits(table, pes::WalkContext{}).kind, 7);
}

TEST(PesCarrierTable, CurveShotFillsTheCutBackSlotWithTheCrossAnswer) {
  // Behind a CurveShot plan, kind 3 takes the cross answer (the cut-back).
  const std::vector<int> kinds{3, 4};
  pes::PassAnswer fwd, lng, cross;
  cross.index = 3;
  cross.kind = pes::kPassThrough;
  const std::vector<pes::UnitSlot> plain = pes::FillUnitTable(kinds, false, fwd, lng, cross, {});
  EXPECT_EQ(plain[0].answer.type, pes::kAnswerNone);
  const std::vector<pes::UnitSlot> curved =
      pes::FillUnitTable(kinds, false, fwd, lng, cross, {}, true);
  EXPECT_EQ(curved[0].answer.type, pes::kAnswerPass);
  EXPECT_EQ(curved[0].answer.sub, pes::kPassThrough);
}

// --- the Dribble sub-kind chooser (report 2.1, 0x14212f1c0) and its dispatch (0x14212fe70) ---

namespace {
pes::DribbleChooserInput OpenChooser() {
  // Straight ahead: facing, plan heading and team attack all zero, nothing near, speed zero.
  pes::DribbleChooserInput in;
  in.plan = pes::kPlanBasic;
  in.teamMode = 2;
  in.pressureClass = 3;
  in.room_m = 50.0f;
  in.laneFree = true;
  return in;
}
}  // namespace

TEST(PesCarrierChooser, AFreeLaneOnTheHeadingIsModeFour) {
  EXPECT_EQ(pes::DribbleChooser(OpenChooser(), Synthetic()), pes::kDribbleMode4);
}

TEST(PesCarrierChooser, ABlockedLaneIsModeThree) {
  pes::DribbleChooserInput in = OpenChooser();
  in.laneFree = false;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode3);
}

TEST(PesCarrierChooser, ARoomTestBelowTwelveMetresKeepsModeThree) {
  pes::DribbleChooserInput in = OpenChooser();
  in.room_m = 10.0f;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode3);
}

TEST(PesCarrierChooser, TheSideBandWidensByTwentyPerUnitOfPressure) {
  pes::DribbleChooserInput in = OpenChooser();
  in.facing_deg = 50.0f;  // past the 40 degree side band of the attack for a Basic carrier
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode3);
  in.pressure = 1.0f;  // 40 + 20 = 60: the same 50 degrees is inside it
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode4);
}

TEST(PesCarrierChooser, PlansOneToThreeCompareWithTheirPlanHeadingNotTheAttack) {
  pes::DribbleChooserInput in = OpenChooser();
  in.facing_deg = 50.0f;
  in.planHeading_deg = 50.0f;
  in.plan = pes::kPlanBasic;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode3);
  in.plan = pes::kPlanBreakthrough;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode4);
}

TEST(PesCarrierChooser, ATurningCarrierIsHeldToSixtyDegreesOfHisPlanHeading) {
  pes::DribbleChooserInput in = OpenChooser();
  in.turning = true;
  in.facing_deg = 70.0f;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode3);
  in.planHeading_deg = 50.0f;  // 20 degrees apart
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode4);
}

TEST(PesCarrierChooser, TheFeintNeedsItsGateAndTheCarrierPastHalfway) {
  pes::DribbleChooserInput in = OpenChooser();
  in.feintGate = true;
  in.forwardOfHalf = true;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleFeint);
  in.forwardOfHalf = false;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode4);
}

TEST(PesCarrierChooser, AVerticalDropInTeamModeOneKeepsModeThree) {
  pes::DribbleChooserInput in = OpenChooser();
  in.teamMode = 1;
  in.verticalVel = -2.0f;
  in.verticalLimit = -1.0f;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode3);
}

TEST(PesCarrierChooser, AFastCarrierWithAPressureUnknownTurningFromHisVelocityIsHeld) {
  pes::DribbleChooserInput in = OpenChooser();
  in.pressureClass = -1;
  in.speed_kmh = 12.0f;
  in.velZero = false;
  in.velHeading_deg = -50.0f;  // facing 0: 50 degrees off his motion
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode3);
  in.velHeading_deg = -20.0f;
  EXPECT_EQ(pes::DribbleChooser(in, Synthetic()), pes::kDribbleMode4);
}

TEST(PesCarrierDispatch, TheLongImageRunsNoChooserAndKeepsModeThree) {
  pes::DribbleChooserInput in = OpenChooser();
  in.plan = pes::kPlanLong;
  in.laneFree = false;
  EXPECT_EQ(pes::DribbleSubKind(in, true, Synthetic()), pes::kDribbleMode3);
}

TEST(PesCarrierDispatch, AnOverrideTurnsABlockedModeThreeIntoAHeading) {
  pes::DribbleChooserInput in = OpenChooser();
  in.laneFree = false;
  EXPECT_EQ(pes::DribbleSubKind(in, true, Synthetic()), pes::kDribbleHeading);
  in.laneFree = true;
  EXPECT_EQ(pes::DribbleSubKind(in, true, Synthetic()), pes::kDribbleMode4);
}

// --- the arbitration walk (0x14210f110): rules 1, 3, 5, 6, 9, 10 and the set-piece branch ---

namespace {
pes::UnitSlot Slot(int kind, int type, int sub = 0) {
  pes::UnitSlot s;
  s.kind = kind;
  s.answer = {type, sub};
  return s;
}
}  // namespace

TEST(PesCarrierWalk, TheTableOrderNotTheKindNumberDecides) {
  const std::vector<pes::UnitSlot> table = {Slot(4, pes::kAnswerPass, pes::kPassShort),
                                            Slot(0, pes::kAnswerShoot, 1)};
  EXPECT_EQ(pes::WalkCarrierUnits(table, pes::WalkContext{}).kind, 4);
}

TEST(PesCarrierWalk, KindTwentyTwoWinsOutsideSetPieces) {
  // The walk reads the table in its build order, and GKCoaching (22) is the second slot of every
  // list, ahead of Shoot (0): it answers before the shot does.
  const std::vector<pes::UnitSlot> table = {Slot(22, pes::kAnswerPass, pes::kPassShort),
                                            Slot(0, pes::kAnswerShoot, 1)};
  EXPECT_EQ(pes::WalkCarrierUnits(table, pes::WalkContext{}).kind, 22);
}

TEST(PesCarrierWalk, ThePKAndClearStatesVetoPassesButNotShots) {
  const std::vector<pes::UnitSlot> pass = {Slot(4, pes::kAnswerPass, pes::kPassShort)};
  pes::WalkContext c;
  c.restartState = 9;
  EXPECT_EQ(pes::WalkCarrierUnits(pass, c).kind, -1);
  c.restartState = 0;
  EXPECT_EQ(pes::WalkCarrierUnits(pass, c).kind, 4);
}

TEST(PesCarrierWalk, StoppedPlayAcceptsOnlyPasses) {
  pes::WalkContext c;
  c.restartMode = 2;
  EXPECT_EQ(pes::WalkCarrierUnits({Slot(0, pes::kAnswerShoot, 1)}, c).kind, -1);
  EXPECT_EQ(pes::WalkCarrierUnits({Slot(4, pes::kAnswerPass, pes::kPassShort)}, c).kind, 4);
}

TEST(PesCarrierWalk, ACutBackReplacesAShotOutsideTheCloseBand) {
  const std::vector<pes::UnitSlot> table = {Slot(0, pes::kAnswerShoot, 1),
                                            Slot(3, pes::kAnswerPass, pes::kPassThrough)};
  pes::WalkContext c;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 3);
  c.closeBand = true;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 0);
}

TEST(PesCarrierWalk, ACrossOverrideNeedsSkillFourteenAndAListedCross) {
  const std::vector<pes::UnitSlot> table = {Slot(4, pes::kAnswerPass, pes::kPassShort),
                                            Slot(7, pes::kAnswerPass, 2)};
  pes::WalkContext c;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 4);
  c.skill14 = true;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 7);
}

TEST(PesCarrierWalk, AnOpponentWithTheBallRejectsAPassButATeammateDoesNot) {
  const std::vector<pes::UnitSlot> table = {Slot(4, pes::kAnswerPass, pes::kPassShort)};
  pes::WalkContext c;
  c.ballOwnerTeam = 1;  // the opponent
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, -1);
  c.ballOwnerTeam = 0;  // the carrier's side
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 4);
}

TEST(PesCarrierWalk, InASetPieceTheKickerPassesOnlyWhenAllowed) {
  const std::vector<pes::UnitSlot> pass = {Slot(4, pes::kAnswerPass, pes::kPassShort)};
  pes::WalkContext c;
  c.restartMode = 3;
  c.carrierIsKicker = true;
  c.passAllowedInSetPiece = false;
  EXPECT_EQ(pes::WalkCarrierUnits(pass, c).kind, -1);
  c.carrierIsKicker = false;
  EXPECT_EQ(pes::WalkCarrierUnits(pass, c).kind, 4);
  c.carrierIsKicker = true;
  EXPECT_EQ(pes::WalkCarrierUnits({Slot(0, pes::kAnswerShoot, 1)}, c).kind, 0);
}

TEST(PesCarrierWalk, TheFirstTimeGateNeedsAHighBallOrTheSkillForAPass) {
  const std::vector<pes::UnitSlot> pass = {Slot(4, pes::kAnswerPass, pes::kPassShort)};
  pes::WalkContext c;
  c.firstTimeGate = true;
  EXPECT_EQ(pes::WalkCarrierUnits(pass, c).kind, -1);
  c.skill12 = true;
  EXPECT_EQ(pes::WalkCarrierUnits(pass, c).kind, 4);
}

TEST(PesCarrierWalk, AClearFallsBackToTheFirstListedPassKindInOrder) {
  // Rule 7 (0x14210F110, carunits5.c:330): with the forced-clear test failing,
  // a clear is replaced by the first of 3, 4, 9, 6 listed after it that holds
  // an answer - here 4, even though 9 also answers.
  const std::vector<pes::UnitSlot> table = {Slot(1, 5, 0), Slot(4, pes::kAnswerPass, 0),
                                            Slot(9, pes::kAnswerPass, 1)};
  pes::WalkContext c;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 4);
}

TEST(PesCarrierWalk, AForcedClearKeepsItsClearingKick) {
  // The same table, but the forced-clear test passes (surrounded): the clear
  // stands and no pass replaces it.
  const std::vector<pes::UnitSlot> table = {Slot(1, 5, 0), Slot(4, pes::kAnswerPass, 0)};
  pes::WalkContext c;
  c.forcedClear = true;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 1);
}

TEST(PesCarrierWalk, ASafetyBackPassToTheKeeperNeedsHimCloseOrOwned) {
  // Rule 7's keeper proviso: with the close band dropping Safety's own slot
  // too, an unowned back-pass to a far keeper leaves the clear standing, but
  // a near keeper takes it through the clear's fallback chain; owned always
  // takes it.
  const std::vector<pes::UnitSlot> table = {Slot(1, 5, 0), Slot(6, pes::kAnswerPass, 0)};
  pes::WalkContext c;
  c.safetyToKeeper = true;
  c.owned = false;
  c.closeBand = true;
  c.keeperWithinNinety = false;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 1);
  c.keeperWithinNinety = true;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 6);
  c.owned = true;
  c.keeperWithinNinety = false;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 6);
}
TEST(PesCarrierWalk, WithNothingAnsweringTheWalkReturnsNone) {
  EXPECT_EQ(pes::WalkCarrierUnits({}, pes::WalkContext{}).kind, -1);
}

TEST(PesCarrierArming, AFeintNeedsItsTeamCapability) {
  // Rule 11 (0x142111610): kind 17 answers only with the 0x1E team cap.
  EXPECT_FALSE(pes::UnitArmed(17, false, true, true, 0.0f, Synthetic()));
  EXPECT_TRUE(pes::UnitArmed(17, true, true, true, 0.0f, Synthetic()));
}

TEST(PesCarrierArming, ABackSpaceNeedsItsTeamCapability) {
  // Kind 3 needs 0x1C, kind 6 needs 0x1D.
  EXPECT_FALSE(pes::UnitArmed(3, true, false, true, 0.0f, Synthetic()));
  EXPECT_TRUE(pes::UnitArmed(3, true, true, true, 0.0f, Synthetic()));
  EXPECT_FALSE(pes::UnitArmed(6, true, true, false, 0.0f, Synthetic()));
  EXPECT_TRUE(pes::UnitArmed(6, true, true, true, 0.0f, Synthetic()));
}

TEST(PesCarrierArming, APassKindBeyondFiveMetresStaysDisarmed) {
  // The 5 m ball-distance gate (dist^2 <= 25): a pass kind farther out is
  // disarmed however the caps read.
  EXPECT_FALSE(pes::UnitArmed(4, true, true, true, 30.0f, Synthetic()));
  EXPECT_TRUE(pes::UnitArmed(4, true, true, true, 25.0f, Synthetic()));
}

TEST(PesCarrierWalk, ASafetyAnswerDiesInTheCloseBand) {
  // Rule 8 (0x14210F110, carunits5.c:393): a Safety pass in the close
  // shooting band is dropped.
  const std::vector<pes::UnitSlot> table = {Slot(6, pes::kAnswerPass, 0)};
  pes::WalkContext c;
  c.closeBand = true;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, -1);
  c.closeBand = false;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 6);
}

TEST(PesCarrierWalk, AForwardAnswerDiesOnAnInstantReturn) {
  // Rule 8 (carunits5.c:421): a Forward pass to the man who just passed to
  // the carrier (inside two frames) is dropped.
  const std::vector<pes::UnitSlot> table = {Slot(4, pes::kAnswerPass, 0)};
  pes::WalkContext c;
  c.forwardIsInstantReturn = true;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, -1);
  c.forwardIsInstantReturn = false;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, 4);
}

TEST(PesCarrierWalk, ANonConformingDribbleKindIsDropped) {
  // Rule 8's class filters (0x14063C9C0/0x14063C9A0): a dribble answer from
  // a kind that is neither the Dribble kind nor a pass kind is dropped.
  const std::vector<pes::UnitSlot> table = {Slot(0, pes::kAnswerDribble, 3)};
  pes::WalkContext c;
  EXPECT_EQ(pes::WalkCarrierUnits(table, c).kind, -1);
}

TEST(PesCarrierLongUnit, ANonOwnedCarrierWithNoDangerStaysSilent) {
  // Kind 9 (0x14212AD40): without the ball and no danger flag, no answer.
  pes::LongUnitInput in;
  in.mates = {{0, 0.0f, 180.0f, 5.0f}, {1, 10.0f, 180.0f, 5.0f}};
  EXPECT_EQ(pes::PassLongUnit(in, Synthetic()).index, -1);
}

TEST(PesCarrierLongUnit, TheMostAdvancedValidMateTakesIt) {
  // The picker (0x142128A20): past 10 m ahead the new mate takes it.
  pes::LongUnitInput in;
  in.owned = true;
  in.mates = {{0, 0.0f, 180.0f, 5.0f}, {1, 15.0f, 180.0f, 5.0f}};
  EXPECT_EQ(pes::PassLongUnit(in, Synthetic()).index, 1);
}

TEST(PesCarrierLongUnit, WithinTenMetresTheTallerMateTakesIt) {
  // Inside (-10, 10) the mate taller by more than 5 cm takes it.
  pes::LongUnitInput in;
  in.owned = true;
  in.mates = {{0, 0.0f, 180.0f, 5.0f}, {1, 5.0f, 190.0f, 5.0f}};
  EXPECT_EQ(pes::PassLongUnit(in, Synthetic()).index, 1);
}

TEST(PesCarrierLongUnit, AnOpenMateGetsAShortBall) {
  // 0x14212AD40's tail: openness class 4 or more goes short (b=0).
  pes::LongUnitInput in;
  in.owned = true;
  in.mates = {{0, 0.0f, 180.0f, 2.0f}, {1, 15.0f, 180.0f, 4.0f}};
  EXPECT_EQ(pes::PassLongUnit(in, Synthetic()).kind, pes::kPassShort);
}

TEST(PesCarrierCross, AWideCarrierAnswersACenteringBall) {
  // Kind 7 (0x142129590): the carry bearing held, the lane open, the
  // tallest mate takes the centering ball (b=2).
  pes::CrossInput in;
  in.mates = {{0, 0.0f, 180.0f, 0.0f}, {1, 5.0f, 190.0f, 0.0f}};
  EXPECT_EQ(pes::CrossUnit(in, Synthetic()).kind, pes::kPassCentering);
  EXPECT_EQ(pes::CrossUnit(in, Synthetic()).index, 1);
}

TEST(PesCarrierCross, ACenteringBallNeedsTheCarryBearing) {
  // Facing more than 130 degrees from the carry bearing, no answer.
  pes::CrossInput in;
  in.mates = {{0, 0.0f, 180.0f, 0.0f}};
  in.carryVsFacing_deg = 140.0f;
  EXPECT_EQ(pes::CrossUnit(in, Synthetic()).index, -1);
}

// --- the GoalFront unit (kind 2; gate 0x14212ABA0, picker 0x1421265C0) ---
//
// The unit runs the lane core itself, so its constants come from the install (PES21_ROOT, as
// pes_lane_test does) and this fixture overrides the picker's gates with crisp values: these tests
// pin the structure, the loader test pins the installed numbers.

namespace {

namespace lane = pes::lane;

using GoalFrontPlayers = std::array<lane::LanePlayer, lane::kSlots>;

constexpr int kGoalFrontCarrierSlot = 3;  // a home player, so side 0

pes::GoalFrontMate GoalFrontMate(int index) {
  pes::GoalFrontMate m;
  m.index = index;
  m.statusValid = true;
  m.difficulty = 0.5f;
  m.distance_m = 12.0f;
  m.angleToFacing_deg = 30.0f;
  m.openness = 50.0f;
  m.opennessClass = 3;
  m.pressure = 1.0f;
  m.zone = {10.0f, 0.0f, pes::kBandClose, pes::kSectorCentral, true};
  return m;
}

class GoalFrontUnitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root)
      GTEST_SKIP() << "PES21_ROOT unset: the unit runs the lane core, whose table is in the install";
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    auto loaded = pes::LoadCarrierConstants(*exe, &err);
    ASSERT_TRUE(loaded.has_value()) << err;
    k_ = *loaded;
    // Crisp gates. The lane request stays the install's (20 m, 80 deg, 1 m): the situation below
    // (an opponent 10 m dead ahead) is inside it for any sane length.
    k_.gfDifficultyMax = 1.0f;
    k_.gfAngle_deg = 70.0f;
    k_.gfStatAngle_deg = 100.0f;
    k_.gfFlagAngle_deg = 120.0f;
    k_.gfDistMin_m = 8.0f;
    k_.gfDistMax_m = 25.0f;
    k_.gfDistMinBand_m = 4.0f;
    k_.gfClass2Dist_m = 12.0f;
    k_.gfZoneDist_m = 14.0f;
    k_.gfStatFloor = 70.0f;
    k_.gfStatBonus = 20.0f;
    in_.players = GoalFrontPlayers{};
    in_.players[kGoalFrontCarrierSlot].pos = {0.0f, 0.0f};
    in_.players[kGoalFrontCarrierSlot].active = true;
    in_.carrierSlot = kGoalFrontCarrierSlot;
    in_.carrierPos = {0.0f, 0.0f};
    in_.laneHeading_deg = 0.0f;  // the lane runs along +x
    in_.carrierStat = 50.0f;
    in_.mates = {GoalFrontMate(5)};
  }

  void BlockTheLane() {
    // The lane's theta is measured over (x, z) with dir(theta) = (sin, cos), so heading 0 runs
    // along +z and the blocker sits 10 m up that axis.
    in_.players[14].pos = {0.0f, 10.0f};
    in_.players[14].active = true;
  }

  pes::CarrierConstants k_{};
  pes::GoalFrontInput in_{};
};

TEST_F(GoalFrontUnitTest, TheGateIsTheBlockedLaneOutsideTheCentralZone) {
  // 0x14212ABA0: the lane test silences the unit when the lane is free, and the central-zone
  // early-out silences it whatever the lane says.
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  BlockTheLane();
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
  in_.central = true;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
}

TEST_F(GoalFrontUnitTest, TheAngleGateHoldsFromTenMetresOut) {
  // 0x1421265C0: inside the distance gate's own floor the angle is not read at all, above it the
  // default 90 degree limit applies.
  BlockTheLane();
  in_.mates = {GoalFrontMate(5)};
  in_.mates[0].distance_m = 30.0f;  // over the band: the answer dies on the band, not the angle
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].distance_m = 20.0f;
  in_.mates[0].angleToFacing_deg = 75.0f;  // over the 70 limit
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].angleToFacing_deg = 65.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
  in_.mates[0].distance_m = 6.0f;
  in_.mates[0].angleToFacing_deg = 179.0f;  // inside the floor the angle is not consulted
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
}

TEST_F(GoalFrontUnitTest, TheStatRaisesTheLimitForAnAICarrier) {
  // 0x1421267FF: 70 < the carrier's stat + 20 (0x14063E960's bonus) moves the limit to 117.
  BlockTheLane();
  in_.mates[0].angleToFacing_deg = 95.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.carrierStat = 80.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
  // The bonus is added before the comparison, so 51 + 20 crosses the floor.
  in_.carrierStat = 51.0f;
  in_.statBonus = true;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
  // A human carrier never reads the stat: the flag path is his.
  in_.humanCarrier = true;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.flagSet = true;
  in_.flagDistFar = true;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
}

TEST_F(GoalFrontUnitTest, TheZoneAndPressureGates) {
  // The mate's own zone must be central inside 15 m with a non-zero pressure class.
  BlockTheLane();
  in_.mates[0].zone.sector = pes::kSectorWide;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].zone = {15.0f, 0.0f, pes::kBandClose, pes::kSectorCentral, true};
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);  // not inside 15 m
  in_.mates[0].zone = {10.0f, 0.0f, pes::kBandClose, pes::kSectorCentral, true};
  in_.mates[0].pressure = 0.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].pressure = 3.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
}

TEST_F(GoalFrontUnitTest, TheOpennessClassLadderAndTheDifficultyGate) {
  BlockTheLane();
  in_.mates[0].opennessClass = 1;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].opennessClass = 2;
  in_.mates[0].distance_m = 20.0f;  // class 2 needs less than 15 m
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].distance_m = 10.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
  in_.mates[0].opennessClass = 0;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].opennessClass = 3;
  in_.mates[0].difficulty = 1.5f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, -1);
  in_.mates[0].difficulty = 1.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
}

TEST_F(GoalFrontUnitTest, OnlyAStrictlyMoreOpenMateReplacesTheBest) {
  // 0x1421265C0's tail: the most open mate wins; an equal openness keeps the earlier player, and a
  // mate whose status byte fails never enters.
  BlockTheLane();
  in_.mates = {GoalFrontMate(5), GoalFrontMate(7)};
  in_.mates[1].openness = 50.0f;  // equal: the earlier player keeps it
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
  in_.mates[1].openness = 40.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
  in_.mates[1].openness = 90.0f;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 7);
  in_.mates[1].statusValid = false;
  EXPECT_EQ(pes::GoalFrontUnit(in_, k_).index, 5);
}

TEST_F(GoalFrontUnitTest, TheAnswerIsAShortPass) {
  // The answer slot writes b = 0 (the short pass).
  BlockTheLane();
  const pes::PassAnswer a = pes::GoalFrontUnit(in_, k_);
  EXPECT_EQ(a.index, 5);
  EXPECT_EQ(a.kind, pes::kPassShort);
}

TEST(PesCarrierTable, KindTwoCarriesOnlyTheGoalFrontAnswer) {
  // The table fill: kind 2 takes the GoalFront answer, in the slot the list gave it.
  const std::vector<int> kinds{4, 2};
  pes::PassAnswer fwd, lng, cross, goalFront;
  goalFront.index = 7;
  goalFront.kind = pes::kPassShort;
  const std::vector<pes::UnitSlot> table =
      pes::FillUnitTable(kinds, false, fwd, lng, cross, goalFront);
  EXPECT_EQ(table[0].answer.type, pes::kAnswerNone);
  EXPECT_EQ(table[1].answer.type, pes::kAnswerPass);
  EXPECT_EQ(table[1].answer.sub, pes::kPassShort);
  const std::vector<pes::UnitSlot> silent = pes::FillUnitTable({2}, false, fwd, lng, cross, {});
  EXPECT_EQ(silent[0].answer.type, pes::kAnswerNone);
}

// --- the BackSpace unit (kind 3; gate 0x142129110, validator 0x1421226C0, cut-back picker
// 0x142125140) ---
//
// Like GoalFront, the unit runs the lane core, so the constants come from the install and the
// fixture overrides the gates with crisp values: these tests pin the structure, the loader test
// pins the installed numbers.

constexpr int kBackSpaceCarrierSlot = 3;  // a home player, so side 0

pes::BackSpaceMate BackSpaceMate(int index) {
  pes::BackSpaceMate m;
  m.index = index;
  m.statusValid = true;
  m.teamNeed_m = 10.0f;    // over the 8 m gate-8 arm
  m.recBDist_m = 12.0f;    // inside the (5, 30) band, over the 10 m angle floor
  m.recBDifficulty = 0.5f;
  m.recBClass = 4;
  m.recAClass = 2.0f;
  m.pressureClass = 1.0f;  // at most 1: no b-drop
  m.slotStat = 1.0f;
  m.speed_kmh = 5.0f;
  return m;
}

class BackSpaceUnitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root)
      GTEST_SKIP() << "PES21_ROOT unset: the unit runs the lane core, whose table is in the install";
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    auto loaded = pes::LoadCarrierConstants(*exe, &err);
    ASSERT_TRUE(loaded.has_value()) << err;
    k_ = *loaded;
    // Crisp gates. The gate lane request stays the install's (20 m, 80 deg, 1 m).
    k_.bsOppDistSq_m2 = 36.0f;
    k_.bsBallHeight_m = 1.0f;
    k_.bsCornerDistSq_m2 = 225.0f;
    k_.bsPredictGain = 0.3f;
    k_.bsBoxSkipCount = 2.0f;
    k_.bsBoxSkipMargin_m = 5.0f;
    k_.bsAngleLimitGap_m = 6.0f;
    k_.bsAlignDist_m = 6.0f;
    k_.bsAlignNeed_m = 8.0f;
    k_.bsAlignAngle_deg = 45.0f;
    k_.bsNearBox_m = 20.0f;
    k_.bsNearSum_m = 40.0f;
    k_.bsFarDist_m = 20.0f;
    k_.bsAngleClass = 4.0f;
    k_.bsFacingLimit_deg = 90.0f;
    k_.bsHumanFacingLimit_deg = 120.0f;
    k_.bsLane1Len_m = 5.0f;
    k_.bsLane1Cone_deg = 30.0f;
    k_.bsLane1Rad_m = 2.0f;
    k_.bsLane2Cone_deg = 140.0f;
    k_.bsLane2Cap_deg = 20.0f;
    k_.bsLane2Floor_deg = -20.0f;
    k_.bsLane2Gain = 2.0f;
    k_.bsLane2XNear_m = 3.0f;
    k_.bsLane2XFar_m = 15.0f;
    k_.bsLane2XSpan_m = 12.0f;
    k_.bsLane2ZGain = 4.0f;
    k_.bsLane2Base_m = 6.0f;
    k_.bsLane2Pad_m = 2.0f;
    k_.bsLane2LenMin_m = 3.0f;
    k_.bsTailDistSq_m2 = 100.0f;
    k_.bsTailHalf = 0.5f;
    k_.bsTailFacing_deg = 90.0f;
    k_.bsTailHeading_deg = 30.0f;
    k_.bsTailPad_m = 1.0f;
    k_.cbDist_m = 15.0f;
    k_.cbSpeedBranch_kmh = 10.0f;
    k_.cbAlign_deg = 70.0f;
    k_.cbFastAlign_kmh = 14.0f;
    k_.cbCrowd_m = 3.0f;
    k_.cbBoxAngle_deg = 100.0f;
    k_.cbSpeedGate_kmh = 5.0f;
    k_.cbAdvance_m = 20.0f;
    // The passing situation: carrier at the origin facing +z, mate 5 at (0, 12) with his target
    // a metre further on, all static. The validator's lanes are empty, so it accepts; the gate
    // lane is empty, so the lane branch is skipped (mid = false).
    in_.players[kBackSpaceCarrierSlot].pos = {0.0f, 0.0f};
    in_.players[kBackSpaceCarrierSlot].active = true;
    in_.players[5].pos = {0.0f, 12.0f};
    in_.players[5].active = true;
    in_.carrierSlot = kBackSpaceCarrierSlot;
    in_.carrierPos = {0.0f, 0.0f};
    in_.carrierFacing_deg = 0.0f;
    in_.carrierHeading_deg = 180.0f;  // against the mate's: the align gate passes
    in_.carrierSlotStat = 1.0f;
    in_.carrierNeed_m = 3.0f;  // under the mate's: the advance gate passes
    in_.attackSign = 1.0f;
    in_.teamLineOpp = 60.0f;  // the side test passes for any mate on the pitch
    in_.pitchBound8 = 60.0f;
    in_.pitchHalfLen_m = 52.5f;
    in_.pitchF6 = 52.5f;
    in_.ball = {0.0f, 0.0f};
    in_.owned = true;
    in_.plan = pes::kPlanPassThrough;
    in_.planRecordTarget = 5;
    in_.planRecordBytesOk = true;
    in_.curveGate = true;  // the mid-pitch target is far from the corner, so the curve arm passes
    pes::BackSpaceMate m = BackSpaceMate(5);
    m.pos = {0.0f, 12.0f};
    m.heading_deg = 0.0f;
    m.recBTarget = {0.0f, 13.0f};
    in_.mates = {m};
  }

  pes::CarrierConstants k_{};
  pes::BackSpaceInput in_{};
};

TEST_F(BackSpaceUnitTest, PlanFourAnswersThePlanRecordTargetThrough) {
  // 0x142129110's plan-4 arm: the validated plan-record target takes a through ball (b = 3).
  const pes::PassAnswer a = pes::BackSpaceUnit(in_, k_);
  EXPECT_EQ(a.index, 5);
  EXPECT_EQ(a.kind, pes::kPassThrough);
}

TEST_F(BackSpaceUnitTest, ThePlanRecordBytesKillTheAnswer) {
  // The two plan-record bytes ([1] == 0, [0] != 0) gate the plan-4 answer before the validator.
  in_.planRecordBytesOk = false;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
}

TEST_F(BackSpaceUnitTest, TheValidatorRejectsABadStatus) {
  // 0x1421226C0 gate 1: the target's b6c status must be 0xB.
  in_.mates[0].statusValid = false;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
}

TEST_F(BackSpaceUnitTest, TheValidatorRejectsADifficultTarget) {
  // 0x1421226C0 gates 3/9: record B's difficulty over 1.0 rejects (no danger).
  in_.mates[0].recBDifficulty = 1.5f;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
  in_.danger = true;  // danger skips gate 3 but not the gate-9 re-read
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
}

TEST_F(BackSpaceUnitTest, TheValidatorRejectsAlignedHeadings) {
  // 0x1421226C0 gate 8: headings agreeing within 45 degrees reject (need under 8 m, dist over 6).
  in_.mates[0].teamNeed_m = 5.0f;
  in_.carrierHeading_deg = 10.0f;  // within 45 of the mate's 0
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
  in_.carrierHeading_deg = 180.0f;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
  // Both comparisons are strict: exactly 8 m need, or exactly 45 degrees apart, passes.
  in_.mates[0].teamNeed_m = 8.0f;
  in_.carrierHeading_deg = 10.0f;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
  in_.mates[0].teamNeed_m = 5.0f;
  in_.carrierHeading_deg = 45.0f;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
}

TEST_F(BackSpaceUnitTest, TheValidatorLaneOneRejectsABlocker) {
  // 0x1421226C0 gate 11: an opponent in the 5 m / 30 deg lane from the ball along the record
  // angle rejects. The record angle is 0 (along +z), so the blocker sits 3 m up that axis.
  in_.players[14].pos = {0.0f, 3.0f};
  in_.players[14].active = true;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
}

TEST_F(BackSpaceUnitTest, OtherPlansAnswerTheCutBackPicker) {
  // 0x142125140: outside plan 4 the picker takes the validated mate (lane B free, distance
  // short): a through ball too.
  in_.plan = pes::kPlanBasic;
  in_.mates[0].pos = {0.0f, 8.0f};
  in_.mates[0].recBTarget = {0.0f, 9.0f};
  in_.players[5].pos = {0.0f, 8.0f};
  const pes::PassAnswer a = pes::BackSpaceUnit(in_, k_);
  EXPECT_EQ(a.index, 5);
  EXPECT_EQ(a.kind, pes::kPassThrough);
}

TEST_F(BackSpaceUnitTest, TheBDropNeedsCloseAndPressureAndClass) {
  // 0x142129110's tail: the through ball drops to short only when all three exceed 1.
  in_.plan = pes::kPlanBasic;
  in_.mates[0].pos = {0.0f, 8.0f};
  in_.mates[0].recBTarget = {0.0f, 9.0f};
  in_.players[5].pos = {0.0f, 8.0f};
  in_.mates[0].closeTest = true;
  in_.mates[0].pressureClass = 2.0f;
  in_.mates[0].recAClass = 2.0f;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).kind, pes::kPassShort);
  in_.mates[0].pressureClass = 1.0f;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).kind, pes::kPassThrough);
}

TEST_F(BackSpaceUnitTest, TheLaneBranchSkipsWhenAnOpponentIsFar) {
  // 0x142129110's lane branch: Mid zone, free gate lane, and the nearest opponent's 9-frame
  // prediction past 6 m from the ball means no answer at all.
  in_.mid = true;
  in_.players[14].pos = {0.0f, 30.0f};
  in_.players[14].active = true;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
  in_.players[14].pos = {5.0f, 0.0f};  // inside 6 m: the branch falls through to the answer
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
}

TEST_F(BackSpaceUnitTest, TheCornerTestPassesNearTheCorner) {
  // 0x142129110's plan-4 arm: a record-B target within 15 m of the attack corner (+52.5, 0)
  // passes without the curve gate.
  in_.curveGate = false;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);  // mid-pitch target, no curve arm
  in_.carrierPos = {42.0f, 5.0f};
  in_.ball = {42.0f, 5.0f};
  in_.players[kBackSpaceCarrierSlot].pos = {42.0f, 5.0f};
  in_.mates[0].recBTarget = {50.0f, 5.0f};  // d2 = 31.25 < 225
  in_.mates[0].pos = {48.0f, 5.0f};
  in_.players[5].pos = {48.0f, 5.0f};
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
}

}  // namespace
