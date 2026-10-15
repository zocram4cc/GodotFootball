#ifndef _HPP_ONTHEPITCH_PESBALLTRACK
#define _HPP_ONTHEPITCH_PESBALLTRACK

#include <array>

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

// The record a time in GF milliseconds reads. PES's records fall on its 54 Hz
// ticks, so a time takes the last record at or before it.
inline unsigned TrackIndexForMs(unsigned ms) {
  return ms * static_cast<unsigned>(pes::kPesTickHz) / static_cast<unsigned>(pes::kMilliPerUnit);
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
