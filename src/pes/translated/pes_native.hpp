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
  // The pool manager's init count (FUN_1403FB610: `if (count++ == 0) { build the table }`): 1 once PES's start-up has built the table, which GF's
  // page stands for; at 0 a later call of FUN_1403FB610 (a guarded static's constructor, FUN_1403FB8A0) would rebuild the table over GF's heap.
  static constexpr uint64_t kInitCountVa = 0x1436EF2D0;
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

  explicit PesHeap(uint64_t arenaBytes = kDefaultArenaBytes);  // fresh: zeroed mappings, the object graph written into them
  // A heap over mappings the caller lays out (a frozen state's clone, PesMemory): nothing is written; RestoreBookkeeping follows.
  struct Adopt {};
  PesHeap(uint64_t arenaBytes, Adopt);
  ~PesHeap();
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

  // The bytes of a live block a translated call allocated: [va, va + n) inside the arena, or null (the GF side reads the containers PES's code
  // grew, e.g. a man's series vector, P+0x3B98).
  const uint8_t* Host(uint64_t va, uint64_t n) const {
    return va >= kArenaVa && va + n <= kArenaVa + arenaBytes_ ? arena_ + (va - kArenaVa) : nullptr;
  }
  uint64_t Allocs() const { return allocs_; }
  uint64_t Frees() const { return frees_; }
  uint64_t Live() const { return live_.size(); }
  uint64_t LiveBytes() const;
  bool Owns(uint64_t va) const { return live_.count(va) != 0; }

  // The page and the arena as mappings (a frozen state's clone lays its pages over them).
  uint8_t* PageData() { return page_; }
  uint64_t PageBytes() const { return kPageBytes; }
  uint8_t* ArenaData() { return arena_; }
  uint64_t ArenaBytes() const { return arenaBytes_; }

  // The bookkeeping a frozen state carries: the bump offset, the blocks, the free lists and the counters.
  struct Block { uint64_t bytes; bool recyclable; };
  struct Bookkeeping {
    uint64_t top = 0;
    std::map<uint64_t, Block> live;                      // block VA -> its size, and whether Free may reuse it
    std::map<uint64_t, std::vector<uint64_t>> freeList;  // block bytes -> reusable kMinAlign-aligned blocks
    uint64_t allocs = 0, frees = 0;
  };
  Bookkeeping SaveBookkeeping() const;
  void RestoreBookkeeping(const Bookkeeping& b);

 private:
  uint8_t* At(uint64_t va) { return arena_ + (va - kArenaVa); }
  static void Store64(uint8_t* mem, uint64_t off, uint64_t v);
  // Copies PES's bytes into the page's words GF does not model (the packer's spin word 0x1436EF2B0 among them). Called by Attach
  // before the page window shadows the state's memory.
  void CopyPesGlobals(const PesState& s);

  uint8_t* page_ = nullptr;   // the allocator's global page at kPageVa
  uint8_t* arena_ = nullptr;  // the objects' own bytes, then the blocks, at kArenaVa
  uint64_t arenaBytes_ = 0;
  uint64_t top_ = 0;          // bump offset into arena_ of the next fresh block
  std::map<uint64_t, Block> live_;                      // block VA -> its size, and whether Free may reuse it
  std::map<uint64_t, std::vector<uint64_t>> freeList_;  // block bytes -> reusable kMinAlign-aligned blocks
  uint64_t allocs_ = 0, frees_ = 0;
};

// The dispatch table's native targets (stubs.json native_services ops). Registers as PES calls them: rcx = the pool's
// object, rdx = size (alloc) or pointer (free, owns), r8 = alignment; the answer is rax. They need s.heap attached.
void pes_native_heap_alloc(PesState& s);
void pes_native_heap_free(PesState& s);
void pes_native_heap_owns(PesState& s);
void pes_native_heap_trim(PesState& s);
// 0x14159F8B0, the packer-protected spin-lock acquire (rcx = the lock word; decode in stubs.json native_services).
void pes_native_spin_lock(PesState& s);

// The named registry the FixDemo camera's target tables and the goal's DemoInfo (0x140451C10) register in. PES keeps it in the
// game's registry (a GameImpl at [0x1437F0AC8], +0x20 the RegistryImpl; find is vslot +0x70 = 0x141B6EAB0 and insert vslot +0x68
// = 0x141B6EB70, both over a tree behind an SRW lock). 0x141B55730 (translated) is the caller: find-or-create by a pair of names.
// PesCameraRegistry is that object graph in GF memory: the GameImpl singleton, a RegistryImpl whose vtable slots +0x70 and +0x68
// are the dispatch table's natives pes_native_registry_find / _insert (a list of name-pair nodes in the arena: find answers the
// value of the pair, insert takes a reference on the object and adds the pair), and the 28 camera names "CAMERA_TARGET_02SS"
// (empty second name) answering the camera's handles. The natives are GF's stand-ins for PES's tree (ponytail: the tree's
// layout and the SRW lock are not modelled; the lookup semantics are PES's).
struct PesCameraRegistry {
  static constexpr uint64_t kGameImplSlotVa = 0x1437F0AC8;  // the singleton slot 0x141B55F40 reads
  static constexpr uint64_t kGameSlotPageVa = 0x1437F0000, kGameSlotPageBytes = 0x1000;
  static constexpr uint64_t kGameRegistryOff = 0x20;  // the GameImpl's +0x20: the RegistryImpl (0x141B55730 reads it through the wrapper)
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

// The file seam: where PES's file layer touches the OS, GF answers from the user's install. The file handle's reader (FUN_1414A9280)
// asks the CRI Fs API for a file's size (0x1415A2FD0) and reads it into the handle's buffer through an operation object
// (0x1415A6CA8 create, 0x1415A734C callback, 0x1415A71B8 read, 0x1415A70B4 status, 0x1415A6CBC destroy). PES's side stays translated;
// these six calls are the pes_service_fs_* functions (stubs.json "gf-service"), answered by the PesFileSource set below. The
// source is the sim library's PesVfs (it owns the install's cpks and mods); this layer cannot name it.
struct PesFileSource {
  virtual ~PesFileSource() = default;
  // The file's size in bytes, or -1 when the path names no file. `path` is the CRI path as PES's reader formed it (FUN_1414A22E0).
  virtual int64_t Size(const char* path) = 0;
  // Copies up to n bytes of the file from `offset` to dst; returns the byte count, or -1 on failure.
  virtual int64_t Read(const char* path, uint64_t offset, uint8_t* dst, uint64_t n) = 0;
};
// The source every service answers from (one per process: the reader runs on one thread). Null: a service call aborts.
void pes_service_set_file_source(PesFileSource* source);
