#include <gtest/gtest.h>

#include "onthepitch/match.hpp"
#include "pes_match_constants.hpp"
#include "pes_shared_match.hpp"

// PES's Kick action starts on the tick its Kick start (FUN_140724CC0) answers a kind with a clip; until then the AI's order stays
// pending and the request builder and chooser run again next tick (docs/ports/pes-port-KickTick.md). Played over one whole PES match
// (the seed-7 match the restart-delay tests play, so the families share one run).
namespace {

constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
constexpr unsigned long kMaxTicks = 30ul * 60ul * 100ul;  // the cap sim_match_test.cpp uses

class KickStartProbe : public pestest::Probe {
 public:
  KickStartProbe() : Probe("PesKickStart", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  pes::KickMotionStats stats;        // the match's counters at its last tick
  unsigned long openPlayKicks = 0;   // contact-frame kicks made while play was open (no restart in progress)
  void OnStart(Match& match) override { last_ = match.PesKickMotionStats().kicks; }
  void OnTick(Match& match) override {
    stats = match.PesKickMotionStats();
    if (stats.kicks != last_ && match.IsInPlay() && !match.IsInSetPiece()) openPlayKicks += stats.kicks - last_;
    last_ = stats.kicks;
  }

 private:
  uint32_t last_ = 0;
};

KickStartProbe probe;

}  // namespace

// The pair runs for the AI's pass or shot order in open play, once per tick the animation is asked for, and no kick is struck in
// open play that it did not run for first.
TEST(PesKickStart, TheKickStartRunsForEveryPassOrShotAskedInOpenPlay) {
  pestest::Run(probe);
  if (probe.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(probe.setupError.empty()) << probe.setupError;
  pestest::Settle(probe);
  EXPECT_GT(probe.stats.tries, 0u) << "the Kick start never ran";
  EXPECT_GE(probe.stats.tries, probe.openPlayKicks) << "a kick was struck in open play without a Kick start try";
}

// A none answer is counted as one: it is a subset of the tries, and a kick with a clip is one the Kick start answered with a kind.
TEST(PesKickStart, ANoneAnswerIsATryAndAKickWithAClipWasAnAnswer) {
  pestest::Run(probe);
  if (probe.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(probe.setupError.empty()) << probe.setupError;
  pestest::Settle(probe);
  EXPECT_LE(probe.stats.answeredNone, probe.stats.tries);
  EXPECT_LE(probe.stats.kicks - probe.stats.noClip, probe.stats.tries - probe.stats.answeredNone)
      << "a kick with a clip was struck without an answered Kick start";
}
