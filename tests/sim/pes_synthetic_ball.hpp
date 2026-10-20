#ifndef _HPP_TESTS_SIM_PES_SYNTHETIC_BALL
#define _HPP_TESTS_SIM_PES_SYNTHETIC_BALL

#include "onthepitch/pesball.hpp"

// The mode-1 column the reports were taken against, with the report's shipped
// values (docs/re/pes-re-ball-constants.md 6.2). Synthetic: a test never
// transcribes an install, it states the rule under test.
inline pes::PesBallConstants SyntheticConstants() {
  using pes::BallRecordConstants;
  pes::PesBallConstants c;
  c.image.gravity = 9.80665f;
  c.image.airDensity = 1.293f;
  c.image.ballArea = 0.037110474f;
  c.image.ballRadius = 0.108685933f;
  c.image.ballRadiusSquared = 0.0118126301f;
  c.image.dragCrisisSquared = 7.71604872f;
  c.image.half = -0.5f;
  c.image.pi = 3.14159274f;
  c.image.kmhPerMs = 3.5999999f;
  c.image.secondsPerHour = 3600.0f;
  c.image.milli = 1000.0f;
  c.image.tickHz = 54.0f;
  c.image.nonSpinTopMax = 23.6111107f;
  c.image.nonSpinTopPlateau = 48.2253113f;
  c.image.spinScale = 492.181f;
  c.image.invSpinScale = 1.0f / 492.181f;
  c.image.spinYDeltaScale = 0.01f;
  c.image.spinYBlendBase = 2.5f;

  BallRecordConstants& r = c.record;
  r.airRegistNormal = 1.0f;
  r.dragSpeedMax = 96.995003f;
  r.dragSpeedMin = 56.995602f;
  r.magnusRate = 0.035f;
  r.nonSpinMax = 105.0f;  // stock ball.o +0xAC (the 4cc mod ships 106.0)
  r.nonSpinMin = 45.0f;
  r.nonSpinRate = 1.0f;
  r.boundBoundSpeedMax = 12.0f;
  r.boundBoundSpeedMin = 2.0f;
  r.boundFrictionSpeedMax = 25.0f;
  r.boundFrictionSpeedMin = 10.0f;
  r.boundRollMax = 7.5f;
  r.boundRotationSpeedMax = 20.0f;
  r.boundRotationSpeedMin = 5.0f;
  r.grounderBoundYAdd = 0.09f;
  r.grounderCTan = 8.0f;
  r.grounderFrictionRate = 0.985f;
  r.grounderSpeed = 20.0f;
  r.natualBoundB = 0.4f;
  r.naturalRoll = 2.0f;
  r.rollToBoundAdd = 0.07f;
  r.rollToBoundRollMax = 7.0f;
  r.rollToBoundRollMin = 2.0f;
  r.rollToSpeedMaxRoll = 8.5f;
  r.rollToSpeedSpeedYMax = 0.5f;
  r.rollToSpeedSpeedYMin = 0.1f;
  r.stopSpeed = 0.175f;
  r.spinScaleSlot = 0;
  const auto set = [&](BallRecordConstants::ArrayRow row, float v0, float v1, float v2, float v3,
                       float v4, float v5) {
    r.arrays[row] = {v0, v1, v2, v3, v4, v5};
  };
  set(BallRecordConstants::kBoundCheckBoundAddRate, 10, 5, 0, 0, 5, 5);
  set(BallRecordConstants::kBoundCheckFrictionAddRate, 0, 0, 0, 0, 7.5f, 0);
  set(BallRecordConstants::kBoundRate, 0.7f, 0.7f, 0.7f, 0.5f, 0.7f, 0.7f);
  set(BallRecordConstants::kBoundToRotationAddRateXZ, 0.6f, 0.62f, 0.64f, 0.12f, 0.62f, 0.62f);
  set(BallRecordConstants::kBoundToRotationAddRateY, 0.07f, 0.07f, 0.07f, 0.07f, 0.07f, 0.03f);
  set(BallRecordConstants::kBoundToRotationBaseXZ, 0.16f, 0.2f, 0.24f, 0.13f, 0.2f, 0.2f);
  set(BallRecordConstants::kBoundToRotationBaseY, 0.03f, 0.03f, 0.03f, 0.03f, 0.03f, 0.02f);
  set(BallRecordConstants::kBoundToRotationSpeedMaxXZ, 25, 25, 25, 25, 0.7f, 25);
  set(BallRecordConstants::kBoundToRotationSpeedMaxY, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 30);
  set(BallRecordConstants::kBoundToRotationSpeedMinXZ, 3, 3, 3, 3, 0.1f, 3);
  set(BallRecordConstants::kBoundToRotationSpeedMinY, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 1);
  set(BallRecordConstants::kFrictionRollRateMax, 0.975f, 0.972f, 0.969f, 0.99f, 0.955f, 0.983f);
  set(BallRecordConstants::kFrictionRollRateMin, 0.985f, 0.982f, 0.979f, 0.98f, 0.987f, 0.983f);
  set(BallRecordConstants::kFrictionRollRateStop, 0.975f, 0.972f, 0.969f, 0.97f, 0.987f, 0.983f);
  set(BallRecordConstants::kFrictionRollSpeedMax, 25, 25, 25, 50, 8, 0);
  set(BallRecordConstants::kFrictionRollSpeedMin, 8, 8, 8, 8, 8, 15);
  set(BallRecordConstants::kFrictionRollSpeedStop, 0, 0, 0, 0, 8, 0);
  // gounnderprob: the grounder tick gate, an integer row (report 6.2's "u32
  // 5,5,5,9,9,5"). Mode 1's element is 5, so one contact tick in five fires the
  // driven grounder. The report's prose calls mode 1's value 0, which would be a
  // divide-by-zero; the shipped bytes are 5 and a port follows the bytes.
  r.rawArrays[BallRecordConstants::kGounnderprob] = {5, 5, 5, 9, 9, 5};
  set(BallRecordConstants::kNatualBoundA, 0.055f, 0.055f, 0.055f, 0.055f, 0.055f, 0.055f);
  set(BallRecordConstants::kNatunalRollBackSpinRate, 0.35f, 0.4f, 0.45f, 0.2f, 0.5f, 0.4f);
  set(BallRecordConstants::kOverRotMax, 1, 1, 1, 1, 1, 1);
  set(BallRecordConstants::kOverRotMin, -2, -2, -2, -2, -2, -2);
  set(BallRecordConstants::kRollToSpeedMaxKph, 20, 20, 20, 8.5f, 18, 16);
  set(BallRecordConstants::kRollToSpeedSpeedYRateBase, 0.8f, 0.8f, 0.8f, 0.4f, 0.2f, 0.8f);
  return c;
}

#endif
