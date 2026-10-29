// PES's foul judge (0x1405B7470, PesWorld::DecideJudge) run on the world window: a contact is staged in PES's records
// (the victim's and the fouler's anime records, the fouler's slot record, the man records, the ball, the match-info
// frame and the period record) and the answer is the pending foul the judge builds.
// Every case follows the decompile of 0x1405B7470 and its helpers (docs/re/pes-re-duels-fouls.md 2.5) and is one the
// retired GF hand port (JudgePoints / JudgeLevel) decided differently: it had no forced "face" card, no deliberate-foul
// minimum and no "the fouler never played the ball" rule. Levels are PES's: 1 foul, 2 foul + warning, 3 yellow, 4 red.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "onthepitch/pesclock.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace {

using namespace pes;

constexpr uint32_t kLevelYellow = 3;
constexpr uint32_t kFirstHalfPeriod = 1;
constexpr uint32_t kLateFrame = 5000;       // far past the one-third-second window since any ball event
constexpr float kHalfLength_m = 52.56f;     // pitch + line half-width, as in the sanction test
constexpr float kHalfWidth_m = 34.06f;
constexpr int kVictim = 12;                 // team 1 attacks +x
constexpr int kFouler = 3;                  // team 0 attacks -x

struct Staged {
  PesWorld world;

  Staged() {
    for (int slot = 0; slot < kSlotCount; ++slot) world.BindSlot(slot);
    world.SetFieldInfo(kHalfLength_m, kHalfWidth_m);
    world.SetAttackSign(0, -1);
    world.SetAttackSign(1, 1);
    world.Period().period = kFirstHalfPeriod;
    StoreStamp(world.Period(), 0.0f);
    StoreU32(world.MatchInfo(), kMiFrameOffset, kLateFrame);
    // Midfield, the fouler two metres from the victim; the ball well away from both.
    Place(kVictim, 0.0f, 0.0f);
    Place(kFouler, -2.0f, 0.0f);
    world.SetBall(30.0f, 20.0f);
  }

  void Place(int slot, float x, float z) {
    uint8_t* man = world.ManRecord(slot);
    StoreF32(man, kManPosXOffset, x);
    StoreF32(man, kManPosZOffset, z);
  }
  uint8_t* Anime(int slot) { return world.Block(slot); }
  void SetFlag(int slot, int bit) {
    uint64_t flags = 0;
    std::memcpy(&flags, Anime(slot) + kAnimeFlagsOffset, sizeof flags);
    flags |= uint64_t{1} << bit;
    std::memcpy(Anime(slot) + kAnimeFlagsOffset, &flags, sizeof flags);
  }
  // The fouler tackles and has played the ball (0x140A63DA0: a challenge with the played-ball flag).
  void CleanTackle() {
    Anime(kFouler)[kAnimeActionOffset] = kActionTackle;
    SetFlag(kFouler, kFlagPlayedBall);
  }
  void VictimFalls() { Anime(kVictim)[kAnimeActionOffset] = kActionFall; }
  PesWorld::JudgeVerdict Decide() { return world.DecideJudge(kVictim, kFouler); }
};

// 0x1405B7470 step 2 (docs 2.5): a fouler who did not play the ball is a foul only when his contact is aimed at the victim
// (ai(B)+0x14A0 == A, the FORCE foul). A tackle on a man who went down, with the ball nowhere near the tackler, is no foul.
TEST(JudgeDecide, ATackleThatNeverPlayedTheBallAndIsNotAimedAtTheManIsNoFoul) {
  Staged s;
  s.Anime(kFouler)[kAnimeActionOffset] = kActionTackle;  // no played-ball flag
  s.VictimFalls();
  EXPECT_FALSE(s.Decide().pending);
}

// Step 1: the victim's face flag (bit 29) outside the box is a forced yellow, whatever the contact scores.
TEST(JudgeDecide, TheFaceFlagOutsideTheBoxIsAYellow) {
  Staged s;
  s.CleanTackle();
  s.SetFlag(kVictim, kFlagFace);
  const PesWorld::JudgeVerdict v = s.Decide();
  ASSERT_TRUE(v.pending);
  EXPECT_EQ(v.level, kLevelYellow);
}

// Step 5: a deliberate foul (ai(B)+0x134) on a man who went down, with no ball event of his own for a third of a second
// and the ball away from him, is at least a yellow however few points the contact scored.
TEST(JudgeDecide, AProfessionalFoulIsAtLeastAYellow) {
  Staged s;
  s.CleanTackle();
  s.VictimFalls();
  s.world.SlotRecord(kFouler)[kSlotIntentOffset] = 1;
  const PesWorld::JudgeVerdict v = s.Decide();
  ASSERT_TRUE(v.pending);
  EXPECT_GE(v.level, kLevelYellow);
}

// The same contact without the intent flag is below a yellow: the minimum is the intent's own doing.
TEST(JudgeDecide, TheSameTackleWithoutIntentIsBelowAYellow) {
  Staged s;
  s.CleanTackle();
  s.VictimFalls();
  const PesWorld::JudgeVerdict v = s.Decide();
  ASSERT_TRUE(v.pending);
  EXPECT_LT(v.level, kLevelYellow);
}

}  // namespace
