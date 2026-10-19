// PES 2021's per-tick team record words (Slice A): the attack level T+0xB8A4,
// its flags T+0xB8A2/B8A3, and the restart word T+0x80 (team-layer §1 steps
// 13/18/28, §2 V1; tactics §3). Pure behaviour tests: no install needed.
#include <gtest/gtest.h>

#include "onthepitch/pesteamtick.hpp"

namespace {

// A1: the inline rule at 0x1404F6E1C-58: B8A2 = (L >= 4), B8A3 = (L <= 0).
TEST(PesTeamTick, Level3SetsFullAttack) {
  pes::TeamTick tick;
  tick.b8a4 = 4;
  pes::WriteAttackFlags(&tick);
  EXPECT_TRUE(tick.b8a2);
  EXPECT_FALSE(tick.b8a3);
  tick.b8a4 = 3;
  pes::WriteAttackFlags(&tick);
  EXPECT_FALSE(tick.b8a2);
  EXPECT_FALSE(tick.b8a3);
}

TEST(PesTeamTick, Level0SetsFullDefence) {
  pes::TeamTick tick;
  tick.b8a4 = 0;
  pes::WriteAttackFlags(&tick);
  EXPECT_FALSE(tick.b8a2);
  EXPECT_TRUE(tick.b8a3);
  tick.b8a4 = 1;
  pes::WriteAttackFlags(&tick);
  EXPECT_FALSE(tick.b8a2);
  EXPECT_FALSE(tick.b8a3);
}

// A2: step 28 at 0x1404F6F91: T+0x80 = 0xFF when MI+0x116C != 1 (dead ball)
// or the latch flipped (FUN_1404FE9C0 returns old^new 0x255).
TEST(PesTeamTick, RestartWordSetWhileDeadBall) {
  pes::TeamTick tick;
  pes::WriteRestartWord(&tick, /*inPlay=*/false, /*possessionFlipped=*/false);
  EXPECT_EQ(tick.t80, 0xFFu);
}

TEST(PesTeamTick, RestartWordSetOnPossessionFlip) {
  pes::TeamTick tick;
  pes::WriteRestartWord(&tick, /*inPlay=*/true, /*possessionFlipped=*/true);
  EXPECT_EQ(tick.t80, 0xFFu);
}

TEST(PesTeamTick, RestartWordKeptInPlay) {
  pes::TeamTick tick;
  pes::WriteRestartWord(&tick, /*inPlay=*/true, /*possessionFlipped=*/false);
  EXPECT_EQ(tick.t80, 0u);
}

// A3: the drift gates of FUN_1404F8350 (ladder 0x1404F8619-63E, mesh
// 0x1404F8640-6C8, freeze 0x1404F86C8-6D4, clamp 0x1404F86DE-E7).
TEST(PesTeamTick, LevelFrozenWhileDeadBall) {
  pes::TeamTick tick;
  tick.b8a4 = 2;
  // Strategy 3 (attacking) wants 3, but the ball is dead: the level holds.
  pes::DriftAttackLevel(&tick, /*strategy=*/3, /*manual=*/2, /*inPlay=*/false,
                        /*frame=*/100);
  EXPECT_EQ(tick.b8a4, 2);
  // Same tick in play: it follows.
  pes::DriftAttackLevel(&tick, /*strategy=*/3, /*manual=*/2, /*inPlay=*/true,
                        /*frame=*/101);
  EXPECT_EQ(tick.b8a4, 3);
  EXPECT_EQ(tick.b8c0Frame, 101);
}

TEST(PesTeamTick, StrategyMapCoversTheRest) {
  pes::TeamTick tick;
  // Anything outside 0/1/3/4 wants 2 (the ladder default); 4 stays 4.
  EXPECT_EQ(pes::StrategyWantLevel(2), 2);
  EXPECT_EQ(pes::StrategyWantLevel(7), 2);
  EXPECT_EQ(pes::StrategyWantLevel(-1), 2);
  EXPECT_EQ(pes::StrategyWantLevel(4), 4);
  EXPECT_EQ(pes::StrategyWantLevel(0), 0);
  tick.b8a4 = 3;
  pes::DriftAttackLevel(&tick, /*strategy=*/7, /*manual=*/2, /*inPlay=*/true,
                        /*frame=*/50);
  EXPECT_EQ(tick.b8a4, 2);
}
TEST(PesTeamTick, ManualOverridePullsLevel) {
  // T+0x210 = 1 below a want of 2 (t < 2, t <= want < 3): want becomes 1.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/2, /*manual=*/1, /*level=*/2), 1);
  // T+0x210 = 3 above a want of 2 (t > 2, 2 <= want <= t): want becomes 3.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/2, /*manual=*/3, /*level=*/2), 3);
  // Balanced manual never overrides.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/3, /*manual=*/2, /*level=*/2), 3);
  // Out of range either way: no override.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/3, /*manual=*/1, /*level=*/2), 3);
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/1, /*manual=*/3, /*level=*/2), 1);
  // In range but already there (0x1404F8663 CMP ESI,EBP; JZ): the strategy
  // want stands, the manual word does not hold the level.
  EXPECT_EQ(pes::ManualOverrideLevel(/*want=*/2, /*manual=*/1, /*level=*/1), 2);
  // End to end: the override moves the stored level in play.
  pes::TeamTick tick;
  tick.b8a4 = 2;
  pes::DriftAttackLevel(&tick, /*strategy=*/2, /*manual=*/3, /*inPlay=*/true,
                        /*frame=*/60);
  EXPECT_EQ(tick.b8a4, 3);
}

}  // namespace
