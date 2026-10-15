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

// FUN_1406b8de0(_, command): the command's speed class from table UNK_1425ddcd8.
int CommandClassOf(int command, const pes::RunTables& t) {
  if (static_cast<uint32_t>(command - 1) < 5u) return t.commandClass[command];
  return -1;
}

// FUN_1406b9e50(_, band, kind): the band's rate from table UNK_1425dde10; band 0 with kind 3
// or 6 has no rate (AS 3.1).
float BandRateOf(int band, int kind, const pes::RunTables& t) {
  if (band == 0 && (kind == 3 || kind == 6)) return 0.0f;
  if (band < 0) return t.bandRate[0];
  if (band > 3) return t.bandRate[3];
  return t.bandRate[band];
}

// FUN_1406ba030(_, rate): the first ladder entry the rate is below, else the last class.
int ClassForRateOf(float rate, const pes::RunTables& t) {
  for (int i = 0; i < 4; ++i) {
    if (rate < t.rateLadder[i]) return t.rateClass[i];
  }
  return t.rateClass[3];
}


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
  if (useClass) return BandRateOf(cls, kind, t);
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
    rate = BandRateOf(cls, kind, t);
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

}  // namespace movestep
}  // namespace pes
