#include "pes/fixdemo_camera_rig.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pes/exe.hpp"
#include "translated.hpp"

namespace pes {
namespace {

// Where GF's memory for the translated code sits (the selector's windows): host stack and scratch above PES's image.
constexpr uint64_t kStackVa = 0x10000000, kStackBytes = 0x20000;

// The install's data sections: constants and vtables (.rdata, read-only) and the statics (.data, writable, with the bytes the
// image starts with). Section extents are the image's own (the PE section table of PES2021.exe).
constexpr uint64_t kRdataVa = 0x14252F000, kRdataBytes = 0xE6C000;
constexpr uint64_t kDataVa = 0x14339B000, kDataBytes = 0x72A000;
constexpr size_t kReadChunk = 0x10000;

// FixDemoCamera (RTTI .?AVFixDemoCamera@@, vtable 0x142BB0D90): the size the arena block needs (docs/ports/pes-port-ProcCamera3.md
// section 4 item 3) and the fields the scene-start stores and the time setter touch.
constexpr uint64_t kCameraBytes = 0x228, kCameraAlign = 0x10;
constexpr uint64_t kCamState = 0x128, kCamRecord = 0x130, kCamTime = 0x214, kCamTimeStep = 0x218, kCamDirty = 0x220,
                   kCamTime0 = 0x210, kCamTime1 = 0x21C, kCamFlag2 = 0x222, kCamTarget = 0x224;
constexpr uint64_t kRecordTargetNo = 0xB1;  // record byte read by SetProcedural: the first target slot, sign-extended

// The camera manager singleton (read by 0x141E9BA30) and the effect list (0x141E92C20): PES's are render-layer owners; the
// engine hands them the record's layer-visibility flags (0x141E9D2F0, 0x141E9CE80) and reads nothing back that the pose uses.
// ponytail: GF has no PES render-layer objects, so the manager is a zeroed block whose layer table pointer (+0xA68) is null,
// which 0x141E9D2F0's first test (`if *(this + 0xA68) != 0`) skips whole, and the effect list is null. Removed when GF's
// render layers are PES's own objects (the writers of [0x1437F4820] and [0x1437F4810]).
constexpr uint64_t kManagerSlotVa = 0x1437F4820, kEffectListSlotVa = 0x1437F4810, kManagerBytes = 0x1000;

uint64_t ReadChunked(const PeImage& exe, uint64_t va, uint64_t n, std::vector<uint8_t>* out) {
  // Raw-size tails of a section are not in the file; a chunk the image does not map stays zero, as the loader zero-fills it.
  out->assign(n, 0);
  uint64_t mapped = 0;
  for (uint64_t off = 0; off < n; off += kReadChunk) {
    const uint64_t len = std::min<uint64_t>(kReadChunk, n - off);
    if (exe.ReadVa(va + off, len, out->data() + off)) mapped += len;
  }
  return mapped;
}

[[noreturn]] void Die(const char* what, uint64_t v) {
  std::fprintf(stderr, "PES_CAMERA_RIG: %s %#llx\n", what, static_cast<unsigned long long>(v));
  std::abort();
}

}  // namespace

struct FixDemoCameraRig::Impl {
  std::unique_ptr<PesHeap> heap = std::make_unique<PesHeap>();
  PesState s;
  std::unique_ptr<PesCameraRegistry> registry;
  std::vector<uint64_t> cameras;

  // A translated function is entered as the dispatch enters it: a return address pushed.
  void Call(void (*fn)(PesState&)) {
    s.gpr[4] -= 8;
    s.wr(s.gpr[4], 0, 8);
    fn(s);
    s.gpr[4] += 8;
  }

  uint64_t Alloc(uint64_t bytes, uint64_t align, const char* what) {
    const uint64_t va = heap->Alloc(bytes, align);
    if (!va) Die(what, bytes);
    return va;
  }
};

std::unique_ptr<FixDemoCameraRig> FixDemoCameraRig::Create(const PeImage& exe, std::string* error) {
  auto impl = std::make_unique<Impl>();
  PesState& s = impl->s;
  impl->heap->Attach(s);
  std::vector<uint8_t> rdata, data;
  if (ReadChunked(exe, kRdataVa, kRdataBytes, &rdata) == 0 || ReadChunked(exe, kDataVa, kDataBytes, &data) == 0) {
    if (error) *error = "the image does not map the data sections the FixDemo camera reads";
    return nullptr;
  }
  s.mem.img.push_back({kRdataVa, std::move(rdata)});
  s.mem.segs.push_back({kDataVa, std::move(data)});
  s.mem.segs.push_back({kStackVa, std::vector<uint8_t>(kStackBytes)});
  s.gpr[4] = kStackVa + kStackBytes - 0x100;
  impl->registry = std::make_unique<PesCameraRegistry>(*impl->heap, s);
  s.wr(kManagerSlotVa, impl->Alloc(kManagerBytes, PesHeap::kMinAlign, "no room for the camera manager stand-in"), 8);
  s.wr(kEffectListSlotVa, 0, 8);
  return std::unique_ptr<FixDemoCameraRig>(new FixDemoCameraRig(std::move(impl)));
}

FixDemoCameraRig::FixDemoCameraRig(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
FixDemoCameraRig::~FixDemoCameraRig() = default;
PesState& FixDemoCameraRig::State() { return impl_->s; }

size_t FixDemoCameraRig::AddCamera(const std::vector<uint8_t>& record) {
  Impl& m = *impl_;
  PesState& s = m.s;
  const uint64_t cam = m.Alloc(kCameraBytes, kCameraAlign, "no room for a FixDemoCamera");
  const uint64_t rec = m.Alloc(record.size(), PesHeap::kMinAlign, "no room for a camera record");
  for (size_t i = 0; i < record.size(); ++i) s.wr(rec + i, record[i], 1);
  // The constructor 0x141F7AE60 (rcx = the block; rdx, r8, r9 only reach the allocator's unused arguments).
  s.gpr[1] = cam;
  s.gpr[2] = s.gpr[8] = s.gpr[9] = 0;
  m.Call(&pes_fn_141F7AE60);
  s.wr(cam + kCamRecord, rec, 8);
  // SetProcedural 0x141F7B630 (vtable 0x142BB0D90 slot +0x50; not in the translated layer: seven stores and a load): the
  // first target byte of the record becomes camera+0x224 (sign-extended), the clocks restart, the engine's next call is its
  // initialisation (+0x128 = 1) and the dirty flags (+0x220, +0x222) are raised.
  // ponytail: written here from the disassembly; removed when 0x141F7B630 is translated (no capture of it: the setter is not hooked).
  s.wr(cam + kCamTime, 0, 8);
  s.wr(cam + kCamTime1, 0, 4);
  s.wr(cam + kCamTime0, 0, 4);
  s.wr(cam + kCamTarget, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(s.rd(rec + kRecordTargetNo, 1)))), 4);
  s.wr(cam + kCamState, 1, 4);
  s.wr(cam + kCamDirty, 1, 1);
  s.wr(cam + kCamFlag2, 1, 1);
  m.cameras.push_back(cam);
  return m.cameras.size() - 1;
}

void FixDemoCameraRig::SetTarget(unsigned slot, const PesCameraRegistry::Target& target) {
  impl_->registry->Set(impl_->s, slot, target);
}

CameraPose FixDemoCameraRig::Step(size_t camera, float timeTicks) {
  Impl& m = *impl_;
  PesState& s = m.s;
  if (camera >= m.cameras.size()) Die("no such camera", camera);
  const uint64_t cam = m.cameras[camera];
  // The time setter 0x141F7B4A0 (vtable slot +0x48) for a procedural camera (+0x128 > 0): the dirty word becomes 0x100, the
  // time is stored and the step is its difference from the old time. ponytail: as SetProcedural, not in the layer.
  if (s.rd(cam + kCamDirty, 1) != 0) s.wr(cam + kCamDirty, 0x100, 2);
  const float old = s.rdf(cam + kCamTime);
  s.wrf(cam + kCamTime, timeTicks);
  s.wrf(cam + kCamTimeStep, timeTicks - old);
  s.gpr[1] = cam;
  s.gpr[2] = kCameraParamsVa;
  m.Call(&pes_fn_141F7BFE0);
  CameraPose pose;
  for (size_t i = 0; i < kCameraParamsBytes; ++i) pose.raw[i] = static_cast<uint8_t>(s.rd(kCameraParamsVa + i, 1));
  auto f3 = [&](uint64_t off) { return std::array<float, 3>{s.rdf(kCameraParamsVa + off), s.rdf(kCameraParamsVa + off + 4), s.rdf(kCameraParamsVa + off + 8)}; };
  pose.target = f3(0x00);
  pose.eye = f3(0x0C);
  pose.up = f3(0x18);
  pose.fovRad = s.rdf(kCameraParamsVa + 0x30);
  pose.nearM = s.rdf(kCameraParamsVa + 0x34);
  pose.farM = s.rdf(kCameraParamsVa + 0x38);
  return pose;
}

}  // namespace pes
