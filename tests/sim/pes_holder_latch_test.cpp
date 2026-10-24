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

// PES's holder is the actor of the newest ring-1 touch key: its only writer is W1 (VA 0x14045725A, MOV
// [RBP+0x2601C],EDI in FUN_140457070; docs/re/pes-re-first-touch.md section 5.1), which GF ports as the per-tick
// recompute behind the restart-step switch (Match::RecomputePesHolder; docs/re/pes-re-restart-step.md section 3).
// PES's carrier is never cleared on loss (it persists on a loose ball until a newer touch replaces it, docs/re/
// pes-re-press-coord.md section 5), so the holder is carried as its own state (Match::GetPesHolder) apart from
// GF's ballRetainer, which is also the "the ball is loose / free" predicate the AI reads (designated possession in
// match.cpp, the press reset, the keeper catch, and the far-loose-ball detector) and so MUST clear when the ball
// leaves the man. The holder arms from a touch that keeps the ball with the man: the touch pushes the key
// (CarrierFromTouch over AI_HasPossession, AIfunctions.cpp:1091, a height/distance/movement gate that can be
// false for one step while the ball settles at his feet) and the recompute adopts the key at the next tick's
// head, so the arm lands one step after the touch. The test therefore tolerates a one-tick gap and counts a miss
// only while it persists for two ticks running: the man live with no holder this tick and the tick before. In
// play, not a set piece.
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

// The split from GF's retainer, at the bytes: W1 rewrites MA+0x2601C and only W2, the object reset
// (0x140A71DF9, HolderRE runs 3-4), stores 0xFF, so once a touch has made a man the holder it stays him through
// every loose-ball flicker - the ball leaving his feet clears GF's retainer (the retain block) but nothing
// clears the holder. A miss is an in-play tick with a set-then-null holder; the pre-first-arm window does not
// count, and it is pinned separately by the arming expectation.
TEST(PesHolderPersist, ASetHolderIsNeverClearedWhileTheBallIsInPlay) {
  bool holderArmed = false;
  unsigned long armedTicks = 0, cleared = 0;
  RunPesLoopMatch([&](Match& match) {
    if (!holderArmed) {
      holderArmed = match.GetPesHolder() != nullptr;
      return;
    }
    if (!match.IsInPlay() || match.IsInSetPiece()) return;
    ++armedTicks;
    if (!match.GetPesHolder()) ++cleared;
  });
  EXPECT_TRUE(holderArmed) << "no touch ever made a man the holder";
  EXPECT_GT(armedTicks, 0ul) << "the holder armed too late to observe";
  EXPECT_EQ(cleared, 0ul) << cleared
                          << " in-play ticks cleared a set holder (PES clears only in W2's object reset)";
}
