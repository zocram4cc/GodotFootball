// PES's memory as GF runs it (OneWorld cut 1): one class owns the image mapping, PES's heap, the stack and the thread block, and the OS
// arena. The fox world (pesfoxworld.hpp) and its boot (pesboot.hpp) run on it. PesImageMapping is the image part the trap memory maps too
// (pestrapdata.hpp); the trap world's heap, stack and window move into this class in cut 3, when the two worlds share one memory.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "pes_native.hpp"
#include "pes_state.hpp"

namespace pes {

// A private, copy-on-write mapping of the unpacked image (PES_EXE): the chain writes image globals, and the file stays as it is. Throws
// std::runtime_error with `who` as the prefix when the image cannot be opened or mapped.
class PesImageMapping {
 public:
  PesImageMapping(const std::string& imagePath, const std::string& who);
  ~PesImageMapping();
  PesImageMapping(const PesImageMapping&) = delete;
  PesImageMapping& operator=(const PesImageMapping&) = delete;

  uint8_t* data() const { return data_; }
  uint64_t bytes() const { return bytes_; }

 private:
  uint8_t* data_ = nullptr;
  uint64_t bytes_ = 0;
};

// The image and the windows PES's process has around it: the stack and the thread block (gs), the OS arena (VirtualAlloc's range), and
// the heap PES's allocator serves from. NewCall maps all of them.
class PesMemory {
 public:
  static constexpr uint64_t kImageBase = 0x140000000ULL;
  static constexpr uint64_t kStackVa = 0x2600000000ULL, kStackBytes = 0x100000;
  static constexpr uint64_t kTebVa = 0x2610000000ULL, kTebBytes = 0x2000;
  // import_stubs.json "os_arena": PES's jemalloc gets its chunks from VirtualAlloc, which the translated model serves from this range (the
  // cell at its base is the bump pointer): the memory maps it, or the allocator reads zeros and never returns.
  static constexpr uint64_t kOsArenaVa = 0x3000000000ULL, kOsArenaBytes = 0x100000000ULL;

  // imagePath: the unpacked image. heapBytes: the arena PES's allocator serves from. Throws std::runtime_error when the image cannot be
  // mapped or the OS arena cannot be reserved.
  PesMemory(const std::string& imagePath, uint64_t heapBytes);
  ~PesMemory();
  PesMemory(const PesMemory&) = delete;
  PesMemory& operator=(const PesMemory&) = delete;

  uint8_t* Image() const { return image_.data(); }
  uint64_t ImageBytes() const { return image_.bytes(); }

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
  PesImageMapping image_;
  PesHeap heap_;
  std::vector<uint8_t> stack_, teb_;
  uint8_t* osArena_ = nullptr;
};

}  // namespace pes
