// pes_native.cpp: PesHeap, the GF-native heap behind PES's allocator tables, and the dispatch table's native targets.
// See pes_native.hpp.
#include "pes_native.hpp"

#include <sys/mman.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

void PesHeap::Store64(uint8_t* mem, uint64_t off, uint64_t v) { std::memcpy(mem + off, &v, 8); }

namespace {
// An anonymous private mapping of `bytes` zero bytes, committed as the heap touches it (the arena reads as zero until PES uses it).
uint8_t* MapZeroed(uint64_t bytes) {
  void* p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) Die("cannot map a heap of bytes", bytes);
  return static_cast<uint8_t*>(p);
}
}  // namespace

PesHeap::PesHeap(uint64_t arenaBytes)
    : page_(MapZeroed(kPageBytes)), arena_(MapZeroed(arenaBytes)), arenaBytes_(arenaBytes), top_(kBlocksOff) {
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
  Store64(page_, kInitCountVa - kPageVa, 1);
  const uint64_t table = kTableVa - kPageVa;
  for (unsigned id : kHeapPools) Store64(page_, table + id * kPoolBytes, heapObj);
  for (unsigned id = kForwarderFirst; id <= kForwarderLast; ++id) Store64(page_, table + id * kPoolBytes, fwdObj);
}

PesHeap::PesHeap(uint64_t arenaBytes, Adopt)
    : page_(MapZeroed(kPageBytes)), arena_(MapZeroed(arenaBytes)), arenaBytes_(arenaBytes), top_(kBlocksOff) {
  if (arenaBytes <= kBlocksOff) Die("arena too small:", arenaBytes);
}

PesHeap::~PesHeap() {
  if (page_) ::munmap(page_, kPageBytes);
  if (arena_) ::munmap(arena_, arenaBytes_);
}

void PesHeap::Attach(PesState& s) {
  s.heap = this;
  // Replaces only the heap's own windows (a previous Attach's): the windows the caller lent (the trap world's shared regions, lent by
  // PesWorld's LendExternals before the heap attaches) stay mapped.
  auto& services = s.mem.services;
  services.erase(std::remove_if(services.begin(), services.end(),
                                [](const PesMem::Window& w) { return w.base == kPageVa || w.base == kArenaVa; }),
                 services.end());
  services.push_back({kPageVa, kPageBytes, nullptr, page_});
  services.push_back({kArenaVa, arenaBytes_, nullptr, arena_});
}

PesHeap::Bookkeeping PesHeap::SaveBookkeeping() const {
  Bookkeeping b;
  b.top = top_;
  b.live = live_;
  b.freeList = freeList_;
  b.allocs = allocs_;
  b.frees = frees_;
  return b;
}

void PesHeap::RestoreBookkeeping(const Bookkeeping& b) {
  top_ = b.top;
  live_ = b.live;
  freeList_ = b.freeList;
  allocs_ = b.allocs;
  frees_ = b.frees;
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
  if (size > arenaBytes_) return 0;  // 0x1403FDB10 refuses sizes that wrap; anything past the arena cannot fit
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
    if (off + n > arenaBytes_) return 0;
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

namespace {

PesFileSource* g_fileSource = nullptr;

// The CRI Fs operation object (0x1415A6CA8 makes it): the fields the reader's calls touch. +0x1C is the loader status
// (0x1415A70B4 reads it: 2 = complete, 3 = error; the reader tests 2), +0x141 a flag 0x1415A70B4 tests, +0x148 / +0x150 the
// completion callback and its argument (0x1415A734C stores them).
constexpr uint64_t kOpBytes = 0x160, kOpStatusOff = 0x1C, kOpCallbackOff = 0x148, kOpArgOff = 0x150;
constexpr uint32_t kStatusComplete = 2, kStatusError = 3;
constexpr uint64_t kCriOk = 0, kCriErrorInvalid = 0xFFFFFFFE;  // 0x1415A2FD0 / 0x1415A71B8 answer these (0xFFFFFFFE: the invalid-argument error)

PesFileSource& Source() {
  if (!g_fileSource) Die("file service call with no file source set; rcx", 0);
  return *g_fileSource;
}

std::string CString(const PesState& s, uint64_t va) {
  std::string out;
  for (uint64_t c = s.rd(va, 1); c != 0 && out.size() < 4096; c = s.rd(va + out.size(), 1)) out.push_back(static_cast<char>(c));
  return out;
}

}  // namespace

void pes_service_set_file_source(PesFileSource* source) { g_fileSource = source; }

// FUN_1415A2FD0(fs, path, int64* size): the file's size at *size, -1 for a missing file; 0 on success (the reader tests the size).
void pes_service_fs_stat(PesState& s) {
  const int64_t size = Source().Size(CString(s, s.gpr[2]).c_str());
  s.wr(s.gpr[8], static_cast<uint64_t>(size), 8);
  s.gpr[0] = size >= 0 ? kCriOk : kCriErrorInvalid;
}

// FUN_1415A6CA8(op**): a zeroed operation object, its pointer at *rcx.
void pes_service_fs_op_create(PesState& s) {
  const uint64_t op = HeapOf(s).Alloc(kOpBytes, PesHeap::kMinAlign);
  if (!op) Die("operation object: the arena is full; bytes", kOpBytes);
  s.wr(s.gpr[1], op, 8);
}

// FUN_1415A734C(op, callback, arg): stored; the completion callback is never invoked here (it only releases the reader's semaphore,
// which the WaitForSingleObject model satisfies).
void pes_service_fs_op_callback(PesState& s) {
  s.wr(s.gpr[1] + kOpCallbackOff, s.gpr[2], 8);
  s.wr(s.gpr[1] + kOpArgOff, s.gpr[8], 8);
  s.gpr[0] = kCriOk;
}

// FUN_1415A71B8(op, fs, path, offset, loadSize, buffer, bufferSize): reads min(loadSize, bufferSize) bytes from `offset`, synchronously,
// into the buffer and completes the operation. The stack arguments are the caller's: a service runs in place of the call, so [rsp + 0x20]
// is the fifth argument.
void pes_service_fs_op_read(PesState& s) {
  const uint64_t op = s.gpr[1], offset = s.gpr[9], loadSize = s.rd(s.gpr[4] + 0x20, 8), buffer = s.rd(s.gpr[4] + 0x28, 8),
                 bufferSize = s.rd(s.gpr[4] + 0x30, 8), n = loadSize < bufferSize ? loadSize : bufferSize;
  uint8_t* dst = s.mem.service_at(buffer, static_cast<unsigned>(n < 0xFFFFFFFFu ? n : 0));
  std::vector<uint8_t> staging;
  if (!dst) {  // the buffer is not in a host window (a snapshot-backed call): stage it
    staging.resize(n);
    dst = staging.data();
  }
  const int64_t got = Source().Read(CString(s, s.gpr[8]).c_str(), offset, dst, n);
  if (dst == staging.data())
    for (uint64_t i = 0; got > 0 && i < static_cast<uint64_t>(got); ++i) s.wr(buffer + i, staging[i], 1);
  s.wr(op + kOpStatusOff, got == static_cast<int64_t>(n) ? kStatusComplete : kStatusError, 4);
  s.gpr[0] = got == static_cast<int64_t>(n) ? kCriOk : kCriErrorInvalid;
}

// FUN_1415A70B4(op, int* status): the loader status at *status (0x1415A70B4 maps a finished status with its flag at +0x141 set to 0,
// a flag this operation never sets); 0 on success.
void pes_service_fs_op_status(PesState& s) {
  s.wr(s.gpr[2], s.rd(s.gpr[1] + kOpStatusOff, 4), 4);
  s.gpr[0] = kCriOk;
}

// FUN_1415A6CBC(op): the operation object is released.
void pes_service_fs_op_destroy(PesState& s) { HeapOf(s).Free(s.gpr[1]); }

// STAND-INS for the packer's virtualized functions (stubs.json gf-service, ops packer_*; the effects are read from PES's own bytes on a real
// pre-state, see the entries' evidence). Registers other than rax follow the caller's use: only rax is read after either call.
namespace {
constexpr uint64_t kPoolMaxSizeOff = 0x60;  // the pool object's max request size: FUN_1403EA250 tests `size <= [pool + 0x60]`
constexpr uint64_t kLuaStateGlobalOff = 0x20, kLuaGlobalPanicOff = 0xA8;  // lua_State::l_G, global_State::panic
constexpr uint64_t kLuaGlobalAllocOff = 0x10, kLuaGlobalAllocUdOff = 0x18;  // global_State::frealloc, ::ud
}  // namespace

// 0x143E98DA0(pool, size): rax = the old max size; the pool's max size = size.
void pes_service_packer_pool_set_max(PesState& s) {
  s.gpr[0] = s.rd(s.gpr[1] + kPoolMaxSizeOff, 8);
  s.wr(s.gpr[1] + kPoolMaxSizeOff, s.gpr[2], 8);
}

// 0x143DFA780(L, panicf) = lua_atpanic: rax = the old panic function; global_State::panic = panicf.
void pes_service_packer_lua_atpanic(PesState& s) {
  const uint64_t panic = s.rd(s.gpr[1] + kLuaStateGlobalOff, 8) + kLuaGlobalPanicOff;
  s.gpr[0] = s.rd(panic, 8);
  s.wr(panic, s.gpr[2], 8);
}

// 0x143DFCCE0(L, f, ud) = lua_setallocf: global_State::frealloc = f, ::ud = ud; rax = the global state (as the real call left it).
void pes_service_packer_lua_setallocf(PesState& s) {
  const uint64_t g = s.rd(s.gpr[1] + kLuaStateGlobalOff, 8);
  s.wr(g + kLuaGlobalAllocOff, s.gpr[2], 8);
  s.wr(g + kLuaGlobalAllocUdOff, s.gpr[8], 8);
  s.gpr[0] = g;
}

// 0x1444CE440(array) and 0x1444C7960(array): the constructors of two arrays of small polymorphic objects (0x18 bytes: vtable, 0xFFFF0000, 0).
// The vtables are read from the boot capture and the running game (stubs.json entries); the translated code never sees the constructors' bodies.
namespace {
constexpr uint64_t kPackerObjectBytes = 0x18, kPackerObjectFlagsOff = 8, kPackerObjectFlags = 0xFFFF0000;
constexpr uint64_t kArrayAVtables[] = {
    0x1427DBB38, 0x1427DBBD8, 0x1427DBC78, 0x1427DBD48,
    0x1427DBDC0, 0x1427DBE60, 0x1427DBEB0, 0x1427DBF50,
    0x1427DBFC8, 0x1427DC068, 0x1427DC108, 0x1427DC180,
    0x1427DC240, 0x1427DC338, 0x1427DC418, 0x1427DC4B8,
};
constexpr uint64_t kArrayBVtables[] = {
    0x1427DB688, 0x1427DB700, 0x1427DB778, 0x1427DB7F0,
    0x1427DB868, 0x1427DB8E0, 0x1427DB958, 0x1427DB9D0,
    0x1427DBA48, 0x1427DBAC0, 0x1427DBB60, 0x1427DBC00,
    0x1427DBC50, 0x1427DBD70, 0x1427DBDE8, 0x1427DBE38,
    0x1427DBED8, 0x1427DBF28, 0x1427DBFF0, 0x1427DC090,
};
template <size_t N>
void StoreObjectArray(PesState& s, uint64_t array, const uint64_t (&vtables)[N]) {
  for (size_t k = 0; k < N; ++k) {
    const uint64_t object = array + k * kPackerObjectBytes;
    s.wr(object, vtables[k], 8);
    s.wr(object + kPackerObjectFlagsOff, kPackerObjectFlags, 8);
    s.wr(object + 2 * kPackerObjectFlagsOff, 0, 8);
  }
}
}  // namespace

void pes_service_packer_array_ctor_a(PesState& s) { StoreObjectArray(s, s.gpr[1], kArrayAVtables); }
void pes_service_packer_array_ctor_b(PesState& s) { StoreObjectArray(s, s.gpr[1], kArrayBVtables); }

// 0x141957C60(size): the graphics device's allocator; GF has no device, so the allocation fails (rax = 0) and the caller's own failure path runs.
void pes_service_gfx_device_alloc(PesState& s) { s.gpr[0] = 0; }

// ponytail: the anime singleton, built by GF (AnimeLayer7; owner ruling 17-11-26: PES builds it behind copy protection, which GF does
// not run). PES's wrapper 0x141AEB7C0 allocates 0x2E0 bytes and calls the setter 0x141AEA910. The setter's Steam-dispatched calls
// (0x1415A1720 at 0x141AEB096; 0x14164FB10 at 0x141AEAD8D, 0x141AEAE4C, 0x141AEAF03, 0x141AEAFBA) and its protection thunks are not
// part of GF. GF makes every store the setter makes on its own path, in the setter's order, with the image constants it loads. The
// objects the protection-dependent calls fill are not built: S+0x20, S+0x30..S+0x58, S+0x70, the vector's elements, and the context
// global's fields. docs/ports/pes-port-AnimeLayer7.md lists them with their readers and the state diff against live captures.
// ponytail (latent release risk, AnimeLayer7): the S+0x20 object is allocated here through the pool (0x14029D370); its release would run
// through the singleton's destructor (0x141AEB5B0 via 0x141AEBE90), which returns unless +0x2C0 bit 0 is set (`test byte [rcx+0x2C0],1` at
// 0x141AEBE90, 0x141AEBE97). No writer sets bit 0 and every snapshot shows +0x2C0 = 0x8, so the release never runs. Its PES-side free is
// the core free 0x1403CF770 (core 0x14037FDB0), which does not match this allocation: a port of 0x140427100, 0x141B04600 or 0x1421742E2
// (the destructor's callers, untranslated and loud if reached) must route the release to the pool's own free first.
namespace {
constexpr uint64_t kSingletonVtableVa = 0x142AB91C0ULL;  // the setter's vptr store (lea at 0x141AEA935)
constexpr uint64_t kDriverVtableVa = 0x142AB9188ULL;     // the +0x28 record's vptr store (lea at 0x141AEB04A)
constexpr uint64_t kSingletonGlobalVa = 0x1437CEA40ULL;  // the wrapper's guard (0x141AEB7D1) and the setter's publish (0x141AEACB7)
constexpr uint64_t kSingletonBytes = 0x2E0;              // the wrapper's allocation (ecx at 0x141AEB7E6)
constexpr uint64_t kDriverBytes = 0x6D0;                 // the +0x28 record's allocation (ecx at 0x141AEB020)
constexpr uint64_t kAllocAlign = 0x10;                   // the edx of both allocations
// The unit vectors the setter copies into +0x80..+0x17F (xmm0..xmm3 at 0x141AEAA74..0x141AEAA5F, cycled four times).
constexpr uint64_t kUnitVectorVa[4] = {0x14253EE50ULL, 0x14253EE60ULL, 0x14253EE70ULL, 0x14253EE80ULL};
constexpr uint64_t kVec4Bytes = 0x10, kBasisOff = 0x80, kBasisBlocks = 0x10;
// The imm32 of the setter's and 0x1418801F0's stores, read from the install at those stores (ReadVa-style reads of the image, never typed):
constexpr uint64_t kTickImmVa = 0x141AEABD1ULL;       // mov dword [r14+0x2c4], imm32 (0x141AEABCA): 1/60 s
constexpr uint64_t kRateImmVa = 0x141AEABDCULL;       // mov dword [r14+0x2c8], imm32 (0x141AEABD5): 60 per second
constexpr uint64_t kRecordHeaderImmVa = 0x1418801F5ULL;  // mov qword [rcx], imm32 (0x1418801F2): the record header (0x30)
constexpr uint64_t kRecordFlagsImmVa = 0x141880207ULL, kRecordCountImmVa = 0x14188020EULL;  // mov dword [rcx+0x18/0x1C], imm32
constexpr uint64_t kRecordOff = 0x190, kRecordStride = 0x28, kRecordCount = 4;
constexpr uint64_t kCountOff = 0x10, kDriverOff = 0x28, kVectorCountOff = 0x60, kVectorCapOff = 0x64, kVectorDataOff = 0x68;
constexpr uint64_t kTailBlockOff = 0x260, kEndNodeOff = 0x288, kListHeadOff = 0x290, kListNodeOff = 0x2A0;
constexpr uint64_t kFlagsOff = 0x2C0, kTickOff = 0x2C4, kRateOff = 0x2C8, kModeOff = 0x2CC;
constexpr uint64_t kVectorCapacity = 2, kVectorElementBytes = 8;
}  // namespace

void pes_service_anime_singleton(PesState& s) {
  // The wrapper's guard (0x141AEB7D1): a created singleton stands.
  if (s.rd(kSingletonGlobalVa, 8) != 0) return;
  PesHeap& heap = HeapOf(s);
  // The wrapper passes its ecx on as the setter's edx: the mode (0 at the match load).
  const uint64_t mode = s.gpr[1] & 0xFFFFFFFFu;
  const uint64_t obj = heap.Alloc(kSingletonBytes, kAllocAlign);
  if (!obj) return;  // the wrapper skips the setter when the allocation returns 0 (0x141AEB7F8)
  s.wr(obj, kSingletonVtableVa, 8);
  s.wr(obj + kCountOff, 0, 4);
  s.wr(obj + 0x18, 0, 8);
  // The slots start empty: the setter's helpers store 0 into each (0x141AEA94C..0x141AEA9E4, 0x141AEAA54..0x141AEAA59, 0x141AEABF2).
  const uint64_t slots[] = {0x20, kDriverOff, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x70, 0x2D0};
  for (uint64_t off : slots) s.wr(obj + off, 0, 8);
  // +0x60..+0x68: the setter reserves two elements (0x141AEAA3B) and stores two null elements in its loop (0x141AEAC10).
  const uint64_t elements = heap.Alloc(kVectorCapacity * kVectorElementBytes, kAllocAlign);
  if (!elements) Die("anime singleton: the vector's elements; the arena is full; bytes", kVectorCapacity * kVectorElementBytes);
  s.wr(obj + kVectorCountOff, kVectorCapacity, 4);
  s.wr(obj + kVectorCapOff, kVectorCapacity, 4);
  s.wr(obj + kVectorDataOff, elements, 8);
  // +0x80..+0x17F: the setter copies the four unit vectors, cycled (0x141AEAA5F..0x141AEAB03).
  for (uint64_t block = 0; block < kBasisBlocks; ++block) {
    const uint64_t src = kUnitVectorVa[block % 4];
    s.wr(obj + kBasisOff + kVec4Bytes * block, s.rd(src, 8), 8);
    s.wr(obj + kBasisOff + kVec4Bytes * block + 8, s.rd(src + 8, 8), 8);
  }
  s.wr(obj + 0x180, 0, 8);
  s.wr(obj + 0x188, 0, 8);
  // Four records at +0x190, 0x28 apart: 0x1418801f0 writes the 0x30 header, and the setter zeroes the record's +0x20 (0x1B0, 0x1D8,
  // 0x200); the last one's +0x20 (0x228) is -1 (0x141AEAB4E), and +0x230 is -1 (0x141AEAB59).
  for (uint64_t k = 0; k < kRecordCount; ++k) {
    const uint64_t rec = obj + kRecordOff + kRecordStride * k;
    s.wr(rec, s.rd(kRecordHeaderImmVa, 4), 8);
    s.wr(rec + 8, 0, 8);
    s.wr(rec + 0x10, 0, 8);
    s.wr(rec + 0x18, s.rd(kRecordFlagsImmVa, 4), 4);
    s.wr(rec + 0x1C, s.rd(kRecordCountImmVa, 4), 4);
    s.wr(rec + 0x20, k + 1 < kRecordCount ? 0 : ~0ULL, 8);
  }
  s.wr(obj + 0x230, ~0ULL, 8);
  for (uint64_t off = 0x238; off <= 0x258; off += 8) s.wr(obj + off, 0, 8);
  // +0x260..+0x2B8: three dwords zeroed (0x14184D5C0), then the list: a head at +0x290 pointing at its node at +0x2A0, whose link is the
  // end node at +0x288 (0x141AEABBA..0x141AEABBE).
  s.wr(obj + kTailBlockOff, 0, 4);
  s.wr(obj + kTailBlockOff + 4, 0, 4);
  s.wr(obj + kTailBlockOff + 8, 0, 4);
  s.wr(obj + kEndNodeOff, 0, 8);
  s.wr(obj + kEndNodeOff + 8, 0, 8);
  s.wr(obj + kEndNodeOff + 0x10, 0, 8);
  s.wr(obj + kListHeadOff, obj + kListNodeOff, 8);
  s.wr(obj + kListNodeOff, obj + kEndNodeOff, 8);
  s.wr(obj + kListNodeOff + 8, 0, 8);
  s.wr(obj + kListNodeOff + 0x10, 0, 8);
  s.wr(obj + kTailBlockOff + 0x58, 0, 4);
  // The flags (bit 3 is set by the layer's match-branch OR, FUN_141AE77B0 at 0x141AE7848), the tick, the rate and the mode.
  s.wr(obj + kFlagsOff, 0, 4);
  s.wr(obj + kTickOff, s.rd(kTickImmVa, 4), 4);
  s.wr(obj + kRateOff, s.rd(kRateImmVa, 4), 4);
  s.wr(obj + kModeOff, mode, 4);
  // The +0x28 record (0x141AEB013..0x141AEB0D7): its zero fill of [+0x44, +0x6A4) is the setter's call 0x1415A1720(+0x44, 0, 0x660),
  // which is not part of GF and is modelled here as the zeros it leaves, by its arguments.
  const uint64_t driver = heap.Alloc(kDriverBytes, kAllocAlign);
  if (driver) {
    s.wr(driver, kDriverVtableVa, 8);
    s.wr(driver + 8, 1, 4);
    s.wr(driver + 0x10, 0, 4);
    for (uint64_t off = 0x18; off <= 0x38; off += 8) s.wr(driver + off, ~0ULL, 8);
    s.wr(driver + 0x40, ~0ULL, 4);
    for (uint64_t off = 0x6A4; off <= 0x6C4; off += 8) s.wr(driver + off, ~0ULL, 8);
    s.wr(driver + 0x6CC, 1, 1);
  }
  s.wr(obj + kDriverOff, driver, 8);
  // The publish (0x141AEACB7): the global holds the object from now on.
  s.wr(kSingletonGlobalVa, obj, 8);
}
