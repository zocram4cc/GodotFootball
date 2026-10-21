#pragma once

// PES 2021's penalty aim scatter: FUN_1406B4000 (the aim-point error) and the
// jitter it feeds, FUN_1406E3210's kick-kind-54 case.
//
// FUN_1406B4000 returns one number: how far, in metres, the aim point misses
// what the taker asked for. The caller (FUN_1408B7550, 0x1408b7752 and
// 0x1408b781b) stores it in the action's aim record; the jitter site
// (0x1406e3421) adds a random offset to the aim point in millimetres.
//
// The formula, from the decompile of FUN_1406B4000 and its disassembly (the
// kick-kind-54 branch, 0x1406b40e1-0x1406b4273):
//
//   acc  = (uint)(placeKicking * 0.6 + finishing * 0.4) & 0xff   // 0x1406b4050-8c
//   q    = ramp(acc, 40, 99)                                    // 0x1406b40bf-e1
//   base = 0.5, or 0.4 with the Penalty Specialist skill            // 0x1406b40f5, 0x1406b4116
//   if (isChip) base += chipWithControl ? 0.2 : 0.4              // 0x1406b4132-4f
//   radius = (base - q * 0.2)                                    // 0x1406b4196
//          + ramp(charge, 0.2, 1.0) * (0.8 - q * 0.3)            // 0x1406b41a3-41e1
//          + ramp(|aim[+8]|, T8 / 3, T8 * 0.9) * (0.15 - q * 0.1)  // 0x1406b41bf-4232
//          + ramp(aim[+4], T12 * 0.25, T12 * 0.9) * (0.3 - q * 0.2) // 0x1406b4223-4273
//
// The aim record holds three floats: the x along the goal line, the lateral
// (+4) and the height (+8). The two ramps read the record's own last two:
// the height (taken as a magnitude at 0x1406b41de) against the bar's scale
// T8, and the lateral (the distance from the goal's centre, so the ramp's
// lower bound is a quarter of the post's scale) against the post's scale T12.
// T8 and T12 are the tuning table's floats at +8 and +0xc
// (DAT_1436f3dc8+0x2aa8).
//
// The jitter (0x1406e3421-0x1406e3482) draws two signed integers in
// [-radius, +radius] millimetres and adds the first to the record's height
// and the second to its lateral, the lateral's sum clamped at zero.
//
// Every number is read from the user's own PES2021.exe through
// pes::PeImage::ReadVa; nothing is transcribed here (the VA of each value is
// in the port report's table).

#include <optional>
#include <string>

#include "pes/exe.hpp"
#include "pesrandom.hpp"

namespace pes {

// The jitter draws are the taker's own generator, slot 2 (FUN_1406eaa10 reads
// player+0x32d0 and hard-wires slot 2).
inline constexpr int kPenaltyRngSlot = 2;

struct PenaltyScatterConstants {
  // --- FUN_1406B4000's accuracy core (0x1406b4050-0x1406b40e1) ---
  float placeKickingWeight = 0.0f;  // 0.6  @0x14257a190
  float finishingWeight = 0.0f;     // 0.4  @0x14259b89c
  float accFloorStat = 0.0f;        // 40.0 @0x14259be2c
  float accTopStat = 0.0f;          // 99.0 @0x1425c48cc
  float accSpan = 0.0f;             // 59.0 @0x1425cb2fc

  // --- the scatter base (0x1406b40f5, 0x1406b4116, 0x1406b4132-4f) ---
  float baseScatter = 0.0f;         // 0.5  @0x142565600
  float specialistScatter = 0.0f;   // 0.4  @0x14259b89c
  float chipScatter = 0.0f;         // 0.2  @0x142598578
  float chipScatterNoControl = 0.0f;  // 0.4 @0x14259b89c

  // --- the weighted terms (0x1406b4158-0x1406b4273) ---
  float qScatterWeight = 0.0f;      // 0.2  @0x142598578
  float qPowerWeight = 0.0f;        // 0.3  @0x142590d98
  float powerLo = 0.0f;             // 0.2  @0x142598578
  float powerSpan = 0.0f;           // 0.8  @0x142572348
  float chargeGain = 0.0f;          // 0.8  @0x142572348 (the ramp's span, reused)
  float qHeightWeight = 0.0f;       // 0.1  @0x142588200
  float heightGain = 0.0f;          // 0.15 @0x1425c48a0
  float qLateralWeight = 0.0f;      // 0.2  @0x142598578
  float lateralGain = 0.0f;         // 0.3  @0x142590d98

  // --- the two aim-point ramps (0x1406b41bf-0x1406b4273) ---
  float aimHeightDivisor = 0.0f;    // 3.0  @0x14258c264
  float aimLateralRatio = 0.0f;     // 0.25 @0x142592228
  float aimTopRatio = 0.0f;         // 0.9  @0x14258c260

  // --- the jitter (0x1406e3426) ---
  float mmPerUnit = 0.0f;           // 1000 @0x14256ae34
};

std::optional<PenaltyScatterConstants> LoadPenaltyScatterConstants(const PeImage& exe,
                                                                     std::string* error);

// What FUN_1406B4000 reads. `placeKicking` and `finishing` are PES's 0..99
// abilities; `heightScale` and `lateralScale` are the tuning table's floats at
// +8 and +0xc (DAT_1436f3dc8+0x2aa8), the bar and the post.
struct PenaltyScatterInput {
  float placeKicking = 0.0f;    // the ability at +0x20
  float finishing = 0.0f;       // the ability at +0x1A
  bool penaltySpecialist = false;  // the skill bit 29 of the player's +0x40
  bool chip = false;               // the chip-shot action
  bool chipShotControl = false;    // the skill bit 31 of the player's +0x40
  float charge = 0.0f;             // the shot charge, 0.2..1.0
  float aimLateral = 0.0f;         // the aim record's +4, the distance from the centre
  float aimHeight = 0.0f;          // the aim record's +8
  float heightScale = 0.0f;        // the tuning table's +8, the bar
  float lateralScale = 0.0f;       // the tuning table's +0xc, the post
};

// FUN_1406B4000's kick-kind-54 result: the aim-point error, in metres.
float PenaltyScatterRadius(const PenaltyScatterConstants& k, const PenaltyScatterInput& in);

// The aim record's two floats the jitter moves: the lateral (m) and the height
// (m). The record's first float (the x along the goal line) is not jittered.
struct PenaltyAim {
  float lateral_m = 0.0f;
  float height_m = 0.0f;
};

// FUN_1406E3210's kick-kind-54 case (0x1406e3421-0x1406e3482): the radius
// becomes the modulus of two signed draws, the height's first and the lateral's
// second, and the lateral's sum is clamped at zero.
PenaltyAim ApplyPenaltyJitter(Random& rng, const PenaltyScatterConstants& k, float radius_m,
                              const PenaltyAim& base);

}  // namespace pes
