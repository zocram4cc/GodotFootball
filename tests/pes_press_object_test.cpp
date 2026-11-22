// The per-slot objects X (docs/re/pes-re-press-owner.md section 7) sit in the world window, one kSlotObjectBytes block per
// on-pitch slot, at the window's end. The layout is what PES's allocator gives each slot (FUN_1409211E0 allocates 0x1A5E0 bytes);
// the window's bytes are the only place GF keeps them. Pure layout checks: no install needed.
#include <gtest/gtest.h>

#include <cstdint>

#include "onthepitch/pesworldrecord.hpp"

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

}  // namespace
