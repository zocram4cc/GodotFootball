#include <gtest/gtest.h>

#include "pes_shared_match.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

// The seed-11 two-minute PES-loop match must reach full time. A match that stops making restarts ends at the stall watch's
// bound (stallStops), so a stalled match fails in bounded time instead of running to the tick cap.
namespace {

constexpr unsigned int kSeed = 11;
constexpr int kMinutes = 2;
// The cap sim_match_test.cpp uses.
constexpr unsigned long kMaxTicks = 30ul * 60ul * 100ul;

class SeedElevenProbe : public pestest::Probe {
 public:
  SeedElevenProbe()
      : Probe("PesHang", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/true, /*judgeEnd=*/true) {}
};

SeedElevenProbe seedEleven;

}  // namespace

TEST(PesHang, SeedElevenTwoMinutePesLoopMatchReachesFullTime) {
  pestest::Run(seedEleven);
  if (seedEleven.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(seedEleven.setupError.empty()) << seedEleven.setupError;
  pestest::Settle(seedEleven);
  ASSERT_TRUE(seedEleven.stop.gameOver) << "the match did not reach full time (stopped at frame " << seedEleven.stop.frame
                                        << ", longest stoppage " << seedEleven.stop.longest << " ticks)";
}
