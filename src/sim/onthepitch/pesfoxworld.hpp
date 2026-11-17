// PES's fox file-load chain on a world of its own (ticket AnimeLayer4): the job pump FUN_141EA8430, the job maker FUN_141EA7190, the job
// FUN_141EA7640 and the file handle under them (FUN_1414A89E0, its reader FUN_1414A9280 and decode stage FUN_1414A8F10) are PES's
// translated code. They run over a PesMemory (pesmemory.hpp): a private copy-on-write mapping of the image (their globals: the file layer's
// managers, the pump's lists, the ResManager singleton), PES's own allocator on GF's heap (PesHeap), a stack and a thread block of their own,
// and GF's file seam: where the reader asks the CRI Fs API for a file, PesFoxWorld answers from the install's VFS (the six pes_service_fs_*
// calls, pes_native.hpp). The bytes PES parses are the install's own, verbatim.
//
// The anime engine's world is booted and loaded once per process and frozen (AnimeWorldTemplate); each match's world is a clone of that
// template: the same memory, copy-on-write over the frozen pages, in the same host state.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "onthepitch/pesanimechain.hpp"
#include "onthepitch/pesmemory.hpp"
#include "pes/movestep.hpp"
#include "pes/vfs.hpp"
#include "pes_native.hpp"
#include "pes_state.hpp"

namespace pes {

class PesFoxWorld;
// One member of a series: a clip (motion number), its start time in seconds and its playback rate (PES's series entry, 0x38 bytes).
struct SeriesMember {
  int motion = -1;
  float startTime = 0.0f;
  float rate = 1.0f;
};

// A series as its caller passes it to SetParametric (FUN_141AF52B0): the members, the descriptor's rate table (one float per member), the
// position on that table (the descriptor's +0x24), and the parameter block's kind byte and blend.
struct SeriesSpec {
  std::vector<SeriesMember> members;
  std::vector<float> rates;
  float position = 0.0f;  // the descriptor's +0x24: the series' position on the rate table (FUN_141AEE840's x)
  uint8_t paramKind = 0;
  float paramBlend = 0.0f;
};

// The fox world frozen after its Boot and LoadBody (PesFoxWorld::Freeze): the memory's template and the world's host state. A PesFoxWorld
// built from it is a clone. Move-only.
class PesFoxWorldTemplate {
 public:
  PesFoxWorldTemplate(PesFoxWorldTemplate&&) = default;
  PesFoxWorldTemplate& operator=(PesFoxWorldTemplate&&) = default;
  PesFoxWorldTemplate(const PesFoxWorldTemplate&) = delete;
  PesFoxWorldTemplate& operator=(const PesFoxWorldTemplate&) = delete;

  // The bytes each region of the frozen memory holds dirty (what the template costs as memfds).
  PesMemoryTemplate::Footprint Footprint() const { return memory_.Dirty(); }

 private:
  friend class PesFoxWorld;
  PesFoxWorldTemplate(PesMemoryTemplate memory, uint64_t serves, uint64_t initializersRun, uint64_t issueScratch);
  PesMemoryTemplate memory_;
  uint64_t serves_ = 0, initializersRun_ = 0, issueScratch_ = 0;
};

class PesFoxWorld final : public PesFileSource {
 public:
  // imagePath: the unpacked image (PES_EXE). heapBytes: the arena PES's allocator serves from. Throws std::runtime_error when the image cannot
  // be mapped.
  PesFoxWorld(const std::string& imagePath, const PesVfs& vfs, uint64_t heapBytes);
  // A clone of a frozen world (AnimeWorldTemplate): the memory is copy-on-write over the frozen pages, the host state is the frozen one.
  PesFoxWorld(const PesFoxWorldTemplate& frozen, const PesVfs& vfs);
  ~PesFoxWorld() override;
  PesFoxWorld(const PesFoxWorld&) = delete;
  PesFoxWorld& operator=(const PesFoxWorld&) = delete;

  // The memory the world runs in (its image, heap, stack and thread block).
  PesMemory& Memory();
  // This world frozen, after its Boot and LoadBody (no call running): the template a clone is built from.
  PesFoxWorldTemplate Freeze();

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

  // The load layer's first frame: PesBoot (pesboot.hpp) on the world's memory. The dump's singletons are cleared and built in the memory's
  // own heap with PES's constructors, as the game's start-up builds them. LoadBody runs the match-load sequence after it.
  void Boot();
  uint64_t InitializersRun() const { return initializersRun_; }
  // The job maker FUN_141EA7190 on `path` (a CRI path, "cpk_dat/common/anime/..."): PES's job object, 0 when its extension names no loader.
  // The job's first FUN_141EA7640 call (inside) opens the file handle and queues the request.
  uint64_t StartJob(const std::string& path);
  // The worker threads' part: the handle's reader FUN_1414A9280 (state 2, the CRI Fs calls answered from the VFS) and, when the reader
  // queued it, the decode stage FUN_1414A8F10 (state 5), then FUN_141EA7640 until the job answers done. Returns false (error set) when the
  // handle never reaches state 8 within a bounded number of steps.
  bool Finish(uint64_t job, std::string* error);
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

// SetParametric's series (FUN_141AF52B0, called by the parametric flush FUN_1406E5FC0 at 0x1406E6325): one record per member, pushed to the control
// and blended by the layer, looping. The request's playback is the rate every record takes (SetSpeed); the series' position on the rate table
// is SeriesSpec.position (the descriptor's +0x24). ponytail: the caller supplies the series (members, rate table, position, parameter block);
// PES builds them from the blend group in the request filler FUN_1406F14E0, which is not translated yet.
void IssueSeries(uint64_t p, const SeriesSpec& series, const MotionRequest& request);
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
  uint64_t initializersRun_ = 0;
};

// The trap world's loader data on a booted and loaded memory: the shared regions and the pointer globals that AnimeWorldTemplate freezes
// (the template's state is the boot, the body and this). Exposed so a reference boot can carry the same state. Throws std::runtime_error
// when the install's trap data cannot be read.
void AddTrapLoaderData(PesMemory& memory, const PesVfs& vfs, const std::string& imagePath);

// The anime engine's booted and loaded world, frozen: built once per process for this image, install and heap size (the first caller boots
// and loads a world, freezes it and drops it), and every world made from it is a clone. Throws std::runtime_error when the load fails.
const PesFoxWorldTemplate& AnimeWorldTemplate(const std::string& imagePath, const PesVfs& vfs, uint64_t heapBytes);

}  // namespace pes
