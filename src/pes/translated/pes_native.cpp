// pes_native.cpp: PesHeap, the GF-native heap behind PES's allocator tables, and the dispatch table's native targets.
// See pes_native.hpp.
#include "pes_native.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// The objects' places in the arena (offsets from kArenaVa); blocks start at kBlocksOff.
constexpr uint64_t kHeapObjOff = 0x000, kHeapVtOff = 0x100, kFwdObjOff = 0x200, kFwdVtOff = 0x300, kBlocksOff = 0x1000;
// Vtable slots the allocator reaches: +8 free (0x1403FB5B0), +0x10 alloc (0x1403FB410), +0x30 owns (0x1403FB5B0's pool
// scan), +0x50 trim (0x1403FB860). The forwarder object keeps its target pool's id at +8 (0x141B34CB0).
constexpr uint64_t kSlotFree = 0x08, kSlotAlloc = 0x10, kSlotOwns = 0x30, kSlotTrim = 0x50, kFwdPoolOff = 0x08;
constexpr uint64_t kForwardsToPool = 1;  // every live forwarder entry (pools 0xA..0x28) answers through pool 1

[[noreturn]] void Die(const char* what, uint64_t va) {
  std::fprintf(stderr, "PES_NATIVE_HEAP: %s %#llx\n", what, (unsigned long long)va);
  std::abort();
}

PesHeap& HeapOf(PesState& s) {
  if (!s.heap) Die("native allocator call with no heap attached to the state; pool object", s.gpr[1]);
  return *s.heap;
}

}  // namespace

void PesHeap::Store64(std::vector<uint8_t>& mem, uint64_t off, uint64_t v) { std::memcpy(mem.data() + off, &v, 8); }

PesHeap::PesHeap(uint64_t arenaBytes) : page_(kPageBytes), arena_(arenaBytes), top_(kBlocksOff) {
  if (arenaBytes <= kBlocksOff) Die("arena too small:", arenaBytes);
  const uint64_t heapObj = kArenaVa + kHeapObjOff, fwdObj = kArenaVa + kFwdObjOff;
  Store64(arena_, kHeapObjOff, kArenaVa + kHeapVtOff);
  Store64(arena_, kHeapVtOff + kSlotFree, kHeapFreeVa);
  Store64(arena_, kHeapVtOff + kSlotAlloc, kHeapAllocVa);
  Store64(arena_, kHeapVtOff + kSlotOwns, kHeapOwnsVa);
  Store64(arena_, kHeapVtOff + kSlotTrim, kHeapTrimVa);
  Store64(arena_, kFwdObjOff, kArenaVa + kFwdVtOff);
  Store64(arena_, kFwdObjOff + kFwdPoolOff, kForwardsToPool);
  Store64(arena_, kFwdVtOff + kSlotFree, kFwdFreeVa);
  Store64(arena_, kFwdVtOff + kSlotAlloc, kFwdAllocVa);
  Store64(arena_, kFwdVtOff + kSlotOwns, kFwdOwnsVa);
  Store64(arena_, kFwdVtOff + kSlotTrim, kFwdTrimVa);
  Store64(page_, kTableSlotVa - kPageVa, kTableVa);
  const uint64_t table = kTableVa - kPageVa;
  for (unsigned id : kHeapPools) Store64(page_, table + id * kPoolBytes, heapObj);
  for (unsigned id = kForwarderFirst; id <= kForwarderLast; ++id) Store64(page_, table + id * kPoolBytes, fwdObj);
}

void PesHeap::Attach(PesState& s) {
  s.heap = this;
  s.mem.services.clear();
  s.mem.services.push_back({kPageVa, kPageBytes, nullptr, page_.data()});
  s.mem.services.push_back({kArenaVa, arena_.size(), nullptr, arena_.data()});
}

uint64_t PesHeap::LiveBytes() const {
  uint64_t n = 0;
  for (const auto& b : live_) n += b.second.bytes;
  return n;
}

uint64_t PesHeap::Alloc(uint64_t size, uint64_t align) {
  uint64_t a = kMinAlign;
  if (align > kMinAlign) {  // 0x1403FDB10: over-aligned requests take the next power of two, at least 0x20
    a = 0x20;
    while (a < align) a <<= 1;
  }
  if (size > arena_.size()) return 0;  // 0x1403FDB10 refuses sizes that wrap; anything past the arena cannot fit
  const uint64_t n = ((size ? size : 1) + kMinAlign - 1) & ~(kMinAlign - 1);
  uint64_t va = 0;
  if (a == kMinAlign) {
    auto it = freeList_.find(n);
    if (it != freeList_.end() && !it->second.empty()) {
      va = it->second.back();
      it->second.pop_back();
      std::memset(At(va), 0, n);
    }
  }
  if (!va) {
    const uint64_t off = (top_ + a - 1) & ~(a - 1);  // the arena's base is aligned, so an offset's alignment is the address's
    if (off + n > arena_.size()) return 0;
    top_ = off + n;
    va = kArenaVa + off;
  }
  live_[va] = {n, a == kMinAlign};
  ++allocs_;
  return va;
}

void PesHeap::Free(uint64_t va) {
  if (!va) return;
  auto it = live_.find(va);
  if (it == live_.end()) Die("free of a pointer this heap did not allocate:", va);
  // ponytail: over-aligned blocks are not recycled (their padding is not tracked); only kMinAlign-aligned ones are.
  if (it->second.recyclable) freeList_[it->second.bytes].push_back(va);
  live_.erase(it);
  ++frees_;
}

// The dispatch table's native targets. Registers follow PES's call: rcx = the pool's object, rdx = size or pointer,
// r8 = alignment; the answer is rax. Every pool is backed by the one arena, so the forwarder and heap VAs share these.
void pes_native_heap_alloc(PesState& s) { s.gpr[0] = HeapOf(s).Alloc(s.gpr[2], s.gpr[8]); }

void pes_native_heap_free(PesState& s) {
  HeapOf(s).Free(s.gpr[2]);
  s.gpr[0] = 1;  // 0x1403FBCB0 answers true for every pointer it is handed
}

// 0x1403FBD00 answers true for a pool whose flags (+0x48) bit 0 is clear, which is every live pool's state.
void pes_native_heap_owns(PesState& s) { s.gpr[0] = 1; }

// 0x1403FBEB0 hands free pages back to the OS: nothing to give back from a GF arena.
void pes_native_heap_trim(PesState&) {}

namespace {

// The registry objects' layout (0x141B55730's reads): GameImpl +0 vtable (slot +0x38 = 0x14217D310, which returns this + 0x20)
// and +0x20 the RegistryImpl; RegistryImpl +0 vtable (+0x68 insert, +0x70 find); a handle +0 vtable, +8 refcount, +0x10 the
// record. A record is 0x30 bytes in the capture (pid3234533/seq6224, handle 0x33F4490: record +0 = 1, info from +4); the
// registry's own tree sits at RegistryImpl +0x10, where GF keeps its handle table.
constexpr uint64_t kGameVtableBytes = 0x40, kGameObjectBytes = 0x30, kGameVtableWrapperSlot = 0x38, kGameRegistryOff = 0x20;
constexpr uint64_t kRegistryObjectBytes = 0x20, kRegistryVtableBytes = 0x78, kRegistryInsertSlot = 0x68, kRegistryFindSlot = 0x70,
                   kRegistryTableOff = 0x10;
constexpr uint64_t kHandleBytes = 0x18, kHandleRefcountOff = 8, kHandleRecordOff = 0x10, kRecordBytes = 0x30, kRecordHeader = 1;
constexpr uint64_t kInfoOff = 4;  // info = record + 4: +0 valid byte, +4 ground, +0x10 point A, +0x1C point B, +0x28 facing
constexpr uint64_t kInfoGround = 4, kInfoPointA = 0x10, kInfoPointB = 0x1C, kInfoFacing = 0x28;
constexpr char kNamePrefix[] = "CAMERA_TARGET_";  // the format 0x142608750 up to its first conversion
constexpr uint64_t kNamePrefixLen = sizeof(kNamePrefix) - 1, kNameDigits = 4;

uint64_t HeapBlock(PesHeap& heap, uint64_t bytes, const char* what) {
  const uint64_t va = heap.Alloc(bytes, PesHeap::kMinAlign);
  if (!va) Die(what, bytes);
  return va;
}

void PutFloats(PesState& s, uint64_t va, const float* f, unsigned n) {
  for (unsigned i = 0; i < n; ++i) {
    uint32_t bits;
    std::memcpy(&bits, f + i, 4);
    s.wr(va + 4 * i, bits, 4);
  }
}

}  // namespace

PesCameraRegistry::PesCameraRegistry(PesHeap& heap, PesState& s) {
  const uint64_t game = HeapBlock(heap, kGameObjectBytes, "registry: no room for the GameImpl");
  const uint64_t gameVt = HeapBlock(heap, kGameVtableBytes, "registry: no room for the GameImpl vtable");
  const uint64_t reg = HeapBlock(heap, kRegistryObjectBytes, "registry: no room for the RegistryImpl");
  const uint64_t regVt = HeapBlock(heap, kRegistryVtableBytes, "registry: no room for the RegistryImpl vtable");
  const uint64_t table = HeapBlock(heap, 8ull * kTargets, "registry: no room for the handle table");
  s.wr(game, gameVt, 8);
  s.wr(gameVt + kGameVtableWrapperSlot, kGameImplSlotWrapper, 8);
  s.wr(game + kGameRegistryOff, reg, 8);
  s.wr(reg, regVt, 8);
  s.wr(regVt + kRegistryInsertSlot, kInsertVa, 8);
  s.wr(regVt + kRegistryFindSlot, kFindVa, 8);
  s.wr(reg + kRegistryTableOff, table, 8);
  for (unsigned i = 0; i < kTargets; ++i) {
    records_[i] = HeapBlock(heap, kRecordBytes, "registry: no room for a record");
    handles_[i] = HeapBlock(heap, kHandleBytes, "registry: no room for a handle");
    s.wr(records_[i], kRecordHeader, 4);
    s.wr(handles_[i], kHandleVtableVa, 8);
    s.wr(handles_[i] + kHandleRefcountOff, 1, 4);  // the registry's own reference
    s.wr(handles_[i] + kHandleRecordOff, records_[i], 8);
    s.wr(table + 8ull * i, handles_[i], 8);
  }
  s.wr(kGameImplSlotVa, game, 8);
}

void PesCameraRegistry::Set(PesState& s, unsigned slot, const Target& t) const {
  if (slot >= kTargets) Die("registry: no such camera target slot", slot);
  const uint64_t info = records_[slot] + kInfoOff;
  s.wr(info, t.valid ? 1 : 0, 1);
  PutFloats(s, info + kInfoGround, t.ground, 3);
  PutFloats(s, info + kInfoPointA, t.pointA, 3);
  PutFloats(s, info + kInfoPointB, t.pointB, 3);
  PutFloats(s, info + kInfoFacing, &t.facingDeg, 1);
}

// A name "CAMERA_TARGET_GGSS" of this group: the slot SS, else kTargets. Anything else (another group, a longer name, a
// foreign prefix) is not in this registry.
void pes_native_registry_find(PesState& s) {
  const uint64_t key = s.gpr[2];
  const uint64_t ptr = s.rd(key, 8), len = s.rd(key + 8, 8);
  s.gpr[0] = 0;
  if (len != kNamePrefixLen + kNameDigits) return;
  for (uint64_t i = 0; i < kNamePrefixLen; ++i)
    if (s.rd(ptr + i, 1) != static_cast<uint8_t>(kNamePrefix[i])) return;
  unsigned digit[kNameDigits];
  for (uint64_t i = 0; i < kNameDigits; ++i) {
    const uint64_t c = s.rd(ptr + kNamePrefixLen + i, 1);
    if (c < '0' || c > '9') return;
    digit[i] = static_cast<unsigned>(c - '0');
  }
  const unsigned group = digit[0] * 10 + digit[1], slot = digit[2] * 10 + digit[3];
  if (group != PesCameraRegistry::kGroup || slot >= PesCameraRegistry::kTargets) return;
  s.gpr[0] = s.rd(s.rd(s.gpr[1] + kRegistryTableOff, 8) + 8ull * slot, 8);
}

// PES's insert adds a node; GF's registry is fixed, so a lookup that missed and tried to create is a bug upstream.
void pes_native_registry_insert(PesState& s) { Die("registry insert of a name this registry does not hold; key", s.rd(s.gpr[2], 8)); }
