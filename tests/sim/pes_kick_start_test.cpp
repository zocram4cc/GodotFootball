#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "onthepitch/match.hpp"
#include "onthepitch/pesballlist.hpp"
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
  // The ball list MA+0x2470C of each side, read in open play every kBallListEvery ticks: lists checked, entries seen, and the
  // lists that broke the writer's own rules (an id of the wrong team, a distance that fell, a rank that is not the entry's place).
  unsigned long listsChecked = 0, listEntries = 0, listBroken = 0;
  void OnStart(Match& match) override { last_ = match.PesKickMotionStats().kicks; }
  void OnTick(Match& match) override {
    stats = match.PesKickMotionStats();
    if (stats.kicks != last_ && match.IsInPlay() && !match.IsInSetPiece()) openPlayKicks += stats.kicks - last_;
    last_ = stats.kicks;
    if (match.IsInPlay() && ++ticks_ % kBallListEvery == 0) CheckBallList(match);
  }

 private:
  static constexpr unsigned long kBallListEvery = 37;
  static constexpr int kTeamMen = 11;
  void CheckBallList(Match& match) {
    for (int side = 0; side < 2; ++side) {
      const uint8_t* ma = match.PesWorldOf().Ma(side);
      bool broken = false;
      for (int team = 0; team < 2; ++team) {
        const unsigned count = ma[pes::kMaBallListCountOffset + team];
        float previous = -1.0f;
        for (unsigned k = 0; k < count && k < kTeamMen; ++k) {
          const uint8_t* entry = ma + pes::kMaBallListOffset + team * pes::kMaListTeamStride + k * 8;
          uint32_t id;
          float d2;
          std::memcpy(&id, entry, 4);
          std::memcpy(&d2, entry + 4, 4);
          broken |= id >= 22 || static_cast<int>(id) / kTeamMen != team || d2 < previous ||
                    ma[pes::kMaBallListRankOffset + id] != k;
          previous = d2;
          ++listEntries;
        }
      }
      ++listsChecked;
      listBroken += broken;
    }
  }
  unsigned long ticks_ = 0;

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

// The ball list PES's writer FUN_14045B090 keeps in each side's MA (translated, proven on 12 real calls) is rebuilt in play by the
// team tick: every list held is ascending, names only its own team's men, and ranks each man at his place; and there were lists.
TEST(PesKickStart, TheBallListIsWrittenAndHoldsThePesRules) {
  pestest::Run(probe);
  if (probe.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(probe.setupError.empty()) << probe.setupError;
  pestest::Settle(probe);
  EXPECT_GT(probe.listsChecked, 0u);
  EXPECT_GT(probe.listEntries, probe.listsChecked) << "the lists were empty: the writer never ran";
  EXPECT_EQ(probe.listBroken, 0u);
}

// The writer's slice (FUN_140457070's gate): a fresh ring-1 event holds the limit at 4; otherwise 4 less the ball's km/h over 15.
TEST(PesBallList, TheSliceLimitFallsOneForEveryFifteenKmhOfBallSpeed) {
  EXPECT_EQ(pes::BallListSliceLimit(true, -200), pes::kBallListFastLimit);
  for (int kmh = 0; kmh <= 200; ++kmh)
    EXPECT_EQ(pes::BallListSliceLimit(false, -kmh), pes::kBallListFastLimit - kmh / 15) << kmh << " km/h";
}

TEST(PesBallList, AFreshEventIsTheNewestRingOneRecordWithKindAndWordInRange) {
  std::array<uint8_t, 0x4000> e{};
  const auto put = [&](std::size_t at, uint32_t v) { std::memcpy(e.data() + at, &v, 4); };
  const std::size_t rec = pes::kBallListRingRecordsOffset;
  EXPECT_FALSE(pes::BallListFreshEvent(e.data())) << "an empty ring answers the default record";
  put(pes::kBallListRingCountOffset, 1);
  put(pes::kBallListRingHeadOffset, 0);
  put(rec, 5);
  put(rec + pes::kBallListEventPairOffset, 1);
  put(rec + pes::kBallListEventPairOffset + 4, pes::kBallListEventWordLimit - 1);
  EXPECT_TRUE(pes::BallListFreshEvent(e.data()));
  put(rec + pes::kBallListEventPairOffset, pes::kBallListEventKindLimit);
  EXPECT_FALSE(pes::BallListFreshEvent(e.data())) << "kind word at its limit";
  put(rec + pes::kBallListEventPairOffset, 1);
  put(rec + pes::kBallListEventPairOffset + 4, pes::kBallListEventWordLimit);
  EXPECT_FALSE(pes::BallListFreshEvent(e.data())) << "second word at its limit";
  put(rec + pes::kBallListEventPairOffset + 4, 0);
  put(rec, pes::kBallListEventNone);
  EXPECT_FALSE(pes::BallListFreshEvent(e.data())) << "a none record";
  // A negative head wraps to the last record of the ring: only that record's pair decides.
  put(rec, 5);  // record 0 is fresh; the head names record 7
  put(pes::kBallListRingHeadOffset, static_cast<uint32_t>(-1));
  const std::size_t last = rec + (pes::kBallListRingRecords - 1) * pes::kBallListRingRecordBytes;
  put(last, 5);
  put(last + pes::kBallListEventPairOffset, pes::kBallListEventKindLimit);
  EXPECT_FALSE(pes::BallListFreshEvent(e.data())) << "record 7 is out of range, record 0 is not the newest";
  put(last + pes::kBallListEventPairOffset, 0);
  EXPECT_TRUE(pes::BallListFreshEvent(e.data()));
}
