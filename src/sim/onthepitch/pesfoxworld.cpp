#include "onthepitch/pesfoxworld.hpp"

#include "pes/translated/translated.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace pes {
namespace {

constexpr uint64_t kImageBase = 0x140000000ULL;
// The world's own windows, above PES's image and the heap arena (PesHeap::kArenaVa = 0x24_0000_0000).
constexpr uint64_t kStackVa = 0x2600000000ULL, kStackBytes = 0x100000;
constexpr uint64_t kTebVa = 0x2610000000ULL, kTebBytes = 0x2000;
constexpr uint64_t kTebSelfOffset = 0x30;  // gs:[0x30]: the thread block's own address (FlsGetValue and the last-error model read it)
// Win64: rsp at a function's entry holds the return address and is 8 (mod 16); the shadow space and the stack arguments sit above it.
constexpr uint64_t kEntryFrameBytes = 0x200;
// import_stubs.json "os_arena": PES's jemalloc (FUN_1403CF740, under FUN_14029D370) gets its chunks from VirtualAlloc, which the translated
// model serves from this range (the cell at its base is the bump pointer): the world maps it, or the allocator reads zeros and never returns.
constexpr uint64_t kOsArenaVa = 0x3000000000ULL, kOsArenaBytes = 0x100000000ULL;

}  // namespace

struct PesFoxWorld::Impl {
  const PesVfs& vfs;
  uint8_t* image = nullptr;
  uint64_t imageBytes = 0;
  PesHeap heap;
  std::vector<uint8_t> stack, teb;
  uint8_t* osArena = nullptr;  // the VirtualAlloc model's arena, lazily zeroed pages (anonymous mmap)
  Impl(const PesVfs& v, uint64_t heapBytes) : vfs(v), heap(heapBytes), stack(kStackBytes), teb(kTebBytes) {}
};

PesFoxWorld::PesFoxWorld(const std::string& imagePath, const PesVfs& vfs, uint64_t heapBytes)
    : impl_(std::make_unique<Impl>(vfs, heapBytes)) {
  const int fd = ::open(imagePath.c_str(), O_RDONLY);
  struct stat st {};
  if (fd < 0 || ::fstat(fd, &st) != 0) {
    if (fd >= 0) ::close(fd);
    throw std::runtime_error("fox world: cannot open the image " + imagePath);
  }
  impl_->imageBytes = static_cast<uint64_t>(st.st_size);
  // Private, copy-on-write: the chain writes image globals (the singletons, the lists), and the file stays as it is.
  void* map = ::mmap(nullptr, impl_->imageBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  ::close(fd);
  if (map == MAP_FAILED) throw std::runtime_error("fox world: cannot map the image " + imagePath);
  impl_->image = static_cast<uint8_t*>(map);
  void* arena = ::mmap(nullptr, kOsArenaBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (arena == MAP_FAILED) {
    ::munmap(map, impl_->imageBytes);
    throw std::runtime_error("fox world: cannot map the OS arena");
  }
  impl_->osArena = static_cast<uint8_t*>(arena);
  std::memcpy(impl_->teb.data() + kTebSelfOffset, &kTebVa, sizeof kTebVa);
  pes_service_set_file_source(this);
}

PesFoxWorld::~PesFoxWorld() {
  pes_service_set_file_source(nullptr);
  ::munmap(impl_->image, impl_->imageBytes);
  ::munmap(impl_->osArena, kOsArenaBytes);
}

PesState PesFoxWorld::NewCall() {
  PesState s;
  s.mem.flat = impl_->image;
  s.mem.flat_base = kImageBase;
  s.mem.flat_size = impl_->imageBytes;
  impl_->heap.Attach(s);  // clears the services, so the world's windows follow
  s.mem.services.push_back({kStackVa, kStackBytes, nullptr, impl_->stack.data()});
  s.mem.services.push_back({kTebVa, kTebBytes, nullptr, impl_->teb.data()});
  s.mem.services.push_back({kOsArenaVa, kOsArenaBytes, nullptr, impl_->osArena});
  s.gs_base = kTebVa;
  s.gpr[4] = kStackVa + kStackBytes - kEntryFrameBytes;
  s.wr(s.gpr[4], 0, 8);  // the return address slot
  return s;
}

uint64_t PesFoxWorld::Call(void (*fn)(PesState&), std::initializer_list<uint64_t> args) {
  PesState s = NewCall();
  static constexpr int kArgRegs[4] = {1, 2, 8, 9};  // rcx, rdx, r8, r9
  int i = 0;
  for (uint64_t a : args) {
    if (i == 4) throw std::runtime_error("fox world: more than four register arguments");
    s.gpr[kArgRegs[i++]] = a;
  }
  fn(s);
  return s.gpr[0];
}

uint64_t PesFoxWorld::Read64(uint64_t va) { return NewCall().rd(va, 8); }

void PesFoxWorld::Write64(uint64_t va, uint64_t value) {
  PesState s = NewCall();
  s.wr(va, value, 8);
}

std::string PesFoxWorld::ReadCString(uint64_t va, uint64_t max) {
  PesState s = NewCall();
  std::string out;
  for (uint64_t c = s.rd(va, 1); c != 0 && out.size() < max; c = s.rd(va + out.size(), 1)) out.push_back(static_cast<char>(c));
  return out;
}

uint64_t PesFoxWorld::Alloc(uint64_t bytes) { return impl_->heap.Alloc(bytes, PesHeap::kMinAlign); }

uint64_t PesFoxWorld::NewCString(const std::string& text) {
  const uint64_t va = impl_->heap.Alloc(text.size() + 1, PesHeap::kMinAlign);
  if (!va) throw std::runtime_error("fox world: the arena is full");
  PesState s = NewCall();
  for (std::size_t i = 0; i < text.size(); ++i) s.wr(va + i, static_cast<uint8_t>(text[i]), 1);
  return va;
}

int64_t PesFoxWorld::Size(const char* path) {
  const std::optional<uint64_t> size = impl_->vfs.Size(path);
  return size ? static_cast<int64_t>(*size) : -1;
}

int64_t PesFoxWorld::Read(const char* path, uint64_t offset, uint8_t* dst, uint64_t n) {
  std::string error;
  const auto bytes = impl_->vfs.Read(path, &error);
  if (!bytes || offset > bytes->size()) return -1;
  const uint64_t got = std::min<uint64_t>(n, bytes->size() - offset);
  std::memcpy(dst, bytes->data() + offset, got);
  ++serves_;
  return static_cast<int64_t>(got);
}

uint8_t PesFoxWorld::Read8(uint64_t va) { return static_cast<uint8_t>(NewCall().rd(va, 1)); }
void PesFoxWorld::Write8(uint64_t va, uint8_t value) {
  PesState s = NewCall();
  s.wr(va, value, 1);
}

uint32_t PesFoxWorld::Read32(uint64_t va) { return static_cast<uint32_t>(NewCall().rd(va, 4)); }

void PesFoxWorld::Write32(uint64_t va, uint32_t value) {
  PesState s = NewCall();
  s.wr(va, value, 4);
}

namespace {

// The singletons the dump holds from the running game (each names a heap object that does not exist in GF's world). Writers: the file
// manager 0x14149A6A3, the managers 0x1414B0530 / 0x1414B0C40, the file system object 0x1414A2738, the pump's list heads 0x1402223DC /
// 0x1402223AC, the ResManager 0x141AE77B0 (flag at 0x141AE78F0) and its singleton cell DAT_1437CEAA0 (FUN_141B02800).
constexpr uint64_t kFileManagerVa = 0x143704E40, kRequestManagerAVa = 0x143705DC8, kRequestManagerBVa = 0x143705DD0,
                   kFileSystemVa = 0x143704F18, kPumpListAVa = 0x1437F48A8, kPumpCountAVa = 0x1437F48B0, kPumpListBVa = 0x1437F48B8,
                   kPumpCountBVa = 0x1437F48C0, kResManagerVa = 0x1437CEAA0, kAnimeInitFlagVa = 0x1437CEA18;
constexpr uint64_t kFileManagerBytes = 0x2E0;  // FUN_14149AB60: FUN_1403FB410(pool, 0x2E0)
// The first call of the game's boot (live2/pid2740946 seq001): FUN_141AE77B0(0, 1, 0, 0, 0).
constexpr uint64_t kBootArgs[4] = {0, 1, 0, 0};
// FUN_141EA7190's flags (FUN_141EA8430 passes 0x20009 for a queued name).
constexpr uint64_t kJobFlags = 0x20009;
constexpr uint64_t kJobHandleOffset = 0x38, kHandleStateOffset = 8;
// The player's anime object and its parts (docs/ports/pes-port-AnimeLayer3.md section 2): A = *(P+0xA08), L1 = *(A+0x88) the AnimControl,
// the joint position array pointer at L1+0x50 and the joint rotation array pointer at A+0x20 (what FUN_141EB49B0 reads).
constexpr uint64_t kAnimeOffset = 0xA08, kControlOffset = 0x88, kPosePointerOffset = 0x50, kQuatPointerOffset = 0x20;
// The phases one frame runs on a body control, ascending (docs/ports/pes-port-AnimeLayer.md section 2: the driver's switch).
constexpr uint32_t kFramePhases[] = {1, 2, 3, 4, 5, 7, 8, 9, 0xB, 0xD, 0xF, 0x10, 0x11};
constexpr uint8_t kHandleQueuedRead = 2, kHandleQueuedDecode = 5;
constexpr int kFinishSteps = 16;
// The registry (FUN_141EC8C40's singleton DAT_1437F4F78) and the project registrars' once-flags (FUN_141ECA500 reads DAT_1437F4F82), which
// the dump holds as the running game left them. The anime systems table DAT_1437CEA30 is left as the dump holds it (a system is made only for
// a null cell, and a made one allocates through PES's jemalloc, whose state is the dead game's: see the report, section 5 item 1).
constexpr uint64_t kRegistryVa = 0x1437F4F78, kRegistrarFlagVa = 0x1437F4F82;
// The scratch arena (FUN_1403EFDF0 pushes frames on the TLS slot's frame): the thread block's TLS pointer (TEB+0x58), the TLS index
// cell, the slot offset in the thread's block, the frame's vtable and the arena's size.
constexpr uint64_t kTebTlsPointerOffset = 0x58, kTlsIndexVa = 0x1437067A0, kTlsSlotOffset = 0x38, kScratchFrameVtable = 0x14258BCA8,
                   kScratchArenaBytes = 0x40000, kTlsBlockBytes = 0x100, kScratchFrameBytes = 0x38;
constexpr uint64_t kScratchFrameBaseOffset = 0x18, kScratchFrameCursorOffset = 0x20, kScratchFrameEndOffset = 0x28;
// The pump's queue node: the job at +0x10 (FUN_141EA8430), the job's handle at +0x38.
constexpr uint64_t kPumpNodeJobOffset = 0x10;
constexpr int kPumpSteps = 4096;
// The constructor's callback argument: FUN_1406E03D0 calls thunk_FUN_141EB5250(P, "body", P[0x754], &LAB_141B970E0) (decompile): an
// image code address.
constexpr uint64_t kBodyCallbackVa = 0x141B970E0;
// The preload owner (see LoadBody): the preload and the registrar touch its lock at +0x48 and its list pointer at +0x70.
constexpr uint64_t kOwnerBytes = 0x200;
constexpr char kBodyName[] = "body";

}  // namespace

void PesFoxWorld::Boot() {
  for (uint64_t va : {kFileManagerVa, kRequestManagerAVa, kRequestManagerBVa, kFileSystemVa, kPumpListAVa, kPumpCountAVa, kPumpListBVa,
                      kPumpCountBVa, kResManagerVa, kRegistryVa})
    Write64(va, 0);
  {
    PesState s = NewCall();
    s.wr(kRegistrarFlagVa, 0, 1);
  }
  {
    PesState s = NewCall();
    s.wr(kAnimeInitFlagVa, 0, 1);
  }
  const uint64_t manager = Alloc(kFileManagerBytes);
  Call(::pes_fn_14149A620, {manager});
  Call(::pes_fn_1414A3550, {});
  Call(::pes_fn_1414B08A0, {});
  Call(::pes_fn_1414B0E90, {});
  Call(::pes_fn_1402223C0, {});
  Call(::pes_fn_140222390, {});
  Call(::pes_fn_141AE77B0, {kBootArgs[0], kBootArgs[1], kBootArgs[2], kBootArgs[3]});
  InstallScratchArena();
}

void PesFoxWorld::InstallScratchArena() {
  const uint64_t tlsArray = Alloc(8 * (static_cast<uint64_t>(Read32(kTlsIndexVa)) + 1));
  const uint64_t block = Alloc(kTlsBlockBytes), frame = Alloc(kScratchFrameBytes), arena = Alloc(kScratchArenaBytes);
  if (!tlsArray || !block || !frame || !arena) throw std::runtime_error("fox world: the arena is full");
  Write64(frame, kScratchFrameVtable);  // {vtable, 0, prev = 0, base, cursor = base, end, 0}
  Write64(frame + kScratchFrameBaseOffset, arena);
  Write64(frame + kScratchFrameCursorOffset, arena);
  Write64(frame + kScratchFrameEndOffset, arena + kScratchArenaBytes);
  Write64(block + kTlsSlotOffset, frame);
  Write64(tlsArray + 8 * static_cast<uint64_t>(Read32(kTlsIndexVa)), block);
  Write64(kTebVa + kTebTlsPointerOffset, tlsArray);
}

bool PesFoxWorld::PumpUntilIdle(std::string* error) {
  for (int step = 0; step < kPumpSteps; ++step) {
    Call(::pes_fn_141EA8430, {});
    if (Read64(kPumpCountAVa) == 0) return true;
    // The worker threads' part for every queued job whose handle waits for a stage.
    const uint64_t head = Read64(kPumpListAVa);
    for (uint64_t node = Read64(head); node != head; node = Read64(node)) {
      const uint64_t job = Read64(node + kPumpNodeJobOffset);
      const uint64_t handle = job ? Read64(job + kJobHandleOffset) : 0;
      if (!handle) continue;
      const uint8_t state = Read8(handle + kHandleStateOffset);
      if (state == kHandleQueuedRead) Call(::pes_fn_1414A9280, {handle});
      else if (state == kHandleQueuedDecode) Call(::pes_fn_1414A8F10, {handle});
    }
  }
  if (error) *error = "fox world: the pump's list never emptied in " + std::to_string(kPumpSteps) + " steps";
  return false;
}

bool PesFoxWorld::LoadBody(std::string* error) {
  pes_service_set_file_source(this);  // one source per process: the world that loads is the one the reader asks
  const uint64_t owner = Alloc(kOwnerBytes);
  if (!owner) throw std::runtime_error("fox world: the arena is full");
  Call(::pes_fn_141EC9AF0, {owner});
  if (!PumpUntilIdle(error)) return false;
  Call(::pes_fn_141ECA500, {owner});
  return true;
}

void PesFoxWorld::ConstructAnime(uint64_t p, uint64_t collision) {
  Call(::pes_fn_141EB5250, {p, NewCString(kBodyName), collision, kBodyCallbackVa});
}

void PesFoxWorld::RunFrame(uint64_t p) {
  const uint64_t a = Read64(p + kAnimeOffset);
  for (uint32_t phase : kFramePhases) Call(::pes_fn_141AF4650, {a, phase});
}

PesJointPose PesFoxWorld::ReadPose(uint64_t p) {
  const uint64_t a = Read64(p + kAnimeOffset), l1 = Read64(a + kControlOffset);
  const uint64_t positions = Read64(l1 + kPosePointerOffset), rotations = Read64(a + kQuatPointerOffset);
  PesState s = NewCall();
  PesJointPose pose;
  for (uint32_t j = 0; j < kAnimeJointCount; ++j)
    for (uint32_t k = 0; k < 4; ++k) {
      pose.position[j][k] = s.mem.rdf(positions + j * kJointEntryBytes + 4 * k);
      pose.rotation[j][k] = s.mem.rdf(rotations + j * kJointEntryBytes + 4 * k);
    }
  return pose;
}

uint64_t PesFoxWorld::HeapLiveBytes() const { return impl_->heap.LiveBytes(); }

uint64_t PesFoxWorld::StartJob(const std::string& path) { return Call(::pes_fn_141EA7190, {NewCString(path), kJobFlags}); }

bool PesFoxWorld::Finish(uint64_t job, std::string* error) {
  pes_service_set_file_source(this);
  for (int step = 0; step < kFinishSteps; ++step) {
    const uint64_t handle = Read64(job + kJobHandleOffset);
    if (handle != 0) {
      const uint8_t state = Read8(handle + kHandleStateOffset);
      if (state == kHandleQueuedRead) Call(::pes_fn_1414A9280, {handle});
      else if (state == kHandleQueuedDecode) Call(::pes_fn_1414A8F10, {handle});
    }
    if ((Call(::pes_fn_141EA7640, {job}) & 0xFF) != 0) return true;
  }
  if (error) *error = "fox world: the job did not finish in " + std::to_string(kFinishSteps) + " steps";
  return false;
}

}  // namespace pes
