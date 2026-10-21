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
#include "onthepitch/pesgoalkick.hpp"
#include "onthepitch/pessequencer.hpp"
#include "onthepitch/restartwatch.hpp"
#include "pes/mbinfo.hpp"
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
// The five-minute run the path-B ceremony test uses. Seed 7 has no seamless throw-in within it (docs/ports/pes-port-ThrowInTaker.md).
constexpr int LONG_MINUTES = 5;
// Seed 7's home keeper stalls at a goal kick about nineteen minutes in; twenty covers it.
constexpr int TWENTY_MINUTES = 20;
// The path-A tests follow the first seamless (walker) throw-in of seed 3's twenty-minute match: the earliest of seeds 11,
// 13, 23 and 3 on the census (first walker prepared at ticks 21459, 7425, none within 30000, and 2461). The search stops
// at 30000 ticks. A trajectory change moves that throw-in, so the tests follow it rather than a fixed window.
constexpr unsigned int SEAMLESS_SEED = 3;
constexpr unsigned long kSeamlessSearchTicks = 30000;
// Seed 7's first throw-in on the twenty-minute match is a placed (ceremony) throw-in: prepared at tick 14428 and released
// at 14521 (docs/ports/pes-port-ThrowInTaker.md). The windowed tests run just past it.
constexpr unsigned long kFirstThrowInWindowTicks = 15000;
// The same 30-minute cap as the other whole-match tests, counted in the 54 Hz logic
// frames the sim ticks in.
constexpr unsigned long kMaxSimTicks = 30ul * 60ul * Match::kFramesPerSecond;
// The 60-minute cap the install's gfsim_run uses, for the twenty-minute match: with its stoppages it
// runs about 114600 ticks, past the 30-minute cap above.
constexpr unsigned long kTwentyMinuteMaxSimTicks = 60ul * 60ul * Match::kFramesPerSecond;

// One match with the tick-order switch set as asked. `onTick` sees the match after each tick; the watch stops the run at
// the first stall, and `done` (when it returns true) stops the run early. `seed` picks the match; `judgeEnd` runs the
// whole-match checks (off for a run that stops short).
void RunMatchWithinTheBound(bool pesLoop, int minutes, const std::function<void(Match&)>& onTick,
                            unsigned long maxSimTicks = kMaxSimTicks,
                            const std::function<bool(Match&)>& done = nullptr, unsigned int seed = SEED,
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
    if (done && done(match)) return;
  }

  if (judgeEnd) {
    EXPECT_FALSE(watch.Stalled()) << "a stoppage ran past " << watch.LimitTicks()
                                  << " ticks without a restart (longest "
                                  << watch.LongestTicks() << ")";
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
// DISABLED 21-10-26 (Main, ThrowInPathB): a loose ball at rest in play after a throw-in; the forced men stand 0.6 to 4.6 m
// away and never take it (the ALooseDeadBall far-chase defect). Re-enable with that fix.
TEST(PesRestartDelay, DISABLED_EveryStoppageRestartsWithinTheInstallBoundAtTwentyMinutes) {
  RunMatchWithinTheBound(true, TWENTY_MINUTES, nullptr, kTwentyMinuteMaxSimTicks);
}

// With pes_loop on, each set piece is armed where PES puts it (a goal kick on the goal-area line, a
// throw-in on the line just outside the touchline), to the programme's spacing, and released on
// its frame. Its side is the team that did not touch the ball last. No corner occurs before seed 7's
// first throw-in (kFirstThrowInWindowTicks); the corner's spot, side and gate are covered by pes_restart_sequencer_test.
TEST(PesRestartDelay, PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames) {
  std::optional<unsigned long> lastStop, lastRelease;
  std::map<int, int> armed;
  RunMatchWithinTheBound(true, TWENTY_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (b.active && b.started && !(lastRelease && *lastRelease == b.stopTime)) {
      lastRelease = b.stopTime;
      // The timed taker is released on the first frame at or past the PES gate, give or take the tick the referee
      // checks it on. A goal kick has no clock: AGoalKickIsReleasedOnThePesLadder checks its ladder frame.
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
  }, kFirstThrowInWindowTicks, nullptr, SEED, false);
  EXPECT_GT(armed[e_SetPiece_GoalKick], 0) << "no goal kick was armed";
  EXPECT_GT(armed[e_SetPiece_ThrowIn], 0) << "no throw-in was armed";
}

// A throw-in is taken from the spot the referee armed. Five minutes of seed 7 stalled here: the
// taker walked onto the spot while play was off, a body touch pushed the ball past the line, and
// the throw was refused for a ball out of bounds. The ball must be at its spot when the throw is
// released - the carry spot the taker lifts it at (FUN_140791ec0, 3.2), within the lift radius of
// the taker - and the match must run through it.
TEST(PesRestartDelay, AThrowInIsReleasedFromItsSpotAndTaken) {
  std::optional<unsigned long> firstStop;  // the first seamless (walker) throw-in's stoppage
  bool firstDone = false;                  // that stoppage has ended: the run stops here
  std::optional<unsigned long> checkedStop;
  int throwIns = 0;
  RunMatchWithinTheBound(true, TWENTY_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    // The first seamless (walker) throw-in of the run is the one this test follows: its stoppage, then done when it ends.
    if (!firstStop && b.active && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker && b.prepared &&
        match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker))
      firstStop = b.stopTime;
    if (firstStop && (!b.active || b.stopTime != *firstStop)) firstDone = true;
    if (!firstStop || b.stopTime != *firstStop) return;
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.started) return;
    if (!match.IsInPlay() || !match.IsInSetPiece()) return;
    if (checkedStop && *checkedStop == b.stopTime) return;
    checkedStop = b.stopTime;
    throwIns++;
    // A seamless taker's spot is his pickup point and carry spot. A ceremony taker (path B) is placed and releases from his hand.
    const TeamAIController* controller = match.GetTeam(b.teamID)->GetController();
    const Vector3 spot =
        controller->IsPieceTakerWalking(b.taker) ? controller->GetPieceWalkTarget() : b.taker->GetPosition();
    const Vector3 ball = match.GetBall()->Predict(0);
    const float off = (ball.Get2D() - spot.Get2D()).GetLength();
    EXPECT_LE(off, match.GetPesRestartSpotConstants().throwInCarryStop_m)
        << "throw-in released with the ball " << off << " m from its spot";
  }, kSeamlessSearchTicks, [&](const Match&) { return firstDone; }, SEAMLESS_SEED, false);
  EXPECT_GT(throwIns, 0) << "no throw-in was released";
}

// PES's taker picks the ball up where it lies and carries it out to the throw spot before the throw
// (FUN_140791ec0, phases 3-4): at the release the taker's spot is the carry spot, 0.7 m outside the
// touchline and not the restart point, and the ball is in his hand there.
TEST(PesRestartDelay, AThrowInIsCarriedToItsSpotAndReleasedThere) {
  std::optional<unsigned long> firstStop;  // the first seamless (walker) throw-in's stoppage
  bool firstDone = false;                  // that stoppage has ended: the run stops here
  std::optional<unsigned long> checkedStop;
  int throwIns = 0;
  RunMatchWithinTheBound(true, TWENTY_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    // The first seamless (walker) throw-in of the run is the one this test follows: its stoppage, then done when it ends.
    if (!firstStop && b.active && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker && b.prepared &&
        match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker))
      firstStop = b.stopTime;
    if (firstStop && (!b.active || b.stopTime != *firstStop)) firstDone = true;
    if (!firstStop || b.stopTime != *firstStop) return;
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.started) return;
    if (!match.IsInPlay() || !match.IsInSetPiece()) return;
    if (!match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker)) return;  // path A only
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
  }, kSeamlessSearchTicks, [&](const Match&) { return firstDone; }, SEAMLESS_SEED, false);
  EXPECT_GT(throwIns, 0) << "no seamless throw-in was released";
}

// A loose dead ball in play must be reached and played. Seed 7 stalled here after a restart was
// taken: the designated man stood about a metre from a ball at rest, and his step onto it never
// reached the PES mover, so the ball lay until the restart watch gave up. The bound is GF's
// stalemate clock, the only time GF gives a dead ball in play before it forces the issue. PES's
// own pickup time is not decoded, so this is a ponytail bound, not a PES number.
// Re-enabled on FirstTouchPort: the loose-ball pickup lands here (PRESS chase
// + HitData touch frame + latch step 4b). Red until then.
// DISABLED 22-10-26 (owner decision, batch-3 landing): red on the merged tree main 9c5cc89 + FirstTouchPort fca173c; main's earlier disable restored (the far-chase defect: a loose ball at rest in play beyond the press gate). Re-enable when FarChasePress lands the press coordinator.
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
// Path A only: a ceremony taker (path B) is placed and releases from his hand, which
// AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand covers.
TEST(PesRestartDelay, AThrowInTakerPlaysABallActionWhenTheBallLiesOverTheLine) {
  std::optional<unsigned long> firstStop;  // the first seamless (walker) throw-in's stoppage
  bool firstDone = false;                  // that stoppage has ended: the run stops here
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
  RunMatchWithinTheBound(true, TWENTY_MINUTES, [&](Match& match) {
    ++tick;
    bound = RestartLimitTicks(match.GetPesStoppageConstants(), match.GetPesSetPieceTakerConstants());
    const RefereeBuffer& b = match.GetRefereeBuffer();
    // The first seamless (walker) throw-in of the run is the one this test follows: its stoppage, then done when it ends.
    if (!firstStop && b.active && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker && b.prepared &&
        match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker))
      firstStop = b.stopTime;
    if (firstStop && (!b.active || b.stopTime != *firstStop)) firstDone = true;
    if (!firstStop || b.stopTime != *firstStop) return;
    if (b.active && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker)
      stoppedAt.emplace(b.stopTime, tick);
    if (b.active && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker &&
        match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker) && seen.insert(b.stopTime).second) {
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
  }, kSeamlessSearchTicks, [&](const Match&) { return firstDone; }, SEAMLESS_SEED, false);
  EXPECT_GT(throwIns, 0) << "no seamless throw-in was released";
  EXPECT_GT(onLocomotion, 0) << "no throw-in taker was in a PES locomotion clip at release";
  EXPECT_EQ(untaken, 0) << untaken << " throw-in taker(s) played no ball action within " << bound
                        << " ticks of the stoppage with the ball over the line:" << who;
}

// A throw-in taker walks to his spot (PES's seamless run, docs/re/pes-re-throwin-choreography.md
// 3.1): he is not placed on it. Right after the placement he is still away from the restart
// point, and at the release he is on it.
TEST(PesRestartDelay, AThrowInTakerWalksToHisSpotAndIsNotPlacedOnIt) {
  std::optional<unsigned long> firstStop;  // the first seamless (walker) throw-in's stoppage
  bool firstDone = false;                  // that stoppage has ended: the run stops here
  constexpr float kOnTheSpot_m = 2.0f;  // the spot is about half a metre off the restart point
  std::set<unsigned long> placed, released;
  int throwIns = 0, awayAtPlacement = 0, offSpotAtRelease = 0;
  RunMatchWithinTheBound(true, TWENTY_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    // The first seamless (walker) throw-in of the run is the one this test follows: its stoppage, then done when it ends.
    if (!firstStop && b.active && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker && b.prepared &&
        match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker))
      firstStop = b.stopTime;
    if (firstStop && (!b.active || b.stopTime != *firstStop)) firstDone = true;
    if (!firstStop || b.stopTime != *firstStop) return;
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.taker) return;
    if (!match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker)) return;  // path A only
    const float off = (b.taker->GetPosition().Get2D() - b.restartPos.Get2D()).GetLength();
    if (b.prepared && placed.insert(b.stopTime).second) {
      throwIns++;
      if (off > kOnTheSpot_m) awayAtPlacement++;
    }
    if (b.started && released.insert(b.stopTime).second && off > kOnTheSpot_m) offSpotAtRelease++;
  }, kSeamlessSearchTicks, [&](const Match&) { return firstDone; }, SEAMLESS_SEED, false);
  EXPECT_GT(throwIns, 0) << "no seamless throw-in was placed";
  EXPECT_GT(awayAtPlacement, 0) << "every seamless throw-in taker was placed on his spot";
  EXPECT_EQ(offSpotAtRelease, 0) << offSpotAtRelease << " throw-in taker(s) released off their spot";
  EXPECT_GT(released.size(), 0u) << "no seamless throw-in was released, so no release spot was checked";
}

// PES's ceremony taker (path B, docs/re/pes-re-throwin-choreography.md 2.1) takes a throw-in that no seamless candidate
// can. ThrowinLoop's SetKick (0x140794280, section 4.1) places him on the stand point: the spot less the release key's
// ball offset from clip 0xcfa's second record (FUN_1406e8cc0, index 1), outside the touchline, facing the pitch. The
// ball is in his hand when he releases.
TEST(PesRestartDelay, AThrowInCeremonyTakerIsPlacedOutsideTheLineAndReleasedWithTheBallInHand) {
  constexpr int kThrowInKeyMotion = 0xcfa;  // the throw-in's SetKick and release clip (pes-re-throwin-choreography.md 4.1)
  constexpr float kStandTolerance_m = 0.1f;
  std::optional<unsigned long> firstStop;  // the first ceremony (path B) throw-in's stoppage
  bool firstDone = false;                  // that stoppage has ended: the run stops here
  std::set<unsigned long> placed, released;
  int throwIns = 0, offTheStandPoint = 0, insideTheLine = 0, ballAway = 0;
  float releaseBall_m = 0.0f;  // the farthest the ball lay from the taker's body at any release
  // The release key's ball offset from the body, horizontal (docs/re/pes-re-throwin-choreography.md, SetKick: (0.022, 0.795) m
  // times bodyScale; bodyScale taken as 1). The carry's lift radius (0.65 m) is a different phase and does not apply here.
  constexpr float kReleaseHandReach_m = 0.80f;
  // The first ceremony throw-in moves with every change to the match before it, so the test follows it, as the path-A tests do.
  RunMatchWithinTheBound(true, TWENTY_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!firstStop && b.active && b.desiredSetPiece == e_SetPiece_ThrowIn && b.taker && b.prepared &&
        !match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker))
      firstStop = b.stopTime;
    if (firstStop && (!b.active || b.stopTime != *firstStop)) firstDone = true;
    if (!firstStop || b.stopTime != *firstStop) return;
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.taker) return;
    if (match.GetTeam(b.teamID)->GetController()->IsPieceTakerWalking(b.taker)) return;  // a walker is path A
    const Vector3 at = b.taker->GetPosition();
    if (b.prepared && placed.insert(b.stopTime).second) {
      throwIns++;
      const float off = (at.Get2D() - b.restartPos.Get2D()).GetLength();
      const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(kThrowInKeyMotion);
      const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants());
      const float standIn_m = hits.size() > 1 ? std::hypot(hits[1].ballX, hits[1].ballZ) * k.centimetre_m : -1.0f;
      if (standIn_m < 0.0f || std::fabs(off - standIn_m) > kStandTolerance_m) offTheStandPoint++;
      if (std::fabs(at.coords[1]) <= pesFieldHalfWidth) insideTheLine++;
    }
    if (b.started && released.insert(b.stopTime).second) {
      const Vector3 ball = match.GetBall()->Predict(0);
      const float away = (ball.Get2D() - at.Get2D()).GetLength();
      releaseBall_m = std::max(releaseBall_m, away);
      if (away > kReleaseHandReach_m) ballAway++;
    }
  }, kMaxSimTicks, [&](const Match&) { return firstDone; }, SEED, false);
  EXPECT_GT(throwIns, 0) << "no ceremony throw-in was placed";
  EXPECT_EQ(offTheStandPoint, 0) << offTheStandPoint << " ceremony taker(s) placed off the stand point";
  EXPECT_EQ(insideTheLine, 0) << insideTheLine << " ceremony taker(s) placed inside the touchline";
  EXPECT_EQ(ballAway, 0) << ballAway << " ceremony taker(s) released with the ball " << releaseBall_m
                         << " m from his body (farthest seen)";
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
  RunMatchWithinTheBound(true, TWENTY_MINUTES, [&](Match& match) {
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
  }, kFirstThrowInWindowTicks, nullptr, SEED, false);
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

// A goal kick is released by PES's timer ladder (sub_142136F00), not by the keeper's arrival: the lofted pass on the
// ladder's frame 82 from the armed tick when the picker finds a mate, the kick into space on frame 109 when it does not.
// The keeper is placed on his stand point at prepare and strikes onto the ball after the release. The gate checks the
// frame the referee releases on, counted from the armed tick, against the frame the mode the ladder chose allows.
TEST(PesRestartDelay, AGoalKickIsReleasedOnThePesLadder) {
  constexpr long kLoftedFrame = 83;   // the ladder's first lofted evaluation is frame 82; the hook reads the clock one tick later (PES live 83)
  constexpr long kForcedFrame = 110;  // the forced kick on ladder frame 109, read one tick later
  std::optional<unsigned long> checkedStop;
  int goalKicks = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || !b.started || b.desiredSetPiece != e_SetPiece_GoalKick || !b.taker) return;
    if (checkedStop && *checkedStop == b.stopTime) return;
    checkedStop = b.stopTime;
    goalKicks++;
    // The mode comes from the ladder's own answer: a receiver is a lofted pass, none a kick into space.
    const bool withReceiver = match.GetTeam(b.teamID)->GetController()->GoalKickReceiver() != nullptr;
    const long expected = withReceiver ? kLoftedFrame : kForcedFrame;
    const long framesFromArmed =
        std::lround(static_cast<double>(match.GetActualTime_ms() - b.startTime) / Match::kTick_ms);
    EXPECT_EQ(framesFromArmed, expected)
        << (withReceiver ? "lofted" : "forced") << " goal kick released off its ladder frame";
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
      1300, nullptr, /*seed=*/57, /*judgeEnd=*/false);
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
      kMaxSimTicks, nullptr, 3, false);
  EXPECT_GT(issues, 0) << "no PES clip was issued";
  EXPECT_EQ(zeroRate, 0) << zeroRate << " of " << issues << " PES clip issues played at rate zero";
}

// Seed 23's kick-off restarts. On 92a0d28 the kick-off taker looped between idle and GotoRun at the spot, because the
// RunLoop turn-stop issue read the copied-in default word instead of the playback word its own arm wrote (PES's
// param_2[0x35], RunLoop), and the stoppage ran past the bound.
TEST(PesRestartDelay, TheSeedTwentyThreeKickOffRestarts) {
  RunMatchWithinTheBound(true, MINUTES, nullptr, kMaxSimTicks, nullptr, 23);
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
      12000, nullptr, 42, false);
  EXPECT_LT(longest, kFrozenLimitTicks) << "a mover held a trap at zero speed while commanded for " << longest << " ticks";
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
// DISABLED 22-10-26 (owner decision, batch-3 landing): red on the merged tree main 9c5cc89 + FirstTouchPort fca173c; FirstTouchPort2 owns the fix (bisect against fca173c alone). Re-enable when it passes on main.
TEST(PesRestartDelay, DISABLED_ABallAtRestBehindTheEndLineIsJudgedOnceWhenTheGateReleases) {
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
  // Each loose tick has a class. A defender on a slide or a get-up clip is `slide`; otherwise a loose presser more than
  // kPresserNear_m from the ball is `approach`, and one within it is `contact`; with no presser, the nearest man beyond
  // kFarBall_m is `far`, and a nearer one `near-nopresser`. An over-bound episode takes its most frequent tick class, and
  // its excess is charged to that class.
  enum : int { kApproach, kFar, kNearNoPresser, kSlide, kContact, kClassCount };
  const char* const kClassNames[kClassCount] = {"approach", "far", "near-nopresser", "slide", "contact"};
  constexpr float kPresserNear_m = 1.5f;
  constexpr float kFarBall_m = 13.0f;
  unsigned long classTicks[kClassCount] = {}, classExcess[kClassCount] = {};
  const auto classifyTick = [&](Match& match) {
    const Vector3 b = match.GetBall()->Predict(0);
    float nearest = 1e9f, presser = 1e9f;
    bool slide = false;
    for (int t = 0; t < 2; ++t) {
      std::vector<Player*> active;
      match.GetActiveTeamPlayers(t, active);
      for (Player* p : active) {
        nearest = std::min(nearest, (p->GetPosition().Get2D() - b.Get2D()).GetLength());
        const auto* anim = p->GetCurrentAnim()->anim;
        if (p->GetCurrentFunctionType() == e_FunctionType_Sliding ||
            (anim && anim->GetVariable("incoming_special_state") != ""))
          slide = true;
      }
      if (Player* pr = match.GetTeam(t)->GetController()->GetLoosePressMan())
        presser = std::min(presser, (pr->GetPosition().Get2D() - b.Get2D()).GetLength());
    }
    if (slide) return int(kSlide);
    if (presser < 1e9f) return presser > kPresserNear_m ? int(kApproach) : int(kContact);
    return nearest > kFarBall_m ? int(kFar) : int(kNearNoPresser);
  };
  const auto closeEpisode = [&] {
    if (episodeTicks > kBoundTicks) {
      ++over;
      excess += episodeTicks - kBoundTicks;
      longEpisodes.emplace_back(episodeTicks, episodeStart);
      const int cls = int(std::max_element(classTicks, classTicks + kClassCount) - classTicks);
      classExcess[cls] += episodeTicks - kBoundTicks;
      printf("[loose-class] seed=%u start=%lu ticks=%lu class=%s\n", SEED, episodeStart, episodeTicks, kClassNames[cls]);
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
        std::fill(classTicks, classTicks + kClassCount, 0ul);
        ++episodes;
      }
      ++episodeTicks;
      ++classTicks[classifyTick(match)];
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
  printf("[loose-classes] seed=%u approach=%lu far=%lu near-nopresser=%lu slide=%lu contact=%lu\n", SEED,
         classExcess[kApproach], classExcess[kFar], classExcess[kNearNoPresser], classExcess[kSlide], classExcess[kContact]);
}

// The trap picker names the clip a loose presser plays from the install's trap rows: over seed 7's match, every
// trap-family clip the presser plays is named by one of the match's trap rows, and the presser plays some.
TEST(PesRestartDelay, ALoosePresserInTheTrapWindowPlaysThatRowsClip) {
  constexpr float kRestSpeed_ms = GameplayTuning::kStalemateBallSpeed_ms;
  std::set<int> rowClips;
  unsigned long trapTicks = 0, strayTicks = 0;
  RunMatchWithinTheBound(true, MINUTES, [&](Match& match) {
    if (rowClips.empty()) {
      for (const auto& row : match.GetPesDefenderDecisionConstants().receive.trapRows) rowClips.insert(row.clip);
    }
    const bool loose = match.IsInPlay() && !match.IsInSetPiece() && !match.GetBallRetainer() &&
                       match.GetBall()->GetMovement().GetLength() <= kRestSpeed_ms;
    if (!loose) return;
    Player* pr = nullptr;
    for (int t = 0; t < 2 && !pr; ++t) pr = match.GetTeam(t)->GetController()->GetLoosePressMan();
    if (!pr) return;
    const auto& anim = pr->GetCurrentAnim()->anim;
    if (anim->GetVariable("specialvar1") != std::to_string(int(pes::PesClipFamily::kTrap))) return;
    ++trapTicks;
    if (!rowClips.count(std::stoi(anim->GetVariable("specialvar2")))) ++strayTicks;
  });
  EXPECT_GT(trapTicks, 0ul) << "the loose presser never played a PES trap clip";
  EXPECT_EQ(strayTicks, 0ul) << "a PES trap clip is not named by a trap row";
}

// A throw-in carrier who has picked the ball up walks to his carry spot and stands there, ready to lift it (PES's
// SeamlessThrowin phase 4, docs/re/pes-re-throwin-choreography.md 3.2). Seed 7's five-minute match picked a walker's ball
// up at 130 s, 4.6 m short of the spot, and the carrier never arrived: the stoppage ran past its bound.
TEST(PesRestartDelay, ACarriedThrowInReachesItsSpot) {
  // The carry is a few seconds of one-handed running. A carrier four or five metres from his spot is there within this.
  constexpr unsigned long kCarryBoundTicks = 5ul * Match::kFramesPerSecond;
  std::optional<unsigned long> pickupTick;
  bool arrived = false;
  RunMatchWithinTheBound(true, LONG_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.taker || b.started) return;
    const TeamAIController* c = match.GetTeam(b.teamID)->GetController();
    if (!pickupTick && c->IsCarryingBall()) pickupTick = static_cast<unsigned long>(match.GetSimFrame());
    if (pickupTick && c->PieceTakerArrived()) arrived = true;
  }, kMaxSimTicks,
  [&](Match& match) {
    return arrived || (pickupTick && static_cast<unsigned long>(match.GetSimFrame()) - *pickupTick > kCarryBoundTicks);
  },
  SEED, false);
  EXPECT_TRUE(pickupTick.has_value()) << "no throw-in carrier picked the ball up";
  EXPECT_TRUE(arrived) << "the carrier did not reach his spot within " << kCarryBoundTicks << " ticks of the pickup";
}

// A throw-in carrier holds the ball only within reach of it. PES's pickup clip root-warps the taker onto the ball, so his hands
// meet the ball at the contact frame (docs/re/pes-re-throwin-choreography.md 3.2, phase 1). Seed 7's five-minute walker held
// the ball 4.0 m from where it had lain, and the ball jumped into his hand.
TEST(PesRestartDelay, AThrowInCarrierHoldsTheBallWithinHandReach) {
  constexpr float kHandReach_m = 0.80f;  // the release key's ball offset from the body (choreography 3.2), as the ceremony test
  std::optional<float> pickupReach;      // the carrier's distance to the loose ball on the tick he first holds it
  Vector3 looseBall;
  bool haveLooseBall = false;
  size_t pickupClipHits = 0;
  RunMatchWithinTheBound(true, LONG_MINUTES, [&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_ThrowIn || !b.taker || b.started) return;
    const TeamAIController* c = match.GetTeam(b.teamID)->GetController();
    if (!c->IsCarryingBall()) {
      looseBall = match.GetBall()->GetPositionBuffer();
      haveLooseBall = true;
      return;
    }
    if (!pickupReach && haveLooseBall) {
      pickupReach = (b.taker->GetPosition() - looseBall).Get2D().GetLength();
      pickupClipHits = match.GetPesMbinfo().Hits(0xcf5).size();
    }
  }, kMaxSimTicks, [&](Match&) { return pickupReach.has_value(); }, SEED, false);
  EXPECT_TRUE(pickupReach.has_value()) << "no throw-in carrier picked the ball up";
  if (pickupReach)
    EXPECT_LE(*pickupReach, kHandReach_m) << "the carrier held the ball " << *pickupReach << " m from where it lay (the pickup clip 0xCF5 has "
                                          << pickupClipHits << " hit records)";
}
