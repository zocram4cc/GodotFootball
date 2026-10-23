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

// PES's holder is the last toucher: its only valid writer is W1 (VA 0x14045725A, MOV [RBP+0x2601C],EDI in
// FUN_140457070; docs/re/pes-re-first-touch.md section 5.1). PES's carrier is never cleared on loss (it persists
// on a loose ball until a newer touch replaces it, docs/re/pes-re-press-coord.md section 5), but GF's ballRetainer
// is not purely the carrier: it is also the "the ball is loose / free" predicate the AI reads (designated
// possession in match.cpp, the press reset, the keeper catch, and the far-loose-ball detector), so it MUST clear
// when the ball leaves the man. The holder rule is the latch added for that: while an outfield last toucher has
// the ball live (AI_HasPossession) and no one holds it, he becomes the holder. The latch runs in his humanoid
// step, and AI_HasPossession (AIfunctions.cpp:1091, a height/distance/movement gate) can be false for one step
// while the ball settles at his feet, so the retain block's clear (humanoid.cpp, CarrierAfterContact != kKeep)
// drops the holder and the next step's latch restores it: one tick of lag. The test therefore tolerates a
// one-tick flicker and counts a miss only while it persists for two ticks running: the man live with no holder
// this tick and the tick before. In play, not a set piece.
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
        if (!match.GetBallRetainer()) {
          unheldNow[p.get()] = true;
          if (unheldLastTick.count(p.get())) ++unheld;  // no holder for two ticks running, not a one-tick flicker
        }
      }
    }
    unheldLastTick = std::move(unheldNow);
  });
  EXPECT_GT(liveTicks, 0ul) << "no outfield last toucher had the ball live in the match";
  EXPECT_EQ(unheld, 0ul) << unheld
                         << " ticks had a live outfield last toucher with no holder for two ticks running";
}
