#pragma once

// PES 2021's MOVESTEP handler group D (FUN_1406ba900's cases): the turn handlers that carry a
// player between the run and the step families, the slip tail RUN_TURN_RUN falls into, and the
// three entries that move another state into them.
//
//   12 RUN_TURN_RUN         FUN_1406c14c0   the turning run: keeps the run family, damps the
//                                            body towards the target, re-selects the row at the
//                                            clip's end and falls into the slip tail
//   18 RUN_TURN_RUN_SLIPDOWN FUN_1406c2f80  the slip tail: damps the facing towards the move
//                                            heading while the clip's frames run out
//   16 RUN_TURN_STEP        FUN_1406c3470   the planted turn: reorients, then either stays in
//                                            the step family or hands back to the run loop
//   15 STEP_TURN_RUN        FUN_1406c7400   the side/back step that resumes the run
//   19 STEP_TURN_STEP       FUN_1406c8d80   the step-to-step turn: corrects the facing
//
// Entries:
//   FUN_1406d6780  RUN_TURN_RUN   FUN_1406d2f60  RUN_TURN_STEP (with its threshold)
//   FUN_1406d38d0  STEP_TURN_RUN
//
// Every function is a port of one PES body: the order, the branch structure and the field writes
// are PES's. The install constants are read from the exe by the loader in the .cpp; tests pass
// synthetic ones. Where a PES read has no GF counterpart yet, a `// ponytail:` note names the PES
// site that supplies it.

#include <array>
#include <cstdint>

#include "pes/movestep.hpp"

namespace pes {
namespace moveword {
// The request words group D reads or writes that the shared contract does not name yet. Each is
// a PES req word; a byte store at `param_2 + i` is the low byte of word i.
inline constexpr int kClassWord = 0x1f;      // req[0x1f]: the live class 0..3 (-1 = unset)
inline constexpr int kSpeedWord = 0x1d;      // req[0x1d]: the live speed word (km/h packed)
inline constexpr int kPrevClassWord = 0x24;  // req[0x24]: the class before the change
inline constexpr int kPrevSpeedWord = 0x22;  // req[0x22]: the speed before the change
inline constexpr int kPrevBucket = 0x25;     // req[0x25]: the bucket before the change
inline constexpr int kPrevKind = 0x1e;       // req[0x1e]: the kind the live clip runs at
inline constexpr int kRowAngle = 0x17;       // req[0x17]: the row echo's angle word
inline constexpr int kTurnedRow = 0x18;      // req[0x18]: the turned heading
inline constexpr int kReorientFlag = 0x3c;   // req[0x3c] byte: a reorient is due
inline constexpr int kTurnFrames = 0x3e;     // req[0x3e]: the turn's tick count
inline constexpr int kMirrorFlag = 0x3f;     // req[0x3f] byte: the mirror re-select flag
inline constexpr int kAngle3d = 0x3d;        // req[0x3d]: the echo block's angle word
inline constexpr int kClampWord = 0x6c;      // req[0x6c]: cleared by the slip tail
inline constexpr int kSlipCandidate = 0x60;  // req[0x60]: the slip row candidate
inline constexpr int kSlipRowOut = 0xf4;     // req[0xf4]: the re-selected row
inline constexpr int kSlipNewRow = 0xf0;     // req[0xf0] byte: a re-selection is due
inline constexpr int kSlipRowFrames = 0xf8;  // req[0xf8]: the re-selected row's frames
inline constexpr int kSlipMirrorOut = 0xfc;  // req[0xfc] byte: the row's mirror bit
inline constexpr int kSlipRate = 0x128;       // req[0x128]: the parametric rate
inline constexpr int kSlipSpeed = 0x74;       // req[0x74]: the speed word
inline constexpr int kFootFlag = 0x98;        // req[0x98] byte: the foot band 1..2 flag
inline constexpr int kStyle = 0x50;           // req[0x50] byte: the locomotion style
}  // namespace moveword

namespace movestep {
// The exe constants group D reads, by VA. The install test checks their relations; the values
// belong to the install, never to the code.
struct MoveTurnConstants {
  // Arithmetic.
  float half;           // 0.5   (DAT_142565600): the generic half / blend factor
  float two;            // 2.0   (DAT_142565604): the class-rate distance bands
  float four;           // 4.0   (DAT_1425656c0): the rate-band ceiling and the tick window
  float negZero;        // -0.0  (DAT_1425656d0): the sign flip of the maxss pair
  float one;            // 1.0   (DAT_14256ae1c): the default scale
  float thousand;       // 1000  (DAT_14256ae34): the clip-frame divisor
  float negOne;         // -1.0  (DAT_142588218): the left sign
  float three;          // 3.0   (DAT_14258c264): the run-stop tail band
  float quarter;        // 0.25  (DAT_142592228): the slip damping band
  float five;           // 5.0   (DAT_14259222c): the speed band floor
  float pointTwo;       // 0.2   (DAT_142598578): the relax override
  float fps;            // 60.0  (DAT_14259a764): the ticks-per-second scale's partner
  float turnTicks;      // 6.0   (DAT_14259bdec): the turn-probability ticks
  float turnTicksB;     // 2.5   (DAT_14259bde4): the slow-player tick window
  float speedA;         // 8.0   (DAT_14259bdf4): the first speed band
  float speedB;         // 11.0  (DAT_14259bdfc): the second speed band
  float speedC;         // 12.0  (DAT_14259be04): the slipdown decision band
  float speedD;         // 16.0  (DAT_14259be10): the third speed band
  float stampsA;        // 20.0  (DAT_14259be1c): the recent-stamp window
  float speedE;         // 25.0  (DAT_14259be20): the tail speed band
  float speed135;       // 13.5  (DAT_1425a1ffc): the out-of-play run band
  float speed22;        // 22.0  (DAT_1425c48c0): the run band
  float speed19;        // 19.0  (DAT_1425c65e8): the slipdown band
  float speed1475;      // 14.75 (DAT_1425ddf88): the stop band
  float speed39375;     // 39.375(DAT_1425ddfa0): the run-stop band
  float speed55;        // 5.5   (DAT_1425c8378): the run-stop tail band
  float kmhToRate;      // 3.6   (DAT_14259ce38): the km/h to rate scale
  float thirty;         // 30.0  (DAT_142591140): the stamp window
  float speed15;        // 15.0  (DAT_14259be0c)
  float speed28;        // 28.0  (DAT_1425a2004): the speed clamp ceiling
  float band15m;        // 1.5   (DAT_14259bddc): the slow band floor
  float displace04;     // 0.4   (DAT_14259b89c): the displacement test
  float blend03;        // 0.3   (DAT_142590d98): the fallback blend
  float relax07;        // 0.7   (DAT_14257a194): the relax override
  float startHalf;      // 0.6   (DAT_14257a190): FUN_1406b97d0's start-time base
  float clipBlend;      // 1/6   (DAT_1425ddf50): the vt+0x10 blend duration
  float loopBlend;      // 0.1   (DAT_142588200): the loop-branch vt+0x10 blend duration
  float westBlend;      // 1/7.5 (DAT_1425ddf4c): the step clip request's blend duration
  float gFloor;         // 0.85  (DAT_1425ab120): the clip-length floor
  float playbackFloor;  // 0.95  (DAT_1425ab124): the playback floor
  float turnRamp;       // 0.15  (DAT_1425c48a0): the playback ramp and the rate floor
  float p8;             // 0.8   (DAT_142572348): the clip-length window
  float ten;            // 10.0  (DAT_142588210)
  float zero;           // 0.0   (DAT_1425c4890)

  // Angles.
  float halfTurn;       // 180.0 (DAT_142598580)
  float fullTurn;       // 360.0 (DAT_14259858c)
  float negFullTurn;    // -360  (DAT_142598598)
  float negHalfTurn;    // -180  (DAT_1425ddffc): FUN_1406b8da0's lower fold
  float angle225m;      // 22.5  (DAT_14259d618): the narrow turn
  float angle45;        // 45.0  (DAT_14259be30)
  float angle5625;      // 56.25 (DAT_14259be3c)
  float angle675;       // 67.5  (DAT_1425cca34)
  float angle90;        // 90.0  (DAT_14259be5c)
  float angle135;       // 135.0 (DAT_14259be60)
  float angle225;       // 225.0 (DAT_14259be70): the mirror threshold
  float angle1125;      // 112.5 (DAT_1425cb30c): the look-at threshold
  float angle1125b;     // 11.25 (DAT_1425cc680): the jostle band
  float angle3375;      // 33.75 (DAT_1425cc688): the jostle mirror
  float angle1575;      // 157.5 (DAT_1425cca3c): the back-turn threshold
  float turn7875;       // 78.75 (DAT_14259be50): the run-turn row-family band
  float turn84375;      // 84.375(DAT_14259be54): the step-turn jostle band
  float seventy;        // 70.0  (DAT_14259be4c): FUN_1406b8da0's argument
  float neg70;          // -70.0 (DAT_1425c4900): the mirrored jostle argument
  float neg90;          // -90.0 (DAT_1425c5958): the look-back heading offset
  float angle3325;      // 33.25 (DAT_1425ddf9c): the mirror test
  float angle40;        // 40.0  (DAT_14259be2c): the bd6 near band
  float speed26;        // 26.0  (DAT_14259857c): the rate tail band
  float turn50625;      // 50.625 (DAT_1425ddfa4)
  float turn81875;      // 81.875 (DAT_1425ddfa8)
  float turn101250;     // 101.25(DAT_1425ddfac)
  float turn123750;     // 123.75(DAT_1425ddfb4)
  float turn140625;     // 140.625(DAT_1425ddfb8)
  float turn146250;     // 146.25(DAT_1425ddfbc)
  float turn163125;     // 163.125(DAT_1425ddfc4)
  float turn168750;     // 168.75(DAT_1425ddfcc)
  float turn174375;     // 174.375(DAT_1425ddfd0)

  // FUN_1406ba070's ramp, the same VAs the shared PlaybackConstants loader reads; kept here so
  // a synthetic group-D test needs no PlaybackConstants instance.
  float gLow;   // 0.9166 (DAT_1425ddf78)
  float gHigh;  // 1.0833 (DAT_1425ddf80)
  float gSlope; // -0.1667(DAT_1425ddfe0)

  // The install tables the handlers index. Each is read straight from the exe; the values are
  // the install's, never the code's.
  std::array<float, 10> bucketAngle;    // DAT_1425dd660: bucket -> turn angle, degrees
  std::array<int32_t, 10> bucketColumn;  // DAT_1425dd988: bucket -> Animetable column
  std::array<uint8_t, 10> bucketLeft;   // DAT_1425dd950: bucket -> left side
  std::array<int32_t, 64> stepRow;      // DAT_1425dd9b0: kind + class*8 -> representative row
};
std::optional<MoveTurnConstants> LoadMoveTurnConstants(const PeImage& exe, std::string* error);

// 12: RUN_TURN_RUN (FUN_1406c14c0).
void RunTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k);
// 18: RUN_TURN_RUN_SLIPDOWN (FUN_1406c2f80).
void RunTurnRunSlipdown(MoveHost& host, MoveReq& req, const MoveTurnConstants& k);
// 16: RUN_TURN_STEP (FUN_1406c3470).
void RunTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k);
// 15: STEP_TURN_RUN (FUN_1406c7400).
void StepTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k);
// 19: STEP_TURN_STEP (FUN_1406c8d80).
void StepTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k);

// FUN_1406d6780: the RUN_TURN_RUN entry.
void EnterRunTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k);
// FUN_1406d2f60: the RUN_TURN_STEP entry, with the threshold the caller computes.
bool EnterRunTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k, float threshold);
// FUN_1406d38d0: the STEP_TURN_RUN entry.
bool EnterStepTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k);
}  // namespace movestep
}  // namespace pes
