// The anime player's chain the kick geometry test's root-motion reader FUN_141AFDF10 walks (docs/ports/pes-port-KickRoot.md):
// the body clip database (DAT_1437CEAA0, FUN_141B034B0), the player's motion -> clip key table (A+0x180), and the layer objects
// down to the pose slot FUN_1423A3FD0 evaluates the clip into (A+0x88). Built from the install: the body archives are PES's loaded
// packs verbatim (FUN_1415BB5C0 keeps the .mtar file in memory; its sorted key array is the file's own entry table, key = index << 32 |
// StrCode32 of the clip's name), and the clip the reader evaluates is the gani's MOTION chunk inside the loaded file.
//
// Notation: A = *(P+0xA08) (the anime player; the window's physique object A), B = *(A+0xF0) (its child, +0xC8 points back to A).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "onthepitch/pestrapworld.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace pes {

// Where the chain sits in the window: one segment, above the loader data (kTrapLoaderVa).
inline constexpr uint64_t kAnimeChainVa = 0x2200000000ULL;
// The clip database singleton FUN_141B034B0 returns (an image global).
inline constexpr uint64_t kClipDatabaseGlobalVa = 0x1437CEAA0ULL;
// The objects every player's A points at, at fixed offsets from kAnimeChainVa (the actor writer needs them without the builder).
inline constexpr uint64_t kAnimeLayerVa = kAnimeChainVa;             // A+0x88 -> the layer controller (FUN_141AF2C20 walks it)
inline constexpr uint64_t kAnimeKeyTableVa = kAnimeChainVa + 0x1000;  // A+0x180 -> the motion -> clip key table (FUN_141AF32C0)
inline constexpr std::size_t kAnimeLayerOffset = 0x88;                // A: the layer controller pointer
inline constexpr std::size_t kAnimeKeyTableOffset = 0x180;            // A: the key table pointer
inline constexpr std::size_t kPhysiqueAnimeOffset = 0xC8;             // B: the pointer back to A (FUN_141AFDF10's *(B+0xC8))
// The anime player's joint pose block (FUN_141EB49B0's inputs): in the fixed head, clear of the other objects. Positions and rotations
// are model space (the player's frame, y up, before the heading), one 16-byte entry per joint in the anime skeleton's joint order
// (GF's rig joint-ID order). The placement object (A+0x188, the player's O2) holds the world matrix: rows at +0xA0 .. +0xC0 the heading's
// rotation about y, +0xD0 the position with w = 1, and the joint count at +0xF8.
inline constexpr uint32_t kAnimeJointCount = 20;
inline constexpr std::size_t kAnimePosArrayOffset = 0x600, kAnimeQuatArrayOffset = 0x780;
inline constexpr uint64_t kAnimePosArrayVa = kAnimeChainVa + kAnimePosArrayOffset;
inline constexpr uint64_t kAnimeQuatArrayVa = kAnimeChainVa + kAnimeQuatArrayOffset;
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

// Points the chain's layer controller at the joint position array (L1+0x50). `seg` is the chain's segment, at least kDynamicOffset
// (0x1100) bytes; BuildAnimeChain calls it, and a test builds a chain without the install's clips from it.
void InitAnimeJointPose(std::vector<uint8_t>& seg);

// The reader's call frame (PesWorld::RunRootMotion): rsp at the entry is the return address slot, the 5th and 6th arguments sit at
// +0x28 / +0x30 (FUN_140829E80 stores them at 0x14082B021 / 0x14082B02B before the call), the output vector in the frame's body.
inline constexpr std::size_t kRootMotionFrameBytes = 0x100, kRootMotionOutOffset = 0x80;
inline constexpr std::size_t kRootMotionArg5Offset = 0x28, kRootMotionArg6Offset = 0x30;


struct PesAnimeChain {
  PesTrapSegment segment;                              // every object, at kAnimeChainVa
  std::vector<std::pair<uint64_t, uint64_t>> globals;  // (image VA, pointer into the segment)
  uint64_t keyTableEntriesVa = 0, databaseVa = 0;
  std::size_t motionCount = 0, trackCount = 0;

  void AddTo(PesTrapExternals& ext) const {
    ext.segments.push_back(segment);
    ext.globals.insert(ext.globals.end(), globals.begin(), globals.end());
  }
};

// Reads the ten body archives and the motion name table through the VFS winner. Null with *error set when a file, a clip or a
// motion's name has no home in them.
std::optional<PesAnimeChain> BuildAnimeChain(const PeImage& exe, const PesVfs& vfs, std::string* error);

}  // namespace pes
