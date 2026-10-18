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
      // The taker is released when he reaches the ball, or on the first frame at or past the
      // PES gate when he never gets there - give or take the tick the referee checks it on.
      const int gate = pes::ReleaseFrames(b.desiredSetPiece);
      const int frames =
          static_cast<int>((match.GetActualTime_ms() - b.startTime) / Match::kTick_ms);
      if (gate > 0) {
        EXPECT_LE(frames, gate + 2) << "released late for set piece " << b.desiredSetPiece;
        float takerD = 1e9f;
        if (b.taker != nullptr)
          takerD = (b.taker->GetPosition().Get2D() - match.GetBall()->Predict(0).Get2D())
                       .GetLength();
        const float reach = match.GetPesRestartSpotConstants().takerReach_m;
        EXPECT_TRUE(frames >= gate || takerD <= reach + 1e-3f)
            << "set piece " << b.desiredSetPiece << " released at frame " << frames << " of "
            << gate << " with the taker " << takerD << " m away";
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
// The writer's throw-in spot survives the prepare: PES's null-spot path (FUN_14050e770, VA
// 0x14050e770) puts it F1 + 0.25 outside the touchline with no F1 clamp - the clamp is the
// foul (explicit-spot) path. Preparing the restart must not pull it back onto the line.
TEST(PesRestartDelay, AThrowInKeepsItsWriterSpotThroughPrepare) {
  std::optional<unsigned long> checkedStop;
  int throwIns = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.prepared || b.started)
      return;
    if (checkedStop && *checkedStop == b.stopTime) return;
    checkedStop = b.stopTime;
    throwIns++;
    EXPECT_GT(std::fabs(b.restartPos.coords[1]), pesFieldHalfWidth)
        << "throw-in prepared on the line at z = " << b.restartPos.coords[1];
  });
  EXPECT_GT(throwIns, 0) << "no throw-in was prepared";
}
// The whistle kills the ball on the PES path: from the stop frame to the prepare the ball must
// not travel - PES's live throw-in sits past the line at its stop frame and shows no outfield
// flight during the stop. A ball still flying at prepare is the whistle without the kill.
TEST(PesRestartDelay, AWhistleLeavesNoFlyingBallBehind) {
  std::optional<unsigned long> checkedStop;
  int stops = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.prepared) return;
    if (checkedStop && *checkedStop == b.stopTime) return;
    checkedStop = b.stopTime;
    stops++;
    EXPECT_LE(match.GetBall()->GetMovement().GetLength(), 0.05f)
        << "ball flying at " << match.GetBall()->GetMovement().GetLength()
        << " m/s during the stop";
  });
  EXPECT_GT(stops, 0) << "no stoppage was seen";
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

// The taker reads the ball out of his own mental image, never out of the live ball:
// humanoid's touch loop takes `currentMentalImage->GetBallPrediction(animTouchFrame * 10)`
// (humanoid.cpp 3184, and again at 3391) and GetBodyBallDistanceAdvantage scores every
// candidate anim against that read. A restart moves the ball to its spot; the taker's read of
// the ball on a prepared restart tick must be that spot, not the ball's flight before the
// restart.
TEST(PesRestartDelay, ARestartPlacesTheBallInTheTakersMentalImage) {
  // The read the taker's own controller makes: an image this many ms old, read this many ms
  // ahead, which lands on the ball as it is now.
  constexpr int kReactionHistory_ms = 300;
  constexpr unsigned int kTouchAhead_ms = 300;
  // Half a metre: a taker aiming at a spot rather than at the ball is off by more than this.
  constexpr float kMentalImageTolerance_m = 0.5f;
  unsigned long preparedTicks = 0;
  float worst = 0.0f;
  unsigned long worstTick = 0;
  int worstPiece = 0;
  Vector3 worstReal, worstSeen;
  RunMatchWithinTheBound(true, LONG_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || !b.prepared || b.started) return;
    if (match.GetBall()->GetMovement().GetLength() > 0.05f) return;  // the ball is not dead yet
    const MentalImage* image = match.GetMentalImage(kReactionHistory_ms);
    if (!image) return;
    ++preparedTicks;
    const Vector3 real = match.GetBall()->Predict(0).Get2D();
    const Vector3 seen = image->GetBallPrediction(kTouchAhead_ms).Get2D();
    const float off = (seen - real).GetLength();
    if (off > worst) {
      worst = off;
      worstTick = preparedTicks;
      worstPiece = b.desiredSetPiece;
      worstReal = real;
      worstSeen = seen;
    }
  });
  EXPECT_GT(preparedTicks, 0ul) << "no prepared restart tick was seen";
  EXPECT_LT(worst, kMentalImageTolerance_m)
      << "prepared tick " << worstTick << " (set piece " << worstPiece << "): the taker's mental "
      << "image reads the ball at (" << worstSeen.coords[0] << ", " << worstSeen.coords[1]
      << ") but it is at (" << worstReal.coords[0] << ", " << worstReal.coords[1] << "), "
      << worst << " m off";
}
