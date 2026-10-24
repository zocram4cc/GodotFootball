// The match reads a player's injury tier, stronger foot, Form and condition byte from the PES player database
// through his PES id only (PlayerData::GetPesPlayerId). A player with no PES id, the synthetic fixture squads
// among them, takes the named fallback whatever record the database holds under his database id.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
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
// teams.sql numbers its synthetic players 1..32.
constexpr int kFixturePlayerCount = 32;
// Raw injury tiers (matchlogic 1.2: 0 C, 1 B, 2 A) and the Player.bin bits the tests write.
constexpr uint8_t kTierC = 0, kTierA = 2;
constexpr size_t kInjuryBit = 458;
constexpr size_t kFootBit = 514;
constexpr size_t kFormBit = 438;
constexpr size_t kFormBits = 3;

// One Player.bin record for `id` with raw injury tier `tier`, the stronger foot on the left when `left`, and
// raw Form `form`.
std::vector<uint8_t> RecordFor(int id, uint8_t tier, bool left, uint8_t form = 3) {
  std::vector<uint8_t> rec(pes::kPlayerRecordBytes, 0);
  const uint32_t id32 = uint32_t(id);
  std::memcpy(&rec[pes::kPlayerIdOffset], &id32, 4);
  rec[kInjuryBit / 8] |= uint8_t(tier << (kInjuryBit % 8));
  if (left) rec[kFootBit / 8] |= uint8_t(1u << (kFootBit % 8));
  for (size_t i = 0; i < kFormBits; ++i)
    if ((form >> i) & 1u) rec[(kFormBit + i) / 8] |= uint8_t(1u << ((kFormBit + i) % 8));
  return rec;
}

std::vector<uint8_t> Concat(const std::vector<std::vector<uint8_t>>& records) {
  std::vector<uint8_t> out;
  for (const auto& rec : records) out.insert(out.end(), rec.begin(), rec.end());
  return out;
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

// The fixture match over the synthetic squads, reading PES traits from `playerDb` (null: none). `statements` run on the
// squads' database before the match is built, for a test's own rows.
struct FixtureMatch {
  blunted::Database db;
  std::unique_ptr<Match> match;

  explicit FixtureMatch(const pes::PlayerDatabase* playerDb, const std::vector<std::string>& statements = {}) {
    if (!db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return;
    for (const std::string& statement : statements) db.Query(statement);
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

// The player of team `team` (0 home, 1 away) whose database id is `databaseId`, or null.
Player* PlayerWithDatabaseId(Match& match, int team, int databaseId) {
  for (const auto& player : match.GetTeam(team)->GetAllPlayers())
    if (player->GetPlayerData() && player->GetPlayerData()->GetDatabaseID() == databaseId) return player.get();
  return nullptr;
}

// Every squad member of both sides reads the fallback. Returns how many were checked, so a test can see it was not vacuous.
int ExpectEveryPlayerTakesTheFallback(Match& match) {
  int checked = 0;
  for (int team = 0; team < 2; ++team)
    for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
      const PlayerData* data = player->GetPlayerData();
      if (!data) continue;
      ++checked;
      EXPECT_EQ(match.InjuryResistanceOf(data), Match::kFallbackInjuryResistance)
          << "database id " << data->GetDatabaseID();
      EXPECT_EQ(match.LeftFootedOf(data), Match::kFallbackLeftFooted) << "database id " << data->GetDatabaseID();
      EXPECT_EQ(match.FormOf(data), Match::kFallbackForm) << "database id " << data->GetDatabaseID();
      EXPECT_EQ(match.ConditionByteOf(data), Match::kFallbackCondition) << "database id " << data->GetDatabaseID();
    }
  return checked;
}

}  // namespace

// A fixture player is a synthetic squad member, not a PES player: a PES record under his database id is never read.
// The fixture's ids 1..32 are ids a stock install's Player.bin holds, so this is the hazard in practice.
TEST(PlayerDatabaseWiring, AFixturePlayerWhoseIdHasARecordStillTakesTheFallback) {
  std::vector<std::vector<uint8_t>> records;
  for (int id = 1; id <= kFixturePlayerCount; ++id) records.push_back(RecordFor(id, kTierA, true));
  std::string err;
  auto playerDb = pes::PlayerDatabase::Parse(Concat(records), &err);
  ASSERT_TRUE(playerDb.has_value()) << err;
  FixtureMatch fixture(&*playerDb);
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  EXPECT_GT(ExpectEveryPlayerTakesTheFallback(*fixture.match), 0);
}

// The same rule on the install's own Player.bin: a fixture player whose id is a record the install holds still takes the
// fallback. The record is chosen at run time, and only one whose traits differ from the fallback can tell the two apart.
TEST(PlayerDatabaseWiring, AFixturePlayerWhoseIdIsAnInstallRecordStillTakesTheFallback) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto blob = vfs.Read(pes::kPlayerDatabasePath, &err);
  ASSERT_TRUE(blob.has_value()) << err;
  auto playerDb = pes::PlayerDatabase::Parse(*blob, &err);
  ASSERT_TRUE(playerDb.has_value()) << err;
  // An id outside the fixture's 1..32, so the rename below cannot collide with a squad member's id.
  std::optional<int> recordId;
  for (size_t at = 0; at + pes::kPlayerRecordBytes <= blob->size() && !recordId; at += pes::kPlayerRecordBytes) {
    uint32_t id = 0;
    std::memcpy(&id, &(*blob)[at + pes::kPlayerIdOffset], 4);
    const auto traits = playerDb->Find(int(id));
    if (id > uint32_t(kFixturePlayerCount) && traits &&
        (traits->injuryResistance != Match::kFallbackInjuryResistance || traits->leftFooted != Match::kFallbackLeftFooted))
      recordId = int(id);
  }
  if (!recordId) GTEST_SKIP() << "no record of this install differs from the fallback, so it cannot tell them apart";

  FixtureMatch fixture(&*playerDb, {"UPDATE players SET id = " + std::to_string(*recordId) + " WHERE id = 1"});
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  Player* renamed = PlayerWithDatabaseId(*fixture.match, 0, *recordId);
  ASSERT_NE(renamed, nullptr) << "the renamed fixture player is not in the home squad";
  EXPECT_EQ(fixture.match->InjuryResistanceOf(renamed->GetPlayerData()), Match::kFallbackInjuryResistance);
  EXPECT_EQ(fixture.match->LeftFootedOf(renamed->GetPlayerData()), Match::kFallbackLeftFooted);
  EXPECT_EQ(fixture.match->FormOf(renamed->GetPlayerData()), Match::kFallbackForm);
  EXPECT_EQ(fixture.match->ConditionByteOf(renamed->GetPlayerData()), Match::kFallbackCondition);
}

// A player whose row carries a PES id reads the record of that id. The match keys the database by the PES id, so the
// synthetic player 2, who has a PES record under his own database id, still takes the fallback.
TEST(PlayerDatabaseWiring, AnImportedPlayerReadsTheRecordOfHisPesId) {
  constexpr int kPesIdOfImportedPlayer = 9001;  // synthetic: no fixture player carries it
  blunted::Database schema;
  ASSERT_TRUE(schema.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  const auto columns = schema.Query("select * from players limit 1");
  ASSERT_NE(std::find(columns->header.begin(), columns->header.end(), "pes_player_id"), columns->header.end())
      << "the fixture's players table has no pes_player_id column, so no player can be imported from PES";

  std::string err;
  auto playerDb = pes::PlayerDatabase::Parse(
      Concat({RecordFor(kPesIdOfImportedPlayer, kTierA, true, 6), RecordFor(2, kTierC, false, 1)}), &err);
  ASSERT_TRUE(playerDb.has_value()) << err;
  FixtureMatch fixture(&*playerDb, {"UPDATE players SET pes_player_id = " + std::to_string(kPesIdOfImportedPlayer) +
                                    " WHERE id = 1"});
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  Player* imported = PlayerWithDatabaseId(*fixture.match, 0, 1);
  Player* synthetic = PlayerWithDatabaseId(*fixture.match, 0, 2);
  ASSERT_NE(imported, nullptr);
  ASSERT_NE(synthetic, nullptr);
  EXPECT_EQ(fixture.match->InjuryResistanceOf(imported->GetPlayerData()), kTierA);
  EXPECT_TRUE(fixture.match->LeftFootedOf(imported->GetPlayerData()));
  EXPECT_EQ(fixture.match->FormOf(imported->GetPlayerData()), 6);
  EXPECT_EQ(fixture.match->ConditionByteOf(imported->GetPlayerData()), 0x46);
  EXPECT_EQ(fixture.match->InjuryResistanceOf(synthetic->GetPlayerData()), Match::kFallbackInjuryResistance);
  EXPECT_EQ(fixture.match->LeftFootedOf(synthetic->GetPlayerData()), Match::kFallbackLeftFooted);
  EXPECT_EQ(fixture.match->FormOf(synthetic->GetPlayerData()), Match::kFallbackForm);
  EXPECT_EQ(fixture.match->ConditionByteOf(synthetic->GetPlayerData()), Match::kFallbackCondition);
}

TEST(PlayerDatabaseWiring, WithoutADatabaseEveryPlayerTakesTheFallback) {
  FixtureMatch fixture(nullptr);
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  EXPECT_GT(ExpectEveryPlayerTakesTheFallback(*fixture.match), 0);
}
