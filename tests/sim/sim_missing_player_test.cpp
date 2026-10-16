// The AI reads other players through two queries that can come back empty: the
// mental image's lookup by player ID, and the closest active opponent. These
// tests pin what each one does when no player answers.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "pes_match_constants.hpp"

#include "data/matchdata.hpp"
#include "onthepitch/AIsupport/mentalimage.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 1000;
constexpr int MATCH_MINUTES = 5;

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

// The fixture match, with the player skeleton and the match constants read from the
// PES install. `match` stays null without one, and the test skips.
struct FixtureMatch {
  blunted::Database db;
  std::unique_ptr<Match> match;

  FixtureMatch() {
    if (!db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return;
    SetDB(&db);
    MatchSetup setup;
    setup.homeTeamDbId = HOME_TEAM;
    setup.awayTeamDbId = AWAY_TEAM;
    setup.seed = SEED;
    setup.durationMinutes = MATCH_MINUTES;
    setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
    setup.rig = SharedRig();
    if (!setup.rig || !pestest::FillMissingPesConstants(&setup)) return;
    match.reset(new Match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties()));
  }
  ~FixtureMatch() {
    match.reset();
    SetDB(nullptr);
  }
};

// The squad holds bench players with no humanoid, so only an active player has a
// position to read or a state to deactivate.
Player* FirstActive(Team* team) {
  for (const auto& player : team->GetAllPlayers())
    if (player->IsActive()) return player.get();
  return nullptr;
}

void DeactivateAll(Team* team) {
  for (const auto& player : team->GetAllPlayers())
    if (player->IsActive()) player->Deactivate();
}

}  // namespace

// No snapshot taken, so the image holds no player at all.
TEST(SimMissingPlayer, MentalImageLookupWithNoPlayersThrows) {
  MentalImage image(nullptr);
  EXPECT_THROW(image.GetPlayerImage(0), std::runtime_error);
}

// The away side is out of the snapshot. The lookup must not hand back the home
// player's image in its place.
TEST(SimMissingPlayer, MentalImageLookupOfAnAbsentPlayerThrows) {
  FixtureMatch fixture;
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES";
  const int homeID = FirstActive(fixture.match->GetTeam(0))->GetID();
  const int awayID = FirstActive(fixture.match->GetTeam(1))->GetID();
  DeactivateAll(fixture.match->GetTeam(1));

  MentalImage image(fixture.match.get());
  image.TakeSnapshot();

  EXPECT_EQ(image.GetPlayerImage(homeID).playerID, homeID);
  EXPECT_THROW(image.GetPlayerImage(awayID), std::runtime_error);
}

// The away side has no active player, so there is no opponent to be close to.
TEST(SimMissingPlayer, ClosestOpponentDistanceWithNoActiveOpponentIsInfinite) {
  FixtureMatch fixture;
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES";
  Player* player = FirstActive(fixture.match->GetTeam(0));
  DeactivateAll(fixture.match->GetTeam(1));

  EXPECT_TRUE(std::isinf(player->GetClosestOpponentDistance()));
}

// Control: with one away player active, the distance is to him.
TEST(SimMissingPlayer, ClosestOpponentDistanceMeasuresTheOnlyActiveOpponent) {
  FixtureMatch fixture;
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES";
  Player* player = FirstActive(fixture.match->GetTeam(0));
  Team* away = fixture.match->GetTeam(1);
  Player* lone = FirstActive(away);
  for (const auto& other : away->GetAllPlayers())
    if (other.get() != lone && other->IsActive()) other->Deactivate();

  EXPECT_FLOAT_EQ(player->GetClosestOpponentDistance(),
                  lone->GetPosition().GetDistance(player->GetPosition()));
}
