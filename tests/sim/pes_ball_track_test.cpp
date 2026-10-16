#include <gtest/gtest.h>

#include <array>

#include "onthepitch/pesball.hpp"
#include "onthepitch/pesballphysics.hpp"
#include "onthepitch/pesballtrack.hpp"
#include "pes_synthetic_ball.hpp"

// PES's ball prediction sampler (docs/re/pes-re-officials-ball.md 5.3): BallInfo holds 250
// future records, one per PES tick. The pure rules are tested against synthetic constants;
// no install is needed.
namespace {

using pes::PesBallConstants;
using pes::Vec3;
using pesball::BallState;
using pesball::GoalFrame;
using pesball::TrackStart;

constexpr double kPredictionSizeMs = 3000.0;  // ballPredictionSize_ms

// A goal frame far from every test ball, so the frame response never fires.
GoalFrame FarGoal() { return GoalFrame{100.0f, 3.0f, 2.0f, 0.1f, 0.1f}; }

TrackStart StartAt(Vec3 position, Vec3 velocity) {
  TrackStart start;
  start.state.position = position;
  start.state.velocity = velocity;
  start.state.spinRaw = Vec3(0.0f, 0.0f, 0.0f);
  return start;
}

// Record k sits at x = perRecord * k, so a read's value says where along the track it is.
std::array<Vec3, pesball::kTrackLength> RampTrack(float perRecord) {
  std::array<Vec3, pesball::kTrackLength> track{};
  for (unsigned k = 0; k < pesball::kTrackLength; ++k) track[k] = Vec3(perRecord * k, 0.0f, 0.0f);
  return track;
}

// The GF time (ms) of PES record `records`, which is records * 1/54 s.
double RecordMs(double records) { return records * 1000.0 / 54.0; }

TEST(PesBallTrack, ReadsAtAPesTickReturnThatRecord) {
  const auto track = RampTrack(1.0f);
  for (unsigned k : {0u, 1u, 27u, 54u, 161u}) {
    EXPECT_NEAR(pesball::TrackAt(track, RecordMs(k)).x, static_cast<float>(k), 1e-3f)
        << "record " << k;
  }
}

TEST(PesBallTrack, ReadsBetweenTwoRecordsFloorToTheEarlierRecord) {
  // Milestone A1: the live ball steps on PES's 1/54 s tick, so the records are the
  // ball's own states and a read between them floors, as FUN_140a6cf70 does. The
  // interpolated read faae222 stood in for the floored one while the ball ran on
  // GF's 10 ms integrator; the mismatch that needed it is gone.
  const auto track = RampTrack(1.0f);
  EXPECT_NEAR(pesball::TrackAt(track, RecordMs(10.5)).x, 10.0f, 1e-3f);
  EXPECT_NEAR(pesball::TrackAt(track, RecordMs(0.25)).x, 0.0f, 1e-3f);
  EXPECT_NEAR(pesball::TrackAt(track, RecordMs(0.99)).x, 0.0f, 1e-3f);
  EXPECT_NEAR(pesball::TrackAt(track, RecordMs(1.0)).x, 1.0f, 1e-3f);
}

TEST(PesBallTrack, OneRecordDifferenceOfAMovingBallIsNeverZero) {
  // A ball at 20 m/s moves 20/54 m per record, so a difference over one record is
  // that distance at every read. A 10 ms difference of the floored records is zero
  // for about half of them, which is what the floored read gave the intercept
  // optimiser (52% zero); with the difference taken over one record instead, it is
  // never zero.
  const auto track = RampTrack(20.0f / 54.0f);
  for (unsigned k = 0; k + 1 < 161u; ++k) {
    const unsigned now = pesball::TrackIndexForAiMs(RecordMs(k), kPredictionSizeMs);
    const float d = pesball::TrackRecord(track, now, 1).x - pesball::TrackRecord(track, now).x;
    EXPECT_NEAR(d, 20.0f / 54.0f, 1e-4f) << "at record " << k;
  }
}

TEST(PesBallTrack, AiHorizonClampsTo2990Milliseconds) {
  EXPECT_DOUBLE_EQ(pesball::ClampToAiHorizon(4000.0, kPredictionSizeMs), 2990.0);
  EXPECT_DOUBLE_EQ(pesball::ClampToAiHorizon(3000.0, kPredictionSizeMs), 2990.0);
  EXPECT_DOUBLE_EQ(pesball::ClampToAiHorizon(2980.0, kPredictionSizeMs), 2980.0);
  EXPECT_DOUBLE_EQ(pesball::ClampToAiHorizon(0.0, kPredictionSizeMs), 0.0);
  // 2990 ms is record 161.46: the last read floors to record 161, like any other.
  EXPECT_NEAR(pesball::TrackAt(RampTrack(1.0f), 2990.0).x, 161.0f, 1e-3f);
}

TEST(PesBallTrack, RecordReadClampsToTheLastRecordAsFun140a6cf70Does) {
  std::array<float, pesball::kTrackLength> track{};
  for (unsigned k = 0; k < pesball::kTrackLength; ++k) track[k] = static_cast<float>(k);
  EXPECT_FLOAT_EQ(pesball::TrackRecord(track, 0), 0.0f);
  EXPECT_FLOAT_EQ(pesball::TrackRecord(track, 249), 249.0f);
  // FUN_140a6cf70 returns p + 0x2770 for n >= 250, which is record 249.
  EXPECT_FLOAT_EQ(pesball::TrackRecord(track, 250), 249.0f);
  EXPECT_FLOAT_EQ(pesball::TrackRecord(track, 100000), 249.0f);
}

TEST(PesBallTrack, ALyingBallHoldsEveryRecordAtItsPosition) {
  const PesBallConstants c = SyntheticConstants();
  const TrackStart start = StartAt(Vec3(1.0f, c.image.ballRadius, 2.0f), Vec3(0.0f, 0.0f, 0.0f));
  const pesball::Track track = pesball::BuildTrack(c, start, FarGoal());
  for (const Vec3& p : track) {
    EXPECT_FLOAT_EQ(p.x, 1.0f);
    EXPECT_FLOAT_EQ(p.y, c.image.ballRadius);
    EXPECT_FLOAT_EQ(p.z, 2.0f);
  }
}

TEST(PesBallTrack, RecordsAreSpacedOnPesTicksOfOneFiftyFourthOfASecond) {
  // Free fall from 20 m at rest. Record 27 is half a second on and record 54 is one second on,
  // so the fall is g*t^2/2 at those times. Drag at these speeds is small. A track stepped on
  // GF's 10 ms tick would put record 54 at 0.54 s, a fall of 1.4 m instead of 4.9 m.
  const PesBallConstants c = SyntheticConstants();
  const float g = c.image.gravity;
  const TrackStart start = StartAt(Vec3(0.0f, 20.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f));
  const pesball::Track track = pesball::BuildTrack(c, start, FarGoal());
  EXPECT_NEAR(20.0f - track[27].y, 0.5f * g * (27.0f / 54.0f) * (27.0f / 54.0f), 0.1f);
  EXPECT_NEAR(20.0f - track[54].y, 0.5f * g, 0.5f);
}

TEST(PesBallTrack, RecordZeroIsTheCurrentStateBeforeAnyStep) {
  const PesBallConstants c = SyntheticConstants();
  const TrackStart start = StartAt(Vec3(3.0f, 5.0f, -4.0f), Vec3(2.0f, 0.0f, 1.0f));
  const pesball::Track track = pesball::BuildTrack(c, start, FarGoal());
  EXPECT_FLOAT_EQ(track[0].x, 3.0f);
  EXPECT_FLOAT_EQ(track[0].y, 5.0f);
  EXPECT_FLOAT_EQ(track[0].z, -4.0f);
}

}  // namespace
