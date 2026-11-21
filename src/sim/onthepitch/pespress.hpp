#pragma once

// PES 2021's press command side: what a PRESS man does once the press writer has marked him (ActionPress FUN_140962440,
// docs/re/pes-re-press-life.md 8): the target sample index, the speed class (FUN_140A75BE0) and the chase command speed.
// WHO presses is decided by PES's translated writer 0x140536FC0, coordinator 0x140634470 and target eval 0x140633940
// (PesWorld::StagePress / RunPressWriter, pesworldrecord.hpp), not here.
//
// Pure: arguments plus the constants. Distances metres. Constants come from the install's PES2021.exe image, read by
// LoadPressConstants at the addresses named below.

#include <cstdint>
#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

// Every float here is a .rdata constant of PES2021.exe (verified in the image).
struct PressConstants {
  float framesPerSec;      // 0x14351ae54 (54): .data initializer, read as the image holds it
  float speedCut0_kmh;     // 0x1425a8a28 (0.05): class 0/1 cut
  float speedCut1_kmh;     // 0x14259bdf4 (8): class 1/2 cut
  float speedCut2_kmh;     // 0x1425a1ffc (13.5): class 2/3 cut
  float speedCut3_kmh;     // 0x1425c48c0 (22): class 3/4 cut
  float targetTickRate;    // 0x142588200 (0.1): k = fps*rate + bias ticks
  float targetTickBias;    // 0x142565600 (0.5)
};

// The T+0xA2A0 words the press writer stores (PesWorld::PressAction): kPressState for a PRESSed man, kNothingState otherwise.
inline constexpr int kPressState = 0x34;
inline constexpr int kNothingState = 0;

std::optional<PressConstants> LoadPressConstants(const PeImage& exe, std::string* error);

// FUN_140639100's no-carrier arm: true when the ball sample at k ticks is at
// least as near the ball now as the presser's base (squared metres, ties go
// to the ball). Points are (x, z) pitch metres. No constants: the comparison
// is pure arithmetic on the caller's points.
struct PressPoint {
  float x = 0, z = 0;
};
bool PressTargetIsBall(PressPoint sample, PressPoint ball, PressPoint base);

// The sample index: int(framesPerSec * targetTickRate + targetTickBias) (54*0.1+0.5 = 5).
int PressTargetTick(const PressConstants& c);

// FUN_140a75be0's speed class 0..4 of `speedKmh` over the four cuts.
int PressSpeedClass(const PressConstants& c, float speedKmh);

// The chase command speed in m/s: the arrival slope (dist_m * slope) capped
// one band above the current class, the caps in m/s from the cuts (class 4 is
// uncapped). The +1 lead is GF's: PES's solver paces AT the class
// (FUN_1406b9270, undecoded), which would stand a standing presser still.
float PressCommandSpeed_ms(const PressConstants& c, float dist_m, float speedKmh, float slope);

}  // namespace pes
