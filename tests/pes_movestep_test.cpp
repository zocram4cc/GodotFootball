// PES 2021's MOVESTEP record and its playback writers (docs/re/pes-re-anim-select.md
// 1-2, 4). The formulas are arithmetic over install constants: synthetic constants
// exercise them, and one install test checks the loaded constants through their
// relations, not pinned numbers.
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <cmath>
#include <cstring>

#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_turn.hpp"
#include "pes/locomotion.hpp"
#include "pes/uniform.hpp"
#include "pes/vfs.hpp"

namespace {

pes::PlaybackConstants Synthetic() {
  pes::PlaybackConstants k{};
  k.gLow = 1.0f;
  k.gHigh = 1.5f;
  k.gSlope = -0.5f;
  k.one = 1.0f;
  k.stepSlope = 0.3f;
  k.stepBase = 0.5f;
  k.turnP2 = 0.4f;
  k.turnT = 0.1f;
  return k;
}

}  // namespace

TEST(MoveStepRecord, StartsAtInitWithZeroedClassBandAndCeiling) {
  const pes::MoveStepRecord r;
  EXPECT_EQ(r.state, pes::MoveState::kInit);
  EXPECT_EQ(r.previous, pes::MoveState::kInit);
  EXPECT_EQ(r.speedClass, 0);
  EXPECT_EQ(r.footBand, 0);
  EXPECT_EQ(r.rateCeiling, 0.0f);
  EXPECT_EQ(r.rateStamp, 0);
  EXPECT_EQ(r.playbackSpeed, 1.0f);
}

TEST(MoveStepRecord, EnterKeepsTheStateItLeftAsPrevious) {
  pes::MoveStepRecord r;
  r.Enter(pes::MoveState::kIdle);
  EXPECT_EQ(r.state, pes::MoveState::kIdle);
  EXPECT_EQ(r.previous, pes::MoveState::kInit);
  r.Enter(pes::MoveState::kStepLoop);
  EXPECT_EQ(r.state, pes::MoveState::kStepLoop);
  EXPECT_EQ(r.previous, pes::MoveState::kIdle);
}

TEST(MoveStepRecord, EnterInitKeepsTheStateItLeftAsPrevious) {
  pes::MoveStepRecord r;
  r.Enter(pes::MoveState::kRunLoop);
  r.EnterInit();
  EXPECT_EQ(r.state, pes::MoveState::kInit);
  EXPECT_EQ(r.previous, pes::MoveState::kRunLoop);
}

TEST(MoveState, HasTwentyFourStatesWithDemoLast) {
  EXPECT_EQ(static_cast<int>(pes::MoveState::kTimingStep), 22);
  EXPECT_EQ(static_cast<int>(pes::MoveState::kDemo), pes::kMoveStateCount - 1);
}

TEST(StoreRateCeiling, RecomputesOnlyWhenTheMatchTickMoves) {
  pes::MoveStepRecord r;
  pes::StoreRateCeiling(r, 5, 1.0f);
  EXPECT_EQ(r.rateStamp, 5);
  EXPECT_FLOAT_EQ(r.rateCeiling, 1.0f);
  pes::StoreRateCeiling(r, 5, 0.9f);  // same tick: the guard keeps 1.0
  EXPECT_FLOAT_EQ(r.rateCeiling, 1.0f);
  pes::StoreRateCeiling(r, 6, 0.9f);  // next tick: stored
  EXPECT_FLOAT_EQ(r.rateCeiling, 0.9f);
}

TEST(StepPlayback, RunsFromBasePlusSlopeAsTheScaleFallsToBase) {
  const auto k = Synthetic();
  EXPECT_FLOAT_EQ(pes::StepPlayback(0.9f, k), 0.8f);    // G <= gLow: t = 1
  EXPECT_FLOAT_EQ(pes::StepPlayback(1.5f, k), 0.5f);    // G >= gHigh: t = 0
  EXPECT_FLOAT_EQ(pes::StepPlayback(1.25f, k), 0.65f);  // t = 0.5
}

TEST(TurnPlayback, AddsTheSquaredAbilityAndTheScaleTermToTheBase) {
  const auto k = Synthetic();
  EXPECT_FLOAT_EQ(pes::TurnPlayback(0.9f, 0.5f, k), 0.5f + 0.4f * 0.25f + 0.1f);  // t = 1
  EXPECT_FLOAT_EQ(pes::TurnPlayback(1.5f, 0.5f, k), 0.5f + 0.4f * 0.25f);         // t = 0
  EXPECT_FLOAT_EQ(pes::TurnPlayback(1.25f, 0.0f, k), 0.5f + 0.1f * 0.5f);         // t = 0.5
}

TEST(PlaybackInstall, LoadsTheExeConstantsWithTheirRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::LoadPlaybackConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  EXPECT_LT(k->gLow, k->gHigh);
  EXPECT_NEAR(k->gSlope, k->gLow - k->gHigh, 1e-4f);
  EXPECT_EQ(k->one, 1.0f);
  EXPECT_GT(pes::StepPlayback(k->gLow, *k), pes::StepPlayback(k->gHigh, *k))
      << "playback falls as the physique scale rises";
}

TEST(RunRateGuard, AllowsARunOnlyUpToStepBaseOfTheCeiling) {
  const auto k = Synthetic();  // stepBase = 0.5 in the synthetic set
  EXPECT_TRUE(pes::RunRateAllowed(0.5f, 1.0f, k.stepBase));   // rate equals 0.5 * ceiling
  EXPECT_TRUE(pes::RunRateAllowed(0.3f, 1.0f, k.stepBase));
  EXPECT_FALSE(pes::RunRateAllowed(0.6f, 1.0f, k.stepBase));  // above the floor: no run
  EXPECT_FALSE(pes::RunRateAllowed(0.4f, 0.5f, k.stepBase));  // a lower ceiling forbids the same rate
}

TEST(MoveReqFields, FloatsAndBytesShareTheirWord) {
  pes::MoveReq req;
  req.SetF(pes::moveword::kDuration, 0.25f);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDuration), 0.25f);
  const float value = 0.25f;
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof bits);
  EXPECT_EQ(req.U(pes::moveword::kDuration), bits);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
  req.SetByte(pes::moveword::kChangeState, 1);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
}

TEST(ApplyTransition, EntersTheRequestedStateOnlyWhenTheFlagIsSet) {
  pes::MoveStepRecord r;
  r.Enter(pes::MoveState::kIdle);
  pes::MoveReq req;
  pes::ApplyTransition(r, req);
  EXPECT_EQ(r.state, pes::MoveState::kIdle);
  pes::RequestState(req, pes::MoveState::kRunLoop);
  pes::ApplyTransition(r, req);
  EXPECT_EQ(r.state, pes::MoveState::kRunLoop);
  EXPECT_EQ(r.previous, pes::MoveState::kIdle);
}

namespace {
pes::SpeedClassConstants SyntheticSpeed() {
  pes::SpeedClassConstants k{};
  k.full = 360.0f;
  k.wrapFull = -360.0f;
  k.half = 180.0f;
  k.wrapHalf = -180.0f;
  k.neg = -1.0f;
  k.one = 1.0f;
  k.quarter = 90.0f;
  k.tabScale = 512.0f;
  k.lo0 = 1.0f;  // synthetic class bounds, not PES's values
  k.lo1 = 2.0f;
  k.hi0 = 3.0f;
  k.hi1 = 4.0f;
  const double kHalfPi = 1.5707963267948966;
  for (int i = 0; i <= 512; ++i) k.sinQuarter[i] = float(std::sin(i * kHalfPi / 512.0));
  return k;
}
}  // namespace

TEST(TableSinDeg, IsSineOverTheFullTurnWithTheSignOfTheHalfTurn) {
  const auto k = SyntheticSpeed();
  EXPECT_NEAR(pes::TableSinDeg(30.0f, 1.0f, k), 0.5f, 1e-3f);
  EXPECT_NEAR(pes::TableSinDeg(150.0f, 1.0f, k), 0.5f, 1e-3f);
  EXPECT_NEAR(pes::TableSinDeg(200.0f, 1.0f, k), -0.342f, 1e-3f);
  EXPECT_NEAR(pes::TableSinDeg(90.0f, 2.0f, k), 2.0f, 1e-3f);
  EXPECT_NEAR(pes::TableSinDeg(-30.0f, 1.0f, k), -0.5f, 1e-3f);
}

TEST(WrapHalfTurn, FoldsAnAngleDifferenceIntoMinusHalfToHalfTurn) {
  const auto k = SyntheticSpeed();
  EXPECT_FLOAT_EQ(pes::WrapHalfTurn(190.0f, k), -170.0f);
  EXPECT_FLOAT_EQ(pes::WrapHalfTurn(-190.0f, k), 170.0f);
  EXPECT_FLOAT_EQ(pes::WrapHalfTurn(45.0f, k), 45.0f);
  EXPECT_FLOAT_EQ(pes::WrapHalfTurn(180.0f, k), 180.0f);
}

TEST(SpeedClassOf, ClassifiesTheCosineOfTheTurnAgainstTheBounds) {
  const auto k = SyntheticSpeed();
  EXPECT_EQ(pes::SpeedClassOf(0.5f, 0.0f, k), 0);
  EXPECT_EQ(pes::SpeedClassOf(1.5f, 0.0f, k), 1);
  EXPECT_EQ(pes::SpeedClassOf(2.5f, 0.0f, k), 2);
  EXPECT_EQ(pes::SpeedClassOf(3.5f, 0.0f, k), 3);
  EXPECT_EQ(pes::SpeedClassOf(5.0f, 0.0f, k), 4);
  EXPECT_EQ(pes::SpeedClassOf(5.0f, 90.0f, k), 0);
  EXPECT_EQ(pes::SpeedClassOf(4.0f, 60.0f, k), 1);
  EXPECT_EQ(pes::SpeedClassOf(5.0f, 180.0f, k), 0);
}

TEST(SpeedClassBounds, EachBoundBelongsToTheLowerClass) {
  const auto k = SyntheticSpeed();
  EXPECT_EQ(pes::SpeedClassOf(1.0f, 0.0f, k), 0);  // v == lo0: class 0
  EXPECT_EQ(pes::SpeedClassOf(2.0f, 0.0f, k), 1);  // v == lo1: class 1
  EXPECT_EQ(pes::SpeedClassOf(3.0f, 0.0f, k), 2);  // v == hi0: class 2
  EXPECT_EQ(pes::SpeedClassOf(4.0f, 0.0f, k), 3);  // v == hi1: class 3
}

TEST(SpeedClassInstall, LoadsTheExeTableWithItsRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::LoadSpeedClassConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  EXPECT_FLOAT_EQ(k->half, 2.0f * k->quarter);
  EXPECT_FLOAT_EQ(k->full, 2.0f * k->half);
  EXPECT_FLOAT_EQ(k->tabScale, float(k->sinQuarter.size() - 1));
  EXPECT_LT(k->lo0, k->lo1);
  EXPECT_LT(k->lo1, k->hi0);
  EXPECT_LT(k->hi0, k->hi1);
  const double kHalfPi = 1.5707963267948966;
  for (size_t i = 0; i < k->sinQuarter.size(); ++i)
    EXPECT_NEAR(k->sinQuarter[i], std::sin(double(i) * kHalfPi / 512.0), 1e-5) << "entry " << i;
}

namespace {
pes::AngleBucketConstants SyntheticBuckets() {
  pes::AngleBucketConstants k{};
  k.wide = {-100, -80, -60, -40, -20, 0, 20, 40, 60, 80, 100};  // synthetic edges
  k.narrow = {-100, -90, -60, -40, -10, 0, 10, 40, 60, 90, 100};
  for (int i = 0; i < 10; ++i) k.bucket[i] = i;  // synthetic identity map
  k.hysteresisUp = 3.0f;                         // synthetic magnitude (the exe's is 5)
  k.hysteresisDown = -3.0f;
  return k;
}
}  // namespace

TEST(AngleBucket, PicksTheWindowThatHoldsTheAngleWithoutACurrentBucket) {
  const auto k = SyntheticBuckets();
  const int kNone = 10;  // FUN_1406b9b20 is called with 10 when no bucket is current
  EXPECT_EQ(pes::movestep::AngleBucket(k, 10.0f, kNone, false), 5);   // [0, 20]
  EXPECT_EQ(pes::movestep::AngleBucket(k, -50.0f, kNone, false), 2);  // [-60, -40]
  EXPECT_EQ(pes::movestep::AngleBucket(k, 15.0f, kNone, true), 6);    // narrow [10, 40]
  EXPECT_EQ(pes::movestep::AngleBucket(k, -2.0f, kNone, false), 4);   // [-20, 0]
}

TEST(AngleBucket, TheCurrentBucketWidensAndItsNeighbourShrinksByTheHysteresis) {
  const auto k = SyntheticBuckets();
  EXPECT_EQ(pes::movestep::AngleBucket(k, -2.0f, 5, false), 5);   // window 5 widens down to -3
  EXPECT_EQ(pes::movestep::AngleBucket(k, -4.0f, 5, false), 4);   // window 4 ends at -3 now
  EXPECT_EQ(pes::movestep::AngleBucket(k, 22.0f, 5, false), 5);   // window 5 widens up to 23
  EXPECT_EQ(pes::movestep::AngleBucket(k, 22.0f, 10, false), 6);  // without hysteresis it is window 6
}

TEST(AngleBucketInstall, LoadsTheExeWindowsWithTheirRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::LoadAngleBucketConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  for (size_t i = 0; i < 11; ++i) {
    EXPECT_FLOAT_EQ(k->wide[i], -k->wide[10 - i]) << "wide edge " << i;
    EXPECT_FLOAT_EQ(k->narrow[i], -k->narrow[10 - i]) << "narrow edge " << i;
  }
  for (size_t i = 0; i + 1 < 11; ++i) {
    EXPECT_LT(k->wide[i], k->wide[i + 1]);
    EXPECT_LT(k->narrow[i], k->narrow[i + 1]);
  }
  int seen[10] = {};
  for (int b : k->bucket) {
    ASSERT_GE(b, 0);
    ASSERT_LT(b, 10);
    ++seen[b];
  }
  for (int i = 0; i < 10; ++i) EXPECT_EQ(seen[i], 1) << "bucket " << i;
  EXPECT_FLOAT_EQ(k->hysteresisDown, -k->hysteresisUp);
}

TEST(StepScalarsInstall, LoadsTheGroupCScalarsWithTheirRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto s = pes::LoadStepScalars(*exe, &err);
  ASSERT_TRUE(s.has_value()) << err;
  EXPECT_FLOAT_EQ(s->one, 1.0f);
  EXPECT_FLOAT_EQ(s->negOne, -1.0f);
  EXPECT_FLOAT_EQ(s->full, 2.0f * s->halfTurn);
  EXPECT_FLOAT_EQ(s->halfTurn, 2.0f * s->deg90);
  EXPECT_FLOAT_EQ(s->deg90, 2.0f * s->deg45);
  EXPECT_FLOAT_EQ(s->neg180, -s->halfTurn);
  // A field with no loader site reads zero; both of these are read by the STEP_LOOP_CHANGE handler.
  EXPECT_GT(s->four, 0.0f);
  EXPECT_GT(s->s0p15, 0.0f);
}

TEST(StepTablesInstall, LoadsTheBucketColumnsWithinTheAnimetable) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto t = pes::LoadStepTables(*exe, &err);
  ASSERT_TRUE(t.has_value()) << err;
  for (int column : t->bucketColumn) {
    EXPECT_GE(column, 0);
    EXPECT_LT(column, 18);  // the Animetable has 18 angle columns (A-AN9)
  }
}

namespace {
// A host that only answers FUN_1406b9e80's two reads (G and the table); every other read is a
// default, because no other test in this file drives a handler.
class RateHost : public pes::MoveHost {
 public:
  pes::MoveCell Cell(int row, int column, bool walkFlag) override {
    ++cellCalls;
    lastCellRow = row;
    lastCellColumn = column;
    lastCellWalk = walkFlag;
    pes::MoveCell c;
    c.motionNo = 0x100 + row;
    c.found = true;
    return c;
  }
  int PickN(int) override { return 0; }
  bool Percent(int) override { return false; }
  int CommandSpeedClass() const override { return 0; }
  float RateCeiling() const override { return 1.0f; }
  float CommandSpeedKmh() const override { return 0.0f; }
  float CurrentSpeedKmh() const override { return 0.0f; }
  float PhysiqueG() const override { return g; }
  float TicksPerSecond() const override { return 54.0f; }
  int MatchTick() const override { return 0; }
  int RecentStamp(int) const override { return 0; }
  float MotionStartTime(int) const override { return 0.0f; }
  float RecordF2468() const override { return 1.0f; }
  int MoveStatePrevious() const override { return 0; }
  float RecordF0bec() const override { return f0bec; }
  void SetRecordF0bec(float value) override { f0bec = value; ++f0becWrites; }
  const pes::AngleBucketConstants& AngleBuckets() const override { return buckets; }
  void PositionAt(int index, float out[3]) const override {
    for (int i = 0; i < 3; ++i) out[i] = index == 0 ? position[i] : 0.0f;
  }
  bool IsReplay() const override { return false; }
  void Issue(const pes::MotionRequest&) override {}
  const pes::StepScalars& StepConstants() const override { return step; }
  float BandSpeedKmh(int, float) const override { return 0.0f; }
  bool MirrorChoice(bool) const override { return false; }
  int FootBand() const override { return 0; }
  const pes::RunConstants& RunConstantsView() const override { return run; }
  const pes::RunTables& RunTablesView() const override { return tables; }
  const pes::SpeedRateTable& SpeedRateTableView() const override { return rate; }
  const pes::movestep::MoveTurnConstants& MoveTurnConstantsView() const override {
    return turnConstants;
  }
  const pes::StepTables& StepTablesView() const override { return stepTables; }
  bool BucketIsLeft(int) const override { return false; }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { return playback; }
  int CurrentMotion() const override { return currentMotion; }
  int CurrentClipFrames() const override { return currentClipFrames; }
  int AnimeState() const override { return animeState; }

  const pes::MotionRecord& Motion(int) const override { return record; }
  bool ClipFlag() const override { return false; }
  const pes::MoveConstants& MoveConstantsView() const override { return moveConstants; }
  const pes::IdleRowTables& IdleRowTablesView() const override { return idleRows; }
  float FacingDeg() const override { return facingDeg; }
  float BallBearingDeg() const override { return ballBearingDeg; }
  int ActionIndex() const override { return actionIndex; }
  int RelatedSlot() const override { return relatedSlot; }
  void RecordPosition(int slot, float out[3]) const override {
    out[0] = recordPosition[slot][0];
    out[1] = recordPosition[slot][1];
    out[2] = recordPosition[slot][2];
  }
  float RecordFacing(int slot) const override { return recordFacing[slot]; }
  int MatchState11d4() const override { return matchState11d4; }
  int MatchState1554() const override { return matchState1554; }
  int PhaseTick() const override { return phaseTick; }
  int MatchModeByte() const override { return matchModeByte; }
  bool MatchObject3b0() const override { return matchObject3b0; }
  bool TrainerFlag1423e7fd0() const override { return trainerFlag; }
  int RecordConditionByte(int) const override { return conditionByte; }
  bool ActionSlotMarked(int) const override { return actionSlotMarked; }
  bool ActionTableOwns(int) const override { return actionTableOwns; }
  int RecordActionA() const override { return recordActionA; }
  int RecordActionB() const override { return recordActionB; }

  float g = 1.0f;
  pes::RunConstants run{};
  pes::RunTables tables{};
  pes::SpeedRateTable rate{};
  pes::movestep::MoveTurnConstants turnConstants{};
  pes::MoveConstants moveConstants{};
  pes::IdleRowTables idleRows{};
  pes::MotionRecord record;  // the motion table entry the host hands the handlers
  float facingDeg = 0.0f;
  float ballBearingDeg = 0.0f;
  int actionIndex = 0;
  int relatedSlot = -1;
  float recordPosition[12][3] = {};
  float recordFacing[12] = {};
  int matchState11d4 = 0;
  int matchState1554 = 0;
  int phaseTick = 0;
  int matchModeByte = 0;
  bool matchObject3b0 = false;
  bool trainerFlag = false;
  int conditionByte = 100;
  bool actionSlotMarked = true;
  bool actionTableOwns = false;
  int recordActionA = 5;
  int recordActionB = 5;
  float f0bec = 0.0f;
  int f0becWrites = 0;
  int cellCalls = 0;
  int lastCellRow = -1;
  int lastCellColumn = -1;
  bool lastCellWalk = false;
  int currentMotion = 0;
  int currentClipFrames = 0;
  int animeState = 0;
  // LocomotionStyle's reads: the open-play phase, the scripted slot, the stamina percentage, the
  // physique height, the ball ring (index 0 = now) and the ball's motion heading.
  int matchPhase = 1;
  bool scriptedSlotOurs = true;
  int staminaPct = 100;
  float heightCm = 180.0f;
  float ballSample[32][3] = {};
  float ballMotionHeadingDeg = 0.0f;
  float position[3] = {};
  int currentState = 0;
  pes::DampingConstants damping{};
  pes::DampingRecord dampingRecord{};
  pes::StyleConstants style{};
  pes::StepScalars step{};
  pes::StepTables stepTables{};
  pes::AngleBucketConstants buckets{};
  pes::PlaybackConstants playback{};
  int MatchPhase() const override { return matchPhase; }
  bool ScriptedSlotIsOurs() const override { return scriptedSlotOurs; }
  int StaminaPercent() const override { return staminaPct; }
  float PlayerHeightCm() const override { return heightCm; }
  void BallSample(int index, float out[3]) const override {
    out[0] = ballSample[index][0];
    out[1] = ballSample[index][1];
    out[2] = ballSample[index][2];
  }
  float BallMotionHeadingDeg() const override { return ballMotionHeadingDeg; }
  const pes::StyleConstants& StyleConstantsView() const override { return style; }
  int CurrentState() const override { return currentState; }
  const pes::DampingConstants& DampingConstantsView() const override { return damping; }
  const pes::DampingRecord& DampingRecordView() const override { return dampingRecord; }
};

pes::SpeedRateTable SyntheticRate() {
  pes::SpeedRateTable t;
  // PES walks high speed first; the records abut, so each vLo is the next record's vHi.
  t.record[0] = {9.0f, 7.0f, 1.0f, 0.9f};
  t.record[1] = {7.0f, 5.0f, 0.9f, 0.7f};
  t.record[2] = {5.0f, 3.0f, 0.7f, 0.5f};
  t.record[3] = {3.0f, 2.5f, 0.5f, 0.45f};
  t.record[4] = {2.5f, 2.2f, 0.45f, 0.4f};
  t.record[5] = {2.2f, 2.0f, 0.4f, 0.38f};
  t.record[6] = {2.0f, 1.8f, 0.38f, 0.35f};
  t.record[7] = {1.8f, 1.6f, 0.35f, 0.32f};
  t.record[8] = {1.6f, 1.4f, 0.32f, 0.3f};
  t.record[9] = {1.4f, 1.0f, 0.5f, 0.15f};
  t.kindScale = 0.295f;
  return t;
}

pes::RunConstants SyntheticAngles() {
  pes::RunConstants k{};
  k.one = 1.0f;
  k.neg = -1.0f;
  k.full = 360.0f;
  k.wrapFull = -360.0f;
  k.half_turn = 180.0f;
  k.deg90 = 90.0f;
  k.deg135 = 135.0f;
  k.deg3375 = 33.75f;
  k.deg1125 = 11.25f;
  k.deg225 = 22.5f;
  k.deg675 = 67.5f;
  k.deg45 = 45.0f;
  k.negHalf = -0.5f;
  k.speed3600 = 3.6f;
  return k;
}
}  // namespace

TEST(AngleDiffDeg, WrapsTheDifferenceIntoMinusHalfToHalfTurn) {
  const auto k = SyntheticAngles();
  EXPECT_FLOAT_EQ(pes::AngleDiffDeg(45.0f, 10.0f, k), 35.0f);
  EXPECT_FLOAT_EQ(pes::AngleDiffDeg(10.0f, 45.0f, k), -35.0f);
  EXPECT_FLOAT_EQ(pes::AngleDiffDeg(0.0f, 350.0f, k), 10.0f);
  EXPECT_FLOAT_EQ(pes::AngleDiffDeg(350.0f, 0.0f, k), -10.0f);
  EXPECT_FLOAT_EQ(pes::AngleDiffDeg(180.0f, 0.0f, k), 180.0f);
}

TEST(FoldFull, FoldsAHeadingIntoTheUnitTurn) {
  const auto k = SyntheticAngles();
  EXPECT_FLOAT_EQ(pes::FoldFull(45.0f, k), 45.0f);
  EXPECT_FLOAT_EQ(pes::FoldFull(370.0f, k), 10.0f);
  EXPECT_FLOAT_EQ(pes::FoldFull(-10.0f, k), 350.0f);
}

TEST(SpeedToRate, WalksTheTableFromHighSpeedToLowAndClampsToOne) {
  RateHost host;
  host.run = SyntheticAngles();
  host.rate = SyntheticRate();
  EXPECT_FLOAT_EQ(pes::movestep::SpeedToRate(host, 36.0f, 0), 1.0f);  // 10 m/s: top knot
  EXPECT_FLOAT_EQ(pes::movestep::SpeedToRate(host, 18.0f, 0), 0.7f);  // 5 m/s: second knot
  EXPECT_FLOAT_EQ(pes::movestep::SpeedToRate(host, 1.8f, 0), 0.0f);   // 0.5 m/s: below the table
}

TEST(SpeedToRate, TheLastRecordTakesTheKindScaleForKindThreeAndSix) {
  RateHost host;
  host.run = SyntheticAngles();
  host.rate = SyntheticRate();
  // 1.2 m/s sits halfway through the last record, so kind 3/6 scales the share rather than
  // walking to the record's own high rate.
  EXPECT_FLOAT_EQ(pes::movestep::SpeedToRate(host, 4.32f, 3), 0.5f * host.rate.kindScale);
  EXPECT_FLOAT_EQ(pes::movestep::SpeedToRate(host, 4.32f, 6), 0.5f * host.rate.kindScale);
  EXPECT_FLOAT_EQ(pes::movestep::SpeedToRate(host, 4.32f, 0), 0.325f);
}

TEST(SpeedRateInstall, LoadsTheKnotsInPesWalkOrder) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto t = pes::LoadSpeedRateTable(*exe, &err);
  ASSERT_TRUE(t.has_value()) << err;
  for (size_t i = 0; i + 1 < t->record.size(); ++i) {
    EXPECT_GT(t->record[i][0], t->record[i + 1][0]) << "vHi record " << i;
    EXPECT_GT(t->record[i][1], t->record[i + 1][1]) << "vLo record " << i;
    EXPECT_GT(t->record[i][2], t->record[i + 1][2]) << "rHi record " << i;
    EXPECT_GT(t->record[i][3], t->record[i + 1][3]) << "rLo record " << i;
  }
  EXPECT_GT(t->kindScale, 0.0f);
  EXPECT_LE(t->kindScale, 1.0f);
}

namespace {
RateHost IdleHost() {
  RateHost host;
  host.run = SyntheticAngles();
  host.run.rate01 = 0.1f;
  host.run.kmh60 = 60.0f;  // the frame conversion's divisor, DAT_14259a764
  host.run.half = 0.5f;    // DAT_142565600, the frame conversion's rounding
  host.step.rate0p9 = 0.9f;
  host.moveConstants.dist2m5 = 2.5f;
  host.moveConstants.dist3 = 3.0f;
  host.moveConstants.near01 = 0.1f;
  return host;
}

// A request whose stay decision refuses: the facing error is past every bearing gate, so the
// idle handlers take their issue arm.
pes::MoveReq StayRefused() {
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 0);
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kFacingError, 200.0f);
  req.SetF(pes::moveword::kDistToTarget, 10.0f);
  return req;
}
}  // namespace

TEST(Dispatch, AdvancesBothDwellClocksBeforeTheStateRuns) {
  RateHost host = IdleHost();
  pes::MoveReq req;
  pes::Dispatch(pes::MoveState::kGotoStep, host, req);
  EXPECT_NEAR(req.F(pes::moveword::kDwellElapsed), 1.0f / 54.0f, 1e-6f);
  EXPECT_NEAR(req.F(pes::moveword::kDwellElapsed2), 1.0f / 54.0f, 1e-6f);
}

TEST(Dispatch, LocksTheFacingAfterTheIdleFamily) {
  for (pes::MoveState state : {pes::MoveState::kIdle, pes::MoveState::kIdleTurn,
                               pes::MoveState::kIdlePkPlayer, pes::MoveState::kIdlePkGk,
                               pes::MoveState::kDemo}) {
    RateHost host = IdleHost();
    pes::MoveReq req = StayRefused();
    req.SetF(pes::moveword::kFacing, 370.0f);
    pes::Dispatch(state, host, req);
    EXPECT_FLOAT_EQ(host.f0bec, 10.0f) << "state " << static_cast<int>(state);
  }
}

TEST(Dispatch, LocksTheHeadingAfterTheRunStepAndTimingStates) {
  for (pes::MoveState state : {pes::MoveState::kRunLoop, pes::MoveState::kStepLoop,
                               pes::MoveState::kStepLoopChange, pes::MoveState::kTimingStep}) {
    RateHost host = IdleHost();
    pes::MoveReq req = StayRefused();
    req.SetF(pes::moveword::kHeading, -10.0f);
    pes::Dispatch(state, host, req);
    EXPECT_FLOAT_EQ(host.f0bec, 350.0f) << "state " << static_cast<int>(state);
  }
}

TEST(Dispatch, LeavesTheYawLockOffAfterTheStatesWithoutIt) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  pes::Dispatch(pes::MoveState::kIdleOther, host, req);
  EXPECT_EQ(host.f0becWrites, 0);
}

TEST(Idle, IssuesTheIdleClipWhenTheStayDecisionRefuses) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  req.SetU(pes::moveword::kLiveKind, 1);
  host.step.rate0p9 = 0.9f;
  pes::movestep::Idle(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
  EXPECT_EQ(req.U(pes::moveword::kMotion), 0x100u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDuration), 0.25f);
  EXPECT_EQ(req.U(pes::moveword::kLiveKind) & 0xffu, 0u);
}

TEST(IdleTurn, WritesTheTurnLockInsideTheReorientWindow) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  host.currentMotion = 0x100;
  host.record.rot = {pes::RotRecord{/*offset*/ 0, /*start*/ 0, /*end*/ 2}};  // 0 and 4 frames
  host.record.blend = {55};                                                  // 99 frames
  host.currentClipFrames = 1;
  req.SetF(pes::moveword::kRecordStampB, 2.0f);
  pes::movestep::IdleTurn(host, req);
  EXPECT_EQ(req.U(0x3c) & 0xffu, 1u);
  EXPECT_EQ(req.U(0x3e), 3u);
}

TEST(IdleOther, IssuesTheIdleClipAndTheRootFacingTailForCommand121) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  req.SetU(pes::moveword::kCommandId, 0x79);
  req.SetF(pes::moveword::kFacingError, 20.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  host.moveConstants.lookLow = -10.0f;
  host.moveConstants.lookHigh = 10.0f;
  host.moveConstants.lookScale = 54.0f;
  pes::movestep::IdleOther(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDuration), 0.3f);
  EXPECT_EQ(req.U(pes::moveword::kRootFacing) & 0xffu, 1u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRootFacingAngle), 20.0f);
}

TEST(IdlePkPlayer, IssuesTheIdleClipOnlyWhenTheLiveKindChanges) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  req.SetU(pes::moveword::kLiveKind, 0);
  pes::movestep::IdlePkPlayer(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kInit));
  req.SetU(pes::moveword::kLiveKind, 3);
  pes::movestep::IdlePkPlayer(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
}

TEST(IdlePkGk, KeepsTheTickUntilTheClipRunsPastTheFixedFrames) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  host.currentClipFrames = 0;
  host.record.blend = {6};  // 11 frames
  pes::movestep::IdlePkGk(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kInit));
  host.currentClipFrames = 12;
  pes::movestep::IdlePkGk(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
}

TEST(TimingStep, IssuesTheIdleClipWhenTheStayDecisionRefuses) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  host.record.blend = {6};  // 11 frames
  host.currentClipFrames = 0;
  pes::movestep::TimingStep(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
}

TEST(Demo, IssuesTheIdleClipWhenTheClipHasRunPastTheRandomThreshold) {
  RateHost host = IdleHost();
  pes::MoveReq req = StayRefused();
  host.record.blend = {6};  // 11 frames
  host.currentClipFrames = 11;
  pes::movestep::Demo(host, req);
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kDuration), 0.3f);
}

TEST(StayDecision, StaysWhenTheFacingErrorIsInsideTheBearingGate) {
  RateHost host = IdleHost();
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandSpeedClass, 1);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDistToTarget, 10.0f);
  host.ballBearingDeg = 0.0f;
  EXPECT_TRUE(pes::movestep::StayDecision(host, req));
  req.SetF(pes::moveword::kFacingError, 200.0f);
  EXPECT_FALSE(pes::movestep::StayDecision(host, req));
}

// INIT's stay decision (docs/re/pes-re-anim-select.md 2.3): a still command whose stay decision
// holds goes to the IDLE state; only a go enters the GOTO_RUN entry.
TEST(Init, AStandingPlayerWhoStaysGoesIdle) {
  RateHost host = IdleHost();
  pes::MoveReq req;
  req.SetU(pes::moveword::kKind, 0);
  req.SetU(pes::moveword::kCommandSpeedClass, 0);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetF(pes::moveword::kDistToTarget, 10.0f);
  host.ballBearingDeg = 0.0f;
  pes::movestep::Init(host, req);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState), uint32_t(pes::MoveState::kIdle));
}

TEST(SimTail, CopiesTheSlipFacingAfterSlipdownEvenWhenRootFacingIsSet) {
  RateHost host = IdleHost();
  pes::MoveStepRecord rec;
  pes::MoveReq req;
  req.SetF(pes::moveword::kSlipAngle, 123.0f);
  req.SetByte(pes::moveword::kRootFacing, 1);
  req.SetF(pes::moveword::kRootFacingAngle, 300.0f);
  pes::movestep::ApplyTail(rec, host, req, pes::MoveState::kRunTurnRunSlipdown);
  EXPECT_FLOAT_EQ(host.f0bec, 123.0f);
}

TEST(SimTail, CopiesTheRootFacingOnOtherStatesOnlyWhenTheFlagIsSet) {
  RateHost host = IdleHost();
  pes::MoveStepRecord rec;
  pes::MoveReq req;
  req.SetByte(pes::moveword::kRootFacing, 0);
  req.SetF(pes::moveword::kRootFacingAngle, 300.0f);
  pes::movestep::ApplyTail(rec, host, req, pes::MoveState::kIdle);
  EXPECT_FLOAT_EQ(host.f0bec, 0.0f);
  req.SetByte(pes::moveword::kRootFacing, 1);
  pes::movestep::ApplyTail(rec, host, req, pes::MoveState::kIdle);
  EXPECT_FLOAT_EQ(host.f0bec, 300.0f);
}

TEST(SimTail, StoresTheRequestsPlaybackSpeed) {
  RateHost host = IdleHost();
  pes::MoveStepRecord rec;
  pes::MoveReq req;
  req.SetF(pes::moveword::kPlayback, 0.75f);
  pes::movestep::ApplyTail(rec, host, req, pes::MoveState::kIdle);
  EXPECT_FLOAT_EQ(rec.playbackSpeed, 0.75f);
}

// The clip the FSM last issued, as the viewer plays it. Each issue is a new clip in PES, so a
// re-issue counts as a restart. A parametric style index is recorded as the blend group it plays.
TEST(IssuedClip, KeepsTheFsmArgumentsOfTheClipItRecords) {
  pes::ParametricGroups groups{};
  pes::MotionRequest req;
  req.motion = 0x3f;
  req.mirror = true;
  req.startTime = 0.25f;
  req.duration = 0.2f;
  pes::IssuedClip clip;
  pes::RecordIssue(clip, req, groups);
  EXPECT_EQ(clip.request.motion, 0x3f);
  EXPECT_TRUE(clip.request.mirror);
  EXPECT_FLOAT_EQ(clip.request.startTime, 0.25f);
  EXPECT_FLOAT_EQ(clip.request.duration, 0.2f);
  EXPECT_EQ(clip.request.group, -1);
}

TEST(IssuedClip, AReIssueIsANewClipAndCountsAsARestart) {
  pes::ParametricGroups groups{};
  pes::MotionRequest req;
  req.motion = 0x3f;
  pes::IssuedClip clip;
  pes::RecordIssue(clip, req, groups);
  const uint32_t first = clip.serial;
  pes::RecordIssue(clip, req, groups);
  EXPECT_EQ(clip.serial, first + 1);
}

TEST(IssuedClip, AParametricStyleIsRecordedAsItsBlendGroup) {
  pes::ParametricGroups groups{};
  groups.group[3] = 42;  // a synthetic map: the lookup is under test, not the install's table
  pes::MotionRequest req;
  req.motion = -1;
  req.group = 3;
  req.rate = 0.5f;
  pes::IssuedClip clip;
  pes::RecordIssue(clip, req, groups);
  EXPECT_EQ(clip.request.group, 42);
  EXPECT_FLOAT_EQ(clip.request.rate, 0.5f);
}

// The style map is PES's own table, read from the install. Every style names a blend group with
// members, or the viewer has nothing to play for a request that reaches it.
TEST(ParametricGroupsInstall, EveryStyleNamesABlendGroupWithMembers) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto groups = pes::LoadParametricGroups(*exe, &err);
  ASSERT_TRUE(groups) << err;
  EXPECT_EQ(groups->group[0], pes::LOCOMOTION_BLEND_GROUP);
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto table = pes::LoadBlendTable(vfs, &err);
  ASSERT_TRUE(table) << err;
  for (int style = 0; style < pes::kStyleCount; ++style)
    EXPECT_FALSE(pes::BlendGroup(*table, groups->group[style]).empty()) << "style " << style;
}

// FUN_1406b97d0 (AS 2.3): a motion's start time is one of nine install floats, picked by the
// motion number through a switch; every other motion starts at zero.
TEST(MotionStartSeconds, ReturnsTheArmFloatThatTheSwitchNamesAndZeroElsewhere) {
  pes::movestep::MotionStartConstants k{};
  k.v590d98 = 1.0f;
  k.v592228 = 2.0f;
  k.v565600 = 3.0f;
  k.v588200 = 4.0f;
  k.v598578 = 5.0f;
  k.v5c48a0 = 6.0f;
  k.v59d5c8 = 7.0f;
  k.v5a8a28 = 8.0f;
  k.v57a190 = 9.0f;
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0x1f), 1.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0x23), 4.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0x27), 3.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0x2b), 5.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0x45), 6.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0xb11), 7.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0xb1e), 8.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0xe24), 9.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0x20), 0.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0xb0b), 0.0f);
  EXPECT_EQ(pes::movestep::MotionStartSeconds(k, 0xe25), 0.0f);
}

TEST(MotionStartInstall, LoadsTheNineFloatsAndTheSwitchReturnsTheInstallBits) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::movestep::LoadMotionStartConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  for (float v : {k->v590d98, k->v592228, k->v565600, k->v588200, k->v598578, k->v5c48a0,
                  k->v59d5c8, k->v5a8a28, k->v57a190}) {
    EXPECT_TRUE(std::isfinite(v));
    EXPECT_GE(v, 0.0f);
  }
  // Motion 0x2b reads the float at 0x142598578 and returns those bits unchanged.
  float raw = -1.0f;
  ASSERT_TRUE(exe->ReadVa(0x142598578, sizeof raw, reinterpret_cast<uint8_t*>(&raw)));
  EXPECT_EQ(pes::movestep::MotionStartSeconds(*k, 0x2b), raw);
}

// LocomotionStyle (FUN_1406ba2f0). The host answers the reads the ladder makes; the constants are
// synthetic here, and the install test checks the loaded ones through their relations.
namespace {
pes::StyleConstants SyntheticStyle() {
  pes::StyleConstants s{};
  s.rate0p895 = 0.9f;
  s.rate0p825 = 0.8f;
  s.speed16 = 16.0f;
  s.heightDiv = 100.0f;
  s.kmhScale = 1000.0f;
  s.sampleTicks = 20.0f;
  s.radToDeg = 57.2957795f;
  return s;
}

// An open-play host standing still with the ball 1 m ahead at 0.2 m high.
RateHost StyleHost() {
  RateHost host;
  host.run = SyntheticAngles();
  host.run.kmh60 = 60.0f;
  host.run.half = 0.5f;
  host.run.rampSlope = 0.15f;
  host.style = SyntheticStyle();
  host.moveConstants.dist3 = 3.0f;
  host.ballSample[18][1] = 1.0f;  // GF order: x, z, height
  host.ballSample[18][2] = 0.2f;
  return host;
}

pes::MoveReq StyleReq(uint32_t command, float rate) {
  pes::MoveReq req;
  req.SetU(pes::moveword::kCommandId, command);
  req.SetF(pes::moveword::kRate, rate);
  return req;
}

int Style(RateHost& host, pes::MoveReq& req, float requested, float rateMax = 1.0f,
          bool rateHeld = false) {
  return pes::movestep::LocomotionStyle(host, host.animeState, requested, req, rateMax, rateHeld);
}
}  // namespace

TEST(LocomotionStyle, OutOfOpenPlayWithoutTheScriptedSlotIsFifteen) {
  RateHost host = StyleHost();
  host.matchPhase = 2;
  host.scriptedSlotOurs = false;
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.2f), 0xf);
}

TEST(LocomotionStyle, ReplayWithoutTheScriptedSlotIsFifteen) {
  RateHost host = StyleHost();
  host.animeState = 0x4a;
  host.scriptedSlotOurs = false;
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.2f), 0xf);
}

TEST(LocomotionStyle, ReplayWithTheScriptedSlotIsOursHasNoStyle) {
  RateHost host = StyleHost();
  host.animeState = 0x4a;
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, OutOfOpenPlayWithTheScriptedSlotOursKeepsTheLadder) {
  RateHost host = StyleHost();
  host.matchPhase = 2;
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, StaminaBelowTwentyPercentIsThirteen) {
  RateHost host = StyleHost();
  host.staminaPct = 19;
  host.actionIndex = 3;
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.2f), 0xd);
}

TEST(LocomotionStyle, StaminaAtTwentyPercentIsNotTired) {
  RateHost host = StyleHost();
  host.staminaPct = 20;
  host.actionIndex = 3;
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, StaminaIsOnlyReadForAPlayerOnThePitch) {
  RateHost host = StyleHost();
  host.staminaPct = 0;
  host.actionIndex = 22;
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, AFastRequestOnAGatedCommandIsTen) {
  RateHost host = StyleHost();
  pes::MoveReq req = StyleReq(2, 0.2f);
  EXPECT_EQ(Style(host, req, 0.5f), 10);
}

TEST(LocomotionStyle, AHeldRateOnAGatedCommandIsZero) {
  RateHost host = StyleHost();
  pes::MoveReq req = StyleReq(2, 0.2f);
  EXPECT_EQ(Style(host, req, 0.5f, 1.0f, true), 0);
}

TEST(LocomotionStyle, ARateNearTheCeilingOnAGatedCommandIsZero) {
  RateHost host = StyleHost();
  pes::MoveReq req = StyleReq(2, 0.95f);
  EXPECT_EQ(Style(host, req, 1.0f), 0);
}

TEST(LocomotionStyle, ARateBetweenTheTwoGatesOnAGatedCommandIsTen) {
  RateHost host = StyleHost();
  pes::MoveReq req = StyleReq(2, 0.85f);
  EXPECT_EQ(Style(host, req, 1.05f), 10);  // 1.05 > 0.85 + 0.15: T1 is false, so the 0.8 gate decides
}

TEST(LocomotionStyle, AnUnlistedCommandIsZeroWhateverTheRequest) {
  RateHost host = StyleHost();
  pes::MoveReq req = StyleReq(0, 0.2f);
  EXPECT_EQ(Style(host, req, 0.5f), 0);
}

TEST(LocomotionStyle, DribbleAtTheFeetIsEight) {
  RateHost host = StyleHost();
  pes::MoveReq req = StyleReq(5, 0.2f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  EXPECT_EQ(Style(host, req, 0.2f), 8);
}

TEST(LocomotionStyle, DribbleNeedsTheBallInFront) {
  RateHost host = StyleHost();
  host.ballSample[18][0] = 1.0f;  // now 90 degrees off the facing
  host.ballSample[18][1] = 0.0f;
  pes::MoveReq req = StyleReq(5, 0.2f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, DribbleNeedsTheBallNearby) {
  RateHost host = StyleHost();
  host.ballSample[18][1] = 4.0f;
  pes::MoveReq req = StyleReq(5, 0.2f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, DribbleNeedsTheBallLow) {
  RateHost host = StyleHost();
  host.ballSample[18][2] = 1.0f;  // above half the 1.8 m height
  pes::MoveReq req = StyleReq(5, 0.2f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, DribbleYieldsToABallRunningAlongTheFacing) {
  RateHost host = StyleHost();
  host.ballSample[2][1] = 0.1f;  // 0.1 m a tick: 19.4 km/h at 54 ticks a second
  pes::MoveReq req = StyleReq(5, 0.2f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(LocomotionStyle, DribbleTakesAFastBallThatTurnsAway) {
  RateHost host = StyleHost();
  host.ballSample[2][1] = 0.1f;
  host.ballMotionHeadingDeg = 90.0f;
  pes::MoveReq req = StyleReq(5, 0.2f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  EXPECT_EQ(Style(host, req, 0.2f), 8);
}

TEST(LocomotionStyle, DribbleNeedsTheOpenPlayPhase) {
  RateHost host = StyleHost();
  host.matchPhase = 2;
  pes::MoveReq req = StyleReq(5, 0.2f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  EXPECT_EQ(Style(host, req, 0.2f), 0);
}

TEST(StyleConstantsInstall, LoadsTheExeScalarsWithTheirRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::LoadStyleConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  EXPECT_LT(k->rate0p825, k->rate0p895);
  EXPECT_LT(k->rate0p895, 1.0f);
  EXPECT_GT(k->speed16, 0.0f);
  EXPECT_GT(k->kmhScale, 0.0f);
  // One radian in degrees is 180 / pi: the conversion's inverse is pi times 180.
  EXPECT_NEAR(k->radToDeg * 3.14159265358979f, 180.0f, 1e-2f);
}

// LateralDamping (FUN_1406cbc10): the drift's decay in the drift case, and the else branch that
// clears the drift past a threshold. The constants and the record are synthetic here; the install
// test checks the loaded ones.
namespace {
pes::DampingConstants SyntheticDamping() {
  pes::DampingConstants d{};
  d.rateGate = 0.3f;
  d.rateSpan = 0.4f;
  d.rateCap = 0.7f;
  d.rateBase = 0.8f;
  d.rateScale = 0.2f;
  d.driftSpeed = 120.0f;
  d.motionRate = 0.6f;
  d.angleGate = 5.625f;
  return d;
}

RateHost DriftHost() {
  RateHost host;
  host.run = SyntheticAngles();
  host.run.half = 0.5f;
  host.moveConstants.dist2m5 = 2.5f;
  host.damping = SyntheticDamping();
  host.dampingRecord = {2.7f, 2.7f, 1.0f};  // lateral bound, forward bound, scale
  return host;
}

// The drift on a request with the playback at 1 and no look delta.
pes::MoveReq DriftReq(float x, float z) {
  pes::MoveReq req;
  req.SetF(pes::moveword::kPlaybackDefault, 1.0f);
  req.SetF(pes::moveword::kDriftX, x);
  req.SetF(pes::moveword::kDriftZ, z);
  return req;
}
}  // namespace

TEST(LateralDamping, WithoutAPhaseAndOutsideTheDriftCaseClearsTheDrift) {
  RateHost host = DriftHost();
  pes::MoveReq req = DriftReq(0.5f, 0.2f);
  pes::movestep::LateralDamping(host, req);
  EXPECT_EQ(req.F(pes::moveword::kDriftX), 0.0f);
  EXPECT_EQ(req.F(pes::moveword::kDriftZ), 0.0f);
}

TEST(LateralDamping, ElseBranchShrinksAFastDriftByTheSpeedThreshold) {
  RateHost host = DriftHost();
  host.currentState = 1;  // a phase other than zero takes the else branch
  pes::MoveReq req = DriftReq(3.0f, 0.1f);
  pes::movestep::LateralDamping(host, req);
  // 3 m a tick against 120 / 54 a tick: the excess stays, and the slow Z clears.
  EXPECT_NEAR(req.F(pes::moveword::kDriftX), 3.0f - 120.0f / 54.0f, 1e-4f);
  EXPECT_EQ(req.F(pes::moveword::kDriftZ), 0.0f);
}

TEST(LateralDamping, RunLoopDriftDecaysTowardZeroByTheLateralStep) {
  RateHost host = DriftHost();
  host.currentState = 3;  // RUN_LOOP, style 0, rate 0.5: the drift case
  pes::MoveReq req = DriftReq(0.5f, 0.2f);
  req.SetF(pes::moveword::kRate, 0.5f);
  pes::movestep::LateralDamping(host, req);
  // The step per tick is the record's bound over 54 / 2 = 27: 2.7 / 27 = 0.1 both ways.
  EXPECT_NEAR(req.F(pes::moveword::kDriftX), 0.4f, 1e-5f);
  EXPECT_NEAR(req.F(pes::moveword::kDriftZ), 0.1f, 1e-5f);
}

TEST(LateralDamping, TheSpecialMotionTakesTheDriftCaseWhateverTheState) {
  RateHost host = DriftHost();
  host.currentState = 0;
  host.currentMotion = 0xbde;
  // A record whose bounds are 2700 keeps each step (bound / 27 = 100) clear of the clamp, so the
  // turn term (-2 / 2700) and the ramp (0.75 at rate 0.6) decide the drift.
  host.dampingRecord = {2700.0f, 2700.0f, 2700.0f};
  pes::MoveReq req = DriftReq(0.5f, 0.2f);
  req.SetF(pes::moveword::kLookDelta, -10.0f);
  req.SetF(pes::moveword::kRate, 0.5f);
  pes::movestep::LateralDamping(host, req);
  // factor = -2 / 2700 * (0.75 * 0.2 + 0.8); the drift steps by -factor * 2700 - x and z - x alike.
  EXPECT_NEAR(req.F(pes::moveword::kDriftX), 1.9f, 1e-3f);
  EXPECT_NEAR(req.F(pes::moveword::kDriftZ), 1.9f, 1e-3f);
}

TEST(LateralDamping, AJostleStyleIsNotTheDriftCase) {
  RateHost host = DriftHost();
  host.currentState = 3;  // RUN_LOOP at rate 0.5 would be the drift case for style 0
  pes::MoveReq req = DriftReq(0.5f, 0.2f);
  req.SetF(pes::moveword::kRate, 0.5f);
  req.SetByte(pes::moveword::kStyle, 6);  // the jostle loop: the else branch runs instead
  pes::movestep::LateralDamping(host, req);
  EXPECT_EQ(req.F(pes::moveword::kDriftX), 0.0f);
  EXPECT_EQ(req.F(pes::moveword::kDriftZ), 0.0f);
}

TEST(DampingConstantsInstall, LoadsTheExeScalarsInTheirOrder) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::LoadDampingConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  EXPECT_LT(k->rateGate, k->rateCap);
  EXPECT_LT(k->rateBase + k->rateScale, 1.0f + 1e-6f);
  EXPECT_GT(k->rateSpan, 0.0f);
  EXPECT_GT(k->driftSpeed, 0.0f);
}

TEST(LateralDamping, ElseBranchClearsADriftUnderTheTwoAndAHalfMetreBound) {
  RateHost host = DriftHost();
  host.currentState = 1;
  pes::MoveReq req = DriftReq(2.4f, 0.0f);  // past the 2.22 threshold, under the 2.5 bound
  pes::movestep::LateralDamping(host, req);
  EXPECT_EQ(req.F(pes::moveword::kDriftX), 0.0f);
}

TEST(DampingRecordInstall, ReadsTheFreemoveRecordsPayloadAtItsRuntimeOffsets) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rec = pes::LoadDampingRecord(vfs, &err);
  ASSERT_TRUE(rec.has_value()) << err;
  // The same bytes read by hand: the table entry's payload at +8, +0xc and +0x10.
  auto blob = vfs.Read("common/match/constant/constant_player.bin", &err);
  ASSERT_TRUE(blob.has_value()) << err;
  auto table = pes::ParseUniformTable(*blob, &err);
  ASSERT_TRUE(table.has_value()) << err;
  auto span = table->Find("freemove.o");
  ASSERT_TRUE(span.has_value());
  float raw[3];
  std::memcpy(raw, span->data() + 8, sizeof raw);
  EXPECT_EQ(rec->lateralBound, raw[0]);
  EXPECT_EQ(rec->forwardBound, raw[1]);
  EXPECT_EQ(rec->scale, raw[2]);
}
