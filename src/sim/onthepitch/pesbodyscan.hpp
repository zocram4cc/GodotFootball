// PES's body contact scan (FUN_1406EDCE0, match::AnimePlayer's per-frame ball-touch scan; docs/ports/pes-port-TrapContact.md) on
// GF's window: the cells it reads that GF writes (the joint block), the cells it answers in, and the driver output record it fills.
//
// What the scan does for a man in the Trap action (CONFIRMED, decompile, 0x1409F1640 is its only caller):
//   - the trap tick (0x140797CD0) sets P+0x2460 bit 9 (contact pending) on the clip's contact frame;
//   - the scan, with the ball free or his, runs the action's slot 23 (0x1406EDCE0 -> [action+0xB8]); for the Trap action that is
//     FUN_140799060, the reach test: the contact point the motion's hit data predicts (the hit bone's joint position + the joint's
//     rotation applied to the hit offset) against the ball's sample 1, within the bone's reach radius + the ball radius
//     horizontally and 0.3 m vertically; a pass sets P+0x2460 bit 10 (touched);
//   - with bit 10 (or bit 7) set the scan fills the driver's output record (FUN_140717ED0, 1406E3210, 1406E47A0, 1406E2EA0).
// The joint block is P+0xA04 (valid) with the bones' world positions at P+0x538 and rotations at P+0x700, written in PES by the
// anime player's FUN_141EB49B0 from the anime layer's pose. GF's source is the skeleton it poses (the node map of the player).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "pes/exe.hpp"

namespace pes {

// The collision bone enum (pes-re-collision.md 2.1) in PES bone names (the rig's `pesBone`), index = the enum value.
inline constexpr std::size_t kScanBoneCount = 19;
inline constexpr const char* kScanBoneNames[kScanBoneCount] = {
    "sk_root_hip", "sk_thigh_l",    "sk_leg_l",      "sk_foot_l",     "sk_thigh_r",    "sk_leg_r",      "sk_foot_r",
    "sk_belly",    "sk_chest",      "sk_neck",       "sk_head",       "sk_shoulder_l", "sk_upperarm_l", "sk_forearm_l",
    "sk_hand_l",   "sk_shoulder_r", "sk_upperarm_r", "sk_forearm_r",  "sk_hand_r"};

// One bone's world frame in PES axes (x, height, z), the cells' own layout: position (x, y, z), rotation (x, y, z, w).
struct BoneFrame {
  std::array<float, 3> position{};
  std::array<float, 4> rotation{0.0f, 0.0f, 0.0f, 1.0f};
};
using BoneFrames = std::array<BoneFrame, kScanBoneCount>;

// P cells (FUN_141EB49B0 writes them, FUN_141EB51A0 / FUN_141EB51F0 / FUN_141EB54D0 read them; the proof reads them as PES's bytes).
inline constexpr std::size_t kPJointReadyOffset = 0xA04;      // u8: the joint block is valid
inline constexpr std::size_t kPJointPosOffset = 0x538;        // 12 bytes a bone: this tick's world position
inline constexpr std::size_t kPJointPrevPosOffset = 0x61C;    // 12 bytes a bone: last tick's (FUN_141EB49B0 stores +0x538 + 0xE4, 0x141EB4A..)
inline constexpr std::size_t kPJointRotOffset = 0x700;        // 16 bytes a bone: this tick's rotation
inline constexpr std::size_t kPJointPrevRotOffset = 0x830;    // 16 bytes a bone: last tick's
inline constexpr std::size_t kPFlagsOffset = 0x2460;          // u32: per-tick flags
inline constexpr uint32_t kPFlagForced = 0x80;                // bit 7
inline constexpr uint32_t kPFlagContactPending = 0x200;       // bit 9: the trap tick's contact frame
inline constexpr uint32_t kPFlagTouched = 0x400;              // bit 10: the reach test passed

// The ball record's byte G+0x40 -> +0x279C: the ball body's B+0x48, copied on every ball update (0x140871A5C; the record copy
// 0x140430C66 also moves it). A value below 0x16 names a man, and the scan's reach test (0x1406EDE54, 0x1406EDE69) and the bone-mode
// setter (0x14081DA86, 0x14081DF29) treat any other man as locked out. B+0x48's only match-path value is the constructed 0xFF.
inline constexpr std::size_t kBallOwnerOffset = 0x279C;
inline constexpr uint8_t kBallNoOwner = 0xFF;

// The driver's output record the scan fills (rdx): the touch the match applies to the ball. Offsets are the bytes the scan writes.
inline constexpr std::size_t kScanOutBytes = 0x200;
inline constexpr std::size_t kOutKindOffset = 0x58;           // u32: the request kind (4 = a ball velocity at +0x9C)
inline constexpr std::size_t kOutVelocityOffset = 0x9C;       // f32 x3
inline constexpr std::size_t kOutFlagsOffset = 0x10D;         // u8: bit 0 = a touch is requested
inline constexpr uint8_t kOutFlagTouch = 1;

struct PesBodyScanOut {
  std::array<uint8_t, kScanOutBytes> bytes{};
  bool TouchRequested() const { return (bytes[kOutFlagsOffset] & kOutFlagTouch) != 0; }
};

// FUN_141EB54D0's mask: which bones the joint block holds (the table at the `lea` of 0x141EB54DB, 19 bytes), read from the image
// behind the opcode of the instruction that names it. False with *error set when the instruction is not the expected one.
bool LoadJointMask(const PeImage& exe, std::array<bool, kScanBoneCount>* mask, std::string* error);

}  // namespace pes
