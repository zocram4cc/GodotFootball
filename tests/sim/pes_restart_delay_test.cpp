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
#include <vector>

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
constexpr unsigned int kDefaultSeed = 7;
// GF_SEED picks another seed; tools/dev/loose_episodes.sh runs the metric over several.
unsigned int SeedFromEnvironment() {
  const char* s = std::getenv("GF_SEED");
  return s ? static_cast<unsigned int>(std::strtoul(s, nullptr, 10)) : kDefaultSeed;
}
const unsigned int SEED = SeedFromEnvironment();
constexpr int MINUTES = 2;
// Seed 7 stalls a throw-in at about four minutes; five covers it.
constexpr int LONG_MINUTES = 5;
// Seed 7's home keeper stalls at a goal kick about nineteen minutes in; twenty covers it.
constexpr int TWENTY_MINUTES = 20;
// A throw-in is released from the line: its ball may not be further than this from the spot.
constexpr float kThrowInReleaseTolerance_m = 0.5f;
// The same 30-minute cap as the other whole-match tests, counted in the 54 Hz logic
// frames the sim ticks in.
constexpr unsigned long kMaxSimTicks = 30ul * 60ul * Match::kFramesPerSecond;
// The 60-minute cap the install's gfsim_run uses, for the twenty-minute match: with its stoppages it
// runs about 114600 ticks, past the 30-minute cap above.
constexpr unsigned long kTwentyMinuteMaxSimTicks = 60ul * 60ul * Match::kFramesPerSecond;

// One match with the tick-order switch set as asked. `onTick` sees the match after each tick;
// the watch stops the run at the first stall.
void RunMatchWithinTheBound(bool pesLoop, int minutes,
                            const std::function<void(Match&)>& onTick,
                            unsigned long maxSimTicks = kMaxSimTicks,
                            const std::function<bool(Match&)>& done = nullptr) {
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
  while (!match.IsGameOver() && !watch.Stalled() && watch.Ticks() < maxSimTicks) {
    match.Process();
    watch.Observe(match);
    if (onTick) onTick(match);
    if (done && done(match)) return;
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
// Re-enabled on FirstTouchPort: the loose-ball pickup lands here (PRESS chase
// + HitData touch frame + latch step 4b). Red until then.
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
    if (std::getenv("FIRSTTOUCH_TRACE")) {
      Player* des = match.GetDesignatedPossessionPlayer();
      const float d = des ? (match.GetBall()->Predict(0).Get2D() - des->GetPosition()).GetLength()
                          : -1.0f;
      printf("[ft] f=%lu ep=%lu man=%s dist=%.2f hasPoss=%d func=%d anim=%s lastTouch=%lu\n",
             match.GetSimFrame(), episodeTicks, des ? des->GetPlayerData()->GetLastName().c_str() : "-",
             d, des ? (int)des->HasPossession() : -1,
             des ? (int)des->GetCurrentFunctionType() : -1,
             des ? des->GetCurrentAnim()->anim->GetName().c_str() : "-",
             des ? match.GetActualTime_ms() - des->GetLastTouchTime_ms() : 0);
      const auto presser = [&](int t) -> std::string {
        Player* p = match.GetTeam(t)->GetController()->GetLoosePressMan();
        return p ? p->GetPlayerData()->GetLastName() : "-";
      };
      printf("[ftp] f=%lu presser=%s|%s\n", match.GetSimFrame(), presser(0).c_str(), presser(1).c_str());
      if (Player* hp = match.GetTeam(0)->GetController()->GetLoosePressMan()) {
        printf("[ftpd] f=%lu dist=%.2f func=%d anim=%s speed=%.2f\n", match.GetSimFrame(),
               (match.GetBall()->Predict(0).Get2D() - hp->GetPosition().Get2D()).GetLength(),
               static_cast<int>(hp->GetCurrentFunctionType()),
               hp->GetCurrentAnim()->anim->GetName().c_str(), hp->GetMovement().GetLength());
      }
    }
    if (episodeTicks > longestTicks) longestTicks = episodeTicks;
  });
  EXPECT_GT(episodes, 0) << "no loose dead ball in play was observed";
  EXPECT_LE(longestTicks, kBoundTicks)
      << "a loose dead ball in play lay for " << longestTicks << " ticks (bound " << kBoundTicks << ")";
}

// PES stops renewing a PRESS man once the loose ball is more than 13 m from him (press-life 12, the 0x1425C6788
// distance row). Seed 7 keeps a presser 13.1 to 13.5 m off a ball lying loose for thousands of ticks.
TEST(PesRestartDelay, ALoosePresserBeyondThirteenMetresIsNotKept) {
  constexpr float kRenewalBall_m = 13.0f;  // press-life 12: the 13 m loose-ball gate (0x1425C6788)
  constexpr float kRestSpeed_ms = GameplayTuning::kStalemateBallSpeed_ms;
  unsigned long farRun = 0, farTicks = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const bool loose = match.IsInPlay() && !match.IsInSetPiece() && !match.GetBallRetainer() &&
                       match.GetBall()->GetMovement().GetLength() <= kRestSpeed_ms;
    bool far = false;
    if (loose) {
      for (int t = 0; t < 2; ++t) {
        Player* p = match.GetTeam(t)->GetController()->GetLoosePressMan();
        if (p && (match.GetBall()->Predict(0).Get2D() - p->GetPosition().Get2D()).GetLength() > kRenewalBall_m)
          far = true;
      }
    }
    farRun = far ? farRun + 1 : 0;
    if (farRun > 1) ++farTicks;  // one tick of grace: the renewal runs on the team tick
  });
  EXPECT_EQ(farTicks, 0ul) << "a presser stood more than " << kRenewalBall_m << " m from a loose ball for " << farTicks
                           << " ticks";
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

// A throw-in's ball must go inside the touchline when the taker releases it. The release is the taker's
// touch frame: the referee ends the set piece there (referee.cpp, "check if set piece has been taken"),
// which is PES's release MI+0x116c 3->1 (docs/re/pes-re-throwin-choreography.md, the summary). A release
// that launches no ball leaves it on the spot, and the match stalls. 27 ticks is the mid release clip's
// release event at 54 Hz (the doc's release table); a launched ball is inside the line within a few ticks.
TEST(PesRestartDelay, AReleasedThrowInCarriesItsBallInsideTheTouchline) {
  constexpr unsigned long kCrossWithinTicks = 27;
  unsigned long tick = 0, releasedAt = 0;
  bool throwInSetPiece = false, awaitingCross = false;
  int releases = 0, stalls = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    ++tick;
    const RefereeBuffer& b = match.GetRefereeBuffer();
    const bool inSetPiece = match.IsInSetPiece();
    if (inSetPiece && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn) throwInSetPiece = true;
    if (throwInSetPiece && !inSetPiece) {
      throwInSetPiece = false;
      awaitingCross = true;
      releasedAt = tick;
      releases++;
    }
    if (!awaitingCross) return;
    if (std::fabs(match.GetBall()->Predict(0).coords[1]) <= pesFieldHalfWidth) {
      awaitingCross = false;
    } else if (tick - releasedAt > kCrossWithinTicks) {
      stalls++;
      awaitingCross = false;
    }
  });
  EXPECT_GT(releases, 0) << "no throw-in was released";
  EXPECT_EQ(stalls, 0) << stalls << " released throw-in(s) left the ball outside the touchline for more than "
                       << kCrossWithinTicks << " ticks";
}

// PES's touchline judge reads the out-of-play event the zone code raises when it changes (K=10). The
// restart's gate holds the code at 0 while the restart is awarded, so the code changes when the gate
// releases: a ball resting past the touchline is then judged out. The event is read one tick after it
// is raised, as the code leads the judge by a frame (docs/re/pes-re-out-side.md 5). The test releases
// the gate the way the taker's touch does (referee.cpp, "check if set piece has been taken").
TEST(PesRestartDelay, ABallRestingPastTheTouchlineIsJudgedOutWhenTheGateReleases) {
  constexpr float kRestPastSpot_m = 0.29f;  // the stall's distance past the spot
  constexpr unsigned long kJudgeWithinTicks = 2;
  unsigned long tick = 0, placedAt = 0;
  bool armed = false, judged = false;
  RunMatchWithinTheBound(
      true, MINUTES,
      [&](Match& match) {
        ++tick;
        if (armed) return;
        const RefereeBuffer& b = match.GetRefereeBuffer();
        if (!match.IsInSetPiece() || !b.started || !b.active ||
            b.desiredSetPiece != e_SetPiece_ThrowIn)
          return;
        const Vector3 spot = b.restartPos;
        const float side = spot.coords[1] > 0 ? 1.0f : -1.0f;
        Ball* ball = match.GetBall();
        ball->SetPosition(
            Vector3(spot.coords[0], side * (std::fabs(spot.coords[1]) + kRestPastSpot_m), 0.0f));
        ball->Stop();
        match.StopSetPiece();
        const_cast<RefereeBuffer&>(b).active = false;
        armed = true;
        placedAt = tick;
      },
      kMaxSimTicks,
      [&](Match& match) {
        if (!armed || tick < placedAt + kJudgeWithinTicks) return false;
        judged = !match.IsInPlay();
        return true;
      });
  EXPECT_TRUE(armed) << "no throw-in was released to place the ball past its spot";
  EXPECT_TRUE(judged) << "a ball at rest " << kRestPastSpot_m
                      << " m past its spot was not judged out within " << kJudgeWithinTicks
                      << " ticks of the gate releasing";
}

// PES's end-line judge reads the same zone event as the touchline (K=10, bit 0x8; FUN_14050e4a0 case 10).
// A ball at rest behind the end line when a restart's gate releases is judged once: the event is read
// once, and the corner or goal kick it awards leaves the ball inside, so no second award follows.
TEST(PesRestartDelay, ABallAtRestBehindTheEndLineIsJudgedOnceWhenTheGateReleases) {
  constexpr float kBehindEndLine_m = 0.29f;  // the touchline test's distance past the line
  constexpr unsigned long kWatchTicks = 600;  // the next restart's hold and taking, about 11 s
  unsigned long tick = 0, armedAt = 0, lastStop = 0;
  bool armed = false;
  int endLineAwards = 0;
  RunMatchWithinTheBound(
      true, MINUTES,
      [&](Match& match) {
        ++tick;
        const RefereeBuffer& b = match.GetRefereeBuffer();
        if (!armed) {
          if (!match.IsInSetPiece() || !b.started || !b.active ||
              b.desiredSetPiece != e_SetPiece_ThrowIn)
            return;
          match.GetBall()->SetPosition(Vector3(pesFieldHalfLength + kBehindEndLine_m, 0.0f, 0.0f));
          match.GetBall()->Stop();
          match.StopSetPiece();
          const_cast<RefereeBuffer&>(b).active = false;
          armed = true;
          armedAt = tick;
          lastStop = b.stopTime;
          return;
        }
        if (b.stopTime != lastStop && b.active &&
            (b.desiredSetPiece == e_SetPiece_Corner || b.desiredSetPiece == e_SetPiece_GoalKick)) {
          endLineAwards++;
          lastStop = b.stopTime;
        }
      },
      kMaxSimTicks, [&](Match&) { return armed && tick >= armedAt + kWatchTicks; });
  EXPECT_TRUE(armed) << "no throw-in was released to place the ball behind the end line";
  EXPECT_EQ(endLineAwards, 1) << endLineAwards << " end-line restart(s) awarded for one ball behind the line";
}

// PES judges the end line from the zone event one tick after the zone code flips: the judge reads the event on
// the next frame (docs/re/pes-re-A5Judge.md 1.4; docs/re/pes-re-out-side.md 5, the code leads the judge by a
// frame). Natural crossing: the first end-line flip of the seed-7 match (trace: the flip at ms 136166, the
// judge at ms 136185, one 54 Hz tick later). The restart it awards must come on the flip tick plus one.
TEST(PesRestartDelay, ATheFirstEndLineCrossingIsJudgedOnTheNextTick) {
  unsigned long tick = 0, flipAt = 0, awardAt = 0, lastStop = 0;
  e_SetPiece awardKind = e_SetPiece_None;
  bool live = false;  // the judge's gate at the start of the tick: in play, not in a set piece, none awarded
  RunMatchWithinTheBound(
      true, MINUTES,
      [&](Match& match) {
        ++tick;
        const RefereeBuffer& b = match.GetRefereeBuffer();
        if (flipAt == 0 && live) {
          const Vector3 p = match.GetBall()->Predict(0);
          if (pes::OutOfPlayCode(p.coords[0], p.coords[1], match.GetPesBallConstants().image.ballRadius) & 0x8)
            flipAt = tick;
        }
        if (flipAt != 0 && awardAt == 0 && b.active && b.stopTime != lastStop) {
          awardAt = tick;
          awardKind = b.desiredSetPiece;
        }
        if (flipAt == 0) lastStop = b.stopTime;
        live = match.IsInPlay() && !match.IsInSetPiece() && !b.active;
      },
      kMaxSimTicks, [&](Match&) { return awardAt != 0; });
  EXPECT_NE(flipAt, 0ul) << "the seed-7 match has no end-line crossing";
  EXPECT_EQ(awardAt, flipAt + 1) << "the end-line restart came on tick " << awardAt
                                 << ", not on the judge's tick after the flip at " << flipAt;
}

// PES scores the goal in the judge, on the judge's frame, one frame after the zone code flips (cc = 1 in
// FUN_14050e4a0 case 10, docs/re/pes-re-A5Judge.md 1.4). The natural crossing of seed 7 flips at tick 7354 and is
// judged on 7355 (ATheFirstEndLineCrossingIsJudgedOnTheNextTick). The score must not move on the flip tick.
TEST(PesRestartDelay, ANaturalGoalIsCountedOnTheJudgesFrame) {
  unsigned long tick = 0, flipAt = 0, lastStop = 0;
  bool live = false;
  int goalsBefore = 0, goalsAtFlip = -1, goalsAfter = -1;
  const auto goals = [](Match& match) {
    return match.GetMatchData()->GetGoalCount(0) + match.GetMatchData()->GetGoalCount(1);
  };
  RunMatchWithinTheBound(
      true, MINUTES,
      [&](Match& match) {
        ++tick;
        const RefereeBuffer& b = match.GetRefereeBuffer();
        if (flipAt == 0 && live) {
          const Vector3 p = match.GetBall()->Predict(0);
          if (pes::OutOfPlayCode(p.coords[0], p.coords[1], match.GetPesBallConstants().image.ballRadius) & 0x8) {
            flipAt = tick;
            goalsAtFlip = goals(match);
          }
        }
        if (flipAt == 0) {
          goalsBefore = goals(match);
          lastStop = b.stopTime;
        } else if (tick == flipAt + 1) {
          goalsAfter = goals(match);
        }
        live = match.IsInPlay() && !match.IsInSetPiece() && !b.active;
      },
      kMaxSimTicks, [&](Match&) { return goalsAfter >= 0; });
  EXPECT_NE(flipAt, 0ul) << "the seed-7 match has no end-line crossing";
  EXPECT_EQ(goalsAtFlip, goalsBefore) << "the score moved on the flip tick, before the judge read the event";
  EXPECT_EQ(goalsAfter, goalsBefore + 1) << "the goal was not counted on the judge's frame";
}

// The loose-episode metric, the acceptance number on the PES path: per seed, the loose episodes longer than
// the stall bound (over) and the excess ticks past it. Disabled so the suite does not run four whole matches;
// tools/dev/loose_episodes.sh runs it over the seeds (--gtest_also_run_disabled_tests).
TEST(PesRestartDelay, DISABLED_LooseEpisodeMetric) {
  constexpr float kRestSpeed_ms = GameplayTuning::kStalemateBallSpeed_ms;
  constexpr unsigned long kBoundTicks = static_cast<unsigned long>(
      GameplayTuning::kStalemateForceAction_s * static_cast<float>(Match::kFramesPerSecond));
  constexpr size_t kTopEpisodes = 3;  // the longest over-bound episodes per seed, printed for the cause table
  bool inEpisode = false;
  unsigned long episodeTicks = 0, episodeStart = 0, episodes = 0, over = 0, excess = 0;
  std::vector<std::pair<unsigned long, unsigned long>> longEpisodes;  // (ticks, start frame)
  const auto closeEpisode = [&] {
    if (episodeTicks > kBoundTicks) {
      ++over;
      excess += episodeTicks - kBoundTicks;
      longEpisodes.emplace_back(episodeTicks, episodeStart);
    }
  };
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const bool loose = match.IsInPlay() && !match.IsInSetPiece() && !match.GetBallRetainer() &&
                       match.GetBall()->GetMovement().GetLength() <= kRestSpeed_ms;
    if (loose) {
      if (!inEpisode) {
        inEpisode = true;
        episodeTicks = 0;
        episodeStart = match.GetSimFrame();
        ++episodes;
      }
      ++episodeTicks;
    } else if (inEpisode) {
      inEpisode = false;
      closeEpisode();
    }
  });
  if (inEpisode) closeEpisode();  // an episode still open when the stall watch stopped the run
  ASSERT_GT(episodes, 0ul) << "no match ran; is PES21_ROOT set?";
  printf("[loose-episodes] seed=%u episodes=%lu over=%lu excess=%lu\n", SEED, episodes, over, excess);
  std::sort(longEpisodes.begin(), longEpisodes.end(), std::greater<>());
  for (size_t i = 0; i < longEpisodes.size() && i < kTopEpisodes; ++i)
    printf("[loose-top] seed=%u rank=%zu start=%lu ticks=%lu\n", SEED, i + 1, longEpisodes[i].second,
           longEpisodes[i].first);
}
