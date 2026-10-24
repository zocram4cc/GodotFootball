#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "gamedefines.hpp"
#include "gametypes.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesevents.hpp"
#include "onthepitch/pespress.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
// The two-minute match with its stoppages, counted in the 54 Hz logic frames the sim ticks in, with headroom.
constexpr unsigned long kMaxTicks = 6ul * 60ul * Match::kFramesPerSecond;

// One PES-loop match on seed 7, `onTick` after each tick, until full time or `kMaxTicks`.
void RunPesLoopMatch(const std::function<void(Match&)>& onTick) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql"));
  SetDB(&db);

  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = HOME_TEAM;
  setup->awayTeamDbId = AWAY_TEAM;
  setup->seed = kSeed;
  setup->durationMinutes = kMinutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, config);
  for (unsigned long t = 0; !match.IsGameOver() && t < kMaxTicks; ++t) {
    match.Process();
    onTick(match);
  }
}

}  // namespace

// PES's coordinator gate (FUN_140632780) fails only on the side's OWN valid T+0x78 (lane-leaves §4:
// the writer FUN_1404FB2A0; audit 7 F6). GF folded "no kick" and "opponent's kick" into one false, so when
// side 0 kicked and side 1 touched last, side 0's gate did not fail on the kick arm. The coordinator latches
// the own-side word and the opponent's touch must not clear it.
TEST(PesPressGate, AnOpponentTouchDoesNotClearThisSidesOwnKickFail) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql"));
  SetDB(&db);
  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = HOME_TEAM;
  setup->awayTeamDbId = AWAY_TEAM;
  setup->seed = kSeed;
  setup->durationMinutes = kMinutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";
  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, config);
  TeamAIController* controller = match.GetTeam(0)->GetController();
  ASSERT_EQ(controller->PressOwnKick(), pes::kPressNoMan) << "the own-side word starts invalid";
  // Settle the kickoff: the referee's preparation calls Match::ResetSituation, which clears the event
  // store, so a touch queued before it is wiped. Queue only once the preparation has run.
  for (int t = 0; t < 300 && !match.GetRefereeBuffer().prepared; ++t) match.Process();
  ASSERT_TRUE(match.GetRefereeBuffer().prepared) << "the kickoff was never prepared";
  // Side 0 kicks: the coordinator latches the own-side word.
  Player* own = match.GetTeam(0)->GetAllPlayers().front().get();
  Player* opp = match.GetTeam(1)->GetAllPlayers().front().get();
  match.QueuePesTouch(own, 7);  // kind 7 routes to ring 0 + newest (the install's table 0x142642e60)
  match.Process();  // the event stage routes it; the team tick runs the coordinator (team.cpp)
  const int latched = controller->PressOwnKick();
  ASSERT_LT(latched, pes::kPressTeamMen) << "the own kick did not latch";
  EXPECT_EQ(latched, match.PesActorKeyOf(own).team * pes::kPressSideMen + match.PesActorKeyOf(own).slot);
  // Side 1 touches last: the newest event is his, but side 0's latched word must stand.
  match.QueuePesTouch(opp, 7);
  match.Process();
  const pes::NewestEventView newest = pes::NewestEvent(match.GetPesEvents());
  ASSERT_TRUE(newest.present);
  ASSERT_EQ(newest.actor.team, 1u) << "the newest touch is not the opponent's";
  EXPECT_EQ(controller->PressOwnKick(), latched) << "the opponent's touch cleared side 0's own kick word";
  // And the gate fed from the latched word still fails with every other arm passing.
  auto c = match.GetPesPressConstants();
  pes::PressGateInput g;
  g.teamKickValid = controller->PressOwnKick() < pes::kPressTeamMen;
  g.classFive = true;
  g.cover = true;
  EXPECT_FALSE(pes::PressGatePasses(c, g)) << "side 0's gate passed with its own kick word valid";
}

// PES's ActionPress steers the loose-ball presser to its target, the ball now or his own base (docs/re/pes-re-press-life.md
// section 8, step 6). His movement command must name that target: a command without one projects its direction a second
// ahead (pesmovestep FillCommandWords), which overshoots a ball a few metres away. Scope: a presser who does not hold the ball
// and is not the PASS_GET receiver (ActionPassGet's approach is another PES action, not this port's), with a movement decision.
TEST(PesPressApproach, ALoosePresserCommandNamesTheActionPressTarget) {
  unsigned long steered = 0, unnamed = 0;
  RunPesLoopMatch([&](Match& match) {
    const bool loose = match.IsInPlay() && !match.IsInSetPiece() && !match.GetBallRetainer() &&
                       match.GetBall()->GetMovement().GetLength() <= GameplayTuning::kStalemateBallSpeed_ms;
    if (!loose) return;
    for (int t = 0; t < 2; ++t) {
      Team* team = match.GetTeam(t);
      Player* presser = team->GetController()->GetLoosePressMan();
      if (!presser || presser->HasPossession() || team->PassGetReceiver() == presser) continue;
      const std::optional<PlayerCommand>& command = presser->GetPesDecision().command;
      if (!command || command->desiredFunctionType != e_FunctionType_Movement) continue;
      ++steered;
      if (!command->useDesiredTarget) ++unnamed;
    }
  });
  EXPECT_GT(steered, 0ul) << "no loose presser was steered in the match";
  EXPECT_EQ(unnamed, 0ul) << unnamed << " of " << steered << " loose-presser movement commands named no target";
}
