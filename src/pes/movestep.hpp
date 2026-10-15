#pragma once

// PES 2021's MOVESTEP record and its playback writers, the per-player locomotion
// state the action dispatcher keeps (docs/re/pes-re-anim-select.md 1-2, 3.5, 4).
// Each writer names the PES function that stores its field.

#include <cstdint>
#include <optional>
#include <string>

namespace pes {

class PeImage;

// FUN_1406ba900's switch cases 0..23, by the MOVESTEP_* names at 0x1425dd698.
enum class MoveState : uint8_t {
  kInit = 0,
  kIdle = 1,
  kIdleTurn = 2,
  kRunLoop = 3,
  kStepLoop = 4,
  kStepLoopChange = 5,
  kTurnStop = 6,
  kGotoRun = 7,
  kStop = 8,
  kAccelerate = 9,
  kDecelerate = 10,
  kGotoStep = 11,  // never dispatched (AS 2.1)
  kRunTurnRun = 12,
  kStepToRun = 13,
  kRunToStep = 14,
  kStepTurnRun = 15,
  kRunTurnStep = 16,
  kIdleOther = 17,
  kRunTurnRunSlipdown = 18,
  kStepTurnStep = 19,
  kIdlePkPlayer = 20,
  kIdlePkGk = 21,
  kTimingStep = 22,
  kDemo = 23,
};
inline constexpr int kMoveStateCount = 24;

// The MOVESTEP fields of one player (PES's P+0x245a..0x3758).
struct MoveStepRecord {
  MoveState state = MoveState::kInit;     // P+0x245c
  MoveState previous = MoveState::kInit;  // P+0x245d
  uint8_t speedClass = 0;                 // P+0x245a: command speed class (writer open, AS 2.3)
  uint8_t footBand = 0;                   // P+0x245b: foot / start band (writer open, AS 2.3)
  float rateCeiling = 0.0f;               // P+0x377c: top running rate (FUN_1406f99f0)
  int rateStamp = 0;                      // P+0x3778: match tick the ceiling was computed on
  float playbackSpeed = 1.0f;             // P+0x3758: FUN_1406f0f70 stores the SetMotion playback

  // FUN_1406b8340, early-out branch after FUN_140708750: the state falls back to INIT.
  void EnterInit() {
    previous = state;
    state = MoveState::kInit;
  }
  // FUN_1406b8340 tail: when a handler set the transition flag (req[0x38]), the
  // record enters the state the handler wrote to req[0x39].
  void Enter(MoveState next) {
    previous = state;
    state = next;
  }
};

// FUN_1406f99f0's guard and store: the ceiling is recomputed only when the match
// tick moves (P+0x3778 against MI+0x1198). `ceiling` is FUN_1406f99f0's value,
// which the stamina port computes as TopRate (pesstamina.cpp) at the record's G.
void StoreRateCeiling(MoveStepRecord& r, int tick, float ceiling);

// PES's float constants behind the playback writers, read from the exe's .rdata by
// LoadPlaybackConstants. The values are the exe's; tests pass synthetic ones.
struct PlaybackConstants {
  float gLow;       // 0.9166: the physique scale G where the playback ramp begins
  float gHigh;      // 1.0833: where it ends
  float gSlope;     // -0.1667: the ramp's slope, gLow - gHigh (FUN_1406ba070)
  float one;        // 1.0
  float stepSlope;  // 0.15: step playback per unit of the ramp (FUN_1406ba070)
  float stepBase;   // 0.95: the playback floor (FUN_1406ba070, FUN_1406e9f10)
  float turnP2;     // 0.1: turn playback per unit of p squared (FUN_1406e9f10)
  float turnT;      // 0.05: turn playback per unit of the ramp (FUN_1406e9f10)
};

std::optional<PlaybackConstants> LoadPlaybackConstants(const PeImage& exe, std::string* error);

// FUN_1406ba070: the step and stop playback, stepBase + stepSlope * t(G).
float StepPlayback(float g, const PlaybackConstants& k);
// FUN_1406e9f10: the turn, accelerate and decelerate playback,
// stepBase + turnP2 * p squared + turnT * t(G).
float TurnPlayback(float g, float p, const PlaybackConstants& k);

}  // namespace pes
