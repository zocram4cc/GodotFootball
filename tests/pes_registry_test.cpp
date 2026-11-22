// PES's named registry as GF keeps it (PesCameraRegistry: the GameImpl singleton, the RegistryImpl whose find and insert
// are the natives behind vtable slots +0x70 and +0x68, as 0x141B55730 calls them). A key is the pair of names the caller
// passes (name1 at rdx, name2 at r8), each a {ptr, len} struct in PES memory; the value is the object a lookup answers.
//
// Semantics under test (PES's 0x141B55730 and its find 0x141B6EAB0 / insert 0x141B6EB70, as the lookup pair the callers use):
// find answers the value inserted under exactly that pair, else 0; insert with a non-null object takes one reference on it
// (PES's LOCK inc at object+8), and a null object inserts nothing.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "pes_native.hpp"
#include "pes_state.hpp"

namespace {

struct Registry {
  PesState s;
  std::unique_ptr<PesHeap> heap = std::make_unique<PesHeap>();
  std::unique_ptr<PesCameraRegistry> reg;
  uint64_t self = 0;  // the RegistryImpl this lookup goes through (the GameImpl's +0x20)

  Registry() {
    heap->Attach(s);
    s.mem.segs.push_back({PesCameraRegistry::kGameSlotPageVa, std::vector<uint8_t>(PesCameraRegistry::kGameSlotPageBytes)});
    s.gpr[4] = 0x10000000 + 0x20000 - 0x100;
    reg = std::make_unique<PesCameraRegistry>(*heap, s);
    self = s.rd(s.rd(PesCameraRegistry::kGameImplSlotVa, 8) + PesCameraRegistry::kGameRegistryOff, 8);
  }

  // A {ptr, len} key struct in PES memory holding `text` (a copy in the arena, as a caller's string).
  uint64_t Key(const std::string& text) {
    const uint64_t bytes = heap->Alloc(text.empty() ? 1 : text.size(), 0x10);
    for (size_t i = 0; i < text.size(); ++i) s.wr(bytes + i, static_cast<uint8_t>(text[i]), 1);
    const uint64_t key = heap->Alloc(0x10, 0x10);
    s.wr(key, bytes, 8);
    s.wr(key + 8, text.size(), 8);
    return key;
  }

  uint64_t Find(const std::string& name1, const std::string& name2) {
    s.gpr[1] = self;
    s.gpr[2] = Key(name1);
    s.gpr[8] = Key(name2);
    pes_native_registry_find(s);
    return s.gpr[0];
  }

  void Insert(const std::string& name1, const std::string& name2, uint64_t object) {
    s.gpr[1] = self;
    s.gpr[2] = Key(name1);
    s.gpr[8] = Key(name2);
    s.gpr[9] = object;
    pes_native_registry_insert(s);
  }

  uint64_t NewObject() {
    const uint64_t obj = heap->Alloc(0x20, 0x10);
    s.wr(obj + 8, 1, 4);  // one reference: the creator's
    return obj;
  }
};

TEST(NamedRegistry, AnUnknownPairIsNotFound) {
  Registry r;
  EXPECT_EQ(r.Find("CAMERA_NOT_A_TARGET", ""), 0u);
  EXPECT_EQ(r.Find("", ""), 0u);
}

TEST(NamedRegistry, TheCameraGroupsNamesStillFind) {
  // The camera slots are keyed by the name and "INFO" (the second name PES's camera lookups pass; 0x14089AEE0 takes it from its
  // object's +0x10: a live lookup reads "INFO" at 0x142596900 on the proc camera's frames).
  Registry r;
  EXPECT_NE(r.Find("CAMERA_TARGET_0200", "INFO"), 0u);
  EXPECT_NE(r.Find("CAMERA_TARGET_0227", "INFO"), 0u);
  EXPECT_EQ(r.Find("CAMERA_TARGET_0228", "INFO"), 0u);  // past the group's 28 slots
  EXPECT_EQ(r.Find("CAMERA_TARGET_0300", "INFO"), 0u);  // another group
  EXPECT_EQ(r.Find("CAMERA_TARGET_0200", ""), 0u);      // the second name is part of the key
}

TEST(NamedRegistry, InsertThenFindAnswersTheObject) {
  Registry r;
  const uint64_t obj = r.NewObject();
  r.Insert("DEMO_INFO_NAME", "GOAL_CUT", obj);
  EXPECT_EQ(r.Find("DEMO_INFO_NAME", "GOAL_CUT"), obj);
}

TEST(NamedRegistry, TheSecondNameIsPartOfTheKey) {
  Registry r;
  const uint64_t obj = r.NewObject();
  r.Insert("DEMO_INFO_NAME", "GOAL_CUT", obj);
  EXPECT_EQ(r.Find("DEMO_INFO_NAME", "FOUL_CUT"), 0u);
  EXPECT_EQ(r.Find("DEMO_INFO_NAME", ""), 0u);
  EXPECT_EQ(r.Find("OTHER_NAME", "GOAL_CUT"), 0u);
}

TEST(NamedRegistry, InsertTakesOneReferenceOnTheObject) {
  Registry r;
  const uint64_t obj = r.NewObject();
  r.Insert("DEMO_INFO_NAME", "GOAL_CUT", obj);
  EXPECT_EQ(r.s.rd(obj + 8, 4), 2u);  // PES's insert: LOCK inc at object+8, from the creator's 1
}

TEST(NamedRegistry, InsertOfANullObjectInsertsNothing) {
  Registry r;
  r.Insert("DEMO_INFO_NAME", "GOAL_CUT", 0);
  EXPECT_EQ(r.Find("DEMO_INFO_NAME", "GOAL_CUT"), 0u);
}

TEST(NamedRegistry, CameraSlotsAreNotShadowedByNamedInserts) {
  Registry r;
  const uint64_t camera = r.Find("CAMERA_TARGET_0201", "INFO");
  const uint64_t obj = r.NewObject();
  r.Insert("DEMO_INFO_NAME", "GOAL_CUT", obj);
  EXPECT_NE(camera, 0u);
  EXPECT_EQ(r.Find("CAMERA_TARGET_0201", "INFO"), camera);
  EXPECT_EQ(r.Find("DEMO_INFO_NAME", "GOAL_CUT"), obj);
}

}  // namespace
