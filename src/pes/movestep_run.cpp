// PES 2021's MOVESTEP handler group B (contract v0.5): the running phase. Each function is one
// PES function, ported from its decompile in order; the PES VA names the source. PES is king:
// a branch, a comparison and a store appear here exactly as they appear there. A GF stand-in
// for a PES input GF lacks is marked with a ponytail comment naming the site that removes it.
// Constants come from the install (LoadRunConstants); tests pass synthetic ones.
//
// The +0xac0 anime interface's function set, decoded for this port from the pointer table at
// 0x1425e35d0 (28 slots; the object's own vtable is runtime-assigned, so the table is the
// evidence, not the object). The slots the handlers call, by their PES function:
//   FUN_1406ea110  ground speed km/h                 MoveHost::CurrentSpeedKmh()
//   FUN_1406eab00  the acceleration ramp             pes::movestep::AccelerationRamp
//   FUN_1406e9f10  the turn playback                 pes::TurnPlayback (AS 4.1)
//   FUN_1406b9b20  the angle bucket                  pes::movestep::AngleBucket
//   FUN_1406b8de0  cmd -> class                      RunTables::CommandClass
//   FUN_1406b9e50  (band, kind) -> rate              RunTables::BandRate
//   FUN_1406ba030  rate -> class                     RunTables::ClassForRate
//   FUN_1406b9d80  the stop-row picker               RunTables::StopRow
//   FUN_1406b9bd0  the goto-row picker               RunTables::GotoRow
//   FUN_1406ba0d0  the turn-stop row picker          RunTables::TurnStopRow
//   FUN_1406ea750  the engine blend rate             the live engine rate (INF, AS 7)
#include "pes/movestep_run.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <string>

#include "pes/exe.hpp"
#include "pes/movestep.hpp"

namespace pes {
namespace {

// The install floats this group reads, by VA. The install test checks their relations; the
// synthetic tests exercise the formulas over synthetic values.
struct RunScalar {
  uint64_t va;
  float pes::RunConstants::* field;
};
constexpr RunScalar RUN_SCALARS[] = {
    {0x14256ae1c, &RunConstants::one},
    {0x142588218, &RunConstants::neg},
    {0x142565600, &RunConstants::half},
    {0x1425ddffc, &RunConstants::wrapHalf},
    {0x14259858c, &RunConstants::full},
    {0x142598598, &RunConstants::wrapFull},
    {0x142598580, &RunConstants::half_turn},
    {0x1425cc680, &RunConstants::deg1125},
    {0x14259d618, &RunConstants::deg225},
    {0x14259be30, &RunConstants::deg45},
    {0x14259be3c, &RunConstants::deg5625},
    {0x1425cca34, &RunConstants::deg675},
    {0x14259be50, &RunConstants::deg7875},
    {0x14259be5c, &RunConstants::deg90},
    {0x1425cb30c, &RunConstants::deg1125b},
    {0x14259be60, &RunConstants::deg135},
    {0x1425cca3c, &RunConstants::deg1575},
    {0x1425cc688, &RunConstants::deg3375},
    {0x1425ddf8c, &RunConstants::turn16875},
    {0x1425ddf50, &RunConstants::blend1667},
    {0x1425ddf4c, &RunConstants::blend1333},
    {0x142598578, &RunConstants::blend2},
    {0x142590d98, &RunConstants::blend3},
    {0x1425a8a28, &RunConstants::blend05},
    {0x142572348, &RunConstants::blend08},
    {0x14257a190, &RunConstants::rate06},
    {0x14257a194, &RunConstants::rate07},
    {0x1425ab124, &RunConstants::rate095},
    {0x1425ddf54, &RunConstants::rate0225},
    {0x1425ddf48, &RunConstants::rate01},
    {0x14259222c, &RunConstants::kmh5},
    {0x14259bdf4, &RunConstants::kmh8},
    {0x14259bdfc, &RunConstants::kmh11},
    {0x1425a1ffc, &RunConstants::kmh135},
    {0x1425ddf88, &RunConstants::kmh1475},
    {0x14259be10, &RunConstants::kmh16},
    {0x1425c65e8, &RunConstants::kmh19},
    {0x1425c48c0, &RunConstants::kmh22},
    {0x14259be20, &RunConstants::kmh25},
    {0x1425a2004, &RunConstants::kmh28},
    {0x14259a764, &RunConstants::kmh60},
    {0x14256ae34, &RunConstants::kmh1000},
    {0x1425ddfd8, &RunConstants::kmh95sq},
    {0x14259bddc, &RunConstants::dist1m5},
    {0x1425a3590, &RunConstants::dist11},
    {0x14259ce38, &RunConstants::speed3600},
    {0x142565604, &RunConstants::speed2},
    {0x1425656c0, &RunConstants::speed4},
    {0x14259be0c, &RunConstants::ramp15},
    {0x14259bdec, &RunConstants::ramp6},
    {0x14259be04, &RunConstants::ramp12},
    {0x14256b140, &RunConstants::ramp45},
    {0x14259d5d0, &RunConstants::ramp65},
    {0x14259d5d4, &RunConstants::ramp120},
    {0x1425ab120, &RunConstants::ramp85},
    {0x14259b89c, &RunConstants::blend04},
    {0x1425aa1c8, &RunConstants::neg45},
    {0x14253ee30, &RunConstants::cm100},
    {0x14259d5c8, &RunConstants::foot035},
    {0x14259d648, &RunConstants::footNeg010},
    {0x1425ddfec, &RunConstants::footNeg035},
    {0x14258d694, &RunConstants::negHalf},
    {0x14259be7c, &RunConstants::bias5},
    {0x14259bdc4, &RunConstants::rampFloor},
    {0x1425c48a0, &RunConstants::rampSlope},
    {0x142588210, &RunConstants::turnP2},
    {0x1425a266c, &RunConstants::foxTime},
};

// The install tables the interface functions read. The port reads them by VA once per call,
// the same way LoadPlaybackConstants does; the synthetic tests fill them in directly.
constexpr uint64_t COMMAND_CLASS_VA = 0x1425ddcd8;   // FUN_1406b8de0, 8 words
constexpr uint64_t BAND_RATE_VA = 0x1425dde10;       // FUN_1406b9e50, 4 floats
constexpr uint64_t RATE_LADDER_VA = 0x1425dde20;     // FUN_1406ba030, 4 floats
constexpr uint64_t RATE_CLASS_VA = 0x1425dde00;      // FUN_1406ba030's answers, 4 words
constexpr uint64_t STOP_ROW_VA = 0x1425ddad0;        // FUN_1406b9d80, kind*8+class, 32 words
constexpr uint64_t GOTO_ROW_VA = 0x1425dda50;        // FUN_1406b9bd0, kind*8+class, 32 words
constexpr uint64_t TURNSTOP_ROW_VA = 0x1425ddb50;    // FUN_1406ba0d0, kind*8+class, 32 words
constexpr uint64_t BUCKET_COLUMN_VA = 0x1425dd988;   // FUN_1406b9b20's column table, 10 words
constexpr uint64_t STYLE_GROUP_VA = 0x1425ddcf0;     // FUN_1406d2030, style*4 + band/4, 16 words

// FUN_1406d5d60's three dwell immediates. They are instruction operands (movss off an immediate
// stored on the stack), not .rdata floats, so they have no VA; the values are the ones the
// decompile shows at each store: 0x3cf5c28f, 0x3d4ccccd and 14 ticks over the 54 Hz clock.
constexpr float STOP_DWELL = 0.05f;       // FUN_1406d5d60, the stop entry's dwell
constexpr float SLOW_DWELL = 0.03f;        // FUN_1406d5d60, the slow-stop dwell
constexpr float SECOND_DWELL = 14.0f / 60.0f;  // FUN_1406d5d60, the second dwell in seconds


// ---------------------------------------------------------------------------------------------
// The interface functions, ported from the pointer table at 0x1425e35d0.
// ---------------------------------------------------------------------------------------------


// FUN_14043a0a0(a, b): the angle difference a - b wrapped to (-180, 180].
float AngleDiffDeg(float a, float b, const RunConstants& k) {
  float d = a - b;
  if (d > 0.0f) {
    if (d >= k.full) d += k.wrapFull;
  } else {
    d += k.full;
  }
  if (k.half_turn < d) {
    const float sign = d < 0.0f ? k.neg : k.one;
    d -= sign * k.full;
  }
  return d;
}

// The 360 fold of a heading (the *(float *)(param_1 + 0x20) block every handler opens with).
float FoldFull(float a, const RunConstants& k) {
  if (a >= 0.0f) {
    if (k.full <= a) return a + k.wrapFull;
    return a;
  }
  return a + k.full;
}

// FUN_1406b9ff0(ap, useClassRate, classIdx, speed, kind): the requested rate. The class-rate
// path is FUN_1406b9e50's table; the speed path is the shared FUN_1406b9e80 (AS 3.2).
float RequestedRate(MoveHost& host, MoveReq& req, const pes::RunTables& t) {
  const bool useClass = req.Flag(moveword::kUseClassRate);
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int kind = static_cast<int>(req.U(moveword::kKind));
  if (useClass) return BandRateFor(cls, kind, t);
  if (cls == 0 && (kind == 3 || kind == 6)) return 0.0f;
  return pes::movestep::SpeedToRate(host, req.F(moveword::kDesiredSpeedKmh), 0);
}

// The clamp PES writes as a pair of unsigned compares (the "band 0..3" idiom).
int Clamp03(int v) { return v < 0 ? 0 : (v > 3 ? 3 : v); }
// FUN_1406d2030's first block: the speed clamped into [5, 28] km/h, PES's compare order.
float ClampSpeedKmh(float v, const RunConstants& k) {
  if (v <= k.kmh28 && k.kmh5 <= v) return v;
  return v <= k.kmh28 ? k.kmh5 : k.kmh28;
}

}  // namespace

// FUN_140a92950(a, b): the pair's first angle; PES's desired-direction helper passes the facing
// and the look heading and uses the first.
float FirstAngle(float a, float /*b*/, const RunConstants& /*k*/) { return a; }

// FUN_1406afbc0(motion, 5): the clip's hit-record count. GF has no hit records yet, so the count
// is zero and the handlers' hit loops stay empty exactly as they do for a clip without hit
// records. ponytail: the AnimePlayer hit records (A-AN13) supply this.
int HitRecordCount() { return 0; }

// The group-C helpers' stand-ins, defined here because group C's signatures carry its own
// TurnDistances and the shared contract names neither. Both return PES's "no" answer, so the
// arms that call them take their no-branch until group C lands; the port keeps the call in
// place so the wiring is one edit. ponytail: group C's movestep_step.cpp defines the real
// bodies; delete these two and call its pair directly.
bool StepLetsRunTurnPlaceholder(MoveHost& /*host*/) { return false; }
bool StepStopsPlaceholder(MoveHost& /*host*/, MoveReq& /*req*/) { return false; }


// FUN_1406b8de0(_, command): the command's speed class from table UNK_1425ddcd8.
int CommandClassFor(int command, const pes::RunTables& t) {
  if (static_cast<uint32_t>(command - 1) < 5u) return t.commandClass[command];
  return -1;
}

// FUN_1406b9e50(_, band, kind): the band's rate from table UNK_1425dde10; band 0 with kind 3
// or 6 has no rate (AS 3.1).
float BandRateFor(int band, int kind, const pes::RunTables& t) {
  if (band == 0 && (kind == 3 || kind == 6)) return 0.0f;
  if (band < 0) return t.bandRate[0];
  if (band > 3) return t.bandRate[3];
  return t.bandRate[band];
}

// FUN_1406ba030(_, rate): the first ladder entry the rate is below, else the last class.
int ClassForRateFor(float rate, const pes::RunTables& t) {
  for (int i = 0; i < 4; ++i) {
    if (rate < t.rateLadder[i]) return t.rateClass[i];
  }
  return t.rateClass[3];
}

// FUN_1406b9bd0's table, which the GOTO_RUN entry itself writes inline at d_1406ce820.c:321:
// `DAT_1425dda50 + ((longlong)(int)*param_2 + (longlong)(int)param_2[9] * 8) * 4` — the request's
// kind (word 0) plus its class (word 9) times 8. The picker and the inline site agree, so the
// port uses that one order for both.
// FUN_1406b9bd0's table, which the GOTO_RUN entry itself writes inline at d_1406ce820.c:321:
// `DAT_1425dda50 + ((longlong)(int)*param_2 + (longlong)(int)param_2[9] * 8) * 4` - the request's
// kind (word 0) plus its class (word 9) times 8. The picker and the inline site agree, so the
// port uses that one order for both.
int GotoRowFor(int kind, int cls, const pes::RunTables& t) {
  const int k = kind < 0 ? 0 : kind;
  const int c = cls < 0 ? 0 : (cls > 3 ? 3 : cls);
  return t.gotoRow[k + c * 8];
}

// FUN_1406b9d80, the stop-row picker (0x1425ddad0). Both call sites settle the mapping: the
// class is arg3 and the kind is arg4, and the row index is *arg4 + *arg3 * 8, i.e.
// kind + class * 8 (ASM of 0x1406d5d60: R8 = &clamped class, R9 = &the-kind, so
// `UNK_1425ddad0 + ((longlong)*param_4 + (longlong)*param_3 * 8) * 4` = kind + class*8).
// arg6 (the selector) is the immediate 0 at both call sites, so the override's guard
// `iVar2 != 1` holds and the override fires for the class-2 family.
int StopRowFor(int kind, int cls, const pes::RunTables& t) {
  const int k = kind < 0 ? 0 : kind;
  const int c = cls < 0 ? 0 : (cls > 2 ? 2 : cls);
  int row = t.stopRow[k + c * 8];
  if (row != 0x582 && c == 2) row = 0x582;
  return row;
}

// FUN_1406ba0d0's table (the turn-stop rows), same kind + class*8 order as the other two
// pickers (the group C/D call sites use the identical shape; see GotoRowFor's evidence).
int TurnStopRowFor(int kind, int cls, const pes::RunTables& t) {
  const int k = kind < 0 ? 0 : kind;
  const int c = cls < 0 ? 0 : (cls > 3 ? 3 : cls);
  return t.turnStopRow[k + c * 8];
}

std::optional<RunConstants> LoadRunConstants(const PeImage& exe, std::string* error) {
  RunConstants k{};
  for (const RunScalar& s : RUN_SCALARS) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*(s.field))))) {
      *error = "run constants: .rdata unreadable";
      return std::nullopt;
    }
  }
  return k;
}

std::optional<RunTables> LoadRunTables(const PeImage& exe, std::string* error) {
  RunTables t{};
  if (!exe.ReadVa(COMMAND_CLASS_VA, sizeof t.commandClass, reinterpret_cast<uint8_t*>(t.commandClass.data())) ||
      !exe.ReadVa(BAND_RATE_VA, sizeof t.bandRate, reinterpret_cast<uint8_t*>(t.bandRate.data())) ||
      !exe.ReadVa(RATE_LADDER_VA, sizeof t.rateLadder, reinterpret_cast<uint8_t*>(t.rateLadder.data())) ||
      !exe.ReadVa(RATE_CLASS_VA, sizeof t.rateClass, reinterpret_cast<uint8_t*>(t.rateClass.data())) ||
      !exe.ReadVa(STOP_ROW_VA, sizeof t.stopRow, reinterpret_cast<uint8_t*>(t.stopRow.data())) ||
      !exe.ReadVa(GOTO_ROW_VA, sizeof t.gotoRow, reinterpret_cast<uint8_t*>(t.gotoRow.data())) ||
      !exe.ReadVa(TURNSTOP_ROW_VA, sizeof t.turnStopRow, reinterpret_cast<uint8_t*>(t.turnStopRow.data())) ||
      !exe.ReadVa(BUCKET_COLUMN_VA, sizeof t.bucketColumn, reinterpret_cast<uint8_t*>(t.bucketColumn.data())) ||
      !exe.ReadVa(STYLE_GROUP_VA, sizeof t.styleGroup, reinterpret_cast<uint8_t*>(t.styleGroup.data()))) {
    *error = "run tables: .rdata unreadable";
    return std::nullopt;
  }
  return t;
}

// FUN_1406d01d0(ap, req): the turn-lock test, true when the request's words are already locked.
// Its decompile (gf-MovestepRun-scratch/d_1406d01d0.c) reads the look bucket and the facing error
// against the 45/112.5/135 gates and writes the same words back; the port keeps the decision.
bool TurnLockEligible(MoveHost& host, MoveReq& req) {
  // The class of -1 is normalised to 1 (the decompile's first block).
  if (req.U(moveword::kCommandSpeedClass) == 0xffffffffu) {
    req.SetU(moveword::kCommandSpeedClass, 1u);
  }
  if (req.U(moveword::kPrecise) != 0u) return false;
  const int motion = host.CurrentMotion();
  if (motion < 0x8ac || motion == 0x8af || motion > 0x8b1) return false;
  return true;
}

namespace movestep {

// ---------------------------------------------------------------------------------------------
// FUN_1406d2030, the run entry (d_1406d2030.c). Every group that starts a run calls it. Its
// decompile's param_1 is the +0xac0 interface pointer, so param_1[-1] is the anime-player
// interface at +0xab8 and param_1[-0x158] is the player itself; the port reads the player
// fields through the host and req words.
// ---------------------------------------------------------------------------------------------
void EnterRun(MoveHost& host, MoveReq& req, int footIn, bool rateHeld) {
  const pes::RunConstants& k = host.RunConstantsView();
  const pes::RunTables& t = host.RunTablesView();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  // FUN_1406d2030's param_3: the foot band the caller passes (AS 2.3: INIT passes P+0x245b, the
  // record's band). The port takes it as the argument, as PES does.
  const int foot = footIn;

  // The class clamps to 0..3 and the speed to [5, 28] km/h (the first block).
  int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  cls = cls < 0 ? 0 : (cls > 3 ? 3 : cls);
  req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(cls));
  float speed = req.F(moveword::kDesiredSpeedKmh);
  speed = (speed <= k.kmh28 && k.kmh5 <= speed) ? speed : (speed <= k.kmh28 ? k.kmh5 : k.kmh28);
  req.SetF(moveword::kDesiredSpeedKmh, speed);

  // The rate: the class rate when the flag is set, else the speed table (FUN_1406b9ff0's
  // shape), then the ceiling guard and the [0, rate] clamp.
  float rate = 0.0f;
  if (req.Flag(moveword::kUseClassRate)) {
    rate = BandRateFor(cls, kind, t);
  } else if (cls == 0 && (kind == 3 || kind == 6)) {
    rate = 0.0f;
  } else {
    rate = SpeedToRate(host, speed, 0);
  }
  // FUN_1406d2030's guard: the shared RunRateAllowed is the compare, and PES stores the
  // requested rate only when it passes and is non-negative, else 0.
  const float ceiling = host.RateCeiling();
  const float guarded =
      pes::RunRateAllowed(rate, ceiling, k.rate095) && 0.0f <= rate ? rate : 0.0f;
  req.SetF(moveword::kRate, guarded);

  // The style and its representative motion (DAT_1425ddcf0 at style*4 + band/4).
  // FUN_1406ba2f0's 9th argument is the caller's rateHeld flag (param_4 above); the contract's
  // LocomotionStyle does not carry it yet, so the port holds it here until the shared body does.
  (void)rateHeld;
  const int style = LocomotionStyle(host, static_cast<int>(req.U(moveword::kCommandId)), rate, req);
  req.SetU(moveword::kParamState, static_cast<uint32_t>(style));
  req.SetByte(moveword::kStyle, static_cast<uint8_t>(style));
  int member = 0;
  if (k.rate0225 <= rate) {
    if (k.blend04 <= rate) {
      member = 12;
      if (rate < k.rate07) member = 8;
    } else {
      member = 4;
    }
  }
  req.SetU(moveword::kMotion, static_cast<uint32_t>(t.styleGroup[style * 4 + member / 4]));

  // The foot band: 1..3 gives the side; 0 and 2 differ only in the start phase.
  const bool footIs13 = (static_cast<uint32_t>(foot - 1) & 0xfffffffd) == 0;
  bool side = static_cast<uint32_t>(foot - 1) < 2u;
  req.SetByte(moveword::kMirror, side ? 1 : 0);
  req.SetF(moveword::kStartRate, footIs13 ? 0.75f : 0.0f);
  // The two style masks: 0x1028 forces the mirror on, 0x814 off; a mask hit flips the side.
  if (style < 13 && ((0x1028u >> (style & 31)) & 1u) != 0) {
    req.SetByte(moveword::kMirror, 1);
  } else if (style <= 11 && ((0x814u >> (style & 31)) & 1u) != 0) {
    req.SetByte(moveword::kMirror, 0);
    side = !side;
  }
  if (side) {
    req.SetF(moveword::kStartRate, 0.0f);
    if (footIs13) req.SetF(moveword::kStartRate, 0.75f);
  } else {
    req.SetF(moveword::kStartRate, 0.5f);
    if (footIs13) req.SetF(moveword::kStartRate, 0.25f);
  }

  // The request words every run entry writes, and the transition to the loop.
  req.SetU(moveword::kTransitionWord, 0);
  req.SetF(moveword::kDwellLimit, 0.1f);
  req.SetF(moveword::kDwellElapsed2, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);
  req.SetF(moveword::kDwellElapsed, 0.0f);
  req.SetU(moveword::kPrevBucket, 0);
  RequestState(req, MoveState::kRunLoop);
  req.SetF(moveword::kDuration, k.blend3);

  // The placed start (0.1667 s) when the body turn is under 45 degrees and the two feet lie in
  // their window; the port reads the bones through the position ring (FUN_141eb51a0, bone 10
  // for the hip, 3 and 6 for the feet).
  float body[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(10, body);
  const float bodyTurn = req.F(moveword::kBodyDirection);
  const float facing = req.F(moveword::kFacing);
  float delta = bodyTurn - facing;
  if (k.half_turn < delta) {
    const float sign = delta < 0.0f ? k.neg : k.one;
    delta -= sign * k.full;
  }
  if (delta < k.deg45) {
    // ponytail: P+0x3b18 (the root-reach scale) and P+0x60b have no GF host read yet, so the
    // placed-start test takes the bone window only; the motion system's root track (P-KIN)
    // supplies the rest.
    float footA[3] = {0.0f, 0.0f, 0.0f};
    float footB[3] = {0.0f, 0.0f, 0.0f};
    host.PositionAt(3, footA);
    host.PositionAt(6, footB);
    const float hip = body[1];
    if (footA[2] < k.foot035 && (k.footNeg010 < footA[2] && footB[2] < k.footNeg010) &&
        k.footNeg035 < footB[2] && hip < body[1] - k.dist1m5) {
      req.SetF(moveword::kDuration, k.blend1667);
    }
  }
  req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
  req.SetF(moveword::kPlayback, k.one);
  req.SetF(moveword::kStartTime, 0.0f);
  req.SetByte(moveword::kTurnLockFlag, 0);
  host.SetRecordF0bec(facing);
}

// ---------------------------------------------------------------------------------------------
// FUN_1406d5d60, the STOP entry (mv4.a.c:1061). Two arms, chosen by the unsigned compare
// `param_2[0x1f] + 1 < 2`, which is true only for the class -1 sentinel:
//   the no-class arm asks the shared idle row (FUN_1406b93a0) at column 9, or 8/9 by the sign
//     of req[7] for kinds other than 3/6, duration 0.2, dwell 0.05 (0.03 below 5 km/h);
//   the run-stop arm turns the body to the target with the clamp +/-45 (67.5 for kinds 3/6 at
//     class 0), picks the bucket, and issues the stop row (FUN_1406b9d80) with the turn
//     playback, duration 0.2, state kStop.
// ---------------------------------------------------------------------------------------------
void EnterStop(MoveHost& host, MoveReq& req) {
  const pes::RunConstants& k = host.RunConstantsView();
  const pes::RunTables& t = host.RunTablesView();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int rowKind = kind == 8 ? 2 : kind;
  req.SetU(moveword::kRecordStampC, 0);

  if (static_cast<uint32_t>(cls + 1) < 2u) {
    // The no-class arm: an idle row and a SetMotion request.
    req.SetByte(moveword::kMotionPending, 1);
    int m = static_cast<uint32_t>(rowKind) < 2u ? 2 : rowKind;
    if (m == 4) m = 5;
    int row = 0x5a1;  // FUN_1406b93a0's fallback row (the shared idle-row picker).
    if (m < 8) row = IdleRow(host, req);
    const MoveCell cell = host.Cell(row, 9, false);
    bool mirror = host.Percent(50);
    if (kind != 3 && kind != 6) {
      const int col = static_cast<int>(k.negHalf < req.F(moveword::kFacingError)) + 8;
      mirror = host.Cell(row, col, false).mirror;
    }
    host.Issue(MotionRequest{cell.motionNo, mirror, 0, k.blend2, k.one, 0.0f, -1, 0.0f, 0.0f, 0});
    req.SetByte(moveword::kMirror, mirror ? 1 : 0);
    req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
    req.SetF(moveword::kDuration, k.blend2);
    req.SetF(moveword::kStartTime, 0.0f);
    req.SetF(moveword::kPlayback, k.one);
    req.SetF(moveword::kDwellLimit, STOP_DWELL);
    // The live-seconds arm: a class of -1, or a first dwell at most 0.2 s, with the current
    // ground speed below 5 km/h shortens the dwell to 0.03 (0x3cf5c28f).
    const float speed = host.CurrentSpeedKmh();
    if ((static_cast<uint32_t>(cls) == 0xffffffffu ||
         (req.F(moveword::kDwellElapsed) <= k.blend2 && k.blend2 != req.F(moveword::kDwellElapsed))) &&
        speed * host.TicksPerSecond() / k.kmh1000 < k.kmh5) {
      req.SetF(moveword::kDwellLimit, SLOW_DWELL);
    }
    req.SetF(moveword::kDwellElapsed, 0.0f);
    req.SetU(moveword::kLiveKind, static_cast<uint32_t>(m));
    req.SetF(moveword::kDwellElapsed2, 0.0f);
    req.SetF(moveword::kDwellLimit2, SECOND_DWELL);
    RequestState(req, MoveState::kStop);
    req.SetU(moveword::kRecordStampA, 0);
    req.SetU(moveword::kRecordStampB, 0);
    req.SetByte(moveword::kTurnLockFlag, 0);
    return;
  }

  // The run-stop arm: the body turn, clamped, gives the heading; the bucket picks the column.
  const float facing = req.F(moveword::kFacing);
  float turn = req.F(moveword::kFacingError);
  float limit = k.deg675;
  if (cls < 1 && (kind == 3 || kind == 6)) limit = k.deg45;
  turn = (turn <= limit && -limit <= turn) ? turn : (turn > limit ? limit : -limit);
  float heading = FoldFull(facing + turn, k);
  // The desired direction: the body direction max +/-90 off the heading, clamped to +/-45 of
  // the facing beyond 90 (FUN_1406d5d60's otherwise arm).
  const float bodyDir = req.F(moveword::kBodyDirection);
  float off = AngleDiffDeg(heading, bodyDir, k);
  if (k.deg90 < off) {
    float c = turn;
    if (turn <= k.deg45 && k.neg45 <= turn) c = turn;
    if (c < k.neg45) c = k.neg45;
    if (c > k.deg45) c = k.deg45;
    heading = FoldFull(facing + c, k);
    off = AngleDiffDeg(heading, bodyDir, k);
  }
  const int band = cls < 3 ? (cls < 0 ? 0 : cls) : 2;
  const int bucket = AngleBucket(host.AngleBuckets(), AngleDiffDeg(heading, facing, k), 10, false);
  req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));
  req.SetU(moveword::kPrevBucket, static_cast<uint32_t>(bucket));
  // The bearing to the player's own position (FUN_14046f230 of P+0x8), used as the lock's
  // stamp, then the bucket's centre angle (DAT_1425dd660) folded against the bearing.
  float pos[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(0, pos);
  req.SetU(moveword::kRecordStampA, static_cast<uint32_t>(pos[0]));
  req.SetU(moveword::kRecordStampB, static_cast<uint32_t>(bucket));
  // The turn playback (FUN_1406e9f10 of the mirror argument) with the capability clamp.
  const float p = host.MirrorChoice(false) ? k.one : k.half;
  float playback = pes::TurnPlayback(host.PhysiqueG(), p, host.PlaybackConstantsView());
  if (!(k.dist1m5 < playback || playback < k.ramp85)) playback = k.one;
  req.SetF(moveword::kPlayback, playback);
  // The stop row (FUN_1406b9d80) with the request's band, the kind (8 maps to 2) and the
  // bucket; the picker's own argument is req[0x1f] clamped to 0..2, not the class.
  // FUN_1406b9d80's class argument is the band word (moveword::kStopBand, req+0x144) clamped to
  // 0..2, and its kind argument is the request's kind with 8 mapped to 2 (ASM of 0x1406d5d60).
  const int stopBand = static_cast<int>(req.U(moveword::kStopBand));
  const int rowBand = stopBand < 3 ? (stopBand < 0 ? 0 : stopBand) : 2;
  const int row = StopRowFor(rowKind, rowBand, t);
  const MoveCell cell = host.Cell(row, bucket, false);
  host.Issue(MotionRequest{cell.motionNo, cell.mirror, 0, k.blend2, playback, 0.0f, -1, 0.0f, 0.0f, 0});
  req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
  req.SetByte(moveword::kMirror, cell.mirror ? 1 : 0);
  req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
  RequestState(req, MoveState::kStop);
  req.SetU(moveword::kRecordStampA, 0);
  req.SetU(moveword::kRecordStampB, 0);
  req.SetByte(moveword::kTurnLockFlag, 0);
  host.SetRecordF0bec(facing);
}


// FUN_140581ce0(ap+0x20, out, sign) + FUN_140591a10(out, target): the AI's picked point,
// re-signed and re-planted ahead of the player. GF has no AI target slot yet, so the port takes
// the request's own heading, which is what the pair returns for an unset target.
// ponytail: the AI target point (P-IMG imageA) removes the stand-in.
float PickedHeading(float playerHeadingDeg, float sign, const RunConstants& k) {
  float h = playerHeadingDeg + sign;
  if (h >= 0.0f) {
    if (k.full <= h) return h + k.wrapFull;
    return h;
  }
  return h + k.full;
}


// ---------------------------------------------------------------------------------------------
// FUN_1406bdf10, the GOTO_RUN handler (d_1406bdf10.c). One tick of the re-plan between a clip
// and the run. Its decompile's plVar1 is the anime-player interface at +0xab8, plVar2 the +0xac0
// one; iVar10 is the clip's current tick (FUN_1406b1f00's total), iVar11 its length
// (FUN_1406b1d00), and local_res18/local_108/local_100 the hit ticks (FUN_1406af620's start,
// FUN_1406af8e0's 0 and 1/2). GF has no hit records, so the hit ticks equal the clip's start.
// ---------------------------------------------------------------------------------------------
void GotoRun(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const float one = k.one;
  const float neg = k.neg;
  const float half = k.half;
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int motion = host.CurrentMotion();

  // The clip's tick frame. FUN_1406b1f00 (the total), FUN_1406b1d00 (the length) and
  // FUN_1406b1c70 (the length at a mirror) all read the live clip's HitData record through the
  // anime table: the record pointer at motion_object + 200 + 8*k and the tick count from bits
  // 10..19 of its first u32 (decompiled at 0x1406b1f00). FUN_1406afbc0 gives the record count.
  // GF has no hit records, so the port reads the clip's own length and holds the hit ticks at
  // zero: every arm that gates on a hit tick therefore takes PES's "no hit yet" branch, which is
  // what a clip without records does in PES too.
  // ponytail: the AnimePlayer hit records (A-AN13) supply hit0/hit1/hit2, and then these arms
  // run their real branches; the port's shape is PES's, only the source of the three numbers
  // is missing.
  const float clipPlayback = req.F(moveword::kDuration);
  const int clipLen = static_cast<int>(clipPlayback * host.TicksPerSecond() + half);
  int hit0 = 0;
  int hit1 = 0;
  int hit2 = 0;
  if (HitRecordCount() > 0) {
    // ponytail: FUN_1406af8e0(motion, playback, 0..2) fills hit0/hit1/hit2 from the records.
  }
  const int clipTotal = hit0;

  req.SetF(moveword::kRate, k.blend3);

  // The two motion families: 0x89c..0x8a4 (the run-step ids) and 0x8bb/0x8be/0x8c0 (the turn ids).
  const bool isRunStep = motion >= 0x89c && motion <= 0x8a4;
  const bool isTurnMotion = motion == 0x8bb || motion == 0x8be || motion == 0x8c0;
  bool turnFamily = false;
  bool turnFlag = false;
  turnFamily = isRunStep;
  if (req.U(moveword::kMoveBucket) == 0u || req.U(moveword::kMoveBucket) == 5u) {
    const int rolled = static_cast<int>(host.PickN(1) * k.kmh8 / host.TicksPerSecond() + half);
    if (isRunStep) {
      hit2 = hit1;
      hit1 = hit0;
      hit0 = rolled;
    }
  }
  turnFamily = isTurnMotion;

  // The yaw lock (FUN_1406cbb00's own call inside the function): the AI target ahead, the clip
  // still running, and the lock written with the desired direction and its tick count.
  if (!isTurnMotion && clipTotal <= clipLen && clipLen < (clipTotal + hit0 * 2) / 3 + 1) {
    float lockHeading = req.F(moveword::kRecordStampB);
    const int lockTicks = static_cast<int>(static_cast<float>(hit0 - clipLen) / req.F(moveword::kRecordStampA) + half);
    const float alt = FirstAngle(req.F(moveword::kHeading), req.F(moveword::kFacing), k);
    if (AngleDiffDeg(alt, req.F(moveword::kRecordStampB), k) < k.deg90) lockHeading = alt;
    const float ratio = AngleDiffDeg(req.F(moveword::kHeading), req.F(moveword::kRecordStampB), k);
    if (AngleDiffDeg(alt, req.F(moveword::kHeading), k) / static_cast<float>(lockTicks) < k.deg45) {
      req.SetU(moveword::kRecordStampB, static_cast<uint32_t>(lockHeading));
      req.SetByte(moveword::kTurnLockFlag, 1);
      req.SetU(moveword::kTurnLockTicks, static_cast<uint32_t>(lockTicks));
      req.SetByte(moveword::kTurnLockDir, 0);
      req.SetF(moveword::kRecordStampC, lockHeading);
      if (k.deg90 < req.F(moveword::kTurn)) {
        const float a = req.F(moveword::kTurn) < 0.0f ? neg : one;
        const float b = ratio < 0.0f ? neg : one;
        if (a != b) {
          uint8_t dir = 0;
          if (k.deg225 < ratio) dir = 1;
          req.SetByte(moveword::kTurnLockDir, dir);
        }
      }
      host.SetRecordF0bec(lockHeading);
    }
  }

  // The re-entry into the run (FUN_1406ce820's call): the class and the band both below 1, the
  // class above 1, the clip past the hit tick, not a keeper, and the turn past 45 degrees.
  const float turn = req.F(moveword::kTurn);
  if (static_cast<int>(req.U(moveword::kRateBand)) < 1 &&
      static_cast<int>(req.U(moveword::kBandTarget)) < 1 && cls > 1 &&
      clipLen < hit0 && host.MatchPhase() != 0x4a &&
      (k.deg45 < turn || k.deg45 < req.F(moveword::kFacingError) ||
       req.Flag(moveword::kTurnLockDir))) {
    EnterGotoRun(host, req, false);
    return;
  }

  // The desired direction and the two bearings: bVar6 is "no turn needed", bVar5 the fast-turn.
  const float desired = FirstAngle(req.F(moveword::kHeading), req.F(moveword::kFacing), k);
  const float bearing = AngleDiffDeg(desired, req.F(moveword::kFacing), k);
  bool noTurn = false;
  if (hit0 <= clipLen || bearing < k.deg45) noTurn = true;
  const float lookBearing = AngleDiffDeg(req.F(moveword::kLookHeading), req.F(moveword::kFacing), k);
  bool fastTurn = false;
  bool fastTurnLocked = false;
  if (!(lookBearing * lookBearing < k.kmh95sq) && req.U(moveword::kPrecise) == 1u) {
    fastTurn = true;
    fastTurnLocked = true;
  }
  if ((motion == 0x8d1 || turnFamily) && clipLen < hit2) {
    fastTurnLocked = fastTurn;
    if (clipLen < hit2) fastTurnLocked = false;
  }

  // The turn-lock test arm (FUN_1406d01d0's call) and the no-op class read.
  if ((static_cast<uint32_t>(static_cast<int>(req.U(moveword::kBandTarget)) - 1) < 2u) && cls > 2) {
    if (req.U(moveword::kPrecise) == 0u) {
      const bool keeper = host.MatchPhase() == 0x4a;
      if (bearing < k.deg45 && !keeper && motion != 0x8d1 && !turnFamily && TurnLockEligible(host, req)) {
        return;
      }
    }
  } else if (req.U(moveword::kPrecise) == 0u && req.U(moveword::kCommandSpeedClass) != 0xffffffffu &&
             bearing < k.deg675) {
    host.CommandSpeedClass();
  }

  // The stop arm: the previous state's dwell has expired and the turn is past 56.25 degrees.
  if (req.Flag(moveword::kTurnLockFlag) == false && req.U(moveword::kCommandSpeedClass) != 0xffffffffu) {
    const float dwellFactor = (req.U(moveword::kMoveBucket) == 0u || req.U(moveword::kMoveBucket) == 5u) ? k.rate06 : k.blend3;
    if ((static_cast<float>(clipLen) < static_cast<float>(hit0) * dwellFactor ||
         clipLen < clipTotal) &&
        (k.deg5625 <= bearing || fastTurnLocked)) {
      req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
      const int bucket = AngleBucket(host.AngleBuckets(), bearing, 10, false);
      req.SetF(moveword::kDesiredSpeedKmh, k.kmh5);
      req.SetU(moveword::kCommandSpeedClass, 0u);
      req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));
      EnterStop(host, req);
      return;
    }
  }

  // The turn arm: the class of -1 goes to the turn-stop entry; otherwise the band's turn
  // allowance (45 degrees plus 22.5 per look-distance unit) decides the run-turn or the re-plan.
  if (cls == -1) {
    if (noTurn && static_cast<int>(req.U(moveword::kBandTarget)) > 0) {
      EnterTurnStop(host, req);
      return;
    }
    req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
    const int bucket = AngleBucket(host.AngleBuckets(), bearing, 10, false);
    req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));
    EnterStop(host, req);
    return;
  }
  const float lookDist = req.F(moveword::kDistToLook);
  float allowance = one;
  if (k.speed2 < lookDist) {
    allowance = (lookDist < k.speed4) ? (lookDist - k.speed2) * half : one;
  } else {
    allowance = 0.0f;
  }
  const float bandAllowance = allowance * k.deg45 + k.deg45;
  if (bandAllowance <= bearing || fastTurnLocked) {
    const float ceiling = host.RateCeiling();
    req.SetF(moveword::kRate, k.rate06);
    const float speed = host.CurrentSpeedKmh();
    if (k.kmh8 <= speed) {
      if (k.kmh135 <= speed) {
        if (k.kmh22 < speed) {
          req.SetF(moveword::kRate, ceiling);
        }
      } else {
        req.SetF(moveword::kRate, k.blend3);
      }
    } else {
      req.SetF(moveword::kRate, k.rate05);
    }
    req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(ClassForRateFor(req.F(moveword::kRate), t)));
    req.SetF(moveword::kDesiredSpeedKmh, host.BandSpeedKmh(cls, host.PhysiqueG()) * k.speed3600);
    if (EnterRunTurnStep(host, req, (ceiling - k.rate06) * half)) return;
    const int foot = host.FootBand();
    req.SetF(moveword::kDwellLimit, 0.0f);
    req.SetF(moveword::kDwellLimit2, 0.0f);
    req.SetByte(moveword::kStyle, 0);
    req.SetByte(moveword::kMirror, static_cast<uint32_t>(foot - 1) < 2u ? 1 : 0);
    host.Issue(MotionRequest{-1, false, 0, k.blend1667, k.one, 0.0f, 0, 0.0f, req.F(moveword::kRate), foot});
    return;
  }

  // The straight-ahead re-plan: the speed bands and the quota share decide the class, then the
  // run continues with the body turned toward the AI target (the 180/112.5-degree guards).
  int quota = clipTotal + hit0 * 2;
  int share = quota / 3;
  if (turnFamily) {
    const float a = req.F(moveword::kTurn) < 0.0f ? neg : one;
    const float b = req.F(moveword::kLookDelta) <= 0.0f && req.F(moveword::kLookDelta) != 0.0f ? neg : one;
    if (a != b) share = hit0 * 75 / 100;
  }
  if (hit0 <= clipLen || (hit0 <= clipLen && share <= clipLen && k.turn16875 < bearing)) {
    const float facing = FoldFull(req.F(moveword::kFacing), k);
    const float heading = FoldFull(req.F(moveword::kHeading), k);
    float aim = AngleDiffDeg(heading, facing, k);
    if (k.deg1125b <= aim) return;
    float turnLimit = k.half_turn;
    if (motion == 0x8d1 || turnFamily) turnLimit = k.deg135;
    const float speedForTurn = FirstAngle(req.F(moveword::kHeading), req.F(moveword::kFacing), k);
    const float speed = AngleDiffDeg(speedForTurn, req.F(moveword::kFacing), k);
    if (k.kmh135 < speed) turnLimit = k.deg1125b;
    if (motion == 0x8d1 || turnFamily) turnLimit = k.deg135;
    float clamped = turnLimit;
    if (aim <= turnLimit && -turnLimit <= aim) clamped = aim;
    const float newHeading = FoldFull(facing + clamped, k);
    req.SetByte(moveword::kRootFacing, 1);
    req.SetF(moveword::kRootFacingAngle, newHeading);
    if (hit0 <= clipLen) req.SetF(moveword::kRecordStampC, newHeading);
    host.SetRecordF0bec(newHeading);
    return;
  }
  req.SetByte(moveword::kStyle, 9);
  const int foot = host.FootBand();
  req.SetByte(moveword::kMirror, static_cast<uint32_t>(foot - 1) < 2u ? 1 : 0);
  req.SetF(moveword::kDwellLimit, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);
  int bandCls = cls < 3 ? (cls < 0 ? 0 : cls) : 2;
  if (static_cast<int>(req.U(moveword::kBandTarget)) == 2 || cls == 2) {
    req.SetF(moveword::kRate, k.half);
  } else if (static_cast<int>(req.U(moveword::kBandTarget)) == 1) {
    req.SetF(moveword::kRate, k.blend3);
  } else {
    float r = RequestedRate(host, req, t);
    const float ceiling = host.RateCeiling();
    if (r <= ceiling && 0.0f <= r) {
      // in range
    } else {
      r = 0.0f;
    }
    req.SetF(moveword::kRate, r);
  }
  (void)bandCls;
  const int bucket = AngleBucket(host.AngleBuckets(), bearing, 10, false);
  host.Issue(MotionRequest{-1, false, 0, k.blend1667, k.one, 0.0f,
                            static_cast<int>(req.U(moveword::kStyle)), 0.0f,
                            req.F(moveword::kRate), foot});
  (void)bucket;
}


// ---------------------------------------------------------------------------------------------
// FUN_1406c9e20, the STOP handler (mv5.a.c:2764). One tick of the stop. Its decompile's
// five arms, in PES's own order:
//
//   A. the rate tail (req[0x4a]): `x - 0.05`, floored at 0.3 when the word is already at 0.3
//      or above (MAXSS of the two).
//   B. the re-plan into GOTO_RUN (FUN_1406ce820 with flag 0): req[0xe] clear, one of the
//      turn-lock words 0x3d/0x3e/0x3f set, band word 0x1f below 3, the ground speed under
//      16 km/h, and the stay decision (FUN_1406b6bb0) true.
//   C. STEP_TURN_RUN (FUN_1406d38d0): the live frame below the draw `tick*6/60 + 0.5`, the
//      class word 9 positive, the band word 0x1f positive, and the entry taking the tick.
//   D. the turn block: the bearing's ramp over 67.5..135 degrees scales the first dwell; the
//      test goes to the IDLE arm when the dwell is past it. Otherwise the small-turn arm
//      (0.8, or 0.5 at or above 22.5 degrees, of the same dwell, or precise with a facing
//      error past 67.5) with the class 1..2 and a positive band picks a stop bearing and takes
//      STEP_TURN_RUN, RUN_TURN_RUN (through FUN_1406b70d0) or RUN_TURN_STEP; then the stay
//      decision with FUN_1406b6ee0 false's update arm (the class/band/speed triple) and the
//      GOTO_RUN re-plan.
//   E. the IDLE arm: the second dwell cleared with the turn ramp's scale (0.7 or 0.8), the
//      stay decision false, the bucket 0/5 or the facing error under 33.75 or the command
//      0x79, the clip's hit tick at or below its own frame, then the idle row, the motion
//      store and the requests for IDLE.
//
// The port's `CurrentSpeedKmh` is the ground speed PES computes from the last tick's
// displacement: `ticks * 60 * 60 / 1000 * |displacement|` (FUN_141eb50b0 index 1). The
// AI-target reads (FUN_14046f230, FUN_140581ce0) have no GF slot; each is a ponytail.
// ---------------------------------------------------------------------------------------------
void Stop(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const int liveFrame = host.CurrentClipFrames();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const float facing = FoldFull(req.F(moveword::kFacing), k);
  float disp[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, disp);
  const float dispLen = std::sqrt(disp[0] * disp[0] + disp[1] * disp[1] + disp[2] * disp[2]);
  const float groundSpeed =
      static_cast<float>(host.TicksPerSecond()) * k.kmh60 * k.kmh60 / k.kmh1000 * dispLen;

  // Arm A: the rate tail. PES's own sequence is `xmm0 = word - 0.05`, and the compare
  // `word > word - 0.05` always holds, so the 0.3 substitution the decompile shows is dead
  // code in the shipped binary: the tail is the single SUBSS.
  req.SetF(moveword::kRate, req.F(moveword::kRate) - k.blend05);

  // Arm B: the re-plan into GOTO_RUN.
  if (req.U(moveword::kPrecise) == 0u &&
      (req.Flag(moveword::kTurnLockFlag) || req.U(moveword::kTurnLockTicks) != 0u ||
       req.Flag(moveword::kTurnLockDir)) &&
      static_cast<uint32_t>(req.U(moveword::kRateBand)) < 3u && groundSpeed < k.kmh16 &&
      StayDecision(host, req)) {
    EnterGotoRun(host, req, false);
    return;
  }

  // Arm C: STEP_TURN_RUN through the live-frame draw.
  const int draw = static_cast<int>(static_cast<float>(liveFrame) * k.ramp6 / k.kmh60 + k.half);
  if (liveFrame < draw && 0 < static_cast<int>(req.U(moveword::kCommandSpeedClass)) &&
      0 < static_cast<int>(req.U(moveword::kRateBand))) {
    if (EnterStepTurnRun(host, req)) return;
  }

  // Arm D's shared bearing: the heading against the AI target's bearing (word 1 here).
  const float targetBearing = AngleDiffDeg(req.F(moveword::kHeading), facing, k);
  float ramp = 0.0f;
  if (k.deg675 < targetBearing) {
    ramp = k.one;
    if (targetBearing < k.deg135) ramp = (targetBearing - k.deg675) / k.deg675;
  }
  const bool idles = req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit) * ramp ||
                     k.deg135 <= targetBearing;
  if (!idles) {
    // The small-turn scale: 0.8, or 0.5 at or above 22.5 degrees of the bearing.
    float smallScale = k.blend08;
    if (k.deg225 <= targetBearing) smallScale = k.half;
    const bool smallTurn = req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit) * smallScale ||
                           (req.U(moveword::kPrecise) == 1u &&
                            k.deg675 < std::fabs(req.F(moveword::kFacingError)));
    const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    if (smallTurn && static_cast<uint32_t>(cls - 1) < 2u &&
        0 < static_cast<int>(req.U(moveword::kRateBand))) {
      // The stop bearing to the AI target (FUN_14046f230 of the last tick's displacement).
      float ai[3] = {0.0f, 0.0f, 0.0f};
      host.PositionAt(0, ai);
      const float stopBearing = AngleDiffDeg(req.F(moveword::kFacing), ai[0], k);
      if (k.deg225 <= stopBearing) {
        if (EnterStepTurnRun(host, req)) return;
      } else if (req.U(moveword::kPrecise) != 1u) {
        // FUN_1406b70d0, the group-C helper: whether the step loop may turn into a run turn.
        if (StepLetsRunTurnPlaceholder(host)) {
          EnterRunTurnRun(host, req);
          return;
        }
      } else {
        // The precise arm's threshold: (rate ceiling - 0.6) / 2 from the +0xac0 interface.
        const float ceiling = host.RateCeiling();
        if (EnterRunTurnStep(host, req, (ceiling - k.rate06) * k.half)) return;
      }
    }
    // LAB_1406ca1b8: the stay decision with the group-C step-stop false. When neither
    // re-plan inside it applies the flow returns here, as PES's decompile does; when the
    // decision itself is false the tick falls straight through to the IDLE arm.
    if (StayDecision(host, req) && !StepStopsPlaceholder(host, req)) {
      if (req.U(moveword::kPrecise) == 0u &&
          AngleDiffDeg(facing, req.F(moveword::kHeading), k) < k.deg675 &&
          AngleDiffDeg(facing, req.F(moveword::kRecordStampB), k) < k.deg675) {
        // The update arm: the band's rate, its class and its speed, then the run re-plan.
        const int band = static_cast<int>(req.U(moveword::kRateBand));
        const int clampedBand = band < 3 ? (band < 0 ? 0 : band) : 2;
        const float bandRate = BandRateFor(clampedBand, kind, t);
        req.SetF(moveword::kRate, bandRate);
        const int newCls = ClassForRateFor(bandRate, t);
        req.SetU(moveword::kRateBand, static_cast<uint32_t>(newCls));
        req.SetF(moveword::kSpeedBand, host.BandSpeedKmh(newCls, host.PhysiqueG()) * k.speed3600);
        EnterGotoRun(host, req, false);
        return;
      }
      if (static_cast<int>(req.U(moveword::kRateBand)) < 2 ||
          smallScale * req.F(moveword::kDwellLimit) <= req.F(moveword::kDwellElapsed)) {
        if (req.F(moveword::kDwellLimit) * k.blend3 <= req.F(moveword::kDwellElapsed)) {
          req.SetU(moveword::kRateBand, 0u);
          req.SetF(moveword::kSpeedBand, k.kmh5);
        }
        EnterGotoRun(host, req, false);
        return;
      }
      return;
    }
  }

  // Arm E: the IDLE arm. The root-facing write is shared with the dwell-cleared block.
  if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2) &&
      req.F(moveword::kDwellElapsed2) != req.F(moveword::kDwellLimit2)) {
    // ponytail: FUN_140470d60's third argument (word 0x19) picks the AI target's stored
    // heading; the port uses the request's own word 2, which is PES's unset-target answer.
    req.SetByte(moveword::kRootFacing, 1);
    req.SetF(moveword::kRootFacingAngle, req.F(moveword::kFacing));
  }
  // The stop's turn ramp: 0 over the turn, 1 at or past 67.5 degrees, the ramp between.
  const float turn = req.F(moveword::kTurn);
  float turnRamp = 0.0f;
  if (k.deg675 < turn) {
    turnRamp = k.one;
    if (turn < k.deg135) turnRamp = (turn - k.deg675) / k.deg675;
  }
  if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1) turnRamp = 0.0f;
  float dwellScale = k.rate07;
  if (static_cast<int>(req.U(moveword::kRateBand)) == 2) dwellScale = k.blend08;
  const float dwellLimit = turnRamp * k.blend2 + dwellScale;
  if (dwellLimit * req.F(moveword::kDwellLimit) < req.F(moveword::kDwellElapsed) &&
      dwellLimit * req.F(moveword::kDwellLimit2) < req.F(moveword::kDwellElapsed2)) {
    if (!StayDecision(host, req)) {
      const int bucket = static_cast<int>(req.U(moveword::kMoveBucket));
      if (bucket == 0 || bucket == 5 || req.F(moveword::kFacingError) < k.deg3375 ||
          req.U(moveword::kCommandId) == 0x79u) {
        // FUN_1406af620's hit tick for the live clip; with no hit records it is the clip's
        // own frame, so the arm always passes.
        const int hitTick = host.CurrentClipFrames();
        if (hitTick <= liveFrame) {
          req.SetByte(moveword::kMotionPending, 1);
          int row = 0x5a1;  // FUN_1406b93a0's fallback row.
          if (static_cast<uint32_t>(kind) < 8u) row = IdleRow(host, req);
          const MoveCell cell = host.Cell(row, 9, false);
          bool mirror = host.Percent(50);
          if (kind != 3 && kind != 6) {
            mirror = host.Cell(row, static_cast<int>(k.negHalf < req.F(moveword::kFacingError)) + 8,
                               false)
                         .mirror;
          }
          req.SetByte(moveword::kMirror, mirror ? 1 : 0);
          req.SetF(moveword::kDuration, k.blend2);
          req.SetF(moveword::kStartTime, 0.0f);
          req.SetF(moveword::kPlayback, k.one);
          req.SetF(moveword::kDwellElapsed, 0.0f);
          req.SetF(moveword::kDwellLimit, 0.0f);
          req.SetF(moveword::kDwellElapsed2, 0.0f);
          req.SetF(moveword::kDwellLimit2, k.blend2);
          req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
          req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
          RequestState(req, MoveState::kIdle);
          req.SetByte(moveword::kTurnLockDir, 0);
          return;
        }
      } else {
        // FUN_1406d1390's arm: the same shape with a different row (not decoded). The port
        // takes PES's IDLE exit through the shared body, which the dispatcher port owns.
        // ponytail: FUN_1406d1390 (the fallback stop exit) is decoded when its group lands.
      }
    } else {
      // The stay-decision true arm: the band becomes the -1 sentinel and GOTO_RUN re-plans.
      req.SetU(moveword::kRateBand, 0xffffffffu);
      req.SetF(moveword::kSpeedBand, 0.0f);
      EnterGotoRun(host, req, false);
    }
  }
}


// ---------------------------------------------------------------------------------------------
// FUN_1406ce820, the GOTO_RUN entry (mv4.a.c:120). It writes the request for a run at the goto
// row and issues it. Its decompile has two arms chosen by `param_3` (the caller's
// runStopVariant flag): the shared second block (the row pick, its overrides, the swing arm
// and the transition writes) and, for the flag-1 path, the extra yaw lock through
// FUN_1406cba60. The port keeps PES's order and names each stand-in.
//
// The shared block, in PES's order:
//   0. the class word -1 becomes 1; with the precise word 0xe clear the playback is read
//      through the +0xac0 interface (vt+0x30's mirror argument, vt+0x50's answer);
//   1. the look-bucket arm (word 0xc at 4 or 9): the facing-error magnitude selects the AI
//      target's heading or the request's own word 1 as the desired angle, and the second word
//      0x5 is swapped between 180 and -180 with the bucket between 4 and 9;
//   2. the turn-lock arm (words 0x3d/0x3e clear): the class above 1, the bucket 4 or 9, the
//      turn-error magnitude past 45 degrees and the team record's flag, then the same swap;
//   3. the row pick (word 0 + word 9*8, through FUN_1406b9bd0's table) and the column from the
//      bucket word 0xc through FUN_1406b9b20's table;
//   4. three row overrides: the turn-lock byte 0x3f with a column of 8/9 (row 0x564), the
//      foot-band rows 0x561..0x566 and the band 0x55f..0x571 with the same column (rows
//      0x577 and 0x575 through FUN_140a58550/FUN_14044e020);
//   5. the distance/speed arm: rows 0x5c9/0x5ca with a column of 8/9 read the ball-line point
//      20 m away, the keeper test and the Percent(40) draw, ending at row 0x662 or 0x661;
//   6. the out-of-play arm: the match phase 1, the row in the two 0xa014-bit bands
//      (values 0..3 and 8..11, rows 0x5cc/0x5cd), the FUN_140a6cf70 point and the event-ring
//      frame, ending at row 0x5cc;
//   7. the two style rows (0x55f..0x571 with the mirror row in 0x562..0x564 or 0x570), the
//      swing row 0x573 (the target under 1.1 m ahead and the facing error under 67.5), the
//      foot row 0x575, and the transition writes (the motion, the mirror, the duration, the
//      playback, the hit words 0x27/0x28, the root-facing pair, the class/speed/band words and
//      the state word 7 for ACCELERATE or 0xe for RUN_TO_STEP).
// The port's mirrors, the AI target point, the event ring and the swing arm are absent in GF;
// each is a ponytail naming the site that supplies it.
// ---------------------------------------------------------------------------------------------
void EnterGotoRun(MoveHost& host, MoveReq& req, bool runStopVariant) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  // Block 0: the class sentinel and the playback read.
  int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  if (cls == -1) {
    cls = 1;
    req.SetU(moveword::kCommandSpeedClass, 1u);
  }
  float playback = k.one;
  if (req.U(moveword::kPrecise) == 0u) {
    // ponytail: the +0xac0 interface's vt+0x30/vt+0x50 pair (FUN_140a62960's mirror argument
    // and its answer) is the anime player's own playback; the port uses the request's default.
  }
  const int lookBucket = static_cast<int>(req.U(moveword::kLookBucket));

  // Block 1: the look-bucket arm.
  if (lookBucket == 4 || lookBucket == 9) {
    const float facingError = req.F(moveword::kFacingError);
    float desired = req.F(moveword::kHeading);
    // ponytail: FUN_140a76240/FUN_140439e50 of the request's +0x144 block (the AI target) are
    // not in GF; the port keeps the request's own heading, which is PES's answer for an unset
    // target.
    const float folded = FoldFull(req.F(moveword::kHeading), k);
    float delta = desired - folded;
    if (k.half_turn < delta) {
      const float sign = delta < 0.0f ? k.neg : k.one;
      delta -= sign * k.full;
    }
    const float mag = std::fabs(delta);
    const bool narrow = !(k.deg1125 < mag || k.deg1575 <= mag);
    const float a = delta < 0.0f ? k.neg : k.one;
    const float b = facingError < 0.0f ? k.neg : k.one;
    bool swap = false;
    if (narrow) {
      swap = (a == b) || !(k.deg135 < mag) || !(k.deg45 <= mag);
    } else {
      if (k.deg1125 < mag && mag < k.deg1575) {
        const float c = req.F(moveword::kLookDelta);
        const float e = (c <= 0.0f && c != 0.0f) ? k.neg : k.one;
        const float f = delta >= 0.0f ? k.one : k.neg;
        if (e != f) swap = true;
      }
    }
    if (swap) {
      const float second = lookBucket == 4 ? k.wrapHalf : k.half_turn;
      req.SetF(moveword::kLookDelta, second);
      req.SetU(moveword::kLookBucket, static_cast<uint32_t>(lookBucket == 4 ? 9 : 4));
    }
  }

  // Block 2: the turn-lock arm.
  if (!req.Flag(moveword::kTurnLockFlag) && req.U(moveword::kTurnLockTicks) == 0u &&
      k.deg45 < std::fabs(req.F(moveword::kLookDelta)) && cls > 1 &&
      (lookBucket == 4 || lookBucket == 9)) {
    // ponytail: FUN_140a48a30/FUN_140a71540 (the team's newest record and its rank byte) and
    // the record's own x position are the foundation's event ring; the port stands the
    // comparison at the request's own heading.
    const float aiX = req.F(moveword::kHeading);
    const float delta = aiX - FoldFull(req.F(moveword::kFacing), k);
    if (k.deg45 < std::fabs(delta)) {
      const float a = (req.F(moveword::kLookDelta) <= 0.0f && req.F(moveword::kLookDelta) != 0.0f)
                          ? k.neg
                          : k.one;
      const float b = delta < 0.0f ? k.neg : k.one;
      if (a != b) {
        // The turn-lock byte 0x5 flag and the 180/-180 swap follow in PES; the port applies
        // the same swap the look arm does, since the two arms share the second word.
        const float second = lookBucket == 4 ? k.half_turn : k.wrapHalf;
        req.SetF(moveword::kLookDelta, second);
        req.SetU(moveword::kLookBucket, static_cast<uint32_t>(lookBucket == 4 ? 9 : 4));
      }
    }
  }

  // Block 3: the row pick and the bucket's column.
  const int kind = static_cast<int>(req.U(moveword::kKind));
  int row = GotoRowFor(kind, cls, t);
  int column = t.bucketColumn[lookBucket];

  // Block 4: the three row overrides.
  if (req.Flag(moveword::kTurnLockDir) && static_cast<uint32_t>(column - 8) < 2u) {
    column = static_cast<int>(k.negHalf < req.F(moveword::kFacingError)) + 8;
    row = 0x564;  // FUN_1406b9bd0's table's override for the turn-lock family.
  }
  // ponytail: the foot-band rows 0x561..0x566 and the band 0x55f..0x571 overrides read
  // FUN_140a58550 (the record's class) and FUN_14044e020 (its draw), which are the
  // foundation's player table; the port leaves both overrides unapplied.
  if (row == 0x5c9) row = 0x5ca;

  // Block 5: the distance/speed arm.
  if (row == 0x5ca && static_cast<uint32_t>(column - 8) < 2u) {
    // ponytail: FUN_140439f90 of the ball-line point's record, the 20 m gate, the phase test
    // and the Percent(40) draw are the ball-track and keeper ports; the port takes the base
    // row, which is PES's answer for a missing point.
    row = 0x661;
  }

  // Block 6: the out-of-play arm.
  if (host.MatchPhase() == 1 && row < 0x10 && ((0xa014u >> (row & 0x1f)) & 1u) != 0) {
    // ponytail: FUN_140a6cf70's point, FUN_140a627e0's rank, the event ring's frame and the
    // two overrides (rows 0x5cc/0x5cd) are the foundation's event store; the port keeps the
    // base row and states the missing comparison here.
    row = 0x5cc;
  }

  // Block 7: the style, swing and foot rows, then the transition writes.
  int stateWord = 7;  // ACCELERATE, the entry's own state word.
  if ((static_cast<uint32_t>(row - 0x55f) < 0x13u) && (host.MirrorChoice(false)) &&
      ((row - 0x562) < 3u || row == 0x570)) {
    // ponytail: the style row's mirror test (FUN_140a62960's answer) is the anime player's
    // own; the port leaves the row as the table picked it.
  }
  const MoveCell cell = host.Cell(row, column, false);
  const float duration = k.blend1667;  // 0.1667 s, the entry's own blend.
  // The foot row 0x575 with a speed under 17.2 km/h takes the keeper's own row instead.
  int motionRow = static_cast<int>(cell.motionNo);
  if (row == 0x575 && host.CurrentSpeedKmh() <= 17.2f) {
    // ponytail: FUN_1406ebf30's two draws (0x662/0x661) are the anime player's; the port
    // takes the row the table gave, which is PES's answer for a missing draw.
  }
  (void)motionRow;
  // PES's vt+0x10 call takes (req, motion, mirror, the 0x3f-word's byte, the duration, the
  // playback, the start time); the port's MotionRequest carries the byte as `transition`
  // because the dispatcher's own Issue reads it as PES passes it.
  host.Issue(MotionRequest{cell.motionNo, cell.mirror, req.U(moveword::kTurnLockDir) & 0xffu,
                            duration, playback, 0.0f});
  // The hit words 0x27/0x28: the clip's own start and its pose scale, for a clip under 0x1125.
  if (cell.motionNo < 0x1125) {
    req.SetF(moveword::kHitFrame, host.MotionStartTime(cell.motionNo));
    req.SetF(moveword::kHitMotion, host.PhysiqueG());
  } else {
    req.SetF(moveword::kHitFrame, 0.0f);
    req.SetF(moveword::kHitMotion, 0.0f);
  }
  // The root-facing pair: the facing plus the bucket's centre angle (DAT_1425dd660), wrapped,
  // against the request's word 1, and the first dwell as the divisor when it is set.
  float rootFacing = FoldFull(req.F(moveword::kFacing), k);
  rootFacing =
      FoldFull(rootFacing + host.StepTablesView().bucketDegrees[Clamp03(lookBucket)], k);
  float bearing = AngleDiffDeg(rootFacing, req.F(moveword::kHeading), k);
  const float dwell = req.F(moveword::kDwellLimit);
  if (dwell == 0.0f) {
    bearing = 0.0f;
  } else {
    bearing = bearing / dwell;
  }
  req.SetF(moveword::kRecordStampA, bearing);
  // The class, speed and band words.
  int bandTarget = cls;
  if (bandTarget > 3) bandTarget = 3;
  req.SetU(moveword::kBandTarget, static_cast<uint32_t>(bandTarget));
  float speed = req.F(moveword::kDesiredSpeedKmh);
  float clampedSpeed = k.kmh16;
  if (speed <= k.kmh16 && k.kmh5 <= speed) clampedSpeed = speed;
  req.SetF(moveword::kSpeedBand, clampedSpeed);
  int band = bandTarget - 1;
  if (band > 2) band = 2;
  if (band < 0) band = 0;
  req.SetU(moveword::kRateBand, static_cast<uint32_t>(band));
  req.SetF(moveword::kSpeedBand, clampedSpeed);
  req.SetU(moveword::kPrevBucket, static_cast<uint32_t>(lookBucket));
  req.SetF(moveword::kRecordStampB, req.F(moveword::kHeading));
  req.SetU(moveword::kNextState, static_cast<uint32_t>(stateWord));
  req.SetByte(moveword::kChangeState, 1);
  req.SetF(moveword::kRecordStampC, req.F(moveword::kHeading));
  req.SetByte(moveword::kTurnLockDir, 0);
}
}  // namespace movestep
}  // namespace pes
