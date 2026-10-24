#pragma once

// PES's turf fin placer (docs/render/pes-render-TurfPlacer.md 3): the 281 x 159
// slot grid at 0.5 m over the ground rect, the per-slot xorshift density gate
// and the per-patch variant pick. Geometry (rect W/H, step) is read from the
// turf init path at run time (0x141a34d78/85/8f immediates, opcode-checked);
// the fin tile itself is the install's turf3dtexlodu4.fmdl mesh0.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/types.hpp"
#include "pes/vfs.hpp"

namespace pes {

// The fin mesh path (TurfFins 0): the only turf3dtex file in the VFS.
inline constexpr const char* kTurfFinMeshPath =
    "Asset/model/bg/common/turf3d/#Win/turf3dtexlodu4.fmdl";
// The fin alpha the match PS tests (t7.w < 0.3, turf3d.ps.asm:26-29).
inline constexpr const char* kGrassfinTexturePath =
    "Asset/model/bg/common/turf3d/sourceimages/#windx11/grassfin_bsm_alp.ftex";

// FUN_141a34860's own derivation (TurfPlacerRE 3.1): u =
// trunc((p - half) * 0.5 / half + 1.0), count = u * 2 + 1.
int TurfGridCount(float extent, float halfStep);

// The placer grid over the ground rect: countX x countZ slots at `step` m,
// kept[i] after the density gate (row-major, x fastest).
struct TurfGrid {
  int countX = 0, countZ = 0;
  float stepM = 0.5f, widthM = 0.0f, heightM = 0.0f;
  std::vector<bool> kept;
  // Per-patch variant pick (TurfPlacerRE 3.4): patch index -> variant ordinal.
  std::vector<int> variant;
};

// The grid geometry from the install: rect W/H and step from the turf init
// immediates, counts from TurfGridCount, gate + variants replayed with PES's
// xorshift32 (the 0x141a3e330/0x141a3c300 idiom).
std::optional<TurfGrid> TurfGridGeometry(const PeImage& exe, std::string* error);

// cb4[1], the PitchOffset affine (TurfMask 2): (1/W, 1/H, 0.5, 0.5) with W/H
// from the same init path.
std::optional<Vec4> TurfMaskAffine(const PeImage& exe, std::string* error);

// The install's fin tile: mesh0 positions/normals/UVs/indices of
// turf3dtexlodu4.fmdl (TurfFins 0).
struct TurfFinTileData {
  std::vector<Vec3> positions, normals;
  std::vector<Vec2> uvs;
  std::vector<uint32_t> indices;
};
std::optional<TurfFinTileData> TurfFinTile(const PesVfs& vfs, std::string* error);

// Fraction of grassfin mip0 texels with alpha >= `threshold` (the broadcast
// survival is 0.303 at 0.3, TurfFins 0). Negative on error.
double GrassfinAlphaSurvival(const PesVfs& vfs, float threshold, std::string* error);

}  // namespace pes
