// PES 2021's CPU substitutions on a real match: the swap itself, through Team::Substitute, the
// request path a stoppage takes, and the allowance. Matches are real: the fixture teams, every PES
// constant from the install (PES21_ROOT); without an install the tests skip.
#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pessubs.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/substitutions.hpp"
#include "onthepitch/team.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

namespace {

constexpr int HOME = 0;                      // team index: Match::GetTeam
constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;  // fixture database ids
constexpr unsigned int SEED = 7;
constexpr int kMatchMinutes = 20;
// Logic frames the new man plays on after the swap (a few seconds of play).
constexpr int kFramesAfterSwap = 600;

bool HaveInstall() { return std::getenv("PES21_ROOT") != nullptr; }

const pesrig::Rig* SharedRig() {
  static std::optional<pesrig::Rig> rig;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      pes::PesVfs vfs;
      std::string err;
      if (vfs.Mount(root, &err)) rig = pesrig::BuildRig(vfs, &err);
    }
  }
  return rig ? &*rig : nullptr;
}

std::unique_ptr<Match> BuildMatch(int minutes) {
  static std::unique_ptr<blunted::Database> held;  // the engine reads a process-wide database
  held = std::make_unique<blunted::Database>();
  if (!held->LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return nullptr;
  SetDB(held.get());
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = minutes;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  if (!setup.rig) return nullptr;
  if (!pestest::FillMissingPesConstants(&setup)) return nullptr;
  Properties config;
  config.SetBool("pes_loop", true);
  return std::make_unique<Match>(new MatchData(HOME_TEAM, AWAY_TEAM), setup, config);
}

// Kicks the match off and runs it until play is live with no set piece in progress.
void PlayUntilLive(Match& match) {
  for (int i = 0; i < 100000 && !(match.IsInPlay() && !match.IsInSetPiece()); ++i) match.Process();
}

// Runs until play stops (a stoppage), so the substitution window is open.
bool RunToStoppage(Match& match) {
  for (int i = 0; i < 100000 && match.IsInPlay() && !match.IsGameOver(); ++i) match.Process();
  return !match.IsInPlay() && match.IsSubstitutionWindow();
}

// An outfield player on the pitch, and a bench player who has not yet played.
void PickSwap(Match& match, Player** out, Player** in) {
  std::vector<Player*> onPitch, bench;
  match.GetTeam(HOME)->GetActivePlayers(onPitch);
  match.GetTeam(HOME)->GetBenchPlayers(bench);
  *out = nullptr;
  for (Player* player : onPitch)
    if (player->GetFormationEntry().role != e_PlayerRole_GK) *out = player;
  *in = bench.empty() ? nullptr : bench.front();
}

}  // namespace

TEST(SimSubstitution, ASwapPutsTheBenchPlayerOnThePitchAndHeStaysThere) {
  if (!HaveInstall()) GTEST_SKIP() << "needs PES21_ROOT";
  auto match = BuildMatch(kMatchMinutes);
  ASSERT_TRUE(match) << "fixture match";
  PlayUntilLive(*match);
  Player* out = nullptr;
  Player* in = nullptr;
  PickSwap(*match, &out, &in);
  ASSERT_NE(out, nullptr);
  ASSERT_NE(in, nullptr);

  ASSERT_TRUE(match->GetTeam(HOME)->Substitute(out, in));
  EXPECT_TRUE(in->IsActive());
  EXPECT_FALSE(out->IsActive());
  for (int i = 0; i < kFramesAfterSwap; ++i) match->Process();
  EXPECT_TRUE(in->IsActive());
}

// The request path a stoppage takes: accepted at the stoppage, executed by the tick that follows.
TEST(SimSubstitution, ARequestMadeAtAStoppageIsExecutedByTheNextTick) {
  if (!HaveInstall()) GTEST_SKIP() << "needs PES21_ROOT";
  auto match = BuildMatch(kMatchMinutes);
  ASSERT_TRUE(match) << "fixture match";
  PlayUntilLive(*match);
  ASSERT_TRUE(RunToStoppage(*match)) << "play must stop in a started match";
  Player* out = nullptr;
  Player* in = nullptr;
  PickSwap(*match, &out, &in);
  ASSERT_NE(out, nullptr);
  ASSERT_NE(in, nullptr);

  ASSERT_EQ(match->RequestSubstitution(HOME, out, in), Substitutions::e_Result_Accepted);
  match->Process();
  EXPECT_TRUE(in->IsActive());
  EXPECT_FALSE(out->IsActive());
  for (int i = 0; i < kFramesAfterSwap; ++i) match->Process();
  EXPECT_TRUE(in->IsActive());
}

// The allowance is PES's: the menu's default stored to settings+0x8d, read from the install. A
// match starts with the whole of it for each side.
TEST(SimSubstitution, TheAllowanceIsTheInstallsSubstitutionLimit) {
  if (!HaveInstall()) GTEST_SKIP() << "needs PES21_ROOT";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  const auto k = pes::LoadSubstitutionConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  auto match = BuildMatch(kMatchMinutes);
  ASSERT_TRUE(match) << "fixture match";
  EXPECT_EQ(Substitutions::GetRemaining(match->GetSubstitutionState(), HOME), k->substitutionLimit);
}

// A request that would take the side past its allowance is refused when it is made, because the
// requests already accepted for the coming tick count against it (PES's queue count).
TEST(SubstitutionsAllowance, AQueuedRequestCountsAgainstTheAllowance) {
  Substitutions::State state;
  state.limit = 2;
  const Substitutions::SquadView swap{true, false, true, false};
  EXPECT_EQ(Substitutions::Validate(state, 0, swap, true, 0), Substitutions::e_Result_Accepted);
  EXPECT_EQ(Substitutions::Validate(state, 0, swap, true, 1), Substitutions::e_Result_Accepted);
  EXPECT_EQ(Substitutions::Validate(state, 0, swap, true, 2),
            Substitutions::e_Result_NoSubstitutionsLeft);
}

TEST(SubstitutionsAllowance, ACommittedSwapSpendsOnlyItsOwnTeamsAllowance) {
  Substitutions::State state;
  state.limit = 2;
  Substitutions::Commit(state, 0);
  EXPECT_EQ(Substitutions::GetRemaining(state, 0), 1);
  EXPECT_EQ(Substitutions::GetRemaining(state, 1), 2);
}
