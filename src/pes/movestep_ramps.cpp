// FUN_1406cbf70, the acceleration ramp of the rate (rev2 §4.2), and FUN_1406cbc10, the lateral drift
// damping (run_2_4_000 and rate >= 0.3). The ramp steps the rate toward its target each tick in RUN_LOOP
// and STEP_TO_RUN; the drift is two floats on the request: X at req[0x4d] (req+0x134) and Z at req[0x4e]
// (req+0x138). In the drift case they step toward zero by at most the record's bound over half a
// tick-second; the else branch clears the part of each that is past a speed threshold. The arithmetic and
// the clamp shapes are PES's own.
//
// The ramp's stages, in PES's order: the base target from the request's speed or class (RequestedRate),
// clamped to [0, rateMax]; the turn slow above one half; the proximity cap in the jostle styles; the factor F
// times rateMax (or rateMax itself with no factor); then the step FUN_1406e66d0 takes toward the target. The
// phase-3 class side effect (rev2 §4.2 stage 1) is not ported: its speed reads the position-history ring, whose
// writer is not decoded. The closing cap after the step (rev2 §4.3) is not ported: it reads the partner and touch
// records, which GF does not have yet.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "pes/movestep.hpp"

namespace pes {
namespace movestep {
namespace {
// The motion whose drift case runs whatever the state (FUN_1406cbc10's 0xbde test).
constexpr int kDriftMotion = 0xbde;
// MoveState::kRunLoop: the drift case's state (the phase byte is 3).
constexpr uint8_t kDriftState = 3;
// The style byte values the drift case refuses: 6 and 7 (the jostle loops, a byte window of width
// 2 from 6) and 0x10.
constexpr uint8_t kJostleStyleBase = 6;
constexpr uint8_t kJostleStyleSpan = 1;  // (style - 6) as a byte must exceed this to pass
constexpr uint8_t kJostleStyleOther = 0x10;

// The player slots: an own id at or above this is not one of the 22 players (FUN_1406cbf70's `> 21`).
constexpr int kPlayerSlots = 22;
// The cover sub-state (pl+0x5d0 == 0x32) and the decelerating sub-state (pl+0x5d0 == 2).
constexpr int kCoverSubState = 0x32;
constexpr int kDeceleratingSubState = 2;
// The movement prediction's ticks: n = int(54 * 4 / 60 + 0.5), with the install's 4 and 60.
int PredictionTicks(const MoveHost& host) {
  const RunConstants& k = host.RunConstantsView();
  return static_cast<int>(host.TicksPerSecond() * k.speed4 / k.kmh60 + k.half);
}

// PES's symmetric clamp, written as the decompile writes it: h when x is above h, -h when x is
// below -h, else x. With h negative the branches still give PES's answer.
float ClampSymmetric(float x, float h) {
  float r = h;
  if (x <= h) {
    r = -h;
    if (-h <= x) r = x;
  }
  return r;
}

// max(0, min(x, hi)): the clamp the ramp writes for a target and a ceiling.
float ClampToCeiling(float x, float hi) { return std::max(0.0f, std::min(x, hi)); }

// FUN_1406eab00(target, rate, rateMax) (raw listing, the install at 0x1406eab00): the time constant tau
// that FUN_1406e66d0 divides the tick into. The accelerating branch (rate at 0.6 or above, target above
// rate) gives tau = d * (1.9 - 0.15 a^2) / (rateMax - 0.6). Below 0.6 the slope is 1.4 - 0.15 a^2, decel
// is 1.25 - 0.2 a^2, and decel below 0.6 in sub-state 2 is -3 d. d = target - rate, zeroed under the
// install's floor, and a is the Acceleration ability's weight.
float Tau(MoveHost& host, float target, float rate, float rateMax) {
  const RunConstants& k = host.RunConstantsView();
  const TeamAi& team = host.Team();
  const int own = host.ActionIndex();
  float d = target - rate;
  if (std::fabs(d) < k.rampFloor) d = 0.0f;
  // a = clamp((Acc' - 40) / 59, 0, 1), Acc' the Acceleration byte less (10 - c) when the condition byte c
  // is 10 or below. An own slot above the list has no Acceleration: a = 0.
  float a = 0.0f;
  if (own < kPlayerSlots) {
    const int condition = team.Condition(own);
    float acc = static_cast<float>(team.Ability(own, kAbilityAcceleration));
    if (static_cast<float>(condition) <= k.condition10) acc -= k.condition10 - static_cast<float>(condition);
    if (acc <= k.rec40) {
      a = 0.0f;
    } else if (acc < k.rec99) {
      a = (acc - k.rec40) / k.rec59;
    } else {
      a = k.one;
    }
  }
  const float a2 = a * a;
  if (k.rate06 <= rate && target > rate) {
    return d * (k.accelAbove - k.rampSlope * a2) / (rateMax - k.rate06);
  }
  if (d >= 0.0f) return d * (k.accelBelow - k.rampSlope * a2);
  if (rate <= k.rate06 && team.SubState(own) == kDeceleratingSubState) return k.decelSub * d;
  return (k.blend2 * a2 - k.decelSpan) * d;
}

// FUN_1406e66d0(P, target, rate): the rate's step toward the target. The target is clamped to [0, rateMax];
// the step is (1 / tick) / tau * (target - rate) past the tick's floor, and the target itself below it.
float RateStep(MoveHost& host, float target, float rate) {
  const float rateMax = host.RateCeiling();
  const float aim = ClampToCeiling(target, rateMax);
  const float tau = Tau(host, aim, rate, rateMax);
  const float tick = host.RunConstantsView().one / host.TicksPerSecond();
  if (tau < tick) return aim;
  return ClampToCeiling(tick / tau * (aim - rate) + rate, rateMax);
}

// Stage 3: the turn slow. Above one half, the target is pulled toward one half by (1 - a), with a = 0 under
// 11.25 degrees of turn and clamp(|turn| / 90) above. In phase 13 the turn is the velocity heading against
// the request's heading; otherwise it is the request's look delta (req[5]).
float TurnSlow(MoveHost& host, MoveReq& req, float x) {
  const RunConstants& k = host.RunConstantsView();
  if (!(k.half < x)) return x;
  float turn = req.F(moveword::kLookDelta);
  if (host.CurrentState() == static_cast<int>(MoveState::kStepToRun)) {
    float v[3] = {0.0f, 0.0f, 0.0f};
    host.PositionAt(1, v);
    const float heading =
        FoldFull(std::atan2(v[0], v[2]) * host.StyleConstantsView().radToDeg, k);  // PES's (x, z) pair: index 2 is z
    turn = PesAngleDiff(heading, req.F(moveword::kHeading), k);
  }
  const float magnitude = std::fabs(turn);
  const float a = magnitude < k.deg1125 ? 0.0f : std::min(magnitude / k.deg90, k.one);
  return k.half + (k.one - a) * (x - k.half);
}

// Stage 4: the proximity cap, in the jostle styles (6, 7, 0x10 are the style words 0x50 can hold at the
// ramp, rev2 §4.2): when the nearest opponent's predicted position is within the threshold and he stands
// ahead, the target is cut by 0.2 times the weight of the turn to him and his PC lead. Its gates follow
// PES's order: the own slot, the target above 0.6, the style, the opponent's slot, bit 27 of his action
// record, and his PC above the own PC.
float ProximityCap(MoveHost& host, MoveReq& req, float x) {
  const RunConstants& k = host.RunConstantsView();
  const TeamAi& team = host.Team();
  const int own = host.ActionIndex();
  const int style = static_cast<int>(req.U(moveword::kStyle) & 0xffu);
  if (!(own < kPlayerSlots && k.rate06 < x && (style == 6 || style == 7))) return x;
  const int opp = team.NearestOpponent(own);
  if (!(opp < kPlayerSlots)) return x;
  const float pcOwn = static_cast<float>(team.Ability(own, kAbilityPc));
  const float pcOpp = static_cast<float>(team.Ability(opp, kAbilityPc));
  if (!team.ActionBit27(opp) || pcOpp <= pcOwn) return x;

  const int n = PredictionTicks(host);
  float self[3], selfVel[3], other[3], otherVel[3];
  team.Position(own, self);
  team.Velocity(own, selfVel);
  team.Position(opp, other);
  team.Velocity(opp, otherVel);
  // ponytail: index 1 is PES's z only because TeamAi::Position copies GF coords (pesmovestep.cpp, Position); PES's z is
  // index 2. Inert while ActionBit27 is false (the gate above); the frame fix waits for a decision.
  const float sx = self[0] + n * selfVel[0];
  const float sz = self[1] + n * selfVel[1];
  const float ox = other[0] + n * otherVel[0];
  const float oz = other[1] + n * otherVel[1];
  const float dx = ox - sx;
  const float dz = oz - sz;
  // THR = 0.7 + 0.2 * clamp((PC(opp) - 50) / 49).
  const float share = std::min(std::max((pcOpp - k.frame50) / k.capSpan, 0.0f), k.one);
  const float threshold = k.rate07 + k.blend2 * share;
  if (!(std::sqrt(dx * dx + dz * dz) < threshold)) return x;

  const float bearing = FoldFull(std::atan2(dx, dz) * host.StyleConstantsView().radToDeg, k);
  const float ang = PesAngleDiff(req.F(moveword::kFacing), bearing, k);
  const float mag = std::fabs(ang);
  float w = k.one;
  if (mag >= k.deg1575) {
    w = 0.0f;
  } else if (k.deg90 < mag) {
    w = (k.deg1575 - mag) / k.deg675;
  }
  const float lead = pcOpp - pcOwn;
  const float g = lead > 0.0f ? std::min(std::max(lead / k.rec40, 0.0f), k.one) : 0.0f;
  return x * (k.one - k.blend2 * w * g);
}

// Stage 5: the ceiling t the target is taken under: rateMax times the factor F, or rateMax itself when no
// factor applies (branch a). The branches are PES's: the keeper, a tackle on either side, the set-piece
// answer, the replay action take no factor; an own slot above the list takes 0.93; a special role takes
// 1 - 0.04 u2; a possession latch takes 0.93 - 0.03 u2; a free owner takes the DA-weighted mix.
float FactorCeiling(MoveHost& host, float rateMax) {
  const RunConstants& k = host.RunConstantsView();
  const TeamAi& team = host.Team();
  const int own = host.ActionIndex();
  const int opp = team.NearestOpponent(own);
  const bool setPiece = own < kPlayerSlots && team.SetPieceAnswer(own);
  const bool tackled = team.SideTackled(own) || (opp < kPlayerSlots && team.SideTackled(opp));
  if (team.IsSideKeeper(own) || tackled || setPiece || host.AnimeState() == kReplayAnimeState) {
    return rateMax;
  }
  if (kPlayerSlots <= own) return rateMax * k.fFree;

  // u2: the reserve weight, 1 at or under 20 %, falling to 0 at 80 %.
  const float reserve = static_cast<float>(host.StaminaPercent());
  float u2 = 0.0f;
  if (reserve <= k.reserveLow) {
    u2 = k.one;
  } else if (reserve < k.reserveHigh) {
    u2 = (reserve - k.reserveHigh) / k.reserveSpan;
  }

  const bool special = team.Carries(own) || team.IsSideSlot78(own) || team.HasRouteRow(own) ||
                       team.IsMatchUpMan(own) || team.SubState(own) == kCoverSubState ||
                       team.IsQueuedCover(own) || team.SideSlotMarked(own);
  if (special) return rateMax * (k.one - k.fStepC * u2);
  if (team.PossessionLatched(own)) return rateMax * (k.fFree - k.fStepB * u2);

  // u1: the DA weight, 1 at or under 55, falling to 0 at 65.
  const float da = static_cast<float>(team.Ability(own, kAbilityDa));
  float u1 = 0.0f;
  if (da <= k.daLow) {
    u1 = k.one;
  } else if (da < k.daHigh) {
    u1 = (da - k.daHigh) / k.daSpan;
  }
  return rateMax * ((k.one - u2) * (k.fFree - k.fStepA * u1) + k.fTargetA * u2);
}
}  // namespace

// Stage 2: the base target, the requested rate (RUN_LOOP's rate read), clamped to the ceiling.
void AccelerationRamp(MoveHost& host, MoveReq& req) {
  const float rateMax = host.RateCeiling();
  float x = ClampToCeiling(RequestedRate(host, req, host.RunTablesView()), rateMax);
  x = TurnSlow(host, req, x);
  x = ProximityCap(host, req, x);
  const float target = ClampToCeiling(x, FactorCeiling(host, rateMax));
  req.SetF(moveword::kRate, RateStep(host, target, req.F(moveword::kRate)));
}

// FUN_1406b8340's steps 4 and 5 (anime-select §1), before the dispatcher: the ramp only in RUN_LOOP and
// STEP_TO_RUN, the damping in every state.
void RampsBeforeDispatch(MoveHost& host, MoveReq& req) {
  const int state = host.CurrentState();
  if (state == static_cast<int>(MoveState::kRunLoop) || state == static_cast<int>(MoveState::kStepToRun)) {
    AccelerationRamp(host, req);
  }
  LateralDamping(host, req);
}

// FUN_1406cbc10(ap, req): the lateral drift damping, run_2_4_000 and rate >= 0.3 (AS 1, step 5).
void LateralDamping(MoveHost& host, MoveReq& req) {
  const DampingConstants& k = host.DampingConstantsView();
  const RunConstants& r = host.RunConstantsView();
  const DampingRecord& rec = host.DampingRecordView();
  const float rate = req.F(moveword::kRate);
  const uint8_t style = static_cast<uint8_t>(req.U(moveword::kStyle) & 0xffu);
  const bool styleAllowed =
      static_cast<uint8_t>(style - kJostleStyleBase) > kJostleStyleSpan && style != kJostleStyleOther;
  const bool driftCase = (static_cast<uint8_t>(host.CurrentState()) == kDriftState && styleAllowed &&
                          k.rateGate <= rate) ||
                         host.CurrentMotion() == kDriftMotion;
  if (driftCase) {
    // The turn term: the drift seed, or the special motion's +-2 from the look delta's side.
    float turn = req.F(moveword::kDriftSeed);
    const float lookDelta = req.F(moveword::kLookDelta);
    float speedRate = rate;
    if (host.CurrentMotion() == kDriftMotion) {
      turn = r.one;
      if (lookDelta <= 0.0f && lookDelta != 0.0f) turn = r.neg;
      turn = turn + turn;
      speedRate = k.motionRate;
    }
    if (std::fabs(lookDelta) < k.angleGate) turn = 0.0f;

    // The record's forward term, scaled by the playback and clamped to one.
    const float playback = req.F(moveword::kPlaybackDefault);
    float factor = ClampSymmetric(playback * turn / rec.scale, r.one);
    // The rate ramp: zero at the floor and at the cap, linear between.
    float ramp = 0.0f;
    if (k.rateGate < speedRate && (ramp = r.one, speedRate < k.rateCap)) {
      ramp = (speedRate - k.rateGate) / k.rateSpan;
    }
    factor = factor * (ramp * k.rateScale + k.rateBase);

    const float forward = -(factor * rec.forwardBound);
    const float x = req.F(moveword::kDriftX);
    const float z = req.F(moveword::kDriftZ);
    // The per-tick step bounds: the record's bounds over half the tick rate (54 * 0.5).
    const float halfTick = host.TicksPerSecond() * r.half;
    const float lateralStep = rec.lateralBound / halfTick;
    const float forwardStep = rec.forwardBound / halfTick;

    const float xDelta = ClampSymmetric(-(factor * rec.lateralBound) - x, lateralStep);
    const float zDelta = ClampSymmetric(forward - z, forwardStep);
    const float xNew = x + xDelta;
    const float zNew = z + zDelta;
    req.SetF(moveword::kDriftX, ClampSymmetric(xNew, rec.lateralBound));
    req.SetF(moveword::kDriftZ, ClampSymmetric(zNew, rec.forwardBound));
    return;
  }

  if (host.CurrentState() == 0) {
    req.SetF(moveword::kDriftX, 0.0f);
    req.SetF(moveword::kDriftZ, 0.0f);
    return;
  }
  // The else branch: the drift past the speed threshold (120 / the tick rate) is taken off, and
  // a drift under the 2.5 m bound clears.
  const float x = req.F(moveword::kDriftX);
  const float z = req.F(moveword::kDriftZ);
  const float threshold = k.driftSpeed / host.TicksPerSecond();
  float nx = 0.0f;
  if (threshold <= std::fabs(x)) nx = x - (x < 0.0f ? r.neg : r.one) * threshold;
  float nz = 0.0f;
  if (threshold <= std::fabs(z)) nz = z - (0.0f <= z ? r.one : r.neg) * threshold;
  const MoveConstants& m = host.MoveConstantsView();
  if (std::fabs(x) < m.dist2m5) nx = 0.0f;
  if (std::fabs(z) < m.dist2m5) nz = 0.0f;
  req.SetF(moveword::kDriftX, nx);
  req.SetF(moveword::kDriftZ, nz);
}

}  // namespace movestep
}  // namespace pes
