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

// The record `records` further on than index n, again clamped: a finite difference
// taken over the track's own tick spacing.
template <class T>
const T& TrackRecord(const std::array<T, kTrackLength>& track, unsigned n, unsigned records) {
  return TrackRecord(track, n + records);
}

// The record index a GF time in milliseconds reads: floor(ms * 54 / 1000), clamped to
// the last record the way FUN_140a6cf70 clamps it.
inline unsigned TrackIndexForMs(double ms);

// GF's AI horizon: a read past ballPredictionSize_ms - 10 takes the horizon's last sample, as
// the 10 ms grid always did.
inline double ClampToAiHorizon(double ms, double predictionSizeMs);

// The track read at a GF time in milliseconds. The live ball now steps on PES's own
// 1/54 s tick (Milestone A1), so the ball is stair-stepped between records and a read
// floors to the record PES's own reader takes: FUN_140a6cf70(p, n) is record n, and a
// GF time maps onto n = floor(ms * 54 / 1000). The interpolated read that stood in for
// the floored one (faae222) is removed with the mismatch that needed it: with the ball
// on the tick, the record is the ball.
template <class V>
V TrackAt(const std::array<V, kTrackLength>& track, double ms) {
  return TrackRecord(track, TrackIndexForMs(ms));
}

inline unsigned TrackIndexForMs(double ms) {
  const double records = ms * pes::kPesTickHz / pes::kMilliPerUnit;
  const double floored = std::floor(records);
  const double base = floored < 0.0 ? 0.0 : floored;
  return static_cast<unsigned>(base);
}

// GF's AI horizon: a read past ballPredictionSize_ms - 10 takes the horizon's last sample, as
// the 10 ms grid always did.
inline double ClampToAiHorizon(double ms, double predictionSizeMs) {
  const double horizon = predictionSizeMs - 10.0;
  return ms < horizon ? ms : horizon;
}

// The record an AI read of a GF time in milliseconds takes, with GF's 3 s AI horizon.
// A read past ballPredictionSize_ms - 10 takes the horizon's last sample, as the
// 10 ms grid always did.
inline unsigned TrackIndexForAiMs(double ms, double predictionSizeMs) {
  return TrackIndexForMs(ClampToAiHorizon(ms, predictionSizeMs));
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
