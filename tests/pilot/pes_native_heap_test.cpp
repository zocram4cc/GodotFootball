// The native heap seam. Translated PES code that allocates calls PES's allocator 0x1403FB410, which reads the pool's
// heap object from the table at [0x1436EF2C8] and calls its vtable +0x10; the live vtable targets (0x1403FBC10,
// 0x141B34CB0) are not in the committed layer. PesHeap puts the same object graph in GF memory and the generated
// dispatch table routes those VAs to pes_native_heap_*. These tests run the translated allocator, the vector growth,
// the push and the FixDemo condition evaluator on a state whose only heap is PesHeap.
//
// Expectations come from the decompiles of the PES functions (docs/ports/pes-port-NativeSeam.md), not from the code:
//   0x141F4D2F0 evaluator(ctx, out vector<u32>*, cond, p4, flag): out.end = out.begin, then for cond 0x3F00 with flag
//     clear pushes 0x3F00 when ctx+0x2A22 is 0 and 0x3F01 otherwise; with flag set it returns without a push.
//   0x14047BE40 push_back(vector<u32>*, const u32*): grows through 0x1403FB410 when end == cap.
//   0x1403FB410 allocator(uint* pool, size, align): pool 0 is pool 1; an empty pool answers 0.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "pes_native.hpp"
#include "pes_state.hpp"
#include "translated.hpp"

namespace {

constexpr uint64_t kStackVa = 0x10000000, kStackBytes = 0x20000;
constexpr uint64_t kCtxVa = 0x20000000, kCtxBytes = 0x6000;   // the FixDemo ctx the evaluator reads (+0x2A22 ...)
constexpr uint64_t kScratchVa = 0x20400000, kScratchBytes = 0x1000;
constexpr uint64_t kVecVa = kScratchVa, kValVa = kScratchVa + 0x40, kPoolIdVa = kScratchVa + 0x80;
constexpr uint64_t kCtxFlagOff = 0x2A22;
constexpr uint64_t kShadowBytes = 0x20;  // the Win64 home area: stack argument 5 is at entry rsp + 0x28 (+8: return address)

// One machine: host-owned stack, ctx and scratch, plus the heap's windows. Translated functions are called as the
// dispatch does: a return address pushed (the callee's [rsp] is it).
struct Machine {
  PesHeap heap;
  PesState s;
  explicit Machine(uint64_t arenaBytes = PesHeap::kDefaultArenaBytes) : heap(arenaBytes) {
    heap.Attach(s);
    s.mem.segs.push_back({kStackVa, std::vector<uint8_t>(kStackBytes)});
    s.mem.segs.push_back({kCtxVa, std::vector<uint8_t>(kCtxBytes)});
    s.mem.segs.push_back({kScratchVa, std::vector<uint8_t>(kScratchBytes)});
    s.gpr[4] = kStackVa + kStackBytes - 0x100;
  }
  void Call(void (*fn)(PesState&)) {
    s.gpr[4] -= 8;
    s.wr(s.gpr[4], 0, 8);
    fn(s);
    s.gpr[4] += 8;
  }
  // Calls the allocator for `pool` (a uint stored in scratch) as 0x1403FB410's callers do.
  uint64_t Alloc(uint32_t pool, uint64_t size, uint64_t align) {
    s.wr(kPoolIdVa, pool, 4);
    s.gpr[1] = kPoolIdVa;
    s.gpr[2] = size;
    s.gpr[8] = align;
    Call(&pes_fn_1403FB410);
    return s.gpr[0];
  }
  void Free(uint32_t pool, uint64_t ptr) {
    s.wr(kPoolIdVa, pool, 4);
    s.gpr[1] = kPoolIdVa;
    s.gpr[2] = ptr;
    Call(&pes_fn_1403FB5B0);
  }
  void Push(uint32_t v) {
    s.wr(kValVa, v, 4);
    s.gpr[1] = kVecVa;
    s.gpr[2] = kValVa;
    Call(&pes_fn_14047BE40);
  }
  // The vector<u32> at kVecVa: its elements, read through the machine.
  std::vector<uint32_t> Vec() const {
    const uint64_t begin = s.rd(kVecVa, 8), end = s.rd(kVecVa + 8, 8);
    std::vector<uint32_t> out;
    for (uint64_t p = begin; p < end; p += 4) out.push_back(static_cast<uint32_t>(s.rd(p, 4)));
    return out;
  }
  // Runs the evaluator for cond 0x3F00: out vector at kVecVa, flag (the fifth argument) as given.
  void Evaluate(uint32_t cond, uint8_t flag) {
    s.gpr[1] = kCtxVa;
    s.gpr[2] = kVecVa;
    s.gpr[8] = cond;
    s.gpr[9] = 0;
    s.wr(s.gpr[4] + kShadowBytes, flag, 1);  // the fifth argument, at entry rsp + 0x28 once the call pushes its return address
    Call(&pes_fn_141F4D2F0);
  }
};

bool InArena(const PesHeap&, uint64_t va, uint64_t bytes) {
  return va >= PesHeap::kArenaVa && va + bytes <= PesHeap::kArenaVa + PesHeap::kDefaultArenaBytes;
}

TEST(NativeHeap, EvaluatorPushesThroughPesAllocatorOntoGfMemory) {
  Machine m;
  m.Evaluate(0x3F00, 0);
  EXPECT_EQ(m.Vec(), std::vector<uint32_t>{0x3F00});
  EXPECT_EQ(m.heap.Allocs(), 1u);  // the empty vector grew once, through the allocator
  const uint64_t begin = m.s.rd(kVecVa, 8);
  EXPECT_TRUE(InArena(m.heap, begin, 4)) << std::hex << begin;  // the elements live in GF's arena, 16-byte aligned
  EXPECT_EQ(begin % PesHeap::kMinAlign, 0u);
  EXPECT_TRUE(m.heap.Owns(begin));
}

TEST(NativeHeap, EvaluatorReusesTheVectorsCapacityAndFollowsTheCtx) {
  Machine m;
  m.s.wr(kCtxVa + kCtxFlagOff, 1, 1);
  m.Evaluate(0x3F00, 0);
  EXPECT_EQ(m.Vec(), std::vector<uint32_t>{0x3F01});
  const uint64_t begin = m.s.rd(kVecVa, 8);
  m.s.wr(kCtxVa + kCtxFlagOff, 0, 1);
  m.Evaluate(0x3F00, 0);  // end resets to begin, the push fits the capacity: no second block
  EXPECT_EQ(m.Vec(), std::vector<uint32_t>{0x3F00});
  EXPECT_EQ(m.s.rd(kVecVa, 8), begin);
  EXPECT_EQ(m.heap.Allocs(), 1u);
  m.Evaluate(0x3F00, 1);  // flag set: no push, and still no allocation
  EXPECT_TRUE(m.Vec().empty());
  EXPECT_EQ(m.heap.Allocs(), 1u);
}

TEST(NativeHeap, VectorGrowthKeepsElementsAndFreesTheOldBlocks) {
  Machine m;
  std::vector<uint32_t> want;
  for (uint32_t i = 0; i < 200; ++i) {
    m.Push(0xA000 + i);
    want.push_back(0xA000 + i);
  }
  EXPECT_EQ(m.Vec(), want);
  EXPECT_GE(m.heap.Allocs(), 3u);               // the buffer grew several times
  EXPECT_EQ(m.heap.Live(), 1u);                 // every outgrown buffer went back through 0x1403FB5B0
  EXPECT_EQ(m.heap.Frees(), m.heap.Allocs() - 1);
}

TEST(NativeHeap, EveryOccupiedPoolAllocatesAndTheEmptyOnesAnswerNull) {
  Machine m;
  for (uint32_t pool : {0u, 1u, 2u, 4u, 6u, 9u, 0xAu, 0x19u, 0x28u}) {  // heap pools, pool 0 (= 1) and forwarders
    const uint64_t p = m.Alloc(pool, 0x40, 0x10);
    EXPECT_NE(p, 0u) << "pool " << pool;
    EXPECT_TRUE(m.heap.Owns(p)) << "pool " << pool;
  }
  EXPECT_EQ(m.Alloc(3, 0x40, 0x10), 0u);    // empty pools (PES's table has nothing there)
  EXPECT_EQ(m.Alloc(0x29, 0x40, 0x10), 0u);
  EXPECT_EQ(m.heap.Live(), 9u);
}

TEST(NativeHeap, FreeThroughEitherPoolKindReturnsTheBlock) {
  Machine m;
  const uint64_t a = m.Alloc(2, 0x30, 0x10), b = m.Alloc(0x19, 0x30, 0x10);  // heap pool, forwarder pool
  ASSERT_NE(a, 0u);
  ASSERT_NE(b, 0u);
  EXPECT_NE(a, b);
  m.Free(2, a);
  m.Free(0x19, b);
  EXPECT_EQ(m.heap.Live(), 0u);
  EXPECT_EQ(m.heap.Frees(), 2u);
}

TEST(NativeHeap, AlignmentSizeZeroAndFailureFollowTheAllocationContract) {
  Machine m(1u << 16);
  const uint64_t a = m.Alloc(2, 100, 0x40);
  EXPECT_EQ(a % 0x40, 0u);
  const uint64_t z = m.Alloc(2, 0, 4);
  EXPECT_NE(z, 0u);  // a zero size still gets a block
  EXPECT_EQ(z % PesHeap::kMinAlign, 0u);
  EXPECT_NE(z, a);
  // A request the arena cannot hold: the allocator trims (native no-op), retries, finds no handler and answers 0.
  EXPECT_EQ(m.Alloc(2, 1u << 20, 0x10), 0u);
  EXPECT_EQ(m.heap.Live(), 2u);  // the failed call left nothing allocated
}

TEST(NativeHeapDeathTest, FreeingAPointerTheHeapNeverGaveOutAborts) {
  PesHeap heap;
  EXPECT_DEATH(heap.Free(PesHeap::kArenaVa + 0x2000), "PES_NATIVE_HEAP");
}

TEST(NativeHeap, ABlockIsReusedAfterItsFreeAndComesBackZeroed) {
  Machine m;
  const uint64_t a = m.Alloc(2, 0x30, 0x10);
  m.s.wr(a, 0xDEADBEEFCAFEF00DULL, 8);
  m.Free(2, a);
  const uint64_t b = m.Alloc(2, 0x30, 0x10);
  EXPECT_EQ(b, a);
  EXPECT_EQ(m.s.rd(b, 8), 0u);
}

}  // namespace
