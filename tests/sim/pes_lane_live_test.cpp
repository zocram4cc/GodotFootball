// PES 2021's lane line on a live PES-loop match: the lane word A and the A-rank (FUN_1405CE4F0;
// docs/re/pes-re-lane-writers.md 2; docs/re/pes-re-lane-leaves.md 3; pesmentality.hpp).
#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "data/matchdata.hpp"
#include "gamedefines.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesmentality.hpp"
#include "onthepitch/pesroles.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
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
constexpr unsigned long kMaxTicks = 6ul * 60ul * Match::kFramesPerSecond;
// The copy-path test sends its man off in play after this match time (ms), before the first stoppage.
constexpr unsigned long kSendOffAfter_ms = 30000ul;

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

// The lane word is a line: 0 back, 1 mid, 2 forward, 3 none (blockA-forward-pull 1). CE150 writes the B words as
// lines, so no active man can carry a class value as his lane word.
TEST(LaneLine, EveryActiveManCarriesALineWord) {
  unsigned long ticks = 0, violations = 0;
  RunPesLoopMatch([&](Match& match) {
    ++ticks;
    for (int t = 0; t < 2; ++t) {
      Team* team = match.GetTeam(t);
      TeamAIController* ctl = team->GetController();
      const auto& players = team->GetAllPlayers();
      for (int k = 0; k < pes::roles::kSideSlots && k < static_cast<int>(players.size()); ++k) {
        if (!players[k]->IsActive()) continue;
        const int word = ctl->LaneWordOfSlot(k);
        if (word < 0 || word > 3) ++violations;
      }
    }
  });
  EXPECT_GT(ticks, 0ul);
  EXPECT_EQ(violations, 0ul) << violations << " active lane words were not a line (0..3)";
}

// The copy path (FUN_1405CE4F0, flag LX+0x1B0 == 0; lane-writers 2 step 2) copies the B words into the A words for
// all eleven slots on each stoppage tick, a man sent off included. One outfield man is sent off in play; on the next
// stoppage tick his lane word must still be his B word.
TEST(LaneCopyPath, AStoppageCopiesTheBWordOfEveryManInclusiveOfTheInactive) {
  int sentOff = -1;
  bool checked = false;
  int expected = -1, observed = -1;
  RunPesLoopMatch([&](Match& match) {
    if (checked) return;
    Team* team = match.GetTeam(0);
    TeamAIController* ctl = team->GetController();
    const auto& players = team->GetAllPlayers();
    if (sentOff < 0) {
      if (!match.IsInPlay() || match.IsInSetPiece() || match.GetActualTime_ms() < kSendOffAfter_ms) return;
      for (int k = 1; k < pes::roles::kSideSlots; ++k) {
        if (players[k]->IsActive() && ctl->RoleClassOfSlot(k) != 0) {
          sentOff = k;
          break;
        }
      }
      if (sentOff >= 0) players[sentOff]->Deactivate();
      return;
    }
    if (match.IsInPlay() || match.IsInSetPiece()) return;
    pes::mentality::LaneInput in;
    in.classes[sentOff] = ctl->RoleClassOfSlot(sentOff);
    expected = pes::mentality::LaneBWords(in)[sentOff];
    observed = ctl->LaneWordOfSlot(sentOff);
    checked = true;
  });
  EXPECT_GE(sentOff, 1) << "no outfield man was on the pitch to send off";
  EXPECT_TRUE(checked) << "the match never stopped after the send-off";
  EXPECT_EQ(expected, observed) << "the inactive man's lane word on a stoppage tick is not his B word";
}

// CE930's rank (lane-leaves 3): among the active on-pitch men with one lane word, the rank is the position in depth
// order, so each word's ranks are 0 .. n-1. Checked on every tick for the three lane words.
TEST(LaneRanks, SameWordRanksCountUpFromZero) {
  unsigned long groups = 0, violations = 0;
  RunPesLoopMatch([&](Match& match) {
    for (int t = 0; t < 2; ++t) {
      Team* team = match.GetTeam(t);
      TeamAIController* ctl = team->GetController();
      const auto& players = team->GetAllPlayers();
      for (int word = 0; word <= 2; ++word) {
        std::vector<int> ranks;
        for (int k = 0; k < pes::roles::kSideSlots && k < static_cast<int>(players.size()); ++k) {
          if (!players[k]->IsActive() || ctl->RoleClassOfSlot(k) == 0) continue;
          if (ctl->LaneWordOfSlot(k) == word) ranks.push_back(ctl->LaneRankOfSlot(k));
        }
        if (ranks.size() < 2) continue;
        ++groups;
        std::sort(ranks.begin(), ranks.end());
        for (size_t i = 0; i < ranks.size(); ++i) {
          if (ranks[i] != static_cast<int>(i)) {
            ++violations;
            break;
          }
        }
      }
    }
  });
  EXPECT_GT(groups, 0ul) << "no lane word was shared by two men in the match";
  EXPECT_EQ(violations, 0ul) << violations << " lane-word groups had ranks other than 0..n-1";
}
