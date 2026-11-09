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

#include "onthepitch/pesanimechain.hpp"
#include "pes/movestep.hpp"
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
  // The scratch-arena frame the anime engine's phases need on the calling thread (docs/ports/pes-port-AnimeLayer.md section 2): the
  // TLS slot +0x38 of the block [0x1437067A0] indexes holds a root frame {vtable 0x14258BCA8, 0, 0, base, base, end, 0} over a 256 KiB
  // arena, which FUN_1403EFDF0 pushes child frames on. PES gives each engine thread one; the match thread had none. Boot calls it.
  void InstallScratchArena();
  // The job pump FUN_141EA8430 and the worker stages it waits on, run until the pending list is empty and the finished resources have
  // moved into the ResManager (the pump drains them when the list empties). False (error set) when a job never completes.
  bool PumpUntilIdle(std::string* error);
  // The match-load sequence of the body animations (PES's, translated): the FoxAni preload FUN_141EC9AF0 queues every animation file
  // (Body mtar x10, skeletons, Face / Hand / Eye) on an owner object, the pump loads them, and the project registrar FUN_141ECA500 runs
  // the registry insert FUN_141EC86D0("body", "cpk_dat/common/anime/FoxAnim/Body/", "Project.hkx") that builds the BodyAnimControlResource.
  // ponytail: the owner is a zeroed block. PES's is the game scene object FUN_141E8B400 builds (its subsystems are not translated); the
  // preload and the registrar read only its lock at +0x48 and its list pointer at +0x70 and write the list there. Remove with that scene.
  bool LoadBody(std::string* error);
  // The player's anime constructor FUN_141EB5250(P, "body", collision, callback) on a player block `p` of the world's heap: the
  // BodyAnimControl at P+0xA08 and everything under it. `collision` is the player's collision object (FUN_1406E03D0 passes *(P+0x3AA0)).
  void ConstructAnime(uint64_t p, uint64_t collision);
  // One engine frame of a constructed player: BodyAnimControl::Exec (FUN_141AF4650) for each phase of the frame in order (INF: the
  // anime system's pipeline, FUN_141AE9010's loops, runs a phase over every control before the next; for one control that is this
  // order). The control's frame time step is the one the constructor stored (FUN_141AF7A90, L2+0x44).
  void RunFrame(uint64_t p);
  // SetMotion's flush into the control (FUN_1406E5FC0 at 0x1406E6268 and 0x1406E6283): the record pusher FUN_141AF61C0(A, record, params, layer 0)
  // that appends the clip's 0x30-byte layer record to the control's vector, then SetSpeed FUN_141AF6E40(A, rate, 0, -1) that sets the rate of
  // every record. `request` is SetMotion FUN_1406F0F70's argument list (motion, startTime, mirror, playback = rate, duration = blend and the
  // transition whose kind byte SetMotion stores); its group must be -1 (SetParametric's records are not pushed here). A motion above 0x1124
  // is dropped, as SetMotion's first test does.
  void IssueMotion(uint64_t p, const MotionRequest& request);
  // The pose the frame left in the control (L1+0x50: joint positions, A+0x20: joint rotations): the arrays FUN_141EB49B0 reads.
  PesJointPose ReadPose(uint64_t p);
  // Bytes of the world's heap in live blocks (the arena a world needs).
  uint64_t HeapLiveBytes() const;
  uint8_t Read8(uint64_t va);
  void Write8(uint64_t va, uint8_t value);
  uint32_t Read32(uint64_t va);
  void Write32(uint64_t va, uint32_t value);
  uint64_t FileServes() const { return serves_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint64_t serves_ = 0;
};

}  // namespace pes
