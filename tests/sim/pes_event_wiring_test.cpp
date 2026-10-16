#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesevents.hpp"
#include "onthepitch/team.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

// A2 wiring: the match owns PES's event store, the event stage writes it each
// tick, and the consumers read it. These tests drive a real Match built from the
// user's install (skipped without PES21_ROOT).
namespace {

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

std::unique_ptr<Match> MakeMatch() {
  MatchSetup setup;
  setup.homeTeamDbId = kHomeTeam;
  setup.awayTeamDbId = kAwayTeam;
  setup.seed = kSeed;
  setup.durationMinutes = 5;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  if (!pestest::FillMissingPesConstants(&setup)) return nullptr;
  return std::unique_ptr<Match>(
      new Match(new MatchData(kHomeTeam, kAwayTeam), setup, Properties()));
}

#define REQUIRE_PES_MATCH()                                                              \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  blunted::Database* db = SharedDb();                                                    \
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";                                        \
  SetDB(db);                                                                             \
  std::unique_ptr<Match> match = MakeMatch();                                             \
  if (!match) GTEST_SKIP() << "PES2021.exe: the match constants are unavailable"

}  // namespace

TEST(PesEventWiring, PlayerKeysRoundTripThroughTheSquad) {
  REQUIRE_PES_MATCH();
  Player* p = match->GetTeam(0)->GetAllPlayers().front().get();
  const pes::ActorKey key = match->PesActorKeyOf(p);
  EXPECT_EQ(key.team, static_cast<uint32_t>(p->GetTeamID()));
  EXPECT_EQ(key.slot, 0u);
  EXPECT_EQ(match->GetPesActorPlayer(key), p);
  EXPECT_EQ(match->GetPesActorPlayer(pes::NoActorKey()), nullptr);
}

// The GF touch reaches the store on the next event stage: a short pass by a
// player is ring-0 record of action kind 1, keyed to him, at the touch's frame.
TEST(PesEventWiring, AQueuedTouchIsRoutedIntoTheStoreByTheNextEventStage) {
  REQUIRE_PES_MATCH();
  Player* passer = match->GetTeam(0)->GetAllPlayers().front().get();
  const int frame = match->GetPesFrame();
  match->QueuePesTouch(passer, 1);  // PES action kind 1: a short pass
  match->Process();
  const pes::NewestEventView newest = pes::NewestEvent(match->GetPesEvents());
  ASSERT_TRUE(newest.present);
  EXPECT_EQ(newest.kind, 1u);
  EXPECT_EQ(newest.actor, match->PesActorKeyOf(passer));
  EXPECT_EQ(newest.frame, frame);
}

// The possession latch's step 2 reads the store: a pass by a man of team 1 takes
// the latch for team 1 and leaves team 0's clear, so the decision follows the
// newest event's side.
TEST(PesEventWiring, ThePossessionLatchFollowsTheNewestEventsSide) {
  REQUIRE_PES_MATCH();
  // The kick-off restart is armed: PES's rule R (restart state 3) decides the latch before
  // step 2, so the test takes the match out of the restart to reach step 2.
  match->GetReferee()->Buffer().active = false;
  Player* passer = match->GetTeam(1)->GetAllPlayers().front().get();
  match->QueuePesTouch(passer, 1);
  match->Process();
  EXPECT_TRUE(match->GetTeam(1)->IsPesPossession());
  EXPECT_FALSE(match->GetTeam(0)->IsPesPossession());
}

// A latch flip is PES's kind 12 record. The team tick takes the latch on the
// first pass; the stage of the next tick sees the change and records it (kind 12
// has no ring of its own in the install's routing table, so it lands in the
// overflow ring).
TEST(PesEventWiring, ALatchFlipIsRecordedAsKindTwelve) {
  REQUIRE_PES_MATCH();
  Player* passer = match->GetTeam(0)->GetAllPlayers().front().get();
  match->QueuePesTouch(passer, 1);
  match->Process();
  match->Process();
  const pes::EventRing10& overflow = match->GetPesEvents().OverflowRing();
  bool found = false;
  for (const pes::EventRecord& rec : overflow.slots) found = found || rec.kind == 12u;
  EXPECT_TRUE(found);
}
