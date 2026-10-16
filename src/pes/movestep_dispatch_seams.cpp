// The MOVESTEP dispatcher port's cross-group seams and the shared helper bodies the dispatcher
// links. They live outside `pesfmt` so the per-group test targets keep their probe stubs: group
// B, C and D test one group's body at a time, while the dispatcher target links this file and
// exercises the seams against the real bodies. The adapter links it too.
#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_step.hpp"
#include "pes/movestep_turn.hpp"

namespace pes {
namespace movestep {

// Group D ported its bodies with its install constants as an argument; the shared dispatcher
// passes the host's view, in the contract's signature.
void RunTurnRun(MoveHost& host, MoveReq& req) {
  RunTurnRun(host, req, host.MoveTurnConstantsView());
}
void RunTurnRunSlipdown(MoveHost& host, MoveReq& req) {
  RunTurnRunSlipdown(host, req, host.MoveTurnConstantsView());
}
void RunTurnStep(MoveHost& host, MoveReq& req) {
  RunTurnStep(host, req, host.MoveTurnConstantsView());
}
void StepTurnRun(MoveHost& host, MoveReq& req) {
  StepTurnRun(host, req, host.MoveTurnConstantsView());
}
void StepTurnStep(MoveHost& host, MoveReq& req) {
  StepTurnStep(host, req, host.MoveTurnConstantsView());
}
void EnterRunTurnRun(MoveHost& host, MoveReq& req) {
  EnterRunTurnRun(host, req, host.MoveTurnConstantsView());
}
bool EnterRunTurnStep(MoveHost& host, MoveReq& req, float threshold) {
  return EnterRunTurnStep(host, req, host.MoveTurnConstantsView(), threshold);
}
bool EnterStepTurnRun(MoveHost& host, MoveReq& req) {
  return EnterStepTurnRun(host, req, host.MoveTurnConstantsView());
}

// FUN_1406b97d0: a motion's start time in its clip. The decompile is a per-motion switch; GF
// resolves rows through its own Animetable and the row's clip supplies the start frame.
// ponytail: 0x1406b97d0's per-motion constant removes this zero.
float MotionStartSeconds(uint16_t /*motionNo*/) { return 0.0f; }

// FUN_1406ba2f0: the locomotion style group. Style 0 is PES's otherwise branch (group 80).
// ponytail: the style ladder's stamina, scripted-slot and dribble arms remove this default.
int LocomotionStyle(MoveHost& /*host*/, int /*commandId*/, float /*rateRequested*/,
                    MoveReq& /*req*/) {
  return 0;
}

// FUN_1406ce550: the out-of-play pre-empt. Phase 1 (open play) never takes it; the row ladder
// needs the command word at req[0x14] and the clip-length host reads PES has not published yet.
// ponytail: the adapter's req[0x14] fill and the row clip lengths remove this phase gate.
bool OutOfPlayPreempt(MoveHost& host, MoveReq& req) {
  (void)host;
  (void)req;
  return false;
}

}  // namespace movestep

// Group B's header declares the TURN_STOP entry in namespace pes (its own file calls it
// unqualified); group C owns the body in namespace pes::movestep. This is the seam between them.
void EnterTurnStop(MoveHost& host, MoveReq& req) { movestep::EnterTurnStop(host, req); }

}  // namespace pes
