#include <gtest/gtest.h>

#include "onthepitch/ball.hpp"
#include "onthepitch/pesball.hpp"
#include "onthepitch/pesballphysics.hpp"
#include "onthepitch/pesballtrack.hpp"
#include "pes_synthetic_ball.hpp"

// PES's ball prediction sampler (docs/re/pes-re-officials-ball.md 5.3): BallInfo
// holds 250 future records, one per PES tick. The pure rules are tested against
// synthetic constants; no install is needed.
namespace {

using pes::PesBallConstants;
using pes::Vec3;
using pesball::BallState;
using pesball::GoalFrame;
using pesball::TrackStart;

// A goal frame far from every test ball, so the frame response never fires.
GoalFrame FarGoal() { return GoalFrame{100.0f, 3.0f, 2.0f, 0.1f, 0.1f}; }

TrackStart StartAt(Vec3 position, Vec3 velocity) {
  TrackStart start;
  start.state.position = position;
  start.state.velocity = velocity;
  start.state.spinRaw = Vec3(0.0f, 0.0f, 0.0f);
  return start;
}

TEST(PesBallTrack, IndexMapsGfMillisecondsOntoPesTicksByFlooring) {
  EXPECT_EQ(pesball::TrackIndexForMs(0), 0u);
  EXPECT_EQ(pesball::TrackIndexForMs(18), 0u);     // 0.97 of a tick
  EXPECT_EQ(pesball::TrackIndexForMs(19), 1u);     // 1.03 ticks
  EXPECT_EQ(pesball::TrackIndexForMs(200), 10u);   // 10.8 ticks
  EXPECT_EQ(pesball::TrackIndexForMs(1000), 54u);  // one second is 54 ticks
}

TEST(PesBallTrack, TimesPastTheAiHorizonReadItsLastSlot) {
  // GF's AI horizon is 3 s, whose last 10 ms slot is 2.99 s: record 161. Times
  // past the horizon, 3 s and 4 s, read that same record.
  EXPECT_EQ(Ball::TrackIndexForAi(1000), 54u);
  EXPECT_EQ(Ball::TrackIndexForAi(2990), 161u);
  EXPECT_EQ(Ball::TrackIndexForAi(3000), 161u);
  EXPECT_EQ(Ball::TrackIndexForAi(4000), 161u);
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
  // Free fall from 20 m at rest. Record 27 is half a second on and record 54 is
  // one second on, so the fall is g*t^2/2 at those times. Drag at these speeds is
  // small. A track stepped on GF's 10 ms tick would put record 54 at 0.54 s, a
  // fall of 1.4 m instead of 4.9 m.
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
