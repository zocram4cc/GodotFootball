// PES 2021's match clock: the period record (MI+0x1168..0x1194), its decode, the
// stoppage rule and the time-up gate (docs/re/pes-re-clock-live.md 8). Constants
// come from the user's own PES2021.exe at runtime; nothing is transcribed.

#include <gtest/gtest.h>

#include <cstdlib>
#include <stdexcept>

#include "onthepitch/pesclock.hpp"
#include <optional>
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace {

// The added-time formula's inputs (docs/re/pes-re-tactics-clock.md 7.2), so the
// arithmetic is exercised without an install.
pes::PesClockConstants FakeConstants() {
  pes::PesClockConstants c;
  c.gameSpeed = 54.0f;             // .data 0x14351ae54
  c.addedTier_3_6_2nd = 7200.0f;   // .rdata 0x142632a68
  c.addedTier_1_2_2nd = 3600.0f;   // .rdata 0x142598590
  c.addedTier_1_2_ET = 1800.0f;    // .rdata 0x142632a60
  c.addedTier_3_6_ET = 3600.0f;    // .rdata 0x142598590 (the same float)
  c.limitCap = 21600.0f;           // .rdata 0x142647c60
  return c;
}

// The install's constants, or a skip: the gate's thresholds are PES's own.
std::optional<pes::PesClockConstants> InstallConstants() {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return std::nullopt;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) return std::nullopt;
  return pes::LoadPesClockConstants(*exe, &err);
}

// A live capture of PES2021 (read-only, OBS): period 1, playing a stoppage
// (state 2), F 134879.0. The ring slot ring[idx & 3] decodes under the record's
// key and checksum, and its stamp is the f32 that the decode gives.
pes::PeriodRecord LivePeriodRecord() {
  pes::PeriodRecord r;
  r.period = 1;
  r.playState = 2;
  r.limit = 21600.0f;
  r.f = 134879.0f;
  r.checksum = 0xd4ecb319911b634dULL;
  r.key = 2280871786u;
  r.ringIdx = 3104894789u;
  r.ring[0] = 0xcca377bcu;
  r.ring[1] = 0xcca37778u;
  r.ring[2] = 0xcca37764u;
  r.ring[3] = 0xcca37794u;
  return r;
}


constexpr int kJudgeRate = 54;  // FUN_141e532e0: logic frames per game-second

// The judge's input on a tick with nothing to see: the ball at the attacking end (outside
// the zone) and no touch on record.
pes::TimeUpInput QuietJudge() {
  pes::TimeUpInput in;
  in.halfLength_m = 52.56f;
  in.ballX_m = 52.0f;
  return in;
}

// The tick on which the judge blows, counting from 1 across `limit` ticks, or 0 if it does not.
int FirstBlowTick(pes::TimeUpCounters& c, const pes::TimeUpInput& in, int limit) {
  for (int tick = 1; tick <= limit; ++tick)
    if (pes::TimeUpWhistle(c, in, kJudgeRate)) return tick;
  return 0;
}
}  // namespace

// --- the period record (MI+0x1168..0x1194, FUN_140A3C070 / FUN_140A3C050) ---

TEST(PesPeriodRecord, DecodesTheLivePeriodClockAsPesDoes) {
  const pes::PeriodRecord r = LivePeriodRecord();
  EXPECT_FLOAT_EQ(pes::PeriodStamp(r), 7177210.0f);
  EXPECT_FLOAT_EQ(pes::PeriodClockUnits(r), 7177210.0f + 134879.0f);
}

TEST(PesPeriodRecord, ASlotChangedUnderItsChecksumIsRefused) {
  pes::PeriodRecord r = LivePeriodRecord();
  r.ring[1] ^= 1u;  // the slot the decode reads, one bit off
  EXPECT_THROW(pes::PeriodStamp(r), std::runtime_error);
}

TEST(PesPeriodRecord, StoredStampReadsBackAndTheRingAdvances) {
  pes::PeriodRecord r;
  r.key = 0x0badf00du;
  pes::StoreStamp(r, 1234.5f);
  EXPECT_FLOAT_EQ(pes::PeriodStamp(r), 1234.5f);
  const uint32_t firstSlot = r.ringIdx & 3u;
  pes::StoreStamp(r, 99.0f);
  EXPECT_FLOAT_EQ(pes::PeriodStamp(r), 99.0f);
  EXPECT_NE(r.ringIdx & 3u, firstSlot) << "each store moves to the next slot";
}

// --- the clock: play and stoppage (docs/re/pes-re-clock-live.md 8, live OBS) ---

TEST(PesClock, InPlayTheStampAdvancesAtTheUnitRate) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set: the unit rate is read from PES2021.exe";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.Advance(10.0f);
  EXPECT_FLOAT_EQ(clock.PeriodClock(), 10.0f * c->secondsUnit);
}

// A stoppage freezes the stamp and adds half a unit per logic frame to F; live
// F rose 19.5 over 39 frames in state 2 (OBS).
TEST(PesClock, AStoppageHoldsTheStampAndAddsHalfAUnitPerFrame) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.Advance(100.0f);
  const float stamp = pes::PeriodStamp(clock.Record());
  clock.SetFrozen(true);
  clock.Advance(1.0f);
  EXPECT_FLOAT_EQ(pes::PeriodStamp(clock.Record()), stamp);
  EXPECT_FLOAT_EQ(clock.Record().f, pes::kStoppageUnitsPerFrame * c->gameSpeed);
}

// Stoppage time counts towards the time-up through F: 2699 s of play is 161940
// units, and F must add 60 units (about 2.2 s of stoppage at 27 units a second)
// before the gate's 162000 opens.
TEST(PesClock, StoppageTimeCountsTowardsTheTimeUp) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.Advance(2699.0f);
  clock.SetFrozen(true);
  clock.Advance(2.0f);
  EXPECT_FALSE(clock.TimeUpGateOpen()) << "the stoppage has not yet made up 162000";
  clock.Advance(1.0f);
  EXPECT_TRUE(clock.TimeUpGateOpen());
}

// --- phases: each period starts its own record (report 2.1 numbering) ---

TEST(PesPeriod, EachPhaseIsItsOwnPESPeriod) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  EXPECT_EQ(clock.Record().period, 1u);
  clock.SetPhase(e_MatchPhase_2ndHalf);
  EXPECT_EQ(clock.Record().period, 3u);
  clock.SetPhase(e_MatchPhase_1stExtraTime);
  EXPECT_EQ(clock.Record().period, 5u);
  clock.SetPhase(e_MatchPhase_2ndExtraTime);
  EXPECT_EQ(clock.Record().period, 7u);
  clock.SetPhase(e_MatchPhase_Penalties);
  EXPECT_EQ(clock.Record().period, 9u);
}

TEST(PesPeriod, ANewPeriodStartsAFreshRecord) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.Advance(500.0f);
  clock.SetFrozen(true);
  clock.Advance(10.0f);
  clock.SetFrozen(false);
  clock.SetPhase(e_MatchPhase_2ndHalf);
  EXPECT_FLOAT_EQ(clock.PeriodClock(), 0.0f);
  EXPECT_FLOAT_EQ(clock.Record().f, 0.0f);
  EXPECT_FLOAT_EQ(clock.Record().limit, 0.0f);
  EXPECT_FALSE(clock.AddedTimeAnnounced());
}

// --- the time-up gate (FUN_14050B9B0 / FUN_140A6BEF0) ---

TEST(PesTimeUp, GateOpensExactlyAtTheHalfLength) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  EXPECT_FLOAT_EQ(c->timeUpBase, 162000.0f) << "45 game-minutes at 60 units a second";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.Advance(2699.0f);
  EXPECT_FALSE(clock.TimeUpGateOpen());
  EXPECT_FALSE(clock.PastPeriodLength());
  clock.Advance(1.0f);
  EXPECT_TRUE(clock.PastPeriodLength());
  // No added time is announced yet, so the gate is at the length itself.
  EXPECT_TRUE(clock.TimeUpGateOpen());
}

TEST(PesTimeUp, AddedMinutesHoldTheGateByWholeMinutes) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.Advance(2700.0f);
  clock.AnnounceAddedTime(4);
  const int minutes = clock.AddedMinutesUnclamped(4);
  ASSERT_GT(minutes, 0) << "the tier earns added time, so the gate is held";
  EXPECT_FLOAT_EQ(clock.Record().limit, static_cast<float>(minutes) * c->minuteUnits);
  EXPECT_FALSE(clock.TimeUpGateOpen());
  clock.Advance(minutes * 60.0f - 0.5f);
  EXPECT_FALSE(clock.TimeUpGateOpen()) << "half a unit short of the added minutes";
  clock.Advance(0.5f);
  EXPECT_TRUE(clock.TimeUpGateOpen()) << "the gate opens at the length plus the minutes";
}

TEST(PesTimeUp, AddedTimeIsAnnouncedOnce) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.Advance(2700.0f);
  clock.AnnounceAddedTime(4);
  const float limit = clock.Record().limit;
  clock.Advance(600.0f);
  clock.AnnounceAddedTime(4);
  EXPECT_FLOAT_EQ(clock.Record().limit, limit) << "the formula is evaluated once per period";
}

// Extra time's halves are 15 game-minutes: periods 5 and 7 use the flag length.
TEST(PesTimeUp, ExtraTimePeriodsUseTheShorterLength) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  EXPECT_FLOAT_EQ(c->timeUpFlag, 54000.0f);
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_1stExtraTime);
  clock.Advance(899.0f);
  EXPECT_FALSE(clock.TimeUpGateOpen());
  clock.Advance(1.0f);
  EXPECT_TRUE(clock.TimeUpGateOpen());
}

// PES's judge skips the time-up in the shoot-out (period 9): it has no clock.
TEST(PesTimeUp, ThePenaltyShootoutHasNoTimeUp) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesClock clock(*c);
  clock.SetPhase(e_MatchPhase_Penalties);
  clock.Advance(100000.0f);
  EXPECT_FALSE(clock.TimeUpGateOpen());
  EXPECT_FALSE(clock.PastPeriodLength());
}

// --- the display (report 2.6) ---

TEST(PesClock, DisplayMinuteAndSecondFromASecondsCount) {
  int minute = -1, second = -1;
  pes::ClockDisplay(1350.0f, &minute, &second);  // 22:30
  EXPECT_EQ(minute, 22);
  EXPECT_EQ(second, 30);
  pes::ClockDisplay(0.0f, &minute, &second);
  EXPECT_EQ(minute, 0);
  EXPECT_EQ(second, 0);
  pes::ClockDisplay(2700.0f, &minute, &second);  // 45:00
  EXPECT_EQ(minute, 45);
  EXPECT_EQ(second, 0);
}

// FUN_140A6BD80 takes hours/minutes/seconds off a seconds count, so the clock
// never exceeds 59 minutes before the wrap: PES wraps it to 0 instead.
TEST(PesClock, DisplayWrapsTheMinuteAtSixty) {
  int minute = -1, second = -1;
  pes::ClockDisplay(3600.0f, &minute, &second);  // 60:00 -> minute wraps
  EXPECT_EQ(minute, 0);
  EXPECT_EQ(second, 0);
  pes::ClockDisplay(5399.0f, &minute, &second);  // 89:59
  EXPECT_EQ(minute, 29);
  EXPECT_EQ(second, 59);
}

// --- stoppage: the linear formula, evaluated once at half end (report 3.2) ---

TEST(PesClock, AddedTimeIsElapsedSecondsTimesRate) {
  const pes::PesClockConstants c = FakeConstants();
  EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, 7), 0.0) << "no tier bonus";
  EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, 4), 720000);
  EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, 2), 360000);
  // Extra time adds less: 5400 vs 9000 per match second.
  EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_ExtraTime, 100.0f, 2), 180000);
  EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_ExtraTime, 100.0f, 4), 360000);
}

// The tier gates are 1..2, 3..6 and everything else (report 3.2's branching).
TEST(PesClock, AddedTimeTierGatesAreExact) {
  const pes::PesClockConstants c = FakeConstants();
  for (int tier : {0, 7, 8, 100}) {
    EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, tier), 0.0)
        << "2nd-half tier " << tier;
    EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_ExtraTime, 100.0f, tier), 0.0)
        << "extra-time tier " << tier;
  }
  for (int tier = 1; tier <= 2; ++tier) {
    EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, tier), 360000)
        << "2nd-half tier " << tier;
    EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_ExtraTime, 100.0f, tier), 180000)
        << "extra-time tier " << tier;
  }
  for (int tier = 3; tier <= 6; ++tier) {
    EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, tier), 720000)
        << "2nd-half tier " << tier;
    EXPECT_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_ExtraTime, 100.0f, tier), 360000)
        << "extra-time tier " << tier;
  }
}

// Extra time adds less stoppage than the second half (report 3.2).
TEST(PesClock, ExtraTimeAddsLessThanASecondHalf) {
  const pes::PesClockConstants c = FakeConstants();
  EXPECT_DOUBLE_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_ExtraTime, 100.0f, 4),
                   0.5 * pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, 4));
  EXPECT_DOUBLE_EQ(pes::AddedTimeUnits(c, pes::e_AddedTimePhase_ExtraTime, 100.0f, 2),
                   0.5 * pes::AddedTimeUnits(c, pes::e_AddedTimePhase_2ndHalf, 100.0f, 2));
}

// Report 3.1: the fourth official's board. Whole game-minutes, never zero once
// any time at all was lost, and clamped to the nine the HUD shows.
TEST(PesClock, AnnouncedAddedMinutesRoundUpAndClampToNine) {
  const pes::PesClockConstants c = FakeConstants();
  EXPECT_EQ(pes::AnnouncedAddedMinutes(c, pes::e_AddedTimePhase_2ndHalf, 0.0f, 7), 0);
  // Tier 7 earns no bonus at all, so the board reads zero however long the
  // half ran: PES's 9000/s term is the clock's own value, already spent.
  EXPECT_EQ(pes::AnnouncedAddedMinutes(c, pes::e_AddedTimePhase_2ndHalf, 2700.0f, 7), 0);
  EXPECT_EQ(pes::AnnouncedAddedMinutes(c, pes::e_AddedTimePhase_2ndHalf, 1.0f, 4), 1);
  // A whole second half at tier 4 is over twenty game-minutes, which the HUD clamps to nine.
  EXPECT_EQ(pes::AnnouncedAddedMinutes(c, pes::e_AddedTimePhase_2ndHalf, 2700.0f, 4),
            pes::kMaxAnnouncedAddedMinutes);
}

// --- the post-gate judge (FUN_14050B9B0 / FUN_14050C3E0): the counts, the timeouts, the patterns ---

namespace {

// A kind-0 touch with the ball on the ground and nobody holding it, so only the 30 s count runs.
// x = 0 keeps the ball out of the penalty box (one count a tick), and the zone sign points away
// from the ball, so the zone count never blows.
pes::TimeUpInput GroundedBall() {
  pes::TimeUpInput in = QuietJudge();
  in.zoneSign = -1;
  in.ballX_m = 0.0f;
  in.penaltyLineX_m = pesPenaltyAreaLineX;
  in.penaltyHalfWidth_m = pesPenaltyAreaHalfWidth;
  in.newest.kind = 0;
  return in;
}

}  // namespace

TEST(PesTimeUpJudge, TheSixtySecondCountBlowsOnTheTickAfterSixtyGameSeconds) {
  pes::TimeUpCounters c;
  EXPECT_EQ(FirstBlowTick(c, QuietJudge(), 10000), kJudgeRate * 60 + 1);
}

TEST(PesTimeUpJudge, ABlowClearsTheCountsSoTheNextBlowNeedsAFullCountAgain) {
  pes::TimeUpCounters c;
  ASSERT_EQ(FirstBlowTick(c, QuietJudge(), 10000), kJudgeRate * 60 + 1);
  EXPECT_EQ(FirstBlowTick(c, QuietJudge(), 10000), kJudgeRate * 60 + 1);
}

TEST(PesTimeUpJudge, TheBallZoneBlowsAfterFourGameSecondsInTheZone) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = QuietJudge();
  in.ballX_m = 0.0f;  // the centre: sign * x = 0 is inside L/3
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 4 + 1);
}

TEST(PesTimeUpJudge, TheBallZoneDoesNotCountWhileTheBallIsInTheAttackingThird) {
  pes::TimeUpCounters c;
  EXPECT_EQ(FirstBlowTick(c, QuietJudge(), 3000), 0) << "x = 52 is beyond L/3: no zone count";
}

// PES compares the signed count with +4 s, so a count running the other way never blows.
TEST(PesTimeUpJudge, AZoneCountRunningTheOtherWayNeverBlowsOnTheZone) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = QuietJudge();
  in.zoneSign = -1;
  in.ballX_m = 0.0f;
  EXPECT_EQ(FirstBlowTick(c, in, 3000), 0);
}

TEST(PesTimeUpJudge, AChangeOfSideInPossessionClearsTheBallZoneCount) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = QuietJudge();
  in.ballX_m = 0.0f;
  for (int i = 0; i < 100; ++i) ASSERT_FALSE(pes::TimeUpWhistle(c, in, kJudgeRate));
  pes::TimeUpInput other = in;
  other.zoneSign = -1;
  EXPECT_FALSE(pes::TimeUpWhistle(c, other, kJudgeRate));
  EXPECT_EQ(FirstBlowTick(c, in, 3000), kJudgeRate * 4 + 1) << "the count restarts from zero";
}

TEST(PesTimeUpJudge, AHolderFlaggedThirdKindBlowsAtOnce) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = QuietJudge();
  in.newest.kind = 3;
  in.newest.flag = 4;
  EXPECT_EQ(FirstBlowTick(c, in, 10), 1);
}

TEST(PesTimeUpJudge, AHolderFlaggedFourthKindBlowsAfterTenGameSeconds) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = QuietJudge();
  in.newest.kind = 4;
  in.newest.flag = 4;
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 10 + 1);
}

TEST(PesTimeUpJudge, AHolderFlaggedSecondKindBlowsAfterFiveGameSeconds) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = QuietJudge();
  in.newest.kind = 2;
  in.newest.flag = 4;
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 5 + 1);
}

TEST(PesTimeUpJudge, WithoutTheHolderFlagTheTouchTimeoutsDoNotBlow) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = QuietJudge();
  in.newest.kind = 4;
  in.newest.flag = 0;
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 60 + 1);
}

// --- the kind-0 touch: the 30 s count of a ball on the ground (FUN_14050B9B0 lines 261-297) ---

TEST(PesTimeUpJudge, AGroundBallWithNobodyOnItBlowsAfterThirtyGameSeconds) {
  pes::TimeUpCounters c;
  EXPECT_EQ(FirstBlowTick(c, GroundedBall(), 10000), kJudgeRate * 30 + 1);
}

TEST(PesTimeUpJudge, ABallInTheAirResetsTheThirtySecondCount) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  for (int tick = 0; tick < 1000; ++tick) ASSERT_FALSE(pes::TimeUpWhistle(c, in, kJudgeRate));
  in.ballHeight_m = 5.0f;
  EXPECT_FALSE(pes::TimeUpWhistle(c, in, kJudgeRate));
  in.ballHeight_m = 0.0f;
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 30 + 1) << "the count starts again at zero";
}

TEST(PesTimeUpJudge, TheThirtySecondBlowClearsItsCountToo) {
  pes::TimeUpCounters c;
  ASSERT_EQ(FirstBlowTick(c, GroundedBall(), 10000), kJudgeRate * 30 + 1);
  EXPECT_EQ(FirstBlowTick(c, GroundedBall(), 10000), kJudgeRate * 30 + 1);
}

TEST(PesTimeUpJudge, ACarrierWhoIsNotMovingKeepsTheThirtySecondCount) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.carrier = true;
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 30 + 1);
}

TEST(PesTimeUpJudge, ARecentRingOneTouchResetsACarriedBallCount) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.carrier = true;
  in.speed_mps = 5.0f;
  in.ring1Word = 100;  // under 6 game-seconds: the carrier's count is reset every tick
  EXPECT_EQ(FirstBlowTick(c, in, 3000), 0);
}

TEST(PesTimeUpJudge, TheAttackingBoxKeepsTheJudgeSilentOnAGroundBall) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.zoneSign = 1;
  in.ballX_m = 52.0f;
  EXPECT_EQ(FirstBlowTick(c, in, 2000), 0) << "the final block returns before the count";
}

TEST(PesTimeUpJudge, ARestartOfKindOneOnTheTouchBeforeBlowsAtOnce) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.second.restartKind = 1;
  EXPECT_EQ(FirstBlowTick(c, in, 10), 1);
}

// --- the kind-0 touch: the out-of-zone pattern (FUN_14050B9B0 lines 202-231) ---

TEST(PesTimeUpJudge, AnOutOfZonePairBlowsWhenTheNewestTouchIsOverThirtyFramesOld) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.newest.player = 3;  // team 0
  in.newest.frame = 100;
  in.second.player = 14;  // team 1
  in.frame = 131;
  EXPECT_EQ(FirstBlowTick(c, in, 10), 1);
}

TEST(PesTimeUpJudge, AThirtyFrameOldTouchPairDoesNotBlowOnTheOutOfZonePattern) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.newest.player = 3;
  in.newest.frame = 100;
  in.second.player = 14;
  in.frame = 130;
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 30 + 1);
}

TEST(PesTimeUpJudge, ASecondTouchOfKindElevenToThirteenBlowsOnTheOutOfZonePattern) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.newest.player = 3;
  in.second.player = 14;
  in.second.kind = 12;
  EXPECT_EQ(FirstBlowTick(c, in, 10), 1);
}

TEST(PesTimeUpJudge, APairOnOneTeamNeverBlowsOnTheOutOfZonePattern) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.newest.player = 3;
  in.newest.frame = 100;
  in.second.player = 5;  // team 0 as well
  in.frame = 131;
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 30 + 1);
}
TEST(PesTimeUpJudge, TheOutOfZonePatternNeedsTheBallInTheNearHalf) {
  pes::TimeUpCounters c;
  pes::TimeUpInput in = GroundedBall();
  in.newest.player = 3;
  in.newest.frame = 100;
  in.second.player = 14;
  in.frame = 131;
  in.ballX_m = 25.0f;  // beyond L/3: the pattern does not apply, so the 30 s count runs
  EXPECT_EQ(FirstBlowTick(c, in, 10000), kJudgeRate * 30 + 1);
}

// --- the stoppage writer (FUN_14044CE80 step 4): LIMIT moves with F while stopped ---

TEST(PesClock, TheStoppageLimitTakesHalfAUnitPerFrameAndCapsAt21600) {
  const pes::PesClockConstants c = FakeConstants();
  pes::PesClock clock(c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.SetFrozen(true);
  clock.AccrueStoppageLimit(1.0f);
  EXPECT_FLOAT_EQ(clock.Record().limit, pes::kStoppageUnitsPerFrame * c.gameSpeed);
  clock.AccrueStoppageLimit(10000.0f);
  EXPECT_FLOAT_EQ(clock.Record().limit, 21600.0f);
}

TEST(PesClock, TheStoppageLimitMovesOnlyWhileStoppedInAClockPeriod) {
  const pes::PesClockConstants c = FakeConstants();
  pes::PesClock clock(c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.AccrueStoppageLimit(1.0f);
  EXPECT_FLOAT_EQ(clock.Record().limit, 0.0f) << "in play it does not move";
  clock.SetPhase(e_MatchPhase_Penalties);
  clock.SetFrozen(true);
  clock.AccrueStoppageLimit(1.0f);
  EXPECT_FLOAT_EQ(clock.Record().limit, 0.0f) << "the shoot-out has no clock";
}

// The announcement adds the added minutes to what the stoppage already put on LIMIT.
TEST(PesTimeUp, AnAnnouncementAddsToTheStoppageAlreadyOnLimit) {
  pes::PesClockConstants c = FakeConstants();
  c.minuteUnits = 3600.0f;
  pes::PesClock clock(c);
  clock.SetPhase(e_MatchPhase_1stHalf);
  clock.SetFrozen(true);
  clock.AccrueStoppageLimit(1.0f);
  clock.SetFrozen(false);
  clock.Advance(2700.0f);
  clock.AnnounceAddedTime(4);
  const int minutes = clock.AddedMinutesUnclamped(4);
  ASSERT_GT(minutes, 0);
  EXPECT_FLOAT_EQ(clock.Record().limit, 27.0f + static_cast<float>(minutes) * c.minuteUnits);
}

// --- the install ---

TEST(PesClockInstall, ConstantsComeFromTheExe) {
  auto c = InstallConstants();
  if (!c) GTEST_SKIP() << "PES21_ROOT not set";
  EXPECT_FLOAT_EQ(c->gameSpeed, 54.0f);
  EXPECT_FLOAT_EQ(c->addedTier_3_6_2nd, 7200.0f);
  EXPECT_FLOAT_EQ(c->addedTier_1_2_2nd, 3600.0f);
  EXPECT_FLOAT_EQ(c->addedTier_1_2_ET, 1800.0f);
  EXPECT_FLOAT_EQ(c->addedTier_3_6_ET, 3600.0f);
  EXPECT_FLOAT_EQ(c->timeUpBase, 162000.0f);
  EXPECT_FLOAT_EQ(c->timeUpFlag, 54000.0f);
  EXPECT_FLOAT_EQ(c->minuteUnits, 3600.0f);
  EXPECT_FLOAT_EQ(c->secondsUnit, 60.0f);
  EXPECT_FLOAT_EQ(c->limitCap, 21600.0f);
}
