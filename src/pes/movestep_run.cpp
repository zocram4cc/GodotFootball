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
#include "pes/movestep_step.hpp"
#include "pes/movestep_issue.hpp"
#include "pes/movestep_turn_entry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_turn.hpp"

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
    {0x1425c4898, &RunConstants::weightScale},
    {0x14259bdd0, &RunConstants::speedDt},
    {0x14259bdd8, &RunConstants::rampSpan13},
    {0x14256ae18, &RunConstants::centi},
    {0x14258c260, &RunConstants::stopPlaybackFloor},
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
    {0x1425ddf84, &RunConstants::deg5p625},
    {0x14259be54, &RunConstants::deg84375},
    {0x1425ddfb0, &RunConstants::deg123p5},
    {0x1425ddfc4, &RunConstants::deg163p125},
    {0x1425ddf90, &RunConstants::kmh172},
    {0x1425ddf88, &RunConstants::kmh1475b},
    {0x1425a358c, &RunConstants::rate055},
    {0x14259bdd4, &RunConstants::foot075},
    {0x1425ddfe8, &RunConstants::foot027},
    {0x14259be2c, &RunConstants::rec40},
    {0x1425cb2fc, &RunConstants::rec59},
    {0x1425c48cc, &RunConstants::rec99},
    {0x14259be34, &RunConstants::frame50},
    {0x1425c594c, &RunConstants::frame95},
    {0x14256b140, &RunConstants::replay45},
    {0x1425ddfbc, &RunConstants::deg146p25},
    {0x1425a3590, &RunConstants::dist110},
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
    {0x142588200, &RunConstants::frameGate01},
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
    {0x1425ddf7c, &RunConstants::fFree},
    {0x1425ddf40, &RunConstants::fStepA},
    {0x1425ddf70, &RunConstants::fTargetA},
    {0x1425ddf38, &RunConstants::fStepB},
    {0x1425ddf3c, &RunConstants::fStepC},
    {0x14259be38, &RunConstants::daLow},
    {0x14259be48, &RunConstants::daHigh},
    {0x14259be84, &RunConstants::daSpan},
    {0x14259be1c, &RunConstants::reserveLow},
    {0x142596e6c, &RunConstants::reserveHigh},
    {0x1425c48fc, &RunConstants::reserveSpan},
    {0x14259d620, &RunConstants::capSpan},
    {0x1425e4a60, &RunConstants::accelAbove},
    {0x14259d5d8, &RunConstants::accelBelow},
    {0x1425ab128, &RunConstants::decelSpan},
    {0x14259d654, &RunConstants::decelSub},
    {0x142588210, &RunConstants::condition10},
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
constexpr uint64_t STYLE_GROUP_VA = 0x1425ddcf0;     // FUN_1406d2030, style*4 + band/4: kRunStyleCount (17) styles x 4 members
constexpr uint64_t CURVE_KNOT_VA = 0x1425dde30;      // FUN_1406b9aa0, 3 floats (the table's fourth, 1.0, is never compared)
constexpr uint64_t CURVE_SPAN_VA = 0x1425dde40;      // FUN_1406b9aa0 0x1406b9af5, 3 floats
constexpr uint64_t CURVE_BASE_VA = 0x1425dde50;      // FUN_1406b9aa0 0x1406b9aec, 3 floats
constexpr uint64_t CURVE_SLOPE_VA = 0x1425dde60;     // FUN_1406b9aa0 0x1406b9afe, 3 floats

// FUN_1406d5d60's three dwell immediates. They are instruction operands (movss off an immediate
// stored on the stack), not .rdata floats, so they have no VA; the values are the ones the
// decompile shows at each store: 0x3cf5c28f, 0x3d4ccccd and 14 ticks over the 54 Hz clock.
constexpr float STOP_DWELL = 0.1f;    // FUN_1406d5d60's first arm stores 0x3dcccccd (0.1 s) at req[0x1b]
constexpr float SLOW_DWELL = 0.03f;   // 0x3cf5c28f (0.03 s): the slow-stop first dwell
constexpr float SECOND_DWELL = 0.35f;  // 0x3eb33333 (0.35 s) at req[0x2e]


// ---------------------------------------------------------------------------------------------
// The interface functions, ported from the pointer table at 0x1425e35d0.
// ---------------------------------------------------------------------------------------------


}  // namespace

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

namespace {

// The clamp PES writes as a pair of unsigned compares (the "band 0..3" idiom).
int Clamp03(int v) { return v < 0 ? 0 : (v > 3 ? 3 : v); }
// FUN_1406d2030's first block: the speed clamped into [5, 28] km/h, PES's compare order.
float ClampSpeedKmh(float v, const RunConstants& k) {
  if (v <= k.kmh28 && k.kmh5 <= v) return v;
  return v <= k.kmh28 ? k.kmh5 : k.kmh28;
}

}  // namespace

// FUN_140a92950(heading, target, clamp): the heading stepped toward the target by at most the
// clamp, wrapped into [0, 360). Ported from its own decompile, keeping PES's comparisons so its
// NaN behaviour carries: the difference is FUN_14043a0a0's wrapped diff, the step is the smaller
// of its magnitude and the clamp, the sign steps toward the target, and the fold matches
// FUN_140a92c80's first fold.
float StepHeading(float heading, float target, float clampDeg, const RunConstants& k) {
  const float diff = PesAngleDiff(heading, target, k);
  const float sign = (0.0f <= diff) ? k.one : k.neg;
  float step = sign * diff;
  if (clampDeg <= step) step = clampDeg;
  heading = FoldFull(heading, k);
  heading += step * sign;
  if (0.0f > heading) return heading + k.full;
  if (k.full <= heading) return heading + k.wrapFull;
  return heading;
}

// The clip's hit words, read from the live motion's record at rate 1.0 by GOTO_RUN, ACCELERATE and
// DECELERATE (FUN_1406bdf10, FUN_1406bb570, FUN_1406bcd20). RotData entry 0 gives the total
// (FUN_1406b1f00), the end (FUN_1406b1d00) and the signed offset (FUN_1406b1c70, negated when the anime
// flag is set). The three hit ticks are CancelData entries 0..2 (FUN_1406af8e0); an entry the motion
// lacks is the Animation blend (FUN_1406af620), which is what PES's branches take.
struct ClipHits {
  int total = 0;
  int end = 0;
  float signedLen = 0.0f;
  int cancelCount = 0;  // FUN_1406afbc0(.., 5)
  int hit[3] = {0, 0, 0};
  int blend = 0;  // FUN_1406af620: the Animation blend, the value the missing hits fall back to
};
ClipHits ReadClipHits(const MoveHost& host, int motion, const RunConstants& k, bool mirror) {
  const pes::MotionRecord& rec = host.Motion(motion);
  const RotRecord rot = rec.Rot(0);
  ClipHits h;
  h.total = host.ClipTicks(pes::ClipReader::kRotStart, motion, k.one, 0);
  h.end = host.ClipTicks(pes::ClipReader::kRotEnd, motion, k.one, 0);
  h.signedLen = static_cast<float>(rot.offset) * (mirror ? k.neg : k.one);
  h.cancelCount = rec.CancelCount();
  const int blend = host.ClipTicks(pes::ClipReader::kBlend, motion, k.one, 0);
  for (int i = 0; i < 3; ++i)
    h.hit[i] = i < h.cancelCount ? host.ClipTicks(pes::ClipReader::kCancel, motion, k.one, i) : blend;
  h.blend = blend;
  return h;
}

// PES's player record's class-17 byte (P+0x2458), which the no-class branch reads twice
// (the AI target's rank test and the set-piece kind fold) and the entries read for their
// own record chains. GF's shared MoveHost contract does not carry it yet.
// ponytail: the shared contract adds the accessor when the record writer is ported (A-AN);
// until then the port stands it at PES's "no record" answer, which is the byte's zero.
int RecordClass17Byte() { return 0; }

// FUN_141eb50b0's index-1 read: the last tick's displacement, whose length the ground-speed
// compute uses.
float LastDisplacement(const MoveHost& host) {
  float d[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, d);
  return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

// The ground speed in km/h PES computes at the head of FUN_1406bb570 and FUN_1406bcd20:
// `ticks * 60 * 60 / 1000 * |last-tick displacement|` (FUN_141e532d0's rate, the two 60s,
// the 1000 cm per metre, and the displacement's length).
float GroundSpeedKmh(const MoveHost& host, const RunConstants& k) {
  return static_cast<float>(host.TicksPerSecond()) * k.kmh60 * k.kmh60 / k.kmh1000 *
         LastDisplacement(host);
}

// FUN_14046f230 of FUN_141eb50b0(P, out, 1): the heading in [0, 360) of the last tick's displacement, PES's (x, z) pair.
float DisplacementBearingDeg(const MoveHost& host, const RunConstants& k) {
  float d[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, d);
  return FoldFull(std::atan2(d[0], d[2]) * host.StyleConstantsView().radToDeg, k);
}


// FUN_1406c9e20's update arm and FUN_1406bb570's LAB_1406bcbda are the same block, which
// PES reaches through the +0xac0 interface's slot 0 = FUN_1406d2030 (EnterRun): the class
// 0/1/2 from the ground speed's 5 / 11 ladder, the band rate through FUN_1406b9e50, the
// class through FUN_1406ba030, the band's speed times 3.6, and the run entry itself. The two
// call sites differ only in the speed they pass (the ground-speed compute in STOP, the
// FUN_14042d470 read in ACCELERATE), so the port factors the block once.
void RunUpdateArm(MoveHost& host, MoveReq& req, const pes::RunTables& t, float speedKmh) {
  const pes::RunConstants& k = host.RunConstantsView();
  const int foot = host.FootBand();
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(foot - 1u < 2u));
  req.SetF(moveword::kDwellLimit, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);
  req.SetByte(moveword::kStyle, 0);
  int band = 0;
  if (k.kmh5 <= speedKmh) {
    band = (k.kmh11 <= speedKmh) ? 2 : 1;
  }
  const int bandClamped = band < 3 ? (band < 0 ? 0 : band) : 2;
  // Slot 0x38 of the +0xac0 interface is FUN_1406b9ff0: the requested rate for the
  // (class, kind) pair the block computes, which is the port's RequestedRate.
  req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(bandClamped));
  const float rate = RequestedRate(host, req, t);
  req.SetF(moveword::kRate, rate);
  // Slot 0x68 is FUN_1406ba030 (the ladder's class), slot 0x48 the band's own speed.
  const int cls = ClassForRateFor(rate, t);
  req.SetU(moveword::kRateBand, static_cast<uint32_t>(cls));
  req.SetF(moveword::kSpeedBand, RateToSpeedFor(rate, host.PhysiqueG(), t, k.rampSlope) * k.speed3600);
  // Slot 0 of the +0xac0 interface is FUN_1406d2030 (group B's run entry).
  movestep::EnterRun(host, req, foot, false);
}

// FUN_1406b8de0(_, command): the command's speed class from table UNK_1425ddcd8.
int CommandClassFor(int command, const pes::RunTables& t) {
  if (static_cast<uint32_t>(command - 1) < 5u) return t.commandClass[command];
  return -1;
}

// FUN_1406b9e50(_, band, kind): the band's rate from table UNK_1425dde10; band 0 with kind 3
// or 6 has no rate (AS 3.1).
float BandRateFor(int band, int kind, const pes::RunTables& t) {
  // 0x1406b9e50: `cmp edx,3; ja zero` (unsigned, so a negative or too large band has no rate), then band 0 at kind 3 or 6.
  if (static_cast<uint32_t>(band) > 3u) return 0.0f;
  if (band == 0 && (kind == 3 || kind == 6)) return 0.0f;
  return t.bandRate[band];
}

// FUN_1406b9aa0(rate, g) (vt+0x48 of +0xac0): the rate floored at the knot floor (0x1425c48a0), then the segment
// (knots 0x1425dde30, last one at or below the rate) gives metres a second. 0x1406b9aa8: `comiss floor, rate; jb` keeps the
// rate only when the floor is below it; 0x1406b9ad7 returns 0 when no knot is at or below.
float RateToSpeedFor(float rate, float g, const pes::RunTables& t, float rateFloor) {
  if (!(rateFloor < rate)) rate = rateFloor;
  for (int i = 2; i >= 0; --i) {
    if (t.curveKnot[i] <= rate) {
      float slope = (rate - t.curveKnot[i]) / t.curveSpan[i] * t.curveSlope[i];
      if (i < 2) slope *= g;
      return g * t.curveBase[i] + slope;
    }
  }
  return 0.0f;
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
      !exe.ReadVa(STYLE_GROUP_VA, sizeof t.styleGroup, reinterpret_cast<uint8_t*>(t.styleGroup.data())) ||
      !exe.ReadVa(CURVE_KNOT_VA, sizeof t.curveKnot, reinterpret_cast<uint8_t*>(t.curveKnot.data())) ||
      !exe.ReadVa(CURVE_SPAN_VA, sizeof t.curveSpan, reinterpret_cast<uint8_t*>(t.curveSpan.data())) ||
      !exe.ReadVa(CURVE_BASE_VA, sizeof t.curveBase, reinterpret_cast<uint8_t*>(t.curveBase.data())) ||
      !exe.ReadVa(CURVE_SLOPE_VA, sizeof t.curveSlope, reinterpret_cast<uint8_t*>(t.curveSlope.data()))) {
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
  // FUN_1406d2030's call: the anime state, the rate ceiling (vt+0x18 of +0xac0) and this entry's rateHeld.
  const int style = LocomotionStyle(host, host.AnimeState(), rate, req, host.RateCeiling(), rateHeld);
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
  req.SetByte(moveword::kReorientFlag, 0);
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
  const int cls = static_cast<int>(req.U(moveword::kClassWord));  // PES's param_2[0x1f] picks the arm and the picker's band
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
    req.SetByte(moveword::kMirror, mirror ? 1 : 0);
    req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
    req.SetF(moveword::kDuration, k.blend2);
    req.SetF(moveword::kStartTime, 0.0f);
    req.SetF(moveword::kPlayback, k.one);
    req.SetF(moveword::kDwellLimit, STOP_DWELL);
    // The live-seconds arm: a class word of -1, or a first dwell at most 0.2 s, with the ground speed below 5 km/h (the last
    // displacement, as FUN_1406d5d60 computes it at line 76) shortens the dwell to 0.03 (0x3cf5c28f).
    if ((static_cast<uint32_t>(cls) == 0xffffffffu ||
         (req.F(moveword::kDwellElapsed) <= k.blend2 && k.blend2 != req.F(moveword::kDwellElapsed))) &&
        GroundSpeedKmh(host, k) < k.kmh5) {
      req.SetF(moveword::kDwellLimit, SLOW_DWELL);
    }
    req.SetF(moveword::kDwellElapsed, 0.0f);
    req.SetU(moveword::kLiveKind, static_cast<uint32_t>(m));
    req.SetF(moveword::kDwellElapsed2, 0.0f);
    req.SetF(moveword::kDwellLimit2, SECOND_DWELL);
    RequestState(req, MoveState::kIdle);  // FUN_1406d5d60 lines 85-86: the no-class arm requests IDLE (req[0x39] = 1)
    req.SetU(moveword::kRecordStampA, 0);
    req.SetU(moveword::kRecordStampB, 0);
    req.SetByte(moveword::kReorientFlag, 0);
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
  float off = InlineDiffDeg(heading, bodyDir, k);
  if (k.deg90 < std::fabs(off)) {
    float c = turn;
    if (turn <= k.deg45 && k.neg45 <= turn) c = turn;
    if (c < k.neg45) c = k.neg45;
    if (c > k.deg45) c = k.deg45;
    heading = FoldFull(facing + c, k);
  }
  const int band = cls < 3 ? (cls < 0 ? 0 : cls) : 2;
  const float bearing = DisplacementBearingDeg(host, k);  // uVar17: *FUN_14046f230 of the ring sample
  const int bucket = AngleBucket(host.AngleBuckets(), PesAngleDiff(heading, bearing, k), 10, false);
  // FUN_1406d5d60 stores the bucket in req[0x20] only (decompile line 151: param_2[0x20] = bucket). Real PES calls (stop_replay.py,
  // seq7084 / seq7145 / seq9209 of pid2392772) leave req[8] and req[0x25] as they were and pass req[0x20] to the selector.
  req.SetU(moveword::kLiveBucket, static_cast<uint32_t>(bucket));
  // The root-facing rate (0x1406d5f..: param_2[0x19]): the bucket's centre angle plus the facing, folded, against the bearing,
  // over one sixth of a second's ticks.
  const float centre = FoldFull(host.StepTablesView().bucketDegrees[bucket] + facing, k);
  req.SetF(moveword::kRecordStampC, PesAngleDiff(centre, bearing, k) / (host.TicksPerSecond() * k.blend1667));
  // The turn playback (vt+0x30 then vt+0x50), reset to 1.0 outside [0.9, 1.5] (0x1406d5fb3 COMISS [0x14259bddc], 0x1406d5fbf
  // COMISS [0x14258c260], both jumping to the reset at 0x1406d5fcc).
  float playback = EntryPlayback(host);
  if (k.dist1m5 < playback || playback < k.stopPlaybackFloor) playback = k.one;
  req.SetF(moveword::kPlayback, playback);
  // The turn-stop selector: FUN_1406d5d60 calls the +0xac0 interface's vt+0xE0 (0x1425E35B0 + 0xE0 = FUN_1406BA0D0), which reads
  // the row from DAT_1425ddb50[kind + class * 8] and the column from DAT_1425dd988[bucket], then FUN_1406ABB30 / FUN_1406ABD60.
  // The kind maps 8 to 2 and the class is the class word req[0x1f] clamped to 0..2 (not the band word req[0x51]).
  const int rowBand = cls < 3 ? (cls < 0 ? 0 : cls) : 2;
  const int row = TurnStopRowFor(rowKind, rowBand, t);
  const MoveCell cell = host.Cell(row, t.bucketColumn[bucket], false);
  WriteIssue(host, req, MotionRequest{cell.motionNo, cell.mirror, 0, k.blend2, playback, 0.0f});  // 0x1406d6074 CALL [RAX+0x10]
  req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
  RequestState(req, MoveState::kStop);
  req.SetU(moveword::kRecordStampA, 0);
  req.SetU(moveword::kRecordStampB, 0);
  req.SetByte(moveword::kReorientFlag, 0);
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

  // The clip's record reads: the total (FUN_1406b1f00), the length (FUN_1406b1d00) and the three hit
  // ticks (FUN_1406af8e0 at entries 0..2, the Animation blend when missing), all from the live
  // motion's record through ReadClipHits.
  const ClipHits clip = ReadClipHits(host, motion, k, host.ClipFlag());
  // iVar10 of FUN_1406bdf10 is the anime interface's vt+0x38: the current clip's frame (decompile line 47).
  const int clipLen = host.CurrentClipFrames();
  const int clipTotal = clip.total;
  int hit0 = clip.hit[0];
  int hit1 = clip.hit[1];
  int hit2 = clip.hit[2];

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
    // PES's turn-lock (ACCELERATE's block): the stamp steps toward the heading by at most 22.5 degrees when the turn from the
    // stamp is under 90; the lock is the stamp itself otherwise.
    const float ratio = PesAngleDiff(req.F(moveword::kRecordStampB), req.F(moveword::kHeading), k);
    if (std::fabs(ratio) < k.deg90) {
      lockHeading = StepHeading(req.F(moveword::kRecordStampB), req.F(moveword::kHeading), k.deg225, k);
    }
    if (std::fabs(PesAngleDiff(req.F(moveword::kFacing), lockHeading, k)) / static_cast<float>(lockTicks) < k.deg45) {
      req.SetU(moveword::kRecordStampB, static_cast<uint32_t>(lockHeading));
      req.SetByte(moveword::kReorientFlag, 1);
      req.SetU(moveword::kTurnFrames, static_cast<uint32_t>(lockTicks));
      req.SetF(moveword::kAngle3d, lockHeading);
      req.SetByte(moveword::kMirrorFlag, 0);
      req.SetF(moveword::kRecordStampC, lockHeading);
      if (k.deg90 < req.F(moveword::kTurn)) {
        const float a = req.F(moveword::kTurn) < 0.0f ? neg : one;
        const float b = ratio < 0.0f ? neg : one;
        if (a != b) {
          uint8_t dir = 0;
          if (k.deg225 < std::fabs(ratio)) dir = 1;
          req.SetByte(moveword::kMirrorFlag, dir);
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
       req.Flag(moveword::kMirrorFlag))) {
    EnterGotoRun(host, req, false);
    return;
  }

  // The desired direction and the two bearings: bVar6 is "no turn needed", bVar5 the fast-turn.
  // ponytail: PES's GOTO_RUN has no FUN_140a92950 call at this point (its one helper call is the turn-lock above); the
  // heading stands here, as FirstAngle's stub had it. The PES value for the desired direction is not decoded yet.
  const float desired = req.F(moveword::kHeading);
  const float bearing = InlineDiffDeg(desired, req.F(moveword::kFacing), k);
  bool noTurn = false;
  if (hit0 <= clipLen || bearing < k.deg45) noTurn = true;
  const float lookBearing = PesAngleDiff(req.F(moveword::kRecordStampB), req.F(moveword::kLookHeading), k);
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
      if (bearing < k.deg45 && !keeper && motion != 0x8d1 && !turnFamily && EnterGotoTurn(host, req)) {
        return;
      }
    }
  } else if (req.U(moveword::kPrecise) == 0u && req.U(moveword::kCommandSpeedClass) != 0xffffffffu &&
             bearing < k.deg675) {
    host.CommandSpeedClass();
  }

  // The stop arm (FUN_1406bdf10 lines 215-217): it runs when the state before the current one is not IDLE, or the second
  // dwell has run out, or the command id is 5. P+0x245d holds that state (read at VA 0x1406be573): it is written as the
  // old state at each change (FUN_1406b8340's tail, VA 0x1406b8736), at its INIT path (0x1406b8408) and at an anime
  // action entry (FUN_1406eb440, 0x1406eb459).
  const bool secondDwellPast = req.F(moveword::kDwellLimit2) <= req.F(moveword::kDwellElapsed2) &&
                               req.F(moveword::kDwellElapsed2) != req.F(moveword::kDwellLimit2);
  const bool stopArmOpen = host.MoveStatePrevious() != static_cast<int>(MoveState::kIdle) || secondDwellPast ||
                           req.U(moveword::kCommandId) == 5u;
  if (stopArmOpen && req.U(moveword::kCommandSpeedClass) != 0xffffffffu) {
    const float dwellFactor = (req.U(moveword::kMoveBucket) == 0u || req.U(moveword::kMoveBucket) == 5u) ? k.rate06 : k.blend3;
    if ((static_cast<float>(clipLen) < static_cast<float>(hit0) * dwellFactor ||
         clipLen < clipTotal) &&
        (k.deg5625 <= bearing || fastTurnLocked)) {
      req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
      const int bucket = AngleBucket(host.AngleBuckets(), bearing, 10, false);
      req.SetU(moveword::kClassWord, 0u);  // PES param_2[0x1f] = 0 before the STOP entry: its no-class arm answers IDLE
      req.SetF(moveword::kStopSpeedWord, k.kmh5);  // PES param_2[0x1d] = 5.0 (FUN_1406bdf10 line 226)
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
    req.SetF(moveword::kDesiredSpeedKmh, RateToSpeedFor(req.F(moveword::kRate), host.PhysiqueG(), t, k.rampSlope) * k.speed3600);
    if (EnterRunTurnStep(host, req, (ceiling - k.rate06) * half)) return;
    // PES's GOTO exit ends in the +0xac0 vt+0 call (FUN_1406bdf10 line 630): the run entry picks the style's motion row, the
    // mirror and the 0.1 dwell limit, and requests RUN_LOOP. The dispatcher's tail issues the clip.
    EnterRun(host, req, host.FootBand(), false);
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
    float aim = InlineDiffDeg(heading, facing, k);
    if (k.deg1125b <= aim) return;
    float turnLimit = k.half_turn;
    if (motion == 0x8d1 || turnFamily) turnLimit = k.deg135;
    // ponytail: PES's speed-for-turn source is not decoded (no FUN_140a92950 call here); the heading stands, as the stub had it.
    const float speedForTurn = req.F(moveword::kHeading);
    const float speed = InlineDiffDeg(speedForTurn, req.F(moveword::kFacing), k);
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
  // FUN_1406bdf10's rate ladder (param_2[0x24], param_2[0x1f]): band target 2 or rate band 2 runs at 0.5.
  if (static_cast<int>(req.U(moveword::kBandTarget)) == 2 || static_cast<int>(req.U(moveword::kRateBand)) == 2) {
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
  const int bucket = AngleBucket(host.AngleBuckets(), bearing, 10, false);
  // LAB_1406bec79: the run entry through the +0xac0 interface's slot 0 (FUN_1406d2030), which requests RUN_LOOP.
  EnterRun(host, req, foot, false);
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
  // PES stop_dec.c:63-66: the pre-empt reads the low bytes of words 0x3d, 0x3e and 0x3f, not the words.
  if (req.U(moveword::kPrecise) == 0u &&
      (req.Flag(moveword::kAngle3d) || req.Flag(moveword::kTurnFrames) ||
       req.Flag(moveword::kMirrorFlag)) &&
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
  const float targetBearing = std::fabs(PesAngleDiff(DisplacementBearingDeg(host, k), req.F(moveword::kHeading), k));
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
      const float stopBearing = std::fabs(PesAngleDiff(req.F(moveword::kFacing), DisplacementBearingDeg(host, k), k));
      if (k.deg225 <= stopBearing) {
        if (EnterStepTurnRun(host, req)) return;
      } else if (req.U(moveword::kPrecise) != 1u) {
        // FUN_1406b70d0, the group-C helper: whether the step loop may turn into a run turn.
        if (StepLetsRunTurn(host, req)) {
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
    if (StayDecision(host, req) && !StepStops(host, req, false)) {
      if (req.U(moveword::kPrecise) == 0u &&
          std::fabs(PesAngleDiff(facing, req.F(moveword::kHeading), k)) < k.deg675 &&
          std::fabs(PesAngleDiff(facing, DisplacementBearingDeg(host, k), k)) < k.deg675) {
        // The update arm: the band's rate, its class and its speed, then the run entry.
        RunUpdateArm(host, req, t, GroundSpeedKmh(host, k));
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
      // FUN_1406c9e20 line 237 reads the look bucket, req[0xd] (VA 0x1406ca4ea), not the move bucket word 8. The
      // facing error is compared by magnitude (the ANDPS mask at 0x1406ca502 before COMISS against 0x1425cc688).
      const int bucket = static_cast<int>(req.U(moveword::kLookBucketAlt));
      if (bucket == 0 || bucket == 5 || std::fabs(req.F(moveword::kFacingError)) < k.deg3375 ||
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
          req.SetByte(moveword::kMirrorFlag, 0);
          return;
        }
      } else {
        // FUN_1406d1390, the idle family's turn exit into IDLE_TURN: PES's else arm of FUN_1406c9e20 (VA 0x1406ca51f).
        EnterIdleTurn(host, req);
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
// FUN_1406ce820, the GOTO_RUN entry, ported from its instruction stream (asm gf-StepAudit3-scratch/a_1406ce820.txt). `runStopVariant`
// is the caller's R8B: it is stored as the issue's transition word (req[0x36]). The decompile's float variables are reused
// across arms, so every compare below cites the instruction it comes from.
//
//   0x1406ce840  the aim flag (FUN_1406ba7d0); the class word -1 becomes 1; a precise request (req[0xe]) takes 0x1406cfd40.
//   0x1406ce8f1  the playback: vt+0x50(vt+0x30(match-up flag)), stored in req[0x35] at once (0x1406ce9ac).
//   0x1406ce9c0  arm 1, look bucket 4 or 9: the look point's turn against the look delta, then the ball arm.
//   0x1406ceb57  arm 2, no lock bytes, a wide look delta, class above 1, bucket 3 or 8: the swing test.
//   0x1406cef41  the goto row of (kind, class) and its overrides (swing 0x572, turn lock byte 0x566, the tired rows 0x565).
//   0x1406cf0cf  the lock arm (rows 0x56f / 0x570) or the carrier contest (row 0x571 and the analyser call).
//   0x1406cf6a5  the jostle block: rows 0x573 and 0x575 against the marked man.
//   0x1406cfa8f  the issue, the hit words, the root-facing rate, and the class / speed words of the request.
// ---------------------------------------------------------------------------------------------
namespace {

float SignOf(float v) { return v < 0.0f ? -1.0f : 1.0f; }  // COMISS 0,v; JBE: -1.0 (XMM11) when 0 > v, else 1.0 (XMM9)

// MAXSS dst, src: dst when dst > src, else src (NaN and equality answer src).
float Maxss(float dst, float src) { return dst > src ? dst : src; }

// The turn from the facing (P+0x20) to `reference`, folded to +-180 (XMM6 in each ball arm).
float TurnToward(const MoveHost& host, float reference, const RunConstants& k) {
  float turn = reference - host.FacingDeg();
  if (k.half_turn < std::fabs(turn)) turn -= SignOf(turn) * k.full;
  return turn;
}

// The own slot is 11..21 (`TEST EAX; JS; CMP EAX,0xb; JL; CMP EAX,0x16; SETL`): the argument of FUN_140a62960 and FUN_140a48a30.
bool OwnSideFlag(int slot) { return slot >= 11 && slot < 22; }

// The ball arm's heading source (0x1406cec67..0x1406ced3b and its copies): the ball sample (clock * 20 / 60 + 0.5 ticks ahead),
// or the folded look heading when the command is 0x2f or 0x1d..0x1f (`withCommand`), or the carrier is out of the roster and the
// look point lies 22.5..157.5 degrees off the facing.
float BallArmReference(MoveHost& host, const MoveReq& req, const RunConstants& k, bool withCommand) {
  using namespace moveword;
  float ball[3];
  host.BallSample(static_cast<int>(host.TicksPerSecond() * k.reserveLow / k.kmh60 + k.half), ball);
  const float ballDeg = host.HeadingToPointDeg(ball);
  const int carrier = host.CarrierSlot();
  const float off = std::fabs(PesAngleDiff(req.F(kFacing), req.F(kLookHeading), k));
  const uint32_t cmd = req.U(kCommandId);
  const bool command = withCommand && (cmd == 0x2f || cmd - 0x1d <= 2u);
  if (command || (static_cast<uint32_t>(carrier) > 0x15u && k.deg225 < off && off < k.deg1575)) return FoldFull(req.F(kLookHeading), k);
  return ballDeg;
}

// FUN_1406ce820 from 0x1406cfd40: the precise request. Its own row table (DAT_1425dd9b0), bucket from the stepped look
// heading, a step playback and a blend of 0.2 or 0.15 less the ability weight squared; the state word is 4.
void GotoRunPrecise(MoveHost& host, MoveReq& req) {
  using namespace moveword;
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const StepTables& st = host.StepTablesView();
  const int kind = static_cast<int>(req.U(kKind));
  const int cmdClass = static_cast<int>(req.U(kCommandSpeedClass));
  int ebx = cmdClass > 2 ? 2 : (cmdClass < 0 ? 0 : cmdClass);  // 0x1406cfd4f..0x1406cfd63
  const float desired = req.F(kDesiredSpeedKmh);
  float speed = desired > k.kmh16 ? k.kmh16 : Maxss(k.kmh5, desired);  // 0x1406cfd68..0x1406cfd84
  if (static_cast<int>(req.U(kClassWord)) >= 1) {
    req.SetF(kStopSpeedWord, speed);  // 0x1406cfdb4
    req.SetU(kClassWord, static_cast<uint32_t>(ebx));
  } else {
    ebx = 0;  // 0x1406cfd8a..0x1406cfd95: EBX = 0 whatever the class
    speed = desired > k.kmh5 ? k.kmh5 : Maxss(k.kmh5, desired);  // 0x1406cfd9b..0x1406cfda3
    req.SetU(kClassWord, 0u);
    req.SetF(kStopSpeedWord, k.kmh5);  // 0x40a00000
  }
  // 0x1406cfdbc..0x1406cfdf4: FUN_140a92950(facing, look heading, 22.5), its turn from the heading, the bucket (vt+0x60).
  const float stepped = StepHeading(req.F(kFacing), req.F(kLookHeading), k.deg225, k);
  const int bucket = AngleBucket(host.AngleBuckets(), PesAngleDiff(stepped, req.F(kHeading), k), 10, false);
  const int col = t.bucketColumn[bucket];
  int row = st.kindBandRow[kind + ebx * 8];  // DAT_1425dd9b0, 0x1406cfe26
  bool settled = false;
  if (col == 0x11 || col == 0) {
    if (row == 0x53a) {
      if (host.Percent(0x32)) row = 0x53b;  // 0x1406cfe41..0x1406cfe57
      settled = true;
    } else if (row == 0x542) {
      if (host.Percent(0x14)) row = 0x543;  // 0x1406cfe66..0x1406cfe7c
      settled = true;
    }
  }
  if (!settled && row == 0x52e) {
    row = 0x530;  // 0x1406cfe83 CMP EDI,0x52e; JZ 0x1406cfea3
  } else if (req.U(kCommandId) == 5u && ebx == 0 && static_cast<uint32_t>(kind - 2) <= 1u) {
    row = 0x530;  // 0x1406cfe8b..0x1406cfea3
  }
  const CellPick cell = PickCell(host, row, col);
  req.SetByte(kFootFlag, host.BucketIsLeft(bucket) ? 1 : 0);  // DAT_1425dd950, 0x1406cff07
  req.SetU(kPrevBucket, req.U(kLiveBucket));                  // req[0x25] = req[0x20]
  req.SetU(kLiveBucket, static_cast<uint32_t>(bucket));
  req.SetF(kSpeedBand, speed);                                // req[0x22]
  req.SetU(kBandTarget, static_cast<uint32_t>(ebx));          // req[0x24]
  // 0x1406cff39..0x1406cff72: FUN_1406afbc0(motion, 4) then FUN_1406b10d0(motion, 1.0, 0), an answer PES drops: no effect.
  float blend = k.blend2;
  float dwell = k.rampSlope;
  if (ebx >= 1 && (ebx >= 2 || (kind != 3 && kind != 6))) {  // 0x1406cff8d..0x1406cffaf
    const bool matchUp = host.Team().IsMatchUpMan(host.ActionIndex());
    const float weight = AbilityWeight(host, matchUp);
    const float lowered = k.rampSlope - weight * weight * k.weightScale;  // 0x1406d0004..0x1406d0014
    blend = lowered > k.rampSlope ? k.rampSlope : Maxss(k.rate01, lowered);
    dwell = blend;
  }
  float playback = StepPlayback(host.PhysiqueG(), host.PlaybackConstantsView());
  req.SetF(kPlayback, playback);  // 0x1406d008b
  if (kind == 4 && req.F(kDistToTarget) < k.dist1m5) {  // 0x1406d007b..0x1406d00d2
    float ramp = 0.0f;
    if (!(k.blend2 >= req.F(kDistToTarget))) ramp = (req.F(kDistToTarget) - k.blend2) / k.rampSpan13;
    ramp *= playback;
    playback = ramp > k.dist1m5 ? k.dist1m5 : Maxss(k.blend08, ramp);
    req.SetF(kPlayback, playback);
  }
  WriteIssue(host, req, MotionRequest{cell.motion, cell.mirror, 0, blend, req.F(kPlayback), host.MotionStartTime(cell.motion)});
  req.SetF(kDwellLimit, dwell);  // 0x1406d0128, after the issue's own hit seconds
  req.SetU(kDwellElapsed, 0u);
  req.SetU(kLiveKind, static_cast<uint32_t>(kind));
  req.SetU(kNextState, static_cast<uint32_t>(MoveState::kStepLoop));  // 0x1406d0135: state 4
}

}  // namespace

void EnterGotoRun(MoveHost& host, MoveReq& req, bool runStopVariant) {
  using namespace moveword;
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const TurnEntryTables& tt = host.TurnEntryTablesView();
  const float tps = host.TicksPerSecond();

  const bool aim = host.EntryAimFlag();                                            // 0x1406ce840
  if (req.U(kCommandSpeedClass) == 0xffffffffu) req.SetU(kCommandSpeedClass, 1u);  // 0x1406ce845
  if (req.U(kPrecise) != 0u) {                                                     // 0x1406ce857
    GotoRunPrecise(host, req);
  } else {
    const float playback = EntryPlayback(host);
    req.SetF(kPlayback, playback);  // 0x1406ce9ac
    bool forced = false;            // [RBP+0xb8]
    bool swing = false;             // [RSP+0x40]

    // Arm 1 (0x1406ce9c0): the look bucket is 4 or 9.
    if (req.U(kLookBucket) == 4u || req.U(kLookBucket) == 9u) {
      const float facingMag = std::fabs(req.F(kFacingError));  // XMM6
      float lookPoint = req.F(kHeading);                        // XMM7
      if (RequestLookCount(req) > 1) {                          // 0x1406ce9e9: CMP AL,1; JBE
        float point[3];
        RequestLookPoint(req, point);
        lookPoint = host.HeadingToPointDeg(point);
      }
      float lookTurn = FoldFull(lookPoint, k) - FoldFull(req.F(kHeading), k);  // XMM7
      if (k.half_turn < std::fabs(lookTurn)) lookTurn -= SignOf(lookTurn) * k.full;
      const float lookMag = std::fabs(lookTurn);  // XMM12
      bool ball = true;
      if (k.deg1125 < facingMag && facingMag < k.deg1575) {  // 0x1406cea80, 0x1406cea9d
        if (SignOf(lookTurn) == SignOf(req.F(kFacingError)) || k.deg135 < lookMag || lookMag < k.deg45) {
          if (k.deg1125 < lookMag) {  // 0x1406ceae6: COMISS XMM12,XMM3; JBE 0x1406cec67
            if (SignOf(req.F(kLookDelta)) != SignOf(req.F(kFacingError))) {
              FlipLookBucket(req, k);
              forced = true;
            }
            ball = false;
          }
        }
      }
      if (ball) {  // 0x1406cec67
        const float turn = TurnToward(host, BallArmReference(host, req, k, true), k);
        const float turnMag = std::fabs(turn);
        if (k.turn16875 < turnMag && turnMag < k.deg163p125) {  // 0x1406ced76, 0x1406ced83
          if (SignOf(lookTurn) == SignOf(turn) || k.deg135 < lookMag || lookMag < k.deg45) {  // 0x1406cedb4..0x1406cedcd
            if (SignOf(req.F(kLookDelta)) != SignOf(turn)) {
              FlipLookBucket(req, k);
              forced = true;
            }
          }
        }
      }
    }

    // Arm 2 (0x1406ceb57).
    if (!req.FlagAtByte(0x3d) && !req.FlagAtByte(0x3e) && k.deg135 < std::fabs(req.F(kLookDelta)) &&
        static_cast<int>(req.U(kCommandSpeedClass)) > 1 && (req.U(kLookBucket) == 3u || req.U(kLookBucket) == 8u)) {
      const float turn = TurnToward(host, BallArmReference(host, req, k, true), k);
      if (k.deg45 < std::fabs(turn) && SignOf(req.F(kLookDelta)) != SignOf(turn)) {  // 0x1406cee4a, 0x1406cee54..0x1406cee75
        const bool side = OwnSideFlag(host.ActionIndex());
        const float anchor = host.TeamAnchorHeading(side);  // FUN_140a71540(MA, FUN_140a48a30(G, side)+0x254)
        const float anchorTurn = PesAngleDiff(req.F(kFacing), anchor, k);
        float own[3];
        host.OwnPosition(own);
        if (SignOf(anchorTurn) != SignOf(turn) || std::fabs(own[0]) < host.PitchLimit()) {  // 0x1406cef12..0x1406cef33
          forced = true;
          swing = true;
        }
      }
    }

    // The goto row (0x1406cef41).
    const int kind = static_cast<int>(req.U(kKind));
    int row = t.gotoRow[kind + static_cast<int>(req.U(kCommandSpeedClass)) * 8];  // DAT_1425dda50, 0x1406cef66
    int col = t.bucketColumn[req.U(kLookBucket)];                                  // DAT_1425dd988, 0x1406cef6e
    const int slot = host.ActionIndex();
    if (swing) {
      row = 0x572;
    } else {
      if (req.FlagAtByte(0x3f)) {  // 0x1406cef83
        row = 0x566;
        if (static_cast<uint32_t>(col - 8) <= 1u) col = (0.0f > req.F(kFacingError) ? 1 : 0) + 8;  // SETA R15B; ADD R15D,8
      }
      if (static_cast<uint32_t>(slot) <= 0x15u && static_cast<uint32_t>(row - 0x561) <= 5u && ((0x29u >> (row - 0x561)) & 1u) != 0u &&
          static_cast<uint32_t>(col - 8) <= 1u) {  // 0x1406cefb1..0x1406cefdc
        const int stamina = host.StaminaPercent();
        const float ticks = static_cast<float>(host.RecordTicks(slot));
        if (stamina < 0x14 && tps * host.MoveConstantsView().dist3 < ticks) row = 0x565;  // 0x1406cf033..0x1406cf04e
      }
    }
    CellPick cell = PickCell(host, row, col);  // 0x1406cf059..0x1406cf0a1

    // 0x1406cf0b4..0x1406cf696: the lock arm, the carrier contest or neither.
    const bool lock3d = req.FlagAtByte(0x3d), lock3e = req.FlagAtByte(0x3e);
    int footTable = 0;  // 0: DAT_1425dd930 (0x1406cf683), 1: DAT_1425dd940 (0x1406cf377)
    if ((lock3d || lock3e) && static_cast<int>(req.U(kCommandSpeedClass)) > 1) {  // 0x1406cf0cf, 0x1406cf0dc
      footTable = 1;
      if (lock3d && !swing) {
        row = 0x56f;  // 0x1406cf0f1
      } else {
        if (!forced && (req.U(kLookBucket) == 4u || req.U(kLookBucket) == 9u)) {  // 0x1406cf0fc..0x1406cf114
          const float errMag = std::fabs(req.F(kFacingError));
          bool flip = false;
          if (k.deg1125 < errMag && errMag < k.deg1575) {  // 0x1406cf132, 0x1406cf13b
            flip = SignOf(req.F(kLookDelta)) != SignOf(req.F(kFacingError));
          } else {
            const float turn = TurnToward(host, BallArmReference(host, req, k, false), k);  // 0x1406cf1a1..0x1406cf268
            const float turnMag = std::fabs(turn);
            flip = k.deg225 < turnMag && turnMag < k.deg1575 && SignOf(req.F(kLookDelta)) != SignOf(turn);
          }
          if (flip) {  // 0x1406cf167..0x1406cf19c: bucket 3, 4 -> 9; 8, 9 -> 4
            const uint32_t to = (req.U(kLookBucket) - 3u) > 1u ? 4u : 9u;
            req.SetU(kLookBucket, to);
            req.SetF(kLookDelta, to == 4u ? k.wrapHalf : k.half_turn);
          }
        }
        row = 0x570;  // 0x1406cf30b
      }
      col = t.bucketColumn[req.U(kLookBucket)];  // 0x1406cf330
      cell = PickCell(host, row, col);
    } else if (host.MatchPhase() == 1 && static_cast<int>(req.U(kCommandSpeedClass)) > 0 &&
               static_cast<uint32_t>(slot) <= 0x15u && !swing) {  // 0x1406cf384..0x1406cf3b2
      const bool trainer = host.MatchObjectFlag2c0();             // [RBP+0xb8]
      bool contest = false;                                       // DIL
      const int carrier = host.CarrierSlot();
      if (static_cast<uint32_t>(carrier) <= 0x15u) {
        float own[3], other[3];
        host.OwnPosition(own);
        host.RecordPosition(carrier, other);
        const float groundSpeed = GroundSpeedKmh(host, k);  // XMM9: FUN_14042d470 of the ring sample's length
        const float dz = own[2] - other[2], dx = own[0] - other[0];
        if (k.kmh5 > std::sqrt(dz * dz + dx * dx)) {  // 0x1406cf4bf
          const bool carrierSide = carrier >= 11;
          if (OwnSideFlag(slot) != carrierSide) {
            const float bodyOff = std::fabs(PesAngleDiff(req.F(kBodyDirection), req.F(kHeading), k));
            if (bodyOff > k.deg135 && groundSpeed > host.MoveConstantsView().dist2m5) {  // 0x1406cf525, 0x1406cf52a
              const bool keeper = host.AnimeRecordClass(carrier) == 0x12;
              const int pct = trainer ? (keeper ? 0x5f : 0x46) : (keeper ? 0x32 : 10);
              contest = host.Percent(pct);
            }
          }
        }
      }
      bool take = true;
      if (aim) {
        contest = true;  // 0x1406cf59b
      } else if (!contest) {
        // 0x1406cf5a5: the class word at or below 0 or a request turn of at most 135 degrees ends the arm.
        if (static_cast<int>(req.U(kClassWord)) <= 0 || !(std::fabs(req.F(kTurn)) > k.deg135)) take = false;
      }
      if (take) {
        const bool three = req.U(kCommandSpeedClass) == 3u;
        const int pct = trainer ? (three ? 0x1e : 0x14) : (three ? 1 : 0);  // 0x1406cf5c8..0x1406cf5ed
        if (contest || host.Percent(pct)) {
          row = 0x571;  // 0x1406cf60d
          col = t.bucketColumn[req.U(kLookBucket)];
          cell = PickCell(host, row, col);
          host.CountAnalyserEvent(trainer, cell.motion);  // 0x1406cf67e: FUN_1406cba60(P, trainer, motion)
        }
      }
    }
    req.SetByte(kFootFlag, footTable != 0 ? tt.footFlagTurn[req.U(kLookBucket)] : tt.footFlag[req.U(kLookBucket)]);  // 0x1406cf6a5

    // The jostle block (0x1406cf6b2..0x1406cfa86): a man marked by this one, level with him in front, takes the swing rows.
    float startTime = 0.0f;  // XMM9 at 0x1406cfaa7, the issue's start
    if (static_cast<uint32_t>(slot) <= 0x15u && static_cast<uint32_t>(col) <= 0x11u && ((0x2a015u >> col) & 1u) != 0u &&
        static_cast<uint32_t>(row - 0x55f) <= 0x12u && ((0x4c03fu >> (row - 0x55f)) & 1u) != 0u) {
      const int target = host.JostleTarget(OwnSideFlag(slot));
      if (static_cast<uint32_t>(target) <= 0x15u) {
        const int targetClass = host.AnimeRecordClass(target);
        const float marked = host.AnimeRecordFloat(target, 0xc8);
        if (targetClass != 0x11 && marked > k.rate07 && !tt.ClassHasFlag7(static_cast<int8_t>(targetClass))) {  // 0x1406cf772..0x1406cf79d
          float own[3], other[3];
          host.OwnPosition(own);
          host.RecordPosition(target, other);
          const float dxz2 = (own[2] - other[2]) * (own[2] - other[2]) + (own[0] - other[0]) * (own[0] - other[0]);
          const float gap = std::sqrt(dxz2);  // FUN_140a8f860(own, target): the ground distance
          float v[3] = {other[0] - own[0], other[1] - own[1], other[2] - own[2]};
          RotateXZ(v, host.FacingDeg() * k.neg, host.SpeedClassConstantsView());  // 0x1406cf82c: FUN_1404bcad0(v, -facing)
          const float markedTurn = std::fabs(PesAngleDiff(req.F(kHeading), host.AnimeRecordFloat(target, 0x124), k));  // XMM12
          float velocity[3];
          host.TargetVelocity(target, k.speedDt / tps, velocity);  // 0x1406cf87b: FUN_140a6c6d0
          const float speed = tps * k.kmh60 * k.kmh60 / k.kmh1000 *
                              std::sqrt(velocity[1] * velocity[1] + velocity[0] * velocity[0] + velocity[2] * velocity[2]);  // XMM15
          const float dx = v[0], dz = v[2];
          const float ahead = std::fabs(dx);  // |XMM6|
          if (k.rate07 > gap && dz > 0.0f && k.half > ahead && k.deg675 > markedTurn && k.kmh16 > speed) {  // 0x1406cf8ac..0x1406cf916
            bool take = false;
            if (col == 0xd || col == 4) {
              take = k.blend3 > ahead;  // 0x1406cf949
            } else if (col == 0xf || col == 2) {
              take = true;  // 0x1406cf956
            } else if (col == 0x11 || col == 0) {
              take = k.blend08 > ahead;  // 0x1406cf943
            }
            if (take) {
              row = 0x573;
              cell = PickCell(host, row, col);  // 0x1406cf962..0x1406cf9a9
            }
          }
          if (k.dist11 > gap && dz > 0.0f && k.blend08 > ahead && k.deg675 > markedTurn && k.kmh16 > speed) {  // 0x1406cf9b6..0x1406cf9f4
            bool take = false;
            if (((col - 0xd) & ~2) == 0) {
              take = k.footNeg010 > dx;  // 0x1406cfa1d: COMISS [0x14259d648], XMM6
            } else if (((col - 2) & ~2) == 0) {
              take = dx > k.frameGate01;  // 0x1406cfa14: COMISS XMM6,[0x142588200]
            }
            if (take) {
              row = 0x575;
              cell = PickCell(host, row, col);  // 0x1406cfa2a..0x1406cfa80
            }
          }
        }
      }
    }

    // The issue (0x1406cfa8f..0x1406cfbb1): the blend is 1/6, or 0.4 in the replay's stop / goto states or on the 0x50 / 0x52 action.
    float blend = k.blend1667;
    if (host.AnimeState() == kReplayAnimeState) {
      const int state = host.CurrentState();
      if ((state == 6 || state == 8) && static_cast<int>(req.U(kCommandSpeedClass)) < 1 && req.U(kKind) == 3u) blend = k.blend04;
    } else if (host.CurrentState() == 0 && (((host.ActionSubByte() - 0x50) & ~2) == 0)) {
      blend = k.blend04;  // 0x1406cfb01..0x1406cfb1a
    }
    if (host.CurrentState() == 0x16) {  // 0x1406cfb22..0x1406cfb77: a motion in the 0x2b50 columns starts at 0.15
      // vt+0x30 of the +0xab8 interface is `movzx eax, byte [rcx+0x19ba]` = P+0x2472 (0x1406ec700), ClipFlag().
      if (cell.mirror == host.ClipFlag() && static_cast<uint32_t>(col) <= 0xdu && ((0x2b50u >> col) & 1u) != 0u &&
          (static_cast<uint32_t>(row - 0x562) <= 2u || row == 0x56e))
        startTime = k.rampSlope;
    }
    WriteIssue(host, req, MotionRequest{cell.motion, cell.mirror, runStopVariant ? 1 : 0, blend, playback, startTime});  // 0x1406cfbb1
    CopyHitWords(req, cell.motion);  // 0x1406cfbb4..0x1406cfc15

    // The root-facing rate (0x1406cfc1b..0x1406cfc9d): the facing plus the look bucket's centre, folded, against the heading,
    // over the hit seconds the issue stored.
    float rootFacing = FoldFull(req.F(kFacing), k);
    rootFacing = FoldFull(rootFacing + host.AngleBuckets().centre[req.U(kLookBucket)], k);
    const float bearing = PesAngleDiff(rootFacing, req.F(kHeading), k);
    req.SetF(kRecordStampA, req.F(kDwellLimit) == 0.0f ? 0.0f : bearing / req.F(kDwellLimit));

    // The class and speed words (0x1406cfc9d..0x1406cfd3b). The store of req[0x1d] = 0.5 * the band speed at 0x1406cfcfc is
    // overwritten at 0x1406cfd1d by the clamp below, so only the clamp is written.
    const int cmdClass = static_cast<int>(req.U(kCommandSpeedClass));
    req.SetU(kBandTarget, static_cast<uint32_t>(cmdClass > 3 ? 3 : (cmdClass < 0 ? 0 : cmdClass)));  // req[0x24]
    const float desired = req.F(kDesiredSpeedKmh);
    req.SetF(kSpeedBand, desired > k.kmh28 ? k.kmh28 : Maxss(k.kmh5, desired));  // req[0x22]
    const int bandIndex = cmdClass - 1;
    req.SetU(kClassWord, static_cast<uint32_t>(bandIndex > 2 ? 2 : (bandIndex < 0 ? 0 : bandIndex)));  // req[0x1f]
    req.SetF(kStopSpeedWord, desired > k.kmh16 ? k.kmh16 : Maxss(k.kmh5, desired));                  // req[0x1d]
    req.SetU(kLiveBucket, req.U(kLookBucket));                                                       // req[0x20] = req[0xc]
    req.SetU(kNextState, static_cast<uint32_t>(MoveState::kGotoRun));                                // 0x1406cfd31: state 7
  }
  // The shared tail (0x1406d013b).
  req.SetU(kRecordStampB, req.U(kHeading));  // req[0x18] = req[1]
  req.SetByte(kChangeState, 1);
  req.SetU(kLiveKind, req.U(kKind));  // req[0x1e] = req[0]
  req.SetByte(kTurnLockFlag, 0);
  // P+0x4d0 vt+8 (the AdjustAngle adjuster's apply): a no-op in this port (AdjustAngleApply, movestep_turn.cpp).
}


// ---------------------------------------------------------------------------------------------
// FUN_1406bb570, the ACCELERATE handler (mv5.a.c:3058). One tick of the speed-up between two
// running states. Its decompile's flow, in PES's own order:
//
//   A. the yaw-lock block: the re-orient object's flag clear, the hit tick at or before the
//      live frame and the live frame at or before the mid-tick, with the hit-signed value
//      non-zero. The desired heading is the AI target's own (word 0x18) when its bearing is
//      under 90 degrees; the distance is the hit-mid over the F2468; the turn per tick is
//      that bearing over the distance, and under 45 degrees the lock words 0x3d/0x3e/0x3f are
//      written and FUN_1406cbb00 stores the yaw.
//   B. the pre-empt gates: the command is not 0x11, the anime state is not the replay 0x4a
//      (or the command is not 0x72), the two motion predicates FUN_140a6e290/FUN_140a6e2e0
//      are clear, and one of the three frame tests holds: the drawn tick, the hit-start
//      times 0.1, or the motion 0xbea with its drawn tick. The bearing at or above 56.25
//      degrees (or the fast-turn flag) clears the dwell words and reaches the shared band /
//      speed block; otherwise the class-3 gate (a word-5 bearing over 90, the band below 2
//      and class 3) takes the ground-speed ladder and RUN_TURN_RUN.
//   C. the guard: the live frame at or before the hit start with the motion not 0xbde /
//      0xbe1 / 0xbdf returns without a write.
//   D. the class -1 arm (the hit end at or before the live frame, or the turn bearing under
//      45 degrees): band 3 with 28 km/h, then the ground-speed ladder 8 / 14.75 / 19 into the
//      turn-stop entry; otherwise the band copy and the stop entry.
//   E. the STEP_TURN_RUN arm: the class ladder 0/1/2 from the same ground speed, then the
//      band copy and the stop entry.
//   F. the re-plan arm: the band word and the target word both below 1, with the hit count
//      and kind gates, re-plans through the GOTO_RUN entry.
//   G. the clip-family mid-tick: motions 0xbe4..0xbe9 compare the signs of the hit-signed
//      value and word 5, and their share is 75% of the clip length.
//   H. the bearing arm (the hit end at or before the live frame, or the mid-tick with the
//      11.25-degree gate): the reorient write, the heading turned by the clamped allowance
//      (180, or 90 when the hit-signed value is zero; 135 and 22.5 from the ground-speed
//      ladder), and the record's 0x18 word.
//
// The hit ticks (FUN_1406b1f00 / b1d00 / b1c70 / af620 / af8e0) come from the live motion's
// record through ReadClipHits. The class/speed block at the function's tail (the FUN_140a48a10
// record chain, FUN_1406ea340, FUN_1406ebf30) is not decoded for this port; the arm that
// reads it stands the value at PES's no-record answer and names the site.
// ---------------------------------------------------------------------------------------------
// PES's shared band block (FUN_1406bb570 lines 224-226 and 335-336, then LAB_1406bcbda): the dwell and style words clear,
// the foot flag is written, then RunUpdateArm sets the band and issues the clip.
void ShareBandBlock(MoveHost& host, MoveReq& req, const RunTables& t, const RunConstants& k) {
  req.SetF(moveword::kDwellLimit, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);
  req.SetByte(moveword::kStyle, 0);
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(host.FootBand() - 1u < 2u));
  RunUpdateArm(host, req, t, GroundSpeedKmh(host, k));
}

// PES's TURN_STOP speed ladder (FUN_1406bb570 lines 289-311 and 395-399): under kmh8 the class is 0 at 5 km/h; under
// kmh1475 it is 1 at 11 km/h; otherwise it is 2 at 16 km/h, unless the speed is 19 or more, where the band already in the
// request stays (the class -1 arm presets it to 3 at line 284). Every arm enters TURN_STOP.
void TurnStopLadder(MoveHost& host, MoveReq& req, float speedKmh, const RunConstants& k) {
  if (speedKmh < k.kmh8) {
    req.SetU(moveword::kRateBand, 0u);
    req.SetF(moveword::kStopSpeedWord, k.kmh5);
    EnterTurnStop(host, req);
    return;
  }
  if (speedKmh < k.kmh1475) {
    req.SetU(moveword::kRateBand, 1u);
    req.SetF(moveword::kStopSpeedWord, k.kmh11);
    EnterTurnStop(host, req);
    return;
  }
  const bool above = k.kmh19 < speedKmh;   // bVar22 of line 304
  const bool equal = k.kmh19 == speedKmh;  // bVar23 of line 303
  if (!above && !equal) {
    req.SetU(moveword::kRateBand, 2u);
    req.SetF(moveword::kStopSpeedWord, k.kmh16);
  }
  EnterTurnStop(host, req);
}

// PES's blend-point exit (FUN_1406bb570 lines 666-717, then the band-3 clamp 718-745). The dwell words, style and foot flag
// clear; the 4-tick speed runs the rate ladder when it is at most 16 km/h or a flag is set; the band and speed word come from
// the interface (vt+0x68, vt+0x48); the run entry then requests the loop and writes the run's motion word; then the clamp.
// The 4-tick sample is FUN_141eb50b0(P, buf, 4): ring slot 0 at P+0x2fc (RingSample).
void ClipEndExit(MoveHost& host, MoveReq& req, const RunConstants& k, const MoveConstants& m, float hitSigned, float hit2,
                 int liveFrame, bool flagged) {
  req.SetF(moveword::kDwellLimit, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);
  req.SetByte(moveword::kStyle, 0);
  req.SetByte(moveword::kFootFlag, static_cast<uint8_t>(host.FootBand() - 1u < 2u));
  float sample[3] = {0.0f, 0.0f, 0.0f};
  host.RingSample(static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.speed4 / k.kmh60 + k.half), sample);
  const float speed = static_cast<float>(host.TicksPerSecond()) * k.kmh60 * k.kmh60 / k.kmh1000 *
                      std::sqrt(sample[0] * sample[0] + sample[1] * sample[1] + sample[2] * sample[2]);
  if (flagged || speed <= k.kmh16) {
    // PES 685-709: the rate ladder over the thresholds, shifted by the three-unit when the last hit is behind the frame.
    float lowA = k.kmh135;
    float lowB = k.kmh8;
    if (hitSigned == 0.0f) {
      lowA = k.kmh11;
      lowB = k.kmh5;
    }
    const float shift = static_cast<int>(hit2) < liveFrame ? m.dist3 : 0.0f;
    float rate = k.rate06;
    if (lowB - shift <= speed) {
      if (lowA - shift <= speed) {
        if (k.kmh22 - shift < speed) rate = host.RateCeiling() * k.rate095;
      } else {
        rate = 0.3f;  // PES's literal 0x3e99999a
      }
    } else {
      rate = 0.15f;  // PES's literal 0x3e19999a
    }
    req.SetF(moveword::kRate, rate);
    // PES 710-714: the band and the speed word come from the interface (vt+0x68 and vt+0x48 on +0xac0), with the physique
    // call (FUN_141af3180, P+0xa08) between them. The speed word is km/h times DAT_14259ce38 (speed3600, 3.6).
    req.SetU(moveword::kRateBand, static_cast<uint32_t>(host.CommandSpeedClass()));
    host.PhysiqueG();
    req.SetF(moveword::kStopSpeedWord, host.CommandSpeedKmh() * k.speed3600);
  }
  // PES 716-717: vt+0 of the +0xac0 interface is the run entry (FUN_1406d2030, EnterRun), called with the request block,
  // the foot band (vt+0x78) and 0 (CALL [RAX] at 0x1406bca77). It picks the next run motion, writes its motion word and
  // requests RUN_LOOP; the loop's handler issues the clip on a later tick (its style arm sets the due byte).
  EnterRun(host, req, host.FootBand(), false);
  // PES 718-745: the band-3 rate clamp, still inside PES's exit block. The rate word is a float: the raw asm stores it with
  // movss after the vt+0x38 getter, so the decompile's (int) stores are bit copies. ponytail: the loop issues the clip on a
  // later tick, after this clamp; the adapter sends the rate at tick end, so the clamped value is the one sent.
  if (req.U(moveword::kRateBand) == 3u && speed < k.kmh28 && k.kmh16 < speed) {
    const float ceiling = host.RateCeiling();  // PES 719, 727-739: vt+0x18 on +0xac0
    float rate = (ceiling + k.rate06) * k.half;  // PES 720-721
    const int motionNow = host.CurrentMotion();
    if (motionNow == 0xbde || motionNow == 0xbe1) {  // PES 722-738
      if (speed <= k.kmh25) {
        if (k.kmh22 < speed) rate = (ceiling + ceiling + ceiling + k.rate06 + k.rate06) / k.kmh5;  // PES 726-730 (fVar28 is 22)
      } else {
        rate = (ceiling + ceiling + k.rate06) / m.dist3;  // PES 733-736
      }
    }
    // PES 739-744: zero up to the ceiling; above the ceiling the ceiling stands.
    float out = ceiling;
    if (rate <= ceiling) {
      out = 0.0f;
      if (0.0f <= rate) out = rate;
    }
    req.SetF(moveword::kRate, out);
  }
}

void Accelerate(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const MoveConstants& m = host.MoveConstantsView();
  const int liveFrame = host.CurrentClipFrames();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int band = static_cast<int>(req.U(moveword::kRateBand));
  const int motion = host.CurrentMotion();
  // PES 53-59: the record's F2468, read as one when it is under DAT_1425ab120 (ramp85).
  const float f2468 = host.RecordF2468() < k.ramp85 ? k.one : host.RecordF2468();
  // ponytail: PES's motion 0xbe0 block (lines 92-138, its own clip re-issue) is not ported; no 0xbe0 clip is in the flow.
  const ClipHits clip = ReadClipHits(host, motion, k, host.ClipFlag());
  const int hitTotal = clip.total;
  const float hitEnd = static_cast<float>(clip.end);
  const float hitSigned = clip.signedLen;
  const float hit0 = static_cast<float>(clip.hit[0]);
  const float hit1 = static_cast<float>(clip.hit[1]);
  const float hit2 = static_cast<float>(clip.hit[2]);
  const int hitCount = clip.cancelCount;
  // PES 139-147: an unsigned clip reads its remaining frames from hit0 with no total. The remaining frames are the smaller
  // of that and the blend, compared as ints (PES's local_res20 after the min is fVar27).
  int total = hitTotal;                                  // uVar10
  float remain = hitEnd;                                 // local_res20, the clip's end (FUN_1406b1d00)
  if (hitSigned == 0.0f) {
    remain = hit0;
    total = 0;
  }
  if (static_cast<int>(clip.blend) < static_cast<int>(remain)) remain = static_cast<float>(clip.blend);
  const int remainFrames = static_cast<int>(remain);

  // PES 148-194: the turn-lock block. Its outer gate: the re-orient object's flag clear (FUN_141eb3320 of P+0x4d0, held clear as
  // ponytail: the re-orient object is the foundation's), the clip past its first hit tick (total <= live), the live frame in the
  // first half of the remainder ((remain + total) / 2), and a signed hit.
  const bool reorientClear = true;
  if (reorientClear && total <= liveFrame && liveFrame <= (remainFrames + total) / 2 && hitSigned != 0.0f) {
    // PES 155-156: the stamp (word 0x18) is the base, and the ticks are (remain - live) / F2468 + 0.5, truncated.
    const float stamp = req.F(moveword::kRecordStampB);
    const float heading = req.F(moveword::kHeading);
    const int ticks = static_cast<int>(static_cast<float>(remainFrames - liveFrame) / f2468 + k.half);
    // PES 157-163: the turn from the stamp is FUN_14043a0a0(stamp, heading), the heading minus the stamp. Under 90 degrees of it
    // the lock heading is FUN_140a92950's step: the stamp moves toward the heading by at most 22.5 degrees (DAT_14259d618), then
    // folds. At 90 degrees or more the lock heading is the stamp itself.
    const float turnFromStamp = PesAngleDiff(stamp, heading, k);
    float lockHeading = stamp;
    if (std::fabs(turnFromStamp) < k.deg90) lockHeading = StepHeading(stamp, heading, k.deg225, k);
    // PES 164-167: the facing error is FUN_14043a0a0(facing, lock), the lock heading minus the facing. Its size per tick must be
    // under 45 degrees.
    const float facingError = PesAngleDiff(req.F(moveword::kFacing), lockHeading, k);
    if (std::fabs(facingError) / static_cast<float>(ticks) < k.deg45) {
      req.SetF(moveword::kAngle3d, lockHeading);                      // PES 168: param_2[0x3d]
      req.SetByte(moveword::kReorientFlag, 1);                        // PES 169: the byte at 0x3c
      req.SetU(moveword::kTurnFrames, static_cast<uint32_t>(ticks));  // PES 170: param_2[0x3e]
      req.SetByte(moveword::kMirrorFlag, 0);                          // PES 171: the byte at 0x3f
      req.SetF(moveword::kRecordStampB, lockHeading);                 // PES 172: param_2[0x18]
      // PES 173-190: past a 90-degree offset, the byte at 0x3f is set when the offset's sign differs from the facing error's and
      // the error passes 22.5 degrees.
      if (k.deg90 < std::fabs(hitSigned)) {
        const float signHit = hitSigned < 0.0f ? k.neg : k.one;
        const float signError = facingError < 0.0f ? k.neg : k.one;
        if (signHit != signError && k.deg225 < std::fabs(facingError)) req.SetByte(moveword::kMirrorFlag, 1);
      }
      // PES 192: FUN_1406cbb00, the yaw lock: P+0xbec takes the folded lock heading.
      host.SetRecordF0bec(FoldFull(lockHeading, k));
    }
  }

  // The bearings (FUN_14043a0a0: word 0x18 against word 1 for the turn, against word 3 for the look) and bVar23, read after the
  // turn-lock block: PES's bearings at 195-199 read the stamp the lock wrote.
  const float turnBearing = PesAngleDiff(req.F(moveword::kRecordStampB), req.F(moveword::kHeading), k);
  const float lookBearing = PesAngleDiff(req.F(moveword::kRecordStampB), req.F(moveword::kLookHeading), k);
  const float turnAbs = std::fabs(turnBearing);
  // PES 267-271 (VA 0x6bbdc8): fVar25 is the body error, word 2 (the facing) against word 4 (the body direction).
  const float bodyErr = std::fabs(PesAngleDiff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k));
  const bool fastTurn = !(lookBearing * lookBearing < k.kmh95sq) && req.U(moveword::kPrecise) == 1u;

  // PES 206-222: the predicate block and its frame gate. The drawn tick is ticks * 6 / 60 + 0.5 (DAT_14259bdec, DAT_14259a764,
  // DAT_142565600), where ticks is FUN_141e532d0 (the match's ticks per second, the operand GroundSpeedKmh reads).
  // ponytail: the motion's two predicates (FUN_140a6e290 / FUN_140a6e2e0) are held at their clear answers.
  const float drawn = static_cast<float>(host.TicksPerSecond()) * k.ramp6 / k.kmh60 + k.half;
  const int drawnTick = static_cast<int>(drawn);
  const bool predicatesClear = true;
  const bool frameGate = (liveFrame <= total + 1 && liveFrame < drawnTick) ||
                         static_cast<float>(liveFrame) < static_cast<float>(static_cast<int>(hit0)) * k.frameGate01 ||
                         (liveFrame < drawnTick && motion == 0xbea);
  if (req.U(moveword::kCommandId) != 0x11u &&
      (host.AnimeState() != 0x4a || req.U(moveword::kCommandId) != 0x72u) &&
      predicatesClear && frameGate) {
    if (k.deg5625 <= turnAbs || fastTurn) {
      // PES 223-232: the turn arm (goto LAB_1406bcbda).
      ShareBandBlock(host, req, t, k);
      return;
    }
    // PES 234-256: the RUN_TURN_RUN arm for command class 3 with a band under 2 and a look turn past 90 degrees.
    if (k.deg90 < std::fabs(req.F(moveword::kLookDelta)) && band < 2 && cls == 3) {
      const float ground = GroundSpeedKmh(host, k);
      int bandWord = 2;
      float speedWord = k.kmh16;
      if (ground < k.kmh5) {
        bandWord = 0;
        speedWord = k.kmh5;
      } else if (ground < k.kmh11) {
        bandWord = 1;
        speedWord = k.kmh11;
      }
      req.SetU(moveword::kRateBand, static_cast<uint32_t>(bandWord));
      req.SetF(moveword::kStopSpeedWord, speedWord);
      EnterRunTurnRun(host, req);
      return;
    }
  }

  // PES 261-266: the clip has not reached hit0, outside the three motions that pass.
  if (liveFrame < static_cast<int>(hit0) && motion != 0xbde && motion != 0xbe1 && motion != 0xbdf) return;

  // PES 267-274: bVar22 is the remaining frames passed, or the body error under 45 degrees (fVar25, VA 0x6bbdc8; fVar26 holds 45).
  const bool bVar22 = remainFrames <= liveFrame || bodyErr < k.deg45;

  // PES 275-312: the command class -1 block. It stops when the arm has passed or no band is set; otherwise it turns stop.
  if (cls == -1) {
    if (!bVar22 || band < 1) {
      req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
      req.SetU(moveword::kMoveBucket,
               static_cast<uint32_t>(AngleBucket(host.AngleBuckets(), req.F(moveword::kHeading), 10, false)));
      EnterStop(host, req);
      return;
    }
    req.SetU(moveword::kRateBand, 3u);
    req.SetF(moveword::kStopSpeedWord, k.kmh28);
    TurnStopLadder(host, req, GroundSpeedKmh(host, k), k);
    return;
  }

  // PES 313-327: theta is the angle threshold from the look distance (word 0x12): 45 degrees plus 22.5 times the ramp.
  // With no signed offset it is 78.75, or 56.25 once the blend has passed (fVar24 == 0.0).
  const float lookDist = req.F(moveword::kDistToLook);
  const float ramp = k.speed2 < lookDist ? (lookDist < k.speed4 ? (lookDist - k.speed2) * k.half : k.one) : 0.0f;
  float theta = ramp * k.deg225 + k.deg45;
  if (hitSigned == 0.0f) {
    theta = k.deg7875;
    if (static_cast<int>(hit2) <= liveFrame) theta = k.deg5625;
  }
  if (theta <= turnAbs || fastTurn) {
    if (!bVar22) {
      // PES 402-429: the stop entry. Its class comes from the ground speed (PES 403-407: the tick rate times 60 squared over
      // 1000, times the displacement's length, which is GroundSpeedKmh).
      const float stopSpeed = GroundSpeedKmh(host, k);
      if (k.kmh5 <= stopSpeed) {
        const bool two = k.kmh11 <= stopSpeed;
        req.SetU(moveword::kRateBand, two ? 2u : 1u);
        req.SetF(moveword::kStopSpeedWord, two ? k.kmh16 : k.kmh11);
      } else {
        req.SetU(moveword::kRateBand, 0u);
        req.SetF(moveword::kStopSpeedWord, k.kmh5);
      }
      // PES 422: the step-turn entry (FUN_1406d38d0) takes the tick first.
      if (EnterStepTurnRun(host, req)) return;
      req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
      req.SetU(moveword::kMoveBucket,
               static_cast<uint32_t>(AngleBucket(host.AngleBuckets(), req.F(moveword::kHeading), 10, false)));
      // ponytail: PES falls into its tail (lines 437-747) after this entry; the tail is not ported, so the arm returns here.
      EnterStop(host, req);
      return;
    }
    // PES 328-401: the bVar22 arm. A precise command outside the arrival band takes the shared band block.
    if (req.U(moveword::kPrecise) != 1u ||
        ((m.dist3 <= lookDist || k.deg1125b <= std::fabs(req.F(moveword::kTurn))) && k.one <= lookDist)) {
      ShareBandBlock(host, req, t, k);
      return;
    }
    // PES 376-400: the rate by speed, the RUN_TURN_STEP entry at (ceiling - 0.6) / 2, then the TURN_STOP ladder.
    const float speed = GroundSpeedKmh(host, k);
    const float stepThreshold = (host.RateCeiling() - k.rate06) * k.half;
    req.SetF(moveword::kRate, k.rate06);
    if (k.kmh5 <= speed) {
      if (speed < k.kmh11) req.SetF(moveword::kRate, 0.3f);  // PES's literal 0x3e99999a
    } else {
      req.SetF(moveword::kRate, 0.15f);  // PES's literal 0x3e19999a
    }
    if (EnterRunTurnStep(host, req, stepThreshold)) return;
    if (theta <= turnAbs) {
      TurnStopLadder(host, req, speed, k);
      return;
    }
  } else if (band < 1 && static_cast<int>(req.U(moveword::kBandTarget)) < 1 &&
             ((cls > 1 && kind != 3 && kind != 6) || cls > 2)) {
    // PES 432-436: the GOTO_RUN entry, tested on the command class (line 275 set iVar11 to it, not the cancel count).
    EnterGotoRun(host, req, false);
    return;
  }

  // PES 437-456: the mid tick (fVar30) is (remain + total) / 2. The 0xbe4-0xbe9 family takes the remain instead, and 75% of it
  // when the hit's sign and the look delta's sign differ. The signs are PES's fVar24 and word 5 (VA 0x6bc3b5 onward).
  const float signHit = hitSigned < 0.0f ? k.neg : k.one;
  const float signLook = (req.F(moveword::kLookDelta) > 0.0f || req.F(moveword::kLookDelta) == 0.0f) ? k.one : k.neg;
  int mid = (remainFrames + total) / 2;
  if (static_cast<unsigned>(motion) - 0xbe4u < 6u) {
    mid = remainFrames;
    if (signHit != signLook) mid = (remainFrames * 75) / 100;
  }

  // PES 457-459: the trigger. Clause 1: the remain has passed. Clause 2: the mid has passed, the turn is past 11.25 degrees
  // (fVar27) and the body error is under 22.5 (fVar25). Clause 3: the hit carries no offset.
  const bool trigger = remainFrames <= liveFrame ||
                       (mid <= liveFrame && k.deg1125 < turnAbs && bodyErr < k.deg225) || hitSigned == 0.0f;
  if (trigger) {
    // PES 460 (VA 0x6bc2cb): the re-orient byte at word 0x3c clears. PES 461-462, vt+0x08 on P+0x4d0, is the AdjustAngle
    // apply: a no-op stand-in here (movestep_turn.cpp, AdjustAngleApply).
    req.SetByte(moveword::kReorientFlag, 0);
    // PES 466-484: the player's facing (P+0x20) and the heading (word 1), each folded into [0, 360); the delta is their
    // difference.
    const float facing = FoldFull(host.FacingDeg(), k);
    float delta = FoldFull(req.F(moveword::kHeading), k) - facing;
    // PES 487-495: a delta past 180 degrees in either direction wraps once by 360 (the test is on the magnitude).
    if (k.half_turn < std::fabs(delta)) delta -= (delta < 0.0f ? k.neg : k.one) * k.full;
    // PES 497-498: the yaw step runs while the wrapped delta is under 112.5 degrees.
    if (std::fabs(delta) < k.deg1125b) {
      // PES 499-505: the cap (fVar28) is 135 with no hit offset, else 180; the ground speed (fVar29) is in km/h.
      const float cap = hitSigned == 0.0f ? k.deg135 : k.half_turn;
      const float ground = GroundSpeedKmh(host, k);
      // PES 507-513: the rate (fVar34) is the cap while the hit tick is ahead or the ground speed is under 19 km/h, else 67.5;
      // it is 135 when the remain and hit1 have passed and the ground speed is past 14.75 km/h.
      const bool armed = liveFrame < static_cast<int>(hit2) || ground <= k.kmh19;
      float rate = armed ? cap : k.deg675;
      if (armed && remainFrames <= liveFrame && static_cast<int>(hit1) <= liveFrame && k.kmh1475 < ground) {
        rate = k.deg135;
      }
      // PES 514-526: the anime predicates (FUN_140a6e290 / FUN_140a6e2e0) are held clear, as the port's other arms hold them.
      // ponytail: with them set, the rate is 22.5 degrees while the live frame is short of the remain.
      const bool motionPredicates = false;
      if (motionPredicates && liveFrame < remainFrames) rate = k.deg225;
      // PES 527-539: while the hit tick is ahead and the hit offset is signed, a sign mismatch with the look delta damps the rate
      // to a quarter (DAT_142592228).
      if (liveFrame < static_cast<int>(hit1) && hitSigned != 0.0f && signHit != signLook) {
        rate *= host.MoveTurnConstantsView().quarter;
      }
      // PES 540-546: the rate per tick, and the step clamped to the delta.
      rate /= static_cast<float>(host.TicksPerSecond());
      const float step = (delta <= rate && -rate <= delta) ? delta : (delta > rate ? rate : -rate);
      // PES 548-556: the heading, folded into [0, 360) once.
      float heading = facing + step;
      if (0.0f <= heading) {
        if (k.full <= heading) heading += k.wrapFull;
      } else {
        heading += k.full;
      }
      // PES 557-569: the root-facing flag and angle; the stamp word 0x18 once the remain has passed; P+0xbec.
      req.SetByte(moveword::kRootFacing, 1);
      req.SetF(moveword::kRootFacingAngle, heading);
      if (remainFrames <= liveFrame) req.SetF(moveword::kRecordStampB, heading);
      host.SetRecordF0bec(heading);
    }
  }
  // PES 572-609: the flag cVar7. After the body, PES's fVar30 is local_res8 on every path (the body writes it at 496, and
  // the skipped path writes it at 459): hit1, or the blend. PES's local_104 is hit0, or the blend. The arm needs the body
  // error under 45 degrees (fVar25, VA 0x6bbdc8); the command class (word 9) and the band (word 0x1f) are read as they stand.
  bool flag = false;
  const bool motionEnd = motion == 0xbde || motion == 0xbe1;
  if ((static_cast<int>(hit1) <= liveFrame || (motionEnd && static_cast<int>(hit0) <= liveFrame)) &&
      bodyErr < k.deg45) {
    const int cmdClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    const int bandNow = static_cast<int>(req.U(moveword::kRateBand));
    if (cmdClass < bandNow) {
      flag = true;  // PES 580-581
    } else {
      if (bandNow < 1 && 1 < cmdClass) {  // PES 584-588: the GOTO_RUN exit
        req.SetU(moveword::kRateBand, 0u);
        req.SetF(moveword::kStopSpeedWord, 5.0f);  // PES's literal 0x40a00000
        EnterGotoRun(host, req, false);
        return;
      }
      const float span = 1 < bandNow ? k.deg225 : k.deg45;  // PES 590-593 (fVar36)
      if (bandNow < cmdClass) {                              // PES 594-605
        // ponytail: FUN_140a6e290 and FUN_140a6e2e0 (the motion predicates) are held clear, as predicatesClear is above.
        if (std::fabs(req.F(moveword::kTurn)) < span || k.deg675 < turnAbs) flag = true;  // PES 599-604: fVar27 is |turn|
      }
    }
  }

  // PES 610-665: the record checks set bVar23 only inside the +0x661 branch (PES 653-658). ponytail: +0x661 is p+0xD1D copied
  // from A+0x729 each frame (S1Watch: 2,711/2,711 frames). The port holds it at false: the byte's writers FUN_1408C2760,
  // FUN_1408C31C0 and FUN_1408C3D10 are undecoded, and the roster map (row to GF player; the class-5 test is PassGetReceiver()
  // == player) and P+0x2458 are not in the MoveHost contract (movestep_run.cpp:231), so this block cannot set bVar23 yet.
  const bool bVar23 = false;

  // PES 668: the blend-point exit. local_res18 is the blend; the flags are cVar7 and bVar23.
  if (static_cast<int>(clip.blend) <= liveFrame || flag || bVar23) {
    ClipEndExit(host, req, k, m, hitSigned, hit2, liveFrame, flag || bVar23);
    return;
  }
}


// ---------------------------------------------------------------------------------------------
// FUN_1406bcd20, the DECELERATE handler (mv5.a.c:3812). One tick of the slow-down between
// ---------------------------------------------------------------------------------------------
// FUN_1406bcd20, the DECELERATE handler (mv5.a.c:3812). One tick of the slow-down between two
// running states. Its decompile's flow, in PES's own order:
//
//   A. the yaw-lock block: the re-orient object's flag clear, the hit tick at or before the
//      live frame and the live frame at or before the mid-tick. The distance is the hit-mid
//      over the F2468, the desired heading is the request's word 0x18 when its bearing is
//      under 90 degrees, the per-tick turn is the second bearing over the distance, and under
//      45 degrees per tick the lock words 0x3d/0x3e/0x3f are written and FUN_1406cbb00 stores
//      the yaw.
//   B. the pre-empt gates (ACCELERATE's shape, with the two 0xc3a/0xc3b motions as the extra
//      flag): the first bearing past 56.25 degrees, or the fast-turn flag, clears the dwell
//      words and reaches the shared band/speed block. Otherwise, when the bearing is under
//      56.25 degrees and the class is not 3 (or word 5 is past 11.25 degrees), PES jumps to
//      LAB_1406bd3a0 and the shared block is not reached at all.
//   C. the class -1 arm: the fast-turn flag with a positive band takes band 3 with 28 km/h,
//      the ground-speed ladder 8 / 14.75 / 19 and the turn-stop entry; otherwise the band
//      copy and the stop entry.
//   D. the allowance arm: the look distance over 2 m grows the allowance by 22.5 per unit to
//      a floor of 45 degrees, and that allowance past the second bearing, or the fast-turn
//      flag, takes the turn-stop entry; otherwise (the precise word set with a look distance
//      under 3 m and the turn under 112.5 degrees, or the look distance under the ground
//      speed) the STEP_TURN_RUN entry's threshold and the band/speed block.
//   E. the bearing arm (the hit end at or before the live frame, or the mid-tick with the
//      11.25-degree gate): the reorient write and the yaw store, with the 180/90 allowance and
//      the 22.5 / 0.25 clamps.
//   F. the state arm: the first bearing under 45 degrees (or the 0xc2b motion) with the class
//      below 3, the band below 1 and word 8 above 1 re-plans into GOTO_RUN; otherwise the
//      speed ladder (5 / 11 / 22) and the run entry.
//
// The shared band/speed block is PES's LAB_1406bcbda, the same three +0xac0 interface reads
// and run entry STOP's update arm uses (see RunUpdateArm above). The +0x4d0 interface's flag
// stands as ACCELERATE's does.
// ---------------------------------------------------------------------------------------------
void Decelerate(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const int liveFrame = host.CurrentClipFrames();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int band = static_cast<int>(req.U(moveword::kRateBand));
  const int motion = host.CurrentMotion();
  // PES 0x1406BCD20 (line 46): P+0x2468 under 0.85 (0x1425AB120) reads 1.0 (0x14256AE1C).
  const float f2468 = host.RecordF2468() < k.ramp85 ? k.one : host.RecordF2468();
  // The hit ticks come from the live motion's record (ReadClipHits), as in ACCELERATE.
  const ClipHits clip = ReadClipHits(host, motion, k, host.ClipFlag());
  int hitTotal = clip.total;
  float hitEnd = static_cast<float>(clip.end);
  const float hitSigned = clip.signedLen;
  const float hit0 = static_cast<float>(clip.hit[0]);
  const float hit1 = static_cast<float>(clip.hit[1]);
  const bool slowMotion = motion == 0xc3a || motion == 0xc3b;

  // The shared band / speed block (PES's LAB_1406bcbda): the dwell words cleared, the
  // class/rate/band triple and the run entry. PES reaches it through a goto from two arms.
  const auto sharedBandBlock = [&]() {
    const float ground = GroundSpeedKmh(host, k);
    const int foot = host.FootBand();
    req.SetU(moveword::kLiveKind, static_cast<uint32_t>(foot));
    req.SetF(moveword::kDwellLimit, 0.0f);
    req.SetF(moveword::kDwellLimit2, 0.0f);
    req.SetByte(moveword::kStyle, 0);
    int speed = 0;
    if (k.kmh5 <= ground) speed = (k.kmh11 <= ground) ? 2 : 1;
    req.SetU(moveword::kRateBand, static_cast<uint32_t>(speed));
    req.SetF(moveword::kSpeedBand, speed == 0 ? k.kmh5 : (speed == 1 ? k.kmh11 : k.kmh16));
    const int speedClamped = speed < 3 ? (speed < 0 ? 0 : speed) : 2;
    req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(speedClamped));
    const float rate = RequestedRate(host, req, t);
    req.SetF(moveword::kRate, rate);
    const int rateCls = ClassForRateFor(rate, t);
    req.SetU(moveword::kRateBand, static_cast<uint32_t>(rateCls));
    req.SetF(moveword::kSpeedBand, RateToSpeedFor(rate, host.PhysiqueG(), t, k.rampSlope) * k.speed3600);
    // Slot 0 of the +0xac0 interface is FUN_1406d2030 (group B's run entry).
    movestep::EnterRun(host, req, foot, false);
  };

  // Arm A: the yaw-lock block.
  if (hitSigned == 0.0f) {
    hitTotal = 0;
    hitEnd = hit0;
  }
  if (static_cast<int>(hit0) < static_cast<int>(hitEnd)) hitEnd = hit0;
  // ponytail: the +0x4d0 interface's flag (P-KIN) is what clears this arm.
  const bool reorientClear = true;
  const float distance = (hitEnd - static_cast<float>(liveFrame)) / f2468 + k.half;
  const float firstBearing = InlineDiffDeg(req.F(moveword::kHeading), req.F(moveword::kFacing), k);
  if (reorientClear && hitTotal <= liveFrame && liveFrame <= (static_cast<int>(hitEnd) + hitTotal) / 2) {
    // PES's turn-lock, as in ACCELERATE: the stamp steps toward the heading by at most 22.5 degrees when the turn from the
    // stamp is under 90; the lock is the stamp itself otherwise.
    float desired = req.F(moveword::kRecordStampB);
    if (std::fabs(PesAngleDiff(req.F(moveword::kRecordStampB), req.F(moveword::kHeading), k)) < k.deg90) {
      desired = StepHeading(req.F(moveword::kRecordStampB), req.F(moveword::kHeading), k.deg225, k);
    }
    // PES: FUN_14043a0a0(facing, lock) with a magnitude gate.
    const float secondBearing = std::fabs(PesAngleDiff(req.F(moveword::kFacing), desired, k));
    const float perTick = secondBearing / distance;
    if (perTick < k.deg45) {
      req.SetU(moveword::kTurnFrames, static_cast<uint32_t>(distance));
      req.SetByte(moveword::kReorientFlag, 1);
      req.SetF(moveword::kAngle3d, desired);
      req.SetByte(moveword::kMirrorFlag, 0);
      req.SetF(moveword::kRecordStampC, desired);
      if (k.deg90 < hitSigned) {
        const float a = hitSigned < 0.0f ? k.neg : k.one;
        const float b = perTick < 0.0f ? k.neg : k.one;
        if (a != b) {
          uint8_t dir = 0;
          if (k.deg225 < perTick) dir = 1;
          req.SetByte(moveword::kMirrorFlag, dir);
        }
      }
      host.SetRecordF0bec(desired);
    }
  }

  // Arm B's measurements.
  const float firstBearingA = PesAngleDiff(req.F(moveword::kRecordStampB), req.F(moveword::kLookHeading), k);
  // INF: the PES threshold on a look bearing is not located (sign table row 1855), so this stays the inline difference.
  const float secondBearingA = InlineDiffDeg(req.F(moveword::kHeading), req.F(moveword::kLookHeading), k);
  const bool fastTurn =
      !(firstBearingA * firstBearingA < k.kmh95sq) && req.U(moveword::kPrecise) == 1u;
  const int drawnTick =
      static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.ramp6 / k.kmh60 + k.half);
  const bool frameGate = (hitTotal + 1 <= liveFrame && liveFrame < drawnTick) ||
                         static_cast<float>(liveFrame) < static_cast<float>(hit0) * k.rate01;
  // ponytail: FUN_140a6e290/FUN_140a6e2e0 are the anime player's own; held at "clear".
  const bool motionPredicatesClear = true;
  const bool preemptGate =
      req.U(moveword::kCommandId) != 0x11u &&
      (host.AnimeState() != 0x4a || req.U(moveword::kCommandId) != 0x72u) && !slowMotion &&
      motionPredicatesClear && frameGate;
  if (preemptGate) {
    if (k.deg5625 <= firstBearing || fastTurn) {
      sharedBandBlock();
      return;
    }
    if (firstBearing < k.deg5625 && (cls != 3 || k.deg1125 <= req.F(moveword::kLookDelta))) {
      // LAB_1406bd3a0: the small-turn path, which skips the shared block entirely.
      if (k.deg90 < req.F(moveword::kLookDelta) && band < 2 && cls == 3) {
        const float ground = GroundSpeedKmh(host, k);
        int speed = 0;
        if (k.kmh5 <= ground) speed = (k.kmh11 <= ground) ? 2 : 1;
        req.SetU(moveword::kRateBand, static_cast<uint32_t>(speed));
        req.SetF(moveword::kSpeedBand, speed == 0 ? k.kmh5 : (speed == 1 ? k.kmh11 : k.kmh16));
        EnterRunTurnRun(host, req);
        return;
      }
    } else {
      sharedBandBlock();
      return;
    }
  }

  // The LAB_1406bd3a0 block: the class -1 arm, the allowance arm and the bearing arm.
  bool b3 = hitEnd <= static_cast<float>(liveFrame) || secondBearingA < k.deg45;
  if (slowMotion && host.MatchPhase() != 1 && liveFrame < static_cast<int>(hit0)) {
    // ponytail: MI+0x116c (the match phase, MB-E) is not ported; the port holds it at 1, so
    // the early return PES writes for the out-of-play phase does not fire.
    return;
  }
  if (cls == -1) {
    if (b3 && 0 < band) {
      req.SetU(moveword::kRateBand, 3u);
      req.SetF(moveword::kSpeedBand, k.kmh28);
      const float ground = GroundSpeedKmh(host, k);
      if (ground < k.kmh8) {
        req.SetU(moveword::kRateBand, 0u);
        req.SetF(moveword::kSpeedBand, k.kmh5);
        EnterTurnStop(host, req);
        return;
      }
      if (ground < k.kmh1475) {
        req.SetU(moveword::kRateBand, 1u);
        req.SetF(moveword::kSpeedBand, k.kmh11);
        EnterTurnStop(host, req);
        return;
      }
      const bool b17 = ground == k.kmh19;
      const bool b18 = k.kmh19 < ground;
      if (!b17 && !b18) {
        req.SetU(moveword::kRateBand, 2u);
        req.SetF(moveword::kSpeedBand, k.kmh16);
      }
      EnterTurnStop(host, req);
      return;
    }
    req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
    const int bucket = static_cast<int>(req.U(moveword::kMoveBucket));
    req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));
    req.SetF(moveword::kRate, k.rate06);
    EnterStop(host, req);
    return;
  }
  if (hitSigned != 0.0f) {
    if (liveFrame < hit0) return;
    if (slowMotion && liveFrame < hit1) return;
  }

  // Arm D: the allowance arm.
  const float lookDist = req.F(moveword::kDistToLook);
  float allowance = 0.0f;
  if (k.speed2 < lookDist) {
    allowance = (lookDist < k.speed4) ? (lookDist - k.speed2) * k.half : k.one;
  }
  if (allowance * k.deg225 + k.deg45 <= secondBearingA || fastTurn) {
    if (!fastTurn) {
      if (req.U(moveword::kPrecise) == 1u &&
          ((lookDist < k.quota3 &&
            InlineDiffDeg(req.F(moveword::kTurn), req.F(moveword::kFacing), k) < k.deg1125b) ||
           lookDist < GroundSpeedKmh(host, k))) {
        // The precise arm's threshold: (rate ceiling - 0.6) / 2 from the +0xac0 interface.
        const float ceiling = host.RateCeiling();
        if (EnterRunTurnStep(host, req, (ceiling - k.rate06) * k.half)) return;
      }
      const float ground = GroundSpeedKmh(host, k);
      int speed = 0;
      if (k.kmh5 <= ground) speed = (k.kmh11 <= ground) ? 2 : 1;
      req.SetU(moveword::kRateBand, static_cast<uint32_t>(speed));
      req.SetF(moveword::kSpeedBand, speed == 0 ? k.kmh5 : (speed == 1 ? k.kmh11 : k.kmh16));
      if (EnterStepTurnRun(host, req)) return;
    }
    req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
    const int bucket = static_cast<int>(req.U(moveword::kMoveBucket));
    req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));
    EnterStop(host, req);
    return;
  }

  // Arm E: the bearing arm.
  if (hitEnd <= static_cast<float>(liveFrame) ||
      (static_cast<float>(hitEnd + hitTotal) * k.half <= static_cast<float>(liveFrame) &&
       k.deg1125 < secondBearingA)) {
    req.SetByte(moveword::kReorientFlag, 0);
    const float playerFacing = FoldFull(req.F(moveword::kFacing), k);
    float delta = FoldFull(req.F(moveword::kHeading), k) - playerFacing;
    if (k.half_turn < delta) {
      const float sign = delta < 0.0f ? k.neg : k.one;
      delta -= sign * k.full;
    }
    if (delta < k.deg90) {
      float turnAllowance = k.half_turn;
      if (hitSigned == 0.0f) turnAllowance = k.deg90;
      if (liveFrame < static_cast<int>(hit1) && hitSigned != 0.0f) {
        const float a = hitSigned < 0.0f ? k.neg : k.one;
        const float b = (req.F(moveword::kLookDelta) > 0.0f || req.F(moveword::kLookDelta) == 0.0f)
                            ? k.one
                            : k.neg;
        if (a != b) turnAllowance = turnAllowance * k.foot035;
      }
      const float turn = FoldFull(playerFacing + turnAllowance, k);
      host.SetRecordF0bec(turn);
    }
  }

  // Arm F: the state arm.
  if (static_cast<int>(hit0) <= liveFrame && (secondBearingA < k.deg45 || motion == 0xc2b)) {
    const int classWord = cls;
    if (classWord < 3) {
      if (band < 1) {
        if (classWord > 1) {
          req.SetU(moveword::kRateBand, 0u);
          req.SetF(moveword::kSpeedBand, k.kmh5);
          EnterGotoRun(host, req, false);
          return;
        }
      }
    }
  }
  const int foot = host.FootBand();
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(foot - 1u < 2u));
  req.SetF(moveword::kDwellLimit, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);
  req.SetByte(moveword::kStyle, 0);
  const float ground = GroundSpeedKmh(host, k);
  int speed = 0;
  if (k.kmh5 <= ground) {
    if (k.kmh11 <= ground) {
      speed = (k.kmh22 <= ground) ? 2 : 1;
    } else {
      speed = 1;
    }
  }
  req.SetU(moveword::kRateBand, static_cast<uint32_t>(speed));
  req.SetF(moveword::kRate, k.rate06);
  const int speedClamped = speed < 3 ? (speed < 0 ? 0 : speed) : 2;
  req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(speedClamped));
  const float rate = RequestedRate(host, req, t);
  req.SetF(moveword::kRate, rate);
  const int rateCls = ClassForRateFor(rate, t);
  req.SetU(moveword::kRateBand, static_cast<uint32_t>(rateCls));
  req.SetF(moveword::kSpeedBand, RateToSpeedFor(rate, host.PhysiqueG(), t, k.rampSlope) * k.speed3600);
  // Slot 0 of the +0xac0 interface is FUN_1406d2030 (group B's run entry).
  movestep::EnterRun(host, req, foot, false);
}

// The +0xac0 interface's slot 0x60, which every branch uses to turn an angle into a bucket:
// FUN_1406b9b20, the port's shared AngleBucket.
int SwingBucketAngle(const MoveHost& host, float angleDeg) {
  return AngleBucket(host.AngleBuckets(), angleDeg, 10, false);
}

namespace {
// RUN_LOOP's jostle override (rev2 §5.2, the block before the style block of FUN_1406c0170). When the nearest
// opponent's predicted position is close, the turn to him is past 11.25 degrees, and his facing is within 67.5
// degrees of the player's, the style is a jostle loop: 6 under 112.5 degrees of turn, 7 from 112.5 (112.5 itself
// included), and 0x10 past 112.5 with a class under 2, or from 157.5 whatever the class. `turn` returns the signed
// turn, which the change block reads for the mirror. The guard keeps the loop already on when the clip's mirror
// and the turn's side disagree; then the base style stands.
int JostleStyle(MoveHost& host, MoveReq& req, int base, float& turn) {
  const RunConstants& k = host.RunConstantsView();
  const TeamAi& team = host.Team();
  const int own = host.ActionIndex();
  const int opp = team.NearestOpponent(own);
  if (!(own < 22 && opp < 22)) return base;
  // The predicted positions, n ticks ahead: n = int(54 * 4 / 60 + 0.5).
  const int n = static_cast<int>(host.TicksPerSecond() * k.speed4 / k.kmh60 + k.half);
  float self[3], selfVel[3], other[3], otherVel[3];
  team.Position(own, self);
  team.Velocity(own, selfVel);
  team.Position(opp, other);
  team.Velocity(opp, otherVel);
  // ponytail: LIVE: JostleStyle reads mirrored coordinates; the fix flips AThrowInIsNotAwarded red, so investigate which is
  // wrong. The same GF-coords read as ProximityCap (index 1 is -PES z), with no ActionBit27 gate (see TeamAi::Position).
  const float dx = (other[0] + n * otherVel[0]) - (self[0] + n * selfVel[0]);
  const float dz = (other[1] + n * otherVel[1]) - (self[1] + n * selfVel[1]);
  // The threshold: 0.7 up to 1.0 at PC 95, 0.3 wide (the install's 0.7, 0.3, 50 and 45).
  const float share = std::min(std::max((static_cast<float>(team.Ability(own, kAbilityPc)) - k.frame50) / k.deg45, 0.0f), k.one);
  float threshold = k.rate07 + k.blend3 * share;
  if (host.AnimeState() == kReplayAnimeState) threshold = k.replay45;
  const float bearing = FoldFull(std::atan2(dx, dz) * host.StyleConstantsView().radToDeg, k);
  const float facing = req.F(moveword::kFacing);
  turn = PesAngleDiff(facing, bearing, k);
  const float face = std::fabs(PesAngleDiff(facing, team.Heading(opp), k));
  const float magnitude = std::fabs(turn);
  if (!(std::sqrt(dx * dx + dz * dz) < threshold && k.deg1125 < magnitude && face < k.deg675)) return base;
  int cand = 6;
  if (k.deg1125b <= magnitude) {
    const int commandClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    cand = (k.deg1575 <= magnitude || (k.deg1125b < magnitude && commandClass < 2)) ? 0x10 : 7;
  }
  // The guard: the loop already on keeps the base when the clip's mirror disagrees with the turn's side.
  if (cand == static_cast<int>(req.U(moveword::kStyle) & 0xffu) && host.ClipFlag() != (turn < 0.0f)) {
    return base;
  }
  return cand;
}
}  // namespace

// ---------------------------------------------------------------------------------------------
// FUN_1406c0170, the RUN_LOOP handler (mv2.a.c:6659). One tick of the running phase. Its
// decompile's running branch, in PES's own order:
//
//   1. the turn-lock re-plan: one of the words 0x3d/0x3e/0x3f set, the class above 1 (or the
//      0x3f byte), the rate word at or below 0.6 and the precise word clear, re-plans through
//      the GOTO_RUN entry;
//   2. the ACCELERATE pre-empt (FUN_1406ccd00) and the DECELERATE pre-empt (FUN_1406cdaf0);
//   3. the re-plan term: the precise word clear, the class above 2, the rate word at or below
//      0.225 (or the ground speed under 11 km/h with the turn past 78.75 degrees), the look
//      distance over 3 m and the turn past 56.25 degrees, re-plans through the GOTO_RUN
//      entry;
//   4. the RUN_TURN_STEP pre-empt: the threshold (rate ceiling - 0.6) / 2, and the
//      keeper-kind arm's turn bearing at 45 degrees instead of 56.25, with the bucket's
//      0/1/5/6 shape and the rate / look-distance gates deciding the entries;
//   5. the style block: the rate read (FUN_1406b9ff0's, with the class word 9), the style
//      (FUN_1406ba2f0's), the representative motion from the style table, the mirror masks
//      and the request words' dwell and style writes, with no state change.
//
// The class -1 / out-of-play branch (the state picker, the keeper / mode arm, the swing arm
// and the 0x662 / 0x661 rows) is the decompile's second half. PES guards it with
// `class != -1 || (precise != 1 && match phase == 1)`, which is the `running` test below.
// ponytail: that half's state picker and swing arm are the foundation's event store; the port
// keeps the running branch's own arms here and names where the other half stands.
// ---------------------------------------------------------------------------------------------
// PES 408-466: RUN_LOOP's T1-false tail (FUN_1406c0170 after its T1 block). It runs on the T1-false path only, where the T1
// gate's FUN_1406b70d0 operand has already set fVar22 = one and fVar3 = DAT_1425cca34 (67.5). The rate starts at rate06 and
// the turn ramp lowers it. Block A exits to RUN_TURN_RUN or TURN_STOP; block B exits to STOP, from the speed scaled by the
// body's cosine to the displacement (the sine table at |gap| + 90, FUN_140a92c80).
void RunLoopTail(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const MoveConstants& m = host.MoveConstantsView();
  const float one = k.one;                               // fVar22, set by the T1 gate's operand
  const float half = k.half;                             // fVar20 (PES 109)
  const float turn = std::fabs(req.F(moveword::kTurn));  // PES 408-409: |req[6]|
  const int classWord = static_cast<int>(req.U(moveword::kClassWord));

  // PES 410-422: the turn ramp over the 67.5..135 degree band lowers the rate, clamped to [blend3, rate06].
  float ramp = 0.0f;
  if (k.deg675 < turn) {
    ramp = one;
    if (turn < k.deg135) ramp = (turn - k.deg675) / k.deg675;
  }
  float rate = k.rate06;  // fVar23 (PES 52)
  const float lowered = rate - (ramp * k.rate07 + k.blend2) * k.blend3;
  if (lowered <= rate) rate = (k.blend3 <= lowered) ? lowered : k.blend3;
  // PES 423: the anime interface's vt+0x08 (P+0xab8) is not ported (ponytail: the clip clock is the host's).

  if (req.U(moveword::kPrecise) != 1u || 2 < classWord || rate < req.F(moveword::kRate)) {
    // Block A (PES 425-447): RUN_TURN_RUN when the look is far enough and the run turn may be taken, else TURN_STOP.
    float f = k.speed4;  // DAT_1425656c0 (PES 426)
    if (classWord != 3) {
      f = half;  // PES 427
      if (classWord == 2) {
        f = m.dist2m5;  // DAT_14259bde4 (PES 428)
        if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) < 2) f = one;  // PES 429
      }
    }
    // PES 431-437: the factor over the 90..157.5 degree band scales it by 1 - factor * half.
    float factor = 0.0f;
    if (k.deg90 < turn) {
      factor = one;
      if (turn < k.deg1575) factor = (turn - k.deg90) / k.deg675;
    }
    f *= one - factor * half;
    // PES 438-442: the look gap is tested unless the walk is precise in class 0..2.
    const bool gapOk = req.U(moveword::kPrecise) != 1u || 2 < classWord ||
                       std::fabs(PesAngleDiff(req.F(moveword::kHeading), req.F(moveword::kLookHeading), k)) <= k.deg5625;
    if (gapOk && f <= req.F(moveword::kDistToLook)) {
      EnterRunTurnRun(host, req);  // FUN_1406d6780
      return;
    }
    EnterTurnStop(host, req);  // FUN_1406d2590
    return;
  }

  // Block B (PES 449-466): the speed scaled by the body's cosine to the displacement decides STOP.
  const float heading = DisplacementBearingDeg(host, k);  // FUN_14046f230 of the last tick's displacement
  const float gap = std::fabs(PesAngleDiff(req.F(moveword::kFacing), heading, k));  // FUN_1404583b0(P+0x20, heading)
  const float cosine = TableSinDeg(gap + k.deg90, one, host.SpeedClassConstantsView());  // FUN_140a92c80(gap + 90, one)
  const float speed = host.CurrentSpeedKmh() * cosine;  // vt+0x20, the ground speed in km/h (PES 453-454)
  const int previous = host.MoveStatePrevious();        // P+0x245d
  if (speed <= k.kmh5 || (speed <= k.kmh135 && (previous == static_cast<int>(MoveState::kTurnStop) ||
                                                previous == static_cast<int>(MoveState::kStop)))) {
    req.SetU(moveword::kClassWord, 0u);  // PES 457
  } else {
    if (speed <= k.kmh5 || k.kmh11 <= speed) {
      EnterStop(host, req);  // PES 460: goto the STOP entry with no word written
      return;
    }
    req.SetU(moveword::kClassWord, 1u);  // PES 462
  }
  // PES 463: word 0x1d takes the speed as a whole number (x86's cvttss2si keeps the sign of a negative speed).
  req.SetU(moveword::kStopSpeedWord, static_cast<uint32_t>(static_cast<int64_t>(speed)));
  EnterStop(host, req);  // PES 466: FUN_1406d5d60
}
namespace {
// PES 267-286 (E3): the ladder selector, checked against the asm. Bucket 0 and 5 take the style arm; bucket 1 and 6 take the
// ladder only under bVar19 (req[0x1F] below 1 with the kind 3 or 6); the other buckets take the ladder. Returns whether the
// ladder answered the tick (STOP, TURN_STOP or RUN_TURN_RUN); false leaves the tick to the style arm (PES 287-398).
bool RunLoopLadder(MoveHost& host, MoveReq& req, const RunConstants& k, int kind) {
  const int bucket = static_cast<int>(req.U(moveword::kMoveBucket));
  const bool bVar19 = static_cast<int>(req.U(moveword::kClassWord)) < 1 && (kind == 3 || kind == 6);
  const bool ladder = (bucket == 1 || bucket == 6) ? bVar19 : (bucket != 0 && bucket != 5);
  if (!ladder) return false;
  // PES 274-284: STOP or IDLE below 0.3 (blend3); TURN_STOP at 2 m or less, and not exactly 2; RUN_TURN_RUN otherwise.
  if (req.F(moveword::kRate) < k.blend3) {
    EnterStop(host, req);
    return true;
  }
  const float toLook = req.F(moveword::kDistToLook);
  if (toLook <= k.speed2 && k.speed2 != toLook) {
    EnterTurnStop(host, req);
    return true;
  }
  EnterRunTurnRun(host, req);
  return true;
}
}  // namespace

void RunLoop(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int band = static_cast<int>(req.U(moveword::kRateBand));
  const bool running = cls != -1 || (req.U(moveword::kPrecise) != 1u && host.MatchPhase() == 1);
  // PES 57-61: a no-class tick (class word -1) with the precise word set, or outside the match phase 1, is TURN_STOP
  // (FUN_1406d2590) before any other arm.
  if (!running) {
    EnterTurnStop(host, req);
    return;
  }

  // Arm 1: the turn-lock re-plan.
  // PES dec_c0170:62-65: the re-plan reads the low bytes of words 0x3d, 0x3e and 0x3f, not the words.
  if ((req.Flag(moveword::kAngle3d) || req.Flag(moveword::kTurnFrames) ||
       req.Flag(moveword::kMirrorFlag)) &&
      (1 < cls || req.Flag(moveword::kMirrorFlag)) &&
      req.F(moveword::kRate) <= k.rate06 && req.U(moveword::kPrecise) == 0u) {
    EnterGotoRun(host, req, false);
    return;
  }

  // Arm 2: the ACCELERATE and DECELERATE pre-empts.
  if (EnterAccelerate(host, req)) return;
  if (EnterDecelerate(host, req)) return;

  // Arm 3: the re-plan term.
  const float ground = GroundSpeedKmh(host, k);
  const float turn = req.F(moveword::kTurn);
  const bool rateLow =
      req.F(moveword::kRate) <= k.rate0225 && req.F(moveword::kRate) != k.rate0225;
  if (req.U(moveword::kPrecise) == 0u && 2 < cls &&
      (rateLow || (ground < k.kmh11 && k.deg7875 < turn)) &&
      k.quota3 < req.F(moveword::kDistToTarget) && k.deg5625 < turn) {
    EnterGotoRun(host, req, false);
    return;
  }

  // Arm 4: the RUN_TURN_STEP pre-empt, then the keeper-kind arm.
  const float ceiling = host.RateCeiling();
  const float threshold = (ceiling - k.rate06) * k.half;
  if (EnterRunTurnStep(host, req, threshold)) return;
  // PES 114-116 (T1): the entry of the swing, style and ladder block. Bucket 0 or 5 with the rate above rampSlope (0.15,
  // 0x1425c48a0), or the run turn is not allowed: FUN_1406b70d0 == 0, and StepLetsRunTurn returns 1 for 'may', so the
  // test is its negation.
  const int bucket = static_cast<int>(req.U(moveword::kMoveBucket));
  const bool t1 = ((bucket == 0 || bucket == 5) && k.rampSlope < req.F(moveword::kRate)) || !StepLetsRunTurn(host, req);
  // PES 117 (T2): the precise word clear, the class -1, or the second dwell still running: the swing arm.
  const bool t2 = req.U(moveword::kPrecise) != 1u || cls == -1 ||
                  req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2);
  // PES 265-266 (E2): the blended rate ceiling below the request's rate: the swing arm. The +0xAC0 interface's slot 0x18 is
  // FUN_1406eadf0 = P+0x377c, the rate ceiling, which host.RateCeiling() reads.
  const float blended = (host.RateCeiling() - k.rate06) * k.half + k.rate06;
  const bool swingNow = t1 && (t2 || blended < req.F(moveword::kRate));
  // PES 267-286 (E3): the ladder answers the tick unless its bucket takes the style arm (see RunLoopLadder).
  if (t1 && !swingNow && RunLoopLadder(host, req, k, kind)) return;

  // PES 408-466: the T1-false tail for a running class.
  if (!t1 && running) {
    RunLoopTail(host, req);
    return;
  }
  // The swing arm (PES 120-263) runs for T1 with T2 or E2. It writes the look words and issues no clip of its own.
  if (swingNow) {
    // PES 120 (E0): the swing entry returns with nothing written while the first dwell is still running.
    if (swingNow && req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit)) return;
    // The playback read through the +0xac0 interface's slot 0x30 (FUN_140a62960's mirror
    // argument) and slot 0x50's answer.
    // ponytail: the interface's slot pair is the anime player's own; the port uses the
    // request's playback default, which is what the pair writes.
    // The playback word is PES's own read here; the port leaves it as the request holds it.
    // The look bucket and the facing error PES selects: word 0xd with word 7 for the
    // out-of-play phase, else word 0xc with word 5.
    int lookBucket = static_cast<int>(req.U(moveword::kLookBucketAlt));
    float lookError = req.F(moveword::kFacingError);
    if (-1 < cls && req.U(moveword::kPrecise) == 0u) {
      lookBucket = static_cast<int>(req.U(moveword::kLookBucket));
      lookError = req.F(moveword::kLookDelta);
    }
    // The band word clamped to 0..3.
    int bandWord = band;
    if (bandWord > 3) bandWord = 3;
    if (bandWord < 0) bandWord = 0;

    // The state picker: the match phase 1 with the look bucket at 4 or 9 and the facing
    // error's magnitude past 146.25 degrees takes the AI target's own heading, folds it
    // against the player's facing, and rewrites the look bucket and the second word.
    if (host.MatchPhase() == 1 && (lookBucket - 3 < 2u || lookBucket - 8 < 2u) &&
        k.deg146p25 < std::fabs(lookError)) {
      // ponytail: FUN_140a6cf70's point, FUN_140a627e0's rank and FUN_140439e50's
      // re-planting are the foundation's event store; the port takes the request's own
      // word 3, which is PES's answer for an unset target.
      float target = req.F(moveword::kLookHeading);
      // PES's 20-in-0x15 test over the record's rank (FUN_140a627e0) picks the AI target's
      // own heading; the port takes the request's word 3, which is PES's unset-target
      // answer for a rank the record chain does not carry.
      float folded = target;
      if (folded < 0.0f) {
        folded += k.full;
      } else if (k.full <= folded) {
        folded += k.wrapFull;
      }
      float delta = folded - FoldFull(req.F(moveword::kFacing), k);
      if (k.half_turn < delta) {
        const float sign = delta < 0.0f ? k.neg : k.one;
        delta -= sign * k.full;
      }
      // The body's own turn (P+0x28 against P+0x20) against the 22.5 / 11.25 pair.
      float bodyTurn = req.F(moveword::kBodyDirection) - req.F(moveword::kFacing);
      if (k.half_turn < bodyTurn) {
        const float sign = bodyTurn < 0.0f ? k.neg : k.one;
        bodyTurn -= sign * k.full;
      }
      float scale = k.deg225;
      if (k.deg225 < std::fabs(bodyTurn)) {
        const float a = bodyTurn < 0.0f ? k.neg : k.one;
        const float b = delta < 0.0f ? k.neg : k.one;
        if (a == b) scale = k.deg1125;
      }
      if (scale < std::fabs(delta) && std::fabs(delta) < k.half_turn - scale) {
        const float a = lookError < 0.0f ? k.neg : k.one;
        const float b = delta < 0.0f ? k.neg : k.one;
        if (a != b) {
          // PES writes the swapped pair straight into the request: the second word's
          // 180 / -180 and the look bucket's 4 / 9.
          lookError = (lookBucket - 3 < 2u) ? k.wrapHalf : k.half_turn;
          lookBucket = (lookBucket - 3 < 2u) ? 9 : 4;
          req.SetF(moveword::kLookDelta, lookError);
          req.SetU(moveword::kLookBucketAlt, static_cast<uint32_t>(lookBucket));
        }
      }
    }

    // The swing arm's bearing: the body's own turn, the live clip's motion family and the
    // +/-90 / +/-45 / 180 clamps PES writes into the bucket word.
    float bodyTurn = req.F(moveword::kBodyDirection) - req.F(moveword::kFacing);
    if (k.half_turn < bodyTurn) {
      const float sign = bodyTurn < 0.0f ? k.neg : k.one;
      bodyTurn -= sign * k.full;
    }
    const int motion2 = host.CurrentMotion();
    if (motion2 > 0xb48) {
      if (motion2 == 0xb4f || motion2 == 0xb52 || motion2 == 0xb5a || motion2 == 0xbc3 ||
          motion2 == 0xc05 || motion2 == 0xc3d) {
        // The 33.75 / 22.5 family's clamp.
        if (k.deg3375 < std::fabs(bodyTurn)) goto swing_180;
        if (k.deg225 < std::fabs(bodyTurn)) {
          const float card = (0.0f < bodyTurn) ? k.deg90 : -k.deg90;
          const float a = lookError < 0.0f ? k.neg : k.one;
          const float b = card < 0.0f ? k.neg : k.one;
          if (a != b) lookBucket = SwingBucketAngle(host, -card);
        }
        goto swing_write;
      }
    } else if (motion2 == 0xb48) {
      goto swing_180_check;
    } else if (motion2 < 0x4f) {
      // The 0x25..0x4e family's mask, decoded as the wide range PES's own bit test reads.
      if (motion2 - 0x25 < 0x2au && ((0x24800000001ULL >> (motion2 - 0x25)) & 1) != 0) {
        goto swing_180_check;
      }
    } else if (motion2 > 0x8b4 && motion2 < 0x8ba) {
      goto swing_180_check;
    } else if (motion2 == 0x8c7) {
      // The 0x8c7 family: the +/-45 card against the second word's magnitude, and the 45
      // degree gate on the card.
      const float card = (0.0f < lookError) ? k.deg45 : -k.deg45;
      if (k.deg45 < std::fabs(lookError)) {
        lookBucket = SwingBucketAngle(host, card);
        goto swing_write;
      }
    } else if (motion2 == 0x8ca) {
      goto swing_180_check;
    }
    lookBucket = SwingBucketAngle(host, lookError);
    goto swing_write;

  swing_180_check:
    if (k.deg3375 < std::fabs(bodyTurn)) {
    swing_180:
      // The 180 / -45 pair: the 146.25 gate on the second word.
      float card = k.wrapHalf;
      float half = -k.deg45;
      if (0.0f < bodyTurn) {
        card = k.half_turn;
        half = k.deg45;
      }
      if (k.deg146p25 <= std::fabs(lookError)) {
        const float a = lookError < 0.0f ? k.neg : k.one;
        const float b = half < 0.0f ? k.neg : k.one;
        if (a != b) {
          lookBucket = SwingBucketAngle(host, card);
          goto swing_write;
        }
      } else {
        const float a = lookError < 0.0f ? k.neg : k.one;
        const float b = half < 0.0f ? k.neg : k.one;
        if (a != b) {
          lookBucket = SwingBucketAngle(host, -half);
          goto swing_write;
        }
      }
    } else if (k.deg225 < std::fabs(bodyTurn)) {
      const float card = (0.0f < bodyTurn) ? k.deg90 : -k.deg90;
      const float a = lookError < 0.0f ? k.neg : k.one;
      const float b = card < 0.0f ? k.neg : k.one;
      if (a != b) lookBucket = SwingBucketAngle(host, -card);
    }
  swing_write : {
    // The bucket's own folds (4 -> 3, 9 -> 8): the bucket word the E3 ladder reads.
    if (lookBucket == 4) lookBucket = 3;
    if (lookBucket == 9) lookBucket = 8;
    // The turned heading: the bucket's own centre angle with the 11.25 allowance for the
    // buckets 3 and 8, clamped to the swing bearing when it sits inside the window, and
    // added to the request's own word 2.
    float allowance = 0.0f;
    if (lookBucket - 3u < 2u || lookBucket - 8u < 2u) allowance = k.deg1125;
    const float centre = host.StepTablesView().bucketDegrees[lookBucket];
    float turned = centre + allowance;
    if (bodyTurn <= turned && (centre - allowance) <= bodyTurn) turned = bodyTurn;
    req.SetF(moveword::kRecordStampC, turned + req.F(moveword::kFacing));
    req.SetU(moveword::kBandTarget, static_cast<uint32_t>(bandWord));
    req.SetF(moveword::kSpeedBand, req.F(moveword::kSpeedBand));
    req.SetU(moveword::kPrevBucket, static_cast<uint32_t>(lookBucket));
    req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(lookBucket));
    req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
    req.SetByte(moveword::kMirrorFlag, 0);
    // PES 265-286: the swing words are written, then the E3 ladder answers the tick; a bucket that takes the style arm falls
    // through to it (PES 287-398).
    if (RunLoopLadder(host, req, k, kind)) return;
  }
  }

  // Arm 5: the style block (the tail of FUN_1406c0170). The ladder's answer, with the jostle override, is the
  // style; a style equal to the request's word 0x50 ends the tick with nothing written.
  const float rate = RequestedRate(host, req, t);
  // FUN_1406c0650's call: rateHeld is 0 there. The ladder reads the ramped rate from req[0x4a], not this one.
  const int base = LocomotionStyle(host, host.AnimeState(), rate, req, host.RateCeiling(), false);
  float jostleTurn = 0.0f;
  const int style = JostleStyle(host, req, base, jostleTurn);
  if (style == static_cast<int>(req.U(moveword::kStyle) & 0xffu)) return;

  // The change: the style, its representative motion (DAT_1425ddcf0 at style * 4 + band), the mirror, and the
  // blend words the change sets.
  const uint32_t previous = req.U(moveword::kStyle) & 0xffu;
  const int bandWord = Clamp03(band);
  req.SetU(moveword::kParamState, static_cast<uint32_t>(style));
  req.SetByte(moveword::kMotionPending, 1);
  req.SetByte(moveword::kStyle, static_cast<uint8_t>(style));
  req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(bandWord));
  req.SetU(moveword::kMotion, static_cast<uint32_t>(t.styleGroup[style * 4 + bandWord]));
  const bool clipBefore = host.ClipFlag();
  bool mirror = clipBefore;
  if (style < 13 && ((0x1028u >> (style & 31)) & 1u) != 0) {
    mirror = true;
  } else if (style < 12 && ((0x814u >> (style & 31)) & 1u) != 0) {
    mirror = false;
  } else if (style == 6 || style == 7 || style == 0x10) {
    mirror = jostleTurn < 0.0f;
  }
  req.SetByte(moveword::kMirror, mirror ? 1 : 0);
  req.SetU(moveword::kTransitionWord, mirror != clipBefore ? 3u : 2u);
  req.SetF(moveword::kDuration, (style == 0 && previous != 0u) ? k.blend2 : k.blend1333);
  req.SetF(moveword::kPlayback, k.one);
  req.SetF(moveword::kStartTime, 0.0f);
  // PES's tail (0x1406C0ACB-0x1406C0AF2): the first dwell and its limit, the second dwell and
  // its limit, the live kind, the change-state flag and the next state.
  req.SetF(moveword::kDwellElapsed, 0.0f);
  req.SetF(moveword::kDwellLimit, k.blend2);
  req.SetF(moveword::kDwellElapsed2, 0.0f);
  req.SetF(moveword::kDwellLimit2, k.blend2);
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  req.SetU(0x20, 0u);  // req[0x20]: PES clears it in the change; the word has no name yet
  req.SetByte(moveword::kMirrorFlag, 0);
  // PES's style arm is the one place that enters the STEP family: req[0x39] = 0xE
  // (RUN_TO_STEP) at 0x1406C0AF2, with the change-state flag at 0x1406C0AEB.
  RequestState(req, MoveState::kRunToStep);
}


// ---------------------------------------------------------------------------------------------
// FUN_1406ccd00, the ACCELERATE entry (mv4.a.c:4478). The entry the running phase calls when
// the speed-up predicate holds. Its decompile's flow, in PES's own order:
//
//   1. the gates, any of which refuses with 0: the band word below `67.5 <= bearing` (the
//      same test, negative when the bearing is under 67.5), the band word at 3, the class 3
//      with the precise word clear and the rate word at or above 0.65, the ground speed at or
//      below 11 km/h, or the look distance at or below 2 m;
//   2. the record chain's block (the class-17 byte and the FUN_140a48a10 record's class,
//      draw, bearing and distance tests), whose failure also refuses;
//   3. the playback read (the +0xac0 interface's slot 0x30 with FUN_140a62960's mirror
//      argument and slot 0x50's answer), the ground-speed compute, the desired angle (the
//      request's word 6 when the bearing's magnitude is past 5.625 degrees, with
//      FUN_140a92950's own fold and a 0.8 blend, else word 6 itself) and the bucket from the
//      +0xac0 interface's slot 0x60;
//   4. FUN_1406d8730 (the yaw lock), the bucket's column from the table and the held-clip
//      family's rows, then the out-of-play arm's rows 0x5cc / 0x5cd;
//   5. the row pick: FUN_1406abdd0's guard, FUN_1406abb30's cell and FUN_1406abd60's mirror,
//      with the foot-window gate over the bones 3 and 6 for the held foot's y;
//   6. the playback: 13.5 km/h, 8 above a rate of 0.55 and 11 for the three turn motions,
//      then the 17.2 / 22 thresholds with 0.75 and 0.25, and the 0.15 of the swing row;
//   7. the writes: the band word 3, the speed word 28 km/h, the previous band and speed
//      words, the bucket and the live-kind words, the state word 9 (ACCELERATE) and the
//      vtable's +8.
//
// The port stands each PES input GF lacks as a ponytail: the record chain, the +0xac0
// interface's slot pair, the AI target's point and the event ring.
// ---------------------------------------------------------------------------------------------
bool EnterAccelerate(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const int band = static_cast<int>(req.U(moveword::kRateBand));
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int motion = host.CurrentMotion();
  const float ground = GroundSpeedKmh(host, k);
  // The desired turn PES gates on (FUN_1406ccd00 lines 150-154): word 6, or the body's step stamp against the heading when the body
  // is past 5.625 degrees off the facing. Under 67.5 degrees the band is 0 and the ground floor is 0.5 km/h (fVar25); otherwise the
  // band is 1 and the floor is 11 km/h.
  float desired = req.F(moveword::kTurn);
  const float bodyFacing = std::fabs(PesAngleDiff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k));
  if (k.deg5p625 < bodyFacing) {
    const float stamp = StepHeading(req.F(moveword::kBodyDirection), req.F(moveword::kFacing), k.blend08 * bodyFacing, k);
    desired = PesAngleDiff(stamp, req.F(moveword::kHeading), k);
  }
  const bool underSixtySeven = std::fabs(desired) < k.deg675;
  const int thresholdBand = underSixtySeven ? 0 : 1;
  const float groundFloor = underSixtySeven ? k.half : k.kmh11;

  // Arm 1: the gates (FUN_1406ccd00's refusals): the class band, command class 3 only, precise must be 0, rate under 0.65
  // (DAT_14259d5d0), the ground speed, and the look distance (word 0x12 <= speed2 = 2 m).
  if (band < thresholdBand || band == 3 ||
      cls != 3 || req.U(moveword::kPrecise) != 0u || k.ramp65 <= req.F(moveword::kRate) ||
      ground <= groundFloor || req.F(moveword::kDistToLook) <= k.speed2) {
    return false;
  }

  // The playback word (FUN_1406ccd00 0x1406cd020..0x1406cd02c): req[0x35] = vt+0x50(vt+0x30(match-up flag)).
  req.SetF(moveword::kPlayback, EntryPlayback(host));

  // Arm 3: the bucket of the desired turn, computed with the gate above.
  const int bucket = AngleBucket(host.AngleBuckets(), desired, 10, false);

  // Arm 4: the bucket's column, the held-clip family's rows and the out-of-play arm.
  int column = t.bucketColumn[bucket];
  int row = 0x575;  // 2.0557e-42: the held-clip family's base row
  if (static_cast<uint32_t>(column - 8) < 2u) {
    // ponytail: the family's own rows (0x575 / 0x573 / 0x571) read the record's class and
    // draw through FUN_140a58550 / FUN_14044e020, which are the foundation's player table;
    // the port keeps the base row.
  }
  if (host.MatchPhase() == 1 && row < 0x10 && ((0xa014u >> (row & 0x1f)) & 1u) != 0) {
    // ponytail: FUN_140a6cf70's point, FUN_140a627e0's rank and the event ring's frame are
    // the foundation's event store; the port keeps the base row and states the missing
    // comparison here.
  }

  // Arm 5: the row pick and the foot-window gate.
  if (bucket < 0) return false;
  const MoveCell cell = host.Cell(row, column, false);
  if (!cell.found) return false;
  bool mirror = cell.mirror;
  const int foot = host.FootBand();
  if (static_cast<uint32_t>(column - 8) < 2u && (foot & 0xfffffffd) == 0) {
    // The two feet's local y at the bones 3 and 6, with the foot band's side choosing which
    // is the near one. The player's own y is PES's P+0x10, which the port stands at zero
    // because the record's position is not modelled.
    float footA[3] = {0.0f, 0.0f, 0.0f};
    float footB[3] = {0.0f, 0.0f, 0.0f};
    host.PositionAt(3, footA);
    host.PositionAt(6, footB);
    const float nearY = (foot == 2) ? footA[1] : footB[1];
    const float farY = (foot == 2) ? footB[1] : footA[1];
    bool gate = false;
    if (foot == 2) {
      gate = !(k.foot035 <= nearY || k.foot027 <= nearY || farY <= k.footNeg010);
    } else {
      gate = (nearY < k.foot035 && farY < k.foot027 && k.footNeg010 < nearY);
      if (gate && foot != 0) gate = false;
    }
    if (!gate) mirror = false;
  }

  // Arm 6: the start-time ladder (FUN_1406ccd00:369-415). fVar33 is 0.15 when the bone
  // window holds, 0.75 past 22 km/h ground speed, the 0.5 default between 17.2 and 22 km/h
  // with a rung above the ground speed, else the 0.5 default. fVar32 (13.5, or 8 past a rate
  // of 0.55, or 11 for the 0x8bb/0x8be/0x8c0 turn motions) is the ground-speed threshold the
  // mirror flip compares against: it is never passed to the issue call.
  float startTime = k.half;
  if (static_cast<uint32_t>(column - 8) < 2u) {
    if (ground <= k.kmh172) {
      // The 0.5 default stands.
    } else if (k.kmh22 < ground) {
      mirror = !mirror;
      startTime = k.foot075;
    } else if (k.kmh135 <= ground) {
      mirror = !mirror;
      startTime = k.foot035;
    }
  }
  float rung = k.kmh135;
  if (k.rate055 <= req.F(moveword::kRate)) rung = k.kmh8;
  if (motion == 0x8bb || motion == 0x8be || motion == 0x8c0) rung = k.kmh11;
  (void)rung;

  // Arm 7: the writes (FUN_1406ccd00:417-418). vt+0x10 takes (req, motion, mirror, 0, 0.1667,
  // req[0x35], startLadder): the playback is the word arm 3 wrote, not req[0x4b].
  WriteIssue(host, req, MotionRequest{cell.motionNo, mirror, 0, k.blend1667, req.F(moveword::kPlayback), startTime});  // vt+0x10
  req.SetU(moveword::kBandTarget, static_cast<uint32_t>(band));
  req.SetF(moveword::kSpeedBand, k.kmh28);
  req.SetU(moveword::kRateBand, 3u);
  req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
  req.SetF(moveword::kRecordStampB, req.F(moveword::kHeading));
  req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
  RequestState(req, MoveState::kAccelerate);
  req.SetByte(moveword::kReorientFlag, 0);
  return true;
}


// ---------------------------------------------------------------------------------------------
// FUN_1406cdaf0, the DECELERATE entry (mv4.a.c:4913). The entry the running phase calls when
// the slow-down predicate holds. Its decompile's flow, in PES's own order:
//
//   1. the gates, any of which refuses with 0: the class word above 1, the precise word set
//      with the turn at or above 33.75 degrees or the facing error at or above 67.5, the rate
//      word at or below 0.5 (0.7 for the command 5), the ground speed at or below 19 (22 for
//      the command 5), or the look distance at or below 1 m;
//   2. the turn-bearing guard: when the turn is under 33.75 degrees the bucket word 8 is
//      folded 1 -> 0 and 6 -> 5;
//   3. FUN_1406d8730 (the yaw lock) and the row: 0x5c9, or 0x5ca when the team record's
//      channel and the ball-line point's 0.3 / 90-degree tests hold; the held-clip family
//      with the 20 m ball-line gate and the Percent(40) draw gives 0x662 / 0x661; the
//      out-of-play arm gives 0x5cc;
//   4. the row pick: FUN_1406abdd0's guard, FUN_1406abb30's cell and FUN_1406abd60's mirror,
//      with the foot-window gate over the bones 3 and 6;
//   5. the writes: the band word from the interface's slot 0x68, the speed word from the
//      band's own value times 3.6, the band word 1 and the speed word 11 km/h, the previous
//      band and speed words, the bucket word, the state word 10 (DECELERATE) and the
//      vtable's +8.
//
// The port stands each PES input GF lacks as a ponytail: the team record's channel, the
// ball-line point and the event ring.
// ---------------------------------------------------------------------------------------------
bool EnterDecelerate(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const float ground = GroundSpeedKmh(host, k);
  // PES's command-5 pair: the rate gate at 0.7 and the speed gate at 22 km/h.
  float rateGate = k.half;
  float speedGate = k.kmh19;
  if (req.U(moveword::kCommandId) == 5u) {
    rateGate = k.rate07;
    speedGate = k.kmh22;
  }

  // The playback word (FUN_1406cdaf0:0x1406cdb85..0x1406cdb99): req[0x35] = vt+0x50(vt+0x30(match-up flag)).
  req.SetF(moveword::kPlayback, EntryPlayback(host));

  // Arm 1: the gates.
  if (static_cast<uint32_t>(cls) > 1u) return false;
  if (req.U(moveword::kPrecise) != 0u) {
    if (k.deg3375 <= std::fabs(req.F(moveword::kTurn))) return false;
    if (k.deg675 <= std::fabs(req.F(moveword::kFacingError))) return false;
  }
  if (req.F(moveword::kRate) <= rateGate) return false;
  if (ground <= speedGate) return false;
  if (req.F(moveword::kDistToLook) <= k.one) return false;

  // Arm 2: the turn-bearing guard's bucket fold.
  int bucket = static_cast<int>(req.U(moveword::kMoveBucket));
  if (std::fabs(req.F(moveword::kTurn)) < k.deg3375) {
    if (bucket == 1) {
      bucket = 0;
    } else if (bucket == 6) {
      bucket = 5;
    }
  }

  // Arm 3: the row. The base row is 0x5c9; the held-clip family's test gives 0x5ca, and the
  // Percent(40) draw 0x662 / 0x661.
  int row = 0x5c9;
  int column = t.bucketColumn[bucket];
  if (static_cast<uint32_t>(column - 8) < 2u) {
    // ponytail: the team record's channel and the ball-line point's 0.3 / 90-degree tests
    // are the foundation's event store and the ball-track port; the port keeps the base row.
  }
  if (static_cast<uint32_t>(column - 8) < 2u && host.FootBand() < 0x16) {
    // ponytail: FUN_140a58550 / FUN_14044e020 (the record's class and draw) and the 20 m
    // ball-line gate are the foundation's player table and the ball-track port; the port
    // keeps the base row.
  }
  if (host.MatchPhase() == 1 && column < 0x10 && ((0xa014u >> (column & 0x1f)) & 1u) != 0) {
    // ponytail: FUN_140a6cf70's point, FUN_140a627e0's rank and the event ring's frame are
    // the foundation's event store; the port keeps the base row and states the missing
    // comparison here.
    row = 0x5cc;
  }

  // Arm 4: the row pick and the foot-window gate.
  if (bucket < 0) return false;
  const MoveCell cell = host.Cell(row, column, false);
  if (!cell.found) return false;
  bool mirror = cell.mirror;
  const int foot = host.FootBand();
  if (static_cast<uint32_t>(column - 8) < 2u && (foot & 0xfffffffd) == 0) {
    float footA[3] = {0.0f, 0.0f, 0.0f};
    float footB[3] = {0.0f, 0.0f, 0.0f};
    host.PositionAt(3, footA);
    host.PositionAt(6, footB);
    const float nearY = (foot == 2) ? footA[1] : footB[1];
    const float farY = (foot == 2) ? footB[1] : footA[1];
    bool gate = false;
    if (foot == 2) {
      gate = !(k.foot035 <= nearY || k.foot027 <= nearY || farY <= k.footNeg010);
    } else {
      gate = (nearY < k.foot035 && farY < k.foot027 && k.footNeg010 < nearY);
      if (gate && foot != 0) gate = false;
    }
    if (!gate) mirror = false;
  }
  // The start time (FUN_1406cdaf0:333-336): 0.2 for the 0xb53 row with a column of 8/9,
  // else 0. The playback is the word the head wrote (req[0x35]), not req[0x4b].
  float startTime = 0.0f;
  if (bucket - 8 < 2u && static_cast<int>(cell.motionNo) == 0xb53) startTime = k.blend2;

  // Arm 5: the writes (FUN_1406cdaf0:337-339).
  WriteIssue(host, req, MotionRequest{cell.motionNo, mirror, 0, k.blend1667, req.F(moveword::kPlayback), startTime});  // 0x1406ce31a.. vt+0x10
  req.SetU(moveword::kBandTarget, static_cast<uint32_t>(cls));
  req.SetF(moveword::kSpeedBand, req.F(moveword::kSpeedBand));
  req.SetU(moveword::kRateBand, 1u);
  req.SetF(moveword::kSpeedBand, k.kmh11);
  req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
  req.SetF(moveword::kRecordStampB, req.F(moveword::kHeading));
  req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
  RequestState(req, MoveState::kDecelerate);
  req.SetByte(moveword::kReorientFlag, 0);
  return true;
}
}  // namespace movestep
}  // namespace pes
