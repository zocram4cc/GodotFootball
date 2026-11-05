#include <gtest/gtest.h>

#include "onthepitch/match.hpp"
#include "pes_match_constants.hpp"
#include "pes_shared_match.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

// The stalemate breaker is GF's band-aid: once nobody has moved the ball for five seconds it designates the
// nearest man, whatever the referee has armed. PES never needs it. So a firing is a defect in one of two places:
// the breaker counted idle time over a restart the referee had armed, or a live ball held by a man who never
// released it. Both are counted over one whole PES match (pes_loop on), tick by tick.
namespace {

constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
// The cap sim_match_test.cpp uses.
constexpr unsigned long kMaxTicks = 30ul * 60ul * 100ul;

// One whole PES match, with each breaker firing filed by the state of play on its tick. The match is the seed-7 match the
// restart-delay tests play, so the two families share one run (pes_shared_match.hpp).
class BreakerCensusProbe : public pestest::Probe {
 public:
  BreakerCensusProbe()
      : Probe("PesStall", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long armedFirings = 0;    // the referee had a restart armed (its buffer is active)
  long liveFirings = 0;     // play is live, nothing is armed: a man on a ball that nobody plays
  long unarmedFirings = 0;  // play is off and nothing is armed
  void OnStart(Match& match) override { lastCount_ = match.GetStalemateCount(); }
  void OnTick(Match& match) override {
    const unsigned long count = match.GetStalemateCount();
    const bool fired = count != lastCount_;
    lastCount_ = count;
    if (!fired) return;
    if (match.GetRefereeBuffer().active)
      armedFirings++;
    else if (match.IsInPlay() && !match.IsInSetPiece())
      liveFirings++;
    else
      unarmedFirings++;
  }

 private:
  unsigned long lastCount_ = 0;
};

BreakerCensusProbe census;

}  // namespace

// The referee arms a restart and releases it when the taker is ready. While it is armed the referee owns the
// ball; a breaker that forces a man over it is counting a hold PES does not stall on.
TEST(PesStall, TheBreakerNeverForcesABallUnderAnArmedRestart) {
  pestest::Run(census);
  if (census.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(census.setupError.empty()) << census.setupError;
  pestest::Settle(census);
  ASSERT_TRUE(census.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(census.armedFirings, 0) << "the breaker forced a man over a restart the referee had armed, "
                                       "for " << census.armedFirings << " ticks";
}

// A live ball that lies still for five seconds is a man who holds it and does nothing with it. PES's holder keeps
// the ball until he releases it, so the breaker must not have to take it from him.
TEST(PesStall, ALiveBallNeverLiesStillLongEnoughForTheBreaker) {
  pestest::Run(census);
  if (census.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(census.setupError.empty()) << census.setupError;
  pestest::Settle(census);
  ASSERT_TRUE(census.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(census.liveFirings, 0) << "the breaker forced a live ball for " << census.liveFirings << " ticks";
}
