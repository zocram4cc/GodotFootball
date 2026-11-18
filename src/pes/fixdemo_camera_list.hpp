#pragma once

// PES's per-layer FixDemo camera record list (docs/ports/pes-port-CameraSelect.md): the writer of the layer player's set word
// (0x141F5D2E0), the scene's random generators (scene constructor 0x141F55110) and the list builder inside the cut start
// 0x141F2DD80 (the draw 0x141F7A100 per tens group).

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/fdc.hpp"
#include "sim/onthepitch/pesrandom.hpp"

namespace pes {

// The immediates of the FixDemo draws, read from the install with the opcode checked (a mismatch fails loudly).
struct FixDemoRandomConstants {
  RngConstants rng;
  int32_t pickScale;   // 0x2AC: the RangeRaw modulus of the pick 0x141F7A100 (mov edx, imm32 at 0x141F7A112)
  int32_t pickAdd;     // 0xAA (add r8d, imm32 at 0x141F7A123)
  uint32_t pickMask;   // 0x3FF (and eax, imm32 at 0x141F7A12D)
  int32_t pickShift;   // 10 (shr eax, imm8 at 0x141F7A13D)
  int32_t wordInit;    // 0x400: every FixDemo generator's +0x20 word starts as RangeRaw(slot 2, this) (0x141F5521E)
  int32_t seedRange;   // 20000: the seed draw of a child generator (0x141F552B9)
  int32_t setRange;    // 100: the draw behind a scene's and a layer's set word (0x141F55413, 0x141F5D308)
  int32_t warmupDraws; // 11: the draws the global generator's seeder 0x141F51FD0 makes after its word (mov edi, imm32 at 0x141F52025)
  int32_t warmupRange; // 0x10: their RangeRaw modulus (mov edx, imm32 at 0x141F52030)
};
std::optional<FixDemoRandomConstants> LoadFixDemoRandomConstants(const PeImage& exe, std::string* error);

// math::Random plus the +0x20 word of a FixDemo generator; only slot 2 is ever drawn.
class FixDemoGenerator {
 public:
  FixDemoGenerator(const FixDemoRandomConstants& k, uint32_t slot2State);
  // A generator as a memory snapshot holds it: slot 2's state and the +0x20 word (no draw).
  static FixDemoGenerator Restore(const FixDemoRandomConstants& k, uint32_t slot2State, uint32_t word);
  uint32_t state() const { return rng_.State(kSlot); }
  uint32_t word() const { return word_; }
  // 0x141F7A150: RangeRaw(slot 2, m).
  int Raw(int m) { return rng_.RangeRaw(kSlot, m); }
  // 0x141F7A100: ((RangeRaw(slot 2, 0x2AC) + word + 0xAA) & 0x3FF) updates the word, the pick is word * n >> 10.
  uint32_t Pick(uint32_t n);

 private:
  FixDemoGenerator(const FixDemoRandomConstants& k, uint32_t slot2State, uint32_t word);
  static constexpr int kSlot = 2;
  const FixDemoRandomConstants& k_;
  Random rng_;
  uint32_t word_;
};

// The scene's generators (0x141F55110): A (+0x128) seeded by the global FixDemo generator's draw, R1 (+0x150) and R2 (+0x178)
// each seeded by A's next draw; the scene's set word (+0x1A0) and the previous layer's pick (+0x1A4) are R1's first two picks.
// ponytail: PES seeds its global generator from the clock (0x141F4C950: 0x141B34AD0 % 10000 into 0x141F51FD0); the caller names the
// seed here (GF's match seed), so a replay of a match plays the same scenes.
class FixDemoScene {
 public:
  FixDemoScene(const FixDemoRandomConstants& k, uint32_t globalSeed);
  // 0x141F5D2E0: the layer player's +0x78. `mode` is the byte of the layer's own record map (player+0xAE8 key 0, +0x48):
  // 0 a fresh pick of R1, 1 the scene's word, 2 a pick that avoids the previous one. `setCount` is the player's +0x7C.
  uint32_t LayerSetWord(uint32_t mode, uint32_t setCount);
  FixDemoGenerator& listRandom() { return r2_; }  // R2: the generator of the list builder 0x141F2DD80

 private:
  FixDemoGenerator a_, r1_, r2_;
  uint32_t sceneWord_ = 0, previousPick_ = 0;
  int32_t setRange_ = 0;
};

// The consecutive leading hundreds groups whose tens-0 vector is not empty (0x141F3CFA0's tail: the player's +0x7C), at most 10.
uint32_t CameraSetCount(const std::vector<fdc::CameraRecord>& records);

// 0x141F2DD80's list: set = word % setCount, then one pick per non-empty tens group of that set (ids set*100 + tens*10 + k, in
// map order). A pick is kept when the list holds fewer than two entries or its start is later than the last one's. Indices into
// `records` (ids >= 1000 are not in a set). Empty when there is no set.
std::vector<size_t> BuildCameraList(const std::vector<fdc::CameraRecord>& records, uint32_t setWord, FixDemoGenerator& r2);

// 0x141F42110's index walk: from the list's first record, the next one takes over once the player clock (1/60 s ticks) reaches
// its start. Index into the list.
size_t CameraListIndex(const std::vector<fdc::CameraRecord>& records, const std::vector<size_t>& list, float clock);

}  // namespace pes
