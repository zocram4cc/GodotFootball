#ifndef _HPP_ONTHEPITCH_PESBALLTRACK
#define _HPP_ONTHEPITCH_PESBALLTRACK

#include <array>
#include <cmath>

#include "onthepitch/pesball.hpp"
#include "onthepitch/pesballcollision.hpp"
#include "onthepitch/pesballphysics.hpp"

namespace pesball {

// PES's ball prediction sampler. BallInfo holds kTrackLength future records, one
// per PES tick (1/54 s): record 0 is the current tick and the array is rebuilt
// from the integrator (docs/re/pes-re-officials-ball.md 5.3). FUN_140a6cf70(p, n)
// returns record min(n, 249); FUN_140a57800 is record 0.
inline constexpr unsigned kTrackLength = 250;  // 0xfa, FUN_140a6cf70's bound

using Track = std::array<pes::Vec3, kTrackLength>;

// The record a PES reader gets for index n: n past the end reads the last record.
template <class T>
const T& TrackRecord(const std::array<T, kTrackLength>& track, unsigned n) {
  return track[n < kTrackLength ? n : kTrackLength - 1];
}

// The track at a GF time in milliseconds, linearly interpolated between the two PES records
// that bracket it. PES's readers take whole frames; GF's AI asks in milliseconds. A read that
// floors to a frame turns a 10 ms finite difference into zero for about half its calls, so the
// AI reads between the records instead.
// ponytail: linear interpolation between PES's own records stands in for a frame-exact read,
// which PES's AI never needs. The error is the curvature across one 18.5 ms record.
template <class V>
V TrackAt(const std::array<V, kTrackLength>& track, double ms) {
  const double x = ms * pes::kPesTickHz / pes::kMilliPerUnit;  // records since record 0
  const double base = std::floor(x);
  const unsigned n = base < 0.0 ? 0u : static_cast<unsigned>(base);
  const float f = static_cast<float>(x - base);
  return TrackRecord(track, n) * (1.0f - f) + TrackRecord(track, n + 1) * f;
}

// GF's AI horizon: a read past ballPredictionSize_ms - 10 takes the horizon's last sample, as
// the 10 ms grid always did.
inline double ClampToAiHorizon(double ms, double predictionSizeMs) {
  const double horizon = predictionSizeMs - 10.0;
  return ms < horizon ? ms : horizon;
}

struct TrackStart {
  BallState state;
  unsigned contactTicks = 0;  // the ball's own contact tick counter
  float recentHit = 0.0f;     // PES's recent-hit memory b
};

// FutureDataAll: the flight, contact and goal-frame steps the live ball takes, run
// from `start` on PES's own tick. Record k is the state after k ticks of 1/54 s.
Track BuildTrack(const pes::PesBallConstants& c, const TrackStart& start, const GoalFrame& goal);

}  // namespace pesball

#endif
