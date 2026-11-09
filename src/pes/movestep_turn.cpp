// PES 2021's MOVESTEP handler group D: FUN_1406ba900's cases 12 (RUN_TURN_RUN),
// 18 (RUN_TURN_RUN_SLIPDOWN), 16 (RUN_TURN_STEP), 15 (STEP_TURN_RUN) and 19 (STEP_TURN_STEP),
// plus their entries FUN_1406d6780, FUN_1406d2f60 and FUN_1406d38d0.
//
// Every function is the body of the PES function named at its head, ported in PES's order with
// PES's field writes. The install constants come from the exe by VA (the table below); tests
// pass synthetic ones. Where a PES read has no GF counterpart yet, a `// ponytail:` note names the
// PES site that supplies it. FUN_1406cbb00 (the yaw lock) is the dispatcher's own, so no handler
// calls it; FUN_1406cba60 (the yaw writer after a RUN_TURN_RUN entry's re-select) writes P+0xbec
// through MoveHost::SetRecordF0bec.
#include "pes/movestep_turn.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "pes/exe.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_step.hpp"
#include "pes/movestep_turn_entry.hpp"

namespace pes {
namespace {

// The group's constants struct lives in namespace pes::movestep; the file-local helpers below
// reference it through this alias.
using MoveTurnConstants = movestep::MoveTurnConstants;

// ---------------------------------------------------------------------------------------------
// The install data, by VA. The install test checks their relations.
// ---------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------
// PES's angle helpers, ported from their own decompiles.
// ---------------------------------------------------------------------------------------------

// PES's inline fold used all over the turn handlers: [0, 360).
float FoldUnit(float x, const MoveTurnConstants& k) {
  if (0.0f <= x) {
    if (k.fullTurn <= x) return x + k.negFullTurn;
    return x;
  }
  return x + k.fullTurn;
}

// FUN_14043a0a0(a, b): both folded into [0, 360), then the difference into (-180, 180].
float Wrap180Diff(float a, float b, const MoveTurnConstants& k) {
  const float d = FoldUnit(b, k) - FoldUnit(a, k);
  if (k.halfTurn < d) return d - k.one * k.fullTurn;
  if (d < -k.halfTurn) return d - k.negOne * k.fullTurn;
  return d;
}

// FUN_1406b8da0(x): the three-step fold onto (-180, 180].
float WrapHalf(float x, const MoveTurnConstants& k) {
  if (x < k.negHalfTurn) return x + k.fullTurn;
  if (k.halfTurn < x) x += k.negFullTurn;
  return x;
}

// FUN_140a92950(heading, target, clamp): the heading stepped towards the target by at most the
// clamp, wrapped into [0, 360). Ported from its own decompile (h_140a92950.c), keeping PES's
// comparisons so its NaN behaviour carries.
float StepHeadingToPoint(float heading, float target, float clampDeg, const MoveTurnConstants& k) {
  const float diff = Wrap180Diff(heading, target, k);
  const float sign = (0.0f <= diff) ? k.one : k.negOne;
  float step = sign * diff;
  if (clampDeg <= step) step = clampDeg;
  heading = FoldUnit(heading, k);
  heading += step * sign;
  if (0.0f > heading) return heading + k.fullTurn;
  if (k.fullTurn <= heading) return heading + k.negFullTurn;
  return heading;
}

// FUN_1415a1780: the vector length.
float Length3(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }

// FUN_1406ba070(P): the step playback, playbackFloor + turnRamp * t(G), with t the group's ramp.
float PlaybackAt(const MoveHost& host, const MoveTurnConstants& k) {
  const float g = host.PhysiqueG();
  float t;
  if (k.gLow < g) {
    t = (g < k.gHigh) ? (g - k.gHigh) / k.gSlope : 0.0f;
  } else {
    t = 0.0f;
  }
  return t * k.turnRamp + k.playbackFloor;
}

// The row-id idiom: PES compares the ROW ID word (req[0x20]'s bit pattern) against raw float bit
// patterns (0x546 = 1.87494e-42, 0x542 = 1.88615e-42, 0x547 = 1.87634e-42, 0x543 = 1.88755e-42,
// 0x54e = 1.85812e-42, 0x54f = 1.86092e-42). The comparisons are integer row-id comparisons.
inline bool RowIs(uint32_t rowWord, uint32_t rowId) { return rowWord == rowId; }

// ---------------------------------------------------------------------------------------------
// The motion record reads. Each helper names the PES reader it stands for. The tick readers (FUN_1406af620, FUN_1406af8e0,
// FUN_1406b1f00, FUN_1406b1d00) are the host's ClipTicks, PES's translated code on the motion table; the RotData offset, the
// counts and the FootType pair still read the install's table through the host's Motion() (see MoveHost::Motion).
// ---------------------------------------------------------------------------------------------
// FUN_1406b1f00 on RotData entry 0: the start, in ticks.
int RecordStartFrames(const MoveHost& host, int motionNo, float scale) {
  return host.ClipTicks(pes::ClipReader::kRotStart, motionNo, scale, 0);
}
// FUN_1406b1d00 on RotData entry 0: the end, in ticks.
int RecordEndFrames(const MoveHost& host, int motionNo, float scale) {
  return host.ClipTicks(pes::ClipReader::kRotEnd, motionNo, scale, 0);
}
// FUN_1406b1c70 on RotData entry 0: the signed offset, negated when mirrored.
float RecordOffsetDeg(const MoveHost& host, int motionNo, bool mirror, const MoveTurnConstants& k) {
  const float offset = static_cast<float>(host.Motion(motionNo).Rot(0).offset);
  return offset * (mirror ? k.negOne : k.one);
}
// FUN_1406af620: the Animation blend, in ticks.
int RecordFixedFrames(const MoveHost& host, int motionNo, float scale) {
  return host.ClipTicks(pes::ClipReader::kBlend, motionNo, scale, 0);
}

// FUN_140a6e290(motion): the motions of the running set, a pure test of the motion id (decompile of 0x140a6e290; the translated
// pes_fn_140A6E290 is the oracle in pes_movestep_wire_test).
bool IsRunningMotion(int motion) {
  switch (motion) {
    case 0xbe7: case 0x8c7: case 0x8cb: case 0xb4f: case 0xbe5: case 0xbe9: case 0xc08: case 0xc0f:
      return true;
    default:
      return false;
  }
}
// FUN_140a6e2e0(motion): the halted set, 0xc09 and the motions whose id less 0xc0d has no bit outside 0b101 (0xc0d, 0xc0e,
// 0xc11, 0xc12) (0x140a6e2e0; oracle: pes_fn_140A6E2E0).
bool IsHaltedMotion(int motion) { return motion == 0xc09 || (motion >= 0xc0d && ((motion - 0xc0d) & ~5) == 0); }
// FUN_141eb3320(P+0x4d0): the AdjustAngle adjuster is busy. ponytail: GF has no adjuster object; its writers are the tail of
// FUN_1406b8340 (FUN_141eb35a0 / FUN_141eb3ae0), which the anime layer owns; idle stands in until it lands.
bool FacingAdjusterBusy(const MoveHost&) { return false; }
// FUN_140a6e4f0(motion): the fading-clip set, a pure test of the u16 motion id (decompile of 0x140a6e4f0; the translated
// pes_fn_140A6E4F0 is the oracle in pes_movestep_wire_test). Below 0xc71: 0xc6e, 0xc70 and the even ids 0x3c..0x42 (bit mask 0x55
// over id - 0x3c, an unsigned compare); above: 0xc73, 0xc76, 0xc78, 0xf39.
bool IsFadingMotion(int motion) {
  const uint32_t u = static_cast<uint32_t>(motion) & 0xffffu;
  if (u < 0xc71u) {
    const uint32_t rel = u - 0x3cu;
    return u == 0xc70u || u == 0xc6eu || (rel <= 6u && ((0x55u >> (rel & 0x1fu)) & 1u) != 0u);
  }
  return u == 0xc73u || u == 0xc76u || u == 0xc78u || u == 0xf39u;
}

// FUN_1406d8730 / FUN_1406d2590 / FUN_1406d5d60 / FUN_1406ce820 / FUN_1406d38d0: the entry
// prologue and the other groups' entries. Each handler port calls them exactly as PES does; the
// bodies live in their owning files and the tests stub them.
void EntryPrologue(const MoveHost&, const MoveReq&, const MoveTurnConstants&) {}

// +0x4d0 vt+0x08 (P+0x4d0, P+0xa08): the AdjustAngle adjuster's apply call. Wave1AMov confirmed
// it is not the P+0xbec store, and the adapter's copy from req[0x3b] is the only P+0xbec store,
// so this is a no-op here.
// ponytail: P+0x4d0 vt+0x08, the AdjustAngle adjuster (AS 6, AL-12); no host read yet.
void AdjustAngleApply(const MoveHost&) {}

// P+0xac0's own readers (the interface the handlers issue through).
// ponytail: the interface readers; the host publishes the action's own values.
float OwnRate(const MoveHost& host) { return host.RateCeiling(); }
int InterfaceClass(const MoveHost& host) { return host.CommandSpeedClass(); }
float InterfaceSpeed(const MoveHost& host) { return host.CommandSpeedKmh(); }

// FUN_1406b9ff0(ap, useClassRate, classWord, speed, kind): the parametric rate, clamped to
// [0, own]. Its class-rate path is PES's own table read; the other two are FUN_1406b9e80's.
int RateOf(MoveHost& host, bool useClassRate, int classWord, float speed, int kind) {
  if (useClassRate) return movestep::SpeedToRate(host, static_cast<float>(classWord), 0);
  if (classWord == 0 && (kind == 3 || kind == 6)) return 0;
  return movestep::SpeedToRate(host, speed, 0);
}

// FUN_140a62960 / FUN_140a627e0: the anime player's slot and its row answer. ponytail: the roster.
int AnimeSlot(const MoveHost&) { return 0; }
int AnimeRow(const MoveHost& host, int row) {
  (void)host;
  return row;
}

// FUN_140a3e190 / FUN_140a48a10 / FUN_140a48a30 / FUN_140a48910 / FUN_140a489d0 / FUN_140a489f0 /
// FUN_140a62e40 / FUN_140a6cbd0 / FUN_140a57720 / FUN_140a76240 / FUN_140a71520 / FUN_140a58550 /
// FUN_140a6ce00 / FUN_14044e020 / FUN_140479470 / FUN_140a3f560 / FUN_1406ba7d0's siblings: the
// match's per-player records and the trainer's flag. ponytail: the roster reads; the ports read
// the host's position ring, the foot band and the match tick instead.
void PlayerPosition(const MoveHost& host, int slot, float out[3]) {
  host.PositionAt(slot >= 2 ? 2 : 2, out);
}
int PlayerByte(const MoveHost&, int, int) { return 0; }
int RecordStampOf(const MoveHost& host) { return host.MatchTick(); }
int LookSlotOf(const MoveHost&) { return 2; }
bool TrainerFlag(const MoveHost&) { return false; }

// +0xac0's bucket reader, which the port resolves through the shared AngleBucket helper.
int InterfaceBucket(const MoveHost& host, float angleDeg) {
  return movestep::AngleBucket(host.AngleBuckets(), angleDeg, 10, false);
}

// FUN_1406abb30 / FUN_1406abd60 resolve the row pair the handlers issue: the representative
// motion and the mirror bit. The port asks the host once per direction PES calls it in.
struct RowPick {
  int motion = -1;
  bool mirror = false;
  bool found = false;
};
RowPick PickRow(MoveHost& host, int row, int column) {
  const MoveCell cell = host.Cell(row, column, false);
  const MoveCell mirrorCell = host.Cell(row, column, false);
  RowPick pick;
  pick.found = cell.found || mirrorCell.found;
  pick.motion = cell.motionNo;  // an empty cell answers -1 (FUN_1406ABB30): never the row id as a motion
  pick.mirror = mirrorCell.mirror;
  return pick;
}



// ---------------------------------------------------------------------------------------------
// The extra PES readers the turn handlers need, each naming its PES site.
// ---------------------------------------------------------------------------------------------

// FUN_14014e6c7 (0x1406c14c0:0x1406c175c): the 24-slot roster walk at +0x180, each entry 0x74
// bytes, its row id at +0x20 and its flag byte at +0xd. ponytail: the roster read.
int PlayerRowId(const MoveHost&, int slot) { return slot; }
int PlayerFlagD(const MoveHost&, int slot) {
  (void)slot;
  return 0;
}

// (P+0xab8 vt+0x78)(): the foot band the row-replay requests carry -> MoveHost::FootBand.
int FootBandOf(const MoveHost& host) { return host.FootBand(); }

// FUN_14042d470's no-argument form: the heading of the last-tick displacement,
// FUN_141eb50b0(P, out, 1) whose length FUN_1415a1780 consumes.
float DispHeading(const MoveHost& host, const MoveTurnConstants& k) {
  float out[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, out);
  if (out[0] == 0.0f && out[2] == 0.0f) return 0.0f;
  return FoldUnit(std::atan2(out[0], out[2]) * 180.0f / 3.14159265358979f, k);
}

// PES's (t * 0x4b) / 100 share.
int Share75(int t) { return (t * 0x4b) / 100; }
// (clock * 60 * 60) / 1000 * |displacement|: the last-tick displacement in km/h.
float DispSpeedKmh(const MoveHost& host, const MoveTurnConstants& k) {
  float out[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, out);
  return static_cast<float>(host.TicksPerSecond()) * k.fps * k.fps / k.thousand *
         Length3(out[0], out[1], out[2]);
}

// The row id PES resolves through FUN_1406abb30 / FUN_1406abd60 and its row-id echo selectors,
// where the row word is compared against PES's own float bit patterns. The bit patterns are the
// integer row ids (0x546 = 1.87494e-42, 0x542 = 1.88615e-42, 0x547 = 1.87634e-42,
// 0x543 = 1.88755e-42, 0x54e = 1.85812e-42, 0x54f = 1.86092e-42).

}  // namespace
}  // namespace pes

namespace pes {
namespace movestep {

// The vt+0x10 clip request, in PES's argument order: (interface, req, motion, mirror, 0, blend,
// playback, start). The clip is a SetMotion (group -1): the start is its start time in seconds.
void IssueClip(MoveHost& host, int motion, bool mirror, float blend, float playback, float start) {
  MotionRequest clip;
  clip.motion = motion;
  clip.mirror = mirror;
  clip.transition = 0;
  clip.duration = blend;
  clip.playback = playback;
  clip.startTime = start;
  clip.group = -1;  // SetMotion (req[0x37] == 0x11)
  host.Issue(clip);
}

// PES's cell lookup in the RUN_TURN_RUN entry: an empty cell (FUN_1406ABDD0 answers 0) issues no clip and the entry hands
// the tick to TURN_STOP (LAB_1406D8523 -> FUN_1406D2590). Returns false once the tick has been handed over.
bool IssueRowOrTurnStop(MoveHost& host, MoveReq& req, int row, int column, float blend, float playback) {
  const RowPick pick = PickRow(host, row, column);
  if (!pick.found) {
    EnterTurnStop(host, req);
    return false;
  }
  IssueClip(host, pick.motion, pick.mirror, blend, playback, 0.0f);
  return true;
}

// PES's turn-family column remap (FUN_1406d6780 at 0x1406D7CAC; TeamDecode's jump table at 0x1406D85E4): on the turn rows
// 0x5BF, 0x5C1 and 0x5C3 (the test (row - 0x5BF) & ~6 == 0, with 0x5C5 excluded) and req[0x11] <= fVar34, the column steps
// back two buckets: 2->0, 4->2, 6->4, 11->13, 13->15, 15->17. Other columns and other rows keep theirs.
int TurnFamilyColumn(const MoveReq& req, int row, int column, float fVar34) {
  const bool turnRow = ((static_cast<uint32_t>(row) - 0x5bfu) & 0xfffffff9u) == 0u && row != 0x5c5;
  if (!turnRow || !(req.F(moveword::kDistToTarget) <= fVar34)) return column;
  switch (column) {
    case 2: return 0;
    case 4: return 2;
    case 6: return 4;
    case 11: return 13;
    case 13: return 15;
    case 15: return 17;
    default: return column;
  }
}

namespace {


// ---------------------------------------------------------------------------------------------
// The install data, by VA. The install test checks their relations.
// ---------------------------------------------------------------------------------------------
struct FloatSource {
  uint64_t va;
  float MoveTurnConstants::* field;
};
constexpr FloatSource FLOAT_SOURCES[] = {
    {0x142565600, &MoveTurnConstants::half},
    {0x142565604, &MoveTurnConstants::two},
    {0x1425656c0, &MoveTurnConstants::four},
    {0x1425656d0, &MoveTurnConstants::negZero},
    {0x14256ae1c, &MoveTurnConstants::one},
    {0x14256ae34, &MoveTurnConstants::thousand},
    {0x142588218, &MoveTurnConstants::negOne},
    {0x14258c264, &MoveTurnConstants::three},
    {0x142592228, &MoveTurnConstants::quarter},
    {0x14259222c, &MoveTurnConstants::five},
    {0x142598578, &MoveTurnConstants::pointTwo},
    {0x14259857c, &MoveTurnConstants::speed26},
    {0x142598580, &MoveTurnConstants::halfTurn},
    {0x14259858c, &MoveTurnConstants::fullTurn},
    {0x142598598, &MoveTurnConstants::negFullTurn},
    {0x14259a764, &MoveTurnConstants::fps},
    {0x14259bde4, &MoveTurnConstants::turnTicksB},
    {0x14259bdec, &MoveTurnConstants::turnTicks},
    {0x14259bdf4, &MoveTurnConstants::speedA},
    {0x14259bdfc, &MoveTurnConstants::speedB},
    {0x14259be04, &MoveTurnConstants::speedC},
    {0x14259be0c, &MoveTurnConstants::speed15},
    {0x14259be10, &MoveTurnConstants::speedD},
    {0x14259be1c, &MoveTurnConstants::stampsA},
    {0x14259be20, &MoveTurnConstants::speedE},
    {0x14259be2c, &MoveTurnConstants::angle40},
    {0x14259be30, &MoveTurnConstants::angle45},
    {0x14259be3c, &MoveTurnConstants::angle5625},
    {0x14259be4c, &MoveTurnConstants::seventy},
    {0x14259be50, &MoveTurnConstants::turn7875},
    {0x14259be54, &MoveTurnConstants::turn84375},
    {0x14259be5c, &MoveTurnConstants::angle90},
    {0x14259be60, &MoveTurnConstants::angle135},
    {0x14259be70, &MoveTurnConstants::angle225},
    {0x14259ce38, &MoveTurnConstants::kmhToRate},
    {0x14259d618, &MoveTurnConstants::angle225m},
    {0x1425a1ffc, &MoveTurnConstants::speed135},
    {0x1425a2004, &MoveTurnConstants::speed28},
    {0x1425ab120, &MoveTurnConstants::gFloor},
    {0x1425ab124, &MoveTurnConstants::playbackFloor},
    {0x1425c48a0, &MoveTurnConstants::turnRamp},
    {0x1425c48c0, &MoveTurnConstants::speed22},
    {0x1425c4900, &MoveTurnConstants::neg70},
    {0x1425c5958, &MoveTurnConstants::neg90},
    {0x1425c65e8, &MoveTurnConstants::speed19},
    {0x1425c8378, &MoveTurnConstants::speed55},
    {0x1425cb30c, &MoveTurnConstants::angle1125},
    {0x1425cc680, &MoveTurnConstants::angle1125b},
    {0x1425cc688, &MoveTurnConstants::angle3375},
    {0x1425cca34, &MoveTurnConstants::angle675},
    {0x1425cca3c, &MoveTurnConstants::angle1575},
    {0x1425ddf88, &MoveTurnConstants::speed1475},
    {0x1425ddf9c, &MoveTurnConstants::angle3325},
    {0x1425ddfa0, &MoveTurnConstants::speed39375},
    {0x1425ddfa4, &MoveTurnConstants::turn50625},
    {0x1425ddfa8, &MoveTurnConstants::turn81875},
    {0x1425ddfac, &MoveTurnConstants::turn101250},
    {0x1425ddfb4, &MoveTurnConstants::turn123750},
    {0x1425ddfb8, &MoveTurnConstants::turn140625},
    {0x1425ddfbc, &MoveTurnConstants::turn146250},
    {0x1425ddfc4, &MoveTurnConstants::turn163125},
    {0x1425ddfcc, &MoveTurnConstants::turn168750},
    {0x1425ddfd0, &MoveTurnConstants::turn174375},
    {0x1425ddffc, &MoveTurnConstants::negHalfTurn},
    {0x142588200, &MoveTurnConstants::loopBlend},
    {0x142588210, &MoveTurnConstants::ten},
    {0x142590d98, &MoveTurnConstants::blend03},
    {0x142591140, &MoveTurnConstants::thirty},
    {0x1425c4890, &MoveTurnConstants::zero},
    {0x1425a8a28, &MoveTurnConstants::rateStep},
    {0x142572348, &MoveTurnConstants::p8},
    {0x14257a190, &MoveTurnConstants::startHalf},
    {0x14257a194, &MoveTurnConstants::relax07},
    {0x14259b89c, &MoveTurnConstants::displace04},
    {0x14259bddc, &MoveTurnConstants::band15m},
    {0x1425ddf4c, &MoveTurnConstants::westBlend},
    {0x1425ddf50, &MoveTurnConstants::clipBlend},
    {0x1425ddf78, &MoveTurnConstants::gLow},
    {0x1425ddf80, &MoveTurnConstants::gHigh},
    {0x1425ddfe0, &MoveTurnConstants::gSlope},
};



// ---------------------------------------------------------------------------------------------
// FUN_1406b8da0(x) then the 45-degree step PES indexes the bucket tables with: the angle folded
// onto (-180, 180] and divided by 45 degrees into 0..9.
int BucketIndex(const MoveTurnConstants& k, float angleDeg) {
  const float folded = WrapHalf(angleDeg, k);
  const float v = folded < 0.0f ? folded + k.fullTurn : folded;
  const int i = static_cast<int>(v / k.angle45);
  if (i < 0) return 0;
  if (i > 9) return 9;
  return i;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// FUN_1406c14c0: RUN_TURN_RUN, the turning run. Ported from the instruction stream (every label below is a VA in
// 0x1406c14c0..0x1406c2e6a) and checked against the decompile; the request words are PES's word indices (byte offset / 4).
//
// Stack and register aliases of the stream: R12D = clip frame F, [rsp+0x44] = row start S, [rsp+0x40] = E = min(row end, fixed
// frames) (CMOVG at 0x1406c16b9), [rsp+0x4c] = fixed frames, [rsp+0x170] / [0x168] / [0x48] = the cancel records 0 / 1 / 2
// (c1, c2, c3; the fixed frames when a record is missing), [rsp+0x50] = the row offset, [rsp+0x160] = the turn lock, XMM9 = the
// first draw d1 and later the ground speed, XMM7 = d2, XMM13 = d3, DIL = the replay flag, XMM8 = -1, XMM11 = 90, XMM15 = 22.5.
// ---------------------------------------------------------------------------------------------
namespace {
constexpr int kLiveBucket = 0x20;       // req[0x20]: the bucket the state was entered on
constexpr int kSavedSpeed = 0x22;       // req[0x22]: the speed word before the last class change
constexpr int kFootFlagWord = 0x26;     // req[0x26] byte: the foot flag (vt+0x78 - 1 < 2)
constexpr int kDwellLimitWord = 0x1b;   // req[0x1b]
constexpr int kDwellLimit2Word = 0x2e;  // req[0x2e]
constexpr int kStyleWord = 0x50;        // req[0x50] byte
constexpr int kLookDeltaWord = 5;       // req[5]
constexpr int kBodyDirectionWord = 4;   // req[4]
constexpr int kTurnWord = 6;            // req[6]
constexpr int kLookDistWord = 0x12;     // req[0x12]
constexpr int kPlaybackWord = 0x35;     // req[0x35]
constexpr int kLiveBucketAngle = 10;    // the bucket count FUN_1406b9b20 is asked for (R8D = 10)

float BitsToFloat(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}
// The immediates PES stores into the speed word req[0x1d] and the rate word req[0x4a] (mov dword ptr [rsi+0x74] / [rsi+0x128]).
const float kSpeedWord5 = BitsToFloat(0x40a00000u);
const float kSpeedWord11 = BitsToFloat(0x41300000u);
const float kSpeedWord16 = BitsToFloat(0x41800000u);
const float kSpeedWord28 = BitsToFloat(0x41e00000u);
const float kRate015 = BitsToFloat(0x3e19999au);
const float kRate03 = BitsToFloat(0x3e99999au);
const float kRate06 = BitsToFloat(0x3f19999au);

// FUN_14042d470(d) is ticks * 60 * 60 / 1000 * d (per-tick metres to km/h); FUN_1415a1780 is a square root.
float TickMetresToKmh(const MoveHost& host, const MoveTurnConstants& k, float metresPerTick) {
  return static_cast<float>(host.TicksPerSecond()) * k.fps * k.fps / k.thousand * metresPerTick;
}

// 0x1406c1ae5..0x1406c1b1d: FUN_141eb50b0(P, v, 1) (the last tick's displacement), FUN_140581ce0(P+0x20, &a, -1) (the facing,
// negated and folded into [0, 360)), FUN_140591a10(v, &a) (the rotation of (x, z) by a) and FUN_14042d470(v[2]): the
// displacement's forward component in km/h.
// ponytail: std::sin stands for FUN_140A92C80's table sine (0x142655950; translated as ball/pes_fn_140A92C80, whose table needs a
// PesState the pes layer does not own); the answer differs in the last float bits.
float ForwardDisplacementKmh(const MoveHost& host, const MoveTurnConstants& k) {
  constexpr float kRadPerDeg = 3.14159265358979323846f / 180.0f;
  float v[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, v);
  float a = FoldUnit(host.FacingDeg(), k) * k.negOne;
  if (a < 0.0f) {
    a += k.fullTurn;
  } else if (k.fullTurn <= a) {
    a += k.negFullTurn;
  }
  const float s = std::sin(a * kRadPerDeg), c = std::sin((a + k.angle90) * kRadPerDeg);
  return TickMetresToKmh(host, k, c * v[2] - s * v[0]);
}

// 0x1406c1f0b..0x1406c1f83: the exit to the run: the rate of the class (vt+0x38), the class of that rate (vt+0x68), the speed
// word (vt+0x48) and the run entry (vt+0 = FUN_1406d2030) with the foot band.
void ExitToRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k, int foot) {
  const int cls = static_cast<int>(req.U(moveword::kClassWord));
  const int band = cls > 2 ? 2 : (cls < 0 ? 0 : cls);
  const pes::RunTables& rt = host.RunTablesView();
  const float rate = pes::BandRateFor(band, static_cast<int>(req.U(moveword::kKind)), rt);
  req.SetF(moveword::kRate, rate);
  req.SetU(moveword::kClassWord, static_cast<uint32_t>(pes::ClassForRateFor(rate, rt)));
  req.SetF(moveword::kSpeedWord,
           pes::RateToSpeedFor(rate, host.PhysiqueG(), rt, host.RunConstantsView().rampSlope) * k.kmhToRate);
  EnterRun(host, req, foot, false);
}

// 0x1406c1e9e and 0x1406c2544: the foot flag, the dwell clears and the class from the ground speed, then the exit to the run.
void ExitRunByGround(MoveHost& host, MoveReq& req, const MoveTurnConstants& k, float ground) {
  const int foot = FootBandOf(host);
  req.SetByte(kFootFlagWord, (foot - 1U < 2U) ? 1 : 0);
  req.SetU(kDwellLimitWord, 0);
  req.SetU(kDwellLimit2Word, 0);
  req.SetByte(kStyleWord, 0);
  if (k.five > ground) {
    req.SetU(moveword::kClassWord, 0);
    req.SetF(moveword::kSpeedWord, kSpeedWord5);
  } else if (k.speedB > ground) {
    req.SetF(moveword::kSpeedWord, kSpeedWord11);
    req.SetU(moveword::kClassWord, 1);
  } else {
    req.SetU(moveword::kClassWord, 2);
    req.SetF(moveword::kSpeedWord, kSpeedWord16);
  }
  ExitToRun(host, req, k, foot);
}

// 0x1406c1c41..0x1406c1c76: the tail of a clip issue made by the handler: the live words are stamped and the adjuster applies.
void StampLiveWords(MoveHost& host, MoveReq& req, float savedSpeed) {
  req.SetF(kSavedSpeed, savedSpeed);
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  req.SetU(moveword::kTurnedRow, req.U(moveword::kHeading));
  req.SetU(kLiveBucket, req.U(moveword::kMoveBucket));
  req.SetByte(moveword::kTurnLockFlag, 0);
  AdjustAngleApply(host);
}
}  // namespace

bool MotionIsRunning(int motion) { return IsRunningMotion(motion); }
bool MotionIsHalted(int motion) { return IsHaltedMotion(motion); }
bool MotionIsFading(int motion) { return IsFadingMotion(motion); }
float DisplacementForwardKmh(const MoveHost& host, const MoveTurnConstants& k) { return ForwardDisplacementKmh(host, k); }

void RunTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const pes::RunTables& rt = host.RunTablesView();
  const int F = host.CurrentClipFrames();  // (P+0xab8 vt+0x38)()
  float clipLength = host.RecordF2468();   // P+0x2468
  if (clipLength < k.gFloor) clipLength = k.one;
  const int motion = host.CurrentMotion();  // (P+0xab8 vt+0x18)()
  const float scale = k.one;
  const int S = RecordStartFrames(host, motion, scale);     // FUN_1406b1f00
  const int endRaw = RecordEndFrames(host, motion, scale);  // FUN_1406b1d00
  const bool clipMirror = req.Flag(moveword::kMirror);
  const float rowOffset = RecordOffsetDeg(host, motion, clipMirror, k);
  const int fixedFrames = RecordFixedFrames(host, motion, scale);  // FUN_1406af620
  // FUN_1406af8e0's records: c1 / c2 / c3 default to the fixed frames. The answer for index bands - 1 is discarded by PES
  // (0x1406c163b: no store).
  const int bands = host.Motion(motion).CancelCount();  // FUN_1406afbc0(..., 5)
  int c1 = fixedFrames, c2 = fixedFrames, c3 = fixedFrames;
  if (0 < bands) {
    c1 = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 0);
    if (1 < bands) {
      c2 = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 1);
      if (2 < bands) c3 = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 2);
    }
  }
  const int E = fixedFrames < endRaw ? fixedFrames : endRaw;  // 0x1406c16ae: CMOVG
  const float ticks = static_cast<float>(host.TicksPerSecond());
  const int cmd = static_cast<int>(req.U(moveword::kCommandId));
  const int kind = static_cast<int>(req.U(moveword::kKind));

  // The turn lock (0x1406c16c4..0x1406c17cf).
  bool lock = false;
  if (motion == 0xbc9 || motion == 0xc15) {
    lock = true;
    const int slot = AnimeSlot(host);
    bool released = false;
    if (slot < 0x16) {
      for (int i = 0; i < 24; ++i) {
        if (PlayerRowId(host, i) == slot) {
          released = PlayerFlagD(host, i) == 0;
          break;
        }
      }
    }
    const int stampAge = host.MatchTick() - RecordStampOf(host);
    if (!released && cmd == 5 &&
        static_cast<uint32_t>(static_cast<int>(ticks * k.speedC / k.fps + k.half)) < static_cast<uint32_t>(stampAge)) {
      released = true;
    }
    if (released) lock = false;
  }

  // The reorient window (0x1406c180f..0x1406c1953): while the facing adjuster is idle and the clip is inside [S, (S + E) / 2].
  if (!FacingAdjusterBusy(host) && S <= F && F <= (S + E) / 2) {
    const int span = static_cast<int>(static_cast<float>(E - F) / clipLength + k.half);  // CVTTSS2SI
    float turned = req.F(moveword::kTurnedRow);
    const float heading = req.F(moveword::kHeading);
    if (k.angle90 > std::fabs(Wrap180Diff(turned, heading, k))) turned = StepHeadingToPoint(turned, heading, k.angle225m, k);
    const float w2 = Wrap180Diff(req.F(moveword::kFacing), turned, k);
    if (k.angle45 > std::fabs(w2) / static_cast<float>(span)) {
      req.SetF(moveword::kTurnLockStamp, turned);
      req.SetByte(moveword::kTurnLockFlag, 1);
      req.SetU(moveword::kTurnLockTicks, static_cast<uint32_t>(span));
      req.SetByte(moveword::kTurnLockDir, 0);
      req.SetF(moveword::kTurnedRow, turned);
      if (k.angle90 < std::fabs(rowOffset)) {
        const float signOffset = 0.0f > rowOffset ? k.negOne : k.one;
        const float signDraw = 0.0f > w2 ? k.negOne : k.one;
        if (signOffset != signDraw && std::fabs(w2) > k.angle225m) req.SetByte(moveword::kTurnLockDir, 1);
      }
      host.SetRecordF0bec(FoldUnit(turned, k));  // FUN_1406cbb00
    }
  }

  // The three draws (0x1406c1957..0x1406c19be) and the replay flag (DIL).
  const float d1 = std::fabs(Wrap180Diff(req.F(moveword::kTurnedRow), req.F(moveword::kHeading), k));
  const float d2 = std::fabs(Wrap180Diff(req.F(moveword::kTurnedRow), req.F(moveword::kLookHeading), k));
  const float d3 = std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kLookHeading), k));
  bool replay = false;
  if (!(d2 < k.angle90) && !(d2 + k.angle225m < d3) && req.U(moveword::kPrecise) == 1) {
    replay = true;
    if (IsRunningMotion(motion) && k.angle225m > d1) replay = !(F < c2);
  }

  // 0x1406c1a10..0x1406c1adc: which arm runs. A held lock, a command 0x11 / 0x72 or a clip of the running or halted sets go to
  // the ladder; any other clip goes there once it is past its first frames and its first cancel record (scaled by
  // DAT_142588200) is behind it.
  bool toLadder = cmd == 0x11 || (host.AnimeState() == 0x4a && cmd == 0x72) || lock || IsRunningMotion(motion) ||
                  IsHaltedMotion(motion);
  if (!toLadder) {
    const int threshold = static_cast<int>(ticks * k.turnTicks / k.fps + k.half);
    if (F > S + 1 || F >= threshold) toLadder = !(static_cast<float>(c1) * k.loopBlend > static_cast<float>(F));
  }

  if (!toLadder) {
    // 0x1406c1ae5: the arm for a clip the ladder does not own.
    const float forward = ForwardDisplacementKmh(host, k);
    const float turnAbs = std::fabs(req.F(kTurnWord)), lookAbs = std::fabs(req.F(kLookDeltaWord));
    if (d1 >= k.angle5625 || replay || (k.angle1125b > turnAbs && k.angle1125b > lookAbs)) {
      ExitRunByGround(host, req, k, forward);  // 0x1406c1e9e
      return;
    }
    const int cls = static_cast<int>(req.U(moveword::kClassWord));
    const int band = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    const float startAtFrame = (static_cast<float>(F) + 1.0f) / ticks;
    if (lookAbs > k.angle90 && cls < 3 && band == 3) {  // 0x1406c1b6c
      int issued = 0;
      if (motion == 0xbc7 && k.angle1125b > d1) {
        issued = 0xbe8;
      } else if (motion == 0xbd0 && k.angle1125b > d1) {
        issued = 0xbea;
      } else if (motion == 0xbd1 && k.angle1125b > d1) {
        issued = 0xbeb;
      }
      if (issued != 0) {
        IssueClip(host, issued, clipMirror, k.turnRamp, req.F(kPlaybackWord), startAtFrame);
        const float savedSpeed = req.F(moveword::kSpeedWord);
        req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
        req.SetF(moveword::kSpeedWord, kSpeedWord28);
        req.SetU(moveword::kClassWord, 3);
        StampLiveWords(host, req, savedSpeed);
        return;
      }
      // 0x1406c1d19: no clip of its own: the run-turn entry by the forward speed.
      if (k.five > forward) {
        req.SetF(moveword::kSpeedWord, kSpeedWord5);
        req.SetU(moveword::kClassWord, 0);
      } else if (k.speedB > forward) {
        req.SetF(moveword::kSpeedWord, kSpeedWord11);
        req.SetU(moveword::kClassWord, 1);
      } else {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kSpeedWord, kSpeedWord16);
      }
      EnterRunTurnRun(host, req, k);  // FUN_1406d6780
      return;
    }
    // 0x1406c1d89: the 0xc0b clip's second turn.
    if (motion == 0xc0b && d1 > k.angle225m && turnAbs > k.angle135 && lookAbs > k.angle135) {
      const int next = (turnAbs > k.angle1575 && lookAbs > k.angle1575) ? 0xc1c : 0xc10;
      IssueClip(host, next, clipMirror, k.turnRamp, req.F(kPlaybackWord), startAtFrame);
      const float savedSpeed = req.F(moveword::kSpeedWord);
      req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
      req.SetF(moveword::kSpeedWord, kSpeedWord16);
      req.SetU(moveword::kClassWord, 2);
      StampLiveWords(host, req, savedSpeed);
      return;
    }
  }

  // 0x1406c1f9a: the ladder over the live clip. XMM9 = the ground speed of the last tick's displacement.
  float ground;
  {
    float v[3] = {0.0f, 0.0f, 0.0f};
    host.PositionAt(1, v);
    ground = TickMetresToKmh(host, k, Length3(v[0], v[1], v[2]));
  }
  const int cmdClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const float w = Wrap180Diff(req.F(moveword::kFacing), req.F(kBodyDirectionWord), k);  // FUN_14043a0a0(req[2], req[4])
  const float wAbs = std::fabs(w);
  if (F < c1) {  // 0x1406c2005
    if (motion != 0xbea || cmdClass != -1 || !(wAbs > k.angle45)) return;
    req.SetU(moveword::kPrevBucket, req.U(kLiveBucket));
    req.SetU(kLiveBucket, static_cast<uint32_t>(AngleBucket(host.AngleBuckets(), w, kLiveBucketAngle, false)));
    if (k.speedA > ground) {
      req.SetF(moveword::kSpeedWord, kSpeedWord5);
      req.SetU(moveword::kClassWord, 0);
    } else if (k.speed1475 > ground) {
      req.SetF(moveword::kSpeedWord, kSpeedWord11);
      req.SetU(moveword::kClassWord, 1);
    } else if (k.speed19 > ground) {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, kSpeedWord16);
    }
    EnterStop(host, req);  // FUN_1406d5d60
    return;
  }
  if (lock && F < c3) return;  // 0x1406c2103
  if ((host.AnimeState() == 0x4a || cmd == 0x72) && (IsRunningMotion(motion) || IsHaltedMotion(motion)) && F < fixedFrames) {
    return;  // 0x1406c2113
  }

  // 0x1406c215c: the stop ladder's flags.
  bool kindPair = false;  // BL, then R15B
  if (static_cast<int>(req.U(moveword::kPrevClassWord)) < 1 && (kind == 3 || kind == 6)) kindPair = true;
  if (IsRunningMotion(motion)) kindPair = false;
  float tolerance = k.angle45;  // [rsp+0x54]
  bool finishedClip = false;    // BL
  if (motion == 0xc1c) {
    if (F >= E) {
      finishedClip = true;
    } else {
      tolerance = k.angle225m;
    }
  }
  if (!finishedClip && (F >= E || tolerance > wAbs)) finishedClip = true;

  const int cls = static_cast<int>(req.U(moveword::kClassWord));
  auto replayBlock = [&]() {  // 0x1406c240d: the bucket from the drawn body angle, then STOP
    req.SetU(moveword::kPrevBucket, req.U(kLiveBucket));
    req.SetU(kLiveBucket, static_cast<uint32_t>(AngleBucket(host.AngleBuckets(), w, kLiveBucketAngle, false)));
    EnterStop(host, req);  // FUN_1406d5d60
  };
  if (cmdClass == -1 && !lock) {
    if (!finishedClip || cls < 1) {
      replayBlock();
      return;
    }
    req.SetU(moveword::kClassWord, 3);
    req.SetF(moveword::kSpeedWord, kSpeedWord28);
    if (k.speedA > ground) {  // 0x1406c2226
      req.SetF(moveword::kSpeedWord, kSpeedWord5);
      req.SetU(moveword::kClassWord, 0);
    } else if (k.speed1475 > ground) {
      req.SetF(moveword::kSpeedWord, kSpeedWord11);
      req.SetU(moveword::kClassWord, 1);
    } else if (k.speed19 > ground) {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, kSpeedWord16);
    }
    EnterTurnStop(host, req);  // FUN_1406d2590
    return;
  }

  // 0x1406c22b4: the tolerance of the first draw, grown by the look distance.
  float tol = k.angle45;
  if (motion == 0xbe8 && k.angle45 > std::fabs(req.F(kLookDeltaWord))) tol = k.angle675;
  const float lookDist = req.F(kLookDistWord);
  float distTerm = 0.0f;
  if (k.two < lookDist) distTerm = lookDist < k.four ? (lookDist - k.two) * k.half : k.one;
  tol += distTerm * k.angle225m;
  if (motion == 0xc1a && F < c2) tol = k.angle45;
  bool joinReplan = lock;  // 0x1406c2595: the lock, or the 0x1406c258d tests
  if (!(d1 >= tol || replay) && (!(d3 > k.angle45) || req.U(moveword::kPrecise) != 1 || F < E || F < c2)) joinReplan = true;

  if (!joinReplan) {
    if (kindPair) {  // 0x1406c23a7
      replayBlock();
      return;
    }
    if (finishedClip) {
      // 0x1406c2446: the finished clip leaves by the run-turn step, the go-to-run or the run.
      float speed = ground;
      if (motion == 0xc44 && k.speedA > speed && F < c2) speed = k.speedA;
      const bool toRunTurnStep = req.U(moveword::kPrecise) == 1 ||
                                 (k.three > lookDist && k.angle1125 > std::fabs(req.F(kTurnWord))) || k.one > lookDist;
      if (!toRunTurnStep && k.speedA > speed) {
        req.SetF(moveword::kSpeedWord, kSpeedWord5);
        req.SetU(moveword::kClassWord, 0);
        EnterGotoRun(host, req, false);  // FUN_1406ce820
        return;
      }
      if (toRunTurnStep) {
        req.SetF(moveword::kRate, kRate06);
        const float threshold = (InterfaceSpeed(host) - k.startHalf) * k.half;
        if (k.five > speed) {
          req.SetF(moveword::kRate, kRate015);
        } else if (k.speedB > speed) {
          req.SetF(moveword::kRate, kRate03);
        }
        if (EnterRunTurnStep(host, req, k, threshold)) return;  // FUN_1406d2f60
      }
      ExitRunByGround(host, req, k, speed);  // 0x1406c2544
      return;
    }
    // 0x1406c23b4: the clip is not finished: the class from the ground speed, then STEP_TURN_RUN.
    if (k.five > ground) {
      req.SetF(moveword::kSpeedWord, kSpeedWord5);
      req.SetU(moveword::kClassWord, 0);
    } else if (k.speedB > ground) {
      req.SetF(moveword::kSpeedWord, kSpeedWord11);
      req.SetU(moveword::kClassWord, 1);
    } else {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, kSpeedWord16);
    }
    if (EnterStepTurnRun(host, req, k)) return;  // FUN_1406d38d0
    replayBlock();
    return;
  }

  // 0x1406c2595: the re-plan gate: a held or zero class with a command above it goes to GOTO_RUN.
  const int saved = static_cast<int>(req.U(moveword::kPrevClassWord));
  if (cls < 1 && saved < 1 && ((cmdClass > 1 && kind != 3 && kind != 6) || cmdClass > 2) && !lock) {
    EnterGotoRun(host, req, false);  // FUN_1406ce820 (0x1406c24d2)
    return;
  }
  // 0x1406c25c5: the clip's mid frame, by motion.
  int mid = (S + E) / 2;
  switch (motion) {
    case 0xb4a: case 0xbc5: case 0xbc6: case 0xbc7: case 0xbe4: case 0xbe5: case 0xbe6: case 0xbe7: case 0xbe8: case 0xbe9:
    case 0xbfb: case 0xbfc: case 0xbfd: case 0xc0b: case 0xc0d: case 0xc0f: case 0xc10: case 0xc11: case 0xc12: case 0xc13:
    case 0xc14: case 0xc16: case 0xc29: case 0xc2c: case 0xc44: {
      const float signOffset = 0.0f > rowOffset ? k.negOne : k.one;
      const float signLook = 0.0f > req.F(kLookDeltaWord) ? k.negOne : k.one;
      mid = signOffset == signLook ? E : Share75(E);
      break;
    }
    case 0xc42:
      mid = c1;
      break;
    default:
      break;
  }

  // 0x1406c2663: past the clip's end, or past its mid frame with the first draw over 11.25 and the body within 22.5 of the move
  // direction, the facing damps toward the heading (0x1406c26ac).
  if (F >= E || (F >= mid && d1 > k.angle1125b && k.angle225m > wAbs)) {
    req.SetByte(moveword::kTurnLockFlag, 0);
    AdjustAngleApply(host);
    float yaw = FoldUnit(host.FacingDeg(), k);
    const float heading = FoldUnit(req.F(moveword::kHeading), k);
    float delta = heading - yaw;
    if (k.halfTurn < std::fabs(delta)) delta -= (0.0f > delta ? k.negOne : k.one) * k.fullTurn;
    if (k.angle90 > std::fabs(delta)) {
      const int cls2 = static_cast<int>(req.U(moveword::kClassWord));
      float limit = k.angle90;
      if ((IsRunningMotion(motion) || IsHaltedMotion(motion)) && F < E) {
        limit = k.angle225m;
      } else if (cls2 < 3 || saved < 3) {
        limit = k.halfTurn;
      } else if (!(k.speed19 > ground) || cls2 != 3 || saved != cls2) {
        limit = k.angle90;
      } else if (F >= E) {
        limit = k.angle135;
      } else {
        limit = k.speed135 > ground ? k.angle225 : k.halfTurn;
      }
      if (F < c2) {
        const float signOffset = 0.0f > rowOffset ? k.negOne : k.one;
        const float signLook = 0.0f > req.F(kLookDeltaWord) ? k.negOne : k.one;
        if (signOffset != signLook) limit *= k.quarter;
      }
      float step = limit / ticks;
      if (!(delta > step)) step = std::max(-step, delta);
      yaw += step;
      if (0.0f > yaw) {
        yaw += k.fullTurn;
      } else if (k.fullTurn <= yaw) {
        yaw += k.negFullTurn;
      }
      req.SetByte(moveword::kRootFacing, 1);
      req.SetF(moveword::kRootFacingAngle, yaw);
      if (F >= E) req.SetF(moveword::kTurnedRow, yaw);
      host.SetRecordF0bec(yaw);
    }
  }

  // 0x1406c28ce: the second-clip decisions, skipped while the turn lock holds.
  bool replay2 = false;  // DIL
  if (!lock) {
    const int cls2 = static_cast<int>(req.U(moveword::kClassWord));
    if (cls2 < 3 && saved == 3 && motion == 0xbc7 && k.angle5625 > d1 && F < c2) {
      IssueClip(host, 0xbe8, clipMirror, k.turnRamp, req.F(kPlaybackWord), static_cast<float>(F) / ticks);  // 0x1406c1bc8
      const float savedSpeed = req.F(moveword::kSpeedWord);
      req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
      req.SetF(moveword::kSpeedWord, kSpeedWord28);
      req.SetU(moveword::kClassWord, 3);
      StampLiveWords(host, req, savedSpeed);
      return;
    }
    if (tolerance > wAbs) {  // 0x1406c2929
      int earlyEnd = (E * 4) / 5;
      int lateEnd = c2;
      if (motion == 0xc42 || motion == 0xc44) {
        lateEnd = c3;
        earlyEnd = (E * 9) / 10;
      }
      if (cmdClass < cls2 && (F >= earlyEnd || F >= lateEnd)) {
        replay2 = true;
      } else {
        const float turnTol = cls2 > 1 ? k.speed39375 : k.angle45;
        const float lookTol = motion == 0xc0b ? k.angle45 : k.angle675;
        if (motion == 0xbc7 && cmdClass > 1 && F >= c2) {
          replay2 = true;
        } else if (cmdClass > cls2 && !IsRunningMotion(motion) && !IsHaltedMotion(motion)) {
          if (turnTol > std::fabs(req.F(kTurnWord)) || d1 > lookTol) {
            replay2 = true;
          } else if (cmdClass == 3) {
            replay2 = motion == 0xbc5 || (motion == 0xc1a && F >= c2);
          }
        }
      }
    } else if (cls2 < 3 && saved == 3 && F >= c2) {  // 0x1406c2ad4
      if (k.five > ground) {
        req.SetF(moveword::kSpeedWord, kSpeedWord5);
        req.SetU(moveword::kClassWord, 0);
      } else if (k.speedB > ground) {
        req.SetF(moveword::kSpeedWord, kSpeedWord11);
        req.SetU(moveword::kClassWord, 1);
      } else {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kSpeedWord, kSpeedWord16);
      }
      if (EnterStepTurnRun(host, req, k)) return;  // FUN_1406d38d0
    }
  }

  // 0x1406c2b3e: the clip is short of its fixed end and nothing asked to leave.
  if (F < fixedFrames && !replay2) return;
  const int foot = FootBandOf(host);
  req.SetByte(kFootFlagWord, (foot - 1U < 2U) ? 1 : 0);
  req.SetU(kDwellLimitWord, 0);
  req.SetU(kDwellLimit2Word, 0);
  req.SetByte(kStyleWord, 0);
  if (replay2) {
    req.SetF(moveword::kRate, kRate06);
    if (k.speedA > ground) {
      req.SetF(moveword::kRate, kRate015);
    } else if (k.speed135 > ground && (static_cast<int>(req.U(moveword::kClassWord)) < 2 || F < c2)) {
      req.SetF(moveword::kRate, kRate03);
    } else if (ground > k.speed22) {
      req.SetF(moveword::kRate, InterfaceSpeed(host) * k.playbackFloor);
    }
    const float rate = req.F(moveword::kRate);
    req.SetU(moveword::kClassWord, static_cast<uint32_t>(pes::ClassForRateFor(rate, rt)));
    req.SetF(moveword::kSpeedWord,
             pes::RateToSpeedFor(rate, host.PhysiqueG(), rt, host.RunConstantsView().rampSlope) * k.kmhToRate);
  }
  EnterRun(host, req, foot, false);  // vt+0 = FUN_1406d2030
  // 0x1406c2c64: the rate of the new class.
  const uint32_t finalClass = req.U(moveword::kClassWord);
  if (finalClass <= 1u) {
    req.SetF(moveword::kRate, kRate03);
    return;
  }
  if (finalClass == 2u) {
    req.SetF(moveword::kRate, kRate06);
    return;
  }
  req.SetF(moveword::kRate, InterfaceSpeed(host) * k.playbackFloor);
  float sample[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(static_cast<int>(ticks * k.four / k.fps + k.half), sample);
  const float sampleKmh = TickMetresToKmh(host, k, Length3(sample[0], sample[1], sample[2]));
  if (k.speedD > sampleKmh || cmdClass < 3) {
    req.SetF(moveword::kRate, kRate06);
    return;
  }
  if (finalClass != 3u || !(k.speed26 > sampleKmh)) return;
  float rate;
  if (sampleKmh > k.speedE) {
    rate = (InterfaceSpeed(host) + InterfaceSpeed(host) + k.startHalf) / k.three;
  } else if (sampleKmh > k.speed22) {
    rate = (InterfaceSpeed(host) + InterfaceSpeed(host) + InterfaceSpeed(host) + k.startHalf + k.startHalf) / k.five;
  } else if (sampleKmh > k.stampsA) {
    rate = (InterfaceSpeed(host) + k.startHalf) * k.playbackFloor;
  } else {
    rate = kRate06;
  }
  const float live = InterfaceSpeed(host);
  req.SetF(moveword::kRate, rate > live ? live : std::max(0.0f, rate));
}

// FUN_1406c2f80's slip tail and the rest of the group are ported below.


// ---------------------------------------------------------------------------------------------
// FUN_1406c2f80: RUN_TURN_RUN_SLIPDOWN, the slip tail of RUN_TURN_RUN.
//
// PES's order:
//   1. while the clip is inside (start, start + 2 * end) / 3 it re-selects the row from the
//      candidate's heading and the turn's tick count, writing req[0xf0]..req[0xfc];
//   2. once the clip's end frames have run out it damps the facing towards the move heading and
//      writes the slip pair (req[0xe8] / req[0xec]), which the adapter copies to P+0xbec;
//   3. once the sub-record's frames have run out it clears the class words and re-issues the
//      row back to the run family's own entry.
// ---------------------------------------------------------------------------------------------
void RunTurnRunSlipdown(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int clipFrame = host.CurrentClipFrames();         // (P+0xab8 vt+0x38)()
  const int motion = host.CurrentMotion();                 // (P+0xab8 vt+0x18)()
  const bool mirror = req.Flag(moveword::kMirror);
  float clipLength = host.RecordF2468();                   // P+0x2468
  if (clipLength < k.gFloor) clipLength = k.one;           // DAT_1425ab120 -> 1.0
  const float scale = k.one;
  const int rowStart = RecordStartFrames(host, motion, scale);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, mirror, k);  // FUN_1406b1c70
  const float quarter = k.angle90;  // fVar14 = DAT_14259be5c (90)
  const float negOne = k.negOne;    // fVar16 = DAT_142588218

  // 1. The re-select window (0x1406c30a7..0x1406c31fa), read from the instruction stream. The first FUN_14043a0a0 (0x1406c3100)
  // takes (candidate, req[1]) and, under 90 degrees, FUN_140a92950 steps the candidate towards req[1] by DAT_1425cc680 (11.25,
  // 0x1406c311a), not the 67.5 the old port used. The second FUN_14043a0a0 (0x1406c3138) takes (req[2] = the facing, candidate):
  // its SIGNED result is the sign test's operand (XMM4) and its magnitude the per-frame gate and the mirror gate.
  if (!FacingAdjusterBusy(host) && rowStart <= clipFrame && clipFrame < (rowStart + rowEnd * 2) / 3) {
    float candidate = req.F(moveword::kSlipCandidate);  // param_2 + 0x60
    const int frames = static_cast<int>((rowEnd - clipFrame) / clipLength + k.half);
    const float loose = std::fabs(Wrap180Diff(candidate, req.F(moveword::kHeading), k));
    if (loose < quarter) {
      candidate = StepHeadingToPoint(candidate, req.F(moveword::kHeading), k.angle1125b, k);
    }
    const float facingDiff = Wrap180Diff(req.F(moveword::kFacing), candidate, k);
    const float tight = std::fabs(facingDiff);
    if (tight / static_cast<float>(frames) < k.angle45) {  // DAT_14259be30
      req.SetF(moveword::kSlipRowOut, candidate);  // param_2 + 0xf4
      req.SetByte(moveword::kSlipNewRow, 1);       // param_2 + 0xf0
      req.SetU(moveword::kSlipRowFrames, static_cast<uint32_t>(frames));  // param_2 + 0xf8
      req.SetByte(moveword::kSlipMirrorOut, 0);                            // param_2 + 0xfc
      req.SetF(moveword::kSlipCandidate, candidate);  // param_2 + 0x60
      if (quarter < std::fabs(rowOffset)) {
        const float signOffset = rowOffset < 0.0f ? negOne : scale;
        const float signTight = facingDiff < 0.0f ? negOne : scale;
        if (signOffset != signTight) {
          uint8_t m = static_cast<uint8_t>(req.U(moveword::kSlipMirrorOut) & 0xffu);
          if (k.angle225m < tight) m = 1;  // DAT_14259d618
          req.SetByte(moveword::kSlipMirrorOut, m);
        }
      }
      host.SetRecordF0bec(FoldUnit(candidate, k));  // 0x1406c31fa: FUN_1406cbb00(P, candidate), the yaw lock
    }
  }

  // 2. The slip damping (0x1406c3206..0x1406c3345). PES writes req[0xe8]/req[0xec] and P+0xbec;
  // the contract routes P+0xbec through the host's setter, which the adapter calls from the
  // slip angle it copied.
  if (rowEnd <= clipFrame) {
    req.SetByte(moveword::kSlipNewRow, 0);  // param_2 + 0xf0 = 0
    AdjustAngleApply(host);                 // (+0x4d0 vt+0x08)(P+0x4d0, P+0xa08)
    const float facing = FoldUnit(req.F(moveword::kFacing), k);  // P+0x20
    const float heading = FoldUnit(req.F(moveword::kHeading), k);  // param_2 + 4 = req[1]
    float delta = heading - facing;
    if (k.halfTurn < std::fabs(delta)) {
      delta -= (0.0f <= delta ? k.one : k.negOne) * k.fullTurn;
    }
    if (std::fabs(delta) < quarter) {
      float limit = k.angle135;                                        // DAT_14259be60
      if (static_cast<int>(req.U(moveword::kClassWord)) < 3) limit = k.halfTurn;  // PES byte 0x7c = word 0x1f (CONFIRMED: FUN_1406c2f80 line 123; pes-re-a3helpers.md addendum item 4)
      const float ticks = static_cast<float>(host.TicksPerSecond());  // FUN_141e532d0
      float step = limit / ticks;
      // 0x1406c32e2: COMISS XMM7,XMM8; JA skip; otherwise XMM8 = max(-step, delta) (the sign flip then MAXSS), a clamp to +-step.
      if (delta <= step) {
        step = std::max(-step, delta);
      }
      float damped = facing + step;
      if (0.0f <= damped) {
        if (k.fullTurn <= damped) damped += k.negFullTurn;
      } else {
        damped += k.fullTurn;
      }
      req.SetByte(moveword::kSlipActive, 1);  // param_2 + 0xe8 = 1
      req.SetF(moveword::kSlipAngle, damped);  // param_2 + 0xec
      host.SetRecordF0bec(damped);             // PES's P+0xbec store, routed through the host
    }
  }

  // 3. The row replay (0x1406c334f..0x1406c3456). FUN_1406af620 minus one, then
  // FUN_1406afbc0(..., 5)'s bands, then FUN_1406af8e0 at the last band.
  int rowFrames = RecordFixedFrames(host, motion, scale) - 1;  // FUN_1406af620
  const pes::MotionRecord& motionRec = host.Motion(motion);
  const int bands = motionRec.CancelCount();  // FUN_1406afbc0(..., 5)
  if (0 < bands) {
    rowFrames = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, bands - 1);  // FUN_1406af8e0
  }
  if (rowFrames <= clipFrame) {
    const int foot = FootBandOf(host);  // (P+0xab8 vt+0x78)()
    req.SetByte(moveword::kFootFlag, (foot - 1U < 2U) ? 1 : 0);  // param_2 + 0x98
    req.SetU(moveword::kClampWord, 0);  // param_2 + 0x6c
    req.SetU(moveword::kDwellLimit2, 0);  // param_2 + 0xb8 (byte offset): req[0x2e]
    req.SetByte(moveword::kStyle, 0);     // param_2 + 0x140 (byte offset): req[0x50]
    EnterRun(host, req, foot, false);     // 0x1406c3441: (P+0xac0 vt+0)(P+0xac0, req, foot, 0), FUN_1406d2030
    req.SetF(moveword::kSlipRate, 0.3f);    // param_2 + 0x128 = 0x3e99999a
    req.SetU(moveword::kClassWord, 1);      // param_2 + 0x7c = 1
    req.SetF(moveword::kSlipSpeed, 11.0f);  // param_2 + 0x74 = 0x41300000
  }
}


// ---------------------------------------------------------------------------------------------
// FUN_1406c3470: RUN_TURN_STEP, the run-to-step turn. Re-ported from the decompile and the instruction stream (0x1406c3470..
// 0x1406c4c0b); every variable below is named for the PES register or decompile local it stands for. The old port differed in
// nearly every block (the window's end, the table term and the clamp of the reorient window, the words of the ladder's tests, the
// speed-word immediates, the row ids, the exits); the red tests in pes_movestep_turn_test.cpp name each.
// ---------------------------------------------------------------------------------------------
namespace {
constexpr int kRowHeadingWord = 0x17;     // req[0x17] (byte 0x5c): the row heading the reorient window turns
constexpr int kTurnedHeadingWord = 0x18;  // req[0x18] (byte 0x60): the turned heading
constexpr uint32_t kRowEchoSlow = 0x53a, kRowEchoSlowAlt = 0x53b, kRowEchoMid = 0x542, kRowEchoMidAlt = 0x543;
constexpr uint32_t kRowEchoStep = 0x52e, kRowEchoStepAlt = 0x530;
constexpr int kColumnStop = 0x11;

// FUN_1404bcad0(v, angle): (x, z) rotated by the folded angle. ponytail: std::sin stands for FUN_140a92c80's table sine, as in
// ForwardDisplacementKmh.
void RotateXZ(float v[3], float angleDeg, const MoveTurnConstants& k) {
  constexpr float kRadPerDeg = 3.14159265358979323846f / 180.0f;
  const float a = FoldUnit(angleDeg, k);
  const float cosA = std::sin((a + k.angle90) * kRadPerDeg), sinA = std::sin(a * kRadPerDeg);
  const float x = v[0], z = v[2];
  v[2] = cosA * z - sinA * x;
  v[0] = sinA * z + cosA * x;
}

// FUN_1406c3470's two copies of the step-row select, both ending in the STEP_LOOP request: the ladder's (0x1406c422f..0x1406c4ba8,
// exitGateCopy == false: it walks the clip's foot sub-records for the start time) and the exit gate's (0x1406c48a7..0x1406c4ba8:
// it widens the request's blend to 0.3 for kind 3 at a side bucket when both history steps run forward by more than 0.2 m).
// Both read the interface's bucket (+0xac0 vt+0x60), take the class and speed word, resolve the step row at the bucket's column and
// issue the clip through vt+0x10. The row ids are the integers behind the decompile's float literals.
void RunTurnStepIssueStepRow(MoveHost& host, MoveReq& req, const MoveTurnConstants& k, float dispSpeed, bool exitGateCopy) {
  const int bucket = InterfaceBucket(host, req.F(moveword::kHeading));  // (+0xac0 vt+0x60)()
  const int liveClass = static_cast<int>(req.U(moveword::kClassWord));
  const float floorSpeed = liveClass < 2 ? k.five : k.speedB;  // fVar25 / fVar7
  const int below = 1 < liveClass ? 1 : 0;                       // uVar34 / uVar15
  int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  float want = req.F(moveword::kDesiredSpeedKmh);
  if (cls < 2 || k.speedB <= dispSpeed) {
    if (cls < 3) {
      if (cls < below) cls = below;
    } else {
      cls = 2;  // LAB_1406c422f is skipped: no max with `below`
    }
  } else {
    cls = 1;
    want = k.speedB;
    if (cls < below) cls = below;
  }
  int kmh = static_cast<int>(want);
  if (kmh < 0x11) {
    if (kmh < static_cast<int>(floorSpeed)) kmh = static_cast<int>(floorSpeed);
  } else {
    kmh = 0x10;
  }
  uint32_t kind = req.U(moveword::kKind);
  if (kind < 2) {
    kind = 2;
  } else if (kind == 4) {
    kind = 5;
  }
  const int column = k.bucketColumn[bucket];
  uint32_t row = static_cast<uint32_t>(k.stepRow[kind + static_cast<uint32_t>(cls) * 8u]);
  const bool edgeColumn = column == kColumnStop || column == 0;
  if (edgeColumn && row == kRowEchoSlow) {
    if (host.Percent(0x32)) row = kRowEchoSlowAlt;  // FUN_1406ebf30(P, 0x32)
  } else if (edgeColumn && row == kRowEchoMid) {
    if (host.Percent(0x14)) row = kRowEchoMidAlt;  // FUN_1406ebf30(P, 0x14)
  } else if (row == kRowEchoStep) {
    row = kRowEchoStepAlt;
  }
  const RowPick cell = PickRow(host, static_cast<int>(row), column);  // FUN_1406abb30 / FUN_1406abd60
  const uint16_t repMotion = static_cast<uint16_t>(cell.motion);
  req.SetByte(moveword::kFootFlag, k.bucketLeft[bucket]);
  req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
  req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
  req.SetF(moveword::kSpeedWord, static_cast<float>(kmh));
  req.SetU(moveword::kPrevBucket, req.U(kLiveBucket));
  req.SetU(kLiveBucket, static_cast<uint32_t>(bucket));
  req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));
  const float playback = PlaybackAt(host, k);  // FUN_1406ba070
  req.SetF(moveword::kPlayback, playback);
  float start = host.MotionStartTime(repMotion);  // FUN_1406b97d0
  float blend = k.clipBlend;
  if (!exitGateCopy) {
    if (column >= 0 && column < 0x12 && ((0x22011u >> (column & 0x1f)) & 1u) != 0u) {
      // The foot sub-record walk (0x1406c4580..0x1406c4650): the sub's own foot pair, mirrored when the row is mirrored, and
      // the frames the matching sub gives.
      const int footNow = FootBandOf(host);
      const int subCount = host.Motion(repMotion).FootCount();
      for (int i = 0; i < subCount; ++i) {
        int sub = host.Motion(repMotion).FootPair(i);
        if (cell.mirror) {
          if (sub == 0) sub = 2;
          else if (sub == 1) sub = 3;
          else if (sub == 2) sub = 0;
          else if (sub == 3) sub = 1;
        }
        if (footNow == sub) {
          const int back = host.ClipTicks(pes::ClipReader::kCancel, repMotion, k.one, i);
          start = static_cast<float>(back + 1) / static_cast<float>(host.TicksPerSecond());
          break;
        }
      }
    }
  } else if (req.U(moveword::kKind) == 3 && (bucket == 2 || bucket == 7)) {
    // 0x1406c4a13..0x1406c4afd: the history samples 3 and 6 (FUN_141eb51a0) less the position, each turned by the facing
    // (FUN_1404bcad0 at -facing); both forward components over 0.2 m widen the blend.
    float own[3], a[3], b[3];
    host.OwnPosition(own);
    host.PositionAt(3, a);
    host.PositionAt(6, b);
    for (int i = 0; i < 3; ++i) {
      a[i] -= own[i];
      b[i] -= own[i];
    }
    RotateXZ(a, req.F(moveword::kFacing) * k.negOne, k);
    RotateXZ(b, req.F(moveword::kFacing) * k.negOne, k);
    if (k.pointTwo < a[2] && k.pointTwo < b[2]) blend = k.blend03;
  }
  IssueClip(host, repMotion, cell.mirror, blend, playback, start);
  req.SetU(moveword::kDwellElapsed, 0);  // req[0x1a], req[0x1b] (qword at +0x68)
  req.SetU(moveword::kDwellLimit, 0);
  req.SetU(moveword::kPrevKind, kind);
  RequestState(req, MoveState::kStepLoop);  // LAB_1406c4ba8: req[0x39] = 4, req[0x38] = 1
}
}  // namespace

void RunTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const float scale = k.one;
  const float ticks = static_cast<float>(host.TicksPerSecond());  // FUN_141e532d0
  const int F = host.CurrentClipFrames();                         // (P+0xab8 vt+0x38)(), R15D
  const int motion = host.CurrentMotion();                        // (P+0xab8 vt+0x18)()
  const float clipLength = host.RecordF2468();                    // P+0x2468: no floor in this handler (0x1406c34da)
  const int rowStart = RecordStartFrames(host, motion, scale);    // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale);        // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, req.Flag(moveword::kMirror), k);  // FUN_1406b1c70
  const int fixed = RecordFixedFrames(host, motion, scale);       // FUN_1406af620
  const int bands = host.Motion(motion).CancelCount();            // FUN_1406afbc0(..., 5)
  const float dispTop = DispSpeedKmh(host, k);                    // [RBP+0x88]
  const pes::RunTables& runTables = host.RunTablesView();

  // The sub-record frames (0x1406c3632..0x1406c36d0): c0 = band 0, c1 = band 1 (the fixed frames when absent), fixedMax = the
  // larger of the fixed frames and the last band.
  int fixedMax = fixed, c0 = fixed, c1 = fixed;
  if (0 < bands) {
    const int last = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, bands - 1);
    c0 = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 0);
    if (1 < bands) {
      c1 = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 1);
      if (2 < bands) (void)host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 2);
    }
    if (fixedMax < last) fixedMax = last;
  }
  // 0x1406c36d4..0x1406c3749: a row with no offset starts at 0 and ends after the tenth of a second; the end never passes the
  // fixed frames (CMOVG: min, not max). `grace` is the frame the exit gate waits for.
  int start = rowStart, end = rowEnd;
  if (rowOffset == 0.0f) {
    start = 0;
    end = static_cast<int>(ticks * k.ten / k.fps + k.half);
  }
  if (end > fixedMax) end = fixedMax;
  int grace = fixedMax;
  if (motion == 0xb51) {
    grace = end;
  } else if (motion == 0xb39) {
    grace = c1;
  }

  // 0x1406c3749..0x1406c37e0: the rate clamp. The table rate is the class's (vt+0x38) when req[0xb] is set, else the rate of the
  // desired speed (FUN_1406b9e80 with the kind), and 0 for a halted class 0 at kind 3 or 6.
  {
    const int kind = static_cast<int>(req.U(moveword::kKind));
    float tableRate;
    if (req.Flag(moveword::kUseClassRate)) {
      const int cls = static_cast<int>(req.U(moveword::kClassWord));
      tableRate = pes::BandRateFor(cls > 2 ? 2 : (cls < 0 ? 0 : cls), kind, runTables);
    } else if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == 0 && (kind == 3 || kind == 6)) {
      tableRate = 0.0f;
    } else {
      tableRate = movestep::SpeedToRate(host, req.F(moveword::kDesiredSpeedKmh), kind);
    }
    const float rate = req.F(moveword::kRate);
    if (rate > tableRate) {
      const float live = host.RateCeiling();  // (+0xac0 vt+0x18)()
      const float lowered = rate - k.rateStep;
      req.SetF(moveword::kRate, lowered > live ? live : std::max(tableRate, lowered));
    } else {
      const float grown = rate + k.rateStep;
      req.SetF(moveword::kRate, grown > tableRate ? tableRate : std::max(k.zero, grown));
    }
  }

  const float narrow = k.angle225m;  // fVar28
  const float quarter = k.angle90;   // fVar32
  const float full = k.fullTurn;     // fVar8
  float turnTol = k.angle45;         // fVar22

  // 0x1406c381f..0x1406c39cd: the reorient window. The heading is turned towards (req[1] less the bucket's table angle) by
  // DAT_14259d618 (22.5, not 67.5), the turned heading adds the table angle back, and the sign test compares the row offset with
  // the sign of FUN_14043a0a0(req[2], rowAngle).
  if (!FacingAdjusterBusy(host) && start <= F && F < (start + end * 2) / 3) {
    float rowAngle = req.F(kRowHeadingWord);
    const int frames = static_cast<int>(static_cast<float>(end - F) / clipLength + k.half);
    const float tableAngle = k.bucketAngle[req.U(kLiveBucket)];
    const float toward = req.F(moveword::kHeading) - tableAngle;
    if (std::fabs(Wrap180Diff(rowAngle, toward, k)) < quarter) {
      rowAngle = StepHeadingToPoint(rowAngle, toward, narrow, k);
    }
    if (full < rowAngle) rowAngle = std::fmod(rowAngle, full);  // FUN_1415a174a (a tail jump through DAT_142530198; INF: fmodf)
    float turned = rowAngle + tableAngle;
    if (0.0f > turned) {
      turned += full;
    } else if (!(turned < full)) {
      turned += k.negFullTurn;
    }
    const float facingDiff = Wrap180Diff(req.F(moveword::kFacing), rowAngle, k);
    if (std::fabs(facingDiff) / static_cast<float>(frames) < k.angle45) {
      req.SetByte(moveword::kReorientFlag, 1);
      req.SetU(moveword::kTurnFrames, static_cast<uint32_t>(frames));
      req.SetByte(moveword::kMirrorFlag, 0);
      req.SetF(moveword::kAngle3d, rowAngle);
      req.SetF(kTurnedHeadingWord, turned);
      if (quarter < std::fabs(rowOffset)) {
        const float signOffset = rowOffset < 0.0f ? k.negOne : scale;
        const float signDiff = facingDiff < 0.0f ? k.negOne : scale;
        if (signOffset != signDiff) {
          uint8_t m = static_cast<uint8_t>(req.U(moveword::kMirrorFlag) & 0xffu);
          if (narrow < std::fabs(facingDiff)) m = 1;
          req.SetByte(moveword::kMirrorFlag, m);
        }
      }
    }
  }

  // 0x1406c39d8..0x1406c3b3d: the loop-branch replay inside the first tenth of a second: the run entry (FUN_1406d2030) with a
  // rate that follows the displacement speed, the class of that rate and its speed word.
  if (F < static_cast<int>(ticks * k.turnTicks / k.fps + k.half) &&
      std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k)) < narrow &&
      req.U(moveword::kCommandId) != 0x11 && req.U(moveword::kPrecise) == 0 &&
      2 < static_cast<int>(req.U(moveword::kCommandSpeedClass))) {
    const int foot = FootBandOf(host);  // (P+0xab8 vt+0x78)()
    req.SetF(moveword::kRate, k.startHalf);
    req.SetByte(moveword::kFootFlag, (foot - 1U < 2U) ? 1 : 0);
    req.SetU(moveword::kClampWord, 0);
    req.SetU(moveword::kDwellLimit2, 0);
    req.SetByte(moveword::kStyle, 0);
    if (k.speedA <= dispTop) {
      if (k.speed135 <= dispTop) {
        if (k.speed22 < dispTop) req.SetF(moveword::kRate, host.RateCeiling() * k.playbackFloor);
      } else {
        req.SetF(moveword::kRate, k.blend03);
      }
    } else {
      req.SetF(moveword::kRate, k.turnRamp);
    }
    req.SetU(moveword::kClassWord, static_cast<uint32_t>(pes::ClassForRateFor(req.F(moveword::kRate), runTables)));  // vt+0x68
    req.SetF(moveword::kSpeedWord,
             pes::RateToSpeedFor(req.F(moveword::kRate), host.PhysiqueG(), runTables, host.RunConstantsView().rampSlope) * k.kmhToRate);  // vt+0x48
    EnterRun(host, req, foot, false);  // vt+0
    req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
    return;
  }

  // 0x1406c3b42..0x1406c3c20: the turn tolerance.
  const float faceAbs = std::fabs(Wrap180Diff(req.F(kTurnedHeadingWord), req.F(moveword::kHeading), k));  // local_res20
  if (req.U(moveword::kPrecise) == 0 && 0 < static_cast<int>(req.U(moveword::kClassWord))) {
    const float dist = req.F(moveword::kDistToTarget);
    if (k.two < dist) {
      turnTol = dist < k.four ? (dist - k.two) * k.half * narrow + turnTol : k.one * narrow + turnTol;
    } else {
      turnTol = narrow * 0.0f + turnTol;
    }
  } else if (req.U(moveword::kPrecise) == 1 && k.angle1125 < std::fabs(req.F(moveword::kFacingError)) && F < end) {
    turnTol = k.angle675;
  }
  const float bodyDelta = std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k));
  const bool near = motion == 0xbd6 ? bodyDelta < k.angle40 : bodyDelta < k.angle45;
  const bool openWindow = F < static_cast<int>(ticks * k.turnTicks / k.fps + k.half);

  auto setClassSpeed = [&req](uint32_t cls, float speedWord) {
    req.SetU(moveword::kClassWord, cls);
    req.SetF(moveword::kSpeedWord, speedWord);
  };

  // 0x1406c3c42..0x1406c4220: the ladder, skipped while the interface plays the scripted state 0x4a.
  if (host.AnimeState() != 0x4a) {
    if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1) {
      // An unset command class: stop (FUN_1406d2590 with the class written, FUN_1406d5d60 otherwise).
      if (near && k.speedA <= dispTop) {
        setClassSpeed(3, kSpeedWord28);
        if (dispTop < k.speed1475) {
          setClassSpeed(1, kSpeedWord11);
          EnterTurnStop(host, req);
          return;
        }
        if (dispTop < k.speed19) setClassSpeed(2, kSpeedWord16);
        EnterTurnStop(host, req);
        return;
      }
      req.SetU(moveword::kPrevBucket, req.U(kLiveBucket));
      req.SetU(kLiveBucket, static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kHeading))));  // (+0xac0 vt+0x60)()
      EnterStop(host, req);  // FUN_1406d5d60
      return;
    }
    if ((turnTol < faceAbs || (req.U(moveword::kPrecise) == 0 && c0 <= F)) && (openWindow || c0 <= F)) {
      if (0 <= static_cast<int>(req.U(moveword::kClassWord))) {
        float disp = DispSpeedKmh(host, k);  // fVar28, recomputed (0x1406c3d5f)
        const int cur = host.CurrentMotion();
        float thr = k.four;  // fVar25
        if (cur == 0xb4c) {
          if (c0 <= F && disp < thr) {
            const float turn = std::fabs(req.F(moveword::kTurn));
            if (!(quarter < turn) && !(quarter == turn)) disp = k.five;
          }
        } else if (cur == 0xbcf) {
          if (c0 <= F && k.angle135 < bodyDelta) thr = scale;
        } else if (cur == 0xbd6) {
          if ((c1 + c0) / 2 < F) thr = scale;
        } else if (cur == 0xc02) {
          if (c0 < F) thr = scale;
        } else if (cur == 0xc03 && c0 <= F) {
          if (!(thr < disp) && !(thr == disp)) disp = k.five;
        }
        if (quarter < std::fabs(req.F(moveword::kFacingError)) && req.F(moveword::kDistToTarget) <= k.blend03 &&
            req.F(moveword::kDistToTarget) != k.blend03) {
          if (disp < k.speedA) {
            setClassSpeed(0, kSpeedWord5);
          } else if (disp < k.speed135) {
            setClassSpeed(1, kSpeedWord11);
          } else {
            setClassSpeed(2, kSpeedWord16);
          }
          EnterStop(host, req);  // FUN_1406d5d60
          return;
        }
        if (disp < thr) {  // LAB_1406c3f2b
          setClassSpeed(0, kSpeedWord5);
          EnterGotoRun(host, req, false);  // FUN_1406ce820
          return;
        }
        auto bandWords = [&] {
          if (k.five <= disp) {
            if (k.speedB <= disp) {
              setClassSpeed(2, kSpeedWord16);
            } else {
              setClassSpeed(1, kSpeedWord11);
            }
          } else {
            setClassSpeed(0, kSpeedWord5);
          }
        };
        if (near) {
          bandWords();
          if (req.U(moveword::kPrecise) == 1 &&
              k.turn50625 < std::fabs(Wrap180Diff(req.F(moveword::kLookHeading), req.F(moveword::kHeading), k))) {
            if (EnterRunTurnStep(host, req, k, (host.RateCeiling() - k.startHalf) * k.half)) return;  // FUN_1406d2f60
            EnterTurnStop(host, req);  // FUN_1406d2590
            return;
          }
          if (static_cast<int>(req.U(moveword::kClassWord)) < 2 && static_cast<int>(req.U(moveword::kCommandSpeedClass)) == 3 &&
              k.angle45 < faceAbs && disp < k.speed22 && c0 <= F) {
            setClassSpeed(1, kSpeedWord11);
            float share = 0.0f;  // fVar33
            float rate;
            if (disp <= 0.0f || (disp < k.speed28 && (share = disp / k.speed28, share <= k.relax07))) {
              rate = k.turnRamp <= share ? share : k.turnRamp;
            } else {
              rate = k.relax07;
            }
            req.SetF(moveword::kRate, rate);
            if (EnterAccelerate(host, req)) return;  // FUN_1406ccd00
          }
          EnterRunTurnRun(host, req, k);  // FUN_1406d6780
          return;
        }
        bandWords();
        if (EnterStepTurnRun(host, req, k)) return;  // FUN_1406d38d0
        if (disp < thr + k.three && quarter < std::fabs(req.F(moveword::kTurn))) {
          setClassSpeed(0, kSpeedWord5);
          EnterGotoRun(host, req, false);  // FUN_1406ce820
          return;
        }
        AdjustAngleApply(host);  // (+0x4d0 vt+0x08)
        req.SetByte(moveword::kReorientFlag, 0);
        RunTurnStepIssueStepRow(host, req, k, disp, /*exitGateCopy=*/false);
        return;
      }
      if (std::fabs(req.F(moveword::kFacingError)) < k.angle675) {  // 0x1406c3c46: the look-at gate
        EnterStop(host, req);  // FUN_1406d5d60
        return;
      }
    }
  }

  // 0x1406c4220..0x1406c45c9: the facing write. The heading is shifted by the live bucket (a quarter turn at buckets 7 and 2, a
  // half turn otherwise), the step follows the look-at point's distance from it, and req[0x3a]/[0x3b] carry the new facing.
  if (end <= F && (rowOffset != 0.0f || c0 <= F)) {
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    const uint32_t liveBucket = req.U(kLiveBucket);
    const float shift = liveBucket == 7 ? quarter : (liveBucket == 2 ? -quarter : -k.halfTurn);
    const float heading = FoldUnit(req.F(moveword::kHeading) + shift, k);
    float toFacing = heading - req.F(moveword::kFacing);
    if (k.halfTurn < std::fabs(toFacing)) toFacing -= (toFacing < 0.0f ? k.negOne : scale) * full;
    float toLook = heading - FoldUnit(req.F(moveword::kLookHeading), k);
    if (k.halfTurn < std::fabs(toLook)) toLook -= (toLook < 0.0f ? k.negOne : scale) * full;
    toLook = std::fabs(toLook);
    float blend = 0.0f;  // fVar33
    if (narrow < toLook) {
      blend = scale;
      if (toLook < k.angle45) blend = (toLook - narrow) / narrow;
    }
    const float step = (blend * quarter + quarter) / ticks;
    float turn = step;
    if (toFacing <= step) {
      turn = -step;
      if (-step <= toFacing) turn = toFacing;
    }
    req.SetByte(moveword::kRootFacing, 1);
    req.SetF(moveword::kRootFacingAngle, FoldUnit(turn + req.F(moveword::kFacing), k));
  }

  // 0x1406c45d0..0x1406c4c0b: the exit gate, then the exit gate's copy of the step-row select.
  bool exitNow = false;
  if (c0 <= F) {
    const uint32_t precise = req.U(moveword::kPrecise);
    const int cmd = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    exitNow = (precise == 0 && cmd == 3) ||
              (cmd < 1 && std::fabs(Wrap180Diff(req.F(kTurnedHeadingWord), req.F(moveword::kBodyDirection), k)) < narrow) ||
              (precise == 1 && k.angle45 < std::fabs(req.F(moveword::kFacingError)) && end <= F) ||
              (precise == 0 && quarter < std::fabs(req.F(moveword::kLookDelta)) && end <= F);
  }
  if (F < grace && !exitNow) return;
  AdjustAngleApply(host);
  req.SetByte(moveword::kReorientFlag, 0);
  RunTurnStepIssueStepRow(host, req, k, dispTop, /*exitGateCopy=*/true);
}

// ---------------------------------------------------------------------------------------------
// FUN_1406d6780: the RUN_TURN_RUN entry.
//
// PES's order:
//   1. the refusal gate: a missing speed word, a class below zero, or a bucket the run family
//      cannot start from hands the tick to the TURN_STOP entry;
//   2. the crowd block: the near player's angle decides a re-select draw;
//   3. the row ladder keyed on the live class word and the speed word, with the jostle block;
//   4. the clip request, the class/speed pair and the transition into RUN_TURN_RUN.
// ---------------------------------------------------------------------------------------------
void EnterRunTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int slot = AnimeSlot(host);
  const int rowNow = AnimeRow(host, slot);
  const bool mirrorSlot =
      rowNow < 0x16 && static_cast<int>(req.U(moveword::kClassWord)) == rowNow;
  req.SetF(moveword::kPlayback, pes::TurnPlayback(host.PhysiqueG(), host.MirrorChoice(false) ? 1.0f : 0.5f,
                                                  host.PlaybackConstantsView()));  // vt+0x50: a float store
  const int speedWord = static_cast<int>(req.U(moveword::kClassWord));
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int bucket = static_cast<int>(req.U(moveword::kMoveBucket));
  const int speedClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  const bool refuses = (speedWord < 1 && (speedWord < 0 || kind == 3 || kind == 6)) ||
                       (speedClass < 0 || bucket == 0 || bucket == 5);
  if (refuses) {
    EnterTurnStop(host, req);  // LAB_1406d8523
    return;
  }
  const float playback = req.F(moveword::kPlayback);
  const bool relax = host.EntryAimFlag();  // FUN_1406ba7d0
  const int sinceStamp = host.MatchTick() - RecordStampOf(host);
  float out[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(2, out);  // FUN_140a6cf70 + FUN_140439ef0
  const float ticks = static_cast<float>(host.TicksPerSecond());
  float span = k.four;  // fVar25 = DAT_1425656c0
  const float bodyBand = k.angle675;  // fVar28 = DAT_1425cca34
  bool reselect = false;              // local_res18[0]
  float scale = 0.0f;                 // fVar26
  float relaxSpan = 0.0f;             // fVar34
  int row = AnimeRow(host, slot);     // FUN_140a627e0(..., 0)
  int motion = 0xbe5;

  if (row < 0x16) {
    // The crowd block (LAB_1406d6c1b's near arm): the distance, the scripted flag, the two angle
    // tests and then the re-select draw.
    float other[3] = {0.0f, 0.0f, 0.0f};
    PlayerPosition(host, row, other);  // FUN_140a48a10: +0x55c / +0x564
    if (Length3(out[0] - other[0], 0.0f, out[2] - other[2]) < k.five) {
      const bool scripted = PlayerByte(host, row, 1) == 0x12;  // lVar22 + 1 == 0x12
      const bool here = 10 < slot && slot < 0x16;
      if (here != (10 < row)) {
        const float a = std::fabs(Wrap180Diff(out[0], other[0], k));
        const float b = std::fabs(Wrap180Diff(out[2], other[2], k));
        if (a < bodyBand && b <= k.angle45) {
          const float len = Length3(out[0], out[1], out[2]) * k.kmhToRate;
          const float d = std::fabs(static_cast<double>(len - b * k.kmhToRate));
          if (d <= static_cast<double>(k.stampsA)) {
            scale = 0.0f;
          } else if (sinceStamp >= static_cast<int>((ticks * k.stampsA) / k.fps + k.half)) {
            scale = 0.0f;
          } else {
            // LAB_1406d6c22: the re-select draw.
            const float turn = req.F(moveword::kTurn);
            int pct = 2;
            if (turn <= k.turn7875) {
              if (speedWord < 3) pct = 0;
              if (scripted && (pct = 7, speedWord < 3)) pct = 2;
            } else {
              pct = 4;
              if (speedWord < 3) pct = 0;
              if (scripted && (pct = 15, speedWord < 3)) pct = 5;
            }
            if (relax) {
              pct = 0x19;
              if (speedWord < 3) pct = 5;
              if (scripted && (pct = 0x2d, speedWord < 3)) pct = 0xf;
            }
            if (host.Percent(pct)) reselect = true;
            goto LAB_1406d6c22;
          }
        }
      }
    }
    scale = 0.0f;  // LAB_1406d6c1b
  } else {
    // LAB_1406d6c1b's other arm: the look slot, the relax span and the second draw.
    const int altSlot = PlayerByte(host, 0, 0);
    scale = 0.0f;
    if (altSlot < 0x16) {
      float other[3] = {0.0f, 0.0f, 0.0f};
      PlayerPosition(host, altSlot, other);
      const int look = RecordStampOf(host) == 2 ? 0 : PlayerByte(host, RecordStampOf(host), 0);
      if (look == slot && !relax) {
        scale = 0.0f;
      } else {
        relaxSpan = k.pointTwo;
        if (look != slot && relax) relaxSpan = k.relax07;
        const float a = std::fabs(Wrap180Diff(out[0], other[0], k));
        const float b = std::fabs(Wrap180Diff(out[2], other[2], k));
        if (a <= k.angle45 || b > k.angle45) {
          relaxSpan = 0.0f;
          span = k.four;
        } else {
          const int win = static_cast<int>((ticks * k.stampsA) / k.fps + k.half);
          const float age = static_cast<float>(sinceStamp);
          if (win < age) {
            relaxSpan *= (age < k.fps) ? ((age - k.fps) / (win - k.fps)) : 0.0f;
          } else {
            relaxSpan *= k.one;
          }
        }
        row = PlayerByte(host, 1, 0);  // FUN_1406ea340(P, 1, span, 0, 90)
        if (row < 0x16) {
          const int scripted = PlayerByte(host, row, 2);  // FUN_140a6e600(*(u16*)(rec + 0x18))
          const float a2 = std::fabs(Wrap180Diff(out[0], other[0], k));
          const float b2 = std::fabs(Wrap180Diff(out[2], other[2], k));
          if (scripted != 0 && a2 < k.angle90 && b2 < k.angle90) {
            int pct = 0x5f;
            if (!relax) pct = 0x19;
            if (speedWord == 2) pct = Share75(pct);
            if (host.Percent(pct)) reselect = true;
          }
        }
      }
    }
  }
LAB_1406d6c22:
  EntryPrologue(host, req, k);  // FUN_1406d8730
  relaxSpan = k.three;          // fVar34 = DAT_14258c264
  // PES reads the column as DAT_1425dd988[req[8]] with no mask (0x1406D6C31-0x1406D6C43).
  const int column = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[bucket]));
  // The turn-family remap (0x1406D7CAC) applies to each turn row's own lookup, with the row the entry has picked.
  const auto turnColumn = [&](int row) { return TurnFamilyColumn(req, row, column, relaxSpan); };
  if (speedWord > 1) {
    if (speedWord == 2) {
      if (!req.Flag(0xf)) {
        motion = 0xbe5;
      } else {
        reselect = false;
        motion = 0x5be;
        if (kind == 3 || kind == 6) motion = 0x5c1;
        if (relax) motion = 0x5cf;
        if (speedClass == 3) {
          motion = 0x5c3;
          reselect = true;
        } else if (speedClass < 1) {
          if (kind == 3 || kind == 6) {
            motion = 0x5d5;
            if (column < 0x10 && (0xa014U >> (column & 0x1f) & 1) != 0 &&
                host.MatchPhase() == 1) {
              // The jostle block: the 20-tick sample, the mirror test and the row pair.
              float other[3] = {0.0f, 0.0f, 0.0f};
              PlayerPosition(host, row >= 0x16 ? slot : row, other);
              const float a = std::fabs(Wrap180Diff(out[0], other[0], k));
              if (0x15 < row && k.angle1125b < a && a < k.turn168750) {
                (void)static_cast<int>((ticks * k.ten) / k.fps + k.half);
              }
              const float loose = Wrap180Diff(out[0], out[2], k);
              const float tight = Wrap180Diff(out[2], out[0], k);
              const float band = (column == 0xf || column == 2) ? k.turn84375 : k.angle1125;
              if ((loose >= 0.0f) == (tight >= 0.0f) || std::fabs(loose) <= band) {
                if (column == 0xf || column == 2) {
                  const float c = std::fabs(Wrap180Diff(out[0], out[2], k));
                  float lo = k.angle675;
                  if (k.angle675 < lo) lo = k.speed39375;
                  const float turn = req.F(moveword::kTurn);
                  const bool negTurn = (turn <= 0.0f && turn != 0.0f);
                  const bool negA = (loose < 0.0f);
                  if (negTurn != negA && lo < c && c < k.turn163125) {
                    motion = 0x5c0;
                    if (speedClass < 2) motion = 0x5cc;
                  }
                }
              } else {
                motion = 0x5cc;
              }
              if (1 < speedClass) motion = 0x5c0;
            }
          } else {
            motion = 0xbe5;
          }
        }
        // The stamp-tick draw, then the row's own select.
        const int stampTicks = static_cast<int>(scale * k.thirty + 2.0f);
        const RowPick pick = PickRow(host, motion, turnColumn(motion));
        if (!pick.found) {  // FUN_1406ABDD0 answers 0 for this cell: no clip, and the entry hands the tick to TURN_STOP
          EnterTurnStop(host, req);
          return;
        }
        if (!host.Percent(pick.motion)) {
          if (host.Percent(stampTicks)) motion = 0x5d4;
          reselect = true;
        }
      }
      if (!IssueRowOrTurnStop(host, req, motion, turnColumn(motion), k.clipBlend, playback)) return;
      const int classWord = reselect ? 3 : 2;
      float speed = reselect ? 30.0f : k.speedD;  // 0x41800000 for class 2
      if (motion == 0x5d5) {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kSpeedWord, k.five);
      } else {
        req.SetU(moveword::kClassWord, static_cast<uint32_t>(classWord));
        req.SetF(moveword::kSpeedWord, speed);
      }
      req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(speedWord));
      req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
      RequestState(req, reselect ? MoveState::kStepTurnRun : MoveState::kRunTurnRun);
      req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
      req.SetU(moveword::kTurnedRow, req.U(moveword::kHeading));
      req.SetU(moveword::kPrevBucket, static_cast<uint32_t>(bucket));
      req.SetByte(moveword::kReorientFlag, 0);
      AdjustAngleApply(host);
      if (reselect) {
        host.SetRecordF0bec(relax ? k.halfTurn : host.RecordF0bec());  // FUN_1406cba60
      }
      return;
    }
    if (speedWord != 3) {
      EntryPrologue(host, req, k);  // LAB_1406d8523
      return;
    }
    if (!req.Flag(0xf)) {
      motion = 0xbe5;
    } else {
      motion = 0x5be;
      if (kind == 3 || kind == 6) motion = 0x5c1;
      if (relax) motion = 0x5cf;
      if (speedClass == 3) {
        motion = 0x5c3;
        reselect = true;
      } else if (speedClass < 1 && (kind == 3 || kind == 6)) {
        motion = 0x5d5;
        if (column < 0x10 && (0xa014U >> (column & 0x1f) & 1) != 0 &&
            host.MatchPhase() == 1) {
          // The jostle block of the 3-band arm.
          float other[3] = {0.0f, 0.0f, 0.0f};
          PlayerPosition(host, row >= 0x16 ? slot : row, other);
          const float a = std::fabs(Wrap180Diff(out[0], other[0], k));
          if (sinceStamp < static_cast<int>((ticks * k.ten) / k.fps + k.half)) {
            const float b = Wrap180Diff(out[2], out[0], k);
            const bool negTurn =
                (req.F(moveword::kTurn) <= 0.0f && req.F(moveword::kTurn) != 0.0f);
            if ((column == 0xf || column == 2) && a < b && b < k.turn174375) {
              const float band = k.angle675;
              const bool negA = (a < 0.0f);
              if (negTurn != negA && band < b && b < k.turn163125) {
                motion = 0x5c0;
                if (speedClass < 2) motion = 0x5cc;
              }
            } else if (b < 0.0f) {
              motion = 0x5cc;
            } else {
              motion = 0x5c0;
            }
          }
        }
      }
      if (motion == 0xbe5) {
        EntryPrologue(host, req, k);  // LAB_1406d8523
        return;
      }
      const bool slow = column < 0x10 && (0xa014U >> (column & 0x1f) & 1) != 0;
      if (slow && (motion == 0x5bc || motion == 0x5c3) && speedClass == 3) {
        // LAB_1406d7c9e's slow arm: the ability draw, then the row's own select.
        int ability = host.AbilityByte(0x29) - 0x14;  // FUN_1406e6660(P, 0x29)
        if (!relax) ability -= 0x14;
        ability = ability > 99 ? 99 : (ability < 0 ? 0 : ability);
        motion = host.Percent(ability) ? 0x5d0 : 0x5cf;
        const int win = static_cast<int>(scale * k.thirty +
                                         (k.angle90 < req.F(moveword::kTurn) ? 5.0f : 2.0f));
        const RowPick pick = PickRow(host, motion, turnColumn(motion));
        if (pick.found) {
          if (!host.Percent(pick.motion)) {
            if (host.Percent(2)) motion = 0x5d1;
            reselect = true;
          }
        }
        (void)win;
      } else {
        // LAB_1406d7d6b's arm: the row's own select.
        const int win = static_cast<int>(scale * k.speed15 +
                                         (k.angle90 < req.F(moveword::kTurn) ? 5.0f : 2.0f));
        const RowPick pick = PickRow(host, motion, turnColumn(motion));
        if (!pick.found) {  // FUN_1406ABDD0 answers 0 for this cell: no clip, and the entry hands the tick to TURN_STOP
          EnterTurnStop(host, req);
          return;
        }
        if (!host.Percent(pick.motion)) {
          if (host.Percent(2)) motion = 0x5d4;
          reselect = true;
        }
        (void)win;
      }
    }
    if (!IssueRowOrTurnStop(host, req, motion, turnColumn(motion), k.clipBlend, playback)) return;
    req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(speedWord));
    req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
    if (motion == 0x5d5) {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
    } else if (reselect) {
      req.SetU(moveword::kClassWord, 3);
      req.SetF(moveword::kSpeedWord, 30.0f);
    } else {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, k.speedD);  // 0x41800000
    }
    RequestState(req, reselect ? MoveState::kStepTurnRun : MoveState::kRunTurnRun);
    req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
    req.SetU(moveword::kTurnedRow, req.U(moveword::kHeading));
    req.SetU(moveword::kPrevBucket, static_cast<uint32_t>(bucket));
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    if (reselect) {
      host.SetRecordF0bec(relax ? k.halfTurn : host.RecordF0bec());  // FUN_1406cba60
    }
    return;
  }
  // The speedWord <= 1 tail (0x1406d7545): the 0x5b3 / 0x5b9 ladder and the run request.
  motion = 0x5b3;
  if (speedClass == 3) {
    motion = 0x5b9;
    reselect = true;
  }
  const float turn = std::fabs(Wrap180Diff(out[0], out[2], k));
  if (k.angle1125 < turn) {
    const bool negTurn = (req.F(moveword::kTurn) <= 0.0f && req.F(moveword::kTurn) != 0.0f);
    const bool negA = (turn < 0.0f);
    if (negTurn != negA) {
      motion = 0x5b3;
      if (speedClass == 3) {
        motion = 0x5b9;
        reselect = true;
      }
    }
  }
  if (host.CommandSpeedClass() < 0x16 && (motion == 0x5b3 || motion == 0x5b9)) {
    // FUN_140a58550 (the stamina percentage, 0x1406D7811) against 0x14 (0x1406D783F CMP DIL,0x14); not the ability reader.
    if (host.StaminaPercent() < 0x14) motion = 0x5b6;
  }
  if (!IssueRowOrTurnStop(host, req, motion, turnColumn(motion), k.clipBlend, playback)) return;
  req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(speedWord));
  req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
  req.SetF(moveword::kSpeedWord, 11.0f);  // 0x41300000
  req.SetU(moveword::kClassWord, 1);
  if (!reselect && 1 < speedClass) {
    req.SetU(moveword::kClassWord, 2);
    req.SetF(moveword::kSpeedWord, k.speedD);  // 0x41800000
  } else if (reselect) {
    req.SetU(moveword::kClassWord, 3);
    req.SetF(moveword::kSpeedWord, 30.0f);
  }
  RequestState(req, MoveState::kRunTurnRun);
  req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
  req.SetU(moveword::kTurnedRow, req.U(moveword::kHeading));
  req.SetU(moveword::kPrevBucket, static_cast<uint32_t>(bucket));
  req.SetByte(moveword::kReorientFlag, 0);
  AdjustAngleApply(host);
  (void)mirrorSlot;
  (void)relaxSpan;
}

// ---------------------------------------------------------------------------------------------
// FUN_1406d2f60: the RUN_TURN_STEP entry, with the threshold the caller computes.
//
// PES's order:
//   1. the guards: kind 7 refuses; the class word clamps into 0..3; the desired speed clamps
//      between 5 and 28; the precise flag and the class must both be set;
//   2. the turn's angle must reach the 50.625 gate, or (past 123.75 and inside the 135 / 33.75
//      band);
//   3. the row ladder over the two head pairs, with the jostle gate;
//   4. the clip request and the transition into RUN_TURN_STEP.
// ---------------------------------------------------------------------------------------------
bool EnterRunTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k, float threshold) {
  const int kind = static_cast<int>(req.U(moveword::kKind));
  if (kind == 7) return false;
  {
    const int liveClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    int cls;
    if (liveClass < 4) {
      cls = liveClass < 0 ? 0 : liveClass;
    } else {
      cls = 3;
    }
    // The desired speed clamped between 5 (DAT_14259222c) and 28 (DAT_1425a2004). PES leaves
    // fVar25 at 28 and only overwrites it inside the band, so an out-of-band speed clamps to 28.
    const float want = req.F(moveword::kDesiredSpeedKmh);
    float wantSpeed = k.speed28;
    if ((want <= k.speed28) && (k.five <= want)) {
      wantSpeed = want;
    }
    if (req.U(moveword::kPrecise) != 1 ||
        static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1) {
      return false;
    }
    // FUN_14043a0a0(req[3], req[1]): the look-at error folded.
    const float lookDelta =
        std::fabs(Wrap180Diff(req.F(moveword::kLookHeading), req.F(moveword::kHeading), k));
    if (req.F(moveword::kRate) <= k.turnRamp) return false;
    if (req.F(moveword::kDistToLook) <= k.one &&
        std::fabs(req.F(moveword::kTurn)) <= k.angle90) {
      return false;
    }
    // The turn gate (0x1406d31b0): under 50.625 nothing runs; between 50.625 and 123.75 the
    // second arm needs 135 or 33.75; past 123.75 it runs outright.
    if (lookDelta <= k.turn50625 ||
        (k.turn123750 <= lookDelta &&
         ((k.angle135 <= lookDelta || k.angle3375 <= std::fabs(req.F(moveword::kTurn)))))) {
      if (lookDelta < k.turn123750) return false;
      // FUN_14043a0a0(req[2], req[1]) then FUN_1406b8da0: the row slot the ladder indexes.
      const float stepped =
          WrapHalf(Wrap180Diff(req.F(moveword::kHeading), req.F(moveword::kFacing), k), k);
      const int slot = BucketIndex(k, stepped);
      const int col = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[slot & 7]));
      int motion = 0x5e2;
      if (req.F(moveword::kRate) < k.startHalf ||
          (static_cast<int>(req.U(moveword::kCommandSpeedClass)) < 2 &&
           req.F(moveword::kRate) <= k.startHalf)) {
        motion = 0x5df;
      }
      if (req.F(moveword::kRate) < k.speed15 && col < 0x12 &&
          (0x28005U >> (col & 0x1f) & 1) != 0) {
        return false;
      }
      if (col == 0xb || col == 6) {
        // The sampled-ball block: the heading from the player to the sample, then the sign
        // test that decides whether the run happens at all.
        float out[3] = {0.0f, 0.0f, 0.0f};
        host.PositionAt(2, out);
        const float sampleHeading = FoldUnit(std::atan2(out[0], out[2]) * 57.29578f, k);
        const float a = std::fabs(Wrap180Diff(req.F(moveword::kHeading), sampleHeading, k));
        const bool negTurn = (req.F(moveword::kTurn) <= 0.0f && req.F(moveword::kTurn) != 0.0f);
        const bool negA = (a < 0.0f);
        if (negTurn == negA) return false;
      }
      if (!PickRow(host, motion, col).found) return false;
      if (threshold + k.startHalf < req.F(moveword::kRate)) return true;
      const RowPick pick = PickRow(host, motion, col);
      // The mirror / speed answer the row's own select gives.
      bool mirror = pick.mirror;
      if (slot == 5 || slot == 0) {
        float out[3] = {0.0f, 0.0f, 0.0f};
        host.PositionAt(2, out);
        const float sampleHeading = FoldUnit(std::atan2(out[0], out[2]) * 57.29578f, k);
        const float a = std::fabs(Wrap180Diff(req.F(moveword::kLookHeading), sampleHeading, k));
        const float b = std::fabs(Wrap180Diff(req.F(moveword::kFacing), sampleHeading, k));
        // FUN_1405aef10(P+0x20, P+0x28): the facing error the last two tests read.
        const float facingErr = std::fabs(Wrap180Diff(req.F(moveword::kLookHeading),
                                                     req.F(moveword::kHeading), k));
        (void)a;
        (void)b;
        (void)facingErr;
      } else {
        mirror = PickRow(host, motion, col).mirror;
      }
      req.SetU(moveword::kRowAngle, req.U(moveword::kHeading));  // param_2[0x18] = param_2[1]
      req.SetF(moveword::kRowAngle,
               req.F(moveword::kHeading) + k.halfTurn);  // param_2[0x17] = req[1] + 180
      req.SetF(moveword::kPlayback, pes::TurnPlayback(host.PhysiqueG(), host.MirrorChoice(false) ? 1.0f : 0.5f,
                                                      host.PlaybackConstantsView()));  // vt+0x50: a float store
      IssueClip(host, pick.motion, mirror, k.clipBlend, req.F(moveword::kPlayback), 0.0f);
      req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));  // param_2[0x24] = [0x1f]
      req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));  // param_2[0x22] = [0x1d]
      req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));      // param_2[0x1f]
      req.SetF(moveword::kSpeedWord, wantSpeed);                        // param_2[0x1d]
      req.SetU(moveword::kPrevBucket, 5);                              // param_2[0x25] = 5
      req.SetU(moveword::kMoveBucket, 9);                              // param_2[0x20] = 9
      req.SetU(moveword::kPrevKind, req.U(moveword::kKind));           // param_2[0x1e] = req[0]
      RequestState(req, MoveState::kRunTurnStep);                      // req[0x39] = 0x10
      req.SetByte(moveword::kReorientFlag, 0);                          // param_2 + 0x3c = 0
      AdjustAngleApply(host);
      return true;
    }
    // The other arm (0x1406d4700): the back-step ladder.
    const float stepped =
        WrapHalf(Wrap180Diff(req.F(moveword::kHeading), req.F(moveword::kFacing), k), k);
    const int slot = BucketIndex(k, stepped);
    int motion = 0x5da;
    int col = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[slot & 7]));
    if (stepped < 0.0f) motion = 0x5db;
    bool narrowSide = ((col - 4U) & 0xfffffffdU) == 0;
    if (req.F(moveword::kRate) < k.blend03 && !narrowSide && col < 0xe &&
        (0x2850U >> (col & 0x1f) & 1) != 0) {
      return false;
    }
    if (req.F(moveword::kRate) < k.blend03 ||
        (static_cast<int>(req.U(moveword::kCommandSpeedClass)) < 2 &&
         req.F(moveword::kRate) <= k.startHalf)) {
      if (kind == 3 || kind == 6) {
        motion = 0x5d6;
      } else {
        motion = 0x5d8;
      }
      if (stepped < 0.0f) motion += 1;
    }
    if (narrowSide) {
      // The jostle gate of the back-step arm.
      float out[3] = {0.0f, 0.0f, 0.0f};
      host.PositionAt(2, out);
      const float sampleHeading = FoldUnit(std::atan2(out[0], out[2]) * 57.29578f, k);
      const float a = std::fabs(Wrap180Diff(req.F(moveword::kHeading), sampleHeading, k));
      const float err = std::fabs(Wrap180Diff(req.F(moveword::kFacing), sampleHeading, k));
      const bool negTurn = (req.F(moveword::kTurn) <= 0.0f && req.F(moveword::kTurn) != 0.0f);
      const bool negA = (a < 0.0f);
      if (negTurn != negA && k.angle90 < err && err < k.angle1575) narrowSide = false;
    }
    if (((kind & 0xfffffffa) == 0) && kind != 5 && col < 0xc &&
        (0xb40U >> (col & 0x1f) & 1) != 0) {
      motion = (stepped < 0.0f) ? 0x5dc : 0x5d8;
    }
    if (1 < static_cast<int>(req.U(moveword::kCommandSpeedClass)) && kind == 4) {
      if (col == 9 || col == 8) {
        return false;
      }
    }
    if (narrowSide) {
      return false;
    }
    if (!PickRow(host, motion, col).found) return false;
    if (threshold + k.startHalf < req.F(moveword::kRate)) return true;
    const RowPick pick = PickRow(host, motion, col);
    bool mirror = pick.mirror;
    req.SetU(moveword::kRowAngle, req.U(moveword::kHeading));
    req.SetF(moveword::kRowAngle,
             req.F(moveword::kHeading) - (stepped < 0.0f ? k.neg90 : k.angle90));
    req.SetF(moveword::kPlayback, pes::TurnPlayback(host.PhysiqueG(), host.MirrorChoice(false) ? 1.0f : 0.5f,
                                                    host.PlaybackConstantsView()));  // vt+0x50: a float store
    IssueClip(host, pick.motion, mirror, k.clipBlend, req.F(moveword::kPlayback), 0.0f);
    req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
    req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
    req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));
    req.SetF(moveword::kSpeedWord, wantSpeed);
    req.SetU(moveword::kPrevBucket, 5);
    req.SetU(moveword::kMoveBucket, 2);  // the back-step arm's own bucket
    req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
    RequestState(req, MoveState::kRunTurnStep);
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    return true;
  }
}

// ---------------------------------------------------------------------------------------------
// FUN_1406d38d0: the STEP_TURN_RUN entry. Returns true when it wrote the request.
//
// PES's order (the register aliases come from the instruction stream):
//   1. the kind-7 guard and the slow-player floor (1.5 km/h, or 4 against the precise pair);
//   2. the body-direction band that re-reads the displacement heading;
//   3. the guard ladder: the class, the look band, the body delta and the turn must line up;
//   4. the row ladder keyed on the class word, the speed word and the left side, with the
//      narrow-column second ladder;
//   5. the clip request, the class/speed pair and the transition into STEP_TURN_RUN (or the
//      small-turn branch that hands the tick to STEP_TURN_STEP).
// ---------------------------------------------------------------------------------------------
bool EnterStepTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int kind = static_cast<int>(req.U(moveword::kKind));
  if (kind == 7) return false;
  const bool relax = host.EntryAimFlag();  // FUN_1406ba7d0
  float disp[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, disp);  // FUN_141eb50b0(P, out, 1)
  // (clock * 60 * 60) / 1000 * length: the last-tick displacement in km/h.
  const float speedKmh = DispSpeedKmh(host, k);
  // The slow-player floor: 1.5 km/h, or 4 when both the turn and the facing error are inside
  // the precise pair.
  float floorBand = k.band15m;
  if (std::fabs(req.F(moveword::kTurn)) < k.angle45 &&
      std::fabs(req.F(moveword::kFacingError)) < k.angle45 &&
      req.U(moveword::kPrecise) != 0) {
    floorBand = k.four;
  }
  if (speedKmh < floorBand) return false;
  // FUN_14043a0a0(req[2], req[4]): the body-direction error folded.
  float bodyDelta = Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k);
  if ((k.angle3375 < std::fabs(bodyDelta) && std::fabs(bodyDelta) < k.angle5625) ||
      (k.turn123750 < std::fabs(bodyDelta) && std::fabs(bodyDelta) < k.turn146250)) {
    // PES re-samples the displacement heading and checks the look band; when it lines up the
    // body error becomes the plain facing fold.
    const float sample = DispHeading(host, k);
    if (req.F(moveword::kBodyDirection) != sample &&
        std::fabs(req.F(moveword::kBodyDirection) - sample) < k.angle45) {
      bodyDelta = WrapHalf(req.F(moveword::kFacing), k);
    }
  }
  const float lookDelta =
      Wrap180Diff(req.F(moveword::kLookHeading), req.F(moveword::kHeading), k);
  const float distLook = req.F(moveword::kDistToLook);
  // The look band: 0 under 0.5 m, then a ramp to 1 at 4 m, then 45 plus 11.25 degrees.
  float lookBand = 0.0f;
  if (k.half < distLook) {
    lookBand = (distLook < k.two) ? ((distLook - k.half) / k.band15m) : 1.0f;
  }
  lookBand = lookBand * k.angle45 + k.angle1125b;
  const int speedWord = static_cast<int>(req.U(moveword::kClassWord));
  const int speedClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
  bool wants = true;
  int state = 0xf;

  if (!(speedWord < 0) && std::fabs(bodyDelta) <= k.angle135) {
    // The fast-turn guard ladder (0x1406d3c0d..0x1406d3f50).
    const float facingBody = (req.U(moveword::kPrecise) == 0)
                                 ? req.F(moveword::kFacingError)
                                 : Wrap180Diff(req.F(moveword::kFacing),
                                               req.F(moveword::kHeading), k);
    if (std::fabs(facingBody) < k.angle675 &&
        std::fabs(req.F(moveword::kTurn)) < k.angle1125b &&
        (std::fabs(bodyDelta) < k.angle675 || k.angle1125 < std::fabs(bodyDelta))) {
      wants = false;
    }
    if (0x7fffffff < static_cast<uint32_t>(speedWord)) {
      if (std::fabs(req.F(moveword::kFacingError)) <= k.angle675) return false;
      if (std::fabs(req.F(moveword::kTurn)) <= k.angle675) return false;
    }
    if (std::fabs(bodyDelta) < k.angle45) return false;
    if (k.angle135 < std::fabs(bodyDelta)) return false;
    if (distLook <= k.band15m && std::fabs(req.F(moveword::kTurn)) <= lookBand) return false;
    if (!wants) return false;
    if (std::fabs(req.F(moveword::kFacingError)) < k.angle225m &&
        std::fabs(req.F(moveword::kTurn)) < k.angle225m && req.U(moveword::kPrecise) == 1) {
      return false;
    }
    const float headingSign = bodyDelta < 0.0f ? k.negOne : k.one;
    const float stepped =
        Wrap180Diff(headingSign * k.angle90 + req.F(moveword::kFacing),
                    req.F(moveword::kHeading), k);
    const int rowSlot = BucketIndex(k, WrapHalf(stepped, k));
    float column = static_cast<float>(k.bucketColumn[rowSlot & 7]);
    const bool left = 0.0f <= bodyDelta;
    int motion = 0x5b3;
    bool reselect = false;
    if (speedClass >= 0 && (req.U(moveword::kPrecise) == 0 || lookDelta < k.angle45) &&
        (k.turn50625 <= std::fabs(bodyDelta) || k.angle45 < std::fabs(req.F(moveword::kTurn)))) {
      // LAB_1406d49xx's row ladder.
      const int row0 = (rowSlot - 8U) < 2u ? 0 : 1;
      if (speedClass > 2 && column != 0.0f && column != 7.00649e-45f) {
        reselect = true;
        motion = 0x60b - (left ? 0 : 1);
      }
      if (speedWord < 1) {
        if (speedClass < 3) {
          if (speedClass < 2) {
            if (speedClass == 1 || !(kind == 3 || kind == 6)) {
              motion = 0x5e7 - (left ? 0 : 1);
            } else {
              motion = 0x5e5 - (left ? 0 : 1);
            }
          } else {
            motion = 0x5eb - (left ? 0 : 1);
          }
        } else if (!relax) {
          const float a = std::fabs(stepped);
          if (k.angle5625 < a && a <= k.angle675) {
            const float arg = left ? k.seventy : k.neg70;
            column = static_cast<float>(k.bucketColumn[BucketIndex(k, WrapHalf(arg, k)) & 7]);
          }
          reselect = true;
          motion = 0x5ef - (left ? 0 : 1);
        } else {
          motion = 0x601 - (left ? 0 : 1);
        }
      } else if (speedWord < 2) {
        if (speedClass < 3) {
          if (speedClass < 2) {
            if (speedClass < 1 && (kind == 3 || kind == 6)) {
              motion = 0x5f3 - (left ? 0 : 1);
            } else {
              motion = 0x5f5 - (left ? 0 : 1);
            }
          } else {
            motion = 0x5f9 - (left ? 0 : 1);
          }
        } else if (!relax) {
          const float a = std::fabs(stepped);
          if (k.angle5625 < a && a <= k.angle675) {
            const float arg = left ? k.seventy : k.neg70;
            column = static_cast<float>(k.bucketColumn[BucketIndex(k, WrapHalf(arg, k)) & 7]);
          }
          reselect = true;
          motion = 0x5fd - (left ? 0 : 1);
        } else {
          motion = 0x601 - (left ? 0 : 1);
        }
      } else if (speedClass < 2) {
        motion = 0x605 - (left ? 0 : 1);
      } else if (relax) {
        motion = 0x601 - (left ? 0 : 1);
      }
      const int col = static_cast<int>(static_cast<uint32_t>(column));
      const bool narrow = ((col - 8U) < 2u) || ((col - 0xbU) < 2u);
      if (narrow) {
        // LAB_1406d5142's second ladder, keyed on the class word.
        if (speedWord < 1) {
          if (speedClass < 3) {
            if (speedClass < 2) {
              motion = 0x5e9 - (left ? 0 : 1);
            } else {
              motion = 0x5ed - (left ? 0 : 1);
            }
          } else if (!relax) {
            reselect = true;
            motion = 0x5f1 - (left ? 0 : 1);
          } else {
            motion = 0x603 - (left ? 0 : 1);
          }
        } else if (speedWord < 2) {
          if (speedClass < 3) {
            if (speedClass < 2) {
              motion = 0x5f7 - (left ? 0 : 1);
            } else {
              motion = 0x5fb - (left ? 0 : 1);
            }
          } else if (!relax) {
            reselect = true;
            motion = 0x5ff - (left ? 0 : 1);
          } else {
            motion = 0x603 - (left ? 0 : 1);
          }
        } else if (speedClass < 1) {
          reselect = true;
          motion = 0x60d - (left ? 0 : 1);
        } else {
          motion = 0x609 - (left ? 0 : 1);
        }
      }
      (void)row0;
      const int col2 = static_cast<int>(static_cast<uint32_t>(column));
      if (!((1 < static_cast<int>(static_cast<uint32_t>(col2)) - 8U) &&
            !(lookDelta >= k.angle675 && lookDelta <= k.turn146250)) ||
          reselect) {
        if (!PickRow(host, motion, col2).found) return false;
        const RowPick pick = PickRow(host, motion, col2);
        IssueClip(host, pick.motion, pick.mirror, k.clipBlend,
                  req.F(moveword::kPlayback), 0.0f);
        req.SetByte(moveword::kFootFlag, pick.mirror ? 1 : 0);
        req.SetU(moveword::kRowAngle, req.U(moveword::kHeading));  // param_2[0x18] = param_2[1]
        req.SetF(moveword::kPlayback, pes::TurnPlayback(host.PhysiqueG(), host.MirrorChoice(false) ? 1.0f : 0.5f,
                                                        host.PlaybackConstantsView()));
        int classWord = 2;
        if (speedClass < 3) {
          classWord = speedClass < 0 ? 0 : speedClass;
        } else {
          classWord = 3;
        }
        float speed = req.F(moveword::kDesiredSpeedKmh);
        const bool slow = !(speed <= k.band15m) || !(k.band15m <= speed);
        float s = slow ? k.five : speed;
        if (!slow) s = k.speed28 < speed ? k.speed28 : s;
        req.SetU(moveword::kRate,
                 static_cast<uint32_t>(RateOf(host, req.U(moveword::kUseClassRate) != 0,
                                              classWord, s, kind)));
        const float own = OwnRate(host);
        const float r = req.F(moveword::kRate);
        req.SetF(moveword::kRate, (r <= own && 0.0f <= r) ? r : (r < 0.0f ? 0.0f : own));
        req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(speedWord));
        req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
        req.SetF(moveword::kSpeedWord, s);
        req.SetU(moveword::kClassWord, static_cast<uint32_t>(classWord));
        req.SetU(moveword::kMoveBucket, 5);
        req.SetU(moveword::kPrevBucket, bodyDelta > 0.0f ? 2 : 7);
        req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
        RequestState(req, MoveState::kStepTurnRun);  // req[0x39] = 0xf
        req.SetByte(moveword::kReorientFlag, 0);
        AdjustAngleApply(host);
        return true;
      }
      EnterTurnStop(host, req);
      return false;
    }
    EnterTurnStop(host, req);
    return false;
  }
  // LAB_1406d4727's tail: the small-turn branch, which hands the tick to STEP_TURN_STEP.
  const float sample =
      Wrap180Diff(req.F(moveword::kFacing) + k.halfTurn, req.F(moveword::kHeading), k);
  float signA = k.one;
  if (req.F(moveword::kLookDelta) <= 0.0f && req.F(moveword::kLookDelta) != 0.0f) signA = k.negOne;
  float signB = k.one;
  if (req.F(moveword::kTurn) < 0.0f) signB = k.negOne;
  float angle = sample;
  if (signA == signB && req.F(moveword::kTurn) < k.angle45) angle = 0.0f;
  const int rowSlot = BucketIndex(k, WrapHalf(angle, k));
  int col = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[rowSlot & 7]));
  if (speedClass < 0 || (req.U(moveword::kPrecise) != 0 && k.angle675 < lookDelta)) {
    if (speedWord > 0 && speedClass > 0 && req.U(moveword::kPrecise) == 1 &&
        k.angle675 < lookDelta && lookDelta < k.angle1575) {
      int row = 0x633;
      if (speedClass < 2) row = 0x633;
      if (lookDelta < 0.0f) {
        const bool wide = row == 0x633;
        row = wide ? 0x638 : 0x634;
      }
      col = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[rowSlot & 7]));
      if (!((col - 8U) < 2u) || k.angle675 <= req.F(moveword::kFacingError)) {
        if (!PickRow(host, row, col).found) return false;
        const RowPick pick = PickRow(host, row, col);
        IssueClip(host, pick.motion, pick.mirror, k.clipBlend,
                  req.F(moveword::kPlayback), 0.0f);
        const int classWord = speedClass < 3 ? (speedClass < 0 ? 0 : speedClass) : 3;
        float speed = req.F(moveword::kDesiredSpeedKmh);
        const bool slow = !(speed <= k.speedD) || !(k.five <= speed);
        float s = slow ? k.five : speed;
        if (!slow) s = k.speedD < speed ? k.speedD : s;
        req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(speedWord));
        req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
        req.SetF(moveword::kSpeedWord, s);
        req.SetU(moveword::kClassWord, static_cast<uint32_t>(classWord));
        req.SetU(moveword::kPrevBucket, bodyDelta > 0.0f ? 4 : 9);
        req.SetU(moveword::kMoveBucket, lookDelta < 0.0f ? 7 : 2);
        req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
        RequestState(req, MoveState::kStepTurnStep);  // req[0x39] = 0x13
        req.SetByte(moveword::kReorientFlag, 0);
        AdjustAngleApply(host);
        return true;
      }
    }
    EnterTurnStop(host, req);
    return false;
  }
  // The default branch: the 0x5b3 / 0x628 ladder and the run request.
  {
    int row = 0x628;
    if (speedClass > 1) row = 0x610;
    if (bodyDelta < 0.0f && (row = 0x616, speedClass < 2)) row = 0x612;
    if (req.U(moveword::kPrecise) == 0 &&
        (std::fabs(bodyDelta) < k.angle1125b || lookBand < std::fabs(bodyDelta))) {
      float disp[3] = {0.0f, 0.0f, 0.0f};
      host.PositionAt(2, disp);
      // FUN_140a58550's byte read on the roster row.
      const int scripted = PlayerByte(host, 0, 0);
      (void)scripted;
      const float sample2 =
          Wrap180Diff(req.F(moveword::kFacing), FoldUnit(req.F(moveword::kLookHeading), k), k);
      const float a2 = Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k);
      float lo = k.turn146250;
      float hi = k.angle3375;
      if (row < 0x16) {
        const bool narrow = (row > 10) != (10 < row);
        if (narrow) {
          lo = k.turn140625;
          hi = k.angle1125b;
        }
      }
      const bool zeroSign = sample2 == 0.0f;
      const bool pos = 0.0f < sample2;
      (void)lo;
      (void)hi;
      (void)zeroSign;
      (void)pos;
      (void)a2;
      const int col2 = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[rowSlot & 7]));
      const float a3 = Wrap180Diff(req.F(moveword::kFacing),
                                   FoldUnit(req.F(moveword::kLookHeading), k), k);
      const float b3 = Wrap180Diff(req.F(moveword::kLookHeading), req.F(moveword::kHeading), k);
      if ((col2 - 8U) < 2u) {
        if (speedClass < 1 || (k.angle5625 <= req.F(moveword::kTurn)) ||
            !(req.F(moveword::kLookDelta) <= k.angle135)) {
          if (speedClass > 1) row = 0x610;
          else row = 0x5f7;
        }
      }
      const bool sameSign = (a2 < 0.0f) == (b3 < 0.0f);
      if (sameSign && req.F(moveword::kTurn) <= k.angle1125) {
        row = 0x62f;
        if (speedClass < 2) row = 0x633;
      }
    }
    if (!PickRow(host, row, col).found) return false;
    const RowPick pick = PickRow(host, row, col);
    IssueClip(host, pick.motion, pick.mirror, k.clipBlend,
              req.F(moveword::kPlayback), 0.0f);
    req.SetByte(moveword::kFootFlag, pick.mirror ? 1 : 0);
    req.SetU(moveword::kRowAngle, req.U(moveword::kHeading));
    req.SetF(moveword::kPlayback, pes::TurnPlayback(host.PhysiqueG(), host.MirrorChoice(false) ? 1.0f : 0.5f,
                                                    host.PlaybackConstantsView()));
    const int classWord = speedClass < 4 ? (speedClass < 0 ? 0 : speedClass) : 3;
    float speed = req.F(moveword::kDesiredSpeedKmh);
    const bool slow = !(speed <= k.speed28) || !(k.five <= speed);
    float s = slow ? k.five : speed;
    if (!slow) s = k.speed28 < speed ? k.speed28 : s;
    req.SetU(moveword::kRate,
             static_cast<uint32_t>(RateOf(host, req.U(moveword::kUseClassRate) != 0, classWord, s,
                                          kind)));
    const float own = OwnRate(host);
    const float r = req.F(moveword::kRate);
    req.SetF(moveword::kRate, (r <= own && 0.0f <= r) ? r : (r < 0.0f ? 0.0f : own));
    req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(speedWord));
    req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
    req.SetF(moveword::kSpeedWord, s);
    req.SetU(moveword::kClassWord, static_cast<uint32_t>(classWord));
    req.SetU(moveword::kMoveBucket, 5);
    req.SetU(moveword::kPrevBucket, bodyDelta > 0.0f ? 4 : 9);
    req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
    RequestState(req, MoveState::kStepTurnRun);
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    return true;
  }
}

// ---------------------------------------------------------------------------------------------
// FUN_1406c7400: STEP_TURN_RUN, the side/back step that resumes the run.
//
// PES's order:
//   1. the motion record's frame spans and the rate word's reset;
//   2. the reorient window (req[0x3c]..[0x3f]) and the yaw lock;
//   3. the run-family branch: the replay row through the step grid;
//   4. the gates over the displacement speed and the turn angle that pick STEP_TURN_RUN,
//      RUN_TURN_STEP, ACCELERATE, GOTO_RUN, STOP or the run-turn entry;
//   5. the run-tail ladder with the rate selection.
// ---------------------------------------------------------------------------------------------
void StepTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int clipFrame = host.CurrentClipFrames();  // (P+0xab8 vt+0x38)()
  const float clipLength = host.RecordF2468();     // P+0x2468: no floor in this handler (0x1406c7400 has no DAT_1425ab120 read)
  const float scale = k.one;
  const int motion = host.CurrentMotion();  // (P+0xab8 vt+0x18)()
  const int rowStart = RecordStartFrames(host, motion, scale);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, req.Flag(moveword::kMirror), k);
  const int rowFixed = RecordFixedFrames(host, motion, scale);  // FUN_1406af620
  // FUN_1406af8e0's sub-record walk (0x1406c74c4..0x1406c7580): local_res8 = band 0's frames and local_res18 = band 1's, both the
  // FIXED frames when the record has fewer bands; the last band's read is unused (its result is dropped in the stream).
  const pes::MotionRecord& motionRec = host.Motion(motion);
  const int bands = motionRec.CancelCount();  // FUN_1406afbc0(..., 5)
  int bandA = rowFixed;
  int bandB = rowFixed;
  if (0 < bands) {
    bandA = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 0);
    if (1 < bands) bandB = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 1);
  }
  const float half = k.half;
  req.SetF(moveword::kRate, 0.3f);  // param_2[0x4a] = 0x3e99999a
  // 0x1406c7630..0x1406c7660: a row with no offset starts at 0 and ends after the tenth of a second; the end never passes the
  // FIXED frames (local_res20 = FUN_1406af620; the old port clamped to the last band).
  int rowStartLive = rowStart;
  int rowEndLive = rowEnd;
  if (rowOffset == 0.0f) {
    const float ticks = static_cast<float>(host.TicksPerSecond());
    rowStartLive = 0;
    rowEndLive = static_cast<int>(ticks * k.ten / k.fps + half);
  }
  if (rowFixed < rowEndLive) rowEndLive = rowFixed;

  const float turn675 = k.angle675;
  const float narrow225 = k.angle225m;
  const float turn7875 = k.turn7875;
  const float angle45 = k.angle45;
  // 2. The reorient window (0x1406c7667..0x1406c77cb), read from the instruction stream: the row is req[0x18]; the inner precise
  // gate measures FUN_14043a0a0(req[1], req[3]) and the outer one FUN_14043a0a0(req[0x18], req[1]); the step towards req[1] is
  // clamped to DAT_14259d618 (22.5); the ratio's numerator is FUN_14043a0a0(req[2], row), signed, and its sign feeds the mirror
  // test; req[0x3d] and req[0x18] both take the row (no table term) and FUN_1406cbb00(P, row) locks the yaw.
  if (!FacingAdjusterBusy(host) && rowStartLive <= clipFrame && clipFrame <= (rowStartLive + rowEndLive) / 2) {
    float row = req.F(kTurnedHeadingWord);
    const uint32_t frames = static_cast<uint32_t>(static_cast<float>(rowEndLive - clipFrame) / clipLength + half);
    const float heading = req.F(moveword::kHeading);
    if ((req.U(moveword::kPrecise) != 1 || std::fabs(Wrap180Diff(heading, req.F(moveword::kLookHeading), k)) < angle45) &&
        std::fabs(Wrap180Diff(row, heading, k)) < turn7875) {
      row = StepHeadingToPoint(row, heading, narrow225, k);
    }
    const float facingDiff = Wrap180Diff(req.F(moveword::kFacing), row, k);
    if (std::fabs(facingDiff) / static_cast<float>(frames) < angle45) {
      req.SetF(moveword::kAngle3d, row);        // param_2[0x3d]
      req.SetByte(moveword::kReorientFlag, 1);  // param_2 + 0x3c = 1
      req.SetU(moveword::kTurnFrames, frames);  // param_2[0x3e]
      req.SetByte(moveword::kMirrorFlag, 0);    // param_2 + 0x3f = 0
      req.SetF(kTurnedHeadingWord, row);        // param_2[0x18]
      if (turn675 < std::fabs(rowOffset)) {
        const float signOffset = rowOffset < 0.0f ? k.negOne : scale;
        const float signDiff = facingDiff < 0.0f ? k.negOne : scale;
        if (signOffset != signDiff) {
          uint8_t flag = static_cast<uint8_t>(req.U(moveword::kMirrorFlag) & 0xffu);
          if (k.angle3325 < std::fabs(facingDiff)) flag = 1;  // DAT_1425ddf9c
          req.SetByte(moveword::kMirrorFlag, flag);
        }
      }
      host.SetRecordF0bec(FoldUnit(row, k));  // 0x1406c77cb: FUN_1406cbb00(P, row)
    }
  }

  // 3. The run-family branch (0x1406c77d0..0x1406c7b00): precise commands inside the first tenth of a second whose facing is
  // close to the look-at but far from the row re-select the step row at the previous bucket (words 0x20, not 8).
  const float ticks = static_cast<float>(host.TicksPerSecond());
  if (static_cast<int>(req.U(moveword::kCommandId)) != 0x11 &&
      clipFrame < static_cast<int>(ticks * k.turnTicks / k.fps + half) &&
      0 <= static_cast<int>(req.U(moveword::kCommandSpeedClass)) && req.U(moveword::kPrecise) == 1) {
    const float facingToLook = std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kLookHeading), k));   // fVar21
    const float rowToLook = std::fabs(Wrap180Diff(req.F(kTurnedHeadingWord), req.F(moveword::kLookHeading), k));     // fVar22
    if ((facingToLook < rowToLook || facingToLook < narrow225) &&
        (k.angle3375 < std::fabs(Wrap180Diff(req.F(kTurnedHeadingWord), req.F(moveword::kHeading), k)) || turn7875 < rowToLook)) {
      AdjustAngleApply(host);
      req.SetByte(moveword::kReorientFlag, 0);  // param_2 + 0x3c = 0
      const uint32_t bucket = req.U(moveword::kPrevBucket);  // uVar23 = param_2[0x25]
      float lowBand = k.five, highBand = k.speedB;
      if (turn675 < std::fabs(req.F(moveword::kTurn))) {
        lowBand = k.speedA;
        highBand = k.speed135;
      }
      const float disp = DispSpeedKmh(host, k);
      if (lowBand <= disp) {
        if (highBand <= disp) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kSpeedWord, kSpeedWord16);
        } else {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kSpeedWord, kSpeedWord11);
        }
      } else {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kSpeedWord, kSpeedWord5);
      }
      int cls = static_cast<int>(req.U(moveword::kClassWord));
      cls = cls < 3 ? (cls < 0 ? 0 : cls) : 2;
      const float word = req.F(moveword::kSpeedWord);
      float speed = k.speedD;  // DAT_14259be10: a word over 16 stays 16
      if (word <= k.speedD) {
        speed = k.five;
        if (k.five <= word) speed = word;
      }
      const uint32_t kind = req.U(moveword::kKind);
      const uint32_t kindSel = kind < 2 ? 2u : (kind == 4 ? 5u : kind);
      const int column = k.bucketColumn[bucket];
      uint32_t row = static_cast<uint32_t>(k.stepRow[kindSel + static_cast<uint32_t>(cls) * 8u]);
      if (row == kRowEchoStep) row = kRowEchoStepAlt;  // 0x52e -> 0x530
      const RowPick pick = PickRow(host, static_cast<int>(row), column);
      req.SetByte(moveword::kFootFlag, k.bucketLeft[bucket]);
      req.SetF(moveword::kSpeedWord, speed);
      req.SetF(moveword::kPrevSpeedWord, speed);
      req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));
      req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(cls));
      req.SetU(moveword::kPrevBucket, bucket);
      req.SetU(kLiveBucket, bucket);
      const float playback = PlaybackAt(host, k);
      req.SetF(moveword::kPlayback, playback);
      const float start = host.MotionStartTime(static_cast<uint16_t>(pick.motion));
      IssueClip(host, pick.motion, pick.mirror, k.westBlend, playback, start);
      req.SetU(moveword::kDwellElapsed, 0);  // param_2[0x1a]
      req.SetU(moveword::kDwellLimit, 0);    // param_2[0x1b]
      req.SetU(moveword::kPrevKind, kindSel);  // param_2[0x1e]
      RequestState(req, MoveState::kStepLoop);  // req[0x39] = 4
      return;
    }
  }

  // 4. The post-branch prologue (0x1406c7b00): the one-tick displacement speed (fVar21 = XMM9), the saved speed and class words
  // (local_100 = [rsp+0x48], local_fc = [rsp+0x4c]) and the first draw. 0x1406c7bb3 is FUN_14043a0a0(req[0x18], req[1]) (XMM0 <-
  // [R14+0x60], XMM1 <- [R14+4]), kept signed in [rsp+0x44] and as its magnitude d1 in [rsp+0x158] (the old port read req[4]).
  const float dispKmh = DispSpeedKmh(host, k);
  const uint32_t savedSpeed = req.U(moveword::kSpeedWord);
  const uint32_t savedClass = req.U(moveword::kClassWord);
  const float d1Signed = Wrap180Diff(req.F(kTurnedHeadingWord), req.F(moveword::kHeading), k);
  const float d1 = std::fabs(d1Signed);
  const auto cmdClass = [&req] { return static_cast<int>(req.U(moveword::kCommandSpeedClass)); };
  const auto liveClass = [&req] { return static_cast<int>(req.U(moveword::kClassWord)); };
  const auto precise = [&req] { return req.U(moveword::kPrecise); };
  const auto setClassSpeed = [&req](uint32_t cls, float speed) {
    req.SetU(moveword::kClassWord, cls);
    req.SetF(moveword::kSpeedWord, speed);
  };
  // The class-by-speed ladder PES writes at 0x1406c82c0..0x1406c82e9 and 0x1406c899c..0x1406c89ff: class 3 / 28 unless the ground
  // speed is under 8 (class 0, 5), under 16 (class 1, 11) or under 22 (class 2, 16). The speed words are the immediates 5, 11, 16.
  const auto classOfSpeed = [&](float kmh) {
    setClassSpeed(3, kSpeedWord28);
    if (k.speedA <= kmh) {
      if (k.speedD <= kmh) {
        if (kmh < k.speed22) setClassSpeed(2, kSpeedWord16);
      } else {
        setClassSpeed(1, kSpeedWord11);
      }
    } else {
      setClassSpeed(0, kSpeedWord5);
    }
  };
  // 0x1406c7bf3..0x1406c7c23: the early gate. [R14+0x3f] is a BYTE offset (the top byte of req[0xf]), not word 0x3f.
  if (static_cast<uint32_t>(savedClass) - 1u < 2u && 2 < cmdClass() && precise() == 0 && d1 < k.angle45 &&
      host.AnimeState() != pes::kReplayAnimeState && !req.FlagAtByte(0x3f) && pes::EnterGotoTurn(host, req)) {
    return;  // FUN_1406d01d0 entered GOTO_RUN (0x1406c7c1c)
  }
  const int sixTicks = static_cast<int>(ticks * k.turnTicks / k.fps + half);  // 0x1406c7c68..0x1406c7c86
  if (motion == 0xe96 && dispKmh < k.five && 1 < cmdClass()) {  // 0x1406c7c32..0x1406c7c4f
    EnterGotoRun(host, req, false);                                // LAB_1406c7f7c
    return;
  }
  if (cmdClass() != -1 && clipFrame < sixTicks && turn675 < d1) {  // 0x1406c7c55..0x1406c7c96
    if (dispKmh < k.turnTicksB) {                                  // 0x1406c7ca8
      EnterGotoRun(host, req, false);
      return;
    }
    setClassSpeed(2, kSpeedWord16);  // 0x1406c7cbb
    if (k.speedA <= dispKmh) {
      if (dispKmh < k.speed135) setClassSpeed(1, kSpeedWord11);  // 0x1406c7cdf..0x1406c7cf2
    } else {
      setClassSpeed(0, kSpeedWord5);
    }
    if (EnterStepTurnRun(host, req, k)) return;  // 0x1406c7cfc
    req.SetU(moveword::kSpeedWord, savedSpeed);
    req.SetU(moveword::kClassWord, savedClass);
  }

  // 0x1406c7d34..0x1406c7d7a: the turn tolerance. Away from a precise arrival it grows with the target distance.
  float tolerance = turn675;
  if (precise() == 0 && 0 < liveClass()) {
    const float dist = req.F(moveword::kDistToTarget);
    float ramp = 0.0f;
    if (k.two < dist) ramp = dist < k.four ? (dist - k.two) * half : k.one;
    tolerance = ramp * k.angle3375 + turn675;
  }
  // 0x1406c7d8b: FUN_14043a0a0(req[2], req[4]) (XMM3 keeps it for the bucket at 0x1406c7efb).
  const float facingToBody = Wrap180Diff(req.F(moveword::kFacing), req.F(kBodyDirectionWord), k);
  const bool jostle = rowEndLive <= clipFrame || std::fabs(facingToBody) < k.angle45;  // bVar3

  // LAB_1406c7ee0: the run-stop entry at the bucket of the facing-to-body angle (vt+0x60 with XMM1 = XMM3, R8D = 10, R9D = 0):
  // req[0x25] <- req[0x20], req[0x20] <- bucket (words 0x94 and 0x80, not req[8]).
  const auto stopAtBucket = [&] {
    req.SetU(moveword::kPrevBucket, req.U(kLiveBucket));
    req.SetU(kLiveBucket, static_cast<uint32_t>(AngleBucket(host.AngleBuckets(), facingToBody, kLiveBucketAngle, false)));
    EnterStop(host, req);  // 0x1406c7f12
  };
  // joined_r0x0001406c7e18: the class-3 request enters TURN_STOP through the speed ladder (0x1406c7e1a..0x1406c7e87); the speed
  // word of class 1 is the immediate 11.
  const auto turnStopLadder = [&] {
    if (dispKmh < k.speedA) {
      setClassSpeed(0, kSpeedWord5);
    } else if (dispKmh < k.speed1475) {
      setClassSpeed(1, kSpeedWord11);
    } else if (dispKmh < k.speed19) {
      setClassSpeed(2, kSpeedWord16);
    }
    EnterTurnStop(host, req);  // FUN_1406d2590
  };
  // LAB_1406c825d: class 0 / speed 5 and the go-to-run entry.
  const auto gotoRunSlow = [&] {
    setClassSpeed(0, kSpeedWord5);
    EnterGotoRun(host, req, false);
  };
  // |req[7]| < 90 (XMM14) unless the arrival is precise: the gate both slow exits share (0x1406c8239..0x1406c825b).
  const auto slowGate = [&] { return precise() != 1 || std::fabs(req.F(moveword::kFacingError)) < k.angle90; };

  if ((cmdClass() == -1 && precise() == 1) || tolerance < d1) {  // 0x1406c7d9a
    if (cmdClass() == -1) {
      if (jostle) {
        if (k.five <= dispKmh) {
          setClassSpeed(3, kSpeedWord28);
          turnStopLadder();
          return;
        }
      } else if (k.five <= dispKmh) {
        if (k.speedB <= dispKmh) {
          setClassSpeed(2, kSpeedWord16);
        } else {
          setClassSpeed(1, kSpeedWord11);  // 0x1406c7ec2: the immediate 11, not the 8.0 band
        }
        stopAtBucket();
        return;
      }
      setClassSpeed(0, kSpeedWord5);
      stopAtBucket();
      return;
    }
    // 0x1406c7f1c: a command that names a speed class.
    if (!(sixTicks <= clipFrame && clipFrame < bandA)) {  // else LAB_1406c8375
      if (precise() == 0 && (liveClass() < 1 || (dispKmh < k.five && clipFrame < bandB))) {
        EnterGotoRun(host, req, false);  // 0x1406c7f7c
        return;
      }
      if (bandA <= clipFrame && std::fabs(facingToBody) < k.angle3375) {  // 0x1406c7fac
        if (0 < cmdClass() && 0 < liveClass()) {
          const float live = host.RateCeiling();  // (+0xac0 vt+0x18), not the command's speed
          if (precise() == 1) {
            req.SetF(moveword::kRate, kRate06);
            const float startThreshold = (live - k.startHalf) * half;  // XMM6, the entry's threshold
            const float floorBand = 2 < liveClass() ? k.five : k.speedA;
            if (k.speedA <= dispKmh || floorBand <= dispKmh) {
              if (k.speed135 <= dispKmh) {
                if (k.speed22 < dispKmh) req.SetF(moveword::kRate, live);
              } else {
                req.SetF(moveword::kRate, kRate03);
              }
            } else {
              req.SetF(moveword::kRate, kRate015);
            }
            req.SetU(moveword::kClassWord, static_cast<uint32_t>(pes::ClassForRateFor(req.F(moveword::kRate), host.RunTablesView())));  // vt+0x68
            req.SetF(moveword::kSpeedWord, pes::RateToSpeedFor(req.F(moveword::kRate), host.PhysiqueG(), host.RunTablesView(), host.RunConstantsView().rampSlope) * k.kmhToRate);  // vt+0x48
            if (EnterRunTurnStep(host, req, k, startThreshold)) return;  // 0x1406c80c5
            if (liveClass() == 0) {
              EnterGotoRun(host, req, false);  // 0x1406c80e6
              return;
            }
          } else if (StepLetsRunTurn(host, req)) {  // 0x1406c80f6: FUN_1406b70d0
            req.SetF(moveword::kRate, kRate06);
            if (k.speedA <= dispKmh) {
              if (k.speed135 <= dispKmh) {
                if (k.speed22 < dispKmh) req.SetF(moveword::kRate, live * k.playbackFloor);
              } else {
                req.SetF(moveword::kRate, kRate03);
              }
            } else {
              req.SetF(moveword::kRate, kRate015);
            }
            req.SetU(moveword::kClassWord, static_cast<uint32_t>(pes::ClassForRateFor(req.F(moveword::kRate), host.RunTablesView())));
            req.SetF(moveword::kSpeedWord, pes::RateToSpeedFor(req.F(moveword::kRate), host.PhysiqueG(), host.RunTablesView(), host.RunConstantsView().rampSlope) * k.kmhToRate);
            EnterRunTurnRun(host, req, k);  // 0x1406c81b7
            return;
          }
        }
        // 0x1406c81c1..0x1406c81ed (class -1 with a precise arrival) is unreachable here: this arm is only entered with
        // req[9] != -1 (0x1406c7c55 sends -1 to the arm above) and no entry in between writes req[9].
      }
      if (0 < liveClass()) {  // 0x1406c8212..0x1406c832d, req[9] != -1
        const float floorBand = bandA <= clipFrame ? k.speedA : k.five;  // XMM6
        if (dispKmh < floorBand && slowGate()) {
          gotoRunSlow();
          return;
        }
        classOfSpeed(dispKmh);
        if (EnterStepTurnRun(host, req, k)) return;  // 0x1406c82ef
        if (dispKmh < floorBand + k.one && slowGate()) {  // 0x1406c82fc
          gotoRunSlow();
          return;
        }
      }
      if (std::fabs(req.F(moveword::kFacingError)) < turn675 || k.angle90 < d1) {  // 0x1406c832d..0x1406c8352
        EnterStop(host, req);                                                       // 0x1406c835a
        return;
      }
    }
  }

  // LAB_1406c8375: the damped facing (0x1406c837b..0x1406c8619). The signs compare the signed first draw with the row's offset.
  const auto signOf = [&k](float v) { return v < 0.0f ? k.negOne : k.one; };
  bool damp = false;
  if (rowEndLive <= clipFrame && (rowOffset != k.zero || bandA <= clipFrame)) {
    damp = true;
  } else if ((rowStartLive + rowEndLive) / 2 <= clipFrame && k.angle1125b < d1 &&
             std::fabs(Wrap180Diff(req.F(kBodyDirectionWord), req.F(moveword::kHeading), k)) < narrow225) {  // 0x1406c83c2: (req[4], req[1])
    damp = true;
  } else if (static_cast<float>(rowEndLive) * k.p8 <= static_cast<float>(clipFrame) && k.angle1125b < d1 && rowOffset != k.zero &&
             signOf(d1Signed) != signOf(rowOffset)) {  // 0x1406c83dc..0x1406c8453
    damp = true;
  }
  if (damp) {  // LAB_1406c8459
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    const float facing = FoldUnit(host.FacingDeg(), k);  // *(P+0x20)
    float delta = FoldUnit(req.F(moveword::kHeading), k) - facing;
    if (k.halfTurn < std::fabs(delta)) delta -= signOf(delta) * k.fullTurn;  // FUN_14043a0a0's body, inline
    if (std::fabs(delta) < k.angle90) {
      float step = k.halfTurn / ticks;
      if (liveClass() == 3) step = k.angle135 / ticks;
      if (clipFrame < rowEndLive && rowOffset != k.zero && signOf(d1Signed) != signOf(rowOffset)) step = k.angle90 / ticks;
      if (motion == 0xc5d && clipFrame < bandA) step = k.angle90 / ticks;
      float turn = step;  // clamp(delta, -step, step) as PES orders it (0x1406c85ae..0x1406c85bb)
      if (delta <= step) turn = std::max(-step, delta);
      const float damped = FoldUnit(facing + turn, k);
      req.SetByte(moveword::kRootFacing, 1);  // req + 0xe8
      req.SetF(moveword::kRootFacingAngle, damped);
      if (rowEndLive <= clipFrame) req.SetF(kTurnedHeadingWord, damped);  // req[0x18] (byte 0x60), a float
      host.SetRecordF0bec(damped);                                         // P+0xbec
    }
  }
  req.SetU(moveword::kSpeedWord, savedSpeed);  // 0x1406c8619..0x1406c8632: both words are restored on every path
  req.SetU(moveword::kClassWord, savedClass);

  // 0x1406c8636..0x1406c8757: a class-3 command right after the first band re-enters the 0x3b clip.
  if (bandA <= clipFrame && static_cast<int>(savedClass) < 3 && cmdClass() == 3 && host.CurrentMotion() == 0x2e && d1 < k.angle45 &&
      clipFrame < bandB) {
    const int clipFrames = host.CurrentClipFrames();  // vt+0x38
    const bool animMirror = req.Flag(moveword::kMirror);  // vt+0x30
    IssueClip(host, 0x3b, !animMirror, k.turnRamp, req.F(kPlaybackWord), static_cast<float>(clipFrames + 1) / ticks);
    req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));  // req[0x24] (byte 0x90)
    req.SetU(kSavedSpeed, req.U(moveword::kSpeedWord));               // req[0x22] (byte 0x88)
    req.SetF(moveword::kSpeedWord, kSpeedWord28);
    req.SetU(moveword::kClassWord, 3);
    req.SetU(moveword::kPrevKind, req.U(moveword::kKind));              // req[0x1e] <- req[0]
    req.SetU(kTurnedHeadingWord, req.U(moveword::kHeading));            // req[0x18] <- req[1]
    req.SetU(kLiveBucket, req.U(moveword::kMoveBucket));                // req[0x20] <- req[8]
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    return;
  }

  // 0x1406c8778..0x1406c8a12: the re-select tests; `restless` is bVar3 (BL).
  bool restless = false;
  if ((clipFrame < rowEndLive && k.angle3375 <= std::fabs(facingToBody)) || clipFrame < bandA) {  // 0x1406c8789
    if (static_cast<uint32_t>(liveClass()) - 1u < 2u && 2 < cmdClass() && precise() == 0 && d1 < k.angle45 && bandA <= clipFrame &&
        k.angle45 < std::fabs(facingToBody) && motion != 0xb32 && motion != 0xc68) {  // 0x1406c8927
      if (!IsFadingMotion(motion) || bandB <= clipFrame) {                           // FUN_140a6e4f0
        classOfSpeed(dispKmh);
        if (EnterStepTurnRun(host, req, k)) return;  // 0x1406c8a05
      }
    }
  } else if ((precise() == 1 && k.angle45 < std::fabs(req.F(moveword::kFacingError))) || (cmdClass() == 0 && liveClass() != 0)) {
    restless = true;  // LAB_1406c88de
  } else if (std::fabs(req.F(kTurnWord)) <= k.angle5625 || d1 <= k.angle5625) {
    bool decided = false;
    if (2 < cmdClass() && liveClass() < 3 && (!IsFadingMotion(motion) || bandB <= clipFrame)) {
      if (motion != 0xc81 || k.angle45 < d1 || rowEndLive <= clipFrame) {
        // 0x1406c8859..0x1406c88a1: the displacement's forward component (FUN_141eb50b0, FUN_140581ce0, FUN_140591a10) in km/h.
        if (k.turnTicksB < ForwardDisplacementKmh(host, k)) restless = true;
      }
      decided = true;  // goto LAB_1406c8a12
    }
    if (!decided && ((cmdClass() < 2 && 1 < liveClass()) || (cmdClass() < 3 && 2 < liveClass()))) restless = true;
  } else {
    restless = true;
  }

  // LAB_1406c8a12: iVar10 is now the FIXED frames (the live end is not used below).
  if (!(liveClass() < 1 || cmdClass() != 3 || k.angle45 <= d1) && rowFixed <= clipFrame && dispKmh < k.speed22) {  // 0x1406c8a28..0x1406c8a3e
    setClassSpeed(2, kSpeedWord16);
    if (EnterAccelerate(host, req)) return;  // 0x1406c8a56: FUN_1406ccd00
  }
  if (clipFrame < rowFixed && !restless) return;  // 0x1406c8a63..0x1406c8a6a

  // 0x1406c8a70: the foot flag, the ground speed over the four-tick window and the final class pick.
  const int foot = FootBandOf(host);  // (P+0xab8 vt+0x78)()
  req.SetByte(kFootFlagWord, (foot - 1U < 2U) ? 1 : 0);
  // ponytail: FUN_141eb50b0(P, v, 4) reads ring entry 0 at P+0x2fc, whose writer is not decoded (docs/re/pes-re-movestep-wave1.md,
  // pes-re-a3helpers.md Stage 1a); the adapter answers a position for index >= 2, which reads as thousands of km/h, so the last
  // tick's displacement stands in for it until the ring writer is decoded.
  const float sampleKmh = dispKmh;
  if (clipFrame < rowFixed) {  // 0x1406c8aae
    if (k.five <= dispKmh || static_cast<uint32_t>(liveClass()) - 2u < 2u) {
      bool picked = false;
      if (dispKmh < k.speedB && (liveClass() != 3 || clipFrame < bandB)) {
        if (!IsFadingMotion(motion) || clipFrame < bandB) {  // 0x1406c8c17
          setClassSpeed(1, kSpeedWord11);
          picked = true;  // goto LAB_1406c8b55
        }
      }
      if (!picked) {
        if (k.speed22 <= dispKmh || (liveClass() == 3 && bandB <= clipFrame)) {
          setClassSpeed(3, kSpeedWord28);
        } else {
          setClassSpeed(2, kSpeedWord16);
        }
      }
    } else {
      setClassSpeed(0, kSpeedWord5);
    }
  }
  // LAB_1406c8b55: the dwell words clear and the run entry takes the foot band (vt+0 = FUN_1406d2030).
  req.SetU(kDwellLimitWord, 0);
  req.SetU(kDwellLimit2Word, 0);
  req.SetByte(kStyleWord, 0);
  EnterRun(host, req, foot, false);
  req.SetU(moveword::kPrevKind, req.U(moveword::kKind));  // req[0x1e] <- req[0]
  if (liveClass() == 3 && sampleKmh < k.speed26) {         // 0x1406c8b88..0x1406c8ba6
    const float live = host.RateCeiling();                 // (+0xac0 vt+0x18), every read below
    float rate;
    if (k.speedE < sampleKmh) {
      rate = (live + live + k.startHalf) / k.three;
    } else if (k.speed22 < sampleKmh) {
      rate = (live + live + live + k.startHalf + k.startHalf) / k.five;
    } else if (k.stampsA < sampleKmh) {
      rate = (live + k.startHalf) * half;
    } else {
      rate = kRate06;
    }
    // 0x1406c8d00..0x1406c8d20: the rate is clamped to [0, live]: above live it becomes live, below 0 it becomes 0.
    req.SetF(moveword::kRate, live < rate ? live : std::max(0.0f, rate));
  }
}

// ---------------------------------------------------------------------------------------------
// FUN_1406c8d80: STEP_TURN_STEP, the step-to-step turn. Re-ported from the instruction stream (0x1406c8d80..0x1406c9dd0) and
// the decompile; every [R14+n] below is a BYTE offset (req word n / 4), every hidden FUN_14043a0a0 argument was read from the
// registers the stream loads (args2: 0x1406c8fe5 (req[0x17], req[1] - table), 0x1406c905a (req[2], row), 0x1406c920e and
// 0x1406c966c (req[2], req[4]), 0x1406c9882 (req[0x18], req[4]), 0x1406c98c9 (req[0x18], req[1]), 0x1406c996a (req[2], req[4]),
// 0x1406c99e7 (req[2], req[2] + table), 0x1406c9a87 (req[4], req[1])).
// ---------------------------------------------------------------------------------------------
void StepTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int clipFrame = host.CurrentClipFrames();  // (P+0xab8 vt+0x38)(), R12D
  const float clipLength = host.RecordF2468();     // P+0x2468: no floor in this handler (no DAT_1425ab120 read)
  const float scale = k.one;
  const float ticks = static_cast<float>(host.TicksPerSecond());  // FUN_141e532d0
  const float half = k.half;
  const int motion = host.CurrentMotion();  // (P+0xab8 vt+0x18)()
  const int rowStart = RecordStartFrames(host, motion, scale);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, req.Flag(moveword::kMirror), k);  // FUN_1406b1c70
  const int rowFixed = RecordFixedFrames(host, motion, scale);  // FUN_1406af620, [rsp+0x168]
  // FUN_1406af8e0's walk (0x1406c8dd9..0x1406c8e5b): local_res8 = band 0's frames ([rsp+0x160]), the FIXED frames without a band.
  const int bands = host.Motion(motion).CancelCount();  // FUN_1406afbc0(..., 5)
  int bandA = rowFixed;
  if (0 < bands) bandA = host.ClipTicks(pes::ClipReader::kCancel, motion, scale, 0);
  // 0x1406c8e5b..0x1406c8f48: a row with no offset starts at 0 and ends after the tenth of a second; E = min(end, FIXED frames).
  int rowStartLive = rowStart;
  int rowEndLive = rowEnd;
  if (rowOffset == 0.0f) {
    rowStartLive = 0;
    rowEndLive = static_cast<int>(ticks * k.ten / k.fps + half);
  }
  if (rowFixed < rowEndLive) rowEndLive = rowFixed;

  // The reorient window (0x1406c8f84..0x1406c9124): S <= F < (S + 2E) / 3 (IMUL 0x55555556), not the run handler's (S + E) / 2.
  // The step is taken from the row heading req[0x17] toward the heading less the live bucket's table angle; req[0x3d] gets the
  // stepped row, req[0x18] the row plus the table angle.
  if (!FacingAdjusterBusy(host) && rowStartLive <= clipFrame && clipFrame < (rowStartLive + 2 * rowEndLive) / 3) {
    float row = req.F(kRowHeadingWord);  // [RSI+0x5c]
    const uint32_t frames = static_cast<uint32_t>(static_cast<float>(rowEndLive - clipFrame) / clipLength + half);
    const float tableAngle = k.bucketAngle[req.U(kLiveBucket)];  // DAT_1425dd660[req[0x20]]
    const float toward = req.F(moveword::kHeading) - tableAngle;  // XMM6
    if (std::fabs(Wrap180Diff(row, toward, k)) < k.angle90) row = StepHeadingToPoint(row, toward, k.angle225m, k);  // 0x1406c9017
    const float turned = FoldUnit(row + tableAngle, k);
    const float facingToRow = Wrap180Diff(req.F(moveword::kFacing), row, k);  // 0x1406c905a, signed (XMM3)
    if (std::fabs(facingToRow) / static_cast<float>(static_cast<int>(frames)) < k.angle45) {
      req.SetF(moveword::kAngle3d, row);      // req[0x3d] (+0xf4)
      req.SetByte(moveword::kReorientFlag, 1);  // +0xf0
      req.SetU(moveword::kTurnFrames, frames);  // +0xf8
      req.SetByte(moveword::kMirrorFlag, 0);    // +0xfc
      req.SetF(kTurnedHeadingWord, turned);     // +0x60
      if (k.angle90 < std::fabs(rowOffset) && (rowOffset < 0.0f) != (facingToRow < 0.0f)) {  // 0x1406c90bb..0x1406c90f3
        if (k.angle225m < std::fabs(facingToRow)) req.SetByte(moveword::kMirrorFlag, 1);
      }
    }
  }

  const auto cmdClass = [&req] { return static_cast<int>(req.U(moveword::kCommandSpeedClass)); };
  const auto liveClass = [&req] { return static_cast<int>(req.U(moveword::kClassWord)); };
  const auto precise = [&req] { return req.U(moveword::kPrecise); };
  const auto setClassSpeed = [&req](uint32_t cls, float speed) {
    req.SetU(moveword::kClassWord, cls);
    req.SetF(moveword::kSpeedWord, speed);
  };
  // 0x1406c9146..0x1406c91bf: d1 = |heading - turned row| (both folded inline); XMM10.
  const float d1 = std::fabs(Wrap180Diff(req.F(kTurnedHeadingWord), req.F(moveword::kHeading), k));
  // 0x1406c91c4..0x1406c9200: the tolerance is 45, widened by 22.5 over the 2..4 m distance ramp away from a precise arrival.
  float tolerance = k.angle45;
  if (precise() == 0 && 0 < liveClass()) {
    const float dist = req.F(moveword::kDistToTarget);
    float ramp = 0.0f;
    if (k.two < dist) ramp = dist < k.four ? (dist - k.two) * half : scale;
    tolerance = ramp * k.angle225m + k.angle45;
  }
  const float facingToBody = Wrap180Diff(req.F(moveword::kFacing), req.F(kBodyDirectionWord), k);  // 0x1406c920e, XMM9
  // 0x1406c92f8 / 0x1406c9671: the run-stop entry at the bucket of the facing-to-body angle (req[0x25] <- req[0x20]).
  const auto stopAtBucket = [&] {
    req.SetU(moveword::kPrevBucket, req.U(kLiveBucket));
    req.SetU(kLiveBucket, static_cast<uint32_t>(AngleBucket(host.AngleBuckets(), facingToBody, kLiveBucketAngle, false)));
    EnterStop(host, req);
  };

  // 0x1406c9213..0x1406c94d3: while the clip is young (and the command is not 0x11).
  if (req.U(moveword::kCommandId) != 0x11 && clipFrame < static_cast<int>(ticks * k.turnTicks / k.fps + half)) {
    if (cmdClass() == -1) {  // 0x1406c924e
      stopAtBucket();
      return;
    }
    if (k.angle5625 <= d1) {  // 0x1406c9254
      const float kmh = DispSpeedKmh(host, k);
      if (kmh < k.speedA) {
        setClassSpeed(0, kSpeedWord5);
      } else if (kmh < k.speed135) {
        setClassSpeed(1, kSpeedWord11);
      } else {
        setClassSpeed(2, kSpeedWord16);
      }
      if (EnterStepTurnRun(host, req, k)) return;  // 0x1406c92eb
      stopAtBucket();
      return;
    }
    // 0x1406c9332..0x1406c94d3: a class-3 command right after the turn starts replays one of three clips into STEP_TURN_RUN.
    if (d1 < k.angle1125b && liveClass() < 3 && cmdClass() == 3) {
      int clip = -1;
      bool mirrorSameAsAnim = false;
      float blend = 0.0f;
      if (motion == 0x35) {
        clip = 0x3f;
        blend = k.loopBlend;  // DAT_142588200
      } else if (motion == 0x39) {
        clip = 0x4e;
        mirrorSameAsAnim = true;  // 0x1406c947b: MOVZX R9D,AL (not negated)
        blend = k.pointTwo;       // DAT_142598578
      } else if (motion == 0xc51) {
        clip = 0xc6d;
        mirrorSameAsAnim = true;  // 0x1406c94ca
        blend = k.pointTwo;
      }
      if (0 <= clip) {
        const float startRate = static_cast<float>(host.CurrentClipFrames() + 1) / ticks;
        const bool animMirror = req.Flag(moveword::kMirror);  // vt+0x30
        IssueClip(host, clip, mirrorSameAsAnim ? animMirror : !animMirror, blend, req.F(kPlaybackWord), startRate);  // 0x1406c93d5
        req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));  // +0x90 <- +0x7c
        req.SetU(kSavedSpeed, req.U(moveword::kSpeedWord));               // +0x88 <- +0x74
        req.SetU(moveword::kPrevKind, req.U(moveword::kKind));            // +0x78 <- [R14]
        req.SetU(kTurnedHeadingWord, req.U(moveword::kHeading));          // +0x60 <- +4
        req.SetU(kLiveBucket, req.U(moveword::kMoveBucket));              // +0x80 <- +0x20
        setClassSpeed(3, kSpeedWord28);
        RequestState(req, MoveState::kStepTurnRun);  // +0xe0 = 1, +0xe4 = 0xf
        req.SetByte(moveword::kReorientFlag, 0);
        AdjustAngleApply(host);
        return;
      }
    }
  }

  // 0x1406c94d3: nothing below runs before the first band ends.
  if (clipFrame < bandA) return;
  // 0x1406c94e7: (P+0xab8 vt+0) is a getter whose result is unused.
  if (host.AnimeState() != pes::kReplayAnimeState &&
      ((cmdClass() == -1 && precise() == 1) || tolerance < d1)) {  // 0x1406c9504..0x1406c9516
    if (cmdClass() != -1 && precise() == 0 && liveClass() < 1) {
      EnterGotoRun(host, req, false);  // 0x1406c95d6 (the class and speed words are not rewritten here)
      return;
    }
    if (cmdClass() != -1 && 0 < liveClass()) {
      const float kmh = DispSpeedKmh(host, k);  // 0x1406c9555..0x1406c9581
      bool slowExit = false;                     // LAB_1406c95c8: class 0, speed 5, go-to-run
      if (motion == 0x4d || k.turnTicksB <= kmh) {
        // LAB_1406c95fe: the band of the speed, 13.5 splitting class 1 (11) from class 2 (16).
        const auto band = [&] { setClassSpeed(k.speed135 <= kmh ? 2u : 1u, k.speed135 <= kmh ? kSpeedWord16 : kSpeedWord11); };
        bool tryEntry = true;
        if (std::fabs(req.F(kTurnWord)) <= k.angle135) {  // 0x1406c95b5
          if (k.speedA <= kmh) {
            band();
          } else {
            setClassSpeed(0, kSpeedWord5);  // 0x1406c95ee
          }
        } else if (kmh < k.speedA) {  // 0x1406c95c3
          tryEntry = false;
          slowExit = true;
        } else {
          band();
        }
        if (tryEntry) {
          if (EnterStepTurnRun(host, req, k)) return;  // 0x1406c962b
          if (kmh < k.speed55) slowExit = true;          // 0x1406c9640: DAT_1425c8378 > kmh
        }
      } else {
        slowExit = true;  // 0x1406c95a3: not the 0x4d clip and under 2.5 km/h
      }
      if (slowExit) {
        setClassSpeed(0, kSpeedWord5);
        EnterGotoRun(host, req, false);  // 0x1406c95df
        return;
      }
    }
    if (std::fabs(req.F(moveword::kFacingError)) < k.angle675) {  // 0x1406c9645..0x1406c9660
      stopAtBucket();
      return;
    }
  }

  // 0x1406c96aa: once the live end is reached the body faces away from the bucket's side, damped toward the look heading.
  if (rowEndLive <= clipFrame) {
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    const uint32_t live = req.U(kLiveBucket);
    float target = req.F(moveword::kHeading);
    if (live == 7) {
      target = FoldUnit(target + k.angle90, k);
    } else if (live == 2) {
      target = FoldUnit(target - k.angle90, k);
    } else {
      target = FoldUnit(target - k.halfTurn, k);
    }
    float toFacing = target - host.FacingDeg();  // *(P+0x20), not folded
    if (k.halfTurn < std::fabs(toFacing)) toFacing -= (toFacing < 0.0f ? k.negOne : scale) * k.fullTurn;
    float toLook = target - FoldUnit(req.F(moveword::kLookHeading), k);
    if (k.halfTurn < std::fabs(toLook)) toLook -= (toLook < 0.0f ? k.negOne : scale) * k.fullTurn;
    toLook = std::fabs(toLook);
    float ramp = 0.0f;
    if (k.angle225m < toLook) ramp = toLook < k.angle45 ? (toLook - k.angle225m) / k.angle225m : scale;
    const float step = (ramp * k.angle90 + k.angle90) / ticks;
    float turn = step;  // clamp(toFacing, -step, step) as PES orders it
    if (toFacing <= step) turn = std::max(-step, toFacing);
    req.SetByte(moveword::kRootFacing, 1);
    req.SetF(moveword::kRootFacingAngle, FoldUnit(turn + host.FacingDeg(), k));
  }
  // 0x1406c985e..0x1406c9933: `restless` is DIL.
  bool restless = false;
  if ((precise() == 0 && cmdClass() == 3) ||
      (cmdClass() < 1 && std::fabs(Wrap180Diff(req.F(kTurnedHeadingWord), req.F(kBodyDirectionWord), k)) < k.angle225m) ||
      (k.angle45 < std::fabs(req.F(kTurnWord)) &&
       k.angle45 < std::fabs(Wrap180Diff(req.F(kTurnedHeadingWord), req.F(moveword::kHeading), k))) ||
      (precise() == 1 && k.angle45 < std::fabs(req.F(moveword::kFacingError)) && rowEndLive <= clipFrame) ||
      (precise() == 0 && k.angle90 < std::fabs(req.F(kLookDeltaWord)) && rowEndLive <= clipFrame)) {
    restless = true;
  }
  if (clipFrame < rowFixed && !restless) return;  // 0x1406c9933..0x1406c993d

  // 0x1406c9946: the final step-row select.
  AdjustAngleApply(host);
  req.SetByte(moveword::kReorientFlag, 0);
  const float wFinal = Wrap180Diff(req.F(moveword::kFacing), req.F(kBodyDirectionWord), k);  // 0x1406c996a
  float bucketArg = wFinal;
  if (rowEndLive <= clipFrame) {  // 0x1406c996f..0x1406c99e7
    const uint32_t live = req.U(kLiveBucket);
    const float aw = std::fabs(wFinal);
    if ((live == 9 || live == 4) && k.angle135 < aw) {
      bucketArg = k.bucketAngle[live];  // 0x1406c99a1
    } else if ((live == 7 || live == 2) && k.angle45 < aw && aw < k.angle135) {
      bucketArg = Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kFacing) + k.bucketAngle[live], k);  // 0x1406c99e7
    }
  }
  const int bucket = AngleBucket(host.AngleBuckets(), bucketArg, kLiveBucketAngle, false);  // (+0xac0 vt+0x60)(arg, 10, 0)
  const int liveClass0 = liveClass();
  const float floorSpeed = liveClass0 < 2 ? k.five : k.speedB;  // XMM14 / XMM13
  int cls = cmdClass();
  const int below = 1 < liveClass0 ? 1 : 0;
  if (cls < 3) {
    if (cls < below) cls = below;
  } else {
    cls = 2;
  }
  int wantKmh = static_cast<int>(req.F(moveword::kDesiredSpeedKmh));
  if (wantKmh < 0x11) {
    if (wantKmh < static_cast<int>(floorSpeed)) wantKmh = static_cast<int>(floorSpeed);
  } else {
    wantKmh = 0x10;
  }
  float speedWord = static_cast<float>(wantKmh);
  // 0x1406c9a87: FUN_14043a0a0(req[4], req[1]) against 45, and the one-tick displacement speed against the 8 / 13.5 bands.
  const float lookToHeading = std::fabs(Wrap180Diff(req.F(kBodyDirectionWord), req.F(moveword::kHeading), k));
  const float kmhNow = DispSpeedKmh(host, k);
  if (lookToHeading <= k.angle45) {
    if (kmhNow < k.speedA && 1 < cls) {
      cls = 1;
      speedWord = kSpeedWord11;
    }
  } else if (k.speedA <= kmhNow) {
    if (kmhNow < k.speed135) {
      cls = 1;
      speedWord = kSpeedWord11;
    } else {
      cls = 2;
      speedWord = kSpeedWord16;
    }
  } else {
    cls = 0;
    speedWord = kSpeedWord5;
  }
  uint32_t kind = req.U(moveword::kKind);
  if (kind < 2) {
    kind = 2;
  } else if (kind == 4) {
    kind = 5;
  }
  const int column = k.bucketColumn[bucket];
  uint32_t row = static_cast<uint32_t>(k.stepRow[kind + static_cast<uint32_t>(cls) * 8u]);
  const bool edgeColumn = column == kColumnStop || column == 0;
  if (edgeColumn && row == kRowEchoSlow) {
    if (host.Percent(0x32)) row = kRowEchoSlowAlt;  // FUN_1406ebf30(P, 0x32)
  } else if (edgeColumn && row == kRowEchoMid) {
    if (host.Percent(0x14)) row = kRowEchoMidAlt;  // FUN_1406ebf30(P, 0x14)
  } else if (row == kRowEchoStep) {
    row = kRowEchoStepAlt;
  }
  const RowPick cell = PickRow(host, static_cast<int>(row), column);  // FUN_1406abb30 / FUN_1406abd60
  const uint16_t repMotion = static_cast<uint16_t>(cell.motion);
  req.SetByte(moveword::kFootFlag, k.bucketLeft[bucket]);             // +0x98
  req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));    // +0x90
  req.SetU(kSavedSpeed, req.U(moveword::kSpeedWord));                 // +0x88
  req.SetU(moveword::kPrevBucket, req.U(kLiveBucket));                // +0x94 <- +0x80
  req.SetF(moveword::kSpeedWord, speedWord);
  req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));
  req.SetU(kLiveBucket, static_cast<uint32_t>(bucket));
  const float playback = PlaybackAt(host, k);  // FUN_1406ba070
  req.SetF(moveword::kPlayback, playback);
  float start = host.MotionStartTime(repMotion);  // FUN_1406b97d0
  if (edgeColumn) {
    // The foot sub-record walk (0x1406c9b..): the sub's own foot pair (mirrored with the row), the frames of the matching one.
    const int footNow = FootBandOf(host);  // (P+0xab8 vt+0x78)()
    const int subCount = host.Motion(repMotion).FootCount();
    for (int i = 0; i < subCount; ++i) {
      int sub = host.Motion(repMotion).FootPair(i);
      if (cell.mirror) {
        if (sub == 0) sub = 2;
        else if (sub == 1) sub = 3;
        else if (sub == 2) sub = 0;
        else if (sub == 3) sub = 1;
      }
      if (footNow == sub) {
        const int back = host.ClipTicks(pes::ClipReader::kCancel, repMotion, scale, i);
        start = static_cast<float>(back + 1) / ticks;
        break;
      }
    }
  }
  IssueClip(host, repMotion, cell.mirror, k.clipBlend, playback, start);  // DAT_1425ddf50
  req.SetU(moveword::kPrevKind, kind);  // req[0x1e] <- the remapped kind
  req.SetU(moveword::kDwellElapsed, 0);
  req.SetU(moveword::kDwellLimit, 0);
  RequestState(req, MoveState::kStepLoop);  // +0xe0 = 1, +0xe4 = 4
}

// ---------------------------------------------------------------------------------------------
// The loader. Every entry names its VA; the install test checks their relations.
// ---------------------------------------------------------------------------------------------
std::optional<MoveTurnConstants> LoadMoveTurnConstants(const PeImage& exe, std::string* error) {
  MoveTurnConstants k{};
  for (const FloatSource& s : FLOAT_SOURCES) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*s.field)))) {
      *error = "turn constants: .rdata unreadable";
      return std::nullopt;
    }
  }
  if (!exe.ReadVa(0x1425dd660, sizeof k.bucketAngle,
                  reinterpret_cast<uint8_t*>(k.bucketAngle.data())) ||
      !exe.ReadVa(0x1425dd988, sizeof k.bucketColumn,
                  reinterpret_cast<uint8_t*>(k.bucketColumn.data())) ||
      !exe.ReadVa(0x1425dd950, sizeof k.bucketLeft,
                  reinterpret_cast<uint8_t*>(k.bucketLeft.data())) ||
      !exe.ReadVa(0x1425dd9b0, sizeof k.stepRow, reinterpret_cast<uint8_t*>(k.stepRow.data()))) {
    *error = "turn tables: .rdata unreadable";
    return std::nullopt;
  }
  return k;
}

}  // namespace movestep
}  // namespace pes
