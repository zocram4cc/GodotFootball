// PES's minimum-players rule and the 3-0 forfeit (docs/re/pes-re-missing-systems.md 2.7, 2.13).
// The rule's immediates come from PES2021.exe under PES21_ROOT; the install-backed tests skip
// without it. Matches are real: the synthetic fixture teams, driven in pes_loop mode.

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesclock.hpp"
#include "onthepitch/pesforfeit.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

namespace {

constexpr int HOME = 0, AWAY = 1;            // team indices: Match::GetTeam and MatchData
constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;  // fixture database ids
constexpr unsigned int SEED = 7;
// A card's effective time that never comes in a test, so the offender stays on the pitch.
constexpr unsigned long kNeverSentOff_ms = 1000000000000ul;
// Simulated time in which the first stoppage must arrive (a match's first whistle comes well inside it).
constexpr unsigned long kFirstStoppageWindow_ms = 10ul * 60ul * 1000ul;
constexpr int kMatchMinutes = 20;

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

// The install's forfeit immediates, or nullopt without an install.
std::optional<pes::ForfeitConstants> InstalledForfeitConstants() {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return std::nullopt;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) return std::nullopt;
  return pes::LoadForfeitConstants(*exe, &err);
}

// A real match from the fixture, every PES constant from the install. Null without one.
std::unique_ptr<Match> BuildMatch() {
  static std::unique_ptr<blunted::Database> held;  // the engine reads a process-wide database
  held = std::make_unique<blunted::Database>();
  if (!held->LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return nullptr;
  SetDB(held.get());
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = kMatchMinutes;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  if (!setup.rig) return nullptr;
  if (!pestest::FillMissingPesConstants(&setup)) return nullptr;
  Properties config;
  config.SetBool("pes_loop", true);
  return std::make_unique<Match>(new MatchData(HOME_TEAM, AWAY_TEAM), setup, config);
}

// Kicks the match off and runs it until play is live.
void PlayUntilLive(Match& match) {
  for (int i = 0; i < 100000 && !match.IsInPlay(); ++i) match.Process();
}

// Deactivates `team`'s outfield players (never the keeper, never `keep`) until `remaining` stay on the pitch.
void ShrinkTeamTo(Match& match, int team, int remaining, const Player* keep) {
  std::vector<Player*> onPitch;
  match.GetTeam(team)->GetActivePlayers(onPitch);
  int count = static_cast<int>(onPitch.size());
  for (Player* player : onPitch) {
    if (count <= remaining) break;
    if (player == keep || player->GetFormationEntry().role == e_PlayerRole_GK) continue;
    player->Deactivate();
    --count;
  }
}

Player* FirstOutfielder(Match& match, int team) {
  std::vector<Player*> onPitch;
  match.GetTeam(team)->GetActivePlayers(onPitch);
  for (Player* player : onPitch)
    if (player->GetFormationEntry().role != e_PlayerRole_GK) return player;
  return nullptr;
}

// Runs the match until it ends or the first-stoppage window closes.
void RunToFullTimeOrWindow(Match& match) {
  while (!match.IsGameOver() && match.GetSimTime_ms() < kFirstStoppageWindow_ms) match.Process();
}

}  // namespace

// --- the rule's arithmetic (no install) ---

TEST(PesForfeitRule, ASideIsShortOnlyBelowTheMinimum) {
  using pes::forfeit::BelowMinimumPlayers;
  using pes::forfeit::kPesMinimumPlayers;
  EXPECT_TRUE(BelowMinimumPlayers(kPesMinimumPlayers - 1, kPesMinimumPlayers));
  EXPECT_FALSE(BelowMinimumPlayers(kPesMinimumPlayers, kPesMinimumPlayers));
  EXPECT_FALSE(BelowMinimumPlayers(11, kPesMinimumPlayers));
}

// --- the forfeit's immediates and the per-player test (install) ---

TEST(PesForfeitInstall, TheOffenceIsTheExactSentOffStateWithNoInjuryMarker) {
  const auto k = InstalledForfeitConstants();
  if (!k) GTEST_SKIP() << "PES21_ROOT not set: the forfeit's values are PES's";
  // player+0x0c == 4 and player+0x18 == 0 (FUN_140A23260, 0x140A2330E and 0x140A23308).
  EXPECT_TRUE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState, 0));
  // A marker set on player+0x18 (an injury: 1 live, §2.12) clears the offence.
  EXPECT_FALSE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState, 1));
  // The test is exact: the states either side of 4 are not the offence.
  EXPECT_FALSE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState - 1, 0));
  EXPECT_FALSE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState + 1, 0));
}

TEST(PesForfeitInstall, ThePostMatchPeriodIsTheOneTheWhistleWrites) {
  const auto k = InstalledForfeitConstants();
  if (!k) GTEST_SKIP() << "PES21_ROOT not set: the forfeit's values are PES's";
  // The min-players writer (0x140447EF8) and the full-time writer agree on the post-match period.
  EXPECT_EQ(static_cast<int>(pes::PeriodAfterWhistle(3, false)), k->postMatchPeriod);
}

// --- the rule in a live match (install) ---

// A side one short of the minimum: the match ends at its next stoppage, in period 10, with play stopped.
TEST(PesForfeitMatch, ASideShortOfTheMinimumEndsTheMatchAtItsNextStoppage) {
  const auto k = InstalledForfeitConstants();
  auto match = BuildMatch();
  if (!k || !match) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  PlayUntilLive(*match);
  ShrinkTeamTo(*match, HOME, pes::forfeit::kPesMinimumPlayers - 1, nullptr);
  RunToFullTimeOrWindow(*match);
  ASSERT_TRUE(match->IsGameOver()) << "no stoppage ended the match";
  EXPECT_FALSE(match->IsInPlay()) << "the match ended in play, not at a stoppage";
  EXPECT_EQ(static_cast<int>(match->GetPesClock().Record().period), k->postMatchPeriod);
}

// Exactly the minimum keeps the match going: the rule is "fewer than", not "at most".
TEST(PesForfeitMatch, ASideAtTheMinimumKeepsPlaying) {
  auto match = BuildMatch();
  if (!match) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  PlayUntilLive(*match);
  ShrinkTeamTo(*match, HOME, pes::forfeit::kPesMinimumPlayers, nullptr);
  RunToFullTimeOrWindow(*match);
  EXPECT_FALSE(match->IsGameOver()) << "a side at the minimum was ended";
}

// The offender's yellow-then-red (state 4) and no injury marker: the side is written 0-3 and the
// scorers on the record are dropped.
TEST(PesForfeitMatch, TheOffendingSideLosesThreeNilAndItsScorersAreNotCounted) {
  const auto k = InstalledForfeitConstants();
  auto match = BuildMatch();
  if (!k || !match) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  PlayUntilLive(*match);
  Player* offender = FirstOutfielder(*match, HOME);
  ASSERT_TRUE(offender);
  // Yellow then red: cards 1 + 3 = 4. The effective time is far off, so he stays on the pitch.
  offender->GiveYellowCard(kNeverSentOff_ms);
  offender->GiveRedCard(kNeverSentOff_ms);
  ShrinkTeamTo(*match, HOME, pes::forfeit::kPesMinimumPlayers - 1, offender);
  // Two goals already on the record for the away side: the forfeit must not count them.
  MatchData* data = match->GetMatchData();
  data->AddEvent(MatchData::Event::Goal, AWAY, 1, "scorer");
  data->AddEvent(MatchData::Event::Goal, AWAY, 2, "scorer");
  data->SetGoalCount(AWAY, 2);
  RunToFullTimeOrWindow(*match);
  ASSERT_TRUE(match->IsGameOver()) << "no stoppage ended the match";
  EXPECT_TRUE(data->IsForfeited(HOME));
  EXPECT_FALSE(data->IsForfeited(AWAY));
  EXPECT_EQ(data->GetGoalCount(HOME), 0);
  EXPECT_EQ(data->GetGoalCount(AWAY), k->forfeitScore);
  for (const MatchData::Event& event : data->GetEvents()) {
    EXPECT_NE(event.kind, MatchData::Event::Goal);
    EXPECT_NE(event.kind, MatchData::Event::OwnGoal);
  }
}

// An injured player (a marker on player+0x18) with the sent-off state is not the offence.
TEST(PesForfeitMatch, AnInjuredSentOffPlayerDoesNotForfeitTheSide) {
  const auto k = InstalledForfeitConstants();
  auto match = BuildMatch();
  if (!k || !match) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  PlayUntilLive(*match);
  Player* offender = FirstOutfielder(*match, HOME);
  ASSERT_TRUE(offender);
  offender->GiveYellowCard(kNeverSentOff_ms);
  offender->GiveRedCard(kNeverSentOff_ms);
  offender->GetStamina().injuryLevel = 2;  // GF's knock: a non-zero injury marker
  ShrinkTeamTo(*match, HOME, pes::forfeit::kPesMinimumPlayers - 1, offender);
  RunToFullTimeOrWindow(*match);
  ASSERT_TRUE(match->IsGameOver()) << "no stoppage ended the match";
  EXPECT_FALSE(match->GetMatchData()->IsForfeited(HOME));
}
