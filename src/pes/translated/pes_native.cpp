// pes_native.cpp: PesHeap, the GF-native heap behind PES's allocator tables, and the dispatch table's native targets.
// See pes_native.hpp.
#include "pes_native.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
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
// and +0x20 the RegistryImpl; RegistryImpl +0 vtable (+0x68 insert, +0x70 find) and +0x10 the head of its name list. A name
// node (0x30 bytes in the arena): +0 next, +8 name1 {ptr, len}, +0x18 name2 {ptr, len}, +0x28 the value find answers, the object
// the caller registered. A handle +0 vtable, +8 refcount, +0x10 the record. A record is 0x30 bytes in the capture
// (pid3234533/seq6224, handle 0x33F4490: record +0 = 1, info from +4).
constexpr uint64_t kGameVtableBytes = 0x40, kGameObjectBytes = 0x30, kGameVtableWrapperSlot = 0x38;
constexpr uint64_t kRegistryObjectBytes = 0x20, kRegistryVtableBytes = 0x78, kRegistryInsertSlot = 0x68, kRegistryFindSlot = 0x70,
                   kRegistryHeadOff = 0x10;
constexpr uint64_t kNodeBytes = 0x30, kNodeNext = 0, kNodeName1 = 8, kNodeName2 = 0x18, kNameLenOff = 8, kNodeValue = 0x28;
constexpr uint64_t kObjectRefcountOff = 8;  // PES's insert takes one reference on the object (LOCK inc at +8)
constexpr uint64_t kHandleBytes = 0x18, kHandleRefcountOff = 8, kHandleRecordOff = 0x10, kRecordBytes = 0x30, kRecordHeader = 1;
constexpr uint64_t kInfoOff = 4;  // info = record + 4: +0 valid byte, +4 ground, +0x10 point A, +0x1C point B, +0x28 facing
constexpr uint64_t kInfoGround = 4, kInfoPointA = 0x10, kInfoPointB = 0x1C, kInfoFacing = 0x28;
constexpr char kNamePrefix[] = "CAMERA_TARGET_";  // the format 0x142608750 up to its first conversion
constexpr uint64_t kNameDigits = 4;  // the two digits of the group and the two of the slot in a camera name
constexpr char kInfoName[] = "INFO";  // the second name of every camera slot: the record's info (0x14089AEE0 passes it)

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

// A name as a caller passes it: the {ptr, len} struct at the register's address, in PES memory.
struct NameKey {
  uint64_t ptr = 0, len = 0;
};

NameKey ReadName(PesState& s, uint64_t at) { return {s.rd(at, 8), s.rd(at + kNameLenOff, 8)}; }

bool SameName(PesState& s, const NameKey& a, const NameKey& b) {
  if (a.len != b.len) return false;
  for (uint64_t i = 0; i < a.len; ++i)
    if (s.rd(a.ptr + i, 1) != s.rd(b.ptr + i, 1)) return false;
  return true;
}

// The value of the node whose name pair is (n1, n2), 0 when the list holds none (0x141B6EAB0's find, over the whole list).
uint64_t FindNode(PesState& s, uint64_t head, const NameKey& n1, const NameKey& n2) {
  for (uint64_t node = head; node != 0; node = s.rd(node + kNodeNext, 8)) {
    const NameKey k1{s.rd(node + kNodeName1, 8), s.rd(node + kNodeName1 + kNameLenOff, 8)};
    const NameKey k2{s.rd(node + kNodeName2, 8), s.rd(node + kNodeName2 + kNameLenOff, 8)};
    if (SameName(s, n1, k1) && SameName(s, n2, k2)) return s.rd(node + kNodeValue, 8);
  }
  return 0;
}

// The arena copy of a name (PES's insert copies its key too, into a fresh block).
NameKey CopyName(PesState& s, PesHeap& heap, const NameKey& n) {
  const uint64_t ptr = HeapBlock(heap, n.len ? n.len : 1, "registry: no room for a name");
  for (uint64_t i = 0; i < n.len; ++i) s.wr(ptr + i, s.rd(n.ptr + i, 1), 1);
  return {ptr, n.len};
}

// Puts the node (n1, n2) -> value at the head of the list at self+0x10. The caller has checked that the pair is new.
void LinkNode(PesState& s, PesHeap& heap, uint64_t self, const NameKey& n1, const NameKey& n2, uint64_t value) {
  const NameKey c1 = CopyName(s, heap, n1), c2 = CopyName(s, heap, n2);
  const uint64_t node = HeapBlock(heap, kNodeBytes, "registry: no room for a node");
  s.wr(node + kNodeNext, s.rd(self + kRegistryHeadOff, 8), 8);
  s.wr(node + kNodeName1, c1.ptr, 8);
  s.wr(node + kNodeName1 + kNameLenOff, c1.len, 8);
  s.wr(node + kNodeName2, c2.ptr, 8);
  s.wr(node + kNodeName2 + kNameLenOff, c2.len, 8);
  s.wr(node + kNodeValue, value, 8);
  s.wr(self + kRegistryHeadOff, node, 8);
}

// The camera's slot name: "CAMERA_TARGET_" + the group's two digits + the slot's two digits (the format 0x142608750 builds).
std::string CameraName(unsigned group, unsigned slot) {
  char digits[kNameDigits + 1];
  std::snprintf(digits, sizeof digits, "%02u%02u", group, slot);
  return std::string(kNamePrefix) + digits;
}

}  // namespace

// A name placed in the arena (a copy of `text`), as its {ptr, len} key.
NameKey PutName(PesState& s, PesHeap& heap, const std::string& text) {
  const NameKey key{HeapBlock(heap, text.size() ? text.size() : 1, "registry: no room for a name"), text.size()};
  for (uint64_t k = 0; k < key.len; ++k) s.wr(key.ptr + k, static_cast<uint8_t>(text[k]), 1);
  return key;
}

PesCameraRegistry::PesCameraRegistry(PesHeap& heap, PesState& s) {
  const uint64_t game = HeapBlock(heap, kGameObjectBytes, "registry: no room for the GameImpl");
  const uint64_t gameVt = HeapBlock(heap, kGameVtableBytes, "registry: no room for the GameImpl vtable");
  const uint64_t reg = HeapBlock(heap, kRegistryObjectBytes, "registry: no room for the RegistryImpl");
  const uint64_t regVt = HeapBlock(heap, kRegistryVtableBytes, "registry: no room for the RegistryImpl vtable");
  s.wr(game, gameVt, 8);
  s.wr(gameVt + kGameVtableWrapperSlot, kGameImplSlotWrapper, 8);
  s.wr(game + kGameRegistryOff, reg, 8);
  s.wr(reg, regVt, 8);
  s.wr(regVt + kRegistryInsertSlot, kInsertVa, 8);
  s.wr(regVt + kRegistryFindSlot, kFindVa, 8);
  s.wr(reg + kRegistryHeadOff, 0, 8);
  for (unsigned i = 0; i < kTargets; ++i) {
    records_[i] = HeapBlock(heap, kRecordBytes, "registry: no room for a record");
    handles_[i] = HeapBlock(heap, kHandleBytes, "registry: no room for a handle");
    s.wr(records_[i], kRecordHeader, 4);
    s.wr(handles_[i], kHandleVtableVa, 8);
    s.wr(handles_[i] + kHandleRefcountOff, 1, 4);  // the registry's own reference
    s.wr(handles_[i] + kHandleRecordOff, records_[i], 8);
    // The slot's name pair: "CAMERA_TARGET_" + the group's and the slot's two digits, and "INFO" (the record's info, the second
    // name the camera's lookups pass: 0x14089AEE0 takes it from its object's +0x10).
    const NameKey name = PutName(s, heap, CameraName(kGroup, i));
    const NameKey info = PutName(s, heap, kInfoName);
    LinkNode(s, heap, reg, name, info, handles_[i]);
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

// The registry's find (vslot +0x70; 0x141B6EAB0 in PES): the value of the node whose name pair is (rdx, r8), 0 when none.
void pes_native_registry_find(PesState& s) {
  s.gpr[0] = FindNode(s, s.rd(s.gpr[1] + kRegistryHeadOff, 8), ReadName(s, s.gpr[2]), ReadName(s, s.gpr[8]));
}

// The registry's insert (vslot +0x68; 0x141B6EB70 in PES): a null object inserts nothing; otherwise the object takes one
// reference and the pair (rdx, r8) joins the list with the object as its value. A pair the registry already holds is a bug
// upstream: the lookup that precedes an insert answered 0.
void pes_native_registry_insert(PesState& s) {
  const uint64_t self = s.gpr[1], object = s.gpr[9];
  if (object == 0) return;
  const NameKey n1 = ReadName(s, s.gpr[2]), n2 = ReadName(s, s.gpr[8]);
  if (FindNode(s, s.rd(self + kRegistryHeadOff, 8), n1, n2) != 0)
    Die("registry insert of a name pair the registry already holds; key", n1.ptr);
  s.wr(object + kObjectRefcountOff, s.rd(object + kObjectRefcountOff, 4) + 1, 4);
  LinkNode(s, HeapOf(s), self, n1, n2, object);
}
