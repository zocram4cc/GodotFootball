// PesWorld's two reads of the player object P for the movestep: PES's translated 0x1406E6660 (the slot record's ability byte) and
// 0x1406BA7D0 (the entry-aim flag), run on GF's world window with P's pieces bound (pesworldrecord.hpp RunAbilityByte / RunEntryAim).
// Each expectation is a relation: the byte the reader returns is the byte the window holds; the flag follows the event the window
// holds. Needs PES21_ROOT (the install's image tables, which carry P's interface vtable).
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "onthepitch/pesimagetables.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"

namespace {

const pes::PesImageTables* Tables() {
  static const pes::PesImageTables* tables = [] () -> const pes::PesImageTables* {
    const char* root = std::getenv("PES21_ROOT");
    if (root == nullptr) return nullptr;
    std::string error;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &error);
    if (!exe) return nullptr;
    static std::optional<pes::PesImageTables> loaded = pes::LoadPesImageTables(*exe, &error);
    return loaded ? &*loaded : nullptr;
  }();
  return tables;
}

class PlayerObject : public ::testing::Test {
 protected:
  void SetUp() override {
    if (Tables() == nullptr) GTEST_SKIP() << "PES21_ROOT is not set or has no PES2021.exe";
    for (int slot = 0; slot < pes::kSlotCount; ++slot) world.BindSlot(slot);
    world.BindImageTables(*Tables());
  }
  uint8_t* Ability(int slot) { return world.Block(slot) + pes::kSlotRecordOffset + pes::kSlotAbilityOffset; }
  // The newest ball event (E+0x180698, what FUN_140A3E190(E, 0) returns): the action kind, the actor team word, the frame.
  void SetNewestEvent(uint32_t kind, uint32_t actor, uint32_t frame) {
    uint8_t* e = world.EventImage().newest.data();
    std::memcpy(e, &kind, 4);
    std::memcpy(e + 4, &actor, 4);
    std::memcpy(e + 0x10, &frame, 4);
  }
  pes::PesWorld world;
};

TEST_F(PlayerObject, AbilityByteIsTheByteOfTheSlotsAbilityBlock) {
  for (const int slot : {0, 5, 21}) {
    for (uint32_t id = 0; id < 0x3C; ++id) Ability(slot)[id] = static_cast<uint8_t>(slot * 7 + id * 3 + 1);
  }
  for (const int slot : {0, 5, 21}) {
    for (const uint32_t id : {0u, 0x14u, 0x27u, 0x29u, 0x3Bu})
      EXPECT_EQ(world.RunAbilityByte(slot, id), Ability(slot)[id]) << "slot " << slot << " id " << id;
  }
}

TEST_F(PlayerObject, AbilityByteReadsTheSlotNotAnotherPlayersBlock) {
  Ability(3)[0x29] = 41;
  Ability(4)[0x29] = 77;
  EXPECT_EQ(world.RunAbilityByte(3, 0x29), 41);
  EXPECT_EQ(world.RunAbilityByte(4, 0x29), 77);
}

TEST_F(PlayerObject, TheEntryAimFlagIsClearWithNoRecentEvent) {
  for (int i = 0; i < 100; ++i) world.AdvanceMatchFrame();
  SetNewestEvent(0x37, 2, 0);  // the empty sentinel's kind
  EXPECT_FALSE(world.RunEntryAim(5, 0, 1));
}

TEST_F(PlayerObject, TheEntryAimFlagFollowsARecentKick) {
  for (int i = 0; i < 100; ++i) world.AdvanceMatchFrame();
  for (const uint32_t kind : {0xAu, 0xDu}) {
    SetNewestEvent(kind, 2, world.MatchFrame());  // this tick's event of kind 0xA or 0xD
    EXPECT_TRUE(world.RunEntryAim(5, 0, 1)) << "kind " << kind;
    EXPECT_FALSE(world.RunEntryAim(5, 0, 2)) << "stopped play (MI+0x116C != 1) clears it, kind " << kind;
    SetNewestEvent(kind, 2, world.MatchFrame() - 100000);  // long ago
    EXPECT_FALSE(world.RunEntryAim(5, 0, 1)) << "an old event, kind " << kind;
  }
  SetNewestEvent(1, 2, world.MatchFrame());  // a short kick is neither
  EXPECT_FALSE(world.RunEntryAim(5, 0, 1));
}

TEST_F(PlayerObject, TheEntryAimFlagLeavesThePeriodRecordsPlayStateAlone) {
  world.Period().playState = 3;
  SetNewestEvent(0xA, 2, world.MatchFrame());
  world.RunEntryAim(5, 0, 1);
  EXPECT_EQ(world.Period().playState, 3u);
}

}  // namespace

// PES's man accessor 0x140A48A10 answers the table entry unchecked and PesWorld binds every entry, so ManRecord is the bound
// slot's man record, and a slot past the table reads entry 0 (the accessor's own fallback). An unbound table is a broken world:
// ManRecord stops there instead of answering a null every caller reads through (the -Wstringop-overread in WriteActorP).
TEST(ManRecord, IsTheBoundSlotsManRecordAndAPastTheTableSlotReadsEntryZero) {
  pes::PesWorld world;
  for (int slot = 0; slot < pes::kSlotCount; ++slot) world.BindSlot(slot);
  for (int slot = 0; slot < pes::kSlotCount; ++slot)
    EXPECT_EQ(world.ManRecord(slot), world.Bytes() + (pes::PesWorld::ManVa(slot) - pes::kWorldWindowVa)) << "slot " << slot;
  EXPECT_EQ(world.ManRecord(pes::kManCount), world.ManRecord(0));
}

TEST(ManRecordDeathTest, AnUnboundSlotStopsTheRunInsteadOfAnsweringNull) {
  pes::PesWorld world;
  EXPECT_DEATH(world.ManRecord(3), "never bound");
}
