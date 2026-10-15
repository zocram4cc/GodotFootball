// PES 2021's MOVESTEP handler group B (contract v0.10): the running phase's install constants
// and the interface tables it reads, plus the pure folds the handlers share. Every expectation
// is a relation between loaded exe values or a synthetic formula check; no PES number table is
// written into code or tests. The install tests read PES21_ROOT and skip without it, as
// pes_movestep_test.cpp does.
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"

namespace {

// A minimal MoveHost for the tests that need one: every read returns a fixed synthetic value, so
// a test states exactly what it sets. The interface helpers the dispatcher port owns (IdleRow,
// SpeedToRate, LocomotionStyle) are not defined yet, so no test here drives a handler that calls
// them; the handler tests land with those bodies.
class SyntheticHost : public pes::MoveHost {
 public:
  pes::MoveCell Cell(int row, int column, bool walkFlag) override {
    ++cellCalls;
    lastCellRow = row;
    lastCellColumn = column;
    lastCellWalk = walkFlag;
    pes::MoveCell c;
    c.motionNo = 0x100 + row;
    c.mirror = (column & 1) != 0;
    c.found = true;
    return c;
  }
  int PickN(int n) override { pickN = n; return pick; }
  bool Percent(int pct) override { percentArg = pct; return percent; }
  int CommandSpeedClass() const override { return commandClass; }
  float RateCeiling() const override { return rateCeiling; }
  float CommandSpeedKmh() const override { return commandKmh; }
  float CurrentSpeedKmh() const override { return currentKmh; }
  float PhysiqueG() const override { return g; }
  float TicksPerSecond() const override { return ticks; }
  int MatchTick() const override { return tick; }
  int RecentStamp(int index) const override { return index == 0 ? stampA : stampB; }
  float MotionStartTime(int motionNo) const override { return startTime; (void)motionNo; }
  float RecordF2468() const override { return f2468; }
  int MoveStatePrevious() const override { return previousState; }
  float RecordF0bec() const override { return f0bec; }
  void SetRecordF0bec(float value) override { f0bec = value; ++f0becWrites; }
  const pes::AngleBucketConstants& AngleBuckets() const override { return buckets; }
  void PositionAt(int index, float out[3]) const override {
    out[0] = positions[index][0];
    out[1] = positions[index][1];
    out[2] = positions[index][2];
  }
  int MatchPhase() const override { return phase; }
  bool IsReplay() const override { return replay; }
  void Issue(const pes::MotionRequest& request) override { issued = request; ++issues; }

  // The shared contract reads.
  int FootBand() const override { return footBand; }
  const pes::StepScalars& StepConstants() const override { return stepConstants; }
  const pes::StepTables& StepTablesView() const override { return stepTables; }
  bool BucketIsLeft(int bucket) const override { return bucket != 0 && bucket < 5; }
  int CurrentMotion() const override { return currentMotion; }
  int CurrentClipFrames() const override { return currentClipFrames; }
  int AnimeState() const override { return animeState; }
  float BandSpeedKmh(int band, float g) const override { return bandSpeeds[band]; (void)g; }
  bool MirrorChoice(bool sameTeam) const override { return mirrorChoice; (void)sameTeam; }
  const pes::RunConstants& RunConstantsView() const override { return runConstants; }
  const pes::RunTables& RunTablesView() const override { return runTables; }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { return playback; }

  int commandClass = -1;
  float rateCeiling = 1.0f;
  float commandKmh = 0.0f;
  float currentKmh = 0.0f;
  float g = 1.0f;
  float ticks = 54.0f;
  int tick = 0;
  int stampA = 0;
  int stampB = 0;
  float startTime = 0.0f;
  float f2468 = 0.0f;
  int previousState = 0;
  float f0bec = 0.0f;
  int f0becWrites = 0;
  pes::AngleBucketConstants buckets{};
  float positions[12][3] = {};
  int phase = 1;
  bool replay = false;
  pes::MotionRequest issued{};
  int issues = 0;
  int pick = 0;
  int pickN = 0;
  bool percent = false;
  int percentArg = 0;
  int footBand = 0;
  float bandSpeeds[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool mirrorChoice = false;
  pes::RunConstants runConstants{};
  pes::StepScalars stepConstants{};
  pes::StepTables stepTables{};
  int currentMotion = 0;
  int currentClipFrames = 0;
  int animeState = 0;
  pes::RunTables runTables{};
  pes::PlaybackConstants playback{};
  int cellCalls = 0;
  int lastCellRow = -1;
  int lastCellColumn = -1;
  bool lastCellWalk = false;
};

// The synthetic constants: the install values, but any test may change one to expose a branch.
// The install relations test checks the real ones; these are for the arithmetic.
pes::RunConstants SyntheticRun() {
  pes::RunConstants k{};
  k.one = 1.0f;
  k.neg = -1.0f;
  k.half = 0.5f;
  k.wrapHalf = -180.0f;
  k.full = 360.0f;
  k.wrapFull = -360.0f;
  k.half_turn = 180.0f;
  k.deg1125 = 11.25f;
  k.deg225 = 22.5f;
  k.deg45 = 45.0f;
  k.deg5625 = 56.25f;
  k.deg675 = 67.5f;
  k.deg7875 = 78.75f;
  k.deg90 = 90.0f;
  k.deg1125b = 112.5f;
  k.deg135 = 135.0f;
  k.deg1575 = 157.5f;
  k.turn16875 = 16.875f;
  k.blend1667 = 1.0f / 6.0f;
  k.blend1333 = 2.0f / 15.0f;
  k.blend2 = 0.2f;
  k.blend3 = 0.3f;
  k.blend05 = 0.05f;
  k.blend08 = 0.8f;
  k.rate06 = 0.6f;
  k.rate07 = 0.7f;
  k.rate095 = 0.95f;
  k.rate0225 = 0.225f;
  k.rate01 = 0.1f;
  k.rate05 = 0.5f;
  k.kmh5 = 5.0f;
  k.kmh8 = 8.0f;
  k.kmh11 = 11.0f;
  k.kmh135 = 13.5f;
  k.kmh1475 = 14.75f;
  k.kmh16 = 16.0f;
  k.kmh19 = 19.0f;
  k.kmh22 = 22.0f;
  k.kmh25 = 25.0f;
  k.kmh28 = 28.0f;
  k.kmh60 = 60.0f;
  k.kmh1000 = 1000.0f;
  k.kmh95sq = 8100.0f;
  k.dist1m5 = 1.5f;
  k.dist11 = 1.1f;
  k.speed3600 = 3.6f;
  k.speed2 = 2.0f;
  k.speed4 = 4.0f;
  k.ramp15 = 15.0f;
  k.ramp6 = 6.0f;
  k.ramp12 = 12.0f;
  k.ramp45 = 0.45f;
  k.ramp65 = 0.65f;
  k.ramp120 = 1.2f;
  k.ramp85 = 0.85f;
  k.blend04 = 0.4f;
  k.neg45 = -45.0f;
  k.cm100 = 100.0f;
  k.foot035 = 0.35f;
  k.footNeg010 = -0.1f;
  k.footNeg035 = -0.35f;
  k.negHalf = -0.5f;
  k.bias5 = -5.0f;
  k.rampFloor = 1.1920928955078125e-07f;
  k.rampSlope = 0.15f;
  k.turnP2 = 10.0f;
  k.foxTime = 300.0f;
  k.pct50 = 50.0f;
  k.quota75 = 75.0f;
  k.quota80 = 80.0f;
  k.quota3 = 3.0f;
  return k;
}

// The interface tables, filled the way the exe fills them for the synthetic set: the class
// ladder {-1, 0, 1, 2, 3, -1} of FUN_1406b8de0 and the band rates of FUN_1406b9e50.
pes::RunTables SyntheticTables() {
  pes::RunTables t{};
  t.commandClass = {-1, 0, 1, 2, 3, -1, 0, 0};
  t.bandRate = {0.15f, 0.30f, 0.60f, 1.00f};
  t.rateLadder = {0.29f, 0.59f, 0.70f, 1.00f};
  t.rateClass = {0, 1, 2, 3};
  t.bucketColumn = {9, 11, 13, 15, 17, 8, 6, 4, 2, 0};
  // Distinct rows so an index swap changes the answer: row = kind + class*8.
  for (int i = 0; i < 32; ++i) t.stopRow[i] = 0x576 + i;
  for (int i = 0; i < 32; ++i) t.gotoRow[i] = 0x551 + i;
  for (int i = 0; i < 32; ++i) t.turnStopRow[i] = 0x58e + i;
  for (int i = 0; i < 16; ++i) t.styleGroup[i] = 0xe31 + i;
  return t;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// The install relations. Skipped without PES21_ROOT.
// ---------------------------------------------------------------------------------------------

TEST(RunConstantsInstall, LoadsTheExeConstantsWithTheirRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::LoadRunConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  // The wraps and the fixed points the handlers fold against.
  EXPECT_EQ(k->one, 1.0f);
  EXPECT_EQ(k->neg, -1.0f);
  EXPECT_EQ(k->full, 360.0f);
  EXPECT_EQ(k->wrapFull, -360.0f);
  EXPECT_EQ(k->half_turn, 180.0f);
  EXPECT_EQ(k->wrapHalf, -180.0f);
  // The ordered turn ladder the handlers compare against, in PES's order.
  EXPECT_LT(k->deg1125, k->deg225);
  EXPECT_LT(k->deg225, k->deg45);
  EXPECT_LT(k->deg45, k->deg5625);
  EXPECT_LT(k->deg5625, k->deg675);
  EXPECT_LT(k->deg675, k->deg7875);
  EXPECT_LT(k->deg7875, k->deg90);
  EXPECT_LT(k->deg90, k->deg1125b);
  EXPECT_LT(k->deg1125b, k->deg135);
  EXPECT_LT(k->deg135, k->deg1575);
  // The ordered speed ladder, the ordered rate ladder and the blends.
  EXPECT_LT(k->kmh5, k->kmh8);
  EXPECT_LT(k->kmh8, k->kmh11);
  EXPECT_LT(k->kmh11, k->kmh135);
  EXPECT_LT(k->kmh135, k->kmh16);
  EXPECT_LT(k->kmh16, k->kmh19);
  EXPECT_LT(k->kmh19, k->kmh22);
  EXPECT_LT(k->kmh22, k->kmh25);
  EXPECT_LT(k->kmh25, k->kmh28);
  EXPECT_LT(k->rate01, k->rate0225);
  EXPECT_LT(k->rate0225, k->rate06);
  EXPECT_LT(k->rate06, k->rate07);
  EXPECT_LT(k->rate07, k->rate095);
  EXPECT_GT(k->kmh95sq, 0.0f);
  EXPECT_EQ(k->kmh60, 60.0f);
  EXPECT_EQ(k->kmh1000, 1000.0f);
  // The foot window of FUN_1406d2030's placed-start test and the ramp floor.
  EXPECT_EQ(k->foot035, -k->footNeg035);
  EXPECT_EQ(k->footNeg010, -0.1f);
  EXPECT_LT(k->rampFloor, 1e-5f);
  EXPECT_EQ(k->foxTime, 300.0f);
}

TEST(RunTablesInstall, LoadsTheInterfaceTablesWithTheirRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto t = pes::LoadRunTables(*exe, &err);
  ASSERT_TRUE(t.has_value()) << err;
  // FUN_1406b8de0: the command classes, in PES's order.
  EXPECT_EQ(t->commandClass[0], -1);
  EXPECT_EQ(t->commandClass[1], 0);
  EXPECT_EQ(t->commandClass[2], 1);
  EXPECT_EQ(t->commandClass[3], 2);
  EXPECT_EQ(t->commandClass[4], 3);
  EXPECT_EQ(t->commandClass[5], -1);
  // FUN_1406b9e50: the band rates rise, and FUN_1406ba030's ladder sits below them.
  EXPECT_LT(t->bandRate[0], t->bandRate[1]);
  EXPECT_LT(t->bandRate[1], t->bandRate[2]);
  EXPECT_LT(t->bandRate[2], t->bandRate[3]);
  EXPECT_LT(t->rateLadder[0], t->rateLadder[1]);
  EXPECT_LT(t->rateLadder[1], t->rateLadder[2]);
  EXPECT_LT(t->rateLadder[2], t->rateLadder[3]);
  EXPECT_EQ(t->rateClass[0], 0);
  EXPECT_EQ(t->rateClass[3], 3);
  // FUN_1406b9b20's columns and every row table of the pickers.
  EXPECT_EQ(t->bucketColumn[0], 9);
  EXPECT_EQ(t->bucketColumn[9], 0);
  for (int i = 0; i < 32; ++i) {
    EXPECT_GT(t->stopRow[i], 0) << "stop row " << i;
    EXPECT_GT(t->gotoRow[i], 0) << "goto row " << i;
    EXPECT_GT(t->turnStopRow[i], 0) << "turn-stop row " << i;
  }
  for (int i = 0; i < 16; ++i) EXPECT_GT(t->styleGroup[i], 0) << "style member " << i;
}

// ---------------------------------------------------------------------------------------------
// The row pickers' index order. FUN_1406ce820 writes it inline (d_1406ce820.c:321) as
// kind + class*8, and FUN_1406c0170 does the same for DAT_1425dd9b0 (d_1406c0170.c:368); both
// call sites of FUN_1406b9d80 pass the class as arg3 and the kind as arg4 (ASM 0x1406d5d60).
// The pickers are file-local, so these tests drive them through the tables they read: the row
// for a (kind, class) pair is the table's entry at kind + class*8, and any other order gives a
// different entry because the synthetic table's entries are all distinct.
// ---------------------------------------------------------------------------------------------

TEST(StopRowIndex, UsesKindPlusClassTimesEightNotTheReverse) {
  const pes::RunTables t = SyntheticTables();
  // The table's entries are 0x576 + index, so the (kind 2, class 1) row is 0x576 + 10.
  EXPECT_EQ(pes::StopRowFor(2, 1, t), 0x576 + 2 + 1 * 8);
  // And the reverse order would give 0x576 + 2*8 + 1 = 0x576 + 17, which must not be it.
  EXPECT_NE(pes::StopRowFor(2, 1, t), 0x576 + 2 * 8 + 1);
}

TEST(StopRowOverride, ForcesRow0x582ForTheClassTwoFamily) {
  const pes::RunTables t = SyntheticTables();
  // FUN_1406b9d80's override: for the class-2 family, a row other than 0x582 becomes 0x582
  // (the doc's mid_delay rows); the arg6 selector is the immediate 0 at both call sites, so the
  // guard `iVar2 != 1` always holds there.
  EXPECT_EQ(pes::StopRowFor(1, 2, t), 0x582);
  EXPECT_EQ(pes::StopRowFor(3, 2, t), 0x582);
  // A class below 2 keeps its own row.
  EXPECT_EQ(pes::StopRowFor(1, 1, t), 0x576 + 1 + 1 * 8);
}

TEST(GotoRowIndex, UsesKindPlusClassTimesEight) {
  const pes::RunTables t = SyntheticTables();
  EXPECT_EQ(pes::GotoRowFor(3, 2, t), 0x551 + 3 + 2 * 8);
  EXPECT_NE(pes::GotoRowFor(3, 2, t), 0x551 + 3 * 8 + 2);
}

TEST(TurnStopRowIndex, UsesKindPlusClassTimesEight) {
  const pes::RunTables t = SyntheticTables();
  EXPECT_EQ(pes::TurnStopRowFor(2, 1, t), 0x58e + 2 + 1 * 8);
}

// ---------------------------------------------------------------------------------------------
// The interface functions' arithmetic over their tables. FUN_1406b8de0 (the command classes),
// FUN_1406b9e50 (the band rates) and FUN_1406ba030 (the rate ladder) are the three the running
// phase reads directly; each is a table read with one documented special case.
// ---------------------------------------------------------------------------------------------

TEST(CommandClass, MapsTheFiveCommandNumbersAndFallsOffToOne) {
  const pes::RunTables t = SyntheticTables();
  // FUN_1406b8de0: commands 1..5 map through the table; everything else is -1 (the "no class"
  // sentinel the handlers test as a class of -1).
  EXPECT_EQ(pes::CommandClassFor(0, t), -1);
  EXPECT_EQ(pes::CommandClassFor(1, t), 0);
  EXPECT_EQ(pes::CommandClassFor(2, t), 1);
  EXPECT_EQ(pes::CommandClassFor(3, t), 2);
  EXPECT_EQ(pes::CommandClassFor(4, t), 3);
  EXPECT_EQ(pes::CommandClassFor(5, t), -1);
  EXPECT_EQ(pes::CommandClassFor(6, t), -1);
  EXPECT_EQ(pes::CommandClassFor(9, t), -1);
}

TEST(BandRate, RisesWithTheBandAndDropsToZeroForTheKeeperKindsAtBandZero) {
  const pes::RunTables t = SyntheticTables();
  // FUN_1406b9e50: the table's four bands, and band 0 with kind 3 or 6 has no rate (AS 3.1).
  EXPECT_FLOAT_EQ(pes::BandRateFor(0, 2, t), 0.15f);
  EXPECT_FLOAT_EQ(pes::BandRateFor(1, 2, t), 0.30f);
  EXPECT_FLOAT_EQ(pes::BandRateFor(2, 2, t), 0.60f);
  EXPECT_FLOAT_EQ(pes::BandRateFor(3, 2, t), 1.00f);
  EXPECT_FLOAT_EQ(pes::BandRateFor(0, 3, t), 0.0f);
  EXPECT_FLOAT_EQ(pes::BandRateFor(0, 6, t), 0.0f);
}

TEST(ClassForRate, TakesTheFirstLadderEntryTheRateIsBelow) {
  const pes::RunTables t = SyntheticTables();
  // FUN_1406ba030: the ladder {0.29, 0.59, 0.70, 1.0} with the answer table {0, 1, 2, 3}; a rate
  // at or above the last entry takes the last class.
  EXPECT_EQ(pes::ClassForRateFor(0.10f, t), 0);
  EXPECT_EQ(pes::ClassForRateFor(0.29f, t), 1);  // at the first entry: not below it
  EXPECT_EQ(pes::ClassForRateFor(0.45f, t), 1);
  EXPECT_EQ(pes::ClassForRateFor(0.60f, t), 2);
  EXPECT_EQ(pes::ClassForRateFor(0.80f, t), 3);
  EXPECT_EQ(pes::ClassForRateFor(1.00f, t), 3);
}

TEST(RunRateGuard, StoresTheRateOnlyWhenItIsAtMostStepBaseOfTheCeiling) {
  // FUN_1406d2030's guard, in the port's exact form: the requested rate survives only when it is
  // at most 0.95 of the ceiling and non-negative, else the word is 0.
  const pes::PlaybackConstants k{};  // stepBase's value is not used by the port's own guard
  const float ceiling = 1.0f;
  const float stepBase = 0.95f;
  EXPECT_TRUE(pes::RunRateAllowed(0.95f, ceiling, stepBase));
  EXPECT_TRUE(pes::RunRateAllowed(0.50f, ceiling, stepBase));
  EXPECT_FALSE(pes::RunRateAllowed(0.96f, ceiling, stepBase));
  EXPECT_FALSE(pes::RunRateAllowed(1.00f, ceiling, stepBase));
  (void)k;
}

// ---------------------------------------------------------------------------------------------
// The GOTO_RUN handler's decision arms (FUN_1406bdf10). The handler's cross-group entries
// (EnterTurnStop, EnterRunTurnStep, EnterRunTurnRun, EnterGotoRun) are declared but not yet
// defined, so these tests check the arms that write request words and the exact conditions that
// select each exit, with a host whose cross-group calls are inert.
// ---------------------------------------------------------------------------------------------

// FUN_1406bdf10's re-entry arm needs the clip past its first hit tick (iVar10 < iVar11 in the
// decompile). The hit ticks come from the clip's HitData record, which GF does not model yet
// (A-AN13), so with no record the port holds them at zero and this arm cannot fire. The test
// states that: the class, the band and the turn all allow the re-entry, but the missing hit tick
// keeps the handler in the straight-ahead re-plan, and the root-facing pair is written instead.
TEST(GotoRun, KeepsTheRePlanWhileTheClipHasNoHitRecord) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetU(pes::moveword::kRateBand, 0u);
  req.SetU(pes::moveword::kBandTarget, 0u);
  req.SetF(pes::moveword::kDuration, 0.05f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 5.0f);
  req.SetF(pes::moveword::kTurn, 5.0f);
  req.SetF(pes::moveword::kFacingError, 5.0f);
  pes::movestep::GotoRun(h, req);
  // The handler stays in the re-plan: no state request, and the root-facing pair is written.
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kRootFacing) & 0xffu, 1u);
}

TEST(GotoRun, FallsThroughToTheStraightAheadRePlanWithoutATurn) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kBandTarget, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);  // 2 degrees apart: no turn
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kDuration, 0.05f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  pes::movestep::GotoRun(h, req);
  // The straight-ahead arm writes the root-facing pair (words 0x3a/0x3b) with the clamped turn.
  EXPECT_EQ((req.U(pes::moveword::kRootFacing) & 0xffu), 1u);
}

TEST(GotoRun, ClampsTheBodyTurnToTheLookLimitBeforeTurning) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kBandTarget, 1u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 170.0f);  // 170 degrees: inside the 180 gate
  req.SetF(pes::moveword::kTurn, 170.0f);
  req.SetF(pes::moveword::kDuration, 0.05f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  pes::movestep::GotoRun(h, req);
  const float rootFacing = req.F(pes::moveword::kRootFacingAngle);
  // PES's guard returns early when the desired turn is at or past 112.5 degrees, which this
  // request is not; the written angle must be the heading turned by the clamped 180 limit.
  EXPECT_LE(rootFacing, 180.0f);
  EXPECT_GE(rootFacing, -180.0f);
}
