#include "onthepitch/pesfoxworld.hpp"

#include "onthepitch/pesboot.hpp"
#include "onthepitch/pestrapdata.hpp"
#include "pes/translated/translated.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <cstring>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <vector>

namespace pes {
namespace {

// The singletons the dump holds from the running game (each names a heap object that does not exist in GF's world). Writers: the file
// manager 0x14149A6A3, the managers 0x1414B0530 / 0x1414B0C40, the file system object 0x1414A2738, the pump's list heads 0x1402223DC /
// 0x1402223AC, the ResManager 0x141AE77B0 (flag at 0x141AE78F0) and its singleton cell DAT_1437CEAA0 (FUN_141B02800).
constexpr uint64_t kFileManagerVa = 0x143704E40, kRequestManagerAVa = 0x143705DC8, kRequestManagerBVa = 0x143705DD0,
                   kFileSystemVa = 0x143704F18, kPumpListAVa = 0x1437F48A8, kPumpCountAVa = 0x1437F48B0, kPumpListBVa = 0x1437F48B8,
                   kPumpCountBVa = 0x1437F48C0, kResManagerVa = 0x1437CEAA0, kAnimeInitFlagVa = 0x1437CEA18;
// FUN_141EA7190's flags (FUN_141EA8430 passes 0x20009 for a queued name).
constexpr uint64_t kJobFlags = 0x20009;
constexpr uint64_t kJobHandleOffset = 0x38, kHandleStateOffset = 8;
// The player's anime object and its parts (docs/ports/pes-port-AnimeLayer3.md section 2): A = *(P+0xA08), L1 = *(A+0x88) the AnimControl,
// the joint position array pointer at L1+0x50 and the joint rotation array pointer at A+0x20 (what FUN_141EB49B0 reads).
constexpr uint64_t kAnimeOffset = 0xA08, kControlOffset = 0x88, kPosePointerOffset = 0x50, kQuatPointerOffset = 0x20;
// The phases one frame runs on a body control, ascending (docs/ports/pes-port-AnimeLayer.md section 2: the driver's switch).
constexpr uint32_t kFramePhases[] = {1, 2, 3, 4, 5, 7, 8, 9, 0xB, 0xD, 0xF, 0x10, 0x11};
constexpr uint8_t kHandleQueuedRead = 2, kHandleQueuedDecode = 5;
constexpr int kFinishSteps = 16;
// The registry (FUN_141EC8C40's singleton DAT_1437F4F78) and the project registrars' once-flags (FUN_141ECA500 reads DAT_1437F4F82), which
// the dump holds as the running game left them. The anime systems table DAT_1437CEA30 is left as the dump holds it (a system is made only for
// a null cell, and a made one allocates through PES's jemalloc, whose state is the dead game's: see the report, section 5 item 1).
constexpr uint64_t kRegistryVa = 0x1437F4F78, kRegistrarFlagVa = 0x1437F4F82;
// The pump's queue node: the job at +0x10 (FUN_141EA8430), the job's handle at +0x38.
constexpr uint64_t kPumpNodeJobOffset = 0x10;
constexpr int kPumpSteps = 4096;
// The constructor's callback argument: FUN_1406E03D0 calls thunk_FUN_141EB5250(P, "body", P[0x754], &LAB_141B970E0) (decompile): an
// image code address.
constexpr uint64_t kBodyCallbackVa = 0x141B970E0;
// The preload owner (see LoadBody): the preload and the registrar touch its lock at +0x48 and its list pointer at +0x70.
constexpr uint64_t kOwnerBytes = 0x200;
constexpr char kBodyName[] = "body";
// SetMotion's layer record, the 0x38-byte block FUN_141AF61C0 reads as its second argument (decompile of the pusher and of the constructor's own
// push, FUN_141EB5250's local_c0..): {+0 resolved resource (0: the pusher looks the motion up), +8 motion u32, +0xC start time f32, +0x10 mirror
// byte, +0x14 rate f32 (DAT_14256AE1C = 1.0 in the constructor's idle record), +0x18 flags (bit 0: resource resolved), +0x20 weight count u32,
// +0x28 weight array pointer, +0x30 byte, +0x34 u32 -1}. The parameter block (third argument, P+0x3B80 in SetMotion): {+0 u32, +4 kind byte, +8
// blend f32}.
constexpr uint64_t kRecordBytes = 0x38, kParamsOffset = 0x40, kIssueScratchBytes = 0x60;
constexpr uint64_t kRecMotion = 0x8, kRecStartTime = 0xC, kRecMirror = 0x10, kRecRate = 0x14, kRecSlotMark = 0x34;
constexpr uint64_t kParamKind = 0x4, kParamBlend = 0x8;
constexpr uint32_t kRecNoSlot = 0xFFFFFFFFu;
// FUN_1406F0F70: `if (0x1124 < motion) return`; the kind byte it stores at P+0x3B84 for its transition argument (1 -> 1, 2 -> 2, 3 -> 6, else 0).
constexpr int kMaxMotion = 0x1124;
constexpr uint8_t kKindByTransition[] = {0, 1, 2, 6};
// The pusher's fifth argument ([rsp+0x28] in a Win64 call) is read only by the upper-layer arm (layer 1: a switch over it); SetMotion's flush
// passes zero there (the high bits of a double masked off, FUN_1406E5FC0 at 0x1406E6268).
constexpr uint64_t kFifthArgumentOffset = 0x28;
// SetSpeed FUN_141AF6E40(A, rate in xmm1, layer 0, all records 0xFFFFFFFF).
constexpr uint32_t kAllRecords = 0xFFFFFFFFu;
constexpr uint64_t kUnitFloatVa = 0x14256AE1C;  // DAT_14256AE1C, 1.0f: the template's rate (FUN_141EB5250 stores it in the idle record)

// SetParametric's series descriptor (rdx, 0x28 bytes used): {+0 count u32, +8 entries pointer, +0x10 rate count u32, +0x18 rate table
// pointer (one f32 per member), +0x20 byte (1 in all eight captures: SetParametric stores it in its state at +0x18), +0x21 kind byte,
// +0x22 flags byte (1 in all eight captures of live2/pid661338 seq001-008), +0x24 blend f32}. The parameter block (r8) is SetMotion's:
// kParamKind and kParamBlend; kSeriesParamsBytes is the block we zero (its fields end at +0xC).
constexpr uint64_t kSeriesDescBytes = 0x28, kDescCount = 0x0, kDescEntries = 0x8, kDescRateCount = 0x10, kDescRates = 0x18;
constexpr uint64_t kDescByte20 = 0x20, kDescKind = 0x21, kDescFlags = 0x22, kDescBlend = 0x24;
constexpr uint8_t kByte20 = 1;
constexpr uint8_t kSeriesFlags = 1;
constexpr uint64_t kRateBytes = sizeof(float), kSeriesParamsBytes = 0x10;

// The bit pattern of a float, as the image stores it.
uint64_t FloatBits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, sizeof b);
  return static_cast<uint64_t>(b);
}

}  // namespace

struct PesFoxWorld::Impl {
  const PesVfs& vfs;
  PesMemory memory;
  uint64_t issueScratch = 0;  // IssueMotion's record and parameter block in the heap (made on the first issue)
  Impl(const PesVfs& v, const std::string& imagePath, uint64_t heapBytes) : vfs(v), memory(imagePath, heapBytes) {}
  Impl(const PesVfs& v, const PesMemoryTemplate& frozen) : vfs(v), memory(frozen) {}
};

PesFoxWorld::PesFoxWorld(const std::string& imagePath, const PesVfs& vfs, uint64_t heapBytes)
    : impl_(std::make_unique<Impl>(vfs, imagePath, heapBytes)) {
  pes_service_set_file_source(this);
}

PesFoxWorld::PesFoxWorld(const PesFoxWorldTemplate& frozen, const PesVfs& vfs)
    : impl_(std::make_unique<Impl>(vfs, frozen.memory_)), serves_(frozen.serves_), initializersRun_(frozen.initializersRun_) {
  impl_->issueScratch = frozen.issueScratch_;
  pes_service_set_file_source(this);
}

PesFoxWorld::~PesFoxWorld() { pes_service_set_file_source(nullptr); }

PesFoxWorldTemplate::PesFoxWorldTemplate(PesMemoryTemplate memory, uint64_t serves, uint64_t initializersRun, uint64_t issueScratch)
    : memory_(std::move(memory)), serves_(serves), initializersRun_(initializersRun), issueScratch_(issueScratch) {}

PesFoxWorldTemplate PesFoxWorld::Freeze() {
  return PesFoxWorldTemplate(impl_->memory.Freeze(), serves_, initializersRun_, impl_->issueScratch);
}

// The trap world's loader data (the motion table, the Animetable, the registry, trap.o, the pad manager and the anime chain's layers),
// built from the install (PesTrapSource): shared regions (every clone maps them), and the pointer globals written into the memory.
// The clip database is not one of them: that global holds the booted ResManager, which the trap chain reads as PES's own.
void AddTrapLoaderData(PesMemory& memory, const PesVfs& vfs, const std::string& imagePath) {
  std::string error;
  const auto pe = PeImage::Open(vfs.Root() + "/PES2021.exe", &error);
  if (!pe) throw std::runtime_error("PES anime engine: " + error);
  const auto source = LoadTrapSource(*pe, vfs, imagePath, &error);
  if (!source) throw std::runtime_error("PES anime engine: " + error);
  PesTrapExternals loader;
  source->loader.AddTo(loader);
  for (const PesTrapSegment& seg : loader.segments) memory.AddSharedRegion(seg.va, seg.bytes);
  for (const auto& [va, value] : loader.globals)
    if (va != kClipDatabaseGlobalVa) memory.Write64(va, value);
}

const PesFoxWorldTemplate& AnimeWorldTemplate(const std::string& imagePath, const PesVfs& vfs, uint64_t heapBytes) {
  static std::mutex mutex;
  static std::map<std::string, std::unique_ptr<PesFoxWorldTemplate>> templates;
  const std::string key = imagePath + '\n' + vfs.Root() + '\n' + std::to_string(heapBytes);
  std::lock_guard<std::mutex> lock(mutex);
  const auto found = templates.find(key);
  if (found != templates.end()) return *found->second;
  PesFoxWorld boot(imagePath, vfs, heapBytes);
  boot.Boot();
  std::string error;
  if (!boot.LoadBody(&error)) throw std::runtime_error("PES anime engine: " + error);
  AddTrapLoaderData(boot.Memory(), vfs, imagePath);
  auto frozen = std::make_unique<PesFoxWorldTemplate>(boot.Freeze());
  const PesFoxWorldTemplate& ref = *frozen;
  templates.emplace(key, std::move(frozen));
  return ref;
}

PesMemory& PesFoxWorld::Memory() { return impl_->memory; }

PesState PesFoxWorld::NewCall() { return impl_->memory.NewCall(); }

uint64_t PesFoxWorld::Call(void (*fn)(PesState&), std::initializer_list<uint64_t> args) { return impl_->memory.Call(fn, args); }

uint64_t PesFoxWorld::Read64(uint64_t va) { return impl_->memory.Read64(va); }

void PesFoxWorld::Write64(uint64_t va, uint64_t value) { impl_->memory.Write64(va, value); }

std::string PesFoxWorld::ReadCString(uint64_t va, uint64_t max) {
  PesState s = NewCall();
  std::string out;
  for (uint64_t c = s.rd(va, 1); c != 0 && out.size() < max; c = s.rd(va + out.size(), 1)) out.push_back(static_cast<char>(c));
  return out;
}

uint64_t PesFoxWorld::Alloc(uint64_t bytes) { return impl_->memory.Alloc(bytes); }

uint64_t PesFoxWorld::NewCString(const std::string& text) {
  const uint64_t va = impl_->memory.Alloc(text.size() + 1);
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

void PesFoxWorld::Write8(uint64_t va, uint8_t value) {
  PesState s = NewCall();
  s.wr(va, value, 1);
}

uint32_t PesFoxWorld::Read32(uint64_t va) { return impl_->memory.Read32(va); }

void PesFoxWorld::Write32(uint64_t va, uint32_t value) { impl_->memory.Write32(va, value); }

void PesFoxWorld::Boot() { initializersRun_ = PesBoot(impl_->memory, impl_->vfs.Root()); }

bool PesFoxWorld::PumpUntilIdle(std::string* error) {
  for (int step = 0; step < kPumpSteps; ++step) {
    Call(::pes_fn_141EA8430, {});
    if (Read64(kPumpCountAVa) == 0) return true;
    // The worker threads' part for every queued job whose handle waits for a stage.
    const uint64_t head = Read64(kPumpListAVa);
    for (uint64_t node = Read64(head); node != head; node = Read64(node)) {
      const uint64_t job = Read64(node + kPumpNodeJobOffset);
      const uint64_t handle = job ? Read64(job + kJobHandleOffset) : 0;
      if (!handle) continue;
      const uint8_t state = Read8(handle + kHandleStateOffset);
      if (state == kHandleQueuedRead) Call(::pes_fn_1414A9280, {handle});
      else if (state == kHandleQueuedDecode) Call(::pes_fn_1414A8F10, {handle});
    }
  }
  if (error) *error = "fox world: the pump's list never emptied in " + std::to_string(kPumpSteps) + " steps";
  return false;
}

bool PesFoxWorld::LoadBody(std::string* error) {
  pes_service_set_file_source(this);  // one source per process: the world that loads is the one the reader asks
  const uint64_t owner = Alloc(kOwnerBytes);
  if (!owner) throw std::runtime_error("fox world: the arena is full");
  Call(::pes_fn_141EC9AF0, {owner});
  if (!PumpUntilIdle(error)) return false;
  // The list's completion callback (FUN_141EC9AB0) runs the FixDemo project's setup, which registers "fixdemo" (FUN_141EBCB10 looks it up
  // and fills the joint-id tables DAT_1437F4E60 from it) before the body's own setup.
  Call(::pes_fn_141ECA210, {owner});
  Call(::pes_fn_141ECA500, {owner});
  return true;
}

void PesFoxWorld::ConstructAnime(uint64_t p, uint64_t collision) {
  Call(::pes_fn_141EB5250, {p, NewCString(kBodyName), collision, kBodyCallbackVa});
}

void PesFoxWorld::RunFrame(uint64_t p) {
  const uint64_t a = Read64(p + kAnimeOffset);
  for (uint32_t phase : kFramePhases) Call(::pes_fn_141AF4650, {a, phase});
}

void PesFoxWorld::IssueMotion(uint64_t p, const MotionRequest& request) {
  if (request.group >= 0) throw std::runtime_error("fox world: IssueMotion takes SetMotion's arguments (group -1)");
  if (request.motion > kMaxMotion) return;
  if (impl_->issueScratch == 0) {
    impl_->issueScratch = Alloc(kIssueScratchBytes);
    if (impl_->issueScratch == 0) throw std::runtime_error("fox world: the arena is full");
  }
  const uint64_t a = Read64(p + kAnimeOffset), record = impl_->issueScratch, params = record + kParamsOffset;
  const auto bits = FloatBits;
  PesState s = NewCall();
  for (uint64_t at = 0; at < kIssueScratchBytes; at += 8) s.wr(record + at, 0, 8);
  s.wr(record + kRecMotion, static_cast<uint32_t>(request.motion), 4);
  s.wr(record + kRecStartTime, bits(request.startTime), 4);
  s.wr(record + kRecMirror, request.mirror ? 1 : 0, 1);
  s.wr(record + kRecRate, s.rd(kUnitFloatVa, 4), 4);
  s.wr(record + kRecSlotMark, kRecNoSlot, 4);
  const bool known = request.transition >= 0 && request.transition < static_cast<int>(std::size(kKindByTransition));
  s.wr(params + kParamKind, known ? kKindByTransition[request.transition] : 0, 1);
  s.wr(params + kParamBlend, bits(request.duration), 4);
  s.gpr[1] = a;
  s.gpr[2] = record;
  s.gpr[8] = params;
  s.gpr[9] = 0;  // layer 0
  s.wr(s.gpr[4] + kFifthArgumentOffset, 0, 4);
  ::pes_fn_141AF61C0(s);
  PesState speed = NewCall();
  speed.gpr[1] = a;
  speed.set_x_f32(1, request.playback);
  speed.gpr[8] = 0;  // layer 0
  speed.gpr[9] = kAllRecords;
  ::pes_fn_141AF6E40(speed);
}

void PesFoxWorld::IssueSeries(uint64_t p, const SeriesSpec& series, const MotionRequest& request) {
  const std::size_t count = series.members.size();
  if (count == 0) throw std::runtime_error("fox world: IssueSeries needs at least one member");
  if (series.rates.size() != count) throw std::runtime_error("fox world: IssueSeries needs one rate per member");
  const uint64_t desc = Alloc(kSeriesDescBytes), entries = Alloc(count * kRecordBytes), rates = Alloc(count * kRateBytes),
                 params = Alloc(kSeriesParamsBytes);
  if (desc == 0 || entries == 0 || rates == 0 || params == 0) throw std::runtime_error("fox world: the arena is full");
  const uint64_t a = Read64(p + kAnimeOffset);
  PesState s = NewCall();
  const auto zero = [&s](uint64_t va, uint64_t bytes) {
    for (uint64_t at = 0; at < bytes; at += 4) s.wr(va + at, 0, 4);
  };
  zero(desc, kSeriesDescBytes);
  zero(entries, count * kRecordBytes);
  zero(rates, count * kRateBytes);
  zero(params, kSeriesParamsBytes);
  const bool known = request.transition >= 0 && request.transition < static_cast<int>(std::size(kKindByTransition));
  s.wr(desc + kDescCount, count, 4);
  s.wr(desc + kDescEntries, entries, 8);
  s.wr(desc + kDescRateCount, count, 4);
  s.wr(desc + kDescRates, rates, 8);
  s.wr(desc + kDescByte20, kByte20, 1);
  s.wr(desc + kDescKind, known ? kKindByTransition[request.transition] : 0, 1);
  s.wr(desc + kDescFlags, kSeriesFlags, 1);
  s.wr(desc + kDescBlend, FloatBits(request.duration), 4);
  for (std::size_t i = 0; i < count; ++i) {
    const SeriesMember& member = series.members[i];
    const uint64_t entry = entries + i * kRecordBytes;
    s.wr(entry + kRecMotion, static_cast<uint32_t>(member.motion), 4);
    s.wr(entry + kRecStartTime, FloatBits(member.startTime), 4);
    s.wr(entry + kRecRate, FloatBits(member.rate), 4);
    s.wr(entry + kRecSlotMark, kRecNoSlot, 4);
    s.wr(rates + i * kRateBytes, FloatBits(series.rates[i]), 4);
  }
  s.wr(params + kParamKind, series.paramKind, 1);
  s.wr(params + kParamBlend, FloatBits(series.paramBlend), 4);
  s.gpr[1] = a;
  s.gpr[2] = desc;
  s.gpr[8] = params;
  ::pes_fn_141AF52B0(s);
  PesState speed = NewCall();
  speed.gpr[1] = a;
  speed.set_x_f32(1, request.playback);
  speed.gpr[8] = 0;  // layer 0
  speed.gpr[9] = kAllRecords;
  ::pes_fn_141AF6E40(speed);
}

PesJointPose PesFoxWorld::ReadPose(uint64_t p) {
  const uint64_t a = Read64(p + kAnimeOffset), l1 = Read64(a + kControlOffset);
  const uint64_t positions = Read64(l1 + kPosePointerOffset), rotations = Read64(a + kQuatPointerOffset);
  PesState s = NewCall();
  PesJointPose pose;
  for (uint32_t j = 0; j < kAnimeJointCount; ++j)
    for (uint32_t k = 0; k < 4; ++k) {
      pose.position[j][k] = s.mem.rdf(positions + j * kJointEntryBytes + 4 * k);
      pose.rotation[j][k] = s.mem.rdf(rotations + j * kJointEntryBytes + 4 * k);
    }
  return pose;
}

uint64_t PesFoxWorld::HeapLiveBytes() const { return impl_->memory.HeapLiveBytes(); }

uint64_t PesFoxWorld::StartJob(const std::string& path) { return Call(::pes_fn_141EA7190, {NewCString(path), kJobFlags}); }

bool PesFoxWorld::Finish(uint64_t job, std::string* error) {
  pes_service_set_file_source(this);
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
