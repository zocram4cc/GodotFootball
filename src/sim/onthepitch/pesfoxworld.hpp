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

  // The load layer's first frame: PES builds its singletons at game start; the image GF maps is a dump of the running game, so they hold
  // that game's heap pointers. Boot clears them and builds the same objects in GF's heap with PES's own constructors:
  //   the file manager FUN_14149A620 (FUN_14149AB60's alloc 0x2E0 + ctor), the file system object FUN_1414A3550, the two request managers
  //   FUN_1414B08A0 / FUN_1414B0E90, the job pump's lists (static initialisers FUN_1402223C0 / FUN_140222390) and the anime engine init
  //   FUN_141AE77B0 (the ResManager FUN_141B02800 and the type loaders), called as the game's boot does (seq001: rcx 0, rdx 1, r8 0, r9 0).
  // ponytail: FUN_14149AB60 also initialises the CRI library (FUN_14149ABE0) and starts its worker threads (FUN_1414B11A0, FUN_1414B1D90);
  // GF has no CRI library (the six CRI Fs calls under the reader are GF's file seam) and serves the request lists itself (ServeFile), so
  // those three calls are left out. Remove with a GF that runs PES's worker threads.
  void Boot();
  // The job maker FUN_141EA7190 on `path` (a CRI path, "cpk_dat/common/anime/..."): PES's job object, 0 when its extension names no loader.
  // The job's first FUN_141EA7640 call (inside) opens the file handle and queues the request.
  uint64_t StartJob(const std::string& path);
  // The worker threads' part: the handle's reader FUN_1414A9280 (state 2, the CRI Fs calls answered from the VFS) and, when the reader
  // queued it, the decode stage FUN_1414A8F10 (state 5), then FUN_141EA7640 until the job answers done. Returns false (error set) when the
  // handle never reaches state 8 within a bounded number of steps.
  bool Finish(uint64_t job, std::string* error);
  uint8_t Read8(uint64_t va);
  uint64_t FileServes() const { return serves_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint64_t serves_ = 0;
};

}  // namespace pes
