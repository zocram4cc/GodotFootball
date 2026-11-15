#include "onthepitch/pesmemory.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace pes {
namespace {

// Win64: rsp at a function's entry holds the return address and is 8 (mod 16); the shadow space and the stack arguments sit above it.
constexpr uint64_t kEntryFrameBytes = 0x200;
constexpr uint64_t kTebSelfOffset = 0x30;  // gs:[0x30]: the thread block's own address (FlsGetValue and the last-error model read it)
// The process block the TlsAlloc model reads (gs:[0x60] = PEB; PEB+0x328 = TlsBitmapBits, the first 64 slots): it sits at the end of the thread
// block's window. The bits are the slots taken when PES's allocator started: its three TlsAlloc calls returned 9, 10 and 11 (the live dump holds
// those at 0x143AC3DCC, 0x143AC2814 and 0x143AC2810, import_stubs.json), so slots 0..8 were taken.
constexpr uint64_t kTebPebPointerOffset = 0x60, kPebOffsetInTeb = 0x1800, kPebTlsBitmapOffset = 0x328, kTlsSlotsTakenAtStart = 9;

}  // namespace

PesImageMapping::PesImageMapping(const std::string& imagePath, const std::string& who) {
  const int fd = ::open(imagePath.c_str(), O_RDONLY);
  struct stat st {};
  if (fd < 0 || ::fstat(fd, &st) != 0) {
    if (fd >= 0) ::close(fd);
    throw std::runtime_error(who + ": cannot open the image " + imagePath);
  }
  bytes_ = static_cast<uint64_t>(st.st_size);
  // Private, copy-on-write: the chain writes image globals (the singletons, the lists), and the file stays as it is.
  void* map = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  ::close(fd);
  if (map == MAP_FAILED) throw std::runtime_error(who + ": cannot map the image " + imagePath);
  data_ = static_cast<uint8_t*>(map);
}

PesImageMapping::~PesImageMapping() {
  if (data_) ::munmap(data_, bytes_);
}

PesMemory::PesMemory(const std::string& imagePath, uint64_t heapBytes)
    : image_(imagePath, "PES memory"), heap_(heapBytes), stack_(kStackBytes), teb_(kTebBytes) {
  void* arena = ::mmap(nullptr, kOsArenaBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (arena == MAP_FAILED) throw std::runtime_error("PES memory: cannot map the OS arena");
  osArena_ = static_cast<uint8_t*>(arena);
  std::memcpy(teb_.data() + kTebSelfOffset, &kTebVa, sizeof kTebVa);
  const uint64_t pebVa = kTebVa + kPebOffsetInTeb, takenBits = (1ULL << kTlsSlotsTakenAtStart) - 1;
  std::memcpy(teb_.data() + kTebPebPointerOffset, &pebVa, sizeof pebVa);
  std::memcpy(teb_.data() + kPebOffsetInTeb + kPebTlsBitmapOffset, &takenBits, sizeof takenBits);
}

PesMemory::~PesMemory() { ::munmap(osArena_, kOsArenaBytes); }

PesState PesMemory::NewCall() {
  PesState s;
  s.mem.flat = image_.data();
  s.mem.flat_base = kImageBase;
  s.mem.flat_size = image_.bytes();
  heap_.Attach(s);  // clears the services, so the memory's windows follow
  s.mem.services.push_back({kStackVa, kStackBytes, nullptr, stack_.data()});
  s.mem.services.push_back({kTebVa, kTebBytes, nullptr, teb_.data()});
  s.mem.services.push_back({kOsArenaVa, kOsArenaBytes, nullptr, osArena_});
  s.gs_base = kTebVa;
  s.gpr[4] = kStackVa + kStackBytes - kEntryFrameBytes;
  s.wr(s.gpr[4], 0, 8);  // the return address slot
  return s;
}

uint64_t PesMemory::Call(void (*fn)(PesState&), std::initializer_list<uint64_t> args) {
  PesState s = NewCall();
  static constexpr int kArgRegs[4] = {1, 2, 8, 9};  // rcx, rdx, r8, r9
  int i = 0;
  for (uint64_t a : args) {
    if (i == 4) throw std::runtime_error("PES memory: more than four register arguments");
    s.gpr[kArgRegs[i++]] = a;
  }
  fn(s);
  return s.gpr[0];
}

uint64_t PesMemory::Read64(uint64_t va) { return NewCall().rd(va, 8); }

void PesMemory::Write64(uint64_t va, uint64_t value) {
  PesState s = NewCall();
  s.wr(va, value, 8);
}

uint32_t PesMemory::Read32(uint64_t va) { return static_cast<uint32_t>(NewCall().rd(va, 4)); }

void PesMemory::Write32(uint64_t va, uint32_t value) {
  PesState s = NewCall();
  s.wr(va, value, 4);
}

uint64_t PesMemory::Alloc(uint64_t bytes) { return heap_.Alloc(bytes, PesHeap::kMinAlign); }

uint64_t PesMemory::HeapLiveBytes() const { return heap_.LiveBytes(); }

}  // namespace pes
