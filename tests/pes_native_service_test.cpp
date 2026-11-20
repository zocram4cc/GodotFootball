// pes_native_service_test.cpp: the boundary services that replace PES's allocator entries (stubs.json native_services):
// 0x14038E3E0 (the region allocator, pes_native_region_alloc) and 0x1403FB410 / 0x1403FB5B0 (the pool alloc and free). Each
// is checked against its contract: fresh blocks that do not overlap, the alignment asked for, the size honoured, zeroed
// contents, the status PES's caller reads, and free accepting what alloc returned. PES's realloc is not on the copy path
// and is not modelled.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "pes_native.hpp"
#include "pes_state.hpp"

namespace {

constexpr uint64_t kOutVa = 0x2300000000ULL;  // the out slot the region allocator writes (a window the fixture maps)

struct Fixture {
  PesState s;
  PesHeap heap;
  std::vector<uint8_t> out = std::vector<uint8_t>(16, 0xAA);
  Fixture() {
    heap.Attach(s);  // Attach replaces the service windows, so the out slot is mapped after it
    s.mem.services.push_back(PesMem::Window{kOutVa, out.size(), nullptr, out.data()});
  }
};

// E6B0's call shape: rcx = out slot, rdx = alignment, r8 = size, r9 = the minimum alignment (1).
uint64_t RegionAlloc(Fixture& f, uint64_t size, uint64_t align, uint64_t minAlign = 1) {
  f.s.gpr[1] = kOutVa;
  f.s.gpr[2] = align;
  f.s.gpr[8] = size;
  f.s.gpr[9] = minAlign;
  pes_native_region_alloc(f.s);
  return f.s.gpr[0];
}

uint64_t OutBlock(const Fixture& f) {
  uint64_t b = 0;
  std::memcpy(&b, f.out.data(), 8);
  return b;
}

TEST(PesNativeRegion, AnswersAZeroedBlockAlignedAndSizedAtTheOutSlot) {
  Fixture f;
  ASSERT_EQ(RegionAlloc(f, 0x100, 0x40), 0u);
  const uint64_t block = OutBlock(f);
  ASSERT_NE(block, 0u);
  EXPECT_EQ(block % 0x40, 0u);
  for (uint64_t off = 0; off < 0x100; off += 8) EXPECT_EQ(f.s.rd(block + off, 8), 0u) << "offset " << off;
}

TEST(PesNativeRegion, TwoCallsReturnDistinctBlocksThatDoNotOverlap) {
  Fixture f;
  ASSERT_EQ(RegionAlloc(f, 0x100, 0x40), 0u);
  const uint64_t first = OutBlock(f);
  ASSERT_EQ(RegionAlloc(f, 0x100, 0x40), 0u);
  const uint64_t second = OutBlock(f);
  ASSERT_NE(first, 0u);
  ASSERT_NE(second, 0u);
  EXPECT_NE(first, second);
  const uint64_t gap = second > first ? second - first : first - second;
  EXPECT_GE(gap, 0x100u);  // the size asked for is held, so the blocks cannot overlap
}

TEST(PesNativeRegion, ABadAlignmentIsRefusedAndWritesNothing) {
  Fixture f;
  EXPECT_EQ(RegionAlloc(f, 0x100, 0x30), 160u);  // not a power of two
  EXPECT_EQ(RegionAlloc(f, 0x100, 0), 160u);     // below the minimum alignment of 1
  for (uint8_t b : f.out) EXPECT_EQ(b, 0xAA);
}

TEST(PesNativeRegion, AnExhaustedArenaAnswersNoMemory) {
  Fixture f;
  EXPECT_EQ(RegionAlloc(f, uint64_t{1} << 30, 0x40), 8u);
  for (uint8_t b : f.out) EXPECT_EQ(b, 0xAA);
}

TEST(PesNativePool, FreeAcceptsWhatTheAllocReturnedAndReleasesIt) {
  Fixture f;
  f.s.gpr[1] = kOutVa;  // &poolId: the arena serves every pool, so the id is not read
  f.s.gpr[2] = 0x80;
  f.s.gpr[8] = 0x10;
  pes_native_pool_alloc(f.s);
  const uint64_t block = f.s.gpr[0];
  ASSERT_NE(block, 0u);
  EXPECT_EQ(block % 0x10, 0u);
  EXPECT_TRUE(f.heap.Owns(block));
  f.s.gpr[1] = kOutVa;
  f.s.gpr[2] = block;
  pes_native_pool_free(f.s);
  EXPECT_EQ(f.s.gpr[0], 1u);
  EXPECT_FALSE(f.heap.Owns(block));
}

// The frame replay answers the outermost allocations from the capture: the block is PES's recorded address, and PES's later
// free of it is answered as one of the arena's.
TEST(PesNativePool, AReplayedAnswerIsTheRecordedBlockAndFreeReleasesIt) {
  Fixture f;
  constexpr uint64_t kRecorded = 0x2400000040ULL;
  f.heap.ReplayAnswers({{0x30, kRecorded}});
  f.s.gpr[1] = kOutVa;
  f.s.gpr[2] = 0x30;
  f.s.gpr[8] = 0x10;
  pes_native_pool_alloc(f.s);
  EXPECT_EQ(f.s.gpr[0], kRecorded);
  EXPECT_TRUE(f.heap.Owns(kRecorded));
  f.s.gpr[1] = kOutVa;
  f.s.gpr[2] = kRecorded;
  pes_native_pool_free(f.s);
  EXPECT_EQ(f.s.gpr[0], 1u);
  EXPECT_FALSE(f.heap.Owns(kRecorded));
}

// The copy's release frees the object PES allocated before the captured call: the replay answers no such block, so in the
// test mode (recorded answers installed) a free of a block this heap did not hand out releases nothing and answers true.
TEST(PesNativePool, InTestModeAFreeOfABlockBeforeTheCallReleasesNothing) {
  Fixture f;
  f.heap.ReplayAnswers({});
  f.s.gpr[1] = kOutVa;
  f.s.gpr[2] = 0x2400000080ULL;  // an arena address the heap never handed out
  pes_native_pool_free(f.s);
  EXPECT_EQ(f.s.gpr[0], 1u);
}

}  // namespace
