// FUN_1406ba900: one PES tick of the MOVESTEP machine. The state switch is PES's own: case 0
// is INIT, the yaw lock FUN_1406cbb00 runs after cases 1, 2, 3, 4, 5, 20, 21, 22 and 23, and
// the facing/config words are the request's. This object is separate from movestep.cpp so a
// group test that only needs RunRateAllowed does not pull the dispatcher's cross-group links.
#include "pes/movestep.hpp"

namespace pes {
namespace {

// FUN_1406cbb00: the dispatcher's yaw lock. It folds the angle into [0, 360) and stores P+0xbec.
void YawLock(MoveHost& host, float angleDeg) {
  host.SetRecordF0bec(FoldFull(angleDeg, host.RunConstantsView()));
}

}  // namespace

void Dispatch(MoveState state, MoveHost& host, MoveReq& req) {
  // FUN_1406ba900's head: the two dwell clocks advance one PES tick before the state runs.
  const RunConstants& k = host.RunConstantsView();
  const float tick = k.one / host.TicksPerSecond();
  req.SetF(moveword::kDwellElapsed, req.F(moveword::kDwellElapsed) + tick);
  req.SetF(moveword::kDwellElapsed2, req.F(moveword::kDwellElapsed2) + tick);
  if (movestep::OutOfPlayPreempt(host, req)) return;

  switch (state) {
    case MoveState::kInit:
      movestep::Init(host, req);
      break;
    case MoveState::kIdle:
      movestep::Idle(host, req);
      YawLock(host, req.F(moveword::kFacing));
      break;
    case MoveState::kIdleTurn:
      movestep::IdleTurn(host, req);
      YawLock(host, req.F(moveword::kFacing));
      break;
    case MoveState::kRunLoop:
      movestep::RunLoop(host, req);
      YawLock(host, req.F(moveword::kHeading));
      break;
    case MoveState::kStepLoop:
      movestep::StepLoop(host, req);
      YawLock(host, req.F(moveword::kHeading));
      break;
    case MoveState::kStepLoopChange:
      movestep::StepLoopChange(host, req);
      YawLock(host, req.F(moveword::kHeading));
      break;
    case MoveState::kTurnStop:
      movestep::TurnStop(host, req);
      break;
    case MoveState::kGotoRun:
      movestep::GotoRun(host, req);
      break;
    case MoveState::kStop:
      movestep::Stop(host, req);
      break;
    case MoveState::kAccelerate:
      movestep::Accelerate(host, req);
      break;
    case MoveState::kDecelerate:
      movestep::Decelerate(host, req);
      break;
    case MoveState::kRunTurnRun:
      movestep::RunTurnRun(host, req);
      break;
    case MoveState::kStepToRun:
      movestep::StepToRun(host, req);
      break;
    case MoveState::kRunToStep:
      movestep::RunToStep(host, req);
      break;
    case MoveState::kStepTurnRun:
      movestep::StepTurnRun(host, req);
      break;
    case MoveState::kRunTurnStep:
      movestep::RunTurnStep(host, req);
      break;
    case MoveState::kIdleOther:
      movestep::IdleOther(host, req);
      break;
    case MoveState::kRunTurnRunSlipdown:
      movestep::RunTurnRunSlipdown(host, req);
      break;
    case MoveState::kStepTurnStep:
      movestep::StepTurnStep(host, req);
      break;
    case MoveState::kIdlePkPlayer:
      movestep::IdlePkPlayer(host, req);
      YawLock(host, req.F(moveword::kFacing));
      break;
    case MoveState::kIdlePkGk:
      movestep::IdlePkGk(host, req);
      YawLock(host, req.F(moveword::kFacing));
      break;
    case MoveState::kTimingStep:
      movestep::TimingStep(host, req);
      YawLock(host, req.F(moveword::kHeading));
      break;
    case MoveState::kDemo:
      movestep::Demo(host, req);
      YawLock(host, req.F(moveword::kFacing));
      break;
    case MoveState::kGotoStep:
      break;  // never dispatched (AS 2.1)
  }
}

}  // namespace pes
