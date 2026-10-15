#include "onthepitch/pesmover.hpp"

#include <cmath>

namespace pesmover {
namespace {

constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kDegreesPerRadian = 180.0f / kPi;
constexpr float kHalfTurnDegrees = 180.0f;

// The player is floored when he stands still, or when he is a slow walker whose
// speed is below the floor. FUN_140a6c6d0 compares floor^2 > |v_xz|^2, strictly.
bool Floored(const MoverRecord& m, float minSpeed) {
  if (m.movementState == kMovementStill) return true;
  const float speedSq = m.velocity.x * m.velocity.x + m.velocity.z * m.velocity.z;
  return m.movementState == kMovementSlow && minSpeed * minSpeed > speedSq;
}

// FUN_140a8f7c0 over FUN_140a92c80: the facing direction (x = sin, z = cos) scaled
// by the speed. The scale is the caller's XMM2, which is the floor.
// ponytail: PES's sine is a 513-entry table in the exe (FUN_140a92c80); std::sin
// stands in for it.
pes::Vec3 FacingAtSpeed(float facingDeg, float speed) {
  const float rad = facingDeg * kPi / kHalfTurnDegrees;
  return pes::Vec3(std::sin(rad) * speed, 0.0f, std::cos(rad) * speed);
}

}  // namespace

pes::Vec3 FloorVelocity(const MoverRecord& m, float minSpeed) {
  if (Floored(m, minSpeed)) return FacingAtSpeed(m.facingDeg, minSpeed);
  return m.velocity;
}

pes::Vec3 MoverVelocity(const MoverRecord& m, float minSpeed) {
  if (Floored(m, minSpeed)) return pes::Vec3(0.0f, 0.0f, 0.0f);
  return m.velocity;
}

// ponytail: PES's arctangent is a table in the exe (FUN_140a92dc0 under FUN_140a92d40);
// std::atan2 stands in for it.
float FloorBearingDeg(const MoverRecord& m, float minSpeed) {
  const pes::Vec3 v = FloorVelocity(m, minSpeed);
  if (v.x == 0.0f && v.z == 0.0f) return 0.0f;
  float heading = std::atan2(v.x, v.z);
  if (heading < 0.0f) heading += kTwoPi;
  return heading * kDegreesPerRadian;
}

}  // namespace pesmover
