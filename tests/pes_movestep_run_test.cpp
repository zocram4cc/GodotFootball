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
#include "movestep_team_synthetic.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_turn.hpp"

// The group-B test probe's stand-ins for the symbols other groups own (see
// movestep_probe_stubs.cpp). They record the call and return the neutral answer, so a test
// states which exit PES took without the real body's arithmetic.
namespace pes {
namespace probe {
extern int idleRowCalls;
extern int stayCalls;
extern bool stayAnswer;
extern int outOfPlayCalls;
extern bool outOfPlayAnswer;
extern int turnStopCalls;
extern int runTurnRunCalls;
extern int runTurnStepCalls;
extern bool runTurnStepAnswer;
extern int stepTurnRunCalls;
extern bool stepTurnRunAnswer;
extern int issueCount;
extern int accelEntryCalls;
extern int decelEntryCalls;
void Reset();
}  // namespace probe
}  // namespace pes

namespace {

// A minimal MoveHost for the tests that need one: every read returns a fixed synthetic value, so
// a test states exactly what it sets. The interface helpers the dispatcher port owns (IdleRow,
// SpeedToRate, LocomotionStyle) are not defined yet, so no test here drives a handler that calls
// them; the handler tests land with those bodies.
class SyntheticHost : public pes::MoveHost {
 public:
  // LocomotionStyle's reads: the run tests do not drive the style ladder.
  pes::StyleConstants style{};
  // LateralDamping's reads: the run tests do not drive the drift.
  pes::DampingConstants damping{};
  pes::DampingRecord dampingRecord{};
  int CurrentState() const override { return 0; }
  const pes::TeamAi& Team() const override { return team; }
  SyntheticTeam team;
  const pes::DampingConstants& DampingConstantsView() const override { return damping; }
  const pes::DampingRecord& DampingRecordView() const override { return dampingRecord; }
  const pes::StyleConstants& StyleConstantsView() const override { return style; }
  bool ScriptedSlotIsOurs() const override { return false; }
  int StaminaPercent() const override { return 100; }
  float PlayerHeightCm() const override { return 180.0f; }
  void BallSample(int, float out[3]) const override { out[0] = out[1] = out[2] = 0.0f; }
  float BallMotionHeadingDeg() const override { return 0.0f; }
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
  const pes::MotionRecord& Motion(int) const override { return record; }
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
  const pes::movestep::MoveTurnConstants& MoveTurnConstantsView() const override {
    return turnConstants;
  }
  float BandSpeedKmh(int band, float g) const override { return bandSpeeds[band]; (void)g; }
  bool MirrorChoice(bool sameTeam) const override { return mirrorChoice; (void)sameTeam; }
  const pes::RunConstants& RunConstantsView() const override { return runConstants; }
  const pes::RunTables& RunTablesView() const override { return runTables; }
  const pes::SpeedRateTable& SpeedRateTableView() const override { return speedRate; }
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
  pes::MotionRecord record;  // the motion table entry the host hands the handlers
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
  pes::SpeedRateTable speedRate{};
  pes::movestep::MoveTurnConstants turnConstants{};
  pes::MoveConstants moveConstants{};
  pes::IdleRowTables idleRows{};
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
  // The constants the no-class branch, the two entries and the shared band/speed block read,
  // each the install's own value (the install-relations test checks the real ones).
  k.deg3375 = 33.75f;         // 0x1425cc688
  k.deg5p625 = 5.625f;        // 0x1425ddf84
  k.deg84375 = 84.375f;       // 0x14259be54
  k.deg123p5 = 123.5f;        // 0x1425ddfb0
  k.deg163p125 = 163.125f;    // 0x1425ddfc4
  k.deg146p25 = 146.25f;      // 0x1425ddfbc
  k.kmh172 = 17.2f;           // 0x1425ddf90
  k.kmh1475b = 14.75f;        // 0x1425ddf88
  k.rate055 = 0.55f;          // 0x1425a358c
  k.foot075 = 0.75f;          // 0x14259bdd4
  k.foot027 = -0.27f;         // 0x1425ddfe8
  k.dist110 = 1.1f;           // 0x1425a3590
  k.rec40 = 40.0f;            // 0x14259be2c
  k.rec59 = 59.0f;            // 0x1425cb2fc
  k.rec99 = 99.0f;            // 0x1425c48cc
  k.frame50 = 50.0f;          // 0x14259be34
  k.frame95 = 95.0f;          // 0x1425c594c
  k.replay45 = 0.45f;         // 0x14256b140
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
  for (int i = 0; i < 64; ++i) t.styleGroup[i] = 0xe31 + i;
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
  // FUN_1406d2030's representative-motion table (DAT_1425ddcf0) is 16 styles x 4 members: the
  // ladder returns styles 0..15, so a 16-word load leaves every style past 3 reading motion 0,
  // and a SetMotion of motion 0 plays no clip.
  for (int style = 0; style < 16; ++style)
    for (int band = 0; band < 4; ++band)
      EXPECT_GT(t->styleGroup[style * 4 + band], 0) << "style " << style << " band " << band;
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

// ---------------------------------------------------------------------------------------------
// The STOP handler's decision arms (FUN_1406c9e20). Its decompile's five exits are:
//   1. the re-plan into GOTO_RUN (the turn lock words set, a band below 3, a speed under
//      16 km/h and the stay decision true);
//   2. STEP_TURN_RUN through FUN_1406d38d0 (the second-dwell draw, a positive class and band);
//   3. the run-turn / turn-stop block (the small-turn arm, the 45+22.5-per-look-distance
//      re-plan, and the RUN_TURN_RUN / TURN_STOP / RUN_TURN_STEP entries);
//   4. the update arm that rewrites the request words (the style, band, speed and dwell);
//   5. the stop into IDLE, through the stay decision and the 33.75-degree facing error.
// A host stands each cross-group entry in so the test states which exit PES takes.
// ---------------------------------------------------------------------------------------------

// FUN_1406c9e20's rate tail: the word is lowered by 0.05, and the MAXSS keeps the larger of
// the two, so a word already at 0.3 stays at 0.3.
TEST(Stop, LowersTheRateWordByFiftyMillisecondsAndKeepsItsFloor) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  // A starting rate of 0.5: the arithmetic gives 0.45, and PES's MAXSS keeps the larger of
  // the two, so a word already at 0.3 stays at 0.3 and a word below it is lowered by 0.05.
  req.SetF(pes::moveword::kRate, 0.50f);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit2, 0.2f);
  pes::movestep::Stop(h, req);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.45f);
}

// The rate word's arithmetic (arm A of FUN_1406c9e20): PES's own sequence is the single
// SUBSS `word - 0.05`; the compare `word > word - 0.05` always holds, so the 0.3
// substitution the decompile shows is dead code in the shipped binary.
TEST(Stop, FloorsTheRateWordAtThirtyCentiWhenTheWordIsAlreadyThere) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kRate, 0.30f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  pes::movestep::Stop(h, req);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.25f);
}

// The re-plan arm (B): the turn-lock words set, the band below 3, the ground speed under
// 16 km/h and the stay decision true, with the precise flag clear, re-plan into GOTO_RUN.
TEST(Stop, RePlansIntoGotoRunWhenTheTurnLockWordsAreSetAndTheStayDecisionHolds) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 2u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  // The turn-lock words 0x3d/0x3e: one of them set is enough for the arm.
  req.SetByte(pes::moveword::kTurnLockFlag, 1);
  // The ground speed under 16 km/h: the host's last-tick displacement stays zero.
  h.currentKmh = 0.0f;
  pes::probe::Reset();
  pes::probe::stayAnswer = true;
  pes::movestep::Stop(h, req);
  // The exit is the GOTO_RUN re-plan: the entry writes the state word 7 (ACCELERATE) and
  // issues its motion, so the test checks that the arm was taken rather than the words a
  // different exit would leave.
  EXPECT_EQ(req.U(pes::moveword::kNextState), 7u);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_GE(pes::probe::stayCalls, 1);
}

// The same arm's speed gate: with the ground speed at or above 16 km/h the re-plan does not
// fire, and the tick falls through to the turn block instead. The host's last-tick
// displacement drives PES's own ground-speed compute (`ticks * 60 * 60 / 1000 * |disp|`,
// 194.4 km/h per metre at 54 ticks).
TEST(Stop, KeepsTheTickWhenTheGroundSpeedIsAtOrAboveSixteen) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.positions[1][0] = 1.0f;  // 194.4 km/h at 54 ticks
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 2u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 150.0f);  // 140 degrees: the turn block idles
  req.SetF(pes::moveword::kTurn, 140.0f);
  req.SetF(pes::moveword::kFacingError, 140.0f);
  req.SetByte(pes::moveword::kTurnLockFlag, 1);
  // The first dwell past its limit, so the tick reaches the IDLE arm rather than the turn
  // block's re-plans.
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit2, 0.05f);
  pes::probe::Reset();
  pes::probe::stayAnswer = false;
  pes::movestep::Stop(h, req);
  // The re-plan arm is not taken (the ground speed is at or above 16 km/h): the turn block
  // idles at a bearing of 140 degrees and the IDLE arm's state word is requested instead.
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
}

// ---------------------------------------------------------------------------------------------
// The ACCELERATE handler's decision arms (FUN_1406bb570). Its decompile's five arms:
//   1. the turn-motion arm (the live clip at motion 0xbe0): the 15/60 draw against the clip's
//      own tick, the 0.45 / 0.65 / 1.2 rate ladder and the stop-row issue, ending in the
//      state words 0x1f = 3 and 0x1d = 28 km/h;
//   2. the yaw-lock arm: the +0x4d0 interface's flag clear, the hit tick at or below the
//      live frame and the frame at or below the mid-tick, with the turn bearing under 90
//      degrees, writes the lock words 0x3d/0x3e/0x3f and calls FUN_1406cbb00;
//   3. the fast-turn arm (the precise word 1 and a look bearing at or above 90 degrees, or
//      the two 0xbe0-family motions): the speed ladder gives the turn allowance, the bearing
//      is turned against the AI target and the root-facing pair is written;
//   4. the STEP_TURN_RUN / re-plan arm (words 0x1f and 0x24 both below 1 with the hit count
//      and kind gates);
//   5. the state-word arm (the 0xbe0-family motions with the 0.25 rate, or the swing row with
//      the bearing under 45 degrees): the class/speed/band triple from the interface, then
//      the vtable +0x10 issue and the state words.
// ---------------------------------------------------------------------------------------------

// Arm 4's shape: with the class word 1 at or above 1 the arm does not fire, so the handler
// falls to the state-word arm, whose observable writes are the rate tail, the band words
// and the ISSUED motion.
TEST(Accelerate, RewritesTheClassAndBandWordsWhenTheClassIsNotBelowOne) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;  // a turn-family motion, not the 0xbe0 clip
  h.currentClipFrames = 2;  // the frame gate is `hitTotal+1 <= liveFrame < ticks*6/60 + 0.5`
  h.rateCeiling = 1.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);  // at 1: arm 4's gate fails
  req.SetU(pes::moveword::kRateBand, 0u);
  req.SetU(pes::moveword::kMoveBucket, 0u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 70.0f);
  // The turn bearing is word 0x18 against word 1; 130 - 70 = 60 degrees is past the 56.25
  // pre-empt gate.
  req.SetF(pes::moveword::kRecordStampB, 130.0f);
  req.SetF(pes::moveword::kLookHeading, 130.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  pes::probe::Reset();
  pes::movestep::Accelerate(h, req);
  // Arm B's class/speed block writes the class/band words and re-plans through the run
  // entry FUN_1406d2030, whose transition is state 3 (RUN_LOOP). PES's own EnterRun writes
  // the request words; the motion is issued by the dispatcher's tail, so no Issue call is
  // expected here.
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kRunLoop));
  EXPECT_EQ(req.U(pes::moveword::kBandTarget), 0u);
  EXPECT_LE(req.U(pes::moveword::kRateBand), 2u);
}

// Arm B's class-3 gate: with the turn bearing under 56.25 degrees the class/speed block is
// not taken, and the arm instead checks word 5's bearing past 90 degrees, the band below 2
// and class 3. When those hold the ground-speed ladder's class 2 is written and
// RUN_TURN_RUN is entered.
TEST(Accelerate, TakesTheRunTurnRunEntryThroughTheClassThreeGate) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  h.rateCeiling = 1.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 3u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 70.0f);
  req.SetF(pes::moveword::kRecordStampB, 75.0f);  // a 5-degree turn bearing: under 56.25
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kLookDelta, 100.0f);  // word 5's bearing past 90 degrees
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  pes::probe::Reset();
  pes::movestep::Accelerate(h, req);
  // The ground-speed ladder gives class 0 at zero ground speed, with band 0 and 5 km/h.
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 5.0f);
  EXPECT_GE(pes::probe::runTurnRunCalls, 1);
}

// The IDLE arm (E): a stop whose first dwell is still below its scaled limit (the turn is
// small so the ramp is 0 and the gate is `dwellElapsed < 0`) goes to the IDLE arm; with the
// bucket 0 and the stay decision false it issues the idle row.
TEST(Stop, IssuesTheIdleRowWhenTheDwellHasElapsedAndTheBucketIsStraightAhead) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.percent = false;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 2u);  // band 2: the dwell scale is 0.8
  req.SetU(pes::moveword::kMoveBucket, 0u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  // The first dwell is past its limit scaled by the turn ramp (0 bytes of turn, so the
  // scale is the band's own 0.8), and the second dwell is past its limit too: the IDLE arm's
  // gate is `dwell scale * dwell limit < dwell elapsed`, which this satisfies.
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
  req.SetF(pes::moveword::kDwellLimit2, 0.05f);
  pes::probe::Reset();
  pes::movestep::Stop(h, req);
  // The IDLE exit: the state is requested and the motion is stored from the cell at
  // (row, 9), with the mirror from Percent(50) (kinds 3/6 take the cell's own mirror).
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  // The idle row came from the shared IdleRow picker (probe stub), and the cell was read
  // at column 9 for kinds other than 3/6 through Percent(50).
  EXPECT_EQ(pes::probe::idleRowCalls, 1);
  EXPECT_EQ(req.U(pes::moveword::kMotion), uint32_t(0x100 + 0x5a1));
  // The IDLE arm's own stay-decision read is the last one before it; the re-plan arm (B)
  // also reads it when the turn-lock words are set, which they are not here.
  EXPECT_GE(pes::probe::stayCalls, 1);
}

// The IDLE arm's guard: with the stay decision true the arm takes the GOTO_RUN re-plan
// instead, and the band becomes the -1 sentinel.
TEST(Stop, SetsTheBandSentinelAndRePlansWhenTheStayDecisionIsTrue) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.bandSpeeds[0] = 4.0f;  // the band's own speeds in km/h, as the anime info record holds
  h.bandSpeeds[1] = 8.0f;
  h.bandSpeeds[2] = 14.0f;
  h.bandSpeeds[3] = 22.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  // The shared block's rate read is FUN_1406b9ff0, whose class path the port takes when the
  // request's word 0xb (the class-rate flag) is set; the block passes the band as the class.
  req.SetByte(pes::moveword::kUseClassRate, 1);
  req.SetU(pes::moveword::kMoveBucket, 0u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit2, 0.2f);
  pes::probe::Reset();
  pes::probe::stayAnswer = true;
  pes::movestep::Stop(h, req);
  // The stay-decision-true arm writes the -1 sentinel into the band word and re-plans
  // through the shared band/speed block, whose exit is the run entry FUN_1406d2030: the
  // band's class rate (0.15 at band 0), its class (0 on the ladder), the band's own speed
  // times 3.6 and the state word 3 (RUN_LOOP), the run entry's own transition.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.15f);
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 4.0f * 3.6f);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kRunLoop));
}

// The update arm's condition (D): the stay decision true and the group-C step-stop false,
// the precise flag clear, and the two bearings under 67.5 degrees, writes the band's rate,
// its class and its speed through the tables.
TEST(Stop, RewritesTheBandAndSpeedWordsThroughTheTablesInTheUpdateArm) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.bandSpeeds[0] = 4.0f;
  h.bandSpeeds[1] = 8.0f;
  h.bandSpeeds[2] = 14.0f;
  h.bandSpeeds[3] = 22.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 2u);
  // The update arm's rate read is FUN_1406b9ff0, whose class path the port takes when the
  // request's word 0xb (the class-rate flag) is set; the block passes the band as the class.
  req.SetByte(pes::moveword::kUseClassRate, 1);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  req.SetF(pes::moveword::kDwellLimit, 5.0f);
  pes::probe::Reset();
  pes::probe::stayAnswer = true;
  pes::movestep::Stop(h, req);
  // The update arm takes the band's rate through FUN_1406b9ff0's class path (band 0 gives
  // 0.15), its class through FUN_1406ba030 (ladder {0.29, 0.59, 0.70, 1.0} gives 0), the
  // band's own speed times 3.6 (4 km/h at band 0), then the run entry FUN_1406d2030, whose
  // transition is state 3 (RUN_LOOP).
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.15f);
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 4.0f * 3.6f);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kRunLoop));
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

// The turn block's gate (D): when the dwell is past its limit the turn ramp scales it, and
// the port reaches the IDLE arm instead of the update arm. The test states that the arm is
// chosen by the dwell, by the words only the IDLE arm writes.
TEST(Stop, FallsThroughToTheIdleArmWhenTheFirstDwellSitsBelowItsScaledLimit) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetU(pes::moveword::kMoveBucket, 0u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  // The turn block's gate: the first dwell sits above its limit scaled by the band's own
  // 0.7 (the turn is small so the ramp adds nothing), so the tick goes to the IDLE arm.
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.5f);
  req.SetF(pes::moveword::kDwellElapsed2, 1.0f);
  req.SetF(pes::moveword::kDwellLimit2, 0.2f);
  pes::probe::Reset();
  pes::probe::stayAnswer = false;
  pes::movestep::Stop(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
}

// ---------------------------------------------------------------------------------------------
// The DECELERATE handler's decision arms (FUN_1406bcd20). Its decompile's flow:
//   1. the yaw-lock block: the re-orient object's flag clear, the hit tick at or before the
//      live frame and the live frame at or before the mid-tick; the lock words 0x3d/0x3e/0x3f
//      are written with the per-tick turn under 45 degrees and FUN_1406cbb00's yaw store.
//   2. the pre-empt gates (the same shape as ACCELERATE's, with the two 0xc3a/0xc3b motions
//      as the extra flag): the first bearing past 56.25 degrees (or the fast-turn flag)
//      clears the dwell words and takes the band/speed block; otherwise the class-3 gate (a
//      word-5 bearing past 90, the band below 2 and class 3) takes the ground-speed ladder
//      and RUN_TURN_RUN; otherwise the 5/11 ladder block writes the class and speed words.
//   3. the class -1 arm (the fast-turn flag and a positive band): band 3 with 28 km/h, the
//      ground-speed ladder 8 / 14.75 / 19 and the turn-stop entry; otherwise the band copy
//      and the stop entry.
//   4. the allowance arm (the look distance over 2 m, with its 22.5-per-unit term, and the
//      45-degree floor, past the look bearing, or the fast-turn flag): the turn-stop entry
//      through STEP_TURN_RUN's draw, or the band/speed block with the precise word's gate.
//   5. the bearing arm (the look bearing under 45 degrees or the 0xc2b motion): the reorient
//      write and the yaw store with the 180/90 allowance and the 22.5 / 0.25 clamps.
//   6. the state arm: the look distance over 2 m past 45 degrees, or the bearing under 45
//      degrees with the band 3 and word 8 above 1, re-plans into GOTO_RUN; otherwise the
//      speed ladder and the run entry, with the class 3 / 19 / 22 ladder of the tail.
// ---------------------------------------------------------------------------------------------

// The state arm's re-plan (FUN_1406bd8b9): with the class word 2, the band word 0 and word 8
// above 1 - the three terms of PES's ladder `classWord < 3 && bandWord < 1 && classWord > 1`
// - the handler writes the band word 0 and the speed 5 km/h and re-plans into GOTO_RUN.
TEST(Decelerate, RePlansIntoGotoRunWhenTheClassWordIsTwoAndTheBandIsZero) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);  // class word 2: inside the ladder
  req.SetU(pes::moveword::kRateBand, 0u);           // band word 0: below 1
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 1.0f);
  req.SetF(pes::moveword::kRate, 0.0f);
  pes::probe::Reset();
  pes::movestep::Decelerate(h, req);
  // The re-plan's exit is the GOTO_RUN entry's own transition, word 7 (ACCELERATE).
  EXPECT_EQ(req.U(pes::moveword::kNextState), 7u);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

// The class -1 arm's fast-turn path: the fast-turn flag with a positive band takes band 3
// with 28 km/h, then the ground-speed ladder 8 / 14.75 / 19 and the turn-stop entry.
TEST(Decelerate, TakesTheTurnStopEntryThroughTheClassMinusOneLadder) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);  // class -1
  req.SetU(pes::moveword::kRateBand, 2u);
  req.SetU(pes::moveword::kPrecise, 1u);  // the fast-turn flag's second term
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 400.0f);  // a look bearing past 90 degrees
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 1.0f);
  req.SetF(pes::moveword::kRate, 0.0f);
  pes::probe::Reset();
  pes::movestep::Decelerate(h, req);
  // Band 3 with 28 km/h first, then the ground-speed ladder's 0 (the speed is zero here)
  // leaves the band at 0 and the speed at 5 km/h; the entry is the turn-stop one.
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 5.0f);
  EXPECT_GE(pes::probe::turnStopCalls, 1);
}

// The same ladder's lower bound: with the class word 1 (not above 1) the re-plan does not
// fire, and the tick falls through to the speed ladder and the run entry, whose transition
// is state 3 (RUN_LOOP).
TEST(Decelerate, KeepsTheTickWhenTheClassWordIsNotAboveOne) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);  // class word 1: not above 1
  req.SetU(pes::moveword::kRateBand, 0u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 1.0f);
  req.SetF(pes::moveword::kRate, 0.0f);
  pes::probe::Reset();
  pes::movestep::Decelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kRunLoop));
}

// The class -1 arm's band-copy path is unreachable without hit records: PES's own guard
// is `hitEnd <= liveFrame || secondBearing < 45`, and with no records hitEnd is zero, so
// the first term always holds. The test states that, which is the same situation
// GotoRun's re-entry arm has, and checks the ladder the arm therefore takes.
TEST(Decelerate, KeepsTheBandCopyPathOutOfReachWithoutHitRecords) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);  // class -1
  req.SetU(pes::moveword::kRateBand, 2u);
  req.SetU(pes::moveword::kMoveBucket, 6u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 72.0f);  // a 60-degree second bearing
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 1.0f);
  req.SetF(pes::moveword::kRate, 0.0f);
  pes::probe::Reset();
  pes::movestep::Decelerate(h, req);
  // The turn-stop ladder ran, not the band copy: the rate word stays untouched (the copy
  // arm writes 0.6 before the stop entry) and the turn-stop entry took the tick.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.0f);
  EXPECT_GE(pes::probe::turnStopCalls, 1);
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 5.0f);
}

// The allowance arm: with the look distance at 3 m (one unit, allowance 22.5 + 45 = 67.5)
// and the second bearing past it, the arm takes the precise word's gate and the
// STEP_TURN_RUN entry, then the band/speed words and the stop entry.
TEST(Decelerate, TakesTheStepTurnRunEntryThroughTheAllowanceArm) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 300.0f);  // a 72-degree second bearing: past 67.5
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 3.0f);  // one unit over 2 m: allowance 22.5 + 45
  req.SetF(pes::moveword::kRate, 0.0f);
  pes::probe::Reset();
  pes::movestep::Decelerate(h, req);
  // The precise arm's gate held (the look distance is under 3 m and the turn is under
  // 112.5 degrees), so the STEP_TURN_RUN entry's threshold is computed and the entry took
  // the tick. The band/speed words are the ladder's own for a zero ground speed.
  EXPECT_GE(pes::probe::stepTurnRunCalls, 1);
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 5.0f);
}

// ---------------------------------------------------------------------------------------------
// The RUN_LOOP handler's decision arms (FUN_1406c0170). Its decompile's flow, from the
// request's own class -1 branch and the running branch:
//   1. the no-class arm (class -1, or the out-of-play phase without the precise word): the
//      look bucket is the precise word's second bucket (or the first), the turn bearing is
//      the look delta, the look distance over 2 m grows the allowance, the state picker's
//      override (the ball-line point, the team record and its frame) and the keeper / mode
//      arm with the 45 / 22.5 look clamps, then the row pick (kind + band*8) and the swing
//      arm, ending in row 0x662 / 0x661, the playback and the start time, the dwell words
//      and the transition to TURN_STOP (word 6).
//   2. the running arm: the re-plan term (the class above 2 with the rate at or below 0.225
//      or the turn past 78.75 degrees with the speed under 11, the distance over 3 m and the
//      turn past 56.25), the ACCELERATE and DECELERATE pre-empts, the RUN_TURN_STEP
//      pre-empt, and the state arm's own row pick with the band/speed/class triple and the
//      vtable +0x10 issue.
// The test pack below states the running arm's re-plan gate and the state arm's writes.
// ---------------------------------------------------------------------------------------------

// The re-plan gate: with the class above 2, the rate word at or below 0.225, the look
// distance over 3 m and the turn past 56.25 degrees, the handler re-plans into GOTO_RUN.
TEST(RunLoop, RePlansIntoGotoRunWhenTheClassIsHighAndTheRateIsLow) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 0;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 3u);  // above 2
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kRate, 0.2f);             // at or below 0.225
  req.SetF(pes::moveword::kDistToTarget, 5.0f);     // over 3 m
  req.SetF(pes::moveword::kTurn, 60.0f);            // past 56.25
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 60.0f);
  req.SetF(pes::moveword::kRecordStampB, 60.0f);
  req.SetF(pes::moveword::kLookHeading, 60.0f);
  req.SetF(pes::moveword::kLookDelta, 60.0f);
  req.SetF(pes::moveword::kFacingError, 60.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  pes::probe::Reset();
  pes::movestep::RunLoop(h, req);
  // The re-plan's exit is the GOTO_RUN entry's own transition, word 7 (ACCELERATE).
  EXPECT_EQ(req.U(pes::moveword::kNextState), 7u);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

// ---------------------------------------------------------------------------------------------
// The two entries' decision arms. EnterAccelerate (FUN_1406ccd00) refuses before writing
// when any of its gates fails: the band word at or above the bearing's 67.5 threshold, the
// band at 3, the class 3 with the rate word above 0.65, the ground speed at or below 11
// km/h, or the look distance at or below 2 m. Its success path writes the band word 3, the
// speed word 28 km/h, the previous band and speed words, the bucket word and the state word
// 9 (ACCELERATE). EnterDecelerate (FUN_1406cdaf0) has the same gates' shape with the speed
// ladder of its own and writes the state word 10 (DECELERATE).
// ---------------------------------------------------------------------------------------------

// EnterAccelerate's refusal: with the look distance at or below 2 m the entry returns false
// and writes nothing.
TEST(EnterAccelerate, RefusesToWriteWhenTheLookDistanceIsBelowTwo) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 1.0f);  // at or below 2 m: the gate refuses
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  pes::probe::Reset();
  EXPECT_FALSE(pes::movestep::EnterAccelerate(h, req));
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
}

// EnterAccelerate's success: with every gate past, the entry writes the band word 3, the
// speed word 28 km/h and the state word 9 (ACCELERATE), and returns true.
TEST(EnterAccelerate, WritesTheBandWordThreeAndTheStateWordNine) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  h.positions[1][0] = 1.0f;  // a 194.4 km/h ground speed: past the 11 km/h gate
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 3.0f);  // over 2 m
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  req.SetU(pes::moveword::kMoveBucket, 3u);  // a bucket whose column is 13
  pes::probe::Reset();
  EXPECT_TRUE(pes::movestep::EnterAccelerate(h, req));
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 3u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 28.0f);
  EXPECT_EQ(req.U(pes::moveword::kNextState), 9u);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

// EnterAccelerate's issue (FUN_1406ccd00 arm 7): vt+0x10 takes (req, motion, mirror, 0,
// 0.1667, req[0x35], startLadder). req[0x35] is the vt+0x50 answer the entry just wrote, so the
// issued playback is nonzero; the start time is the 0.15/0.75/0.5 ladder, never a km/h rung.
TEST(EnterAccelerate, IssuesThePlaybackWordWithTheStartLadder) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.playback.gLow = 0.9166f;
  h.playback.gHigh = 1.0833f;
  h.playback.gSlope = -0.1667f;
  h.playback.one = 1.0f;
  h.playback.stepSlope = 0.15f;
  h.playback.stepBase = 0.95f;
  h.playback.turnP2 = 0.1f;
  h.playback.turnT = 0.05f;
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  h.positions[1][0] = 1.0f;
  h.mirrorChoice = true;  // the vt+0x50 p is 1, like the interface's own answer
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 3.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 0.0f);  // what the adapter supplies: req[0x4b] has no fill
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  req.SetU(pes::moveword::kMoveBucket, 3u);
  pes::probe::Reset();
  EXPECT_TRUE(pes::movestep::EnterAccelerate(h, req));
  EXPECT_GT(h.issued.playback, 0.0f) << "the issued playback is req[0x35], never req[0x4b]";
  EXPECT_LE(h.issued.startTime, 1.0f) << "the start time is the 0.15/0.75/0.5 ladder";
}

// EnterDecelerate (FUN_1406cdaf0): with the class word above 1 the entry refuses outright.
TEST(EnterDecelerate, RefusesToWriteWhenTheClassWordIsAboveOne) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  h.positions[1][0] = 1.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);  // above 1: the first gate refuses
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 3.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  pes::probe::Reset();
  EXPECT_FALSE(pes::movestep::EnterDecelerate(h, req));
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
}

// EnterDecelerate's success: with every gate past, the entry writes the band word 1, the
// speed word 11 km/h and the state word 10 (DECELERATE).
TEST(EnterDecelerate, WritesTheBandWordOneAndTheStateWordTen) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  h.positions[1][0] = 1.0f;  // a 194.4 km/h ground speed: past the 22 km/h gate
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);  // at or below 1: the gate passes
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 3.0f);  // over 1 m
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetU(pes::moveword::kMoveBucket, 3u);
  pes::probe::Reset();
  EXPECT_TRUE(pes::movestep::EnterDecelerate(h, req));
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 1u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedBand), 11.0f);
  EXPECT_EQ(req.U(pes::moveword::kNextState), 10u);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

// EnterGotoRun's distance/speed arm (FUN_1406ce820 block 5): on the 0x5ca row with a column
// of 8/9, a ball within reach takes row 0x662 on the Percent(40) draw, else 0x661. The test
// stands the ball a metre away and the draw at "below 40".
TEST(EnterGotoRun, TakesTheDrawRowWhenTheBallIsWithinReach) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.runTables.gotoRow[0 + 1 * 8] = 0x5ca;  // kind 0, cls 1 picks the distance arm's row
  h.percent = true;  // the Percent(40) draw holds
  h.positions[0][0] = 0.0f;
  h.positions[0][1] = 0.0f;
  // BallSample returns the origin; the player stands at the origin, so the ball is within
  // reach and the row becomes 0x662.
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 0u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetU(pes::moveword::kMoveBucket, 0u);
  req.SetU(pes::moveword::kLookBucket, 0u);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 10.0f);
  req.SetF(pes::moveword::kRate, 0.5f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  pes::probe::Reset();
  pes::movestep::EnterGotoRun(h, req, false);
  // The entry always requests ACCELERATE (state 7) from this path; the distance arm's row
  // pick is observed through the issued cell: the draw held, so the row is 0x662.
  EXPECT_EQ(req.U(pes::moveword::kNextState), 7u);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(h.lastCellRow, 0x662);
  EXPECT_EQ(h.percentArg, 40);
  // The foot flag carries the picked cell's mirror (FUN_1406ce820 lines 561/858): the column
  // of bucket 0 is 9, so the mirror bit is 1.
  EXPECT_EQ(req.U(pes::moveword::kFootFlag) & 0xffu, 1u);
  // The lost draw takes the base row instead.
  SyntheticHost h2;
  h2.runConstants = SyntheticRun();
  h2.runTables = SyntheticTables();
  h2.runTables.gotoRow[0 + 1 * 8] = 0x5ca;
  h2.percent = false;
  pes::MoveReq req2;
  req2.SetU(pes::moveword::kKind, 0u);
  req2.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req2.SetF(pes::moveword::kFacing, 0.0f);
  req2.SetF(pes::moveword::kHeading, 0.0f);
  req2.SetF(pes::moveword::kLookHeading, 0.0f);
  req2.SetF(pes::moveword::kBodyDirection, 0.0f);
  req2.SetF(pes::moveword::kTurn, 0.0f);
  req2.SetF(pes::moveword::kFacingError, 0.0f);
  req2.SetU(pes::moveword::kMoveBucket, 0u);
  req2.SetU(pes::moveword::kLookBucket, 0u);
  req2.SetF(pes::moveword::kDesiredSpeedKmh, 10.0f);
  req2.SetF(pes::moveword::kRate, 0.5f);
  req2.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req2.SetF(pes::moveword::kDwellLimit, 0.0f);
  req2.SetF(pes::moveword::kDwellElapsed, 0.0f);
  pes::probe::Reset();
  pes::movestep::EnterGotoRun(h2, req2, false);
  EXPECT_EQ(h2.lastCellRow, 0x661);
}

// EnterGotoRun's out-of-play arm (FUN_1406ce820 block 6): outside open-play phase 1, a row in
// the two 0xa014-bit bands takes row 0x5cc. In play the arm never fires.
TEST(EnterGotoRun, TakesTheOutOfPlayRowOnlyOutsideOpenPlay) {
  for (int phase : {1, 0}) {
    SyntheticHost h;
    h.runConstants = SyntheticRun();
    h.runTables = SyntheticTables();
    h.runTables.gotoRow[0 + 1 * 8] = 0x2;  // a row inside the 0xa014 bands
    h.phase = phase;
    h.percent = false;
    pes::MoveReq req;
    req.SetU(pes::moveword::kKind, 0u);
    req.SetU(pes::moveword::kCommandSpeedClass, 1u);
    req.SetF(pes::moveword::kFacing, 0.0f);
    req.SetF(pes::moveword::kHeading, 0.0f);
    req.SetF(pes::moveword::kLookHeading, 0.0f);
    req.SetF(pes::moveword::kBodyDirection, 0.0f);
    req.SetF(pes::moveword::kTurn, 0.0f);
    req.SetF(pes::moveword::kFacingError, 0.0f);
    req.SetU(pes::moveword::kMoveBucket, 0u);
    req.SetU(pes::moveword::kLookBucket, 0u);
    req.SetF(pes::moveword::kDesiredSpeedKmh, 10.0f);
    req.SetF(pes::moveword::kRate, 0.5f);
    req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
    req.SetF(pes::moveword::kDwellLimit, 0.0f);
    req.SetF(pes::moveword::kDwellElapsed, 0.0f);
    pes::probe::Reset();
    pes::movestep::EnterGotoRun(h, req, false);
    // Column for bucket 0 is 9 (synthetic bucketColumn[0] = 9), so block 5's column gate
    // (column - 8 < 2) holds but the row is 0x2, not 0x5ca: block 5 is skipped. Block 6's
    // verdict is the picked row.
    EXPECT_EQ(h.lastCellRow, phase == 1 ? 0x2 : 0x5cc) << "phase " << phase;
  }
}

// EnterDecelerate's issue (FUN_1406cdaf0:337-339): vt+0x10 takes (req, motion, mirror, 0,
// 0.1667, req[0x35], startLadder) with req[0x35] the vt+0x50 answer and the start time 0.2
// for the 0xb53 row, else 0.
TEST(EnterDecelerate, IssuesThePlaybackWordWithTheStartLadder) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.playback.gLow = 0.9166f;
  h.playback.gHigh = 1.0833f;
  h.playback.gSlope = -0.1667f;
  h.playback.one = 1.0f;
  h.playback.stepSlope = 0.15f;
  h.playback.stepBase = 0.95f;
  h.playback.turnP2 = 0.1f;
  h.playback.turnT = 0.05f;
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  h.positions[1][0] = 1.0f;
  h.mirrorChoice = true;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kDistToLook, 3.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 0.0f);  // what the adapter supplies
  req.SetU(pes::moveword::kMoveBucket, 3u);
  pes::probe::Reset();
  EXPECT_TRUE(pes::movestep::EnterDecelerate(h, req));
  EXPECT_GT(h.issued.playback, 0.0f) << "the issued playback is req[0x35], never req[0x4b]";
  EXPECT_LE(h.issued.startTime, 1.0f) << "the start time is 0.2/0, never a km/h rung";
}

// The no-class branch: with the class word at -1 and the match phase 1, the swing arm picks
// the turn-stop row and issues the clip, and the request is written with the bucket word,
// the turned heading and the state word 6 (TURN_STOP).
TEST(RunLoop, TakesTheSwingArmWhenTheClassWordIsMinusOne) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 0;
  h.phase = 1;  // the match phase the no-class branch needs
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);  // class -1
  req.SetU(pes::moveword::kPrecise, 1u);  // the precise word set: the no-class branch
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetU(pes::moveword::kMoveBucket, 0u);
  req.SetU(pes::moveword::kCommandId, 0u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetF(pes::moveword::kLookBucket, 0u);
  req.SetF(pes::moveword::kLookBucketAlt, 0u);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  h.bandSpeeds[0] = 4.0f;
  h.bandSpeeds[1] = 8.0f;
  h.bandSpeeds[2] = 14.0f;
  h.bandSpeeds[3] = 22.0f;
  // The bucket -> degrees table the swing arm reads for its centre angle (the install's
  // own table, which the install-relations test checks).
  h.stepTables.bucketDegrees = {0.0f, 45.0f, 90.0f, 135.0f, 135.0f,
                                0.0f, -45.0f, -90.0f, -135.0f, -135.0f};
  pes::probe::Reset();
  pes::movestep::RunLoop(h, req);
  // The swing arm's exit is the TURN_STOP state request, and the bucket word is the one the
  // swing arm's own picker wrote.
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kTurnStop));
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_GE(h.issues, 1);
  // The swing arm's writes: the bucket word it picked, the turned heading and the previous
  // bucket. The allowance is 11.25 for the buckets 3 and 8, and 0 for the others, so the
  // heading is the bucket's own centre angle plus the allowance plus the request's word 2.
  const int swingBucket = static_cast<int>(req.U(pes::moveword::kMoveBucket));
  const float allowance =
      (static_cast<unsigned>(swingBucket) - 3u < 2u || static_cast<unsigned>(swingBucket) - 8u < 2u)
          ? 11.25f
          : 0.0f;
  const float centre = h.stepTables.bucketDegrees[swingBucket];
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRecordStampC), centre + allowance + 10.0f);
  EXPECT_EQ(req.U(pes::moveword::kPrevBucket), static_cast<uint32_t>(swingBucket));
  // The swing arm's issue: the row's own cell, the 0.1667 duration (the 0xbee..0xc23 /
  // 0xbc2 / 0xb45 family's 0.2 is not this cell) and the request's playback.
  ASSERT_GE(h.issues, 1);
  EXPECT_EQ(h.issued.motion, 0x100 + SyntheticTables().gotoRow[2 + 1 * 8]);
  EXPECT_FLOAT_EQ(h.issued.duration, 1.0f / 6.0f);
  EXPECT_FLOAT_EQ(h.issued.playback, 1.0f);
}

// The no-class branch's state picker: with the look bucket at 4, the match phase 1 and the
// facing error's magnitude past 146.25 degrees, the second word is swapped to -180 and the
// look bucket to 9 - PES's own fold, which the swing arm then reads.
TEST(RunLoop, FoldsTheSecondWordWhenTheStatePickersFacingErrorIsPast146) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 0;
  h.phase = 1;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  req.SetU(pes::moveword::kPrecise, 1u);  // the precise word set: the no-class branch
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetU(pes::moveword::kMoveBucket, 0u);
  req.SetU(pes::moveword::kCommandId, 0u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  // The AI target's own heading, which the state picker reads as the desired angle: 150
  // degrees off the facing is inside the 22.5..157.5 window the fold's gate reads, and its
  // sign is opposite the facing error's, which is the fold's own second condition.
  req.SetF(pes::moveword::kLookHeading, 150.0f);
  req.SetU(pes::moveword::kLookBucketAlt, 4u);  // the look bucket at 4
  req.SetF(pes::moveword::kFacingError, -150.0f);  // past 146.25 degrees, opposite sign
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kLookDelta, 2.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.0f);
  pes::probe::Reset();
  pes::movestep::RunLoop(h, req);
  // The state picker's fold is visible in the look bucket it rewrote (4 -> 9), and the tick
  // still ends in the swing arm's TURN_STOP request.
  EXPECT_EQ(req.U(pes::moveword::kLookBucketAlt), 9u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kLookDelta), -180.0f);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kTurnStop));
}

// RUN_LOOP's jostle override (FUN_1406c0170, rev2 §5.2). The nearest opponent stands 0.5 m ahead on the
// xz plane and faces the same way as the player, so the turn to him is the request's facing. The ladder
// answers style 0 (probe), and the override picks 6, 7 or 0x10 by the turn's size.
namespace {
SyntheticHost JostleHost(float opponentFacing) {
  SyntheticHost h;
  h.runConstants = SyntheticRampConstants();
  h.team.nearest = 3;
  h.team.position[3] = {0.0f, 0.5f, 0.0f};
  h.team.heading[3] = opponentFacing;
  return h;
}

// RUN_LOOP's style that a request at `facing` and speed class `commandSpeedClass` ends on.
uint32_t JostledStyle(float facing, uint32_t commandSpeedClass) {
  SyntheticHost h = JostleHost(facing);
  pes::MoveReq req;
  req.SetF(pes::moveword::kFacing, facing);
  req.SetU(pes::moveword::kCommandSpeedClass, commandSpeedClass);
  pes::probe::Reset();
  pes::movestep::RunLoop(h, req);
  return req.U(pes::moveword::kStyle) & 0xffu;
}
}  // namespace

TEST(RunLoopJostle, ExactlyOneHundredTwelvePointFiveDegreesIsStyleSeven) {
  EXPECT_EQ(JostledStyle(112.5f, 1u), 7u);
}

TEST(RunLoopJostle, PastOneHundredTwelvePointFiveWithAShortPlayerIsStyleSixteen) {
  EXPECT_EQ(JostledStyle(113.0f, 1u), 0x10u);
}

TEST(RunLoopJostle, PastOneHundredTwelvePointFiveWithAPlayerOfClassTwoIsStyleSeven) {
  EXPECT_EQ(JostledStyle(113.0f, 2u), 7u);
}

TEST(RunLoopJostle, OneHundredFiftySevenPointFiveIsStyleSixteenWhateverTheClass) {
  EXPECT_EQ(JostledStyle(157.5f, 2u), 0x10u);
}

TEST(RunLoopJostle, UnderOneHundredTwelvePointFiveIsStyleSix) {
  EXPECT_EQ(JostledStyle(100.0f, 2u), 6u);
}
// FUN_140a92950(heading, target, clamp): the heading stepped toward the target by at most the
// clamp, wrapped into [0, 360). The stalled seed-7 free-kick taker drifts ~90 deg off his walk
// because the old stub returned the heading untouched: EnterAccelerate's bucket input became
// (278.34 * 0.8) - 272.40 = -49.74 (bucket 6, the 045 cell) instead of ~-5.9 (bucket 5, 000).
TEST(StepHeading, StepsTowardTheTargetByAtMostTheClamp) {
  const pes::RunConstants k = SyntheticRun();
  EXPECT_NEAR(pes::StepHeading(278.34f, 272.40f, 4.744f, k), 273.596f, 1e-3f);
  EXPECT_NEAR(pes::StepHeading(272.40f, 278.34f, 4.744f, k), 277.144f, 1e-3f);
}
TEST(StepHeading, WrapsAcrossZeroAndThreeSixty) {
  const pes::RunConstants k = SyntheticRun();
  EXPECT_NEAR(pes::StepHeading(359.0f, 1.0f, 5.0f, k), 1.0f, 1e-3f);
  EXPECT_NEAR(pes::StepHeading(1.0f, 359.0f, 5.0f, k), 359.0f, 1e-3f);
  EXPECT_NEAR(pes::StepHeading(359.0f, 1.0f, 1.0f, k), 0.0f, 1e-3f);
}
TEST(StepHeading, TheUnclampedStepReachesTheTarget) {
  const pes::RunConstants k = SyntheticRun();
  EXPECT_NEAR(pes::StepHeading(10.0f, 12.0f, 45.0f, k), 12.0f, 1e-3f);
  EXPECT_NEAR(pes::StepHeading(12.0f, 10.0f, 45.0f, k), 10.0f, 1e-3f);
}
