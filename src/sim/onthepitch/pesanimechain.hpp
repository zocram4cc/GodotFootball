// The anime player's layout the engine-backed trap path reads (pesfoxworld.cpp, pesworldrecord.cpp): the joint pose block (FUN_141EB49B0's
// inputs), the placement object, the root-motion reader's call frame and the clip database global. The chain itself is the engine player's
// own BodyAnimControl (PesWorld::BindAnimeChain): the engine's body loader builds its layer, key table and clips, so no chain is built here.
// Notation: A = *(P+0xA08) (the anime player), B = *(A+0xF0) (its child).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "onthepitch/pestrapworld.hpp"
namespace pes {

// The clip database singleton FUN_141B034B0 returns (an image global).
inline constexpr uint64_t kClipDatabaseGlobalVa = 0x1437CEAA0ULL;
// A: the layer controller pointer (A+0x88 names L1).
inline constexpr std::size_t kAnimeLayerOffset = 0x88;
// The anime player's joint pose block (FUN_141EB49B0's inputs): positions and rotations are model space (the player's frame, y up, before
// the heading), one 16-byte entry per joint in the anime skeleton's joint order (GF's rig joint-ID order). The placement object (A+0x188, the
// player's O2) holds the world matrix: rows at +0xA0 .. +0xC0 the heading's rotation about y, +0xD0 the position with w = 1, and the joint
// count at +0xF8.
inline constexpr uint32_t kAnimeJointCount = 20;
inline constexpr std::size_t kL1PoseOffset = 0x50;   // L1: the pointer to the joint positions (FUN_141AFC6A0)
inline constexpr std::size_t kAQuatOffset = 0x20;    // A: the pointer to the joint rotations (FUN_141AFC700)
inline constexpr std::size_t kPlacementJointCountOffset = 0xF8, kPlacementRowOffset = 0xA0, kPlacementRowStride = 0x10;
inline constexpr std::size_t kJointEntryBytes = 16;
inline constexpr std::size_t kJointFrameBytes = 0x100;  // the joint writers' call frame: the return address slot and the spill space above it
inline constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;  // the placement matrix's heading (P+0x20 is degrees)

// One player's joint pose in the anime player's layout (the readers of FUN_141EB49B0): model space, PES axes (x right, y up, z forward at
// heading 0), before the heading. position is (x, y, z, 0); rotation (x, y, z, w) is the joint's world orientation with the heading
// taken out (world = RotY(heading) * rotation).
struct PesJointPose {
  std::array<std::array<float, 4>, kAnimeJointCount> position{};
  std::array<std::array<float, 4>, kAnimeJointCount> rotation{};
};

// The reader's call frame (PesWorld::RunRootMotion): rsp at the entry is the return address slot, the 5th and 6th arguments sit at
// +0x28 / +0x30 (FUN_140829E80 stores them at 0x14082B021 / 0x14082B02B before the call), the output vector in the frame's body.
inline constexpr std::size_t kRootMotionFrameBytes = 0x100, kRootMotionOutOffset = 0x80;
inline constexpr std::size_t kRootMotionArg5Offset = 0x28, kRootMotionArg6Offset = 0x30;

}  // namespace pes
