#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "gamedefines.hpp"
#include "gametypes.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
// The two-minute match with its stoppages, counted in the 54 Hz logic frames the sim ticks in, with headroom.
constexpr unsigned long kMaxTicks = 6ul * 60ul * Match::kFramesPerSecond;

// One PES-loop match on seed 7, `onTick` after each tick, until full time or `kMaxTicks`.
void RunPesLoopMatch(const std::function<void(Match&)>& onTick) {
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
  setup->seed = kSeed;
  setup->durationMinutes = kMinutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, config);
  for (unsigned long t = 0; !match.IsGameOver() && t < kMaxTicks; ++t) {
    match.Process();
    onTick(match);
  }
}

}  // namespace

// PES's ActionPress steers the loose-ball presser to its target, the ball now or his own base (docs/re/pes-re-press-life.md
// section 8, step 6). His movement command must name that target: a command without one projects its direction a second
// ahead (pesmovestep FillCommandWords), which overshoots a ball a few metres away. Scope: a presser who does not hold the ball
// and is not the PASS_GET receiver (ActionPassGet's approach is another PES action, not this port's), with a movement decision.
TEST(PesPressApproach, ALoosePresserCommandNamesTheActionPressTarget) {
  unsigned long steered = 0, unnamed = 0;
  RunPesLoopMatch([&](Match& match) {
    const bool loose = match.IsInPlay() && !match.IsInSetPiece() && !match.GetBallRetainer() &&
                       match.GetBall()->GetMovement().GetLength() <= GameplayTuning::kStalemateBallSpeed_ms;
    if (!loose) return;
    for (int t = 0; t < 2; ++t) {
      Team* team = match.GetTeam(t);
      Player* presser = team->GetController()->GetLoosePressMan();
      if (!presser || presser->HasPossession() || team->PassGetReceiver() == presser) continue;
      const std::optional<PlayerCommand>& command = presser->GetPesDecision().command;
      if (!command || command->desiredFunctionType != e_FunctionType_Movement) continue;
      ++steered;
      if (!command->useDesiredTarget) ++unnamed;
    }
  });
  EXPECT_GT(steered, 0ul) << "no loose presser was steered in the match";
  EXPECT_EQ(unnamed, 0ul) << unnamed << " of " << steered << " loose-presser movement commands named no target";
}
