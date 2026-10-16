// FUN_1406cbc10, the lateral drift damping (run_2_4_000 and rate >= 0.3). The drift is two floats on
// the request: X at req[0x4d] (req+0x134) and Z at req[0x4e] (req+0x138). In the drift case they step
// toward zero by at most the record's bound over half a tick-second; the else branch clears the part
// of each that is past a speed threshold. The arithmetic and the clamp shapes are PES's own.
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
}  // namespace

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
