#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "data/matchdata.hpp"
#include "gametypes.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/pessequencer.hpp"
#include "onthepitch/restartwatch.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

// Every stoppage - whistle, goal or period break - must end in its restart within
// the bound the install sets for it (RestartLimitTicks). A restart that never comes
// freezes the match clock, so the match never reaches full time. The run stops at
// the first stall, so a stalled match fails in seconds rather than at the sim cap.
namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 7;
constexpr int MINUTES = 2;
// Seed 7 stalls a throw-in at about four minutes; five covers it.
constexpr int LONG_MINUTES = 5;
// A throw-in is released from the line: its ball may not be further than this from the spot.
constexpr float kThrowInReleaseTolerance_m = 0.5f;
// The same 30-minute cap as the other whole-match tests, counted in the 54 Hz logic
// frames the sim ticks in.
constexpr unsigned long kMaxSimTicks = 30ul * 60ul * Match::kFramesPerSecond;

// One match with the tick-order switch set as asked. `onTick` sees the match after each tick;
// the watch stops the run at the first stall.
void RunMatchWithinTheBound(bool pesLoop, int minutes,
                            const std::function<void(Match&)>& onTick) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql"));
  SetDB(&db);

  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = HOME_TEAM;
  setup->awayTeamDbId = AWAY_TEAM;
  setup->seed = SEED;
  setup->durationMinutes = minutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", pesLoop);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, config);
  RestartWatch watch(
      RestartLimitTicks(*setup->pesStoppageConstants, *setup->pesSetPieceTakerConstants));
  while (!match.IsGameOver() && !watch.Stalled() && watch.Ticks() < kMaxSimTicks) {
    match.Process();
    watch.Observe(match);
    if (onTick) onTick(match);
  }

  EXPECT_FALSE(watch.Stalled()) << "a stoppage ran past " << watch.LimitTicks()
                                << " ticks without a restart (longest "
                                << watch.LongestTicks() << ")";
  EXPECT_TRUE(match.IsGameOver()) << "the match did not reach full time";
  EXPECT_GT(watch.MeasuredStoppages(), 0ul) << "no stoppage was measured";
  EXPECT_LE(watch.LongestTicks(), watch.LimitTicks());
}

}  // namespace

TEST(PesRestartDelay, EveryStoppageRestartsWithinTheInstallBound) {
  RunMatchWithinTheBound(false, MINUTES, nullptr);
}

// The PES restart programme (pes_loop on): its release gates are longer than the GF taker's, and
// every restart must still arrive inside the watch's bound.
TEST(PesRestartDelay, EveryStoppageRestartsWithinTheInstallBoundWithPesLoop) {
  RunMatchWithinTheBound(true, MINUTES, nullptr);
}

// With pes_loop on, each set piece is armed where PES puts it (a goal kick on the goal-area line, a
// throw-in on the line just outside the touchline), to the programme's spacing, and released on
// its frame. Its side is the team that did not touch the ball last. A corner does not occur in this
// two-minute run; the corner's spot, side and gate are covered by pes_restart_sequencer_test.
TEST(PesRestartDelay, PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames) {
  std::optional<unsigned long> lastStop, lastRelease;
  std::map<int, int> armed;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (b.active && b.started && !(lastRelease && *lastRelease == b.stopTime)) {
      lastRelease = b.stopTime;
      // The taker is released on the first frame at or past the PES gate, give or take the
      // tick the referee checks it on.
      const int gate = pes::ReleaseFrames(b.desiredSetPiece);
      const int frames =
          static_cast<int>((match.GetActualTime_ms() - b.startTime) / Match::kTick_ms);
      if (gate > 0) {
        EXPECT_GE(frames, gate) << "released early for set piece " << b.desiredSetPiece;
        EXPECT_LE(frames, gate + 2) << "released late for set piece " << b.desiredSetPiece;
      }
    }
    if (!b.active || (lastStop && *lastStop == b.stopTime)) return;
    lastStop = b.stopTime;
    if (b.desiredSetPiece == e_SetPiece_KickOff) return;  // timed by GoalSequence, not PES's programme

    const pes::RestartSpotConstants& k = match.GetPesRestartSpotConstants();
    const pes::StoppageConstants& stoppage = match.GetPesStoppageConstants();
    // The programme's spacing from prepare to armed does not depend on the hold, so any hold works.
    const pes::Timing want =
        pes::RunProgramme(pes::BuildProgramme(b.desiredSetPiece), b.stopTime, stoppage, false, 5);
    EXPECT_EQ(static_cast<long>(b.startTime - b.prepareTime),
              static_cast<long>(want.armedAt_ms - want.prepareAt_ms))
        << "set piece " << b.desiredSetPiece;
    armed[b.desiredSetPiece]++;

    const float x = std::fabs(b.restartPos.coords[0]);
    const float y = std::fabs(b.restartPos.coords[1]);
    Team* lastTouch = match.GetLastTouchTeam();
    if (!lastTouch) lastTouch = match.GetTeam(0);
    switch (b.desiredSetPiece) {
      case e_SetPiece_GoalKick:
        EXPECT_NEAR(x, pesGoalAreaLineX + k.goalAreaPushOut_m, 1e-3);
        EXPECT_NEAR(y, k.goalAreaPushZ_m, 1e-3);
        break;
      case e_SetPiece_ThrowIn:
        EXPECT_LE(x, pesFieldHalfLength - 1.0f + 1e-3f);
        EXPECT_GT(y, pesFieldHalfWidth);
        EXPECT_LT(y, pesFieldHalfWidth + 1.0f);
        break;
      default:
        return;
    }
    EXPECT_EQ(b.teamID, pes::AwardedTeamAfterBallOut(lastTouch->GetID()));
  });
  EXPECT_GT(armed[e_SetPiece_GoalKick], 0) << "no goal kick was armed";
  EXPECT_GT(armed[e_SetPiece_ThrowIn], 0) << "no throw-in was armed";
}

// A throw-in is taken from the spot the referee armed. Five minutes of seed 7 stalled here: the
// taker walked onto the spot while play was off, a body touch pushed the ball past the line, and
// the throw was refused for a ball out of bounds. The ball must still be at its spot when the
// throw is released, and the match must run through it.
TEST(PesRestartDelay, AThrowInIsReleasedFromItsSpotAndTaken) {
  std::optional<unsigned long> checkedStop;
  int throwIns = 0;
  RunMatchWithinTheBound(true, LONG_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.started) return;
    if (!match.IsInPlay() || !match.IsInSetPiece()) return;
    if (checkedStop && *checkedStop == b.stopTime) return;
    checkedStop = b.stopTime;
    throwIns++;
    const Vector3 ball = match.GetBall()->Predict(0);
    const float off = (ball.Get2D() - b.restartPos.Get2D()).GetLength();
    EXPECT_LE(off, kThrowInReleaseTolerance_m)
        << "throw-in released with the ball " << off << " m from its spot";
  });
  EXPECT_GT(throwIns, 0) << "no throw-in was released";
}

// A loose dead ball in play must be reached and played. Seed 7 stalled here after a restart was
// taken: the designated man stood about a metre from a ball at rest, and his step onto it never
// reached the PES mover, so the ball lay until the restart watch gave up. The bound is GF's
// stalemate clock, the only time GF gives a dead ball in play before it forces the issue. PES's
// own pickup time is not decoded, so this is a ponytail bound, not a PES number.
TEST(PesRestartDelay, ALooseDeadBallInPlayIsReachedAndPlayed) {
  constexpr float kRestSpeed_ms = GameplayTuning::kStalemateBallSpeed_ms;
  constexpr unsigned long kBoundTicks = static_cast<unsigned long>(
      GameplayTuning::kStalemateForceAction_s * static_cast<float>(Match::kFramesPerSecond));
  bool inEpisode = false;
  unsigned long episodeTicks = 0, longestTicks = 0;
  int episodes = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const bool loose = match.IsInPlay() && !match.IsInSetPiece() && !match.GetBallRetainer() &&
                       match.GetBall()->GetMovement().GetLength() <= kRestSpeed_ms;
    if (!loose) {
      inEpisode = false;
      return;
    }
    if (!inEpisode) {
      inEpisode = true;
      episodeTicks = 0;
      episodes++;
    }
    episodeTicks++;
    if (episodeTicks > longestTicks) longestTicks = episodeTicks;
  });
  EXPECT_GT(episodes, 0) << "no loose dead ball in play was observed";
  EXPECT_LE(longestTicks, kBoundTicks)
      << "a loose dead ball in play lay for " << longestTicks << " ticks (bound " << kBoundTicks << ")";
}

// A restart taker must take the restart: within the bound after the release he plays a ball action
// (a pass, a lofted kick, a shot or a throw). A taker whose clip stays a PES locomotion clip is
// refused his ball action by SelectAnim, so this stays red until the requeue gate accepts it.
TEST(PesRestartDelay, ARestartTakerPlaysHisBallActionAfterRelease) {
  unsigned long bound = 0;  // the install's restart limit (RestartLimitTicks), set from the match each tick
  const auto isBallAction = [](int type) {
    return type == e_FunctionType_ShortPass || type == e_FunctionType_LongPass ||
           type == e_FunctionType_HighPass || type == e_FunctionType_Shot;
  };
  struct Taking {
    Player* taker;
    unsigned long releasedAt;
    int piece;
    std::string name;
  };
  std::map<unsigned long, Taking> taking;  // by stop time: restarts released, not yet taken
  std::set<unsigned long> seen;
  unsigned long tick = 0;
  int restarts = 0, untaken = 0;
  std::string who;  // the untaken restarts, named in the failure message
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    ++tick;
    bound = RestartLimitTicks(match.GetPesStoppageConstants(), match.GetPesSetPieceTakerConstants());
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (b.active && b.started && b.taker && seen.insert(b.stopTime).second) {
      taking[b.stopTime] = Taking{b.taker, tick, b.desiredSetPiece, b.taker->GetPlayerData()->GetLastName()};
      restarts++;
    }
    for (auto it = taking.begin(); it != taking.end();) {
      const Anim* anim = it->second.taker->GetCurrentAnim();
      if (anim && isBallAction(anim->functionType)) {
        it = taking.erase(it);
      } else if (tick - it->second.releasedAt > bound) {
        untaken++;
        who += " piece " + std::to_string(it->second.piece) + " taker " + it->second.name + ";";
        it = taking.erase(it);
      } else {
        ++it;
      }
    }
  });
  EXPECT_GT(restarts, 0) << "no restart was taken";
  EXPECT_EQ(untaken, 0) << untaken << " restart taker(s) played no ball action within " << bound
                        << " ticks of release:" << who;
}
