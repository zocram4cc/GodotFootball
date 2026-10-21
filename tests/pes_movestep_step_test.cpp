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

// Whether the stay decision (FUN_1406b6bb0) agrees to hold the player, so a test can open the
// paths that gate on it.
bool stayDecision = false;

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
  k.s0p7 = 0.7f;
  k.sq1139 = 1139.0625f;
  k.sq2025 = 2025.0f;
  k.sq8100 = 8100.0f;
  k.sq12656p25 = 12656.25f;
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
  return t;
}

// The speed-rate table (FUN_1406b9e80's ten records: vHi, vLo, rHi, rLo). One record maps
// every speed to a rate of 1.0, so the rate walk's target is 1.0.
pes::SpeedRateTable SyntheticSpeedRate() {
  pes::SpeedRateTable t{};
  t.record[0] = {1000.0f, 0.0f, 1.0f, 1.0f};
  t.kindScale = 1.0f;
  return t;
}

// The run constants the shared helpers read: the speed scale (3.6), the one and the fold pair.
pes::RunConstants SyntheticRunConstants() {
  pes::RunConstants k{};
  k.one = 1.0f;
  k.speed3600 = 3.6f;
  k.full = 360.0f;
  k.wrapFull = -360.0f;
  k.half_turn = 180.0f;
  k.neg = -1.0f;
  return k;
}

// The representative motion per style and band (RunTables::styleGroup, the install's
// 0x1425ddcf0 table).
pes::RunTables SyntheticRunTables() {
  pes::RunTables t{};
  for (int i = 0; i < 64; ++i) t.styleGroup[i] = 0x600 + i;
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
    const float* v = index < 2 ? displacement : position;  // the adapter's rule: index 0 and 1 are the displacement
    out[0] = v[0];
    out[1] = v[1];
    out[2] = v[2];
  }
  void OwnPosition(float out[3]) const override {
    out[0] = position[0];
    out[1] = position[1];
    out[2] = position[2];
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
  // The step tests do not reach RUN_LOOP's tail, so the sine view is never read here.
  const pes::SpeedClassConstants& SpeedClassConstantsView() const override { static const pes::SpeedClassConstants none{}; return none; }
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
  pes::RunConstants runConstants = SyntheticRunConstants();
  pes::RunTables runTables = SyntheticRunTables();
  pes::SpeedRateTable speedRate = SyntheticSpeedRate();
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
  float position[3] = {0.0f, 0.0f, 0.0f};      // the player's own ground position, PES's (x, height, z)
  float displacement[3] = {0.0f, 0.0f, 1.0f};  // PES's (x, height, z): +z is heading 0 degrees

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
// The request is precise, so the STEP_TO_RUN arm (which needs the precise word clear) stays out
// of the way and the walk is the only thing that changes.
TEST(StepLoop, WalksTheBandDownWhenThePreviousBandWasOne) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
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
  requested = MoveState::kInit;  // each test starts from a clean state
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
  requested = MoveState::kInit;  // each test starts from a clean state
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

// The blend branch asks for STEP_LOOP once the second dwell has run out. The blend branch is
// only reached when the tick is inside all three of PES's gates at 0x1406C1232: the class is
// not -1, the heading error is inside 8100 deg^2 and the body error inside 12656.25 deg^2.
TEST(RunToStep, AsksForStepLoopWhenTheSecondDwellRunsOut) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
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

// PES's branch at 0x1406C1232: the class -1, a heading error past 8100 deg^2 (0x1425DDFD8) or
// a body error past 12656.25 deg^2 (0x1425DDFDC) sends the tick to the STOP / IDLE / TURN_STOP
// arm, and only a tick inside all three gates blends the root facing (0x1406C1261).
// So the class -1 does NOT blend: it stops.
TEST(RunToStep, StopsInsteadOfBlendingWhenTheClassIsMinusOne) {
  FakeHost host;
  MoveReq req;
  requested = MoveState::kInit;  // each test starts from a clean state
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);  // -1
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
  req.SetF(pes::moveword::kRecordStampA, 20.0f);
  req.SetF(pes::moveword::kRecordStampB, 20.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 12.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetU(pes::moveword::kClassWord, 1u);
  RunToStep(host, req);
  // The blend branch never runs: no root facing is written.
  EXPECT_FALSE(req.Flag(pes::moveword::kRootFacing));
  // The stop arm runs: the class word is 1, so FUN_1406D5D60 asks for STOP.
  EXPECT_EQ(requested, MoveState::kStop);
}

// The heading-error gate is 8100 deg^2 (0x1425DDFD8 = 90 deg), not 2025. A 100-deg heading
// error is past 8100 deg^2, so the tick leaves the blend branch and takes the stop arm.
// The stop arm's own gate (PES at 0x1406C142A) is wrap180(the facing - the body direction),
// and it must be past 22.5 degrees for FUN_1406D5D60: the body direction is 40 degrees from
// the facing here.
TEST(RunToStep, StopsWhenTheHeadingErrorIsPastNinetyDegrees) {
  FakeHost host;
  MoveReq req;
  requested = MoveState::kInit;  // each test starts from a clean state
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
  req.SetF(pes::moveword::kRecordStampA, 20.0f);
  req.SetF(pes::moveword::kRecordStampB, 110.0f);  // the heading error is 100 deg
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 50.0f);  // the body turn is 40 deg
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetU(pes::moveword::kClassWord, 1u);
  RunToStep(host, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kRootFacing));
  EXPECT_EQ(requested, MoveState::kStop);
}

// The stop arm's gate is the body direction, not the look heading: with the look heading 40
// degrees from the facing and the body direction level with it, the tick takes the TURN_STOP
// arm (FUN_1406D2590), not the stop arm.
TEST(RunToStep, TheStopArmReadsTheBodyDirectionAndNotTheLookHeading) {
  FakeHost host;
  MoveReq req;
  requested = MoveState::kInit;  // each test starts from a clean state
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
  req.SetF(pes::moveword::kRecordStampA, 20.0f);
  req.SetF(pes::moveword::kRecordStampB, 110.0f);  // the heading error is 100 deg
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);  // the body turn is 0 deg
  req.SetF(pes::moveword::kLookHeading, 50.0f);     // the look heading is 40 deg from the facing
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetU(pes::moveword::kClassWord, 1u);
  RunToStep(host, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kRootFacing));
  // The body turn is inside 22.5 degrees, so the stop arm does not run; the class is not -1
  // and the distance is 0, so the TURN_STOP arm does.
  EXPECT_NE(requested, MoveState::kStop);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(MoveState::kTurnStop));
}

// A heading error of 80 deg is inside the 90-deg gate, so the tick blends: the root facing is
// written and no state is asked for while the second dwell still runs.
TEST(RunToStep, BlendsWhenTheHeadingErrorIsInsideNinetyDegrees) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
  req.SetF(pes::moveword::kRecordStampA, 20.0f);
  req.SetF(pes::moveword::kRecordStampB, 90.0f);  // the heading error is 80 deg
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 12.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  RunToStep(host, req);
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
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
    // The heading error is 100 deg, past PES's 8100 deg^2 gate (0x1425DDFD8), so the tick takes
    // the stop arm, which restores the band.
    req.SetF(pes::moveword::kRecordStampB, 110.0f);
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
    // The band at or above the previous one: the window is 0.7 of the limit (0x14257A194), so
    // an elapsed under 0.7 restores and one past it does not.
    FakeHost host;
    MoveReq base;
    base.SetU(pes::moveword::kCommandSpeedClass, 3);
    base.SetU(pes::moveword::kClassWord, 2);
    base.SetU(pes::moveword::kPrevClassWord, 2);
    base.SetF(pes::moveword::kDwellLimit2, 1.0f);
    base.SetF(pes::moveword::kRecordStampA, 10.0f);
    base.SetF(pes::moveword::kRecordStampB, 110.0f);  // the heading error is 100 deg
    base.SetF(pes::moveword::kHeading, 10.0f);
    base.SetF(pes::moveword::kFacing, 10.0f);
    base.SetF(pes::moveword::kBodyDirection, 12.0f);
    base.SetF(pes::moveword::kBandSpeedCopy, 12.0f);
    MoveReq restored = base;
    restored.SetF(pes::moveword::kDwellElapsed2, 0.50f);  // under 0.7
    RunToStep(host, restored);
    EXPECT_FLOAT_EQ(restored.F(pes::moveword::kBandSpeedPrev), 12.0f);
    MoveReq held = base;
    held.SetF(pes::moveword::kDwellElapsed2, 0.80f);  // past 0.7
    RunToStep(host, held);
    EXPECT_FLOAT_EQ(held.F(pes::moveword::kBandSpeedPrev), 0.0f);
  }
  TEST(RunToStep, KeepsTheBandPastTheNarrowWindow) {
    // The narrow window: the band below the previous one gives 0.3 of the limit (0x142590D98),
    // so an elapsed of 0.5 is past it and does not restore (while 0.5 would restore under the
    // wide window).
    FakeHost host;
    MoveReq req;
    req.SetU(pes::moveword::kCommandSpeedClass, 3);
    req.SetU(pes::moveword::kClassWord, 0);
    req.SetU(pes::moveword::kPrevClassWord, 2);
    req.SetF(pes::moveword::kDwellLimit2, 1.0f);
    req.SetF(pes::moveword::kDwellElapsed2, 0.50f);
    req.SetF(pes::moveword::kRecordStampA, 10.0f);
    req.SetF(pes::moveword::kRecordStampB, 110.0f);  // the heading error is 100 deg
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
  req.SetF(pes::moveword::kRecordStampB, 90.0f);  // the heading error is 80 deg, inside the gate
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 12.0f);
  RunToStep(host, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
}

// -------------------------------------------------------------------------------------
// STEP_LOOP, FUN_1406c4c10: the STEP_TO_RUN exit.
// -------------------------------------------------------------------------------------

// PES at 0x1406C567D-0x1406C592A: with the CLASS WORD at 0, the precise word clear, the turn
// (req[0x06]) and the body turn (wrap180(req[0x04] - req[0x02]) at 0x1406C55C5) inside the
// 112.5 / 67.5 / 90 window, the handler re-issues the clip through FUN_1406BA2F0's style and
// asks for STEP_TO_RUN (req[0x39] = 0xD at 0x1406C592A). The port never asked for STEP_TO_RUN
// at all, so the state was unreachable.
TEST(StepLoop, AsksForStepToRunWhenTheStyleChangesOnANonPreciseStep) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 1);
  req.SetU(pes::moveword::kPrecise, 0u);
  req.SetU(pes::moveword::kClassWord, 0);  // the arm's gate at 0x1406C55D2
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  // The player's own displacement points at 0 degrees, so the ground speed is the standing one
  // and the stop helper does not fire.
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoop(host, req);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(MoveState::kStepToRun));
}

// The arm's class gate: with the class word at 1 the arm does not fire, so the tick falls
// through to the STEP_LOOP_CHANGE tail instead.
TEST(StepLoop, TheStepToRunArmNeedsTheClassWordAtZero) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 1);
  req.SetU(pes::moveword::kPrecise, 0u);
  req.SetU(pes::moveword::kClassWord, 1);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoop(host, req);
  EXPECT_NE(req.U(pes::moveword::kNextState), uint32_t(MoveState::kStepToRun));
}

// The arm's window: the turn is 100 degrees and the body turn is 30 degrees, so the sum is
// past 112.5 and the turn is past 67.5 - the arm does not fire, and the tick falls through to
// the STEP_LOOP_CHANGE tail instead.
TEST(StepLoop, TheStepToRunArmNeedsTheTurnInsideTheWindow) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 1);
  req.SetU(pes::moveword::kPrecise, 0u);
  req.SetU(pes::moveword::kClassWord, 1);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 40.0f);  // the body turn is 30 degrees
  req.SetF(pes::moveword::kTurn, 100.0f);          // and the request's turn is 100 degrees
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoop(host, req);
  // The arm did not fire: the state is not STEP_TO_RUN.
  EXPECT_NE(req.U(pes::moveword::kNextState), uint32_t(MoveState::kStepToRun));
}

// -------------------------------------------------------------------------------------
// STEP_LOOP, FUN_1406c4c10: the body turn and the stamp it writes.
// -------------------------------------------------------------------------------------

// PES at 0x1406C4FB9: the body turn is FUN_14043a0a0(turned, req[0x01]) =
// wrap180(kHeading - turned), and the clamp at 0x1406C4FBE-0x1406C4FD8 holds it inside
// [-90, 90]. The port computed the difference the other way round and zeroed the
// out-of-range values instead of clamping them.
TEST(StepLoop, TheBodyTurnRunsFromTheHeadingTowardsTheFacing) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kFacing, 0.0f);
  // The bucket 0 sits at 0 degrees, so the turned heading is 0 and a heading of 315 gives PES's
  // body turn of -45. A negative body turn takes the port's `bodyTurn <= 0` arm, which keeps
  // the bucket 0 or 5 and re-picks it from the wide turn; a positive one (the port's old
  // operand) skips that arm and leaves the angle-bucket answer, which is 1.
  req.SetF(pes::moveword::kHeading, 315.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoop(host, req);
  // The negative body turn took the `bodyTurn <= 0` arm, which kept the bucket at 0.
  EXPECT_EQ(req.U(pes::moveword::kMoveBucket), 0u);
}

// PES at 0x1406C4E7A: the bucket's look test reads
// FUN_14043a0a0(req[0x03], req[0x01]) = wrap180(kHeading - kLookHeading), and the sign picks
// the bucket's half (0/5 against 4/9). The port computed the difference the other way round.
TEST(StepLoop, TheBucketHalfFollowsTheHeadingAgainstTheLookHeading) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  // The look heading is 20 degrees below the heading, so PES's difference is +20 and the
  // bucket stays in the upper half (0 or 5).
  req.SetF(pes::moveword::kLookHeading, 340.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 5);
  req.SetU(pes::moveword::kBucketPrev, 5);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoop(host, req);
  // The positive difference re-picked the bucket 0; the port's old sign would have kept 5.
  EXPECT_EQ(req.U(pes::moveword::kMoveBucket), 0u);
}

// PES's STEP_TURN_RUN arm (0x1406C5CFE-0x1406C5D2B): the class is clamp(req[0x09], 0, 2), the
// speed band is the desired speed clamped into [5, 11] (or 5 when it is already at or below
// 16), the class word and its speed are carried over, the row's own word 0x20 is copied into
// word 0x25 and then zeroed, and the live kind is the request's own. The port wrote none of
// them, and the arm was unreachable because the band gate was inverted.
TEST(StepLoop, TheStepTurnRunArmCarriesTheClassAndTheRowOver) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kPrecise, 0u);
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetU(0x20, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 60.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  // The distance is past 4 metres, so PES's distance term is closed and the band gate's
  // fall-through (the STEP_TURN_RUN arm) runs.
  req.SetF(pes::moveword::kDistToTarget, 9.0f);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoop(host, req);
  // The tail asks for STEP_TURN_RUN, and the class and the row's own word follow.
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(MoveState::kStepTurnRun));
  EXPECT_EQ(req.U(pes::moveword::kClassWord), 2u);
  EXPECT_EQ(req.U(pes::moveword::kBucketPrev), 1u);
  EXPECT_EQ(req.U(0x20), 0u);
}

// PES's STEP_TURN_RUN arm writes the root facing (0x1406C5EDF-0x1406C5F31 and the block
// after it): the turn to the body's own direction, clamped against the clock's draw
// (180 / the frame rate, 54.0), is added to the player's facing, and that becomes req[0x3b].
// The port computed the values and discarded them, so the facing never moved.
TEST(StepLoop, TheStepTurnRunArmWritesTheRootFacing) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kPrecise, 0u);
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetU(0x20, 1u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 60.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  // The body direction is 30 degrees from the heading, so the turn is -30 and the delta is the
  // negated draw (-3.33): the facing lands on 356.67.
  req.SetF(pes::moveword::kBodyDirection, 30.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  req.SetU(pes::moveword::kBucketPrev, 0);
  req.SetF(pes::moveword::kDistToTarget, 9.0f);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoop(host, req);
  // The root facing is the player's own facing (0) plus the delta.
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 356.66666f, 0.01f);
}

// -------------------------------------------------------------------------------------
// STEP_LOOP_CHANGE, FUN_1406c6480.
// -------------------------------------------------------------------------------------

// PES at 0x1406C6632: the turn-to-target is FUN_14043a0a0(req[0x18], req[0x01]) =
// wrap180(kHeading - kRecordStampB). The drift block adds that signed value to the blended
// root facing, so the sign decides which way the body turns. The port had it the other way
// round.
TEST(StepLoopChange, TheDriftFollowsTheSignedTurnToTarget) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetF(pes::moveword::kFacing, 100.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 100.0f);
  // The stamp is 30 degrees behind the heading, so wrap180(heading - stamp) = +30.
  req.SetF(pes::moveword::kRecordStampB, 330.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);  // the stop fallback's own turn gate stays closed
  req.SetU(pes::moveword::kClassWord, 2u);  // the band is up, so the re-plan gates stay closed
  req.SetU(pes::moveword::kPrecise, 1u);  // the precise flag keeps the non-precise arms closed
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kRecordStampA, 0.0f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetU(pes::moveword::kBucketPrev, 0);
  StepLoopChange(host, req);
  // The drift block ran: the root facing is the blended turn (0) plus the facing (100) plus the
  // clock's draw. The turn (30 degrees) is past the draw (180 / 54 = 3.33), so the delta IS the
  // draw - 103.33, not the turn.
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 103.33333f, 1e-3f);
}

// PES at 0x1406C6EF0: the final gate tests the FIRST dwell (req[0x1a] against req[0x1b]) -
// the second dwell's pair is only the window for the new limit. The port tested the second
// dwell's pair, so the handler left for STEP_LOOP on the wrong clock.
TEST(StepLoopChange, TheFinalGateWaitsForTheFirstDwell) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kRecordStampB, 10.0f);
  // The second dwell has run out but the first has not, so the handler keeps the tick.
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetU(pes::moveword::kBucketPrev, 0);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoopChange(host, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
}

// The same request with the first dwell also run out: the handler asks for STEP_LOOP and
// gives the first dwell a 0.05 limit (the second dwell is past four tenths of its limit).
TEST(StepLoopChange, TheFinalGateAsksForStepLoopOnceTheFirstDwellRunsOut) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kRecordStampB, 10.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetU(pes::moveword::kBucketPrev, 0);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoopChange(host, req);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(MoveState::kStepLoop));
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit), 0.0f);
}

// PES at 0x1406C6CEB: when the turn-to-target is past the clock's draw (180 degrees per tick),
// the delta added to the drift IS the draw; otherwise it is the larger of the negated draw and
// the turn itself. The port kept the turn in both cases.
TEST(StepLoopChange, TheDriftClampUsesTheDrawWhenTheTurnIsPastIt) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  // The precise flag is set, so the drift's sign test can return early: the look heading is
  // level with the facing, so the signs match and the write happens.
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 340.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  // The turn-to-target is 20 degrees: past the 5.625-degree gate and past the draw
  // (180 / 54 = 3.33 degrees), so the delta IS the draw.
  req.SetF(pes::moveword::kRecordStampB, 340.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  req.SetF(pes::moveword::kRecordStampA, 0.0f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetU(pes::moveword::kBucketPrev, 0);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.02f;
  StepLoopChange(host, req);
  // The drift is the blended turn (0) plus the facing (0) plus the draw (3.33 degrees) - the
  // turn (20 degrees) is past the draw, so the delta IS the draw.
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 3.3333333f, 0.01f);
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

// PES FUN_1406D2590 takes its clip's row from the stop-row selector FUN_1406B9D80, through the +0xac0 interface's slot 0xd0
// (decompile 285-293). The row is DAT_1425DDAD0[kind + class * 8], with the class the class word req[0x1f] clamped to 0..3
// (lines 74-82) and the action the command id req[0x16] (line 290). The bucket is folded 4 -> 3 and 9 -> 8 first (279-284).
namespace {
void StopSelectorRequest(pes::MoveReq& req, int classWord, int command, int lookBucket) {
  req.SetU(pes::moveword::kKind, 3u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kClassWord, static_cast<uint32_t>(classWord));
  req.SetU(pes::moveword::kCommandId, static_cast<uint32_t>(command));
  req.SetU(pes::moveword::kLookBucket, static_cast<uint32_t>(lookBucket));
  req.SetU(pes::moveword::kLookBucketAlt, static_cast<uint32_t>(lookBucket));
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDistToTarget, 5.0f);
  req.SetF(pes::moveword::kDistToLook, 5.0f);
  req.SetF(pes::moveword::kRate, 0.3f);
}
}  // namespace

TEST(EnterTurnStop, TheRowIsTheStopSelectorsRowForTheClassWord) {
  FakeHost host;
  host.currentKmh = 0.0f;
  host.runTables.stopRow[3 + 1 * 8] = 0x5ab;  // kind 3, class 1
  pes::MoveReq req;
  StopSelectorRequest(req, /*classWord=*/1, /*command=*/0, /*lookBucket=*/0);
  EnterTurnStop(host, req);
  EXPECT_EQ(host.lastRow, 0x5ab) << "DAT_1425DDAD0[kind + class * 8], not the step grid";
}

TEST(EnterTurnStop, TheClassTwoRowIsTheTurnLockRowUnlessTheCommandIsExempt) {
  FakeHost host;
  host.currentKmh = 0.0f;
  host.runTables.stopRow[3 + 2 * 8] = 0x5ac;  // kind 3, class 2
  pes::MoveReq req;
  StopSelectorRequest(req, /*classWord=*/2, /*command=*/0, /*lookBucket=*/0);
  EnterTurnStop(host, req);
  EXPECT_EQ(host.lastRow, 0x582) << "FUN_1406B9D80 lines 17-18: class 2 forces row 0x582";
  pes::MoveReq exempt;
  StopSelectorRequest(exempt, /*classWord=*/2, /*command=*/0x11, /*lookBucket=*/0);
  EnterTurnStop(host, exempt);
  EXPECT_EQ(host.lastRow, 0x5ac) << "command ids 1, 0x11 and 0x70 keep the table's row (lines 16-17)";
}

TEST(EnterTurnStop, TheClassWordThreeReadsTheStopTableBeyondItsTwentyFourRows) {
  FakeHost host;
  host.currentKmh = 0.0f;
  host.runTables.stopRow[3 + 3 * 8] = 0x5bc;  // kind 3, class 3: index 27, past the 24 rows the notes list
  pes::MoveReq req;
  StopSelectorRequest(req, /*classWord=*/3, /*command=*/0, /*lookBucket=*/0);
  EnterTurnStop(host, req);
  EXPECT_EQ(host.lastRow, 0x5bc) << "the class word reaches 3 (decompile line 81); the table has 32 entries";
}

TEST(EnterTurnStop, TheStopColumnIsTheFoldedBucketsColumn) {
  FakeHost host;
  host.currentKmh = 0.0f;
  pes::MoveReq req;
  StopSelectorRequest(req, /*classWord=*/1, /*command=*/0, /*lookBucket=*/4);
  EnterTurnStop(host, req);
  EXPECT_EQ(host.lastColumn, 5) << "bucket 4 folds to 3 before the selector (lines 279-284): DAT_1425DD988[3]";
}

// PES's cell lookup answers -1 for an empty cell (TeamDecode 3). The TURN_STOP entry issued the row id when its step cell was
// empty; the cell's own motion goes out, -1 when empty.
TEST(EnterTurnStop, AnEmptyStepCellIssuesMinusOneNotTheRowId) {
  FakeHost host;
  host.currentKmh = 0.0f;
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
  host.cellMotion = -1;     // the step cell is empty
  host.issued.motion = -7;  // a sentinel: stays -7 if the entry issued nothing
  EnterTurnStop(host, req);
  EXPECT_EQ(host.issued.motion, -1) << "an empty step cell's motion is -1, never the row id";
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

// PES at 0x1406CAD06: the turn-to-target is FUN_14043a0a0(req[0x02], req[0x01]) =
// wrap180(kHeading - kFacing); the magnitude of that result is the first path's gate against
// 22.5 degrees. The port used the player's displacement heading instead of the request's
// heading, so the gate read a different angle.
TEST(TurnStop, TheFirstPathGateReadsTheRequestHeadingAgainstTheFacing) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kRate, 1.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  // The request's heading is 30 degrees from the facing, so PES's gate (30 >= 22.5) fails.
  req.SetF(pes::moveword::kHeading, 40.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.10f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.10f);
  // The displacement points at 0 degrees, so the port's old operand (the displacement heading
  // against the facing) was 10 degrees and would have passed the gate.
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 1.0f;
  // The stay decision agrees, so the first path's inner gate would open if the gate passed.
  stayDecision = true;
  TurnStop(host, req);
  stayDecision = false;
  // The gate failed, so the first path did not run: the dwell words are untouched.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit), 1.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit2), 1.0f);
}

// PES at 0x1406CB262: with the displacement turn past 22.5 degrees (0x14259D618) the
// non-precise arm jumps to the class ladder at 0x1406CB0B6 - the class word and its speed
// from the ground speed - and hands the tick to FUN_1406D38D0 (STEP_TURN_RUN). The port's
// gate had an empty body, so the ladder never ran and the run-turn block took the tick
// instead.
TEST(TurnStop, TheWideDisplacementTurnRunsTheClassLadder) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kPrecise, 0u);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 12.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.5f);
  // The bucket is not 0 or 5, so the arm runs.
  req.SetU(pes::moveword::kMoveBucket, 3);
  // Word 0x0f is clear, so the run-turn block (which needs it) cannot run: only the class
  // ladder can write the class word and the speed.
  req.SetU(0x0f, 0u);
  // The displacement points at 25 degrees, so the gate's angle - wrap180(the displacement
  // heading - the facing) - is 25 degrees: past PES's 22.5-degree gate and inside the
  // 28.125-degree one. The displacement is 10 units long, so the ground speed is under
  // 8 km/h and the ladder writes the class 0 and the 5.0 speed.
  host.displacement[0] = 4.2262f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 9.0631f;
  TurnStop(host, req);
  EXPECT_EQ(req.U(pes::moveword::kClassWord), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kBandSpeed), 5.0f);
}

// The request's heading level with the facing, so PES's gate passes, while the displacement
// heading is 60 degrees away - the port's old operand would have failed the gate.
TEST(TurnStop, TheFirstPathGateAcceptsALevelRequestHeading) {
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
  req.SetF(pes::moveword::kDwellElapsed2, 0.10f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.10f);
  // The displacement points at 60 degrees, so the port's old operand was 50 degrees.
  host.displacement[0] = 0.8660254f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.5f;
  stayDecision = true;
  TurnStop(host, req);
  stayDecision = false;
  // The gate passed, so the first path wrote the step clip and cleared the dwell words.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit), 0.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit2), 0.0f);
}

// PES FUN_1406cab20's first path issues FUN_1406ABB30's cell: the row DAT_1425dd9b0[kind + band * 8] and the column
// DAT_1425dd988[the move bucket] (decompile 0x1406CB35D-0x1406CB46A). The port issued the foot band as the motion, a value
// the humanoid refuses every tick, so a standing taker's turn-back clip never played.
TEST(TurnStop, TheFirstPathIssuesTheStepCellNotTheFootBand) {
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
  req.SetF(pes::moveword::kDwellElapsed2, 0.10f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.10f);
  host.displacement[0] = 0.8660254f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.5f;
  host.footBand = 2;
  host.cellMotion = 0x3c5;  // the populated step cell
  stayDecision = true;
  TurnStop(host, req);
  stayDecision = false;
  EXPECT_EQ(host.lastRow, 0x540 + 2) << "the row is DAT_1425dd9b0[kind 2 + band 0 * 8]";
  EXPECT_EQ(host.lastColumn, 9) << "the column is DAT_1425dd988[move bucket 0]";
  EXPECT_EQ(host.issued.motion, 0x3c5) << "the issued motion is the step cell's, not the foot band";
}

// PES's cell lookup answers -1 for an empty cell (FUN_1406ABB30; TeamDecode 3). TURN_STOP's third path looked the step
// cell up and issued the row id when it was empty, a motion the humanoid cannot play. The sentinel shows the path issued.
TEST(TurnStop, AnEmptyStepCellIssuesMinusOneNotTheRowId) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 2);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kPrecise, 1);
  req.SetF(pes::moveword::kRate, 1.0f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetF(pes::moveword::kDistToTarget, 5.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 5.0f);  // past the first and second paths' gates
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 5.0f);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 1.0f;  // heading 0: the third path's gates read the facing error as 0
  host.cellMotion = -1;         // the step cell is empty
  host.issued.motion = -7;      // a sentinel: stays -7 if the path issued nothing
  TurnStop(host, req);
  EXPECT_EQ(host.issued.motion, -1) << "the empty cell's motion is -1, never the row id";
}

// TURN_STOP's stop (decompile block 5, the stop's own motion row IDLE) issues the IDLE row's cell at column 9, as STOP's IDLE
// arm reads it. The port issued the left/right foot pick (0 or 1) as the motion, which the humanoid refuses.
TEST(TurnStop, TheStopIssuesTheIdleCellNotTheFootPick) {
  FakeHost host;
  MoveReq req;
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 0);
  req.SetU(pes::moveword::kClassWord, 0);
  req.SetU(pes::moveword::kPrecise, 0);
  req.SetF(pes::moveword::kRate, 0.3f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  // 30 degrees off the facing: past the first path's 22.5-degree gate, inside the stop's 67.5-degree mirror window.
  req.SetF(pes::moveword::kHeading, 30.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetF(pes::moveword::kDwellLimit, 100.0f);
  req.SetF(pes::moveword::kDwellElapsed, 80.0f);  // past the stop's 0.6 gate, and past the first path's clock limit
  req.SetF(pes::moveword::kDwellLimit2, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  host.displacement[0] = 0.5f;  // heading 30 degrees
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.8660254f;
  host.cellMotion = 0x3c5;      // the IDLE cell's motion
  host.issued.motion = -7;      // a sentinel: stays -7 if the stop issued nothing
  stayDecision = true;
  TurnStop(host, req);
  stayDecision = false;
  EXPECT_EQ(host.issued.motion, 0x3c5) << "the stop's clip is the IDLE cell's motion, not the foot pick";
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
int LocomotionStyle(MoveHost&, int, float, MoveReq&, float, bool) { return 0; }
int IdleRow(MoveHost&, MoveReq&) { return 0x5a1; }
bool StayDecision(MoveHost&, MoveReq&) { return stayDecision; }
bool OutOfPlayPreempt(MoveHost&, MoveReq&) { return false; }
bool EnterAccelerate(MoveHost&, MoveReq&) { return false; }
void EnterGotoRun(MoveHost&, MoveReq&, bool) { requested = MoveState::kGotoRun; }
void EnterStop(MoveHost&, MoveReq&) { requested = MoveState::kStop; }
void EnterRunTurnRun(MoveHost&, MoveReq&) { requested = MoveState::kRunTurnRun; }
bool EnterRunTurnStep(MoveHost&, MoveReq&, float) { return false; }
bool EnterStepTurnRun(MoveHost&, MoveReq&) { return false; }
}  // namespace movestep
}  // namespace pes

// FUN_1406b6ee0, the stop gate with flag 0 (decompile 0x1406b6ee0): the move bucket picks the rule and the class
// picks the look-distance gate (blend2 0x142598578 or blend04 0x14259b89c below class 1; blend04 or rate07
// 0x14257a194 above). Words are the request's own: req[8] bucket, req[0x1f] class, req[0x12] look distance,
// req[6] turn, req[7] facing error. Built on the group-C body, these read the wrong words and a 1.6 gate.
TEST(MoveStepStops, FollowsTheMoveBucketGatesOfFunction1406b6ee0) {
  FakeHost h;
  h.runConstants.deg1125 = 11.25f;
  h.runConstants.deg90 = 90.0f;
  h.runConstants.blend2 = 0.2f;
  h.runConstants.blend04 = 0.4f;
  h.runConstants.rate07 = 0.7f;
  pes::MoveReq r;
  r.SetU(pes::moveword::kCommandSpeedClass, 2);
  r.SetU(pes::moveword::kClassWord, 2);
  r.SetU(pes::moveword::kMoveBucket, 2);
  r.SetF(pes::moveword::kDistToLook, 0.9f);  // above class 2's gate of 0.7
  r.SetF(pes::moveword::kFacingError, 50.0f);
  EXPECT_FALSE(pes::movestep::StepStops(h, r, false)) << "a 50-degree facing error does not stop";
  r.SetF(pes::moveword::kFacingError, 100.0f);
  EXPECT_TRUE(pes::movestep::StepStops(h, r, false)) << "past 90 degrees the stop is taken";

  // Class 0's gate is 0.4: a look distance of 0.5 is above it, so the facing error decides again.
  r.SetU(pes::moveword::kClassWord, 0);
  r.SetF(pes::moveword::kDistToLook, 0.5f);
  EXPECT_TRUE(pes::movestep::StepStops(h, r, false));

  // Bucket 1 with a turn past 11.25 degrees: class 0's gate is 0.2, above the look distance of 0.1.
  r.SetU(pes::moveword::kMoveBucket, 1);
  r.SetF(pes::moveword::kTurn, 20.0f);
  r.SetF(pes::moveword::kDistToLook, 0.1f);
  EXPECT_TRUE(pes::movestep::StepStops(h, r, false));

  // Bucket 0 never stops on the gate.
  r.SetU(pes::moveword::kMoveBucket, 0);
  EXPECT_FALSE(pes::movestep::StepStops(h, r, false));
}

// FUN_1406b70d0, buckets 2 and 7: the gate is the class's back distance, scaled between 67.5 and 112.5 degrees (full
// scale past 112.5), and the result is whether the turn is past 101.25 degrees (VA 0x1425ddfac). A 20-degree turn with
// the target inside the gate is not a run turn.
TEST(MoveStepLetsRunTurn, BucketTwoGateReturnsWhetherTheTurnIsPast101Point25) {
  FakeHost h;
  h.runConstants.deg675 = 67.5f;
  h.runConstants.deg1125b = 112.5f;
  h.runConstants.deg45 = 45.0f;
  h.runConstants.deg90 = 90.0f;
  h.runConstants.blend04 = 0.4f;
  h.runConstants.rate07 = 0.7f;
  h.turnConstants.turn101250 = 101.25f;
  h.tables.turnBack = {1.0f, 1.0f, 1.0f, 1.0f};
  h.tables.turnCentre = {0.5f, 0.5f, 0.5f, 0.5f};
  h.tables.turnFront = {0.3f, 0.3f, 0.3f, 0.3f};
  pes::MoveReq r;
  r.SetU(pes::moveword::kCommandSpeedClass, 2);
  r.SetU(pes::moveword::kClassWord, 2);
  r.SetU(pes::moveword::kMoveBucket, 2);
  r.SetF(pes::moveword::kTurn, 20.0f);
  r.SetF(pes::moveword::kDistToTarget, 2.0f);
  r.SetF(pes::moveword::kDistToLook, 0.5f);
  r.SetF(pes::moveword::kRate, 0.5f);
  EXPECT_FALSE(pes::movestep::StepLetsRunTurn(h, r)) << "a 20-degree turn is not a run turn";
  r.SetF(pes::moveword::kTurn, 120.0f);
  EXPECT_TRUE(pes::movestep::StepLetsRunTurn(h, r)) << "past 101.25 degrees it is";
}

// FUN_1406b70d0, buckets 1 and 6: the gate is the class's front distance plus a scale that is 0 up to 33.75 degrees,
// rises to 1 at 67.5 and stays 1 past it (PES starts its scale at 1.0). Inside the gate the command flag decides.
TEST(MoveStepLetsRunTurn, BucketOneGateTakesTheFullScalePastSixtySevenPoint5) {
  FakeHost h;
  h.runConstants.deg3375 = 33.75f;
  h.runConstants.deg675 = 67.5f;
  h.tables.turnFront = {0.3f, 0.3f, 0.3f, 0.3f};
  pes::MoveReq r;
  r.SetU(pes::moveword::kCommandSpeedClass, 2);
  r.SetU(pes::moveword::kClassWord, 0);
  r.SetU(pes::moveword::kMoveBucket, 1);
  r.SetByte(pes::moveword::kTurnCommandFlag, 1);
  r.SetF(pes::moveword::kDistToTarget, 1.2f);
  r.SetF(pes::moveword::kDistToLook, 1.2f);
  r.SetF(pes::moveword::kTurn, 20.0f);
  EXPECT_FALSE(pes::movestep::StepLetsRunTurn(h, r)) << "at 20 degrees the gate is the front distance, 0.3 m";
  r.SetF(pes::moveword::kTurn, 80.0f);
  EXPECT_TRUE(pes::movestep::StepLetsRunTurn(h, r)) << "at 80 degrees the gate is 1.3 m and the flag answers";
}

// FUN_1406b6ee0's first gate: with a class word at 0 or above and the command's speed class at -1, the step stops
// (decompile 0x1406b6ee0 line 16: `-1 < req[0x1f] && req[9] == -1`).
TEST(MoveStepStops, AMissingCommandClassStopsAStepThatHasAClass) {
  FakeHost h;
  pes::MoveReq r;
  r.SetU(pes::moveword::kClassWord, 0);
  r.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  EXPECT_TRUE(pes::movestep::StepStops(h, r, false));
}
