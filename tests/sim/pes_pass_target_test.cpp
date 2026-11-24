#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <cmath>
#include <cstring>
#include "onthepitch/pestrapworld.hpp"

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
#include "onthepitch/pesteamtick.hpp"
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
  std::vector<double> episodes_s;          // each PASS_GET episode: consecutive ticks one man was his side's PassGetReceiver()
  double worstRootGap_m = 0.0;             // the largest mismatch between a PASS_GET man's root step and his strike adjuster's step
  unsigned long rootSamples = 0;           // ticks with such a comparison
  bool ran = false;
};

// The pass-get man's root and strike adjuster point on the previous tick, for the root-step comparison.
struct RootSample {
  bool valid = false;
  double gx = 0.0, gy = 0.0, ax = 0.0, az = 0.0;
  int16_t count = 0;  // the adjuster's counter on that tick: a pair is one strike's consecutive ticks
};

class PesPassTarget : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    double restingSinceTargetSet_s = 0.0;
    std::array<const Player*, 2> episodeMan{nullptr, nullptr};
    std::array<double, 2> episode_s{0.0, 0.0};
    stats_.episodes_s.clear();
    stats_.ran = false;
    std::array<RootSample, 2> rootPrev{};
    RunPesLoopMatch([&](Match& match) {
      stats_.ran = true;
      const bool targetSet = match.GetTeam(0)->GetPassTarget() != nullptr || match.GetTeam(1)->GetPassTarget() != nullptr;
      stats_.targetTicks += targetSet ? 1 : 0;
      // PES's play state (MI+0x116C): 1 in play, 3 set-play ready (the take, from the awarded restart's readiness to the touch), 2 stopped.
      // The in-play arm clears or renames the target at 1 and at 3; the stoppage keep is at 2 and is not bounded (docs/re/pes-re-taker-release.md 1.2-1.3).
      const int playState = match.PesPlayState();
      const bool resting = (playState == pes::kPlayStateInPlay || playState == pes::kPlayStateSetPlayReady) &&
                           match.GetBall()->GetMovement().GetLength() < kRestSpeed_ms;
      const double tick_s = static_cast<double>(Match::kTick_ms) / 1000.0;
      restingSinceTargetSet_s = targetSet && resting ? restingSinceTargetSet_s + tick_s : 0.0;
      stats_.worstStale_s = std::max(stats_.worstStale_s, restingSinceTargetSet_s);
      // The PASS_GET man's root against his strike adjuster (see ATrapsRootMovesByExactlyTheStrikeAdjusterStep).
      for (int side = 0; side < 2; ++side) {
        Player* const r = match.GetTeam(side)->PassGetReceiver();
        const uint8_t* const p = r != nullptr ? match.PesWorldOf().P(match.PesSlotOf(r)) : nullptr;
        uint32_t state = 0;
        float ax = 0.0f, az = 0.0f;
        // A+0x14 holds the layer's root point once the adjuster's counter has reached 1: FUN_141AF3CA0 loads it there
        // (FUN_141EB3AF0, docs/ports/pes-port-RootMotion.md section 1). Before that the field keeps its unwritten zeros, so a sample
        // taken then is not a reference point (the old test's 38 m). Only ticks at counter 2 or later, of one strike, form a pair.
        int16_t count = 0;
        const bool strike = p != nullptr && p[pes::kPByteAC8Offset] == pes::kAnimeActionTrap &&
                            (std::memcpy(&state, p + pes::kPAdjusterOffset + pes::kAdjStateOffset, sizeof state), state == pes::kAdjRunning) &&
                            (std::memcpy(&count, p + pes::kPAdjusterOffset + pes::kAdjCountOffset, sizeof count), count >= 2);
        if (!strike) {
          rootPrev[side].valid = false;
          continue;
        }
        std::memcpy(&ax, p + pes::kPAdjusterOffset + pes::kAdjStepOffset, sizeof ax);
        std::memcpy(&az, p + pes::kPAdjusterOffset + pes::kAdjStepOffset + 2 * sizeof(float), sizeof az);
        // PES's (x, z) is GF's (x, y): the man record is written that way (WritePesManMovement).
        const Vector3 g = r->GetPosition();
        RootSample& prev = rootPrev[side];
        if (prev.valid && count == prev.count + 1) {
          const double gap = std::hypot((g.coords[0] - prev.gx) - (ax - prev.ax), (g.coords[1] - prev.gy) - (az - prev.az));
          stats_.worstRootGap_m = std::max(stats_.worstRootGap_m, gap);
          ++stats_.rootSamples;
        }
        prev = RootSample{true, g.coords[0], g.coords[1], ax, az, count};
      }
      for (int side = 0; side < 2; ++side) {
        const Player* receiver = match.GetTeam(side)->PassGetReceiver();
        stats_.receiverTicks += receiver != nullptr ? 1 : 0;
        stats_.foreignReceiverTicks += receiver != nullptr && receiver->GetTeamID() != side ? 1 : 0;
        if (receiver != episodeMan[side]) {
          if (episodeMan[side] != nullptr) stats_.episodes_s.push_back(episode_s[side]);
          episodeMan[side] = receiver;
          episode_s[side] = 0.0;
        }
        if (receiver != nullptr) episode_s[side] += tick_s;
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
// NAMED GAP (owner ruling 19-11-26): red on the whole TeamTick -> OneWorld line (8.5 s on seed 7). The stale target is a ball left at
// rest by four GF stand-ins around it (docs/re/pes-re-approach.md G1-G4, pes-re-move-gate.md, pes-re-holder-clear.md): the presser's
// body on GF's own kick clips so MOVESTEP never steers him, no DRIBBLE state for the holder, and the non-pressers' stand. Skipped until
// the walker work (handover step 4) ports them; the assertion below is the acceptance for that work and is not to be relaxed.
TEST_F(PesPassTarget, IsSetInFlightAndNeverStaleWhileTheBallRests) {
  GTEST_SKIP() << "named gap: the walker port (docs/re/pes-re-approach.md) closes it";
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

// PES's PASS_GET episodes (a man named the receiver by T+0x78 and walking to the ball) last 0.02 to 2.94 s in live play
// (docs/re/pes-re-receiving.md 5.2, 40 live episodes). Every episode of the seed-7 match must fall in that range: a longer one is a
// receiver who arrives late or never plays the ball (the idle stalls of docs/ports/pes-port-PassReceiver.md section 3).
TEST_F(PesPassTarget, EveryPassGetEpisodeIsWithinPesLiveRange) {
  if (!stats_.ran) GTEST_SKIP() << "no match ran";
  ASSERT_FALSE(stats_.episodes_s.empty()) << "no PASS_GET episode in the match";
  std::vector<double> sorted = stats_.episodes_s;
  std::sort(sorted.begin(), sorted.end());
  const double median = sorted[sorted.size() / 2];
  const long over = std::count_if(sorted.begin(), sorted.end(), [](double e) { return e > kPesPassTargetMaxLive_s; });
  RecordProperty("episodes", static_cast<int>(sorted.size()));
  RecordProperty("median_s", std::to_string(median));
  EXPECT_EQ(over, 0) << over << " of " << sorted.size() << " PASS_GET episodes exceeded " << kPesPassTargetMaxLive_s
                     << " s; longest " << sorted.back() << " s, median " << median << " s";
}

// PES's strike adjuster is the trapping man's whole root translation while it runs: FUN_141AFD400 replaces the layer's root
// translation with the adjuster's root point (A+0x14) whenever the strike flag B+0x81 is set, so per tick his root moves by exactly
// the change in A+0x14 (the captures of the strike adjuster: O2 root == A+0x14 at every entry, pes-port PassArrival section 10).
// GF adds the trap clip's root track on top of the adjuster's warps, which carries a trapping man past the ball.
TEST_F(PesPassTarget, ATrapsRootMovesByExactlyTheStrikeAdjusterStep) {
  if (!stats_.ran) GTEST_SKIP() << "no match ran";
  // Derived, not a PES constant: positions are float near 20 to 35 m, whose ulp is about 4e-6 m; a few float operations on the
  // same two values stay under 1e-4 m, so 1 mm leaves a 10x margin over the float noise and nothing tuned.
  constexpr double kRootStepTolerance_m = 0.001;
  ASSERT_GT(stats_.rootSamples, 0ul) << "no pass-get man in the Trap action with his strike adjuster running";
  EXPECT_LE(stats_.worstRootGap_m, kRootStepTolerance_m)
      << "a trapping man's root moved " << stats_.worstRootGap_m << " m more (or less) than his strike adjuster's step, in the "
      << stats_.rootSamples << " ticks sampled";
}
