#pragma once

// PES 2021's MOVESTEP handler group C: the stepping states of the locomotion machine
// (docs/re/pes-re-anim-select.md 2.1, 5.1-5.2), ported one handler from PES's own code.
// Each handler is the body of FUN_1406ba900's case, called through the shared host.
//
// Group C states (the switch cases of FUN_1406ba900):
//   4  STEP_LOOP          FUN_1406c4c10   the side/back step loop; exits to STEP_TO_RUN,
//                                        STEP_TURN_RUN, STEP_LOOP_CHANGE, GOTO_RUN, STOP,
//                                        RUN_LOOP, STEP_TURN_STEP and (band 2) RUN_TURN_RUN
//   5  STEP_LOOP_CHANGE   FUN_1406c6480   the style-change tick of the step loop
//   6  TURN_STOP          FUN_1406cab20   the stop while still facing away
//   13 STEP_TO_RUN        FUN_1406c6f80   the transition from stepping to running
//   14 RUN_TO_STEP        FUN_1406c1070   the transition from running to stepping
// plus the TURN_STOP entry FUN_1406d2590, which only group C calls.
//
// The install data (StepTables, StepScalars) and the loaders are the contract's, in
// src/pes/movestep.hpp/.cpp; a handler reads them through MoveHost::StepTablesView() and
// MoveHost::StepConstants(). The request words the shared contract does not name yet are in
// `moveword` below, each with the PES site that reads or writes it. Where a PES read has no GF
// counterpart yet, a `// ponytail:` note names the PES site that replaces the stand-in.

#include <array>
#include <cstdint>

#include "pes/movestep.hpp"

namespace pes {
namespace moveword {
// Group C's request words that the shared contract does not name yet. Each is a PES req word;
// a byte store at `param_2 + i` is the low byte of word i. If another group needs one of these,
// it moves into the contract's `moveword` and both groups read it from there.
inline constexpr int kBandSpeed = 0x1c;      // STEP_LOOP: the blended band speed, km/h (OBS)
inline constexpr int kBandSpeedPrev = 0x1d;  // STEP_LOOP: the band speed before the change (OBS)
inline constexpr int kBandSpeedCopy = 0x22;  // STEP_LOOP: the band speed paired with the class word
inline constexpr int kBucketPrev = 0x25;     // STEP_LOOP: the bucket before the last change (OBS)
inline constexpr int kBlendA = 0x2b;         // STEP_LOOP_CHANGE: the blended heading integral
inline constexpr int kBlendB = 0x2c;         // STEP_LOOP_CHANGE: the blended heading remainder
inline constexpr int kTurnGateFlags = 0x0f;   // req[0xf] byte (req+0x3c): FUN_1406b70d0's gate flags
inline constexpr int kTurnCommandFlag = 0x15;  // req[0x15] byte (req+0x54): FUN_1406b70d0's answer inside the front gate
}  // namespace moveword

namespace movestep {

// The handlers, each taking the shared host and the request. The PES body of each is named in
// the comment.
void StepLoop(MoveHost& host, MoveReq& req);       // 4  FUN_1406c4c10
void StepLoopChange(MoveHost& host, MoveReq& req); // 5  FUN_1406c6480
void TurnStop(MoveHost& host, MoveReq& req);       // 6  FUN_1406cab20
void StepToRun(MoveHost& host, MoveReq& req);      // 13 FUN_1406c6f80
void RunToStep(MoveHost& host, MoveReq& req);      // 14 FUN_1406c1070

// Entries: the code that sets a state's request when another state (or INIT) moves into it.
void EnterTurnStop(MoveHost& host, MoveReq& req);  // FUN_1406d2590

// Group C's own helpers, ported here because only group C calls them.
// FUN_1406b6ee0: whether the step loop must stop.
bool StepStops(MoveHost& host, MoveReq& req, bool live);
// FUN_1406b70d0: whether the step loop may turn into a run turn. Its distance rows are StepTables' turn rows.
bool StepLetsRunTurn(MoveHost& host, MoveReq& req);
// FUN_1406c4c10 0x1406C4DCC-0x1406C4DEE: the band the step loop reads, from the live class word and the clamped command
// class. The loop reads the class word, walks it one step toward the command, and returns the walked band.
int StepBand(int liveBand, int commandBand);
// FUN_1406c4c10 lines 86-103: the command class the walk compares with. It is the command speed class below 3, clamped at 0
// from below, and 2 from 3 up. Under the special condition (the previous state is 0x13 or 0x10 and req[0x1a] is at or below
// the install's bound and not equal to it) it is the class word itself, read the same way. The bound is DAT_142588200.
int WalkCommandClass(int commandClass, int classWord, int prevState, float dwellElapsed, float dwellBound);

}  // namespace movestep
}  // namespace pes
