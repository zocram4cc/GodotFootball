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

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pes {
namespace movestep {
namespace {

// The keep-tick tail of STEP_LOOP (PES's LAB_1406c5b79 fall-through), defined after StepLoop.
void StepLoopKeepsTick(MoveHost& host, MoveReq& req, int bucket, float turn);

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

// FUN_140141e532d0: the logic-clock draw in [0, 1). The handlers only scale it, so the scale
// (one tick = 1 / ticksPerSecond) is the only thing this port needs.
float ClockStep(MoveHost& host) { return 1.0f / host.TicksPerSecond(); }

// DAT_14259be40 (57.2957..): FUN_14046f260's radians-to-degrees scale.
constexpr float kDegToRad = 57.2957763671875f;

// STEP_LOOP_CHANGE's three rate immediates, each from the instruction that stores it into
// req[0x4a] (opcode-checked, as the owner rules allow). 0x3f19999a, 0x3e99999a, 0x3e19999a.
constexpr float kRateHigh = 0.6000000238418579f;  // STEP_LOOP_CHANGE 0x1406c66f9
constexpr float kRateMid = 0.44999998807907104f;  // STEP_LOOP_CHANGE 0x1406c676e
constexpr float kRateLow = 0.15000000596046448f;  // STEP_LOOP_CHANGE 0x1406c675c

// FUN_1406b9ff0(interface, class, speed, kind): the rate for a class/speed pair. req[0xb] is the
// "use the class rate" flag; the rate comes from FUN_1406b9e80's 10-knot table, which the
// contract exposes as SpeedToRate with the same argument order.
float RateFor(MoveHost& host, float classRate, float speedKmh, int speedClass, int kind) {
  if (classRate != 0.0f) return SpeedToRate(host, float(speedClass), 0);
  if (speedClass == 0 && (kind == 3 || kind == 6)) return 0.0f;
  return SpeedToRate(host, speedKmh, 0);
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
  const float tick = ClockStep(host);
  float blended = 0.0f;
  if (tick * req.F(moveword::kDwellLimit2) != 0.0f)
    blended = req.F(moveword::kRecordStampA) / (tick * req.F(moveword::kDwellLimit2));

  // 2. The rate target and the walk toward it.
  const float target = RateFor(host, req.F(moveword::kUseClassRate),
                               req.F(moveword::kDesiredSpeedKmh),
                               int(req.U(moveword::kCommandSpeedClass)),
                               int(req.U(moveword::kKind)));
  // PES's own walk: on the rise, a step of 0.05 that overshoots the target leaves the rate at
  // 0.0 (its `fVar5 = 0.0` in the clamp's no-branch); on the fall, the step stops at the
  // ceiling and otherwise keeps the lowered value.
  const float rate = req.F(moveword::kRate);
  if (rate <= target) {
    const float raised = rate + k.step0p05;
    req.SetF(moveword::kRate, raised <= target && 0.0f <= raised ? raised : 0.0f);
  } else {
    const float ceiling = host.RateCeiling();
    const float lowered = rate - k.step0p05;
    req.SetF(moveword::kRate, lowered <= ceiling && target <= lowered ? lowered : target);
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
      req.SetF(moveword::kBandSpeedPrev, req.F(moveword::kBandSpeedCopy));
    }
    const float bodyTurn = Wrap180(req.F(moveword::kFacing) - req.F(moveword::kLookHeading));
    if (k.deg22p5 <= std::fabs(bodyTurn) ||
        (int(req.U(moveword::kClassWord)) < 1 &&
         int(req.U(moveword::kCommandSpeedClass)) == -1)) {
      EnterStop(host, req);
    } else if (int(req.U(moveword::kCommandSpeedClass)) == -1 ||
               req.F(moveword::kDistToTarget) <= k.one) {
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
    if (req.F(moveword::kDwellLimit2) * k.s0p6 <= req.F(moveword::kDwellElapsed2) &&
        k.deg31p64 < headingError * headingError) {
      // The drift is clamped to what the clock can cover, and FUN_140470d60 keeps the
      // shortest-way side of the wrapped heading.
      const float limit = k.deg22p5 / tick;
      float delta = headingError;
      if (limit < delta) {
        delta = -limit;
        if (-limit <= headingError) delta = headingError;
      }
      req.SetF(moveword::kRootFacingAngle, FoldHalfTurn(drift + delta));
      req.SetByte(moveword::kRootFacing, 1);
    }
    // PES's exit gate at 0x1406C1343: the second dwell run out, or the dwell past three
    // quarters (0x14259BDD4) with a heading error past 1139.0625 deg^2 (0x1425DDFD4), the
    // precise flag set and the command speed class below the live class word.
    if (req.F(moveword::kDwellLimit2) <= req.F(moveword::kDwellElapsed2) ||
        (req.F(moveword::kDwellLimit2) * k.threeQuarters <= req.F(moveword::kDwellElapsed2) &&
         k.sq1139 < headingError * headingError && req.Flag(moveword::kPrecise) &&
         int(req.U(moveword::kCommandSpeedClass)) < int(req.U(moveword::kClassWord)))) {
      req.SetU(moveword::kDwellElapsed, 0);
      req.SetU(moveword::kDwellLimit, 0);
      req.SetU(moveword::kDwellElapsed2, 0);
      req.SetU(moveword::kDwellLimit2, 0);
      req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
      RequestState(req, MoveState::kStepLoop);
      req.SetByte(moveword::kMotionPending, 0);
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
  const float tick = ClockStep(host);
  float blended = 0.0f;
  if (tick * req.F(moveword::kDwellLimit) != 0.0f)
    blended = req.F(moveword::kRecordStampA) / (tick * req.F(moveword::kDwellLimit));

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
        const float speed = (tick * k.ticks60 * k.ticks60) / k.scale1000 *
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
        if (!EnterRunTurnStep(host, req, (host.RateCeiling() - k.s0p6) * k.half)) return;
      }
    }
    req.SetF(moveword::kRecordStampA, blended);
    const float dwellLeft = req.F(moveword::kDwellLimit) - req.F(moveword::kDwellElapsed);
    req.SetF(moveword::kRecordStampB, dwellLeft <= 0.0f ? 0.0f : dwellLeft);
    EnterStop(host, req);
    return;
  }

  if (1 < speedClass && !req.Flag(moveword::kPrecise) && int(req.U(moveword::kClassWord)) < 1) {
    // A class 2..4 non-precise step may accelerate instead of blending. FUN_1406ccd00 returns
    // whether it wrote the request; when it refuses, the handler re-plans through
    // FUN_1406ce820, exactly as PES's call site does.
    const float bodyTurn = Wrap180(req.F(moveword::kFacing) - req.F(moveword::kBodyDirection));
    const float lookDelta = req.F(moveword::kLookDelta);
    if (2 < speedClass && -bodyTurn < k.deg22p5 && -lookDelta < k.deg22p5 &&
        EnterAccelerate(host, req))
      return;
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }

  // 4. The blend: the wrapped body heading, drifted by the blended turn so far.
  float drift = facingTurn + blended;
  if (0.0f <= drift) {
    if (k.full <= drift) drift += k.wrapFull;
  } else {
    drift += k.full;
  }
  req.SetF(moveword::kRootFacingAngle, drift);
  req.SetByte(moveword::kRootFacing, 1);

  // The drift gate: the clock covers 270 degrees per tick; below that the heading error is
  // split over the remaining dwell and clamped by FUN_140470d60's shortest-way fold.
  const float draw = k.deg270 / tick;
  if (blended * blended < draw * draw && k.deg31p64 < remaining * remaining) {
    const float span = tick * (req.F(moveword::kDwellLimit2) - req.F(moveword::kDwellElapsed2));
    float error = remaining;
    if (span != 0.0f) error = remaining / span;
    float delta = draw - std::fabs(blended);
    if (error <= delta) {
      delta = -delta;
      if (-delta <= error) delta = error;
    }
    // FUN_140470d60 keeps the shortest-way side, so on the wrapped path the result is the
    // folded sum; the handler adds the delta to req[0x18] either way.
    req.SetF(moveword::kRootFacingAngle, FoldHalfTurn(drift + delta));
    req.SetByte(moveword::kRootFacing, 1);
    req.SetF(moveword::kRecordStampB, req.F(moveword::kRecordStampB) + delta);
  }

  if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2)) return;
  req.SetU(moveword::kDwellElapsed, 0);
  req.SetU(moveword::kDwellLimit, 0);
  req.SetU(moveword::kDwellElapsed2, 0);
  req.SetU(moveword::kDwellLimit2, 0);
  req.SetU(moveword::kMoveBucket, 0);
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  RequestState(req, MoveState::kRunLoop);
  req.SetByte(moveword::kMotionPending, 0);
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
  const float tick = ClockStep(host);

  // 1. The band walk: `if (req[0x1f] == 0 && req[0x24] == 1 && 0 < req[0x2e])` in PES.
  if (int(req.U(moveword::kClassWord)) == 0 && int(req.U(moveword::kPrevClassWord)) == 1 &&
      0.0f < req.F(moveword::kDwellLimit2)) {
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

  // 2. The band of the tick and the blended band speed.
  const int prevState = host.MoveStatePrevious();
  int classBand = int(req.U(moveword::kCommandSpeedClass));
  if (classBand < 3) classBand = std::max(classBand, 0);
  if ((prevState == 19 || prevState == 14) && req.F(moveword::kDwellElapsed) <= k.deg10 &&
      k.deg10 != req.F(moveword::kDwellElapsed)) {
    classBand = int(req.U(moveword::kClassWord));
    if (classBand < 3) classBand = std::max(classBand, 0);
  }
  const float desired = req.F(moveword::kDesiredSpeedKmh);
  float speed = k.kmh16;
  if (desired <= k.kmh16 && k.kmh5 <= desired) speed = desired;
  // PES reads `param_2[0x1f]` at each use, so the band walk above is visible here.
  const int liveBand = int(req.U(moveword::kClassWord));
  int nextBand = classBand;
  if (liveBand != classBand) nextBand = classBand <= liveBand ? liveBand + 1 : liveBand - 1;

  // 3. The blended band speed and the ground speed.
  float blendedSpeed = speed * k.half + req.F(moveword::kBandSpeed) * k.half;
  float ground[3];
  host.PositionAt(1, ground);
  const float groundSpeed = (tick * k.ticks60 * k.ticks60) / k.scale1000 *
                            std::sqrt(ground[0] * ground[0] + ground[1] * ground[1] +
                                      ground[2] * ground[2]);

  // 4. The slow-player re-plan.
  if (req.U(0x3f) != 0 && liveBand < 3 && !precise && groundSpeed < k.kmh16) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }

  // 5. The turn into a run, and the stop.
  if (EnterStepTurnRun(host, req)) return;
  const float turnToTarget = Wrap180(req.F(moveword::kLookHeading) - req.F(moveword::kHeading));
  // FUN_1406d6290 (the sidestep trap) reads the team's marking flag (ctx T+0x255) and the
  // offside line, which MB-C does not port yet; it cannot fire before then.
  // ponytail: port FUN_1406d6290 when the team context reaches GF.
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
  bool bucketMoved = false;
  bool slowTurn = false;
  const float bucketAngle = tables.bucketDegrees[int(req.U(moveword::kMoveBucket))];
  float turned = bucketAngle + facing;
  if (0.0f <= turned) {
    if (k.full <= turned) turned += k.wrapFull;
  } else {
    turned += k.full;
  }
  float bodyTurn = Wrap180(turned - req.F(moveword::kHeading));
  if (bodyTurn <= k.deg90 && k.neg90 <= bodyTurn) {
    if (k.deg90 < bodyTurn) bodyTurn = k.deg90;
  } else {
    bodyTurn = 0.0f;
  }
  if (k.sq1139 < bodyTurn * bodyTurn) {
    bucketMoved = true;
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
  const float turnToTargetDeg = Wrap180(req.F(moveword::kLookHeading) - req.F(moveword::kHeading));
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
  const float lookAhead = Wrap180(facing + tables.bucketDegrees[bucket] - req.F(moveword::kHeading));
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
          drift = k.halfTurn / (tick * 2.0f);
          if (0.0f < lookDelta) drift = k.neg180;
        }
      }
    }
  } else {
    drift = Wrap180(lookAhead + facing - req.F(moveword::kLookHeading));
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

  // The row and its keep-variant fold, then the column's cell.
  int row = int(tables.kindBandRow[kind + nextBand * 8]);
  if (row == 0x52e) row = 0x530;
  const MoveCell cell = host.Cell(row, int(tables.bucketColumn[column]), false);
  const int motion = cell.found ? cell.motionNo : row;

  // The two tails. PES's first test is
  //     if ((!bVar20) && (local_res8[0] == 0 && !bVar19)) goto LAB_1406c5eba;
  // i.e. nothing moved, so the tick is simply kept. `bVar20` is the bucket moved by the
  // look-bucket crossover or the column re-fold; `local_res8` the drift taken from the sampled
  // ball position; `bVar19` the wide turn that lowered the band.
  // Then the band gate decides which tail runs:
  //     if (A) { if (req[0x1a] < req[0x1b] * 0.25) goto keep; ...issue... }
  //     else   { ...keep the tick, ask for STEP_TURN_RUN... }
  // with
  //     A = (band < 0) || ((nextBand < 3 && ((nextBand < 2 || kind == 3) || kind == 6)) || !precise)
  //         || ((1 < bucket - 3 && 1 < bucket - 8) || (turnToLook <= drift || fVar24 * 22.5 + 45 <= |bodyToTarget|))
  const float bodyToTarget = Wrap180(req.F(moveword::kFacing) - req.F(moveword::kHeading));
  const int band0 = int(req.U(moveword::kClassWord));
  const float distance = req.F(moveword::kDistToTarget);
  const float distanceFactor =
      k.kmh5 < distance ? (distance < k.kmh5 ? (distance - k.kmh5) / k.dist3 : 1.0f) : 0.0f;
  const float driftLimit = drift;  // fVar28, the drift after the window clamp
  const bool gateA =
      band0 < 0 || ((nextBand < 3 && ((nextBand < 2 || kind == 3) || kind == 6)) || !precise) ||
      ((1 < bucket - 3 && 1 < bucket - 8) ||
       (driftLimit <= k.deg67p5 ||
        distanceFactor * k.deg22p5 + k.deg45 <= std::fabs(bodyToTarget)));
  if (!bucketMoved && req.U(0x3f) == 0 && !slowTurn) {
    // LAB_1406c5eba: keep the tick, record only the band and the bucket.
    req.SetU(moveword::kPrevClassWord, uint32_t(liveBand));
    req.SetU(moveword::kBandSpeedCopy, uint32_t(blendedSpeed));
    req.SetU(moveword::kClassWord, uint32_t(nextBand));
    req.SetU(moveword::kMoveBucket, uint32_t(bucket));
    return;
  }
  if (gateA) {
    if (req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit) * k.s0p25) {
      req.SetU(moveword::kPrevClassWord, uint32_t(liveBand));
      req.SetU(moveword::kBandSpeedCopy, uint32_t(blendedSpeed));
      req.SetU(moveword::kClassWord, uint32_t(nextBand));
      req.SetU(moveword::kMoveBucket, uint32_t(bucket));
      return;
    }
    if (!precise && int(req.U(moveword::kClassWord)) > 0) req.SetU(moveword::kKind, 3);
  } else {
    StepLoopKeepsTick(host, req, bucket, turn);
    return;
  }

  // The clip tail. PES's own order: FUN_1406b97d0's start time and the mirror from FUN_1406abd60,
  // the request words 0x17/0x18 the headings, the table's motion, the blend, the playback, the
  // dwell reset, the rate walk, the recorded band and bucket, and STEP_LOOP_CHANGE (req[0x39] = 5).
  // The mirror PES reads back from the interface's own choice (vt+0x30), which the port stands
  // in with the table's left flag.
  const float startTime = host.MotionStartTime(motion);
  const float clipBlend =
      (!precise && req.F(moveword::kLookDelta) <= k.deg11p25) ||
              (k.deg67p5 < req.F(moveword::kLookDelta) && !precise)
          ? k.s0p2
          : k.s0p1333;
  // FUN_1406c4f59's call: rateHeld is 0 there.
  const int group = LocomotionStyle(host, host.AnimeState(), req.F(moveword::kRate), req,
                                    host.RateCeiling(), false);
  pes::MotionRequest request;
  request.motion = motion;
  request.mirror = host.BucketIsLeft(bucket);
  request.group = group;
  request.duration = clipBlend;
  request.playback = StepPlayback(host.PhysiqueG(), host.PlaybackConstantsView());
  request.startTime = startTime;
  host.Issue(request);
  req.SetF(moveword::kRecordStampA, Wrap180(facing - req.F(moveword::kHeading)));
  req.SetF(moveword::kRecordStampB, req.F(moveword::kHeading));
  req.SetU(moveword::kDwellElapsed, 0);
  req.SetU(moveword::kDwellLimit, precise ? 0u : uint32_t(clipBlend));
  const float rateTarget = RateFor(host, req.F(moveword::kUseClassRate), blendedSpeed,
                                   nextBand, kind);
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
  req.SetByte(moveword::kMotionPending, 0);
}

// The keep-tick tail PES runs when the band gate fails (LAB_1406c5b79's fall-through): the
// request keeps its headings, the drift is re-blended, the row is the stop row, and the state
// asks for STEP_TURN_RUN (req[0x39] = 0xf).
namespace {
void StepLoopKeepsTick(MoveHost& host, MoveReq& req, int bucket, float turn) {
  const StepScalars& k = host.StepConstants();
  const StepTables& tables = host.StepTablesView();
  // ponytail: PES samples the match's ball-position list (ctx+0x40 through FUN_140a6cf70) for
  // the drift's heading, and reads the team's offside line (ctx T+0x255) to widen the two
  // windows below; neither is ported, so the port uses the player's own displacement and leaves
  // the windows at their default.
  const float ballHeading = DisplacementHeading(host);
  const float turnToBall = Wrap180(req.F(moveword::kFacing) - ballHeading);
  const float lookError =
      Wrap180(req.F(moveword::kFacing) - tables.bucketDegrees[bucket]);
  float wide = k.deg33p75, narrow = k.deg146p25;
  if (turn <= narrow) {
    if (turn <= k.deg168p75) {
      // the look bucket decides which of the two errors the mirror uses
    } else {
      narrow = k.deg140p625;
    }
  }
  (void)turnToBall;
  (void)lookError;
  (void)wide;
  (void)narrow;
  // The row: the stop row for this bucket, and the mirror from the sameness of the two errors.
  const int row = bucket == 3 || bucket == 8 ? 0x639 : 0x631;
  const MoveCell cell = host.Cell(row, int(tables.bucketColumn[bucket]), false);
  const int motion = cell.found ? cell.motionNo : row;
  const bool mirror = turn <= 0.0f;
  RequestState(req, MoveState::kStepTurnRun);
  req.SetU(moveword::kMotion, uint32_t(motion));
  req.SetByte(moveword::kMirror, mirror ? 1 : 0);
  req.SetF(moveword::kPlayback, StepPlayback(host.PhysiqueG(), host.PlaybackConstantsView()));
  req.SetByte(moveword::kMotionPending, 0);
}
}  // namespace
// -------------------------------------------------------------------------------------------------
// EnterTurnStop, FUN_1406d2590 (0x1406d2590..0x1406d2e64), the TURN_STOP entry. RUN_TO_STEP and
// TURN_STOP itself call it. It picks the stop row's bucket from the player's own motion and the
// body turn, sets the request (motion, mirror, blend, playback, start time) and enters TURN_STOP.
//
//   1. The playback speed and the mirror come from the interface's own answers (vt+0x50 for the
//      speed, vt+0x30/0x50 for the mirror), with the sameness test FUN_140a62960 drives.
//   2. The bucket: req[0xd] normally, or req[0xc] when the class is not negative and the request
//      is not precise; and the turn: req[7] normally, or req[5] in that same case.
//   3. In play (phase 1), with the bucket 3/4 or 8/9 and the turn past 146.25 deg, PES re-aims
//      at a sampled ball position (ctx+0x40 through FUN_140a6cf70), recomputes the body turn,
//      and may flip the bucket to its mirrored pair.
//   4. The row comes from the player's own motion (the interface's vt+0x18, a motion id) for the
//      special groups, or from the turn and the body direction for the rest.
//   5. The mirror, blend and playback follow the chosen motion's own group; the tail writes the
//      recorded band/bucket/speed and enters TURN_STOP.
// -------------------------------------------------------------------------------------------------
void EnterTurnStop(MoveHost& host, MoveReq& req) {
  const StepScalars& k = host.StepConstants();
  const StepTables& tables = host.StepTablesView();
  const float facing = req.F(moveword::kFacing);
  const bool precise = req.Flag(moveword::kPrecise);
  const int speedClass = int(req.U(moveword::kCommandSpeedClass));

  // 2. The bucket and turn the entry starts from.
  int bucket = int(req.U(moveword::kLookBucketAlt));  // req[0xd]
  float turn = req.F(moveword::kFacingError);         // req[7]
  if (speedClass >= 0 && !precise) {
    bucket = int(req.U(moveword::kLookBucket));       // req[0xc]
    turn = req.F(moveword::kLookDelta);               // req[5]
  }
  // 1. The playback speed: the +0xac0 interface's vt+0x50 answer (FUN_1406e9f10), which is
  // TurnPlayback(G, p); p is the interface's own (mirror-argument side test), answered here by
  // MirrorChoice. It is never the band's km/h speed: writing a speed into req[0x35] latches a
  // zero playback into the record of a standing player, and his clip never advances.
  req.SetF(moveword::kPlayback, pes::TurnPlayback(host.PhysiqueG(),
                                                  host.MirrorChoice(false) ? 1.0f : 0.5f,
                                                  host.PlaybackConstantsView()));
  int band = int(req.U(moveword::kClassWord));
  band = std::min(std::max(band, 0), 3);

  // 3. In play, the look-behind re-aim and the mirrored-bucket flip.
  if (host.MatchPhase() == 1 &&
      ((bucket - 3 < 2 || bucket - 8 < 2) && k.deg146p25 < std::fabs(turn))) {
    // ponytail: PES samples the match's ball-position list (ctx+0x40 through FUN_140a6cf70) for
    // the re-aim heading; GF has no such list, so the port aims at the player's own displacement.
    float ball[3];
    host.PositionAt(2, ball);
    float aim = Atan2Delta(ball[0], ball[2]) / kDegToRad;
    if (aim < 0.0f) aim += 360.0f;
    // PES: `uVar9 = FUN_140a627e0(ctx, 0)` (the player's record slot), and when it is past 21
    // (an outfield player, not an official) and the turn to the look point is inside
    // (22.5, 157.5) degrees, the re-aim aims at the look heading instead of the ball.
    // ponytail: FUN_140a627e0's slot is the roster index; GF's Player::GetID() is not the same
    // numbering, so the port uses the record slot the adapter supplies through RecentStamp(1).
    const int slot = host.RecentStamp(1);
    const float toLook = std::fabs(Wrap180(facing - req.F(moveword::kLookHeading)));
    if (0x15 < slot && k.deg22p5 < toLook && toLook < 157.5f) {
      const float look = req.F(moveword::kLookHeading);
      aim = 0.0f <= look ? (k.full <= look ? look + k.wrapFull : look) : look + k.full;
    }
    float reAim = aim - facing;
    if (k.halfTurn < std::fabs(reAim)) reAim -= (reAim < 0.0f ? k.negOne : k.one) * k.full;
    // The flip: PES compares the sign of the entry's own turn against the sign of the re-aimed
    // body direction (both folded), and flips the bucket to its mirrored pair when they differ.
    const float a = turn < 0.0f ? k.negOne : k.one;
    const float b = reAim < 0.0f ? k.negOne : k.one;
    if (a != b) {
      turn = k.neg180;
      if (1 < bucket - 3) turn = k.halfTurn;
      bucket = 9;
      if (1 < bucket - 3) bucket = 4;
    }
  }

  // The body direction: P+0x28 minus P+0x20, folded.
  float bodyDir = req.F(moveword::kBodyDirection) - facing;
  if (k.halfTurn < std::fabs(bodyDir)) bodyDir -= (bodyDir < 0.0f ? k.negOne : k.one) * k.full;
  const float bodyError = std::fabs(bodyDir);

  // 4. The row's bucket. PES switches on the player's own motion id (the interface's vt+0x18):
  //    LAB_1406d299d (the body-turn path) for the special groups, and direct otherwise.
  //    In the body-turn path, `fVar21` is the body turn's magnitude (the (x, z) difference folded),
  //    `fVar22` is 22.5 deg, `fVar13` the folded body direction and `fVar16` the entry's turn.
  int bucketSet = 0;  // 1 = the bucket was set by the bucket function below
  switch (host.CurrentMotion()) {
    case 0xb4f:
    case 0xb52:
    case 0xb5a:
    case 0xbc3:
    case 0xc05:
    case 0xc3d:
    case 0xb48:
    case 0x8ba:
    case 0x8ca:
      goto BodyTurnBucket;
    case 0x8c7: {
      const float side = 0.0f < turn ? k.deg45 : k.neg45;
      if (k.deg45 < std::fabs(turn)) {
        bucket = AngleBucket(host.AngleBuckets(), side, 10, false);
        goto BucketSet;
      }
      break;
    }
    default:
      break;
  }
  if (host.CurrentMotion() < 0x4f) {
    const int motionGroup = host.CurrentMotion() - 0x25;
    if (motionGroup < 0x2a && ((0x24800000001ull >> (motionGroup & 0x3f)) & 1) != 0)
      goto BodyTurnBucket;
  }

  // LAB_1406d2965: the entry keeps its own turn, and the bucket is not set here.
  goto BucketDone;

BodyTurnBucket:
  if (bodyError <= k.deg33p75) {
    if (k.deg22p5 < bodyError) {
      float side = k.neg90;
      if (0.0f < bodyDir) side = k.deg90;
      const float signA = turn < 0.0f ? k.negOne : k.one;
      const float signB = side < 0.0f ? k.negOne : k.one;
      if (signA != signB) {
        bucket = AngleBucket(host.AngleBuckets(), side == 0.0f ? 0.0f : -side, 10, false);
        goto BucketSet;
      }
    }
  } else {
    // The wide angle: the side and the window follow the body direction's sign.
    float wide = k.neg180;
    float window = k.neg45;
    if (0.0f < bodyDir) {
      wide = k.halfTurn;
      window = k.deg45;
    }
    if (k.deg146p25 <= std::fabs(turn)) {
      const float signA = turn < 0.0f ? k.negOne : k.one;
      const float signB = window < 0.0f ? k.negOne : k.one;
      if (signA != signB) {
        bucket = AngleBucket(host.AngleBuckets(), wide, 10, false);
        goto BucketSet;
      }
    } else {
      const float signA = turn < 0.0f ? k.negOne : k.one;
      const float signB = window < 0.0f ? k.negOne : k.one;
      if (signA != signB) {
        bucket = AngleBucket(host.AngleBuckets(),
                             FoldHalfTurn(window == 0.0f ? 0.0f : -window), 10, false);
        goto BucketSet;
      }
    }
  }
  goto BucketDone;

BucketSet:
  bucketSet = 1;
BucketDone:;
  (void)bucketSet;

  // 5. The row id and the tail's writes.
  int kind = int(req.U(moveword::kKind));
  if (host.RecentStamp(0) >= 0 && host.RecentStamp(0) < 0x16 &&
      ((int(req.U(moveword::kCommandId)) - 0x33u < 2 ||
       int(req.U(moveword::kCommandId)) == 0x4d) &&
      kind - 2u < 2)) {
    kind = 1;
  }
  const int rowId = int(tables.kindBandRow[kind + band * 8]);
  const MoveCell cell = host.Cell(rowId, int(tables.bucketColumn[bucket]), false);
  const int motion = cell.found ? cell.motionNo : rowId;
  // ponytail: PES reads the mirror and the blend from the interface's own answers
  // (vt+0x30 with FUN_140a62960's sameness, and vt+0xd0's motion-record walk); GF has neither
  // record, so the mirror is the table's left flag and the blend the default.
  const bool mirror = host.MirrorChoice(false);
  float blend = k.s0p1667;
  const int motionGroup = motion >= 0xbee && motion - 0xbee < 0x36
                              ? int((0x30000000000011ull >> ((motion - 0xbee) & 0x3f)) & 1)
                              : 0;
  if (motionGroup != 0 || motion == 0xbc2 || (motion >= 0xb45 && motion - 0xb45 < 2)) {
    blend = k.s0p2;
    if (k.deg22p5 < bodyError &&
        ((0.0f < bodyDir && mirror) || (bodyDir < 0.0f && !mirror)))
      blend = k.s0p25;
  }
  pes::MotionRequest request;
  request.motion = motion;
  request.mirror = mirror;
  request.transition = 0;
  request.duration = blend;
  request.playback = req.F(moveword::kPlayback);
  request.startTime = 0.0f;
  host.Issue(request);
  RequestState(req, MoveState::kTurnStop);
  float window = k.deg33p75;
  if (bucket - 3 < 2 || bucket - 8 < 2) window = k.deg11p25;
  float look = tables.bucketDegrees[bucket] + window;
  if (bodyError <= look) {
    look = tables.bucketDegrees[bucket] - window;
    if (look <= bodyError) look = bodyError;
  }
  req.SetF(moveword::kRecordStampB, look + facing);
  req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
  req.SetU(moveword::kBandSpeedCopy, req.U(moveword::kBandSpeed));
  req.SetU(moveword::kBucketPrev, req.U(moveword::kMoveBucket));
  req.SetU(moveword::kMoveBucket, uint32_t(bucket));
  req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
  req.SetByte(moveword::kMotionPending, 0);
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
  const int kind = int(req.U(moveword::kKind));
  const bool precise = req.Flag(moveword::kPrecise);
  const int speedClass = int(req.U(moveword::kCommandSpeedClass));
  const float facing = req.F(moveword::kFacing);
  const float tick = ClockStep(host);

  // 1. The blended heading integral, and the drift's weight.
  float weight = 1.0f;
  float blended = 0.0f;
  if (req.F(moveword::kDwellLimit2) == 0.0f) {
    req.SetU(moveword::kBlendA, 0);
    req.SetU(moveword::kBlendB, 0);
  } else {
    const float prev = std::min(std::max(req.F(moveword::kRate), 0.0f), 1.0f);
    SpeedToRate(host, float(int(req.U(moveword::kPrevClassWord))), 0);
    const float live = std::min(std::max(req.F(moveword::kRate), 0.0f), 1.0f);
    SpeedToRate(host, float(int(req.U(moveword::kClassWord))), 0);
    if (req.F(moveword::kDwellElapsed2) / req.F(moveword::kDwellLimit2) <= k.one)
      weight = req.F(moveword::kDwellElapsed2) / req.F(moveword::kDwellLimit2);
    const float share = (live - prev) * weight + prev;
    blended = share - live;
    if (std::fabs(blended) < k.eps1e7) blended = 0.0f;
    const float held = req.F(moveword::kDwellLimit2) - req.F(moveword::kDwellLimit2) * weight;
    req.SetU(moveword::kBlendB, uint32_t(held));
    req.SetU(moveword::kBlendA, uint32_t(held * blended * tick + held * share));
  }

  // 2/3. The band and the rate, then the two run-turn entries.
  const float turnToTarget = Wrap180(req.F(moveword::kRecordStampB) - req.F(moveword::kHeading));
  if (precise && speedClass < 3 && k.deg22p5 < std::fabs(turnToTarget) &&
      int(req.U(moveword::kMoveBucket)) == int(req.U(moveword::kBucketPrev))) {
    if (k.deg45 <= std::fabs(Wrap180(req.F(moveword::kFacing) - DisplacementHeading(host)))) {
      // The ground speed decides the band, in km/h: the clock times the displacement length.
      float d[3];
      host.PositionAt(0, d);
      const float speed = (tick * k.ticks60 * k.ticks60) / k.scale1000 *
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
      // The rate and the band from the ground speed's own band (PES's three constants are the
      // exe floats 0x3f19999a / 0x3e99999a / 0x3e19999a, i.e. 0.6, 0.45 and 0.15; the loader
      // carries 0.6 as s0p6 and the other two are pinned in the scalar table below).
      const float ceiling = host.RateCeiling();
      req.SetF(moveword::kRate, kRateHigh);
      float d[3];
      host.PositionAt(0, d);
      const float speed = (tick * k.ticks60 * k.ticks60) / k.scale1000 *
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
      const int band = host.BandSpeedKmh(int(req.F(moveword::kRate)), host.PhysiqueG());
      req.SetU(moveword::kClassWord, uint32_t(band));
      req.SetF(moveword::kBandSpeed, host.RateCeiling() * k.s0p6 * k.kmh3p6);
      // FUN_1406d2f60's threshold: (rate ceiling - 0.6) * the clock step.
      if (EnterRunTurnStep(host, req, (ceiling - k.s0p6) * tick)) return;
    }
  }

  // 4. The arrival flag: does the change complete, or does the loop re-plan?
  float d[3];
  host.PositionAt(1, d);
  const float groundSpeed = (tick * k.ticks60 * k.ticks60) / k.scale1000 *
                            std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (req.U(0x3f) == 0 || int(req.U(moveword::kClassWord)) > 1 || !precise) {
    if (!precise && 1 < int(req.U(moveword::kCommandSpeedClass)) &&
        0 < int(req.U(moveword::kClassWord))) {
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
  } else if (groundSpeed * k.kmh16 < 0.0f) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }
  if (req.F(moveword::kDwellLimit2) * k.half <= req.F(moveword::kDwellElapsed2) &&
      ((int(req.U(moveword::kClassWord)) < 1 ||
        (int(req.U(moveword::kClassWord)) < 2 &&
         k.deg67p5 < std::fabs(req.F(moveword::kTurn)))) &&
       1 < speedClass && !precise)) {
    EnterGotoRun(host, req, /*runStopVariant=*/false);
    return;
  }
  if (speedClass == 3 && !precise && int(req.U(moveword::kClassWord)) < 2 &&
      Wrap180(req.F(moveword::kFacing) - req.F(moveword::kBodyDirection)) < k.deg45) {
    if (EnterAccelerate(host, req)) return;
    if (groundSpeed < k.kmh11) {
      EnterGotoRun(host, req, /*runStopVariant=*/false);
      return;
    }
  }

  // 6. The dwell gates.
  bool stopFallback = false;
  if (int(req.U(moveword::kClassWord)) >= 0 && speedClass == -1) stopFallback = true;
  // PES: `if (1.5 < d) { f = 4.0; if (d < 4.0) f = (d - 1.5) / 1.25; }` — the share is 0 below
  // 1.5 m, the linear ramp above it, and clamped at 4.0.
  const float share =
      k.dist1p5 < req.F(moveword::kDistToLook)
          ? (req.F(moveword::kDistToLook) < k.four
                 ? (req.F(moveword::kDistToLook) - k.dist1p5) / k.dist1p25
                 : k.four)
          : 0.0f;
  if (share * k.deg22p5 + k.deg45 < std::fabs(turnToTarget) &&
      k.deg22p5 < std::fabs(req.F(moveword::kTurn)) &&
      req.F(moveword::kDwellElapsed2) <= req.F(moveword::kDwellLimit2) * k.s0p7) {
    stopFallback = true;
  }
  float blendedTurn = 0.0f;
  if (tick * req.F(moveword::kDwellLimit2) != 0.0f)
    blendedTurn = req.F(moveword::kRecordStampA) / (tick * req.F(moveword::kDwellLimit2));
  if ((kind != 4 || (k.s0p15 < req.F(moveword::kDwellElapsed2) ||
                     k.s0p15 == req.F(moveword::kDwellElapsed2))) &&
      stopFallback) {
    req.SetF(moveword::kRecordStampA, blendedTurn);
    const float dwellLeft = req.F(moveword::kDwellLimit2) - req.F(moveword::kDwellElapsed2);
    req.SetF(moveword::kRecordStampB, dwellLeft <= 0.0f ? 0.0f : dwellLeft);
    const float scale = int(req.U(moveword::kClassWord)) < int(req.U(moveword::kPrevClassWord))
                            ? k.s0p3
                            : k.s0p6;
    if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2) * scale) {
      req.SetU(moveword::kClassWord, req.U(moveword::kPrevClassWord));
      req.SetF(moveword::kBandSpeed, req.F(moveword::kBandSpeedPrev));
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
  float window = k.s0p2;
  if (int(req.U(moveword::kMoveBucket)) == int(req.U(moveword::kBucketPrev))) window = k.deg90;
  if (req.F(moveword::kDwellLimit2) * window <= req.F(moveword::kDwellElapsed2) &&
      k.deg5p625 < std::fabs(turnToTarget)) {
    // The clock draws 180 degrees per tick; below that the drift is clamped to the look heading.
    const float draw = k.halfTurn / tick;
    float delta = turnToTarget;
    if (delta <= draw) {
      delta = -draw;
      if (-draw <= turnToTarget) delta = turnToTarget;
    }
    drift += delta;
    if (0.0f <= drift) {
      if (k.full <= drift) drift += k.wrapFull;
    } else {
      drift += k.full;
    }
    const float toLook = req.F(moveword::kLookHeading);
    float toLookFolded = toLook;
    if (0.0f <= toLookFolded) {
      if (k.full <= toLookFolded) toLookFolded += k.wrapFull;
    } else {
      toLookFolded += k.full;
    }
    float deltaLook = drift - toLookFolded;
    if (k.halfTurn < std::fabs(deltaLook))
      deltaLook -= (deltaLook < 0.0f ? k.negOne : k.one) * k.full;
    if (tick * 2.0f <= std::fabs(deltaLook) && std::fabs(deltaLook) <= k.deg135) {
      const float toFacing = Wrap180(req.F(moveword::kLookHeading) - req.F(moveword::kFacing));
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
      if (signA != signB && precise) return;
    }
    req.SetF(moveword::kRootFacingAngle, drift);
    req.SetByte(moveword::kRootFacing, 1);
  }

  // The final gate: the dwell runs out and the loop returns to STEP_LOOP.
  const float elapsed = req.F(moveword::kDwellElapsed2);
  const float limit = req.F(moveword::kDwellLimit2);
  const int bucketNow = int(req.U(moveword::kMoveBucket));
  const int bucketPrev = int(req.U(moveword::kBucketPrev));
  if (elapsed < limit &&
      ((elapsed < limit * k.s0p2 || precise || speedClass < 2) ||
       (bucketNow == 0 || bucketNow == 5 || bucketNow != bucketPrev))) {
    if (speedClass != 1 && !(speedClass < 2 && int(req.U(moveword::kClassWord)) > 0) &&
        !(bucketNow == 0 || bucketNow == 5 || bucketNow != bucketPrev)) {
      return;
    }
  }
  req.SetU(moveword::kDwellLimit, limit * k.s0p4 <= elapsed ? 0u : uint32_t(0.1f));
  req.SetU(moveword::kDwellElapsed, 0);
  req.SetU(moveword::kDwellElapsed2, 0);
  req.SetU(moveword::kDwellLimit2, 0);
  RequestState(req, MoveState::kStepLoop);
  req.SetByte(moveword::kMotionPending, 0);
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
  const float tick = ClockStep(host);

  // 1. The motion's own record: the toe-off window and the rate.
  // ponytail: PES reads the motion record through FUN_1406b1f00 / FUN_1406b1d00 (the hit-frame
  // and the foot-contact frames of the current clip); GF has neither record, so the port uses
  // the clip's frame count the adapter supplies and the record's 0.85 divisor (P+0x2468, which
  // the contract exposes).
  const float divisor = std::max(host.RecordF2468(), k.s0p85);
  const int frames = host.CurrentClipFrames();
  // ponytail: the record's cancel/foot frames (FUN_1406af8e0/afbc0 of the live motion's
  // record); neither is a km/h band speed, so both stand at the no-record answer until the
  // record reader is ported.
  const int cancelFrames = 0;
  const int footFrames = 0;
  const bool yawBusy = host.AnimeState() != 0;
  if (!yawBusy && footFrames <= cancelFrames &&
      cancelFrames < (footFrames + footFrames * 2) / 3) {
    // ponytail: the toe-off window writes req[0x3d]/[0x3e]/[0x3f]; the port keeps PES's test
    // and leaves the window to the record's own two frames, which stand in for the unwritten
    // motion record.
    (void)frames;
  }
  const float distance = req.F(moveword::kDistToTarget);
  const float distanceFactor =
      k.kmh5 < distance ? (distance < k.kmh5 ? (distance - k.kmh5) / k.dist3 : 1.0f) : 0.0f;
  float ground[3];
  host.PositionAt(1, ground);
  const float groundSpeed = (tick * k.ticks60 * k.ticks60) / k.scale1000 *
                            std::sqrt(ground[0] * ground[0] + ground[1] * ground[1] +
                                      ground[2] * ground[2]);
  req.SetF(moveword::kRate, req.F(moveword::kRate) * k.rate0p9);
  // PES at 0x1406CAD06: FUN_14043a0a0(req[0x02], req[0x01]) = wrap180(kHeading - kFacing).
  // The result's magnitude (the abs mask at 0x1406CAD24) is the first path's gate.
  const float turnToTarget = Wrap180(req.F(moveword::kHeading) - facing);
  const float turnToTargetDeg = std::fabs(turnToTarget);
  const float dwellLimit = k.one / tick * k.deg10;  // the clock's 10-degree window

  // 2. The first path: turn the facing back with a step clip.
  if (req.F(moveword::kDwellElapsed) <= dwellLimit && dwellLimit != req.F(moveword::kDwellElapsed) &&
      (turnToTargetDeg < k.deg22p5 ||
       turnToTargetDeg < distanceFactor * k.deg45 + k.deg22p5)) {
    if (turnToTargetDeg < k.deg22p5 && StayDecision(host, req) &&
          !StepLetsRunTurn(host, req)) {
      // The band from the ground speed and the rate from the ceiling.
      const int foot = host.FootBand();
      req.SetF(moveword::kRate, kRateHigh);
      req.SetByte(moveword::kMirror, (foot - 1u) < 2 ? 1 : 0);
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
      const int band = host.BandSpeedKmh(int(req.F(moveword::kRate)), host.PhysiqueG());
      req.SetU(moveword::kClassWord, uint32_t(band));
      req.SetF(moveword::kBandSpeed, host.RateCeiling() * k.s0p6 * k.kmh3p6);
      const int footBand = host.FootBand();
      pes::MotionRequest request;
      request.motion = footBand;
      request.mirror = (foot - 1u) < 2;
      host.Issue(request);
      return;
    }
  }

  // 3. The second path: the class-3 and bucket gates lead to the run-turn states.
  const float lookAhead = k.one / tick * k.deg10;
  if (req.F(moveword::kDwellElapsed) <= lookAhead && lookAhead != req.F(moveword::kDwellElapsed) &&
      speedClass > 0 && int(req.U(moveword::kClassWord)) > 0) {
    const float turn = Wrap180(facing - req.F(moveword::kBodyDirection));
    if (precise) {
      if (k.deg28p125 <= std::fabs(turnToTarget)) {
        // The band from the ground speed, then STEP_TURN_RUN.
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
      } else {
        if (EnterRunTurnStep(host, req,
                             (host.RateCeiling() - k.s0p6) * k.half))
          return;
      }
    } else if (int(req.U(moveword::kMoveBucket)) != 0 &&
               int(req.U(moveword::kMoveBucket)) != 5) {
      if (k.deg22p5 <= std::fabs(turnToTarget)) { /* fall through to the run-turn below */ }
      if (k.kmh8 < groundSpeed && k.half < req.F(moveword::kDistToTarget) &&
          req.U(0x0f) != 0) {
        if (groundSpeed < k.kmh11 && int(req.U(moveword::kClassWord)) > 1) {
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kBandSpeed, k.kmh11);
          EnterRunTurnRun(host, req);
          return;
        }
        if (groundSpeed < k.kmh16 && int(req.U(moveword::kClassWord)) > 2)
          req.SetU(moveword::kClassWord, 2);
        EnterRunTurnRun(host, req);
        return;
      }
    }
  }

  // 4. The third path: the request's own destination re-plans into the step grid.
  if (speedClass > 1 && int(req.U(moveword::kClassWord)) < 1 && StayDecision(host, req))
    goto RePlan;
  if (speedClass >= 0 && precise && k.dist1p5 < req.F(moveword::kDistToTarget)) {
    const float a = Wrap180(DisplacementHeading(host) - facing);
    const float b = Wrap180(DisplacementHeading(host) - req.F(moveword::kBodyDirection));
    if (std::fabs(a) < k.deg45 && std::fabs(b) < k.deg45) {
      // The bucket comes from the request's own facing.
      // ponytail: PES calls the interface's bucket function (vt+0x60) on FUN_14043a0a0(facing,
      // req[0x1]); the port asks AngleBucket for the same answer.
      const int bucket = AngleBucket(host.AngleBuckets(), facing, 10, false);
      if (k.kmh5 <= groundSpeed) {
        int band = 2;
        if (k.kmh11 <= groundSpeed) {
          req.SetU(moveword::kClassWord, 2);
          req.SetF(moveword::kBandSpeed, k.kmh16);
        } else {
          band = 1;
          req.SetU(moveword::kClassWord, 1);
          req.SetF(moveword::kBandSpeed, k.kmh11);
        }
      } else {
        req.SetU(moveword::kClassWord, 0);
        req.SetF(moveword::kBandSpeed, k.kmh5);
      }
      const int bandNow = std::min(std::max(int(req.U(moveword::kClassWord)), 0), 2);
      float speed = k.kmh16;
      if (std::fabs(req.F(moveword::kBandSpeed)) <= k.kmh16 && k.kmh5 <= std::fabs(req.F(moveword::kBandSpeed)))
        speed = std::fabs(req.F(moveword::kBandSpeed));
      const int rowId = int(tables.kindBandRow[kind + bandNow * 8]);
      const MoveCell cell = host.Cell(rowId, int(tables.bucketColumn[bucket]), false);
      const int motion = cell.found ? cell.motionNo : rowId;
      req.SetU(moveword::kMoveBucket, uint32_t(bucket));
      req.SetU(moveword::kPrevClassWord, req.U(moveword::kClassWord));
      req.SetU(moveword::kBandSpeedCopy, uint32_t(speed));
      req.SetU(moveword::kClassWord, uint32_t(bandNow));
      req.SetF(moveword::kBandSpeed, speed);
      req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
      req.SetF(moveword::kPlayback, StepPlayback(host.PhysiqueG(), host.PlaybackConstantsView()));
      pes::MotionRequest request;
      request.motion = motion;
      request.mirror = host.BucketIsLeft(bucket);
      request.duration = k.s0p1333;
      request.playback = req.F(moveword::kPlayback);
      request.startTime = host.MotionStartTime(motion);
      host.Issue(request);
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
    // ponytail: PES reads the motion record's hit-frame (FUN_1406af620) and cancel-frame
    // (FUN_1406af8e0); GF has neither record, so the port's dwell uses the two frame counts the
    // adapter supplies and stands in for the record.
    const float dwell = replayOnly ? (k.one / tick) * float(cancelFrames) : req.F(moveword::kDwellLimit);
    bool stopNow = false;
    if (!replayOnly) {
      if (speedClass > 1 || int(req.U(moveword::kClassWord)) > 2 ||
          (footFrames + cancelFrames) / 2 <= static_cast<int>(divisor)) {
        stopNow = true;
      }
    }
    const int footFramesBack = footFrames;
    const int cancelFramesBack = cancelFrames;
    if (speedClass > 0 && int(req.U(moveword::kClassWord)) > 0) {
      const float turn = Wrap180(DisplacementHeading(host) - facing);
      if (k.deg22p5 <= std::fabs(turn)) {
        if (k.kmh13p5 <= groundSpeed) {
          if (k.kmh22 < groundSpeed) {
            req.SetU(moveword::kClassWord, 2);
            req.SetF(moveword::kBandSpeed, k.kmh16);
          } else {
            req.SetU(moveword::kClassWord, 1);
            req.SetF(moveword::kBandSpeed, k.kmh11);
          }
        } else {
          req.SetF(moveword::kBandSpeed, k.kmh5);
          req.SetU(moveword::kClassWord, 0);
        }
        if (EnterStepTurnRun(host, req)) return;
      } else if (precise &&
                 EnterRunTurnStep(host, req, (host.RateCeiling() - k.s0p6) * k.half)) {
        return;
      }
    }
    if (stopNow && StayDecision(host, req)) {
      if (!precise && std::fabs(Wrap180(DisplacementHeading(host) - facing)) < k.deg67p5 &&
          std::fabs(Wrap180(DisplacementHeading(host) - facing)) < std::fabs(dwellLimit)) {
        // The left/right feet decide the mirror and the stop's own row.
        // ponytail: PES reads the two feet through FUN_141eb50b0 indices 3 and 6 and rotates
        // them by the facing; GF has no pose bones, so the mirror is a coin flip from the
        // player's own generator until the two indices are ported.
        float left[3], right[3];
        host.PositionAt(3, left);
        host.PositionAt(6, right);
        const int row = host.PickN(2);
        req.SetByte(moveword::kMirror, row == 1 ? 1 : 0);
        req.SetU(moveword::kDwellLimit, 0);
        req.SetU(moveword::kDwellLimit2, 0);
        req.SetByte(moveword::kStyle, 0);
        pes::MotionRequest request;
        request.motion = row;
        host.Issue(request);
        req.SetF(moveword::kRate, kRateMid);
        return;
      }
      if (req.F(moveword::kDwellElapsed) < dwell && k.deg22p5 <= std::fabs(dwellLimit)) return;
    } else {
      if (req.F(moveword::kDwellElapsed) < dwell) return;
      if (!StayDecision(host, req)) {
        if (int(req.U(moveword::kLookBucketAlt)) != 0 &&
            int(req.U(moveword::kLookBucketAlt)) != 5 &&
            k.deg33p75 <= std::fabs(req.F(moveword::kFacingError)) &&
            int(req.U(moveword::kCommandId)) != 0x79) {
          return;  // FUN_1406d1390 is not decompiled; the handler keeps the tick.
        }
        if (footFramesBack < cancelFramesBack) return;
        // The idle row.
        req.SetByte(moveword::kMotionPending, 1);
        const int row = kind < 8 ? IdleRow(host, req) : 0x5a1;
        // ponytail: PES calls the anime table's lookup (FUN_1406abb30) on the idle row and its
        // two column choices; the port reads the row through the host's Cell, and the mirror
        // from the two column choices PES names (9, or 8/9 by the facing error's sign).
        const int column = req.F(moveword::kFacingError) < 0.0f ? 9 : 8;
        const MoveCell cell = host.Cell(row, column, false);
        req.SetU(moveword::kMotion, uint32_t(cell.found ? cell.motionNo : row));
        // ponytail: the 50-percent roll into the second column (FUN_1406ebf30(P, 0x32)).
        const bool other = host.Percent(50);
        if ((kind != 3 && kind != 6) && other) {
          const MoveCell alt = host.Cell(row, req.F(moveword::kFacingError) < 0.0f ? 9 : 8, false);
          if (alt.found) req.SetU(moveword::kMotion, uint32_t(alt.motionNo));
        }
        req.SetByte(moveword::kMirror, other ? 1 : 0);
        req.SetU(moveword::kDwellElapsed, 0);
        req.SetU(moveword::kDwellLimit, 0);
        req.SetU(moveword::kDwellElapsed2, 0);
        req.SetU(moveword::kDuration, 0.2f);
        req.SetU(moveword::kStartTime, 0);
        req.SetF(moveword::kPlayback, 1.0f);
        req.SetU(moveword::kDwellLimit2, 0.2f);
        req.SetU(moveword::kLiveKind, req.U(moveword::kKind));
        RequestState(req, MoveState::kIdle);
        req.SetByte(moveword::kMotionPending, 0);
        return;
      }
    }
  }

RePlan:
  req.SetU(moveword::kClassWord, 0xffffffffu);  // -1
  req.SetF(moveword::kBandSpeed, 0.0f);
  EnterGotoRun(host, req, /*runStopVariant=*/false);
}

}  // namespace movestep
}  // namespace pes
