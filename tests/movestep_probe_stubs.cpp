// The MOVESTEP group-B test probe's stand-ins. The group-B file calls four kinds of symbol
// that other groups own:
//   * the shared helpers the dispatcher port defines (IdleRow, StayDecision, SpeedToRate,
//     LocomotionStyle, AccelerationRamp, LateralDamping, OutOfPlayPreempt, MotionStartSeconds);
//   * group C's entries and helpers (EnterTurnStop, StepStops, StepLetsRunTurn);
//   * group D's entries (EnterRunTurnRun, EnterRunTurnStep, EnterStepTurnRun).
// None of them is defined until its group lands, so the group-B test target cannot link.
// This file supplies a stand-in of each, deliberately inert: it records the call and returns
// the neutral answer, so a test that reaches one states which exit PES took without the real
// body's arithmetic. The real bodies replace this file; the group-B tests then exercise the
// same calls against them. ponytail: delete this file when the dispatcher port's
// movestep_common.cpp and groups C and D land.
#include "pes/movestep.hpp"

#include <vector>

namespace pes {
namespace probe {

int idleRowCalls = 0;
int stayCalls = 0;
bool stayAnswer = false;
int speedToRateCalls = 0;
float speedToRateAnswer = 0.5f;
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

void Reset() {
  idleRowCalls = 0;
  stayCalls = 0;
  stayAnswer = false;
  speedToRateCalls = 0;
  speedToRateAnswer = 0.5f;
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

float MotionStartSeconds(uint16_t /*motionNo*/) { return 0.0f; }

float SpeedToRate(MoveHost& /*host*/, float speedKmh, int /*tableSelector*/) {
  ++probe::speedToRateCalls;
  (void)speedKmh;
  return probe::speedToRateAnswer;
}

int LocomotionStyle(MoveHost& /*host*/, int /*commandId*/, float /*rateRequested*/, MoveReq& /*req*/) {
  ++probe::styleCalls;
  return probe::styleAnswer;
}

void AccelerationRamp(MoveHost& /*host*/, MoveReq& /*req*/) { ++probe::rampCalls; }

void LateralDamping(MoveHost& /*host*/, MoveReq& /*req*/) { ++probe::dampingCalls; }

bool OutOfPlayPreempt(MoveHost& /*host*/, MoveReq& /*req*/) {
  ++probe::outOfPlayCalls;
  return probe::outOfPlayAnswer;
}

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
