// PES 2021's team sheet and transition concepts (pesteamconcepts.hpp).
// Synthetic constants exercise the rules; one install test checks the loader
// against the user's PES2021.exe and VFS through relations, not pinned numbers.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>

#include "gametypes.hpp"
#include "onthepitch/pesteamconcepts.hpp"

namespace {

using blunted::Vector3;

constexpr float kHalfLength = 52.5f;  // gametypes pitchHalfW
constexpr float kBoxLine = pesPenaltyAreaLineX;  // F6
constexpr float kBoxHalfWidth = pesPenaltyAreaHalfWidth;  // F7

pes::TeamConceptConstants Synthetic() {
  pes::TeamConceptConstants k{};
  k.defaultSliders.fill(5);
  k.enabled.fill(true);
  k.endTestAfter_s = 0.1f;
  k.chaseWindow_s = 3, k.chaseThirdMargin_m = 10, k.chaseWideMargin_m = 2;
  k.chaseRangeSq_m2 = 100, k.chaseGoalSideMax_deg = 100, k.chaseInsideMin_deg = 90;
  k.chaseSecondRangeSq_m2 = 400, k.chaseSecondWidthSlack_m = 15;
  k.chaseTargetDistance_m = 8, k.chaseTargetTurn_deg = 160;
  k.counterWindow_s = 3, k.counterKickoffGuard_s = 6, k.counterFacingMax_deg = 55;
  k.counterBallMaxDepth_m = -12, k.counterLineMargin_m = -10, k.counterAbortLineMargin_m = -14;
  k.counterAbortWideMargin_m = 4, k.counterMaxGoalSide = 2;
  k.runnerUnmarkedSq_m2 = 42.25f, k.runnerChannelShare = 0.5f, k.runnerChannelSlack_m = 11;
  k.runnerCentreFactor = 1.2f, k.runnerAreaFactor = 0.7f, k.runnerWideLane_m = 17;
  k.supportAhead_m = 15, k.supportSide_m = 12, k.supportTurn_deg = 22, k.supportMinAhead_m = 8;
  k.supportHoldBonus_m = -5;
  k.counterRunGrace_s = 2, k.counterMaxAge_s = 6, k.counterRunnerFarSq = 1369, k.counterRunnerMaxDx_m = 30;
  k.foreCheckWindow_s = 5, k.foreCheckHold_s = 2, k.foreCheckBallFloor_m = -10, k.foreCheckCorridor_m = 12;
  k.pressGoalSide_deg = 70, k.pressRampFrom_deg = 100, k.pressRampTo_deg = 170;
  k.pressRadius_m = 8, k.pressRadiusRamp_m = 4, k.pressBonusAbove = 0.2f, k.pressCount = 3;
  k.lineDownBallDivisor = -2.5f, k.lineDownBallMax_m = 27, k.lineDownGap_m = 3, k.lineDownLongPassFacing_deg = 70;
  k.retreatGap_m = 5, k.retreatBackFront_m = 8, k.retreatMidFront_m = 18;
  k.pushUpBehind_m = -4, k.pushUpBallGap_m = 6, k.pushUpCap_m = -5, k.measuredLineClamp_m = 4;
  k.pressLaneRadius_m = 6.5f, k.pressLaneBonus_m = 5;
  k.uncoveredLineMargin_m = 12, k.uncoveredNearSq_m2 = 9, k.uncoveredFacing_deg = 90;
  k.uncoveredRampFrom_m = 5, k.uncoveredRampTo_m = 20, k.uncoveredRampSpan_m = 15;
  k.uncoveredRadiusRamp_m = 3, k.uncoveredRadius_m = 4, k.uncoveredCone_deg = 80;
  k.lineUpWideMargin_m = 5, k.lineUpBallMax_m = 18, k.lineUpBallMin_m = -10;
  k.containFarSq_m2 = 12.25f, k.containNearSq_m2 = 4, k.containNearAngle_deg = 85, k.containFacing_deg = 135;
  k.containWideFrom_m = 10, k.containCone_deg = 90, k.containRadius_m = 3;
  k.lineUpBandAbove_m = 7, k.lineUpBallAhead_m = 5;
  return k;
}

TEST(PesForeCheck, LineUpRunsWhileOurManContainsAWideCarrierAndLiftsTheBackEdge) {
  const auto k = Synthetic();
  pes::DefenceConceptInput in;
  in.inPlay = true;
  in.ball = Vector3(5, 22, 0);
  in.holder = in.ball;
  in.holderOpponent = true, in.holderHeading_deg = 90;  // facing away from our goal
  in.mates = {Vector3(2, 22, 0)};                       // 3 m goal-side
  in.presser = 0;
  EXPECT_TRUE(pes::ForeCheckLineUpStart(k, in, false));
  EXPECT_FALSE(pes::ForeCheckLineUpStart(k, in, true));  // LineDown runs
  in.mates[0] = Vector3(5, 25, 0);                       // beside him, not goal-side
  EXPECT_TRUE(pes::ForeCheckLineUpEnd(k, in, false));
  in.mates[0] = Vector3(2, 22, 0);
  in.ball.coords[1] = in.holder.coords[1] = 5;           // central
  EXPECT_TRUE(pes::ForeCheckLineUpEnd(k, in, false));
  EXPECT_FLOAT_EQ(pes::LineUpBackEdge(k, 5, -20, -22), -13);
  EXPECT_FLOAT_EQ(pes::LineUpBackEdge(k, 5, -20, -10), -10);
}

// Ball at (10, 0) in the opponents' half, three men round it, one goal-side.
pes::DefenceConceptInput PressScene() {
  pes::DefenceConceptInput in;
  in.inPlay = true;
  in.sincePossessionChange_s = 1;
  in.dir = 1;
  in.ball = Vector3(10, 0, 0);
  in.opponentsHoldBall = true;
  in.teamMid = 20;  // our block is up: the ball is not beyond its midpoint
  in.mates = {Vector3(5, 0, 0), Vector3(10, 7, 0), Vector3(20, 0, 0)};  // goal-side, side, behind (11 m)
  return in;
}

TEST(PesForeCheck, PressCountNeedsThreeInsideTheirAngleRadiusAndOneGoalSide) {
  const auto k = Synthetic();
  auto in = PressScene();
  EXPECT_TRUE(pes::ForeCheckPressCount(k, in));      // behind man: w = 1, r = 12
  in.mates[2] = Vector3(10, -9, 0);                   // side man at 9 m: w = 0, r = 8
  EXPECT_FALSE(pes::ForeCheckPressCount(k, in));
  in = PressScene();
  in.mates[0] = Vector3(10, -5, 0);                   // nobody goal-side
  EXPECT_FALSE(pes::ForeCheckPressCount(k, in));
  in = PressScene();
  in.opponentsHoldBall = false;
  in.mates.clear();
  EXPECT_TRUE(pes::ForeCheckPressCount(k, in));
}

TEST(PesForeCheck, PressStartsInTheirHalfAndHoldsTwoSecondsThenRecounts) {
  const auto k = Synthetic();
  auto in = PressScene();
  EXPECT_TRUE(pes::ForeCheckPressStart(k, in));
  in.teamMid = 5;  // ball 10 m upfield, beyond our midpoint and inside 12 m: still in our block
  EXPECT_FALSE(pes::ForeCheckPressStart(k, in));
  in.ball.coords[0] = 13;  // past the 12 m corridor
  EXPECT_TRUE(pes::ForeCheckPressStart(k, in));
  in = PressScene();
  in.ball.coords[0] = -1;
  EXPECT_FALSE(pes::ForeCheckPressStart(k, in));
  in = PressScene();
  in.sincePossessionChange_s = 5.5f;
  EXPECT_FALSE(pes::ForeCheckPressStart(k, in));
  EXPECT_TRUE(pes::ForeCheckPressEnd(k, in));
  in = PressScene();
  in.mates.pop_back();
  in.sincePossessionChange_s = 1.5f;
  EXPECT_FALSE(pes::ForeCheckPressEnd(k, in));  // inside the 2 s hold
  in.sincePossessionChange_s = 3;
  EXPECT_TRUE(pes::ForeCheckPressEnd(k, in));
  in = PressScene();
  in.ball.coords[0] = -12;
  EXPECT_TRUE(pes::ForeCheckPressEnd(k, in));
}

TEST(PesForeCheck, LineDownAndRetreatFollowTheLineGaps) {
  const auto k = Synthetic();
  pes::DefenceConceptInput in;
  in.inPlay = true;
  in.ball = Vector3(0, 0, 0);
  in.measuredBack = -20, in.backBand = -25;  // 5 m off its band
  in.measuredMid = -10, in.midBand = -12;
  EXPECT_FALSE(pes::ForeCheckLineDownStart(k, in));  // no trigger
  in.attackLevelZero = true;
  EXPECT_TRUE(pes::ForeCheckLineDownStart(k, in));
  in.keeperHolds = true;
  EXPECT_FALSE(pes::ForeCheckLineDownStart(k, in));
  in.keeperHolds = false;
  in.backBand = -22;  // within 3 m
  EXPECT_TRUE(pes::ForeCheckLineDownEnd(k, in));
  in.attackLevelZero = false, in.backBand = -25;
  in.holderLongPass = true, in.holderHeading_deg = 100;  // within 70 of our attack heading (90)
  EXPECT_FALSE(pes::ForeCheckLineDownEnd(k, in));
  EXPECT_FALSE(pes::RetreatBlockCreateEnd(k, in));  // back line 5 m off and in front of the box
  in.backBand = -21;
  EXPECT_TRUE(pes::RetreatBlockCreateEnd(k, in));   // both lines within 5 m
  in.backBand = -25, in.measuredBack = -30, in.measuredMid = -20;  // deep: behind 8 - F6 and 18 - F6
  EXPECT_TRUE(pes::RetreatBlockCreateEnd(k, in));
}

TEST(PesForeCheck, PushUpLiftsTheBandsToFourMetresBehindTheMenCapped) {
  const auto k = Synthetic();
  EXPECT_FLOAT_EQ(pes::BandPushUp_m(k, -20, 10, -30), 6);  // -24 vs band -30
  EXPECT_FLOAT_EQ(pes::BandPushUp_m(k, -20, -22, -30), 2);  // ball - 6 = -28
  EXPECT_FLOAT_EQ(pes::BandPushUp_m(k, 10, 30, -30), 25);   // capped at -5
  EXPECT_FLOAT_EQ(pes::BandPushUp_m(k, -20, 10, -20), 0);   // never down
}

TEST(PesForeCheck, GoalSidePresserWidensTheWideAngleRadiiByFive) {
  const auto k = Synthetic();
  auto in = PressScene();
  in.mates[2] = Vector3(25, 0, 0);  // behind the ball at 15 m: r 12, 17 with the bonus
  EXPECT_FALSE(pes::ForeCheckPressCount(k, in));
  in.presser = 0;                   // (5, 0): 5 m from the ball, goal-side
  EXPECT_TRUE(pes::ForeCheckPressCount(k, in));
  in.mates[0] = Vector3(10, -5, 0); // presser no longer goal-side: no bonus (and no close man)
  EXPECT_FALSE(pes::ForeCheckPressCount(k, in));
}

TEST(PesForeCheck, LineDownStartsOnAnUncoveredCarrierFacingOurGoal) {
  const auto k = Synthetic();
  pes::DefenceConceptInput in;
  in.inPlay = true;
  in.ball = Vector3(0, 0, 0);
  in.measuredBack = -20, in.backBand = -25;
  in.ownLine = -30;
  in.holderOpponent = true, in.holderHeading_deg = 270;
  in.allMen = {Vector3(-10, 0, 0)};  // 10 m away: outside the 7 m radius
  EXPECT_TRUE(pes::ForeCheckLineDownStart(k, in));
  in.allMen.push_back(Vector3(-5, 1, 0));  // in the cone, inside 7 m
  EXPECT_FALSE(pes::ForeCheckLineDownStart(k, in));
  in.allMen.pop_back();
  in.holderHeading_deg = 90;  // facing away from our goal
  EXPECT_FALSE(pes::ForeCheckLineDownStart(k, in));
}

TEST(PesTeamSheet, GatesFollowTheToggles) {
  pes::TeamSheet s;
  s.SetToggle(pes::kDefensiveStyle, 0);
  s.SetToggle(pes::kAttackingStyle, 0);
  EXPECT_TRUE(pes::ConceptGate(pes::kConceptTransitionChase, s));
  EXPECT_TRUE(pes::ConceptGate(pes::kConceptSideCounter, s));
  s.SetToggle(pes::kDefensiveStyle, 1);  // All-out Defence: no fore-check family
  s.SetToggle(pes::kAttackingStyle, 1);  // Possession Game: no counters
  EXPECT_FALSE(pes::ConceptGate(pes::kConceptTransitionChase, s));
  EXPECT_FALSE(pes::ConceptGate(pes::kConceptSideCounter, s));
}

// The sheet is PES's block S = T+0x8e08. Each accessor reads the offset its PES
// function reads and refuses what that function refuses; plan 3 is the current
// plan (the index stored at S+0, FUN_1408c5e20).
TEST(PesTeamSheet, PlanThreeIsTheStoredPlanAndExplicitPlansStayIndependent) {
  pes::TeamSheet s;
  s.SetSlider(0, 7, 0);
  s.SetSlider(0, 4, 2);
  s.SetPlan(2);
  EXPECT_EQ(s.Plan(), 2);
  EXPECT_EQ(s.Slider(0), 4);
  EXPECT_EQ(s.Slider(0, 0), 7);
  s.SetPlan(3);  // FUN_1408c5e20 refuses anything above 2
  EXPECT_EQ(s.Plan(), 2);
}

TEST(PesTeamSheet, SlidersKeepAnyByteAndRefuseAnOutOfRangeSliderOrPlan) {
  pes::TeamSheet s;
  s.SetSlider(4, 255, 1);
  EXPECT_EQ(s.Slider(4, 1), 255);
  s.SetSlider(5, 9, 1);
  EXPECT_EQ(s.Slider(5, 1), 0);
  s.SetSlider(0, 9, 3);
  EXPECT_EQ(s.Slider(0, 0), 9);
}

TEST(PesTeamSheet, TogglesStoreOneForAnyValueAboveOne) {
  pes::TeamSheet s;
  s.SetToggle(pes::kPressuring, 5, 0);
  EXPECT_EQ(s.Toggle(pes::kPressuring, 0), 1);
  s.SetToggle(pes::kPressuring, 0, 0);
  EXPECT_EQ(s.Toggle(pes::kPressuring, 0), 0);
}

TEST(PesTeamSheet, RolesAcceptBelowThirteenAndReadTwelveOutOfRange) {
  pes::TeamSheet s;
  EXPECT_EQ(s.Role(3, 1, 0), 0);
  s.SetRole(3, 12, 1, 0);
  EXPECT_EQ(s.Role(3, 1, 0), 12);
  s.SetRole(3, 13, 1, 0);
  EXPECT_EQ(s.Role(3, 1, 0), 12);
  EXPECT_EQ(s.Role(11, 0, 0), 12);
  EXPECT_EQ(s.RegisteredRole(11, 0, 0), 12);
}

TEST(PesTeamSheet, CoordinatesStoreBothAxesWithinPesBounds) {
  pes::TeamSheet s;
  s.SetCoord(9, 48, 104, 2, 1);
  EXPECT_EQ(s.CoordX(9, 2, 1), 48);
  EXPECT_EQ(s.CoordY(9, 2, 1), 104);
  s.SetCoord(9, 49, 0, 2, 1);
  EXPECT_EQ(s.CoordX(9, 2, 1), 48);
  s.SetCoord(10, 1, 1, 0, 0);
  EXPECT_EQ(s.CoordX(10, 0, 0), 0);
}

TEST(PesTeamSheet, SlotTargetsAreSlotsOrNone) {
  pes::TeamSheet s;
  s.SetSlotTarget(4, 10, 0);
  EXPECT_EQ(s.SlotTarget(4, 0), 10);
  s.SetSlotTarget(4, 11, 0);
  EXPECT_EQ(s.SlotTarget(4, 0), 10);
  s.SetSlotTarget(4, 0xff, 0);
  EXPECT_EQ(s.SlotTarget(4, 0), 0xff);
}

TEST(PesTeamSheet, CaptainDesignatedAndTakersAreRosterIndicesOrNone) {
  pes::TeamSheet s;
  s.SetCaptain(5);
  EXPECT_EQ(s.Captain(), 5);
  s.SetCaptain(pes::kSheetRosterBound);  // FUN_1408c5c50 refuses 0x28
  EXPECT_EQ(s.Captain(), 5);
  s.SetDesignated(5, 7);
  EXPECT_EQ(s.Designated(5), 7);
  EXPECT_EQ(s.Designated(6), 0xff);
  s.SetTaker(2, 3);
  EXPECT_EQ(s.Taker(2), 3);
  EXPECT_EQ(s.Taker(3), 0xff);
}

TEST(PesTeamSheet, SlotRecordBStoresTwelveBytesPerSlotPhaseAndPlan) {
  pes::TeamSheet s;
  const std::array<uint32_t, 3> rec{11, 22, 33};
  s.SetSlotRecordB(6, 1, rec, 2);
  EXPECT_EQ(s.SlotRecordB(6, 1, 2), rec);
  EXPECT_EQ(s.SlotRecordB(6, 1, 0), (std::array<uint32_t, 3>{}));
}

TEST(PesTeamSheet, InstructionWriterKeepsIdArgumentParamAndFlag) {
  pes::TeamSheet s;
  s.SetInstruction(1, 0x5, 7, 4, 0, 1);
  EXPECT_EQ(s.InstructionId(1, 0), 5);
  EXPECT_EQ(s.InstructionArg(1, 0), 0xff);  // only id 0xe stores the argument
  EXPECT_EQ(s.InstructionParam(1, 0), 4);
  EXPECT_EQ(s.InstructionFlag(1, 0), 1);
  s.SetInstruction(1, 0xe, 7, 4, 0, 0);
  EXPECT_EQ(s.InstructionArg(1, 0), 7);
  EXPECT_EQ(s.InstructionFlag(1, 0), 0);
  s.SetInstruction(1, 0x11, 3, 4, 0, 1);  // FUN_140a42600 refuses id 0x11
  EXPECT_EQ(s.InstructionId(1, 0), 0xe);
}

TEST(PesTeamSheet, InstructionIdsEightTenAndFifteenFlagTheArgumentSlot) {
  pes::TeamSheet s;
  s.SetInstruction(0, 0xf, 13, 0, 0, 0);  // 13 % 11 = slot 2
  EXPECT_EQ(s.InstructionSlotFlag(0, 2, 0), 1);
  EXPECT_EQ(s.InstructionSlotFlag(0, 1, 0), 0);
  s.SetInstruction(0, 0x8, 21, 0, 0, 0);  // 21 % 11 = slot 10
  EXPECT_EQ(s.InstructionSlotFlag(0, 10, 0), 1);
  s.SetInstruction(0, 0xa, 22, 0, 0, 0);  // argument 22 is refused
  EXPECT_EQ(s.InstructionSlotFlag(0, 10, 0), 0);
}

TEST(PesTeamSheet, IdZeroLeavesTheFlagByteAlone) {
  pes::TeamSheet s;
  s.SetInstruction(2, 0x3, 0, 0, 1, 1);
  s.SetInstruction(2, 0x0, 0, 0, 1, 0);
  EXPECT_EQ(s.InstructionFlag(2, 1), 1);
}

TEST(PesTeamSheet, OutOfRangeRecordIndexWritesRecordZero) {
  pes::TeamSheet s;
  s.SetInstructionByte30(4, 9, 0);
  EXPECT_EQ(s.InstructionByte30(0, 0), 9);
  EXPECT_EQ(s.InstructionByte30(1, 0), 0);
}

TEST(PesTeamSheet, ResetPutsARecordBackToThePesResetState) {
  pes::TeamSheet s;
  s.SetInstruction(3, 0xe, 9, 5, 2, 1);
  s.SetInstructionByte30(3, 7, 2);
  s.ResetInstruction(3, 2);
  EXPECT_EQ(s.InstructionId(3, 2), 0);
  EXPECT_EQ(s.InstructionArg(3, 2), 0xff);
  EXPECT_EQ(s.InstructionParam(3, 2), 0);
  EXPECT_EQ(s.InstructionByte30(3, 2), 0);
  EXPECT_EQ(s.InstructionFlag(3, 2), 0);
  EXPECT_EQ(s.InstructionSlotFlag(3, 9, 2), 0);
}

// The sheet starts at plan 0 on both sides (plan-selector §3): the only writer of setup+0xB34 is
// the reset (MOV [RBX+0xB34/0xBDC],EDI at 0x1408ADEA5/0x1408ADFAD with EDI = 0), so the exhibition
// reader (FUN_1408E16D0) picks up 0.
TEST(PesTeamSheetDefaults, DefaultsAreTheConstructorStoresFromTheInstall) {
  pes::TeamConceptConstants k;
  k.defaultSliders[pes::kSliderSupportRange] = 6;
  k.defaultSliders[pes::kSliderDefensiveLine] = 6;
  k.defaultSliders[pes::kSliderCompactness] = 7;
  k.defaultCaptain = 9;
  k.defaultDesignated = {8, 8, 8, 8, 8, 8};
  k.defaultTaker = {0xff, 0xff, 0xff};
  k.defaultSlotTarget = 0xff;
  const pes::TeamSheet s = pes::DefaultTeamSheet(k);
  EXPECT_EQ(s.Plan(), 0);
  EXPECT_EQ(s.Slider(pes::kSliderSupportRange), 6);
  EXPECT_EQ(s.Slider(pes::kSliderDefensiveLine), 6);
  EXPECT_EQ(s.Slider(pes::kSliderCompactness), 7);
  EXPECT_EQ(s.Captain(), 9);
  EXPECT_EQ(s.Designated(5), 8);
  EXPECT_EQ(s.Taker(0), 0xff);
  EXPECT_EQ(s.SlotTarget(10, 2), 0xff);
  EXPECT_EQ(s.Role(10, 2, 2), 0);
  EXPECT_EQ(s.CoordX(9, 2, 2), 0);
}

// A wide carrier in our attacking third, the chaser goal-side and inside him,
// one wide team-mate within reach.
pes::TransitionChaseInput ChaseScene() {
  pes::TransitionChaseInput in;
  in.inPlay = true;
  in.sincePossessionChange_s = 1.0f;
  in.dir = 1;
  in.carrier = Vector3(30, 22, 0);
  in.chaser = 0;
  in.men = {{Vector3(26, 19, 0), false, false},  // chaser: behind (toward +x goal) and inside
            {Vector3(24, 25, 0), false, true},   // widest
            {Vector3(28, 10, 0), false, true}};
  return in;
}

TEST(PesTransitionChase, StartsOnAWideCarrierAndPicksTheWidestSecondMan) {
  const auto pick = pes::TransitionChaseStart(Synthetic(), ChaseScene());
  ASSERT_TRUE(pick);
  EXPECT_EQ(pick->chaser, 0);
  EXPECT_EQ(pick->second, 1);
}

TEST(PesTransitionChase, EveryStartRuleVetoes) {
  const auto k = Synthetic();
  auto in = ChaseScene();
  in.sincePossessionChange_s = 3.5f;
  EXPECT_FALSE(pes::TransitionChaseStart(k, in));
  in = ChaseScene();
  in.carrier.coords[1] = 5;  // central
  EXPECT_FALSE(pes::TransitionChaseStart(k, in));
  in = ChaseScene();
  in.carrier.coords[0] = kBoxLine - 12;  // short of our attacking third
  EXPECT_FALSE(pes::TransitionChaseStart(k, in));
  in = ChaseScene();
  in.carrierKicking = true;
  EXPECT_FALSE(pes::TransitionChaseStart(k, in));
  in = ChaseScene();
  in.men[0].pos = Vector3(26, 24, 0);  // outside the carrier
  EXPECT_FALSE(pes::TransitionChaseStart(k, in));
  in = ChaseScene();
  in.men[0].pos = Vector3(34, 20, 0);  // not goal-side (the opponents attack -x)
  EXPECT_FALSE(pes::TransitionChaseStart(k, in));
  in = ChaseScene();
  in.men[1].available = in.men[2].available = false;
  EXPECT_FALSE(pes::TransitionChaseStart(k, in));
}

TEST(PesTransitionChase, TrapPointIsBehindTheChaserOnTheTouchlineSide) {
  const auto k = Synthetic();
  const Vector3 p = pes::TransitionChaseTarget(k, Vector3(26, 19, 0), Vector3(30, 22, 0), 1);
  const float rad = 20.0f * 3.14159265f / 180.0f;  // 180 - 160
  EXPECT_NEAR(p.coords[0], 26 - 8 * std::cos(rad), 1e-3);
  EXPECT_NEAR(p.coords[1], 19 + 8 * std::sin(rad), 1e-3);
  // Mirrored team and touchline: still behind, still outward.
  const Vector3 q = pes::TransitionChaseTarget(k, Vector3(-26, -19, 0), Vector3(-30, -22, 0), -1);
  EXPECT_NEAR(q.coords[0], -26 + 8 * std::cos(rad), 1e-3);
  EXPECT_NEAR(q.coords[1], -19 - 8 * std::sin(rad), 1e-3);
}

// Ball won in our half, two opponents goal-side, a free winger.
pes::SideCounterInput CounterScene() {
  pes::SideCounterInput in;
  in.attackAllowed = true;
  in.sincePossessionChange_s = 0.5f;
  in.sinceKickoff_s = 1e9f;
  in.dir = 1;
  in.ball = Vector3(-20, 0, 0);
  in.holder = 0;
  in.holderHeading_deg = 90;  // facing +x
  in.oppLineX = 10;
  in.opponents = {Vector3(12, 5, 0), Vector3(14, -6, 0), Vector3(-25, 3, 0)};
  in.men = {{Vector3(-20, 0, 0), false, true, 100},
            {Vector3(-5, 25, 0), true, true, 400},    // winger, free
            {Vector3(-15, 8, 0), false, true, 100},
            {Vector3(-10, -12, 0), true, true, 16}};  // marked
  return in;
}

TEST(PesSideCounter, StartsAndPicksTheFreeWideMan) {
  const auto runner = pes::SideCounterStart(Synthetic(), CounterScene());
  ASSERT_TRUE(runner);
  EXPECT_EQ(*runner, 1);
}

TEST(PesSideCounter, EveryStartRuleVetoes) {
  const auto k = Synthetic();
  auto in = CounterScene();
  in.attackAllowed = false;
  EXPECT_FALSE(pes::SideCounterStart(k, in));
  in = CounterScene();
  in.sinceKickoff_s = 2;
  EXPECT_FALSE(pes::SideCounterStart(k, in));
  in = CounterScene();
  in.holderHeading_deg = 160;
  EXPECT_FALSE(pes::SideCounterStart(k, in));
  in = CounterScene();
  in.ball.coords[0] = -5;  // not 12 m short of halfway
  EXPECT_FALSE(pes::SideCounterStart(k, in));
  in = CounterScene();
  in.opponents.push_back(Vector3(15, -2, 0));  // a third man goal-side
  EXPECT_FALSE(pes::SideCounterStart(k, in));
  in = CounterScene();
  in.keeperHolds = true;
  EXPECT_FALSE(pes::SideCounterStart(k, in));
}

TEST(PesSideCounter, RunTargetIsTheChannelAtTheOffsideLine) {
  const auto k = Synthetic();
  const Vector3 t = pes::SideCounterRunTarget(k, Vector3(-5, 30, 0), 10);
  EXPECT_FLOAT_EQ(t.coords[0], 10);
  EXPECT_FLOAT_EQ(t.coords[1], 30);  // already wider than the channel
  const Vector3 u = pes::SideCounterRunTarget(k, Vector3(-5, -3, 0), 10);
  EXPECT_FLOAT_EQ(u.coords[1], -(kBoxHalfWidth + 0.5f * (34.0f - kBoxHalfWidth)));
}

TEST(PesSideCounter, SupportersTakeAheadAndBothSides) {
  const auto k = Synthetic();
  auto in = CounterScene();
  in.men.push_back({Vector3(-8, 0, 0), false, true, 100});    // 12 m ahead of the ball
  in.men.push_back({Vector3(-18, -12, 0), false, true, 100}); // low side
  const auto s = pes::SideCounterSupporters(k, in, 1, {-1, -1, -1});
  EXPECT_EQ(s.men[0], 4);
  EXPECT_NEAR(s.points[0].coords[0], -20 + 15, 1e-3);
  EXPECT_EQ(s.men[1], 5);  // point 1 turns from 180 deg: the low side
  EXPECT_EQ(s.men[2], 2);
}

TEST(PesSideCounter, EndsWhenTheRunIsOverOrStale) {
  const auto k = Synthetic();
  const Vector3 ball(-20, 0, 0);
  EXPECT_FALSE(pes::SideCounterEnd(k, true, true, 1.0f, ball, Vector3(0, 20, 0)));
  EXPECT_TRUE(pes::SideCounterEnd(k, false, true, 1.0f, ball, Vector3(0, 20, 0)));
  EXPECT_TRUE(pes::SideCounterEnd(k, true, false, 1.0f, ball, Vector3(0, 20, 0)));
  EXPECT_TRUE(pes::SideCounterEnd(k, true, true, 2.5f, ball, Vector3(15, 20, 0)));  // 35 m ahead
  EXPECT_FALSE(pes::SideCounterEnd(k, true, true, 2.5f, ball, Vector3(5, 20, 0)));
  EXPECT_TRUE(pes::SideCounterEnd(k, true, true, 6.5f, ball, Vector3(5, 20, 0)));
}

TEST(PesTeamConceptsInstall, LoadsRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto k = pes::LoadTeamConceptConstants(*exe, vfs, &err);
  ASSERT_TRUE(k) << err;
  for (int v : k->defaultSliders) {
    EXPECT_GE(v, 0);
    EXPECT_LE(v, 10);
  }
  EXPECT_TRUE(k->defaultCaptain < pes::kSheetRosterBound || k->defaultCaptain == 0xff);
  for (int v : k->defaultDesignated)
    EXPECT_TRUE(v < pes::kSheetRosterBound || v == 0xff);
  EXPECT_GT(k->chaseSecondRangeSq_m2, k->chaseRangeSq_m2);
  EXPECT_LT(k->chaseInsideMin_deg, k->chaseGoalSideMax_deg);
  EXPECT_GT(k->counterKickoffGuard_s, k->counterWindow_s);
  EXPECT_GT(k->counterMaxAge_s, k->counterRunGrace_s);
  EXPECT_LT(k->counterAbortLineMargin_m, k->counterLineMargin_m);
  EXPECT_GT(k->supportAhead_m, k->supportSide_m);
  EXPECT_LT(k->runnerAreaFactor, 1.0f);
  EXPECT_GT(k->runnerCentreFactor, 1.0f);
  // The concepts this port runs are enabled in whatever the VFS resolves.
  EXPECT_TRUE(k->enabled[pes::kConceptTransitionChase]);
  (void)kHalfLength;
}

}  // namespace
