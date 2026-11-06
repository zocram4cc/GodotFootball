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

}  // namespace

struct PesFoxWorld::Impl {
  const PesVfs& vfs;
  uint8_t* image = nullptr;
  uint64_t imageBytes = 0;
  PesHeap heap;
  std::vector<uint8_t> stack, teb;
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
  std::memcpy(impl_->teb.data() + kTebSelfOffset, &kTebVa, sizeof kTebVa);
  pes_service_set_file_source(this);
}

PesFoxWorld::~PesFoxWorld() {
  pes_service_set_file_source(nullptr);
  ::munmap(impl_->image, impl_->imageBytes);
}

PesState PesFoxWorld::NewCall() {
  PesState s;
  s.mem.flat = impl_->image;
  s.mem.flat_base = kImageBase;
  s.mem.flat_size = impl_->imageBytes;
  impl_->heap.Attach(s);  // clears the services, so the world's windows follow
  s.mem.services.push_back({kStackVa, kStackBytes, nullptr, impl_->stack.data()});
  s.mem.services.push_back({kTebVa, kTebBytes, nullptr, impl_->teb.data()});
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
constexpr uint8_t kHandleQueuedRead = 2, kHandleQueuedDecode = 5;
constexpr int kFinishSteps = 16;

}  // namespace

void PesFoxWorld::Boot() {
  for (uint64_t va : {kFileManagerVa, kRequestManagerAVa, kRequestManagerBVa, kFileSystemVa, kPumpListAVa, kPumpCountAVa, kPumpListBVa,
                      kPumpCountBVa, kResManagerVa})
    Write64(va, 0);
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
}

uint64_t PesFoxWorld::StartJob(const std::string& path) { return Call(::pes_fn_141EA7190, {NewCString(path), kJobFlags}); }

bool PesFoxWorld::Finish(uint64_t job, std::string* error) {
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
