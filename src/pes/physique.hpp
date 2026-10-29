#pragma once

// PES 2021's body-scale G: the factor FUN_141EB5810 stores at P+0xA00 and in the physique chain
// (*(*(P+0xA08)+0xF0)+0xC0) that FUN_141AF3180 reads back, from the player's physique block
// (PM+0x784, 0x44 bytes: the height in cm at +0x0, the leg-length slider at +0x30, the "unscaled" byte at +0x40).
// The root bone's arm multiplies the block's scale (FUN_14195CA70) by the "root_main" entry of the joint-scale
// tables (FUN_14195C710), which has none, so the factor is 1 (CONFIRMED by tests/pes_physique_test.cpp on every
// player of the full captures), then clamps to [lo, hi] (0x141EB5959 .. 0x141EB596F).
//
// Every constant comes from the user's PES2021.exe through PeImage::ReadVa; each is named by the instruction that
// loads it and that instruction's opcode bytes are checked before the value is trusted.

#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

struct PhysiqueConstants {
  float one = 0.0f;            // 0x14256AE1C
  float heightFloorCm = 0.0f;  // 0x142A83540: below it the deviation extends the lower segment linearly
  float heightCeilCm = 0.0f;   // 0x1425CA310: above it the upper segment applies
  float heightAnchorCm = 0.0f; // 0x142598580: the neutral height (also P+0x3B18's divisor)
  float lowerSpanCm = 0.0f;    // 0x142588214
  float upperSpanCm = 0.0f;    // 0x14259BE2C
  float tallGain[2] = {};      // [0] 0x142A83538 for a deviation >= 0, [1] 0x142A8353C below it
  float legGain[2] = {};       // [0] 0x142A83530 for a slider >= 0, [1] 0x142A83534 below it
  float scaleLo = 0.0f;        // 0x1425DDF78
  float scaleHi = 0.0f;        // 0x1425DDF80
};

std::optional<PhysiqueConstants> LoadPhysiqueConstants(const PeImage& exe, std::string* error);

// The physique block's three fields the scale reads.
struct PhysiqueBlock {
  float heightCm = 180.0f;  // block[0x0]
  float legSlider = 0.0f;   // block[0x30], -1..1
  bool unscaled = false;    // block[0x40] != 0
};

// FUN_14195C4B0: the height's signed deviation from the anchor, in PES's piecewise order.
float HeightDeviation(const PhysiqueConstants& k, float heightCm);
// FUN_14195CA70: (1 + legGain * slider) * (1 + tallGain * deviation); 1 for an unscaled block.
float BlockScale(const PhysiqueConstants& k, const PhysiqueBlock& b);
// FUN_141EB5810's root-bone arm: BlockScale clamped to [scaleLo, scaleHi] (P+0xA00 and the chain's G).
float PhysiqueG(const PhysiqueConstants& k, const PhysiqueBlock& b);

}  // namespace pes
