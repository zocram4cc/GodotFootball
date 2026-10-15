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
inline constexpr int kMoveReqWords = 0x51;

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
bool RunRateAllowed(float rate, float rateCeiling, const PlaybackConstants& k);

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
void EnterRun(MoveHost& host, MoveReq& req);           // FUN_1406d2030 (group B)
void EnterGotoRun(MoveHost& host, MoveReq& req);       // FUN_1406ce820 (group B)
void EnterStop(MoveHost& host, MoveReq& req);          // FUN_1406d5d60 (group B)
void EnterAccelerate(MoveHost& host, MoveReq& req);    // FUN_1406ccd00 (group B)
void EnterDecelerate(MoveHost& host, MoveReq& req);    // FUN_1406cdaf0 (group B)
void EnterRunTurnRun(MoveHost& host, MoveReq& req);    // FUN_1406d6780 (group D)
void EnterRunTurnStep(MoveHost& host, MoveReq& req);   // FUN_1406d2f60 (group D)
void EnterStepTurnRun(MoveHost& host, MoveReq& req);   // FUN_1406d38d0 (group D)

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
// FUN_1406b9b20(_, angle, buckets, narrow): the 10-way angle bucket (AS 2.2); narrow = the ±10 / ±170 variant.
int AngleBucket(float angleDeg, int buckets, bool narrow);
}  // namespace movestep

}  // namespace pes
