// PES 2021's per-tick team record words (Slice A): the attack level T+0xB8A4,
// its flags T+0xB8A2/B8A3, and the restart word T+0x80 (team-layer §1 steps
// 13/18/28, §2 V1; tactics §3). Pure behaviour tests: no install needed.
#include <gtest/gtest.h>

#include "onthepitch/pesteamtick.hpp"
#include "onthepitch/pesrunslots.hpp"

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

// The period-end restart kind (plan-selector §4): PES raises restart 7 (thunk 0x143F559F0 behind
// gate 0x14050C3E0) and holds it until the next award overwrites it. The next kick-off reads a
// fresh 1, so 7 changes no branch (the rows rule only special-cases 1, the selector only tests 2).
TEST(PesTeamTick, PeriodEndRestartKindIsSeven) {
  EXPECT_EQ(pes::RestartKindOf(e_SetPiece_PeriodEnd), 7);
}

// The mode-word copy (plan-selector §5: 0x1404F6DBD MOVs +0xBCC to T+0x134 between the decider and
// the drift, so the drift reads the previous tick's word). The copy carries the whole mode word,
// including the keep sentinel below zero.
TEST(PesTeamTick, ModeWordCopyCarriesBccTo134) {
  pes::TeamTick tick;
  tick.modeBcc = 3;
  pes::CopyModeWord(&tick);
  EXPECT_EQ(tick.word134, 3);
  tick.modeBcc = -1;
  pes::CopyModeWord(&tick);
  EXPECT_EQ(tick.word134, -1);
}

// The stoppage plan-2 rule (plan-selector §1: 0x140500FA9-DB7): the sheet's current plan stands
// when the setup word is zero or it is already 2; otherwise the selector writes plan 2.
TEST(PesTeamTick, StoppagePlanTwoRule) {
  EXPECT_EQ(pes::StoppagePlan2(/*current=*/0, /*setupNonZero=*/false), 0);
  EXPECT_EQ(pes::StoppagePlan2(/*current=*/2, /*setupNonZero=*/true), 2);
  EXPECT_EQ(pes::StoppagePlan2(/*current=*/0, /*setupNonZero=*/true), 2);
  EXPECT_EQ(pes::StoppagePlan2(/*current=*/1, /*setupNonZero=*/true), 2);
}

// S2 (audit-2): the selector's mode word (+0xBCC, 0x140501076) is −1 (keep) until the decider
// first runs; PES's < 0 arm (TEST EAX,EAX; JS at 0x14050107C) then keeps the default plan.
TEST(PesTeamTick, SelectorModeHoldsKeepUntilTheDeciderRuns) {
  EXPECT_EQ(pes::SelectorMode(false, 3), -1);
  EXPECT_EQ(pes::SelectorMode(true, 3), 3);
  EXPECT_EQ(pes::SelectorMode(true, -1), -1);
}

TEST(PesTeamTick, DeciderHasNotRunOnAFreshTick) { EXPECT_FALSE(pes::TeamTick().deciderRan); }

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
  c.headPointX = 10.0f;
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

// T9 (FUN_14057F600 at 0x14057F600): the release/claim dispatcher. MI+0x116C
// == 2 exits immediately (0x14057F61B), whatever the arms say.
TEST(PesTeamTick, ReleaseDispatchExitsWhenPlayState2) {
  pes::run::ReleaseDispatchInput in;
  in.playState = 2;
  in.stoppageClaim = true;
  in.latchSet = true;
  EXPECT_EQ(pes::run::ReleaseDispatch(in), pes::run::ReleaseRoute::kExit);
}

// The latch fork (0x14057F6CA): latch set routes to the set loop; latch clear
// needs MI+0x116C == 1 for the clear loop, else the tick exits.
TEST(PesTeamTick, ReleaseDispatchForksOnLatch) {
  pes::run::ReleaseDispatchInput in;
  in.playState = pes::kPlayStateInPlay;
  in.stoppageClaim = true;
  in.latchSet = true;
  EXPECT_EQ(pes::run::ReleaseDispatch(in), pes::run::ReleaseRoute::kLatchSetLoop);
  in.latchSet = false;
  EXPECT_EQ(pes::run::ReleaseDispatch(in), pes::run::ReleaseRoute::kLatchClearLoop);
  in.playState = 3;
  EXPECT_EQ(pes::run::ReleaseDispatch(in), pes::run::ReleaseRoute::kExit);
  in.playState = pes::kPlayStateInPlay;
  in.stoppageClaim = false;
  in.latchSet = true;
  EXPECT_EQ(pes::run::ReleaseDispatch(in), pes::run::ReleaseRoute::kExit);
}
// T3a: step 19 (FUN_140500200). Level >= 4 takes every slot (0x140500240 BTS loop, 0x1405008BD) whatever the
// list holds.
TEST(PesTeamTick, LevelFourMasksEverySlot) {
  const pes::Step19ListInput empty{};
  EXPECT_EQ(pes::Step19Mask(4, /*classGate=*/true, empty), (1u << pes::kReactionSlots) - 1u);
}

// The count arm (0x1405003ED-F1): fewer men ahead than the licence and the whole side takes the mask; from
// the licence on, the list decides.
TEST(PesTeamTick, FewerAheadThanLicenceTakesEverySlot) {
  pes::Step19ListInput in;
  in.count = 1;  // level 1: licence 2
  EXPECT_EQ(pes::Step19Mask(1, /*classGate=*/false, in), (1u << pes::kReactionSlots) - 1u);
  in.count = 2;
  EXPECT_EQ(pes::Step19Mask(1, /*classGate=*/false, in), 0u);  // the list is consulted: no man on the pitch
}

// Step 19's licence map (0x1405002F2-327): 0->0, 1->2, 2->4, 3->6, else 5.
TEST(PesTeamTick, LevelLicenceMap) {
  EXPECT_EQ(pes::Step19Licence(0), 0);
  EXPECT_EQ(pes::Step19Licence(1), 2);
  EXPECT_EQ(pes::Step19Licence(2), 4);
  EXPECT_EQ(pes::Step19Licence(3), 6);
  EXPECT_EQ(pes::Step19Licence(9), 5);
  EXPECT_EQ(pes::Step19Licence(-1), 5);
}

// The class branch (0x14050024F-2C5): with 0 < level and the 5AAF00 gate, slots of class > 1 take their bit and
// the list is skipped. Level 0 and a false gate fall through to the list.
TEST(PesTeamTick, Level1to3GatesOnClass) {
  // Slot 0 is class 2 (> 1), slot 1 class 1 (not > 1). Every slot is on the pitch and the count clears.
  const std::array<int, pes::kReactionSlots> classes{2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  pes::Step19ListInput in;
  in.count = 9;
  for (int k = 0; k < pes::kReactionSlots; ++k)
    in.slots[static_cast<size_t>(k)] = {k, classes[static_cast<size_t>(k)], 0.0f, 0.0f, false, true};
  EXPECT_EQ(pes::Step19Mask(2, /*classGate=*/true, in), 1u << 0);
  // Without the gate the list decides: every key ties at -5 (d = 0), so the first four slots take the mask.
  EXPECT_EQ(pes::Step19Mask(2, /*classGate=*/false, in), 0xFu);
  EXPECT_EQ(pes::Step19Mask(0, /*classGate=*/true, in), 0u);  // level 0: licence 0, no class branch
}

// The list branch (0x140500544-88E): on-pitch slots rank by tableFloat - ramp(d)*5 (+ -3 on the holder match),
// ascending and stable; the first `licence` take the bit. Slot 0 (d = 10, ramp 1.0, key 0) ranks ahead of
// slot 1 (d = 30, ramp 0.0, key 5); a descending sort would take slot 1.
TEST(PesTeamTick, ListRanksNearerManFirst) {
  pes::Step19ListInput in;
  in.licence = 1;
  in.count = 1;
  in.slots[0] = {0, 4, 0.0f, 0.0f, false, true};
  in.slots[1] = {1, 4, 40.0f, 0.0f, false, true};
  in.ballX = 10.0f;
  in.ballZ = 0.0f;
  in.tableFloat = {5.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  EXPECT_EQ(pes::Step19ListPick(in), 1u << 0);
}

// A forward (folded class >= 7) takes his bit at once (BTS at 0x140500663-6A6) and ranks -100 with the rest.
TEST(PesTeamTick, ForwardsTakeTheirBitAtOnce) {
  pes::Step19ListInput in;
  in.licence = 1;
  in.count = 1;
  in.slots[0] = {0, 7, 20.0f, 0.0f, false, true};  // forward
  in.slots[1] = {1, 7, 30.0f, 0.0f, false, true};  // forward
  in.slots[2] = {2, 4, 10.0f, 0.0f, false, true};  // nearer the ball, not a forward
  in.ballX = 10.0f;
  in.ballZ = 0.0f;
  in.tableFloat = {5.0f, 5.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  // Both forwards keep their bits past the licence of 1; the non-forward ranks last at key 0.
  EXPECT_EQ(pes::Step19ListPick(in), (1u << 0) | (1u << 1));
}

// An off-pitch man is not listed (0x140500621), however near the ball he stands.
TEST(PesTeamTick, OffPitchSlotsAreNotListed) {
  pes::Step19ListInput in;
  in.licence = 1;
  in.count = 1;
  in.slots[0] = {0, 4, 10.0f, 0.0f, false, false};  // off the pitch, on the ball
  in.slots[1] = {1, 4, 30.0f, 0.0f, false, true};
  in.ballX = 10.0f;
  in.ballZ = 0.0f;
  in.tableFloat = {5.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  EXPECT_EQ(pes::Step19ListPick(in), 1u << 1);
}

// T3b slice 1: step 20's gates (FUN_1404FF8B0, 0x1404FF8D9/8EF/946). The tick
// needs the latch, a live holder (carrier else T+0x78) and open play; else
// it returns with the tables untouched (0x140500134).
TEST(PesTeamTick, NoTargetRecordedWithoutBall) {
  pes::Step20Gates shut{};
  shut.latchSet = false;
  shut.carrier = 5;
  shut.playState = pes::kPlayStateInPlay;
  EXPECT_FALSE(pes::Step20GatesPass(shut));
  pes::Step20Gates deadBall{true, 5, 0xFF, 2};
  EXPECT_FALSE(pes::Step20GatesPass(deadBall));
  pes::Step20Gates noHolder{true, 0xFF, 0xFF, pes::kPlayStateInPlay};
  EXPECT_FALSE(pes::Step20GatesPass(noHolder));
  pes::Step20Gates live{true, 5, 0xFF, pes::kPlayStateInPlay};
  EXPECT_TRUE(pes::Step20GatesPass(live));
  pes::Step20Gates kickFallback{true, 0xFF, 7, pes::kPlayStateInPlay};
  EXPECT_TRUE(pes::Step20GatesPass(kickFallback));
}

// Step 20's band write (0x14050017F-1BE): below 0.5 -> 1.0, above 4.0 ->
// 0.0, else (x-4.0)/-3.5 (0x14259D658 at 0x1405001A4); the store at
// 0x1405001BE runs on the hit path only.
TEST(PesTeamTick, ScoreBandClamps) {
  EXPECT_FLOAT_EQ(pes::Step20Band(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(pes::Step20Band(5.0f), 0.0f);
  EXPECT_FLOAT_EQ(pes::Step20Band(2.25f), (2.25f - 4.0f) / -3.5f);
}

// T3InteriorRE §4: NaN's comparisons both fail, but 0x140500187's COMISS XMM13,0.5 + JC arm takes
// NaN to 1.0 (JC = CF = unordered).
TEST(PesTeamTick, NaNGivesOne) {
  EXPECT_FLOAT_EQ(pes::Step20Band(std::numeric_limits<float>::quiet_NaN()), 1.0f);
}

// The receiver tables' reset values (port spec §8): flags 0, ids 0xFF
// (0x1405001E7), scores 0.0.
TEST(PesTeamTick, OffPitchSlotMisses) {
  const pes::TeamTick tick;
  for (int k = 0; k < pes::kReactionSlots; ++k) {
    EXPECT_EQ(tick.slotFlag[static_cast<size_t>(k)], 0u);
    // Literal 0xFF: the miss-store immediate at 0x1405001E7, so the test
    // catches a wrong constant value rather than echoing it.
    EXPECT_EQ(tick.slotTarget[static_cast<size_t>(k)], 0xFFu);
    EXPECT_FLOAT_EQ(tick.slotScore[static_cast<size_t>(k)], 0.0f);
  }
}

// Step 20's outer loop (T3InteriorRE §1): the first blocking opponent names the slot's target
// ((0, opp), 0x1404FFE38/0x140500140), an out-of-range slot takes (0, 0xFF) (0x140500149), an open
// slot takes (1, 0xFF) on exhaust (0x140500015), and the holder's slot keeps its old values
// (0x140500174 skips the store).
TEST(PesTeamTick, BlockedSlotNamesOpponent) {
  pes::TeamTick tick;
  pes::Step20Input in;
  in.side = 0;
  in.holder = 3;
  for (int k = 0; k < pes::kReactionSlots; ++k) in.slots[static_cast<size_t>(k)].onPitch = true;
  in.slots[0].blocks = 14;
  in.slots[2].inRange = false;
  tick.slotFlag[3] = 9;
  tick.slotTarget[3] = 9;
  pes::Step20Table(&tick, in);
  EXPECT_EQ(tick.slotFlag[0], 0u);
  EXPECT_EQ(tick.slotTarget[0], 14u);
  EXPECT_EQ(tick.slotFlag[1], 1u);
  EXPECT_EQ(tick.slotTarget[1], 0xFFu);
  EXPECT_EQ(tick.slotFlag[2], 0u);
  EXPECT_EQ(tick.slotTarget[2], 0xFFu);
  EXPECT_EQ(tick.slotFlag[3], 9u);
  EXPECT_EQ(tick.slotTarget[3], 9u);
}

// An off-pitch slot misses with (0, 0xFF) (0x1404FFA55), and side 1 reads the other half: its
// holder is his own slot (1·11 + 5), not slot 5.
TEST(PesTeamTick, OpenSlotScoresFF) {
  pes::TeamTick tick;
  pes::Step20Input in;
  in.side = 1;
  in.holder = 16;
  for (int k = 0; k < pes::kReactionSlots; ++k) in.slots[static_cast<size_t>(k)].onPitch = true;
  in.slots[4].onPitch = false;
  tick.slotFlag[5] = 7;
  tick.slotTarget[5] = 7;
  pes::Step20Table(&tick, in);
  EXPECT_EQ(tick.slotFlag[4], 0u);
  EXPECT_EQ(tick.slotTarget[4], 0xFFu);
  EXPECT_EQ(tick.slotFlag[0], 1u);
  EXPECT_EQ(tick.slotTarget[0], 0xFFu);
  EXPECT_EQ(tick.slotFlag[5], 7u);
  EXPECT_EQ(tick.slotTarget[5], 7u);
}

// T3c slice 1: step 21's gates (FUN_1404F7BA0, 0x1404F7BB3-D15). The pick
// needs the no-possession arm (T+0x255 == 0), the roster, the level words
// and the clear side flag, plus a live holder; any fail writes 0xFF
// (0x1404F7BE3) — only the holder-missing tail leaves B8D0 untouched.
TEST(PesTeamTick, RestartReceiverOnlyWithoutPossession) {
  pes::Step21Gates latched{false, true, true, true, 5, 0xFF};
  EXPECT_FALSE(pes::Step21GatesPass(latched));
  pes::Step21Gates open{true, true, true, true, 5, 0xFF};
  EXPECT_TRUE(pes::Step21GatesPass(open));
  // Each gate arm fails the tick on its own (0x1404F7BCC-D15).
  pes::Step21Gates noRoster{true, false, true, true, 5, 0xFF};
  EXPECT_FALSE(pes::Step21GatesPass(noRoster));
  pes::Step21Gates lowLevel{true, true, false, true, 5, 0xFF};
  EXPECT_FALSE(pes::Step21GatesPass(lowLevel));
  pes::Step21Gates sideSet{true, true, true, false, 5, 0xFF};
  EXPECT_FALSE(pes::Step21GatesPass(sideSet));
  pes::Step21Gates noHolder{true, true, true, true, 0xFF, 0xFF};
  EXPECT_FALSE(pes::Step21GatesPass(noHolder));
  pes::Step21Gates kickFallback{true, true, true, true, 0xFF, 7};
  EXPECT_TRUE(pes::Step21GatesPass(kickFallback));
}

// Step 21's tight/loose cones (0x1404F81C9-E4/E9-21E): tight (level <= 1)
// skips past 60.0 (0x14259A764); loose adds 0 above score 30 (XMM15 floor),
// 50 at/above 160, else the ((angle-30)/130)*50 ramp.
TEST(PesTeamTick, TightConeKeepsSixty) {
  EXPECT_FLOAT_EQ(pes::Step21LooseCone(40.0f, 170.0f), 0.0f);
  EXPECT_FLOAT_EQ(pes::Step21LooseCone(0.0f, 170.0f), 50.0f);
  EXPECT_FLOAT_EQ(pes::Step21LooseCone(0.0f, 160.0f), 50.0f);
  EXPECT_FLOAT_EQ(pes::Step21LooseCone(0.0f, 95.0f), ((95.0f - 30.0f) / 130.0f) * 50.0f);
}

// Step 21's score arms (0x1404F81A1/1B8/1D0/26C): incumbent -10.0, x0.6
// (0x14257A190, §7 correction vs 0.7), +d, support +20.0.
TEST(PesTeamTick, LooseConeAddsFifty) {
  EXPECT_FLOAT_EQ(pes::Step21Score(10.0f, 5.0f, false, 0.0f), 10.0f * 0.6f + 5.0f);
  EXPECT_FLOAT_EQ(pes::Step21Score(10.0f, 5.0f, true, 0.0f), (10.0f - 10.0f) * 0.6f + 5.0f);
  EXPECT_FLOAT_EQ(pes::Step21Score(10.0f, 5.0f, false, 20.0f), 10.0f * 0.6f + 5.0f + 20.0f);
}

// Step 21's stamp (0x1404F82FC): brain+0x6D40 := MI+0x1198 only when the
// pick is a slot (best <= 0x15); 0xFF writes no stamp.
TEST(PesTeamTick, StampWrittenOnlyOnPick) {
  const pes::TeamTick tick;
  EXPECT_EQ(tick.bestSlot, pes::kStep21None);
  EXPECT_EQ(tick.bestStamp, -1);
  const std::array<float, pes::kReactionSlots> scores{1.0f, 3.0f, 2.0f, 0.0f, 0.0f, 0.0f,
                                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  EXPECT_EQ(pes::Step21Best(scores), 1);
  // Ties keep the earlier slot (strict-greater take-over, 0x1404F8284 JBE
  // skips equals); a >= take-over would hand the tie to the later slot.
  const std::array<float, pes::kReactionSlots> tied{2.0f, 3.0f, 3.0f, 0.0f, 0.0f, 0.0f,
                                                     0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  EXPECT_EQ(pes::Step21Best(tied), 1);
}

// T3InteriorRE §4: the empty scan writes 0xFF (R15D init at 0x1404F7EBA; no other writer), not the
// 0x15 stamp-gate bound of T3 §6's misread.
TEST(PesTeamTick, EmptyScanKeepsFF) {
  const std::array<float, pes::kReactionSlots> below{-2000.0f, -2000.0f, -2000.0f, -2000.0f,
                                                      -2000.0f, -2000.0f, -2000.0f, -2000.0f,
                                                      -2000.0f, -2000.0f, -2000.0f};
  EXPECT_EQ(pes::Step21Best(below), 0xFF);
}
}  // namespace
