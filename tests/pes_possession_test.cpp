// PES 2021's team-possession latch (onthepitch/pespossession.hpp): synthetic
// inputs check each branch of the decoded rule, one install test reads the
// game speed the latch's windows are measured in.
#include <gtest/gtest.h>

#include <cstdlib>

#include "onthepitch/pesclock.hpp"
#include "onthepitch/pespossession.hpp"
#include "pes/exe.hpp"

namespace {

// An input in which no branch decides: open play, both heads valid, no carrier,
// no event, and a frame whose age from the (absent) event is off the cadence.
pes::PossessionLatchInput Base() {
  pes::PossessionLatchInput in;
  in.team = 0;
  in.old = false;
  in.restartState = 1;
  in.ownHeadValid = true;
  in.otherHeadValid = true;
  in.newestKind = 0;
  in.newestFrame = 0;
  in.frame = 100;  // age 100: (100 - 2) % 5 != 0, so step 3 does not decide
  in.gs = 54;
  return in;
}

// Holder-side inputs for step 3b: age 12 is past the step-2 window (9) and on the
// cadence ((12 - 2) % 5 == 0), with 3a switched off.
pes::PossessionLatchInput ThreeB(int team) {
  pes::PossessionLatchInput in = Base();
  in.team = team;
  in.newestKind = 7;  // touch: not a step-2 kind, not the shot
  in.newestFrame = 88;  // age 12
  in.holderValid = true;
  in.holderSide = 0;
  in.holderTeamLatch = true;
  return in;
}

}  // namespace

TEST(PesPossessionFrames, FrameIsTheFloorOfMillisecondsTimesGameSpeed) {
  EXPECT_EQ(pes::PesFrameOf(0, 54), 0);
  EXPECT_EQ(pes::PesFrameOf(10, 54), 0);    // 0.54 frames
  EXPECT_EQ(pes::PesFrameOf(999, 54), 53);  // 53.9 frames
  EXPECT_EQ(pes::PesFrameOf(1000, 54), 54);
}

TEST(PesPossessionFrames, GfEventKindsMapOntoTheNumberedRing) {
  EXPECT_EQ(pes::PesEventKindOf(pes::BallEventKind::kTouch), 7);
  EXPECT_EQ(pes::PesEventKindOf(pes::BallEventKind::kPass), 1);
  EXPECT_EQ(pes::PesEventKindOf(pes::BallEventKind::kHighPass), 2);
  EXPECT_EQ(pes::PesEventKindOf(pes::BallEventKind::kShot), 5);
}

TEST(PesPossessionLatch, RestartStateThreeGivesTheRestartTeamBeforeAnyOtherTerm) {
  pes::PossessionLatchInput in = Base();
  in.restartState = 3;
  in.restartTeam = 0;
  in.ownHeadValid = false;  // R comes before the heads gate
  in.team = 0;
  in.old = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.team = 1;
  in.old = true;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, RestartStateTwoIsRestartOnlyWhenTheGateIsNotOne) {
  pes::PossessionLatchInput in = Base();
  in.restartState = 2;
  in.restartTeam = 1;
  in.team = 0;
  in.ownHeadValid = false;  // with R off, the heads gate keeps the old value
  in.old = true;
  in.restartGate = 1;  // FUN_140a3c950 == 1: R is off, so the old value (true) is kept
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.restartGate = 0;  // R on: the restart team is 1, so team 0 loses the latch
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, OpenPlayNeverUsesTheRestartTeam) {
  pes::PossessionLatchInput in = Base();
  in.restartState = 1;
  in.restartTeam = 0;
  in.team = 1;
  in.old = true;  // if R were on, team 1 would lose the latch; open play keeps it
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, AnInvalidRankedHeadKeepsThePreviousValue) {
  pes::PossessionLatchInput in = Base();
  in.carrierValid = true;  // step 1 would say team 0 holds it
  in.carrierSide = 0;
  in.team = 0;
  in.ownHeadValid = false;
  in.old = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.old = true;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.ownHeadValid = true;
  in.otherHeadValid = false;
  in.old = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, TheCarrierDecidesStepOneWhateverTheEvent) {
  pes::PossessionLatchInput in = Base();
  in.carrierValid = true;
  in.carrierSide = 1;
  in.newestKind = 1;  // step 2 would say team 0 (its actor is on side 0)...
  in.newestFrame = 95;
  in.actorValid = true;
  in.actorSide = 0;
  in.team = 0;
  in.old = true;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));  // ...but the carrier, side 1, decides
  in.team = 1;
  in.old = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, StepTwoTakesTheNewestActorInsideTheWindow) {
  pes::PossessionLatchInput in = Base();
  in.newestKind = 1;
  in.newestFrame = 92;  // age 8 < 9
  in.actorValid = true;
  in.actorSide = 0;
  in.team = 0;
  in.old = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.team = 1;  // the actor is on side 0: team 1 does not hold it
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.team = 0;  // either old value gives the actor's side
  in.old = true;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.team = 1;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, StepTwoOnlyForTheKindsItLists) {
  pes::PossessionLatchInput in = Base();
  in.newestFrame = 95;  // age 5: inside the window
  in.actorValid = true;
  in.actorSide = 0;
  in.team = 0;
  in.old = true;
  in.newestKind = 7;  // a touch is not a step-2 kind: the old value is kept
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.old = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.newestKind = 5;  // nor a shot
  in.old = true;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.newestKind = 0x10;  // a restart is
  in.old = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.newestKind = 2;  // the kicks 1..3 all decide in step 2
  in.old = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.newestKind = 3;
  in.old = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.newestKind = 4;  // not a kick
  in.old = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, AnInvalidActorInStepTwoKeepsThePreviousValue) {
  pes::PossessionLatchInput in = Base();
  in.newestKind = 1;
  in.newestFrame = 95;
  in.actorValid = false;
  in.team = 0;
  in.old = true;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.old = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, StepTwoWindowEndsAtGameSpeedOverSix) {
  pes::PossessionLatchInput in = Base();
  in.newestKind = 1;
  in.actorValid = true;
  in.actorSide = 0;
  in.team = 0;
  in.old = false;
  in.newestFrame = 92;  // age 8: inside
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.newestFrame = 91;  // age 9: outside. Step 3 at age 9: (9 - 2) % 5 != 0, so kept
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, TheWindowFollowsTheGameSpeed) {
  pes::PossessionLatchInput in = Base();
  in.gs = 60;  // gs / 6 = 10: age 9 is inside, age 10 is outside
  in.newestKind = 1;
  in.actorValid = true;
  in.actorSide = 0;
  in.team = 0;
  in.old = false;
  in.newestFrame = 91;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.newestFrame = 90;  // age 10: step 3, (10 - 2) % 5 != 0, so kept
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, StepThreeOnlyOnTheCadenceOfTheNewestEvent) {
  pes::PossessionLatchInput in = ThreeB(0);
  in.old = false;
  in.newestFrame = 88;  // age 12: (12 - 2) % 5 == 0, a decision frame
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.newestFrame = 87;  // age 13: (13 - 2) % 5 != 0
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.newestFrame = 86;  // age 14: (14 - 2) % 5 != 0
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.newestFrame = 83;  // age 17: (17 - 2) % 5 == 0 again
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, TheShotIsNotAStepThreeKind) {
  pes::PossessionLatchInput in = ThreeB(0);
  in.old = false;
  in.newestKind = 5;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, TheSubThreeExclusionKeepsKindThreeWithSubKindThreeOrFour) {
  pes::PossessionLatchInput in = ThreeB(0);
  in.old = false;
  in.newestKind = 3;
  in.newestSubKind = 3;
  in.stateFlag40_80 = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));  // excluded
  in.newestSubKind = 4;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));  // excluded
  in.stateFlag40_80 = true;  // the flag lifts the exclusion
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.stateFlag40_80 = false;
  in.newestSubKind = 2;  // not 3 or 4: not excluded
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, ThreeADecidesByTheNewestActorWhenItsGatesHold) {
  pes::PossessionLatchInput in = ThreeB(1);  // 3b would say side 0, team 1 loses
  in.old = false;
  in.newestFlag2d = true;
  in.newestField1c = 4;
  in.actorValid = true;
  in.actorSide = 1;  // 3a: the actor's side is 1, so team 1 holds it
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.team = 0;
  in.old = true;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, EachThreeAGateSwitchesTheBranchOff) {
  // With 3a off, 3b decides: the holder is on side 0, so team 1 does not hold it.
  pes::PossessionLatchInput base = ThreeB(1);
  base.old = false;
  base.newestFlag2d = true;
  base.newestField1c = 4;
  base.actorValid = true;
  base.actorSide = 1;
  EXPECT_TRUE(pes::TeamPossessionLatch(base));  // 3a on

  pes::PossessionLatchInput in = base;
  in.newestField1c = 6;  // not in 3..5
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in = base;
  in.newestField1c = 3;  // the bottom of 3..5 is on
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in = base;
  in.newestFlag2d = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in = base;
  in.ballSumRaw = 162;  // 3 * gs: not below it
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.ballSumRaw = 161;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, AnInvalidThreeAActorIsSideZero) {
  pes::PossessionLatchInput in = ThreeB(0);
  in.old = false;
  in.newestFlag2d = true;
  in.newestField1c = 4;
  in.actorValid = false;  // PES parses an invalid actor as side 0
  in.actorSide = 1;       // a stale side must not be used: the actor is invalid
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.team = 1;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, ThreeBNeedsTheChaseOffAndAValidHolder) {
  pes::PossessionLatchInput in = ThreeB(0);
  in.old = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.chaseBlocked = true;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.chaseBlocked = false;
  in.holderValid = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, ThreeBNeedsTheHolderTeamLatchOrAClearOtherHeadFlag) {
  pes::PossessionLatchInput in = ThreeB(0);
  in.old = false;
  in.holderTeamLatch = false;
  in.otherHeadFlagClear = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
  in.otherHeadFlagClear = true;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.holderTeamLatch = true;
  in.otherHeadFlagClear = false;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
}

TEST(PesPossessionLatch, WithNoDecisionTheValueIsKept) {
  pes::PossessionLatchInput in = Base();  // age 100: off the cadence, no decision
  in.team = 0;
  in.old = true;
  EXPECT_TRUE(pes::TeamPossessionLatch(in));
  in.old = false;
  EXPECT_FALSE(pes::TeamPossessionLatch(in));
}

// The install's game speed is the frame rate every window above is measured in.
TEST(PesPossessionInstall, TheGameSpeedIsTheLatchsFrameRate) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto clock = pes::LoadPesClockConstants(*exe, &err);
  ASSERT_TRUE(clock.has_value()) << err;
  const int gs = static_cast<int>(clock->gameSpeed);
  EXPECT_FLOAT_EQ(clock->gameSpeed, 54.0f);
  EXPECT_EQ(pes::PesFrameOf(1000, gs), gs);
  EXPECT_EQ(gs / 6, 9);  // the step-2 window
}
