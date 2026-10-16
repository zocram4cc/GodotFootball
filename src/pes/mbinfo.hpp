#pragma once

// PES's per-motion metadata, the Mbinfo bins (docs/re/pes-re-animselect.md 2.1-2.2,
// pes-re-anim-select.md 5.3 and 6.1), read from the user's install:
//   Animation.bin  36-byte records: end frame, blend start, root offsets
//   HitData.bin   108-byte records: the frame and place a clip meets the ball
//   CancelData.bin 12-byte records: the frame a clip may be cancelled from
// Frames are gani frames (30 Hz); positions cm, as stored.
//
// The motion table (docs/re/pes-re-gaps-anime.md gap 2 and rounds 4-5) is PES's own per-motion
// record: FUN_1406b20e0 allocates it, FUN_1406aef60 files each bin into it through its builder, and
// the readers FUN_1406af620 .. FUN_1406b1930 walk it. Each motion keeps, per kind, at most the cap
// PES stores (DAT_1425dc370, read from the exe) in file order; a record the motion lacks reads as
// PES's zero-filled fallback object.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace pes {

struct MotionTiming {
  int endFrame = -1;    // last frame (u32 a bits 13+ = end - 1)
  int blendStart = -1;  // u32 b bits 10-19
  // root_pos_blend_z/x/y, root_pos_first_x/z, cancel_speed, end_move_angle
  float rootBlendZ = 0, rootBlendX = 0, rootBlendY = 0, rootFirstX = 0, rootFirstZ = 0;
  float cancelSpeed = 0, endMoveAngle = 0;
};

struct HitRecord {
  int frame = 0;        // u32 +0 bits 13..20
  int contactPart = 0;  // bits 27..31 + 1
  float ballX = 0, ballY = 0, ballZ = 0;  // +0x2c / +0x28 / +0x24, cm
  float rootX = 0, rootZ = 0;             // +0x30 / +0x38, cm
  float rootVelX = 0, rootVelZ = 0;       // +0x3c / +0x40, cm per frame
  float yaw_deg = 0;                      // +0x44
};

struct CancelRecord {
  int frame = 0;  // u32 bits 13..22
};

// RotData, 8 bytes, kind 6 (list +0xc8, cap 3). A default record is the zero word PES's fallback
// holds, so its fields are the values the readers give for a missing record.
struct RotRecord {
  int offset = -878;  // FUN_1406b1c70: (short)((w0 >> 13 & 0x7ff) - 0x36e), mirrored by the reader
  int start = -517;   // FUN_1406b1f00: (short)((w1 >> 10 & 0x3ff) - 0x205)
  int end = 1;        // FUN_1406b1d00: (ushort)((w1 & 0x3ff) + 1)
};

// FootType, 4 bytes, kind 9 (list +0x110, cap 20). FUN_1406afe20: the foot pair, w0 >> 22 & 3.
struct FootRecord {
  int pair = 0;
};

// PivothitInfo, 16 bytes, kind 12 (list +0x208, cap 1). FUN_1406b1930 tests bit 29 of word 0.
struct PivotRecord {
  bool flag = false;
};

// SubHitData, 32 bytes, kind 2 (list +0x48, cap 6). Its fields are read by FUN_1406aff90 and
// FUN_1406b05e0 (the L4 solver), not typed yet: the words are kept as the file has them.
struct SubRecord {
  std::array<uint32_t, 8> words{};
};

// One motion's entry. Each list's size is PES's count byte for that kind.
struct MotionRecord {
  std::vector<int> blend;         // Animation, kind 0, cap 1: FUN_1406af620, u32 +4 bits 10..19
  std::vector<int> cancel;        // CancelData, kind 5, cap 5: FUN_1406af8e0, w0 bits 13..22
  std::vector<RotRecord> rot;     // kind 6
  std::vector<FootRecord> foot;   // kind 9
  std::vector<PivotRecord> pivot; // kind 12
  std::vector<SubRecord> sub;     // kind 2

  // The readers' answers. Past the end of a list they give PES's fallback value.
  int BlendStart() const { return blend.empty() ? 0 : blend[0]; }
  int CancelCount() const { return static_cast<int>(cancel.size()); }  // FUN_1406afbc0(.., 5)
  int CancelFrames(int i) const;                                        // FUN_1406af8e0(.., i)
  int FootCount() const { return static_cast<int>(foot.size()); }      // FUN_1406afbc0(.., 9)
  int FootPair(int i) const;                                           // FUN_1406afe20(.., i)
  RotRecord Rot(int i) const;                                          // FUN_1406b1c70 / b1f00 / b1d00
  bool PivotFlag(int i) const;                                         // FUN_1406b1930(.., i)
};

// PES's per-kind list caps (DAT_1425dc370, one dword per kind), read from the exe by VA.
struct MotionCaps {
  uint32_t animation = 0, subHit = 0, cancel = 0, rot = 0, foot = 0, pivot = 0;
};
std::optional<MotionCaps> LoadMotionCaps(const PeImage& exe, std::string* error);

// The bins the loader reads: the three legacy layouts, then the four lists of the motion table
// (the Animation and CancelData bins feed both).
struct MbinfoBins {
  std::vector<uint8_t> animation, hitData, cancelData;
  std::vector<uint8_t> subHitData, rotData, footType, pivothitInfo;
};

class Mbinfo {
 public:
  // Null when the motion has no record of that kind.
  const MotionTiming* Timing(int motionNo) const;
  const std::vector<HitRecord>& Hits(int motionNo) const;
  const CancelRecord* Cancel(int motionNo) const;
  size_t MotionCount() const { return hits_.size(); }
  // PES's table entry for a motion: an empty record past the table's last slot.
  const MotionRecord& Motion(int motionNo) const;

 private:
  friend std::optional<Mbinfo> ParseMbinfo(const MbinfoBins&, const MotionCaps&, std::string*);
  std::vector<std::optional<MotionTiming>> timing_;
  std::vector<std::vector<HitRecord>> hits_;
  std::vector<std::optional<CancelRecord>> cancel_;
  std::vector<MotionRecord> motions_;
};

std::optional<Mbinfo> ParseMbinfo(const MbinfoBins& bins, const MotionCaps& caps, std::string* error);
std::optional<Mbinfo> LoadMbinfo(const PesVfs& vfs, const PeImage& exe, std::string* error);

}  // namespace pes
