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

// FUN_1406ce550: the out-of-play pre-empt. Phase 1 (open play) never takes it; the row ladder
// needs the command word at req[0x14] and the clip-length host reads PES has not published yet.
// ponytail: the adapter's req[0x14] fill and the row clip lengths remove this phase gate.
bool OutOfPlayPreempt(MoveHost& host, MoveReq& req) {
  (void)host;
  (void)req;
  return false;
}

// FUN_1406b8340's tail: the P+0xbec copy and the playback store. The slip state takes req[0xec]
// unconditionally; every other state takes req[0x3b] only when req[0x3a] asks for it.
void ApplyTail(MoveStepRecord& rec, MoveHost& host, MoveReq& req, MoveState stateAtEntry) {
  if (stateAtEntry == MoveState::kRunTurnRunSlipdown) {
    host.SetRecordF0bec(req.F(moveword::kSlipAngle));
  } else if (req.Flag(moveword::kRootFacing)) {
    host.SetRecordF0bec(req.F(moveword::kRootFacingAngle));
  }
  rec.playbackSpeed = req.F(moveword::kPlayback);
}

}  // namespace movestep

// Group B's header declares the TURN_STOP entry in namespace pes (its own file calls it
// unqualified); group C owns the body in namespace pes::movestep. This is the seam between them.
void EnterTurnStop(MoveHost& host, MoveReq& req) { movestep::EnterTurnStop(host, req); }

}  // namespace pes
