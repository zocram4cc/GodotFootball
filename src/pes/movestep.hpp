#pragma once

// PES 2021's MOVESTEP machine: the MOVE action's per-player state record, the move request
// block (`req`), the host interface the handlers call, the state ids, and the playback writers
// (docs/re/pes-re-anim-select.md 1-2, 3.5, 4).
//
// Shared contract (v0). Handler groups port FUN_1406ba900's cases against this header; the
// header is owned by the dispatcher port, so group files only include it, never edit it.
// Fields are tagged by their evidence: OBS = a decompiled store or read in the request fill
// (FUN_1406b7560), the dispatcher (FUN_1406ba900) or the tail (FUN_1406b8340); AS = the
// anim-select report's table (pes-re-anim-select.md 1.1, 5.2) with no store seen yet.

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace pes {

class PeImage;

// FUN_1406ba900's switch cases 0..23, by the MOVESTEP_* names at 0x1425dd698 (AS 2.1).
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

// The MOVESTEP fields of one player (PES's P+0x245a..0x3758). Writers are named in the .cpp.
struct MoveStepRecord {
  MoveState state = MoveState::kInit;     // P+0x245c: the current phase (OBS: FUN_1406b8340 tail)
  MoveState previous = MoveState::kInit;  // P+0x245d
  uint8_t speedClass = 0;                 // P+0x245a: the action's speed class 0..4 (A-AN6)
  uint8_t footBand = 0;                   // P+0x245b: foot / start band (A-AN6)
  float rateCeiling = 0.0f;               // P+0x377c: top running rate (FUN_1406f99f0)
  int rateStamp = 0;                      // P+0x3778: match tick the ceiling was computed on
  float playbackSpeed = 1.0f;             // P+0x3758: the SetMotion playback (FUN_1406f0f70)

  // FUN_1406b8340, early-out branch after FUN_140708750: the state falls back to INIT.
  void EnterInit() {
    previous = state;
    state = MoveState::kInit;
  }
  // FUN_1406b8340 tail: when a handler set the transition flag (req[0x38]), the record enters
  // the state the handler wrote to req[0x39].
  void Enter(MoveState next) {
    previous = state;
    state = next;
  }
};

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


// FUN_1406f99f0's guard and store: the ceiling is recomputed only when the match tick moves
// (P+0x3778 against MI+0x1198). `ceiling` is the TopRate value (pesstamina.cpp) at the record's G.
void StoreRateCeiling(MoveStepRecord& r, int tick, float ceiling);

// ---------------------------------------------------------------------------------------------
// The move request block: PES's `req` (FUN_1406b7560 fills it; FUN_1406ba900 and the handlers
// read and write it; FUN_1406b8340's tail issues SetMotion / SetParametric from it).
// Word i is req[i], four bytes at 4*i. A byte store at `param_2 + i` is the low byte of word i.
// Words 0x17..0x2e, 0x4b and 0x50 are copied in from and back to the player record
// (P+0x2770..P+0x27d4, P+0x3750, P+0x27b1); the adapter does that, handlers see plain words.
// ---------------------------------------------------------------------------------------------
// The fill (FUN_1406b7560) writes words up to req[0x1a8], so the block spans them all.
inline constexpr int kMoveReqWords = 0x1b0;

namespace moveword {
inline constexpr int kKind = 0x00;             // req[0]: move kind 0..7 (cmd[0x16]; 8 maps to 2) (OBS)
inline constexpr int kHeading = 0x01;          // req[1]: heading to the move target, degrees (OBS)
inline constexpr int kFacing = 0x02;           // req[2]: facing, P+0x20 (OBS)
inline constexpr int kLookHeading = 0x03;      // req[3]: heading to the look-at point, cmd+0x20 (OBS)
inline constexpr int kBodyDirection = 0x04;    // req[4]: desired body direction (OBS; INIT reads it)
inline constexpr int kLookDelta = 0x05;        // req[5]: wrap180 difference (OBS)
inline constexpr int kTurn = 0x06;             // req[6]: target minus facing, the turn angle (OBS)
inline constexpr int kFacingError = 0x07;      // req[7]: facing error to the look-at point (OBS)
inline constexpr int kMoveBucket = 0x08;       // req[8]: 10-way angle bucket of the move (OBS)
inline constexpr int kCommandSpeedClass = 0x09;  // req[9]: cmd[0x17] mapped to 0..3, else -1 (OBS)
inline constexpr int kDesiredSpeedKmh = 0x0a;  // req[0xa]: desired speed, km/h, cmd[0x172] (OBS)
inline constexpr int kUseClassRate = 0x0b;     // req[0xb] byte: "use the class rate" flag, cmd+0x5c5 (OBS)
inline constexpr int kLookBucket = 0x0c;       // req[0xc]: bucket of the look-at angle (OBS)
inline constexpr int kLookBucketAlt = 0x0d;    // req[0xd]: second bucket (OBS)
inline constexpr int kPrecise = 0x0e;          // req[0xe]: precise-arrival flag (OBS)
inline constexpr int kDistToTarget = 0x11;     // req[0x11]: distance to the target, metres (OBS)
inline constexpr int kDistToLook = 0x12;       // req[0x12]: distance to the look-at point, metres (OBS)
inline constexpr int kCommandId = 0x16;        // req[0x16]: command id, cmd[0] (OBS)
inline constexpr int kRecordStampA = 0x17;     // req[0x17]: P+0x2770 (OBS copy-in)
inline constexpr int kRecordStampB = 0x18;     // req[0x18]: P+0x2774 (OBS copy-in)
inline constexpr int kRecordStampC = 0x19;     // req[0x19]: P+0x2778 (OBS copy-in)
inline constexpr int kDwellElapsed = 0x1a;     // req[0x1a]: first dwell, seconds elapsed; P+0x277c (OBS copy-in; AS 5.2 grows it 1/54 per tick)
inline constexpr int kDwellLimit = 0x1b;       // req[0x1b]: first dwell threshold, seconds; P+0x2780 (OBS copy-in; AS 5.2)
inline constexpr int kDwellElapsed2 = 0x2d;    // req[0x2d]: second dwell, seconds elapsed; P+0x27d0 (OBS copy-in; AS 5.2)
inline constexpr int kDwellLimit2 = 0x2e;      // req[0x2e]: second dwell threshold, seconds; P+0x27d4 (OBS copy-in; AS 2.3 uses 0.2)
inline constexpr int kMotionPending = 0x2f;    // req[0x2f] byte: a SetMotion/SetParametric request is due (OBS: FUN_1406b8340 tail)
inline constexpr int kMotion = 0x30;           // req[0x30]: representative motion number (OBS)
inline constexpr int kMirror = 0x31;           // req[0x31]: mirror bit (OBS)
inline constexpr int kDuration = 0x32;         // req[0x32]: blend duration, seconds, default 0.2 (OBS)
inline constexpr int kStartTime = 0x33;        // req[0x33]: start time in the clip, seconds (OBS)
inline constexpr int kPlayback = 0x35;         // req[0x35]: playback speed, default 1.0 (OBS)
inline constexpr int kTransitionWord = 0x36;   // req[0x36]: transition record (OBS)
inline constexpr int kParamState = 0x37;       // req[0x37]: 0x11 = a single clip (SetMotion); else the parametric group (OBS)
inline constexpr int kChangeState = 0x38;      // req[0x38] byte: a state change is requested (OBS)
inline constexpr int kNextState = 0x39;        // req[0x39]: the state entered when kChangeState is set (OBS)
inline constexpr int kRate = 0x4a;             // req[0x4a]: parametric rate (OBS; AS 1)
inline constexpr int kPlaybackDefault = 0x4b;  // req[0x4b]: P+0x3750, default 1.0 (OBS copy-in)
inline constexpr int kStartRate = 0x4f;        // req[0x4f]: start phase, the normalised foot phase (AS 1)
inline constexpr int kFootBandSide = 0x7c;     // req[0x7c]: foot band side (OBS: FUN_1406c2f80 reads it; INIT writes 1)
inline constexpr int kRootFacing = 0x3a;       // req[0x3a] byte: a root-facing write is due (OBS: FUN_1406c14c0 rate tail)
inline constexpr int kRootFacingAngle = 0x3b;  // req[0x3b]: the wrapped root-facing angle, degrees (OBS)
inline constexpr int kSlipActive = 0xe8;       // req[0xe8] byte: slip damping is active (OBS: FUN_1406c2f80)
inline constexpr int kSlipAngle = 0xec;        // req[0xec]: the damped facing, degrees (OBS). The adapter copies it to P+0xbec.
inline constexpr int kLiveKind = 0x1e;        // req[0x1e]: the live clip's kind; FUN_1406d5d60 writes it (OBS per MovestepRun)
inline constexpr int kStopBand = 0x51;        // req[0x51] (req+0x144): the band FUN_1406d5d60 reads for its stop-row picker (OBS per MovestepRun)
}  // namespace moveword

// The move request. Typed views over the same words; PES's bit patterns are kept exact.
class MoveReq {
 public:
  float F(int word) const {
    float v;
    std::memcpy(&v, &words_[word], sizeof v);
    return v;
  }
  void SetF(int word, float v) { std::memcpy(&words_[word], &v, sizeof v); }
  uint32_t U(int word) const { return words_[word]; }
  void SetU(int word, uint32_t v) { words_[word] = v; }
  // A PES byte flag: the low byte of the word.
  bool Flag(int word) const { return (words_[word] & 0xffu) != 0; }
  void SetByte(int word, uint8_t v) { words_[word] = (words_[word] & ~0xffu) | v; }

 private:
  std::array<uint32_t, kMoveReqWords> words_{};
};

// A request to a state (FUN_1406ba900 INIT: param_2[0x38] = 1; param_2[0x39] = next). The
// dispatcher applies it after the handler returns (ApplyTransition).
inline void RequestState(MoveReq& req, MoveState next) {
  req.SetByte(moveword::kChangeState, 1);
  req.SetU(moveword::kNextState, uint32_t(next));
}

// One Animetable cell: the motion number and mirror bit (pes::Animetable::Cell, A-AN9).
struct MoveCell {
  int motionNo = -1;
  bool mirror = false;
  bool found = false;  // false: the row has no clip at that column (the populated-walk failed)
};

// What the move machine reads from its player and the match. The sim adapter implements it;
// tests implement it with synthetic values. Each method names the PES read it stands for.
struct AngleBucketConstants;  // defined below, with the shared helpers
struct StepScalars;  // group C's install scalars, defined by its loader in movestep_step.hpp
// Group B's install scalars and tables (RUN_LOOP, GOTO_RUN, STOP, ACCELERATE, DECELERATE and the run
// entry), read by LoadRunConstants / LoadRunTables from the exe at the VAs in movestep_run.cpp.
struct RunConstants {
  float one;          // 1.0   (0x14256ae1c)
  float neg;          // -1.0  (0x142588218)
  float half;         // 0.5   (0x142565600)
  float wrapHalf;     // -180  (0x1425ddffc)
  float full;         // 360   (0x14259858c)
  float wrapFull;     // -360  (0x142598598)
  float half_turn;    // 180   (0x142598580)
  float deg1125;      // 11.25 (0x1425cc680)
  float deg225;       // 22.5  (0x14259d618)
  float deg45;        // 45    (0x14259be30)
  float deg5625;      // 56.25 (0x14259be3c)
  float deg675;       // 67.5  (0x1425cca34)
  float deg7875;      // 78.75 (0x14259be50)
  float deg90;        // 90    (0x14259be5c)
  float deg1125b;     // 112.5 (0x1425cb30c)
  float deg135;       // 135   (0x14259be60)
  float deg1575;      // 157.5 (0x1425cca3c)
  float turn16875;    // 16.875 (0x1425ddf8c)
  float blend1667;    // 1/6   (0x1425ddf50)
  float blend1333;    // 2/15  (0x1425ddf4c)
  float blend2;       // 0.2   (0x142598578)
  float blend3;       // 0.3   (0x142590d98)
  float blend05;      // 0.05  (0x1425a8a28)
  float blend08;      // 0.8   (0x142572348)
  float rate06;       // 0.6   (0x14257a190)
  float rate07;       // 0.7   (0x14257a194)
  float rate095;      // 0.95  (0x1425ab124)
  float rate0225;     // 0.225 (0x1425ddf54)
  float rate01;       // 0.1   (0x1425ddf48)
  float rate05;       // 0.5   (0x142565600)
  float kmh5;         // 5     (0x14259222c)
  float kmh8;         // 8     (0x14259bdf4)
  float kmh11;        // 11    (0x14259bdfc)
  float kmh135;       // 13.5  (0x1425a1ffc)
  float kmh1475;      // 14.75 (0x1425ddf88)
  float kmh16;        // 16    (0x14259be10)
  float kmh19;        // 19    (0x1425c65e8)
  float kmh22;        // 22    (0x1425c48c0)
  float kmh25;        // 25    (0x14259be20)
  float kmh28;        // 28    (0x1425a2004)
  float kmh60;        // 60    (0x14259a764)
  float kmh1000;      // 1000  (0x14256ae34)
  float kmh95sq;      // 8100 = 90^2 (0x1425ddfd8)
  float dist1m5;      // 1.5   (0x14259bddc)
  float dist11;       // 1.1   (0x1425a3590)
  float speed3600;    // 3.6   (0x14259ce38)
  float speed2;       // 2     (0x142565604)
  float speed4;       // 4     (0x1425656c0)
  float ramp15;       // 15    (0x14259be0c)
  float ramp6;        // 6     (0x14259bdec)
  float ramp12;       // 12    (0x14259be04)
  float ramp45;       // 0.45  (0x14256b140)
  float ramp65;       // 0.65  (0x14259d5d0)
  float ramp120;      // 1.2   (0x14259d5d4)
  float ramp85;       // 0.85  (0x1425ab120)
  float blend04;      // 0.4   (0x14259b89c)
  float neg45;        // -45   (0x1425aa1c8)
  float cm100;        // 100   (0x14253ee30)
  float foot035;      // 0.35  (0x14259d5c8): the foot-placement window, FUN_1406d2030
  float footNeg010;   // -0.1  (0x14259d648)
  float footNeg035;   // -0.35 (0x1425ddfec)
  float negHalf;      // -0.5  (0x14258d694)
  float bias5;        // 5     (0x14259be7c): FUN_1406b9b20's hysteresis bias
  float rampFloor;    // 0.001 (0x14259bdc4): FUN_1406eab00's tau floor
  float rampSlope;    // 0.15  (0x1425c48a0): FUN_1406b9aa0's rate floor
  float turnP2;       // 0.1   (0x142588210): the turn playback's p^2 term
  float foxTime;      // 300.0 (0x1425a266c): the Fox time unit of the parametric startTime
  float pct50;        // 50    percent test (FUN_1406ebf30's argument, no exe float)
  float quota75;      // 0x4b = 75, the (t * 0x4b) / 100 share
  float quota80;      // 8, the (t * 8) / 10 share
  float quota3;       // 3, the (a + b * 2) / 3 share
};

// Group B's install tables: the anime interface's tables (the dispatch table at 0x1425e35d0, decoded for
// the port) and the stop, goto and turn-stop row grids (kind + class * 8).
struct RunTables {
  std::array<int32_t, 8> commandClass;    // FUN_1406b8de0, 0x1425ddcd8
  std::array<float, 4> bandRate;          // FUN_1406b9e50, 0x1425dde10
  std::array<float, 4> rateLadder;        // FUN_1406ba030, 0x1425dde20
  std::array<int32_t, 4> rateClass;       // FUN_1406ba030's answers, 0x1425dde00
  std::array<int32_t, 32> stopRow;        // FUN_1406b9d80, kind + class*8, 0x1425ddad0
  std::array<int32_t, 32> gotoRow;        // FUN_1406b9bd0, kind + class*8, 0x1425dda50
  std::array<int32_t, 32> turnStopRow;    // FUN_1406ba0d0, kind + class*8, 0x1425ddb50
  std::array<int32_t, 10> bucketColumn;   // FUN_1406b9b20's columns, 0x1425dd988
  std::array<int32_t, 16> styleGroup;     // FUN_1406d2030's representative motion, DAT_1425ddcf0 (style*4 + band/4)
};

// Group C's install tables (STEP_LOOP, STEP_LOOP_CHANGE, TURN_STOP, the transitions), read by
// LoadStepTables from the exe at the VAs in movestep.cpp.
struct StepTables {
  std::array<int32_t, 10> bucketColumn;  // 0x1425dd988: bucket -> Animetable column
  std::array<int32_t, 64> kindBandRow;  // 0x1425dd9b0: kind + band * 8 -> Animetable row; 8 bands x 8 kinds (verified: 64 rows, 1325..1385)
  std::array<float, 10> bucketDegrees;   // 0x1425dd960: bucket -> degrees (TURN_STOP entry)
  std::array<int32_t, 12> styleMotion;   // 0x1425ddcf0: class + style * 4 -> representative motion
};
std::optional<StepTables> LoadStepTables(const PeImage& exe, std::string* error);

// Group C's install scalars: the .rdata / .data floats the handlers compare against. Each name carries
// its meaning; its VA is in movestep.cpp's table, pinned to the instruction that reads it.
struct StepScalars {
  float one, half, full, wrapFull, halfTurn, negOne;
  float ticks60, scale1000, four, kmh5;
  float kmh8, kmh11, kmh13p5, kmh16, kmh20, kmh22, kmh3p6, kmh28;
  float deg5p625, deg10, deg11p25, deg16p875, deg22p5, deg28p125, deg31p64, deg33p75;
  float deg45, deg67p5, deg78p75, deg90, deg112p5, deg135, deg140p625, deg146p25, deg168p75;
  float deg174p375, deg175, deg270;
  float neg45, neg67p5, neg90, neg112p5, neg180;
  float dist1p25, dist1p5, dist3, eps1e7, threeQuarters, rate1p6;
  float rate0p9, rate0p95, clamp0p8, six;
  float s0p1333, s0p15, s0p1667, s0p2, s0p25, s0p3, s0p35, s0p4, s0p6, s0p85;
  float sq1139, sq2025, sq8100, step0p05;
};
std::optional<StepScalars> LoadStepScalars(const PeImage& exe, std::string* error);


// A motion request for the anime interface (PES's vt+0x10: SetMotion when group < 0, else
// SetParametric). The fields are PES's arguments in the order of its call sites (FUN_1406b8340's
// tail, and the dispatcher's INIT call). A handler fills one and calls MoveHost::Issue.
struct MotionRequest {
  int motion = -1;         // req[0x30]: the motion number
  bool mirror = false;     // req[0x31]
  int transition = 0;      // req[0x36]: the transition record
  float duration = 0.0f;   // req[0x32], seconds
  float playback = 1.0f;   // req[0x35]
  float startTime = 0.0f;  // req[0x33], seconds
  int group = -1;          // req[0x37]: -1 = SetMotion (req[0x37] == 0x11), else the parametric group
  float startRate = 0.0f;  // req[0x4f]: SetParametric only
  float rate = 0.0f;       // req[0x4a]: SetParametric only
  int foot = 0;            // SetParametric's foot argument (not req[0x7c])
};

class MoveHost {
 public:
  virtual ~MoveHost() = default;

  // Animetable cell (row, column). FUN_1406abb30 / FUN_1406abdd0 on the anime table manager
  // (FUN_1406abba0). walkFlag is FUN_1406abb30's last argument (the populated-neighbour walk).
  virtual MoveCell Cell(int row, int column, bool walkFlag) = 0;

  // The player's own generator (P+0x32d0, slot 2). PickN is FUN_1406e6bf0(P, n);
  // Percent is FUN_1406ebf30(P, pct), true when the draw is below pct.
  virtual int PickN(int n) = 0;
  virtual bool Percent(int pct) = 0;

  // The anime player's values (FUN_1406ba900 reads them through the interface): the command
  // speed class (vt+0x98, P+0x245a), the rate ceiling (P+0x377c), and the speeds in km/h
  // (vt+0x20 index 0 = the command's desired speed; index 1 = the current speed, INF).
  virtual int CommandSpeedClass() const = 0;
  virtual float RateCeiling() const = 0;
  virtual float CommandSpeedKmh() const = 0;
  virtual float CurrentSpeedKmh() const = 0;

  // The physique scale G (FUN_141af3180 of P+0xa08), the logic clock (FUN_141e532d0 = 54 Hz)
  // and the match tick (MI+0x1198, the stamp of FUN_1406f99f0's guard).
  virtual float PhysiqueG() const = 0;
  virtual float TicksPerSecond() const = 0;
  virtual int MatchTick() const = 0;

  // The per-player recent-tick stamps P+0x2814 (index 0) and P+0x2816 (index 1): INIT's
  // duration override (AS 2.3).
  virtual int RecentStamp(int index) const = 0;

  // The start time of a motion in its clip (FUN_1406b97d0: a per-motion constant).
  virtual float MotionStartTime(int motionNo) const = 0;

  // P+0x2468 (float): FUN_1406eb670 sets it to 1.0; FUN_1406b8340 stores it from RBP+0x44. Group D's
  // five handlers floor the clip-length divisor it feeds at DAT_1425ab120 (0.85).
  virtual float RecordF2468() const = 0;
  // P+0x245d: the state before the current one (FUN_1406b8340's tail writes it).
  virtual int MoveStatePrevious() const = 0;
  // P+0xbec (float): two PES writers. The yaw helpers FUN_1406cbb00 and FUN_1406cba60 write it
  // through this host; the slip damping FUN_1406c2f80 writes req[0xec], which the adapter copies
  // to P+0xbec after Dispatch.
  virtual float RecordF0bec() const = 0;
  virtual void SetRecordF0bec(float value) = 0;
  // The angle-bucket install tables of FUN_1406b9b20 (see AngleBucketConstants).
  virtual const AngleBucketConstants& AngleBuckets() const = 0;
  // Group C's install scalars (STEP_LOOP, STEP_LOOP_CHANGE, TURN_STOP and the transitions), read from
  // the exe by VA by the group's loader (movestep_step.hpp).
  virtual const StepScalars& StepConstants() const = 0;
  // The band's natural speed (km/h) of the anime info record, times 3.6 into req[0x1d] (the parametric
  // member's speed). PES reads the record region of the anime interface (the table at 0x1425e35d0,
  // slots 0x1cb4..0x1d0c; its writer is not decoded). ponytail: the adapter supplies the GF stand-in
  // until that writer is ported.
  virtual float BandSpeedKmh(int band, float g) const = 0;
  // FUN_140a62960's answer for this player's record: the mirror bit for the side. ponytail: the adapter
  // supplies the GF stand-in until the record writer is ported.
  virtual bool MirrorChoice(bool sameTeam) const = 0;
  // The foot / start band of the record, P+0x245b: the action setter FUN_1406eebb0 stores it from
  // anime vt+0x78. It is the foot argument of the run entry (FUN_1406d2030) and of SetParametric.
  virtual int FootBand() const = 0;
  // Group B's install views (RUN_LOOP, GOTO_RUN, STOP, ACCELERATE, DECELERATE and the run entry), read
  // from the exe by VA by the group's loader (movestep_run.hpp).
  virtual const RunConstants& RunConstantsView() const = 0;
  virtual const RunTables& RunTablesView() const = 0;
  // Group C's install tables (StepTables, the scalars are StepConstants above). The view is named
  // StepTablesView because StepTables is the type.
  virtual const StepTables& StepTablesView() const = 0;
  // (&DAT_1425dd950)[bucket]: whether the bucket's side is the left one (AS 2.2).
  virtual bool BucketIsLeft(int bucket) const = 0;
  // The playback constants (FUN_1406ba070 / FUN_1406e9f10 and the run guard's 0.95), read from the exe.
  // Named PlaybackConstantsView because PlaybackConstants is the type.
  virtual const PlaybackConstants& PlaybackConstantsView() const = 0;
  // The +0xab8 anime interface: the current motion number (vt+0x18, PES's motion id).
  virtual int CurrentMotion() const = 0;
  // The current clip's frame count (vt+0x38 of the +0xab8 interface).
  virtual int CurrentClipFrames() const = 0;
  // The anime state (vt+0x28 of the +0xab8 interface). IsReplay() is AnimeState() == 0x4a.
  virtual int AnimeState() const = 0;

  // FUN_141eb50b0(P, out, index): index 0 or 1 is the last-tick displacement (pos - prev, per axis).
  // index >= 2 reads the position history ring at P+0x304 (its writer is not decoded yet).
  virtual void PositionAt(int index, float out[3]) const = 0;
  // MI+0x116c: the match phase (MB-E is not ported; a stand-in until it is).
  virtual int MatchPhase() const = 0;
  // The anime predicate vt+0x28 == 0x4a: the replay action.
  virtual bool IsReplay() const = 0;
  // SetMotion / SetParametric: the anime interface's vt+0x10 (FUN_1406b8340's tail; the INIT call).
  virtual void Issue(const MotionRequest& request) = 0;
};

// ---------------------------------------------------------------------------------------------
// Dispatch. FUN_1406ba900 runs one tick of the current state's handler; the handler writes its
// transition into req. FUN_1406b8340's tail then applies it.
// ---------------------------------------------------------------------------------------------
void Dispatch(MoveState state, MoveHost& host, MoveReq& req);  // the switch: case k calls its handler
void ApplyTransition(MoveStepRecord& rec, MoveReq& req);       // req[0x38] set -> rec.Enter(req[0x39])
// FUN_1406d2030's run guard: a run may start only while the rate is at most stepBase (0.95,
// DAT_1425ab124, the playback floor) times the rate ceiling (P+0x377c, rateMax). Every entry
// that starts a run calls this one guard; no state keeps its own copy.
bool RunRateAllowed(float rate, float rateCeiling, float stepBase);  // stepBase: the install 0.95 (DAT_1425ab124)

// ---------------------------------------------------------------------------------------------
// A-AN6: the action's speed class (P+0x245a), stored by the action setter FUN_1406eebb0 from
// FUN_14081e340: the command's desired speed times the cosine of the turn, against four bounds.
// FUN_140a92c80 is the angle helper: s * sin(x), a 512-step table of sin over 0..90 degrees with a
// mirror fold. The scalars and the table are read from the exe at runtime.
// ---------------------------------------------------------------------------------------------
struct SpeedClassConstants {
  float full;      // 360 degrees (0x14259858c): the turn wraps modulo this
  float wrapFull;  // -360 (0x142598598): FUN_140a92c80's first fold
  float half;      // 180 (0x142598580)
  float wrapHalf;  // -180 (0x1425ddffc): FUN_140a92c80's second fold
  float neg;       // -1 (0x142588218): the sign of the second half turn
  float one;       // 1 (0x14256ae1c)
  float quarter;   // 90 (0x14259be5c): the table covers 0..90 degrees
  float tabScale;  // 512 (0x1426550e8): table steps per quarter turn
  float lo0;       // 3.5 km/h (0x14259bde8): class 0 at or below
  float lo1;       // 8 km/h (0x14259bdf4): class 1 at or below
  float hi0;       // 13.5 km/h (0x1425a1ffc): class 2 at or below
  float hi1;       // 22 km/h (0x1425c48c0): class 3 at or below, else 4
  std::array<float, 513> sinQuarter;  // FUN_140a92c80's table at 0x142655950
};
std::optional<SpeedClassConstants> LoadSpeedClassConstants(const PeImage& exe, std::string* error);
// FUN_140a92c80: s * sin(x) with x in degrees, the table linearly interpolated.
float TableSinDeg(float x, float s, const SpeedClassConstants& k);
// The fold of FUN_14081e340 and FUN_1406e4230: an angle difference wrapped into [-180, 180].
float WrapHalfTurn(float deltaDeg, const SpeedClassConstants& k);
// FUN_14081e340's tail: the class 0..4 of a desired speed (km/h) at a turn (heading minus facing, degrees).
int SpeedClassOf(float desiredKmh, float deltaDeg, const SpeedClassConstants& k);

// FUN_1406b9b20's install tables: the window edges of the wide and the narrow variant (11 each),
// the window -> bucket map (10 entries), and the hysteresis of the current bucket: +5 widens its
// upper edge, -5 widens its lower edge and shrinks the window below it at its top. Read from the
// exe at runtime by VA.
struct AngleBucketConstants {
  std::array<float, 11> wide;      // 0x1425ddeb0: -180 .. 180 with the ±22.5 / ±157.5 windows
  std::array<float, 11> narrow;    // 0x1425ddee0: the variant with ±10 and ±170
  std::array<int32_t, 10> bucket;  // 0x1425ddf10: window -> bucket
  float hysteresisUp;              // 0x14259222c: +5 degrees
  float hysteresisDown;            // 0x14259be7c: -5 degrees
};
std::optional<AngleBucketConstants> LoadAngleBucketConstants(const PeImage& exe, std::string* error);

// Per-state handlers. Each takes the host and the request, returns nothing, and never touches
// MoveStepRecord: a state change is RequestState(req, next). The VA of each PES handler is in
// the comment; the owning group is in the file header of its .cpp.
namespace movestep {
// Group A (INIT stays in the dispatcher)
void Idle(MoveHost& host, MoveReq& req);               // 1  FUN_1406bf0c0
void IdleTurn(MoveHost& host, MoveReq& req);           // 2  FUN_1406bfe70
void IdleOther(MoveHost& host, MoveReq& req);          // 17 FUN_1406bf7f0
void IdlePkPlayer(MoveHost& host, MoveReq& req);       // 20 FUN_1406bfd10
void IdlePkGk(MoveHost& host, MoveReq& req);           // 21 FUN_1406bfb70
void TimingStep(MoveHost& host, MoveReq& req);         // 22 FUN_1406ca6b0
void Demo(MoveHost& host, MoveReq& req);               // 23 FUN_1406bdd70

// Group B
void RunLoop(MoveHost& host, MoveReq& req);            // 3  FUN_1406c0170
void GotoRun(MoveHost& host, MoveReq& req);            // 7  FUN_1406bdf10
void Stop(MoveHost& host, MoveReq& req);               // 8  FUN_1406c9e20
void Accelerate(MoveHost& host, MoveReq& req);         // 9  FUN_1406bb570
void Decelerate(MoveHost& host, MoveReq& req);         // 10 FUN_1406bcd20

// Group C
void StepLoop(MoveHost& host, MoveReq& req);           // 4  FUN_1406c4c10
void StepLoopChange(MoveHost& host, MoveReq& req);     // 5  FUN_1406c6480
void TurnStop(MoveHost& host, MoveReq& req);           // 6  FUN_1406cab20
void StepToRun(MoveHost& host, MoveReq& req);          // 13 FUN_1406c6f80
void RunToStep(MoveHost& host, MoveReq& req);          // 14 FUN_1406c1070

// Group D
void RunTurnRun(MoveHost& host, MoveReq& req);         // 12 FUN_1406c14c0
void RunTurnRunSlipdown(MoveHost& host, MoveReq& req); // 18 FUN_1406c2f80
void RunTurnStep(MoveHost& host, MoveReq& req);        // 16 FUN_1406c3470
void StepTurnRun(MoveHost& host, MoveReq& req);        // 15 FUN_1406c7400
void StepTurnStep(MoveHost& host, MoveReq& req);       // 19 FUN_1406c8d80

// Entries: the code that sets a state's request when another state (or INIT) moves into it.
// The run entry FUN_1406d2030 holds the run-rate guard (RunRateAllowed, below).
// FUN_1406d2030 (group B). PES takes two more arguments, (int foot, byte flag), which the caller passes;
// the port reads the foot from FootBand() and names the flag when group B decodes it.
void EnterRun(MoveHost& host, MoveReq& req, int foot, bool rateHeld);
// FUN_1406ce820 (group B). PES takes one more argument, a byte flag (char) that the caller passes; group B
// names it when it decodes the caller.
void EnterGotoRun(MoveHost& host, MoveReq& req, bool runStopVariant);
void EnterStop(MoveHost& host, MoveReq& req);          // FUN_1406d5d60 (group B)
// FUN_1406ccd00 (group B): returns whether it wrote the request (undefined8 in PES; 1 on success, 0 on each refusal).
bool EnterAccelerate(MoveHost& host, MoveReq& req);
// FUN_1406cdaf0 (group B): returns whether it wrote the request (undefined8 in PES).
bool EnterDecelerate(MoveHost& host, MoveReq& req);
void EnterRunTurnRun(MoveHost& host, MoveReq& req);    // FUN_1406d6780 (group D)
// FUN_1406d2f60 (group D): the entry takes a float threshold and returns whether it wrote the
// request. true = the request is written and RUN_TURN_STEP is entered (RequestState); false = the
// calling state keeps the tick. Callers: RUN_TURN_STEP twice (threshold 0), RUN_LOOP (rateMax term),
// INIT (computed from the rate ceiling and the turn factor).
bool EnterRunTurnStep(MoveHost& host, MoveReq& req, float threshold);
// FUN_1406d38d0 (group D): returns whether it wrote the request (undefined8 in PES).
bool EnterStepTurnRun(MoveHost& host, MoveReq& req);

// Shared helpers, defined in movestep_common.cpp (owned by the dispatcher port). Arguments
// follow the PES call order; PES's player pointer (the anime interface) is the MoveHost.
// FUN_1406b93a0(ap, req): the idle row id for the request (AS 2.4).
int IdleRow(MoveHost& host, MoveReq& req);
// FUN_1406b6bb0(ap, req): INIT's stay decision (AS 2.3); true = stay idle.
bool StayDecision(MoveHost& host, MoveReq& req);
// FUN_1406b97d0(motion): a motion's start time in its clip, seconds (AS 2.3, a per-motion constant).
float MotionStartSeconds(uint16_t motionNo);
// FUN_1406b9e80(interface, speed km/h, table selector): the playback rate for a speed (AS 3.2).
// The third argument selects the table row; its meaning is INF until the port verifies it.
float SpeedToRate(MoveHost& host, float speedKmh, int tableSelector);
// FUN_1406ba2f0(ap, command id, requested rate, req): the locomotion style group (AS 3.3).
int LocomotionStyle(MoveHost& host, int commandId, float rateRequested, MoveReq& req);
// FUN_1406cbf70(ap, req): the acceleration ramp of the rate, per tick in RUN_LOOP and STEP_TO_RUN (AS 3.6).
void AccelerationRamp(MoveHost& host, MoveReq& req);
// FUN_1406cbc10(ap, req): the lateral drift damping, run_2_4_000 and rate >= 0.3 (AS 1, step 5).
void LateralDamping(MoveHost& host, MoveReq& req);
// FUN_1406ce550(ap, req): the out-of-play pre-empt (AS 2.1). true = the tick was taken by dm_oop.
bool OutOfPlayPreempt(MoveHost& host, MoveReq& req);
// FUN_1406b9b20: the 10-way angle bucket of an angle in degrees, with the hysteresis of the current
// bucket (10 = no current bucket). narrow selects the ±10 / ±170 windows.
int AngleBucket(const AngleBucketConstants& k, float angleDeg, int currentBucket, bool narrow);
}  // namespace movestep

}  // namespace pes
