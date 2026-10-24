#include "pes/turffin.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "pes/fmdl.hpp"
#include "pes/ftex.hpp"

namespace pes {
namespace {

// {VA, expected opcode bytes, immediate offset}: the opcode is checked before
// the immediate is trusted, so another build fails loudly (pesrandom.cpp's
// pattern). VAs from TurfPlacerRE 3.1 / TurfMaskRE 0 (M3-M5), script-read.
struct Imm {
  uint64_t va;
  std::initializer_list<uint8_t> opcode;
  int offset;
};
constexpr Imm GRID_W{0x141a34d78, {0xc7, 0x83, 0x40, 0x01, 0x00, 0x00}, 6};  // mov [rbx+0x140], 139.82
constexpr Imm GRID_H{0x141a34d85, {0xc7, 0x83, 0x44, 0x01, 0x00, 0x00}, 6};  // mov [rbx+0x144], 78.91
constexpr Imm GRID_S{0x141a34d8f, {0xc7, 0x83, 0x48, 0x01, 0x00, 0x00}, 6};  // mov [rbx+0x148], 0.5

template <typename T>
bool ReadImm(const PeImage& exe, const Imm& imm, T* out, std::string* error) {
  uint8_t code[8] = {};
  if (!exe.ReadVa(imm.va, imm.opcode.size(), code) ||
      !std::equal(imm.opcode.begin(), imm.opcode.end(), code) ||
      !exe.ReadVa(imm.va + imm.offset, sizeof *out, reinterpret_cast<uint8_t*>(out))) {
    char va[32];
    std::snprintf(va, sizeof va, "%#llx", static_cast<unsigned long long>(imm.va));
    *error = std::string("turf grid: unexpected instruction at ") + va;
    return false;
  }
  return true;
}

// PES's xorshift32 step (the 0x141a3e330 / 0x141a3c300 idiom: shl 13, shr 17,
// shl 5, xored back into the state word).
uint32_t XorShift32(uint32_t* state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

}  // namespace

int TurfGridCount(float extent, float halfStep) {
  const float u = (extent - halfStep) * 0.5f / halfStep + 1.0f;
  return static_cast<int>(u) * 2 + 1;
}

std::optional<TurfGrid> TurfGridGeometry(const PeImage& exe, std::string* error) {
  float w = 0, h = 0, s = 0;
  if (!ReadImm(exe, GRID_W, &w, error) || !ReadImm(exe, GRID_H, &h, error) ||
      !ReadImm(exe, GRID_S, &s, error))
    return std::nullopt;
  if (!(w > 0) || !(h > 0) || !(s > 0)) {
    *error = "turf grid: non-positive geometry from the install";
    return std::nullopt;
  }
  TurfGrid g;
  g.countX = TurfGridCount(w, s);
  g.countZ = TurfGridCount(h, s);
  g.stepM = s;
  g.widthM = w;
  g.heightM = h;
  const size_t n = size_t(g.countX) * size_t(g.countZ);
  g.kept.assign(n, false);
  // The density gate (TurfPlacerRE 3.3): per slot one xorshift32 draw, kept
  // when draw % 100 < 0x32 (the gate's compare immediate). The reciprocal
  // divide (0x51eb851f, shr 5) is / 100; the per-slot byte table and flag
  // bytes need live state and stay out (coverage is bracketed, not pinned).
  // ponytail: slot seed/state words are live RNG state (TurfPlacerRE 3.3
  // OPEN); the replay below seeds from the slot ordinal so the gate is
  // deterministic in GF.
  for (size_t i = 0; i < n; ++i) {
    uint32_t state = uint32_t(i * 2 + 1);  // xorshift needs a nonzero seed
    g.kept[i] = XorShift32(&state) % 100 < 0x32;
  }
  // The variant pick (TurfPlacerRE 3.4): up to 10 xorshift draws of variant
  // entries per patch, kept when the entry byte matches. Entry bytes are live
  // state; GF replays the draw count cadence with the slot-ordinal seed.
  g.variant.assign(n, 0);
  for (size_t i = 0; i < n; ++i) {
    uint32_t state = uint32_t(i * 2 + 1);
    int picks = 0;
    for (int k = 0; k < 10; ++k) {
      XorShift32(&state);
      ++picks;
    }
    g.variant[i] = picks;
  }
  return g;
}

std::optional<Vec4> TurfMaskAffine(const PeImage& exe, std::string* error) {
  float w = 0, h = 0, s = 0;
  if (!ReadImm(exe, GRID_W, &w, error) || !ReadImm(exe, GRID_H, &h, error) ||
      !ReadImm(exe, GRID_S, &s, error))
    return std::nullopt;
  if (!(w > 0) || !(h > 0)) {
    *error = "turf mask affine: non-positive rect from the install";
    return std::nullopt;
  }
  return Vec4{1.0f / w, 1.0f / h, s, s};
}

std::optional<TurfFinTileData> TurfFinTile(const PesVfs& vfs, std::string* error) {
  auto blob = vfs.Read(kTurfFinMeshPath, error);
  if (!blob) return std::nullopt;
  auto fmdl = ParseFmdl(*blob, error);
  if (!fmdl || fmdl->meshes.empty()) {
    if (error->empty()) *error = "turf fin tile: no meshes in " + std::string(kTurfFinMeshPath);
    return std::nullopt;
  }
  const FmdlMesh& m = fmdl->meshes[0];
  TurfFinTileData t;
  t.positions = m.pos;
  t.normals = m.nrm;
  t.uvs = m.uv0;
  t.indices = m.indices;
  return t;
}

namespace {

// DXT5 alpha block: two base alphas + 48 bits of 3-bit indices over the
// 4x4 block, row-major.
void Dxt5Alphas(const uint8_t* block, uint8_t out[16]) {
  const uint8_t a0 = block[0], a1 = block[1];
  uint64_t bits = 0;
  for (int i = 0; i < 6; ++i) bits |= uint64_t(block[2 + i]) << (8 * i);
  for (int i = 0; i < 16; ++i) {
    const int idx = int((bits >> (3 * i)) & 7);
    uint8_t a;
    if (a0 > a1) {
      // 8 interpolated alphas: idx 0/1 are the bases, else (n*a0 + m*a1)/7.
      static constexpr int kNum[8] = {0, 0, 6, 5, 4, 3, 2, 1};
      if (idx == 0)
        a = a0;
      else if (idx == 1)
        a = a1;
      else
        a = uint8_t((kNum[idx] * a0 + (7 - kNum[idx]) * a1) / 7);
    } else {
      if (idx == 0)
        a = a0;
      else if (idx == 1)
        a = a1;
      else if (idx == 6)
        a = 0;
      else if (idx == 7)
        a = 255;
      else
        a = uint8_t(((6 - idx) * a0 + (idx - 1) * a1) / 5);
    }
    out[i] = a;
  }
}

}  // namespace

double GrassfinAlphaSurvival(const PesVfs& vfs, float threshold, std::string* error) {
  auto blob = vfs.Read(kGrassfinTexturePath, error);
  if (!blob) return -1.0;
  auto ftex = ParseFtex(*blob, error);
  if (!ftex) return -1.0;
  if (ftex->pixelFormat != 4 /* DXT5 */ || ftex->mips.empty()) {
    *error = "grassfin: expected a DXT5 ftex with mips";
    return -1.0;
  }
  const std::vector<uint8_t>& mip0 = ftex->mips[0];
  const int blocksX = (ftex->width + 3) / 4, blocksY = (ftex->height + 3) / 4;
  if (mip0.size() < size_t(blocksX) * size_t(blocksY) * 16) {
    *error = "grassfin: truncated mip0";
    return -1.0;
  }
  const uint8_t cut = uint8_t(threshold * 255.0f);
  size_t at = 0, kept = 0;
  uint8_t alphas[16];
  for (int by = 0; by < blocksY; ++by)
    for (int bx = 0; bx < blocksX; ++bx) {
      const size_t off = (size_t(by) * blocksX + bx) * 16;
      Dxt5Alphas(mip0.data() + off, alphas);
      for (int py = 0; py < 4 && by * 4 + py < ftex->height; ++py)
        for (int px = 0; px < 4 && bx * 4 + px < ftex->width; ++px) {
          ++at;
          if (alphas[py * 4 + px] >= cut) ++kept;
        }
    }
  if (at == 0) {
    *error = "grassfin: empty mip0";
    return -1.0;
  }
  return double(kept) / double(at);
}

}  // namespace pes
