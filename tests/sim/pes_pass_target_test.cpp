#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "gamedefines.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
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
constexpr unsigned long kMaxTicks = 6ul * 60ul * Match::kFramesPerSecond;

// The longest a team's pass target stayed set in PES's live play (docs/re/pes-re-receiving.md section 2: PASS_GET lasts 0.02 to 2.94 s).
constexpr double kPesPassTargetMaxLive_s = 2.94;
// The ball counts as at rest below GF's stalemate breaker's own bound.
constexpr double kRestSpeed_ms = GameplayTuning::kStalemateBallSpeed_ms;

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

// The pass target is PES's own team record T+0x78, rewritten by the selector chain every team tick (VA 0x1404FB2A0 -> 0x1404FB380;
// docs/ports/pes-port-TeamTick.md). GF used to clear its copy only at ResetSituation, so a pass that died on the grass kept its receiver
// (a keeper 35 m away for 8 s on seed 7, docs/ports/pes-port-RestingBall3.md). On the PES path a set target must (a) exist while a pass is
// in flight, and (b) be gone within PES's own live bound once the ball has come to rest in play.
TEST(PesPassTarget, IsSetInFlightAndNeverStaleWhileTheBallRests) {
  double restingSinceTargetSet_s = 0.0, worstStale_s = 0.0;
  unsigned long targetTicks = 0;
  RunPesLoopMatch([&](Match& match) {
    const bool targetSet = match.GetTeam(0)->GetPassTarget() != nullptr || match.GetTeam(1)->GetPassTarget() != nullptr;
    targetTicks += targetSet ? 1 : 0;
    const bool resting = match.IsInPlay() && match.GetBall()->GetMovement().GetLength() < kRestSpeed_ms;
    if (targetSet && resting)
      restingSinceTargetSet_s += static_cast<double>(Match::kTick_ms) / 1000.0;
    else
      restingSinceTargetSet_s = 0.0;
    worstStale_s = std::max(worstStale_s, restingSinceTargetSet_s);
  });
  EXPECT_GT(targetTicks, 0ul) << "the selector chain never named a pass target in the match";
  EXPECT_LE(worstStale_s, kPesPassTargetMaxLive_s) << "a pass target stayed set for " << worstStale_s << " s with the ball at rest";
}
