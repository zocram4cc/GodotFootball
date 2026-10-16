#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesball.hpp"
#include "onthepitch/pesballphysics.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "pes_synthetic_ball.hpp"

// PES 2021's ball flight and ground contact (docs/re/pes-re-ball-contact.md,
// docs/re/pes-re-officials-ball.md 6.3, docs/re/pes-re-ball-constants.md).
//
// The pure functions are tested against synthetic constants, so no install is
// needed for the rules themselves; one test loads the real install's constants
// and checks the report's own numbers against them.

namespace {

using pes::BallRecordConstants;
using pes::PesBallConstants;
using pesball::BallState;

constexpr double kEps = 1e-6;

// --- the snap and the vertical response (report 2.3, 3.4) ---

TEST(PesBallContact, SnapsToRadiusAndKeepsHorizontalSpeed) {
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(1.0f, 0.05f, -2.0f);  // below r
  s.velocity = pes::Vec3(2.0f, -3.0f, 0.5f);
  pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
  EXPECT_NEAR(s.position.y, c.image.ballRadius, 1e-6);
  EXPECT_NEAR(s.position.x, 1.0f, 1e-6) << "a contact tick integrates no position";
  EXPECT_NEAR(s.position.z, -2.0f, 1e-6);
}

// The vertical keep of report 3.4 is the raw-spin-Y blend's input, NOT the
// vertical bounce (report 2 §2.1/§4.1). It is pinned here so the port keeps the
// literal rule available, and the contact tick is checked against the bounce
// formula that is the vertical response (report 1.3).
TEST(PesBallContact, VerticalKeepIsPesThreeToTenPercentAndIsNotTheBounce) {
  // Report 3.4, read literally from the raw: the vertical keep ramps from
  // boundToRotationBaseY to boundToRotationBaseY + boundToRotationAddRateY
  // across boundToRotationSpeedMinY .. MaxY (km/h), so mode 1 keeps 3% of |vy| at
  // rest and 10% above 0.7 km/h (0.19 m/s).
  const PesBallConstants c = SyntheticConstants();
  const float loV = c.record.At(BallRecordConstants::kBoundToRotationSpeedMinY, 1) / c.image.kmhPerMs;
  const float hiV = c.record.At(BallRecordConstants::kBoundToRotationSpeedMaxY, 1) / c.image.kmhPerMs;
  const float base = c.record.At(BallRecordConstants::kBoundToRotationBaseY, 1);
  const float add = c.record.At(BallRecordConstants::kBoundToRotationAddRateY, 1);
  EXPECT_NEAR(pesball::VerticalKeep(c, 1, loV * 0.5f), base, 1e-6);
  EXPECT_NEAR(pesball::VerticalKeep(c, 1, hiV * 2.0f), base + add, 1e-6);
  EXPECT_NEAR(pesball::VerticalKeep(c, 1, (loV + hiV) * 0.5f), base + add * 0.5f, 1e-6);

  // And that 3..10% is nowhere near what a contact does to vy: the bounce is
  // F*min(imp, |vy|), which is a speed clamp, not a keep.
  BallState hard;
  hard.position = pes::Vec3(0, c.image.ballRadius, 0);
  hard.velocity = pes::Vec3(0, -4.0f, 0);
  pesball::ContactTick(c, &hard, 0, true, false, pesball::kGfTick_s);
  EXPECT_GT(hard.velocity.y, 0.3f) << "a 4 m/s drop must not come back at 0.4 m/s";
}

// --- the bounce: report 1.3's min-clamp, the two regimes (report 2 §1.3/§5) ---

TEST(PesBallContact, BounceIsSpeedClampWhenTheContactIsFasterThanTheCap) {
  // A normal falling contact: |vy| is far larger than imp, so min(-imp, vy) is
  // the more-negative -imp... no: min is the smaller of the two, so for a steep
  // fall min(-imp, vy) = vy and the contact is a true reflection with ratio F.
  // imp is only >= |vy| for a slow/rolling contact - that is the clamp regime,
  // and it is the one that re-launches a grazed ball at F*imp.
  const PesBallConstants c = SyntheticConstants();
  const pesball::BounceResult b = pesball::Bounce(c, 2, -6.1746f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(b.minOperand, -6.1746f) << "z = vy for a falling contact";
  EXPECT_NEAR(b.speedCap_mps, 0.1111f, 1e-4) << "imp = (0.055*3.6*0 + 0.4)/3.6";
  EXPECT_NEAR(b.vy, 4.3222f, 1e-3) << "report 5.2 case A, contact #1 (mode 2)";
}

TEST(PesBallContact, BounceIsAReflectionOfTheIncomingSpeed) {
  // Report 5.2, the mode-2 series for a 2 m drop: boundCheckBoundAddRate[2] = 0
  // removes the speed ramp, so F is a flat boundBoundRate and every contact of
  // the series comes back at exactly 0.70 of |vy|.
  const PesBallConstants c = SyntheticConstants();
  const float kVy[] = {-6.1746f, -4.3948f, -3.0982f, -2.1898f, -1.5544f};
  const float kExpected[] = {4.3222f, 3.0764f, 2.1687f, 1.5329f, 1.0881f};
  for (int i = 0; i < 5; ++i) {
    const pesball::BounceResult b = pesball::Bounce(c, 2, kVy[i], 0.0f, 0.0f);
    EXPECT_NEAR(b.restitution, 0.7f, 1e-6);
    EXPECT_NEAR(b.vy, kExpected[i], 1e-3) << "contact #" << (i + 1);
    EXPECT_NEAR(b.vy / -kVy[i], 0.7f, 1e-3) << "ratio is F in mode 2";
  }
}

TEST(PesBallContact, BounceRelaunchesAGrazeAboveItsIncomingSpeed) {
  // A graze: |vy| <= imp, so min(-imp, vy) = -imp and the ball leaves at F*imp,
  // which is above its own inbound speed. That is the bobble regime report 5.3
  // identifies as the top of the live 0.37..2.26 band.
  const PesBallConstants c = SyntheticConstants();
  const float graze = 0.05f;  // |vy| far under imp
  const pesball::BounceResult b = pesball::Bounce(c, 2, -graze, 0.5f, 0.0f);
  EXPECT_FLOAT_EQ(b.minOperand, -b.speedCap_mps) << "z = -imp for a graze";
  EXPECT_NEAR(b.vy, 0.7f * b.speedCap_mps, 1e-6) << "vy_out = +F*imp";
  EXPECT_GT(b.vy, graze) << "a graze leaves faster than it arrived";
  const pesball::BounceResult stop = pesball::Bounce(c, 2, -0.01f, 1.0f, 0.0f);
  EXPECT_NEAR(stop.vy, 0.1163f, 1e-3) << "report 5.3's vxz=1, |vy|=0.010 row";
  EXPECT_NEAR(stop.vy / 0.01f, 11.628f, 0.01) << "its ratio, the report's 11.628";
}

TEST(PesBallContact, BounceDoesNotDependOnSpin) {
  // Report 3.3: the tangential vector is exactly horizontal (T_y = A_*e_y +
  // B_*e_y with e_y = 0), so a body-axis Y spin couples into vy nowhere in the
  // contact function. Report 5's sweep confirms it: vy_out is identical across
  // spin = +-20 rps. F does carry a `s4^2 * rollToBoundAdd` term, but over
  // hypot(wx, wz) - the HORIZONTAL spin - so a pure Y spin must not move it.
  const PesBallConstants c = SyntheticConstants();
  auto bounceWith = [&](float spinRawY) {
    BallState s;
    s.position = pes::Vec3(0, c.image.ballRadius, 0);
    s.velocity = pes::Vec3(5.0f, -4.99f, 0.0f);
    s.spinRaw = pes::Vec3(0.0f, spinRawY, 0.0f);
    pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
    return s.velocity.y;
  };
  const float none = bounceWith(0.0f);
  EXPECT_FLOAT_EQ(bounceWith(10.0f * 2.0f * 3.14159265f), none)
      << "10 rps topspin does not change vy";
  EXPECT_FLOAT_EQ(bounceWith(-10.0f * 2.0f * 3.14159265f), none)
      << "10 rps backspin does not change vy";
}

TEST(PesBallContact, RollSpinRampAddsRestitutionThroughTheHorizontalSpin) {
  // Report 3.3/2.2: F carries `s4^2 * rollToBoundAdd`, s4 being the clamped ramp over the horizontal spin in Hz,
  // |w_xz|/(2 pi), from rollToBoundRollMin (2 Hz) to rollToBoundRollMax (7 Hz). So a horizontal spin raises F
  // above boundBoundRate, while a small spin does not. The spin arguments below are rad/s.
  const PesBallConstants c = SyntheticConstants();
  const float twoPi = 2.0f * std::acos(-1.0f);
  EXPECT_NEAR(pesball::RollSpinRamp(c, 2, 0.0f), 0.0f, 1e-6);
  EXPECT_NEAR(pesball::RollSpinRamp(c, 2, 2.0f * twoPi), 0.0f, 1e-6);
  EXPECT_NEAR(pesball::RollSpinRamp(c, 2, 7.0f * twoPi), 1.0f, 1e-6);
  EXPECT_NEAR(pesball::RollSpinRamp(c, 2, 4.5f * twoPi), 0.5f, 1e-6);
  const float flat = pesball::Bounce(c, 2, -0.5f, 0.0f, 0.0f).restitution;
  const float spinning = pesball::Bounce(c, 2, -0.5f, 0.0f, 7.0f * twoPi).restitution;
  EXPECT_NEAR(spinning, flat + c.record.rollToBoundAdd, 1e-6)
      << "s4 = 1 adds s4^2 * rollToBoundAdd = rollToBoundAdd";
  EXPECT_GT(spinning, flat) << "a rolling spin makes the ball livelier";
}

// --- the grounder kick (report 2.5) ---

TEST(PesBallContact, GrounderKickOnlyFiresOnFastShallowShots) {
  const PesBallConstants c = SyntheticConstants();
  const float fast = 6.0f;   // 21.6 km/h, above grounderSpeed 20
  const float slow = 2.0f;   // 7.2 km/h, below it

  auto kick = pesball::GrounderKick(c, 1, 1.0f, 2.0f, 0, fast, -0.2f);
  EXPECT_TRUE(kick.applied) << "a fast shallow shot takes the driven grounder";
  EXPECT_GT(kick.keepMultiplier, 0.98f);
  EXPECT_NEAR(kick.vyTrim_mps, fast * 0.09f, 0.01f);

  EXPECT_FALSE(pesball::GrounderKick(c, 1, 1.0f, 2.0f, 0, slow, -0.2f).applied);
  EXPECT_FALSE(pesball::GrounderKick(c, 1, 1.0f, 2.0f, 0, fast, 0.0f).applied) << "vy == 0 skips";
  EXPECT_FALSE(pesball::GrounderKick(c, 1, 1.0f, 2.0f, 0, fast, -4.0f).applied)
      << "a steep shot is not a grounder";
}

TEST(PesBallContact, GrounderKickRespectsItsTickGate) {
  // The gate is `int(|x|*1000) mod frictionRollSpeedStop[mode][3] == 0`, so only
  // one tick in five fires it for mode 1 (the shipped array holds 5).
  const PesBallConstants c = SyntheticConstants();
  int fired = 0;
  for (float x = 0.0f; x < 5.0f; x += 0.001f)
    if (pesball::GrounderKick(c, 1, x, x, 0, 6.0f, -0.2f).applied) ++fired;
  EXPECT_GT(fired, 0);
  EXPECT_LT(fired, 5000) << "the gate must thin the branch out";
}

// --- the friction multiplier F (report 3.1) ---

TEST(PesBallContact, FrictionMultiplierIsOneWithoutSpinAndBoostedByBounce) {
  const PesBallConstants c = SyntheticConstants();
  float baseA = 0.0f;
  // Mode 1: boundCheckFrictionAddRate = 0, so s1 never changes K.
  EXPECT_NEAR(pesball::FrictionMultiplier(c, 1, 6.0f, -1.0f, 12.0f, &baseA), 1.0f, 1e-6);
  // A hard bounce raises A through s3 (boundCheckBoundAddRate = 5, i.e. +5%).
  EXPECT_GT(pesball::FrictionMultiplier(c, 1, 6.0f, -4.0f, 0.0f, &baseA), 0.99f);
  EXPECT_GT(baseA, 0.7f);
  // Mode 4 does carry a friction add rate (7.5), and high spin engages it.
  float baseA4 = 0.0f;
  EXPECT_LT(pesball::FrictionMultiplier(c, 4, 6.0f, -1.0f, 20.0f, &baseA4), 1.0f);
}

// --- the spin cap and rebuild (report 3.4) ---

TEST(PesBallContact, SpinIsCappedAtBoundRollMaxHertzOfHorizontalRolling) {
  // Report 3.4 as PES does it (FUN_140876360 0x14087704e..0x140877081): the horizontal rolling frequency
  // f = |v_xz| / (2 pi r) is capped at boundRollMax (a frequency, in Hz), and the X and Z spin scale by
  // boundRollMax / f. Y is not scaled.
  const PesBallConstants c = SyntheticConstants();
  const float twoPiR = 2.0f * 3.14159274f * c.image.ballRadius;
  EXPECT_NEAR(pesball::SpinCapScale(c, 2.0f), 1.0f, 1e-6);
  EXPECT_NEAR(pesball::SpinCapScale(c, 15.0f), c.record.boundRollMax * twoPiR / 15.0f, 1e-5);
}

TEST(PesBallContact, ARollingBallKeepsItsRollingSpinConvention) {
  // PES's rolling spin is w = (v_z/r, 0, -v_x/r) in raw units (live records: 13,518 rolling rows match
  // this exactly, ratio 1.0). A ball already rolling at 4 m/s on the ground keeps its velocity and
  // its spin: its tangential is zero (pure rolling has slip zero).
  const PesBallConstants c = SyntheticConstants();
  const float r = c.image.ballRadius;
  BallState s;
  s.position = pes::Vec3(0, r, 0);
  s.velocity = pes::Vec3(4.0f, 0.0f, 0.0f);
  s.spinRaw = pes::Vec3(0, 0, -4.0f / r / c.image.spinScale);
  pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
  EXPECT_NEAR(s.spinRaw.x, 0.0f, 1e-6f) << "no spin across the heading";
  EXPECT_NEAR(s.spinRaw.y, 0.0f, 1e-6f);
  EXPECT_NEAR(s.spinRaw.z * c.image.spinScale, -s.velocity.x / r, 1e-2f) << "w_z = -v_x / r";
}

TEST(PesBallContact, RollingSpinAboveTheCapKeepsTheCappedHorizontalFrequency) {
  // 20 m/s on the ground: f = 28.7 Hz, far above boundRollMax (7.5 Hz). The horizontal spin magnitude is
  // then capped at 2 pi * boundRollMax rad/s, and the spin is still on the rolling axis.
  const PesBallConstants c = SyntheticConstants();
  const float r = c.image.ballRadius;
  BallState s;
  s.position = pes::Vec3(0, r, 0);
  s.velocity = pes::Vec3(20.0f, 0.0f, 0.0f);
  s.spinRaw = pes::Vec3(0, 0, -20.0f / r / c.image.spinScale);
  pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
  const float horizontalRad = std::sqrt(s.spinRaw.x * s.spinRaw.x + s.spinRaw.z * s.spinRaw.z) *
                              c.image.spinScale;
  EXPECT_NEAR(horizontalRad, 2.0f * 3.14159274f * c.record.boundRollMax, 1e-3f * horizontalRad);
  EXPECT_NEAR(s.spinRaw.x, 0.0f, 1e-6f);
}


// --- the stop gate (report 3.4, report 2 correction 2) ---

TEST(PesBallContact, StopGateZeroesOnlyTheHorizontalVelocity) {
  const PesBallConstants c = SyntheticConstants();
  const float stop = pesball::StopSpeed_mps(c, 1);
  EXPECT_NEAR(stop, 0.175f * 1000.0f / 3600.0f, 1e-6);
  EXPECT_NEAR(stop, 0.04861f, 1e-4);

  // A ball slower than the stop speed - measured on the post-contact horizontal
  // velocity, which is what the raw compares - loses its horizontal motion, and
  // keeps its vertical one: report 2's correction 2 zeroes xmm13, xmm3 and xmm12
  // (vx, and the two vz stores) and leaves xmm5 (vy) untouched at 0x140877323.
  BallState s;
  s.position = pes::Vec3(0, c.image.ballRadius, 0);
  s.velocity = pes::Vec3(0.001f, -0.02f, 0.001f);  // horizontal 0.0014 m/s, below
  pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
  EXPECT_EQ(s.velocity.x, 0.0f);
  EXPECT_EQ(s.velocity.z, 0.0f);
  EXPECT_GT(s.velocity.y, 0.0f) << "the vertical velocity survives the stop gate";

  // A rolling ball is not stopped, and its vertical is likewise untouched.
  BallState rolling;
  rolling.position = pes::Vec3(0, c.image.ballRadius, 0);
  rolling.velocity = pes::Vec3(1.0f, -0.5f, 0.0f);
  pesball::ContactTick(c, &rolling, 0, true, false, pesball::kGfTick_s);
  EXPECT_GT(std::fabs(rolling.velocity.x), 0.5f) << "a rolling ball keeps rolling";
}

// --- the contact mode table (report 3.1) ---

TEST(PesBallContact, ContactModeComesFromThePitchWeatherTable) {
  // The table report 3.1 read out of FUN_1408723d0's operand, stated here as the
  // rule under test. Rows = pitch, columns = weather; a dry, normal pitch (weather
  // 0) is mode 2 on every row, never the mode 1 the reports observed live.
  PesBallConstants c = SyntheticConstants();
  c.image.modeTable = {{{2, 1, 1}, {2, 1, 0}, {1, 0, 0}}};
  c.contactMode = 1;
  EXPECT_EQ(pes::ContactModeFor(c.image, 0, 0), 2);
  EXPECT_EQ(pes::ContactModeFor(c.image, 1, 0), 2);
  EXPECT_EQ(pes::ContactModeFor(c.image, 2, 0), 1);
  EXPECT_EQ(pes::ContactModeFor(c.image, 0, 1), 1);
  EXPECT_EQ(pes::ContactModeFor(c.image, 1, 1), 1);
  EXPECT_EQ(pes::ContactModeFor(c.image, 2, 2), 0);
  // The writer's out-of-range fallback is 1.
  EXPECT_EQ(pes::ContactModeFor(c.image, 3, 0), pes::kContactModeOutOfRange);
  EXPECT_EQ(pes::ContactModeFor(c.image, 0, 9), pes::kContactModeOutOfRange);
  EXPECT_EQ(pes::ContactModeFor(c.image, -1, 0), pes::kContactModeOutOfRange);
}

TEST(PesBallContact, DryNormalIsModeTwoAndModeTwoHasASeventyPercentBounce) {
  // Report 3.2: a dry, normal pitch is mode 2, which no match plays; GF plays the live mode 1.
  const PesBallConstants c = SyntheticConstants();
  EXPECT_EQ(pes::kContactModeDryNormal, 2);
  EXPECT_EQ(pes::WeatherIndexForGfWeather(0.0f), 0) << "no weather";
  // boundCheckBoundAddRate[2] = 0 removes mode 1's speed ramp, so F is a flat
  // boundBoundRate = 0.7 at every speed in the dry-normal mode.
  EXPECT_NEAR(c.record.At(BallRecordConstants::kBoundCheckBoundAddRate, 2), 0.0f, 1e-6);
  for (const float vy : {-0.05f, -0.5f, -3.0f, -8.0f}) {
    const pesball::BounceResult slow = pesball::Bounce(c, 2, vy, 0.5f, 0.0f);
    EXPECT_NEAR(slow.restitution, 0.7f, 1e-6) << "F is flat in mode 2 at vy = " << vy;
  }
}

// --- rolling K(V) and the report's own numbers ---

TEST(PesBallContact, RollingKeepReproducesTheObserved0981AtTwoMetresPerSecond) {
  // Report 5.2, computed from the tables with no fitting: at v = 2 m/s the
  // mode-1 slow branch gives K = 0.98100 exactly.
  const PesBallConstants c = SyntheticConstants();
  const float k = pesball::RollingKeepFactor(c, 1, 2.0f);
  EXPECT_NEAR(k, 0.981f, 1e-6);
  // ... so the per-tick deceleration is (1 - K) * 54 * v = 2.05 m/s^2.
  EXPECT_NEAR((1.0f - k) * 54.0f * 2.0f, 2.05f, 0.01f);
}

TEST(PesBallContact, RollingDecelerationIsAboutTwoMetresPerSecondSquared) {
  // Report 5.2/6.4's live window: a ball rolling at 0.5-3 m/s decelerates at
  // about 2.05 m/s^2, which is (1 - K) * 54 * v at 1/54 s. The instantaneous
  // figure falls as the ball slows, so check the figure at 2 m/s directly, then
  // that a second of PES ticks really does take the speed down.
  const PesBallConstants c = SyntheticConstants();
  const float k = pesball::RollingKeepFactor(c, 1, 2.0f);
  const float decel = (1.0f - k) * c.image.tickHz * 2.0f;
  EXPECT_NEAR(decel, 2.05f, 0.01f);
  float v = 2.0f;
  const float dt = 1.0f / 54.0f;
  for (int i = 0; i < 54; ++i) v *= pesball::RollingKeepFactor(c, 1, v);
  // A second of PES ticks must remove more than one tick's worth of the 2 m/s
  // figure, and cannot take the ball past a standstill.
  EXPECT_LT(v, 2.0f - 2.05f * dt) << "the decay is at least one tick's 2 m/s figure";
  EXPECT_GT(v, 0.0f) << "the ball is still rolling after a second";
}

// --- the 10 ms conversion ---

TEST(PesBallConversion, KeepFactorIsItselfOnTheSimsOwnTick) {
  // PES applies K once per 1/54 s, and the sim's tick is PES's own 1/54 s
  // (Milestone A1), so the per-tick factor is K itself: a second of ticks is
  // exactly 54 applications of K.
  const float perPesTick = 0.981f;
  const float perGfTick = pesball::KeepFactorPerStep(perPesTick, pesball::kGfTick_s);
  EXPECT_NEAR(perGfTick, perPesTick, 1e-6);
  EXPECT_NEAR(perGfTick, std::pow(perPesTick, 1.0f), 1e-6);
  float gf = 1.0f, pes = 1.0f;
  for (int i = 0; i < 54; ++i) gf *= perGfTick;
  for (int i = 0; i < 54; ++i) pes *= perPesTick;
  EXPECT_FLOAT_EQ(gf, pes);
  // A step of any other length still converts exactly, so the render side's own
  // sub-tick would stay PES.
  EXPECT_NEAR(pesball::KeepFactorPerStep(perPesTick, 0.01f), std::pow(perPesTick, 0.54f), 1e-6);
}

TEST(PesBallConversion, FlightTickIntegratesTheAccelerationOverTheTickStep) {
  // The flight model is an acceleration, integrated with dt = the sim's tick
  // (Milestone A1: one PES tick, 1/54 s).
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(0, 1.0f, 0);
  s.velocity = pes::Vec3(0, 0, 20.0f);  // 72 km/h, above the crisis band
  // One tick is the acceleration at the tick's entry velocity held across the
  // step: semi-implicit Euler integrates the acceleration first.
  const pes::Vec3 a = pesball::FlightAcceleration(c, s, 0, 0, 0, false);
  const float v0 = s.velocity.z;
  pesball::FlightTick(c, &s, pesball::kGfTick_s, false);
  const float dv = s.velocity.z - v0;
  EXPECT_LT(dv, 0.0f) << "drag slows the ball down";
  EXPECT_NEAR(dv, a.z * pesball::kGfTick_s, 1e-6);
  // Gravity is applied on top: after a second of ticks the ball has fallen.
  BallState falling;
  falling.position = pes::Vec3(0, 5.0f, 0);
  for (int i = 0; i < 54; ++i) pesball::FlightTick(c, &falling, pesball::kGfTick_s, false);
  EXPECT_LT(falling.position.y, 5.0f);
}

// --- the flight model (report 6.3) ---

TEST(PesBallFlight, BelowTheCrisisThresholdGravityOnly) {
  // Report 6.3: s2 <= 7.716049 (below 2.778 m/s horizontal) is gravity only -
  // no drag and no Magnus.
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(0, 1.0f, 0);
  s.velocity = pes::Vec3(0, 0, 2.0f);
  s.spinRaw = pes::Vec3(0, 2.0f, 0);  // a huge spin that must not bend it
  const pes::Vec3 a = pesball::FlightAcceleration(c, s, 0, 1000.0f, 0, false);
  EXPECT_NEAR(a.x, 0.0f, 1e-6);
  EXPECT_NEAR(a.z, 0.0f, 1e-6);
  EXPECT_NEAR(a.y, -c.image.gravity, 1e-6);
}

TEST(PesBallFlight, DragCrisisBandShapesTheDrag) {
  // Report 6.3's shaping: between dragSpeedMin and dragSpeedMax the effective
  // speed is mirrored (2*v2 - s), and above the max it is offset
  // (2*(v2 - v1) + s). The drag magnitude therefore dips inside the band.
  const PesBallConstants c = SyntheticConstants();
  auto dragMagnitude = [&](float mps) {
    BallState s;
    s.velocity = pes::Vec3(mps, 0, 0);
    const pes::Vec3 a = pesball::FlightAcceleration(c, s, 0, 0, 0, true);
    return std::fabs(a.x);
  };
  const float v2 = c.record.dragSpeedMin / c.image.kmhPerMs;  // 15.83 m/s
  const float v1 = c.record.dragSpeedMax / c.image.kmhPerMs;  // 26.94 m/s
  const float mid = (v1 + v2) * 0.5f;
  const float below = dragMagnitude(v2 * 0.9f);
  const float inside = dragMagnitude(mid);
  const float above = dragMagnitude(v1 * 1.1f);
  EXPECT_GT(below, 0.0f);
  EXPECT_GT(above, below) << "faster still means more drag";
  // Inside the band the shaped speed is mirrored to 2*v2 - s, while a ball
  // actually moving at that mirrored speed takes the plain branch - so the drag
  // force per unit velocity (the k factor) is identical for both.
  auto shapedSpeed = [&](float mps) {
    const float v1 = c.record.dragSpeedMax / c.image.kmhPerMs;
    const float v2 = c.record.dragSpeedMin / c.image.kmhPerMs;
    if (mps <= v1) return mps > v2 ? (2.0f * v2 - mps) : mps;
    return 2.0f * (v2 - v1) + mps;
  };
  EXPECT_NEAR(shapedSpeed(mid), shapedSpeed(2.0f * v2 - mid), 1e-4)
      << "the band mirrors the speed onto the plain branch";
  // The mirror falls as the speed rises across the band: that is the crisis dip.
  EXPECT_LT(shapedSpeed(mid + 1.0f), shapedSpeed(mid - 1.0f));
  // Above the max, the offset branch keeps climbing again.
  EXPECT_GT(shapedSpeed(v1 * 1.1f), shapedSpeed(v1));
}

TEST(PesBallFlight, MagnusBendsTheBallWithItsSpin) {
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.velocity = pes::Vec3(0, 0, 20.0f);  // +z
  s.spinRaw = pes::Vec3(0, 0, 0);
  const pes::Vec3 spin(0, 1, 0);  // rad/s, +y spin (backspin against +z motion)
  const pes::Vec3 a = pesball::FlightAcceleration(c, s, spin.x, spin.y, spin.z, false);
  const pes::Vec3 none = pesball::FlightAcceleration(c, s, 0, 0, 0, false);
  // cross(w, v) with w = +y and v = +z is +x, so the ball curves sideways.
  EXPECT_GT(a.x, none.x + 1e-6f);
  EXPECT_NEAR(a.y, none.y, 1e-6) << "a pure side spin does not change the vertical drag";
}

// --- resting (report 2.1) ---

TEST(PesBallContact, RestingBallSkipsTheFlightFunction) {
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(0, c.image.ballRadius, 0);
  s.velocity = pes::Vec3(0, 0, 0);
  EXPECT_TRUE(pesball::IsResting(c, s));
  s.velocity = pes::Vec3(0.1f, 0, 0);
  EXPECT_FALSE(pesball::IsResting(c, s)) << "a rolling ball is not resting";
  s.velocity = pes::Vec3(0, 0, 0);
  s.spinRaw = pes::Vec3(0.05f, 0, 0);
  EXPECT_FALSE(pesball::IsResting(c, s)) << "a spinning ball is not resting";
}

TEST(PesBallContact, ABallMovingUpAtRestHeightIsNotResting) {
  // Report 2.1 (FUN_140876360 0x14087648e..0x1408764c8): the resting test squares the 3-D pre-velocity, not the
  // horizontal. A ball at rest height with only vertical speed is airborne: it takes its flight step and keeps
  // the upward speed (gravity takes g/54 off it in one tick), instead of being zeroed as a resting ball.
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(0, c.image.ballRadius, 0);
  s.velocity = pes::Vec3(0, 2.0f, 0);
  EXPECT_FALSE(pesball::IsResting(c, s));
  EXPECT_FALSE(pesball::BallTick(c, &s, 0, pesball::kGfTick_s));
  EXPECT_GT(s.velocity.y, 1.5f) << "the flight step keeps the upward speed: " << s.velocity.y;
  EXPECT_GT(s.position.y, c.image.ballRadius);
}

// --- the mode column and its clamp (report 0.3) ---

TEST(PesBallContact, ModeIsClampedAboveFive) {
  EXPECT_EQ(pesball::ClampMode(1), 1);
  EXPECT_EQ(pesball::ClampMode(5), 5);
  EXPECT_EQ(pesball::ClampMode(6), 1);
  EXPECT_EQ(pesball::ClampMode(99), 1);
}

TEST(PesBallContact, ModesSelectDifferentColumns) {
  const PesBallConstants c = SyntheticConstants();
  // Mode 4 (0.99 / 0.98 / 0.97) keeps far more speed than mode 2
  // (0.969 / 0.979 / 0.969) at the same speed.
  EXPECT_GT(pesball::RollingKeepFactor(c, 4, 2.0f), pesball::RollingKeepFactor(c, 2, 2.0f));
}

// --- the real install ---

// The install's constants: one test, so a rule that depends on the shipped data
// is checked against it, and a synthetic-only test can never drift from it.
TEST(PesBallInstall, LoadsTheInstallConstantsAndReproducesTheReportNumbers) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the ball constants come from the install";
  std::string err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto constants = pes::LoadPesBallConstants(*exe, vfs, &err);
  ASSERT_TRUE(constants.has_value()) << err;
  const PesBallConstants& c = *constants;

  // docs/re/pes-re-ball-constants.md 3.1: the three flight scalars.
  EXPECT_NEAR(c.record.dragSpeedMax, 96.995003f, 1e-3);
  EXPECT_NEAR(c.record.dragSpeedMin, 56.995602f, 1e-3);
  EXPECT_NEAR(c.record.magnusRate, 0.035f, 1e-6);
  // 6.1/6.2: the contact tables' mode-1 column.
  EXPECT_NEAR(c.record.At(BallRecordConstants::kFrictionRollSpeedMin, 1), 8.0f, 1e-5);
  EXPECT_NEAR(c.record.At(BallRecordConstants::kFrictionRollSpeedMax, 1), 25.0f, 1e-5);
  EXPECT_NEAR(c.record.At(BallRecordConstants::kFrictionRollRateMin, 1), 0.982f, 1e-5);
  EXPECT_NEAR(c.record.At(BallRecordConstants::kFrictionRollRateMax, 1), 0.972f, 1e-5);
  EXPECT_NEAR(c.record.At(BallRecordConstants::kFrictionRollRateStop, 1), 0.972f, 1e-5);
  EXPECT_NEAR(c.record.boundRollMax, 7.5f, 1e-5);
  EXPECT_NEAR(c.record.stopSpeed, 0.175f, 1e-5);
  EXPECT_NEAR(c.record.At(BallRecordConstants::kBoundToRotationBaseY, 1), 0.03f, 1e-5);
  EXPECT_NEAR(c.record.At(BallRecordConstants::kBoundToRotationAddRateY, 1), 0.07f, 1e-5);
  EXPECT_NEAR(c.record.rollToSpeedMaxRoll, 8.5f, 1e-5);
  EXPECT_NEAR(c.record.rollToSpeedSpeedYMax, 0.5f, 1e-5);
  EXPECT_NEAR(c.record.rollToSpeedSpeedYMin, 0.1f, 1e-5);
  EXPECT_NEAR(c.record.At(BallRecordConstants::kBoundRate, 1), 0.7f, 1e-5);

  // The image constants (officials 6.2 / ball-constants 4).
  EXPECT_NEAR(c.image.gravity, 9.80665f, 1e-4);
  EXPECT_NEAR(c.image.ballRadius, 0.10868593f, 1e-6);
  EXPECT_NEAR(c.image.dragCrisisSquared, 7.7160487f, 1e-5);
  EXPECT_NEAR(c.image.tickHz, 54.0f, 1e-4);
  EXPECT_NEAR(c.image.spinScale, 492.181f, 1e-3);
  EXPECT_NEAR(c.image.invSpinScale, 1.0f / 492.181f, 1e-8);

  // Report 3.1's contact-mode table, read out of the exe's .data at run time.
  // The table is a runtime-written slot, not .rdata; this engine reads the
  // install's own copy, so no on-disk value is transcribed here.
  EXPECT_EQ(pes::ContactModeFor(c.image, 0, 0), 2) << "a dry, normal pitch is mode 2";
  EXPECT_EQ(pes::ContactModeFor(c.image, 1, 0), 2);
  EXPECT_EQ(pes::ContactModeFor(c.image, 0, 1), 1);
  EXPECT_EQ(pes::ContactModeFor(c.image, 1, 1), 1);
  EXPECT_EQ(pes::ContactModeFor(c.image, 2, 0), 1);
  EXPECT_EQ(pes::ContactModeFor(c.image, 1, 2), 0);
  EXPECT_EQ(pes::ContactModeFor(c.image, 2, 2), 0);
  EXPECT_EQ(pes::ContactModeFor(c.image, 5, 0), 1) << "out of range falls back to 1";
  // Report 5.2's mode-2 dry-pitch numbers, from the shipped mode-2 table.
  EXPECT_NEAR(c.record.At(BallRecordConstants::kBoundCheckBoundAddRate, 2), 0.0f, 1e-5);
  EXPECT_NEAR(pesball::Bounce(c, 2, -6.1746f, 0.0f, 0.0f).vy, 4.3222f, 1e-3);
  EXPECT_NEAR(pesball::Bounce(c, 2, -4.9883f, 0.0f, 0.0f).vy, 3.4918f, 1e-3);

  // Report 5.2's own numbers, from the shipped tables.
  EXPECT_NEAR(pesball::RollingKeepFactor(c, 1, 2.0f), 0.981f, 1e-6);
  const float k = pesball::RollingKeepFactor(c, 1, 2.0f);
  EXPECT_NEAR((1.0f - k) * c.image.tickHz * 2.0f, 2.05f, 0.01f);
  EXPECT_NEAR(pesball::StopSpeed_mps(c, 1), 0.175f * 1000.0f / 3600.0f, 1e-6);
  EXPECT_NEAR(pesball::SpinCapScale(c, 10.0f),
              c.record.boundRollMax * 2.0f * 3.14159274f * c.image.ballRadius / 10.0f, 1e-5);

  // The mode-1 vertical keep: 3% at rest, ramping to 10% across 0.1 .. 0.7 km/h
  // (report 3.4, the raw's boundToRotation*Y band; the report's text says the
  // low bound is 8 km/h = frictionRollSpeedMin - a discrepancy recorded in the
  // final message).
  EXPECT_NEAR(pesball::VerticalKeep(c, 1, 0.0f), 0.03f, 1e-6);
  EXPECT_NEAR(pesball::VerticalKeep(c, 1, 4.0f), 0.10f, 1e-6);
  const float loV = c.record.At(BallRecordConstants::kBoundToRotationSpeedMinY, 1);
  const float hiV = c.record.At(BallRecordConstants::kBoundToRotationSpeedMaxY, 1);
  EXPECT_NEAR(pesball::VerticalKeep(c, 1, (loV + hiV) * 0.5f / c.image.kmhPerMs), 0.065f, 1e-6);
}

// --- the shot that reaches the goal (the bug this suite regressed on) ---

// A ground-level shot from the penalty spot, through the same loop ball.cpp
// runs: contact ticks rewrite the velocity and then carry the ball forward,
// flight ticks integrate the acceleration. The ball must cross the goal plane.
//
// The bug: the per-tick keep factors PES applies once per 1/54 s were applied
// once per GF tick (10 ms), so a GF tick lost 1.85x the speed PES loses per
// second (0.972^100 = 0.058 vs 0.972^54 = 0.216). A 25 m/s shot from the
// penalty spot stopped at 7.19 m, 9 m short of the goal plane at 56.31 m, and
// every penalty kick in a drawn match came back MISSED/SAVED.
TEST(PesBallShot, GroundShotFromThePenaltySpotCrossesTheGoalPlane) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the ball constants come from the install";
  std::string err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto cc = pes::LoadPesBallConstants(*exe, vfs, &err);
  ASSERT_TRUE(cc.has_value()) << err;
  const PesBallConstants c = *cc;

  // gametypes.hpp: the goal plane is pitchHalfW + lineHalfW + ballRadius
  // (CheckForGoal's triangles). goalHalfWidth is lateral, not longitudinal.
  const float goalPlane = 52.5f + 0.06f + c.image.ballRadius;
  const float kDt = pesball::kGfTick_s;  // one tick of ball.cpp's own loop

  // The penalty spot is 11 m out; a 25 m/s shot (90 km/h) is a struck penalty.
  BallState s;
  s.position = pes::Vec3(41.5f, c.image.ballRadius, 0.0f);
  s.velocity = pes::Vec3(25.0f, 0.0f, 0.0f);

  unsigned contactTicks = 0;
  bool crossed = false;
  for (int i = 0; i < 3000 && !crossed; ++i) {
    if (s.position.y <= c.image.ballRadius) {
      pesball::ContactTick(c, &s, contactTicks, true, false, pesball::kGfTick_s);
      ++contactTicks;
      s.position.x += s.velocity.x * kDt;
      s.position.y += s.velocity.y * kDt;
      s.position.z += s.velocity.z * kDt;
    } else {
      pesball::FlightTick(c, &s, kDt, false);
    }
    if (s.position.x >= goalPlane) crossed = true;
  }
  EXPECT_TRUE(crossed) << "a 25 m/s ground shot from the penalty spot must reach the goal "
                       << "(it reached x = " << s.position.x << " of " << goalPlane << " m)";
}

// One second of the sim's ticks must lose the same speed PES loses in a second of
// its own ticks: the per-tick factors are converted, not re-applied.
TEST(PesBallConversion, ASecondOfSimTicksIsASecondOfPesTicksWhileRolling) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the ball constants come from the install";
  std::string err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto cc = pes::LoadPesBallConstants(*exe, vfs, &err);
  ASSERT_TRUE(cc.has_value()) << err;
  const PesBallConstants c = *cc;

  // A ball rolling below the spin cap has spin v/r and no slip (live: the rolling records, convention
  // w = (v_z, 0, -v_x)/r). Report 3.3's block then adds nothing: A_dot = Hz, S3 = Hz, B = 1, so A = 0 and
  // the horizontal is K per tick (report 5.2). The state carries across the ticks. The cap is 2 pi
  // boundRollMax rad/s, i.e. 5.12 m/s on the ground (report 3.4), so 4 m/s is below it.
  float vPes = 4.0f, vGf = 4.0f;
  for (int i = 0; i < 54; ++i) vPes *= pesball::RollingKeepFactor(c, pesball::ClampMode(c.contactMode), vPes);
  const float r = c.image.ballRadius;
  BallState s;
  s.position = pes::Vec3(0, r, 0);
  s.velocity = pes::Vec3(vGf, 0.0f, 0.0f);
  s.spinRaw = pes::Vec3(0, 0, -vGf / r / c.image.spinScale);
  for (int i = 0; i < 54; ++i) {
    pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
    vGf = s.velocity.x;
  }
  EXPECT_NEAR(vGf, vPes, vPes * 0.01)
      << "54 sim ticks and 54 PES ticks are both one second: " << vGf << " vs " << vPes;
  EXPECT_NEAR(s.velocity.z, 0.0f, 1e-4f) << "a rolling ball does not drift sideways";
}

TEST(PesBallConversion, ARollingBallAboveTheSpinCapLosesMoreThanItsRollingKeep) {
  // Above 5.12 m/s the spin cap (report 3.4, f^2 against boundRollMax^2) stops the spin at 2 pi * 7.5 rad/s,
  // short of v/r, so the contact slips and friction takes more speed than the rolling keep alone (report
  // 3.3's block). A ball that starts rolling at 10 m/s, with its state carried, must end below the rolling
  // keep's own 54-tick decay.
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the ball constants come from the install";
  std::string err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto cc = pes::LoadPesBallConstants(*exe, vfs, &err);
  ASSERT_TRUE(cc.has_value()) << err;
  const PesBallConstants c = *cc;
  float vPes = 10.0f, vGf = 10.0f;
  for (int i = 0; i < 54; ++i) vPes *= pesball::RollingKeepFactor(c, pesball::ClampMode(c.contactMode), vPes);
  const float r = c.image.ballRadius;
  BallState s;
  s.position = pes::Vec3(0, r, 0);
  s.velocity = pes::Vec3(vGf, 0.0f, 0.0f);
  s.spinRaw = pes::Vec3(0, 0, -vGf / r / c.image.spinScale);
  for (int i = 0; i < 54; ++i) {
    pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
    vGf = s.velocity.x;
  }
  EXPECT_LT(vGf, vPes) << "the capped spin slips, so the ball ends slower than its rolling keep alone: " << vGf
                       << " vs " << vPes;
}
TEST(PesBallContact, SpinAboutTheSidewaysAxisPushesARollingBallAcrossItsHeading) {
  // Report 3.3 (FUN_140876360 0x140876c0f..0x140876f6e): the rolling block builds the tangential vector
  // from the spin across and along the pre-flight heading e. With e along +x, the spin about x enters
  // only the along-track term B, and T_z = -B*e_x, so the sideways speed follows the spin's sign.
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(0.0f, c.image.ballRadius, 0.0f);
  s.velocity = pes::Vec3(6.0f, 0.0f, 0.0f);
  s.spinRaw = pes::Vec3(0.02f, 0.0f, 0.0f);  // 9.8 rad/s about x: a clear spin, below the cap
  pesball::ContactTick(c, &s, 0, true, false, pesball::kGfTick_s);
  EXPECT_GT(s.velocity.z, 0.05f) << "topspin about +x pushes the ball toward +z";
}

TEST(PesBallContact, ABallRollingSlowerThanTwoMetresPerSecondIgnoresTheTangential) {
  // Report 3.3's entry test: a ball that starts exactly on the ground and moves under 2.2222 m/s takes
  // the rolling flag (bVar3), which skips the block. Its horizontal velocity is K times its own.
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(0.0f, c.image.ballRadius, 0.0f);
  s.velocity = pes::Vec3(1.0f, 0.0f, 0.0f);
  s.spinRaw = pes::Vec3(0.02f, 0.0f, 0.0f);
  pesball::BallTick(c, &s, 0, pesball::kGfTick_s);
  EXPECT_NEAR(s.velocity.z, 0.0f, 1e-6f);
}

TEST(PesBallContact, ALandingWithNoHorizontalSpeedPushesAlongItsSidewaysSpinBoundedly) {
  // Report 3.3 (FUN_140876360 0x140876c28..0x140876c79): with no horizontal speed before the contact, the block's
  // heading is the fallback e = (0, 0, 1), not zero, so the spin about x is the across-track term and pushes the
  // ball along z. The input is over cap (10 Hz against boundRollMax 7.5), and the push is bounded: its rate is
  // clamped to the cap, so the speed it adds is at most rollToSpeedMaxKph.
  const PesBallConstants c = SyntheticConstants();
  const float twoPi = 2.0f * std::acos(-1.0f);
  BallState s;
  s.position = pes::Vec3(0.0f, c.image.ballRadius + 0.001f, 0.0f);
  s.velocity = pes::Vec3(0.0f, -3.0f, 0.0f);
  s.spinRaw = pes::Vec3(twoPi * 10.0f / c.image.spinScale, 0.0f, 0.0f);
  ASSERT_TRUE(pesball::BallTick(c, &s, 0, pesball::kGfTick_s));
  const float vmax = c.record.At(BallRecordConstants::kRollToSpeedMaxKph, 0) * c.image.milli /
                     c.image.secondsPerHour;
  EXPECT_GT(s.velocity.z, pesball::StopSpeed_mps(c, 0)) << "topspin about +x pushes the ball toward +z";
  EXPECT_LE(s.velocity.z, vmax) << "the tangential push is bounded by the rolling speed cap";
  // By hand from the synthetic column 1 (the live mode, the default contactMode): RollRamp = 0.2 + 0.62 *
  // (10.8-3)/(25-3) = 0.4198, S3 = 10 - 10*0.4198 = 5.802, |A - S3| = 4.198 > 4 so blend = 0.5, A = 10 - 0.5*5.802
  // = 7.099, and the push is A*yRate*vmax/cap = 7.099*1*5.556/8.5 = 4.64 m/s (yRate is 1 at zero heading).
  EXPECT_NEAR(s.velocity.z, 4.64f, 0.02f) << "the fallback heading's across-track term";
  EXPECT_NEAR(s.velocity.x, 0.0f, 1e-6f);
}

TEST(PesBallContact, ASlowSpinBelowTwoRollingHertzAddsNoBounceRestitution) {
  // Report 3.3's s4 band: PES divides the horizontal spin by 2 pi before the band (0x140876cd4: x0.5, then
  // /pi), so the band is over Hz. 8 rad/s is 1.27 Hz, below the 2 Hz floor, so it adds nothing. Read as
  // rad/s it would sit inside the band and raise the restitution.
  const PesBallConstants c = SyntheticConstants();
  const float none = pesball::Bounce(c, 0, -3.0f, 0.0f, 0.0f).restitution;
  const float slow = pesball::Bounce(c, 0, -3.0f, 0.0f, 8.0f).restitution;
  EXPECT_NEAR(slow, none, 1e-6f);
}

TEST(PesBallContact, ASpinningBallWithNoVelocityFallingFromTheAirStaysFinite) {
  // With no velocity before the flight step, the block's heading is the fallback and the Y rate's slope has no
  // divisor (0x140876e1a..0x140876e37). The contact must stay finite; 0/0 would poison the whole match.
  const PesBallConstants c = SyntheticConstants();
  BallState s;
  s.position = pes::Vec3(0.0f, c.image.ballRadius + 0.001f, 0.0f);
  s.velocity = pes::Vec3(0.0f, 0.0f, 0.0f);
  s.spinRaw = pes::Vec3(0.0f, 0.0f, 0.02f);
  ASSERT_TRUE(pesball::BallTick(c, &s, 0, pesball::kGfTick_s));
  EXPECT_TRUE(std::isfinite(s.velocity.x) && std::isfinite(s.velocity.y) && std::isfinite(s.velocity.z));
}

}  // namespace
