#include "onthepitch/pesballtrack.hpp"

namespace pesball {
namespace {

float SpeedSquared(const pes::Vec3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

}  // namespace

// ponytail: GF's net stand-in (ApplyGoalNettingCollision, ball.cpp) runs on the live
// loop's first step only and is not replayed here, so the sampler sees the frame but
// not the net. Add the net to the sampler when PES's cloth-net response is ported.
Track BuildTrack(const pes::PesBallConstants& c, const TrackStart& start, const GoalFrame& goal) {
  Track track{};
  BallState s = start.state;
  unsigned contactTicks = start.contactTicks;
  float recentHit = start.recentHit;
  const float r = c.image.ballRadius;

  track[0] = s.position;
  for (unsigned k = 1; k < kTrackLength; ++k) {
    const pes::Vec3 stepStart = s.position;

    if (IsResting(c, s)) {
      // Report 2.1: a resting ball skips the flight function entirely.
      s.velocity = pes::Vec3(0.0f, 0.0f, 0.0f);
    } else if (s.position.y <= r) {
      // Report 2.3: the contact tick rewrites the velocity, and the position follows
      // on it, as PES's next flight step does.
      ContactTick(c, &s, contactTicks, true, false, kPesTick_s);
      ++contactTicks;
      s.position = s.position + s.velocity * kPesTick_s;
    } else {
      FlightTick(c, &s, kPesTick_s, false);
    }

    // Report 3.2: the frame response runs on every tick, so the sampler sees the
    // woodwork too.
    const float speedIn = SpeedSquared(s.velocity);
    if (const auto hit = GoalFrameHit(goal, r, stepStart, s.position, s.velocity)) {
      FrameContact(c, *hit, recentHit, &s);
    }
    recentHit = RecentHitAfter(c, recentHit, speedIn, SpeedSquared(s.velocity), kPesTick_s);

    track[k] = s.position;
  }
  return track;
}

}  // namespace pesball
