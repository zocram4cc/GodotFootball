#include "pesgoalkick.hpp"

#include <cmath>

#include "pesballtrack.hpp"

namespace pes {
namespace {

// Bisection steps: 2^-40 of the search range, well below the 0.25 m test tolerance.
constexpr int kBisectSteps = 40;

// The ball's flight in PES's space (Y up, metres): GF (x, y, z up) -> Fox (x, z, -y), the inverse
// of ball.cpp's Fox -> GF mapping.
Vec3 ToFox(const Vec3& gf) { return Vec3(gf.x, gf.z, -gf.y); }

}  // namespace

GoalKickCommand GoalKickAfterWait(bool receiverFound) {
  return receiverFound ? GoalKickCommand::kLoftedToReceiver : GoalKickCommand::kLoftedIntoSpace;
}

int PickGoalKickReceiver(const std::vector<float>& ratings, int takerIndex) {
  int best = -1;
  for (int i = 0; i < static_cast<int>(ratings.size()); ++i) {
    if (i == takerIndex) continue;
    if (best < 0 || ratings[i] > ratings[best]) best = i;
  }
  return best;
}

float PowerForLaunchSpeed(float speed_mps) {
  return speed_mps / kPassTouchSpeedPerPower_mps - kPassTouchPowerOffset;
}

float LoftedFlightDistance(const PesBallConstants& c, const Vec3& start, const Vec3& dir,
                           float speed_mps, float receiveHeight_m) {
  pesball::BallState s;
  s.position = ToFox(start);
  s.velocity = ToFox(dir * speed_mps);
  const Vec3 origin = s.position;
  const float radius_m = c.image.ballRadius;
  bool wasAbove = false;
  auto horizontal = [&]() { return std::hypot(s.position.x - origin.x, s.position.z - origin.z); };
  // The flight ends at PES's own horizon: one record per tick, 250 records (pesballtrack.hpp).
  for (unsigned tick = 1; tick < pesball::kTrackLength; ++tick) {
    pesball::BallTick(c, &s, 0, pesball::kPesTick_s);
    if (s.position.y > receiveHeight_m) wasAbove = true;
    if (wasAbove && s.velocity.y < 0.0f && s.position.y <= receiveHeight_m) return horizontal();
    if (s.position.y <= radius_m) return horizontal();
  }
  return horizontal();
}

float LoftedKickSpeed(const PesBallConstants& c, const Vec3& start, const Vec3& dir,
                      float distance_m, float receiveHeight_m) {
  if (LoftedFlightDistance(c, start, dir, kLoftedSearchTop_mps, receiveHeight_m) < distance_m)
    return kLoftedSearchTop_mps;
  float lo = 0.0f;
  float hi = kLoftedSearchTop_mps;
  for (int i = 0; i < kBisectSteps; ++i) {
    const float mid = 0.5f * (lo + hi);
    if (LoftedFlightDistance(c, start, dir, mid, receiveHeight_m) < distance_m) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return 0.5f * (lo + hi);
}

}  // namespace pes
