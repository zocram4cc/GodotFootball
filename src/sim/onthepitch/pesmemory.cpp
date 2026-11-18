#include "onthepitch/pesmemory.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
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
// gs:[0x58] is the thread's TLS array (one block pointer per module index). The exe's index is the image global DAT_1437067a0 (0 in the
// image and in the live capture pid3778393/seq061); its block sits in the TEB window, and its +0x38 is the head of PES's scope list
// (FUN_1403EFDF0 links a scope into it, FUN_1403EFEB0 pops it), empty between ticks (CONFIRMED in seq061).
constexpr uint64_t kTebTlsPointerOffset = 0x58, kTlsArrayTebOffset = 0x1000, kTlsBlockTebOffset = 0x1100, kTlsIndexVa = 0x1437067A0ULL;
constexpr uint64_t kTlsArrayEntries = (kTlsBlockTebOffset - kTlsArrayTebOffset) / sizeof(uint64_t);
// The granule of a frozen region's dirty runs: the page.
constexpr uint64_t kFrozenPage = 0x1000;
const uint8_t kZeroPage[kFrozenPage] = {};

// Writes all `n` bytes of `p` into `fd` at `off`.
void WriteAll(int fd, const uint8_t* p, uint64_t n, uint64_t off) {
  while (n > 0) {
    const ssize_t w = ::pwrite(fd, p, n, static_cast<off_t>(off));
    if (w < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error(std::string("PES template: cannot write the memfd: ") + std::strerror(errno));
    }
    p += w;
    off += static_cast<uint64_t>(w);
    n -= static_cast<uint64_t>(w);
  }
}

// The image's frozen pages against the install's file (the image's base): the file is mapped read-only for the comparison.
PesFrozenRegion CaptureOverFile(const uint8_t* live, uint64_t bytes, const std::string& path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  struct stat st {};
  if (fd < 0 || ::fstat(fd, &st) != 0 || static_cast<uint64_t>(st.st_size) != bytes) {
    if (fd >= 0) ::close(fd);
    throw std::runtime_error("PES template: the image " + path + " is not the file the memory was mapped from");
  }
  void* base = ::mmap(nullptr, bytes, PROT_READ, MAP_PRIVATE, fd, 0);
  ::close(fd);
  if (base == MAP_FAILED) throw std::runtime_error("PES template: cannot map the image " + path);
  PesFrozenRegion region = PesFrozenRegion::Capture(live, bytes, static_cast<const uint8_t*>(base));
  ::munmap(base, bytes);
  return region;
}

}  // namespace

PesFrozenRegion::PesFrozenRegion(PesFrozenRegion&& other) noexcept
    : fd_(other.fd_), bytes_(other.bytes_), runs_(std::move(other.runs_)) {
  other.fd_ = -1;
}

PesFrozenRegion& PesFrozenRegion::operator=(PesFrozenRegion&& other) noexcept {
  if (this != &other) {
    if (fd_ >= 0) ::close(fd_);
    fd_ = other.fd_;
    bytes_ = other.bytes_;
    runs_ = std::move(other.runs_);
    other.fd_ = -1;
  }
  return *this;
}

PesFrozenRegion::~PesFrozenRegion() {
  if (fd_ >= 0) ::close(fd_);
}

PesFrozenRegion PesFrozenRegion::Capture(const uint8_t* live, uint64_t bytes, const uint8_t* base) {
  PesFrozenRegion region;
  region.bytes_ = bytes;
  region.fd_ = ::memfd_create("pes-template", MFD_CLOEXEC);
  if (region.fd_ < 0) throw std::runtime_error(std::string("PES template: cannot create a memfd: ") + std::strerror(errno));
  if (::ftruncate(region.fd_, static_cast<off_t>(bytes)) != 0)
    throw std::runtime_error(std::string("PES template: cannot size a memfd: ") + std::strerror(errno));
  for (uint64_t off = 0; off < bytes; off += kFrozenPage) {
    const uint64_t n = std::min<uint64_t>(kFrozenPage, bytes - off);
    const uint8_t* p = live + off;
    if (std::memcmp(p, base ? base + off : kZeroPage, n) == 0) continue;
    WriteAll(region.fd_, p, n, off);
    if (!region.runs_.empty() && region.runs_.back().first + region.runs_.back().second == off) region.runs_.back().second += n;
    else region.runs_.push_back({off, n});
  }
  return region;
}

void PesFrozenRegion::LayOver(uint8_t* mapped) const {
  for (const auto& run : runs_) {
    void* p = ::mmap(mapped + run.first, run.second, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd_, static_cast<off_t>(run.first));
    if (p == MAP_FAILED) throw std::runtime_error(std::string("PES template: cannot lay the frozen pages: ") + std::strerror(errno));
  }
}

uint64_t PesFrozenRegion::DirtyBytes() const {
  uint64_t n = 0;
  for (const auto& run : runs_) n += run.second;
  return n;
}

PesAnonMapping::PesAnonMapping(uint64_t bytes, bool noReserve) : bytes_(bytes) {
  const int flags = MAP_PRIVATE | MAP_ANONYMOUS | (noReserve ? MAP_NORESERVE : 0);
  void* p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, flags, -1, 0);
  if (p == MAP_FAILED) throw std::runtime_error("PES memory: cannot map " + std::to_string(bytes) + " bytes");
  data_ = static_cast<uint8_t*>(p);
}

PesAnonMapping::~PesAnonMapping() {
  if (data_) ::munmap(data_, bytes_);
}

PesImageMapping::PesImageMapping(const std::string& imagePath, const std::string& who, const PesFrozenRegion* frozen) : path_(imagePath) {
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
  if (frozen) {
    if (frozen->Bytes() != bytes_) {
      ::munmap(data_, bytes_);
      throw std::runtime_error(who + ": the frozen image is not the size of " + imagePath);
    }
    try {
      frozen->LayOver(data_);
    } catch (...) {
      ::munmap(data_, bytes_);
      throw;
    }
  }
}

PesImageMapping::~PesImageMapping() {
  if (data_) ::munmap(data_, bytes_);
}

PesMemoryTemplate::Footprint PesMemoryTemplate::Dirty() const {
  uint64_t shared = 0;
  for (const PesFrozenShared& s : shared_) shared += s.data.DirtyBytes();
  return Footprint{image_.DirtyBytes(), heapPage_.DirtyBytes(), heapArena_.DirtyBytes(), stack_.DirtyBytes(), teb_.DirtyBytes(),
                   osArena_.DirtyBytes(), shared};
}

PesMemory::PesMemory(const std::string& imagePath, uint64_t heapBytes)
    : image_(imagePath, "PES memory"),
      heap_(heapBytes),
      stack_(kStackBytes, false),
      teb_(kTebBytes, false),
      osArena_(kOsArenaBytes, true) {
  std::memcpy(teb_.data() + kTebSelfOffset, &kTebVa, sizeof kTebVa);
  const uint64_t pebVa = kTebVa + kPebOffsetInTeb, takenBits = (1ULL << kTlsSlotsTakenAtStart) - 1;
  std::memcpy(teb_.data() + kTebPebPointerOffset, &pebVa, sizeof pebVa);
  std::memcpy(teb_.data() + kPebOffsetInTeb + kPebTlsBitmapOffset, &takenBits, sizeof takenBits);
  // The exe's TLS block (gs:[0x58] -> the array, array[index] -> the block; its +0x38 starts empty, as between the captured ticks).
  uint32_t tlsIndex = 0;
  std::memcpy(&tlsIndex, image_.data() + (kTlsIndexVa - kImageBase), sizeof tlsIndex);
  if (tlsIndex >= kTlsArrayEntries) throw std::runtime_error("PES memory: the exe's TLS index is past the TEB's TLS array");
  const uint64_t tlsArrayVa = kTebVa + kTlsArrayTebOffset, tlsBlockVa = kTebVa + kTlsBlockTebOffset;
  std::memcpy(teb_.data() + kTebTlsPointerOffset, &tlsArrayVa, sizeof tlsArrayVa);
  std::memcpy(teb_.data() + kTlsArrayTebOffset + tlsIndex * sizeof tlsBlockVa, &tlsBlockVa, sizeof tlsBlockVa);
}

PesMemory::PesMemory(const PesMemoryTemplate& frozen)
    : image_(frozen.imagePath_, "PES memory", &frozen.image_),
      heap_(frozen.heapArena_.Bytes(), PesHeap::Adopt{}),
      stack_(kStackBytes, false),
      teb_(kTebBytes, false),
      osArena_(kOsArenaBytes, true) {
  frozen.heapPage_.LayOver(heap_.PageData());
  frozen.heapArena_.LayOver(heap_.ArenaData());
  heap_.RestoreBookkeeping(frozen.heap_);
  frozen.stack_.LayOver(stack_.data());
  frozen.teb_.LayOver(teb_.data());
  frozen.osArena_.LayOver(osArena_.data());
  for (const PesFrozenShared& s : frozen.shared_) {
    shared_.push_back(SharedRegion{s.va, s.bytes, std::make_unique<PesAnonMapping>(s.bytes, false)});
    s.data.LayOver(shared_.back().map->data());
  }
}

PesMemory::~PesMemory() = default;

PesMemoryTemplate PesMemory::Freeze() {
  PesMemoryTemplate frozen;
  frozen.imagePath_ = image_.path();
  frozen.image_ = CaptureOverFile(image_.data(), image_.bytes(), image_.path());
  frozen.heapPage_ = PesFrozenRegion::Capture(heap_.PageData(), heap_.PageBytes(), nullptr);
  frozen.heapArena_ = PesFrozenRegion::Capture(heap_.ArenaData(), heap_.ArenaBytes(), nullptr);
  frozen.stack_ = PesFrozenRegion::Capture(stack_.data(), stack_.bytes(), nullptr);
  frozen.teb_ = PesFrozenRegion::Capture(teb_.data(), teb_.bytes(), nullptr);
  frozen.osArena_ = PesFrozenRegion::Capture(osArena_.data(), osArena_.bytes(), nullptr);
  frozen.heap_ = heap_.SaveBookkeeping();
  for (const SharedRegion& r : shared_) {
    PesFrozenShared s;
    s.va = r.va;
    s.bytes = r.bytes;
    s.data = PesFrozenRegion::Capture(r.map->data(), r.bytes, nullptr);
    frozen.shared_.push_back(std::move(s));
  }
  return frozen;
}

void PesMemory::AddSharedRegion(uint64_t va, const std::vector<uint8_t>& bytes) {
  shared_.push_back(SharedRegion{va, bytes.size(), std::make_unique<PesAnonMapping>(bytes.size(), false)});
  if (!bytes.empty()) std::memcpy(shared_.back().map->data(), bytes.data(), bytes.size());
}

std::vector<PesMem::Window> PesMemory::SharedWindows() const {
  std::vector<PesMem::Window> windows;
  for (const SharedRegion& r : shared_) {
    PesMem::Window w;
    w.base = r.va;
    w.size = r.bytes;
    w.rw = r.map->data();
    windows.push_back(w);
  }
  return windows;
}

uint8_t* PesMemory::AddWindow(uint64_t va, uint64_t bytes) {
  windows_.push_back(std::make_unique<PesAnonMapping>(bytes, false));
  matchWindow_.base = va;
  matchWindow_.size = bytes;
  matchWindow_.rw = windows_.back()->data();
  return matchWindow_.rw;
}

PesState PesMemory::NewCall() {
  PesState s;
  s.mem.flat = image_.data();
  s.mem.flat_base = kImageBase;
  s.mem.flat_size = image_.bytes();
  heap_.Attach(s);  // clears the services, so the memory's windows follow
  s.mem.services.push_back({kStackVa, kStackBytes, nullptr, stack_.data()});
  s.mem.services.push_back({kTebVa, kTebBytes, nullptr, teb_.data()});
  s.mem.services.push_back({kOsArenaVa, kOsArenaBytes, nullptr, osArena_.data()});
  if (matchWindow_.rw != nullptr) s.mem.services.push_back(matchWindow_);  // the match's window: the trap world's records and the players' blocks
  s.gs_base = kTebVa;
  s.gpr[4] = kStackVa + kStackBytes - kEntryFrameBytes;
  s.wr(s.gpr[4], 0, 8);  // the return address slot
  return s;
}

uint64_t PesMemory::Call(void (*fn)(PesState&), std::initializer_list<uint64_t> args) {
  PesState s = NewCall();
  static constexpr int kArgRegs[4] = {1, 2, 8, 9};  // rcx, rdx, r8, r9
  // Win64: the fifth and later arguments sit above the caller's 0x20-byte shadow space, at [rsp + 0x20] and up, when the call is made;
  // the callee reads them at [rsp + 0x28] on entry, past the return address the call pushes.
  static constexpr uint64_t kShadowBytes = 0x20;
  if (args.size() > 8) throw std::runtime_error("PES memory: more than eight arguments");
  int i = 0;
  for (uint64_t a : args) {
    if (i < 4) s.gpr[kArgRegs[i]] = a;
    else s.wr(s.gpr[4] + kShadowBytes + 8 * static_cast<uint64_t>(i - 4), a, 8);
    ++i;
  }
  if (i > 4) {
    s.gpr[4] -= 8;
    s.wr(s.gpr[4], 0, 8);  // the return address (the translated calls push 0)
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
