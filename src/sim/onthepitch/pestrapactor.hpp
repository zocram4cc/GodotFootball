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

#include "gamedefines.hpp"
#include "onthepitch/pesclips.hpp"
#include "pes/exe.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "onthepitch/pesbodyscan.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/physique.hpp"

namespace pes {

inline constexpr std::size_t kLocomotionPhases = 0x18;  // P+0x245C below this indexes the class table (0x1406D8C59)

// PES's anime action ids (P+0xAC8; the names are the image's table at 0x1434A5470, read by script: 0 NOTHING, 2 MOVE, 3 STEP_MOVE, 4 DRIBBLE,
// 7 TRAP, 8 LONG_PASS, 9 SHORT_PASS, 0xA SHOOT, 0xB SLIDING, 0xD TACKLE, 0x15 DODGE, 0x38 KP_CATCH, 0x3A KP_DEFLECT) and P+0xAE0 for a body
// that plays one of GF's own clips. PES has no such body: every man plays a motion of its Animetable. So P+0xAE0 holds the "none" index PES
// stores in the member's motion cells (kMemberNoneIndex, FUN_141EB42B0), which is past the 0x1125-motion table: no class predicate of the
// gate-4a handlers (FUN_140A6E290, 6E2E0, 6E4D0, the STEP handler's four motions) names it, and FUN_1406AFBC0 reads the default record for
// it. The previous value, 0xBE5, is the Animetable's CELL row count (FUN_1406AA440), not a "no motion": as a motion it is a real idle motion
// whose first cancel point is about 23 ticks in, and a body that plays no PES motion has no elapsed ticks to reach it, so every man in a GF
// clip was refused by gate 4a for good (2,806 of 4,293 slow-ball candidates on seed 7).
// ponytail: GF's own clips stand in for PES motions; MoveStepCell issuing a PES motion for every locomotion request removes the case.
inline constexpr uint8_t kAnimeActionNothing = 0;
inline constexpr uint8_t kAnimeActionMove = 2;
inline constexpr uint8_t kAnimeActionDribble = 4;
inline constexpr uint8_t kAnimeActionTrap = 7;
inline constexpr uint8_t kAnimeActionLongPass = 8;
inline constexpr uint8_t kAnimeActionShortPass = 9;
inline constexpr uint8_t kAnimeActionShoot = 0xA;
inline constexpr uint8_t kAnimeActionSliding = 0xB;
inline constexpr uint8_t kAnimeActionTackle = 0xD;
inline constexpr uint8_t kAnimeActionDodge = 0x15;
inline constexpr uint8_t kAnimeActionKeeperCatch = 0x38;
inline constexpr uint8_t kAnimeActionKeeperDeflect = 0x3A;
inline constexpr uint16_t kPNoMotion = kMemberNoneIndex;

// The PES action a GF body runs, from the clip it plays. GF has no action machine: PES writes P+0xAC8 only in FUN_1406EEBB0, which
// the action ticks call. `function` is the clip's function type, `specialVar1` its family (pesclips.hpp), `carrier` that the man is PES's
// holder (MA+0x2601C == him). Each row of the function names the PES action that plays that behaviour and cites the PES site that writes it.
// MOVE against STEP_MOVE is not derived: PES's two ids are the AI command's choice (FUN_1406EEA90's cmd[1]), and the 17 captured gate-4a
// calls (live2/pid3332804) hold both ids at phase 5 of P+0x245C and STEP_MOVE at the run-turn phase 12 and the idle phase 1, so no state
// family of GF's MOVESTEP says which one PES commanded. GF is always MOVE.
uint8_t AnimeActionOfClip(e_FunctionType function, int specialVar1, bool carrier);

// The third argument that keeps the sub id (FUN_1406EEBB0 compares it at 0x1406EED32 and 0x1406EED5C; FEINT_KIND_AUTO in the table at 0x1434A5470). Of the 125
// direct call sites, 117 pass it, five compute the sub id and three leave r8 as an earlier call set it; so P+0xAC9 equals P+0xAC8 whenever an
// action changes (every captured pair, 49 of 49, is equal).
inline constexpr uint8_t kActionSubKeep = 0x5E;
// P+0xAC8..+0xACB: the current action and sub id, then the previous pair.
struct ActionIds {
  uint8_t action = 0, sub = 0, prevAction = 0, prevSub = 0;
};
// The four bytes after PES's action switch ran with `request` and `sub` on a man whose bytes were `now`.
ActionIds ActionIdsAfterSwitch(const ActionIds& now, uint8_t request, uint8_t sub = kActionSubKeep);

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
  std::array<bool, 19> jointMask{};      // DAT_14351E540: the bones the joint block holds (the body scan's input; pesbodyscan.hpp)
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

// Whether P offset is a cell of the member constructor's start state (the "none" indices, -1/1 cells, flags, unit tables, scale, pair):
// PES's trap start path (FUN_141EB3900 sets the +0x47E/+0x480 blend counters, the unit-table cells follow the layers) moves them in the
// ticks before a mid-action window, which a GF match runs itself. The vtables and the member rate are not state: they stay derived.
bool IsMemberStateCell(std::size_t pOffset);

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
