// PES's press team step (the writer 0x140536FC0, the coordinator 0x140634470 and the target eval 0x140633940, translated and
// proven bit-exact on live captures: results_press.json) run on the world window through PesWorld::StagePress. GF's retired
// hand ports (pespress.cpp: RunPresserPass1 / PressWinner / PressEvalPick) picked the loose-ball presser from GF's own
// positions; these cases follow PES's own decompile (docs/re/pes-re-press-life.md 3, pes-re-press-coord.md): the writer
// enrols a man of a side with no designated man, in play, whose d^2 to the ball is under the candidate radius, and writes
// PRESS (0x34) into T+0xA2A0.

#include <gtest/gtest.h>

#include <cstdint>

#include "onthepitch/pesclock.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace {

using namespace pes;

constexpr uint32_t kPress = 0x34;
constexpr uint32_t kLateFrame = 5000;  // far past any ball event: the writer's pressure score is at its cap
constexpr float kBallX_m = 10.0f;
constexpr float kBallZ_m = 3.0f;
constexpr float kNear_m = 4.0f;    // inside the writer's 9 m pick radius
constexpr float kFar_m = 60.0f;    // outside the candidate table's 21.5 m
constexpr int kNearMan = 4;
constexpr int kSide = 0;

struct Staged {
  PesWorld world;
  PesWorld::PressFacts facts;

  Staged() {
    for (int slot = 0; slot < kSlotCount; ++slot) world.BindSlot(slot);
    StoreU32(world.MatchInfo(), kMiFrameOffset, kLateFrame);
    for (int j = 0; j < kBallSampleCount; ++j) world.SetBallSample(j, kBallX_m, 0.0f, kBallZ_m);
    world.SetMaBall(kSide, kBallX_m, 0.0f, kBallZ_m);
    world.SetMaBall(1 - kSide, kBallX_m, 0.0f, kBallZ_m);
    for (int slot = 0; slot < kSlotCount; ++slot) Place(slot, kFar_m, slot * 2.0f);
    Place(kNearMan, kBallX_m - kNear_m, kBallZ_m);
    facts.playState = kMiPlayStateLive;
    facts.frame = kLateFrame;
    facts.armed = 1;
    facts.attackSign = kSide == 0 ? -1 : 1;
    facts.onPitch.fill(true);
    for (int k = 0; k < kSlotsPerSide; ++k) facts.roleClass[k] = 3;  // an outfield role class (0 is the keeper)
    facts.roleClass[0] = 0;
  }

  void Place(int slot, float x, float z) {
    uint8_t* man = world.ManRecord(slot);
    StoreF32(man, kManPosXOffset, x);
    StoreF32(man, kManPosZOffset, z);
  }

  void Run() {
    world.StagePress(kSide, facts);
    world.RunPressWriter(kSide);
  }

  int Pressers() {
    int n = 0;
    for (int k = 0; k < kSlotsPerSide; ++k) n += world.PressAction(kSide, k) == kPress;
    return n;
  }
};

TEST(PesPressWorld, TheManNearALooseBallIsPressedWhenTheSideHasNoDesignatedMan) {
  Staged s;
  s.Run();
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), kPress);
  EXPECT_EQ(s.Pressers(), 1);
}

TEST(PesPressWorld, NobodyIsPressedWhileTheSideHasADesignatedMan) {
  Staged s;
  s.facts.designated = kNearMan;  // T+0x78 valid: the writer leaves before the search (0x140537116)
  s.Run();
  EXPECT_EQ(s.Pressers(), 0);
}

TEST(PesPressWorld, NobodyIsPressedWhenTheBallIsNotInPlay) {
  Staged s;
  s.facts.playState = 2;  // MI+0x116C != 1: the writer leaves (0x1405370xx)
  s.Run();
  EXPECT_EQ(s.Pressers(), 0);
}

TEST(PesPressWorld, ANearManOffThePitchIsNotPressed) {
  Staged s;
  s.facts.onPitch[kNearMan] = false;  // MI+0x1748: the candidate table skips a man who is not on the pitch
  s.Run();
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), 0u);
}

TEST(PesPressWorld, TheManOutsideThePickRadiusIsNotPressed) {
  Staged s;
  s.Place(kNearMan, kBallX_m - kFar_m, kBallZ_m);
  s.Run();
  EXPECT_EQ(s.Pressers(), 0);
}

}  // namespace
