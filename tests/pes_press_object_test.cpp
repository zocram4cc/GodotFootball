// The per-slot objects X (docs/re/pes-re-press-owner.md section 7) sit in the world window, one kSlotObjectBytes block per
// on-pitch slot, at the window's end. The layout is what PES's allocator gives each slot (FUN_1409211E0 allocates 0x1A5E0 bytes);
// the window's bytes are the only place GF keeps them. Pure layout checks: no install needed.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include "onthepitch/pesfoxworld.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/vfs.hpp"

namespace {

TEST(SlotObjects, EachSlotHasItsOwnBlockOfTheAllocatorsSize) {
  pes::PesWorld world;
  for (int index = 1; index < pes::kSlotCount; ++index) {
    EXPECT_EQ(world.SlotObject(index) - world.SlotObject(index - 1), static_cast<std::ptrdiff_t>(pes::kSlotObjectBytes))
        << "slot " << index << " starts where slot " << index - 1 << "'s block ends";
  }
}

TEST(SlotObjects, TheLastSlotsBlockEndsTheWindowExceptForAlignmentPadding) {
  // The window struct is rounded up to its alignment: the bytes after the last block are that padding and nothing else.
  const uint64_t end = pes::PesWorld::SlotObjectVa(pes::kSlotCount - 1) + pes::kSlotObjectBytes;
  EXPECT_LE(end, pes::kWorldWindowVa + pes::kWorldBytes);
  EXPECT_LT(pes::kWorldWindowVa + pes::kWorldBytes - end, alignof(pes::PesWorldImage));
}

// The anime engine's heap, as the match builds it (matchevents.cpp kPesAnimeHeapBytes).
constexpr uint64_t kAnimeHeapBytes = 768ull << 20;
// PES's slot table of per-slot objects: FUN_14091E9F0 stores each X at (side * kSlotsPerSide + slot) * 8 (CONFIRMED from the ctor's stores).
constexpr uint64_t kSlotObjectTableVa = 0x1436F3FC0;

TEST(SlotObjects, BindMemoryBuildsEachSlotsObjectThroughPesCtorAndTheTableNamesIt) {
  // Install-backed: the anime world is booted from the user's image (PES_EXE) and install (PES21_ROOT); skipped otherwise.
  const char* exe = std::getenv("PES_EXE");
  const char* root = std::getenv("PES21_ROOT");
  if (exe == nullptr || *exe == '\0' || root == nullptr || *root == '\0') GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  pes::PesVfs vfs;
  std::string error;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  pes::PesFoxWorld anime(pes::AnimeWorldTemplate(exe, vfs, kAnimeHeapBytes), vfs);
  pes::PesWorld world;
  world.BindMemory(anime.Memory());
  for (int index = 0; index < pes::kSlotCount; ++index) {
    EXPECT_EQ(anime.Memory().Read64(kSlotObjectTableVa + sizeof(uint64_t) * index), pes::PesWorld::SlotObjectVa(index))
        << "slot " << index << ": the table entry must name the slot's X, which the ctor built at BindMemory";
  }
}

}  // namespace
