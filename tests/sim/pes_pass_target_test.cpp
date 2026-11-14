#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "gamedefines.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

#include "onthepitch/referee.hpp"
namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
constexpr unsigned long kMaxTicks = 6ul * 60ul * Match::kFramesPerSecond;

// The longest a team's pass target stayed set in PES's live play (docs/re/pes-re-receiving.md section 2: PASS_GET lasts 0.02 to 2.94 s).
constexpr double kPesPassTargetMaxLive_s = 2.94;
// The ball counts as at rest below GF's stalemate breaker's own bound.
constexpr double kRestSpeed_ms = GameplayTuning::kStalemateBallSpeed_ms;

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

// What one PES-loop match did with the pass target (T+0x78) and the receiver derived from it, collected once for the tests below.
struct PassStats {
  double worstStale_s = 0.0;           // the longest a set target survived with the ball at rest in live play
  unsigned long targetTicks = 0;       // ticks with a target named (either side)
  unsigned long receiverTicks = 0;     // ticks with a PassGetReceiver() (either side)
  unsigned long foreignReceiverTicks = 0;  // ticks a side's receiver was not on that side
  unsigned long foreignNamedTicks = 0;     // receiver ticks on which the side's T+0x78 was rewritten to the other side's man for one query
  unsigned long foreignAcceptedTicks = 0;  // of those, the ticks the side still answered a receiver
  bool ran = false;
};

class PesPassTarget : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    double restingSinceTargetSet_s = 0.0;
    stats_.ran = false;
    RunPesLoopMatch([&](Match& match) {
      stats_.ran = true;
      const bool targetSet = match.GetTeam(0)->GetPassTarget() != nullptr || match.GetTeam(1)->GetPassTarget() != nullptr;
      stats_.targetTicks += targetSet ? 1 : 0;
      const bool resting = match.IsInPlay() && match.GetBall()->GetMovement().GetLength() < kRestSpeed_ms;
      const double tick_s = static_cast<double>(Match::kTick_ms) / 1000.0;
      restingSinceTargetSet_s = targetSet && resting ? restingSinceTargetSet_s + tick_s : 0.0;
      stats_.worstStale_s = std::max(stats_.worstStale_s, restingSinceTargetSet_s);
      for (int side = 0; side < 2; ++side) {
        const Player* receiver = match.GetTeam(side)->PassGetReceiver();
        stats_.receiverTicks += receiver != nullptr ? 1 : 0;
        stats_.foreignReceiverTicks += receiver != nullptr && receiver->GetTeamID() != side ? 1 : 0;
        if (receiver == nullptr) continue;
        // The pass has its own side's receiver. Rewrite the record T+0x78 to name a man of the OTHER side for the one query (the writer
        // never does, PES's own record always names the side's own man) and see whether the receiver read follows it; restore at once.
        Player* const foreignMan = match.GetTeam(1 - side)->GetAllPlayers().front().get();
        if (!foreignMan->IsActive()) continue;
        uint8_t* const record = match.PesWorldOf().SideRecord(side);
        const uint32_t named = pes::LoadU32(record, pes::kSidePassTargetOffset);
        pes::StoreU32(record, pes::kSidePassTargetOffset, static_cast<uint32_t>(match.PesSlotOf(foreignMan)));
        ++stats_.foreignNamedTicks;
        stats_.foreignAcceptedTicks += match.GetTeam(side)->PassGetReceiver() != nullptr ? 1 : 0;
        pes::StoreU32(record, pes::kSidePassTargetOffset, named);
      }
    });
  }
  static PassStats stats_;
};
PassStats PesPassTarget::stats_;

// The pass target is PES's own team record T+0x78, rewritten by the selector chain every team tick (VA 0x1404FB2A0 -> 0x1404FB380;
// docs/ports/pes-port-TeamTick.md). GF used to clear its copy only at ResetSituation, so a pass that died on the grass kept its receiver
// (a keeper 35 m away for 8 s on seed 7, docs/ports/pes-port-RestingBall3.md). On the PES path a set target must (a) exist while a pass is
// in flight, and (b) be gone within PES's own live bound once the ball has come to rest in live play. PES's selector keeps T+0x78 through a
// stoppage on purpose (0x1404FB780, the MI+0x116C == 2 arm: it returns with a set target untouched while MI+0x11D4 == 2 and nobody
// holds the ball), but a stoppage is not live play: `resting` requires IsInPlay(). The in-play arms (0x1404FB380, MI+0x116C 1 and 3)
// clear or rename the target on their cadence. The bound is PES's, measured in live play (PASS_GET lasts 0.02 to 2.94 s).
TEST_F(PesPassTarget, IsSetInFlightAndNeverStaleWhileTheBallRests) {
  if (!stats_.ran) GTEST_SKIP() << "no match ran";
  EXPECT_GT(stats_.targetTicks, 0ul) << "the selector chain never named a pass target in the match";
  EXPECT_LE(stats_.worstStale_s, kPesPassTargetMaxLive_s) << "a pass target stayed set for " << stats_.worstStale_s << " s with the ball at rest";
}

// The kick no longer writes a receiver on the PES path, so the receiver is read from T+0x78 (Team::PassGetReceiver): a pass in flight
// has one, and it is always a man of the side whose record names him (FUN_140931c60 compares the man's slot id with his own side's
// T+0x78, so another side's man is never a receiver and never the team's designated man).
TEST_F(PesPassTarget, EveryPassInFlightHasItsOwnSidesReceiver) {
  if (!stats_.ran) GTEST_SKIP() << "no match ran";
  EXPECT_GT(stats_.receiverTicks, 0ul) << "PassGetReceiver() was null for every pass of the match";
  EXPECT_EQ(stats_.foreignReceiverTicks, 0ul) << "a side's receiver was a man of the other side";
  EXPECT_GT(stats_.foreignNamedTicks, 0ul) << "no pass in flight to rewrite the record under";
  EXPECT_EQ(stats_.foreignAcceptedTicks, 0ul) << "a record naming the other side's man still produced a receiver";
}
