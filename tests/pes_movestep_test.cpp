// PES 2021's MOVESTEP record and its playback writers (docs/re/pes-re-anim-select.md
// 1-2, 4). The formulas are arithmetic over install constants: synthetic constants
// exercise them, and one install test checks the loaded constants through their
// relations, not pinned numbers.
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

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
