#include "onthepitch/pesballphysics.hpp"

#include <algorithm>
#include <cmath>

namespace pesball {
namespace {

// The report's clamp01: saturating, as the `comiss`+`jb`/`cmova` pattern is.
inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// The report's clamped ramp, literal. The raw compares the low bound first:
// `if (lo < v) { if (v < hi) interp else 1 } else 0`, so a non-positive span
// saturates rather than dividing by zero.
inline float Ramp(float v, float lo, float hi) {
  if (!(lo < v)) return 0.0f;
  return v < hi ? (v - lo) / (hi - lo) : 1.0f;
}

// km/h from m/s, with the image's own 3.6 (.rdata 0x14259ce38).
inline float ToKmh(const pes::PesBallConstants& c, float mps) { return mps * c.image.kmhPerMs; }

// .rdata 0x142578480: PES's pi, as a float (the spin cap's and the rolling block's 2 pi).
constexpr float kPi = 3.14159274f;

// 100.0 (.rdata 0x14253ee30): the per-cent divisor of the add-rate fields.
constexpr float kPercent = 100.0f;
// 0.01 s: PES's own |vy| blend divisor (.rdata 0x14256ae18) and the km/h->m/s of
// the stop gate's inner form. It is PES's constant, not this engine's tick.
constexpr float kStopBlend_s = 0.01f;

}  // namespace

int ClampMode(int mode) {
  // Report 0.3: `cmp mode, 5; cmova mode, 1` - anything above 5 becomes 1.
  return mode > 5 ? 1 : mode;
}

float RollingKeepFactor(const pes::PesBallConstants& c, int mode, float speed_mps) {
  // Report 2.4, literal. V = 3.6*|v| (km/h). Above frictionRollSpeedMin:
  //   t = clamp01((V - U)/(H - U))
  //   K = t*frictionRollRateMax + (1-t)*frictionRollRateMin
  // at or below it:
  //   L = frictionRollSpeedStop
  //   t = clamp01((V - L)/(U - L))
  //   K = t*frictionRollRateMin + (1-t)*frictionRollRateStop
  const pes::BallRecordConstants& r = c.record;
  const float V = ToKmh(c, speed_mps);
  const float U = r.At(pes::BallRecordConstants::kFrictionRollSpeedMin, mode);
  if (V > U) {
    const float H = r.At(pes::BallRecordConstants::kFrictionRollSpeedMax, mode);
    const float t = Clamp01((V - U) / (H - U));
    return t * r.At(pes::BallRecordConstants::kFrictionRollRateMax, mode) +
           (1.0f - t) * r.At(pes::BallRecordConstants::kFrictionRollRateMin, mode);
  }
  const float L = r.At(pes::BallRecordConstants::kFrictionRollSpeedStop, mode);
  const float t = Clamp01((V - L) / (U - L));
  return t * r.At(pes::BallRecordConstants::kFrictionRollRateMin, mode) +
         (1.0f - t) * r.At(pes::BallRecordConstants::kFrictionRollRateStop, mode);
}

float FrictionMultiplier(const pes::PesBallConstants& c, int mode, float speed_mps, float vy,
                         float spinRad, float* baseA) {
  // Report 3.1, literal. Three clamped ramps over |spin| (rad/s) and |vy| (km/h):
  //   s1 = clamp01((|w| - boundFrictionSpeedMin)/(boundFrictionSpeedMax - boundFrictionSpeedMin))
  //   s2 = clamp01((|vy|*3.6 - boundBoundSpeedMin)/(boundBoundSpeedMax - boundBoundSpeedMin))
  //   s3 = clamp01((|vy|*3.6 - boundRotationSpeedMin)/(boundRotationSpeedMax - boundRotationSpeedMin))
  //   K *= 1 - boundCheckFrictionAddRate[mode]/100 * s1
  //   A  = boundBoundRate[mode] * (1 + boundCheckBoundAddRate[mode]/100 * s3)
  const pes::BallRecordConstants& r = c.record;
  const float vyKmh = ToKmh(c, std::fabs(vy));
  const float s1 = Ramp(spinRad, r.boundFrictionSpeedMin, r.boundFrictionSpeedMax);
  const float s2 = Ramp(vyKmh, r.boundBoundSpeedMin, r.boundBoundSpeedMax);
  const float s3 = Ramp(vyKmh, r.boundRotationSpeedMin, r.boundRotationSpeedMax);
  if (baseA)
    *baseA = r.At(pes::BallRecordConstants::kBoundRate, mode) *
             (1.0f + r.At(pes::BallRecordConstants::kBoundCheckBoundAddRate, mode) / kPercent * s2);
  return 1.0f -
         r.At(pes::BallRecordConstants::kBoundCheckFrictionAddRate, mode) / kPercent * s1;
}

float VerticalKeep(const pes::PesBallConstants& c, int mode, float vy) {
  // Report 3.4 / report 2 corrections: this 0.03..0.10 keep is NOT PES's vertical
  // bounce path. Read literally from 0x1408770e6..0x14087718f:
  //   X = 3.6*|vy| (km/h)
  //   t = clamp01((X - boundToRotationSpeedMinY[mode]) /
  //               (boundToRotationSpeedMaxY[mode] - boundToRotationSpeedMinY[mode]))
  //   vy_keep = boundToRotationBaseY[mode] + t*boundToRotationAddRateY[mode]
  // Its output is consumed by 0x140877169..0x140877185 and multiplied into the
  // raw-spin-Y blend at 0x1408771ba only. It never touches vy: report 2 §2.1
  // proves the raw-spin pump is the only consumer, and report 2 §4.1 removes
  // every other candidate (no -e*vy reflection, no second contact routine, and
  // no caller post-processing). So this function is not wired into the contact
  // tick's vertical response; the bounce is Bounce() below.
  const pes::BallRecordConstants& r = c.record;
  const float X = ToKmh(c, std::fabs(vy));
  const float lo = r.At(pes::BallRecordConstants::kBoundToRotationSpeedMinY, mode);
  const float hi = r.At(pes::BallRecordConstants::kBoundToRotationSpeedMaxY, mode);
  const float t = Ramp(X, lo, hi);
  return r.At(pes::BallRecordConstants::kBoundToRotationBaseY, mode) +
         t * r.At(pes::BallRecordConstants::kBoundToRotationAddRateY, mode);
}

float StopSpeed_mps(const pes::PesBallConstants& c, int mode) {
  // Report 3.4: `stopSpeed * 1000 / 3600`, i.e. 0.175 km/h -> 0.04861 m/s.
  (void)mode;
  return c.record.stopSpeed * c.image.milli / c.image.secondsPerHour;
}

float RollSpinRamp(const pes::PesBallConstants& c, int mode, float spinRad) {
  // Report 3.3's s4 band, at 0x140876ec5, over the horizontal spin in Hz: 0x140876cd4 halves hypot(wx, wz)
  // and divides by pi (2 pi per cycle) before the band. The clamped ramp runs from rollToBoundRollMin to
  // rollToBoundRollMax. s4 enters F as `s4^2 * rollToBoundAdd` (0x140876f61).
  const pes::BallRecordConstants& r = c.record;
  const float spinHz = spinRad * 0.5f / kPi;
  return Ramp(spinHz, r.rollToBoundRollMin, r.rollToBoundRollMax);
}

float NaturalBound_mps(const pes::PesBallConstants& c, int mode, float horizontalSpeed_mps) {
  // Report 1.3's impulse cap, from 0x1408771d2..0x1408771ea: the record's
  // natualBoundB is a scalar in km/h, so the whole sum is in km/h and is divided
  // by 3600 after a *1000 - i.e. `(A*3.6*|v_xz| + B)/3.6` m/s. A SPEED, not a
  // ratio: with the shipped A = 0.055 and B = 0.4 it is >= 0.111 m/s.
  const pes::BallRecordConstants& r = c.record;
  const float kmh = r.At(pes::BallRecordConstants::kNatualBoundA, mode) *
                        ToKmh(c, horizontalSpeed_mps) +
                    r.natualBoundB;
  return kmh * c.image.milli / c.image.secondsPerHour;
}

BounceResult Bounce(const pes::PesBallConstants& c, int mode, float vy, float horizontalSpeed_mps,
                    float spinRad) {
  // Report 1.3, the whole vertical response of a contact tick, read from the raw
  // bytes at 0x1408771d2..0x1408771f9 and 0x14087723d/0x140877248:
  //   F   = boundBoundRate[m] * (1 + boundCheckBoundAddRate[m]/100 * s2) + s4^2 * rollToBoundAdd
  //   s2  = clamp01((3.6*|vy| - boundBoundSpeedMin) / (boundBoundSpeedMax - boundBoundSpeedMin))
  //   imp = (natualBoundA[m]*3.6*|v_xz| + natualBoundB)/3.6     [m/s]
  //   z   = min(-imp, vy_trimmed)      vy_trimmed = vy - the grounder's vy trim
  //   vy_out = -F * z
  //
  // This is PES's per-contact instantaneous response: there is no k^0.54
  // mapping, because it is not a per-tick keep factor that compounds. Its terms
  // are a speed clamp and ratios of speeds at this contact, both dimensionless
  // in time - dividing them by 54 (as KeepFactorPerStep does for a keep factor)
  // would corrupt them. The only per-tick factor in the contact tick is K, the
  // horizontal keep, and that one is mapped (see ContactTick).
  const pes::BallRecordConstants& r = c.record;
  BounceResult out;

  // s2, over the incoming |vy| in km/h (0x1408770e6..0x14087710f's band, applied
  // here to the boundBoundSpeed band of F at 0x140876b26).
  const float vyKmh = ToKmh(c, std::fabs(vy));
  const float s2 = Ramp(vyKmh, r.boundBoundSpeedMin, r.boundBoundSpeedMax);
  const float s4 = RollSpinRamp(c, mode, spinRad);
  out.restitution = r.At(pes::BallRecordConstants::kBoundRate, mode) *
                    (1.0f +
                     r.At(pes::BallRecordConstants::kBoundCheckBoundAddRate, mode) / kPercent * s2) +
                    s4 * s4 * r.rollToBoundAdd;

  out.speedCap_mps = NaturalBound_mps(c, mode, horizontalSpeed_mps);
  // Both operands of PES's min() are negative for a falling ball: -imp is the
  // speed clamp and vy_trimmed is the incoming vertical speed. min picks the
  // less-negative one, i.e. the smaller magnitude.
  out.minOperand = std::min(-out.speedCap_mps, vy);
  out.vy = -out.restitution * out.minOperand;
  return out;
}

float SpinCapScale(const pes::PesBallConstants& c, float horizontalSpeed_mps) {
  // Report 3.4 as PES does it (FUN_140876360 0x14087704e..0x140877081): the horizontal rolling frequency
  // f = |v_xz| / (2 pi r), in Hz, is capped at boundRollMax. A ball already below the cap is untouched; above
  // it, the X and Z spin scale by boundRollMax / f. PES compares f^2 with boundRollMax^2.
  const float cap = c.record.boundRollMax;
  const float rollHz = horizontalSpeed_mps / (2.0f * kPi * c.image.ballRadius);
  if (rollHz <= cap) return 1.0f;
  return cap / rollHz;
}

GrounderResult GrounderKick(const pes::PesBallConstants& c, int mode, float gateX, float fracZ,
                            unsigned tickIndex, float horizontalSpeed_mps, float vy) {
  // Report 2.5, the guard chain verbatim:
  //   H = 3.6*|v_xz| >= grounderSpeed (km/h)
  //   old vy != 0 and |v_xz/vy| > grounderCTan (shallow shots only)
  //   int(|x|*1000) mod frictionRollSpeedStop[mode][3] == 0
  // then the driven grounder: the horizontal speed is trimmed by
  // grounderBoundYAdd*(1-f) and K is multiplied by (f + grounderFrictionRate),
  // with f = frac(|x|*1000)*0.3.
  //
  // PES reads the *x* coordinate for the mod-gate (0x1408769c7, [rsp+0x48] = old
  // x) and the *z* coordinate for the fraction (0x1408769e6, [rsp+0x50] = old z).
  // GF's ball is in GF space, so the caller passes the coordinate it calls
  // longitudinal; both are the same role in their own frame.
  //
  // Mode 1 ships 0 as that array's [3] element, so a signed idiv would fault;
  // PES never reaches it for mode 1 because its guards never all pass. This
  // engine keeps the gate but treats a zero divisor as "not taken", which is
  // the only outcome PES itself produces.
  GrounderResult out;
  const pes::BallRecordConstants& r = c.record;
  const float H = ToKmh(c, horizontalSpeed_mps);
  if (!(H >= r.grounderSpeed)) return out;
  if (vy == 0.0f) return out;
  if (!(std::fabs(horizontalSpeed_mps / vy) > r.grounderCTan)) return out;
  const int divisor = r.AtInt(pes::BallRecordConstants::kGounnderprob, mode);
  if (divisor == pes::kGrounderTickGateNever) return out;
  if (static_cast<int>(static_cast<unsigned>(std::fabs(gateX) * c.image.milli)) % divisor != 0)
    return out;
  // 0.3 is .rdata 0x142590d98.
  constexpr float kGrounderFracScale = 0.3f;
  const float scaled = std::fabs(fracZ) * c.image.milli;
  const float f = (scaled - static_cast<float>(static_cast<int>(scaled))) * kGrounderFracScale;
  out.applied = true;
  out.keepMultiplier = f + r.grounderFrictionRate;
  // 0x140876a57..0x140876a5c: vy -= |v_xz| * grounderBoundYAdd * (1 - f). Nothing else is trimmed.
  out.vyTrim_mps = horizontalSpeed_mps * r.grounderBoundYAdd * (1.0f - f);
  (void)tickIndex;
  return out;
}

bool IsResting(const pes::PesBallConstants& c, const BallState& s) {
  // Report 2.1: y <= r AND |spin| == 0 AND horizontal |v| == 0 skips the flight
  // function entirely (PES's r15b = 0). GF's ball carries a zero spin when
  // nothing has set one, so this is the ordinary stopped-ball path.
  const float spinSq =
      s.spinRaw.x * s.spinRaw.x + s.spinRaw.y * s.spinRaw.y + s.spinRaw.z * s.spinRaw.z;
  const float hSq = s.velocity.x * s.velocity.x + s.velocity.z * s.velocity.z;
  return s.position.y <= c.image.ballRadius && spinSq <= 0.0f && hSq <= 0.0f;
}

pes::Vec3 FlightAcceleration(const pes::PesBallConstants& c, const BallState& s, float spinRadX,
                             float spinRadY, float spinRadZ, bool noMagnus) {
  // Report 6.3 (FUN_140874470), literal and validated live to a median error of
  // 0.0013 m/s^2:
  //   s2 = vx^2 + vz^2
  //   if s2 <= dragCrisisSquared: gravity only (also no Magnus)
  //   s = sqrt(s2); v1 = dragSpeedMax/3.6; v2 = dragSpeedMin/3.6
  //   s' = (s <= v1) ? (s > v2 ? 2*v2 - s : s) : (2*(v2 - v1) + s)   // drag crisis
  //   k = airRegistNormal * -0.5 * rho * A * s'
  //   a = k*v + (0, -g, 0)                       // drag on all three
  //   if !noMagnus:
  //     m = min(s, nonSpinTopMax)                // the non-wobble branch
  //     a += m * magnusRate * pi^2 * r^2 * r * cross(w, v)
  const float s2 = s.velocity.x * s.velocity.x + s.velocity.z * s.velocity.z;
  pes::Vec3 a(0.0f, -c.image.gravity, 0.0f);
  if (s2 <= c.image.dragCrisisSquared) return a;
  const float speed = std::sqrt(s2);
  const float v1 = c.record.dragSpeedMax / c.image.kmhPerMs;
  const float v2 = c.record.dragSpeedMin / c.image.kmhPerMs;
  float shaped;
  if (speed <= v1)
    shaped = (speed > v2) ? (2.0f * v2 - speed) : speed;
  else
    shaped = 2.0f * (v2 - v1) + speed;
  const float k = c.record.airRegistNormal * c.image.half * c.image.airDensity * c.image.ballArea *
                  shaped;
  a.x = k * s.velocity.x;
  a.y = k * s.velocity.y - c.image.gravity;
  a.z = k * s.velocity.z;
  if (noMagnus) return a;
  const float m = speed < c.image.nonSpinTopMax ? speed : c.image.nonSpinTopMax;
  const float scale = m * c.record.magnusRate * c.image.pi * c.image.pi *
                      c.image.ballRadiusSquared * c.image.ballRadius;
  // cross(w, v) = (wy*vz - wz*vy, wz*vx - wx*vz, wx*vy - wy*vx)
  a.x += (spinRadY * s.velocity.z - spinRadZ * s.velocity.y) * scale;
  a.y += (spinRadZ * s.velocity.x - spinRadX * s.velocity.z) * scale;
  a.z += (spinRadX * s.velocity.y - spinRadY * s.velocity.x) * scale;
  return a;
}

void FlightTick(const pes::PesBallConstants& c, BallState* state, float dt_s, bool noMagnus) {
  // Report 6.1: semi-implicit Euler, v += a*dt first, then p += v*dt.
  const pes::Vec3 a = FlightAcceleration(
      c, *state, state->spinRaw.x * c.image.spinScale, state->spinRaw.y * c.image.spinScale,
      state->spinRaw.z * c.image.spinScale, noMagnus);
  state->velocity.x += a.x * dt_s;
  state->velocity.y += a.y * dt_s;
  state->velocity.z += a.z * dt_s;
  state->position.x += state->velocity.x * dt_s;
  state->position.y += state->velocity.y * dt_s;
  state->position.z += state->velocity.z * dt_s;
}

namespace {

// PES's rolling block (report 3.3; contact-2 §2; FUN_140876360 0x140876c0f..0x140876f6e). A spinning ball's
// contact adds horizontal deltas from its spin. e is the unit pre-flight heading. The spin across it gives the
// rotation frequency A, the spin along it B; both are speed-capped and scaled, and the tangential vector is
// T = (A e_x + B e_z, A e_z - B e_x). Every constant is a record row or scalar, or a VA literal named below.
struct TangentialXZ {
  float x;
  float z;
};

// .rdata 0x1425a9668: the heading's epsilon. Below it the block takes PES's fallback heading e = (0, 0, 1).
constexpr float kHeadingEpsilon_mps = 0.001f;
// .rdata 0x1425cc23c (8 km/h in m/s): the rolling flag's speed limit.
constexpr float kRollingSpeed_mps = 2.22222233f;


TangentialXZ RollingTangential(const pes::PesBallConstants& c, int mode, const BallState& pre,
                               float vxPost, float vzPost, float wx, float wz) {
  const pes::BallRecordConstants& r = c.record;
  const float headingSpeed = std::sqrt(pre.velocity.x * pre.velocity.x + pre.velocity.z * pre.velocity.z);
  // Below the epsilon there is no heading, and PES does not skip the block: its entry copy gets ez = 1.0 and
  // ex = 0 (0x140876c28..0x140876c40, the divide is skipped by the JC at 0x140876c50). An airborne ball with
  // no horizontal speed therefore still takes the spin's tangential push.
  float ex = 0.0f, ez = 1.0f;
  if (headingSpeed >= kHeadingEpsilon_mps) {
    ex = pre.velocity.x / headingSpeed;
    ez = pre.velocity.z / headingSpeed;
  }
  const float twoPi = 2.0f * kPi;
  // The rotation frequencies (Hz): spin across the heading (A) and along it (B).
  const float aDot = (ez * wx - ex * wz) / twoPi;
  const float bDot = -(ex * wx + ez * wz) / twoPi;
  // The rolling frequency of the post-flight velocity, capped at boundRollMax (report 3.4's cap, in Hz).
  const float rollHz = std::sqrt(vxPost * vxPost + vzPost * vzPost) / (c.image.ballRadius * twoPi);
  const float over = std::max(0.0f, rollHz - r.boundRollMax);
  // RollRamp: the XZ base plus the add rate across the downward speed (km/h), over the XZ speed bounds.
  const float downKmh = -ToKmh(c, pre.velocity.y);
  const float rollRamp =
      r.At(pes::BallRecordConstants::kBoundToRotationBaseXZ, mode) +
      r.At(pes::BallRecordConstants::kBoundToRotationAddRateXZ, mode) *
          Ramp(downKmh, r.At(pes::BallRecordConstants::kBoundToRotationSpeedMinXZ, mode),
               r.At(pes::BallRecordConstants::kBoundToRotationSpeedMaxXZ, mode));
  const float s3 = (rollHz - over - aDot) * rollRamp + aDot;
  // The blend B: 1 while |A - S3| <= 1, falling linearly to 0 at 4 (the 4.0 and -3.0 .rdata literals).
  const float s = Clamp01((4.0f - std::fabs(aDot - s3)) / 3.0f);
  const float blend = 0.5f + 0.5f * s;
  // Both rates are clamped to the record's speed cap (rollToSpeedMaxRoll, BallBase +0x2AC).
  const float cap = r.rollToSpeedMaxRoll;
  const float aCapped = std::clamp(aDot - blend * s3, -cap, cap);
  const float bCapped = std::clamp(bDot * rollRamp, -cap, cap);
  // The Y rate: from the base to 1 as the slope |vy|/|v_xz| goes from rollToSpeedSpeedYMin to YMax.
  // PES skips the divide at zero heading (0x140876e1a..0x140876e37) and takes the YMax bound as the slope,
  // which puts t at 1. Dividing 0/0 here would give NaN.
  const float slope = headingSpeed > 0.0f ? std::fabs(pre.velocity.y) / headingSpeed
                                          : r.rollToSpeedSpeedYMax;
  const float t = Clamp01((slope - r.rollToSpeedSpeedYMin) /
                          (r.rollToSpeedSpeedYMax - r.rollToSpeedSpeedYMin));
  const float base = r.At(pes::BallRecordConstants::kRollToSpeedSpeedYRateBase, mode);
  const float yRate = base + (1.0f - base) * t;
  const float vmax = r.At(pes::BallRecordConstants::kRollToSpeedMaxKph, mode) * c.image.milli /
                     c.image.secondsPerHour;
  const float a = yRate * aCapped * vmax / cap;
  const float b = yRate * bCapped * vmax / cap;
  return {a * ex + b * ez, a * ez - b * ex};
}

}  // namespace

void ContactTick(const pes::PesBallConstants& c, BallState* state, unsigned tickIndex, bool airborne,
                 bool quatIntegrate, float step_s, const BallState* preFlight) {
  // The contact mode. Report 3.1 closed report 7.2: the mode byte is written by
  // FUN_1408723d0 (0x140872479, `mov [rax+0x279d], bl`) as
  // `modeTable[pitch*3 + weather]`, and the table is read out of the install's
  // exe into pes::PesBallConstants::image (see pesball.cpp). The caller picks the
  // indices; the launch default is the live mode (kContactModeLive), not the dry-normal
  // index 2 that report 3.2 gives a dry pitch.
  const int mode = ClampMode(c.contactMode);

  // Report 2.3/2.7: the snap and the wobble-flag clear, and NO position
  // integration on a contact tick - y stays r and x,z stay where the flight
  // step of this tick put them.
  const float r = c.image.ballRadius;
  // The pre-flight state, or the current one with no flight step. PES reads the imp cap, the grounder and
  // the rolling flag from its entry copies (FUN_140876360 [rsp+0x58/0x60], bVar3 at 0x14087685e).
  const BallState& pre = preFlight ? *preFlight : *state;
  // bVar3: a ball that starts exactly on the ground and moves under 2.2222 m/s is rolling; the rolling block is
  // skipped for it (0x1425cc23c = 8 km/h in m/s).
  const float preHorizontal = std::sqrt(pre.velocity.x * pre.velocity.x + pre.velocity.z * pre.velocity.z);
  const bool rolling = pre.position.y == r && preHorizontal < kRollingSpeed_mps;
  state->position.y = r;
  const float vx = state->velocity.x, vy = state->velocity.y, vz = state->velocity.z;
  const float horizontal = std::sqrt(vx * vx + vz * vz);
  const float horizontalPre = preHorizontal;
  const float spinRad =
      std::sqrt(state->spinRaw.x * state->spinRaw.x + state->spinRaw.y * state->spinRaw.y +
                state->spinRaw.z * state->spinRaw.z) *
      c.image.spinScale;
  // Report 3.3's s4 band is over the *horizontal* spin, hypot(wx, wz).
  const float spinHorizontal =
      std::sqrt(state->spinRaw.x * state->spinRaw.x + state->spinRaw.z * state->spinRaw.z) *
      c.image.spinScale;

  // Report 2.4: the per-tick keep K, over the pre-contact 3-D speed. PES applies
  // it once per 1/54 s and this engine ticks at that same rate (Milestone A1), so
  // <func>KeepFactorPerStep</func> raises it to the power 1; the conversion is kept
  // so a caller stepping any other length stays exact.
  const float speed3d = std::sqrt(horizontal * horizontal + vy * vy);
  float keep = KeepFactorPerStep(RollingKeepFactor(c, mode, speed3d), step_s);
  // Report 2.5: the grounder kick. Its guards, its gate (old x, old z) and its vertical trim read the
  // state from before this tick's flight step (FUN_140876360's entry values at [rsp+0x48..0x60]).
  // Only K is multiplied; the trim lands on vy alone (report 2's correction 3, 0x140876a5c).
  const GrounderResult grounder = GrounderKick(c, mode, pre.position.x, pre.position.z, tickIndex,
                                               horizontalPre, pre.velocity.y);
  if (grounder.applied) keep *= KeepFactorPerStep(grounder.keepMultiplier, step_s);

  // Report 3.1: the tangential friction multiplier F. F is the per-tick ratio
  // PES multiplies into K, so it converts with the same exponent. The F of the
  // bounce (0x1408768a6/0x140876b26, [rbp+0x90]) is a different quantity, built
  // inside Bounce() from boundBoundRate and the s2/s4 ramps.
  const float friction = FrictionMultiplier(c, mode, speed3d, vy, spinRad, nullptr);
  keep *= KeepFactorPerStep(friction, step_s);

  // Report 1.3's vertical response, the literal rule from report 2:
  //   vy_out = -F * min(-imp, vy_trimmed)
  // Report 2's correction 3 gives the trim: `vy -= |v_xz|*grounderBoundYAdd*(1-f)`
  // at 0x140876a5c, stored to the min()'s second operand at 0x140876a61.
  const float vyTrimmed = grounder.applied ? vy - grounder.vyTrim_mps : vy;
  float newVy = Bounce(c, mode, vyTrimmed, horizontalPre, spinHorizontal).vy;

  // The horizontal: W = K*v + T, with T the tangential vector of PES's rolling block (contact-2 §2,
  // `Wx = K*vx + T_x`). A rolling ball (bVar3) skips the block.
  const TangentialXZ tangential =
      rolling ? TangentialXZ{0.0f, 0.0f}
              : RollingTangential(c, mode, pre, vx, vz, state->spinRaw.x * c.image.spinScale,
                                  state->spinRaw.z * c.image.spinScale);
  float newVx = vx * keep + tangential.x, newVz = vz * keep + tangential.z;

  // Report 3.4: the spin rebuild and the cap, from the post-contact velocity W. spin_raw := w_raw with
  // k = 1 on the airborne path (contact-2 §2).
  const float invSpinScale = c.image.invSpinScale;
  // PES's rolling spin, raw: w = (W_z, 0, -W_x)/r (live: the rolling records match this with ratio 1.0).
  // Report 3.4's cap scales X and Z only (0x14087708d, `xmm13, xmm12 *= 7.5/f`); W_y = 0 keeps Y at zero
  // (`[rbp+0x98] = 0`, contact-2 §2). The blend is k = 1 on the airborne path (contact-2 §2).
  const float capScale = SpinCapScale(c, std::sqrt(newVx * newVx + newVz * newVz));
  const float newSpinX = (newVz / r) * invSpinScale * capScale;
  const float newSpinY = 0.0f;
  const float newSpinZ = (-newVx / r) * invSpinScale * capScale;
  const float blendRaw = pes::kRawSpinBlendFactor;
  state->spinRaw.x = (newSpinX - state->spinRaw.x) * blendRaw + state->spinRaw.x;
  state->spinRaw.y = (newSpinY - state->spinRaw.y) * blendRaw + state->spinRaw.y;
  state->spinRaw.z = (newSpinZ - state->spinRaw.z) * blendRaw + state->spinRaw.z;

  // Report 3.4's stop gate, with report 2's correction 2: the gate zeroes ONLY
  // vx and vz (`xmm13`, `xmm3`, `xmm12` at 0x1408772d5/2d9/2dc; the vy register
  // `xmm5` is untouched at 0x140877323 and is stored straight through). A ball
  // whose vertical energy falls below the min-clamp is therefore re-launched at
  // F*imp every tick until its horizontal speed also drops under the gate -
  // which is why PES balls roll forever instead of stopping.
  const float stopSpeed = StopSpeed_mps(c, mode);
  const bool stopped = newVx * newVx + newVz * newVz < stopSpeed * stopSpeed;
  if (stopped) newVx = newVz = 0.0f;
  state->velocity.x = newVx;
  state->velocity.y = newVy;
  state->velocity.z = newVz;
}

bool BallTick(const pes::PesBallConstants& c, BallState* state, unsigned tickIndex, float dt_s) {
  if (IsResting(c, *state)) {
    // Report 2.1: a resting ball skips the flight function entirely. Nothing moves, and the
    // leftover vertical speed of the last contact goes too, so a still ball does not read moving.
    state->velocity = pes::Vec3(0.0f, 0.0f, 0.0f);
    return false;
  }
  // Report 2.1 and 2.3 (0x140876488, 0x140876800): a non-resting ball always takes its flight step
  // first, then the contact test runs on the new position. `r <= y` is airborne, so a ball sitting
  // exactly on the ground and still rising lifts by its integrated speed. Only a step that ends below
  // r is a contact, resolved in this same tick from the pre-flight state (report 6.3, 2.5).
  const BallState preFlight = *state;
  FlightTick(c, state, dt_s, false);
  if (state->position.y >= c.image.ballRadius) return false;
  ContactTick(c, state, tickIndex, true, false, dt_s, &preFlight);
  return true;
}

}  // namespace pesball
