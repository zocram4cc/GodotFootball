#include "onthepitch/pesattrs.hpp"

#include <cmath>
#include <cstring>
#include <optional>

#include "pes/exe.hpp"

namespace pes {
namespace attrs {
namespace {

constexpr uint64_t kVaRatingTable = 0x142645a40;  // row 0's flag; row k is at +32k
constexpr uint64_t kVaRatingScale = 0x14253ee30;  // DAT_14253ee30 = 100.0f
constexpr size_t kRowBytes = 32;
constexpr size_t kLastRatingRow = 0x21;           // FUN_140A603C0 0x140a603c8: row > 0x21 returns 0
constexpr int kClampQ = 6;                        // the largest band (q is clamped to 0..6)
constexpr int kStarters = 11;                     // FUN_140A62610's loop bound (team+0x4C00[0..10])
constexpr int kSlotsPerSide = 11;                 // FUN_140A62690's side multiplier
constexpr int kSlotCount = 22;                    // on-pitch flags 0..21 (MI+0x1748 + slot)
// The guarded word's keystream and mixing constants (FUN_140A58450, 0x140a58450 decompile).
constexpr uint32_t kKeystreamStep = 4u;           // K_i = byte i of 3*f + 4*i
constexpr uint32_t kKeystreamScale = 3u;          // 3 * f
constexpr uint32_t kMixMultiplier = 63u;          // '?' in the decompile: K_i * 63
constexpr uint64_t kFnvBasis = 0xcbf29ce484222325ull;
constexpr uint64_t kFnvPrime = 0x100000001b3ull;

bool ReadFloat(const PeImage& exe, uint64_t va, float* out) {
  uint8_t bytes[sizeof(float)];
  if (!exe.ReadVa(va, sizeof bytes, bytes)) return false;
  std::memcpy(out, bytes, sizeof *out);
  return std::isfinite(*out);
}

}  // namespace

std::optional<RatingTable> LoadRatingTable(const PeImage& exe, std::string* error) {
  float scale = 0.0f;
  if (!ReadFloat(exe, kVaRatingScale, &scale) || scale != static_cast<float>(kRatingScale)) {
    if (error != nullptr) *error = "rating scale 0x14253ee30 is not 100";
    return std::nullopt;
  }
  uint8_t bytes[kRatingRows * kRowBytes];
  if (!exe.ReadVa(kVaRatingTable, sizeof bytes, bytes)) {
    if (error != nullptr) *error = "rating table 0x142645a40 unreadable";
    return std::nullopt;
  }
  RatingTable table;
  for (int row = 0; row < kRatingRows; ++row) {
    const uint8_t* base = bytes + static_cast<size_t>(row) * kRowBytes;
    RatingRow& out = table.rows[static_cast<size_t>(row)];
    out.flag = base[0];  // FUN_140A603C0 tests the low byte of the row's flag dword
    for (size_t q = 0; q < out.t.size(); ++q) {
      std::memcpy(&out.t[q], base + 4 + 4 * q, sizeof(float));
      if (!std::isfinite(out.t[q])) {
        if (error != nullptr) *error = "rating table holds a non-finite value";
        return std::nullopt;
      }
    }
  }
  return table;
}

float RatingCurve(const RatingTable& table, int value, unsigned row) {
  if (row > kLastRatingRow) return 0.0f;
  int q = value / kRatingScale;  // C division: the asm's IDIV by 100
  const int r = value % kRatingScale;
  if (q < 7) {
    if (q < 0) q = 0;
  } else {
    q = kClampQ;
  }
  const RatingRow& band = table.rows[row];
  if (q < 1) return band.t[0];
  // q <= 6 after the clamp, so PES's `q > 6` alias (DAT_142645a5c) is unreachable here.
  const float scale = static_cast<float>(kRatingScale);
  if (band.flag != 0 && r != 0 && q != kClampQ) {
    const float rf = static_cast<float>(r);
    float f = 0.0f;
    if (0.0f < rf) f = (rf < scale) ? rf / scale : 1.0f;
    const float lo = band.t[static_cast<size_t>(q)];
    const float hi = band.t[static_cast<size_t>(q) + 1];
    return ((hi - lo) * f * scale) / scale + lo;
  }
  return band.t[static_cast<size_t>(q)];
}

int KeeperStarterIndex(int side, const std::array<bool, 22>& onPitch,
                       const std::array<int, 11>& roleClass) {
  for (int k = 0; k < kStarters; ++k) {
    const int slot = side * kSlotsPerSide + k;
    if (slot < kSlotCount && onPitch[static_cast<size_t>(slot)] &&
        roleClass[static_cast<size_t>(k)] == 0) {
      return k;
    }
  }
  return kNoSlot;
}

int KeeperGlobalSlot(int side, const std::array<bool, 22>& onPitch,
                     const std::array<int, 11>& roleClass) {
  const int k = KeeperStarterIndex(side, onPitch, roleClass);
  if (k == kNoSlot) return kNoSlot;
  return side * kSlotsPerSide + k;
}

GuardedWordResult ReadGuardedWord(const GuardedWordSet& set) {
  const uint32_t word = set.ring[set.index & 3u];
  const uint32_t base = static_cast<uint32_t>(set.f) * kKeystreamScale;
  uint8_t e[4];
  for (uint32_t i = 0; i < 4; ++i) {
    const uint32_t w = (word >> (8 * i)) & 0xffu;
    const uint32_t key = ((base + kKeystreamStep * i) >> (8 * i)) & 0xffu;
    e[i] = static_cast<uint8_t>(((w + key) & 0xffu) ^ ((key * kMixMultiplier) & 0xffu));
  }
  uint64_t h = kFnvBasis;
  for (uint32_t i = 0; i < 4; ++i) {
    h = (h ^ e[i]) * kFnvPrime;
  }
  GuardedWordResult result;
  if (h != set.hash) {
    result.status = GuardedWordStatus::kTampered;
    return result;
  }
  const uint32_t encoded = static_cast<uint32_t>(e[0]) | (static_cast<uint32_t>(e[1]) << 8) |
                           (static_cast<uint32_t>(e[2]) << 16) |
                           (static_cast<uint32_t>(e[3]) << 24);
  result.word = static_cast<int32_t>(encoded);
  return result;
}

}  // namespace attrs
}  // namespace pes
