#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/properties.hpp"
#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

// The tick-order switch (SW): Match::Process runs the head of the tick in one order per
// tick. These tests read Match::TickStages(), the stages of the last tick in the order
// they ran, from a real Match built from the user's install (skipped without PES21_ROOT).
namespace {

using Stage = Match::TickStage;

constexpr int kHomeTeam = 1, kAwayTeam = 2;
constexpr unsigned int kSeed = 1000;

blunted::Database* SharedDb() {
  static blunted::Database db;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    if (!db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return nullptr;
  }
  return &db;
}

const pesrig::Rig* SharedRig() {
  static std::optional<pesrig::Rig> rig;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return nullptr;
    pes::PesVfs vfs;
    std::string err;
    if (vfs.Mount(root, &err)) rig = pesrig::BuildRig(vfs, &err);
  }
  return rig ? &*rig : nullptr;
}

std::unique_ptr<Match> MakeMatch(const Properties& config) {
  MatchSetup setup;
  setup.homeTeamDbId = kHomeTeam;
  setup.awayTeamDbId = kAwayTeam;
  setup.seed = kSeed;
  setup.durationMinutes = 5;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  if (!pestest::FillMissingPesConstants(&setup)) return nullptr;
  return std::unique_ptr<Match>(new Match(new MatchData(kHomeTeam, kAwayTeam), setup, config));
}

#define REQUIRE_PES_MATCH(config)                                                          \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  blunted::Database* db = SharedDb();                                                    \
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";                                        \
  SetDB(db);                                                                             \
  std::unique_ptr<Match> match = MakeMatch(config);                                      \
  if (!match) GTEST_SKIP() << "PES2021.exe: the match constants are unavailable"

}  // namespace

// Switch off (the GF order): the referee, then the ball, then the event stage, then the
// mental image and the two teams.
TEST(PesTickOrder, SwitchOffRunsTheRefereeBeforeTheBall) {
  REQUIRE_PES_MATCH(Properties());
  match->Process();
  EXPECT_EQ(match->TickStages(),
            (std::vector<Stage>{Stage::Referee, Stage::Ball, Stage::Events, Stage::MentalImage,
                                Stage::Teams}));
}

// Switch on (the PES order): the ball and the event stage run before the referee, so the
// judge reads this tick's events. Every tick takes that order.
TEST(PesTickOrder, SwitchOnRunsTheBallAndEventsBeforeTheReferee) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  const std::vector<Stage> pesOrder = {Stage::Ball, Stage::Events, Stage::Referee,
                                       Stage::MentalImage, Stage::Teams};
  for (int tick = 0; tick < 3; ++tick) {
    match->Process();
    EXPECT_EQ(match->TickStages(), pesOrder) << "tick " << tick;
  }
}
