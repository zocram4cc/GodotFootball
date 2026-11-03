#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

#include "data/matchdata.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

// A goal's kick-off waits for the goal scene: PES's DemoInfo runs the scene inside the restart step, so the restart's wipe,
// teleport and kick-off come after the scene's end, not at a time fixed when the ball crossed the line (GF's goal schedule
// knows nothing of the scene's length: the seed-7 scorer jumped 40 m at the second step because the kick-off placement
// fired under the scene; docs/ports/pes-port-ProcCamera8.md section 3). GfMatch raises HoldRestartForScene while a goal
// scene's timeline runs.
namespace {

constexpr int kHome = 1, kAway = 2;
constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 5;  // seed 7 scores its first goal well inside this
constexpr unsigned long kSceneLongerThanSchedule_ms = 15000;  // past the kick-off schedule by far
constexpr unsigned long kMaxTicksAfterRelease = 4000;
constexpr unsigned long kMaxTicksToGoal = 3ul * kMinutes * 60ul * Match::kFramesPerSecond;

void WithTheFirstGoal(bool pesLoop, const std::function<void(Match&)>& body) {
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
  setup->homeTeamDbId = kHome;
  setup->awayTeamDbId = kAway;
  setup->seed = kSeed;
  setup->durationMinutes = kMinutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", pesLoop);
  Match match(new MatchData(kHome, kAway), *setup, config);
  // The tick after the referee has armed the goal's kick-off: the scene's timeline starts here in GfMatch. Bounded: a restart
  // that never comes (a stall before the goal) must fail here, not hang the run.
  for (unsigned long t = 0; t < kMaxTicksToGoal && !match.IsGameOver() && !(match.IsGoalScored() && match.GetRefereeBuffer().active); ++t)
    match.Process();
  ASSERT_TRUE(match.IsGoalScored()) << "no goal in the match";
  body(match);
}

}  // namespace

class GoalSceneHold : public testing::TestWithParam<bool> {};

// With a scene running, the restart is neither prepared (the teleport to the centre) nor taken, however long the scene.
// Once the scene ends it comes with the spacing its schedule gave it, and the match restarts.
TEST_P(GoalSceneHold, TheKickOffWaitsForTheSceneAndThenKeepsItsSpacing) {
  WithTheFirstGoal(GetParam(), [](Match& match) {
    const RefereeBuffer armed = match.GetRefereeBuffer();
    ASSERT_EQ(armed.desiredSetPiece, e_SetPiece_KickOff);
    const unsigned long spacing_ms = armed.startTime - armed.prepareTime;
    const unsigned long sceneEnd_ms = armed.startTime + kSceneLongerThanSchedule_ms;
    while (match.GetActualTime_ms() < sceneEnd_ms) {
      match.HoldRestartForScene(true);
      match.Process();
      ASSERT_FALSE(match.GetRefereeBuffer().prepared) << "prepared under the scene at " << match.GetActualTime_ms();
      ASSERT_FALSE(match.GetRefereeBuffer().started) << "kick-off taken under the scene at " << match.GetActualTime_ms();
    }
    EXPECT_TRUE(match.IsGoalScored()) << "the goal state was cleared under the scene";

    match.HoldRestartForScene(false);
    const unsigned long released_ms = match.GetActualTime_ms();
    unsigned long preparedAt_ms = 0;
    bool restarted = false;
    for (unsigned long t = 0; t < kMaxTicksAfterRelease && !restarted && !match.IsGameOver(); ++t) {
      match.Process();
      const RefereeBuffer& b = match.GetRefereeBuffer();
      if (b.prepared && !preparedAt_ms) {
        preparedAt_ms = match.GetActualTime_ms();
        EXPECT_EQ(b.startTime - b.prepareTime, spacing_ms) << "the schedule's prepare-to-start spacing is kept";
      }
      restarted = match.IsInPlay() && !b.active;
    }
    ASSERT_GT(preparedAt_ms, 0ul) << "the restart never prepared after the scene";
    EXPECT_LE(preparedAt_ms, released_ms + 3 * static_cast<unsigned long>(Match::kTick_ms))
        << "the restart follows the scene's end at once";
    EXPECT_TRUE(restarted) << "the match did not restart after the scene";
  });
}

// No scene: the restart fires at the time its schedule gave it (the hold is not a standing delay). One mode: the PES loop's first
// goal is four minutes of wall time away, and the hold's code is the same in both.
TEST(GoalSceneNoHold, WithoutASceneTheKickOffKeepsItsSchedule) {
  WithTheFirstGoal(false, [](Match& match) {
    const RefereeBuffer armed = match.GetRefereeBuffer();
    unsigned long preparedAt_ms = 0;
    while (!preparedAt_ms && !match.IsGameOver() && match.GetActualTime_ms() < armed.prepareTime + 5000) {
      match.Process();
      if (match.GetRefereeBuffer().prepared) preparedAt_ms = match.GetActualTime_ms();
    }
    ASSERT_GT(preparedAt_ms, 0ul);
    EXPECT_LE(preparedAt_ms, armed.prepareTime + 2 * static_cast<unsigned long>(Match::kTick_ms));
  });
}

INSTANTIATE_TEST_SUITE_P(Loops, GoalSceneHold, testing::Values(true, false),
                         [](const testing::TestParamInfo<bool>& i) { return i.param ? "PesLoop" : "SwitchOff"; });
