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
#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/player/controller/elizacontroller.hpp"
#include "pes/playerdb.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 1000;
constexpr int MATCH_MINUTES = 5;
// Writes `value` into `width` bits from absolute bit `bit` of a record, LSB-first.
void SetField(std::vector<uint8_t>& rec, size_t bit, size_t width, uint32_t value) {
  for (size_t i = 0; i < width; ++i) {
    const uint8_t mask = uint8_t(1u << ((bit + i) % 8));
    if ((value >> i) & 1u) rec[(bit + i) / 8] |= mask;
    else rec[(bit + i) / 8] &= uint8_t(~mask);
  }
}

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
// squads' database before the match is built, for a test's own rows. `pesLoop` picks the PES loop or the switch-off path.
struct FixtureMatch {
  blunted::Database db;
  std::unique_ptr<Match> match;

  explicit FixtureMatch(const pes::PlayerDatabase* playerDb, const std::vector<std::string>& statements = {},
                        bool pesLoop = false) {
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
    Properties config;
    config.SetBool("pes_loop", pesLoop);
    match.reset(new Match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, config));
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

// The slot record's base bytes (PES's roster row, WriteSlotAbilities): a player with a PES id starts from his database record's
// Speed, Jump, Height and stronger foot; a player without one starts from GF's own stat and height and the right foot. Player.bin
// layout offsets pinned as literals (pes_playerdb_test AbilityOffsetsEqualTheExesAccessors checks them against the exe).
TEST(PlayerDatabaseWiring, SlotAbilityBasesComeFromThePesRecordOfAnImportedPlayerAndGfStatsOtherwise) {
  constexpr int kPesId = 9001;  // synthetic: no fixture player carries it
  constexpr uint8_t kRecordSpeed = 71, kRecordJump = 83, kRecordHeightCm = 197;
  std::vector<uint8_t> rec = RecordFor(kPesId, kTierA, true);
  SetField(rec, 306, 6, kRecordSpeed - 40);
  SetField(rec, 294, 6, kRecordJump - 40);
  SetField(rec, 216, 8, kRecordHeightCm - 100);
  std::string err;
  auto playerDb = pes::PlayerDatabase::Parse(rec, &err);
  ASSERT_TRUE(playerDb.has_value()) << err;
  FixtureMatch fixture(&*playerDb, {"UPDATE players SET pes_player_id = " + std::to_string(kPesId) + " WHERE id = 1"});
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  Player* imported = PlayerWithDatabaseId(*fixture.match, 0, 1);
  Player* synthetic = PlayerWithDatabaseId(*fixture.match, 0, 2);
  ASSERT_NE(imported, nullptr);
  ASSERT_NE(synthetic, nullptr);

  pes::PesWorld world;
  const auto stored = [&](Player* p, uint32_t id) {
    const int slot = fixture.match->PesSlotOf(p);
    WriteSlotAbilities(*fixture.match, world, slot, *p);
    return world.Block(slot)[pes::kSlotRecordOffset + pes::kSlotAbilityOffset + id];
  };
  EXPECT_EQ(stored(imported, pes::kAbilityHeightId), kRecordHeightCm);
  EXPECT_EQ(stored(imported, pes::kAbilityStrongerFootId), 1);
  // Speed and Jump leave through the store step's cells (condition, familiarity, fatigue): the record's byte is the base, so a
  // neutral-cell player stores it unchanged and no cell can raise it above the clamp table's ceiling.
  EXPECT_NEAR(stored(imported, pes::kAbilitySpeedId), kRecordSpeed, 8);
  EXPECT_NEAR(stored(imported, pes::kAbilityJumpId), kRecordJump, 8);

  PlayerData* data = synthetic->GetPlayerData();
  EXPECT_EQ(stored(synthetic, pes::kAbilityHeightId), static_cast<uint8_t>(std::clamp(data->GetHeight() * 100.0f, 0.0f, 255.0f)));
  EXPECT_NE(stored(synthetic, pes::kAbilityHeightId), kRecordHeightCm) << "the synthetic player must not read the record";
  EXPECT_EQ(stored(synthetic, pes::kAbilityStrongerFootId), Match::kFallbackLeftFooted ? 1 : 0);
  const float gfSpeedByte = data->GetStat("physical_velocity") * pes::kAbilityBytesPerStat;
  EXPECT_NEAR(stored(synthetic, pes::kAbilitySpeedId), gfSpeedByte, 8);
}

// PES's copy of the roster row into the AI record (FUN_140922BC0 -> FUN_140A58F00): every one of the row's 0x50 bytes lands in the
// slot's ability block, the bytes beside it do not, and a player without a row (the synthetic one) gets none of them.
TEST(PlayerDatabaseWiring, ARowOfTheSaveIsCopiedWholeIntoTheSlotsAbilityBlock) {
  constexpr int kPesId = 9002;
  std::string err;
  auto playerDb = pes::PlayerDatabase::Parse(RecordFor(kPesId, kTierA, false), &err);
  ASSERT_TRUE(playerDb.has_value()) << err;
  pes::EditSave save;
  pes::EditPlayerRecord rec{};
  const uint32_t id = kPesId;
  std::memcpy(rec.data() + pes::kEditIdOffset, &id, sizeof id);
  const auto put = [&](size_t bit, size_t width, uint32_t value) {
    for (size_t i = 0; i < width; ++i) {
      const size_t b = bit + i;
      rec[b / 8] = uint8_t((rec[b / 8] & ~(1u << (b % 8))) | (((value >> i) & 1u) << (b % 8)));
    }
  };
  for (const pes::EditRowField& f : pes::kEditRowFields) {
    const uint32_t lo = std::max(f.gateMin, f.clampMin), hi = std::min(f.gateMax, f.clampMax);
    put(f.bit, f.width, lo + (f.row * 7u + 3u) % (hi - lo + 1));
  }
  put(pes::kEditStyleBit, pes::kEditStyleWidth, 12);
  put(pes::kEditFlagFirstBit + 4, 1, 1);
  put(pes::kEditSkillFirstBit + 9, 1, 1);
  save.players.push_back(rec);
  playerDb->ApplyEditSave(save);
  const pes::EditRow decoded = pes::DecodeEditRow(rec);

  FixtureMatch fixture(&*playerDb, {"UPDATE players SET pes_player_id = " + std::to_string(kPesId) + " WHERE id = 1"});
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
  Player* imported = PlayerWithDatabaseId(*fixture.match, 0, 1);
  Player* synthetic = PlayerWithDatabaseId(*fixture.match, 0, 2);
  ASSERT_NE(imported, nullptr);
  ASSERT_NE(synthetic, nullptr);
  pes::PesWorld world;
  const int slot = fixture.match->PesSlotOf(imported), otherSlot = fixture.match->PesSlotOf(synthetic);
  WriteSlotAbilities(*fixture.match, world, slot, *imported);
  const uint8_t* block = world.Block(slot) + pes::kSlotRecordOffset + pes::kSlotAbilityOffset;
  const uint8_t* untouched = world.Block(otherSlot) + pes::kSlotRecordOffset + pes::kSlotAbilityOffset;
  for (size_t i = 0; i < pes::kRosterRowBytes; ++i) {
    // Speed and Jump leave through the store step's cells; every other byte, Height and the foot included, is the row's.
    if (i == pes::kAbilitySpeedId || i == pes::kAbilityJumpId) continue;
    EXPECT_EQ(block[i], decoded.image[i]) << "row byte 0x" << std::hex << i;
  }
  EXPECT_NE(decoded.styleMask, 0u);
  EXPECT_EQ(block[pes::kRowStyleMask], uint8_t(decoded.styleMask)) << "the style word is in the block";
  EXPECT_EQ(block[pes::kRosterRowBytes], untouched[pes::kRosterRowBytes]) << "the byte past the row is not the copy's";
  for (size_t i = 0; i < pes::kRosterRowBytes; ++i)
    EXPECT_EQ(untouched[i], 0) << "an untouched slot starts with an empty block: byte 0x" << std::hex << i;
}

// The run-depth term of the formation spot (the site the DribbleRow doc left on GF's style enum) is PES's rule on the style
// WORD (FUN_1405D8B70 L650-657: FUN_140A64030 per bit, style word AND the class's compat row). The word is the save row's
// +0x48 for a player the save holds and the word of his database style for one it does not (Match::StyleWordOf).
namespace {
constexpr int kForwardDatabaseId = 10;  // the fixture's home CF (teams.sql)
constexpr int kForwardPesId = 9003;     // synthetic: no fixture player carries it
constexpr int kGoalPoacherBit = 0, kFoxInTheBoxBit = 2, kHolePlayerBit = 9;  // engine bits (pesstyle.cpp EngineBit)

void PutBits(pes::EditPlayerRecord& rec, size_t bit, size_t width, uint32_t value) {
  for (size_t i = 0; i < width; ++i) {
    const size_t b = bit + i;
    rec[b / 8] = uint8_t((rec[b / 8] & ~(1u << (b % 8))) | (((value >> i) & 1u) << (b % 8)));
  }
}

pes::EditPlayerRecord SaveRecordWithStyle(uint32_t saveStyle) {
  pes::EditPlayerRecord rec{};
  const uint32_t id = kForwardPesId;
  std::memcpy(rec.data() + pes::kEditIdOffset, &id, sizeof id);
  PutBits(rec, pes::kEditStyleBit, pes::kEditStyleWidth, saveStyle);
  return rec;
}

// The save style value whose decoded style word is `word`. The word comes from the save's own decode, not a table here.
uint32_t SaveStyleOfWord(uint32_t word) {
  for (uint32_t value = 0; value < (1u << pes::kEditStyleWidth); ++value)
    if (pes::DecodeEditRow(SaveRecordWithStyle(value)).styleMask == word) return value;
  ADD_FAILURE() << "no save style decodes to word 0x" << std::hex << word;
  return 0;
}

struct ForwardSpot {
  float x = 0.0f;
  float towardOpponentGoal = 0.0f;  // +1 or -1: the sign of the attack along x
  float runDepth_m = 0.0f;          // the install's run-depth term
  bool gateOpen = false;            // the forward's compat row lists the style under test
};

// The home forward's formation spot with the save style value `saveStyle` (nullopt: Player.bin only, no row) and `gfStyle` as
// his database style. nullopt when PES21_ROOT is not set.
std::optional<ForwardSpot> SpotOfForward(bool pesLoop, std::optional<uint32_t> saveStyle, PlayingStyles::Player gfStyle,
                                         int bitUnderTest) {
  std::string err;
  auto playerDb = pes::PlayerDatabase::Parse(RecordFor(kForwardPesId, kTierC, false), &err);
  EXPECT_TRUE(playerDb.has_value()) << err;
  if (!playerDb) return std::nullopt;
  if (saveStyle) {
    pes::EditSave save;
    save.players.push_back(SaveRecordWithStyle(*saveStyle));
    playerDb->ApplyEditSave(save);
  }
  FixtureMatch fixture(&*playerDb,
                       {"UPDATE players SET pes_player_id = " + std::to_string(kForwardPesId) +
                        ", profile_xml = profile_xml || '<playing_style>" + PlayingStyles::Serialize(gfStyle) +
                        "</playing_style>' WHERE id = " + std::to_string(kForwardDatabaseId)},
                       pesLoop);
  if (!fixture.match) return std::nullopt;
  Player* forward = PlayerWithDatabaseId(*fixture.match, 0, kForwardDatabaseId);
  EXPECT_NE(forward, nullptr);
  if (!forward) return std::nullopt;
  EXPECT_EQ(forward->GetPlayerData()->GetPlayingStyle(), gfStyle) << "the database style reached the player";
  Team* team = fixture.match->GetTeam(0);
  TeamAIController* controller = team->GetController();
  ForwardSpot spot;
  // The controller's possession and tactic members are unset until its first tick: one step, as the set-piece tests do.
  fixture.match->Process();
  spot.x = controller->GetAdaptedFormationPosition(forward, true).coords[0];
  spot.towardOpponentGoal = float(-team->GetSide());
  spot.runDepth_m = fixture.match->GetPesOffBallConstants().styleRunDepth_m;
  spot.gateOpen = pes::style::Gate(fixture.match->GetPesOffBallConstants().styleCompat,
                                   controller->RoleClassOfSlot(team->FormationSlotOf(forward)),
                                   fixture.match->StyleWordOf(forward->GetPlayerData()), bitUnderTest);
  return spot;
}

// Two save rows whose style words differ (Goal Poacher bit 0 against Hole Player bit 9, which PES's run-depth term does not
// list): the forward's spot moves by the install's depth toward the opponent's goal and by nothing for the other.
// The database style is the same in both, so only the word can have moved him. Both loop paths.
TEST(StyleRunDepthSite, TwoSavedStyleWordsThatDifferGiveTheForwardTheTermOrNot) {
  for (const bool pesLoop : {false, true}) {
    SCOPED_TRACE(pesLoop ? "pes loop" : "switch-off");
    const auto poacher = SpotOfForward(pesLoop, SaveStyleOfWord(1u << kGoalPoacherBit), PlayingStyles::Player::None,
                                       kGoalPoacherBit);
    const auto hole = SpotOfForward(pesLoop, SaveStyleOfWord(1u << kHolePlayerBit), PlayingStyles::Player::None,
                                    kGoalPoacherBit);
    if (!poacher || !hole) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
    ASSERT_TRUE(poacher->gateOpen) << "the install's compat row for the forward must list Goal Poacher";
    EXPECT_FALSE(hole->gateOpen);
    ASSERT_GT(poacher->runDepth_m, 0.0f);
    EXPECT_FLOAT_EQ((poacher->x - hole->x) * poacher->towardOpponentGoal, poacher->runDepth_m);
  }
}

// Fox in the Box (bit 2) takes the same term as the Poacher: a second word, a second bit, the same shift.
TEST(StyleRunDepthSite, FoxInTheBoxWordTakesTheSameTermAsThePoacher) {
  for (const bool pesLoop : {false, true}) {
    SCOPED_TRACE(pesLoop ? "pes loop" : "switch-off");
    const auto fox = SpotOfForward(pesLoop, SaveStyleOfWord(1u << kFoxInTheBoxBit), PlayingStyles::Player::None,
                                   kFoxInTheBoxBit);
    const auto none = SpotOfForward(pesLoop, SaveStyleOfWord(0u), PlayingStyles::Player::None, kFoxInTheBoxBit);
    if (!fox || !none) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
    ASSERT_TRUE(fox->gateOpen);
    EXPECT_FLOAT_EQ((fox->x - none->x) * fox->towardOpponentGoal, fox->runDepth_m);
  }
}

// GF's style enum no longer moves the spot: a forward whose database style is Box to Box (GF's old +/-4 m) and whose saved
// word is none stands where a forward with no style at all stands.
TEST(StyleRunDepthSite, TheDatabaseStyleEnumNoLongerMovesTheSpotWhenTheSavedWordIsNone) {
  for (const bool pesLoop : {false, true}) {
    SCOPED_TRACE(pesLoop ? "pes loop" : "switch-off");
    const auto boxToBox = SpotOfForward(pesLoop, SaveStyleOfWord(0u), PlayingStyles::Player::BoxToBox, kGoalPoacherBit);
    const auto none = SpotOfForward(pesLoop, SaveStyleOfWord(0u), PlayingStyles::Player::None, kGoalPoacherBit);
    if (!boxToBox || !none) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
    EXPECT_FLOAT_EQ(boxToBox->x, none->x);
  }
}

// A player the save does not hold (Player.bin only) has no row: PES's word for him is not decoded, so his database style's
// word stands in (Match::StyleWordOf). A Fox in the Box by database style still gets the term, not GF's old 0 or 3 m.
TEST(StyleRunDepthSite, APlayerWithoutARowTakesTheTermFromHisDatabaseStylesWord) {
  for (const bool pesLoop : {false, true}) {
    SCOPED_TRACE(pesLoop ? "pes loop" : "switch-off");
    const auto fox = SpotOfForward(pesLoop, std::nullopt, PlayingStyles::Player::FoxInTheBox, kFoxInTheBoxBit);
    const auto none = SpotOfForward(pesLoop, std::nullopt, PlayingStyles::Player::None, kFoxInTheBoxBit);
    if (!fox || !none) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton and constants come from PES";
    ASSERT_TRUE(fox->gateOpen);
    EXPECT_FLOAT_EQ((fox->x - none->x) * fox->towardOpponentGoal, fox->runDepth_m);
  }
}
}  // namespace
