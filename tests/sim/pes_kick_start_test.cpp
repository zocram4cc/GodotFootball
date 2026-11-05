#include <gtest/gtest.h>

#include "onthepitch/match.hpp"
#include "pes_match_constants.hpp"
#include "pes_shared_match.hpp"

// PES's Kick action starts only on the tick its Kick start (FUN_140724CC0) answers a kind with a clip; until then the AI's order stays
// pending and the request builder and chooser run again next tick (docs/ports/pes-port-KickTick.md). Played over one whole PES match
// (the seed-7 match the restart-delay tests play, so the families share one run).
namespace {

constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
constexpr unsigned long kMaxTicks = 30ul * 60ul * 100ul;  // the cap sim_match_test.cpp uses

class KickStartProbe : public pestest::Probe {
 public:
  KickStartProbe() : Probe("PesKickStart", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  pes::KickMotionStats stats;     // the match's counters at its last tick
  unsigned long openPlayNoClip = 0;  // contact-frame kicks in open play that carried no Kick start answer
  void OnStart(Match& match) override { last_ = match.PesKickMotionStats().noClip; }
  void OnTick(Match& match) override {
    stats = match.PesKickMotionStats();
    if (stats.noClip != last_ && match.IsInPlay() && !match.IsInSetPiece()) openPlayNoClip += stats.noClip - last_;
    last_ = stats.noClip;
  }

 private:
  uint32_t last_ = 0;
};

KickStartProbe probe;

}  // namespace

// A pass or shot taken in open play was started by an answer of the Kick start: the kick has PES's kind and clip, never the none
// descriptor a restart's kick keeps.
TEST(PesKickStart, EveryOpenPlayKickIsStartedOnAKindWithAClip) {
  pestest::Run(probe);
  if (probe.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(probe.setupError.empty()) << probe.setupError;
  pestest::Settle(probe);
  EXPECT_GT(probe.stats.kicks, probe.stats.noClip) << "no kick of the match had a clip";
  EXPECT_EQ(probe.openPlayNoClip, 0ul) << "an open-play kick was started without the Kick start's answer";
}

// PES answers none while the geometry is not there (all 15 captured run-up calls), so over a match the pair runs more often than a
// Kick starts: refusals exist, and every kick with a clip was one accepted start.
TEST(PesKickStart, TheAIAsksAgainNextTickWhileTheKickStartAnswersNone) {
  pestest::Run(probe);
  if (probe.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(probe.setupError.empty()) << probe.setupError;
  pestest::Settle(probe);
  EXPECT_GT(probe.stats.refused, 0u) << "the Kick start never answered none";
  EXPECT_LE(probe.stats.refused, probe.stats.tries);
  EXPECT_GE(probe.stats.tries - probe.stats.refused, probe.stats.kicks - probe.stats.noClip)
      << "a kick with a clip was struck without an accepted Kick start";
}
