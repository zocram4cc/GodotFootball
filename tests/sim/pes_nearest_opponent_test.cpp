// PES's rank-0 opponent as the MOVESTEP team seam reads it: the nearest active opposing player in
// the run world the team's controller built on its parity tick (pesoffball.hpp NearestOpponents).
// The jostle case drives ProcessMoveStep in RUN_LOOP through that seam.

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesmovestep.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace {

constexpr int kHomeTeam = 1, kAwayTeam = 2;
constexpr unsigned int kSeed = 1000;
constexpr float kFar = 200.0f;  // metres: beyond every threshold the seam reads

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
  // MOVESTEP runs only with pes_loop on (the PES tick order).
  Properties config;
  config.SetBool("pes_loop", true);
  return std::unique_ptr<Match>(new Match(new MatchData(kHomeTeam, kAwayTeam), setup, config));
}

#define REQUIRE_PES_MATCH()                                                              \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  blunted::Database* db = SharedDb();                                                    \
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";                                        \
  SetDB(db);                                                                             \
  std::unique_ptr<Match> match = MakeMatch();                                            \
  if (!match) GTEST_SKIP() << "PES2021.exe: the match constants are unavailable"

// Planar position (x, z) with the focus point (fx, fz): a player faces his focus. Height is 0.
void Place(Player* p, float x, float z, float fx, float fz) {
  p->ResetPosition(Vector3(x, z, 0.0f), Vector3(fx, fz, 0.0f));
}

// Every active player of a side that is not `keep` goes far away, so only the kept ones are near.
void SendAwayExcept(Team* team, const Player* keep) {
  std::vector<Player*> active;
  team->GetActivePlayers(active);
  for (size_t i = 0; i < active.size(); ++i) {
    if (active[i] == keep) continue;
    const float x = kFar + 10.0f * static_cast<float>(i);
    Place(active[i], x, 0.0f, x, 10.0f);
  }
}

}  // namespace

// Home's first player stands at the origin. A home teammate stands 1 m away, an away player 3 m
// away, and the rest of the away side 200 m or more out. Rank 0 of the home player is the away
// player at 3 m: the teammate is in his own block, and the far away players rank behind.
TEST(PesNearestOpponent, RankZeroIsTheNearestOpponentNotATeammate) {
  REQUIRE_PES_MATCH();
  Team* home = match->GetTeam(0);
  Team* away = match->GetTeam(1);
  Player* own = home->GetAllPlayers().at(0).get();
  Player* teammate = home->GetAllPlayers().at(1).get();
  Player* nearOpp = away->GetAllPlayers().at(0).get();
  Place(own, 0.0f, 0.0f, 0.0f, 10.0f);
  Place(teammate, 1.0f, 0.0f, 1.0f, 10.0f);
  Place(nearOpp, 3.0f, 0.0f, 3.0f, 10.0f);
  SendAwayExcept(away, nearOpp);

  match->StartPlay();
  match->Process();  // home's parity frame: the controller builds the run world

  const Player* opp = home->GetController()->NearestOpponentOf(own);
  ASSERT_NE(opp, nullptr) << "a run world with an opponent in it must answer";
  EXPECT_EQ(opp, nearOpp);
  EXPECT_EQ(home->GetController()->RunSlotOf(own), 0);
  EXPECT_EQ(home->GetController()->RunSlotOf(nearOpp), 11) << "away slots start at kSlots";
}

// An away starter has match id 22, outside the 0..21 slots the MOVESTEP gates read. Through the
// seam he must still get his run-world slot (11 + 6), and a home player 0.4 m off his flank must
// jostle him in RUN_LOOP. With the opponent far away the style stays the base one.
TEST(PesMoveStepJostle, AwayStarterJostlesTheNearOpponentInRunLoop) {
  REQUIRE_PES_MATCH();
  Team* home = match->GetTeam(0);
  Team* away = match->GetTeam(1);
  Player* own = away->GetAllPlayers().at(6).get();
  Player* near = home->GetAllPlayers().at(0).get();
  ASSERT_EQ(own->GetID(), 22) << "the away starter this test is about";
  SendAwayExcept(home, near);
  SendAwayExcept(away, own);
  match->StartPlay();
  // The controller reads the clock before the tick's advance, so the away side (frame parity 1) first
  // builds on the third logic frame. Run until it has, at most a few frames.
  for (int frame = 0; frame < 4 && away->GetController()->RunSlotOf(own) < 0; ++frame) match->Process();
  ASSERT_EQ(away->GetController()->RunSlotOf(own), 17) << "team 1, roster index 6";

  // Own faces +z; the opponent 0.57 m away at 45 degrees, facing the same way.
  const auto runAt = [&](float ox, float oz) {
    Place(own, 0.0f, 0.0f, 0.0f, 10.0f);
    Place(near, ox, oz, ox, oz + 10.0f);
    pes::MoveStepRecord& rec = own->GetMoveStep();
    rec.state = pes::MoveState::kRunLoop;
    rec.style = 0;
    pesmovestep::ProcessMoveStep(*own, *match);
    return rec.style;
  };
  const int jostled = runAt(0.4f, 0.4f);
  const int base = runAt(kFar, 0.0f);
  EXPECT_NE(jostled, base) << "the near opponent must change the RUN_LOOP style";
}
