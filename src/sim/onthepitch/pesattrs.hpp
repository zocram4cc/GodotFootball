// PES 2021's helpers that FUN_1405027A0 (27A0) calls, decoded and ported exactly:
//   FUN_140A603C0  rating curve over the install's 34-row table (row 32 and 33 are the keeper rows)
//   FUN_140A62610  the keeper's starter index of one side (FUN_140A62690 adds side*11)
//   FUN_140A58450 / FUN_140A58350  a guarded word of the slot record: the normal path returns the
//                  encoded word; the tamper path (FUN_1415A1714, an indirect call through a PES
//                  pointer) is named here and not ported.
// Unwired: 27A0 is not ported (its inputs are listed in docs/ports/pes-port-TeamT2.md).

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <optional>

namespace pes {

class PeImage;

namespace attrs {

// The rating table at 0x142645a40: 34 rows of 32 bytes. Each row is a flag (its low byte
// is read by FUN_140A603C0) and seven floats T[0..6]. The scale is DAT_14253ee30 = 100.
inline constexpr int kRatingRows = 34;
inline constexpr int kRatingScale = 100;  // the scale the loader checks against 0x14253ee30
struct RatingRow {
  uint8_t flag = 0;
  std::array<float, 7> t{};
};
struct RatingTable {
  std::array<RatingRow, kRatingRows> rows{};
};

std::optional<RatingTable> LoadRatingTable(const PeImage& exe, std::string* error);

// FUN_140A603C0(value, row): q = value / 100 clamped to 0..6, r = value % 100 (C division).
// q < 1 returns T[0]; with the row's flag set, r != 0 and q != 6, the result interpolates
// T[q] and T[q+1] by r/100; otherwise T[q]. Rows above 33 return 0.
float RatingCurve(const RatingTable& table, int value, unsigned row);

// FUN_140A62610 (side, team): the first starter k in 0..10 whose slot side*11+k is on the
// pitch (MI+0x1748) and whose role class is 0 (team+0x4C00[k] == 0). Returns k, or 0xff.
inline constexpr int kNoSlot = 0xff;
int KeeperStarterIndex(int side, const std::array<bool, 22>& onPitch,
                       const std::array<int, 11>& roleClass);

// FUN_140A62690 (side): side*11 + the starter index, or 0xff.
int KeeperGlobalSlot(int side, const std::array<bool, 22>& onPitch,
                     const std::array<int, 11>& roleClass);

// A guarded word set of a slot record. FUN_140A58450 reads it at +0xe8 (hash), +0xf0 (f),
// +0xf4 (index), +0xf8 (ring); FUN_140A58350 at +0xc8, +0xd0, +0xd4, +0xd8.
struct GuardedWordSet {
  uint64_t hash = 0;
  int32_t f = 0;
  uint32_t index = 0;
  std::array<uint32_t, 4> ring{};
};

enum class GuardedWordStatus : uint8_t {
  kOk,        // the normal path: the hash matches; `word` is the encoded word
  kTampered,  // the hash does not match: FUN_1415A1714 is the PES response (not ported)
};

struct GuardedWordResult {
  GuardedWordStatus status = GuardedWordStatus::kOk;
  int32_t word = 0;  // the encoded word, valid when kOk
};

// The normal path of FUN_140A58450 / FUN_140A58350. The four bytes of the selected ring
// word (w_i) are mixed with the keystream K_i = byte i of (3*f + 4*i): e_i = (w_i + K_i) ^
// (K_i * 63), all mod 256; the word is e0 | e1<<8 | e2<<16 | e3<<24, and the hash is FNV-1a
// 64 over e0, e1, e2, e3 in that order.
GuardedWordResult ReadGuardedWord(const GuardedWordSet& set);

}  // namespace attrs
}  // namespace pes
