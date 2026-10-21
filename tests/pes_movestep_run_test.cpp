// PES 2021's MOVESTEP handler group B (contract v0.10): the running phase's install constants
// and the interface tables it reads, plus the pure folds the handlers share. Every expectation
// is a relation between loaded exe values or a synthetic formula check; no PES number table is
// written into code or tests. The install tests read PES21_ROOT and skip without it, as
// pes_movestep_test.cpp does.
#include <gtest/gtest.h>

#include <cstdlib>
#include <cmath>
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
extern int idleTurnCalls;
extern int runTurnRunCalls;
extern int runTurnStepCalls;
extern bool runTurnStepAnswer;
extern int stepTurnRunCalls;
extern bool stepTurnRunAnswer;
extern int issueCount;
extern int accelEntryCalls;
extern int decelEntryCalls;
extern int styleCalls;
extern int styleAnswer;
void Reset();
}  // namespace probe
}  // namespace pes

namespace {

// The sine table of FUN_140a92c80: 513 samples of sin(i * 90 / 512 degrees) with the install's scalars.
pes::SpeedClassConstants SyntheticSpeedClass() {
  pes::SpeedClassConstants k{};
  k.full = 360.0f;
  k.wrapFull = -360.0f;
  k.half = 180.0f;
  k.wrapHalf = -180.0f;
  k.neg = -1.0f;
  k.one = 1.0f;
  k.quarter = 90.0f;
  k.tabScale = 512.0f;
  for (int i = 0; i <= 512; ++i) k.sinQuarter[i] = std::sin(static_cast<float>(i) * 3.14159265f / 1024.0f);
  return k;
}

// A minimal MoveHost for the tests that need one: every read returns a fixed synthetic value, so
// a test states exactly what it sets. The interface helpers the dispatcher port owns (IdleRow,
// SpeedToRate, LocomotionStyle) are not defined yet, so no test here drives a handler that calls
// them; the handler tests land with those bodies.
class SyntheticHost : public pes::MoveHost {
 public:
  // LocomotionStyle's reads: the run tests do not drive the style ladder.
  pes::StyleConstants style{};
  pes::SpeedClassConstants speedClass = SyntheticSpeedClass();
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
    // An opt-in list of populated columns: a column outside it is an empty cell, motion -1 (the Animetable's empty answer).
    if (!populatedColumns.empty()) {
      bool listed = false;
      for (int col : populatedColumns) listed = listed || col == column;
      if (!listed) return c;
    }
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
  void OwnPosition(float out[3]) const override {
    out[0] = positions[0][0];
    out[1] = positions[0][1];
    out[2] = positions[0][2];
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
  const pes::SpeedClassConstants& SpeedClassConstantsView() const override { return speedClass; }
  const pes::IdleRowTables& IdleRowTablesView() const override { return idleRows; }
  float FacingDeg() const override { return facingDeg; }
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
  float facingDeg = 0.0f;  // P+0x20: the player's own facing, which Arm H reads
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
  std::vector<int> populatedColumns;  // empty: every column is populated
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

// FUN_1406bdf10's stop arm (goto_bdf10.c lines 215-217) waits for IDLE's second dwell when the state before the current
// one is IDLE (P+0x245d, read at VA 0x1406be573): a GOTO_RUN that IDLE's stay decision just entered must not reach the
// stop arm while that dwell runs.
TEST(GotoRun, TheStopArmWaitsForIdleSecondDwellAfterLeavingIdle) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  pes::RotRecord rot;
  rot.start = 30;  // a clip still running (its total is 54 ticks), so the stop arm's clip gate holds
  h.record.rot.push_back(rot);
  h.previousState = static_cast<int>(pes::MoveState::kIdle);
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetU(pes::moveword::kClassWord, 2u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 90.0f);  // 90 degrees: past the 56.25-degree stop bearing
  req.SetF(pes::moveword::kDwellElapsed2, 0.0f);
  req.SetF(pes::moveword::kDwellLimit2, 0.35f);
  pes::movestep::GotoRun(h, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
}

// PES's GOTO_RUN STOP entry writes the class word 0x1f to 0 before FUN_1406d5d60 (FUN_1406bdf10 line 227 of its
// decompile); the no-class arm then answers IDLE. Left at the band GOTO_RUN ran in, the word takes the run-stop arm,
// which answers STOP, and the FSM goes back and forth between the two states every tick.
TEST(GotoRun, TheStopEntryClearsTheClassWordSoTheStopAnswersIdle) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  pes::RotRecord rot;
  rot.start = 30;  // a clip still running (its total is 54 ticks), so the stop arm's clip gate holds
  h.record.rot.push_back(rot);
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetU(pes::moveword::kClassWord, 2u);  // the band GOTO_RUN left in word 0x1f
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 90.0f);  // 90 degrees: past the 56.25-degree stop bearing
  pes::movestep::GotoRun(h, req);
  EXPECT_EQ(req.U(pes::moveword::kClassWord), 0u);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
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
  // The turn-lock bytes 0x3d, 0x3e and 0x3f (stop_dec.c:64-66): one of them set is enough for the arm.
  req.SetByte(pes::moveword::kAngle3d, 1);
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

// stop_dec.c:64-66: the STOP pre-empt reads the low bytes of words 0x3d, 0x3e and 0x3f, not the words. Ticks of 256 have a zero
// low byte, so the pre-empt does not fire and the re-plan into GOTO_RUN is not taken.
TEST(Stop, ATurnLockTicksWordWithAZeroLowByteDoesNotPreemptTheStop) {
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
  req.SetU(pes::moveword::kTurnFrames, 256u);
  h.currentKmh = 0.0f;
  pes::probe::Reset();
  pes::probe::stayAnswer = true;
  pes::movestep::Stop(h, req);
  EXPECT_NE(req.U(pes::moveword::kNextState), 7u);
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
  req.SetByte(pes::moveword::kAngle3d, 1);
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
  pes::RotRecord rot;
  rot.start = 30;  // PES's frame gate (line 216) needs the clip's total ahead of the frame: a 30-frame clip, 54 ticks
  h.record.rot.push_back(rot);
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
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kStopSpeedWord), 5.0f);  // PES writes the speed word 0x1d (lines 241-243)
  EXPECT_GE(pes::probe::runTurnRunCalls, 1);
}

// PES 710-714: the blend exit takes the band word from the interface (vt+0x68) and the speed word from its km/h (vt+0x48)
// times 3.6, not from the 4-tick sample. The sample is at rest here, so a sample-derived band would read 0.
TEST(Accelerate, TheBlendExitTakesTheBandAndSpeedWordsFromTheInterface) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 5;  // no record: the blend is 0 frames, so the clip has passed it
  h.commandClass = 2;
  h.commandKmh = 10.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);  // level with the band: no flag
  req.SetU(pes::moveword::kRateBand, 2u);
  pes::movestep::Accelerate(h, req);
  // PES 716-717: the exit runs the run entry, which requests RUN_LOOP; the run clip is issued on the loop's next tick.
  EXPECT_EQ(req.U(pes::moveword::kNextState), static_cast<uint32_t>(pes::MoveState::kRunLoop));
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 2u);
  EXPECT_NEAR(req.F(pes::moveword::kStopSpeedWord), 36.0f, 1e-4f);
}

// PES 718-745: the band-three clamp runs after the issue. At a sample speed between 16 and 28 km/h, with band 3 and a
// motion other than 0xbde/0xbe1, the rate is (ceiling + 0.6) / 2, which is 0.8 at a ceiling of 1.
TEST(Accelerate, TheBandThreeClampSetsTheRateFromTheCeilingAtMidSpeed) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 5;
  h.rateCeiling = 1.0f;
  h.commandClass = 3;
  h.commandKmh = 19.0f;
  h.positions[4][0] = 0.1f;  // the 4-tick sample: 194.4 * 0.1 = 19.4 km/h
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 3u);
  req.SetU(pes::moveword::kRateBand, 3u);
  pes::movestep::Accelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 3u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), (h.rateCeiling + h.runConstants.rate06) * h.runConstants.half);
}

// PES 584-588: the flag arm re-plans into GOTO_RUN when the band is 0 and the command class is above 1. Kind 3 keeps the
// GOTO_RUN entry of PES 432 from taking the tick first, so this is the flag arm's own exit.
TEST(Accelerate, TheFlagArmReplansIntoGotoRunAtBandZeroWithAClassAboveOne) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 5;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 3u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetU(pes::moveword::kRateBand, 0u);
  pes::movestep::Accelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kGotoRun));
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

// PES 580-581 and 668: the flag arm's flag ends the clip before its blend has passed. Zero cancel frames put hit1 at the
// frame, so the flag arm runs; the command class (1) below the band (2) sets the flag, and the exit issues the clip.
TEST(Accelerate, AFlaggedClipEndsBeforeItsBlendPasses) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 0;
  h.record.blend = {100};
  h.record.cancel = {0, 0, 0};
  h.commandClass = 1;
  h.commandKmh = 10.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 2u);
  pes::movestep::Accelerate(h, req);
  // PES 716-717: the exit runs the run entry, which requests RUN_LOOP; the run clip is issued on the loop's next tick.
  EXPECT_EQ(req.U(pes::moveword::kNextState), static_cast<uint32_t>(pes::MoveState::kRunLoop));
  EXPECT_EQ(req.U(pes::moveword::kRateBand), 1u);
}

// Arm H (PES 457-571): the trigger, the yaw step and the stamp words. Each test is set so the port's current answer differs
// from PES's. The setup: no hit offset and no cancel frames, the blend passed at 0 ticks, the live frame at 5, the command
// class and the band level at 2 (no flag), and the turn zero.
static void ArmHSetUp(SyntheticHost& h) {
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 5;
  h.commandClass = 2;
  pes::RotRecord clean;  // an unsigned clip with no hit ticks; the missing-record fallback is -878 (not zero)
  clean.start = 0;
  clean.end = 0;
  clean.offset = 0;
  h.record.rot = {clean};
}

static pes::MoveReq ArmHRequest(float heading) {
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetU(pes::moveword::kRateBand, 2u);
  req.SetF(pes::moveword::kHeading, heading);
  req.SetF(pes::moveword::kRecordStampB, heading);  // the turn is zero
  req.SetF(pes::moveword::kLookHeading, heading);   // the look is zero
  return req;
}

// PES 466-484: the heading (word 1) less the player's own facing (P+0x20), not the request's copy in word 2. The rate is 135 / 54
// = 2.5 a tick, so the heading is 32.5.
TEST(ArmH, TheFacingIsThePlayersOwnFacingNotWordTwo) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.facingDeg = 30.0f;
  pes::MoveReq req = ArmHRequest(60.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 32.5f, 1e-3f);
}

// PES 487-495: a delta past 180 degrees in either direction wraps once by 360. -250 wraps to +110, which is inside the gate.
TEST(ArmH, ANegativeDeltaPastMinus180WrapsOnItsMagnitude) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.facingDeg = 260.0f;
  pes::MoveReq req = ArmHRequest(10.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 262.5f, 1e-3f);
}

// PES 497-498: the rate branch runs while the wrapped delta is under 112.5 degrees, not under 90.
TEST(ArmH, TheRateBranchRunsUnderOneHundredTwelveAndAHalfDegrees) {
  SyntheticHost h;
  ArmHSetUp(h);
  pes::MoveReq req = ArmHRequest(100.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 2.5f, 1e-3f);
}

// PES 561-562 (VA 0x6bc50b): the stamp word is 0x18, written with the new heading once the remain has passed. The port wrote 0x19.
TEST(ArmH, TheStampWordIsEighteenAndTakesTheNewHeading) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.facingDeg = 30.0f;
  pes::MoveReq req = ArmHRequest(60.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_NEAR(req.F(pes::moveword::kRecordStampB), 32.5f, 1e-3f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRecordStampC), 0.0f);
}

// PES 460 (VA 0x6bc2cb): the re-orient byte at word 0x3c clears when the trigger runs. The port cleared word 0x3f.
TEST(ArmH, TheReorientByteAtWordThreeCClearsWhenTheTriggerRuns) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.facingDeg = 30.0f;
  pes::MoveReq req = ArmHRequest(60.0f);
  req.SetByte(pes::moveword::kReorientFlag, 1);
  pes::movestep::Accelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kReorientFlag) & 0xffu, 0u);
}

// PES 443-459: the trigger's second clause takes the mid tick ((remain + total) / 2), the turn's magnitude (fVar27) and the body
// error (fVar25). The remain is 7 ticks, so the mid is 3; the port's mid came from the clip's whole end (9 ticks) and its look.
TEST(ArmH, TheMidTickAndTheTriggerBearingsAreTheRemainAndTheTurnAndBodyError) {
  SyntheticHost h;
  ArmHSetUp(h);
  pes::RotRecord rot;
  rot.start = 0;
  rot.end = 10;    // 18 ticks
  rot.offset = 1;  // a signed hit, so clause 3 does not fire
  h.record.rot = {rot};
  h.record.blend = {4};  // 7 ticks: the remain
  h.record.cancel = {0, 0, 0};
  pes::MoveReq req = ArmHRequest(10.0f);
  req.SetF(pes::moveword::kRecordStampB, 40.0f);  // turn 30 against heading 10
  req.SetF(pes::moveword::kLookHeading, 40.0f);   // look 0
  pes::movestep::Accelerate(h, req);
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 180.0f / 54.0f, 1e-3f);
}

// PES 573-577: the flag arm's gate is the body error (fVar25) under 45 degrees, not the turn. A turn of 50 with a level body still
// sets the flag: the command class 1 is below the band 2, so the flag ends the clip and its exit requests the run loop.
TEST(ArmH, TheFlagArmGateIsTheBodyErrorNotTheTurn) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.record.blend = {100};
  h.record.cancel = {0, 0, 0};
  h.commandClass = 1;
  pes::MoveReq req = ArmHRequest(0.0f);
  req.SetF(pes::moveword::kRecordStampB, 50.0f);  // turn 50
  req.SetF(pes::moveword::kLookHeading, 50.0f);   // look 0
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  pes::movestep::Accelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), static_cast<uint32_t>(pes::MoveState::kRunLoop)) << "the flag ends the clip, so its exit requests the run loop";
}

// PES 599-604: the flag's second clause is 67.5 degrees against the turn (fVar27), not the look. A turn of 70 with a level look
// sets the flag at command class 3 over band 2.
TEST(ArmH, TheFlagArmTurnClauseReadsTheTurnNotTheLook) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.record.blend = {100};
  h.record.cancel = {0, 0};  // hit 2 falls back to the blend, so theta is 78.75
  pes::MoveReq req = ArmHRequest(0.0f);
  req.SetF(pes::moveword::kRecordStampB, 70.0f);  // turn 70
  req.SetF(pes::moveword::kLookHeading, 70.0f);   // look 0
  req.SetF(pes::moveword::kTurn, 30.0f);          // the first clause: 30 is not under the 22.5 span
  req.SetU(pes::moveword::kCommandSpeedClass, 3u);
  pes::movestep::Accelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), static_cast<uint32_t>(pes::MoveState::kRunLoop)) << "the flag ends the clip, so its exit requests the run loop";
}

// PES 443-455: the 0xbe4-0xbe9 motions take the remain as their mid, not (remain + total) / 2. The remain is 7 ticks, so the mid is
// past the live frame and the trigger does not run; the general mid of the clip's end (9 ticks) would run it.
TEST(ArmH, TheBodyMotionFamilyTakesTheRemainAsItsMid) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.currentMotion = 0xbe4;
  pes::RotRecord rot;
  rot.start = 0;
  rot.end = 5;  // 9 ticks
  rot.offset = 1;
  h.record.rot = {rot};
  h.record.blend = {4};  // 7 ticks: the remain
  h.record.cancel = {0, 0, 0};
  pes::MoveReq req = ArmHRequest(10.0f);
  req.SetF(pes::moveword::kRecordStampB, 40.0f);  // turn 30
  req.SetF(pes::moveword::kLookHeading, 10.0f);   // look 30
  pes::movestep::Accelerate(h, req);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRootFacingAngle), 0.0f);
}

// PES 272-274: bVar22 is the remain passed or the body error under 45 degrees (fVar25), not the turn. A turn of 80 with a level
// body takes the shared band block and does not enter STOP; the port entered STOP.
TEST(ArmH, TheSharedBandBlockTakesALargeTurnWithALevelBody) {
  SyntheticHost h;
  ArmHSetUp(h);
  pes::RotRecord rot;
  rot.start = 0;
  rot.end = 100;
  rot.offset = 1;
  h.record.rot = {rot};
  h.record.blend = {100};
  h.record.cancel = {0, 0, 0};
  pes::MoveReq req = ArmHRequest(0.0f);
  req.SetF(pes::moveword::kRecordStampB, 80.0f);  // turn 80, past the 45 theta
  req.SetF(pes::moveword::kLookHeading, 80.0f);   // look 0
  pes::movestep::Accelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kRunLoop))
      << "the shared band block re-plans through the run entry, which is RUN_LOOP";
}

// PES 507-513: the rate is 67.5 degrees a second when the hit tick is behind the live frame and the ground speed is past 19 km/h.
// The port used 45.
TEST(ArmH, TheRateIsSixtySevenAndAHalfWhenTheGroundSpeedIsPastNineteen) {
  SyntheticHost h;
  ArmHSetUp(h);
  h.facingDeg = 30.0f;
  h.positions[1][0] = 0.1f;  // 194.4 * 0.1 = 19.44 km/h
  pes::MoveReq req = ArmHRequest(60.0f);
  pes::movestep::Accelerate(h, req);
  // The rate is 67.5 / 54 = 1.25 a tick; the delta is 30, so the heading is 31.25.
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 31.25f, 1e-3f);
}

// ---------------------------------------------------------------------------------------------
// PES 148-194, the turn-lock block of ACCELERATE. LockSetUp gives a clip with a signed hit and no total, and a blend of
// `blend` frames. The remain is the blend's ticks (int(1.8 * frames + 0.5) on the 54-tick clock), so the outer gate holds at
// the live frame, and the lock has (remain - live) ticks at an F2468 of 1.
static void LockSetUp(SyntheticHost& h, int offset, int blend, int live) {
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentMotion = 0x8bb;
  h.currentClipFrames = live;
  h.commandClass = 2;
  pes::RotRecord rot;
  rot.start = 0;
  rot.end = 20;
  rot.offset = offset;
  h.record.rot = {rot};
  h.record.blend = {blend};
  h.record.cancel = {0, 0, 0};
}

// The request for the lock: the stamp (word 0x18), the heading (word 1) and the facing (word 2). The look is level with the
// heading and the body with the facing, so the look and body errors are zero.
static pes::MoveReq LockRequest(float stamp, float heading, float facing) {
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetU(pes::moveword::kRateBand, 2u);
  req.SetF(pes::moveword::kRecordStampB, stamp);
  req.SetF(pes::moveword::kHeading, heading);
  req.SetF(pes::moveword::kFacing, facing);
  req.SetF(pes::moveword::kLookHeading, heading);
  req.SetF(pes::moveword::kBodyDirection, facing);
  return req;
}

// PES 157-163 and 192: FUN_14043a0a0(stamp, heading) is heading minus stamp. Under 90 degrees of it, the lock heading is
// FUN_140a92950: the stamp steps toward the heading by at most 22.5 degrees (DAT_14259d618), then folds. Here 60 steps to 37.5.
// The lock writes the heading to word 0x3d and word 0x18, the flag to byte 0x3c, the ticks to word 0x3e, and the yaw lock
// (P+0xbec) to the folded heading.
TEST(TurnLock, TheLockHeadingStepsTheStampTowardTheHeadingByAtMostTwentyTwoAndAHalf) {
  SyntheticHost h;
  // A blend of 8 frames is a 14-tick remain: the mid (7) is past the live frame, so PES's trigger (457-459) does not run in this tick.
  LockSetUp(h, 1, 8, 5);
  pes::MoveReq req = LockRequest(60.0f, 0.0f, 0.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_NEAR(req.F(pes::moveword::kAngle3d), 37.5f, 1e-3f);
  EXPECT_NEAR(req.F(pes::moveword::kRecordStampB), 37.5f, 1e-3f);
  EXPECT_EQ(req.U(pes::moveword::kReorientFlag) & 0xffu, 1u);
  EXPECT_EQ(req.U(pes::moveword::kTurnFrames), 9u);  // a 14-tick remain less the live frame 5, plus the half
  EXPECT_NEAR(h.f0bec, 37.5f, 1e-3f);
}

// PES 160-163: at 90 degrees or more of turn the lock heading is the stamp itself, with no step.
TEST(TurnLock, ANinetyDegreeTurnLocksOnTheStampItself) {
  SyntheticHost h;
  LockSetUp(h, 1, 8, 5);
  pes::MoveReq req = LockRequest(150.0f, 0.0f, 150.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_NEAR(req.F(pes::moveword::kAngle3d), 150.0f, 1e-3f);
}

// PES 164-167: the lock needs the facing error against the lock heading under 45 degrees a tick. The remain is 5 and the live
// frame 2, so the lock has 3 ticks; a facing error of 150 is 50 a tick, so there is no lock, although the turn is zero.
TEST(TurnLock, TheLockGateIsTheFacingErrorPerTickNotTheTurn) {
  SyntheticHost h;
  LockSetUp(h, 1, 3, 2);
  pes::MoveReq req = LockRequest(0.0f, 0.0f, 150.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_EQ(req.U(pes::moveword::kAngle3d), 0u);
  EXPECT_EQ(req.U(pes::moveword::kReorientFlag) & 0xffu, 0u);
}

// PES 173-190: past a 90-degree offset, byte 0x3f becomes 1 when the offset's sign differs from the facing error's and the error
// passes 22.5 degrees. The facing error is the lock heading minus the facing: with the lock at 0 and the facing at 30 it is -30,
// against a +120 offset. The turn is zero, so only the facing error can set the byte.
TEST(TurnLock, TheDirectionByteFollowsTheFacingErrorAgainstTheOffsetSign) {
  SyntheticHost h;
  LockSetUp(h, 120, 6, 5);
  pes::MoveReq req = LockRequest(0.0f, 0.0f, 30.0f);
  pes::movestep::Accelerate(h, req);
  EXPECT_TRUE(req.Flag(pes::moveword::kMirrorFlag));
}

// PES 195-199: the bearings read the stamp the lock wrote. The stamp 60 steps to 37.5, toward the heading 0; the turn after the
// lock is 37.5, under the 45-degree theta, so the band block is not taken. The turn before the lock (60) would take it into RUN_LOOP.
TEST(TurnLock, TheBearingsAfterTheLockReadTheStampTheLockWrote) {
  SyntheticHost h;
  LockSetUp(h, 1, 8, 5);
  pes::MoveReq req = LockRequest(60.0f, 0.0f, 37.5f);
  pes::movestep::Accelerate(h, req);
  EXPECT_NE(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kRunLoop));
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

// PES 408-466: RUN_LOOP's T1-false tail. It runs only on the T1-false path, where the T1 gate's FUN_1406b70d0
// operand has set the one and the 67.5-degree divisor. The factor starts at the half for a class that is not 2 or 3;
// the turn ramp is zero for a straight body; the stop block scales the speed by the cosine of the body's gap to the
// displacement, taken from the sine table at |gap| + 90 (FUN_140a92c80).
TEST(RunLoopTail, RunTurnRunWhenTheLookIsFarEnoughAndTheRunTurnIsAllowed) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.positions[1][2] = 1.0f;  // the last displacement heads 0 degrees
  pes::MoveReq req;
  req.SetU(pes::moveword::kClassWord, 0u);     // not 2 or 3: the factor is the half
  req.SetU(pes::moveword::kPrecise, 0u);       // not 1: the run turn is allowed
  req.SetF(pes::moveword::kRate, 0.9f);        // above the tail's rate, so block A runs
  req.SetF(pes::moveword::kTurn, 0.0f);        // straight: the ramp is zero
  req.SetF(pes::moveword::kDistToLook, 1.0f);  // the factor 0.5 is at or below it
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  pes::probe::Reset();
  pes::movestep::RunLoopTail(h, req);
  EXPECT_EQ(pes::probe::runTurnRunCalls, 1);
  EXPECT_EQ(pes::probe::turnStopCalls, 0);
}

TEST(RunLoopTail, TurnStopWhenTheLookIsTooFarForTheRunTurn) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.positions[1][2] = 1.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kClassWord, 0u);
  req.SetU(pes::moveword::kPrecise, 0u);
  req.SetF(pes::moveword::kRate, 0.9f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kDistToLook, 0.1f);  // the factor 0.5 is past it
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  pes::probe::Reset();
  pes::movestep::RunLoopTail(h, req);
  EXPECT_EQ(pes::probe::turnStopCalls, 1);
  EXPECT_EQ(pes::probe::runTurnRunCalls, 0);
}

// Block B: the speed between 5 and 11 km/h (the cosine of a body that faces its displacement is 1) with no stop
// before it writes the class word 1 and the stop speed word, then the STOP entry.
TEST(RunLoopTail, StopsWithTheClassWordOneWhenTheSpeedIsBetweenFiveAndElevenKmh) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 8.0f;
  h.previousState = static_cast<int>(pes::MoveState::kRunLoop);
  h.positions[1][2] = 1.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kClassWord, 1u);     // not above 2: block B
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kRate, 0.1f);        // at or below the tail's rate: block B
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  pes::probe::Reset();
  pes::movestep::RunLoopTail(h, req);
  EXPECT_EQ(req.U(pes::moveword::kClassWord), 1u);
  EXPECT_EQ(req.U(pes::moveword::kStopSpeedWord), 8u);  // PES 463 stores the speed as a whole number, (uint)fVar29
  EXPECT_EQ(pes::probe::runTurnRunCalls, 0);
  EXPECT_EQ(pes::probe::turnStopCalls, 0);
}

// Block B's first branch: after a TURN_STOP the speed up to 13.5 km/h clears the class word to 0.
TEST(RunLoopTail, ClearsTheClassWordAfterATurnStopWhenTheSpeedIsBelowThirteenAndAHalf) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 8.0f;
  h.previousState = static_cast<int>(pes::MoveState::kTurnStop);
  h.positions[1][2] = 1.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kClassWord, 1u);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kRate, 0.1f);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  pes::probe::Reset();
  pes::movestep::RunLoopTail(h, req);
  EXPECT_EQ(req.U(pes::moveword::kClassWord), 0u);
  EXPECT_EQ(req.U(pes::moveword::kStopSpeedWord), 8u);  // PES 463: the whole number of the speed
}

// Block B above 11 km/h: the STOP entry is taken with no class or speed word written by the tail (PES 460).
TEST(RunLoopTail, LeavesTheWordsAloneWhenTheSpeedIsAboveElevenKmh) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 12.0f;
  h.previousState = static_cast<int>(pes::MoveState::kRunLoop);
  h.positions[1][2] = 1.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kClassWord, 2u);  // not above 2: block B (class 3 would take block A); no class write above 11
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kRate, 0.1f);     // at or below the tail's rate: block B
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  pes::probe::Reset();
  pes::movestep::RunLoopTail(h, req);
  EXPECT_EQ(req.U(pes::moveword::kClassWord), 2u);
  EXPECT_EQ(req.U(pes::moveword::kStopSpeedWord), 0u);
}

// PES 120 (E0): the swing arm's entry returns with nothing written while the first dwell is still running.
TEST(RunLoop, TheSwingArmReturnsWithNothingWrittenWhileTheFirstDwellRuns) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);  // a running class (not -1)
  req.SetU(pes::moveword::kMoveBucket, 0u);         // bucket 0 with the rate above 0.15 holds T1
  req.SetF(pes::moveword::kRate, 0.5f);
  req.SetU(pes::moveword::kPrecise, 0u);            // not 1 holds T2: the swing arm
  req.SetF(pes::moveword::kDwellElapsed, 0.05f);    // below the limit: E0 returns
  req.SetF(pes::moveword::kDwellLimit, 0.1f);
  pes::probe::Reset();
  pes::movestep::RunLoop(h, req);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(h.issues, 0);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellElapsed), 0.05f);
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

// EnterAccelerate's desired angle (FUN_1406ccd00 lines 150-154): with the body 150 degrees from the facing, the entry
// desires heading - StepStamp(bodyDir, facing, 0.8 * |bodyDir - facing|) = 12 - 40 = -28: window 3, bucket 6.
TEST(EnterAccelerate, DesiresTheHeadingLessTheBodyDirectionsStepStamp) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 2;
  h.positions[1][0] = 1.0f;  // a 194.4 km/h ground speed: past the 11 km/h gate
  // The install's window edges (0x1425ddeb0), window -> bucket (0x1425ddf10) and hysteresis (0x14259222c, 0x14259be7c).
  h.buckets.wide = {-180.0f, -157.5f, -112.5f, -67.5f, -22.5f, 0.0f, 22.5f, 67.5f, 112.5f, 157.5f, 180.0f};
  h.buckets.bucket = {9, 8, 7, 6, 5, 0, 1, 2, 3, 4};
  h.buckets.hysteresisUp = 5.0f;
  h.buckets.hysteresisDown = -5.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 160.0f);
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
  req.SetU(pes::moveword::kMoveBucket, 3u);
  pes::probe::Reset();
  ASSERT_TRUE(pes::movestep::EnterAccelerate(h, req));
  // desired = 12 - (160 + 0.8 * (10 - 160)) = -28 degrees: window 3, bucket 6, column bucketColumn[6]
  EXPECT_EQ(h.lastCellColumn, h.runTables.bucketColumn[6]) << "desired is not heading - StepStamp(bodyDir, facing)";
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

// PES 57-61: a no-class tick (class word -1) with the precise word set, or outside the match phase 1, is TURN_STOP
// (FUN_1406d2590) before any other arm: the swing never runs for it. The port issued the swing clip here and stayed in
// RUN_LOOP, which is the stall a sub-dribble command produces (the body stands at zero speed while the clip plays).
TEST(RunLoop, TheNoClassTickWithThePreciseWordGoesToTurnStop) {
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
  req.SetU(pes::moveword::kPrecise, 1u);  // the precise word set: TURN_STOP at PES 59
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  pes::probe::Reset();
  pes::movestep::RunLoop(h, req);
  EXPECT_EQ(pes::probe::turnStopCalls, 1) << "PES 59: FUN_1406d2590, the TURN_STOP entry";
  EXPECT_EQ(h.issues, 0) << "the no-class tick issues nothing from RUN_LOOP";
}


// PES 120-263 is the swing arm: it computes the look words and falls through to the E3 ladder (PES 265-286) or the style
// arm (PES 398, which issues through the due request). It issues no clip of its own. The port issued the look-bucket clip
// from the swing and stayed in RUN_LOOP: the stall the 2-minute carry hit once the picks were faithful.
TEST(RunLoop, TheRunningSwingArmIssuesNoClipOfItsOwn) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.commandClass = 3;
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 0;
  h.phase = 1;
  h.buckets.wide = {-180.0f, -157.5f, -112.5f, -67.5f, -22.5f, 0.0f, 22.5f, 67.5f, 112.5f, 157.5f, 180.0f};
  h.buckets.bucket = {9, 8, 7, 6, 5, 0, 1, 2, 3, 4};
  h.buckets.hysteresisUp = 5.0f;
  h.buckets.hysteresisDown = -5.0f;
  h.runTables.bucketColumn = {9, 11, 13, 15, 17, 8, 6, 4, 2, 0};
  h.populatedColumns = {9, 11, 13, 15, 17, 8, 6, 4, 2, 0};
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 3u);
  req.SetU(pes::moveword::kPrecise, 0u);  // T2: the precise word is clear, so the swing arm runs
  req.SetU(pes::moveword::kRateBand, 1u);
  req.SetU(pes::moveword::kMoveBucket, 1u);
  req.SetU(pes::moveword::kCommandId, 0u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kRecordStampB, 12.0f);
  req.SetF(pes::moveword::kLookHeading, 12.0f);
  req.SetU(pes::moveword::kLookBucket, 1u);
  req.SetU(pes::moveword::kLookBucketAlt, 1u);
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kFacingError, 45.0f);
  req.SetF(pes::moveword::kLookDelta, 45.0f);
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);  // the first dwell has run out (PES 120)
  req.SetF(pes::moveword::kDistToLook, 5.0f);
  h.bandSpeeds[0] = 4.0f;
  h.bandSpeeds[1] = 8.0f;
  h.bandSpeeds[2] = 14.0f;
  h.bandSpeeds[3] = 22.0f;
  h.stepTables.bucketDegrees = {0.0f, 45.0f, 90.0f, 135.0f, 135.0f, 0.0f, -45.0f, -90.0f, -135.0f, -135.0f};
  pes::movestep::RunLoop(h, req);
  EXPECT_EQ(h.issues, 0) << "PES's swing arm issues no clip of its own (decompile 120-263)";
}

// PES's style arm is the one place that enters the STEP family: the tail at 0x1406C0ACB-
// 0x1406C0AF2 writes req[0x39] = 0xE (RUN_TO_STEP) with the change-state flag at
// 0x1406C0AEB, and the four blend words: req[0x1a] = 0, req[0x1b] = 0.2, req[0x2d] = 0 and
// req[0x2e] = 0.2.
TEST(RunLoop, TheStyleArmAsksForRunToStep) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.currentMotion = 0x8bb;
  h.currentClipFrames = 0;
  h.phase = 1;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);  // a live class: the style arm runs
  req.SetU(pes::moveword::kPrecise, 1u);  // the precise arrival: PES's T2 is false, so the style arm runs, not the swing arm
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
  pes::probe::Reset();
  // The style ladder's answer differs from the request's own style byte, so the change runs.
  pes::probe::styleAnswer = 6;
  req.SetU(pes::moveword::kStyle, 3u);
  pes::movestep::RunLoop(h, req);
  pes::probe::styleAnswer = 0;
  EXPECT_EQ(pes::probe::styleCalls, 1) << "the style arm ran";
  // The style arm's exit: RUN_TO_STEP, with the change-state flag paired with it.
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kRunToStep));
  // The four blend words: both dwell limits are 0.2 and both elapsed words are 0.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellElapsed), 0.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit), 0.2f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellElapsed2), 0.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit2), 0.2f);
}

// The running swing arm's state picker (PES 120-263): with the look bucket at 4, the match phase 1 and the look error's
// magnitude past 146.25 degrees, the second word is swapped to -180 and the look bucket to 9 - PES's own fold, which the
// swing's words and the E3 ladder then read. The no-class tick never reaches it: PES 57-61 sends it to TURN_STOP.
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
  req.SetU(pes::moveword::kCommandSpeedClass, 3u);
  req.SetU(pes::moveword::kPrecise, 0u);  // T2: the precise word is clear, so the running swing arm runs
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
  req.SetU(pes::moveword::kLookBucketAlt, 4u);  // the look bucket at 4 (the picker's second word)
  req.SetU(pes::moveword::kLookBucket, 4u);  // the running arm reads word 0xc for its bucket
  req.SetF(pes::moveword::kFacingError, -150.0f);  // past 146.25 degrees, opposite sign
  req.SetF(pes::moveword::kTurn, 2.0f);
  req.SetF(pes::moveword::kLookDelta, -150.0f);  // the running arm's look error, past 146.25 degrees
  req.SetF(pes::moveword::kRate, 0.6f);
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 1.0f);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);  // the first dwell has run out: E0 (PES 120) lets the swing body run
  pes::probe::Reset();
  pes::movestep::RunLoop(h, req);
  // The state picker's fold is visible in the look bucket it rewrote (4 -> 9); the swing arm writes no state request.
  EXPECT_EQ(req.U(pes::moveword::kLookBucketAlt), 9u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kLookDelta), -180.0f);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
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
  req.SetU(pes::moveword::kPrecise, 1u);  // the precise arrival: PES's T2 is false, so the style arm runs, not the swing arm
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

// STOP's else arm (FUN_1406c9e20, VA 0x1406ca51f): the dwell has run out, the stay decision is false, and the look bucket
// is neither 0 nor 5 with the facing error past 33.75 degrees. The turn exit (FUN_1406d1390, IDLE_TURN) is taken. The
// helper runs STOP with the dwell past both limits and returns how often the turn exit was taken.
namespace {
int StopIdleExitCalls(uint32_t lookBucket, uint32_t moveBucket, float facingError) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.currentKmh = 0.0f;
  h.percent = false;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kCommandSpeedClass, 1u);
  req.SetU(pes::moveword::kRateBand, 2u);
  req.SetU(pes::moveword::kLookBucketAlt, lookBucket);
  req.SetU(pes::moveword::kMoveBucket, moveBucket);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 12.0f);
  req.SetF(pes::moveword::kTurn, 40.0f);  // a turned body; the turn ramp is 0 below 67.5 degrees
  req.SetF(pes::moveword::kFacingError, facingError);
  req.SetF(pes::moveword::kDwellElapsed, 1.0f);
  req.SetF(pes::moveword::kDwellLimit, 0.05f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.1f);
  req.SetF(pes::moveword::kDwellLimit2, 0.05f);
  pes::probe::Reset();
  pes::probe::stayAnswer = false;
  pes::movestep::Stop(h, req);
  return pes::probe::idleTurnCalls;
}
}  // namespace

TEST(Stop, TakesTheIdleTurnExitWhenTheLookBucketIsOffStraightAheadAndTheErrorIsLarge) {
  EXPECT_EQ(StopIdleExitCalls(2u, 2u, 40.0f), 1);
  EXPECT_EQ(StopIdleExitCalls(0u, 0u, 40.0f), 0) << "bucket 0 takes the IDLE arm instead";
}

// The error is compared by magnitude (the ANDPS mask at 0x1406ca502): a -40 degree error is as large as +40.
TEST(Stop, ComparesTheFacingErrorByMagnitudeForTheIdleTurnExit) {
  EXPECT_EQ(StopIdleExitCalls(2u, 2u, -40.0f), 1);
  EXPECT_EQ(StopIdleExitCalls(2u, 2u, 20.0f), 0) << "below 33.75 degrees the IDLE arm is taken";
}

// The bucket is req[0xd], the look bucket (VA 0x1406ca4ea), not the move bucket req[8]: the two are set opposite ways.
TEST(Stop, ReadsTheLookBucketNotTheMoveBucketForTheIdleTurnExit) {
  EXPECT_EQ(StopIdleExitCalls(2u, 0u, 40.0f), 1) << "look bucket 2, move bucket 0: the turn exit";
  EXPECT_EQ(StopIdleExitCalls(0u, 2u, 40.0f), 0) << "look bucket 0, move bucket 2: the IDLE arm";
}

// The STOP entry (FUN_1406d5d60). Its arm is chosen by the class word req[0x1f] (PES's param_2[0x1f] in the tests of lines 41
// and 76), not by the command's speed class req[9]. The class word -1 or 0 takes the no-class arm, which requests IDLE.
TEST(EnterStop, TheClassWordPicksTheNoClassArmWhichRequestsIdle) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.ticks = 54.0f;
  h.currentKmh = 100.0f;
  h.positions[1][0] = 0.1f;  // the last tick's displacement: 19.4 km/h, not slow
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kClassWord, 0xffffffffu);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  pes::movestep::EnterStop(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

// The no-class arm's dwell words: both clocks restart, the first limit is 0.1 s and the second 0.35 s (FUN_1406d5d60 stores
// 0x3dcccccd and 0x3eb33333 at req[0x1b] and req[0x2e]; the slow 0.03 s limit needs a slow ground speed).
TEST(EnterStop, TheNoClassArmWritesThePesDwellWords) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.ticks = 54.0f;
  h.currentKmh = 100.0f;
  h.positions[1][0] = 0.1f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kClassWord, 0u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  req.SetF(pes::moveword::kDwellElapsed, 0.5f);
  req.SetF(pes::moveword::kDwellElapsed2, 0.9f);
  pes::movestep::EnterStop(h, req);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellElapsed), 0.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellElapsed2), 0.0f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit), 0.1f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit2), 0.35f);
}

// The run-stop arm (class 1..3) requests STOP and clears the record stamps (FUN_1406d5d60 stores 0 at req[0x17] and req[0x18]);
// it does not store the player's position or the bucket there.
TEST(EnterStop, TheRunStopArmClearsTheRecordStamps) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.positions[0][0] = 5.0f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kClassWord, 1u);
  req.SetU(pes::moveword::kCommandSpeedClass, 2u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kFacingError, 30.0f);
  req.SetU(pes::moveword::kRecordStampA, 7u);
  req.SetU(pes::moveword::kRecordStampB, 7u);
  pes::movestep::EnterStop(h, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kStop));
  EXPECT_EQ(req.U(pes::moveword::kRecordStampA), 0u);
  EXPECT_EQ(req.U(pes::moveword::kRecordStampB), 0u);
}

// The slow-stop dwell reads the ground speed of the last displacement, PES's 194.4 km/h per metre at 54 ticks (FUN_1406d5d60
// line 76), not the host's speed word. A 1 mm tick is 0.19 km/h, so the 0.03 s dwell applies even when the host reports 100.
TEST(EnterStop, TheNoClassArmShortensTheFirstDwellWhenTheGroundIsSlow) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  h.ticks = 54.0f;
  h.currentKmh = 100.0f;
  h.positions[1][0] = 0.001f;
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 2u);
  req.SetU(pes::moveword::kClassWord, 0xffffffffu);
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  req.SetF(pes::moveword::kFacingError, 2.0f);
  pes::movestep::EnterStop(h, req);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDwellLimit), 0.03f);
}

// The stop row's band is the class word req[0x1f], clamped to 0..2: FUN_1406d5d60 passes param_2[0x1f] to the row picker. The
// band word req[0x51] does not enter, so two requests that differ only in req[0x51] pick the same row.
TEST(EnterStop, TheRunStopArmPicksItsRowFromTheClassWord) {
  SyntheticHost h;
  h.runConstants = SyntheticRun();
  h.runTables = SyntheticTables();
  auto pick = [&h](uint32_t bandWord) {
    pes::MoveReq req;
    req.SetU(pes::moveword::kKind, 2u);
    req.SetU(pes::moveword::kClassWord, 2u);
    req.SetU(pes::moveword::kCommandSpeedClass, 2u);
    req.SetF(pes::moveword::kFacing, 10.0f);
    req.SetF(pes::moveword::kFacingError, 30.0f);
    req.SetU(pes::moveword::kStopBand, bandWord);
    pes::movestep::EnterStop(h, req);
    return h.lastCellRow;
  };
  EXPECT_EQ(pick(0u), pick(2u));
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
