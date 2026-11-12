// PES 2021's MOVESTEP handler group C: the stepping states STEP_LOOP, STEP_LOOP_CHANGE,
// TURN_STOP, STEP_TO_RUN and RUN_TO_STEP (docs/re/pes-re-anim-select.md 2.1, 5.1-5.2), plus the
// TURN_STOP entry. Each handler is the body of FUN_1406ba900's case, ported statement for
// statement from the annotated decompiles in the RE evidence
// (the gf-AnimSelectRE-2 scratch's mv5.c for the five bodies and mv4.c for the TURN_STOP entry
// FUN_1406d2590) and verified against the raw instruction stream: each install scalar's VA is the target of the instruction the contract's
// loader table lists (movestep.hpp).
//
// PES's helpers are shared movestep helpers or this file's own ports (each with the VA it was
// decompiled from); PES's per-player and match reads are MoveHost methods; PES's request block
// is MoveReq. Where a PES read has no GF counterpart yet, a `// ponytail:` note names the PES
// site that replaces the stand-in.

#include "pes/movestep_step.hpp"
#include "pes/movestep_turn.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_issue.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pes {
namespace movestep {
namespace {

// The keep-tick tail of STEP_LOOP (PES's LAB_1406c5b79 fall-through), defined after StepLoop.
void StepLoopKeepsTick(MoveHost& host, MoveReq& req);
void StepLoopTurnRunArm(MoveHost& host, MoveReq& req, int bucket);

// FUN_14043a0a0(a, b): the difference of two angles folded into (-180, 180].
float Wrap180(float delta) {
  while (delta > 180.0f) delta -= 360.0f;
  while (delta <= -180.0f) delta += 360.0f;
  return delta;
}

// FUN_1406b8da0(a): the fold onto (-180, 180], PES's own three-step version.
float FoldHalfTurn(float a) {
  if (a < -180.0f) return a + 360.0f;
  if (180.0f < a) a = a - 360.0f;
  return a;
}

// FUN_140470d60(a, out, delta): `a` folded into [0, 360) (one step), plus the delta, folded again (one step).
float AddAngle360(const StepScalars& k, float a, float delta) {
  const auto fold = [&k](float x) {
    if (0.0f <= x) return k.full <= x ? x + k.wrapFull : x;
    return x + k.full;
  };
  return fold(fold(a) + delta);
}

// FUN_140a92d40(a, b): the atan2 difference in (-pi, pi], then * 2pi by the caller.
float Atan2Delta(float dx, float dz) { return std::atan2(dx, dz); }

// FUN_14046f230 of FUN_141eb50b0(P, out, 1): the heading in [0, 360) of the last tick's displacement, PES's (x, z) pair
// (d[0] and d[2]). DAT_1425795e4 is 2 pi, DAT_14259be40 is 180/pi.
float DisplacementHeading(MoveHost& host) {
  float d[3];
  host.PositionAt(1, d);
  float a = Atan2Delta(d[0], d[2]) * float(180.0 / 3.14159265358979323846);
  if (a < 0.0f) a += 360.0f;
  return a;
}

// FUN_140439e50(pos, sample): the heading in [0, 360) of the vector from `pos` to `sample`.
// Both are [x, y, z]; PES uses (x, z), so the second component is the last one.
float HeadingFrom(float x, float z, float toX, float toZ) {
  float a = Atan2Delta(toX - x, toZ - z) * float(180.0 / 3.14159265358979323846);
  if (a < 0.0f) a += 360.0f;
  return a;
}

// FUN_141e532d0: the frame rate as a float, 54.0 (image bytes 0x42580000; docs/re/pes-re-A6Writers.md). Every handler in this file
// multiplies or divides by it exactly as PES does: ground speed in km/h is `fps * 60 * 60 / 1000 * |metres per tick|`, a dwell
// is `fps * seconds`, a per-tick angle is `degrees / fps`. (The port once read it as 1/54 "the logic clock draw": every ground
// speed came out ~2,900 times too small, so the step loop saw a running man as standing.)
float FramesPerSecond(MoveHost& host) { return host.TicksPerSecond(); }

// DAT_14259be40 (57.2957..): FUN_14046f260's radians-to-degrees scale.
constexpr float kDegToRad = 57.2957763671875f;

// STEP_LOOP_CHANGE's rate immediates, each from the instruction that stores it into req[0x4a] (opcode-checked, as the owner
// rules allow): 0x3f19999a = 0.6, 0x3e99999a = 0.3 (not 0.45, which is 0x3ee66666), 0x3e19999a = 0.15.
constexpr float kRateHigh = 0.6000000238418579f;  // STEP_LOOP_CHANGE 0x1406c66f9
constexpr float kRateMid = 0.30000001192092896f;  // STEP_LOOP_CHANGE 0x1406c676e
constexpr float kRateLow = 0.15000000596046448f;  // STEP_LOOP_CHANGE 0x1406c675c
// The first dwell's limit when STEP_LOOP_CHANGE hands back to STEP_LOOP before the second dwell is 0.4 spent: the immediate
// 0x3dcccccd (0.1) at 0x1406c6f08, not 0.05 (0x3d4ccccd).
constexpr float kRestartDwell = 0.10000000149011612f;
// TURN_STOP's third path remaps the step grid's row 0x52e to 0x530: the immediates at 0x1406cb397 (CMP EDI,0x52e) and
// 0x1406cb38e (MOV EAX,0x530).
constexpr int kStepRowRemapFrom = 0x52e;
constexpr int kStepRowRemapTo = 0x530;

// STEP_LOOP's STEP_TO_RUN arm blend, the immediate PES stores into req[0x1b] and req[0x32]
// (0x3e888889), opcode-checked as the owner rules allow.
constexpr float kBlend0266 = 0.2666666805744171f;  // STEP_LOOP 0x1406c5880 / 0x1406c5915

// FUN_1406b9ff0(interface, class, speed, kind): the rate for a class/speed pair. req[0xb] is the
// "use the class rate" flag; the rate comes from FUN_1406b9e80's 10-knot table, which the
// contract exposes as SpeedToRate with the same argument order.
float RateFor(MoveHost& host, bool useClassRate, float speedKmh, int speedClass, int kind) {
  // FUN_1406b9ff0 (inlined at 0x1406c10b0): with the flag, vt+0x38 = FUN_1406b9e50(class, kind); else a class 0 at kind 3 or
  // 6 has no rate; else FUN_1406b9e80(speed, kind).
  if (useClassRate) return BandRateFor(speedClass, kind, host.RunTablesView());
  if (speedClass == 0 && (kind == 3 || kind == 6)) return 0.0f;
  return SpeedToRate(host, speedKmh, kind);
}

}  // namespace

// FUN_1406b6ee0(ap, req, live): whether the step loop must stop. STEP_LOOP passes its live flag and STOP passes 0. The
// class word (req[0x1f]) and the command's speed class (req[9]) decide first. A live step then compares the bearing from the
// player to the move target (req[0x1a6], req[0x1a8]) with the last displacement's heading, and holds until the second dwell
// (req[0x2d] reaches req[0x2e]). The move bucket (req[8]) picks the rule; the class picks the look gate: blend2 (VA
// 0x142598578) or blend04 (0x14259b89c) below class 1, blend04 or rate07 (0x14257a194) from class 1 on.
bool StepStops(MoveHost& host, MoveReq& req, bool live) {
  const RunConstants& r = host.RunConstantsView();
  const int cls = int(req.U(moveword::kClassWord));
  if (cls >= 0 && int(req.U(moveword::kCommandSpeedClass)) == -1) return true;
  if (live) {
    if (r.deg45 < std::fabs(req.F(moveword::kTurn))) {
      float self[3] = {0.0f, 0.0f, 0.0f};
      host.OwnPosition(self);
      const float bearing =
          HeadingFrom(self[0], self[2], req.F(moveword::kMoveTargetX), req.F(moveword::kMoveTargetZ));
      if (r.deg675 < std::fabs(Wrap180(bearing - DisplacementHeading(host)))) return true;
    }
    if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2)) return false;
  }
  switch (int(req.U(moveword::kMoveBucket))) {
    case 1:
    case 6: {
      const float gate = cls < 1 ? r.blend2 : r.blend04;
      if (r.deg1125 < std::fabs(req.F(moveword::kTurn))) return !(gate <= req.F(moveword::kDistToLook));
      return false;
    }
    case 2:
    case 7: {
      const float gate = cls > 0 ? r.rate07 : r.blend04;
      if (gate <= req.F(moveword::kDistToLook)) return r.deg90 < std::fabs(req.F(moveword::kFacingError));
      return true;
    }
    case 0:
    case 5:
      return false;
    default:
      return true;
  }
}

// FUN_1406b70d0(ap, req): whether the step loop may turn into a run turn. The flag byte (req[0xf]) and the precise word
// (req[0xe]) gate it with the class; then the move bucket (req[8]) picks the rule. Buckets 1 and 6 test the class's front
// distance plus a scale that is 0 up to 33.75 degrees, rises to 1 at 67.5 and stays 1 past it (PES starts the scale at
// 1.0). Buckets 2 and 7 test the class's back distance, scaled between 67.5 and 112.5 degrees (full past 112.5) against
// the centre, and then whether the turn is past 101.25 degrees (VA 0x1425ddfac).
// ponytail: the carrier record and the recent ball event (FUN_140a627e0, FUN_140a48a10, FUN_140a3e190) are not in GF, so
// the carrier branch and its held row (0x1425ddea0) are absent.
bool StepLetsRunTurn(MoveHost& host, MoveReq& req) {
  const RunConstants& r = host.RunConstantsView();
  const MoveConstants& m = host.MoveConstantsView();
  const MoveTurnConstants& t = host.MoveTurnConstantsView();
  const StepTables& tables = host.StepTablesView();
  const int cmdClass = int(req.U(moveword::kCommandSpeedClass));
  if (cmdClass == -1) return true;
  const int cls = int(req.U(moveword::kClassWord));
  const int row = std::clamp(cls, 0, 3);  // ponytail: a class word outside 0..3 reads row 0, not the word before the table
  const int move = int(req.U(moveword::kMoveBucket));
  const float turn = std::fabs(req.F(moveword::kTurn));
  const float toTarget = req.F(moveword::kDistToTarget);
  const float toLook = req.F(moveword::kDistToLook);
  const uint32_t flags = req.U(moveword::kTurnGateFlags) & 0xffu;
  if ((flags & 0xfdu) != 0 && ((int(req.U(moveword::kPrecise)) != 1 || cls > 2) && cls > 1)) {
    if (move == 1 || move == 6) {
      if (flags != 3) return !(toTarget <= m.dist2m5 || turn <= r.deg5625 || cls > 2);
      return false;
    }
    return move != 0 && move != 5;
  }
  switch (move) {
    case 1:
    case 6: {
      float scale = 0.0f;
      if (r.deg3375 < turn) scale = turn < r.deg675 ? (turn - r.deg3375) / r.deg3375 : 1.0f;
      const float gate = tables.turnFront[row] + scale;
      if (toTarget < gate && toLook < gate) return req.Flag(moveword::kTurnCommandFlag);
      return false;
    }
    case 0:
    case 5:
      return false;
    case 2:
    case 7: {
      float scale = 0.0f;
      if (r.deg675 < turn) scale = turn < r.deg1125b ? (turn - r.deg675) / r.deg45 : 1.0f;
      const float back = tables.turnBack[row];
      const float gate = back + (back - tables.turnCentre[row]) * scale;
      const bool moreThanOne = cls > 1 || req.F(moveword::kRate) >= r.blend04 || turn <= r.deg90;
      if ((gate <= toTarget || gate <= toLook) && (cmdClass < 3 || moreThanOne) && cmdClass > 0) {
        return t.turn101250 < turn;
      }
      return true;
    }
    default:
      return true;
  }
}

// -------------------------------------------------------------------------------------------------
// RUN_TO_STEP, FUN_1406c1070 (0x1406c1070..0x1406c1265).
//
//   1. The blended heading so far, req[0x17] / (clock * req[0x2e]).
//   2. The rate target: with req[0xb] the class rate (the interface's), else 0 for kind 3/6 at
//      class 0, else FUN_1406b9e80's for the desired speed. req[0x4a] walks toward it by 0.05
//      per tick, clamped to [0, rate ceiling].
//   3. The heading error is FUN_14043a0a0(req[0x18], req[1]): req[1] minus req[0x18]. The body error is the wrapped
//      difference between req[1] and the body direction, which is req[0x4] except while
//      req[0x2d] < req[0x2e] * 0.25, when the heading of the last tick's displacement is used.
//   4. With the class -1, or either error past its gate (2025 and 12656.25 squared degrees), the
//      handler blends the root facing (req[0x3a]/req[0x3b]) and, once the second dwell has run
//      out, asks for STEP_LOOP.
//   5. Otherwise it writes the remaining dwell, restores the previous band when it is nearly
//      out, and exits to STOP / TURN_STOP / RUN_TURN_RUN from the body turn.
// -------------------------------------------------------------------------------------------------
void RunToStep(MoveHost& host, MoveReq& req) {
  const StepScalars& k = host.StepConstants();
  const float fps = FramesPerSecond(host);
  float blended = 0.0f;
  if (fps * req.F(moveword::kDwellLimit2) != 0.0f)
    blended = req.F(moveword::kRecordStampA) / (fps * req.F(moveword::kDwellLimit2));

  // 2. The rate target and the walk toward it.
  const float target = RateFor(host, req.Flag(moveword::kUseClassRate),
                               req.F(moveword::kDesiredSpeedKmh),
                               int(req.U(moveword::kCommandSpeedClass)),
                               int(req.U(moveword::kKind)));
  // 0x1406c10b0..: on the rise the rate moves by 0.05 and stops at the target (`fVar5` holds the target unless the step fits,
  // and 0 only when the step is below 0); on the fall the step stops at the rate ceiling, and the target is the floor.
  const float rate = req.F(moveword::kRate);
  if (rate <= target) {
    const float raised = rate + k.step0p05;
    float next = target;
    if (raised <= target) next = 0.0f <= raised ? raised : 0.0f;
    req.SetF(moveword::kRate, next);
  } else {
    const float lowered = rate - k.step0p05;
    float next = host.RateCeiling();
    if (lowered <= next) next = target <= lowered ? lowered : target;
    req.SetF(moveword::kRate, next);
  }

  const float headingError = Wrap180(req.F(moveword::kHeading) - req.F(moveword::kRecordStampB));

  // 3. The body direction: the request's, or the displacement heading while the dwell is
  //    nearly out.
  //    PES at 0x1406C11A2: the displacement heading is KEPT when 0.25 * limit <= elapsed, and
  //    the request's own req[2] takes over otherwise.
  float body = req.F(moveword::kFacing);
  if (req.F(moveword::kDwellLimit2) * k.s0p25 <= req.F(moveword::kDwellElapsed2))
    body = DisplacementHeading(host);
  const float bodyError = Wrap180(Wrap180(req.F(moveword::kHeading)) - Wrap180(body));

  // PES's branch at 0x1406C1232: the class -1, a heading error past 8100 deg^2 (0x1425DDFD8) or
  // a body error past 12656.25 deg^2 (0x1425DDFDC) goes to the stop arm; only a tick inside
  // all three gates blends the root facing (0x1406C1261).
  if (int(req.U(moveword::kCommandSpeedClass)) == -1 || k.sq8100 < headingError * headingError ||
      k.sq12656p25 < bodyError * bodyError) {
    // 5. The remaining dwell, the restored band, and the three exits.
    req.SetF(moveword::kRecordStampA, blended);
    const float dwellLeft = req.F(moveword::kDwellLimit2) - req.F(moveword::kDwellElapsed2);
    req.SetF(moveword::kRecordStampB, dwellLeft <= 0.0f ? 0.0f : dwellLeft);
    // PES at 0x1406C13FC: the window is 0.3 (0x142590D98) when the band is below the previous
    // band, and 0.7 (0x14257A194) otherwise.
    const float scale =
        int(req.U(moveword::kClassWord)) < int(req.U(moveword::kPrevClassWord)) ? k.s0p3 : k.s0p7;
    if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2) * scale) {
      // PES: `req[0x1f] = req[0x24]; req[0x74 word] = req[0x88 word];` — the band and its speed come
      // back from the previous ones (byte offsets 0x7c/0x88 inside the words 0x1f and 0x22).
      req.SetU(moveword::kClassWord, req.U(moveword::kPrevClassWord));
      req.SetF(moveword::kBandSpeed, req.F(moveword::kBandSpeedCopy));
    }
    // PES at 0x1406C142A: FUN_14043a0a0(P+0x20, req[0x04]) = wrap180(the facing - the body
    // direction). The port read the look heading (req[0x03]) instead of the body direction.
    const float bodyTurn = Wrap180(req.F(moveword::kFacing) - req.F(moveword::kBodyDirection));
    if (k.deg22p5 <= std::fabs(bodyTurn) ||
        (int(req.U(moveword::kClassWord)) < 1 &&
         int(req.U(moveword::kCommandSpeedClass)) == -1)) {
      EnterStop(host, req);
    } else if (int(req.U(moveword::kCommandSpeedClass)) == -1 ||
               req.F(moveword::kDistToTarget) < k.one) {
      EnterTurnStop(host, req);
    } else {
      EnterRunTurnRun(host, req);
    }
    return;
  }

  {
    // 4. The blend branch: the wrapped facing, drifted by the blended turn so far.
    const float facing = req.F(moveword::kFacing);
    float drift = facing + blended;
    if (0.0f <= drift) {
      if (k.full <= drift) drift += k.wrapFull;
    } else {
      drift += k.full;
    }
    req.SetF(moveword::kRootFacingAngle, drift);
    req.SetByte(moveword::kRootFacing, 1);
    // 0x1406c1070 (decompile lines 131-147): the second dwell past 0.7 of its limit (DAT_14257a194) and a heading error past 31.64 deg^2.
    if (req.F(moveword::kDwellLimit2) * k.s0p7 <= req.F(moveword::kDwellElapsed2) &&
        k.deg31p64 < headingError * headingError) {
      // The drift is clamped to what the clock can cover (22.5 / 54 degrees): fVar5 = fVar6 (the limit) above it, -limit below
      // -limit, the error itself between; FUN_140470d60 keeps the shortest-way side of the wrapped heading.
      const float limit = k.deg22p5 / fps;
      float delta = limit;
      if (headingError <= limit) {
        delta = -limit;
        if (-limit <= headingError) delta = headingError;
      }
      req.SetF(moveword::kRootFacingAngle, AddAngle360(k, drift, delta));
      req.SetByte(moveword::kRootFacing, 1);
    }
    // PES's exit gate at 0x1406C1343: the second dwell run out, or the dwell past three
    // quarters (0x14259BDD4) with a heading error past 1139.0625 deg^2 (0x1425DDFD4), the
    // precise flag set and the command speed class below the live class word.
    // 0x1406c1343: the second dwell run out, or (from 0.75 of it) a heading error past 1139.0625 deg^2 or the precise flag set
    // with the command class below the live class word.
    const float elapsed2 = req.F(moveword::kDwellElapsed2);
    const float limit2 = req.F(moveword::kDwellLimit2);
    if (limit2 <= elapsed2 ||
        (limit2 * k.threeQuarters <= elapsed2 &&
         (k.sq1139 < headingError * headingError ||
          (req.Flag(moveword::kPrecise) && int(req.U(moveword::kCommandSpeedClass)) < int(req.U(moveword::kClassWord)))))) {
      req.SetU(moveword::kDwellElapsed, 0);
      req.SetU(moveword::kDwellLimit, 0);
      req.SetU(moveword::kDwellElapsed2, 0);
      req.SetU(moveword::kDwellLimit2, 0);
      req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
      RequestState(req, MoveState::kStepLoop);
      req.SetByte(moveword::kTurnLockFlag, 0);  // 0x1406c13fc..: word 0x3c, not the motion-pending byte
      // ponytail: P+0x4d0 vt+0x08 (AdjustAngleApply), no host read yet.
    }
    return;
  }
}

// -------------------------------------------------------------------------------------------------
// STEP_TO_RUN, FUN_1406c6f80 (0x1406c6f80..0x1406c79c0).
//
//   1. The blended heading so far, req[0x17] / (clock * req[0x1b]).
//   2. The request's remaining heading (req[0x18] - req[1]) and the player's own facing-turn
//      (req[2] - req[1]) decide: with the class -1 and precise, or both headings past their
//      gates (8100 and 2025 squared degrees), the handler stops first (FUN_1406d5d60);
//      otherwise it blends.
//   3. Inside the blend branch: a heading error below 11.25 deg hands over to RUN_TURN_RUN
//      (FUN_1406d6780) or RUN_TURN_STEP (FUN_1406d2f60, the ceiling term), with the band from
//      the speed of the last tick's displacement; the class below 0 with speed class 1 stops.
//   4. The blend writes the wrapped heading into req[0x3b] and adds the delta to req[0x18].
//   5. When req[0x2d] reaches req[0x2e] the handler asks for RUN_LOOP.
// -------------------------------------------------------------------------------------------------
void StepToRun(MoveHost& host, MoveReq& req) {
  const StepScalars& k = host.StepConstants();
  const float fps = FramesPerSecond(host);
  float blended = 0.0f;
  if (fps * req.F(moveword::kDwellLimit) != 0.0f)
    blended = req.F(moveword::kRecordStampA) / (fps * req.F(moveword::kDwellLimit));

  const float heading = req.F(moveword::kHeading);
  // PES at 0x1406C6FEC and 0x1406C7000: FUN_14043a0a0(stamp, heading) and
  // FUN_14043a0a0(facing, heading), i.e. heading - stamp and heading - facing.
  const float remaining = Wrap180(heading - req.F(moveword::kRecordStampB));
  const float facingTurn = Wrap180(heading - req.F(moveword::kFacing));
  const int speedClass = int(req.U(moveword::kCommandSpeedClass));

  // PES's stop gate at 0x1406C7005: the class -1 with the precise flag set (req+0x38 is
  // word 0x0E), or both the remaining heading past 8100 deg^2 (0x1425DDFD8) and the facing
  // turn past 2025 deg^2 (0x1425CB494).
  if ((speedClass == -1 && req.Flag(moveword::kPrecise)) ||
      (k.sq8100 < remaining * remaining && k.sq2025 < facingTurn * facingTurn)) {
    // 3. A small heading error hands over to a run-turn state instead of stopping.
    if (speedClass != -1) {
      if (std::fabs(Wrap180(req.F(moveword::kFacing) - DisplacementHeading(host))) <
          k.deg11p25) {
        // The speed of the last tick's displacement decides the band, in km/h: the frame speed
        // (60 * 60 / 1000) times the displacement length.
        float d[3];
        host.PositionAt(0, d);
        const float speed = (fps * k.ticks60 * k.ticks60) / k.scale1000 *
                            std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (k.kmh5 <= speed) {
          if (k.kmh13p5 <= speed) {
            req.SetU(moveword::kClassWord, 2);
            req.SetF(moveword::kBandSpeed, k.kmh16);
          } else {
            req.SetU(moveword::kClassWord, 1);
            req.SetF(moveword::kBandSpeed, k.kmh11);
          }
        } else {
          req.SetU(moveword::kClassWord, 0);
          req.SetF(moveword::kBandSpeed, k.kmh5);
        }
        if (!req.Flag(moveword::kPrecise)) {
          EnterRunTurnRun(host, req);
          return;
        }
        // FUN_1406d2f60's threshold: (rate ceiling - 0.6) * 0.5.
        if (EnterRunTurnStep(host, req, (host.RateCeiling() - k.s0p6) * k.half)) return;
      }
    }
    req.SetF(moveword::kRecordStampA, blended);
    const float dwellLeft = req.F(moveword::kDwellLimit) - req.F(moveword::kDwellElapsed);
    req.SetF(moveword::kRecordStampB, dwellLeft <= 0.0f ? 0.0f : dwellLeft);
    EnterStop(host, req);
    return;
  }

  if (1 < speedClass && !req.Flag(moveword::kPrecise) && int(req.U(moveword::kClassWord)) < 1) {
    // 0x1406c7060: a class 3 or 4 request whose body error and look delta are both inside 22.5 degrees (absolute values) may
    // accelerate; otherwise the handler re-plans through FUN_1406ce820.
    const float bodyTurn = Wrap180(req.F(moveword::kFacing) - req.F(moveword::kBodyDirection));
    const float lookDelta = req.F(moveword::kLookDelta);
    if (2 < speedClass && std::fabs(bodyTurn) < k.deg22p5 && std::fabs(lookDelta) < k.deg22p5 &&
        EnterAccelerate(host, req))
      return;
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }

  // 4. The blend: PES adds the blended turn to the facing (*(P+0x20), 0x1406c70ba) and folds it into [0, 360).
  float drift = req.F(moveword::kFacing) + blended;
  if (0.0f <= drift) {
    if (k.full <= drift) drift += k.wrapFull;
  } else {
    drift += k.full;
  }
  req.SetF(moveword::kRootFacingAngle, drift);
  req.SetByte(moveword::kRootFacing, 1);

  // The drift gate: the clock covers 270 / the frame rate degrees a tick; below that the heading error is split over the
  // remaining dwell and clamped; FUN_140470d60 adds the delta to the drift and folds it into [0, 360).
  const float draw = k.deg270 / fps;
  if (blended * blended < draw * draw && k.deg31p64 < remaining * remaining) {
    const float span = fps * (req.F(moveword::kDwellLimit2) - req.F(moveword::kDwellElapsed2));
    float error = remaining;
    if (span != 0.0f) error = remaining / span;
    // 0x1406c7185: the bound is draw - |blended|; the clamp is the bound above it, minus the bound below it, the error between.
    const float bound = draw - std::fabs(blended);
    float delta = bound;
    if (error <= bound) {
      delta = -bound;
      if (-bound <= error) delta = error;
    }
    req.SetF(moveword::kRootFacingAngle, AddAngle360(k, drift, delta));
    req.SetByte(moveword::kRootFacing, 1);
    req.SetF(moveword::kRecordStampB, req.F(moveword::kRecordStampB) + delta);
  }


  if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2)) return;
  // 0x1406c7978..: the two dwell pairs clear, req[0x20] (not the move bucket req[8]) clears, the live kind is kept, RUN_LOOP
  // is asked for and the turn-lock byte (word 0x3c) clears; the adjuster apply call (P+0x4d0 vt+0x08) is AdjustAngleApply.
  req.SetU(moveword::kDwellElapsed, 0);
  req.SetU(moveword::kDwellLimit, 0);
  req.SetU(moveword::kDwellElapsed2, 0);
  req.SetU(moveword::kDwellLimit2, 0);
  req.SetU(moveword::kLiveBucket, 0);
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  RequestState(req, MoveState::kRunLoop);
  req.SetByte(moveword::kTurnLockFlag, 0);
  // ponytail: P+0x4d0 vt+0x08 (the AdjustAngle adjuster's apply call, AdjustAngleApply in movestep_turn.cpp) has no host read yet.
}

// The band the step loop reads (FUN_1406c4c10 0x1406C4DDD-0x1406C4DEE, from the image bytes): one step from the live class
// toward the command class E, which is the request's command class clamped to 0..2 (the clamp at 0x1406C4D6C-0x1406C4D7A, see
// WalkCommandClass): live + 1 when E is above the live class, live - 1 when E is below it, and the live class when they are
// equal. The step table is read with the result, which stays in 0..3 for a live class of -1 or 0..3.
int StepBand(int liveBand, int commandBand) {
  if (commandBand > liveBand) return liveBand + 1;
  if (commandBand < liveBand) return liveBand - 1;
  return liveBand;
}

// FUN_1406c4c10 lines 86-103: the command class the walk compares with. Below 3 it is the value, clamped at 0 from below, and
// a class of 3 or more reads as 2 (decompile line 89). Under the special condition (the previous state is 0x13 or 0x10, and
// req[0x1a] is at or below the install's bound and not equal to it) the class word is read the same way instead.
int WalkCommandClass(int commandClass, int classWord, int prevState, float dwellElapsed, float dwellBound) {
  const bool special =
      (prevState == 0x13 || prevState == 0x10) && dwellElapsed <= dwellBound && dwellBound != dwellElapsed;
  const int value = special ? classWord : commandClass;
  if (value >= 3) return 2;
  return value < 0 ? 0 : value;
}

// -------------------------------------------------------------------------------------------------
// STEP_LOOP, FUN_1406c4c10 (0x1406c4c10..0x1406c64f7, 901 assembly lines).
//
//   1. The band walks down one step when it is 0, the previous band was 1, and the second dwell
//      has passed its threshold (0.6 s, or 0.4 s at kind 7).
//   2. The band of the tick: the command class clamped to 0..2, or the live band when the
//      previous state was STEP_TURN_STEP (19) / RUN_TO_STEP (14) and the first dwell is at or
//      below 0.1 s. Then the band steps one toward the live band.
//   3. The blended band speed is `speed * 0.5 + bandSpeed * 0.5`, with the desired speed clamped
//      into [5, 16] km/h; the ground speed is the last tick's displacement scaled by the clock.
//   4. A slow player with the arrival flag (req[0x3f]) re-plans through FUN_1406ce820.
//   5. FUN_1406d38d0 (STEP_TURN_RUN) may take the tick; the turn gates re-plan; FUN_1406b6ee0
//      stops.
//   6. The bucket follows the body turn; a turn past 8100 deg^2 lowers the band to 1.
//   7. The bucket, the column and the row from the install tables, the drift from the look
//      heading (or the sampled ball position when the look is behind), and the step-loop
//      request: motion, mirror, blend, playback, start time, and the state STEP_TO_RUN,
//      STEP_LOOP_CHANGE or (through FUN_1406b70d0) RUN_TURN_RUN.
// -------------------------------------------------------------------------------------------------
void StepLoop(MoveHost& host, MoveReq& req) {
  const StepScalars& k = host.StepConstants();
  const StepTables& tables = host.StepTablesView();
  const int kind = int(req.U(moveword::kKind));
  const bool precise = req.Flag(moveword::kPrecise);
  const float facing = req.F(moveword::kFacing);
  const float fps = FramesPerSecond(host);

  // 1. The band walk: `if (req[0x1f] == 0 && req[0x24] == 1 && 0 < req[0x2e])` in PES.
  bool localRes18 = true;  // d_1406c4c10.c 61-76: cleared when the band walk's gate holds
  if (int(req.U(moveword::kClassWord)) == 0 && int(req.U(moveword::kPrevClassWord)) == 1 &&
      0.0f < req.F(moveword::kDwellLimit2)) {
    localRes18 = false;
    const float threshold = kind == 7 ? k.s0p4 : k.s0p6;
    if (req.F(moveword::kDwellLimit2) * threshold < req.F(moveword::kDwellElapsed2)) {
      req.SetF(moveword::kBandSpeed, req.F(moveword::kBandSpeedCopy));
      req.SetU(moveword::kClassWord, 1);
      req.SetF(moveword::kDwellLimit2, 0.0f);
    }
  }

  // PES's two separate flags, which the decompile keeps in one stack slot:
  //   local_res18[0] low byte = 1, and is cleared (set to `<< 8`) when the band walk applied.
  //     STEP_LOOP tests it at the end of the prologue's bucket section (`if (local_res18[0] == 0)`)
  //     and it gates the band restore in the tail.
  //   uVar11 = 1 when the band is below 1 at kind 3 or 6, the class is below 2 and the previous
  //     band matches the live one; else 0. It is FUN_1406b6ee0's third argument.
  const bool liveArg = int(req.U(moveword::kClassWord)) < 1 && (kind == 3 || kind == 6) &&
                       int(req.U(moveword::kCommandSpeedClass)) < 2 &&
                       int(req.U(moveword::kPrevClassWord)) == int(req.U(moveword::kClassWord));

  // 2. The band of the tick and the blended band speed. The command class and the band walk are PES's (FUN_1406c4c10 lines
  // 86-103 and 0x1406C4DCC-0x1406C4DEE); the bound is DAT_142588200, which the install gives as frameGate01 (0.1).
  const int classBand = WalkCommandClass(int(req.U(moveword::kCommandSpeedClass)), int(req.U(moveword::kClassWord)),
                                         host.MoveStatePrevious(), req.F(moveword::kDwellElapsed),
                                         host.RunConstantsView().frameGate01);
  const float desired = req.F(moveword::kDesiredSpeedKmh);
  float speed = k.kmh16;
  if (desired <= k.kmh16 && k.kmh5 <= desired) speed = desired;
  // PES reads `param_2[0x1f]` at each use, so the band walk above is visible here.
  const int liveBand = int(req.U(moveword::kClassWord));
  int nextBand = StepBand(liveBand, classBand);

  // 3. The blended band speed and the ground speed.
  float blendedSpeed = speed * k.half + req.F(moveword::kBandSpeed) * k.half;
  float ground[3];
  host.PositionAt(1, ground);
  const float groundSpeed = (fps * k.ticks60 * k.ticks60) / k.scale1000 *
                            std::sqrt(ground[0] * ground[0] + ground[1] * ground[1] +
                                      ground[2] * ground[2]);

  // 4. The slow-player re-plan.
  // 0x1406c4d.. `cmp byte ptr [req+0x3f]`: the BYTE at request offset 0x3f (the command's flag byte), not word 0x3f.
  if (req.FlagAtByte(0x3f) && liveBand < 3 && !precise && groundSpeed < k.kmh16) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }

  // 5. The turn into a run, and the stop.
  if (EnterStepTurnRun(host, req)) return;
  const float turnToTarget = Wrap180(req.F(moveword::kLookHeading) - req.F(moveword::kHeading));
  // FUN_1406D6290 (the TIMING_STEP entry, req[0x39] = 0x16) is called by PES at 0x1406C4E51,
  // after FUN_1406D38D0 declines. Its rule chain (TeamDecode's decode, §8 of
  // local://pes-re-step-family.md) needs two things GF does not carry: the FORMATION's home
  // position and home angle (rules 12 and 13 - the distance window around the side's own
  // home x, and the angle gate), which the match has no counterpart for. The team possession
  // latch (rules 2 and 11) IS available - Team::IsPesPossession() - and the adapter's
  // TeamAi::PossessionLatched is where it would be read.
  // ponytail: port FUN_1406D6290 when the formation's home position and home angle reach GF
  // (the two rules that block it), and read the latch through TeamAi::PossessionLatched.
  if (liveBand < 2 &&
      ((k.deg33p75 < std::fabs(req.F(moveword::kTurn)) ||
        (liveBand < 1 && k.deg22p5 < std::fabs(req.F(moveword::kLookDelta)) &&
         std::fabs(req.F(moveword::kLookDelta)) < k.deg135)) &&
       1 < int(req.U(moveword::kCommandSpeedClass)) && !precise)) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }
  // FUN_1406b6ee0: whether the step loop must stop (its port is the helper above).
  if (StepStops(host, req, liveArg)) {
    if (liveBand == 1 && groundSpeed < k.kmh5) {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kBandSpeed, k.kmh5);
    }
    EnterStop(host, req);
    return;
  }

  // 6. The bucket follows the body turn; a wide turn lowers the band to 1.
  // PES keeps three flags the keep test reads (d_1406c4c10.c 505): local_res8 ([RSP+0x170], the body turn is wide: set when its
  // square passes DAT_1425ddfd4), bVar20 ([RSP+0x58], the bucket was re-picked) and bVar19 (the wide turn lowered the band).
  bool bucketMoved = false;  // bVar20
  bool driftTaken = false;   // local_res8
  bool slowTurn = false;
  const float bucketAngle = tables.bucketDegrees[int(req.U(moveword::kMoveBucket))];
  float turned = bucketAngle + facing;
  if (0.0f <= turned) {
    if (k.full <= turned) turned += k.wrapFull;
  } else {
    turned += k.full;
  }
  // PES at 0x1406C4FB9: FUN_14043a0a0(turned, req[0x01]) = wrap180(kHeading - turned), and
  // the clamp at 0x1406C4FBE-0x1406C4FD8 holds it inside [-90, 90].
  float bodyTurn = Wrap180(req.F(moveword::kHeading) - turned);
  if (k.deg90 < bodyTurn) {
    bodyTurn = k.deg90;
  } else if (bodyTurn < k.neg90) {
    bodyTurn = k.neg90;
  }
  if (k.sq1139 < bodyTurn * bodyTurn) {
    driftTaken = true;
    if (k.sq8100 <= bodyTurn * bodyTurn && 1 < nextBand) {
      const int lower = nextBand - 1;
      nextBand = 1;
      if (lower < 2) nextBand = std::max(lower, 0);
      if (req.F(moveword::kBandSpeed) <= k.kmh11) {
        blendedSpeed = k.kmh5;
        if (k.kmh5 <= req.F(moveword::kBandSpeed)) blendedSpeed = req.F(moveword::kBandSpeed);
      } else {
        blendedSpeed = k.kmh11;
      }
      slowTurn = true;
    }
  } else {
    bodyTurn = 0.0f;
  }

  // 7. The bucket, the column and the row.
  // FUN_1406b8da0 folds the turn + the bucket angle, then the interface's bucket function
  // (vt+0x60, 10 buckets) answers. The contract's AngleBucket is the same function with the
  // same tables, so the port calls it with the current bucket for its hysteresis.
  int bucket = AngleBucket(host.AngleBuckets(), bucketAngle + bodyTurn,
                           int(req.U(moveword::kMoveBucket)), false);
  const int currentBucket = int(req.U(moveword::kMoveBucket));
  // PES at 0x1406C4E7A: FUN_14043a0a0(req[0x03], req[0x01]) = wrap180(kHeading - kLookHeading).
  const float turnToTargetDeg = Wrap180(req.F(moveword::kHeading) - req.F(moveword::kLookHeading));
  const bool turnIsWide = k.deg5p625 <= turnToTargetDeg;
  if (bodyTurn <= 0.0f) {
    if (currentBucket == 0 || currentBucket == 5) {
      bucket = currentBucket;
      if (turnIsWide) {
        bucket = turnToTargetDeg < 0.0f ? 5 : 0;
        bucketMoved = bucket != currentBucket;
      }
    } else if ((currentBucket == 4 || currentBucket == 9) && bucket == currentBucket &&
               std::fabs(turnToTargetDeg) <= k.deg174p375) {
      bucket = turnToTargetDeg < 0.0f ? 9 : 4;
      if (bucket != currentBucket) bucketMoved = true;
    }
  }

  // The heading the step's own angle points at, and the drift from the look heading.
  // PES at 0x1406C5FD4: the inline difference is (req[0x01] - the column's angle) - the facing,
  // and the call at 0x1406C5FE0 is FUN_14043a0a0(facing, that).
  const float lookAhead =
      Wrap180(req.F(moveword::kHeading) - tables.bucketDegrees[bucket] - facing);
  const float lookDelta = req.F(moveword::kLookDelta);
  float drift = 0.0f;
  if (!precise) {
    const float lookError = req.F(moveword::kFacingError);
    if (k.deg175 < std::fabs(lookDelta)) {
      if (lookError <= k.deg11p25 || lookError <= k.deg168p75) {
        // A 20-point sample on the pitch decides the drift. ponytail: PES samples the match's
        // ball-position list (ctx+0x40 through FUN_140a6cf70); GF has no such list, so the
        // sample stands in as the player's own displacement heading until the ball history
        // lands. PES: FUN_140439e50(player, sample) then the wrap, and the sign follows the
        // request's own turn.
        const float ballHeading = DisplacementHeading(host);
        drift = Wrap180(ballHeading - facing);
        const float turnSign = Wrap180(facing + lookDelta);
        const float toward = Wrap180(turnSign + facing - req.F(moveword::kLookHeading));
        drift = toward;
      } else {
        const float a = lookDelta < 0.0f ? k.negOne : k.one;
        const float b = lookError < 0.0f ? k.negOne : k.one;
        if (a != b) {
          drift = k.halfTurn;  // 0x1406c4c10: fVar24 = fVar21 = DAT_142598580 (180), or DAT_1425ddffc (-180) just below
          if (0.0f < lookDelta) drift = k.neg180;
        }
      }
    }
  } else {
    drift = Wrap180(lookAhead + facing - req.F(moveword::kLookHeading));
  }

  // 6b. The STEP_TO_RUN arm (0x1406C55CA-0x1406C592A). The gate is `req[0xe] == 0 && (|req[6] + body| <= 112.5 ||
  // (|req[6]| < 67.5 && |body| <= 90))` (0x1406c55ca CMP [RSP+0x50],0 / JNZ; 0x1406c55f7 COMISS 112.5 / JNC; 0x1406c560f,
  // 0x1406c562f), body = FUN_14043a0a0(req[2], req[4]) = req[4] - req[2]; there is no class gate.
  if (!precise) {
    const float bodyTurnArm = Wrap180(req.F(moveword::kBodyDirection) - facing);
    const float headTurnArm = Wrap180(req.F(moveword::kHeading) - facing);  // FUN_14043a0a0(req[2], req[1])
    const float turnArm = req.F(moveword::kTurn);
    if (std::fabs(turnArm + bodyTurnArm) <= k.deg112p5 ||
        (std::fabs(turnArm) < k.deg67p5 && std::fabs(bodyTurnArm) <= k.deg90)) {
      // The re-plan escape (0x1406C5638): the body turn under 45 degrees, |req[6]| over 45 and FUN_1406b70d0 agreeing hand the
      // tick to FUN_1406d6780 (RUN_TURN_RUN).
      if (std::fabs(bodyTurnArm) < k.deg45 && k.deg45 < std::fabs(turnArm) && StepLetsRunTurn(host, req)) {
        EnterRunTurnRun(host, req);
        return;
      }
      // The heading turn is stamped (clamped to +-112.5) and the stamp pair follows (decompile lines 392-396).
      const float stamp = std::clamp(headTurnArm, k.neg112p5, k.deg112p5);
      req.SetF(moveword::kRecordStampA, stamp);
      req.SetF(moveword::kRecordStampB, stamp + facing);
      // The rate: FUN_1406b9ff0 for the class (at least 1 when the command class is positive) and the speed word (11 or more
      // when the desired speed is over 5), eased by the turn the body still has to make, and held in [0, ceiling].
      const int cmdClass = int(req.U(moveword::kCommandSpeedClass));
      int rateClass = int(req.U(moveword::kClassWord));
      if (0 < cmdClass && rateClass < 1) rateClass = 1;
      float rateSpeed = req.F(moveword::kBandSpeed);
      if (k.kmh5 < req.F(moveword::kDesiredSpeedKmh) && rateSpeed < k.kmh11) rateSpeed = k.kmh11;
      req.SetF(moveword::kRate, RateFor(host, req.Flag(moveword::kUseClassRate), rateSpeed, rateClass, kind));
      const float headAbs = std::fabs(headTurnArm);
      const float headShare = k.deg90 < headAbs ? (headAbs < k.deg135 ? (headAbs - k.deg90) / k.deg45 : k.one) : 0.0f;
      const float eased = std::fabs(turnArm) + headShare * k.deg22p5;
      if (k.s0p35 < req.F(moveword::kRate)) {
        float ease = k.one;
        if (k.deg22p5 < eased) {
          if (eased < k.deg67p5) ease = (eased - k.deg22p5) / k.deg45;
        } else {
          ease = 0.0f;
        }
        req.SetF(moveword::kRate, req.F(moveword::kRate) - ease * k.s0p25);
      }
      const float arrivalCeiling = host.RateCeiling();
      const float easedRate = req.F(moveword::kRate);
      req.SetF(moveword::kRate, arrivalCeiling < easedRate ? arrivalCeiling : std::max(0.0f, easedRate));
      // The re-issue: FUN_1406BA2F0's style, the representative motion from the style table
      // (DAT_1425ddcf0), the mirror masks, and the blend words.
      req.SetByte(moveword::kFootFlag, host.BucketIsLeft(int(req.U(moveword::kLookBucket))) ? 1 : 0);  // DAT_1425dd950[req[0xc]]
      req.SetByte(moveword::kMotionPending, 1);
      const float styleRate = RateFor(host, req.Flag(moveword::kUseClassRate), req.F(moveword::kDesiredSpeedKmh), cmdClass, kind);
      const int styleArm = LocomotionStyle(host, host.AnimeState(), styleRate, req, host.RateCeiling(), false);
      // PES at 0x1406C5898: req[0x09] (the command speed class) is clamped into 0..3 and
      // stored back; that is the class the row uses.
      const int bandArm = std::clamp(int(req.U(moveword::kCommandSpeedClass)), 0, 3);
      req.SetU(moveword::kParamState, static_cast<uint32_t>(styleArm));
      req.SetByte(moveword::kStyle, static_cast<uint8_t>(styleArm));
      req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(bandArm));
      req.SetU(moveword::kMotion,
               static_cast<uint32_t>(host.RunTablesView().styleGroup[styleArm * 4 + bandArm]));
      bool mirrorArm = host.ClipFlag();
      if (styleArm < 13 && ((0x1028u >> (styleArm & 31)) & 1u) != 0) {
        mirrorArm = true;
      } else if (styleArm < 12 && ((0x814u >> (styleArm & 31)) & 1u) != 0) {
        mirrorArm = false;
      } else {
        mirrorArm = req.Flag(moveword::kFootFlag);
      }
      req.SetByte(moveword::kMirror, mirrorArm ? 1 : 0);
      req.SetF(moveword::kPlayback, k.one);
      req.SetU(moveword::kTransitionWord, 0u);
      req.SetF(moveword::kDuration, kBlend0266);
      req.SetF(moveword::kStartTime, 0.0f);
      // PES's tail (0x1406C587C-0x1406C592A): both dwell pairs, the live kind, the class word
      // and the state; the pending byte stays 1 (0x1406c4...: byte [req+0xbc] = 1 before the style call) and the turn-lock
      // byte (word 0x3c) clears.
      req.SetF(moveword::kDwellElapsed, 0.0f);
      req.SetF(moveword::kDwellLimit, kBlend0266);
      req.SetF(moveword::kDwellElapsed2, 0.0f);
      req.SetF(moveword::kDwellLimit2, kBlend0266);
      req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
      req.SetU(moveword::kLiveBucket, 0u);
      RequestState(req, MoveState::kStepToRun);
      req.SetByte(moveword::kTurnLockFlag, 0);
      return;
    }
  }

  // The column window and the row.
  float lo = k.deg45, hi = k.deg45;
  if (1 < liveBand || kind != 3 || int(req.U(moveword::kCommandId)) == 5) {
    if (!slowTurn && ((currentBucket != 1 && currentBucket != 8) || drift <= 0.0f) &&
        ((currentBucket != 3 && currentBucket != 6) || 0.0f <= drift) &&
        ((bucket & ~3u) != 0 || 0.0f <= drift) &&
        ((bucket - 5 & ~3u) != 0 || drift <= 0.0f)) {
      lo = k.deg67p5;
      hi = k.neg67p5;
    }
  }
  float turn = drift;
  if (turn <= lo) {
    turn = hi;
    if (hi <= turn) turn = drift;
  }
  float window = k.deg28p125;
  if (bucket < 10 && ((0x231u >> (bucket & 0x1f)) & 1) != 0) window = k.deg16p875;
  int column = bucket;
  if (window * window < turn * turn) {
    column = AngleBucket(host.AngleBuckets(), tables.bucketDegrees[bucket] - turn, 10, false);
    bucketMoved = true;
  }
  if (kind == 7) {
    if (currentBucket == 8 && bucket == 3) {
      bucket = 4;
    } else if (currentBucket == 3 && bucket == 8) {
      bucket = 9;
    }
  }

  // The row, its folds and the column's cell are read where PES reads them (the clip-issue arm below): they draw the generator.

  // The two tails. PES's first test is
  //     if ((!bVar20) && (local_res8[0] == 0 && !bVar19)) goto LAB_1406c5eba;
  // i.e. nothing moved, so the tick is simply kept. `bVar20` is the bucket moved by the
  // look-bucket crossover or the column re-fold; `local_res8` the drift taken from the sampled
  // ball position; `bVar19` the wide turn that lowered the band.
  // Then the band gate decides which tail runs:
  //     if (A) { if (req[0x1a] < req[0x1b] * 0.25) goto keep; ...issue... }
  //     else   { ...keep the tick, ask for STEP_TURN_RUN... }
  // with PES's own A (the decompile's five-way || at 0x1406C5177):
  //     A = (class < 0)
  //      || ((nextBand < 3 && ((nextBand < 2 || kind == 3 || kind == 6) || precise != 0))
  //      || ((unsigned(rowBucket - 3) > 1 && unsigned(rowBucket - 8) > 1)
  //          || (|wrap180(kHeading - kFacing)| <= 112.5 || distanceFactor * 22.5 + 45 <= |wrap180(kHeading - kBodyDirection)|))
  // (asm 0x1406c59b0..0x1406c5a21). The port had the precise test inverted, the bucket term on the column instead of the row, and
  // a 4 m distance test where the heading turn belongs.
  const float bodyToTarget = Wrap180(req.F(moveword::kHeading) - req.F(moveword::kBodyDirection));
  const int band0 = int(req.U(moveword::kClassWord));
  const float distance = req.F(moveword::kDistToTarget);
  const float distanceFactor =
      k.two < distance ? (distance < k.kmh5 ? (distance - k.two) / k.dist3 : k.one) : 0.0f;  // 0x1406c5d5..: 2.0, 5.0, 3.0
  // PES's row bucket: param_2[0x20] is the row id, and DAT_1425dd660[it] is that row's angle.
  // The bucket in 4..7 is the one the term tests.
  const uint32_t rowBucket = req.U(0x20);
  // 0x1406c59eb..0x1406c59ff: `LEA EAX,[RCX-3]; CMP EAX,1; JBE` then `LEA EAX,[RCX-8]; CMP EAX,1; JA A`: UNSIGNED, so a row bucket
  // outside {3, 4, 8, 9} (0..2 wrap high) takes path A. 0x1406c5a05..0x1406c5a21: the keep-tick tail needs |req[1] - req[2]| (the
  // heading turn, XMM11 from FUN_14043a0a0(req[2], req[1]) at 0x1406c55b1) above 112.5 (XMM6 = DAT_1425cb30c, 0x1406c55d2) and the
  // distance factor's 45 + 22.5 * f above |FUN_14043a0a0(req[4], req[1])| (XMM3 at 0x1406c595e); the port had a 4 m distance
  // test where the heading turn belongs.
  const float headTurn = Wrap180(req.F(moveword::kHeading) - facing);
  const bool gateA =
      band0 < 0 || ((nextBand < 3 && ((nextBand < 2 || kind == 3) || kind == 6)) || precise) ||
      ((rowBucket - 3u > 1u && rowBucket - 8u > 1u) ||
       (std::fabs(headTurn) <= k.deg112p5 ||
        distanceFactor * k.deg22p5 + k.deg45 <= std::fabs(bodyToTarget)));
  // d_1406c4c10.c 367-385 (asm 0x1406c553c..0x1406c559d): the cell-changed flag (local_res10, the decompile's reused bVar19): it needs
  // the band walk not to have applied, and is set when the live band or kind differs from this tick's, or when both are band 0 / kind 3
  // and the clip's blend, in seconds, is shorter than the dwell it has run.
  bool cellChanged = false;
  if (localRes18) {
    if (int(req.U(moveword::kClassWord)) != nextBand || int(req.U(moveword::kLiveKind)) != kind) {
      cellChanged = true;
    } else if (nextBand == 0 && kind == 3) {
      const int blendTicks = host.ClipTicks(pes::ClipReader::kBlend, host.CurrentMotion(), k.one, 0);
      cellChanged = static_cast<float>(blendTicks) / host.TicksPerSecond() < req.F(moveword::kDwellElapsed);
    }
  }
  // d_1406c4c10.c 505: `if (!bVar20 && local_res8 == 0 && !bVar19) goto LAB_1406c5eba`, the keep tick (asm 0x1406c5940..0x1406c5951):
  // bVar19 there is the cell-changed flag above, no longer the wide-turn band drop (`slowTurn`), which only shapes the band and bucket.
  if (!bucketMoved && !driftTaken && !cellChanged) {
    StepLoopKeepsTick(host, req);
    return;
  }
  if (gateA) {
    // 0x1406c5d3f..0x1406c5d58: the dwell has not run a quarter of its limit, so the tick is kept.
    if (req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit) * k.s0p25) {
      StepLoopKeepsTick(host, req);
      return;
    }
    if (!precise && int(req.U(moveword::kClassWord)) > 0) req.SetU(moveword::kKind, 3);
  } else {
    StepLoopTurnRunArm(host, req, bucket);
    return;
  }
  // d_1406c4c10.c 536-563 (asm 0x1406c5d3f..0x1406c5e6e): the row of the kind PES re-reads (after the kind-3 store above) and the
  // band, its folds, the column's cell and its mirror bit (FUN_1406abd60). The folds draw the generator, so they run only on this path.
  const int kindNow = int(req.U(moveword::kKind));
  const int col = int(tables.bucketColumn[column]);
  int row = int(tables.kindBandRow[kindNow + nextBand * 8]);
  constexpr int kSwingColumn = 0x11;                          // 0x1406c5d96: the columns 0 and 0x11 hold the two keep rows
  constexpr int kRowHoldA = 0x53a, kRowHoldAFold = 0x53b;     // 0x1406c5da1, 0x1406c5dba
  constexpr int kRowHoldB = 0x542, kRowHoldBFold = 0x543;     // 0x1406c5dc1, 0x1406c5dda
  constexpr int kRowStart = 0x52e, kRowStartFold = 0x530;     // 0x1406c5de1, 0x1406c5e1f
  constexpr int kPercentHoldA = 0x32, kPercentHoldB = 0x14;   // 0x1406c5da9, 0x1406c5dc9, 0x1406c5dfa
  constexpr uint32_t kCommandStart = 5;                       // 0x1406c5e0b: req[0x16] == 5
  const bool swingColumn = col == kSwingColumn || col == 0;
  if (swingColumn && row == kRowHoldA) {
    if (host.Percent(kPercentHoldA)) row = kRowHoldAFold;
  } else if (swingColumn && row == kRowHoldB) {
    if (host.Percent(kPercentHoldB)) row = kRowHoldBFold;
  } else if (row == kRowStart) {
    if (bucketMoved || driftTaken || host.Percent(kPercentHoldA)) row = kRowStartFold;
  }
  if (req.U(moveword::kCommandId) == kCommandStart && nextBand == 0 && static_cast<uint32_t>(kindNow) - 2u < 2u) row = kRowStartFold;
  const MoveCell cell = host.Cell(row, col, false);
  const int motion = cell.found ? cell.motionNo : row;
  const bool mirror = cell.mirror;
  // 0x1406c5e8a..0x1406c5eb7: the clip about to be issued is the clip playing (vt+0x18 == the cell's motion) with the mirror it
  // plays in (vt+0x30): the speed word and the band are recorded and the tick is kept; the clip is not issued again.
  if (host.CurrentMotion() == motion && host.ClipFlag() == mirror) {
    req.SetF(moveword::kBandSpeed, blendedSpeed);
    req.SetU(moveword::kClassWord, uint32_t(nextBand));
    StepLoopKeepsTick(host, req);
    return;
  }

  // The clip tail. PES's own order: the blend, the foot start and transition, the request words 0x17/0x18 the headings, the playback,
  // the issue, the dwell reset, the rate walk, the recorded band and bucket, and STEP_LOOP_CHANGE (req[0x39] = 5).
  // 0x1406c5f5d..0x1406c5fbd: the blend is DAT_1425ddf4c, or DAT_1425ddf50 when nothing moved (no cell change, no drift) or the bucket
  // moved, and the look delta is past 11.25 degrees or the request is not precise; DAT_142598578 (0.2) past 67.5 degrees.
  const float lookDeltaAbs = std::fabs(req.F(moveword::kLookDelta));
  float clipBlend = k.s0p1333;
  if (((!cellChanged && !driftTaken) || bucketMoved) && (k.deg11p25 <= lookDeltaAbs || !precise)) {
    clipBlend = k.deg67p5 < lookDeltaAbs ? k.s0p2 : k.s0p1667;
  }
  // 0x1406c6076..0x1406c61a6: the clip starts at FUN_1406b97d0's time and plays with transition 0 when the mirror differs from the
  // playing one, the bucket moved, the drift was taken, the cell did not change, or the column is in the foot set (mask 0x2a015, which
  // starts the clip at the frame of the foot record that matches the anime's foot band); otherwise it blends from 0 with transition 2.
  constexpr uint32_t kFootColumnMask = 0x2a015u;  // 0x1406c6076: R8D
  constexpr uint32_t kFootColumnLimit = 0x12;     // 0x1406c609b: CMP R15D,0x11 / JA
  const bool footColumn = static_cast<uint32_t>(col) < kFootColumnLimit && ((kFootColumnMask >> col) & 1u) != 0u;
  float startTime = 0.0f;
  int transition = 2;
  if (mirror != host.ClipFlag() || bucketMoved || driftTaken || !cellChanged || footColumn) {
    transition = 0;
    startTime = host.MotionStartTime(motion);
    if (footColumn) {
      const int footNow = host.FootBand();
      const int subCount = host.Motion(motion).FootCount();
      for (int i = 0; i < subCount; ++i) {
        int sub = host.Motion(motion).FootPair(i);
        if (mirror) sub = sub == 0 ? 2 : sub == 1 ? 3 : sub == 2 ? 0 : sub == 3 ? 1 : sub;  // the pair's sides swap with the mirror
        if (footNow == sub) {
          startTime = static_cast<float>(host.ClipTicks(pes::ClipReader::kCancel, motion, k.one, i) + 1) / host.TicksPerSecond();
          break;
        }
      }
    }
  }
  pes::MotionRequest request;
  request.motion = motion;
  request.mirror = mirror;
  request.transition = transition;
  request.duration = clipBlend;
  request.playback = StepPlayback(host.PhysiqueG(), host.PlaybackConstantsView());
  request.startTime = startTime;
  WriteIssue(host, req, request);
  // PES at 0x1406C5FE0 and the store at 0x1406C5FF1: the value written into req[0x17] is
  // wrap180((req[0x01] - the bucket's angle) - the player's facing), with the bucket's angle
  // from DAT_1425dd660.
  req.SetF(moveword::kRecordStampA,
           Wrap180(req.F(moveword::kHeading) - tables.bucketDegrees[bucket] - facing));
  req.SetF(moveword::kRecordStampB, req.F(moveword::kHeading));
  req.SetU(moveword::kDwellElapsed, 0);
  req.SetF(moveword::kDwellLimit, precise ? 0.0f : clipBlend);  // d_1406c4c10.c after the vt+0x10 issue: req[0x1a] = 0, req[0x1b] = req[0xe] == 0 ? the blend (a float) : 0
  const float rateTarget = RateFor(host, req.Flag(moveword::kUseClassRate), blendedSpeed,
                                   nextBand, kindNow);
  if (req.F(moveword::kRate) <= rateTarget) {
    const float raised = req.F(moveword::kRate) + k.step0p05;
    req.SetF(moveword::kRate, (raised <= rateTarget && 0.0f <= raised) ? raised : 0.0f);
  } else {
    const float ceiling = host.RateCeiling();
    const float lowered = req.F(moveword::kRate) - k.step0p05;
    req.SetF(moveword::kRate, (lowered <= ceiling && rateTarget <= lowered) ? lowered
                                                                            : rateTarget);
  }
  req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
  req.SetU(moveword::kBandSpeedCopy, req.U(moveword::kBandSpeed));
  req.SetU(moveword::kBucketPrev, req.U(moveword::kMoveBucket));
  req.SetU(moveword::kMoveBucket, uint32_t(bucket));
  req.SetF(moveword::kBandSpeed, blendedSpeed);
  req.SetU(moveword::kClassWord, uint32_t(nextBand));
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  RequestState(req, MoveState::kStepLoopChange);
  req.SetByte(moveword::kTurnLockFlag, 0);  // word 0x3c (d_1406c4c10.c:894), not the motion-pending byte WriteIssue set
}

namespace {
// The stop rows the else arm issues (0x1406c5b8c..0x1406c5ba4): 0x639 when req[0x20] is 3 or 8, else 0x631; the cell's column is
// 9, or 8 for the left (the mirror) (0x1406c5bc5..0x1406c5bcd).
constexpr int kStopRowSide = 0x639, kStopRowEven = 0x631;
constexpr int kStopColumnRight = 9, kStopColumnLeft = 8;
constexpr uint32_t kStopRowBucketA = 3, kStopRowBucketB = 8;
// The slot split of the roster: slots 0..10 are the home side, 11..21 the away side (the tests at 0x1406c5af1..0x1406c5b08).
constexpr int kAwayFirstSlot = 11;
constexpr uint32_t kRosterSlots = 0x16;

// LAB_1406c5eba (asm 0x1406c5eba..0x1406c6415, d_1406c4c10.c 572-657): the tick is kept. The turn-lock byte clears, the adjuster
// is applied (P+0x4d0 vt+8: the host's ApplyBodyTail), and the root facing is blended towards the body direction: the turn
// wrap(req[1] - req[4]) is clamped to +-180 / the tick rate and added to the facing (folded into 0..360); when the folded facing
// lies 22.5..135 degrees off the folded look heading and the look heading's side (wrap(req[2] - req[3])) differs from the side the
// facing moved to, a precise request ends the tick without the write. Otherwise req[0x3a] = 1 and req[0x3b] = the new facing.
// The band, the bucket and the speed words are not touched: only the same-clip keep writes req[0x1d] and req[0x1f] first.
void StepLoopKeepsTick(MoveHost& host, MoveReq& req) {
  const StepScalars& k = host.StepConstants();
  const RunConstants& rk = host.RunConstantsView();
  req.SetByte(moveword::kTurnLockFlag, 0);
  const float toBody = Wrap180(req.F(moveword::kHeading) - req.F(moveword::kBodyDirection));
  const float draw = k.halfTurn / host.TicksPerSecond();
  const float delta = toBody > draw ? draw : std::max(-draw, toBody);
  const float faced = FoldFull(req.F(moveword::kFacing) + delta, rk);
  float residual = faced - FoldFull(req.F(moveword::kLookHeading), rk);
  if (k.halfTurn < std::fabs(residual)) residual -= (residual < 0.0f ? k.negOne : k.one) * k.full;
  const float off = std::fabs(residual);
  if (k.deg22p5 <= off && off <= k.deg135) {
    const float lookSide = Wrap180(req.F(moveword::kFacing) - req.F(moveword::kLookHeading));
    float moved = FoldFull(req.F(moveword::kFacing), rk) - faced;
    if (k.halfTurn < std::fabs(moved)) moved -= (moved < 0.0f ? k.negOne : k.one) * k.full;
    if ((lookSide < 0.0f) != (moved < 0.0f) && req.U(moveword::kPrecise) != 0u) return;
  }
  req.SetByte(moveword::kRootFacing, 1);
  req.SetF(moveword::kRootFacingAngle, faced);
}

// The gate's else arm (asm 0x1406c5a27..0x1406c5d3a, d_1406c4c10.c 786-893): the step turns into the run (STEP_TURN_RUN). The
// heading stamps are written, the mirror is picked from the heading turn, the ball sample's bearing and the look point's bearing,
// the stop row of the OLD row bucket is issued in the column the mirror names, and the class, speed band, rate and the
// previous-class words follow. No root facing is written.
void StepLoopTurnRunArm(MoveHost& host, MoveReq& req, int bucket) {
  const StepScalars& k = host.StepConstants();
  const StepTables& tables = host.StepTablesView();
  const RunConstants& rk = host.RunConstantsView();
  const float facing = req.F(moveword::kFacing);
  const float heading = req.F(moveword::kHeading);
  req.SetF(moveword::kRecordStampA, Wrap180(heading - tables.bucketDegrees[bucket] - facing));  // 0x1406c5a2c: XMM14
  req.SetF(moveword::kRecordStampB, heading);                                                    // 0x1406c5a32
  const float headTurn = Wrap180(heading - facing);                    // XMM11
  const float lookTurn = Wrap180(req.F(moveword::kLookHeading) - facing);  // XMM10
  float ball[3];
  host.BallSample(static_cast<int>(host.TicksPerSecond() * rk.reserveLow / rk.kmh60 + rk.half), ball);  // 0x1406c5a61..0x1406c5a81
  const float ballTurn = Wrap180(host.HeadingToPointDeg(ball) - facing);                                  // XMM8, 0x1406c5a99
  // 0x1406c5ac5..0x1406c5b24: the gates, narrowed when the ball carrier is on the other side of the roster from this man.
  float headGate = k.deg146p25, ballGate = k.deg33p75;
  const int carrier = host.CarrierSlot();
  if (static_cast<uint32_t>(carrier) < kRosterSlots) {
    const int own = host.ActionIndex();
    const bool ownAway = own >= kAwayFirstSlot && static_cast<uint32_t>(own) < kRosterSlots;
    if (ownAway != (carrier >= kAwayFirstSlot)) {
      headGate = k.deg140p625;
      ballGate = k.deg11p25;
    }
  }
  // 0x1406c5b2c..0x1406c5b8c: which bearing names the left.
  const float headMag = std::fabs(headTurn);
  bool left;
  if (headMag > headGate && std::fabs(ballTurn) > ballGate) {
    left = ballTurn < 0.0f;
  } else if (headMag > headGate && std::fabs(lookTurn) > k.deg22p5) {
    left = lookTurn < 0.0f;
  } else {
    left = (headMag > k.deg168p75 ? ballTurn : headTurn) < 0.0f;
  }
  const uint32_t oldRow = req.U(0x20);
  const int row = (oldRow == kStopRowBucketA || oldRow == kStopRowBucketB) ? kStopRowSide : kStopRowEven;
  const MoveCell cell = host.Cell(row, left ? kStopColumnLeft : kStopColumnRight, false);  // 0x1406c5bd9: FUN_1406abb30
  const int motion = cell.found ? cell.motionNo : row;
  req.SetByte(moveword::kFootFlag, left ? 1 : 0);  // 0x1406c5bde
  // 0x1406c5c34..0x1406c5c7e (decompile 859-862): req[0x35] = vt+0x50(vt+0x30(match-up flag)), then
  // vt+0x10(req, motion, mirror, 0, DAT_1425ddf50, req[0x35], 0), which leaves the request due for the tail.
  req.SetF(moveword::kPlayback, EntryPlayback(host));
  WriteIssue(host, req, pes::MotionRequest{motion, left, 0, k.s0p1667, req.F(moveword::kPlayback), 0.0f});
  // 0x1406c5c81..0x1406c5d3a: the class is clamp(req[9], 0, 2), the speed band clamp(req[10], 5, 16), the rate that pair's, held
  // in [0, ceiling]; the previous class, speed and row words are saved and the class, speed and live kind carried.
  const int cls = std::clamp(static_cast<int>(req.U(moveword::kCommandSpeedClass)), 0, 2);
  const float desired = req.F(moveword::kDesiredSpeedKmh);
  const float speed = desired <= k.kmh16 ? std::max(desired, k.kmh5) : k.kmh16;
  const float rate = RateFor(host, req.Flag(moveword::kUseClassRate), speed, cls, static_cast<int>(req.U(moveword::kKind)));
  const float ceiling = host.RateCeiling();
  req.SetF(moveword::kRate, rate > ceiling ? ceiling : std::max(0.0f, rate));
  req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
  req.SetU(moveword::kBandSpeedCopy, req.U(moveword::kBandSpeed));
  req.SetU(moveword::kBucketPrev, oldRow);
  req.SetU(0x20, 0u);
  req.SetF(moveword::kBandSpeed, speed);
  req.SetU(moveword::kClassWord, static_cast<uint32_t>(cls));
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  RequestState(req, MoveState::kStepTurnRun);
  req.SetByte(moveword::kTurnLockFlag, 0);  // 0x1406c62cb
}
}  // namespace
// -------------------------------------------------------------------------------------------------
// EnterTurnStop, FUN_1406d2590 (0x1406d2590..0x1406d2e64), the TURN_STOP entry. RUN_TO_STEP and TURN_STOP itself call it. Ported from
// the instruction stream (the call sites 0x1406d25fa..0x1406d2e17) and the decompile d_1406d2590.c:
//   0x1406d25fa..0x1406d2654  the playback from the match-up flag (EntryPlayback), stored in req[0x35].
//   0x1406d2655..0x1406d26a0  the bucket and turn the entry starts from: req[0xd] and req[7], or req[0xc] and req[5] when the command
//                             class is not -1 and the request is not precise; the class word clamped into 0..3.
//   0x1406d26af..0x1406d2828  in play (phase 1) at bucket 3, 4, 8 or 9 (UNSIGNED tests) with the turn past 146.25: the look-behind
//                             re-aim at the ball sample (or the look heading), and the flip of the bucket to its mirrored pair.
//   0x1406d2828..0x1406d2b4c  the bucket of the body turn (P+0x28 against P+0x20) for the motion groups the player is in, through the
//                             interface's bucket function (vt+0x60).
//   0x1406d2968..0x1406d2bc3  the 4 -> 3 and 9 -> 8 fold, the kind fold, and the stop cell (vt+0xd0: FUN_1406b9d80's row, the cell's
//                             motion and mirror).
//   0x1406d2bc3..0x1406d2d76  the blend by the clip's group, and the 0xe2d arm's mirror from the bone ring.
//   0x1406d2d76..0x1406d2e64  the issue (vt+0x10), TURN_STOP, and the tail's request words.
// -------------------------------------------------------------------------------------------------
namespace {

// FUN_1405aef10(&a, &b): b - a, wrapped once by 360 when its magnitude passes 180 (no fold of either side first, unlike
// FUN_14043a0a0). The inline `x - P+0x20` of the re-aim is the same wrap.
float PlainDiffDeg(float a, float b, const RunConstants& k) {
  float d = b - a;
  if (k.half_turn < std::fabs(d)) d -= (d < 0.0f ? k.neg : k.one) * k.full;
  return d;
}

// COMISS 0, x; the -1.0 / 1.0 pair every sign test of the entry builds: -1 when x < 0, else 1.
float SignOne(float x, const RunConstants& k) { return x < 0.0f ? k.neg : k.one; }

}  // namespace

void EnterTurnStop(MoveHost& host, MoveReq& req) {
  const RunConstants& rk = host.RunConstantsView();
  const StepScalars& k = host.StepConstants();
  const StepTables& tables = host.StepTablesView();

  // 0x1406d25fa..0x1406d2654 (d_1406d2590.c:52-69): req[0x35] is a local of PES's, handed to the issue.
  const float playback = EntryPlayback(host);
  req.SetF(moveword::kPlayback, playback);

  // 0x1406d2655.. (c:67-82).
  int bucket = int(req.U(moveword::kLookBucketAlt));  // req[0xd]
  float turn = req.F(moveword::kFacingError);         // req[7]
  if (-1 < int(req.U(moveword::kCommandSpeedClass)) && req.U(moveword::kPrecise) == 0) {
    bucket = int(req.U(moveword::kLookBucket));  // req[0xc]
    turn = req.F(moveword::kLookDelta);          // req[5]
  }
  const int classBand = std::clamp(int(req.U(moveword::kClassWord)), 0, 3);

  // 0x1406d26af..0x1406d2828 (c:91-166): `bucket - 3 < 2 || bucket - 8 < 2` are unsigned compares (3, 4, 8, 9 only).
  if (host.MatchPhase() == 1 && (static_cast<uint32_t>(bucket - 3) < 2u || static_cast<uint32_t>(bucket - 8) < 2u) &&
      rk.deg146p25 < std::fabs(turn)) {
    float ball[3];
    host.BallSample(static_cast<int>(host.TicksPerSecond() * rk.reserveLow / rk.kmh60 + rk.half), ball);  // 0x1406d2748..0x1406d276c
    float aim = host.HeadingToPointDeg(ball);                                                            // FUN_140439e50(P+8, sample)
    const uint32_t carrier = static_cast<uint32_t>(host.CarrierSlot());                                    // FUN_140a627e0(trainer, 0)
    const float look = req.F(moveword::kLookHeading);
    const float lookOff = std::fabs(PesAngleDiff(req.F(moveword::kFacing), look, rk));
    if (0x15u < carrier && rk.deg225 < lookOff && lookOff < rk.deg1575) {
      aim = 0.0f <= look ? (rk.full <= look ? look + rk.wrapFull : look) : look + rk.full;
    }
    const float reAim = PlainDiffDeg(host.FacingDeg(), aim, rk);  // aim - P+0x20, wrapped
    const float bodyOff = PlainDiffDeg(host.FacingDeg(), host.BodyDeg(), rk);  // FUN_1405aef10(P+0x20, P+0x28)
    float window = rk.deg225;
    if (rk.deg225 < std::fabs(bodyOff) && SignOne(bodyOff, rk) == SignOne(reAim, rk)) window = rk.deg1125;
    const float reAimAbs = std::fabs(reAim);
    if (window < reAimAbs && reAimAbs < rk.half_turn - window && SignOne(turn, rk) != SignOne(reAim, rk)) {
      const uint32_t was = static_cast<uint32_t>(bucket - 3);
      turn = 1u < was ? rk.half_turn : -rk.half_turn;  // fVar19 = DAT_1425ddffc (-180), or 180 past the first pair
      bucket = 1u < was ? 4 : 9;
    }
  }

  // 0x1406d2828.. (c:167-275): the body turn P+0x28 - P+0x20 and its magnitude.
  const float bodyDir = PlainDiffDeg(host.FacingDeg(), host.BodyDeg(), rk);
  const float bodyError = std::fabs(bodyDir);
  float stamp = turn;  // fVar14: the value the tail clamps into the bucket's window (the turn unless an arm replaced it)
  const auto interfaceBucket = [&](float angle) { return AngleBucket(host.AngleBuckets(), angle, 10, false); };  // vt+0x60(angle, 10)

  // The motion-group arms (c:178-275). `uVar3` is the 16-bit motion id of the interface (vt+0x18).
  const uint16_t current = static_cast<uint16_t>(host.CurrentMotion());
  bool bodyTurnGroup = false;
  bool lowArm = false;  // 0x8c7
  if (0xb48u < current) {
    bodyTurnGroup = current == 0xb4f || current == 0xb52 || current == 0xb5a || current == 0xbc3 || current == 0xc05 || current == 0xc3d;
  } else if (current == 0xb48) {
    bodyTurnGroup = true;
  } else if (current < 0x4fu) {
    const uint32_t group = static_cast<uint32_t>(current) - 0x25u;
    bodyTurnGroup = group < 0x2au && ((0x24800000001ull >> (group & 0x3f)) & 1) != 0;
  } else if (0x8b4u < current) {
    if (current < 0x8bau) {
      bodyTurnGroup = true;
    } else if (current == 0x8c7) {
      lowArm = true;
    } else if (current == 0x8ca) {
      bodyTurnGroup = true;
    }
  }
  if (lowArm) {
    const float side = 0.0f < turn ? k.deg45 : k.neg45;  // DAT_14259be30 / DAT_1425aa1c8
    if (k.deg45 < std::fabs(turn)) {
      stamp = side;
      bucket = interfaceBucket(side);
    }
  } else if (bodyTurnGroup) {
    if (bodyError <= k.deg33p75) {
      if (k.deg22p5 < bodyError) {
        const float side = 0.0f < bodyDir ? k.deg90 : k.neg90;  // DAT_14259be5c / DAT_1425c5958
        if (SignOne(turn, rk) != SignOne(side, rk)) {
          stamp = -side;  // the sign bit flipped (XORPS with DAT_1425656d0)
          bucket = interfaceBucket(stamp);
        }
      }
    } else {
      const float wide = 0.0f < bodyDir ? k.halfTurn : k.neg180;
      const float window = 0.0f < bodyDir ? k.deg45 : k.neg45;
      if (SignOne(turn, rk) != SignOne(window, rk)) {
        stamp = k.deg146p25 <= std::fabs(turn) ? wide : -window;  // 0x1406d2a9f: the 180 arm, else the window with its sign flipped
        bucket = interfaceBucket(stamp);
      }
    }
  }

  // 0x1406d2968 (c:279-289): the 4 -> 3 and 9 -> 8 fold, and the kind fold (P+0x2458 is the slot byte, read as unsigned).
  if (bucket == 4) {
    bucket = 3;
  } else if (bucket == 9) {
    bucket = 8;
  }
  int kind = int(req.U(moveword::kKind));
  const int command = int(req.U(moveword::kCommandId));
  if (static_cast<uint8_t>(host.ActionIndex()) < 0x16 && (static_cast<uint32_t>(command - 0x33) < 2u || command == 0x4d) &&
      static_cast<uint32_t>(kind - 2) < 2u) {
    kind = 1;
  }
  // vt+0xd0 (c:293): FUN_1406b9d80's stop row at kind + class * 8, and the cell's motion and mirror at the bucket's column.
  const int row = StopRowFor(kind, classBand, command, host.RunTablesView());
  const CellPick cell = PickCell(host, row, int(tables.bucketColumn[bucket]));
  const int motion = static_cast<uint16_t>(cell.motion);
  bool mirror = cell.mirror;

  // 0x1406d2bc3.. (c:294-325): the blend by the motion's group, and the 0xe2d arm's mirror from the bone ring.
  float blend = k.s0p1667;
  const uint32_t groupBit = static_cast<uint16_t>(motion - 0xbee);
  if ((groupBit < 0x36u && ((0x30000000000011ull >> (groupBit & 0x3f)) & 1) != 0) || motion == 0xbc2 ||
      static_cast<uint16_t>(motion - 0xb45) < 2) {
    blend = k.s0p2;
    if (k.deg22p5 < bodyError && ((0.0f < bodyDir && mirror) || (bodyDir < 0.0f && !mirror))) blend = k.s0p25;
  } else if (motion == 0xe2d) {
    // FUN_141eb51a0(P, out, 3 / 6): the bone ring's joints 3 and 6, each less the own position and turned by -facing.
    float own[3], joint3[3], joint6[3];
    host.OwnPosition(own);
    host.PositionAt(3, joint3);
    host.PositionAt(6, joint6);
    for (int i = 0; i < 3; ++i) {
      joint3[i] -= own[i];
      joint6[i] -= own[i];
    }
    RotateXZ(joint3, host.FacingDeg() * k.negOne, host.SpeedClassConstantsView());
    RotateXZ(joint6, host.FacingDeg() * k.negOne, host.SpeedClassConstantsView());
    if (joint3[2] <= 0.0f || 0.0f <= joint6[2]) {
      if (joint3[2] < 0.0f && 0.0f < joint6[2]) mirror = true;
    } else {
      mirror = false;
    }
  }

  // 0x1406d2d76 (c:326-330): vt+0x10(req, motion, mirror, 0, blend, playback, 0), then TURN_STOP.
  WriteIssue(host, req, pes::MotionRequest{motion, mirror, 0, blend, playback, 0.0f});
  RequestState(req, MoveState::kTurnStop);

  // c:331-346: the heading the stop turns toward (the stamp clamped into the bucket's window about its angle) and the history words.
  const float centre = tables.bucketDegrees[bucket];  // 0x1425dd960
  const float window = (static_cast<uint32_t>(bucket - 3) < 2u || static_cast<uint32_t>(bucket - 8) < 2u) ? rk.deg1125 : rk.deg225;
  float heading = centre + window;
  if (stamp <= heading) {
    heading = centre - window;
    if (heading <= stamp) heading = stamp;
  }
  req.SetF(moveword::kRecordStampB, heading + req.F(moveword::kFacing));
  req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
  req.SetU(moveword::kBandSpeedCopy, req.U(moveword::kBandSpeed));
  req.SetU(moveword::kBucketPrev, req.U(moveword::kLiveBucket));
  req.SetU(moveword::kLiveBucket, uint32_t(bucket));
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  req.SetByte(moveword::kTurnLockFlag, 0);  // c:346: the byte of word 0x3c
}

// -------------------------------------------------------------------------------------------------
// STEP_LOOP_CHANGE, FUN_1406c6480 (0x1406c6480..0x1406c6ad0). The style-change tick of the step
// loop: it blends the rate between the two bands, decides whether the change completes or the
// loop falls back to a stop, and asks for STEP_LOOP once the dwell has run out.
//
//   1. The blended heading integral (req[0x2b], req[0x2c]): the two bands' rates interpolated by
//      the dwell's share, and the difference against the live rate as the drift's weight.
//   2. The precise branch: a wide turn with the bucket unchanged calls FUN_1406d38d0
//      (STEP_TURN_RUN) or FUN_1406d2f60 (RUN_TURN_STEP, the ceiling term).
//   3. The non-precise branch: the band restores, and the same two entries.
//   4. The arrival flag decides whether the change completes or re-plans (FUN_1406ce820).
//   5. The class-3 accelerate (FUN_1406ccd00) and the fallback stop.
//   6. The dwell gates: the stop fallback, the blended root facing (req[0x3a]/req[0x3b]), and
//      STEP_LOOP (req[0x39] = 4) when the dwell has run out.
// -------------------------------------------------------------------------------------------------
void StepLoopChange(MoveHost& host, MoveReq& req) {
  const StepScalars& k = host.StepConstants();
  const MoveConstants& m = host.MoveConstantsView();
  const RunTables& rt = host.RunTablesView();
  const float rampFloor = host.RunConstantsView().rampSlope;
  const int kind = int(req.U(moveword::kKind));
  const bool precise = req.Flag(moveword::kPrecise);
  const int speedClass = int(req.U(moveword::kCommandSpeedClass));
  // 0x1406c6c2b: the blended root facing adds [RSI+0x20], the man's own facing P+0x20, not the request's req[2] (they coincide
  // only while the request was just built from it).
  const float facing = host.FacingDeg();
  const float fps = FramesPerSecond(host);

  // 1. The blended heading integral (0x1406c64b3..0x1406c6620). The two vt+0x38 calls at 0x1406c650d / 0x1406c6538 drop their
  // answer (FUN_1406b9e50's xmm0) and the code reloads req[0x4a] (0x1406c6510 `movss xmm0,[rdi+0x128]`), so both ends of the
  // blend are the live rate clamped to [0, 0.5] (xmm11 = DAT_142565600): the blend term is 0 in PES too. The speeds the
  // weighted rate maps to (vt+0x48, FUN_1406b9aa0) are what req[0x2b] integrates.
  if (req.F(moveword::kDwellLimit2) == 0.0f) {
    req.SetU(moveword::kBlendA, 0);
    req.SetU(moveword::kBlendB, 0);
  } else {
    const float rate = req.F(moveword::kRate);
    const float prev = rate <= k.half ? std::max(0.0f, rate) : k.half;
    const float live = prev;
    const float ratio = req.F(moveword::kDwellElapsed2) / req.F(moveword::kDwellLimit2);
    const float weight = ratio <= k.one ? ratio : k.one;
    const float g = host.PhysiqueG();
    const float shareSpeed = RateToSpeedFor((live - prev) * weight + prev, g, rt, rampFloor);
    float blended = shareSpeed - RateToSpeedFor(live, g, rt, rampFloor);
    if (std::fabs(blended) < k.eps1e7) blended = 0.0f;
    const float held = req.F(moveword::kDwellLimit2) - req.F(moveword::kDwellLimit2) * weight;
    req.SetF(moveword::kBlendB, held);
    req.SetF(moveword::kBlendA, held * blended * k.half + held * shareSpeed);
  }

  // 2/3. The band and the rate, then the two run-turn entries.
  // PES at 0x1406C6632: FUN_14043a0a0(req[0x18], req[0x01]) = wrap180(kHeading - kRecordStampB).
  const float turnToTarget = Wrap180(req.F(moveword::kHeading) - req.F(moveword::kRecordStampB));
  // 0x1406c666f: the word compared with req[0x25] is req[0x20] (the live bucket), not the move bucket req[8].
  const int liveBucket = int(req.U(moveword::kLiveBucket));
  const int prevBucket = int(req.U(moveword::kBucketPrev));
  if (precise && speedClass < 3 && k.deg22p5 < std::fabs(turnToTarget) && liveBucket == prevBucket) {
    if (k.deg45 <= std::fabs(Wrap180(req.F(moveword::kFacing) - DisplacementHeading(host)))) {
      // The ground speed decides the band, in km/h: the clock times the displacement length.
      float d[3];
      host.PositionAt(1, d);
      const float speed = (fps * k.ticks60 * k.ticks60) / k.scale1000 *
                          std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      if (k.kmh8 <= speed) {
        if (k.kmh13p5 <= speed) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kBandSpeed, k.kmh16);
        } else {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kBandSpeed, k.kmh11);
        }
      } else {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kBandSpeed, k.kmh5);
      }
      if (EnterStepTurnRun(host, req)) return;
    } else {
      // The rate from the ground speed's own band (the immediates 0x3f19999a, 0x3e99999a, 0x3e19999a at 0x1406c66f9 /
      // 0x1406c676e / 0x1406c675c are 0.6, 0.3 and 0.15), the class from the rate (vt+0x68, FUN_1406ba030) and the class's
      // speed from the rate (vt+0x48, FUN_1406b9aa0, times 3.6).
      const float ceiling = host.RateCeiling();
      req.SetF(moveword::kRate, kRateHigh);
      float d[3];
      host.PositionAt(1, d);
      const float speed = (fps * k.ticks60 * k.ticks60) / k.scale1000 *
                          std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      if (k.kmh8 <= speed) {
        if (k.kmh13p5 <= speed) {
          if (k.kmh22 < speed) req.SetF(moveword::kRate, ceiling);
        } else {
          req.SetF(moveword::kRate, kRateMid);
        }
      } else {
        req.SetF(moveword::kRate, kRateLow);
      }
      req.SetU(moveword::kClassWord, uint32_t(ClassForRateFor(req.F(moveword::kRate), rt)));
      req.SetF(moveword::kBandSpeed,
               RateToSpeedFor(req.F(moveword::kRate), host.PhysiqueG(), rt, rampFloor) * k.kmh3p6);
      // FUN_1406d2f60's threshold: (rate ceiling - 0.6) * 0.5.
      if (EnterRunTurnStep(host, req, (ceiling - k.s0p6) * k.half)) return;
    }
  }

  // 4. The ground speed, then the arrival tests (0x1406c68a1..0x1406c6ad3).
  float d[3];
  host.PositionAt(1, d);
  const float groundSpeed = (fps * k.ticks60 * k.ticks60) / k.scale1000 *
                            std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  const int classWord = int(req.U(moveword::kClassWord));
  // 0x1406c68f..: the flag is the BYTE at request offset 0x3f (the command's flag byte, req+0x3f), not word 0x3f. With the
  // class at most 1 and the word 0, a man under 16 km/h is re-planned; otherwise the ladder below runs for a class above 1
  // or an unflagged request.
  if (req.FlagAtByte(0x3f) && classWord <= 1 && !precise && groundSpeed < k.kmh16) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }
  if (!precise && 1 < speedClass && 0 < classWord) {
    if (k.kmh8 <= groundSpeed) {
      if (k.kmh13p5 <= groundSpeed) {
        req.SetU(moveword::kClassWord, 2);
        req.SetF(moveword::kBandSpeed, k.kmh16);
      } else {
        req.SetU(moveword::kClassWord, 1);
        req.SetF(moveword::kBandSpeed, k.kmh11);
      }
    } else {
      req.SetU(moveword::kClassWord, 0);
      req.SetF(moveword::kBandSpeed, k.kmh5);
    }
    if (EnterStepTurnRun(host, req)) return;
  }
  if (req.F(moveword::kDwellLimit2) * k.half <= req.F(moveword::kDwellElapsed2) &&
      ((int(req.U(moveword::kClassWord)) < 1 ||
        (int(req.U(moveword::kClassWord)) < 2 &&
         k.deg67p5 < std::fabs(req.F(moveword::kTurn)))) &&
       1 < speedClass && !precise)) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }
  // 0x1406c6a3b: |FUN_14043a0a0(req[2], req[4])| < 45.
  if (speedClass == 3 && !precise && int(req.U(moveword::kClassWord)) < 2 &&
      std::fabs(Wrap180(req.F(moveword::kBodyDirection) - req.F(moveword::kFacing))) < k.deg45) {
    if (EnterAccelerate(host, req)) return;
    if (groundSpeed < k.kmh11) {
      EnterGotoRun(host, req, /*runStopVariant=*/false);
      return;
    }
  }

  // 6. The dwell gates.
  bool stopFallback = false;
  if (int(req.U(moveword::kClassWord)) >= 0 && speedClass == -1) stopFallback = true;
  // PES: `if (1.5 < d) { f = 1.0; if (d < 4.0) f = (d - 1.5) / 2.5 } else f = 0` (0x14259bddc, DAT_14256ae1c,
  // DAT_1425656c0, DAT_14259bde4).
  const float toLook = req.F(moveword::kDistToLook);
  const float share = k.dist1p5 < toLook ? (toLook < k.four ? (toLook - k.dist1p5) / m.dist2m5 : k.one) : 0.0f;
  if (share * k.deg22p5 + k.deg45 < std::fabs(turnToTarget) &&
      k.deg22p5 < std::fabs(req.F(moveword::kTurn)) &&
      req.F(moveword::kDwellElapsed2) <= req.F(moveword::kDwellLimit2) * k.s0p7) {
    stopFallback = true;
  }
  float blendedTurn = 0.0f;
  if (fps * req.F(moveword::kDwellLimit2) != 0.0f)
    blendedTurn = req.F(moveword::kRecordStampA) / (fps * req.F(moveword::kDwellLimit2));
  if ((kind != 4 || (k.s0p15 < req.F(moveword::kDwellElapsed2) ||
                     k.s0p15 == req.F(moveword::kDwellElapsed2))) &&
      stopFallback) {
    req.SetF(moveword::kRecordStampA, blendedTurn);
    const float dwellLeft = req.F(moveword::kDwellLimit2) - req.F(moveword::kDwellElapsed2);
    req.SetF(moveword::kRecordStampB, dwellLeft <= 0.0f ? 0.0f : dwellLeft);
    // 0x1406c6bf6: the scale is 0.3 (0x142590d98) when the class fell below the previous one, else xmm11 -- 0.7 (0x14257a194,
    // reloaded at 0x1406c6b48; xmm11 holds 0.5 only until there).
    const float scale = int(req.U(moveword::kClassWord)) < int(req.U(moveword::kPrevClassWord)) ? k.s0p3 : k.s0p7;
    if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2) * scale) {
      req.SetU(moveword::kClassWord, req.U(moveword::kPrevClassWord));
      req.SetF(moveword::kBandSpeed, req.F(moveword::kBandSpeedCopy));
    }
    EnterStop(host, req);
    return;
  }

  // The blended root facing.
  float drift = blendedTurn + facing;
  if (0.0f <= drift) {
    if (k.full <= drift) drift += k.wrapFull;
  } else {
    drift += k.full;
  }
  req.SetF(moveword::kRootFacingAngle, drift);
  req.SetByte(moveword::kRootFacing, 1);
  // 0x1406c6c79..0x1406c6cac: window = req[0x2e] * xmm11 = 0.7 (0x14257a194, reloaded at 0x1406c6b48), and 0.2 with 90 degrees
  // when req[0x20] == req[0x25]. The degrees xmm10 is 60.0 (0x1406c68a1 `movss xmm10,[14259a764]`, the ground-speed scale held in
  // a register) when the buckets differ.
  float window = k.s0p7;
  float drawDegrees = k.ticks60;
  if (liveBucket == prevBucket) {
    window = k.s0p2;
    drawDegrees = k.deg90;
  }
  bool skipToFinalGate = false;
  if (req.F(moveword::kDwellLimit2) * window <= req.F(moveword::kDwellElapsed2) &&
      k.deg5p625 < std::fabs(turnToTarget)) {
    // 0x1406c6cd8..0x1406c6cfe: the per-tick draw is degrees / the frame rate (FUN_141e532d0 = 54.0); the delta is the draw
    // when the turn is past it, else the larger of the negated draw and the turn.
    const float draw = drawDegrees / fps;
    float delta = draw;
    if (turnToTarget <= draw) {
      delta = -draw;
      if (turnToTarget > delta) delta = turnToTarget;
    }
    drift += delta;
    if (0.0f <= drift) {
      if (k.full <= drift) drift += k.wrapFull;
    } else {
      drift += k.full;
    }
    const float toLookHeading = req.F(moveword::kLookHeading);
    float toLookFolded = toLookHeading;
    if (0.0f <= toLookFolded) {
      if (k.full <= toLookFolded) toLookFolded += k.wrapFull;
    } else {
      toLookFolded += k.full;
    }
    float deltaLook = drift - toLookFolded;
    if (k.halfTurn < std::fabs(deltaLook))
      deltaLook -= (deltaLook < 0.0f ? k.negOne : k.one) * k.full;
    if (k.deg67p5 <= std::fabs(deltaLook) && std::fabs(deltaLook) <= k.deg135) {  // 0x1406c6d8d: COMISS XMM15 (DAT_1425cca34 = 67.5)
      // 0x1406c6daf: FUN_14043a0a0(req[3], req[2]) = req[2] - req[3] (the facing less the look heading).
      const float toFacing = Wrap180(req.F(moveword::kFacing) - req.F(moveword::kLookHeading));
      float facingFolded = req.F(moveword::kFacing);
      if (0.0f <= facingFolded) {
        if (k.full <= facingFolded) facingFolded += k.wrapFull;
      } else {
        facingFolded += k.full;
      }
      float deltaFacing = facingFolded - drift;
      if (k.halfTurn < std::fabs(deltaFacing))
        deltaFacing -= (deltaFacing < 0.0f ? k.negOne : k.one) * k.full;
      const float signA = toFacing < 0.0f ? k.negOne : k.one;
      const float signB = deltaFacing < 0.0f ? k.negOne : k.one;
      // 0x1406c6e22: differing signs on a precise request jump to the final gate without the root-facing store.
      if (signA != signB && precise) skipToFinalGate = true;
    }
    if (!skipToFinalGate) {
      req.SetF(moveword::kRootFacingAngle, drift);
      req.SetByte(moveword::kRootFacing, 1);
    }
  }

  // The final gate (0x1406c6e40..0x1406c6f2a). While the second dwell runs the tick ends unless the unflagged tail below
  // passes: PES returns when the dwell is young and the request is not a precise class-2+ straight on one live bucket with
  // the looks inside 45 degrees.
  const float elapsed = req.F(moveword::kDwellElapsed2);
  const float limit = req.F(moveword::kDwellLimit2);
  if (elapsed < limit) {
    const bool livePrecise = elapsed >= limit * k.s0p2 && !precise && 1 < speedClass && liveBucket != 0 && liveBucket != 5 &&
                             liveBucket == prevBucket;
    if (!livePrecise) {
      if (!precise || speedClass < 2 || 0 < int(req.U(moveword::kClassWord)) || liveBucket == 0 || liveBucket == 5 ||
          liveBucket != prevBucket || k.deg45 <= std::fabs(req.F(moveword::kFacingError)) ||
          k.deg45 <= std::fabs(req.F(moveword::kTurn)))
        return;
    }
  }
  if (req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit)) return;
  req.SetF(moveword::kDwellLimit, limit * k.s0p4 <= elapsed ? 0.0f : kRestartDwell);
  req.SetU(moveword::kDwellElapsed, 0);
  req.SetU(moveword::kDwellElapsed2, 0);
  req.SetU(moveword::kDwellLimit2, 0);
  RequestState(req, MoveState::kStepLoop);
}

// -------------------------------------------------------------------------------------------------
// TURN_STOP, FUN_1406cab20 (0x1406cab20..0x1406cba5d). The stop while still facing away: it
// either turns the facing back to the heading, or decides the player must stop after all.
//
//   1. The motion's own record decides the toe-off window (req[0x3d]..0x3f) and the rate.
//   2. The first path: a small error and a stay decision turn the facing back with a step clip
//      (the band from the ground speed, the rate from the ceiling).
//   3. The second path: the class-3 and bucket gates lead to STEP_TURN_RUN or RUN_TURN_STEP.
//   4. The third path: the request's own destination re-plans into the step grid.
//   5. Otherwise the stop's own motion row (req[0x30]) is issued with IDLE, or the player
//      re-plans through FUN_1406ce820.
// -------------------------------------------------------------------------------------------------
void TurnStop(MoveHost& host, MoveReq& req) {
  const StepScalars& k = host.StepConstants();
  const StepTables& tables = host.StepTablesView();
  const int kind = int(req.U(moveword::kKind));
  const int speedClass = int(req.U(moveword::kCommandSpeedClass));
  const bool precise = req.Flag(moveword::kPrecise);
  const float facing = req.F(moveword::kFacing);
  const float fps = FramesPerSecond(host);
  const RunTables& rt = host.RunTablesView();
  const float rampFloor = host.RunConstantsView().rampSlope;

  // 1. The motion's own record: the toe-off window and the divisor (0x1406CAB95..0x1406CACBB).
  // P+0x2468 under 0.85 (0x1425AB120, COMISS at 0x1406CABC1) reads 1.0 (0x14256AE1C, 0x1406CABD2).
  const float divisor = host.RecordF2468() < k.s0p85 ? k.one : host.RecordF2468();
  const int motion = host.CurrentMotion();       // (P+0xab8 vt+0x18)()
  const int elapsed = host.CurrentClipFrames();  // (P+0xab8 vt+0x38)()
  const int start = host.ClipTicks(pes::ClipReader::kRotStart, motion, k.one, 0);  // FUN_1406b1f00
  const int end = host.ClipTicks(pes::ClipReader::kRotEnd, motion, k.one, 0);      // FUN_1406b1d00
  // ponytail: FUN_141eb3320(P+0x4d0) (the AdjustAngle adjuster's state == 2) is held clear, as RUN's turn-lock block holds it; the
  // adjuster object (P+0x4d0 vt+0x08, AS 6) is not ported, and its port replaces this constant.
  const bool adjusterBusy = false;
  if (!adjusterBusy && start <= elapsed && elapsed < (start + end * 2) / 3) {
    const int ticks = int(float(end - elapsed) / divisor + k.half);
    // FUN_14043a0a0(req[2], req[0x18]) (0x1406CAC77): the facing against the stamp, per tick, under 45 degrees (0x14259BE30).
    if (std::fabs(Wrap180(facing - req.F(moveword::kRecordStampB))) / float(ticks) < k.deg45) {
      req.SetU(moveword::kTurnLockStamp, req.U(moveword::kRecordStampB));
      req.SetByte(moveword::kTurnLockFlag, 1);
      req.SetU(moveword::kTurnLockTicks, uint32_t(ticks));
      req.SetByte(moveword::kTurnLockDir, 0);
    }
  }
  // 0x1406cad06..0x1406cad53: the distance factor is 0 up to 2.0 m (0x142565604), rises over 3.0 m (0x14258c264) to 5.0 m
  // (0x14259222c) and is 1 beyond.
  const float distance = req.F(moveword::kDistToTarget);
  const float distanceFactor = k.two < distance ? (distance < k.kmh5 ? (distance - k.two) / k.dist3 : k.one) : 0.0f;
  float ground[3];
  host.PositionAt(1, ground);
  const float groundSpeed = (fps * k.ticks60 * k.ticks60) / k.scale1000 *
                            std::sqrt(ground[0] * ground[0] + ground[1] * ground[1] +
                                      ground[2] * ground[2]);
  req.SetF(moveword::kRate, req.F(moveword::kRate) * k.rate0p9);
  // PES at 0x1406CAD06: FUN_14043a0a0(req[0x02], req[0x01]) = wrap180(kHeading - kFacing).
  // The result's magnitude (the abs mask at 0x1406CAD24) is the first path's gate.
  const float turnToTarget = Wrap180(req.F(moveword::kHeading) - facing);
  const float turnToTargetDeg = std::fabs(turnToTarget);
  const float dispTurn = Wrap180(DisplacementHeading(host) - facing);  // FUN_14043a0a0(req[2], the displacement heading)
  const float dwellLimit = k.one / fps * k.deg10;  // 0x1406cadc9: (1 / frame rate) * DAT_142588210 (10.0)

  // 2. The first path: turn the facing back with a step clip, through the run entry (vt+0 = FUN_1406d2030).
  if (req.F(moveword::kDwellElapsed) <= dwellLimit && dwellLimit != req.F(moveword::kDwellElapsed) &&
      (turnToTargetDeg < k.deg22p5 ||
       turnToTargetDeg < distanceFactor * k.deg45 + k.deg22p5)) {
    if (std::fabs(dispTurn) < k.deg22p5 && StayDecision(host, req) && !StepLetsRunTurn(host, req)) {
      // The band from the ground speed and the rate from the ceiling; the class from the rate (vt+0x68) and its speed (vt+0x48).
      const int foot = host.FootBand();
      req.SetF(moveword::kRate, kRateHigh);
      req.SetByte(moveword::kFootFlag, (foot - 1u) < 2 ? 1 : 0);  // 0x1406caeac: byte [req+0x98] = word 0x26
      req.SetU(moveword::kDwellLimit, 0);
      req.SetU(moveword::kDwellLimit2, 0);
      req.SetByte(moveword::kStyle, 0);
      if (k.kmh8 <= groundSpeed) {
        if (k.kmh13p5 <= groundSpeed) {
          if (k.kmh22 < groundSpeed) req.SetF(moveword::kRate, host.RateCeiling() * k.rate0p95);
        } else {
          req.SetF(moveword::kRate, kRateMid);
        }
      } else {
        req.SetF(moveword::kRate, kRateLow);
      }
      req.SetU(moveword::kClassWord, uint32_t(ClassForRateFor(req.F(moveword::kRate), rt)));
      req.SetF(moveword::kBandSpeed,
               RateToSpeedFor(req.F(moveword::kRate), host.PhysiqueG(), rt, rampFloor) * k.kmh3p6);
      EnterRun(host, req, foot, false);  // 0x1406caf72: (vt+0)(this+0xac0, req, foot, 0)
      return;
    }
  }

  // 3. The second path: the class-3 and bucket gates lead to the run-turn states.
  const float lookAhead = k.one / fps * k.deg10;
  if (req.F(moveword::kDwellElapsed) <= lookAhead && lookAhead != req.F(moveword::kDwellElapsed) &&
      speedClass > 0 && int(req.U(moveword::kClassWord)) > 0) {
    // 0x1406cafe7: FUN_14043a0a0(req[2], the displacement heading) is dispTurn, read at 0x1406caff1.
    const auto stepLadder = [&]() {
      if (k.kmh8 <= groundSpeed) {
        if (k.kmh13p5 <= groundSpeed) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kBandSpeed, k.kmh16);
        } else {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kBandSpeed, k.kmh11);
        }
      } else {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kBandSpeed, k.kmh5);
      }
    };
    if (precise) {
      // 0x1406cb06a: under 28.125 degrees of displacement turn (0x1425ddf94) the run-turn-step entry, else the ladder.
      if (k.deg28p125 <= std::fabs(dispTurn)) {
        stepLadder();
        if (EnterStepTurnRun(host, req)) return;
      } else {
        if (EnterRunTurnStep(host, req, (host.RateCeiling() - k.s0p6) * k.half)) return;
      }
    } else if (int(req.U(moveword::kMoveBucket)) != 0 &&
               int(req.U(moveword::kMoveBucket)) != 5) {
      // PES's non-precise arm (0x1406CB23A): past 22.5 degrees of displacement turn the class ladder and STEP_TURN_RUN
      // (0x1406CB0B6); the tick falls on when FUN_1406D38D0 declines (0x1406CB107).
      if (k.deg22p5 <= std::fabs(dispTurn)) {
        stepLadder();
        if (EnterStepTurnRun(host, req)) return;
      } else {
        // 0x1406cb268: the forward component of the displacement (FUN_140581ce0 / FUN_140591a10 rotate it by the facing), not
        // its length; the flag is the low byte of word 0xf (byte [req+0x3c]).
        const float forward = DisplacementForwardKmh(host, host.MoveTurnConstantsView());
        if (k.kmh8 < forward && k.half < req.F(moveword::kDistToTarget) && req.Flag(moveword::kTurnGateFlags)) {
          if (forward < k.kmh11 && int(req.U(moveword::kClassWord)) > 1) {
            req.SetU(moveword::kClassWord, 1);
            req.SetF(moveword::kBandSpeed, k.kmh11);
            EnterRunTurnRun(host, req);
            return;
          }
          if (forward < k.kmh16 && int(req.U(moveword::kClassWord)) > 2) {
            req.SetU(moveword::kClassWord, 2);
            req.SetF(moveword::kBandSpeed, k.kmh16);  // 0x1406cb2d8: req[0x1d] = 16.0 with the class
          }
          EnterRunTurnRun(host, req);
          return;
        }
      }
    }
  }

  // 4. The third path: a class-2+ command with no step class re-plans straight to GOTO_RUN (0x1406cb11e..0x1406cb880: the jump
  // lands on the call, past the class -1 stores).
  if (speedClass > 1 && int(req.U(moveword::kClassWord)) < 1 && StayDecision(host, req)) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }
  if (speedClass >= 0 && precise && k.dist1p5 < req.F(moveword::kDistToTarget)) {
    // 0x1406cb168..0x1406cb1bb: FUN_14043a0a0(req[2], req[3]) (the look heading against the facing) and
    // FUN_14043a0a0(req[4], req[1]) (the heading against the body direction), both inside 45 degrees.
    const float lookError = Wrap180(req.F(moveword::kLookHeading) - facing);
    const float headingError = Wrap180(req.F(moveword::kHeading) - req.F(moveword::kBodyDirection));
    if (std::fabs(lookError) < k.deg45 && std::fabs(headingError) < k.deg45) {
      // ponytail: P+0x4d0 vt+0x08 (AdjustAngleApply, movestep_turn.cpp) has no host read yet.
      req.SetByte(moveword::kTurnLockFlag, 0);  // 0x1406cb1e9: byte [req+0xf0]
      // 0x1406cb1f1: FUN_14043a0a0(req[2], req[4]) into vt+0x60 (the bucket function) with the bucket 10 and the narrow flag 0.
      const int bucket = AngleBucket(host.AngleBuckets(), Wrap180(req.F(moveword::kBodyDirection) - facing), 10, false);
      if (k.kmh5 <= groundSpeed) {
        if (k.kmh11 <= groundSpeed) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kBandSpeed, k.kmh16);
        } else {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kBandSpeed, k.kmh11);
        }
      } else {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kBandSpeed, k.kmh5);
      }
      const int band = std::clamp(int(req.U(moveword::kClassWord)), 0, 2);
      // 0x1406cb344..0x1406cb354: the speed word clamped into [5, 16].
      const float speed = std::clamp(req.F(moveword::kBandSpeed), k.kmh5, k.kmh16);
      int row = int(tables.kindBandRow[kind + band * 8]);
      if (row == kStepRowRemapFrom) row = kStepRowRemapTo;  // 0x1406cb397 CMP EDI,0x52e / 0x1406cb39d CMOVZ
      const MoveCell cell = host.Cell(row, int(tables.bucketColumn[bucket]), false);  // FUN_1406abb30 / FUN_1406abd60
      req.SetByte(moveword::kFootFlag, host.BucketIsLeft(bucket) ? 1 : 0);  // DAT_1425dd950[bucket], 0x1406cb3d8
      req.SetU(moveword::kClassWord, uint32_t(band));
      req.SetU(moveword::kPrevClassWord, uint32_t(band));
      req.SetU(moveword::kBucketPrev, uint32_t(bucket));
      req.SetU(moveword::kLiveBucket, uint32_t(bucket));
      req.SetF(moveword::kBandSpeed, speed);
      req.SetF(moveword::kBandSpeedCopy, speed);
      req.SetF(moveword::kPlayback, StepPlayback(host.PhysiqueG(), host.PlaybackConstantsView()));
      // 0x1406cb344..: vt+0x10(req, motion, cell mirror, 0, DAT_1425ddf4c, req[0x35], FUN_1406b97d0(motion)).
      WriteIssue(host, req, pes::MotionRequest{cell.motionNo, cell.mirror, 0, k.s0p1333, req.F(moveword::kPlayback),
                                               host.MotionStartTime(cell.motionNo)});
      req.SetU(moveword::kDwellElapsed, 0);
      req.SetU(moveword::kDwellLimit, 0);
      req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
      RequestState(req, MoveState::kStepLoop);
      return;
    }
  }

  // 5. The stop's own motion row (IDLE), or the re-plan.
  {
    // ponytail: PES's replay gate reads MI+0x116c (phase), +0x1554 and +0x11d4, none of which
    // MB-E ports; the anime predicate vt+0x28 == 0x4a is IsReplay().
    const bool replayOnly = host.MatchPhase() == 2 && host.IsReplay();
    if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2)) return;
    if (req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit) * k.s0p6) return;
    // PES 0x1406CB523 / 0x1406CB541: blend = FUN_1406af620(motion, 1.0), cancel0 = FUN_1406af8e0(motion, 1.0, 0); the replay dwell
    // is blend / tps (0x1406CB55C FUN_141e532d0); otherwise the stop is due when req[9] > 1 (0x1406CB570 JG), req[0x1f] < 3
    // (0x1406CB576 CMP [R13+0x7C],3 / JL) or (cancel0 + blend) / 2 <= elapsed (0x1406CB57D..0x1406CB588).
    const int blend = host.ClipTicks(pes::ClipReader::kBlend, motion, k.one, 0);
    const int cancel0 = host.ClipTicks(pes::ClipReader::kCancel, motion, k.one, 0);
    const float dwell = replayOnly ? float(blend) / host.TicksPerSecond() : req.F(moveword::kDwellLimit);
    const bool stopNow =
        !replayOnly && (speedClass > 1 || int(req.U(moveword::kClassWord)) < 3 || (cancel0 + blend) / 2 <= elapsed);
    if (speedClass > 0 && int(req.U(moveword::kClassWord)) > 0) {
      // 0x1406cb5cc: FUN_14043a0a0(req[2], the displacement heading) against 22.5 degrees.
      if (k.deg22p5 <= std::fabs(dispTurn)) {
        // 0x1406cb613: below 13.5 km/h class 0, from 13.5 class 1, from 22 class 2.
        if (groundSpeed < k.kmh13p5) {
          req.SetF(moveword::kBandSpeed, k.kmh5);
          req.SetU(moveword::kClassWord, 0);
        } else if (groundSpeed < k.kmh22) {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kBandSpeed, k.kmh11);
        } else {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kBandSpeed, k.kmh16);
        }
        if (EnterStepTurnRun(host, req)) return;
      } else if (precise &&
                 EnterRunTurnStep(host, req, (host.RateCeiling() - k.s0p6) * k.half)) {
        return;
      }
    }
    if (stopNow && StayDecision(host, req)) {
      // 0x1406cb68f..0x1406cb6e4: |wrap180(req[1] - req[2])| under 67.5 (DAT_1425cca34), and |wrap180(req[1] - req[4])| under 45
      // (XMM12 = DAT_14259be30, loaded at 0x1406cb48e / 0x1406cb179 on every path that reaches here).
      const float bodyHeadingError = Wrap180(req.F(moveword::kHeading) - req.F(moveword::kBodyDirection));
      if (!precise && turnToTargetDeg < k.deg67p5 && std::fabs(bodyHeadingError) < k.deg45) {
        // 0x1406cb6ea..0x1406cb7eb: the foot flag and the run entry with the foot band; the rate is 0.3 (0x3e99999a).
        // ponytail: PES reads the two feet through FUN_141eb51a0 indices 3 and 6, rotates them by the facing and, when the
        // motion has no hit record (FUN_1406afbc0 == 0) and the left foot is behind the right, takes foot band 2; GF has no
        // pose bones, so the foot flag is a coin flip from the player's own generator until the two indices are ported.
        const int foot = host.FootBand();
        float left[3], right[3];
        host.PositionAt(3, left);
        host.PositionAt(6, right);
        const int footPick = host.PickN(2);
        req.SetByte(moveword::kFootFlag, footPick == 1 ? 1 : 0);
        req.SetU(moveword::kDwellLimit, 0);
        req.SetU(moveword::kDwellLimit2, 0);
        req.SetByte(moveword::kStyle, 0);
        EnterRun(host, req, foot, false);
        req.SetF(moveword::kRate, kRateMid);
        return;
      }
      // 0x1406cb83d..0x1406cb868: the dwell is not over and |wrap180(req[1] - req[4])| (the heading against the body direction)
      // is at least 22.5 (XMM8 = DAT_14259d618): the tick is kept.
      if (req.F(moveword::kDwellElapsed) < dwell && k.deg22p5 <= std::fabs(bodyHeadingError)) return;
    } else {
      if (req.F(moveword::kDwellElapsed) < dwell) return;
      if (!StayDecision(host, req)) {
        if (int(req.U(moveword::kLookBucketAlt)) != 0 &&
            int(req.U(moveword::kLookBucketAlt)) != 5 &&
            k.deg33p75 <= std::fabs(req.F(moveword::kFacingError)) &&
            int(req.U(moveword::kCommandId)) != 0x79) {
          return;  // ponytail: FUN_1406d1390 (0x1406cb8e9) is not translated; the handler keeps the tick.
        }
        if (elapsed < blend) return;  // 0x1406CB8F3 CMP R14D,R15D / JL: the clip has not reached its blend tick
        // The idle row: 0x1406cb8fc..0x1406cb9f6.
        req.SetByte(moveword::kMotionPending, 1);
        const int row = kind < 8 ? IdleRow(host, req) : 0x5a1;
        const MoveCell cell = host.Cell(row, 9, false);  // FUN_1406abb30(row, 9)
        bool mirror = host.Percent(50);                  // FUN_1406ebf30(P, 0x32), always drawn
        if (kind != 3 && kind != 6) {
          // 0x1406cb973: the column is 9 when req[7] is above -0.5 (DAT_14258d694), else 8; the mirror is that cell's.
          const int column = host.RunConstantsView().negHalf < req.F(moveword::kFacingError) ? 9 : 8;
          mirror = host.Cell(row, column, false).mirror;  // FUN_1406abd60
        }
        req.SetU(moveword::kMotion, uint32_t(cell.motionNo));
        req.SetU(moveword::kDwellElapsed, 0);
        req.SetU(moveword::kDwellLimit, 0);
        req.SetU(moveword::kDwellElapsed2, 0);
        req.SetByte(moveword::kMirror, mirror ? 1 : 0);
        req.SetF(moveword::kDuration, k.s0p2);       // 0x1406cb9be: 0x3e4ccccd = 0.2
        req.SetU(moveword::kStartTime, 0);
        req.SetF(moveword::kPlayback, k.one);
        req.SetF(moveword::kDwellLimit2, k.s0p2);    // 0x1406cb9d4..0x1406cb9df
        req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
        RequestState(req, MoveState::kIdle);
        req.SetByte(moveword::kTurnLockFlag, 0);     // 0x1406cb9f6: byte [req+0xf0]
        // ponytail: P+0x4d0 vt+0x08 (AdjustAngleApply) has no host read yet. The motion-pending byte stays 1: PES leaves it for
        // the tail's issue.
        return;
      }
    }
  }

  // 0x1406cb872: the class -1 and a zero speed word, then the re-plan.
  req.SetU(moveword::kClassWord, 0xffffffffu);
  req.SetF(moveword::kBandSpeed, 0.0f);
  EnterGotoRun(host, req, /*runStopVariant=*/false);
}

}  // namespace movestep
}  // namespace pes
