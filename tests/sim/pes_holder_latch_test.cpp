#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "data/matchdata.hpp"
#include "gamedefines.hpp"
#include "gametypes.hpp"
#include "onthepitch/AIsupport/AIfunctions.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
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

// PES's holder is the actor of the newest ring-1 key: its only writers are W1 (0x140457259, MOV [R13+0x2601C],EDI in
// FUN_140457070; docs/re/pes-re-holder-clear.md section 2), which stores the actor of the newest valid key and 0xFF
// otherwise, and W2 (0x140A71DF9), which resets it at object init. GF ports W1 as the per-tick recompute behind the
// restart-step switch (Match::RecomputePesHolder; docs/re/pes-re-restart-step.md section 3). PES's kick pushes the
// none key into ring 1, so a pass clears the holder to 0xFF for the whole flight until a receiver's touch
// (docs/ports/pes-port-StallLive6.md section 2: MA+0x2601C = 255 on the kick frame and on all 6,926 flight frames).
// A live last toucher therefore holds the holder only while no pass has come since his touch. The arm lands one tick
// after the touch (the recompute adopts the key at the next tick's head), so the test tolerates a one-tick gap and
// counts a miss only while it persists for two ticks running. In play, not a set piece.
TEST(PesHolderLatch, ALastToucherWithLivePossessionBecomesTheHolder) {
  std::map<const Player*, bool> unheldLastTick;  // last toucher, ball live, no holder - at the previous post-tick
  unsigned long liveTicks = 0, unheld = 0;
  RunPesLoopMatch([&](Match& match) {
    std::map<const Player*, bool> unheldNow;
    const bool inPlay = match.IsInPlay() && !match.IsInSetPiece();
    for (int t = 0; inPlay && t < 2; ++t) {
      for (const std::unique_ptr<Player>& p : match.GetTeam(t)->GetAllPlayers()) {
        if (!p->IsActive() || p->GetFormationEntry().role == e_PlayerRole_GK) continue;
        if (match.GetLastTouchPlayer() != p.get() || !AI_HasPossession(match.GetBall(), p.get())) continue;
        ++liveTicks;
        const auto& pass = match.GetLastPassEvent();
        if (pass.valid && pass.time_ms >= p->GetLastTouchTime_ms()) continue;  // the none key is the newest, so no holder is PES's
        if (!match.GetPesHolder()) {
          unheldNow[p.get()] = true;
          if (unheldLastTick.count(p.get())) ++unheld;  // no holder for two ticks running, not a one-tick gap
        }
      }
    }
    unheldLastTick = std::move(unheldNow);
  });
  EXPECT_GT(liveTicks, 0ul) << "no outfield last toucher had the ball live in the match";
  EXPECT_EQ(unheld, 0ul) << unheld
                         << " ticks had a live outfield last toucher with no holder for two ticks running";
}

// PES's W1 stores 0xFF while the newest ring-1 key is the kick's none key, so a pass clears a set holder. W2 only
// resets it at object init. A loose ball does not clear it: the ball leaving his feet clears GF's retainer (the retain
// block) but never the holder. So every in-play tick that finds the holder null must follow a pass since the holder
// was last set (docs/ports/pes-port-StallLive6.md section 2). A miss is an in-play clear with no pass in between; the
// pre-first-arm window does not count.
TEST(PesHolderPersist, ASetHolderIsClearedOnlyByAPassWhileTheBallIsInPlay) {
  bool holderArmed = false;
  unsigned long armedTicks = 0, unattributed = 0;
  // The kick is recorded in its own tick. The holder is observed set one tick after it (the recompute that clears it
  // runs at the next tick's head), so a pass counts when it falls within two ticks before that last observation.
  const double kKickWindowMs = 2.0 * 1000.0 / Match::kFramesPerSecond;
  double holderSetAt = 0.0;  // when the holder was last observed set, in any stage
  RunPesLoopMatch([&](Match& match) {
    if (match.GetPesHolder()) holderSetAt = double(match.GetActualTime_ms());
    if (!holderArmed) {
      holderArmed = match.GetPesHolder() != nullptr;
      return;
    }
    if (!match.IsInPlay() || match.IsInSetPiece()) return;
    ++armedTicks;
    if (match.GetPesHolder()) return;
    const auto& pass = match.GetLastPassEvent();
    if (!pass.valid || double(pass.time_ms) + kKickWindowMs < holderSetAt) ++unattributed;
  });
  EXPECT_TRUE(holderArmed) << "no touch ever made a man the holder";
  EXPECT_GT(armedTicks, 0ul) << "the holder armed too late to observe";
  EXPECT_EQ(unattributed, 0ul) << unattributed
                               << " in-play ticks cleared a set holder with no pass since it was set (PES clears only on the kick's none key)";
}
