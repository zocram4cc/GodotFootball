#include <gtest/gtest.h>

#include <cmath>
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
#include "onthepitch/pesworldrecord.hpp"
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

// PES's movement records hold the tick's start state through the whole tick: a tick's writer runs before the team
// stage, so every reader in it (the S1 rows, the mark table, the run world) sees the start positions. GF wrote the
// records on each run frame instead, after one side had already moved, so a tick where the second side built found
// the first side's records one move behind. RED on that per-build writer.
TEST(PesTickOrder, MovementRecordsHoldTheTicksStartPositions) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  for (int warm = 0; warm < 300; ++warm) match->Process();  // into play, where the players move every tick
  for (int tick = 0; tick < 4; ++tick) {
    std::vector<std::pair<int, Vector3>> start;  // (PES slot, position at the tick's start)
    for (int t = 0; t < 2; ++t) {
      std::vector<Player*> all;
      match->GetTeam(t)->GetAllPlayers(all);
      for (Player* p : all)
        if (p->IsActive()) start.emplace_back(match->PesSlotOf(p), p->GetPosition());
    }
    ASSERT_FALSE(start.empty());
    match->Process();
    for (const auto& [slot, pos] : start) {
      const uint8_t* man = match->PesWorldOf().ManRecord(slot);
      EXPECT_FLOAT_EQ(pes::LoadF32(man, pes::kManPosXOffset), pos.coords[0]) << "tick " << tick << " slot " << slot;
      EXPECT_FLOAT_EQ(pes::LoadF32(man, pes::kManPosZOffset), pos.coords[1]) << "tick " << tick << " slot " << slot;
    }
  }
}

// A restart's placement is a teleport the referee stage makes (PrepareSetPiece's ResetPosition on the set-piece men),
// after the tick's writer has run at the head of the tick. The readers of the team stage (the S1 rows, the mark table,
// the run world) must see the placed men where they stand, so a placed man's record must match his position once the
// tick is done, up to one tick's movement. RED while the writer runs before the referee: the record keeps the
// pre-placement spot.
TEST(PesTickOrder, RestartPlacementsReachTheRecordsTheSameTick) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  constexpr int kMaxTicks = 6000;         // the first restart placement of this seed falls well inside this
  constexpr size_t kPlacedMen = 5;        // a placement moves a side's men at once, not one man (a warp moves one)
  constexpr float kPlacedJump_m = 3.0f;   // a tick of play moves a sprinter 0.17 m, so 3 m in one tick is a placement
  constexpr float kMaxTickMove_m = 0.5f;  // the record may trail a man by one tick of movement, no more
  for (int tick = 0; tick < kMaxTicks; ++tick) {
    std::vector<std::pair<int, Player*>> men;  // (PES slot, active player)
    std::vector<Vector3> before;
    for (int t = 0; t < 2; ++t) {
      std::vector<Player*> all;
      match->GetTeam(t)->GetAllPlayers(all);
      for (Player* p : all)
        if (p->IsActive()) {
          men.emplace_back(match->PesSlotOf(p), p);
          before.push_back(p->GetPosition());
        }
    }
    match->Process();
    std::vector<size_t> placed;
    for (size_t i = 0; i < men.size(); ++i)
      if ((men[i].second->GetPosition() - before[i]).GetLength() >= kPlacedJump_m) placed.push_back(i);
    if (placed.size() < kPlacedMen) continue;
    for (size_t i : placed) {
      const Vector3 now = men[i].second->GetPosition();
      const uint8_t* man = match->PesWorldOf().ManRecord(men[i].first);
      const float dx = pes::LoadF32(man, pes::kManPosXOffset) - now.coords[0];
      const float dz = pes::LoadF32(man, pes::kManPosZOffset) - now.coords[1];
      EXPECT_LT(std::sqrt(dx * dx + dz * dz), kMaxTickMove_m) << "tick " << tick << " slot " << men[i].first;
    }
    return;  // the first placement is the one under test
  }
  FAIL() << "no restart placement within " << kMaxTicks << " ticks: the scenario did not run";
}
