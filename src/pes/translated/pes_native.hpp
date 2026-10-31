// pes_native.hpp: GF-native implementations of PES runtime services the translated layer reaches through the dispatch
// table. Hand-written runtime, like pes_state.hpp; the table that selects it is generated (gen_dispatch.py) from the
// global stub table (tools/dev/translate/stubs.json "native_services").
//
// The PES allocator 0x1403FB410(uint* poolId, size, align) reads the pool's heap object from the table at
// [0x1436EF2C8] (0x40 bytes per pool) and calls its vtable +0x10 (alloc); 0x1403FB5B0 frees through +8 and 0x1403FB860
// trims through +0x50. The live objects sit in front of PES's dlmalloc heap, whose OS imports and tracking chain are
// not translated. PesHeap is the same object graph in GF memory: the global slots, the pool table, pool objects whose
// vtables hold the PES VAs of the heap's methods, and an arena the blocks come from. The translated allocator, the
// vector growth and every container that allocates through it run unchanged on it; the vtable VAs dispatch to
// pes_native_heap_* (pes_native.cpp), which allocate in the arena.
#pragma once
#include <cstdint>
#include <map>
#include <vector>

#include "pes_state.hpp"

struct PesHeap {
  // The allocator's globals and pool table (0x1403FB410: [0x1436EF2C8] = table; 0x1403FB860: 0x32 pools, 0x40 bytes
  // each; entry +0 = the pool's heap object). The page window holds the two slots, the table and nothing else.
  static constexpr uint64_t kTableSlotVa = 0x1436EF2C8;
  static constexpr uint64_t kFailHandlerSlotVa = 0x1436EF2B8;  // 0x1403FB410's last resort; null: no handler
  static constexpr uint64_t kTableVa = 0x1436EF300;
  static constexpr uint64_t kPageVa = 0x1436EF000, kPageBytes = 0x1000;
  static constexpr unsigned kPools = 0x32, kPoolBytes = 0x40;
  // Pool occupancy of the live game (snapshot pid1694737/seq009, table walked by script): ids 1, 2, 4, 5, 6 and 9 hold
  // a heap object, 0xA..0x28 a forwarder to pool 1, the rest are empty (an allocation from an empty pool answers 0).
  static constexpr unsigned kForwarderFirst = 0xA, kForwarderLast = 0x28;
  static constexpr unsigned kHeapPools[] = {1, 2, 4, 5, 6, 9};
  // The VAs the objects' vtables hold: the dispatch table's native targets (stubs.json native_services).
  static constexpr uint64_t kHeapAllocVa = 0x1403FBC10, kHeapFreeVa = 0x1403FBCB0, kHeapOwnsVa = 0x1403FBD00,
                            kHeapTrimVa = 0x1403FBEB0;
  static constexpr uint64_t kFwdAllocVa = 0x141B34CB0, kFwdFreeVa = 0x141B34D80, kFwdOwnsVa = 0x141B34C30,
                            kFwdTrimVa = 0x141B34DB0;
  // Where GF's arena sits in the translated code's address space: above PES's image and the other GF windows.
  static constexpr uint64_t kArenaVa = 0x2400000000ULL;
  // ponytail: one fixed, bump-and-free-list arena; Alloc answers 0 when it is full, as PES's heap does when it cannot grow.
  // The FixDemo chains (vectors of a few hundred bytes, one std::map node per cached cut) need kilobytes; a chain that
  // keeps more live (replay, camera caches) raises kDefaultArenaBytes or moves to size classes with coalescing.
  static constexpr uint64_t kDefaultArenaBytes = 8u << 20;
  static constexpr uint64_t kMinAlign = 0x10;  // PES's block alignment (0x1403FB410's floor for sizes >= 0x10)

  explicit PesHeap(uint64_t arenaBytes = kDefaultArenaBytes);
  PesHeap(const PesHeap&) = delete;
  PesHeap& operator=(const PesHeap&) = delete;

  // Maps the page and the arena into s.mem.services and points s.heap here. Idempotent per state.
  void Attach(PesState& s);

  // The allocation contract: a block of at least `size` bytes (a zero size still gets a block), aligned to `align`
  // rounded up to a power of two (at least kMinAlign; above kMinAlign PES's heap takes at least 0x20), or 0 when the
  // arena cannot hold it. The contents are zero. Never moves, never overlaps a live block.
  uint64_t Alloc(uint64_t size, uint64_t align);
  // Frees a block Alloc returned (null is a no-op). Any other pointer is a bug in the caller: aborts loudly, as PES's
  // heap corrupts. A freed kMinAlign-aligned block is reused by a later allocation of its size class.
  void Free(uint64_t va);

  uint64_t Allocs() const { return allocs_; }
  uint64_t Frees() const { return frees_; }
  uint64_t Live() const { return live_.size(); }
  uint64_t LiveBytes() const;
  bool Owns(uint64_t va) const { return live_.count(va) != 0; }

 private:
  uint8_t* At(uint64_t va) { return arena_.data() + (va - kArenaVa); }
  void Store64(std::vector<uint8_t>& mem, uint64_t off, uint64_t v);

  std::vector<uint8_t> page_;   // the allocator's global page at kPageVa
  std::vector<uint8_t> arena_;  // the objects' own bytes, then the blocks, at kArenaVa
  uint64_t top_ = 0;            // bump offset into arena_ of the next fresh block
  struct Block { uint64_t bytes; bool recyclable; };
  std::map<uint64_t, Block> live_;                       // block VA -> its size, and whether Free may reuse it
  std::map<uint64_t, std::vector<uint64_t>> freeList_;  // block bytes -> reusable kMinAlign-aligned blocks
  uint64_t allocs_ = 0, frees_ = 0;
};

// The dispatch table's native targets (stubs.json native_services ops). Registers as PES calls them: rcx = the pool's
// object, rdx = size (alloc) or pointer (free, owns), r8 = alignment; the answer is rax. They need s.heap attached.
void pes_native_heap_alloc(PesState& s);
void pes_native_heap_free(PesState& s);
void pes_native_heap_owns(PesState& s);
void pes_native_heap_trim(PesState& s);

// The CAMERA_TARGET registry the FixDemo camera's target tables look their records up in. PES keeps them in the game's
// registry (a GameImpl at [0x1437F0AC8], +0x20 the RegistryImpl, whose find is vslot +0x70 = 0x141B6EAB0: hash the name,
// walk a std::map behind an SRW lock, answer the node's typed handle). The table lookup 0x141B55730 reads a handle H
// (vtable 0x142608698, refcount at +8) and keeps it in the entry's ref; the entry's info is *(H+0x10)+4. PesCameraRegistry
// is the same object graph in GF memory for the 28 names "CAMERA_TARGET_%02d%02d" of one group: the GameImpl singleton,
// a RegistryImpl whose vtable slot +0x70 is the dispatch table's native pes_native_registry_find (stubs.json
// native_services) and whose +0x68 (insert) aborts, a handle table, and one record per slot in the arena. A name outside
// the group answers 0: the lookup's not-found callback then runs and its creator reaches the aborting insert, a loud miss.
struct PesCameraRegistry {
  static constexpr uint64_t kGameImplSlotVa = 0x1437F0AC8;  // the singleton slot 0x141B55F40 reads
  static constexpr uint64_t kGameSlotPageVa = 0x1437F0000, kGameSlotPageBytes = 0x1000;
  // GF-private code VAs of the vtable slots: no PES function lives here; the dispatch table maps them to the natives.
  static constexpr uint64_t kFindVa = 0x2500000070, kInsertVa = 0x2500000068;
  static constexpr uint64_t kGameImplSlotWrapper = 0x14217D310;  // GameImpl vslot +0x38: returns this + 0x20 (translated)
  static constexpr uint64_t kHandleVtableVa = 0x142608698;       // a registry handle's vtable (read from the install)
  static constexpr unsigned kTargets = 28, kGroup = 2;
  // A record's info (record + 4), the layout the readers 0x14089AD80 / 0x14089AC40 take: valid byte, ground position,
  // two body points, facing. Units and axes are PES's (metres, y up; facing in degrees).
  struct Target {
    bool valid = false;
    float ground[3] = {0, 0, 0};  // info +0x04
    float pointA[3] = {0, 0, 0};  // info +0x10
    float pointB[3] = {0, 0, 0};  // info +0x1C
    float facingDeg = 0;          // info +0x28
  };

  // Builds the graph in `heap`'s arena and points the singleton slot at it in `s`. `s.mem` needs a writable segment over the
  // slot's page (kGameSlotPageVa, kGameSlotPageBytes) and `s.heap` attached to `heap`.
  PesCameraRegistry(PesHeap& heap, PesState& s);
  // Writes slot's record (the camera-side readers see it at once: they hold the handle, not a copy).
  void Set(PesState& s, unsigned slot, const Target& t) const;

 private:
  uint64_t handles_[kTargets] = {};
  uint64_t records_[kTargets] = {};
};

// The dispatch table's native targets for the registry's vtable slots. rcx = the RegistryImpl, rdx = the name key
// ({char* ptr, u64 length}), r8 = the type key; the answer is rax = the slot's handle, 0 for an unknown name.
void pes_native_registry_find(PesState& s);
void pes_native_registry_insert(PesState& s);
