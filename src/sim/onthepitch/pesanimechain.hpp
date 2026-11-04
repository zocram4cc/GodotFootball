// The anime player's chain the kick geometry test's root-motion reader FUN_141AFDF10 walks (docs/ports/pes-port-KickRoot.md):
// the body clip database (DAT_1437CEAA0, FUN_141B034B0), the player's motion -> clip key table (A+0x180), and the layer objects
// down to the pose slot FUN_1423A3FD0 evaluates the clip into (A+0x88). Built from the install: the body archives are PES's loaded
// packs verbatim (FUN_1415BB5C0 keeps the .mtar file in memory; its sorted key array is the file's own entry table, key = index << 32 |
// StrCode32 of the clip's name), and the clip the reader evaluates is the gani's MOTION chunk inside the loaded file.
//
// Notation: A = *(P+0xA08) (the anime player; the window's physique object A), B = *(A+0xF0) (its child, +0xC8 points back to A).

#pragma once

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
