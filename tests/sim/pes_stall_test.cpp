#include <gtest/gtest.h>

#include <map>

#include "base/log.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/player/humanoid/humanoid.hpp"
#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
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

// A man who has the ball at rest at his feet and does not play it is a trap the reaction keeps starting over: the trap clip
// is issued, the next tick the reaction starts the trap again, and the clip restarts from its first frame, so it never reaches
// its contact frame (PressAI's seed-7 goal-kick stall: the designated man 0.1 m from a resting ball for 86 s). PES's receive
// reaction holds the man in his trap while the clip plays (the action tick's end handler 0x14079E5C0 leaves at its first
// statement while the playing motion is in the anime's motion table: docs/ports/pes-port-TrapTick4.md), so a trap clip is
// never restarted before the contact frame it was heading for.
class TrapRestartProbe : public pestest::Probe {
 public:
  TrapRestartProbe()
      : Probe("PesStallTrap", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long trapClips = 0;        // trap clips that started (a tick on which a man's trap clip began)
  long restartedTraps = 0;   // those that began over a trap clip of his that had not reached its contact frame
  long holderTraps = 0;      // those that began while he was PES's holder (MA+0x2601C == him)
  long trapStarts = 0;       // PES trap starts: ticks on which a man's P+0xAC8 became the Trap action
  long refusedStarts = 0;    // those whose clip the body did not take on the next tick (the action tick's PlayIssuedClip refused it)
  void OnTick(Match& match) override {
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        Humanoid* body = player->CastHumanoid();
        Seen& seen = seen_[player.get()];
        const bool onTrap = body->OnPesTrap();
        const Anim* clip = body->GetCurrentAnim();
        const bool began = onTrap && (!seen.onTrap || clip->frameNum < seen.frame);
        if (began) {
          trapClips++;
          if (seen.onTrap && seen.frame < seen.touchFrame) restartedTraps++;
          if (match.GetPesHolder() == player.get()) holderTraps++;
        }
        const bool pesTrap = match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPByteAC8Offset] == pes::kAnimeActionTrap;
        if (seen.startedLastTick && !onTrap) refusedStarts++;
        const bool started = pesTrap && !seen.pesTrap;
        if (started) trapStarts++;
        seen = {onTrap, clip->frameNum, clip->touchFrame, pesTrap, started};
      }
    }
  }

 private:
  struct Seen {
    bool onTrap = false;
    int frame = 0;
    int touchFrame = -1;
    bool pesTrap = false;
    bool startedLastTick = false;
  };
  std::map<const Player*, Seen> seen_;
};

TrapRestartProbe trapCensus;

// The body's last-resort fallback (humanoid.cpp: "NO APPLICABLE ANIM FOUND", then ResetPosition) means the AI handed a man whose clip
// ended no command at all. It is never meant to happen in a match. The goal kick's strike (the keeper's root is carried onto the contact
// by the referee's strike, pesgoalkick.hpp) used to leave the keeper's queue empty from the release to the contact tick, so any
// keeper whose clip ended in that window hit the fallback.
class FallbackProbe : public pestest::Probe {
 public:
  FallbackProbe()
      : Probe("PesStallFallback", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long fallbacks = 0;
  void OnStart(Match&) override {
    connection_ = blunted::BindLog([this](blunted::e_LogType, std::string, std::string, std::string message) {
      if (message.find("NO APPLICABLE ANIM FOUND") != std::string::npos) fallbacks++;
    });
  }

 private:
  boost::signals2::scoped_connection connection_;
};

FallbackProbe fallbackCensus;

}  // namespace

TEST(PesStall, ATrapClipIsNeverRestartedBeforeItsContactFrame) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapClips, 0) << "no trap clip was played";
  EXPECT_EQ(trapCensus.restartedTraps, 0) << trapCensus.restartedTraps << " of " << trapCensus.trapClips
                                          << " trap clips restarted before their contact frame";
}

// PES's reaction entry 3 refuses the carrier (MA+0x2601C) unless he is in stagger, dodge or the keeper's drop (FUN_1409fa400 at
// 0x1409FA4AD..0x1409FA4D0), so a man who holds the ball is never handed a new trap: after his trap ends the action tick starts his
// dribble (0x1407A38E0) and he plays on. GF re-issued the trap for a holder every two or three ticks while the ball lay still.
TEST(PesStall, AHolderIsNeverHandedANewTrap) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapClips, 0) << "no trap clip was played";
  EXPECT_EQ(trapCensus.holderTraps, 0) << trapCensus.holderTraps << " of " << trapCensus.trapClips
                                       << " trap clips began for the man who already held the ball";
}

// PES's reaction gate 4a (the Trap action's slot +0x10, 0x14079BBE0) decides from the man's current action whether he may take a trap, and
// PES's action switch then replaces whatever he was doing: a trap PES starts is a trap the body plays. GF's body used to refuse 84 of 113
// starts (it was in a ball-control, slide or pass clip PlayIssuedClip would not interrupt), so the action tick ended the trap unplayed
// and the reaction re-issued it two ticks later.
TEST(PesStall, NoPesTrapStartIsRefusedByTheBody) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapStarts, 0) << "no PES trap started";
  EXPECT_EQ(trapCensus.refusedStarts, 0) << trapCensus.refusedStarts << " of " << trapCensus.trapStarts
                                         << " PES trap starts were refused by the body";
}

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

TEST(PesStall, ANoManIsEverHandedNoCommandAtAClipEnd) {
  pestest::Run(fallbackCensus);
  if (fallbackCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(fallbackCensus.setupError.empty()) << fallbackCensus.setupError;
  pestest::Settle(fallbackCensus);
  ASSERT_TRUE(fallbackCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(fallbackCensus.fallbacks, 0) << fallbackCensus.fallbacks << " clip ends found no applicable anim";
}
