// PES 2021's per-tick team record words (Slice A): the attack level T+0xB8A4,
// its flags T+0xB8A2/B8A3, and the restart word T+0x80 (team-layer §1 steps
// 13/18/28, §2 V1; tactics §3). Pure behaviour tests: no install needed.
#include <gtest/gtest.h>

#include "onthepitch/pesteamtick.hpp"

namespace {

// A1: the inline rule at 0x1404F6E1C-58: B8A2 = (L >= 4), B8A3 = (L <= 0).
TEST(PesTeamTick, Level3SetsFullAttack) {
  pes::TeamTick tick;
  tick.b8a4 = 4;
  pes::WriteAttackFlags(&tick);
  EXPECT_TRUE(tick.b8a2);
  EXPECT_FALSE(tick.b8a3);
  tick.b8a4 = 3;
  pes::WriteAttackFlags(&tick);
  EXPECT_FALSE(tick.b8a2);
  EXPECT_FALSE(tick.b8a3);
}

TEST(PesTeamTick, Level0SetsFullDefence) {
  pes::TeamTick tick;
  tick.b8a4 = 0;
  pes::WriteAttackFlags(&tick);
  EXPECT_FALSE(tick.b8a2);
  EXPECT_TRUE(tick.b8a3);
  tick.b8a4 = 1;
  pes::WriteAttackFlags(&tick);
  EXPECT_FALSE(tick.b8a2);
  EXPECT_FALSE(tick.b8a3);
}

// A2: step 28 at 0x1404F6F91: T+0x80 = 0xFF when MI+0x116C != 1 (dead ball)
// or the latch flipped (FUN_1404FE9C0 returns old^new 0x255).
TEST(PesTeamTick, RestartWordSetWhileDeadBall) {
  pes::TeamTick tick;
  pes::WriteRestartWord(&tick, /*inPlay=*/false, /*possessionFlipped=*/false);
  EXPECT_EQ(tick.t80, 0xFFu);
}

TEST(PesTeamTick, RestartWordSetOnPossessionFlip) {
  pes::TeamTick tick;
  pes::WriteRestartWord(&tick, /*inPlay=*/true, /*possessionFlipped=*/true);
  EXPECT_EQ(tick.t80, 0xFFu);
}

TEST(PesTeamTick, RestartWordKeptInPlay) {
  pes::TeamTick tick;
  pes::WriteRestartWord(&tick, /*inPlay=*/true, /*possessionFlipped=*/false);
  EXPECT_EQ(tick.t80, 0u);
}

// A3: the drift gates of FUN_1404F8350 (ladder 0x1404F8619-63E, mesh
// 0x1404F8640-6C8, freeze 0x1404F86C8-6D4, clamp 0x1404F86DE-E7).
TEST(PesTeamTick, LevelFrozenWhileDeadBall) {
  pes::TeamTick tick;
  tick.b8a4 = 2;
  // Strategy 3 (attacking) wants 3, but the ball is dead: the level holds.
  pes::DriftAttackLevel(&tick, /*strategy=*/3, /*manual=*/2, /*inPlay=*/false,
                        /*frame=*/100);
  EXPECT_EQ(tick.b8a4, 2);
  // Same tick in play: it follows.
  pes::DriftAttackLevel(&tick, /*strategy=*/3, /*manual=*/2, /*inPlay=*/true,
                        /*frame=*/101);
  EXPECT_EQ(tick.b8a4, 3);
  EXPECT_EQ(tick.b8c0Frame, 101);
}

TEST(PesTeamTick, StrategyMapCoversTheRest) {
  pes::TeamTick tick;
  // Anything outside 0/1/3/4 wants 2 (the ladder default); 4 stays 4.
  EXPECT_EQ(pes::StrategyWantLevel(2), 2);
  EXPECT_EQ(pes::StrategyWantLevel(7), 2);
  EXPECT_EQ(pes::StrategyWantLevel(-1), 2);
  EXPECT_EQ(pes::StrategyWantLevel(4), 4);
  EXPECT_EQ(pes::StrategyWantLevel(0), 0);
  tick.b8a4 = 3;
  pes::DriftAttackLevel(&tick, /*strategy=*/7, /*manual=*/2, /*inPlay=*/true,
                        /*frame=*/50);
  EXPECT_EQ(tick.b8a4, 2);
}
TEST(PesTeamTick, ManualOverridePullsLevel) {
  // T+0x210 = 1 below a want of 2 (t < 2, t <= want < 3): want becomes 1.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/2, /*manual=*/1, /*level=*/2), 1);
  // T+0x210 = 3 above a want of 2 (t > 2, 2 <= want <= t): want becomes 3.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/2, /*manual=*/3, /*level=*/2), 3);
  // Balanced manual never overrides.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/3, /*manual=*/2, /*level=*/2), 3);
  // Out of range either way: no override.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/3, /*manual=*/1, /*level=*/2), 3);
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/1, /*manual=*/3, /*level=*/2), 1);
  // In range but already there (0x1404F8663 CMP ESI,EBP; JZ): the strategy
  // want stands, the manual word does not hold the level.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/2, /*manual=*/1, /*level=*/1), 2);
  // End to end: the override moves the stored level in play.
  pes::TeamTick tick;
  tick.b8a4 = 2;
  pes::DriftAttackLevel(&tick, /*strategy=*/2, /*manual=*/3, /*inPlay=*/true,
                        /*frame=*/60);
  EXPECT_EQ(tick.b8a4, 3);
}

// T2a: step 8 (FUN_1405008E0) sets brain+0x6C40 = 2 only when the latch (T+0x255) is clear, play
// is stopped (MI+0x116C != 1) and the restart is a free kick (MI+0x1554 == 5). Its roster scan has
// no effect: FUN_140A66FF0 is a pure getter and its result is discarded.
TEST(PesTeamTick, StoppedSelectSets6C40OnlyWhenLatchClear) {
  pes::TeamTick tick;
  pes::StoppedSelect(&tick, /*latchClear=*/false, /*playState=*/2, pes::kRestartFreeKick);
  EXPECT_EQ(tick.b6c40, 0);
  pes::StoppedSelect(&tick, /*latchClear=*/true, /*playState=*/2, pes::kRestartFreeKick);
  EXPECT_EQ(tick.b6c40, 2);
  // A set-piece-ready stop (MI+0x116C == 3) passes the gate too.
  tick.b6c40 = 0;
  pes::StoppedSelect(&tick, /*latchClear=*/true, /*playState=*/3, pes::kRestartFreeKick);
  EXPECT_EQ(tick.b6c40, 2);
}

TEST(PesTeamTick, StoppedSelectNeedsAStoppedFreeKick) {
  // Every restart kind but the free kick (5) leaves the word alone.
  for (int kind : {0, 1, 2, 3, 4, 6, 7}) {
    pes::TeamTick tick;
    pes::StoppedSelect(&tick, /*latchClear=*/true, /*playState=*/2, kind);
    EXPECT_EQ(tick.b6c40, 0) << "restart kind " << kind;
  }
  // In play (MI+0x116C == 1) the gate is shut even for a free kick.
  pes::TeamTick inPlay;
  pes::StoppedSelect(&inPlay, /*latchClear=*/true, /*playState=*/1, pes::kRestartFreeKick);
  EXPECT_EQ(inPlay.b6c40, 0);
}

// The restart kinds (MI+0x1554) by referee set piece: pes-re-A5Judge.md's JR table.
TEST(PesTeamTick, RestartKindIsThePesStoppageCode) {
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_None), 0);
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_KickOff), 1);
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_ThrowIn), 2);
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_GoalKick), 3);
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_Corner), 4);
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_FreeKick), 5);
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_Penalty), 6);
}

// T2b: step 9's out-of-play branch (0x14050247E-0x140502740) zeroes both reaction-delay banks,
// T+0x6A4[11] and T+0xB860[11], on every call while MI+0x116C != 1. The in-play half of step 9 is
// OPEN, so this test checks the return of the call in play and not the banks it leaves there.
TEST(PesTeamTick, ReactionDelaysClearWhenPlayIsStopped) {
  pes::TeamTick tick;
  for (int i = 0; i < 11; ++i) {
    tick.reactionDelay[i] = 7 + i;
    tick.reactionDelayMirror[i] = 20 + i;
  }
  EXPECT_FALSE(pes::ClearReactionDelays(&tick, /*playState=*/1)) << "in play the clear does not run";
  EXPECT_TRUE(pes::ClearReactionDelays(&tick, /*playState=*/2));
  for (int i = 0; i < 11; ++i) {
    EXPECT_EQ(tick.reactionDelay[i], 0) << "bank T+0x6A4, slot " << i;
    EXPECT_EQ(tick.reactionDelayMirror[i], 0) << "bank T+0xB860, slot " << i;
  }
  // A set piece awaiting its take (MI+0x116C == 3) is stopped too.
  tick.reactionDelay[0] = 5;
  EXPECT_TRUE(pes::ClearReactionDelays(&tick, /*playState=*/3));
  EXPECT_EQ(tick.reactionDelay[0], 0);
}

// T2c: step 11's head (FUN_1404FEDC0, 0x1404FEDC0-0x1404FEF9B). With the latch set the bank is the
// base weight times +1.0; with the latch clear it is the base weight times [0x142588218] = -1.0.
// The fill of the base weights to 1.0 (0x1404FF760) runs after the bank, in the same call.
TEST(PesTeamTick, LatchClearNegatesWeights) {
  pes::TeamTick tick;
  for (int i = 0; i < 11; ++i) tick.supportWeight[i] = 0.25f * (i + 1);
  pes::SupportWeights(&tick, /*latchSet=*/true, /*flipped=*/true, /*playState=*/1, /*scoreFlag=*/false);
  for (int i = 0; i < 11; ++i) EXPECT_FLOAT_EQ(tick.supportBank[i], 0.25f * (i + 1)) << "slot " << i;
  for (int i = 0; i < 11; ++i) tick.supportWeight[i] = 0.25f * (i + 1);
  pes::SupportWeights(&tick, /*latchSet=*/false, /*flipped=*/true, /*playState=*/1, /*scoreFlag=*/false);
  for (int i = 0; i < 11; ++i) EXPECT_FLOAT_EQ(tick.supportBank[i], -0.25f * (i + 1)) << "slot " << i;
}

// Out of play (MI+0x116C != 1) the bank is all 1.0 whatever the latch, and the stamp is -1
// (0x1404FEF56-0x1404FEF89 and 0x1404FEF91-0x1404FEF9B).
TEST(PesTeamTick, StoppedPlayFillsOnes) {
  pes::TeamTick tick;
  tick.supportStamp = 5;
  for (int i = 0; i < 11; ++i) tick.supportBank[i] = 7.0f;
  pes::SupportWeights(&tick, /*latchSet=*/false, /*flipped=*/true, /*playState=*/2, /*scoreFlag=*/false);
  for (int i = 0; i < 11; ++i) EXPECT_FLOAT_EQ(tick.supportBank[i], 1.0f) << "slot " << i;
  EXPECT_EQ(tick.supportStamp, -1);
  // Without a flip the stamp is still -1 out of play (0x1404FEF91-0x1404FEF9B runs on every call).
  tick.supportStamp = 5;
  pes::SupportWeights(&tick, /*latchSet=*/false, /*flipped=*/false, /*playState=*/2, /*scoreFlag=*/false);
  EXPECT_EQ(tick.supportStamp, -1);
}

// Without a latch flip (R8B == 0) the counter T+0x218 counts up and the bank is left alone; a
// flip resets the counter to 0 (0x1404FEF8B INC, 0x1404FEE33 MOV).
TEST(PesTeamTick, SkipFlagIncrements218) {
  pes::TeamTick tick;
  for (int i = 0; i < 11; ++i) tick.supportBank[i] = 7.0f;
  pes::SupportWeights(&tick, /*latchSet=*/true, /*flipped=*/false, /*playState=*/1, /*scoreFlag=*/false);
  pes::SupportWeights(&tick, /*latchSet=*/true, /*flipped=*/false, /*playState=*/1, /*scoreFlag=*/false);
  EXPECT_EQ(tick.noBallCounter, 2);
  EXPECT_FLOAT_EQ(tick.supportBank[0], 7.0f) << "no flip leaves the bank";
  pes::SupportWeights(&tick, /*latchSet=*/true, /*flipped=*/true, /*playState=*/1, /*scoreFlag=*/false);
  EXPECT_EQ(tick.noBallCounter, 0);
}

// The score flag DL == 0 (0x1404FEFD7 TEST DL, 0x1404FF760) sets every base weight to 1.0 after
// the bank has been taken from the old weights.
TEST(PesTeamTick, ZeroScoreFlagFillsOnes) {
  pes::TeamTick tick;
  for (int i = 0; i < 11; ++i) tick.supportWeight[i] = 0.3f;
  pes::SupportWeights(&tick, /*latchSet=*/true, /*flipped=*/false, /*playState=*/1, /*scoreFlag=*/false);
  for (int i = 0; i < 11; ++i) EXPECT_FLOAT_EQ(tick.supportWeight[i], 1.0f) << "slot " << i;
}

// T1b: step 15's in-play arm (FUN_1404FA840, 0x1404FA869-0x1404FA887): T+0xB8A8 = 8, T+0xB8AC = 4, brain+0x6D44 = 0.
TEST(PesTeamTick, CountersSeed8And4InPlay) {
  pes::TeamTick tick;
  pes::ArmInPlayCounters(&tick);
  EXPECT_EQ(tick.b8a8, 8);
  EXPECT_EQ(tick.b8ac, 4);
}

TEST(PesTeamTick, InPlayArmClearsDoneFlag) {
  pes::TeamTick tick;
  tick.counterDone = true;
  pes::ArmInPlayCounters(&tick);
  EXPECT_FALSE(tick.counterDone);
}

// T7a (FUN_1404F61E0): the mark table builds each team tick on the PES path.
TEST(PesTeamTick, MarkBuildCommitsMarkedCandidate) {
  pes::marktable::MarkState state;
  state.markSlot = 3;
  pes::marktable::Facts facts;
  facts.side = 0;
  facts.holder = 5;
  facts.sign = 1.0f;
  facts.holderX = 14.6984631f;
  facts.holderZ = 1.7101007f;
  facts.goalAreaHalfWidth_m = 9.16f;
  pes::marktable::Candidate c;
  c.id = 7;
  c.d2 = 100.0f;
  c.actionWord = 1;
  c.posX = 10.0f;
  c.pairToTarget = 10.0f;
  c.pairToHolder = 70.0f;
  facts.candidates[0] = c;
  pes::marktable::Constants k;
  k.rampBase = 7.0f;
  k.rampTop = 3.0f;
  k.coneOffset = 15.0f;
  k.ceilD2 = 1369.0f;
  k.wrapCap = 105.0f;
  k.wrapSide = 80.0f;
  k.coneDeg = 90.0f;
  k.coneLen = 12.0f;
  k.gateFrac = 64.0f;
  k.gateRadius = 12.0f;
  k.gateWidth = 2.0f;
  k.half = 0.5f;
  pes::BuildMarkTableTick(&state, facts, k);
  EXPECT_EQ(state.mark[7], 1);
}

}  // namespace
