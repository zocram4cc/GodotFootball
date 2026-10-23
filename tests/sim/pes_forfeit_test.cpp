// PES's minimum-players rule and the 3-0 forfeit (docs/re/pes-re-missing-systems.md 2.7, 2.13).
// The rule's immediates come from PES2021.exe under PES21_ROOT; without an install the install tests
// skip, and with one a missing constant fails. Matches are real: the synthetic fixture teams, pes_loop.
//
// No test assigns a card or injury field by hand. Cards go through Player::GiveRedCard /
// GiveYellowCard, the same calls the referee makes when it issues a card; the removal is GF's own
// SendOff. Injuries go through pes::RecordContact, the contact path. The referee's foul decision and
// its card book are not reachable from outside the referee (both protected), so the card's issue
// step is the entry point; the foul that led to it is not driven. Scores and scorers in the forfeit
// test are data, set through MatchData, because a goal cannot be scored on demand.

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
#include "onthepitch/pesrandom.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/substitutions.hpp"
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
constexpr int kMatchMinutes = 20;
constexpr int kShortMatchMinutes = 2;  // reaches full time quickly in simulated time
// Simulated time in which a stoppage must arrive or a match must end.
constexpr unsigned long kWindow_ms = 10ul * 60ul * 1000ul;
// A card whose removal never takes effect within the match (the reachable window at full time).
constexpr unsigned long kNeverSentOff_ms = 1000000000000ul;
// One PES frame-second is 54 logic frames: a stoppage must stay open this long for the substitution
// decision (once a frame-second) to have run during it.
constexpr int kStoppageSampleTicks = 64;
// Contacts that take a player to PES's injured level (the contact path, pes_stamina_test's slide).
constexpr int kInjuringContacts = 200;

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

// The install's forfeit immediates (the caller has checked that an install is present).
std::optional<pes::ForfeitConstants> InstalledForfeitConstants(std::string* error) {
  std::string err;
  auto exe = pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", &err);
  if (!exe) {
    *error = err;
    return std::nullopt;
  }
  return pes::LoadForfeitConstants(*exe, error);
}

// A real match from the fixture, every PES constant from the install. Null when one is missing.
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

// Runs until the substitution window has been open for a whole frame-second in a row: the
// substitution decision has then run during a stoppage. False if the match ends first.
bool RunThroughAStoppage(Match& match) {
  int open = 0;
  while (!match.IsGameOver() && match.GetSimTime_ms() < kWindow_ms) {
    match.Process();
    open = match.IsSubstitutionWindow() ? open + 1 : 0;
    if (open >= kStoppageSampleTicks) return true;
  }
  return false;
}

// Runs until the match ends or the simulated-time window closes.
void RunToFullTimeOrWindow(Match& match) {
  while (!match.IsGameOver() && match.GetSimTime_ms() < kWindow_ms) match.Process();
}

// Outfield players of `team` on the pitch, in squad order.
std::vector<Player*> OutfieldOnPitch(Match& match, int team) {
  std::vector<Player*> onPitch, outfield;
  match.GetTeam(team)->GetActivePlayers(onPitch);
  for (Player* player : onPitch)
    if (player->GetFormationEntry().role != e_PlayerRole_GK) outfield.push_back(player);
  return outfield;
}

int ActiveCount(Match& match, int team) {
  std::vector<Player*> onPitch;
  match.GetTeam(team)->GetActivePlayers(onPitch);
  return static_cast<int>(onPitch.size());
}

// A red card through GF's card issue (Player::GiveRedCard, as the referee calls it). GF's SendOff
// removes the player at the card's effective time, on the next tick.
void ShowRed(Match& match, Player* player) {
  player->GiveRedCard(match.GetActualTime_ms() + 1);
}

// A contact on `player` that takes him to the injured level, through the contact path
// (pes::RecordContact, PES's report 6.2/6.3). Returns true once he is injured.
bool InjureByContact(Match& match, Player* player) {
  const std::optional<pes::RngConstants>& rngConstants = pestest::SharedPesMatchConstants().rng;
  if (!rngConstants) return false;
  pes::Random rng(*rngConstants);
  const pes::StaminaConstants& k = match.GetPesStaminaConstants();
  for (int i = 0; i < kInjuringContacts && player->GetStamina().injuryLevel < 3; ++i)
    pes::RecordContact(k, &player->GetStamina(), 2, true, 25.0f, 25.0f, true, false, 180.0f, &rng);
  return player->GetStamina().injuryLevel >= 3;
}

// Requests `count` substitutions for `team` through the real request path, each taking an on-pitch
// outfielder off for a bench player. Returns how many were accepted.
int RequestSubstitutions(Match& match, int team, int count) {
  std::vector<Player*> onPitch = OutfieldOnPitch(match, team);
  std::vector<Player*> bench;
  for (const auto& player : match.GetTeam(team)->GetAllPlayers())
    if (!player->IsActive() && !match.GetTeam(team)->HasBeenSubstituted(player->GetID()))
      bench.push_back(player.get());
  int accepted = 0;
  for (int i = 0; i < count && i < static_cast<int>(onPitch.size()) && i < static_cast<int>(bench.size());
       ++i) {
    if (match.RequestSubstitution(team, onPitch[i], bench[i]) == Substitutions::e_Result_Accepted)
      ++accepted;
  }
  return accepted;
}

int SubstitutionsLeft(Match& match, int team) {
  return Substitutions::GetRemaining(match.GetSubstitutionState(), team);
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
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the forfeit's values are PES's";
  std::string err;
  const auto k = InstalledForfeitConstants(&err);
  ASSERT_TRUE(k) << err;
  // player+0x0c == 4 and player+0x18 == 0 (FUN_140A23260, 0x140A2330E and 0x140A23308).
  EXPECT_TRUE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState, 0));
  // A marker on player+0x18 (an injury, §2.12) clears the offence: PES's writer requires it clear.
  EXPECT_FALSE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState, 1));
  // The test is exact: the states either side of 4 are not the offence.
  EXPECT_FALSE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState - 1, 0));
  EXPECT_FALSE(pes::forfeit::IsForfeitOffence(*k, k->sentOffState + 1, 0));
}

TEST(PesForfeitInstall, ThePostMatchPeriodIsTheOneTheWhistleWrites) {
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the forfeit's values are PES's";
  std::string err;
  const auto k = InstalledForfeitConstants(&err);
  ASSERT_TRUE(k) << err;
  // The min-players writer (0x140447EF8) and the full-time writer agree on the post-match period.
  EXPECT_EQ(static_cast<int>(pes::PeriodAfterWhistle(3, false)), k->postMatchPeriod);
}

// --- the live match: the minimum-players rule (cards through GiveRedCard, removal through SendOff) ---

// Five outfielders sent off: six remain, below the minimum. The match ends at the next restart, in
// period 10, with play stopped.
TEST(PesForfeitMatch, ASideShortOfTheMinimumEndsTheMatchAtItsNextStoppage) {
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  std::string err;
  const auto k = InstalledForfeitConstants(&err);
  ASSERT_TRUE(k) << err;
  auto match = BuildMatch(kMatchMinutes);
  ASSERT_TRUE(match) << "a match constant or the rig is missing from the install";
  PlayUntilLive(*match);
  const int toRemove = 11 - (pes::forfeit::kPesMinimumPlayers - 1);
  std::vector<Player*> outfield = OutfieldOnPitch(*match, HOME);
  ASSERT_GE(static_cast<int>(outfield.size()), toRemove);
  for (int i = 0; i < toRemove; ++i) ShowRed(*match, outfield[i]);
  RunToFullTimeOrWindow(*match);
  ASSERT_TRUE(match->IsGameOver()) << "no restart ended the match";
  EXPECT_FALSE(match->IsInPlay()) << "the match ended in play, not at a stoppage";
  EXPECT_EQ(static_cast<int>(match->GetPesClock().Record().period), k->postMatchPeriod);
  EXPECT_EQ(ActiveCount(*match, HOME), pes::forfeit::kPesMinimumPlayers - 1);
}

// Four outfielders sent off: seven remain, which is the minimum. The rule is "fewer than", so the
// match keeps going through the whole window.
TEST(PesForfeitMatch, ASideAtTheMinimumKeepsPlaying) {
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  auto match = BuildMatch(kMatchMinutes);
  ASSERT_TRUE(match) << "a match constant or the rig is missing from the install";
  PlayUntilLive(*match);
  const int toRemove = 11 - pes::forfeit::kPesMinimumPlayers;
  std::vector<Player*> outfield = OutfieldOnPitch(*match, HOME);
  ASSERT_GE(static_cast<int>(outfield.size()), toRemove);
  for (int i = 0; i < toRemove; ++i) ShowRed(*match, outfield[i]);
  RunToFullTimeOrWindow(*match);
  EXPECT_FALSE(match->IsGameOver()) << "a side at the minimum was ended";
  EXPECT_EQ(ActiveCount(*match, HOME), pes::forfeit::kPesMinimumPlayers);
}

// --- the live match: injuries (contact path) and substitutions (request path) ---

// PES's injury scan (FUN_140523910) keeps an injured player on the pitch when the picker finds no
// bench player: FUN_140521930 returns 0xFF once the side's allowance is used (FUN_140521000). GF's
// ProcessAutoSubstitutions does the same. Here the bench's swaps go through the real request and
// execution path, then a real contact injures a player with no bench player left.
TEST(PesForfeitMatch, AnInjuredPlayerStaysOnThePitchWhenTheBenchIsUsedUp) {
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  auto match = BuildMatch(kMatchMinutes);
  ASSERT_TRUE(match) << "a match constant or the rig is missing from the install";
  PlayUntilLive(*match);
  ASSERT_TRUE(RunToStoppage(*match)) << "no stoppage to make substitutions in";
  const int allowance = SubstitutionsLeft(*match, HOME);
  const int benched = RequestSubstitutions(*match, HOME, allowance);  // capped by the bench
  ASSERT_GT(benched, 0) << "no bench player to bring on";
  for (int i = 0; i < 4; ++i) match->Process();
  ASSERT_EQ(SubstitutionsLeft(*match, HOME), allowance - benched) << "the swaps were not made";
  std::vector<Player*> outfield = OutfieldOnPitch(*match, HOME);
  ASSERT_FALSE(outfield.empty());
  Player* injured = outfield.front();
  ASSERT_TRUE(InjureByContact(*match, injured)) << "the contact path did not injure him";
  ASSERT_TRUE(RunThroughAStoppage(*match)) << "no stoppage after the injury";
  EXPECT_TRUE(injured->IsActive()) << "an injured player with no sub left came off the pitch";
  EXPECT_EQ(SubstitutionsLeft(*match, HOME), allowance - benched);
}

// The control for the test above: with a sub left, the injured player is replaced.
TEST(PesForfeitMatch, AnInjuredPlayerIsReplacedWhileASubIsLeft) {
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  auto match = BuildMatch(kMatchMinutes);
  ASSERT_TRUE(match) << "a match constant or the rig is missing from the install";
  PlayUntilLive(*match);
  const int allowance = SubstitutionsLeft(*match, HOME);
  std::vector<Player*> outfield = OutfieldOnPitch(*match, HOME);
  ASSERT_FALSE(outfield.empty());
  Player* injured = outfield.front();
  ASSERT_TRUE(InjureByContact(*match, injured)) << "the contact path did not injure him";
  ASSERT_TRUE(RunThroughAStoppage(*match)) << "no stoppage after the injury";
  EXPECT_FALSE(injured->IsActive()) << "a bench player was available and the injured player stayed";
  EXPECT_LT(SubstitutionsLeft(*match, HOME), allowance);
}

// --- the live match: a red card through GF's path, and the forfeit ---

// GF's live path: a sent-off player is removed at his effective time, so at full time the side has
// no sent-off player on the pitch and is not forfeited. PES keeps a sent-off player on the pitch
// when no replacement is queued (FUN_140523110, FUN_140523210); GF does not port that path yet (OPEN,
// docs/re/pes-re-missing-systems.md 2.7 and 2.13). When it is ported, this test must change with it.
TEST(PesForfeitMatch, ARedCardRemovesTheOffenderAndTheSideIsNotForfeited) {
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  auto match = BuildMatch(kShortMatchMinutes);
  ASSERT_TRUE(match) << "a match constant or the rig is missing from the install";
  PlayUntilLive(*match);
  Player* offender = OutfieldOnPitch(*match, HOME).front();
  ShowRed(*match, offender);
  RunToFullTimeOrWindow(*match);
  ASSERT_TRUE(match->IsGameOver()) << "the match did not reach full time";
  EXPECT_FALSE(offender->IsActive()) << "a sent-off player is still on the pitch";
  EXPECT_FALSE(match->GetMatchData()->IsForfeited(HOME));
}

// A yellow then a red whose removal has not taken effect by full time: the player is still on the
// pitch with the sent-off state when the final whistle blows. That is the one case GF reaches the
// 3-0 writer through its live card path. The score and scorers are data, set through MatchData.
TEST(PesForfeitMatch, AYellowThenRedStillOnThePitchAtFullTimeForfeitsThreeNil) {
  if (!HaveInstall()) GTEST_SKIP() << "PES21_ROOT not set: the match's constants come from PES";
  std::string err;
  const auto k = InstalledForfeitConstants(&err);
  ASSERT_TRUE(k) << err;
  auto match = BuildMatch(kShortMatchMinutes);
  ASSERT_TRUE(match) << "a match constant or the rig is missing from the install";
  PlayUntilLive(*match);
  Player* offender = OutfieldOnPitch(*match, HOME).front();
  offender->GiveYellowCard(kNeverSentOff_ms);
  offender->GiveRedCard(kNeverSentOff_ms);
  MatchData* data = match->GetMatchData();
  data->AddEvent(MatchData::Event::Goal, AWAY, 1, "scorer");
  data->AddEvent(MatchData::Event::Goal, AWAY, 2, "scorer");
  data->SetGoalCount(AWAY, 2);
  RunToFullTimeOrWindow(*match);
  ASSERT_TRUE(match->IsGameOver()) << "the match did not reach full time";
  EXPECT_TRUE(data->IsForfeited(HOME));
  EXPECT_FALSE(data->IsForfeited(AWAY));
  EXPECT_EQ(data->GetGoalCount(HOME), 0);
  EXPECT_EQ(data->GetGoalCount(AWAY), k->forfeitScore);
  for (const MatchData::Event& event : data->GetEvents()) {
    EXPECT_NE(event.kind, MatchData::Event::Goal);
    EXPECT_NE(event.kind, MatchData::Event::OwnGoal);
  }
}
