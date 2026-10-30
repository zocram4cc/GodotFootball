// PES's officials' involvement (0x140A3A690, PesWorld::Involved) run on the world window. A stop is written into the
// match-info block as PES reads it, and the answer is whether an official is involved in it. PES's label helper
// 0x141E53520 gives an official index minus 22 when that is 0..2 and 4 otherwise (replay/pes_fn_141E53520.cpp), so:
// the referee (22) is involved in a stop whatever the distance. An assistant (23, 24) is involved when he is on the spot's
// half (same X sign) and within 14 m of the spot across the touchline (|spot z - his z| < 14, same Z sign): PES's stopped
// branch compares the Z offset only, so distance along the goal line is not an input (impure_sample/pes_fn_140A3A500.cpp).

#include <gtest/gtest.h>

#include <cstdint>

#include "onthepitch/pesworldrecord.hpp"

namespace {

using namespace pes;

constexpr uint32_t kFreeKick = 5;          // restart kind: free kick
constexpr uint32_t kStoppedPlayState = 2;  // play state 2: stopped
constexpr uint32_t kStopReasonFoul = 5;    // stop reason: foul or offside
constexpr uint32_t kDirectFoulKind = 2;    // foul kind: direct foul
constexpr int kTeamZero = 0;
constexpr int kAttackSignPositive = 1;     // team 0 attacks +x
constexpr int kFoulerSlot = 0;
constexpr int kVictimSlot = 1;
constexpr int kAssistantOfficialIndex = 23;  // the label helper's 1: the first assistant

// The official's ground position in metres: his man record's (+0x55C, +0x564).
void PlaceOfficial(PesWorld& world, int official, float x, float z) {
  uint8_t* man = world.ManRecord(official);
  StoreF32(man, kManPosXOffset, x);
  StoreF32(man, kManPosZOffset, z);
}

// A free kick for a direct foul, stopped at (spotX, spotZ) with team 0 awarded it; both officials are bound.
void StageFoulStop(PesWorld& world, float spotX, float spotZ) {
  world.BindOfficial(kRefereeOfficialIndex);
  world.BindOfficial(kAssistantOfficialIndex);
  uint8_t* mi = world.MatchInfo();
  world.Period().playState = kStoppedPlayState;
  StoreU32(mi, kMiRestartKindOffset, kFreeKick);
  StoreU32(mi, kMiRestartSideOffset, kTeamZero);
  StoreF32(mi, kMiSpotXOffset, spotX);
  StoreF32(mi, kMiSpotZOffset, spotZ);
  StoreU32(mi, kMiStopReasonOffset, kStopReasonFoul);
  StoreU32(mi, kMiFoulKindOffset, kDirectFoulKind);
  StoreU32(mi, kMiFoulerOffset, kFoulerSlot);
  StoreU32(mi, kMiVictimOffset, kVictimSlot);
  for (int j = 0; j < pes::kBallSampleCount; ++j) world.SetBallSample(j, spotX, 0.0f, spotZ);  // a ball at rest on the spot
  world.SetAttackSign(kTeamZero, kAttackSignPositive);
}

TEST(OfficialInvolvement, TheRefereeIsInvolvedInAFoulWhateverTheDistance) {
  PesWorld world;
  StageFoulStop(world, -40.0f, 5.0f);
  PlaceOfficial(world, kRefereeOfficialIndex, 10.0f, -20.0f);
  EXPECT_TRUE(world.Involved(kRefereeOfficialIndex));
}

TEST(OfficialInvolvement, AnAssistantFiveMetresFromAFoulOnHisHalfIsInvolved) {
  PesWorld world;
  StageFoulStop(world, -30.0f, 5.0f);
  PlaceOfficial(world, kAssistantOfficialIndex, -25.0f, 5.0f);
  EXPECT_TRUE(world.Involved(kAssistantOfficialIndex));
}

TEST(OfficialInvolvement, AnAssistantThirtyFiveMetresDownTheLineIsStillInvolved) {
  PesWorld world;
  StageFoulStop(world, -40.0f, 5.0f);
  PlaceOfficial(world, kAssistantOfficialIndex, -5.0f, 5.0f);
  EXPECT_TRUE(world.Involved(kAssistantOfficialIndex));
}

TEST(OfficialInvolvement, AnAssistantThirteenMetresAcrossTheTouchlineIsInvolved) {
  PesWorld world;
  StageFoulStop(world, -40.0f, 5.0f);
  PlaceOfficial(world, kAssistantOfficialIndex, -40.0f, 18.0f);
  EXPECT_TRUE(world.Involved(kAssistantOfficialIndex));
}

TEST(OfficialInvolvement, AnAssistantFifteenMetresAcrossTheTouchlineIsNotInvolved) {
  PesWorld world;
  StageFoulStop(world, -40.0f, 5.0f);
  PlaceOfficial(world, kAssistantOfficialIndex, -40.0f, 20.0f);
  EXPECT_FALSE(world.Involved(kAssistantOfficialIndex));
}

TEST(OfficialInvolvement, AnAssistantOnTheOtherHalfIsNotInvolved) {
  PesWorld world;
  StageFoulStop(world, -30.0f, 5.0f);
  PlaceOfficial(world, kAssistantOfficialIndex, 10.0f, 5.0f);
  EXPECT_FALSE(world.Involved(kAssistantOfficialIndex));
}

}  // namespace
