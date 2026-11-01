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

// ---- Ball events, staged through the match's own event store (A1+0xE0 names it; 0x140A3E190 reads it). ----
// The judge reads two things from it: the newest event E0 (the copy at E+0x180698) and the one before it E1 (ring 0, head - 1),
// and for each the kind (payload+0), the maker (payload+4: {side, squad place}, resolved through the roster 0x140A62EE0, 0xFF
// for a key 0x140A60530 refuses) and the frame (payload+0x10). `Route` is what the match's event stage calls: a kind-7 record
// is pushed into ring 0 and copied to the newest slot, so staging oldest first leaves E0 = the last one routed.
constexpr uint32_t kRouteKind = 7;      // the routing kind of a ball event (the record's +0x00; pesevents.hpp Route)
constexpr uint32_t kEventKick = 1;      // the switch's cases 1..4 (1 ground, 2 through, 3 lofted, 4)
constexpr uint32_t kEventShot = 5;      // case 5
constexpr uint32_t kEventTouchA = 6;    // cases 6 and 7
constexpr uint32_t kEventTouchB = 7;
constexpr uint32_t kEventIgnored = 9;   // the jump table's default: no case reads it
constexpr uint32_t kEventsThatLookBack[] = {8, 10, 11, 12, 13, 14};  // the case that reads E1
constexpr uint32_t kStale = kLateFrame;  // an event this many frames old is far outside the judge's third-of-a-second window
constexpr int kNoActor = -1;
constexpr int kFoulerMate = 5;   // team 0, not the keeper
constexpr int kVictimMate = 14;  // team 1

struct BallEvent {
  uint32_t kind;
  int maker;  // a slot, or kNoActor
  uint32_t age = 0;
};

// `newestFirst`: E0, E1, ...
void StageEvents(Staged& s, std::initializer_list<BallEvent> newestFirst) {
  const EventRingTable allToRingZero{};
  const BallEvent* events = newestFirst.begin();
  for (std::size_t i = newestFirst.size(); i-- > 0;) {
    const BallEvent& e = events[i];
    EventRecord rec;
    rec.kind = kRouteKind;
    rec.SetActionKind(e.kind);
    rec.SetU32(0x18, e.maker == kNoActor ? EventRecord::kNoActorTeam : static_cast<uint32_t>(e.maker / kSlotsPerSide));
    rec.SetU32(0x1c, e.maker == kNoActor ? EventRecord::kNoActorSlot : static_cast<uint32_t>(e.maker % kSlotsPerSide));
    rec.SetActionFrame(static_cast<int32_t>(kLateFrame - e.age));
    s.world.Events().Route(allToRingZero, rec);
  }
}

// B is his side's keeper: 0x140A62690 names the side's first man with an on-pitch byte (MI+0x1748+slot, 0x140A62610) whose unit
// word (side record +0x4C00 + 4 * place) is 0. The staged world has no man on the pitch, so marking B alone makes him the first.
constexpr std::size_t kMiOnPitchOffset = 0x1748;
void MakeKeeper(Staged& s, int slot) { s.world.MatchInfo()[kMiOnPitchOffset + slot] = 1; }

// The verdict's level for one contact, 0 when no foul is pending; `keeper` is the slot made his side's keeper, -1 for none.
uint32_t Level(const PesWorld::ContactFacts& facts, std::initializer_list<BallEvent> newestFirst, int keeper = -1) {
  Staged s;
  if (keeper >= 0) MakeKeeper(s, keeper);
  StageEvents(s, newestFirst);
  const PesWorld::JudgeVerdict v = s.world.DecideContact(facts);
  return v.pending ? v.level : 0;
}

PesWorld::ContactFacts Block() {
  PesWorld::ContactFacts facts = Staged::Tackle();
  facts.foulerAction = kActionBlock;
  return facts;
}

// A block that scores a foul by itself: aimed at a man who went down, from directly behind (ai(B)+0x12A0 = 180).
constexpr float kFromBehind_deg = 180.0f;
PesWorld::ContactFacts BlockFromBehind() {
  PesWorld::ContactFacts facts = Block();
  facts.targeted = true;
  facts.approachAngle_deg = kFromBehind_deg;
  return facts;
}

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

// The FORCE foul is cancelled like any other by the victim's own shot (case 5): the store decides, so it is read.
TEST(JudgeDecide, ANoBallContactAimedAtTheManIsAForceFoul) {
  Staged s;
  PesWorld::ContactFacts facts = Staged::Tackle(false);
  facts.foulerAction = 0;
  facts.targeted = true;
  const PesWorld::JudgeVerdict v = s.world.DecideContact(facts);
  ASSERT_TRUE(v.pending);
  EXPECT_EQ(v.level, kLevelFoul);
  EXPECT_EQ(v.type, kForceFoulType);
  EXPECT_EQ(Level(facts, {{kEventShot, kVictim}}), 0u);
  EXPECT_EQ(Level(facts, {{kEventShot, kFouler}}), kLevelFoul);
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
// and the ball away from him, is at least a yellow however few points the contact scored; a ball event of his own inside the
// window (read from the store) takes the minimum away.
TEST(JudgeDecide, AProfessionalFoulIsAtLeastAYellow) {
  Staged s;
  PesWorld::ContactFacts facts = Staged::Tackle();
  facts.intent = true;
  const PesWorld::JudgeVerdict v = s.world.DecideContact(facts);
  ASSERT_TRUE(v.pending);
  EXPECT_GE(v.level, kLevelYellow);
  EXPECT_LT(Level(facts, {{kEventKick, kVictim}}), kLevelYellow);
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

// ---- The newest event E0 (0x1405B7470 step 3 switches on its kind). ----
// The judge is PES's translated 0x1405B7470; the cases below are its jump table (0x1405B7F6C): kinds 1..4, 5, 6..7 and
// {8, 10..14} have a case, 9 and the rest fall to the default. A, the victim, is the man on the ground; B, the fouler.

// Cases 1..4 with B's action 0xE (a block): the block alone scores no foul, but on a man whose own kick is the
// newest ball event it is at least a foul (0x1405B7470 step 5, else-if "a contact on a man who just played the ball").
// The same event made by another man changes nothing.
TEST(JudgeDecide, ABlockOnAManWhoJustKickedIsAFoul) {
  ASSERT_EQ(Level(Block(), {}), 0u);
  for (uint32_t kind = kEventKick; kind < kEventKick + 4; ++kind) {
    EXPECT_EQ(Level(Block(), {{kind, kVictim}}), kLevelFoul) << "event kind " << kind;
    EXPECT_EQ(Level(Block(), {{kind, kFouler}}), 0u) << "event kind " << kind;
  }
}

// Case 5: the victim's own shot clears the foul a contact scored (the same tackle is pending without it, see
// TheSameTackleWithoutIntentIsBelowAYellow), and another man's shot does not.
TEST(JudgeDecide, TheVictimsOwnShotClearsAFoul) {
  EXPECT_EQ(Level(Staged::Tackle(), {{kEventShot, kVictim}}), 0u);
  EXPECT_EQ(Level(Staged::Tackle(), {{kEventShot, kFouler}}), kLevelFoul);
}

// Cases 1..5 when B is his side's keeper (bVar24 in the decompile): whatever kick or shot is newest, a keeper's contact is play
// on, except on a man whose own kick it was (the else-if above raises it to a foul, a shot of the victim's clears it).
TEST(JudgeDecide, AKeepersContactAfterAKickOrAShotIsPlayOn) {
  ASSERT_EQ(Level(Staged::Tackle(), {}, kFouler), kLevelFoul);
  for (uint32_t kind = kEventKick; kind <= kEventShot; ++kind) {
    for (const int maker : {kFouler, kFoulerMate, kVictimMate})
      EXPECT_EQ(Level(Staged::Tackle(), {{kind, maker}}, kFouler), 0u) << "kind " << kind << " maker " << maker;
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kVictim}}, kFouler), kind == kEventShot ? 0u : kLevelFoul) << "kind " << kind;
    // The same event for a fouler who is no keeper cancels nothing.
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}}), kLevelFoul) << "kind " << kind;
  }
}

// Cases 6 and 7: a keeper's contact is play on when the newest event's maker is on the keeper's own side (the side of the
// maker, 0 for an invalid key, against the side of B): a touch by his own side, by himself or by no one (side 0). A man of the
// victim's side does not cancel it, and neither does it cancel a fouler who is no keeper. The kind next to them (9) is no case.
TEST(JudgeDecide, AKeepersContactAfterATouchOfHisOwnSideIsPlayOn) {
  for (const uint32_t kind : {kEventTouchA, kEventTouchB}) {
    for (const int maker : {kFouler, kFoulerMate, kNoActor})
      EXPECT_EQ(Level(Staged::Tackle(), {{kind, maker}}, kFouler), 0u) << "kind " << kind << " maker " << maker;
    for (const int maker : {kVictim, kVictimMate})
      EXPECT_EQ(Level(Staged::Tackle(), {{kind, maker}}, kFouler), kLevelFoul) << "kind " << kind << " maker " << maker;
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}}), kLevelFoul) << "kind " << kind;
  }
  EXPECT_EQ(Level(Staged::Tackle(), {{kEventIgnored, kFouler}}, kFouler), kLevelFoul);
}

// An invalid maker key is side 0 whoever is fouling (0x1405B7470 computes the side as `0 unless 11 <= slot < 22`): the same
// keeper touch with no maker cancels a keeper of side 0 and leaves a keeper of side 1 called.
TEST(JudgeDecide, AnEventWithNoMakerCountsForSideZero) {
  EXPECT_EQ(Level(Staged::Tackle(), {{kEventTouchA, kNoActor}}, kFouler), 0u);
  constexpr int kVictimOfZero = 1, kKeeperOfOne = 14;  // the mirror of the staged pair: side 0 on the ground, side 1 fouling
  Staged s;
  s.Place(kVictimOfZero, 0.0f, 0.0f);
  s.Place(kKeeperOfOne, 2.0f, 0.0f);
  MakeKeeper(s, kKeeperOfOne);
  PesWorld::ContactFacts facts = Staged::Tackle();
  facts.victim = kVictimOfZero;
  facts.fouler = kKeeperOfOne;
  const PesWorld::JudgeVerdict before = s.world.DecideContact(facts);
  ASSERT_TRUE(before.pending);
  StageEvents(s, {{kEventTouchA, kNoActor}});
  const PesWorld::JudgeVerdict after = s.world.DecideContact(facts);
  ASSERT_TRUE(after.pending);
  EXPECT_EQ(after.level, before.level);
}

// ---- Cases {8, 10..14}: they read the event before (E1) as well. ----
// A newest event of one of these kinds leaves the verdict to E1: a shot of the victim's makes it no foul, a kick of his
// (kinds 1..4) is the "he just played the ball" raise, and a keeper's contact after any of them is play on. Any other kind of
// E0 (9, 15) is no case, so the same E1 changes nothing.
TEST(JudgeDecide, TheEventBeforeAVictimsShotClearsAFoulWhateverTheNewestKind) {
  for (const uint32_t kind : kEventsThatLookBack) {
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}, {kEventShot, kVictim}}), 0u) << "E0 kind " << kind;
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}, {kEventShot, kFouler}}), kLevelFoul) << "E0 kind " << kind;
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}, {kEventTouchA, kVictim}}), kLevelFoul) << "E0 kind " << kind;
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}}), kLevelFoul) << "E0 kind " << kind;
  }
  for (const uint32_t kind : {kEventIgnored, 15u})
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}, {kEventShot, kVictim}}), kLevelFoul) << "E0 kind " << kind;
}

TEST(JudgeDecide, TheEventBeforeAVictimsKickMakesABlockAFoulWhateverTheNewestKind) {
  for (const uint32_t kind : kEventsThatLookBack) {
    for (uint32_t before = kEventKick; before < kEventKick + 4; ++before) {
      EXPECT_EQ(Level(Block(), {{kind, kFouler}, {before, kVictim}}), kLevelFoul) << "E0 " << kind << " E1 " << before;
      EXPECT_EQ(Level(Block(), {{kind, kFouler}, {before, kFouler}}), 0u) << "E0 " << kind << " E1 " << before;
    }
    EXPECT_EQ(Level(Block(), {{kind, kFouler}, {kEventShot, kVictim}}), 0u) << "E0 " << kind;
    EXPECT_EQ(Level(Block(), {{kind, kFouler}, {kEventTouchA, kVictim}}), 0u) << "E0 " << kind;
  }
  EXPECT_EQ(Level(Block(), {{kEventIgnored, kFouler}, {kEventKick, kVictim}}), 0u);
}

TEST(JudgeDecide, AKeepersContactIsPlayOnAfterAnyKickOrShotBeforeTheNewestEvent) {
  for (const uint32_t kind : kEventsThatLookBack) {
    for (uint32_t before = kEventKick; before <= kEventShot; ++before)
      EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}, {before, kFouler}}, kFouler), 0u) << "E0 " << kind << " E1 " << before;
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}, {kEventTouchA, kFouler}}, kFouler), kLevelFoul) << "E0 " << kind;
    EXPECT_EQ(Level(Staged::Tackle(), {{kind, kFouler}}, kFouler), kLevelFoul) << "E0 " << kind;
  }
}

// A block aimed at a man on the ground from behind is a foul on its own; when E0 is one of these kinds and its maker is on
// B's own side the judge reads the block as a challenge that played the ball and calls nothing (bVar3: same side as B and
// B's action 0xE or 0x3D). A maker of the victim's side leaves it a foul, and so does a kind that is no case. Action 0x3D
// scores nothing under any staged contact, so only 0xE is driven here.
TEST(JudgeDecide, ABlockAfterAnEventOfTheFoulersSideIsPlayOn) {
  ASSERT_EQ(Level(BlockFromBehind(), {}), kLevelFoul);
  for (const uint32_t kind : kEventsThatLookBack) {
    for (const int maker : {kFouler, kFoulerMate, kNoActor})
      EXPECT_EQ(Level(BlockFromBehind(), {{kind, maker}}), 0u) << "kind " << kind << " maker " << maker;
    for (const int maker : {kVictim, kVictimMate})
      EXPECT_EQ(Level(BlockFromBehind(), {{kind, maker}}), kLevelFoul) << "kind " << kind << " maker " << maker;
  }
  for (const uint32_t kind : {kEventIgnored, kEventTouchA, kEventKick})
    EXPECT_EQ(Level(BlockFromBehind(), {{kind, kFouler}}), kLevelFoul) << "kind " << kind;
}

// ---- The ring walk (step 5): who made a ball event inside the last third of a second. ----
// The walk goes E0, E1, ... and stops at the first event made by the victim (not professional) or the first one older than
// the window (professional: no ball event of his own lately). The kind is not read.
TEST(JudgeDecide, AProfessionalFoulNeedsNoBallEventOfTheVictimsInTheWindow) {
  PesWorld::ContactFacts facts = Staged::Tackle();
  facts.intent = true;
  EXPECT_GE(Level(facts, {{kEventTouchA, kFouler}}), kLevelYellow);
  EXPECT_GE(Level(facts, {{kEventKick, kVictim, kStale}}), kLevelYellow);
  // His own event further back but inside the window still counts; an older man's event behind a stale one does not.
  EXPECT_LT(Level(facts, {{kEventKick, kFouler}, {kEventTouchA, kVictim}}), kLevelYellow);
  EXPECT_LT(Level(facts, {{kEventKick, kFouler}, {kEventKick, kFoulerMate}, {kEventTouchB, kVictim}}), kLevelYellow);
  EXPECT_GE(Level(facts, {{kEventKick, kFouler}, {kEventKick, kFouler, kStale}, {kEventKick, kVictim}}), kLevelYellow);
}

}  // namespace
