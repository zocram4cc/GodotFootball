// PES's sanction tree (0x14047F170, PesWorld::DecideSanction) run on the world window: a foul stop is staged in PES's
// records (match-info block, foul event ring, team info, field info) and the answer is the scene id the tree returns.
// Scene ids are the tree's own immediates (docs/ports/pes-port-FoulSanction.md 3). Four cases mirror recorded PES calls
// (snapdiff replays of captures live/seq084, 086, 090 and live2/seq109: the facts staged below are what the capture's
// records held, the expected scene is what PES's bytes returned); the rest follow the decompile's branches.

#include <gtest/gtest.h>

#include <cstdint>

#include "onthepitch/pesworldrecord.hpp"

namespace {

using namespace pes;

constexpr uint32_t kSceneNone = 0x20022;            // the tree's "no scene" (its default return)
constexpr uint32_t kSceneFirstYellow = 0x20001;     // yellow, 0x14047F8xx group
constexpr uint32_t kSceneSecondYellow = 0x20004;    // yellow on a player who already holds one
constexpr uint32_t kSceneRedFirst = 0x20003;        // red, no prior booking
constexpr uint32_t kSceneRedBooked = 0x20005;       // red on a player with one yellow
constexpr uint32_t kSceneWarning = 0x20008;         // foul + warning
constexpr uint32_t kSceneWarningBooked = 0x20006;   // foul + warning on a booked player
constexpr uint32_t kSceneDeepFoulFoul = 0x2000B;    // a foul deep in the fouler's own half
constexpr uint32_t kSceneDeepFoulYellow = 0x20009;  // a yellow deep in the fouler's own half

constexpr uint32_t kFreeKick = 5;
constexpr uint32_t kStoppedPlayState = 2;
constexpr uint32_t kStopReasonFoul = 5;
constexpr uint32_t kDirectFoulKind = 2;
constexpr uint32_t kFirstHalfPeriod = 1;
constexpr float kHalfLength_m = 52.56f;  // the field info the captures held (pitch + line half-width)
constexpr float kHalfWidth_m = 34.06f;

struct Stop {
  uint32_t level;     // 1 foul, 2 warning, 3 yellow, 4 red
  int fouler;         // slot 0..21
  int victim;         // slot 0..21
  int foulerYellows;  // his count, the card being decided included
  float spotX;
  float spotZ;
  uint32_t restartSide;  // the fouled side
};

// PES's sides: team 0 attacks -x in the captures (attack sign -1), team 1 +x.
void Stage(PesWorld& world, const Stop& stop) {
  world.SetAttackSign(0, -1);
  world.SetAttackSign(1, 1);
  world.SetFieldInfo(kHalfLength_m, kHalfWidth_m);
  for (int slot = 0; slot < kSlotCount; ++slot) world.SetPlayerInfo(slot, 0x1000u + slot, 0);
  world.SetPlayerInfo(stop.fouler, 0x1000u + stop.fouler, stop.foulerYellows);
  world.Period().period = kFirstHalfPeriod;
  world.Period().playState = kStoppedPlayState;
  uint8_t* mi = world.MatchInfo();
  StoreU32(mi, kMiRestartKindOffset, kFreeKick);
  StoreU32(mi, kMiRestartSideOffset, stop.restartSide);
  StoreF32(mi, kMiSpotXOffset, stop.spotX);
  StoreF32(mi, kMiSpotZOffset, stop.spotZ);
  StoreU32(mi, kMiStopReasonOffset, kStopReasonFoul);
  StoreU32(mi, kMiFoulKindOffset, kDirectFoulKind);
  StoreU32(mi, kMiCardLevelOffset, stop.level);
  StoreU32(mi, kMiFoulerOffset, static_cast<uint32_t>(stop.fouler));
  StoreU32(mi, kMiVictimOffset, static_cast<uint32_t>(stop.victim));
  PesWorld::FoulEvent event;
  event.periodState = kFirstHalfPeriod;
  event.foulKind = kDirectFoulKind;
  event.cardLevel = stop.level;
  event.side = stop.restartSide;
  world.PushFoulEvent(event);
}

uint32_t Decide(const Stop& stop) {
  PesWorld world;
  Stage(world, stop);
  return world.DecideSanction();
}

// Recorded: live/seq084 (level 1, a foul by team 0's slot 8 on team 1's slot 15 in the fouler's own half).
TEST(SanctionTree, APlainFoulInTheOpenHasNoScene) {
  EXPECT_EQ(Decide({1, 8, 15, 0, -51.7081f, -11.0806f, 1}), kSceneNone);
}

// Recorded: live2/seq109 (level 3, team 1's slot 4 books his first yellow near the centre).
TEST(SanctionTree, AFirstYellowPlaysTheFirstYellowScene) {
  EXPECT_EQ(Decide({3, 15, 9, 1, 4.8629f, 4.2794f, 0}), kSceneFirstYellow);
}

// Recorded: live/seq086 (level 1 at x = 31.008: just past the 31 m line in the fouler's own half).
TEST(SanctionTree, AFoulJustPastThirtyOneMetresInTheFoulersHalfHasItsOwnScene) {
  EXPECT_EQ(Decide({1, 2, 20, 0, 31.0077f, -1.1598f, 1}), kSceneDeepFoulFoul);
}

// Boundary: the same foul 0.01 m inside the line has no scene (the tree compares |x| <= 31.0).
TEST(SanctionTree, TheSameFoulJustInsideThirtyOneMetresHasNoScene) {
  EXPECT_EQ(Decide({1, 2, 20, 0, 30.99f, -1.1598f, 1}), kSceneNone);
}

// Recorded: live/seq090 (level 3, a yellow at x = 34.6 in the fouler's own half).
TEST(SanctionTree, AYellowDeepInTheFoulersHalfPlaysItsOwnScene) {
  EXPECT_EQ(Decide({3, 4, 20, 1, 34.6039f, 19.3707f, 1}), kSceneDeepFoulYellow);
}

// GF folded PES's level 2 into a plain foul; PES plays a warning scene, another for a player already booked.
TEST(SanctionTree, AWarningHasItsOwnScene) { EXPECT_EQ(Decide({2, 8, 15, 0, 10.0f, 5.0f, 1}), kSceneWarning); }

TEST(SanctionTree, AWarningOnABookedPlayerHasADifferentScene) {
  EXPECT_EQ(Decide({2, 8, 15, 1, 10.0f, 5.0f, 1}), kSceneWarningBooked);
}

TEST(SanctionTree, ASecondYellowPlaysTheSecondYellowScene) {
  EXPECT_EQ(Decide({3, 15, 9, 2, 4.8629f, 4.2794f, 0}), kSceneSecondYellow);
}

TEST(SanctionTree, ARedPlaysTheRedSceneAndABookedPlayersRedAnother) {
  EXPECT_EQ(Decide({4, 15, 9, 0, 4.8629f, 4.2794f, 0}), kSceneRedFirst);
  EXPECT_EQ(Decide({4, 15, 9, 1, 4.8629f, 4.2794f, 0}), kSceneRedBooked);
}

// The tree stores its answer in the request at +0, where the cutscene request reads it.
TEST(SanctionTree, TheAnswerIsStoredInTheRequest) {
  PesWorld world;
  Stage(world, {3, 15, 9, 1, 4.8629f, 4.2794f, 0});
  const uint32_t answer = world.DecideSanction();
  EXPECT_EQ(answer, world.SanctionScene());
}

}  // namespace
