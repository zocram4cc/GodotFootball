#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <algorithm>
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
#include "onthepitch/pesclips.hpp"
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
// Seed 7's home keeper stalls at a goal kick about nineteen minutes in; twenty covers it.
constexpr int TWENTY_MINUTES = 20;
// The same 30-minute cap as the other whole-match tests, counted in the 54 Hz logic
// frames the sim ticks in.
constexpr unsigned long kMaxSimTicks = 30ul * 60ul * Match::kFramesPerSecond;
// The 60-minute cap the install's gfsim_run uses, for the twenty-minute match: with its stoppages it
// runs about 114600 ticks, past the 30-minute cap above.
constexpr unsigned long kTwentyMinuteMaxSimTicks = 60ul * 60ul * Match::kFramesPerSecond;

// One match with the tick-order switch set as asked. `onTick` sees the match after each tick; the watch stops the run at
// the first stall. `seed` picks the match; `judgeEnd` runs the whole-match checks (off for a run that stops short).
void RunMatchWithinTheBound(bool pesLoop, int minutes, const std::function<void(Match&)>& onTick,
                            unsigned long maxSimTicks = kMaxSimTicks, unsigned int seed = SEED,
                            bool judgeEnd = true) {
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
  setup->seed = seed;
  setup->durationMinutes = minutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", pesLoop);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, config);
  RestartWatch watch(
      RestartLimitTicks(*setup->pesStoppageConstants, *setup->pesSetPieceTakerConstants));
  while (!match.IsGameOver() && !watch.Stalled() && watch.Ticks() < maxSimTicks) {
    match.Process();
    watch.Observe(match);
    if (onTick) onTick(match);
  }

  if (judgeEnd) {
    EXPECT_FALSE(watch.Stalled()) << "a stoppage ran past " << watch.LimitTicks()
                                  << " ticks without a restart (longest " << watch.LongestTicks() << ")";
    EXPECT_TRUE(match.IsGameOver()) << "the match did not reach full time";
    EXPECT_GT(watch.MeasuredStoppages(), 0ul) << "no stoppage was measured";
    EXPECT_LE(watch.LongestTicks(), watch.LimitTicks());
  }
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

// The whole match at twenty minutes with pes_loop on. Seed 7's home keeper, taking a goal kick about
// nineteen minutes in, stood 2.3 m off the ball: his walk was refused behind the MOVESTEP clip's stale
// command, and nothing restarted the match. Every stoppage must restart inside the bound and the match
// must reach full time.
TEST(PesRestartDelay, EveryStoppageRestartsWithinTheInstallBoundAtTwentyMinutes) {
  RunMatchWithinTheBound(true, TWENTY_MINUTES, nullptr, kTwentyMinuteMaxSimTicks);
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
      // The timed taker is released on the first frame at or past the PES gate, give or take the tick the referee
      // checks it on. A goal kick has no gate (ReleaseFrames is zero): AGoalKickIsReleasedWhenItsKeeperReachesTheBall
      // checks it.
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
// the throw was refused for a ball out of bounds. The ball must be at its spot when the throw is
// released - the carry spot the taker lifts it at (FUN_140791ec0, 3.2), within the lift radius of
// the taker - and the match must run through it.
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
    const Vector3 spot = match.GetTeam(b.teamID)->GetController()->GetPieceWalkTarget();
    const Vector3 ball = match.GetBall()->Predict(0);
    const float off = (ball.Get2D() - spot.Get2D()).GetLength();
    EXPECT_LE(off, match.GetPesRestartSpotConstants().throwInCarryStop_m)
        << "throw-in released with the ball " << off << " m from its spot";
  });
  EXPECT_GT(throwIns, 0) << "no throw-in was released";
}

// PES's taker picks the ball up where it lies and carries it out to the throw spot before the throw
// (FUN_140791ec0, phases 3-4): at the release the taker's spot is the carry spot, 0.7 m outside the
// touchline and not the restart point, and the ball is in his hand there.
TEST(PesRestartDelay, AThrowInIsCarriedToItsSpotAndReleasedThere) {
  std::optional<unsigned long> checkedStop;
  int throwIns = 0;
  RunMatchWithinTheBound(true, LONG_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.started) return;
    if (!match.IsInPlay() || !match.IsInSetPiece()) return;
    if (checkedStop && *checkedStop == b.stopTime) return;
    checkedStop = b.stopTime;
    throwIns++;
    const pes::RestartSpotConstants& k = match.GetPesRestartSpotConstants();
    const Vector3 spot = match.GetTeam(b.teamID)->GetController()->GetPieceWalkTarget();
    const float carried = (spot.Get2D() - b.restartPos.Get2D()).GetLength();
    EXPECT_GT(carried, 0.4f) << "released at the pickup point, not after the carry";
    EXPECT_NEAR(std::fabs(spot.coords[1]) - pesFieldHalfWidth, k.throwInCarryOutset_m, 1e-3f);
    const Vector3 ball = match.GetBall()->Predict(0);
    EXPECT_LE((ball.Get2D() - spot.Get2D()).GetLength(), k.throwInCarryStop_m)
        << "the ball was not in the taker's hand at the spot";
  });
  EXPECT_GT(throwIns, 0) << "no throw-in was released";
}

// A loose dead ball in play must be reached and played. Seed 7 stalled here after a restart was
// taken: the designated man stood about a metre from a ball at rest, and his step onto it never
// reached the PES mover, so the ball lay until the restart watch gave up. The bound is GF's
// stalemate clock, the only time GF gives a dead ball in play before it forces the issue. PES's
// own pickup time is not decoded, so this is a ponytail bound, not a PES number.
// DISABLED 17-10-26 (Main): red on main by design until the loose-ball pickup lands. The designated
// man reaches the ball, then AI_HasPossession hands control to GF's on-ball branch and he walks off;
// PES's first touch (FUN_1404f4be0 helpers) is undecoded. See docs/ports/pes-port-DeadBall.md.
TEST(PesRestartDelay, DISABLED_ALooseDeadBallInPlayIsReachedAndPlayed) {
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

// A throw-in must be taken within the bound, and its taker must play a ball action then. Seed 7's
// first throw-in is released with its taker in a PES locomotion clip. The throw-in spot sits F1 + the
// install's outset outside the touchline (the referee's real restart path), so the taker's candidate
// touches all predict a ball past the line and, without the taker's-own-spot exemption, are refused
// as out of bounds (humanoid GetBestCheatableAnimID), and no pass or shot is selected and the match
// stalls. This test requires the taker's ball action within the bound counted from the stoppage, as
// the restart watch counts it.
TEST(PesRestartDelay, AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine) {
  const auto isBallAction = [](int type) {
    return type == e_FunctionType_ShortPass || type == e_FunctionType_LongPass ||
           type == e_FunctionType_HighPass || type == e_FunctionType_Shot;
  };
  struct Taking {
    Player* taker;
    unsigned long stoppedAt;  // the tick the stoppage began
    std::string name;
  };
  std::map<unsigned long, unsigned long> stoppedAt;  // by stop time
  std::map<unsigned long, Taking> taking;            // released, not yet taken
  std::set<unsigned long> seen;
  unsigned long tick = 0, bound = 0;
  int throwIns = 0, onLocomotion = 0, untaken = 0;
  std::string who;
  RunMatchWithinTheBound(true, LONG_MINUTES, [&](Match& match) {
    ++tick;
    bound = RestartLimitTicks(match.GetPesStoppageConstants(), match.GetPesSetPieceTakerConstants());
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (b.active && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker)
      stoppedAt.emplace(b.stopTime, tick);
    if (b.active && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker &&
        seen.insert(b.stopTime).second) {
      const Anim* anim = b.taker->GetCurrentAnim();
      if (anim && anim->functionType == e_FunctionType_Special && anim->anim &&
          pes::IsPesLocomotionFamily(std::atoi(anim->anim->GetVariable("specialvar1").c_str())))
        onLocomotion++;
      throwIns++;
      taking[b.stopTime] =
          Taking{b.taker, stoppedAt[b.stopTime], b.taker->GetPlayerData()->GetLastName()};
    }
    for (auto it = taking.begin(); it != taking.end();) {
      const Anim* anim = it->second.taker->GetCurrentAnim();
      if (anim && isBallAction(anim->functionType)) {
        it = taking.erase(it);
      } else if (tick - it->second.stoppedAt > bound) {
        untaken++;
        who += " taker " + it->second.name + ";";
        it = taking.erase(it);
      } else {
        ++it;
      }
    }
  });
  EXPECT_GT(throwIns, 0) << "no throw-in was released";
  EXPECT_GT(onLocomotion, 0) << "no throw-in taker was in a PES locomotion clip at release";
  EXPECT_EQ(untaken, 0) << untaken << " throw-in taker(s) played no ball action within " << bound
                        << " ticks of the stoppage with the ball over the line:" << who;
}

// A throw-in taker walks to his spot (PES's seamless run, docs/re/pes-re-throwin-choreography.md
// 3.1): he is not placed on it. Right after the placement he is still away from the restart
// point, and at the release he is on it.
TEST(PesRestartDelay, AThrowInTakerWalksToHisSpotAndIsNotPlacedOnIt) {
  constexpr float kOnTheSpot_m = 2.0f;  // the spot is about half a metre off the restart point
  std::set<unsigned long> placed, released;
  int throwIns = 0, awayAtPlacement = 0, offSpotAtRelease = 0;
  RunMatchWithinTheBound(true, LONG_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.taker) return;
    const float off = (b.taker->GetPosition().Get2D() - b.restartPos.Get2D()).GetLength();
    if (b.prepared && placed.insert(b.stopTime).second) {
      throwIns++;
      if (off > kOnTheSpot_m) awayAtPlacement++;
    }
    if (b.started && released.insert(b.stopTime).second && off > kOnTheSpot_m) offSpotAtRelease++;
  });
  EXPECT_GT(throwIns, 0) << "no throw-in was placed";
  EXPECT_GT(awayAtPlacement, 0) << "every throw-in taker was placed on his spot";
  EXPECT_EQ(offSpotAtRelease, 0) << offSpotAtRelease << " throw-in taker(s) released off their spot";
}

// A throw-in's ball lies over the line at its spot (PES writes it at F1 + 0.25), and the taker's pass
// carries it inward over the next few ticks. PES's judge acts on the ball event raised as the ball
// crosses the line (docs/re/pes-re-A5Judge.md 1.4), so a ball already over the line at the release
// raises none. A judge that read the ball's centre on every tick would award the throw again to the
// other side at the same spot, on the first tick after the release. No throw-in may be awarded again
// within a second of a release.
TEST(PesRestartDelay, AThrowInIsNotAwardedAgainOnItsOwnSpot) {
  constexpr double kRewardWindow_ms = 1000.0;  // no pass from a spot reaches the far line sooner
  std::optional<unsigned long> awarded, released;
  double releasedAt_ms = 0.0;
  int throwIns = 0, reawarded = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn) return;
    const double now_ms = static_cast<double>(match.GetActualTime_ms());
    if (!awarded || *awarded != b.stopTime) {
      if (released && now_ms < releasedAt_ms + kRewardWindow_ms) reawarded++;
      awarded = b.stopTime;
    }
    if (b.started && (!released || *released != b.stopTime)) {
      released = b.stopTime;
      releasedAt_ms = now_ms;
      throwIns++;
    }
  });
  EXPECT_GT(throwIns, 0) << "no throw-in was released";
  EXPECT_EQ(reawarded, 0) << reawarded << " throw-in(s) awarded again within a second of a release, at the spot";
}

// PES's whistle kills the ball where it stands: the live throw-in sits past the line at its stop
// frame and shows no outfield flight during the stop. So on the tick the referee takes the ball out
// of play the ball's velocity and its spin are both zero - the resting-ball gate
// (pesballphysics.cpp IsResting) needs the spin zero too, or the flight function keeps stepping a
// "dead" ball and it drifts off the spot the writer put it on.
TEST(PesRestartDelay, AWhistleLeavesNoFlyingBallBehind) {
  constexpr float kRestSpeed_ms = 0.05f;
  float beforeTheWhistle = 0.0f;
  std::optional<unsigned long> seenStop;
  int stops = 0, flyingStops = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    Ball* ball = match.GetBall();
    const RefereeBuffer& b = match.GetRefereeBuffer();
    const float movement = ball->GetMovement().GetLength();
    if (!b.active) {
      beforeTheWhistle = movement;
      return;
    }
    if (seenStop && *seenStop == b.stopTime) return;
    seenStop = b.stopTime;
    stops++;
    if (beforeTheWhistle > kRestSpeed_ms) flyingStops++;
    EXPECT_LE(movement, kRestSpeed_ms)
        << "the ball was still moving at " << movement << " m/s at the whistle of stop "
        << b.stopTime << " (it was at " << beforeTheWhistle << " m/s the tick before)";
    const Quaternion& spin = ball->GetSpin();
    const float spinAngle_rad =
        2.0f * std::acos(std::min(1.0f, std::fabs(spin.elements[3])));
    EXPECT_LE(spinAngle_rad, 1e-3f)
        << "the ball was still spinning at " << spinAngle_rad << " rad/tick at the whistle of stop "
        << b.stopTime;
  });
  EXPECT_GT(stops, 0) << "no stoppage was seen";
  EXPECT_GT(flyingStops, 0) << "no stoppage caught a ball in flight - the test proved nothing";
}

// A goal kick is released when its keeper reaches the ball, not on a clock: PES releases it on the taker's reach and
// kick (A5 §4.7; pessetpiecetaker.hpp). The keeper is not placed on the ball; he runs to it. The taker is checked on
// the tick the restart starts, when the referee has just passed its reach test, so the tolerance is one tick of his
// movement.
TEST(PesRestartDelay, AGoalKickIsReleasedWhenItsKeeperReachesTheBall) {
  constexpr float kReleaseTickTolerance_m = 0.1f;
  std::optional<unsigned long> checkedStop;
  int goalKicks = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || !b.started || b.desiredSetPiece != e_SetPiece_GoalKick || !b.taker) return;
    if (checkedStop && *checkedStop == b.stopTime) return;
    checkedStop = b.stopTime;
    goalKicks++;
    const float reach = match.GetPesSetPieceTakerConstants().takerReach_m;
    const float off = (b.taker->GetPosition().Get2D() - match.GetBall()->Predict(0).Get2D()).GetLength();
    EXPECT_LT(off, reach + kReleaseTickTolerance_m)
        << "the goal kick was released with its keeper " << off << " m from the ball";
  });
  EXPECT_GT(goalKicks, 0) << "no goal kick was released";
}

// A mover whose PES clip reaches its last frame must go on. On seed 57 a presser (team 0, id 4) held zero speed in
// ACCELERATE for over a thousand ticks, commanded 8 m/s, on a clip held at its last frame: the clip's end issued motion 0 on
// every tick, since its exit never wrote the run's motion word. The longest such run must stay under one second.
TEST(PesRestartDelay, AMoverWhoseClipEndsGoesOnAndMoves) {
  constexpr int kFrozenLimitTicks = 54;  // one second at 54 Hz
  int run = 0, longest = 0;
  RunMatchWithinTheBound(
      true, MINUTES,
      [&](Match& match) {
        Player* p = match.GetTeam(0)->GetAllPlayers().at(4).get();
        const PesDecision& decision = p->GetPesDecision();
        const bool frozen = p->GetMoveStep().state == pes::MoveState::kAccelerate &&
                            p->GetMovement().GetLength() < 0.01f && decision.command &&
                            decision.command->desiredVelocityFloat > 1.0f;
        run = frozen ? run + 1 : 0;
        longest = std::max(longest, run);
      },
      1300, /*seed=*/57, /*judgeEnd=*/false);
  EXPECT_LT(longest, kFrozenLimitTicks) << "the presser held zero speed on a finished clip for " << longest << " ticks";
}

// Every PES clip MOVESTEP issues on seed 3 plays at a positive rate. The playback word is a float that the copy-in fills
// from P+0x3750 (1.0); the integer writes into it left 0.0 on the kick-off stance (motion 2220) and others.
TEST(PesRestartDelay, ASeedThreePesIssuePlaysAtAPositiveRate) {
  std::map<int, unsigned int> lastSerial;
  int issues = 0, zeroRate = 0;
  RunMatchWithinTheBound(
      true, MINUTES,
      [&](Match& match) {
        for (int t = 0; t < 2; ++t) {
          if (!match.GetTeam(t)) continue;
          const auto& all = match.GetTeam(t)->GetAllPlayers();
          for (size_t i = 0; i < all.size(); ++i) {
            Player* q = all[i].get();
            if (!q) continue;
            const pes::IssuedClip& ic = q->GetIssuedClip();
            const int key = t * 100 + static_cast<int>(i);
            auto it = lastSerial.find(key);
            if (it != lastSerial.end() && it->second == ic.serial) continue;
            lastSerial[key] = ic.serial;
            if (ic.request.group >= 0 || ic.request.motion < 0) continue;  // PES SetMotion clips only
            ++issues;
            if (!(ic.request.playback > 0.0f)) ++zeroRate;
          }
        }
      },
      kMaxSimTicks, 3, false);
  EXPECT_GT(issues, 0) << "no PES clip was issued";
  EXPECT_EQ(zeroRate, 0) << zeroRate << " of " << issues << " PES clip issues played at rate zero";
}

// Seed 23's kick-off restarts. On 92a0d28 the kick-off taker looped between idle and GotoRun at the spot, because the
// RunLoop turn-stop issue read the copied-in default word instead of the playback word its own arm wrote (PES's
// param_2[0x35], RunLoop), and the stoppage ran past the bound.
TEST(PesRestartDelay, TheSeedTwentyThreeKickOffRestarts) {
  RunMatchWithinTheBound(true, MINUTES, nullptr, kMaxSimTicks, 23);
}

// A finished trap must not freeze a mover. On seed 42 (b1f958a) a loose presser sat in a trap clip at zero speed while
// commanded past a metre a second, for hundreds of ticks: the clip's end never handed the body back, because the FSM's hold
// blocked the end-of-clip switch (FirstTouchPort2's freeze). No mover may hold that state for 120 ticks.
TEST(PesRestartDelay, AFinishedTrapDoesNotFreezeAMover) {
  constexpr int kFrozenLimitTicks = 120;
  std::map<int, int> run;
  int longest = 0;
  RunMatchWithinTheBound(
      true, MINUTES,
      [&](Match& match) {
        for (int t = 0; t < 2; ++t) {
          std::vector<Player*> active;
          match.GetActiveTeamPlayers(t, active);
          for (size_t i = 0; i < active.size(); ++i) {
            Player* pl = active[i];
            if (!pl) continue;
            const bool frozen = pl->CastHumanoid() && pl->GetCurrentFunctionType() == e_FunctionType_Trap &&
                                pl->GetMovement().GetLength() < 0.05f && pl->GetPesDecision().command &&
                                pl->GetPesDecision().command->desiredVelocityFloat > 1.0f;
            int& r = run[t * 100 + static_cast<int>(i)];
            r = frozen ? r + 1 : 0;
            if (r > longest) longest = r;
          }
        }
      },
      12000, 42, false);
  EXPECT_LT(longest, kFrozenLimitTicks) << "a mover held a trap at zero speed while commanded for " << longest << " ticks";
}
