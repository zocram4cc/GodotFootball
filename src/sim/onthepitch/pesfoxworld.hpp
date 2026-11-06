// PES's fox file-load chain on a world of its own (ticket AnimeLayer4): the job pump FUN_141EA8430, the job maker FUN_141EA7190, the job
// FUN_141EA7640 and the file handle under them (FUN_1414A89E0, its reader FUN_1414A9280 and decode stage FUN_1414A8F10) are PES's
// translated code. They run over a private copy-on-write mapping of the image (their globals: the file layer's managers, the pump's lists,
// the ResManager singleton), PES's own allocator on GF's heap (PesHeap), a stack and a thread block of their own, and GF's file seam:
// where the reader asks the CRI Fs API for a file, PesFoxWorld answers from the install's VFS (the six pes_service_fs_* calls,
// pes_native.hpp). The bytes PES parses are the install's own, verbatim.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>

#include "pes/vfs.hpp"
#include "pes_native.hpp"
#include "pes_state.hpp"

namespace pes {

class PesFoxWorld final : public PesFileSource {
 public:
  // imagePath: the unpacked image (PES_EXE). heapBytes: the arena PES's allocator serves from. Throws std::runtime_error when the
  // image cannot be mapped.
  PesFoxWorld(const std::string& imagePath, const PesVfs& vfs, uint64_t heapBytes);
  ~PesFoxWorld() override;
  PesFoxWorld(const PesFoxWorld&) = delete;
  PesFoxWorld& operator=(const PesFoxWorld&) = delete;

  // A call state over the world: the image mapping, the heap, the stack and the thread block (gs), rsp at a return slot.
  PesState NewCall();
  // Runs `fn` with rcx, rdx, r8, r9 = args (up to four) and returns rax. The world's heap, image globals and file source persist.
  uint64_t Call(void (*fn)(PesState&), std::initializer_list<uint64_t> args);

  uint64_t Read64(uint64_t va);
  void Write64(uint64_t va, uint64_t value);
  std::string ReadCString(uint64_t va, uint64_t max = 4096);
  // Writes `text` and its NUL in the heap and returns its VA (a path argument).
  uint64_t NewCString(const std::string& text);

  // The seam (PesFileSource): the CRI path as PES's reader formed it, looked up in the VFS.
  int64_t Size(const char* path) override;
  // A zeroed block of the world's heap (0 when the arena is full), as PES's allocator hands them out.
  uint64_t Alloc(uint64_t bytes);
  int64_t Read(const char* path, uint64_t offset, uint8_t* dst, uint64_t n) override;

  uint64_t FileServes() const { return serves_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint64_t serves_ = 0;
};

}  // namespace pes
