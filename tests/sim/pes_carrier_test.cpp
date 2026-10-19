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
  k.intentionClose_deg = 100.0f;  // 0x14253ee30
  k.intentionLead_m = 10.0f;      // 0x142588210
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

TEST(PesCarrierZone, CaseBIsTheGoalLineDistanceToTheAbsoluteBallX) {
  // R4 case B (pf[2] = 3.66 >= |K.z|), the fall-through of 0x140698D10: `movss pf[0]` (52.56) at
  // 0x140698ECA, `mulss sign` (bp+0x30) at 0x140698ED5, `subss` at 0x140698EDA, and `andps` (the
  // 0x142565D30 absolute-value mask) at 0x140698EE1 before the store at 0x140698EE9. So
  // +0x170 = |52.56 - sign*K.x|. The 8-byte zero at 0x140698EC3 clears +0x174 and +0x178, so the
  // sector is 0 in case B.
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.56f, GOAL_HALF = 3.66f;
  // Attacking +x, the ball 12.56 m short of the line, inside the mouth band.
  const pes::ShootZone atk =
      pes::ComputeShootZone(40.0f, 0.0f, 1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_FLOAT_EQ(atk.d_m, 12.56f);
  EXPECT_EQ(atk.sector, pes::kSectorCentral);
  EXPECT_TRUE(pes::IsClose(atk));  // 12.56 <= 0x188 (18.0)
  // The other half: attacking -x with the ball 10 m on the far side of the centre. sign*K.x = 10, so
  // the distance is 52.56 - 10 = 42.56.
  const pes::ShootZone def =
      pes::ComputeShootZone(-10.0f, 0.0f, -1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_FLOAT_EQ(def.d_m, 42.56f);
  EXPECT_FALSE(pes::IsClose(def));
  // Past the goal line the signed form would go negative; the audited formula takes the absolute
  // value (the `andps` at 0x140698EE1), so the distance is 2.44 either way.
  EXPECT_NEAR(pes::ComputeShootZone(55.0f, 0.0f, 1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k).d_m,
              2.44f, 1e-4f);
  EXPECT_NEAR(pes::ComputeShootZone(-55.0f, 0.0f, -1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k).d_m,
              2.44f, 1e-4f);
}

TEST(PesCarrierZone, CaseAMeasuresToTheNearPost) {
  // R4 case A (pf[2] < |K.z|): `+0x170 = |K - P|` with P = (sign*pf[0], 0, sgn(K.z)*pf[2]) - the near
  // post, no radius (0x140698F09..F22: the point is built on the stack and FUN_140A8F820 is the 3D
  // distance). Ball (40, 10) attacking +x: P = (52.56, 0, 3.66).
  const pes::CarrierConstants k = Synthetic();
  constexpr float HALF_LEN = 52.56f, GOAL_HALF = 3.66f;
  const pes::ShootZone z =
      pes::ComputeShootZone(40.0f, 10.0f, 1.0f, 70.0f, false, HALF_LEN, GOAL_HALF, k);
  EXPECT_FLOAT_EQ(z.d_m, std::sqrt(12.56f * 12.56f + 6.34f * 6.34f));
  // The bearing K -> P folded to [0, 360) is 333.2, so the signed angle is -26.8: inside the 40
  // degree side band, on the low side (R4's 15.0/40.0 pair at 0x140699688/0x140699692).
  EXPECT_EQ(z.sector, pes::kSectorSideLow);
  // 14.07 m at a side sector uses the 16.0 threshold (R4's +0x190), so this one is close.
  EXPECT_TRUE(pes::IsClose(z));
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
  const std::vector<pes::UnitSlot> table = pes::FillUnitTable(kinds, false, fwd, lng, cross, {}, {});
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
  const std::vector<pes::UnitSlot> table = pes::FillUnitTable(kinds, false, fwd, lng, cross, {}, {});
  EXPECT_EQ(pes::WalkCarrierUnits(table, pes::WalkContext{}).kind, 7);
}

TEST(PesCarrierTable, KindThreeCarriesOnlyTheBackSpaceAnswer) {
  // The table fill: kind 3 takes the BackSpace answer now, not the cross answer behind a plan flag.
  const std::vector<int> kinds{3, 4};
  pes::PassAnswer fwd, lng, cross, goalFront, backSpace;
  backSpace.index = 3;
  backSpace.kind = pes::kPassThrough;
  const std::vector<pes::UnitSlot> table =
      pes::FillUnitTable(kinds, false, fwd, lng, cross, goalFront, backSpace);
  EXPECT_EQ(table[0].answer.type, pes::kAnswerPass);
  EXPECT_EQ(table[0].answer.sub, pes::kPassThrough);
  const std::vector<pes::UnitSlot> silent =
      pes::FillUnitTable({3}, false, fwd, lng, cross, goalFront, {});
  EXPECT_EQ(silent[0].answer.type, pes::kAnswerNone);
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
      pes::FillUnitTable(kinds, false, fwd, lng, cross, goalFront, {});
  EXPECT_EQ(table[0].answer.type, pes::kAnswerNone);
  EXPECT_EQ(table[1].answer.type, pes::kAnswerPass);
  EXPECT_EQ(table[1].answer.sub, pes::kPassShort);
  const std::vector<pes::UnitSlot> silent = pes::FillUnitTable({2}, false, fwd, lng, cross, {}, {});
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

pes::Curve FlatCurve(float x_m) {
  pes::Curve c;
  c.x.fill(x_m);
  return c;
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
    in_.curve = FlatCurve(-100.0f);  // far behind every mate: the curve arm passes
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
  in_.curve.reset();
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);  // mid-pitch target, no curve arm
  in_.carrierPos = {42.0f, 5.0f};
  in_.ball = {42.0f, 5.0f};
  in_.players[kBackSpaceCarrierSlot].pos = {42.0f, 5.0f};
  in_.mates[0].recBTarget = {50.0f, 5.0f};  // d2 = 31.25 < 225
  in_.mates[0].pos = {48.0f, 5.0f};
  in_.players[5].pos = {48.0f, 5.0f};
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
}

TEST_F(BackSpaceUnitTest, TheCurveGateAdmitsAMateOnTheCurveAndRefusesOneBehind) {
  // 0x1421225E0: k*mate.x >= k*curve(mate.z), frames 0. The mate stands at (0, 12): a curve at x = 0 admits
  // him (the boundary is inclusive), a curve at x = 0.5 lies past him. The record target is far from the
  // attack corner, so the corner arm is shut and only the curve can admit him.
  pes::Curve c;
  c.x.fill(0.0f);
  in_.curve = c;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
  c.x.fill(0.5f);
  in_.curve = c;
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, -1);
}

// --- the curve table (R2): the reader is exact on any table; the builder is checked on known defenders.
TEST(PesCarrierCurve, CurveEvalInterpolatesTheSamplesAndClampsTheEnds) {
  // 0x142139650: u = clamp(z*scale + 35, 0, 70); u == 70 returns x[70]; else the lerp of x[floor u] and the next.
  pes::Curve c;
  for (int i = 0; i < pes::kCurveSamples; ++i) c.x[i] = 0.1f * static_cast<float>(i);
  EXPECT_NEAR(pes::CurveEval(c, 0.0f), 3.5f, 1e-5f);     // u = 35: x[35]
  EXPECT_NEAR(pes::CurveEval(c, 0.5f), 3.55f, 1e-5f);    // u = 35.5: halfway from x[35] to x[36]
  EXPECT_NEAR(pes::CurveEval(c, 35.0f), 7.0f, 1e-5f);    // u = 70: the end value x[70]
  EXPECT_NEAR(pes::CurveEval(c, 100.0f), 7.0f, 1e-5f);   // clamped at 70
  EXPECT_NEAR(pes::CurveEval(c, -100.0f), 0.0f, 1e-5f);  // clamped at 0
  c.scale = -1.0f;
  EXPECT_NEAR(pes::CurveEval(c, -0.5f), 3.55f, 1e-5f);   // u = 35.5 with the sign reversed
}

class CurveTableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root)
      GTEST_SKIP() << "PES21_ROOT unset: the table's arm angle, arm length and cone are read from the install";
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    auto loaded = pes::LoadCarrierConstants(*exe, &err);
    ASSERT_TRUE(loaded.has_value()) << err;
    k_ = *loaded;
    // The home side attacks +x (psi 90, sign +1); its opponents are slots 11..21. One of them, slot 11,
    // is a field player at (20, 0).
    players_[11].pos = {20.0f, 0.0f};
    players_[11].active = true;
    players_[11].positionCategory = 1;
  }

  pes::CarrierConstants k_{};
  std::array<pes::lane::LanePlayer, pes::lane::kSlots> players_{};
};

TEST_F(CurveTableTest, ADefenderBendsTheCurveBackAtFortyDegreesFromHisPoint) {
  // His segment point is 1 m on along the attack, (21, 0). Its two arms leave at psi - 180 +- 50 degrees,
  // so the lateral sample z meets the arm on its own side at x = 21 - |z| * tan(40 degrees).
  constexpr float kTan40 = 0.8391f;  // tan(40 degrees)
  const auto c = pes::BuildCurve(players_, /*carrierSide*/ 0, /*attackSign*/ 1.0f, /*psi*/ 90.0f, 52.5f, k_);
  ASSERT_TRUE(c.has_value());
  EXPECT_NEAR(c->x[35], 21.0f, 1e-3f);
  EXPECT_NEAR(c->x[45], 21.0f - 10.0f * kTan40, 1e-3f);
  EXPECT_NEAR(c->x[25], 21.0f - 10.0f * kTan40, 1e-3f);
  EXPECT_NEAR(c->x[0], 21.0f - 35.0f * kTan40, 1e-3f);
  EXPECT_FLOAT_EQ(c->scale, 1.0f);
}

TEST_F(CurveTableTest, AKeeperIsNotListed) {
  // The side's keeper (category 0) at (40, 0) would pull the curve forward; the builder skips him.
  players_[12].pos = {40.0f, 0.0f};
  players_[12].active = true;
  players_[12].positionCategory = 0;
  const auto c = pes::BuildCurve(players_, 0, 1.0f, 90.0f, 52.5f, k_);
  ASSERT_TRUE(c.has_value());
  EXPECT_NEAR(c->x[35], 21.0f, 1e-3f);
}

TEST_F(CurveTableTest, AttackingTheOtherWayMirrorsTheCurve) {
  // The away side attacks -x (psi 270, sign -1). Its opponents are slots 0..10; the defender is at (-20, 0).
  constexpr float kTan40 = 0.8391f;  // tan(40 degrees)
  players_[11].active = false;
  players_[0].pos = {-20.0f, 0.0f};
  players_[0].active = true;
  players_[0].positionCategory = 1;
  const auto c = pes::BuildCurve(players_, /*carrierSide*/ 1, /*attackSign*/ -1.0f, /*psi*/ 270.0f, 52.5f, k_);
  ASSERT_TRUE(c.has_value());
  EXPECT_NEAR(c->x[35], -21.0f, 1e-3f);
  EXPECT_NEAR(c->x[45], -21.0f + 10.0f * kTan40, 1e-3f);
  EXPECT_FLOAT_EQ(c->scale, -1.0f);
}

TEST_F(CurveTableTest, NobodyListedGivesNoTable) {
  players_[11].active = false;
  EXPECT_FALSE(pes::BuildCurve(players_, 0, 1.0f, 90.0f, 52.5f, k_).has_value());
}

// --- the Safety unit (kind 6; gate 0x14212B520, short picker 0x142126B70, through picker
// 0x142128490) ---
//
// Like the other lane units, the constants come from the install and the fixture overrides the
// gates with crisp values.

constexpr int kSafetyCarrierSlot = 3;  // a home player, so side 0

pes::SafetyMate SafetyMate(int index) {
  pes::SafetyMate m;
  m.index = index;
  m.statusValid = true;
  m.roleClass = 4;
  m.recATurnAngle_deg = 90.0f;  // over 80, under 110
  m.recAVsFacing_deg = 30.0f;
  m.recADist_m = 12.0f;
  m.recADifficulty = 0.5f;
  m.recAClass = 3;
  m.recBVsFacing_deg = 30.0f;
  m.recBDist_m = 12.0f;
  m.recBDifficulty = 0.5f;
  m.recBClass = 3;
  m.pressureClass = 3.0f;  // over the carrier's: the pressure gate passes
  m.speed_kmh = 5.0f;
  m.nearestOppD2 = 900.0f;  // 30 m: over the 225 floor, under the 35 m gate
  m.score = 900.0f;  // the S1 row d^2, over the 225 floor (gate 4 reads it, not nearestOppD2)
  m.zoneMid = true;
  return m;
}

class SafetyUnitTest : public ::testing::Test {
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
    // Crisp gates. The gate lane's cone stays the install's (80 deg); its radius is overridden.
    k_.sfFacingTurn_deg = 135.0f;
    k_.sfLaneRad_m = 3.0f;
    k_.sfKeeperDist_m = 15.0f;
    k_.sfKeeperFar_m = 40.0f;
    k_.sfKeeperScalar = 1.5f;
    k_.sfKeeperSpeedCap = 60.0f;
    k_.sfOppD2Min = 225.0f;
    k_.sfScoreDist_m = 10.0f;
    k_.sfTurnOver_deg = 110.0f;
    k_.sfTurnUnder_deg = 80.0f;
    k_.sfClass4Bonus = 20.0f;
    k_.sfNeedDanger = 120.0f;
    k_.sfNeedCap = 135.0f;
    k_.sfNeedFloor = 45.0f;
    k_.sfSpeedFloor_kmh = 5.0f;
    k_.sfSpeedCeil_kmh = 25.0f;
    k_.sfSpeedSpan_kmh = -20.0f;
    k_.sfDistCeil_m = 30.0f;
    k_.sfNeedBase = 60.0f;
    k_.sfNeedGain = 30.0f;
    k_.sfClass5Bonus = 30.0f;
    k_.sfTurnAttack_deg = 90.0f;
    k_.sfTurnFace_deg = 45.0f;
    k_.sfCeilDefault_m = 40.0f;
    k_.sfCeilNear_m = 15.0f;
    k_.sfCeilFar_m = 60.0f;
    k_.sfCeilOpp_m = 100.0f;
    k_.sfMateNear_m = 5.0f;
    k_.sfMateSpeed_kmh = 20.0f;
    k_.sfFrameSpeed = 80.0f;
    k_.sfOppGate_m = 35.0f;
    k_.sfLimGain = 0.4f;
    k_.sfLimNear_m = 10.0f;
    k_.sfLimFar_m = 20.0f;
    k_.sfLimMin_m = 3.0f;
    k_.sfOppPad_m = 2.0f;
    k_.sfKeepPad_m = 5.0f;
    k_.sfOuterTurn_deg = 50.0f;
    k_.sfPressDist_m = 15.0f;
    k_.sfLaneDivisor = 5.0f;
    k_.sfLaneLenMin_m = 4.0f;
    k_.sfLaneLenMax_m = 12.0f;
    k_.sfLaneCone_deg = 220.0f;
    k_.sfTieDiff_deg = 130.0f;
    k_.sfTiePress = 9.0f;
    k_.sfTieCornerSq = 1600.0f;
    k_.sfTieGap_m = 5.0f;
    k_.sfThruIdle = 3.0f;
    k_.sfThruPress = 9.0f;
    k_.sfThruCarrierSq = 1600.0f;
    k_.sfThruRowSq = 900.0f;
    k_.sfThruAlign_deg = 70.0f;
    k_.sfThruAim_deg = 135.0f;
    k_.sfThruClass = 3.0f;
    k_.sfThruFacing_deg = 60.0f;
    k_.sfThruNearSq = 100.0f;
    k_.sfThruFarSq = 900.0f;
    k_.sfThruLaneLen_m = 10.0f;
    k_.sfThruLaneCone_deg = 60.0f;
    k_.sfThruLaneRad_m = 5.0f;
    // The passing situation: k = 0 (no pressure), so the short branch answers. The gate lane
    // must be BLOCKED: the opponent at (0, 5) sits in the 80-deg cone about heading 0 (along
    // +z) with len = 52.5 (his distance to his own goal, the S1 row). Mate 5 at (0, -8) survives the short picker: turn 90
    // (under-80 false, over-110 false), dist 12 (window [5, 40], scored limit max(need, 45):
    // carrier speed 0 -> wS 1, dist 12 -> wD (12-30)/(-20) = 0.9, need = 30+60+27 = 117,
    // lim = 117 >= vsFacing 30), class 3 (floor 3, b28: pressClass 0), the one-touch lane free
    // (no opponents near (0,-8)), and the first-candidate tiebreak.
    in_.players[kSafetyCarrierSlot].pos = {0.0f, 0.0f};
    in_.players[kSafetyCarrierSlot].active = true;
    in_.players[5].pos = {0.0f, -8.0f};
    in_.players[5].active = true;
    in_.players[14].pos = {0.0f, 5.0f};
    in_.players[14].active = true;
    in_.carrierSlot = kSafetyCarrierSlot;
    in_.carrierPos = {0.0f, 0.0f};
    in_.carrierFacing_deg = 0.0f;
    in_.carrierSpeed_kmh = 0.0f;
    in_.laneHeading_deg = 0.0f;
    // The gate lane's length: the carrier's d^2 in the row for sign !k (0x140A714A0): the goal at +xg.
    in_.carrierGateD2_m2 = 52.5f * 52.5f;
    in_.attackSign = 1.0f;
    in_.sideAngle_deg = 0.0f;
    in_.ball = {0.0f, 0.0f};
    in_.carrierPressClass = 0;
    in_.teamLineB = 60.0f;
    in_.teamLineOpp = 60.0f;
    in_.pitchHalfLen_m = 52.5f;
    in_.pitchBound8 = 60.0f;  // F[8]: the validator's gate 3 bound on the target's x
    in_.planHeading_deg = 0.0f;
    pes::SafetyMate m = SafetyMate(5);
    m.pos = {0.0f, -8.0f};
    in_.mates = {m};
  }

  pes::CarrierConstants k_{};
  pes::SafetyInput in_{};
};

TEST_F(SafetyUnitTest, NoPressureAnswersTheShortMate) {
  // 0x14212B520 with k = 0: the short picker's mate takes a short ball (b = 0).
  const pes::PassAnswer a = pes::SafetyUnit(in_, k_);
  EXPECT_EQ(a.index, 5);
  EXPECT_EQ(a.kind, pes::kPassShort);
}

TEST_F(SafetyUnitTest, AFreeGateLaneMeansNoAnswer) {
  // 0x14212B520: the gate lane free (blocker removed) returns with no answer.
  in_.players[14].active = false;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
}

TEST_F(SafetyUnitTest, AnOpponentOffTheGateLaneLeavesTheLaneFree) {
  // The gate lane's cone decides the block, not the nearest opponent's distance alone: the same
  // nearest opponent 9.4 m out, 45 degrees off the lane's heading (past the 40-degree half cone),
  // leaves the lane free.
  in_.players[14].pos = {8.0f, 5.0f};
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
}

TEST_F(SafetyUnitTest, TheScoredLimitRejectsATurnedMate) {
  // 0x142126B70's scoring: vsFacing 130 over lim max(117, 45) = 117 rejects.
  in_.mates[0].recAVsFacing_deg = 130.0f;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
  in_.mates[0].recAVsFacing_deg = 100.0f;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, 5);
}

TEST_F(SafetyUnitTest, TheCeilingCascadeRejectsAFarMate) {
  // 0x142126B70's ceiling: dist 50 over the default 40 rejects.
  in_.mates[0].recADist_m = 50.0f;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
}

TEST_F(SafetyUnitTest, TheOneTouchLaneRejectsABlockedMate) {
  // 0x142126B70's lane: an opponent in the 220-deg cone about the ball-advanced origin
  // 0x142126B70's one-touch lane: origin the ball advanced 12 m along his heading (0, toward +z),
  // aimed back at the ball, length clamp(12 / 5, 4, 12) = 4, so it covers z in [8, 12] at x = 0.
  // An opponent at (0, 10) sits in that lane and blocks the mate.
  in_.players[15].pos = {0.0f, 10.0f};
  in_.players[15].active = true;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
}

TEST_F(SafetyUnitTest, PressureRunsTheThroughPicker) {
  // 0x14212B520 with k = 2 (press+0x30 > 0): the through picker's mate takes a through ball
  // (b = 3). The through run needs: idle gate (pressCount4 <= 162, or its pressure arm),
  // carrier 40+ m from his nearest opp, carrier advanced, the row scan reaching mate 5, and
  // the final lane free.
  in_.pressCount1 = 1;
  in_.pressCount4 = 0;
  in_.players[14].pos = {0.0f, 50.0f};  // carrier's nearest opp 50 m out
  in_.players[15].active = false;
  in_.mates[0].pos = {10.0f, -8.0f};  // advanced: k*x = 10 > 0
  in_.players[5].pos = {10.0f, -8.0f};
  in_.mates[0].recBTarget = {14.0f, -8.0f};
  in_.mates[0].recBDist_m = 12.0f;
  in_.mates[0].recBClass = 4;  // over the through picker's class-3 floor; the validator's facing gate skips
  const pes::PassAnswer a = pes::SafetyUnit(in_, k_);
  EXPECT_EQ(a.index, 5);
  EXPECT_EQ(a.kind, pes::kPassThrough);
}

TEST_F(SafetyUnitTest, TheShortGateReadsTheScoreRowNotTheNearestOpponent) {
  // Gate 4 (0x14259BE70 = 225) reads 0x140587E70, the S1 score row of the attack sign: the mate's
  // d^2 to the goal behind the team. His nearest opponent is 2 m away (nearestOppD2 = 4), which the
  // row does not see: he is 50 m from that goal, so the gate passes him.
  in_.mates[0].nearestOppD2 = 4.0f;
  in_.mates[0].score = 2500.0f;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, 5);
}

TEST_F(SafetyUnitTest, AScoredNeedOverTheCapIsHeldTo135NotTheFloor) {
  // 0x1421271C1-C6 (raw asm): a non-keeper whose need is over 135 gets lim = 135. A class-5 mate 12 m out with a still
  // carrier needs 30 + 60 + 27 + 30 = 147: a turn of 140 is over it, a turn of 100 is inside it.
  in_.mates[0].recAClass = 5;
  in_.mates[0].recAVsFacing_deg = 140.0f;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
  in_.mates[0].recAVsFacing_deg = 100.0f;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, 5);
}

TEST_F(SafetyUnitTest, AKeeperIsHeldTo45WhateverHisNeed) {
  // 0x1421271BC (raw asm): a keeper's limit is the 45 floor, whatever his need; a turn of 100 is over it.
  in_.mates[0].roleClass = 0;
  in_.mates[0].recAClass = 5;
  in_.mates[0].recAVsFacing_deg = 100.0f;
  in_.pressCount1 = 1;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
  in_.mates[0].recAVsFacing_deg = 40.0f;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, 5);
}

TEST_F(SafetyUnitTest, TheGoalMouthFlagSilencesTheUnit) {
  // 0x14212B520: +0x31C0 set returns with no answer whatever the pickers say.
  in_.goalMouth = true;
  EXPECT_EQ(pes::SafetyUnit(in_, k_).index, -1);
}

// --- the OneTwo unit (kind 10; gate 0x14212AEB0, picker 0x142126A80) and the one-touch predicate
// 0x142128C10 it needs. The one-touch lane is the lane core, so the constants come from the install
// (PES21_ROOT). One situation passes every gate; each test breaks one. The carrier (home slot 3)
// stands at (30, 0) and attacks +x (attack angle 90 degrees); positions are metres.

constexpr int kOneTwoCarrier = 3;
constexpr int kOneTwoMate = 5;
constexpr int kOneTwoOpp = 14;
constexpr float kOneTwoHalfLen = 52.5f;

pes::TouchReceiver OneTwoMateAt(int index, float x, float z, float facing) {
  pes::TouchReceiver r;
  r.index = index;
  r.pos = {x, z};
  r.facing_deg = facing;
  r.statusValid = true;
  r.roleClass = 1;  // a field man: class 1, not the keeper
  r.difficulty = 0.5f;
  r.dist_m = 14.0f;
  r.turnFacing_deg = 30.0f;   // within 60 degrees: the window's ramp is full
  r.attackAngle_deg = 20.0f;  // under 120 degrees: the near window base
  r.opennessClass = 3;
  return r;
}

class OneTwoUnitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root)
      GTEST_SKIP() << "PES21_ROOT unset: the one-touch lane runs the lane core, whose table is in the install";
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    auto loaded = pes::LoadCarrierConstants(*exe, &err);
    ASSERT_TRUE(loaded.has_value()) << err;
    k_ = *loaded;
    pes::TouchInput& t = in_.touch;
    t.players[kOneTwoCarrier].pos = {30.0f, 0.0f};
    t.players[kOneTwoCarrier].active = true;
    t.players[kOneTwoOpp].pos = {38.0f, 0.0f};  // in the one-touch lane, 8 m out, outside the closing annulus
    t.players[kOneTwoOpp].active = true;
    t.players[kOneTwoMate].pos = {40.0f, 10.0f};
    t.players[kOneTwoMate].active = true;
    t.carrierSlot = kOneTwoCarrier;
    t.carrierPos = {30.0f, 0.0f};
    t.attackSign = 1.0f;
    t.attackAngle_deg = 90.0f;
    t.carrierFacing_deg = 90.0f;
    t.planHeading_deg = 90.0f;  // the one-touch lane runs along +x
    t.planMode = 2;
    t.ownLine_m = -kOneTwoHalfLen;
    t.pitchBound_m = kOneTwoHalfLen;
    t.teamLevelRaw = 200;  // level 2 (x 100): G3 passes
    t.teamMode = 3;
    t.score = 2500.0f;     // 50 m: over the 40 m floor
    t.curve = FlatCurve(35.0f);  // the carrier 5 m behind the front line
    in_.carrierAngle_deg = 90.0f;
    in_.mates = {OneTwoMateAt(kOneTwoMate, 40.0f, 10.0f, 225.0f)};
  }

  pes::CarrierConstants k_{};
  pes::OneTwoInput in_{};
};

TEST_F(OneTwoUnitTest, TheFirstReceiverTakesAShortOneTouchBall) {
  // 0x14212AEB0: the angle gate passes, the one-touch holds, and the picker takes the first receiver.
  // The answer is a short ball (b = 0) with the one-touch flag.
  const pes::PassAnswer a = pes::OneTwoUnit(in_, k_);
  EXPECT_EQ(a.index, kOneTwoMate);
  EXPECT_EQ(a.kind, pes::kPassShort);
  EXPECT_TRUE(a.oneTouch);
}

TEST_F(OneTwoUnitTest, TheAngleGateIsInclusiveAtNinetyDegrees) {
  // 0x14212AF76 (JBE): |wrap180(K+0x34 - C[4])| of exactly 90 passes; past it, with no closing
  // opponent, nothing does.
  in_.carrierAngle_deg = 180.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
  in_.carrierAngle_deg = 181.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
}

TEST_F(OneTwoUnitTest, AClosingOpponentOpensTheGateAgainstTheAngle) {
  // 0x140A4E720: an opponent 4 m ahead of the carrier's 16-frame point, inside the attack bearing,
  // counts even though the carrier faces away from the attack.
  in_.carrierAngle_deg = 270.0f;  // 180 degrees off: the angle gate is shut
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.touch.players[kOneTwoOpp].pos = {34.0f, 0.0f};  // d2 = 16, inside (0, 36)
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(OneTwoUnitTest, AReceiverWhoDoesNotFaceTheCarrierIsRejected) {
  // 0x142124950's M[r][c]: the receiver's facing sits within 120 degrees of the bearing to the carrier.
  in_.mates[0].facing_deg = 0.0f;  // 135 degrees off the carrier
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
}

TEST_F(OneTwoUnitTest, TheDifficultyLimitIsOneBeyondFifteenMetresAndTwoNearer) {
  // 0x142124950's band 1: beyond 15 m (or past 110 degrees of facing) the difficulty is held to 1.0;
  // nearer it is held to 2.0.
  in_.mates[0].dist_m = 20.0f;
  in_.mates[0].difficulty = 1.5f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.mates[0].difficulty = 0.9f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
  in_.mates[0].dist_m = 14.0f;
  in_.mates[0].difficulty = 1.5f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(OneTwoUnitTest, TheOneTouchLaneMustBeBlocked) {
  // 0x142128C10's G11: the lane 15 m along the plan heading must hold an opponent. With him 30 m off
  // the lane the one-touch fails, and so does the answer.
  in_.touch.players[kOneTwoOpp].pos = {38.0f, 30.0f};
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
}

TEST_F(OneTwoUnitTest, G11sLaneRunsAlongThePlanHeadingNotTheAttack) {
  // G11's theta is the PLAN heading (ctx7+0xC0), which the predicate passes to the lane core; the
  // attack direction is a different field (T+0x250). The fixture's opponent sits 8 m along +x, so a
  // plan heading of 90 holds him and a heading of 0 (the old 0 stand-in's direction) does not.
  in_.touch.planHeading_deg = 90.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
  in_.touch.planHeading_deg = 0.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  // The attack direction does not move the lane: it stays the plan heading.
  in_.touch.attackAngle_deg = 0.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.touch.planHeading_deg = 90.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(OneTwoUnitTest, TheTeamLevelGateNeedsLevelTwo) {
  // 0x142128C10's G3: the team level, level/100, must reach 2.
  in_.touch.teamLevelRaw = 100;  // level 1
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
}

TEST_F(OneTwoUnitTest, TheTeamLevelIsTheRawLevelOverOneHundred) {
  // 0x140A40BD0 reads +0xA294 (zero when +0xA29C is set); 0x140A60360 compares that level / 100 with 2 for G3.
  in_.touch.teamLevelRaw = 199;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.touch.teamLevelRaw = 200;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
  in_.touch.teamLevelZero = true;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
}

TEST_F(OneTwoUnitTest, G4TakesTheTeamModeThreeOrFourOrACategoryFourCarrier) {
  // G4 (0x142128C10): `0x141052E30(param_1[6]) == 3 || == 4`, or the carrier's category >= 4. The
  // mode is the carrier's Intention at bp+0x32B0+8 (writer 0x1421197B0); the controller used to pass
  // the neutral 2 stand-in, which closed OneTwo for every carrier.
  in_.touch.teamMode = 2;  // the old stand-in: G4 shut (the fixture's category is not >= 4 either)
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.touch.teamMode = 3;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
  // A category-4 carrier opens G4 whatever the mode is.
  in_.touch.teamMode = 2;
  in_.touch.carrierCategory = 4;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(OneTwoUnitTest, G8sIncrementDoesNotCoverModeFour) {
  // G8's increment (0x142128C10): +1 for the attack level >= 3, the mode 2 or the mode 3. At level 2
  // (base 0) the cap is 1 for mode 3 and 0 for mode 4, so mode 4 opens G4 but G8 closes it. The
  // controller passing the real Intention must not turn mode 4 into an answer by itself.
  in_.touch.teamMode = 4;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
}


TEST_F(OneTwoUnitTest, TheScoreFloorIsFortyMetresSquared) {
  // 0x142128C10's G5: the score row must read 1600 (40 m) or more.
  in_.touch.score = 1599.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.touch.score = 1600.0f;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(OneTwoUnitTest, TheReceiverNeedsHisStatus) {
  // 0x142124950's P1: his b6c status must read 0xB.
  in_.mates[0].statusValid = false;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
}

TEST_F(OneTwoUnitTest, TheReceiverGateIsHisRoleClassNotTheKeeper) {
  // 0x142124A83 exits on snapshot[0xD] == 0, and snapshot[0xD] = T(side)+0x4BD4[r%11] is his role class:
  // class 0 is the keeper and is refused; a field class (1) takes the ball.
  in_.mates[0].roleClass = 0;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.mates[0].roleClass = 1;
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(OneTwoUnitTest, TheFirstListedReceiverThatPassesTakesIt) {
  // 0x142126A80's loop returns the first list entry the receiver predicate takes.
  in_.mates = {OneTwoMateAt(7, 40.0f, -10.0f, 315.0f), OneTwoMateAt(kOneTwoMate, 40.0f, 10.0f, 225.0f)};
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, 7);
  in_.mates = {OneTwoMateAt(kOneTwoMate, 40.0f, 10.0f, 225.0f), OneTwoMateAt(7, 40.0f, -10.0f, 315.0f)};
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

// --- the DirectHighBall unit (kind 11; gate 0x142129BE0) ---
//
// The carrier (slot 3) stands at (30, 0) attacking +x. The ball is low (1.2 m, under the carrier's
// 1.8 m head plus 0.1 m) and opponent 14 stands 2 m from it, so the low-ball block passes. Mate 5
// stands 12 m ahead, unmoving, scoring over 900 (no branch A). The volatile flag (K+0x6E) is on, the
// unit's own gate. The one-touch is shut unless a test opens it (team level 0).

pes::DirectHighBallMate DirectHighBallMateAt(int index, float x, float z, float score) {
  pes::DirectHighBallMate m;
  m.receiver = OneTwoMateAt(index, x, z, 270.0f);
  m.receiver.dist_m = 10.0f;
  m.heading_deg = 90.0f;       // along +x: X = 0 against the attack
  m.speed_kmh = 0.0f;
  m.tableAngle_deg = 90.0f;    // the lane heading is 90 + 180
  m.recAClass = 2;
  m.score = score;
  return m;
}

class DirectHighBallUnitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root)
      GTEST_SKIP() << "PES21_ROOT unset: the teammate lane runs the lane core, whose table is in the install";
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    auto loaded = pes::LoadCarrierConstants(*exe, &err);
    ASSERT_TRUE(loaded.has_value()) << err;
    k_ = *loaded;
    pes::TouchInput& t = in_.touch;
    t.players[kOneTwoCarrier].pos = {30.0f, 0.0f};
    t.players[kOneTwoCarrier].active = true;
    t.players[kOneTwoOpp].pos = {32.0f, 0.0f};
    t.players[kOneTwoOpp].active = true;
    t.players[kOneTwoMate].pos = {42.0f, 0.0f};
    t.players[kOneTwoMate].active = true;
    t.carrierSlot = kOneTwoCarrier;
    t.carrierPos = {30.0f, 0.0f};
    t.attackSign = 1.0f;
    t.attackAngle_deg = 90.0f;
    t.carrierFacing_deg = 90.0f;
    t.planHeading_deg = 90.0f;
    t.planMode = 2;
    t.ownLine_m = -kOneTwoHalfLen;
    t.pitchBound_m = kOneTwoHalfLen;
    t.teamLevelRaw = 0;
    t.teamMode = 3;
    t.score = 2500.0f;
    t.curve = FlatCurve(35.0f);
    in_.ballHeight_m = 1.2f;
    in_.carrierHeight_m = 1.8f;
    in_.ball = {30.0f, 0.0f};
    in_.volatileBall = true;
    in_.mates = {DirectHighBallMateAt(kOneTwoMate, 42.0f, 0.0f, 1000.0f)};
  }

  pes::CarrierConstants k_{};
  pes::DirectHighBallInput in_{};
};

TEST_F(DirectHighBallUnitTest, ALowBallWithAnOpponentAtHisFeetGivesTheThroughBall) {
  // 0x142129BE0: the low-ball block passes, the mate is taken, his lane is free: b = 3 (through).
  const pes::PassAnswer a = pes::DirectHighBallUnit(in_, k_);
  EXPECT_EQ(a.index, kOneTwoMate);
  EXPECT_EQ(a.kind, pes::kPassThrough);
  EXPECT_FALSE(a.oneTouch);
}

TEST_F(DirectHighBallUnitTest, ALowBallWithNoOpponentWithinFiveMetresGivesNothing) {
  // The low-ball block: the nearest opponent of the ball must be within 5 m (d2 25), or no answer.
  in_.touch.players[kOneTwoOpp].pos = {38.0f, 0.0f};  // 8 m from the ball
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, -1);
}

TEST_F(DirectHighBallUnitTest, AHighBallSkipsTheLowBallBlock) {
  // Over the head height plus 0.1 m the block does not run, so the far opponent does not matter.
  in_.touch.players[kOneTwoOpp].pos = {38.0f, 0.0f};
  in_.ballHeight_m = 1.95f;
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(DirectHighBallUnitTest, TheLowBallLineIsTheHeadPlusTenCentimetres) {
  // 0x142129BE0's threshold: a ball at 1.85 m (under 1.9) runs the block, which refuses a far opponent.
  in_.touch.players[kOneTwoOpp].pos = {38.0f, 0.0f};
  in_.ballHeight_m = 1.85f;
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, -1);
}

TEST_F(DirectHighBallUnitTest, TheVolatileFlagAndTheFirstTimeBitSilenceTheUnit) {
  // K+0x6E (the unit's first test after the first-time bit) and rec+1's bit 5.
  in_.volatileBall = false;
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, -1);
  in_.volatileBall = true;
  in_.firstTimeWhileReceiving = true;
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, -1);
}

TEST_F(DirectHighBallUnitTest, TheScoreFloorIs225ExactlyKept) {
  // 0x142129BE0: a score under 225 is no candidate; exactly 225 is kept (the branch-A gap is 100, so
  // the mate at 42 m with his opponent 10 m away is kept too).
  in_.mates = {DirectHighBallMateAt(kOneTwoMate, 42.0f, 0.0f, 224.0f)};
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, -1);
  in_.mates = {DirectHighBallMateAt(kOneTwoMate, 42.0f, 0.0f, 225.0f)};
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, kOneTwoMate);
}

TEST_F(DirectHighBallUnitTest, ABranchAMateWithAnOpponentWithinTenMetresIsSkipped) {
  // Score 500 puts the mate in branch A: his nearest opponent 8 m away skips him.
  in_.touch.players[kOneTwoOpp].pos = {34.0f, 0.0f};
  in_.mates = {DirectHighBallMateAt(kOneTwoMate, 42.0f, 0.0f, 500.0f)};
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, -1);
}

TEST_F(DirectHighBallUnitTest, ANineFrameCutOffSkipsTheMate) {
  // The mate's 9-frame point must stay 3 m or more from the ball; running back onto it skips him.
  pes::DirectHighBallMate m = DirectHighBallMateAt(kOneTwoMate, 42.0f, 0.0f, 1000.0f);
  m.velPerFrame = {-1.2f, 0.0f};  // 9 frames put him at x 31.2
  in_.mates = {m};
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, -1);
}

TEST_F(DirectHighBallUnitTest, TheSmallestTwentySevenFrameDistanceWins) {
  // Two mates both pass; the one nearer the ball 27 frames on takes it, whatever the list order.
  in_.mates = {DirectHighBallMateAt(kOneTwoMate, 42.0f, 0.0f, 1000.0f),
               DirectHighBallMateAt(7, 36.0f, 4.0f, 1000.0f)};
  EXPECT_EQ(pes::DirectHighBallUnit(in_, k_).index, 7);
}

TEST_F(DirectHighBallUnitTest, ABackwardsMateGetsAShortBall) {
  // X = wrap180(heading - attack) = -90: past 80 degrees the ball stays short and f10 is not written.
  pes::DirectHighBallMate m = DirectHighBallMateAt(kOneTwoMate, 42.0f, 0.0f, 1000.0f);
  m.heading_deg = 0.0f;
  in_.mates = {m};
  const pes::PassAnswer a = pes::DirectHighBallUnit(in_, k_);
  EXPECT_EQ(a.index, kOneTwoMate);
  EXPECT_EQ(a.kind, pes::kPassShort);
  EXPECT_FALSE(a.oneTouch);
}

TEST_F(DirectHighBallUnitTest, ATeammateInTheLaneMakesTheBallShort) {
  // The teammate lane (the branch the decode shows is live): a team-mate standing in the 15 m lane
  // from the mate's advanced origin means the ball is not free, so it goes short.
  in_.touch.players[7].pos = {30.0f, 1.0f};
  in_.touch.players[7].active = true;
  const pes::PassAnswer a = pes::DirectHighBallUnit(in_, k_);
  EXPECT_EQ(a.index, kOneTwoMate);
  EXPECT_EQ(a.kind, pes::kPassShort);
}

TEST_F(DirectHighBallUnitTest, TheFirstTouchFlagNeedsTheOneTouchAndTheReceiver) {
  // f10 is written when the one-touch predicate and the mate's receiver predicate both hold. The
  // high ball keeps the low-ball block out of it; the mate stands off the attack line.
  in_.ballHeight_m = 2.5f;
  in_.touch.teamLevelRaw = 200;
  in_.mates = {DirectHighBallMateAt(kOneTwoMate, 42.0f, 6.0f, 1000.0f)};
  const pes::PassAnswer a = pes::DirectHighBallUnit(in_, k_);
  EXPECT_EQ(a.index, kOneTwoMate);
  EXPECT_EQ(a.kind, pes::kPassThrough);
  EXPECT_TRUE(a.oneTouch);
}

// --- the attack heading (T+0x250) is PES's bearing of the attack direction ---

TEST_F(OneTwoUnitTest, TheAttackBearingIsTheXAxisSoABehindOpponentDoesNotClose) {
  // T+0x250 has one writer, 0x1404FEBD0, which stores 0x14045DF70's MA+0x228D8 (90) for attack sign +1
  // and MA+0x228D4 (270) for -1 through 0x140A71520: the bearing is on the x axis, not z. The angle
  // gate is |wrap180(carrierAngle - attackAngle)| <= 90, and the fixture's opponent (38, 0) sits
  // outside the 6 m closing annulus, so only the convention decides the answer.
  in_.carrierAngle_deg = 270.0f;                          // the carrier faces away from the attack
  in_.touch.attackAngle_deg = pes::AttackAngleDeg(1.0f);  // 90: the difference is 180, gate shut
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, -1);
  in_.touch.attackAngle_deg = 0.0f;  // the z-axis value the controller used before: 90, gate open
  EXPECT_EQ(pes::OneTwoUnit(in_, k_).index, kOneTwoMate);
}

TEST(PesCarrierBearing, TheAttackAngleIsTheBearingOfTheAttackDirection) {
  // The relation the axis flips: the attack bearing must equal the bearing of the +x/-x direction, not
  // the +z/-z one the old 0/180 pair named.
  const float home = pes::AttackAngleDeg(1.0f);
  const float away = pes::AttackAngleDeg(-1.0f);
  const auto wrap180 = [](float a) {
    a = std::fmod(a + 180.0f, 360.0f);
    if (a < 0.0f) a += 360.0f;
    return a - 180.0f;
  };
  EXPECT_NEAR(std::fabs(wrap180(home - 0.0f)), 90.0f, 1e-3f);   // 90 from the +z axis: the attack is on x
  EXPECT_NEAR(std::fabs(wrap180(away - 180.0f)), 90.0f, 1e-3f);
  EXPECT_NEAR(std::fabs(wrap180(home - away)), 180.0f, 1e-3f);
}

TEST(PesCarrierBearing, ACarrierFacingTheGoalHeAttacksReadsZeroDegreesOffTheAim) {
  // The shoot gate's aim-vs-facing (report 5.1: angle(ball->aim, facing) > 100 at 0x142121920, and
  // central_eval's 60/90 term). The aim of a side attacking +x is the attack bearing (90) and the
  // facing is a PES bearing, so a carrier facing +x reads 0, not 180. The old controller subtracted a
  // mirrored angle (atan2(-y, -x) = 270 - bearing) from the bearing, which inverted this.
  EXPECT_NEAR(pes::AimVsFacingDeg(1.0f, 1.0f, 0.0f), 0.0f, 1e-3f);     // facing +x, attacking +x
  EXPECT_NEAR(pes::AimVsFacingDeg(1.0f, -1.0f, 0.0f), 180.0f, 1e-3f);  // facing his own goal
  EXPECT_NEAR(pes::AimVsFacingDeg(1.0f, 0.0f, 1.0f), 90.0f, 1e-3f);    // facing +z
  EXPECT_NEAR(pes::AimVsFacingDeg(-1.0f, -1.0f, 0.0f), 0.0f, 1e-3f);    // the other end mirrors
  EXPECT_NEAR(pes::AimVsFacingDeg(-1.0f, 1.0f, 0.0f), 180.0f, 1e-3f);
}

// --- the stock team level (G3's input): class x 100, from the install ---

TEST(PesCarrierInstall, TheStockTeamLevelIsAClassScaledByAHundred) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadCarrierConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // 0x1B5D8340 stores clamp(class x 100, 0, 700) with class = the masked global (0x14810F4E0 ^ the
  // immediate at 0x1401B5D8378); 0x140A602B0 is the scale, 0x140A602B5 the cap. GF has no
  // match-setting selector, so the value is the stock default class.
  EXPECT_GT(k->teamLevelRaw, 0);                      // read, not left at the 0 stand-in
  EXPECT_EQ(k->teamLevelRaw % 100, 0);                // a class scaled by the game's own 100
  EXPECT_LE(k->teamLevelRaw / 100, 7);                // inside 0x144286B70's 7-class cap
  EXPECT_GE(k->teamLevelRaw / 100, 2);                // the stock class opens G3 (0x140A60360's >= 2)
}

// --- G4's team mode: the carrier's Intention, written by 0x1421197B0 at bp+0x32B0+8 ---
//
// Read in order (raw asm 0x142119862..0x142119CD2): 4 is absorbing (0x142119868), then the
// shoot-zone angle test (0x1421198FC), then 3 (0x14211993A), then 2 (0x142119956), then the
// corridor block whose only stores are 2 (0x142119AD9), 1 (0x142119CB8) and 0 (the `xor edi,edi` at
// 0x1421199B8 reaching 0x142119CD2 past the `ja`). The reader 0x141052E30 = *(u32*)(p+8).

namespace {
// Every gate of the writer off: the penalty test under its line, the B3C/B58 lead test under its
// limit (so `B58 + 10 > B3C` is false), the shoot-zone test off and the corridor block shut.
pes::IntentionInput QuietIntention() {
  pes::IntentionInput in;
  in.penaltyLine_m = 36.14f;
  in.ballLimit_m = 1000.0f;
  return in;
}
}  // namespace

TEST(PesCarrierIntention, FourIsAbsorbing) {
  const pes::CarrierConstants k = Synthetic();
  EXPECT_EQ(pes::NextIntention(4, QuietIntention(), k), 4);
}

TEST(PesCarrierIntention, TheShootZoneAngleTestGivesFour) {
  // 0x1421198FC: IsClose(pred+0x9CC) and |wrap180(bp+0x2C - bp+0x4C)| < 100 (0x14253EE30).
  const pes::CarrierConstants k = Synthetic();
  pes::IntentionInput in = QuietIntention();
  in.closeBand = true;
  in.headingDiff_deg = 99.0f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 4);
  in.headingDiff_deg = 100.0f;  // strict: `jbe` skips at exactly 100
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  in.headingDiff_deg = 0.0f;
  in.closeBand = false;  // the zone test alone is not enough
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
}

TEST(PesCarrierIntention, ThreeIsStickyAndThePenaltyAndLeadTestsGiveThree) {
  // 0x142119909..0x142119938: prev == 3, or F[6] < attackSign*K.x (strict), or B3C <= B58 + 10
  // (strict, 0x142588210).
  const pes::CarrierConstants k = Synthetic();
  EXPECT_EQ(pes::NextIntention(3, QuietIntention(), k), 3);  // sticky
  pes::IntentionInput in = QuietIntention();
  in.attackDot = 36.15f;  // just past the penalty line
  EXPECT_EQ(pes::NextIntention(1, in, k), 3);
  in.attackDot = 36.14f;  // strict: exactly on the line is not past it
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  in.attackDot = 0.0f;
  in.ballLead_m = 24.0f;  // B58 + 10 > B3C
  in.ballLimit_m = 33.0f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 3);
  in.ballLimit_m = 34.0f;  // B58 + 10 == B3C: the `jbe` skips
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
}

TEST(PesCarrierIntention, TheCorridorBlockGivesOneTwoOrZero) {
  // 0x142119960: behind the guard (team+4 < 2 and prev != 2) and after both queries answer
  // (0x1421B7D40 count > 1 at 0x142119C91), the value is 1 when the block's own gate holds
  // (0x142119C9A) and 0 when it does not. Every other outcome latches at 2 (0x142119AD9).
  const pes::CarrierConstants k = Synthetic();
  pes::IntentionInput in = QuietIntention();
  in.corridorGuard = true;
  in.corridorAnswered = true;
  in.corridorState1 = true;
  EXPECT_EQ(pes::NextIntention(1, in, k), 1);
  in.corridorState1 = false;
  EXPECT_EQ(pes::NextIntention(1, in, k), 0);  // the `xor edi,edi` path, reachable
  in.corridorAnswered = false;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  in.corridorAnswered = true;
  in.corridorGuard = false;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  in.corridorGuard = true;
  EXPECT_EQ(pes::NextIntention(2, in, k), 2);  // prev == 2 skips the corridor entirely
}

}  // namespace

// K+0x6E (FUN_14063A490, 0x14063AA5E..0x14063AA91): set when the ctx4 height K+0x44 is above the
// carrier's height byte P+0xA1 / 100, and only while K+0x6C is 0 (0x14063AA62). Heights in metres.
TEST(CarrierVolatileBall, IsSetWhenTheBallIsAboveTheCarriersHeight) {
  pes::VolatileBallInput in;
  in.ballHeight_m = 1.95f;
  in.carrierHeight_m = 1.80f;
  EXPECT_TRUE(pes::VolatileBall(in));
  in.ballHeight_m = 1.50f;
  EXPECT_FALSE(pes::VolatileBall(in));
  in.ballHeight_m = 1.80f;  // strict: K+0x44 > byte / 100 (comiss at 0x14063AA8C)
  EXPECT_FALSE(pes::VolatileBall(in));
  in.ballHeight_m = 1.95f;
  in.humanCarrier = true;  // K+0x6C != 0 skips the write (0x14063AA62)
  EXPECT_FALSE(pes::VolatileBall(in));
}

// K+0x6E's ball-track arm (0x14063AB01..0x14063AB06): B+0x4C != 0, B+0x80 == 0 and r14 == 0 (d1 >= d2, 2-D).
TEST(CarrierVolatileBall, TheTrackArmSetsItWhenTheBallIsNotNearerItsAnchor) {
  pes::VolatileBallInput in;
  in.ballTrackFlag = true;
  in.ball = {0.0f, 0.0f};
  in.anchor = {1.0f, 0.0f};   // d1 = 1: the ball is nearer its anchor, so r14 = 1 and the arm is off
  in.target = {10.0f, 0.0f};  // d2 = 100
  EXPECT_FALSE(pes::VolatileBall(in));
  in.anchor = {10.0f, 0.0f};  // d1 = 100 >= d2 = 1 (the target is now at 1): r14 = 0, so the arm sets K+0x6E
  in.target = {1.0f, 0.0f};
  EXPECT_TRUE(pes::VolatileBall(in));
  in.ballFlag80 = true;       // B+0x80 != 0 makes r14b nonzero, so the arm is off
  EXPECT_FALSE(pes::VolatileBall(in));
  in.ballFlag80 = false;
  in.ballTrackFlag = false;   // B+0x4C == 0: no arm
  EXPECT_FALSE(pes::VolatileBall(in));
}

// PES 2021's receiver status (0x142117c30): which team-mates a pass unit is offered.
pes::ReceiverStatusInput AMateAtRest() {  // 20 m behind the carrier, inside the pitch, onside
  pes::ReceiverStatusInput in;
  in.pos = {-20.0f, 0.0f};
  in.offsideLine_m = 60.0f;
  in.attackSign = 1.0f;
  in.halfLen_m = pesFieldHalfLength;
  in.halfWidth_m = pesFieldHalfWidth;
  return in;
}

TEST(ReceiverStatus, ABackwardMateAtRestIsOffered) {
  const pes::CarrierConstants k{};
  EXPECT_EQ(pes::ReceiverStatusOf(AMateAtRest(), k), pes::kReceiverValid);
}

TEST(ReceiverStatus, AMateAheadOfTheOffsideLineIsOffside) {
  const pes::CarrierConstants k{};
  pes::ReceiverStatusInput in = AMateAtRest();
  in.pos = {30.0f, 0.0f};
  in.offsideLine_m = 20.0f;  // the line is 10 m behind him: offside
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverOffside);
  in.offsideLine_m = 40.0f;  // the line is past him: onside
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverValid);
}

TEST(ReceiverStatus, AMateWhoseEighteenFrameTargetLeavesThePitchIsPredictedOff) {
  const pes::CarrierConstants k{};
  pes::ReceiverStatusInput in = AMateAtRest();
  in.pos = {50.0f, 0.0f};
  in.velPerFrame = {0.2f, 0.0f};  // 50 + 18 * 0.2 = 53.6 m, past the 52.56 m goal line
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverPredictedOff);
  in.velPerFrame = {0.0f, 0.0f};  // at rest he stays on the pitch
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverValid);
}

TEST(ReceiverStatus, ATripAnAnimeClassWithBusyBitAndKeeperPickupAreNotOffered) {
  pes::CarrierConstants k{};
  pes::ReceiverStatusInput in = AMateAtRest();
  in.anime = 16;  // STAGGER
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverBusy8);
  in.anime = 2;  // MOVE, given a synthetic word with bit 9 here (not the install's word)
  k.animeClassWord[2] = 1u << 9;
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverBusy7);
  in.anime = 87;  // KP_PICKUP_BALL
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverBusy9);
}

TEST(ReceiverStatus, AMateOffThePitchTheCarrierAndTheSetPieceTakerAreNotOffered) {
  const pes::CarrierConstants k{};
  pes::ReceiverStatusInput in = AMateAtRest();
  in.onPitch = false;
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverNotOnPitch);
  in.onPitch = true;
  in.carrier = true;
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverCarrier);
  in.carrier = false;
  in.setPieceTaker = true;
  EXPECT_EQ(pes::ReceiverStatusOf(in, k), pes::kReceiverSetPiece);
}

TEST(ReceiverStatus, TheOffsideLineIsTheDeepestOfTheFirstFiveNonKeepersAndTheBall) {
  // Attacking +x: the first five listed men, a keeper skipped, and the ball, all 16 frames ahead.
  const std::vector<pes::OffsideEntry> listed = {
      {20.0f, 0.0f, false}, {45.0f, 0.0f, true}, {30.0f, 0.0f, false},
      {25.0f, 0.0f, false}, {10.0f, 0.0f, false}, {50.0f, 0.0f, false}};  // the sixth is not read
  EXPECT_FLOAT_EQ(pes::CarrierOffsideLine(1.0f, listed, -40.0f, 0.0f, 16), 30.0f);
  // The ball at 1.5 m a frame is 10 + 16 * 1.5 = 34 m, past the line.
  EXPECT_FLOAT_EQ(pes::CarrierOffsideLine(1.0f, listed, 10.0f, 1.5f, 16), 34.0f);
}

TEST(ReceiverStatus, ALineManIsReadSixteenFramesAhead) {
  // At 1 m a frame, a man on 10 m is on 26 m after 16 frames.
  EXPECT_FLOAT_EQ(pes::CarrierOffsideLine(1.0f, {{10.0f, 1.0f, false}}, -40.0f, 0.0f, 16), 26.0f);
}

TEST(ReceiverStatus, TheLineStartsAtHalfwayAndFollowsTheAttackDirection) {
  // Everyone behind halfway leaves the line on halfway: the running maximum starts at 0.
  EXPECT_FLOAT_EQ(
      pes::CarrierOffsideLine(1.0f, {{-40.0f, 0.0f, false}, {-35.0f, 0.0f, false}}, -20.0f, 0.0f, 16),
      0.0f);
  // Attacking -x (s = -1), the deepest is the most negative x: -30.
  EXPECT_FLOAT_EQ(pes::CarrierOffsideLine(-1.0f,
                                          {{-20.0f, 0.0f, false}, {-30.0f, 0.0f, false},
                                           {-25.0f, 0.0f, false}},
                                          0.0f, 0.0f, 16),
                  -30.0f);
}

TEST(ReceiverStatusInstall, TheOffsideFramesAndTheAnimeWordsAreTheInstallsValues) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadCarrierConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // int(54 x 0.30) = 16 frames.
  EXPECT_EQ(pes::ReceiverOffsideFrames(*k), 16);
  pes::ReceiverStatusInput in = AMateAtRest();
  in.anime = 56;  // bit 9 of its word (FUN_140a6e450)
  EXPECT_EQ(pes::ReceiverStatusOf(in, *k), pes::kReceiverBusy7);
  in.anime = 73;  // bit 27 of its word (FUN_140a6e110)
  EXPECT_EQ(pes::ReceiverStatusOf(in, *k), pes::kReceiverBusy10);
}

// PES's sample 0 (0x140874100): the first bounce after the ball has been a metre up.
pes::TrackPoint TrackAt(float x, float height, float z) { return pes::TrackPoint{x, height, z}; }

TEST(CarrierBallTrack, TheFirstBounceAfterAMetreUpIsTheSample) {
  // Heights 0.5, 1.2 (latched), 3.0, 1.5, 0.2, 0.7: the ball falls to 0.2 and turns up at tick 5.
  const std::vector<pes::TrackPoint> track = {
      TrackAt(10.0f, 0.5f, -4.0f),  TrackAt(10.5f, 1.2f, -4.0f), TrackAt(11.0f, 3.0f, -4.0f),
      TrackAt(11.5f, 1.5f, -4.0f),  TrackAt(12.0f, 0.2f, -4.0f), TrackAt(12.5f, 0.7f, -4.0f),
      TrackAt(13.0f, 1.5f, -4.0f)};
  const pes::BounceSample s = pes::FirstBounceSample(track, 1.0f);
  EXPECT_TRUE(s.valid);
  EXPECT_FLOAT_EQ(s.xz.x, 12.5f);
  EXPECT_FLOAT_EQ(s.xz.z, -4.0f);
}

TEST(CarrierBallTrack, ABounceBeforeTheBallHasBeenAMetreUpIsNotSampled) {
  // The ball bobs at 0.1 to 0.3 m (a bounce at tick 2), then rises to 2.0 m: only the bounce after that counts.
  const std::vector<pes::TrackPoint> track = {
      TrackAt(0.0f, 0.3f, 0.0f), TrackAt(1.0f, 0.1f, 0.0f), TrackAt(2.0f, 0.3f, 0.0f),
      TrackAt(3.0f, 2.0f, 0.0f), TrackAt(4.0f, 1.0f, 0.0f), TrackAt(5.0f, 0.4f, 0.0f),
      TrackAt(6.0f, 0.9f, 0.0f)};
  const pes::BounceSample s = pes::FirstBounceSample(track, 1.0f);
  EXPECT_TRUE(s.valid);
  EXPECT_FLOAT_EQ(s.xz.x, 6.0f);
}

TEST(CarrierBallTrack, ARollingOrFallingBallHasNoSample) {
  // A ball that never rises a metre is never latched, and a fall that never turns up has no bounce.
  const std::vector<pes::TrackPoint> rolling = {TrackAt(0.0f, 0.109f, 0.0f), TrackAt(1.0f, 0.12f, 0.0f),
                                                TrackAt(2.0f, 0.109f, 0.0f), TrackAt(3.0f, 0.12f, 0.0f)};
  EXPECT_FALSE(pes::FirstBounceSample(rolling, 1.0f).valid);
  const std::vector<pes::TrackPoint> falling = {TrackAt(0.0f, 3.0f, 0.0f), TrackAt(1.0f, 2.0f, 0.0f),
                                                TrackAt(2.0f, 1.0f, 0.0f), TrackAt(3.0f, 0.5f, 0.0f)};
  EXPECT_FALSE(pes::FirstBounceSample(falling, 1.0f).valid);
}

TEST(CarrierVolatileBall, TheTrackArmSetsItOnAValidSampleWithNoSideFlag) {
  // The ball at (10, -4) with its sample at (12.5, -4): the distance test is off (K+0x40 stands in as the ball).
  pes::VolatileBallInput in;
  in.ballTrackFlag = true;
  in.ball = {10.0f, -4.0f};
  in.anchor = {12.5f, -4.0f};
  in.target = {10.0f, -4.0f};
  EXPECT_TRUE(pes::VolatileBall(in));
  in.ballFlag80 = true;  // B+0x80 set: arm B is off
  EXPECT_FALSE(pes::VolatileBall(in));
}

// K+0x40 (FUN_14063A490, 0x14063A854): the set-piece display point while the set piece runs, else the AI point.
TEST(CarrierCtxPoint, ASetPieceTakesTheDisplayPointAndOpenPlayTheCarriersAiPoint) {
  const pes::CtxPoint display{10.0f, 1.2f, -4.0f};
  const pes::CtxPoint aiPoint{-20.0f, 0.0f, 3.0f};
  const pes::CtxPoint inSetPiece = pes::CarrierCtxPoint(true, display, aiPoint);
  EXPECT_FLOAT_EQ(inSetPiece.x, 10.0f);
  EXPECT_FLOAT_EQ(inSetPiece.y, 1.2f);
  EXPECT_FLOAT_EQ(inSetPiece.z, -4.0f);
  const pes::CtxPoint openPlay = pes::CarrierCtxPoint(false, display, aiPoint);
  EXPECT_FLOAT_EQ(openPlay.x, -20.0f);
  EXPECT_FLOAT_EQ(openPlay.y, 0.0f);
  EXPECT_FLOAT_EQ(openPlay.z, 3.0f);
}

// PES 2021's scored limit (0x142126B70 L183-232, asm): a keeper's limit is the 45 floor (0x1421271BC), and a field
// mate's is clamp(need, 45, 135): a need over the 135 cap gives 135, not the floor.
TEST(CarrierShortLimit, AKeeperIsAt45AndAFieldMateIsCappedAt135) {
  pes::CarrierConstants k{};
  k.sfSpeedFloor_kmh = 5.0f;   // 0x14259222C
  k.sfSpeedCeil_kmh = 25.0f;   // 0x14259BE20
  k.sfSpeedSpan_kmh = -20.0f;  // 0x1425A946C
  k.sfScoreDist_m = 10.0f;     // 0x142588210
  k.sfDistCeil_m = 30.0f;      // 0x142591140
  k.sfNeedGain = 30.0f;        // 0x142591140
  k.sfNeedBase = 60.0f;        // 0x14259A764
  k.sfClass4Bonus = 20.0f;     // 0x14259BE1C
  k.sfClass5Bonus = 30.0f;     // 0x142591140
  k.sfNeedDanger = 120.0f;     // 0x14256AE30
  k.sfNeedCap = 135.0f;        // 0x14259BE60
  k.sfNeedFloor = 45.0f;       // 0x14259BE30
  pes::SafetyInput in;
  in.carrierSpeed_kmh = 0.0f;  // wS = 1: a carrier at rest
  pes::SafetyMate m;
  m.recADist_m = 5.0f;         // wD = 1: a distance under 10 m
  m.recAClass = 4;             // +20: need = 30 + 60 + 30 + 20 = 140, over the cap
  EXPECT_FLOAT_EQ(pes::ShortLimit(in, m, /*keeper*/ false, k), 135.0f);
  m.recAClass = 3;             // need = 120: inside the cap
  EXPECT_FLOAT_EQ(pes::ShortLimit(in, m, /*keeper*/ false, k), 120.0f);
  EXPECT_FLOAT_EQ(pes::ShortLimit(in, m, /*keeper*/ true, k), 45.0f);
}

// PES's speed ramp reads the CARRIER's speed (0x142127109: CALL 0x140477130 on the passer's record); the mate's speed
// only gates the mates under 5 m (0x142127337).
TEST(CarrierShortLimit, TheSpeedRampReadsTheCarriersSpeed) {
  pes::CarrierConstants k{};
  k.sfSpeedFloor_kmh = 5.0f;
  k.sfSpeedCeil_kmh = 25.0f;
  k.sfSpeedSpan_kmh = -20.0f;
  k.sfScoreDist_m = 10.0f;
  k.sfDistCeil_m = 30.0f;
  k.sfNeedGain = 30.0f;
  k.sfNeedBase = 60.0f;
  k.sfNeedCap = 135.0f;
  k.sfNeedFloor = 45.0f;
  pes::SafetyInput in;
  in.carrierSpeed_kmh = 30.0f;  // a carrier past 25 km/h: wS = 0
  pes::SafetyMate m;
  m.speed_kmh = 10.0f;          // the mate's speed must not enter the ramp
  m.recADist_m = 20.0f;         // wD = (20 - 30) / -20 = 0.5
  m.recAClass = 3;
  // need = 0 + 60 + 0.5 * 30 = 75
  EXPECT_FLOAT_EQ(pes::ShortLimit(in, m, /*keeper*/ false, k), 75.0f);
}

// Record B's target ("into space", carrier-ai 4.3): the mate's point, then 8 m + 5 m * clamp01(speed * cos(delta) / 20)
// along the attack angle turned by sigma * 45 deg * clamp01(d / 30), sigma the side of the carrier->mate bearing.
TEST(CarrierRecordB, TheIntoSpaceTargetTurnsFromTheAttackAngleByTheNearestOpponentGap) {
  pes::CarrierConstants k{};
  k.intoSpaceTurn_deg = 45.0f;    // 0x14259BE30
  k.intoSpaceTurnDist_m = 30.0f;  // 0x142591140
  k.intoSpaceBase_m = 8.0f;       // 0x14259BDF4
  k.intoSpaceGain_m = 5.0f;       // 0x14259222C
  k.intoSpaceSpeedScale = 20.0f;  // 0x14259BE1C
  // Attacking +x (90 deg). The mate 10 m ahead, still, 30 m from his nearest opponent: sigma +1, the turn +45, the
  // direction 135 deg, the length 8 m.
  const pes::CarrierPoint a = pes::IntoSpaceTarget({0.0f, 0.0f}, {10.0f, 0.0f}, 90.0f, 0.0f, 30.0f, 90.0f, k);
  EXPECT_NEAR(a.x, 15.656854f, 1e-4f);
  EXPECT_NEAR(a.z, -5.656854f, 1e-4f);
  // No opponent near him: no turn, so the direction is the attack angle.
  const pes::CarrierPoint b = pes::IntoSpaceTarget({0.0f, 0.0f}, {10.0f, 0.0f}, 90.0f, 0.0f, 0.0f, 90.0f, k);
  EXPECT_NEAR(b.x, 18.0f, 1e-4f);
  EXPECT_NEAR(b.z, 0.0f, 1e-4f);
  // The mate at 45 deg from the carrier is on the minus side of the attack: the turn is -45, so the direction is 45 deg.
  // Running 20 km/h along it (cos 1) lengthens the target to 13 m.
  const pes::CarrierPoint c = pes::IntoSpaceTarget({0.0f, 0.0f}, {10.0f, 10.0f}, 45.0f, 20.0f, 30.0f, 90.0f, k);
  EXPECT_NEAR(c.x, 19.192388f, 1e-4f);
  EXPECT_NEAR(c.z, 19.192388f, 1e-4f);
}

// 0x1421226C0 passes 0 as the lane call's frame count (0x142122D00 stores ESI = 0), so lane 1 scans the present
// positions. The opponent 9 m up the lane runs at the origin, 1.2 m a frame: 3 m in by the mate's slot frame (5).
TEST_F(BackSpaceUnitTest, TheLaneOneScansThePresentPositionsNotTheMatesSlotFrame) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset: the lane constants come from the install";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto lk = pes::lane::LoadLaneConstants(*exe, &err);
  ASSERT_TRUE(lk) << err;
  k_.lane = *lk;
  k_.bsLane1Len_m = 5.0f;   // the immediates at 0x142122cc1 / cc8 / cd8
  k_.bsLane1Cone_deg = 30.0f;
  k_.bsLane1Rad_m = 2.0f;
  in_.players[12].active = true;
  in_.players[12].pos = {0.0f, 9.0f};
  in_.players[12].vel = {0.0f, -1.2f};
  EXPECT_EQ(pes::BackSpaceUnit(in_, k_).index, 5);
}
