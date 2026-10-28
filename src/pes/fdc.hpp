#pragma once

// FixDemo cut file (.fdc) reader: the container, the cut block's records (Camera and Timing decoded,
// every record's bytes claimed by its size) and the embedded CANM camera streams. Layout and oracle:
// docs/re/pes-re-fdc-format.md. The coverage rule mirrors tools/dev/cutscene/fdc_parse.py
// (CutsceneSelect branch): every byte of a file is claimed once, or is a zero pool/padding byte.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pes/vfs.hpp"

namespace pes::fdc {

// Record type bytes (docs/re/pes-re-fdc-format.md §2.1). 99 is the editor-disabled record.
inline constexpr uint8_t kCut = 0, kEnv = 1, kTiming = 2, kEffect = 3, kHuman = 4, kObject = 5, kCamera = 6,
                         kFunction = 7, kHumanControl = 8, kDisabled = 99;

// Nominal record size of a type byte, or 0 for a byte the loader does not know.
uint32_t NominalSize(uint8_t type);

struct Resource {
  std::string name;             // the entry's name (cpk path)
  bool external = false;        // size == 4: a stub that names a VFS file
  std::string vfsPath;          // external: the name with its leading cpk_dat/ stripped
  std::vector<uint8_t> data;    // embedded: the bytes
};

// Type 6, 284 bytes. Offsets from the record start.
struct CameraRecord {
  uint32_t key = 0;                      // +0x00: the map key (FUN_141f369f0 compares it)
  float startTime = 0;                   // +0x04: startCutTime, in 1/60 s ticks (docs; see report §3)
  uint16_t originalCameraId = 0;         // +0x08
  uint16_t envDataId = 0;                // +0x0a
  std::string animePath;                 // +0x0c char[128]: the CANM clip, "<cat>/canm/<clip>.canm"
  float animeStartTime = 0;              // +0x8c
  float nearClip = 0;                    // +0x90: overrides the clip's near channel
  float farClip = 0;                     // +0x94: overrides the clip's far channel
  float nearClipScale = 0;               // +0x98
  float correctRateForHeight = 0;        // +0xa0
  uint32_t functionNo = 0;               // +0xa8
  std::array<uint8_t, 4> targetBytes{};  // +0xb0 raw: targetKind and targetNo[3] (0xff = none)
  uint32_t zoomingTimeRate = 0;          // +0xb4
  std::array<int32_t, 8> zoom{};         // +0xb8: ZOOM_HEADUP..FULLSHOT_X4, ticks, -1 = off
};

// Type 2, 92 bytes.
struct TimingRecord {
  uint32_t word0 = 0;                    // +0x00: type in the high 16 bits, id in the low 16 (INF)
  float startTime = 0, endTime = 0;      // +0x04, +0x08: ticks
  int32_t targetNo = 0, paramNo1 = 0, paramNo2 = 0;  // +0x0c, +0x10, +0x14
  float value = 0;                       // +0x18
  std::string actionName;                // +0x1c char[64]: e.g. demo_goal_level3
};

struct Coverage {
  size_t gapBytes = 0;                   // unclaimed, non-padding bytes
  size_t padBytes = 0;                   // zero bytes in the block's pool tail, unreferenced
  size_t overlapBytes = 0;               // bytes claimed twice
  std::vector<size_t> gapOffsets;        // the first gaps, for diagnosis (at most 8)
};

struct File {
  std::string name;                      // the cut block's own path (entry 0)
  std::vector<Resource> resources;       // entries 1..n-1
  std::vector<uint8_t> recordTypes;      // one per cut-block record, table order
  std::vector<CameraRecord> cameras;     // type 6, table order
  std::vector<TimingRecord> timings;     // type 2, table order
  Coverage coverage;

  bool Consumed() const { return coverage.gapBytes == 0 && coverage.overlapBytes == 0; }
  bool HasRecordType(uint8_t type) const;
  // The resource a camera's animePath names (cpk_dat/ ignored), or nullptr.
  const Resource* FindResource(const std::string& path) const;
};

// Parses one .fdc. Returns false with *err set when a structure points outside the file or a record
// type is unknown. A file that parses but leaves bytes unclaimed returns true with Consumed() false.
bool Parse(const std::vector<uint8_t>& bytes, File* out, std::string* err);

// Every cut_data/*.fdc under common/demo/fixdemo in the mounted VFS, original-case paths.
std::vector<std::string> ListCutFiles(const PesVfs& vfs);

// CANM v1 camera stream: 30 fps, six channels (0 rotation quat, 1 position vec4, 2 vertical fov in
// degrees, 3 near, 4 far, 5 aspect), per docs/re/pes-re-cutscenes.md §3 and
// docs/PES21_CAMERA_TRACE.md §4.
struct Canm {
  uint32_t frameCount = 0;               // last frame index; keys are 0..frameCount
  uint32_t frameRate = 0;
  struct Channel {
    std::vector<uint16_t> frames;        // key frame indices, ascending
    std::vector<std::array<float, 4>> values;  // type 1/4: four floats; type 8: values[i][0]
  };
  std::array<Channel, 6> channels;
};

bool ParseCanm(const std::vector<uint8_t>& bytes, Canm* out, std::string* err);

struct CameraSample {
  std::array<float, 3> position{};       // Fox space: Y up, metres, pitch-centre origin
  std::array<float, 4> rotation{};       // quaternion x, y, z, w; camera looks down local -Z
  float fovDeg = 0;                      // vertical, degrees
  float nearM = 0, farM = 0, aspect = 0;
};

// The pose at a (possibly fractional) frame: position and fov linear, rotation slerped (the sign
// flipped to the short arc). Frames before the first key or after the last hold the end value.
CameraSample SampleCanm(const Canm& c, float frame);

}  // namespace pes::fdc
