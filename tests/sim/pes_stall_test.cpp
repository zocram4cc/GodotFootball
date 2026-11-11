#include "pes/movestep.hpp"
#include <gtest/gtest.h>

#include <map>

#include "base/log.hpp"
#include "onthepitch/pesmovestep.hpp"
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
  long gateRefusedStarts = 0;  // those that left an action gate 4a (0x14079BBE0's admitted ids) does not admit
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
        if (started) {
          trapStarts++;
          // StartTrapAction moved the action the man left into P+0xACA. PES admits a trap from the ids of 0x14079BBE0's list only; a tackle,
          // a keeper's catch or deflect and a man in no action (GF's own cards and celebrations) are not in it.
          const uint8_t left = match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPPrevActionOffset];
          if (left == pes::kAnimeActionTackle || left == pes::kAnimeActionNothing || left == pes::kAnimeActionKeeperCatch ||
              left == pes::kAnimeActionKeeperDeflect)
            gateRefusedStarts++;
        }
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

// A PES trap's touch is the body contact scan's grant (FUN_1406EDCE0, docs/ports/pes-port-TrapContact.md): the trap tick sets the
// contact-pending bit on the clip's contact frame, the scan's reach test passes or refuses it, and only a pass touches the ball. Every
// tick, each man in a PES trap clip is filed by whether the scan granted him a touch this tick (the grant counter moved) and whether he
// touched the ball this tick (his last-touch time is now).
class TrapContactProbe : public pestest::Probe {
 public:
  TrapContactProbe()
      : Probe("PesStallContact", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long grants = 0;               // ticks a man in a trap clip took a grant
  long restingGrants = 0;        // those with the ball at rest (under a metre per second) at the tick's start
  long touchesWithoutGrant = 0;  // a man in a trap clip touched the ball with no grant: a touch with no PES source
  long grantsWithoutTouch = 0;   // a grant that did not touch the ball
  // Match::Process advances the clock at its end (match.cpp, `actualTime_ms += kTick_ms`), after the bodies stamped this tick's
  // touches, so a touch made on the tick that just ran carries the clock as it stood when the tick began.
  void OnStart(Match& match) override { tickStart_ms_ = match.GetActualTime_ms(); }
  void OnTick(Match& match) override {
    const float restingSpeed_mps = 1.0f;
    const float speed = match.GetBall()->GetMovement().GetLength();
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        const int slot = match.PesSlotOf(player.get());
        const unsigned taken = match.PesTouchesTaken(slot);
        const bool granted = taken != taken_[slot];
        taken_[slot] = taken;
        const bool touched = player->GetLastTouchTime_ms() == tickStart_ms_;
        if (granted) {
          grants++;
          if (previousSpeed_ < restingSpeed_mps) restingGrants++;
          if (!touched) grantsWithoutTouch++;
        } else if (touched && player->CastHumanoid()->OnPesTrap()) {
          touchesWithoutGrant++;
        }
      }
    }
    previousSpeed_ = speed;
    tickStart_ms_ = match.GetActualTime_ms();
  }

 private:
  std::map<int, unsigned> taken_;
  float previousSpeed_ = 0.0f;
  unsigned long tickStart_ms_ = 0;
};

TrapContactProbe contactCensus;

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

// On PES's path every man plays a motion PES issued (MOVESTEP's, or another PES action's): the anime player has no clip of GF's own. A
// body whose clip carries no PES motion number (pesmovestep::CurrentMotionOf < 0, the case P+0xAE0 holds the none index for) is a path
// where GF's own animation selection (Humanoid::SelectAnim) replaced PES's issue. Each man-tick on such a clip is filed by the
// function type and the clip's folder, and each START of one by what the man was playing before and what handed the clip over.
class GfClipProbe : public pestest::Probe {
 public:
  GfClipProbe()
      : Probe("PesStallGfClip", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long manTicks = 0;      // active men, ticks
  long gfClipTicks = 0;   // those on a GF-native clip
  long loopClipsLost = 0;  // a locomotion clip PES issued in IDLE, RUN_LOOP or STEP_LOOP replaced by a GF clip
  std::map<std::string, long> byClip;    // "fn / folder": ticks
  std::map<std::string, long> byEntry;   // "from -> to (interrupt, command)": clip starts
  void OnTick(Match& match) override {
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        Humanoid* body = player->CastHumanoid();
        const Anim* clip = body->GetCurrentAnim();
        const bool gf = pesmovestep::CurrentMotionOf(*player) < 0;
        const std::string name = clip->anim ? clip->anim->GetName() : "-";
        Seen& seen = seen_[player.get()];
        manTicks++;
        if (gf) {
          gfClipTicks++;
          byClip["fn " + std::to_string(int(clip->functionType)) + " " + Folder(name) + " | movestep " +
                 std::to_string(int(player->GetMoveStep().state))]++;
        }
        if (name != seen.name || clip->frameNum < seen.frame) {
          if (gf && seen.looping) loopClipsLost++;
          if (gf)
            byEntry[std::string(seen.gf ? "GF " : "PES ") + "fn" + std::to_string(seen.function) + " -> fn" +
                    std::to_string(int(clip->functionType)) + " " + Folder(name) + " (interrupt " +
                    std::to_string(int(clip->originatingInterrupt)) + ", command fn " +
                    std::to_string(int(clip->originatingCommand.desiredFunctionType)) + ")"]++;
        }
        const pes::MoveState state = player->GetMoveStep().state;
        seen = {name, clip->frameNum, gf, int(clip->functionType),
                body->OnPesLocomotion() && (state == pes::MoveState::kIdle || state == pes::MoveState::kRunLoop ||
                                            state == pes::MoveState::kStepLoop)};
      }
    }
  }

 private:
  struct Seen {
    std::string name;
    int frame = 0;
    bool gf = false;
    int function = 0;
    bool looping = false;  // a MOVESTEP locomotion clip in a state whose clip PES's anime player loops
  };
  // The clip's folder under gf_anims, its first three components ("movement/idle/135_accel.anim" -> "movement/idle"); autogen blends have none.
  static std::string Folder(const std::string& name) {
    const std::size_t at = name.find("gf_anims/");
    if (at == std::string::npos) return name.compare(0, 7, "autogen") == 0 ? "autogen" : name.substr(0, name.find(' ')).substr(0, 24);
    const std::string rest = name.substr(at + 9);
    const std::size_t last = rest.rfind('/');
    return last == std::string::npos ? rest : rest.substr(0, last);
  }
  std::map<const Player*, Seen> seen_;
};

GfClipProbe gfClipCensus;

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

// docs/ports/pes-port-ClipOwner.md: on PES's path no man plays a clip of GF's own. Every clip is a motion PES issued, so P+0xAE0 names a
// motion of PES's Animetable and gate 4a can admit the man.
TEST(PesStall, NoPesLoopManPlaysAClipOfGfsOwn) {
  pestest::Run(gfClipCensus);
  if (gfClipCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(gfClipCensus.setupError.empty()) << gfClipCensus.setupError;
  pestest::Settle(gfClipCensus);
  for (const auto& [key, ticks] : gfClipCensus.byClip) std::fprintf(stderr, "[gfclip] %8ld ticks  %s\n", ticks, key.c_str());
  for (const auto& [key, starts] : gfClipCensus.byEntry) std::fprintf(stderr, "[gfclip-start] %6ld starts  %s\n", starts, key.c_str());
  ASSERT_TRUE(gfClipCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(gfClipCensus.gfClipTicks, 0) << gfClipCensus.gfClipTicks << " of " << gfClipCensus.manTicks
                                         << " man-ticks were on a clip of GF's own";
}

// The clip a MOVESTEP handler issued in IDLE, RUN_LOOP or STEP_LOOP is one PES's anime player loops (the handlers issue again only when
// the request changes), so GF's end-of-clip switch must not put a clip of GF's own over it. Each such replacement is counted.
TEST(PesStall, ALoopingLocomotionClipIsNeverReplacedByAGfClip) {
  pestest::Run(gfClipCensus);
  if (gfClipCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(gfClipCensus.setupError.empty()) << gfClipCensus.setupError;
  pestest::Settle(gfClipCensus);
  ASSERT_TRUE(gfClipCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(gfClipCensus.loopClipsLost, 0) << gfClipCensus.loopClipsLost << " looping PES locomotion clips were replaced by a GF clip";
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

// Gate 4a refuses a man whose current action is not in 0x14079BBE0's list (a tackle, the keeper's catch and deflect, no action at all): the
// reaction never hands him a trap, so no trap start leaves one of those actions.
TEST(PesStall, NoTrapStartsFromAnActionGate4aRefuses) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapStarts, 0) << "no PES trap started";
  EXPECT_EQ(trapCensus.gateRefusedStarts, 0) << trapCensus.gateRefusedStarts << " of " << trapCensus.trapStarts
                                             << " PES trap starts left an action gate 4a does not admit";
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

// The trap's touch is PES's body contact scan, not the clip's contact frame: a man in a PES trap clip touches the ball on the tick the
// scan grants it and on no other, and a grant always touches. GF's own gate at the clip's frame (touchframe) has no PES source.
TEST(PesStall, ATrapTouchIsTheBodyScansGrantAndNothingElse) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.grants, 0) << "the body scan granted no touch in the whole match";
  EXPECT_EQ(contactCensus.touchesWithoutGrant, 0)
      << contactCensus.touchesWithoutGrant << " touches by a man in a trap clip with no grant of the body scan";
  EXPECT_EQ(contactCensus.grantsWithoutTouch, 0)
      << contactCensus.grantsWithoutTouch << " of " << contactCensus.grants << " grants did not touch the ball";
}

// A resting ball a trapping man reaches is the case PES's reach test decides and GF's distance gate used to refuse (the loose-ball
// stalls: men 0.4 to 2.7 m from a ball at rest who never play it): at least one grant is a touch of a ball at rest.
TEST(PesStall, ATrapReachesABallAtRest) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_GT(contactCensus.restingGrants, 0) << "no trap touched a ball at rest in the whole match";
}
