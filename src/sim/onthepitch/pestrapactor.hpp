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

#include "pes/exe.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/physique.hpp"

namespace pes {

inline constexpr std::size_t kLocomotionPhases = 0x18;  // P+0x245C below this indexes the class table (0x1406D8C59)

// PES's anime action id for MOVE (P+0xAC8; names read from 0x1434A5470: 2 MOVE, 3 STEP_MOVE, 7 TRAP) and the "no motion"
// sentinel of P+0x2810 (0xBE5, FUN_1406EB670; also the Animetable's row count).
inline constexpr uint8_t kAnimeActionMove = 2;
inline constexpr uint16_t kPNoMotion = 0xBE5;
inline constexpr uint8_t kAnimeActionTrap = 7;

// One motion of the anime player's list, as FUN_141EB4650 reads it (FUN_141AF3070 id, FUN_141AF4160 flag byte, FUN_141AF3100
// seconds played, FUN_141AF3330 the layer's rate).
struct PesPlayingMotion {
  uint16_t id = kPNoMotion;
  uint8_t flag = kMotionEntryListed;
  float seconds = 0.0f;
  float rate = 1.0f;  // ponytail: GF's clip has no layer rate; 1.0 is the neutral rate (the captures read 1.02..1.09)
};

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
  float legSlider = 0.0f;      // physique block +0x30 (PM+0x7B4), -1..1. ponytail: no GF source (appearance nibble 0x11:0,
                               // FUN_141AD98B0); 0 is neutral
  int side = 0;                // the actor's side: P+0x3AC0 is that side's AI context (the window's ctx record)
  StaminaState stamina;        // slot record: tank cur/max, breath reserve
  // ponytail: the anime player's motion list. PES's FUN_141EB4650 reads it from the AnimePlayer's layer tables (P+0xA08: count
  // FUN_141AF34B0, entry i FUN_141AF3070 / 4160 / 3100 / 3330), which the anime engine fills and GF does not run (the closure of
  // that update is 4277 instructions over the player's blend structures, not in the window). The body's own clip, while it has
  // frames left, is the one listed motion; FUN_141EB4650's table block then builds the table from it (RebuildMotionTable). The
  // AnimePlayer's layer tables (the writer of those cells: SetMotion FUN_141AF61C0 and the engine update FUN_141AF7A90) remove it.
  std::array<PesPlayingMotion, kPMotionTableMax> playing{};
  std::size_t playingCount = 0;
  // The man is in the Trap action: the tick owns the action ids (P+0xAC8..0xACB), the phase byte (P+0x245C), the motion
  // (P+0xAE0) and the class (P+0xB08), so WriteActorP leaves them as the last tick wrote them.
  bool trapActive = false;
};

// ponytail: the one PES input with no GF source. It names the PES site that removes it:
//  velocityRing: P+0x2FC, six 3-vectors (12 bytes each) read by 0x141EB50B0 (idx >= 2: entry min(idx / 10, 5)); its writer is
//    OPEN (the action issuer 0x1406F0F70 or a per-tick write; docs/ports/pes-port-TrapOpen2.md 6). The oracle fills it from the
//    capture; a match fills each entry with the player's present velocity (the decoded entry is the predicted displacement
//    over 10 (k + 1) ticks divided by 10 (k + 1): the same vector when the prediction holds the velocity, which drops PES's
//    braking prediction, INF from two snapshots of one player).
// (G and its chain at P+0xA08 are derived from the facts: pes::PhysiqueG; P+0x3AC0 names the side's AI context in the window.)
inline constexpr std::size_t kVelocityRingEntries = 6;
inline constexpr std::size_t kVelocityRingEntryBytes = 12;
struct PesActorOpenInputs {
  std::array<uint8_t, kVelocityRingEntries * kVelocityRingEntryBytes> velocityRing{};
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
  uint64_t memberVtable[3] = {0, 0, 0};  // P+0x428, +0x498, +0x4D0: the member constructor 0x141EB42B0's three `lea rax,[vtable]` sites
  float memberRateNumerator = 0.0f;      // 0x14256AE1C: P+0x41C = this / tickRate (0x141EB4333)
};

// Reads the constants from the install's exe, each behind the opcode of the instruction that names it. Empty with `*error` set
// when an opcode check fails or a read leaves the image.
std::optional<PesActorConstants> LoadActorConstants(const PeImage& exe, std::string* error);

// P+0xB08 (FUN_1406D8BF0): the locomotion class from the phase, the root speed and the phase-1 test.
uint8_t LocomotionClass(const PesActorConstants& c, const PesActorFacts& f, const uint8_t* p);

// Writes the actor's fields into the window's P: position, previous position and heading from the man record of f.slot;
// slot, phase, action ids, motion, class, physique scale, slot-record pointer, kick-approach state, the rate ceiling
// (S1b, FUN_1406F99F0) and the interface vtables; then the open inputs.
void WriteActorP(PesWorld& world, const PesActorFacts& f, const PesActorOpenInputs& open, const PesActorConstants& c,
                 const StaminaConstants& stamina, const PhysiqueConstants& physique);

// The member constructor FUN_141EB42B0's stores into the slot's P (the vtables the tick calls through, the "none" indices, the unit
// tables), once per match when PES builds the actor, before any WriteActorP: the per-tick writes leave these bytes as the tick left them.
void ConstructActorP(PesWorld& world, int slot, const PesActorConstants& c);

// The AnimPlugin constructor FUN_141B158F0's stores into the slot's O1 (nine layer objects, the self-pointing layer array at +0xB0; a
// recording of the constructor from the image, pesanimplugin_stores.inc), once per match. NOT built (the constructor's heap calls, OPEN):
// the parameter blocks at +0x90/+0x98/+0xD40..+0xD88 and +0x1370, and the base-class fields FUN_142397EA0 / FUN_1403CF7E0 set.
void ConstructAnimPlugin(PesWorld& world, int slot);
// What a constructor cell is. The layers' weights and blend state the constructor also stores (kState) are rewritten by PES's animation
// engine, which GF does not run; the other two are structure: a pointer into the object (window addresses differ from any capture's) and
// an image VA (a vtable).
enum class AnimPluginCell { kNotStored, kState, kSelfPointer, kImageAddress };
AnimPluginCell AnimPluginCellAt(std::size_t offset);

// The action table's constructors (FUN_1406DF630 objects + FUN_1406D9040 pointers; pesactiontable_stores.inc) into the slot's P+0xC40, once
// per match, with each pointer naming a window object. Its stores include the Trap action's vtable (P+0xC78) and the other 89 actions'
// start states, which the action switch's exit/enter calls run on. IsActionTablePointer: P offset is one of the 94 pointer cells (window
// addresses differ from any capture's heap addresses).
void ConstructActionTable(PesWorld& world, int slot);
bool IsActionTablePointer(std::size_t pOffset);

// The action switch's stores for a committed trap (FUN_1406EEBB0: the previous ids take the current pair, the current pair becomes
// TRAP, TRAP) and the action's start phase 0 (P+0x245C), which the tick's phase machine switches on. The man's action object was
// written by the chain; from here the tick owns the ids (PesActorFacts::trapActive).
void StartTrapAction(PesWorld& world, int slot);

}  // namespace pes
