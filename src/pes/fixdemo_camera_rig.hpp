#pragma once

// PES's FixDemo procedural camera on GF's memory: the FixDemoCamera object (constructor 0x141F7AE60, proven bit-exact in
// docs/ports/pes-port-ProcCamera3.md) and its per-frame engine 0x141F7BFE0, run unchanged from the translated layer in a
// window GF owns. A camera record with no CANM clip (record mode +0xB0 = 4, the goal scene's behind-the-goal telephoto) is
// posed by that engine, not by a clip: it configures the DmCameraScene at camera+0x38 from the record's zoom stages and
// style bytes, reads the targets' positions out of the CAMERA_TARGET registry and writes the pose into the params block
// 0x1437F4A40. The registry (pes_native.hpp PesCameraRegistry) and the allocator (PesHeap) are GF's own, like the selector's.
//
// What the rig holds, and what stands in for PES runtime state (each ponytail names the PES writer that replaces it):
//   - the install's .rdata and .data, read through PeImage: vtables, constants, the preset table 0x1434A0290.., fps 0x14351AE54;
//   - the camera manager [0x1437F4820] (render-layer objects, effect list [0x1437F4810]): none, see fixdemo_camera_rig.cpp.

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "pes_native.hpp"
#include "pes_state.hpp"

namespace pes {

class PeImage;

// The camera params block the engine fills (0x1437F4A40, FixDemoCamera::Update's output; docs/re/pes-re-camera-pose.md 1.4).
constexpr uint64_t kCameraParamsVa = 0x1437F4A40;
constexpr size_t kCameraParamsBytes = 0x60;
struct CameraPose {
  std::array<float, 3> target{}, eye{}, up{};  // params +0x00, +0x0C, +0x18: Fox space, metres
  float fovRad = 0, nearM = 0, farM = 0;       // +0x30, +0x34, +0x38
  std::array<uint8_t, kCameraParamsBytes> raw{};  // the whole block, for the oracle
};

class FixDemoCameraRig {
 public:
  // Null with *error when the install's image does not map the data sections.
  static std::unique_ptr<FixDemoCameraRig> Create(const PeImage& exe, std::string* error);
  ~FixDemoCameraRig();
  FixDemoCameraRig(const FixDemoCameraRig&) = delete;
  FixDemoCameraRig& operator=(const FixDemoCameraRig&) = delete;

  // Scene start for one procedural camera record (the 284 bytes of a .fdc camera record, pes::fdc::CameraRecord::raw):
  // constructs a FixDemoCamera on the arena, points camera+0x130 at a copy of the record and sets it procedural (0x141F7B630).
  // Returns the camera's index for Step.
  size_t AddCamera(const std::vector<uint8_t>& record);

  // The record's target slot `slot` (CAMERA_TARGET_02<slot>), as the scene's actor writes it each frame.
  void SetTarget(unsigned slot, const PesCameraRegistry::Target& target);

  // One engine frame of camera `camera`: the time setter (0x141F7B4A0) with `timeTicks` (1/60 s ticks of the step's player
  // clock), then 0x141F7BFE0 with the params block. The first call of a camera is its initialisation (camera+0x128 == 1).
  CameraPose Step(size_t camera, float timeTicks);

  PesState& State();

 private:
  struct Impl;
  explicit FixDemoCameraRig(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace pes
