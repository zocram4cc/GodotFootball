// The press at the kick-off, against live PES. The oracle is `teamsel_1404FB2A0` (live2/pid1231179 seq337-361 minus 343: the first 24 in-play
// team-tick calls after the kick-off whistle, T = *(Y+0x6D50), read at the selector's entry, one call per side and frame): the
// kicking side (latch T+0x255 = 1) holds T+0x78 = 0xFF at its first call and the kick-off taker from its second on, and none of its 11
// action words T+0xA2A0 is ever set (it marks no one); the defending side (latch 0) holds T+0x78 = 0xFF throughout and sets action words
// on its men. GF's hand-ported presser did the reverse: it named the kicking side's own man PRESS and left the defenders unmarked, which
// ran the taker off the ball (MoveStepWire.TheKickoffTakerClosesToTheBall, 0.87 m).
//
// The window words are read after each Match::Process, so they are what the tick's selector (Team::RunPesPassTarget) and then the press
// (PesWorld::RunMatchUp, coordinator, reset, writer: PES's dispatcher FUN_140535C60 order) left in the team record.
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/player/player.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

constexpr int kHomeTeam = 1, kAwayTeam = 2;
constexpr unsigned int kSeed = 1003;  // the seed MoveStepWire stages its kick-off on
constexpr int kCalls = 12;            // in-play team ticks per side: PES's 24 captured calls are 12 frames of two sides
constexpr int kMaxTicks = 4000;

struct Row {
  uint32_t passTarget = 0;  // T+0x78
  uint32_t second = 0;      // T+0x7C
  std::array<uint32_t, pes::kSlotsPerSide> words{};
};

struct Census {
  bool ready = false;
  int kicker = -1;  // the side whose latch T+0x255 is 1 at its first in-play call
  std::array<std::vector<Row>, 2> rows;
  int takerSlot = -1;  // the kick-off taker's slot within his side
};

const Census& RunKickoff() {
  static const Census census = [] {
    Census c;
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return c;
    pes::PesVfs vfs;
    std::string err;
    if (!vfs.Mount(root, &err)) return c;
    auto rig = pesrig::BuildRig(vfs, &err);
    if (!rig) return c;
    static blunted::Database db;
    if (!db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql")) return c;
    SetDB(&db);
    MatchSetup setup;
    setup.homeTeamDbId = kHomeTeam;
    setup.awayTeamDbId = kAwayTeam;
    setup.seed = kSeed;
    setup.durationMinutes = 5;
    setup.animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
    setup.rig = &*rig;
    if (!pestest::FillMissingPesConstants(&setup)) return c;
    Properties config;
    config.SetBool("pes_loop", true);
    Match match(new MatchData(kHomeTeam, kAwayTeam), setup, config);
    c.ready = true;
    match.StartPlay();
    pes::PesWorld& world = match.PesWorldOf();
    for (int t = 0; t < kMaxTicks && static_cast<int>(c.rows[0].size()) < kCalls; ++t) {
      match.Process();
      if (pes::LoadU32(world.MatchInfo(), pes::kMiPlayStateOffset) != pes::kMiPlayStateLive) continue;
      if (Player* taker = match.GetRefereeBuffer().taker) c.takerSlot = match.PesSlotOf(taker) % pes::kSlotsPerSide;
      for (int side = 0; side < 2; ++side) {
        const uint8_t* t = world.SideRecord(side);
        if (c.rows[side].empty() && t[pes::kSideLatchOffset] == 1) c.kicker = side;
        Row row;
        row.passTarget = pes::LoadU32(t, pes::kSidePassTargetOffset);
        row.second = pes::LoadU32(t, pes::kTeamSecondOffset);
        for (int k = 0; k < pes::kSlotsPerSide; ++k)
          row.words[k] = pes::LoadU32(t, pes::kTeamActionOffset + sizeof(uint32_t) * k);
        c.rows[side].push_back(row);
      }
    }
    for (size_t i = 0; i < c.rows[0].size(); ++i) {
      for (int side = 0; side < 2; ++side) {  // the capture order: one call per side and frame
        const Row& r = c.rows[side][i];
        std::printf("[press-kickoff] side=%d kicker=%d pass=%u second=%u words=", side, side == c.kicker ? 1 : 0, r.passTarget, r.second);
        for (uint32_t w : r.words) std::printf(" %02x", w);
        std::printf("\n");
      }
    }
    return c;
  }();
  return census;
}

#define REQUIRE_KICKOFF()                                                  \
  const Census& c = RunKickoff();                                          \
  if (!c.ready) GTEST_SKIP() << "PES21_ROOT not set or the install's constants are unavailable"; \
  ASSERT_GE(c.kicker, 0) << "no side held the possession latch at the kick-off"; \
  ASSERT_EQ(static_cast<int>(c.rows[0].size()), kCalls) << "the kick-off did not reach play"

// PES: the kicking side marks no one in its first 12 in-play ticks (all eleven action words zero).
TEST(PesPressKickoff, TheKickingSideSetsNoActionWord) {
  REQUIRE_KICKOFF();
  for (int i = 0; i < kCalls; ++i)
    for (int k = 0; k < pes::kSlotsPerSide; ++k)
      EXPECT_EQ(c.rows[c.kicker][i].words[k], 0u) << "tick " << i << ", the kicking side's man " << k << " holds an action word";
}

// PES: the defending side's T+0x78 stays 0xFF: only the kicking side designates (the kick-off taker).
TEST(PesPressKickoff, TheDefendingSidesPassTargetStaysEmpty) {
  REQUIRE_KICKOFF();
  for (int i = 0; i < kCalls; ++i)
    EXPECT_EQ(c.rows[1 - c.kicker][i].passTarget, pes::kNoPassTarget) << "tick " << i;
}

// PES: the kicking side's T+0x78 is the kick-off taker from its second call on (255 at the first).
TEST(PesPressKickoff, TheKickingSidesPassTargetIsTheTaker) {
  REQUIRE_KICKOFF();
  ASSERT_GE(c.takerSlot, 0);
  const uint32_t taker = static_cast<uint32_t>(c.kicker * pes::kSlotsPerSide + c.takerSlot);
  EXPECT_EQ(c.rows[c.kicker].back().passTarget, taker);
}

// PES: the defending side sets action words on its men while the other side kicks off (5-6 of its 11 in the captures).
TEST(PesPressKickoff, TheDefendingSideSetsActionWords) {
  REQUIRE_KICKOFF();
  int set = 0;
  for (int k = 0; k < pes::kSlotsPerSide; ++k) set += c.rows[1 - c.kicker].back().words[k] != 0;
  EXPECT_GE(set, 1) << "the defending side marks nobody";
}

}  // namespace
