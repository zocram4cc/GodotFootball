// The MOVESTEP group-B test probe's stand-ins. The group-B file calls four kinds of symbol
// that the dispatcher port owns or that another group's seams keep out of this target:
//   * the shared helpers the dispatcher port's dispatcher-core library defines behind its own
//     seams (IdleRow, StayDecision, LocomotionStyle, OutOfPlayPreempt);
//   * group C's TURN_STOP entry seam (namespace pes);
//   * group D's entry seams (EnterRunTurnRun, EnterRunTurnStep, EnterStepTurnRun);
//   * the run/loop ramps the dispatcher port has not ported yet (AccelerationRamp,
//     LateralDamping).
// The stand-ins record the call and return the neutral answer, so a test states which exit PES
// took without the real body's arithmetic. The real bodies are linked into the dispatcher-core
// target, not into this one, so the probe and the real bodies never share a link. ponytail:
// delete the ramp stand-ins when the two ramps land.
#include "pes/movestep.hpp"

#include <vector>

namespace pes {
namespace probe {

int idleRowCalls = 0;
int stayCalls = 0;
bool stayAnswer = false;
int styleCalls = 0;
int styleAnswer = 0;
int rampCalls = 0;
int dampingCalls = 0;
int outOfPlayCalls = 0;
bool outOfPlayAnswer = false;
int turnStopCalls = 0;
int runTurnRunCalls = 0;
int runTurnStepCalls = 0;
bool runTurnStepAnswer = false;
int stepTurnRunCalls = 0;
bool stepTurnRunAnswer = false;
// The group-B handlers' own observable calls, so a test can check which exit was taken.
int issueCount = 0;
std::vector<int> enterStopCalls;
std::vector<int> enterGotoRunCalls;
int accelEntryCalls = 0;
int decelEntryCalls = 0;

void Reset() {
  idleRowCalls = 0;
  stayCalls = 0;
  stayAnswer = false;
  styleCalls = 0;
  styleAnswer = 0;
  rampCalls = 0;
  dampingCalls = 0;
  outOfPlayCalls = 0;
  outOfPlayAnswer = false;
  turnStopCalls = 0;
  runTurnRunCalls = 0;
  runTurnStepCalls = 0;
  runTurnStepAnswer = false;
  stepTurnRunCalls = 0;
  stepTurnRunAnswer = false;
  issueCount = 0;
  accelEntryCalls = 0;
  decelEntryCalls = 0;
  enterStopCalls.clear();
  enterGotoRunCalls.clear();
}

}  // namespace probe

namespace movestep {

int IdleRow(MoveHost& /*host*/, MoveReq& /*req*/) {
  ++probe::idleRowCalls;
  return 0x5a1;  // FUN_1406b93a0's fallback row, as the shared contract's comment records.
}

bool StayDecision(MoveHost& /*host*/, MoveReq& /*req*/) {
  ++probe::stayCalls;
  return probe::stayAnswer;
}

int LocomotionStyle(MoveHost& /*host*/, int /*animeState*/, float /*rateRequested*/,
                    MoveReq& /*req*/, float /*rateMax*/, bool /*rateHeld*/) {
  ++probe::styleCalls;
  return probe::styleAnswer;
}


void AccelerationRamp(MoveHost& /*host*/, MoveReq& /*req*/) { ++probe::rampCalls; }

void LateralDamping(MoveHost& /*host*/, MoveReq& /*req*/) { ++probe::dampingCalls; }

bool OutOfPlayPreempt(MoveHost& /*host*/, MoveReq& /*req*/) {
  ++probe::outOfPlayCalls;
  return probe::outOfPlayAnswer;
}

// FUN_1406ccd00 (group B's ACCELERATE entry) and FUN_1406cdaf0 (group B's DECELERATE entry):
// the port's next pieces. Until their bodies land the probe returns PES's "no write"
// answer, so the arms that call them take their no-branch.

}  // namespace movestep

// FUN_1406d2590 (group C): the turn-stop entry. The group-B header declares it in namespace
// pes, as the group-B file calls it unqualified from pes::movestep.
void EnterTurnStop(MoveHost& /*host*/, MoveReq& /*req*/) { ++probe::turnStopCalls; }

}  // namespace pes

// FUN_1406d6780 (group D): the RUN_TURN_RUN entry.
namespace pes {
namespace movestep {
void EnterRunTurnRun(MoveHost& /*host*/, MoveReq& /*req*/) { ++probe::runTurnRunCalls; }

// FUN_1406d2f60 (group D): the RUN_TURN_STEP entry.
bool EnterRunTurnStep(MoveHost& /*host*/, MoveReq& /*req*/, float /*threshold*/) {
  ++probe::runTurnStepCalls;
  return probe::runTurnStepAnswer;
}

// FUN_1406d38d0 (group D): the STEP_TURN_RUN entry.
bool EnterStepTurnRun(MoveHost& /*host*/, MoveReq& /*req*/) {
  ++probe::stepTurnRunCalls;
  return probe::stepTurnRunAnswer;
}
}  // namespace movestep
}  // namespace pes
