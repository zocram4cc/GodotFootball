#include "onthepitch/pesfoxworld.hpp"

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

}  // namespace pes
