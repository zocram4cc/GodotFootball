// PES's foul judge (0x1405B7470, PesWorld::DecideContact) run on the world window: a contact is staged in PES's records
// (the victim's and the fouler's anime records, the fouler's slot record, the man records, the ball, the match-info
// frame and the period record) and the answer is the pending foul the judge builds.
// Every case follows the decompile of 0x1405B7470 and its helpers (docs/re/pes-re-duels-fouls.md 2.5) and is one the
// retired GF hand port (JudgePoints / JudgeLevel) decided differently: it had no forced "face" card, no deliberate-foul
// minimum and no "the fouler never played the ball" rule. Levels are PES's: 1 foul, 2 foul + warning, 3 yellow, 4 red.

#include <gtest/gtest.h>

#include <cstdint>

#include "onthepitch/pesclock.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace {

using namespace pes;

constexpr uint32_t kLevelYellow = 3;
constexpr uint32_t kFirstHalfPeriod = 1;
constexpr uint32_t kLateFrame = 5000;    // far past the one-third-second window since any ball event
constexpr float kHalfLength_m = 52.56f;  // pitch + line half-width, as in the sanction test
constexpr float kHalfWidth_m = 34.06f;
constexpr int kVictim = 12;  // team 1 attacks +x
constexpr int kFouler = 3;   // team 0 attacks -x

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
    for (int j = 0; j < kBallSampleCount; ++j) world.SetBallSample(j, 30.0f, 0.0f, 20.0f);
  }

  void Place(int slot, float x, float z) {
    uint8_t* man = world.ManRecord(slot);
    StoreF32(man, kManPosXOffset, x);
    StoreF32(man, kManPosZOffset, z);
  }

  // A tackle on a man who went down, the fouler having played the ball unless `played` is false.
  static PesWorld::ContactFacts Tackle(bool played = true) {
    PesWorld::ContactFacts facts;
    facts.victim = kVictim;
    facts.fouler = kFouler;
    facts.foulerAction = kActionTackle;
    facts.victimFell = true;
    facts.playedBall = played;
    return facts;
  }
};

// 0x1405B7470 step 2 (docs 2.5): a fouler who did not play the ball can only be called for a FORCE foul. A tackle that
// never played the ball is a challenge whose "no play" 0x140A63DA0 reports through its out flag, so the contact rule
// 0x140A639A0 refuses it and nothing is called, aimed or not (the aimed cases are below, with a no-challenge action).
TEST(JudgeDecide, ATackleThatNeverPlayedTheBallIsNoFoul) {
  Staged s;
  EXPECT_FALSE(s.world.DecideContact(Staged::Tackle(false)).pending);
}

// The same step with a contact the rule 0x140A639A0 lets through (the fouler's action is no challenge, so 0x140A63DA0
// does not swallow it): the FORCE foul is the contact aimed at the man (ai(B)+0x14A0 == A), PES's type 8 at level 1;
// aimed anywhere else it is no foul.
constexpr uint8_t kForceFoulType = 8;  // 0x1405B7470: the sanction type the FORCE foul writes
constexpr uint32_t kLevelFoul = 1;

TEST(JudgeDecide, ANoBallContactAimedAtTheManIsAForceFoul) {
  Staged s;
  PesWorld::ContactFacts facts = Staged::Tackle(false);
  facts.foulerAction = 0;
  facts.targeted = true;
  const PesWorld::JudgeVerdict v = s.world.DecideContact(facts);
  ASSERT_TRUE(v.pending);
  EXPECT_EQ(v.level, kLevelFoul);
  EXPECT_EQ(v.type, kForceFoulType);
}

TEST(JudgeDecide, ANoBallContactNotAimedAtTheManIsNoFoul) {
  Staged s;
  PesWorld::ContactFacts facts = Staged::Tackle(false);
  facts.foulerAction = 0;
  EXPECT_FALSE(s.world.DecideContact(facts).pending);
}

// Step 1: the victim's face flag (bit 29) outside the box is a forced yellow, whatever the contact scores.
TEST(JudgeDecide, TheFaceFlagOutsideTheBoxIsAYellow) {
  Staged s;
  PesWorld::ContactFacts facts = Staged::Tackle();
  facts.victimFace = true;
  const PesWorld::JudgeVerdict v = s.world.DecideContact(facts);
  ASSERT_TRUE(v.pending);
  EXPECT_EQ(v.level, kLevelYellow);
}

// Step 5: a deliberate foul (ai(B)+0x134) on a man who went down, with no ball event of his own for a third of a second
// and the ball away from him, is at least a yellow however few points the contact scored.
TEST(JudgeDecide, AProfessionalFoulIsAtLeastAYellow) {
  Staged s;
  PesWorld::ContactFacts facts = Staged::Tackle();
  facts.intent = true;
  const PesWorld::JudgeVerdict v = s.world.DecideContact(facts);
  ASSERT_TRUE(v.pending);
  EXPECT_GE(v.level, kLevelYellow);
}

// The same contact without the intent flag is below a yellow: the minimum is the intent's own doing.
TEST(JudgeDecide, TheSameTackleWithoutIntentIsBelowAYellow) {
  Staged s;
  const PesWorld::JudgeVerdict v = s.world.DecideContact(Staged::Tackle());
  ASSERT_TRUE(v.pending);
  EXPECT_LT(v.level, kLevelYellow);
}

// The anime records' action byte also carries GF's keeper-class stand-in (SetAnimeClass): a decision puts it back.
TEST(JudgeDecide, TheStagedActionBytesAreRestored) {
  Staged s;
  s.world.SetAnimeClass(kFouler, 3);
  s.world.SetAnimeClass(kVictim, 0);
  s.world.DecideContact(Staged::Tackle());
  EXPECT_EQ(s.world.AnimeClass(kFouler), 3);
  EXPECT_EQ(s.world.AnimeClass(kVictim), 0);
}

// ---- The ball-event store the judge reads through A1+0xE0 (0x140A3E190 on the match's own event store). ----
// 0x1405B7470 step 3 switches on the newest ball event's kind (E+0x180698, payload+0) and names its maker through the
// roster (payload+4: {team, squad place}, 0x140A62EE0). GF's match writes that store; the judge reads it, so a staged event
// must change the verdict, and an event of the same kind by someone else must not.
constexpr uint32_t kEventShortKick = 1;  // the switch's cases 1..4
constexpr uint32_t kEventShot = 5;       // case 5

void StageNewestEvent(Staged& s, uint32_t kind, int slot) {
  uint8_t* payload = s.world.EventImage().newest.data();
  StoreU32(payload, 0, kind);
  StoreU32(payload, 4, static_cast<uint32_t>(slot / kSlotsPerSide));  // the maker's team
  StoreU32(payload, 8, static_cast<uint32_t>(slot % kSlotsPerSide));  // and squad place (the identity roster BindSanction stages)
}

PesWorld::ContactFacts Block() {
  PesWorld::ContactFacts facts = Staged::Tackle();
  facts.foulerAction = kActionBlock;
  return facts;
}

// Cases 1..4 of the switch with B's action 0xE (a block): the block alone scores no foul, but on a man whose own kick is the
// newest ball event it is at least a foul (0x1405B7470 step 5, else-if "a contact on a man who just played the ball").
// The same event made by another man changes nothing.
TEST(JudgeDecide, ABlockOnAManWhoJustKickedIsAFoul) {
  Staged none;
  ASSERT_FALSE(none.world.DecideContact(Block()).pending);
  for (uint32_t kind = kEventShortKick; kind < kEventShortKick + 4; ++kind) {
    Staged mine;
    StageNewestEvent(mine, kind, kVictim);
    const PesWorld::JudgeVerdict v = mine.world.DecideContact(Block());
    ASSERT_TRUE(v.pending) << "event kind " << kind;
    EXPECT_EQ(v.level, kLevelFoul) << "event kind " << kind;
    Staged theirs;
    StageNewestEvent(theirs, kind, kFouler);
    EXPECT_FALSE(theirs.world.DecideContact(Block()).pending) << "event kind " << kind;
  }
}

// Case 5: the victim's own shot clears the foul a contact scored (the same tackle is pending without it, see
// TheSameTackleWithoutIntentIsBelowAYellow), and another man's shot does not.
TEST(JudgeDecide, TheVictimsOwnShotClearsAFoul) {
  Staged shot;
  StageNewestEvent(shot, kEventShot, kVictim);
  EXPECT_FALSE(shot.world.DecideContact(Staged::Tackle()).pending);
  Staged other;
  StageNewestEvent(other, kEventShot, kFouler);
  EXPECT_TRUE(other.world.DecideContact(Staged::Tackle()).pending);
}

}  // namespace
