#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <memory>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
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
// The same 30-minute cap as the other whole-match tests, counted in the 54 Hz logic
// frames the sim ticks in.
constexpr unsigned long kMaxSimTicks = 30ul * 60ul * Match::kFramesPerSecond;

}  // namespace

TEST(PesRestartDelay, EveryStoppageRestartsWithinTheInstallBound) {
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
  setup->durationMinutes = MINUTES;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, Properties());
  RestartWatch watch(
      RestartLimitTicks(*setup->pesStoppageConstants, *setup->pesSetPieceTakerConstants));
  while (!match.IsGameOver() && !watch.Stalled() && watch.Ticks() < kMaxSimTicks) {
    match.Process();
    watch.Observe(match);
  }

  EXPECT_FALSE(watch.Stalled()) << "a stoppage ran past " << watch.LimitTicks()
                                << " ticks without a restart (longest "
                                << watch.LongestTicks() << ")";
  EXPECT_TRUE(match.IsGameOver()) << "the match did not reach full time";
  EXPECT_GT(watch.MeasuredStoppages(), 0ul) << "no stoppage was measured";
  EXPECT_LE(watch.LongestTicks(), watch.LimitTicks());
}
