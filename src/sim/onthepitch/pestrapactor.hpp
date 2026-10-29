// P, the actor object the Trap action chain reads (match::AnimePlayer, docs/re/pes-re-trap-inputs.md 1), written from the
// state GF holds: the man record (position, previous position, facing), the player's slot, height and stamina, and the
// locomotion phase and action ids. The PES sites that write each field are cited at the writer in pestrapactor.cpp; the
// constants (interface vtables, the locomotion-class table, the rate ceiling's) are read from the image, each behind the
// opcode of the instruction that names it.
//
// Inputs PES keeps that GF has no source for are named in PesActorOpenInputs; each carries the PES site that will remove it.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace pes {

inline constexpr std::size_t kLocomotionPhases = 0x18;  // P+0x245C below this indexes the class table (0x1406D8C59)

// What the actor's fields are made from, in PES's units. `speedByte` is the Speed ability byte (slot record +0x97, 40..99).
struct PesActorFacts {
  int slot = 0;                // P+0x2458: the squad place (PM+0x18); also the man and slot record index
  float heightCm = 180.0f;     // PM+0x30
  uint8_t phase = 0;           // P+0x245C: the locomotion phase byte
  uint8_t action = 0, sub = 0, prevAction = 0, prevSub = 0;  // P+0xAC8..+0xACB: PES's ids (2 MOVE, 3 STEP_MOVE, 7 TRAP)
  uint16_t motion = 0;         // P+0xAE0: the PES motion number playing
  uint32_t kickApproach = 0;   // P+0x430: 0 unless the kick approach is armed (adjuster A state)
  float word277C = 0.0f;       // P+0x277C and +0x2780: the phase-1 test of the class writer (writers OPEN)
  float word2780 = 0.0f;
  float speedByte = 0.0f;      // slot record +0x97
  StaminaState stamina;        // slot record: tank cur/max, breath reserve
};

// ponytail: PES inputs with no GF source, as captured bytes. Each names the PES site that removes it:
//  velocityRing: P+0x2FC, six 3-vectors read by 0x141EB50B0 (idx >= 2); its writer is OPEN (the action issuer 0x1406F0F70
//    or a per-tick write; pes-re-trap-inputs.md 1.3).
//  physiquePtr, g: P+0xA08 and the scale *(*(P+0xA08)+0xF0)+0xC0 (FUN_141AF3180); the writer is FUN_141EB5810 from the body's
//    root_main joint scale, OPEN (pes-re-trap-inputs.md 1.4). `g` also feeds the rate ceiling P+0x377C.
//  analyzePtr: P+0x3AC0, PM+0x7E0, a match::registry::AnalyzeInfoRef whose referents the chain reads; GF has no such object.
struct PesActorOpenInputs {
  std::array<uint8_t, 12> velocityRing{};
  uint64_t physiquePtr = 0;
  float g = 1.0f;
  uint64_t analyzePtr = 0;
};

// The image's bytes P is made from.
struct PesActorConstants {
  uint64_t interfaceVtable[2] = {0, 0};  // P+0xAB8, +0xAC0 (the constructor 0x1406E03D0 stores them)
  std::array<uint8_t, kLocomotionPhases> locomotionClass{};  // DAT_14344E1F0[4 * phase]: P+0xB08 (0x1406D8C92)
  float standingSpeedKmh = 0.0f;  // 0x14259222C: the root speed above which phase 0 is class 3, else 1 (0x1406D8C35)
  float tickRate = 0.0f;          // 0x14351AE54 (FUN_141E532D0): ticks per second
  float secondsPerHour60 = 0.0f;  // 0x14259A764: multiplied in twice (0x1406EA14E, 0x1406EA15B)
  float metresPerKm = 0.0f;       // 0x14256AE34 (0x1406EA168)
  float heightDivisor = 0.0f;     // 0x142598580: P+0x3B18 = height_cm / 180 (0x1406EB231)
};

// Reads the constants from the flat image (base 0x140000000). Empty with `*error` set when an opcode check fails or a
// read leaves the image.
std::optional<PesActorConstants> LoadActorConstants(const uint8_t* image, uint64_t imageBytes, std::string* error);

// P+0xB08 (FUN_1406D8BF0): the locomotion class from the phase, the root speed and the phase-1 test.
uint8_t LocomotionClass(const PesActorConstants& c, const PesActorFacts& f, const uint8_t* p);

// Writes the actor's fields into the window's P: position, previous position and heading from the man record of f.slot;
// slot, phase, action ids, motion, class, physique scale, slot-record pointer, kick-approach state, the rate ceiling
// (S1b, FUN_1406F99F0) and the interface vtables; then the open inputs.
void WriteActorP(PesWorld& world, const PesActorFacts& f, const PesActorOpenInputs& open, const PesActorConstants& c,
                 const StaminaConstants& stamina);

}  // namespace pes
