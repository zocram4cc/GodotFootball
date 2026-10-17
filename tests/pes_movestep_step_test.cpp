// PES 2021's MOVESTEP handler group C: the stepping states STEP_LOOP, STEP_LOOP_CHANGE,
// TURN_STOP, STEP_TO_RUN and RUN_TO_STEP (docs/re/pes-re-anim-select.md 2.1, 5.1-5.2), plus the
// TURN_STOP entry FUN_1406d2590. The handlers are ports of FUN_1406c4c10, FUN_1406c6480,
// FUN_1406cab20, FUN_1406c6f80 and FUN_1406c1070; the tests exercise each branch against
// synthetic install constants, and the install relations live in pes_movestep_test.cpp.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <string>

#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "movestep_team_synthetic.hpp"
#include "pes/movestep_step.hpp"
#include "pes/movestep_turn.hpp"

// The last entry any group-C handler asked for, so a test can assert the exit without depending
// on another group's TU.
pes::MoveState requested = pes::MoveState::kInit;

// The last table selector SpeedToRate was asked for, so a test can see which table it read.
int rateSelector = -1;

namespace {

using pes::MoveHost;
using pes::MoveReq;
using pes::MoveState;
using pes::movestep::EnterTurnStop;
using pes::movestep::RunToStep;
using pes::movestep::StepLoop;
using pes::movestep::StepLoopChange;
using pes::movestep::StepToRun;
using pes::movestep::TurnStop;

// The synthetic install scalars, in the field order of pes::StepScalars. A test changes only
// the one its branch reads, so a value carries through from these defaults.
pes::StepScalars SyntheticScalars() {
  pes::StepScalars k{};
  k.one = 1.0f;
  k.half = 0.5f;
  k.full = 360.0f;
  k.wrapFull = -360.0f;
  k.halfTurn = 180.0f;
  k.negOne = -1.0f;
  k.ticks60 = 60.0f;
  k.scale1000 = 1000.0f;
  k.kmh5 = 5.0f;
  k.kmh11 = 11.0f;
  k.kmh13p5 = 13.5f;
  k.kmh16 = 16.0f;
  k.kmh22 = 22.0f;
  k.kmh8 = 8.0f;
  k.deg22p5 = 22.5f;
  k.deg11p25 = 11.25f;
  k.deg33p75 = 33.75f;
  k.deg67p5 = 67.5f;
  k.deg90 = 90.0f;
  k.deg135 = 135.0f;
  k.deg112p5 = 112.5f;
  k.deg175 = 175.0f;
  k.deg5p625 = 5.625f;
  k.deg28p125 = 28.125f;
  k.deg31p64 = 31.640625f;
  k.deg168p75 = 168.75f;
  k.deg146p25 = 146.25f;
  k.deg140p625 = 140.625f;
  k.deg174p375 = 174.375f;
  k.deg270 = 270.0f;
  k.neg45 = -45.0f;
  k.neg67p5 = -67.5f;
  k.neg90 = -90.0f;
  k.neg112p5 = -112.5f;
  k.neg180 = -180.0f;
  k.deg45 = 45.0f;
  k.kmh20 = 20.0f;
  k.dist3 = 3.0f;
  k.rate0p9 = 0.9f;
  k.s0p6 = 0.6f;
  k.s0p4 = 0.4f;
  k.s0p3 = 0.3f;
  k.s0p1333 = 0.1333f;
  k.s0p1667 = 0.1667f;
  k.s0p2 = 0.2f;
  k.s0p25 = 0.25f;
  k.s0p35 = 0.35f;
  k.dist1p5 = 1.5f;
  k.dist1p25 = 1.25f;
  k.clamp0p8 = 0.8f;
  k.step0p05 = 0.05f;
  k.eps1e7 = 1.1920928955078125e-07f;
  k.kmh3p6 = 3.6f;
  k.rate0p95 = 0.95f;
  k.deg10 = 10.0f;
  k.deg78p75 = 78.75f;
  k.s0p85 = 0.85f;
  k.six = 6.0f;
  k.threeQuarters = 0.75f;
  k.rate1p6 = 1.6f;
  k.sq1139 = 1139.0625f;
  k.sq2025 = 2025.0f;
  k.sq8100 = 8100.0f;
  return k;
}

pes::PlaybackConstants SyntheticPlayback() {
  pes::PlaybackConstants k{};
  k.gLow = 0.9166f;
  k.gHigh = 1.0833f;
  k.gSlope = -0.1667f;
  k.one = 1.0f;
  k.stepSlope = 0.15f;
  k.stepBase = 0.95f;
  k.turnP2 = 0.1f;
  k.turnT = 0.05f;
  return k;
}

pes::AngleBucketConstants SyntheticBuckets() {
  pes::AngleBucketConstants k{};
  k.wide = {-180, -157.5f, -112.5f, -67.5f, -22.5f, 0, 22.5f, 67.5f, 112.5f, 157.5f, 180};
  k.narrow = {-180, -170, -112.5f, -67.5f, -10, 0, 10, 67.5f, 112.5f, 170, 180};
  for (int i = 0; i < 10; ++i) k.bucket[i] = i;
  k.hysteresisUp = 5.0f;
  k.hysteresisDown = -5.0f;
  return k;
}

pes::StepTables SyntheticTables() {
  pes::StepTables t{};
  for (int i = 0; i < 10; ++i) t.bucketColumn[i] = (i * 2 + 9) % 10;
  for (int i = 0; i < 64; ++i) t.kindBandRow[i] = 0x540 + i;
  for (int i = 0; i < 10; ++i) t.bucketDegrees[i] = (i < 5 ? 45.0f * i : 45.0f * (i - 10));
  for (int i = 0; i < 12; ++i) t.styleMotion[i] = 0x600 + i;
  return t;
}

// A host answering the handlers with the synthetic values above. Each accessor names the PES
// read it stands for (movestep.hpp's MoveHost).
class FakeHost : public MoveHost {
 public:
  // LateralDamping's reads: the step tests do not drive the drift.
  pes::DampingConstants damping{};
  pes::DampingRecord dampingRecord{};
  int CurrentState() const override { return 0; }
  const pes::TeamAi& Team() const override { return team; }
  SyntheticTeam team;
  const pes::DampingConstants& DampingConstantsView() const override { return damping; }
  const pes::DampingRecord& DampingRecordView() const override { return dampingRecord; }
  // LocomotionStyle's reads: the step tests do not drive the style ladder.
  pes::StyleConstants style{};
  const pes::StyleConstants& StyleConstantsView() const override { return style; }
  bool ScriptedSlotIsOurs() const override { return false; }
  int StaminaPercent() const override { return 100; }
  float PlayerHeightCm() const override { return 180.0f; }
  void BallSample(int, float out[3]) const override { out[0] = out[1] = out[2] = 0.0f; }
  float BallMotionHeadingDeg() const override { return 0.0f; }
  pes::MoveCell Cell(int row, int column, bool walkFlag) override {
    ++cellCalls;
    lastRow = row;
    lastColumn = column;
    lastWalk = walkFlag;
    pes::MoveCell c;
    c.motionNo = cellMotion;
    c.mirror = false;
    c.found = cellMotion >= 0;
    return c;
  }
  int PickN(int n) override { return 0; }
  bool Percent(int pct) override { return false; }

  int CommandSpeedClass() const override { return commandClass; }
  float RateCeiling() const override { return rateCeiling; }
  float CommandSpeedKmh() const override { return commandKmh; }
  float CurrentSpeedKmh() const override { return currentKmh; }
  float PhysiqueG() const override { return 1.0f; }
  float TicksPerSecond() const override { return 54.0f; }
  int MatchTick() const override { return 7; }
  int RecentStamp(int index) const override { return recentStamp[index]; }
  float MotionStartTime(int motionNo) const override { return 0.0f; }

  float RecordF2468() const override { return 1.0f; }
  int MoveStatePrevious() const override { return previous; }
  float RecordF0bec() const override { return 0.0f; }
  void SetRecordF0bec(float value) override {}
  const pes::AngleBucketConstants& AngleBuckets() const override { return buckets; }

  void PositionAt(int index, float out[3]) const override {
    out[0] = displacement[0];
    out[1] = displacement[1];
    out[2] = displacement[2];
  }
  int MatchPhase() const override { return 1; }
  bool IsReplay() const override { return false; }
  void Issue(const pes::MotionRequest& request) override { issued = request; }

  const pes::StepScalars& StepConstants() const override { return scalars; }
  float BandSpeedKmh(int band, float g) const override { return 0.0f; }
  bool MirrorChoice(bool sameTeam) const override { return false; }
  int FootBand() const override { return footBand; }
  const pes::RunConstants& RunConstantsView() const override { return runConstants; }
  const pes::RunTables& RunTablesView() const override { return runTables; }
  const pes::SpeedRateTable& SpeedRateTableView() const override { return speedRate; }
  const pes::movestep::MoveTurnConstants& MoveTurnConstantsView() const override {
    return turnConstants;
  }
  const pes::StepTables& StepTablesView() const override { return tables; }
  bool BucketIsLeft(int bucket) const override { return bucket < 5; }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { return playback; }

  int CurrentMotion() const override { return currentMotion; }
  int CurrentClipFrames() const override { return currentClipFrames; }
  int AnimeState() const override { return 0; }
  const pes::MotionRecord& Motion(int) const override {
    static const pes::MotionRecord kNoRecord;  // group C reads no motion record
    return kNoRecord;
  }
  bool ClipFlag() const override { return false; }
  const pes::MoveConstants& MoveConstantsView() const override { return moveConstants; }
  const pes::IdleRowTables& IdleRowTablesView() const override { return idleRows; }
  float FacingDeg() const override { return 0.0f; }
  float BallBearingDeg() const override { return 0.0f; }
  int ActionIndex() const override { return 0; }
  int RelatedSlot() const override { return -1; }
  void RecordPosition(int, float out[3]) const override { out[0] = out[1] = out[2] = 0.0f; }
  float RecordFacing(int) const override { return 0.0f; }
  int MatchState11d4() const override { return 0; }
  int MatchState1554() const override { return 0; }
  int PhaseTick() const override { return 0; }
  int MatchModeByte() const override { return 0; }
  bool MatchObject3b0() const override { return false; }
  bool TrainerFlag1423e7fd0() const override { return false; }
  int RecordConditionByte(int) const override { return 100; }
  bool ActionSlotMarked(int) const override { return true; }
  bool ActionTableOwns(int) const override { return false; }
  int RecordActionA() const override { return 5; }
  int RecordActionB() const override { return 5; }

  pes::StepScalars scalars = SyntheticScalars();
  pes::StepTables tables = SyntheticTables();
  pes::PlaybackConstants playback = SyntheticPlayback();
  pes::AngleBucketConstants buckets = SyntheticBuckets();
  pes::RunConstants runConstants{};
  pes::RunTables runTables{};
  pes::SpeedRateTable speedRate{};
  pes::movestep::MoveTurnConstants turnConstants{};
  pes::MoveConstants moveConstants{};
  pes::IdleRowTables idleRows{};

  int commandClass = 3;
  float rateCeiling = 1.0f;
  float commandKmh = 16.0f;
  float currentKmh = 12.0f;
  int recentStamp[2] = {0, 0};
  int previous = 0;
  int footBand = 0;
  float displacement[3] = {0.0f, 0.0f, 1.0f};  // +z: heading 0 degrees

  int currentMotion = 0;
  int currentClipFrames = 0;
  int cellCalls = 0, lastRow = -1, lastColumn = -1;
  bool lastWalk = false;
  int cellMotion = -1;
  pes::MotionRequest issued{};
};

// -------------------------------------------------------------------------------------
// STEP_LOOP, FUN_1406c4c10: the band walk and the tick-keeping tail.
// -------------------------------------------------------------------------------------

// The band walk: with the band 0, the previous band 1, and the second dwell past its threshold
// (0.6 s, or 0.4 s at kind 7), the band becomes 1 and the speed comes back from req[0x22].
TEST(StepLoop, WalksTheBandDownWhenThePreviousBandWasOne) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kClassWord, 0);
  req.SetU(pes::moveword::kPrevClassWord, 1);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.70f);  // past 0.6 * 1.0
  req.SetF(pes::moveword::kBandSpeed, 0.0f);
  req.SetF(pes::moveword::kBandSpeedCopy, 12.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  // A moving player, so the stop helper does not fire before the walk is observable.
  host.displacement[0] = 0.5f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 2.0f;
  StepLoop(host, req);
  // The walk itself: the band becomes 1 and the speed comes back from req[0x22]. The tail then
  // records the stepped band, so the restored speed carries through the blend's 0.5/0.5 mix.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit2), 0.0f);
  EXPECT_NEAR(req.F(pes::moveword::kBandSpeed), 12.0f, 1e-3f);
}

// The walk's threshold at kind 7 is 0.4 s instead of 0.6 s, so an elapsed of 0.5 walks the band
// down at kind 7 while it would not at kind 2.
TEST(StepLoop, WalksTheBandDownAfterTheShorterKeeperThreshold) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 7);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kClassWord, 0);
  req.SetU(pes::moveword::kPrevClassWord, 1);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.50f);  // past 0.4, under 0.6
  req.SetF(pes::moveword::kBandSpeed, 0.0f);
  req.SetF(pes::moveword::kBandSpeedCopy, 12.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  host.displacement[0] = 0.5f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 2.0f;
  StepLoop(host, req);
  EXPECT_NEAR(req.F(pes::moveword::kBandSpeed), 12.0f, 1e-3f);
}

// The slow-player re-plan: with the arrival flag req[0x3f] set, a live band below 3, no precise
// arrival and the ground speed under 16 km/h, the handler re-plans through GOTO_RUN.
TEST(StepLoop, RePlansASlowPlayerThroughGotoRun) {
  FakeHost host;
  MoveReq req;
  host.displacement[0] = 0.0f;  // a stationary player: the ground speed is 0
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.0f;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kPrevClassWord, 2);
  req.SetF(pes::moveword::kDwellLimit2, 0.0f);
  req.SetU(0x3f, 1);  // the arrival flag
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  StepLoop(host, req);
  EXPECT_EQ(requested, MoveState::kGotoRun);
}

// -------------------------------------------------------------------------------------
// STEP_TO_RUN, FUN_1406c6f80.
// -------------------------------------------------------------------------------------

// Both heading gates fire: the remaining heading past 2025 deg^2 and the facing-turn past
// 8100 deg^2. The handler stops (FUN_1406d5d60) and leaves the remaining dwell in req[0x18].
TEST(StepToRun, StopsWhenBothHeadingGatesFire) {
  FakeHost host;
  MoveReq req;
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.25f);
  // Both gates fire: the remaining heading past 90 deg (8100 deg^2) and the facing-turn past
  // 45 deg (2025 deg^2) — req[0x18] - req[1] = 100 deg, req[2] - req[1] = 60 deg.
  req.SetF(pes::moveword::kRecordStampB, 110.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 70.0f);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  StepToRun(host, req);
  // The stop path writes req[0x18] = dwell limit - elapsed, clamped at zero, and calls the stop
  // entry (FUN_1406d5d60), which owns the state request.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRecordStampB), 0.75f);
  EXPECT_EQ(requested, MoveState::kStop);
}

// Only the request heading is past its gate: the handler blends the heading and, with the second
// dwell run out, asks for RUN_LOOP.
TEST(StepToRun, AsksForRunLoopOnceTheSecondDwellRunsOut) {
  FakeHost host;
  MoveReq req;
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  // The remaining heading past its gate but the facing-turn inside: only the request heading
  // is off, so the handler blends and asks for RUN_LOOP once the second dwell has run out.
  req.SetF(pes::moveword::kRecordStampB, 110.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 20.0f);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  // The class 2+ non-precise branch runs first (the band below 1), so the band must be set for
  // the blend tail to be reached: PES's order is that branch, then the blend.
  req.SetU(pes::moveword::kClassWord, 2);
  StepToRun(host, req);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(MoveState::kRunLoop));
  // The blend wrote a wrapped root facing (req[0x3a] set), and the dwell was reset.
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellElapsed), 0.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellElapsed2), 0.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kMoveBucket), 0.0f);
}

// -------------------------------------------------------------------------------------
// RUN_TO_STEP, FUN_1406c1070.
// -------------------------------------------------------------------------------------

// With the class -1 the blend branch runs, and the second dwell elapsed asks for STEP_LOOP.
TEST(RunToStep, AsksForStepLoopWhenTheClassIsMinusOne) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);  // -1
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetF(pes::moveword::kRecordStampA, 20.0f);
  req.SetF(pes::moveword::kRecordStampB, 20.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 12.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  RunToStep(host, req);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(MoveState::kStepLoop));
}

// The rate walk: with the target above the current rate, req[0x4a] rises by 0.05 and stops at
// the target (never above it). The stub's target is 1.0, so a start of 0.98 rises to 1.0
// exactly (the clamp), and a start of 0.10 rises to 0.15 without it.
TEST(RunToStep, RaisesTheRateTowardTheTargetAndStopsAtIt) {
    FakeHost host;
    MoveReq req;
    req.SetU(pes::moveword::kCommandSpeedClass, 3);
    req.SetF(pes::moveword::kDwellLimit2, 1.0f);
    req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
    req.SetF(pes::moveword::kRecordStampA, 20.0f);
    req.SetF(pes::moveword::kRecordStampB, 20.0f);
    req.SetF(pes::moveword::kHeading, 10.0f);
    req.SetF(pes::moveword::kFacing, 10.0f);
    req.SetF(pes::moveword::kBodyDirection, 12.0f);
    req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
    req.SetF(pes::moveword::kRate, 0.10f);
    RunToStep(host, req);
    EXPECT_NEAR(req.F(pes::moveword::kRate), 0.15f, 1e-6f);

  // The band restore: when the second dwell is nearly out (elapsed < limit * scale, with
  // scale = 0.3 when the foot band is below the previous band), the band and its speed come
  // back from the previous ones.
}

  TEST(RunToStep, RestoresThePreviousBandWhenTheDwellIsNearlyOut) {
    FakeHost host;
    MoveReq req;
    req.SetU(pes::moveword::kCommandSpeedClass, 3);
    req.SetU(pes::moveword::kClassWord, 0);
    req.SetU(pes::moveword::kPrevClassWord, 2);
    req.SetF(pes::moveword::kDwellLimit2, 1.0f);
    req.SetF(pes::moveword::kDwellElapsed2, 0.10f);  // under 0.3 * 1.0
    req.SetF(pes::moveword::kRecordStampA, 20.0f);
    // Both errors inside their gates (heading 0 and body 2 deg), so the tick-keeping tail runs
    // rather than the blend branch.
    req.SetF(pes::moveword::kRecordStampB, 10.0f);
    req.SetF(pes::moveword::kHeading, 10.0f);
    req.SetF(pes::moveword::kFacing, 10.0f);
    req.SetF(pes::moveword::kBodyDirection, 12.0f);
    req.SetF(pes::moveword::kBandSpeed, 0.0f);
    req.SetF(pes::moveword::kBandSpeedCopy, 12.0f);
    RunToStep(host, req);
    EXPECT_EQ(req.U(pes::moveword::kClassWord), 2u);
    EXPECT_FLOAT_EQ(req.F(pes::moveword::kBandSpeedPrev), 12.0f);
  }
  TEST(RunToStep, RestoresTheBandWithTheWiderWindowWhenTheBandIsUp) {
    // The band at or above the previous one: the window halves to 0.6 of the limit, so an
    // elapsed under 0.6 restores and one past it does not.
    FakeHost host;
    MoveReq base;
    base.SetU(pes::moveword::kCommandSpeedClass, 3);
    base.SetU(pes::moveword::kClassWord, 2);
    base.SetU(pes::moveword::kPrevClassWord, 2);
    base.SetF(pes::moveword::kDwellLimit2, 1.0f);
    base.SetF(pes::moveword::kRecordStampA, 10.0f);
    base.SetF(pes::moveword::kRecordStampB, 10.0f);
    base.SetF(pes::moveword::kHeading, 10.0f);
    base.SetF(pes::moveword::kFacing, 10.0f);
    base.SetF(pes::moveword::kBodyDirection, 12.0f);
    base.SetF(pes::moveword::kBandSpeedCopy, 12.0f);
    MoveReq restored = base;
    restored.SetF(pes::moveword::kDwellElapsed2, 0.50f);  // under 0.6
    RunToStep(host, restored);
    EXPECT_FLOAT_EQ(restored.F(pes::moveword::kBandSpeedPrev), 12.0f);
    MoveReq held = base;
    held.SetF(pes::moveword::kDwellElapsed2, 0.70f);  // past 0.6
    RunToStep(host, held);
    EXPECT_FLOAT_EQ(held.F(pes::moveword::kBandSpeedPrev), 0.0f);
  }
  TEST(RunToStep, KeepsTheBandPastTheNarrowWindow) {
    // The narrow window: the band below the previous one gives 0.3 of the limit, so an elapsed
    // of 0.5 is past it and does not restore (while 0.5 would restore under the wide window).
    FakeHost host;
    MoveReq req;
    req.SetU(pes::moveword::kCommandSpeedClass, 3);
    req.SetU(pes::moveword::kClassWord, 0);
    req.SetU(pes::moveword::kPrevClassWord, 2);
    req.SetF(pes::moveword::kDwellLimit2, 1.0f);
    req.SetF(pes::moveword::kDwellElapsed2, 0.50f);
    req.SetF(pes::moveword::kRecordStampA, 10.0f);
    req.SetF(pes::moveword::kRecordStampB, 10.0f);
    req.SetF(pes::moveword::kHeading, 10.0f);
    req.SetF(pes::moveword::kFacing, 10.0f);
    req.SetF(pes::moveword::kBodyDirection, 12.0f);
    req.SetF(pes::moveword::kBandSpeedCopy, 12.0f);
    RunToStep(host, req);
    EXPECT_FLOAT_EQ(req.F(pes::moveword::kBandSpeedPrev), 0.0f);
  }


// The blend branch with the dwell unfinished: no state change, and the root facing is written.
TEST(RunToStep, KeepsTheTickWhileBlendingTheHeading) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
  req.SetF(pes::moveword::kRecordStampA, 20.0f);
  req.SetF(pes::moveword::kRecordStampB, 200.0f);  // past the 2025 deg^2 gate
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 12.0f);
  RunToStep(host, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
}

// -------------------------------------------------------------------------------------
// TURN_STOP, FUN_1406cab20: the rate scale and the dwell-return gate.
// -------------------------------------------------------------------------------------

// The rate is scaled by 0.9 (DAT_14258c260) at the top of the tick, so a request entering with
// a rate of 1.0 leaves it at 0.9.
// EnterTurnStop's playback (FUN_1406d2590 arm 1): the interface's vt+0x50 answer
// (FUN_1406e9f10 = TurnPlayback(G, p)), never the band's km/h speed. A taker standing still
// to take a restart has ground speed 0; writing a speed into req[0x35] latches playback 0
// into the record, and the issued clip's frame never advances.
TEST(EnterTurnStop, KeepsAStandingTakersPlaybackNonzero) {
  FakeHost host;
  host.currentKmh = 0.0f;  // standing still at the restart spot
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 3u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kClassWord, 1u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetU(pes::moveword::kLookBucket, 0u);
  req.SetU(pes::moveword::kLookBucketAlt, 0u);
  req.SetF(pes::moveword::kDistToTarget, 5.0f);
  req.SetF(pes::moveword::kDistToLook, 5.0f);
  req.SetF(pes::moveword::kRate, 0.3f);
  EnterTurnStop(host, req);
  EXPECT_GT(req.F(pes::moveword::kPlayback), 0.0f) << "a standing taker's playback";
}

TEST(TurnStop, ScalesTheRateByTheRecordConstant) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kRate, 1.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.10f);  // not out: the handler keeps the tick
  TurnStop(host, req);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.9f);
}

// The dwell gate: with the second dwell unfinished the handler returns without a state change.
TEST(TurnStop, KeepsTheTickWhileTheSecondDwellRuns) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kRate, 1.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.60f);  // under the limit: the tick is kept
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.10f);   // and the first dwell has not run out
  TurnStop(host, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
}

}  // namespace
// The shared helpers and the other groups' entries the handlers call, stubbed so this test
// links without their translation units. Each carries the contract's signature in its own
// namespace; a test asserts the state the handler asked for, not what the entry did, and the
// entry's own port is tested in its owner's test file.
namespace pes {
namespace movestep {
float SpeedToRate(MoveHost& host, float speedKmh, int tableSelector) {
  rateSelector = tableSelector;
  return speedKmh > 0.0f ? 1.0f : 0.0f;
}
int LocomotionStyle(MoveHost&, int, float, MoveReq&, float, bool) { return 0; }
int IdleRow(MoveHost&, MoveReq&) { return 0x5a1; }
bool StayDecision(MoveHost&, MoveReq&) { return false; }
bool OutOfPlayPreempt(MoveHost&, MoveReq&) { return false; }
bool EnterAccelerate(MoveHost&, MoveReq&) { return false; }
void EnterGotoRun(MoveHost&, MoveReq&, bool) { requested = MoveState::kGotoRun; }
void EnterStop(MoveHost&, MoveReq&) { requested = MoveState::kStop; }
void EnterRunTurnRun(MoveHost&, MoveReq&) { requested = MoveState::kRunTurnRun; }
bool EnterRunTurnStep(MoveHost&, MoveReq&, float) { return false; }
bool EnterStepTurnRun(MoveHost&, MoveReq&) { return false; }
}  // namespace movestep
}  // namespace pes
