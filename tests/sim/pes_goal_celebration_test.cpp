#include <gtest/gtest.h>

#include "onthepitch/goalcelebration.hpp"

using namespace GoalCelebration;

namespace {
constexpr float kFar_m = 30.0f;  // well outside kArrivalRadius_m
}

// The capped run (the headless sim) performs when the cap passes; a scorer that is there already performs at once.
TEST(GoalCelebrationApproach, ACappedRunnerPerformsAtItsCapNotBefore) {
  EXPECT_FALSE(HasArrived(kFar_m, kApproachCap_ms - 1, kApproachCap_ms));
  EXPECT_TRUE(HasArrived(kFar_m, kApproachCap_ms, kApproachCap_ms));
  EXPECT_TRUE(HasArrived(kArrivalRadius_m, 0, kApproachCap_ms));
}

// The PES-spot run (a long run to T) gets more time than the capped one, so a scorer still running when the capped run's
// cap passes has not "arrived" under the PES cap. The headless sim must keep the capped cap: moving it moved every
// 20-minute match after the first goal (CutsceneGate, smoke hash e663d1e9 vs f3bfd329).
TEST(GoalCelebrationApproach, APesSpotRunnerIsGivenLongerThanACappedRunner) {
  EXPECT_GT(kPesSpotApproachCap_ms, kApproachCap_ms);
  EXPECT_FALSE(HasArrived(kFar_m, kApproachCap_ms, kPesSpotApproachCap_ms));
  EXPECT_TRUE(HasArrived(kFar_m, kPesSpotApproachCap_ms, kPesSpotApproachCap_ms));
}
