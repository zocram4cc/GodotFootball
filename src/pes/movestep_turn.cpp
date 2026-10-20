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
#include "pes/movestep_step.hpp"

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
// The motion record reads. Each helper names the PES reader it stands for; all of them go through
// the host's one Motion() read, the install's motion table (pes/mbinfo.hpp).
// ---------------------------------------------------------------------------------------------
// A record value in ticks: FUN_1406af620, FUN_1406b1f00, FUN_1406b1d00 and FUN_1406af8e0 at a rate.
int FrameTicks(const MoveHost& host, int value, float scale, const MoveTurnConstants& k) {
  return pes::movestep::RecordTicks(host, static_cast<float>(value), scale, k.fps, k.half, k.one);
}
// FUN_1406b1f00 on RotData entry 0: the start, in ticks.
int RecordStartFrames(const MoveHost& host, int motionNo, float scale, const MoveTurnConstants& k) {
  return FrameTicks(host, host.Motion(motionNo).Rot(0).start, scale, k);
}
// FUN_1406b1d00 on RotData entry 0: the end, in ticks.
int RecordEndFrames(const MoveHost& host, int motionNo, float scale, const MoveTurnConstants& k) {
  return FrameTicks(host, host.Motion(motionNo).Rot(0).end, scale, k);
}
// FUN_1406b1c70 on RotData entry 0: the signed offset, negated when mirrored.
float RecordOffsetDeg(const MoveHost& host, int motionNo, bool mirror, const MoveTurnConstants& k) {
  const float offset = static_cast<float>(host.Motion(motionNo).Rot(0).offset);
  return offset * (mirror ? k.negOne : k.one);
}
// FUN_1406af620: the Animation blend, in ticks.
int RecordFixedFrames(const MoveHost& host, int motionNo, float scale, const MoveTurnConstants& k) {
  return FrameTicks(host, host.Motion(motionNo).BlendStart(), scale, k);
}

// FUN_1406ba7d0(P, req): the relax/aim flag the entries branch on. ponytail: its body.
bool EntryAimFlag(const MoveHost&, const MoveReq&) { return false; }

// FUN_1406e6660(ap, idx): the ability byte. ponytail: its body.
int AbilityByte(const MoveHost& host, int) {
  const int cls = host.CommandSpeedClass();
  return cls >= 0 ? 50 : 50;
}

// FUN_140a6e290 / FUN_140a6e2e0 / FUN_140a6e4f0: the anime player's own predicates on the live
// clip. ponytail: their bodies.
bool ClipRunning(const MoveHost&) { return false; }
bool ClipHalted(const MoveHost&) { return false; }
bool ClipFading(const MoveHost&) { return false; }

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
  pick.motion = cell.motionNo >= 0 ? cell.motionNo : row;
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

// FUN_1406c3470's step-family re-select (0x1406c422f..0x1406c4ba8): resolve the next row and
// column from the class/speed/kind words, issue the clip request and hand the tick to STEP_LOOP.
// PES reaches it twice - once from the ladder's fall-through and once from the exit gate - so
// both callers share this one copy of PES's order.
// ---------------------------------------------------------------------------------------------
void RunTurnStepReselect(MoveHost& host, MoveReq& req, const MoveTurnConstants& k, int clipFrame,
                         float spanF, float spanG, float dispBack, float rowFixed, float rowFixedB) {
  AdjustAngleApply(host);
  req.SetByte(moveword::kReorientFlag, 0);
  const int liveClass = static_cast<int>(req.U(moveword::kClassWord));
  {
    const int bucket = InterfaceBucket(host, req.F(moveword::kFacing));
    float wantSpeed = req.F(moveword::kDesiredSpeedKmh);
    const float want = liveClass < 2 ? k.five : k.speedB;
    const int below = 1 < liveClass ? 1 : 0;
    int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    if (cls < 2 || k.speedB <= dispBack) {
      if (3 <= cls) cls = 2;
    } else {
      cls = 1;
      wantSpeed = k.speedB;
    }
    if (cls < below) cls = below;
    int kmh = static_cast<int>(wantSpeed);
    if (kmh < 0x11) {
      if (kmh < static_cast<int>(want)) kmh = static_cast<int>(want);
    } else {
      kmh = 0x10;
    }
    int kind = static_cast<int>(req.U(moveword::kKind));
    if (kind < 2) {
      kind = 2;
    } else if (kind == 4) {
      kind = 5;
    }
    const uint32_t col = static_cast<uint32_t>(k.bucketColumn[bucket & 9]);
    uint32_t row = static_cast<uint32_t>(k.stepRow[(kind + cls * 8) & 63]);
    if (RowIs(col, 0x520u) || RowIs(col, 0u)) {
      if (RowIs(row, 0x546u)) {
        if (host.Percent(0x32)) row = 0x547u;  // FUN_1406ebf30
      } else if (RowIs(row, 0x542u)) {
        if (host.Percent(0x14)) row = 0x543u;  // FUN_1406ebf30
      }
    } else if (RowIs(row, 0x54eu)) {
      row = 0x54fu;
    }
    const RowPick cell = PickRow(host, static_cast<int>(row), static_cast<int>(col));
    const uint16_t repMotion = static_cast<uint16_t>(cell.motion);
    req.SetByte(moveword::kFootFlag, k.bucketLeft[bucket & 9]);  // param_2 + 0x26
    req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));  // param_2[0x24] = [0x1f]
    req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));  // param_2[0x22] = [0x1d]
    req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));    // param_2[0x25] = [0x20]
    req.SetF(moveword::kSpeedWord, static_cast<float>(kmh));  // param_2[0x1d] = (uint)(float)
    req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));  // param_2[0x20]
    req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));      // param_2[0x1f]
    const float playback = PlaybackAt(host, k);  // FUN_1406ba070(P)
    req.SetF(moveword::kPlayback, playback);
    float start = host.MotionStartTime(repMotion);  // FUN_1406b97d0
    if (col < 0x12 && ((0x22011u >> (col & 0x1f)) & 1u) != 0u) {
      // The foot-band sub-record walk (0x1406c4580..0x1406c4650): the sub's own foot pair,
      // mirrored when the row is mirrored, and the frames the matching sub gives.
      const int footNow = FootBandOf(host);
      const int subCount = host.Motion(repMotion).FootCount();
      if (0 < subCount) {
        for (int i = 0; i < subCount; ++i) {
          int sub = host.Motion(repMotion).FootPair(i);
          if (cell.mirror) {
            if (sub == 0) sub = 2;
            else if (sub == 1) sub = 3;
            else if (sub == 2) sub = 0;
            else if (sub == 3) sub = 1;
          }
          if (footNow == sub) {
            const int back = FrameTicks(host, host.Motion(repMotion).CancelFrames(i), k.one, k);
            start = static_cast<float>(back + 1) / static_cast<float>(host.TicksPerSecond());
            break;
          }
        }
      }
    }
    IssueClip(host, repMotion, cell.mirror, k.clipBlend, playback, start);
    req.SetU(moveword::kDwellElapsed, 0);  // param_2[0x1a] = 0
    req.SetU(moveword::kDwellLimit, 0);    // param_2[0x1b] = 0
    req.SetU(moveword::kPrevKind, static_cast<uint32_t>(kind));  // param_2[0x1e] = uVar34
    RequestState(req, MoveState::kStepLoop);  // LAB_1406c4ba8: req[0x39] = 4
  }
  (void)clipFrame;
  (void)spanF;
  (void)spanG;
  (void)rowFixed;
  (void)rowFixedB;
}
}  // namespace

// LAB_1406c7c9e / LAB_1406c8339: the joined TURN_STOP ladder. PES reaches the same ladder from
// the class-3 write and from the class-unset path, so the port funnels both into one block.
void TurnStopLadderOrStop(MoveHost& host, MoveReq& req, const MoveTurnConstants& k,
                          float dispKmh) {
  if (dispKmh < k.speedA) {  // DAT_14259bdf4
    req.SetU(moveword::kClassWord, 0);
    req.SetF(moveword::kSpeedWord, k.five);
    EnterTurnStop(host, req);  // FUN_1406d2590
    return;
  }
  if (dispKmh < k.speed1475) {
    req.SetF(moveword::kSpeedWord, k.speedA);
    req.SetU(moveword::kClassWord, 1);
    EnterTurnStop(host, req);
    return;
  }
  if (dispKmh < k.speed19) {
    req.SetU(moveword::kClassWord, 2);
    req.SetF(moveword::kSpeedWord, k.speedD);
  }
  EnterTurnStop(host, req);  // FUN_1406d2590
}

// LAB_1406c8459: the damped-facing block, reached from the two run-tail tests and by the goto in
// the else-if below. Both are the same block, so the port keeps one copy of PES's order.
void DampFacingTowardMove(MoveHost& host, MoveReq& req, const MoveTurnConstants& k,
                          float dispDeltaAbs, float rowOffset, int rowEndLive, float firstFrames,
                          int classWord) {
  req.SetByte(moveword::kReorientFlag, 0);  // param_2 + 0x3c = 0
  AdjustAngleApply(host);                   // (P+0x4d0 vt+0x08)
  const float negFull = k.negFullTurn;
  const float full = k.fullTurn;
  float facing = FoldUnit(req.F(moveword::kFacing), k);
  float heading = FoldUnit(req.F(moveword::kHeading), k);
  float delta = heading - facing;
  if (k.halfTurn < std::fabs(delta)) {
    delta -= (delta < 0.0f ? k.negOne : k.one) * k.fullTurn;
  }
  const float quarter = k.angle90;
  if (std::fabs(delta) < quarter) {
    const float ticks = static_cast<float>(host.TicksPerSecond());
    float step = k.halfTurn / ticks;
    if (classWord == 3) step = k.angle135 / ticks;
    if (classWord < static_cast<int>(firstFrames) && rowOffset != 0.0f) {
      const float signDisp = dispDeltaAbs < 0.0f ? k.negOne : k.one;
      const float signOffset = rowOffset < 0.0f ? k.negOne : k.one;
      if (signDisp != signOffset) step = quarter / ticks;
    }
    float clamped = step;
    if (delta <= step && WrapHalf(step, k) <= delta) clamped = delta;
    facing = FoldUnit(facing + clamped, k);
    req.SetByte(moveword::kRootFacing, 1);   // param_2 + 0x3a = 1
    req.SetF(moveword::kRootFacingAngle, facing);  // param_2[0x3b]
    if (rowEndLive <= host.CurrentClipFrames()) {
      req.SetU(moveword::kRowAngle, static_cast<uint32_t>(facing));  // param_2[0x18]
    }
    host.SetRecordF0bec(facing);  // P+0xbec
  }
  (void)negFull;
}


// FUN_1406c8d80's joined replay tail: the three motion-id arms all write the class/speed pair,
// the row echo and the transition into STEP_TURN_RUN, so one block serves all three.
void StepTurnRunReplay(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  // The joined replay tail: PES's three motion-id arms all write the class/speed pair, the
  // row echo and the transition into STEP_TURN_RUN, so one block serves all three.
  req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
  req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));
  req.SetF(moveword::kSpeedWord, 28.0f);  // 0x41e00000
  req.SetU(moveword::kClassWord, 3);
  req.SetU(moveword::kPrevKind, req.U(moveword::kKind));  // param_2[0x1e] = *param_2
  req.SetU(moveword::kRowAngle, req.U(moveword::kHeading));  // param_2[0x18] = param_2[1]
  req.SetU(moveword::kMoveBucket, req.U(moveword::kMoveBucket));  // param_2[0x20] = [0x8]
  req.SetByte(moveword::kReorientFlag, 0);                        // param_2 + 0x3c = 0
  AdjustAngleApply(host);
  RequestState(req, MoveState::kStepTurnRun);  // req[0x39] = 0xf
}

// ---------------------------------------------------------------------------------------------
// FUN_1406c14c0: RUN_TURN_RUN, the turning run.
//
// PES's order (each label below is PES's own, from the annotated decompile and the instruction
// stream):
//   the record walk and the turn lock, then the reorient window (0x1406c180f), the three draws
//   (0x1406c1957), the ladder over the live clip (0x1406c215c) and the damping tail
//   (0x1406c26ac). The register aliases: R12D = the clip frame, R15D = the row start,
//   EBX = the row end, EDI = the clamped row end, R13D = 2, XMM9 = the displacement speed,
//   XMM6/XMM7 = the turned heading, XMM11 = 90, XMM14 = 1, XMM15 = 22.5, XMM8 = 45,
//   [RSP+0x40] = the clamped row end, [RSP+0x44] = the row start, [RSP+0x4c] = the fixed frames,
//   [RSP+0x48] = the third sub frame, [RSP+0x50] = the row offset, [RSP+0x160] = the turn lock.
// ---------------------------------------------------------------------------------------------
void RunTurnRun(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int clipFrame = host.CurrentClipFrames();  // (P+0xab8 vt+0x38)()
  float clipLength = host.RecordF2468();           // P+0x2468
  if (clipLength < k.gFloor) clipLength = k.one;
  const float scale = k.one;
  const int motion = host.CurrentMotion();  // (P+0xab8 vt+0x18)()
  const int rowStart = RecordStartFrames(host, motion, scale, k);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale, k);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, req.Flag(moveword::kMirror), k);
  const int rowFixed = RecordFixedFrames(host, motion, scale, k);  // FUN_1406af620
  // FUN_1406af8e0's sub-record walk: the last band, then 0, then 1, then 2.
  const pes::MotionRecord& motionRec = host.Motion(motion);
  const int bands = motionRec.CancelCount();  // FUN_1406afbc0(..., 5)
  float lastFrames = static_cast<float>(rowFixed);
  float firstFrames = static_cast<float>(rowFixed);
  float secondFrames = static_cast<float>(rowFixed);
  float thirdFrames = static_cast<float>(rowFixed);
  if (0 < bands) {
    lastFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(bands - 1), scale, k));
    firstFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(0), scale, k));
    if (1 < bands) {
      secondFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(1), scale, k));
      if (2 < bands)
        thirdFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(2), scale, k));
    }
    if (static_cast<int>(firstFrames) < rowStart) firstFrames = static_cast<float>(rowStart);
  }
  // 0x1406c16ae: R15 = the row end clamped to the fixed frames.
  const int clampedEnd = rowFixed < rowEnd ? rowEnd : rowFixed;

  // The turn lock (0x1406c16c4): the two motion ids, the roster walk and the stamp window. The
  // helper that holds the lock is the dispatcher's own FUN_1406d01d0, so the port keeps its
  // inputs and its flag.
  bool turnLock = false;
  if (motion == 0xbc9 || motion == 0xc15) {
    turnLock = true;
    const int slot = AnimeSlot(host);
    const int stampAge = host.MatchTick() - RecordStampOf(host);
    bool rosterMatch = false;
    for (int i = 0; i < 24; ++i) {
      if (PlayerRowId(host, i) == slot) {
        rosterMatch = PlayerFlagD(host, i) != 0;
        break;
      }
    }
    if (!rosterMatch &&
        static_cast<int>(req.U(moveword::kCommandId)) == 5 &&
        stampAge >
            static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.speedC / k.fps +
                             k.half)) {
      turnLock = false;
    }
  }

  // The reorient window (0x1406c180f): while the facing interface is idle and the clip is inside
  // (start, (start + clampedEnd) / 2) the handler writes the turned heading.
  // 0x1406c1820: SAR EAX,1 on (rowStart + rowEnd), then CMP R12D,EAX; JG skips the window, so
  // the handler runs while the clip frame is at or below the mid frame.
  if (!ClipHalted(host) && rowStart <= clipFrame &&
      clipFrame <= (rowStart + clampedEnd) / 2) {
    const int span = static_cast<int>((clampedEnd - clipFrame) / clipLength + k.half);
    float turned = req.F(moveword::kTurnedRow);
    // FUN_14043a0a0(candidate, heading): heading - candidate, folded.
    const float loose = std::fabs(Wrap180Diff(turned, req.F(moveword::kHeading), k));
    if (loose < k.angle90) {
      turned = StepHeadingToPoint(turned, req.F(moveword::kHeading), k.angle675, k);
    }
    // FUN_14043a0a0(heading, candidate): candidate - heading, folded.
    const float tight = std::fabs(Wrap180Diff(turned, req.F(moveword::kHeading), k));
    if (tight / static_cast<float>(span) < k.angle45) {
      req.SetF(moveword::kSlipRowOut, turned);  // param_2 + 0xf4
      req.SetByte(moveword::kSlipNewRow, 1);    // param_2 + 0xf0
      req.SetU(moveword::kSlipRowFrames, static_cast<uint32_t>(span));  // param_2 + 0xf8
      req.SetByte(moveword::kSlipMirrorOut, 0);                          // param_2 + 0xfc
      req.SetF(moveword::kSlipCandidate, turned);                        // param_2 + 0x60
      if (k.angle90 < std::fabs(rowOffset)) {
        const float signOffset = rowOffset < 0.0f ? k.negOne : scale;
        const float signTight = tight < 0.0f ? k.negOne : scale;
        if (signOffset != signTight) {
          uint8_t m = static_cast<uint8_t>(req.U(moveword::kSlipMirrorOut) & 0xffu);
          if (k.angle225m < std::fabs(tight)) m = 1;
          req.SetByte(moveword::kSlipMirrorOut, m);
        }
      }
      // FUN_1406cbb00 (the yaw lock) is the dispatcher's own.
    }
  }

  // The three draws (0x1406c1957): the turn, the look-at and the heading error.
  const float turnAbs =
      std::fabs(Wrap180Diff(req.F(moveword::kTurn), req.F(moveword::kTurnedRow), k));
  const float lookAbs =
      std::fabs(Wrap180Diff(req.F(moveword::kLookHeading), req.F(moveword::kHeading), k));
  const float headAbs =
      std::fabs(Wrap180Diff(req.F(moveword::kTurn), req.F(moveword::kHeading), k));
  const float facingErr =
      std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kFacingError), k));
  const float dispSpeed = DispSpeedKmh(host, k);
  // 0x1406c19af: the bVar24 gate.
  bool replay = false;
  if (headAbs < k.angle90 || headAbs + k.angle225m < facingErr) {
    replay = true;
    if (!ClipRunning(host) && headAbs < k.angle225m) {
      replay = clipFrame >= static_cast<int>(secondFrames);
    }
  }

  // 0x1406c1a10: the live clip's gate.
  if (static_cast<int>(req.U(moveword::kCommandId)) == 0x11 ||
      (host.AnimeState() == 0x4a &&
       static_cast<int>(req.U(moveword::kCommandId)) == 0x72) ||
      turnLock) {
    goto LAB_0x1406c1f93;
  }
  if (ClipRunning(host) || ClipHalted(host)) goto LAB_0x1406c1f93;
  if (clipFrame <= static_cast<int>(firstFrames) + 1) goto LAB_0x1406c1ade;
  if (static_cast<int>(static_cast<float>(clipFrame) * k.loopBlend) >
      static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.turnTicks / k.fps + k.half)) {
    goto LAB_0x1406c1ae5;
  }
LAB_0x1406c1ade : {
  // 0x1406c1b22: the turned-heading test decides the replay tail.
  const float echo = std::fabs(Wrap180Diff(req.F(moveword::kTurn), req.F(moveword::kTurnedRow), k));
  if (k.angle5625 <= echo || replay ||
      (echo < k.angle1125b && k.angle5625 < turnAbs) || lookAbs < k.angle225m) {
    // 0x1406c1e9e: the clip-foot block: the foot flag, the class/speed pair and the rate ladder.
    const int foot = FootBandOf(host);
    req.SetByte(moveword::kFootFlag, (foot - 1U < 2U) ? 1 : 0);
    req.SetU(moveword::kClampWord, 0);  // param_2 + 0x6c
    req.SetU(0xb8, 0);                  // param_2 + 0xb8
    req.SetByte(0x140, 0);              // param_2 + 0x140
    if (dispSpeed < k.five) {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
    } else if (dispSpeed < k.speedB) {
      req.SetU(moveword::kClassWord, 1);
      req.SetF(moveword::kSpeedWord, k.speedA);
    } else {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, k.speedD);  // 0x41800000
    }
    // 0x1406c2c64..0x1406c2dfa: the rate ladder and its clamp.
    if (static_cast<int>(req.U(moveword::kClassWord)) > 1 &&
        static_cast<int>(req.U(moveword::kClassWord)) != 2) {
      req.SetF(moveword::kRate, InterfaceSpeed(host) * k.playbackFloor);
      float out[3] = {0.0f, 0.0f, 0.0f};
      host.PositionAt(static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.four /
                                       k.fps + k.half), out);
      const float sample = std::fabs(DispHeading(host, k));
      if (!(k.speedD <= sample)) return;
      if (!(2 < static_cast<int>(req.U(moveword::kCommandSpeedClass)))) return;
      if (static_cast<int>(req.U(moveword::kClassWord)) != 3) return;
      if (k.speed26 <= sample) return;
      if (k.speedE < sample) {
        float a = InterfaceSpeed(host);
        float b = InterfaceSpeed(host);
        req.SetF(moveword::kRate, (a + b + k.startHalf) / k.three);
      } else if (dispSpeed < dispSpeed) {
        float a = InterfaceSpeed(host);
        float b = InterfaceSpeed(host);
        float c = InterfaceSpeed(host);
        req.SetF(moveword::kRate, (a + b + c + k.startHalf + k.startHalf) / k.three);
      } else if (k.stampsA < sample) {
        float a = InterfaceSpeed(host);
        req.SetF(moveword::kRate, (a + k.startHalf) * k.half);
      } else {
        req.SetF(moveword::kRate, 0.6f);
      }
      const float rate = req.F(moveword::kRate);
      const float live = InterfaceSpeed(host);
      req.SetF(moveword::kRate, (rate <= live && 0.0f <= rate) ? rate : 0.0f);
      return;
    }
    if (static_cast<int>(req.U(moveword::kClassWord)) == 2) {
      req.SetF(moveword::kRate, 0.6f);
      return;
    }
    req.SetF(moveword::kRate, 0.3f);
    return;
  }
  goto LAB_0x1406c1f93;
}

LAB_0x1406c1ae5 : {
  // 0x1406c1ae5: the rotation tail. GF has no rotation helper, so the two rotated offsets stay
  // in the locals PES computes and the adapter owns the rotation.
  // ponytail: FUN_140581ce0 / FUN_140591a10's rotation removes these locals.
  float disp[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, disp);
  const float rotated = DispHeading(host, k);
  if (k.angle5625 <= rotated || replay ||
      (rotated < k.angle1125b && k.angle5625 < turnAbs) ||
      lookAbs < k.angle225m) {
    // 0x1406c1e9e is reached through the same block as the 0x1ade join above.
    const int foot = FootBandOf(host);
    req.SetByte(moveword::kFootFlag, (foot - 1U < 2U) ? 1 : 0);
    req.SetU(moveword::kClampWord, 0);
    req.SetU(0xb8, 0);
    req.SetByte(0x140, 0);
    if (dispSpeed < k.five) {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
    } else if (dispSpeed < k.speedB) {
      req.SetU(moveword::kClassWord, 1);
      req.SetF(moveword::kSpeedWord, k.speedA);
    } else {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, k.speedD);  // 0x41800000
    }
    return;
  }
  goto LAB_0x1406c1f93;
}

LAB_0x1406c1f93 : {
  // The ladder over the live clip (0x1406c215c) and the damping tail (0x1406c26ac).
  const int liveClass = static_cast<int>(req.U(moveword::kClassWord));
  const bool alive = liveClass >= 0;
  // 0x1406c2005: the 0xbea clip's stop ladder.
  if (motion == 0xbea && clipFrame < static_cast<int>(firstFrames) && alive) {
    float turned = req.F(moveword::kTurnedRow);
    const float echo = std::fabs(Wrap180Diff(turned, req.F(moveword::kHeading), k));
    if (k.angle45 < echo) {
      turned = StepHeadingToPoint(turned, req.F(moveword::kHeading), k.angle675, k);
    }
    req.SetU(moveword::kPrevBucket, req.U(moveword::kPrevBucket));
    req.SetU(moveword::kPrevBucket,
             static_cast<uint32_t>(InterfaceBucket(host, turned)));
    if (dispSpeed < k.speedA) {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
      EnterStop(host, req);  // FUN_1406d5d60
      return;
    }
    if (dispSpeed < k.speed1475) {
      req.SetF(moveword::kSpeedWord, k.speedA);
      req.SetU(moveword::kClassWord, 1);
      EnterStop(host, req);  // FUN_1406d5d60
      return;
    }
    if (dispSpeed < k.speed19) {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, k.speedD);  // 0x41800000
    }
    EnterStop(host, req);  // FUN_1406d5d60
    return;
  }
  // 0x1406c2103: the replay / idle tail and the ladder.
  if ((host.AnimeState() == 0x4a || static_cast<int>(req.U(moveword::kCommandId)) == 0x72 ||
       turnLock || alive) &&
      ((ClipRunning(host) || ClipHalted(host)) ? clipFrame < static_cast<int>(firstFrames)
                                               : clipFrame < static_cast<int>(firstFrames))) {
    if (clipFrame < static_cast<int>(firstFrames)) return;
  }
  goto LAB_0x1406c215c;
}

LAB_0x1406c215c : {
  // 0x1406c215c: the draw PES keeps in XMM7 = FUN_14043a0a0(req[2], req[1]).
  const int liveClass = static_cast<int>(req.U(moveword::kClassWord));
  const float drawTurn = Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k);
  // 0x1406c2175: the bVar3 arm's inputs: the saved class word and the kind's 3/6 pair.
  const int savedClass = static_cast<int>(req.U(moveword::kPrevClassWord));
  const int liveKind = static_cast<int>(req.U(moveword::kKind));
  bool kindPair = false;
  if (savedClass < 1 && (liveKind == 3 || liveKind == 6)) kindPair = true;
  // 0x1406c21a7: FUN_140a6e290 clears the flag when the clip runs.
  bool flagB = kindPair && !ClipRunning(host);
  // 0x1406c21bf: the turn tolerance, 45 degrees unless the 0xc1c clip lowers it to 22.5.
  float turnTol = k.angle45;
  if (motion == 0xc1c && clipFrame < clampedEnd) turnTol = k.angle225m;
  // 0x1406c21ef: the second arm of the same flag.
  if (clampedEnd <= clipFrame || turnTol < std::fabs(drawTurn)) flagB = true;
  // 0x1406c21fc: the class-unset arm of the stop ladder.
  if (liveClass < 0 && !turnLock) {
    if (flagB && 1 <= liveClass) {
      // The 0x1406c2226 ladder over the displacement speed.
      req.SetU(moveword::kClassWord, 3);
      req.SetF(moveword::kSpeedWord, 28.0f);  // 0x41e00000
      if (dispSpeed < k.speedA) {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kSpeedWord, k.five);
        EnterTurnStop(host, req);  // FUN_1406d2590
        return;
      }
      if (dispSpeed < k.speed1475) {
        req.SetU(moveword::kClassWord, 1);
        req.SetF(moveword::kSpeedWord, k.speedA);
        EnterTurnStop(host, req);  // FUN_1406d2590
        return;
      }
      if (dispSpeed < k.speed19) {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kSpeedWord, k.speedD);
      }
      EnterTurnStop(host, req);  // FUN_1406d2590
      return;
    }
    goto LAB_0x1406c240d;
  }

  // 0x1406c22b4: the tolerance shaping and the row-echo selection.
  {
    // XMM6 starts at 45 and the 0xbe8 clip raises it to 67.5.
    float echoTol = k.angle45;
    if (motion == 0xbe8 && k.angle45 < std::fabs(req.F(moveword::kFacingError))) {
      echoTol = k.angle675;
    }
    // 0x1406c22f2: the distance term PES folds into the tolerance.
    const float dist = req.F(moveword::kDistToTarget);
    float distTerm = 0.0f;
    if (k.two < dist) {
      distTerm = (dist < k.four) ? (dist - k.two) * k.half : k.one;
    }
    echoTol += distTerm * k.angle225m;
    // 0x1406c2352: the 0xc1a clip flattens the tolerance back to 45.
    if (motion == 0xc1a && clipFrame < static_cast<int>(thirdFrames)) echoTol = k.angle45;
    // 0x1406c235c: the drawn tolerance is stored in [RSP+0x178] and compared with XMM13
    // (the third draw) and with the live request's precise flag.
    const float thirdDraw =
        std::fabs(Wrap180Diff(req.F(moveword::kTurn), req.F(moveword::kTurnedRow), k));
    if (thirdDraw < echoTol && !ClipRunning(host) && k.angle45 <= thirdDraw &&
        req.U(moveword::kPrecise) == 1 && clampedEnd <= clipFrame &&
        clipFrame < static_cast<int>(thirdFrames)) {
      goto LAB_0x1406c2595;  // the same join the class-unset arm takes
    }
  }
  // 0x1406c2397: the turnLock arm, then the flagB arm, then the ladder.
  if (!turnLock) {
    // 0x1406c23a7: the kind-pair flag hands the tick to the replay block.
    if (kindPair) goto LAB_0x1406c240d;
    if (!flagB) {
      // 0x1406c23b4: the class/speed pair from the displacement speed, then STEP_TURN_RUN.
      if (dispSpeed < k.five) {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kSpeedWord, k.five);
      } else if (dispSpeed < k.speedB) {
        req.SetU(moveword::kClassWord, 1);
        req.SetF(moveword::kSpeedWord, k.speedA);
      } else {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kSpeedWord, k.speedD);
      }
      if (!EnterStepTurnRun(host, req, k)) goto LAB_0x1406c240d;  // FUN_1406d38d0
      return;
    }
    // 0x1406c2446: the 0xc44 clip doubles the drawn speed's floor.
    float speedForLadder = dispSpeed;
    if (motion == 0xc44 && k.speedA <= speedForLadder &&
        clipFrame < static_cast<int>(firstFrames)) {
      speedForLadder = k.speedA;
    }
    // 0x1406c2475: the precise/distance arm decides between the run-turn and the re-plan.
    if (req.U(moveword::kPrecise) == 1) {
      const float dist = req.F(moveword::kDistToTarget);
      const bool nearBand = k.three < dist && dist <= k.three;
      if (k.angle1125 < std::fabs(req.F(moveword::kTurn)) && k.one <= dist && nearBand) {
        // 0x1406c24c6: the run-turn ladder over the drawn speed.
        const float live = InterfaceSpeed(host);
        req.SetF(moveword::kRate, 0.6f);  // 0x3f19999a
        const float threshold = (live - k.startHalf) * k.half;
        if (!(k.five <= speedForLadder)) {
          req.SetF(moveword::kRate, 0.15f);  // 0x3e19999a
        } else if (k.speedB <= speedForLadder) {
          req.SetF(moveword::kRate, 0.3f);  // 0x3e99999a
        }
        if (EnterRunTurnStep(host, req, k, threshold)) return;  // FUN_1406d2f60
      }
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
      EnterGotoRun(host, req, false);  // FUN_1406ce820
      return;
    }
    // 0x1406c2544: the foot flag, the class/speed pair and the loop-branch replay.
    const int foot = FootBandOf(host);
    req.SetByte(moveword::kFootFlag, (foot - 1U < 2u) ? 1 : 0);
    req.SetU(moveword::kClampWord, 0);
    req.SetU(0xb8, 0);
    req.SetByte(0x140, 0);
    if (dispSpeed < k.five) {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
    } else if (dispSpeed < k.speedB) {
      req.SetU(moveword::kClassWord, 1);
      req.SetF(moveword::kSpeedWord, k.speedA);
    } else {
      req.SetU(moveword::kClassWord, 2);
      req.SetF(moveword::kSpeedWord, k.speedD);
    }
  }
LAB_0x1406c2595 : {
  // 0x1406c2595: the re-plan gate over the class word, the saved class and the kind pair.
  // PES's condition to reach the re-plan is the negation of the compound test below, so the
  // port keeps the same two-branch shape.
  const int classWord = static_cast<int>(req.U(moveword::kClassWord));
  const int savedClassWord = static_cast<int>(req.U(moveword::kPrevClassWord));
  const bool reachesRowEcho = classWord >= 1 && savedClassWord >= 1 &&
                              (savedClassWord <= 1 ||
                               ((liveKind == 3 || liveKind == 6) && 1 < liveClass)) &&
                              !turnLock;
  if (!reachesRowEcho) {
    req.SetU(moveword::kClassWord, 0);
    req.SetF(moveword::kSpeedWord, k.five);
    EnterGotoRun(host, req, false);  // FUN_1406ce820
    return;
  }
  int echoSpan = (rowStart + clampedEnd) / 2;
  uint32_t rowSpan = static_cast<uint32_t>(echoSpan);
  if (motion == 0xc42) {
    rowSpan = static_cast<uint32_t>(lastFrames);
  } else if (motion == 0xc44) {
    rowSpan = static_cast<uint32_t>(thirdFrames);
  } else if (motion != 0xbc7 && classWord < liveClass &&
             (echoSpan <= clipFrame || static_cast<int>(rowSpan) <= clipFrame)) {
    // 0x1406c260e: the sign pair decides between the full span and its 75% share.
    const bool signOffset = 0.0f < rowOffset;
    const bool signDraw =
        0.0f < Wrap180Diff(req.F(moveword::kTurn), req.F(moveword::kFacing), k);
    rowSpan = signOffset == signDraw ? static_cast<uint32_t>(echoSpan)
                                     : static_cast<uint32_t>(Share75(echoSpan));
  }
  (void)rowSpan;
  // 0x1406c2663: the frame tests over the drawn tolerance and the row echo.
  const float echoDraw =
      std::fabs(Wrap180Diff(req.F(moveword::kTurn), req.F(moveword::kTurnedRow), k));
  if (clampedEnd <= clipFrame || echoDraw < k.angle1125b ||
      (clipFrame < static_cast<int>(firstFrames) && echoDraw >= k.angle1125b)) {
    goto LAB_0x1406c26ac;  // the damping tail below
  }
  // 0x1406c28ce: the class-word arm that jumps back into the ladder.
  if (!turnLock && classWord < 3 && savedClassWord == 3 && motion == 0xbc7 &&
      k.angle5625 <= echoDraw && clipFrame < static_cast<int>(firstFrames)) {
    return;  // 0x1406c2921: the loop-branch replay
  }
  // 0x1406c2929: the tolerance's own class/speed ladder.
  const float tolDraw =
      std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k));
  int echoSpanLive = clampedEnd / 2;
  if (motion == 0xc42) echoSpanLive = static_cast<int>(lastFrames);
  if (motion == 0xc44) echoSpanLive = static_cast<int>(thirdFrames);
  if (echoSpanLive < 0) echoSpanLive = -echoSpanLive;
  if (echoSpanLive <= tolDraw) {
    goto LAB_0x1406c2b36;
  }
  {
    // 0x1406c2ad4: the class ladder from the drawn tolerance.
    if (classWord < 3 && savedClassWord == 3 && echoSpanLive <= clipFrame) {
      float band = k.five;
      if (1 < classWord) band = k.speed39375;
      bool motionOk = true;
      if (motion == 0xbc7 && 1 < savedClassWord &&
          static_cast<int>(lastFrames) <= clipFrame) {
        motionOk = false;
      } else if (1 < classWord && ClipRunning(host) && ClipHalted(host)) {
        motionOk = false;
      } else if (motion == 0xc0b) {
        band = k.angle45;
      }
      if (band <= tolDraw && tolDraw < k.speed39375 &&
          static_cast<int>(req.U(moveword::kPrevClassWord)) == 3 && motion != 0xbc5) {
        motionOk = motion == 0xc1a && static_cast<int>(lastFrames) <= clipFrame;
      }
      if (motionOk) {
        if (k.five <= dispSpeed) {
          if (k.speedB <= dispSpeed) {
            req.SetU(moveword::kClassWord, 2);
            req.SetF(moveword::kSpeedWord, k.speedD);
          } else {
            req.SetU(moveword::kClassWord, 1);
            req.SetF(moveword::kSpeedWord, k.speedA);
          }
        } else {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
        }
        if (!EnterStepTurnRun(host, req, k)) goto LAB_0x1406c2b36;  // FUN_1406d38d0
        return;
      }
    }
    goto LAB_0x1406c2b36;
  }
}

LAB_0x1406c26ac : {
  // 0x1406c26ac: the facing damping. The two folds, the >180 wrap, the limit band and the
  // sign flip of the maxss pair are all PES's own order.
  req.SetByte(moveword::kSlipNewRow, 0);  // param_2 + 0xf0 = 0
  AdjustAngleApply(host);                 // (+0x4d0 vt+0x08)(P+0x4d0, P+0xa08)
  float facing = FoldUnit(req.F(moveword::kFacing), k);
  float heading = FoldUnit(req.F(moveword::kHeading), k);
  float delta = heading - facing;
  if (k.halfTurn < std::fabs(delta)) {
    delta -= (0.0f <= delta ? k.one : k.negOne) * k.fullTurn;
  }
  if (std::fabs(delta) < k.angle45) {
    // 0x1406c2764: the limit band from the clip predicates and the class pair.
    const int savedClass = static_cast<int>(req.U(moveword::kPrevClassWord));
    float limit = k.halfTurn;
    if ((!ClipRunning(host) && ClipHalted(host)) && clipFrame >= clampedEnd) {
      limit = k.angle225m;
    } else if (static_cast<int>(req.U(moveword::kClassWord)) >= 3 && savedClass >= 3 &&
               k.speed19 <= dispSpeed && liveClass == 3 && savedClass == 3) {
      limit = k.angle135;
      if (clampedEnd <= clipFrame) {
        if (k.speed135 <= dispSpeed) limit = k.angle225;
      }
    }
    // 0x1406c27f0: the sign pair quarters the limit.
    if (clipFrame < static_cast<int>(secondFrames)) {
      const float signOffset = 0.0f < rowOffset;
      const float signDraw =
          0.0f < Wrap180Diff(req.F(moveword::kTurn), req.F(moveword::kFacing), k);
      if (signOffset == signDraw) limit *= k.quarter;
    }
    const float ticks = static_cast<float>(host.TicksPerSecond());
    float step = limit / ticks;
    if (delta <= step) step = std::max(WrapHalf(step, k), delta);
    float damped = facing + step;
    if (0.0f <= damped) {
      if (k.fullTurn <= damped) damped += k.negFullTurn;
    } else {
      damped += k.fullTurn;
    }
    req.SetByte(moveword::kSlipActive, 1);   // param_2 + 0xe8 = 1
    req.SetF(moveword::kSlipAngle, damped);  // param_2 + 0xec
    if (clampedEnd <= clipFrame) {
      req.SetF(moveword::kSlipCandidate, damped);  // param_2 + 0x60
    }
    host.SetRecordF0bec(damped);  // P+0xbec
  }
  return;
}

LAB_0x1406c240d : {
  // 0x1406c240d: the replay block - the bucket save, then the STOP entry.
  req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
  req.SetU(moveword::kMoveBucket,
           static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kHeading))));
  EnterStop(host, req);  // FUN_1406d5d60
  return;
}

LAB_0x1406c2b36 : {
  // 0x1406c2b4e: the final class/speed ladder and the rate tail.
  const int foot = FootBandOf(host);
  req.SetByte(moveword::kFootFlag, (foot - 1U < 2u) ? 1 : 0);
  req.SetU(moveword::kClampWord, 0);  // param_2 + 0x6c
  req.SetU(0xb8, 0);                   // param_2 + 0xb8
  req.SetByte(0x140, 0);               // param_2 + 0x140
  const float band22 = k.speed22;
  const float playbackFloor = k.playbackFloor;
  // XMM6 starts at 22 (the >1 class band) and XMM7 at 0.95.
  if (dispSpeed < k.speedA) {
    req.SetF(moveword::kRate, 0.15f);  // 0x3e19999a
  } else if (dispSpeed < k.speed135) {
    if (liveClass >= 2 && static_cast<int>(secondFrames) <= clipFrame) {
      req.SetF(moveword::kRate, 0.3f);  // 0x3e99999a
    } else {
      req.SetF(moveword::kRate, 0.3f);
    }
  } else if (band22 < dispSpeed) {
    req.SetF(moveword::kRate, InterfaceSpeed(host) * playbackFloor);
  } else {
    req.SetF(moveword::kRate, 0.6f);  // 0x3f19999a
  }
  req.SetU(moveword::kClassWord, static_cast<uint32_t>(InterfaceClass(host)));  // vt+0x68
  host.PhysiqueG();  // FUN_141af3180(P+0xa08)
  req.SetF(moveword::kSpeedWord, InterfaceSpeed(host) * k.kmhToRate);  // vt+0x48
  // 0x1406c2c4c: the loop-branch replay through +0xac0's own slot.
  req.SetF(moveword::kSpeedWord, req.F(moveword::kSpeedWord));
  // 0x1406c2c64: the rate ladder selected by the class word.
  const int finalClass = static_cast<int>(req.U(moveword::kClassWord));
  if (finalClass > 1 && finalClass != 2) {
    // 0x1406c2c79: the sampled-speed tail.
    float sample[3] = {0.0f, 0.0f, 0.0f};
    host.PositionAt(static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.four /
                                     k.fps + k.half), sample);
    const float sampleKmh = DispSpeedKmh(host, k);
    if (!(k.speedD <= sampleKmh)) return;
    if (!(2 < static_cast<int>(req.U(moveword::kCommandSpeedClass)))) return;
    if (finalClass != 3) return;
    if (k.speed26 <= sampleKmh) return;
    float rate;
    if (k.speedE < sampleKmh) {
      const float a = InterfaceSpeed(host);
      const float b = InterfaceSpeed(host);
      rate = (a + b + k.startHalf) / k.three;
    } else if (band22 < sampleKmh && sampleKmh < sampleKmh) {
      const float a = InterfaceSpeed(host);
      const float b = InterfaceSpeed(host);
      const float c = InterfaceSpeed(host);
      rate = (a + b + c + k.startHalf + k.startHalf) / k.three;
    } else if (k.stampsA < sampleKmh) {
      const float a = InterfaceSpeed(host);
      rate = (a + k.startHalf) * k.half;
    } else {
      rate = 0.6f;
    }
    const float live = InterfaceSpeed(host);
    req.SetF(moveword::kRate, (rate <= live && 0.0f <= rate) ? rate : 0.0f);
    return;
  }
  if (finalClass == 2) {
    req.SetF(moveword::kRate, 0.6f);
    return;
  }
  req.SetF(moveword::kRate, 0.3f);
}
}
}  // LAB_0x1406c215c

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
  const int rowStart = RecordStartFrames(host, motion, scale, k);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale, k);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, mirror, k);  // FUN_1406b1c70
  const float quarter = k.angle90;  // fVar14 = DAT_14259be5c (90)
  const float negOne = k.negOne;    // fVar16 = DAT_142588218

  // 1. The re-select window (0x1406c30a7..0x1406c31f4). The two FUN_14043a0a0 calls' arguments
  // come from the instruction stream: 0x1406c2fd0 passes (candidate, req[1]) and 0x1406c3130
  // passes (req[1], candidate), i.e. the heading minus the candidate both times.
  if (!ClipHalted(host) && rowStart <= clipFrame && clipFrame < (rowStart + rowEnd * 2) / 3) {
    float candidate = req.F(moveword::kSlipCandidate);  // param_2 + 0x60
    const int frames = static_cast<int>((rowEnd - clipFrame) / clipLength + k.half);
    const float loose = std::fabs(Wrap180Diff(candidate, req.F(moveword::kHeading), k));
    if (loose < quarter) {
      candidate = StepHeadingToPoint(candidate, req.F(moveword::kHeading), k.angle675, k);
    }
    const float tight = std::fabs(Wrap180Diff(candidate, req.F(moveword::kHeading), k));
    if (tight / static_cast<float>(frames) < k.angle45) {  // DAT_14259be30
      req.SetF(moveword::kSlipRowOut, candidate);  // param_2 + 0xf4
      req.SetByte(moveword::kSlipNewRow, 1);       // param_2 + 0xf0
      req.SetU(moveword::kSlipRowFrames, static_cast<uint32_t>(frames));  // param_2 + 0xf8
      req.SetByte(moveword::kSlipMirrorOut, 0);                            // param_2 + 0xfc
      req.SetF(moveword::kSlipCandidate, candidate);  // param_2 + 0x60
      if (quarter < std::fabs(rowOffset)) {
        const float signOffset = rowOffset < 0.0f ? negOne : scale;
        const float signTight = tight < 0.0f ? negOne : scale;
        if (signOffset != signTight) {
          uint8_t m = static_cast<uint8_t>(req.U(moveword::kSlipMirrorOut) & 0xffu);
          if (k.angle225m < std::fabs(tight)) m = 1;  // DAT_14259d618
          req.SetByte(moveword::kSlipMirrorOut, m);
        }
      }
      // FUN_1406cbb00 (the yaw lock) is the dispatcher's own, so the handler stops here.
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
      // 0x1406c32e2: COMISS XMM7,XMM8; JA skip. Otherwise the -0.0 sign flip and the maxss pair.
      if (delta <= step) {
        step = std::max(WrapHalf(step, k), delta);
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
  int rowFrames = RecordFixedFrames(host, motion, scale, k) - 1;  // FUN_1406af620
  const pes::MotionRecord& motionRec = host.Motion(motion);
  const int bands = motionRec.CancelCount();  // FUN_1406afbc0(..., 5)
  if (0 < bands) {
    rowFrames = FrameTicks(host, motionRec.CancelFrames(bands - 1), scale, k);  // FUN_1406af8e0
  }
  if (rowFrames <= clipFrame) {
    const int foot = FootBandOf(host);  // (P+0xab8 vt+0x78)()
    req.SetByte(moveword::kFootFlag, (foot - 1U < 2U) ? 1 : 0);  // param_2 + 0x98
    req.SetU(moveword::kClampWord, 0);  // param_2 + 0x6c
    req.SetU(0xb8, 0);                   // param_2 + 0xb8
    req.SetByte(0x140, 0);               // param_2 + 0x140
    // (P+0xac0 vt+0x00)(P+0xac0, req, foot, 0): the row replay through +0xac0's first slot; the
    // adapter owns that slot, so the foot band travels on the host (MoveHost::FootBand), not the request.
    (void)foot;
    req.SetF(moveword::kSlipRate, 0.3f);    // param_2 + 0x128 = 0x3e99999a
    req.SetF(moveword::kSlipSpeed, 11.0f);  // param_2 + 0x74 = 0x41300000
  }
}


// ---------------------------------------------------------------------------------------------
// FUN_1406c3470: RUN_TURN_STEP, the run-to-step turn.
//
// PES's order:
//   1. the motion record sets the reorient window, the dwell and the span;
//   2. the rate clamp (req[0x4a]) against the table rate or the interface's own;
//   3. the reorient window writes the turned row (req[0x3c]..[0x3f]);
//   4. the loop-branch replay and the tolerance shaping;
//   5. the ladder: the class-3 gate, the run-turn entry, the accelerate entry, the run entries,
//      or the step-family re-select with the row grid;
//   6. the facing write (req[0x3a]/[0x3b]) and the exit gate.
// ---------------------------------------------------------------------------------------------
void RunTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int motion = host.CurrentMotion();  // (P+0xab8 vt+0x18)()
  const int clipFrame = host.CurrentClipFrames();  // (P+0xab8 vt+0x38)()
  float clipLength = host.RecordF2468();           // P+0x2468
  if (clipLength < k.gFloor) clipLength = k.one;
  const float scale = k.one;
  const int rowStart = RecordStartFrames(host, motion, scale, k);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale, k);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, req.Flag(moveword::kMirror), k);
  const int rowFixed = RecordFixedFrames(host, motion, scale, k);  // FUN_1406af620
  // FUN_1406af620's sub-record walk (0x1406c3640): the last band, then 0, then 1, then 2.
  const pes::MotionRecord& motionRec = host.Motion(motion);
  const int bands = motionRec.CancelCount();  // FUN_1406afbc0(..., 5)
  float spanF = static_cast<float>(rowFixed);
  float rowFixedB = static_cast<float>(rowFixed);
  float lastSpan = static_cast<float>(rowFixed);
  float dispBack = DispSpeedKmh(host, k);
  if (0 < bands) {
    lastSpan = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(bands - 1), scale, k));
    spanF = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(0), scale, k));
    if (1 < bands) {
      rowFixedB = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(1), scale, k));
      if (2 < bands) FrameTicks(host, motionRec.CancelFrames(2), scale, k);
    }
    if (static_cast<int>(spanF) < static_cast<int>(lastSpan)) spanF = lastSpan;
  }
  // The dwell: the clock's 10-degree window when the row has no offset, else the row start.
  int dwell;
  if (rowOffset == 0.0f) {
    dwell = static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.ten / k.fps + k.half);
  } else {
    dwell = rowStart;
  }
  float spanG = spanF;
  if (static_cast<int>(rowFixed) < rowEnd) spanG = static_cast<float>(rowEnd);
  // 0x1406c36c0: the two motion ids that swap the span for the second sub-record's.
  const int curMotion = host.CurrentMotion();
  if (curMotion == 0xb39) spanG = rowFixedB;

  // 2. The rate clamp (0x1406c3710..0x1406c37c0). The table rate is FUN_1406b9e80's answer when
  // req[0xb] is clear and the interface's own rate otherwise.
  float tableRate;
  if (!req.Flag(moveword::kUseClassRate)) {
    if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == 0 &&
        (static_cast<int>(req.U(moveword::kKind)) == 3 ||
         static_cast<int>(req.U(moveword::kKind)) == 6)) {
      tableRate = 0.0f;
    } else {
      tableRate = movestep::SpeedToRate(host, host.CurrentSpeedKmh(), 0);
    }
  } else {
    tableRate = req.F(moveword::kRate);
  }
  {
    const float rate = req.F(moveword::kRate);
    if (rate <= tableRate) {
      const float grown = rate + k.zero + k.turnRamp;  // DAT_1425a8a28's step
      if (grown <= tableRate) tableRate = 0.0f <= grown ? grown : 0.0f;
      req.SetF(moveword::kRate, tableRate);
    } else {
      const float live = InterfaceSpeed(host);  // (+0xac0 vt+0x18)()
      const float shrunk = rate - k.turnRamp;
      float out = live;
      if (shrunk <= live) {
        if (tableRate <= shrunk) tableRate = shrunk;
        out = tableRate;
      }
      req.SetF(moveword::kRate, out);
    }
  }

  const bool facingIdle = !ClipHalted(host);  // FUN_141eb3320(P+0x4d0) == 0
  const float narrow = k.angle225m;           // fVar28 = DAT_14259d618
  const float quarter = k.angle90;            // fVar32 = DAT_14259be5c
  const float full = k.fullTurn;              // fVar8 = DAT_14259858c
  float turnTol = k.angle45;                  // fVar22 = DAT_14259be30

  // 3. The reorient window (0x1406c381f..0x1406c39cd).
  if (facingIdle && dwell <= clipFrame) {
    int grown = dwell + static_cast<int>(spanG) * 2;
    // PES keeps the division-heavy third-of-clip test exactly as written.
    const int third = grown / 3 + (grown >> 31) +
                      static_cast<int>((static_cast<int64_t>(grown / 3) +
                                       ((static_cast<int64_t>(grown) >> 63) & 0xffffffffu)) >> 31);
    grown = third < clipFrame ? grown : grown;
    if (clipFrame < third) {
      const int framesLeft =
          static_cast<int>(static_cast<float>(static_cast<int>(spanG) - clipFrame) / clipLength +
                           k.half);
      float rowAngle = req.F(moveword::kRowAngle);  // param_2[0x17]
      // 0x1406c3885: FUN_14043a0a0(req[1], req[0x17]), i.e. the row angle minus the heading.
      const float loose = std::fabs(Wrap180Diff(req.F(moveword::kHeading), rowAngle, k));
      if (loose < quarter) {
        rowAngle = StepHeadingToPoint(rowAngle, req.F(moveword::kHeading), k.angle675, k);
      }
      float turned =
          rowAngle + k.bucketAngle[static_cast<int>(req.U(moveword::kMoveBucket)) & 9];
      turned = FoldUnit(turned, k);
      // 0x1406c3903: FUN_14043a0a0(req[1], rowAngle), i.e. the row angle minus the heading.
      const float tight = std::fabs(Wrap180Diff(req.F(moveword::kHeading), rowAngle, k));
      if (tight / static_cast<float>(framesLeft) < k.angle45) {
        const int echoRow = static_cast<int>(req.U(moveword::kMoveBucket));
        req.SetByte(moveword::kReorientFlag, 1);  // param_2 + 0x3c = 1
        req.SetU(moveword::kTurnFrames, static_cast<uint32_t>(framesLeft));  // param_2[0x3e]
        req.SetByte(moveword::kMirrorFlag, 0);                                 // param_2 + 0x3f
        req.SetF(moveword::kAngle3d, rowAngle);                                // param_2[0x3d]
        req.SetF(moveword::kTurnedRow, turned);  // param_2[0x18]
        if (quarter < std::fabs(rowOffset)) {
          const float signRow = scale;
          const float signRe = tight < 0.0f ? k.negOne : scale;
          if (signRow != signRe) {
            uint8_t m = static_cast<uint8_t>(req.U(moveword::kMirrorFlag) & 0xffu);
            if (narrow < std::fabs(tight)) m = 1;
            req.SetByte(moveword::kMirrorFlag, m);
          }
        }
        (void)echoRow;
        (void)full;
      }
    }
  }

  // 4. The loop-branch replay (0x1406c39d8..0x1406c3b3d).
  {
    const float ticks = static_cast<float>(host.TicksPerSecond());  // FUN_141e532d0
    if (clipFrame <
            static_cast<int>(ticks * k.turnTicks / k.fps + k.half) &&
        std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k)) <
            narrow &&
        static_cast<int>(req.U(moveword::kCommandId)) != 0x11 &&
        req.U(moveword::kPrecise) == 0 &&
        2 < static_cast<int>(req.U(moveword::kCommandSpeedClass))) {
      const int foot = FootBandOf(host);
      float rateWord = k.startHalf;  // 0.6
      req.SetF(moveword::kRate, rateWord);
      req.SetByte(moveword::kFootFlag, (foot - 1U < 2U) ? 1 : 0);
      req.SetU(moveword::kDwellLimit, 0);  // param_2[0x1b]
      req.SetU(moveword::kDwellLimit2, 0);  // param_2[0x2e]
      req.SetByte(moveword::kStyle, 0);  // param_2 + 0x50
      if (k.speedA <= dispBack) {
        if (k.speed135 <= dispBack) {
          if (k.speed22 < dispBack) {
            req.SetF(moveword::kRate, InterfaceSpeed(host) * k.playbackFloor);
          }
        } else {
          req.SetF(moveword::kRate, k.blend03);  // 0.3
        }
      } else {
        req.SetF(moveword::kRate, k.turnRamp);  // 0.15
      }
      req.SetU(moveword::kClassWord, static_cast<uint32_t>(InterfaceClass(host)));
      host.PhysiqueG();  // FUN_141af3180(P+0xa08)
      req.SetU(moveword::kSpeedWord,
               static_cast<uint32_t>(InterfaceSpeed(host) * k.kmhToRate));
      req.SetU(moveword::kPrevKind, req.U(moveword::kKind));  // param_2[0x1e] = *param_2
      return;
    }
  }

  // The tolerance shaping (0x1406c3bc0..0x1406c3c20).
  // PES at 0x1406C3B4C: FUN_14043a0a0(req[0x18], req[0x01]) = wrap180(kHeading - kRecordStampB).
  // The magnitude is the decompile's local_res20, which the ladder compares against the
  // tolerance and the 45-deg gate.
  float faceAbs = std::fabs(Wrap180Diff(req.F(moveword::kRecordStampB), req.F(moveword::kHeading), k));
  {
    const float dist = req.F(moveword::kDistToTarget);  // param_2[0x11]
    if (req.U(moveword::kPrecise) == 0 && 0 < static_cast<int>(req.U(moveword::kClassWord))) {
      if (k.two < dist) {
        if (dist < k.four) {
          turnTol = (dist - k.two) * k.half * narrow + turnTol;
        } else {
          turnTol = scale * narrow + turnTol;
        }
      } else {
        turnTol = narrow * 0.0f + turnTol;
      }
    } else if (req.U(moveword::kPrecise) == 1 && k.angle1125 < std::fabs(req.F(moveword::kFacingError)) &&
               clipFrame < static_cast<int>(spanF)) {
      // PES at 0x1406C3BD2 reads req[0x1c] (word 7, the facing error) directly, not a
      // difference of two headings.
      turnTol = k.angle675;
    }
  }
  // PES at 0x1406C3C02: FUN_14043a0a0(req[0x02], req[0x04]) = wrap180(kBodyDirection -
  // kFacing). The magnitude drives the `near` gate (40 deg for motion 0xbd6, 45 deg otherwise).
  const float bodyFacingDelta =
      std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k));
  const bool near = curMotion == 0xbd6 ? bodyFacingDelta < k.angle40 : bodyFacingDelta < k.angle45;
  const bool scripted = host.AnimeState() == 0x4a;  // (+0xab8 vt+0x28)() == 0x4a
  float stopBand = k.speed1475;  // fVar24 = DAT_1425ddf88
  // The class ladder's own comparison (0x1406C3F54): the signed difference of the body
  // direction and the facing, not the magnitude.
  const float facingDelta =
      Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k);

  // 5. The ladder. The scripted tail and the class ladder both start from here.
  if (scripted) {
    return;  // the +0xab8 vt+0x28 replay state stops the handler (PES's own join)
  }
  const int liveClass = static_cast<int>(req.U(moveword::kClassWord));
  // 0x1406c3c51: the unset class path stops the player.
  if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1) {
    if (near && k.speedA <= dispBack) {
      req.SetU(moveword::kClassWord, 3);
      req.SetF(moveword::kSpeedWord, k.speed28);
      if (dispBack < stopBand) {
        req.SetF(moveword::kSpeedWord, k.speedA);
        req.SetU(moveword::kClassWord, 1);
        EnterStop(host, req);  // FUN_1406d2590
        return;
      }
      if (dispBack < k.speed19) {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kSpeedWord, k.speedD);
      }
      EnterStop(host, req);  // FUN_1406d2590
      return;
    }
    req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));  // param_2[0x25] = [0x20]
    req.SetU(moveword::kMoveBucket,
             static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kHeading))));
    EnterStop(host, req);  // FUN_1406d5d60
    return;
  }
  if (turnTol < faceAbs ||
      (req.U(moveword::kPrecise) == 0 &&
       static_cast<int>(rowFixed) <= clipFrame)) {
    // 0x1406c3cf0: the drawn speed and the step-motion ladder.
    const float ticks = static_cast<float>(host.TicksPerSecond());
    if (clipFrame <
            static_cast<int>(ticks * k.turnTicks / k.fps + k.half) ||
        static_cast<int>(rowFixed) <= clipFrame) {
      if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) != -1 && 0 <= liveClass) {
        float out[3] = {0.0f, 0.0f, 0.0f};
        host.PositionAt(1, out);
        const float speed = DispSpeedKmh(host, k);
        float top = k.angle135;
        float band = k.five;
        if (curMotion == 0xb4c) {
          if (static_cast<int>(rowFixed) <= clipFrame && speed < k.speed1475) {
            // PES at 0x1406C3E82 compares req[0x06] (word 6, the turn angle) with 90 directly.
            const float turn = req.F(moveword::kTurn);
            const bool eq = quarter == turn;
            const bool lt = quarter < turn;
            if (!lt && !eq) band = k.five;
          }
        } else if (curMotion == 0xbcf) {
          // PES at 0x1406C3C02 reads the signed body-direction difference (fVar23 in the
          // decompile), not its magnitude.
          if (static_cast<int>(rowFixed) <= clipFrame && k.angle135 < facingDelta) top = scale;
        } else if (curMotion == 0xbd6) {
          if ((static_cast<int>(rowFixedB) + static_cast<int>(rowFixed)) / 2 < clipFrame) top = scale;
        } else if (curMotion == 0xc02) {
          if (static_cast<int>(rowFixed) < clipFrame) top = scale;
        } else if (curMotion == 0xc03 && static_cast<int>(rowFixed) <= clipFrame) {
          const bool eq = stopBand == speed;
          const bool lt = stopBand < speed;
          if (!lt && !eq) band = k.five;
        }
        const float lookAbs =
            std::fabs(Wrap180Diff(req.F(moveword::kLookHeading), req.F(moveword::kHeading), k));
        if (quarter < lookAbs && req.F(moveword::kDistToTarget) <= k.blend03 &&
            req.F(moveword::kDistToTarget) != k.blend03) {
          if (speed < k.speedA) {
            req.SetU(moveword::kClassWord, 0);
            req.SetF(moveword::kSpeedWord, k.five);
            EnterStop(host, req);  // FUN_1406d5d60
            return;
          }
          if (speed < k.speed135) {
            req.SetF(moveword::kSpeedWord, k.speedA);
            req.SetU(moveword::kClassWord, 1);
            EnterStop(host, req);  // FUN_1406d5d60
            return;
          }
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kSpeedWord, k.speedD);
          EnterStop(host, req);  // FUN_1406d5d60
          return;
        }
        if (speed < stopBand) {  // LAB_1406c3f2b
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
          EnterGotoRun(host, req, false);  // FUN_1406ce820
          return;
        }
        if (near) {
          if (k.five <= speed) {
            if (k.speedB <= speed) {
              req.SetU(moveword::kClassWord, 2);
              req.SetF(moveword::kSpeedWord, k.speedD);
            } else {
              req.SetF(moveword::kSpeedWord, k.speedA);
              req.SetU(moveword::kClassWord, 1);
            }
          } else {
            req.SetU(moveword::kClassWord, 0);
            req.SetF(moveword::kSpeedWord, k.five);
          }
          if (req.U(moveword::kPrecise) == 1 &&
              k.turn50625 <
                  std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k))) {
            const float iface = InterfaceSpeed(host);
            if (EnterRunTurnStep(host, req, k, (iface - k.startHalf) * k.half)) return;
            EnterStop(host, req);  // FUN_1406d2590
            return;
          }
          if (liveClass < 2 &&
              static_cast<int>(req.U(moveword::kCommandSpeedClass)) == 3 &&
              k.angle45 < faceAbs && speed < k.speed22 &&
              static_cast<int>(rowFixed) <= clipFrame) {
            req.SetU(moveword::kClassWord, 1);
            req.SetF(moveword::kSpeedWord, k.speedA);
            float rate;
            if (speed <= 0.0f) {
              rate = k.turnRamp;
            } else {
              const float share = speed / k.speed28;
              rate = (share <= k.relax07 ? k.turnRamp <= share ? share : k.turnRamp
                                                : k.turnRamp);
            }
            req.SetF(moveword::kRate, rate);
            EnterAccelerate(host, req);  // FUN_1406ccd00
          }
          EnterRunTurnRun(host, req, k);  // FUN_1406d6780
          return;
        }
        if (k.five <= speed) {
          if (k.speedB <= speed) {
            req.SetU(moveword::kClassWord, 2);
            req.SetF(moveword::kSpeedWord, k.speedD);
          } else {
            req.SetF(moveword::kSpeedWord, k.speedA);
            req.SetU(moveword::kClassWord, 1);
          }
        } else {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
        }
        if (!EnterStepTurnRun(host, req, k)) {
          // PES at 0x1406C3F4B compares req[0x06] (word 6, the turn angle) with 90.
          if (speed < stopBand + k.three && quarter < std::fabs(req.F(moveword::kTurn))) {
            req.SetU(moveword::kClassWord, 0);  // LAB_1406c3f2b
            req.SetF(moveword::kSpeedWord, k.five);
            EnterGotoRun(host, req, false);  // FUN_1406ce820
            return;
          }
          AdjustAngleApply(host);      // (+0x4d0 vt+0x08)
          req.SetByte(moveword::kReorientFlag, 0);  // param_2 + 0x3c = 0
          RunTurnStepReselect(host, req, k, clipFrame, spanF, spanG, dispBack, rowFixed,
                              rowFixedB);
          return;
        }
        return;
      }
      // 0x1406c3c46: the look-at gate hands the tick to the stop entry.
      if (std::fabs(req.F(moveword::kFacingError)) < k.angle675) {
        EnterStop(host, req);  // FUN_1406d5d60
        return;
      }
    }
  }

  // 6. The facing write (0x1406c4220..0x1406c45c9) and the exit gate.
  if (static_cast<int>(spanF) <= clipFrame &&
      (rowOffset != 0.0f || static_cast<int>(rowFixed) <= clipFrame)) {
    req.SetByte(moveword::kRootFacing, 0);  // param_2 + 0x3c = 0
    AdjustAngleApply(host);
    float facing = req.F(moveword::kFacing);
    const int echoRow = static_cast<int>(req.U(moveword::kMoveBucket));
    if (echoRow == 7) {
      facing += quarter;
      facing = FoldUnit(facing, k);
    } else if (echoRow == 2) {
      facing -= quarter;
      facing = FoldUnit(facing, k);
    } else {
      facing -= k.halfTurn;
      facing = FoldUnit(facing, k);
    }
    float toFace = WrapHalf(Wrap180Diff(facing, req.F(moveword::kFacing), k), k);
    float lookHead = FoldUnit(req.F(moveword::kLookHeading), k);
    float toLook = std::fabs(Wrap180Diff(facing, lookHead, k));
    float blend;
    if (dispBack < toLook) {
      blend = scale;
      if (toLook < k.angle45) blend = (toLook - dispBack) / dispBack;
    } else {
      blend = 0.0f;
    }
    const float ticks = static_cast<float>(host.TicksPerSecond());
    float step = (blend * quarter + quarter) / ticks;
    float stepped = step;
    if (toFace <= step) {
      stepped = WrapHalf(WrapHalf(step, k), k);
      if (stepped <= toFace) stepped = step;
      stepped = toFace;
    }
    stepped += req.F(moveword::kFacing);
    stepped = FoldUnit(stepped, k);
    req.SetByte(moveword::kRootFacing, 1);  // param_2 + 0x3a = 1
    req.SetF(moveword::kRootFacingAngle, stepped);  // param_2[0x3b]
  }
  // The exit gate: while the row has frames and the turn still needs work the tick is kept.
  bool stay = false;
  if (static_cast<int>(spanG) <= clipFrame) {
    const int precise = static_cast<int>(req.U(moveword::kPrecise));
    const int cmdClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    if (precise == 0 && cmdClass == 3) {
      stay = true;
    } else if (cmdClass < 1 &&
               std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kFacingError),
                                     k)) < narrow) {
      stay = true;
    } else if (precise == 1 && k.angle45 < std::fabs(req.F(moveword::kLookHeading)) &&
               static_cast<int>(spanF) <= clipFrame) {
      stay = true;
    } else if (precise == 0 &&
               quarter <
                   std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k)) &&
               static_cast<int>(spanF) <= clipFrame) {
      stay = true;
    }
  }
  if (clipFrame < static_cast<int>(spanG) && !stay) return;

  // The final select (FUN_1406c3470:489-613 and :739-855): the row grid at the live speed.
  RunTurnStepReselect(host, req, k, clipFrame, spanF, spanG, dispBack, rowFixed, rowFixedB);
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
  const bool relax = EntryAimFlag(host, req);
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
  const int column = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[bucket & 7]));
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
        const RowPick pick = PickRow(host, motion, column);
        if (pick.found) {
          if (!host.Percent(pick.motion)) {
            if (host.Percent(stampTicks)) motion = 0x5d4;
            reselect = true;
          }
        }
      }
      IssueClip(host, PickRow(host, motion, column).motion, PickRow(host, motion, column).mirror,
                k.clipBlend, playback, 0.0f);
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
        int ability = AbilityByte(host, 0x29) - 0x14;
        if (!relax) ability -= 0x14;
        ability = ability > 99 ? 99 : (ability < 0 ? 0 : ability);
        motion = host.Percent(ability) ? 0x5d0 : 0x5cf;
        const int win = static_cast<int>(scale * k.thirty +
                                         (k.angle90 < req.F(moveword::kTurn) ? 5.0f : 2.0f));
        const RowPick pick = PickRow(host, motion, column);
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
        const RowPick pick = PickRow(host, motion, column);
        if (pick.found) {
          if (!host.Percent(pick.motion)) {
            if (host.Percent(2)) motion = 0x5d4;
            reselect = true;
          }
        }
        (void)win;
      }
    }
    IssueClip(host, PickRow(host, motion, column).motion, PickRow(host, motion, column).mirror,
              k.clipBlend, playback, 0.0f);
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
    // FUN_140a58550's ability byte against 0x14 (20).
    if (AbilityByte(host, 0) < 0x14) motion = 0x5b6;
  }
  IssueClip(host, PickRow(host, motion, column).motion, PickRow(host, motion, column).mirror,
            k.clipBlend, playback, 0.0f);
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
  const bool relax = EntryAimFlag(host, req);  // FUN_1406ba7d0
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
  float clipLength = host.RecordF2468();           // P+0x2468
  if (clipLength < k.gFloor) clipLength = k.one;
  const float scale = k.one;
  const int motion = host.CurrentMotion();  // (P+0xab8 vt+0x18)()
  const int rowStart = RecordStartFrames(host, motion, scale, k);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale, k);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, req.Flag(moveword::kMirror), k);
  const int rowFixed = RecordFixedFrames(host, motion, scale, k);  // FUN_1406af620
  // FUN_1406af8e0's sub-record walk: the last band, then 0, then 1.
  const pes::MotionRecord& motionRec = host.Motion(motion);
  const int bands = motionRec.CancelCount();  // FUN_1406afbc0(..., 5)
  float lastFrames = static_cast<float>(rowFixed);
  float firstFrames = static_cast<float>(rowFixed);
  float secondFrames = static_cast<float>(rowFixed);
  if (0 < bands) {
    lastFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(bands - 1), scale, k));
    firstFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(0), scale, k));
    if (1 < bands)
      secondFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(1), scale, k));
  }
  const float half = k.half;
  req.SetF(moveword::kRate, 0.3f);  // param_2[0x4a] = 0x3e99999a
  // 0x1406c7630: the row offset of zero swaps the dwell for the clock's own window.
  int rowStartLive = rowStart;
  int rowEndLive = rowEnd;
  if (rowOffset == 0.0f) {
    const float ticks = static_cast<float>(host.TicksPerSecond());
    rowStartLive = 0;
    rowEndLive = static_cast<int>(ticks * k.ten / k.fps + half);
  }
  if (static_cast<int>(lastFrames) < rowEndLive) rowEndLive = static_cast<int>(lastFrames);

  const float turn675 = k.angle675;
  const float narrow225 = k.angle225m;
  const float turn7875 = k.turn7875;
  const float angle45 = k.angle45;
  // 2. The reorient window (0x1406c770e..0x1406c7800) and the yaw lock's write.
  if (!ClipHalted(host) && rowStartLive <= clipFrame && clipFrame <= (rowStartLive + rowEndLive) / 2) {
    // PES's row angle is param_2[0x18] (the turned heading), not the bucket word.
    float row = req.F(moveword::kTurnedRow);  // param_2[0x18]
    const uint32_t frames =
        static_cast<uint32_t>(static_cast<float>(rowEndLive - clipFrame) / clipLength + half);
    // PES at 0x1406C7695: the precise gate's inner test is
    // wrap180(kLookHeading - kBodyDirection), and at 0x1406C76CF the window's own gate is
    // wrap180(kBodyDirection - kTurnedRow).
    const float bodyDelta =
        std::fabs(Wrap180Diff(req.F(moveword::kBodyDirection), req.F(moveword::kLookHeading), k));
    const float facingDelta =
        std::fabs(Wrap180Diff(req.F(moveword::kTurnedRow), req.F(moveword::kBodyDirection), k));
    if ((req.U(moveword::kPrecise) != 1 || bodyDelta < angle45) && facingDelta < turn7875) {
      row = StepHeadingToPoint(row, req.F(moveword::kHeading), turn675, k);
    }
    // PES at 0x1406C7835: the ratio's numerator is wrap180(kLookHeading - kFacing).
    const float ratio =
        std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kLookHeading), k)) /
        static_cast<float>(frames);
    if (ratio < angle45) {
      const float bucketRow = static_cast<float>(req.U(moveword::kMoveBucket));
      const float turned = row + k.bucketAngle[static_cast<int>(bucketRow) & 9];
      req.SetF(moveword::kAngle3d, turned);  // param_2[0x3d] = uVar23
      req.SetByte(moveword::kReorientFlag, 1);  // param_2 + 0x3c = 1
      req.SetU(moveword::kTurnFrames, frames);   // param_2[0x3e] = uVar18
      req.SetByte(moveword::kMirrorFlag, 0);     // param_2 + 0x3f = 0
      req.SetF(moveword::kRowAngle, static_cast<float>(row));  // param_2[0x18] = uVar23
      if (turn675 < std::fabs(rowOffset)) {
        const float signOffset = rowOffset < 0.0f ? k.negOne : scale;
        const float signDelta = facingDelta < 0.0f ? k.negOne : scale;
        if (signOffset != signDelta) {
          uint8_t flag = static_cast<uint8_t>(req.U(moveword::kMirrorFlag) & 0xffu);
          if (k.angle3325 < std::fabs(facingDelta)) flag = 1;  // DAT_1425ddf9c
          req.SetByte(moveword::kMirrorFlag, flag);
        }
      }
      host.SetRecordF0bec(host.RecordF0bec());  // FUN_1406cbb00's own write, kept for the port
    }
  }

  // 3. The run-family branch (0x1406c7800..0x1406c7b00).
  const float ticks = static_cast<float>(host.TicksPerSecond());
  const bool runBranch =
      static_cast<int>(req.U(moveword::kCommandId)) != 0x11 &&
      clipFrame < static_cast<int>(ticks * k.turnTicks / k.fps + half) &&
      0 <= static_cast<int>(req.U(moveword::kCommandSpeedClass)) &&
      req.U(moveword::kPrecise) == 1;
  if (runBranch) {
    // PES's three reads here: 0x1406C76CF is wrap180(kBodyDirection - kTurnedRow), 0x1406C770A
    // is wrap180(kTurnedRow - kFacing) and 0x1406C7835 is wrap180(kLookHeading - kFacing).
    const float loose =
        std::fabs(Wrap180Diff(req.F(moveword::kTurnedRow), req.F(moveword::kBodyDirection), k));
    const float tight =
        std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kTurnedRow), k));
    const float lookDelta =
        std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kLookHeading), k));
    if ((loose < tight || loose < narrow225) &&
        (k.angle3375 < lookDelta || turn7875 < tight)) {
      AdjustAngleApply(host);
      const float bandHigh = k.speedB;
      const float bandLow = k.five;
      req.SetByte(moveword::kReorientFlag, 0);  // param_2 + 0x3c = 0
      const int bucket = static_cast<int>(req.U(moveword::kPrevBucket));  // param_2[0x25]
      float band = bandLow;
      if (turn675 < std::fabs(req.F(moveword::kTurn))) {
        band = k.speedA;
        band = k.speed135;
      }
      const float dispHeading = DispSpeedKmh(host, k);
      if (band <= dispHeading) {
        if (bandHigh <= dispHeading) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kSpeedWord, k.speedD);  // 0x41800000
        } else {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kSpeedWord, k.speedA);  // 0x41300000
        }
      } else {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kSpeedWord, k.five);  // 0x40a00000
      }
      int cls = static_cast<int>(req.U(moveword::kClassWord) & 0xffu);
      cls = cls < 3 ? (cls < 0 ? 0 : cls) : 2;
      const float speedWord = req.F(moveword::kSpeedWord);
      float speed = k.speedD;
      if (band <= speedWord && k.five <= speedWord) {
        speed = k.speedD < speedWord ? k.speedD : speedWord;
      } else {
        speed = k.five;
      }
      const int kind = static_cast<int>(req.U(moveword::kKind));
      const int kindSel = kind < 2 ? 2 : (kind == 4 ? 5 : kind);
      const int column = static_cast<int>(static_cast<uint32_t>(k.bucketColumn[bucket]));
      int row = static_cast<int>(static_cast<uint32_t>(k.stepRow[kindSel + cls * 8]));
      if (RowIs(static_cast<uint32_t>(row), 0x54eu)) row = 0x54f;  // 1.85812e-42 -> 1.86092e-42
      const RowPick pick = PickRow(host, row, column);
      req.SetByte(moveword::kFootFlag, k.bucketLeft[bucket]);
      req.SetF(moveword::kSpeedWord, speed);
      req.SetF(moveword::kPrevSpeedWord, speed);
      req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));
      req.SetU(moveword::kPrevClassWord, static_cast<uint32_t>(cls));
      req.SetU(moveword::kPrevBucket, static_cast<uint32_t>(bucket));
      req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));
      req.SetF(moveword::kPlayback, PlaybackAt(host, k));
      const float start = host.MotionStartTime(static_cast<uint16_t>(pick.motion));
      IssueClip(host, pick.motion, pick.mirror, k.westBlend,
                req.F(moveword::kPlayback), start);
      req.SetU(moveword::kDwellElapsed, 0);  // param_2[0x1a]
      req.SetU(moveword::kDwellLimit, 0);    // param_2[0x1b]
      req.SetU(moveword::kPrevKind, static_cast<uint32_t>(kindSel));  // param_2[0x1e]
      RequestState(req, MoveState::kStepLoop);  // req[0x39] = 4
      return;
    }
  }

  // 4. The post-branch prologue (0x1406c7b00) and the gates.
  const float dispKmh = DispSpeedKmh(host, k);
  const uint32_t savedSpeed = req.U(moveword::kSpeedWord);
  const uint32_t savedClass = req.U(moveword::kClassWord);
  // PES at 0x1406C7BB3: wrap180(kBodyDirection - kTurnedRow).
  const float dispDelta =
      std::fabs(Wrap180Diff(req.F(moveword::kTurnedRow), req.F(moveword::kBodyDirection), k));
  const float dispDeltaAbs = dispDelta;
  if (static_cast<int>(savedClass) - 1 < 2u &&
      2 < static_cast<int>(req.U(moveword::kCommandSpeedClass)) &&
      req.U(moveword::kPrecise) == 0 && dispDeltaAbs < angle45 &&
      host.AnimeState() != 0x4a && (req.U(moveword::kMirrorFlag) & 0xffu) == 0) {
    return;  // FUN_1406d01d0's carried tick
  }

  // The motion-gated branches (0x1406c7f00..0x1406c8100).
  const float band135 = k.speed135;
  const float band8 = k.speedA;
  const float halfB = k.half;
  if (motion == 0xe96 && dispKmh < k.five &&
      1 < static_cast<int>(req.U(moveword::kCommandSpeedClass))) {
    EnterGotoRun(host, req, false);  // LAB_1406c7f7c: FUN_1406ce820
    return;
  }
  if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) != -1 &&
      clipFrame < static_cast<int>(ticks * k.turnTicks / k.fps + halfB) &&
      turn675 < dispDeltaAbs) {
    if (dispKmh < k.turnTicksB) {
      EnterGotoRun(host, req, false);  // LAB_1406c7f7c
      return;
    }
    req.SetU(moveword::kClassWord, 2);
    req.SetF(moveword::kSpeedWord, k.speedD);
    if (band8 <= dispKmh) {
      if (dispKmh < band135) {
        req.SetF(moveword::kSpeedWord, k.speedA);
        req.SetU(moveword::kClassWord, 1);
      }
    } else {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
    }
    if (EnterStepTurnRun(host, req, k)) return;  // FUN_1406d38d0
    req.SetU(moveword::kSpeedWord, savedSpeed);
    req.SetU(moveword::kClassWord, savedClass);
  }

  // 5. The tolerance shaping and the ladder (0x1406c8100..0x1406c88de).
  const uint32_t precise = req.U(moveword::kPrecise);
  float turnTol = turn675;
  if (precise == 0 && 0 < static_cast<int>(req.U(moveword::kClassWord))) {
    const float dist = req.F(moveword::kDistToTarget);
    float scaleTerm = 0.0f;
    if (k.two < dist) {
      scaleTerm = (dist < k.four) ? (dist - k.two) * halfB : scale;
    }
    turnTol = scaleTerm * k.angle3375 + turn675;
  }
  bool jostle = false;
  // PES at 0x1406C7D8B: wrap180(kBodyDirection - kFacing).
  if (rowEndLive <= clipFrame ||
      std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k)) <
          angle45) {
    jostle = true;
  }
  bool ladderDone = false;
  if ((static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1 && precise == 1) ||
      turnTol < dispDeltaAbs) {
    if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1) {
      if (jostle && k.five <= dispKmh) {
        req.SetU(moveword::kClassWord, 3);
        req.SetF(moveword::kSpeedWord, 28.0f);  // 0x41e00000
        TurnStopLadderOrStop(host, req, k, dispKmh);
        return;
      }
      if (!jostle && k.five <= dispKmh) {
        if (k.speedB <= dispKmh) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kSpeedWord, k.speedD);
        } else {
          req.SetF(moveword::kSpeedWord, k.speedA);
          req.SetU(moveword::kClassWord, 1);
        }
        req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
        req.SetU(moveword::kMoveBucket,
                 static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kFacing))));
        EnterStop(host, req);  // FUN_1406d5d60
        return;
      }
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
      req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
      req.SetU(moveword::kMoveBucket,
               static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kFacing))));
      EnterStop(host, req);
      return;
    }
    if (static_cast<int>(ticks * k.turnTicks / k.fps + halfB) <= clipFrame &&
        clipFrame < static_cast<int>(firstFrames)) {
      ladderDone = true;  // LAB_1406c8375: the run-tail ladder below
    } else {
      uint32_t cmdClass = req.U(moveword::kCommandSpeedClass);
      if (cmdClass != 0xffffffff && precise == 0 &&
          (static_cast<int>(req.U(moveword::kClassWord)) < 1 ||
           (dispKmh < k.five && clipFrame < static_cast<int>(secondFrames)))) {
        EnterGotoRun(host, req, false);  // LAB_1406c7f7c
        return;
      }
      const float band22 = k.speed22;
      if (static_cast<int>(firstFrames) <= clipFrame &&
          std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k)) <
              k.angle3375) {
        if (0 < static_cast<int>(cmdClass) && 0 < static_cast<int>(req.U(moveword::kClassWord))) {
          if (precise == 1) {
            const float live = InterfaceSpeed(host);
            req.SetF(moveword::kRate, 0.6f);  // 0x3f19999a
            const float halfStart = (live - k.startHalf) * halfB;
            if (band8 <= dispKmh) {
              if (band135 <= dispKmh) {
                if (band22 < dispKmh) req.SetF(moveword::kRate, live);
              } else {
                req.SetF(moveword::kRate, 0.3f);
              }
            } else {
              const float floor = 2 < static_cast<int>(req.U(moveword::kClassWord))
                                      ? k.five
                                      : band8;
              if (floor > dispKmh) {
                req.SetF(moveword::kRate, 0.15f);  // 0x3e19999a
              } else if (band135 <= dispKmh) {
                if (band22 < dispKmh) req.SetF(moveword::kRate, live);
              } else {
                req.SetF(moveword::kRate, 0.3f);
              }
            }
            req.SetU(moveword::kClassWord, static_cast<uint32_t>(InterfaceClass(host)));
            req.SetF(moveword::kSpeedWord, InterfaceSpeed(host) * k.kmhToRate);
            if (EnterRunTurnStep(host, req, k, halfStart)) return;  // FUN_1406d2f60
            if (static_cast<int>(req.U(moveword::kClassWord)) == 0) {
              EnterGotoRun(host, req, false);  // FUN_1406ce820
              return;
            }
          } else if (movestep::StepLetsRunTurn(host, req)) {  // FUN_1406b70d0
            req.SetF(moveword::kRate, 0.6f);
            if (band8 <= dispKmh) {
              if (band135 <= dispKmh) {
                if (band22 < dispKmh) {
                  req.SetF(moveword::kRate, InterfaceSpeed(host) * k.playbackFloor);
                }
              } else {
                req.SetF(moveword::kRate, 0.3f);
              }
            } else {
              req.SetF(moveword::kRate, 0.15f);
            }
            req.SetU(moveword::kClassWord, static_cast<uint32_t>(InterfaceClass(host)));
            req.SetF(moveword::kSpeedWord, InterfaceSpeed(host) * k.kmhToRate);
            EnterRunTurnRun(host, req, k);  // FUN_1406d6780
            return;
          }
        }
        cmdClass = req.U(moveword::kCommandSpeedClass);
        if (cmdClass == 0xffffffff && precise == 1) {
          req.SetU(moveword::kClassWord, 3);
          req.SetF(moveword::kSpeedWord, 28.0f);
          TurnStopLadderOrStop(host, req, k, dispKmh);
          return;
        }
      }
      if (cmdClass != 0xffffffff && 0 < static_cast<int>(req.U(moveword::kClassWord))) {
        const float band22 = k.speed22;
        float floorBand = band22;
        if (static_cast<int>(firstFrames) <= clipFrame) floorBand = band8;
        if (dispKmh < floorBand &&
            (precise != 1 || std::fabs(req.F(moveword::kFacingError)) < k.angle90)) {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
          EnterGotoRun(host, req, false);
          return;
        }
        req.SetU(moveword::kClassWord, 3);
        req.SetF(moveword::kSpeedWord, 28.0f);
        if (band8 <= dispKmh) {
          if (k.speedD <= dispKmh) {
            if (dispKmh < band22) {
              req.SetU(moveword::kClassWord, 2);
              req.SetF(moveword::kSpeedWord, k.speedD);
            }
          } else {
            req.SetF(moveword::kSpeedWord, k.speedA);
            req.SetU(moveword::kClassWord, 1);
          }
        } else {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
        }
        if (EnterStepTurnRun(host, req, k)) return;  // FUN_1406d38d0
        if (dispKmh < floorBand + halfB &&
            (precise != 1 || std::fabs(req.F(moveword::kFacingError)) < k.angle90)) {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
          EnterGotoRun(host, req, false);
          return;
        }
      }
      if (std::fabs(req.F(moveword::kFacingError)) < turn675 || k.half < dispDeltaAbs) {
        EnterStop(host, req);
        return;
      }
      ladderDone = true;
    }
  }

  // The run-tail ladder and the damping (0x1406c8375..0x1406c8b55).
  const float bandwidth = k.angle1125b;
  const float band0 = k.zero;
  if (ladderDone ||
      (rowEndLive <= clipFrame && (rowOffset != band0 ||
                                   static_cast<int>(firstFrames) <= clipFrame)) ||
      ((rowStartLive + rowEndLive) / 2 <= clipFrame &&
       (bandwidth < dispDeltaAbs &&
        std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k)) <
            narrow225))) {
    DampFacingTowardMove(host, req, k, dispDeltaAbs, rowOffset, rowEndLive, firstFrames,
                         static_cast<int>(req.U(moveword::kClassWord)));
  }

  // The parametric replay block (0x1406c8b55's tail).
  firstFrames = static_cast<float>(RecordStartFrames(host, motion, scale, k));
  req.SetU(moveword::kSpeedWord, savedSpeed);
  req.SetU(moveword::kClassWord, savedClass);
  if (static_cast<int>(firstFrames) <= clipFrame &&
      static_cast<int>(savedClass) < 3 &&
      static_cast<int>(req.U(moveword::kCommandSpeedClass)) == 3 && motion == 0x2e &&
      dispDeltaAbs < angle45 && clipFrame < static_cast<int>(secondFrames)) {
    const int clipFrames = host.CurrentClipFrames();
    const bool animMirror = req.Flag(moveword::kMirror);
    const float startRate = static_cast<float>(clipFrames + 1) / ticks;
    IssueClip(host, 0x3b, !animMirror, k.turnRamp,
              req.F(moveword::kPlayback), startRate);
    req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
    req.SetF(moveword::kPrevSpeedWord, req.F(moveword::kSpeedWord));
    req.SetF(moveword::kSpeedWord, 28.0f);  // 0x41e00000
    req.SetU(moveword::kClassWord, 3);
    req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
    req.SetU(moveword::kRowAngle, req.U(moveword::kHeading));
    req.SetU(moveword::kMoveBucket, req.U(moveword::kMoveBucket));
    req.SetByte(moveword::kReorientFlag, 0);
    AdjustAngleApply(host);
    return;
  }

  // The accelerate / step branch (0x1406c88de..0x1406c8a68) and the final rate tail.
  const float turn45 = angle45;
  bool localJostle = jostle;
  if ((clipFrame < rowEndLive && k.angle3375 <= std::fabs(Wrap180Diff(
                                      req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k))) ||
      clipFrame < static_cast<int>(firstFrames)) {
    if (static_cast<int>(req.U(moveword::kClassWord)) - 1 < 2u &&
        2 < static_cast<int>(req.U(moveword::kCommandSpeedClass)) &&
        req.U(moveword::kPrecise) == 0 && dispDeltaAbs < turn45 &&
        static_cast<int>(firstFrames) <= clipFrame &&
        (turn45 < std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k)) &&
         motion != 0xb32 && motion != 0xc68)) {
      if (!ClipFading(host) || static_cast<int>(secondFrames) <= clipFrame) {
        req.SetU(moveword::kClassWord, 3);
        req.SetF(moveword::kSpeedWord, 28.0f);
        if (band8 <= dispKmh) {
          if (k.speedD <= dispKmh) {
            if (dispKmh < k.speed22) {
              req.SetU(moveword::kClassWord, 2);
              req.SetF(moveword::kSpeedWord, k.speedD);
            }
          } else {
            req.SetF(moveword::kSpeedWord, k.speedA);
            req.SetU(moveword::kClassWord, 1);
          }
        } else {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
        }
        if (EnterStepTurnRun(host, req, k)) return;
      }
    }
  } else if ((precise == 1 && turn45 < std::fabs(req.F(moveword::kFacingError))) ||
             (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == 0 &&
              req.U(moveword::kClassWord) != 0)) {
    localJostle = true;
  } else if (std::fabs(req.F(moveword::kTurn)) <= k.angle5625 ||
             dispDeltaAbs <= k.angle5625) {
    if (2 < static_cast<int>(req.U(moveword::kCommandSpeedClass)) &&
        static_cast<int>(req.U(moveword::kClassWord)) < 3) {
      if (!ClipFading(host) || static_cast<int>(secondFrames) <= clipFrame) {
        if (motion != 0xc81 || (turn45 < dispDeltaAbs || rowEndLive <= clipFrame)) {
          float disp[3] = {0.0f, 0.0f, 0.0f};
          host.PositionAt(1, disp);
          const float rotatedHeading = DispSpeedKmh(host, k);
          if (k.turnTicksB < rotatedHeading) localJostle = true;
        }
      }
    }
    if ((static_cast<int>(req.U(moveword::kCommandSpeedClass)) < 2 &&
         1 < static_cast<int>(req.U(moveword::kClassWord))) ||
        (static_cast<int>(req.U(moveword::kCommandSpeedClass)) < 3 &&
         2 < static_cast<int>(req.U(moveword::kClassWord)))) {
      localJostle = true;
    }
  } else {
    localJostle = true;
  }

  if (static_cast<int>(req.U(moveword::kClassWord)) < 1 ||
      static_cast<int>(req.U(moveword::kCommandSpeedClass)) != 3 ||
      !(turn45 <= dispDeltaAbs)) {
    if (clipFrame < static_cast<int>(lastFrames)) return;
  } else {
    if (static_cast<int>(lastFrames) <= clipFrame) {
      if (dispKmh < k.speed22) {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kSpeedWord, k.speedD);
        if (EnterAccelerate(host, req)) return;  // FUN_1406ccd00
      }
      if (clipFrame < static_cast<int>(lastFrames)) return;
    }
    if (!localJostle) return;
  }

  const int foot = FootBandOf(host);
  req.SetByte(moveword::kFootFlag, (foot - 1U < 2u) ? 1 : 0);
  const float sampleTicks = static_cast<float>(host.TicksPerSecond());
  float sample[3] = {0.0f, 0.0f, 0.0f};
  // FUN_141eb50b0(P, out, 4): the 4-tick displacement ring.
  host.PositionAt(static_cast<int>(sampleTicks * k.four / k.fps + halfB), sample);
  const float sampleKmh = DispSpeedKmh(host, k);
  float band = k.speed22;
  if (clipFrame < static_cast<int>(lastFrames)) {
    if (k.five <= dispKmh || static_cast<int>(req.U(moveword::kClassWord)) - 2 < 2u) {
      if (dispKmh < k.speedB &&
          (static_cast<int>(req.U(moveword::kClassWord)) != 3 ||
           clipFrame < static_cast<int>(secondFrames))) {
        if (!ClipFading(host) || clipFrame < static_cast<int>(secondFrames)) {
          req.SetF(moveword::kSpeedWord, k.speedA);
          req.SetU(moveword::kClassWord, 1);
          band = k.speed22;
        }
      }
      band = k.speed22;
      if (k.speed22 <= dispKmh ||
          (static_cast<int>(req.U(moveword::kClassWord)) == 3 &&
           static_cast<int>(secondFrames) <= clipFrame)) {
        req.SetU(moveword::kClassWord, 3);
        req.SetF(moveword::kSpeedWord, 28.0f);
      } else {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kSpeedWord, k.speedD);
      }
    } else {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kSpeedWord, k.five);
      band = k.speed22;
    }
  }
  req.SetU(moveword::kDwellLimit, 0);     // param_2[0x1b]
  req.SetU(moveword::kDwellLimit2, 0);     // param_2[0x2e]
  req.SetByte(moveword::kStyle, 0);        // param_2 + 0x50
  // PES writes the foot band into the 0x7c word here; the port's foot band lives on the host
  // (MoveHost::FootBand), and the request's word 0x7c is the shared class word, so nothing is
  // stored on the request.
  (void)foot;
  req.SetU(moveword::kPrevKind, req.U(moveword::kKind));
  if (static_cast<int>(req.U(moveword::kClassWord)) == 3 && sampleKmh < k.speed26) {
    float rate;
    if (sampleKmh <= k.speedE) {
      if (sampleKmh <= band) {
        rate = sampleKmh <= k.stampsA ? 0.6f : (InterfaceSpeed(host) + k.startHalf) * k.half;
      } else {
        rate = (sampleKmh + sampleKmh + InterfaceSpeed(host) + k.startHalf + k.startHalf) / k.three;
      }
    } else {
      rate = (sampleKmh + sampleKmh + k.startHalf) / k.three;
    }
    const float live = InterfaceSpeed(host);
    req.SetF(moveword::kRate, (rate <= live && 0.0f <= rate) ? rate : 0.0f);
  }
  (void)clipLength;
}

// ---------------------------------------------------------------------------------------------
// The loader. Every entry names its VA; the install test checks the relations.
// ---------------------------------------------------------------------------------------------
// FUN_1406c8d80: STEP_TURN_STEP, the step-to-step turn.
//
// PES's order:
//   1. the motion record's frame spans and the dwell window;
//   2. the reorient window (req[0x3c]..[0x3f]) and the slip write;
//   3. the early gate: the three motion ids replay a row and hand the tick to STEP_TURN_RUN;
//   4. the ladder: the stop entry, the run entries, or the step-family re-select;
//   5. the facing write (req[0x3a]/[0x3b]) and the final grid select.
// ---------------------------------------------------------------------------------------------
void StepTurnStep(MoveHost& host, MoveReq& req, const MoveTurnConstants& k) {
  const int clipFrame = host.CurrentClipFrames();  // (P+0xab8 vt+0x38)()
  float clipLength = host.RecordF2468();           // P+0x2468
  if (clipLength < k.gFloor) clipLength = k.one;
  const float scale = k.one;
  const int motion = host.CurrentMotion();  // (P+0xab8 vt+0x18)()
  const int rowStart = RecordStartFrames(host, motion, scale, k);  // FUN_1406b1f00
  const int rowEnd = RecordEndFrames(host, motion, scale, k);      // FUN_1406b1d00
  const float rowOffset = RecordOffsetDeg(host, motion, req.Flag(moveword::kMirror), k);
  const int rowFixed = RecordFixedFrames(host, motion, scale, k);  // FUN_1406af620
  // FUN_1406af8e0's sub-record walk: the last band, then 0.
  const pes::MotionRecord& motionRec = host.Motion(motion);
  const int bands = motionRec.CancelCount();  // FUN_1406afbc0(..., 5)
  float lastFrames = static_cast<float>(rowFixed);
  float firstFrames = static_cast<float>(rowFixed);
  if (0 < bands) {
    lastFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(bands - 1), scale, k));
    firstFrames = static_cast<float>(FrameTicks(host, motionRec.CancelFrames(0), scale, k));
  }
  const float half = k.half;
  // 0x1406c8de0: the row offset of zero swaps the dwell for the clock's own window.
  int rowStartLive = rowStart;
  int rowEndLive = rowEnd;
  if (rowOffset == 0.0f) {
    const float ticks = static_cast<float>(host.TicksPerSecond());
    rowStartLive = 0;
    rowEndLive = static_cast<int>(ticks * k.ten / k.fps + half);
  }
  int rowEndClamped = rowEndLive;
  if (static_cast<int>(lastFrames) < rowEndClamped) rowEndClamped = static_cast<int>(lastFrames);

  // 2. The reorient window (0x1406c8e10..0x1406c8e90) and the slip write.
  if (!ClipHalted(host) && rowStartLive <= clipFrame &&
      clipFrame < (rowStartLive + rowEndClamped * 2) / 3) {
    float turned = req.F(moveword::kRowAngle);  // param_2[0x17]
    const uint32_t frames =
        static_cast<uint32_t>(static_cast<float>(rowEndClamped - clipFrame) / clipLength + half);
    // PES at 0x1406C8FE5: FUN_14043a0a0(req[0x17], heading - the bucket's angle) =
    // wrap180((req[0x01] - DAT_1425dd660[bucket]) - req[0x17]).
    const float loose = std::fabs(Wrap180Diff(
        turned, req.F(moveword::kHeading) - k.bucketAngle[static_cast<int>(req.U(moveword::kMoveBucket)) & 9],
        k));
    if (loose < k.angle90) {
      turned = StepHeadingToPoint(turned, req.F(moveword::kHeading), k.angle675, k);
    }
    const float toward = turned + k.bucketAngle[static_cast<int>(req.U(moveword::kMoveBucket)) & 9];
    const float folded = FoldUnit(toward, k);
    // PES at 0x1406C905A: FUN_14043a0a0(req[0x02], turned) = wrap180(turned - facing), where
    // `turned` is the stepped heading (the value FUN_140a92950 returned), not the folded one.
    // PES keeps the value SIGNED: the mirror's fix-up compares its sign with the row offset's.
    const float tightSigned = Wrap180Diff(req.F(moveword::kFacing), turned, k);
    const float tight = std::fabs(tightSigned);
    if (tight / static_cast<float>(frames) < k.angle45) {
      req.SetF(moveword::kAngle3d, turned);  // param_2[0x3d]
      req.SetByte(moveword::kReorientFlag, 1);  // param_2 + 0x3c = 1
      req.SetU(moveword::kTurnFrames, frames);  // param_2[0x3e]
      req.SetByte(moveword::kMirrorFlag, 0);    // param_2 + 0x3f = 0
      req.SetF(moveword::kTurnedRow, folded);   // param_2[0x18]
      const float ego = rowOffset;
      if (k.angle90 < std::fabs(ego)) {
        const float signEgo = ego < 0.0f ? k.negOne : scale;
        const float signTight = tightSigned < 0.0f ? k.negOne : scale;
        if (signEgo != signTight) {
          uint8_t m = static_cast<uint8_t>(req.U(moveword::kMirrorFlag) & 0xffu);
          if (k.angle225m < std::fabs(tight)) m = 1;
          req.SetByte(moveword::kMirrorFlag, m);
        }
      }
    }
  }

  // The framing of the ladder's shared values.
  const float narrow = k.angle225m;
  const float quarter = k.angle90;
  float dispDelta =
      std::fabs(Wrap180Diff(req.F(moveword::kTurnedRow), req.F(moveword::kHeading), k));
  const float lookDelta =
      std::fabs(Wrap180Diff(req.F(moveword::kLookHeading), req.F(moveword::kHeading), k));
  float turnTol = k.angle45;
  if (req.U(moveword::kPrecise) == 0 && 0 < static_cast<int>(req.U(moveword::kClassWord))) {
    const float dist = req.F(moveword::kDistToTarget);
    if (k.two < dist) {
      turnTol = (dist < k.four ? (dist - k.two) * half : scale) * k.angle225m + turnTol;
    }
  }

  // 3. The early gate (0x1406c8e90..0x1406c9010): while the clip is young the three motion ids
  // replay a row and hand the tick to STEP_TURN_RUN.
  if (static_cast<int>(req.U(moveword::kCommandId)) != 0x11 &&
      clipFrame < static_cast<int>(static_cast<float>(host.TicksPerSecond()) * k.turnTicks /
                                   k.fps + half)) {
    if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1) {
      req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));  // param_2[0x25] = [0x20]
      req.SetU(moveword::kMoveBucket,
               static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kFacing))));
      EnterStop(host, req);  // FUN_1406d5d60
      return;
    }
    if (k.angle5625 <= dispDelta) {
      const float dispKmh = DispSpeedKmh(host, k);
      if (k.speedA <= dispKmh) {
        if (k.speed135 <= dispKmh) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kSpeedWord, k.speedD);
        } else {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kSpeedWord, k.speedA);
        }
      } else {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kSpeedWord, k.five);
      }
      if (EnterStepTurnRun(host, req, k)) return;  // FUN_1406d38d0
      req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
      req.SetU(moveword::kMoveBucket,
               static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kFacing))));
      EnterStop(host, req);  // FUN_1406d5d60
      return;
    }
    if (dispDelta < k.angle1125b && static_cast<int>(req.U(moveword::kClassWord)) < 3 &&
        static_cast<int>(req.U(moveword::kCommandSpeedClass)) == 3) {
      // The three motion ids' replay rows.
      if (motion == 0x35) {
        const int subFrames = host.CurrentClipFrames();
        const bool animMirror = req.Flag(moveword::kMirror);
        const float startRate =
            static_cast<float>(subFrames + 1) / static_cast<float>(host.TicksPerSecond());
        IssueClip(host, 0x3f, animMirror, k.loopBlend,
                  req.F(moveword::kPlayback), startRate);
        StepTurnRunReplay(host, req, k);
        return;
      }
      if (motion == 0x39) {
        const int subFrames = host.CurrentClipFrames();
        const bool animMirror = req.Flag(moveword::kMirror);
        const float startRate =
            static_cast<float>(subFrames + 1) / static_cast<float>(host.TicksPerSecond());
        IssueClip(host, 0x4e, !animMirror, k.pointTwo,
                  req.F(moveword::kPlayback), startRate);
        StepTurnRunReplay(host, req, k);
        return;
      }
      if (motion == 0xc51) {
        const int subFrames = host.CurrentClipFrames();
        const bool animMirror = req.Flag(moveword::kMirror);
        const float startRate =
            static_cast<float>(subFrames + 1) / static_cast<float>(host.TicksPerSecond());
        IssueClip(host, 0xc6d, !animMirror, k.pointTwo,
                  req.F(moveword::kPlayback), startRate);
        StepTurnRunReplay(host, req, k);
        return;
      }
    }
  }

  // 4. The ladder (0x1406c9010..0x1406c9500).
  if (clipFrame < static_cast<int>(firstFrames)) {
    return;
  }
  {
    const int subFrames = host.CurrentClipFrames();
    (void)subFrames;
  }
  {
    // The replay / idle tail.
    const bool scripted = host.AnimeState() == 0x4a;
    if (!scripted) {
      if ((static_cast<int>(req.U(moveword::kCommandSpeedClass)) == -1 &&
           req.U(moveword::kPrecise) == 1) ||
          turnTol < dispDelta) {
        if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) != -1 &&
            req.U(moveword::kPrecise) == 0 &&
            static_cast<int>(req.U(moveword::kClassWord)) < 1) {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
          EnterGotoRun(host, req, false);  // LAB_1406c92f8: FUN_1406ce820
          return;
        }
        const float dispKmh = DispSpeedKmh(host, k);
        if (static_cast<int>(req.U(moveword::kCommandSpeedClass)) != -1 &&
            0 < static_cast<int>(req.U(moveword::kClassWord))) {
          const bool motionOk = motion == 0x4d || k.turnTicksB <= dispKmh;
          if (motionOk) {
            if (std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k)) <=
                k.angle135) {
              if (k.speedA <= dispKmh) {
                goto StepTurnRunBand;
              }
              req.SetU(moveword::kClassWord, 0);
              req.SetF(moveword::kSpeedWord, k.five);
            } else {
              if (dispKmh < k.speedA) {
                req.SetU(moveword::kClassWord, 0);
                req.SetF(moveword::kSpeedWord, k.five);
                EnterGotoRun(host, req, false);
                return;
              }
            StepTurnRunBand:
              if (k.speed135 <= dispKmh) {
                req.SetU(moveword::kClassWord, 2);
                req.SetF(moveword::kSpeedWord, k.speedD);
              } else {
                req.SetU(moveword::kClassWord, 1);
                req.SetF(moveword::kSpeedWord, k.speedA);
              }
            }
            if (EnterStepTurnRun(host, req, k)) return;  // FUN_1406d38d0
            if (k.speed55 <= dispKmh) {
              req.SetU(moveword::kClassWord, 0);
              req.SetF(moveword::kSpeedWord, k.five);
              EnterGotoRun(host, req, false);  // LAB_1406c95d6
              return;
            }
          }
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kSpeedWord, k.five);
          EnterGotoRun(host, req, false);
          return;
        }
        if (lookDelta < k.angle675) {  // DAT_1425cca34
          std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k));
          req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));
          req.SetU(moveword::kMoveBucket,
                   static_cast<uint32_t>(InterfaceBucket(host, req.F(moveword::kFacing))));
          EnterStop(host, req);  // FUN_1406d5d60
          return;
        }
      }
    }
  }

  // 5. The facing write (0x1406c9600..0x1406c9700) and the exit gate.
  if (rowEndClamped <= clipFrame) {
    req.SetByte(moveword::kReorientFlag, 0);  // param_2 + 0x3c = 0
    AdjustAngleApply(host);
    float facing = FoldUnit(req.F(moveword::kFacing), k);
    const int echoRow = static_cast<int>(req.U(moveword::kMoveBucket));
    if (echoRow == 7) {
      facing = FoldUnit(facing + quarter, k);
    } else if (echoRow == 2) {
      facing = FoldUnit(facing - quarter, k);
    } else {
      facing = FoldUnit(facing - k.halfTurn, k);
    }
    float toFace = WrapHalf(Wrap180Diff(facing, req.F(moveword::kFacing), k), k);
    float lookHead = FoldUnit(req.F(moveword::kLookHeading), k);
    float toLook = std::fabs(Wrap180Diff(facing, lookHead, k));
    float blend;
    if (k.angle225m < toLook) {
      blend = scale;
      if (toLook < k.angle45) blend = (toLook - k.angle225m) / k.angle225m;
    } else {
      blend = 0.0f;
    }
    const float ticks = static_cast<float>(host.TicksPerSecond());
    float step = (blend * quarter + quarter) / ticks;
    float stepped = step;
    if (toFace <= step) {
      stepped = WrapHalf(WrapHalf(step, k), k);
      if (stepped <= toFace) stepped = step;
      stepped = toFace;
    }
    float damped = FoldUnit(req.F(moveword::kFacing) + stepped, k);
    req.SetByte(moveword::kRootFacing, 1);
    req.SetF(moveword::kRootFacingAngle, damped);
    (void)toFace;
    (void)toLook;
    (void)blend;
    (void)lookHead;
  }
  // The exit gate (0x1406c9700..0x1406c9800).
  bool stay = false;
  {
    const int precise = static_cast<int>(req.U(moveword::kPrecise));
    const int cmdClass = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    if ((precise == 0 && cmdClass == 3) ||
        (cmdClass < 1 &&
         std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kFacingError), k)) <
             k.angle225m)) {
      stay = true;
    } else if (quarter <
                   std::fabs(Wrap180Diff(req.F(moveword::kFacing), req.F(moveword::kHeading), k)) &&
               (precise == 1 ||
                (precise == 0 &&
                 rowEndClamped <= clipFrame))) {
      // PES's last arm folds the two precise-flag arms into one test when the row has frames.
      stay = true;
    } else if (precise == 1 && quarter < std::fabs(req.F(moveword::kFacingError)) &&
               rowEndClamped <= clipFrame) {
      // PES's last arm (0x1406C9700): kPrecise == 1 and the facing error past 90 degrees.
      stay = true;
    }
  }
  if (clipFrame < static_cast<int>(firstFrames) && !stay) {
    return;
  }

  // The final grid select (0x1406c9800..0x1406c9bf2).
  AdjustAngleApply(host);
  req.SetByte(moveword::kReorientFlag, 0);
  {
    const int bucket = InterfaceBucket(host, req.F(moveword::kTurn));
    const int liveClass = static_cast<int>(req.U(moveword::kClassWord));
    float wantSpeed = req.F(moveword::kDesiredSpeedKmh);
    const float want = liveClass < 2 ? k.five : k.speedB;
    const int below = 1 < liveClass ? 1 : 0;
    int cls = static_cast<int>(req.U(moveword::kCommandSpeedClass));
    if (cls < 3) {
      if (cls < below) cls = below;
    } else {
      cls = 2;
    }
    int kmh = static_cast<int>(wantSpeed);
    if (kmh < 0x11) {
      if (kmh < static_cast<int>(want)) kmh = static_cast<int>(want);
    } else {
      kmh = 0x10;
    }
    int kind = static_cast<int>(req.U(moveword::kKind));
    if (kind < 2) {
      kind = 2;
    } else if (kind == 4) {
      kind = 5;
    }
    const uint32_t col = static_cast<uint32_t>(k.bucketColumn[bucket & 9]);
    uint32_t row = static_cast<uint32_t>(k.stepRow[(kind + cls * 8) & 63]);
    if (RowIs(col, 0x520u) || RowIs(col, 0u)) {
      if (RowIs(row, 0x546u)) {
        if (host.Percent(0x32)) row = 0x547u;  // FUN_1406ebf30
      } else if (RowIs(row, 0x542u)) {
        if (host.Percent(0x14)) row = 0x543u;  // FUN_1406ebf30
      }
    } else if (RowIs(row, 0x54eu)) {
      row = 0x54fu;
    }
    const RowPick pick = PickRow(host, static_cast<int>(row), static_cast<int>(col));
    const uint16_t repMotion = static_cast<uint16_t>(pick.motion);
    req.SetByte(moveword::kFootFlag, k.bucketLeft[bucket & 9]);  // param_2 + 0x26
    req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));  // param_2[0x24]
    req.SetU(moveword::kPrevSpeedWord, req.U(moveword::kSpeedWord));  // param_2[0x22]
    req.SetU(moveword::kPrevBucket, req.U(moveword::kMoveBucket));    // param_2[0x25] = [0x20]
    req.SetF(moveword::kSpeedWord, static_cast<float>(kmh));  // param_2[0x1d] = (uint)(float)
    req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));       // param_2[0x1f]
    req.SetU(moveword::kMoveBucket, static_cast<uint32_t>(bucket));   // param_2[0x20]
    req.SetF(moveword::kPlayback, PlaybackAt(host, k));  // FUN_1406ba070(P)
    float start = host.MotionStartTime(repMotion);  // FUN_1406b97d0
    if (RowIs(col, 0x520u) || RowIs(col, 0u)) {
      // The foot-band sub-record walk (0x1406c9a00..0x1406c9b40).
      const int footNow = FootBandOf(host);
      const int subCount = host.Motion(repMotion).FootCount();
      if (0 < subCount) {
        for (int i = 0; i < subCount; ++i) {
          int sub = host.Motion(repMotion).FootPair(i);
          if (pick.mirror) {
            if (sub == 0) sub = 2;
            else if (sub == 1) sub = 3;
            else if (sub == 2) sub = 0;
            else if (sub == 3) sub = 1;
          }
          if (footNow == sub) {
            const int back = FrameTicks(host, host.Motion(repMotion).CancelFrames(i), scale, k);
            start = static_cast<float>(back + 1) / static_cast<float>(host.TicksPerSecond());
            break;
          }
        }
      }
    }
    IssueClip(host, repMotion, pick.mirror, k.clipBlend,
              req.F(moveword::kPlayback), start);
    req.SetU(moveword::kPrevKind, static_cast<uint32_t>(kind));  // param_2[0x1e]
    req.SetU(moveword::kDwellElapsed, 0);  // param_2[0x1a]
    req.SetU(moveword::kDwellLimit, 0);    // param_2[0x1b]
    RequestState(req, MoveState::kStepLoop);  // req[0x39] = 4
  }
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
