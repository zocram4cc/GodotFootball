// The match reads a player's injury tier and stronger foot from the PES player database record of his database id
// (the PES player id). A player with no record takes the named fallback (the middle tier, the right foot).
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pes_match_constants.hpp"

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "pes/playerdb.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 1000;
constexpr int MATCH_MINUTES = 5;

// One Player.bin record for `id` with raw injury tier 2 (A) and the stronger foot on the left, as the file
// writes them: the id u32 at +8, the tier at bit 458, the foot at bit 514.
std::vector<uint8_t> LeftFootedAStarRecord(int id) {
  std::vector<uint8_t> rec(pes::kPlayerRecordBytes, 0);
  const uint32_t id32 = uint32_t(id);
  std::memcpy(&rec[pes::kPlayerIdOffset], &id32, 4);
  rec[458 / 8] |= uint8_t(2u << (458 % 8));
  rec[514 / 8] |= uint8_t(1u << (514 % 8));
  return rec;
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

// The fixture match with the install's skeleton and constants, given the PES database to read traits from.
struct FixtureMatch {
  blunted::Database db;
  std::unique_ptr<Match> match;

  explicit FixtureMatch(const pes::PlayerDatabase* playerDb) {
    if (!db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return;
    SetDB(&db);
    MatchSetup setup;
    setup.homeTeamDbId = HOME_TEAM;
    setup.awayTeamDbId = AWAY_TEAM;
    setup.seed = SEED;
    setup.durationMinutes = MATCH_MINUTES;
    setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
    setup.rig = SharedRig();
    setup.pesPlayerDatabase = playerDb;
    if (!setup.rig || !pestest::FillMissingPesConstants(&setup)) return;
    match.reset(new Match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties()));
  }
  ~FixtureMatch() {
    match.reset();
    SetDB(nullptr);
  }
};

// The first two active home players: the one with a PES record and the one without.
std::vector<Player*> FirstTwoActive(Team* team) {
  std::vector<Player*> out;
  for (const auto& player : team->GetAllPlayers())
    if (player->IsActive() && out.size() < 2) out.push_back(player.get());
  return out;
}

}  // namespace

TEST(PlayerDatabaseWiring, APlayerWithARecordReadsItsInjuryTierAndForeFoot) {
  // The fixture's players carry database ids 1..32; a record for the first active home player's id.
  FixtureMatch probe(nullptr);
  if (!probe.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  const std::vector<Player*> home = FirstTwoActive(probe.match->GetTeam(0));
  ASSERT_EQ(home.size(), 2u);
  const int withRecord = home[0]->GetPlayerData()->GetDatabaseID();
  const int withoutRecord = home[1]->GetPlayerData()->GetDatabaseID();
  ASSERT_NE(withRecord, withoutRecord);
  probe.match.reset();

  std::string err;
  auto playerDb = pes::PlayerDatabase::Parse(LeftFootedAStarRecord(withRecord), &err);
  ASSERT_TRUE(playerDb.has_value()) << err;
  FixtureMatch fixture(&*playerDb);
  ASSERT_TRUE(fixture.match);
  const std::vector<Player*> players = FirstTwoActive(fixture.match->GetTeam(0));
  ASSERT_EQ(players.size(), 2u);

  EXPECT_EQ(fixture.match->InjuryResistanceOf(players[0]->GetPlayerData()), 2);  // A from the record
  EXPECT_TRUE(fixture.match->LeftFootedOf(players[0]->GetPlayerData()));
  EXPECT_EQ(fixture.match->InjuryResistanceOf(players[1]->GetPlayerData()), 1);  // no record: B, the fallback
  EXPECT_FALSE(fixture.match->LeftFootedOf(players[1]->GetPlayerData()));
}

TEST(PlayerDatabaseWiring, WithoutADatabaseEveryPlayerTakesTheFallback) {
  FixtureMatch fixture(nullptr);
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  for (Player* player : FirstTwoActive(fixture.match->GetTeam(0))) {
    EXPECT_EQ(fixture.match->InjuryResistanceOf(player->GetPlayerData()), 1);
    EXPECT_FALSE(fixture.match->LeftFootedOf(player->GetPlayerData()));
  }
}
