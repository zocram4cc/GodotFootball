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
}  // namespace moveword

namespace movestep {

// FUN_1406b70d0's four tables, indexed by req[0x7c] (the foot band side). Read from the exe;
// the port's StepTables carries them and the install relations test checks their sizes.
struct TurnDistances {
  std::array<float, 4> front;   // 0x1425dde70: the near bucket's distance
  std::array<float, 4> back;    // 0x1425dde80: the far bucket's distance
  std::array<float, 4> centre;  // 0x1425dde90: the centre's distance
  std::array<float, 4> held;    // 0x1425ddea0: the held ball's distance
};
std::optional<TurnDistances> LoadTurnDistances(const PeImage& exe, std::string* error);

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
// FUN_1406b70d0: whether the step loop may turn into a run turn. The four distance tables are
// the exe's own, loaded by LoadTurnDistances; the handler takes them as a parameter because
// only group C reads them (no host accessor until another group does).
bool StepLetsRunTurn(MoveHost& host, const TurnDistances& d, MoveReq& req);

}  // namespace movestep
}  // namespace pes
