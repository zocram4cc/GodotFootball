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
  pes::MoveCell Cell(int, int, bool) override { return {}; }
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
  float RecordF0bec() const override { return 0.0f; }
  void SetRecordF0bec(float) override {}
  const pes::AngleBucketConstants& AngleBuckets() const override { return buckets; }
  void PositionAt(int, float out[3]) const override { out[0] = out[1] = out[2] = 0.0f; }
  int MatchPhase() const override { return 1; }
  bool IsReplay() const override { return false; }
  void Issue(const pes::MotionRequest&) override {}
  const pes::StepScalars& StepConstants() const override { return step; }
  float BandSpeedKmh(int, float) const override { return 0.0f; }
  bool MirrorChoice(bool) const override { return false; }
  int FootBand() const override { return 0; }
  const pes::RunConstants& RunConstantsView() const override { return run; }
  const pes::RunTables& RunTablesView() const override { return tables; }
  const pes::SpeedRateTable& SpeedRateTableView() const override { return rate; }
  const pes::StepTables& StepTablesView() const override { return stepTables; }
  bool BucketIsLeft(int) const override { return false; }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { return playback; }
  int CurrentMotion() const override { return 0; }
  int CurrentClipFrames() const override { return 0; }
  int AnimeState() const override { return 0; }

  float g = 1.0f;
  pes::RunConstants run{};
  pes::RunTables tables{};
  pes::SpeedRateTable rate{};
  pes::StepScalars step{};
  pes::StepTables stepTables{};
  pes::AngleBucketConstants buckets{};
  pes::PlaybackConstants playback{};
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
