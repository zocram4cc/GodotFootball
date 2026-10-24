#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "pes/turffin.hpp"
#include "pes/vfs.hpp"

namespace {

// The one installed PES VFS, mounted once for the whole test binary.
const pes::PesVfs* Vfs() {
  static pes::PesVfs vfs;
  static bool ready = [] {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return false;
    std::string err;
    return vfs.Mount(root, &err);
  }();
  return ready ? &vfs : nullptr;
}

const pes::PeImage* Exe() {
  static std::optional<pes::PeImage> exe;
  static bool ready = [] {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return false;
    std::string path = std::string(root) + "/PES2021.exe";
    std::string err;
    exe = pes::PeImage::Open(path, &err);
    return exe.has_value();
  }();
  return ready ? &*exe : nullptr;
}

}  // namespace

// The turf placer's grid (TurfPlacerRE 3.1): 281 x 159 slots at 0.5 m over the
// 139.82 x 78.91 ground rect. The derivation is FUN_141a34860's own: u =
// trunc((p - half) * 0.5 / half + 1.0), count = u * 2 + 1.
TEST(TurfFin, GridCountsMatchThePlacerArithmetic) {
  EXPECT_EQ(pes::TurfGridCount(139.82f, 0.5f), 281);
  EXPECT_EQ(pes::TurfGridCount(78.91f, 0.5f), 159);
  EXPECT_EQ(pes::TurfGridCount(139.82f, 0.5f) * pes::TurfGridCount(78.91f, 0.5f), 44679);
}

// The placer reads its rect and step from the turf init path
// (0x141a34d78/85/8f); the grid it builds from them is 281 x 159.
TEST(TurfFinInstall, GridGeometryComesFromTheInstall) {
  const pes::PesVfs* vfs = Vfs();
  const pes::PeImage* exe = Exe();
  if (!vfs || !exe) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  auto geo = pes::TurfGridGeometry(*exe, &err);
  ASSERT_TRUE(geo.has_value()) << err;
  EXPECT_EQ(geo->countX, 281);
  EXPECT_EQ(geo->countZ, 159);
  EXPECT_EQ(geo->kept.size(), 281u * 159u);
  // The density gate keeps a nontrivial fraction: neither everything nor nothing.
  const size_t kept =
      std::count(geo->kept.begin(), geo->kept.end(), true);
  EXPECT_GT(kept, geo->kept.size() / 4);
  EXPECT_LT(kept, geo->kept.size());
  // Deterministic: the same seed replays the same gate.
  auto geo2 = pes::TurfGridGeometry(*exe, &err);
  ASSERT_TRUE(geo2.has_value()) << err;
  EXPECT_EQ(geo->kept, geo2->kept);
}

// The fin tile is the install's turf3dtexlodu4.fmdl mesh0: 9248 verts / 27744
// indices over a 0.64 x 0.58 m tile (TurfFins 0).
TEST(TurfFinInstall, FinTileComesFromTheInstall) {
  const pes::PesVfs* vfs = Vfs();
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  auto tile = pes::TurfFinTile(*vfs, &err);
  ASSERT_TRUE(tile.has_value()) << err;
  EXPECT_EQ(tile->positions.size(), 9248u);
  EXPECT_EQ(tile->indices.size(), 27744u);
  EXPECT_EQ(tile->uvs.size(), 9248u);
  // Blade normals: mostly horizontal (TurfFins 1.2).
  double horiz = 0.0;
  for (const auto& n : tile->normals)
    horiz += std::sqrt(n.x * n.x + n.z * n.z);
  horiz /= tile->normals.size();
  EXPECT_GT(horiz, 0.7);
}

// The alpha-test survival at broadcast mip0 is 0.303 (TurfFins 0): the texture
// the fin PS samples keeps ~30% of fragments.
TEST(TurfFinInstall, GrassfinSurvivalMatchesTheDecode) {
  const pes::PesVfs* vfs = Vfs();
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  const double survival = pes::GrassfinAlphaSurvival(*vfs, 0.3f, &err);
  ASSERT_GE(survival, 0.0) << err;
  EXPECT_NEAR(survival, 0.303, 0.01);
}

// cb4[1] is the PitchOffset affine (1/W, 1/H, 0.5, 0.5) with W/H from the same
// init path (TurfMask 2).
TEST(TurfFinInstall, MaskAffineComesFromTheInstall) {
  const pes::PeImage* exe = Exe();
  if (!exe) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  auto affine = pes::TurfMaskAffine(*exe, &err);
  ASSERT_TRUE(affine.has_value()) << err;
  EXPECT_NEAR(affine->x, 1.0 / 139.82, 1e-6);
  EXPECT_NEAR(affine->y, 1.0 / 78.91, 1e-6);
  EXPECT_FLOAT_EQ(affine->z, 0.5f);
  EXPECT_FLOAT_EQ(affine->w, 0.5f);
}
