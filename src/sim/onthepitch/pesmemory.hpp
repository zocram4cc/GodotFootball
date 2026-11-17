// PES's memory as GF runs it (OneWorld). One class owns the image mapping, PES's heap, the stack and the thread block, the OS arena, the
// shared regions (the trap world's loader data and anime chain, built once per process in the template) and the window storage of a match
// (the trap world's window, per match). The fox world (pesfoxworld.hpp) and its boot (pesboot.hpp) run on it; a match's trap world runs on
// the same memory (pesworldrecord.hpp: BindMemory), so the trap chain reads the booted image and the heap the anime engine built.
//
// A PesMemory can be frozen after its boot (Freeze) into a PesMemoryTemplate: each region's dirty pages go to a memfd (sparse), and the
// heap's bookkeeping is kept. A PesMemory built from the template is a clone: each region is its base (the install's file for the image,
// zeros for the rest) with the frozen pages laid over it, copy-on-write. Clones share the template's pages and never write to them.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "pes_native.hpp"
#include "pes_state.hpp"

namespace pes {

// A region's frozen bytes: the pages that differ from the region's base (the install's file for the image, zeros for the rest), in a memfd.
// The memfd is sparse: only the dirty pages are allocated. Move-only.
class PesFrozenRegion {
 public:
  PesFrozenRegion() = default;
  PesFrozenRegion(PesFrozenRegion&& other) noexcept;
  PesFrozenRegion& operator=(PesFrozenRegion&& other) noexcept;
  ~PesFrozenRegion();
  PesFrozenRegion(const PesFrozenRegion&) = delete;
  PesFrozenRegion& operator=(const PesFrozenRegion&) = delete;

  // The pages of `live` (`bytes` long) that differ from `base` (null: from zeros). Throws std::runtime_error when the memfd cannot be made.
  static PesFrozenRegion Capture(const uint8_t* live, uint64_t bytes, const uint8_t* base);
  // Lays the frozen pages over `mapped`: a private, writable mapping of at least Bytes() bytes whose other bytes are the frozen base.
  void LayOver(uint8_t* mapped) const;

  uint64_t Bytes() const { return bytes_; }
  uint64_t DirtyBytes() const;

 private:
  int fd_ = -1;
  uint64_t bytes_ = 0;
  std::vector<std::pair<uint64_t, uint64_t>> runs_;  // (offset, length) of the dirty runs, page-aligned, in offset order
};

// An anonymous private mapping: zero bytes, committed as they are touched (MAP_NORESERVE when asked). Throws std::runtime_error when the
// mapping fails.
class PesAnonMapping {
 public:
  PesAnonMapping(uint64_t bytes, bool noReserve);
  ~PesAnonMapping();
  PesAnonMapping(const PesAnonMapping&) = delete;
  PesAnonMapping& operator=(const PesAnonMapping&) = delete;

  uint8_t* data() const { return data_; }
  uint64_t bytes() const { return bytes_; }

 private:
  uint8_t* data_ = nullptr;
  uint64_t bytes_ = 0;
};

// A private, copy-on-write mapping of the unpacked image (PES_EXE). Throws std::runtime_error with `who` as the prefix when the image cannot
// be opened or mapped. With `frozen`, its dirty pages are laid over the mapping (the image of a clone).
class PesImageMapping {
 public:
  PesImageMapping(const std::string& imagePath, const std::string& who, const PesFrozenRegion* frozen = nullptr);
  ~PesImageMapping();
  PesImageMapping(const PesImageMapping&) = delete;
  PesImageMapping& operator=(const PesImageMapping&) = delete;

  uint8_t* data() const { return data_; }
  uint64_t bytes() const { return bytes_; }
  const std::string& path() const { return path_; }

 private:
  std::string path_;
  uint8_t* data_ = nullptr;
  uint64_t bytes_ = 0;
};

// A shared region frozen with the template: its VA, its size and its dirty pages. Move-only.
struct PesFrozenShared {
  uint64_t va = 0;
  uint64_t bytes = 0;
  PesFrozenRegion data;
};

// PES's memory frozen after its boot (PesMemory::Freeze): the dirty pages of every region, the shared regions and the heap's bookkeeping.
// Move-only.
class PesMemoryTemplate {
 public:
  // The bytes each region holds dirty.
  struct Footprint {
    uint64_t image, heapPage, heapArena, stack, teb, osArena, shared;
  };
  PesMemoryTemplate() = default;
  PesMemoryTemplate(PesMemoryTemplate&&) = default;
  PesMemoryTemplate& operator=(PesMemoryTemplate&&) = default;
  PesMemoryTemplate(const PesMemoryTemplate&) = delete;
  PesMemoryTemplate& operator=(const PesMemoryTemplate&) = delete;

  Footprint Dirty() const;

 private:
  friend class PesMemory;
  std::string imagePath_;
  PesFrozenRegion image_, heapPage_, heapArena_, stack_, teb_, osArena_;
  PesHeap::Bookkeeping heap_;
  std::vector<PesFrozenShared> shared_;
};

class PesMemory {
 public:
  static constexpr uint64_t kImageBase = 0x140000000ULL;
  static constexpr uint64_t kStackVa = 0x2600000000ULL, kStackBytes = 0x100000;
  static constexpr uint64_t kTebVa = 0x2610000000ULL, kTebBytes = 0x2000;
  // import_stubs.json "os_arena": PES's jemalloc gets its chunks from VirtualAlloc, which the translated model serves from this range (the
  // cell at its base is the bump pointer): the memory maps it, or the allocator reads zeros and never returns.
  static constexpr uint64_t kOsArenaVa = 0x3000000000ULL, kOsArenaBytes = 0x100000000ULL;

  // The memory of a fresh boot: the image mapped privately, the heap and the windows zeroed (PesBoot fills them). Throws std::runtime_error
  // when the image cannot be mapped or the OS arena cannot be reserved.
  PesMemory(const std::string& imagePath, uint64_t heapBytes);
  // A clone of a frozen memory (see PesMemoryTemplate): every region holds the frozen bytes, copy-on-write.
  explicit PesMemory(const PesMemoryTemplate& frozen);
  ~PesMemory();
  PesMemory(const PesMemory&) = delete;
  PesMemory& operator=(const PesMemory&) = delete;

  uint8_t* Image() const { return image_.data(); }
  uint64_t ImageBytes() const { return image_.bytes(); }
  PesHeap& Heap() { return heap_; }
  uint8_t* Stack() const { return stack_.data(); }
  uint8_t* Teb() const { return teb_.data(); }
  uint8_t* OsArena() const { return osArena_.data(); }

  // This memory frozen (no call running): every region's dirty pages go to memfds; the memory itself is unchanged.
  PesMemoryTemplate Freeze();

  // A region outside the image and the heap, at `va`, copied from `bytes` (the trap world's loader data). Before Freeze: the template
  // freezes it, and every clone maps it. A call of a trap world on the memory maps it as a borrowed window (SharedWindows).
  void AddSharedRegion(uint64_t va, const std::vector<uint8_t>& bytes);
  // The shared regions as host windows (translated code addresses them by VA; nothing is copied).
  std::vector<PesMem::Window> SharedWindows() const;
  // Storage owned by this memory for a match's window at `va`: zeroed, `bytes` long, valid for the memory's lifetime. Not frozen. Every call
  // of the memory (NewCall) maps it, so the anime engine's calls on a player block inside the window (P) read and write the window's bytes,
  // as the trap world's calls do.
  uint8_t* AddWindow(uint64_t va, uint64_t bytes);

  // A call state over the memory: the image, the heap, the stack and thread block, the OS arena; rsp at a return slot.
  PesState NewCall();
  // Runs `fn` with rcx, rdx, r8, r9 = args (up to four) and returns rax. The image and the heap persist across calls.
  uint64_t Call(void (*fn)(PesState&), std::initializer_list<uint64_t> args);

  uint64_t Read64(uint64_t va);
  void Write64(uint64_t va, uint64_t value);
  uint32_t Read32(uint64_t va);
  void Write32(uint64_t va, uint32_t value);

  // A zeroed block of the heap, aligned as PES's allocator aligns it (0 when the arena is full).
  uint64_t Alloc(uint64_t bytes);
  uint64_t HeapLiveBytes() const;

 private:
  struct SharedRegion {
    uint64_t va;
    uint64_t bytes;
    std::unique_ptr<PesAnonMapping> map;
  };
  PesImageMapping image_;
  PesHeap heap_;
  PesAnonMapping stack_, teb_, osArena_;
  std::vector<SharedRegion> shared_;
  std::vector<std::unique_ptr<PesAnonMapping>> windows_;
  PesMem::Window matchWindow_;  // the match's window (AddWindow): mapped by every call
};

}  // namespace pes
