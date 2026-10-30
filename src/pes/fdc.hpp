#pragma once

// FixDemo cut file (.fdc) reader: the container, the cut block's records (Camera and Timing decoded,
// every record's bytes claimed by its size) and the embedded CANM camera streams. Layout and oracle:
// docs/re/pes-re-fdc-format.md. The coverage rule mirrors tools/dev/cutscene/fdc_parse.py
// (CutsceneSelect branch): every byte of a file is claimed once, or is a zero pool/padding byte.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
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
  uint8_t timeMode = 0;                  // +0x9c: 0 = the player clock as is, else relative to startTime (0x141f421bb)
  uint8_t loop = 0;                      // +0x9d: wrap the frame at the clip length (0x141f7b4a0 +0x208)
  uint8_t useCutOffset = 0;              // +0xa6: the player's cut offset applies (0x141f39e56)
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

// Type 0, 64 bytes: the placement fields the player reads at cut start (0x141F2E11C..: the map node's +0x49 =
// record +0x19, +0x4C = +0x1C, +0x58 = +0x28, +0x5C = +0x2C; the node base is record - 0x30).
struct CutRecord {
  int32_t id = 0;                        // +0x00: the player's cut map key
  // The step length (0x141F2DD80 -> player+0x64, lines 195-199 of its decompile): node +0x38/+0x3C = record +0x08/+0x0C,
  // node +0x40 = +0x10 (player+0xAA8 speed); the node base is record - 0x30. Ticks of 1/60 s.
  float minTime = 0, maxTime = 0;        // +0x08, +0x0C
  float playSpeed = 0;                   // +0x10
  uint32_t functionNo = 0;               // +0x38: the function-number term 0x141F31D80 may override min/max (not decoded)
  bool useOffset = false;                // +0x19
  std::array<float, 3> offsetPos{};      // +0x1C
  float offsetRotYDeg = 0;               // +0x28
  bool offsetFlipX = false;              // +0x2C
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
  std::vector<CutRecord> cuts;           // type 0, table order
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

// CANM v1 camera stream (docs/re/pes-re-camera-pose.md §1): six channels selected by their semantic byte
// (0 rotation quaternion, 1 position, 2 vertical fov in degrees, 3 near, 4 far, 5 aspect).
// PES (ktCameraAnimation 0x142139D60) reads the length as s16(+4) frames and the frame rate as i32(+8)
// only when bit 0 of byte +0xC is set, else its default.
struct Canm {
  float length = 0;                      // (float)s16(+4): the last frame; evaluate clamps to [0, length]
  bool hasFps = false;                   // byte +0xC bit 0
  int32_t fps = 0;                       // i32(+8), meaningful when hasFps
  enum class Kind { kQuat, kVec, kScalar };  // interpolator, from the channel's type (table 0x143542620)
  struct Channel {
    Kind kind = Kind::kScalar;
    std::vector<uint16_t> frames;        // key times, ascending; no table = one key per frame (0, 1, ..)
    std::vector<std::array<float, 4>> values;  // quat and vec4: four floats; scalar: values[i][0]
  };
  std::array<Channel, 6> channels;
};

bool ParseCanm(const std::vector<uint8_t>& bytes, Canm* out, std::string* err);

// Float constants the pose maths reads from the executable (docs/re/pes-re-camera-pose.md): each is read
// through the instruction that loads it, with its opcode and RIP-relative operand checked.
struct PoseConstants {
  float defaultFps = 0;       // 0x142139E30: the fps when the stream has none
  float pi = 0;               // 0x142139B67
  float oneEighty = 0;        // 0x142139B70
  float sixty = 0;            // 0x141F7B51A: ticks per second
  float one = 0;              // 0x140A93299
  float two = 0;              // 0x140A91D24
  float half = 0;             // 0x141F7B2A6
  float minusOne = 0;         // 0x141F7B87D
  float degToRad = 0;         // 0x141F7B8FC
  float snapLow = 0;          // 0x14213A184: the blend weight below which a rotation is the first key
  float snapHigh = 0;         // 0x14213A1AC: ... above which it is the second key
  float slerpLinearEps = 0;   // 0x140A932AA: 1 - dot at or below which the weights stay linear
  float heightRef = 0;        // 0x141F39F10: target height (cm) with no camera correction
  float heightScale = 0;      // 0x141F39F18
  float renderFarFloor = 0;   // 0x141EAF914: the render far plane is at least this
  float fovMaxRad = 0;        // 0x141EAF60C: the render fov clamp
  float wideAspect = 0;       // 0x141F7B294: the aspect the widescreen fix keeps the fov for
  float radToDeg = 0;         // 0x140A91E7A: the axis-angle builder turns its radians into degrees
  float quarterTurnDeg = 0;   // 0x140A91E99: ... and takes the cosine as the sine 90 degrees on
  std::array<float, 513> sinTable{};  // 0x142655950 (lea 0x140A92D04): sin(i * 90 / 512 degrees), the table of 0x140A92C80
};
std::optional<PoseConstants> LoadPoseConstants(const PeImage& exe, std::string* err);

// The rotation, position and fov at a (possibly fractional) frame, as ktCameraAnimation::Evaluate
// (0x142139AB0) leaves them. The frame is clamped to [0, length]. The CANM's near/far/aspect channels
// are never used (0x141F7B586 overwrites them with the record's).
struct CameraSample {
  std::array<float, 3> position{};       // Fox space: Y up, metres, pitch-centre origin
  std::array<float, 4> rotation{};       // quaternion x, y, z, w, not normalised (the matrix does that)
  float fovRad = 0;                      // vertical, radians
};
CameraSample SampleCanm(const PoseConstants& k, const Canm& c, float frame);

using Mat3 = std::array<std::array<float, 3>, 3>;  // m[row][col], column-vector convention

// FixDemoPlayer's cut offset (player+0xA08..+0xA18; FixDemoCamera slot +0x60, 0x141F7B700, which wraps rotY
// into [0, 360)): applied when the record asks for it (record +0xA6). A rendezvous capture of 0x141F39CC0
// read it at every record activation of the entrance (six calls, pid 1694737) and it was zero each time
// (docs/ports/pes-port-FdcCamera5.md); the writers (0x141F2E11C: the caller's position, player+0xACC and
// +0xAD0, or a map node's +0x4C..+0x5C) need a FixDemo player and are not ported.
struct CutOffset {
  std::array<float, 3> pos{};
  float rotYDeg = 0;
  bool flip = false;                     // mirror X
};

// The player's cut offset after cut start (0x141F2DD80 at 0x141F2E115..0x141F2E1B0): the stage default
// (+0xAC0..+0xAD0), replaced whole by the first cut's own offset (the smallest id >= 0 of the file's cuts:
// the player map's lower_bound on key 0) when that cut's offset flag is set. The rotY wrap into [0, 360) the
// writer applies is not repeated here: RotYMatrix (0x140A92C80) wraps its input the same way.
CutOffset CutStartOffset(const File& file, const CutOffset& stageDefault);

// FUN_140A91E60 about the axis (0, 1, 0) for rotYDeg degrees, as FixDemoCamera::Update builds it
// (0x141F7B8FC): the angle goes degrees -> radians (0x1425A0C48) -> degrees (0x14259BE40), and the sine
// and cosine come from the table of 0x140A92C80 (translated), not from libm.
Mat3 RotYMatrix(const PoseConstants& k, float rotYDeg);

// FixDemoCamera::Update's CANM pose (0x141F7B770). Columns of m: 0 = -right, 1 = up, 2 = forward
// (the camera looks down the quaternion's -Z; 0x142139CAB negates columns 0 and 2).
struct CameraPose {
  Mat3 m{};
  std::array<float, 3> eye{};
  float fovRad = 0;
  float frame = 0;                       // the clip frame the clock gave
  std::array<float, 3> forward() const { return {m[0][2], m[1][2], m[2][2]}; }
  std::array<float, 3> up() const { return {m[0][1], m[1][1], m[2][1]}; }
};

// The clip frame at a player clock in 1/60 s ticks (0x141F42191, SetTime 0x141F7B4A0).
float ClipFrame(const PoseConstants& k, const Canm& c, const CameraRecord& rec, float clockTicks);

// 0x141F39F10: (targetHeight - 180) * 0.01 * rate; the camera's height offset (cam+0x20C).
float HeightOffsetM(const PoseConstants& k, float targetHeightCm, float correctRateForHeight);

CameraPose EvaluateCameraPose(const PoseConstants& k, const Canm& c, const CameraRecord& rec, float clockTicks,
                              const CutOffset& playerOffset, float heightOffsetM);

// What the controller hands the render camera (0x141EAF8E7..0x141EAF928).
struct RenderCamera {
  float fovRad = 0, nearM = 0, farM = 0;
};
// widescreenFix: the option byte G(0x141F500D0)+0x522E (0x141F7B250): with it set and a display aspect
// below 16:9 the fov is widened to keep the 16:9 horizontal fov.
RenderCamera RenderCameraParams(const PoseConstants& k, const CameraPose& pose, const CameraRecord& rec,
                                float displayAspect, bool widescreenFix);

// The clip's frame rate: the stream's, or the default.
float ClipFps(const PoseConstants& k, const Canm& c);

}  // namespace pes::fdc
