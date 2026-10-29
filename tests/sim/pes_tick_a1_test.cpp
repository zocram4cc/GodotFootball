#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/AIsupport/mentalimage.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesball.hpp"
#include "onthepitch/pesballphysics.hpp"
#include "onthepitch/pesballtrack.hpp"
#include "onthepitch/pescarrier.hpp"
#include "onthepitch/pesscorerows.hpp"
#include "onthepitch/player/humanoid/humanoid_utils.hpp"
#include "onthepitch/team.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "pes_synthetic_ball.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

// Milestone A1: the sim's logic frame is PES's 1/54 s, the live ball integrates on
// PES's tick, and the track is read with floor reads. The four 10 ms finite
// differences become record differences multipled by PES's tick rate.
namespace {

using pes::Vec3;
using pesball::BallState;
using pesball::GoalFrame;
using pesball::TrackStart;

constexpr double kPredictionSizeMs = 3000.0;  // ballPredictionSize_ms

GoalFrame FarGoal() { return GoalFrame{100.0f, 3.0f, 2.0f, 0.1f, 0.1f}; }

TrackStart StartAt(Vec3 position, Vec3 velocity) {
  TrackStart start;
  start.state.position = position;
  start.state.velocity = velocity;
  start.state.spinRaw = Vec3(0.0f, 0.0f, 0.0f);
  return start;
}

// Record k sits at x = perRecord * k, so a read's value says which record it landed on.
std::array<Vec3, pesball::kTrackLength> RampTrack(float perRecord) {
  std::array<Vec3, pesball::kTrackLength> track{};
  for (unsigned k = 0; k < pesball::kTrackLength; ++k) track[k] = Vec3(perRecord * k, 0.0f, 0.0f);
  return track;
}

// The GF time (ms) of PES record `records`: records * 1/54 s.
double RecordMs(double records) { return records * 1000.0 / 54.0; }

// --- a whole match, for the parts only the live objects can show ------------
// MatchData reads the fixture database; the player skeleton comes from the user's
// install.

constexpr int kHomeTeam = 1, kAwayTeam = 2;
constexpr unsigned int kSeed = 1000;

// Loaded once and kept: the database outlives the test that installed it, so a
// later test cannot find a dangling global.
blunted::Database* SharedDb() {
  static blunted::Database db;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    if (!db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return nullptr;
  }
  return &db;
}

const pesrig::Rig* SharedRig() {
  static std::optional<pesrig::Rig> rig;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return nullptr;
    pes::PesVfs vfs;
    std::string err;
    if (vfs.Mount(root, &err)) rig = pesrig::BuildRig(vfs, &err);
  }
  return rig ? &*rig : nullptr;
}

std::unique_ptr<Match> MakeMatch() {
  MatchSetup setup;
  setup.homeTeamDbId = kHomeTeam;
  setup.awayTeamDbId = kAwayTeam;
  setup.seed = kSeed;
  setup.durationMinutes = 5;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  if (!pestest::FillMissingPesConstants(&setup)) return nullptr;
  return std::unique_ptr<Match>(
      new Match(new MatchData(kHomeTeam, kAwayTeam), setup, Properties()));
}

// Builds the match the three install-backed tests below run on.
#define REQUIRE_PES_MATCH()                                                              \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  blunted::Database* db = SharedDb();                                                    \
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";                                        \
  SetDB(db);                                                                             \
  std::unique_ptr<Match> match = MakeMatch();                                             \
  if (!match) GTEST_SKIP() << "PES2021.exe: the match constants are unavailable"


}  // namespace

// --- the tick itself -------------------------------------------------------

TEST(PesA1Tick, TheSimTickIsPesOneFiftyFourthOfASecond) {
  // A1: one logic frame is one PES tick, not GF's 10 ms. The rate PES runs at is
  // the install's own 54 (pesball.hpp: read from .rdata 0x14259d624).
  const double tick_s = 1.0 / pes::kPesTickHz;
  EXPECT_NEAR(tick_s, 0.0185185, 1e-7);
  EXPECT_NEAR(pesball::kGfTickHz, pes::kPesTickHz, 1e-9);
}

TEST(PesA1Tick, TheTrackIsReadByFloorNotByInterpolation) {
  // A stair-stepped ball has no value between the records: a read at a
  // between-tick time takes the record the PES reader would take.
  const auto track = RampTrack(1.0f);
  EXPECT_NEAR(pesball::TrackAt(track, RecordMs(3.7)).x, 3.0f, 1e-3f);
  EXPECT_NEAR(pesball::TrackAt(track, RecordMs(10.9)).x, 10.0f, 1e-3f);
  EXPECT_NEAR(pesball::TrackAt(track, RecordMs(161.4)).x, 161.0f, 1e-3f);
}

// --- the four finite differences -------------------------------------------
// Each site asked for the ball's movement as a difference of two grid samples
// 10 ms apart, scaled by 100 (per 10 ms -> per second). With one record per PES
// tick the 10 ms spacing spans 0 or 1 records, so the difference is taken over
// one record and scaled by the tick rate instead.

// The velocity a difference of two track records at a 1-record spacing gives:
// metres per PES tick * 54 ticks per second = m/s.
TEST(PesA1FiniteDifference, RecordDifferenceTimesTheTickRateIsMetresPerSecond) {
  const auto track = RampTrack(0.2f);  // 0.2 m per record
  const double v = (pesball::TrackAt(track, RecordMs(4)).x - pesball::TrackAt(track, RecordMs(3)).x) *
                   pes::kPesTickHz;
  EXPECT_NEAR(v, 10.8, 1e-4);  // 0.2 m / (1/54 s)
}

TEST(PesA1FiniteDifference, OneRecordSpacingNeverYieldsZeroMovement) {
  // A ball at 20 m/s covers 20/54 m per record. The AI's finite difference is now
  // taken over one record (GetBallPredictionAtRecord), so it is that distance at
  // every read, never zero - which is what the 10 ms grid read gave for 52% of
  // intercept calls under the floored read.
  const auto track = RampTrack(20.0f / 54.0f);
  for (unsigned k = 0; k + 1 < 161u; ++k) {
    const unsigned now = pesball::TrackIndexForAiMs(RecordMs(k), kPredictionSizeMs);
    const float d = pesball::TrackRecord(track, now, 1).x - pesball::TrackRecord(track, now).x;
    EXPECT_NEAR(d, 20.0f / 54.0f, 1e-4f) << "at record " << k;
  }
}

// The AIfunctions site reads the ball 10 ms further on than the AI's own time.
// With records 1/54 s apart, that further-on read is one record ahead.
TEST(PesA1FiniteDifference, TenMillisecondsFurtherIsAtMostOnePesRecordAhead) {
  // 10 ms is 0.54 of a record, so a 10 ms further-on read lands on the same record
  // 46% of the time and the next one 54% of the time: never more than one record
  // ahead. (The AI takes its differences over a whole record instead - that is what
  // makes a 10 ms difference of floor reads zero for half its calls.)
  unsigned moved = 0;
  for (double ms = 0.0; ms < RecordMs(160.0); ms += 10.0) {
    const unsigned later = pesball::TrackIndexForAiMs(ms + 10.0, kPredictionSizeMs);
    const unsigned now = pesball::TrackIndexForAiMs(ms, kPredictionSizeMs);
    EXPECT_LE(later - now, 1u) << "at " << ms << " ms";
    moved += later != now ? 1u : 0u;
  }
  EXPECT_GT(moved, 0u) << "10 ms must cross a record sometimes";
  EXPECT_LT(moved, 161u) << "and it must not cross one every time";
  // A second of milliseconds is 54 records, not the 100 of the 10 ms grid the sim
  // used to run on: the reader is PES's.
  const unsigned second = pesball::TrackIndexForAiMs(1000.0, kPredictionSizeMs) -
                          pesball::TrackIndexForAiMs(0.0, kPredictionSizeMs);
  EXPECT_EQ(second, 54u);
}

TEST(PesA1FiniteDifference, AiHorizonStillClampsTo2990Milliseconds) {
  EXPECT_DOUBLE_EQ(pesball::ClampToAiHorizon(4000.0, kPredictionSizeMs), 2990.0);
  EXPECT_DOUBLE_EQ(pesball::ClampToAiHorizon(0.0, kPredictionSizeMs), 0.0);
}

// --- the live ball on PES's tick -------------------------------------------

// The track the live ball writes: one record per PES tick from the live state,
// so record 1 k is where the ball is after k ticks of 1/54 s.
TEST(PesA1LiveBall, TrackRecordsAreSpacedOnPesTicks) {
  const pes::PesBallConstants c = SyntheticConstants();
  const float g = c.image.gravity;
  // Free fall from 20 m at rest: record 27 is half a second on.
  const TrackStart start = StartAt(Vec3(0.0f, 20.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f));
  const pesball::Track track = pesball::BuildTrack(c, start, FarGoal());
  EXPECT_NEAR(20.0f - track[27].y, 0.5f * g * 0.5f * 0.5f, 0.1f);
  EXPECT_NEAR(20.0f - track[54].y, 0.5f * g, 0.5f);
}

TEST(PesA1LiveBall, RestingBallStaysPutOnEveryRecord) {
  const pes::PesBallConstants c = SyntheticConstants();
  const TrackStart start = StartAt(Vec3(1.0f, c.image.ballRadius, 2.0f), Vec3(0.0f, 0.0f, 0.0f));
  const pesball::Track track = pesball::BuildTrack(c, start, FarGoal());
  for (unsigned k = 0; k < 10; ++k) {
    EXPECT_NEAR(track[k].x, 1.0f, 1e-6f);
    EXPECT_NEAR(track[k].y, c.image.ballRadius, 1e-6f);
    EXPECT_NEAR(track[k].z, 2.0f, 1e-6f);
  }
}

// A track read one second out against the live ball's own records is the MBF
// diagnostic's comparison: the live ball is the track, so the read is exact.
TEST(PesA1LiveBall, ReadingTheLiveBallsOwnTrackIsExactAtEveryHorizon) {
  const pes::PesBallConstants c = SyntheticConstants();
  const TrackStart start = StartAt(Vec3(0.0f, 1.0f, 0.0f), Vec3(12.0f, 3.0f, -6.0f));
  const pesball::Track track = pesball::BuildTrack(c, start, FarGoal());
  for (unsigned ms : {100u, 300u, 1000u, 2000u}) {
    const Vec3 at1s = pesball::TrackAt(track, static_cast<double>(ms));
    // Floor to the same record the live ball would hold at that time.
    const unsigned k = static_cast<unsigned>(ms * pes::kPesTickHz / 1000.0);
    const Vec3 atRecord = track[k];
    const float dx = at1s.x - atRecord.x, dy = at1s.y - atRecord.y, dz = at1s.z - atRecord.z;
    EXPECT_LT(std::sqrt(dx * dx + dy * dy + dz * dz), 0.02f) << "at " << ms << " ms";
  }
}

// --- the tick's rate, its frame-second and its frame-counted work ----------

TEST(PesA1Tick, TheTickRateIsTheFrameCountNotTheMillisecondLength) {
  // kTick_ms is 1000 * PES's 1/54 s carried as a float, so it is 18.5185187 ms -
  // a hair over 1000/54, which makes 1000 / kTick_ms truncate to 53. A caller that
  // derives the ticks in a second from the millisecond length is a frame short
  // every second; the rate is PES's own frame count.
  EXPECT_NEAR(Match::kTick_ms, 1000.0 / 54.0, 1e-6);
  EXPECT_EQ(static_cast<int>(1000.0 / Match::kTick_ms), 53);
  EXPECT_EQ(Match::kFramesPerSecond, 54ul);
  EXPECT_EQ(Match::kFramesPerSecond, static_cast<unsigned long>(pes::kPesTickHz));
}

TEST(PesA1Requeue, TheRequeueCadenceIsEveryNFramesPhasedBySide) {
  // Humanoid::Process's five requeue sites as (every, phase) in logic frames: the
  // old (t + id * 10) % 20 and % 30, (t + id * 20) % 40 and % 50, (t + id * 40) % 80
  // on the 10 ms tick, read as frame counts now.
  const unsigned long kEvery[] = {2, 3, 4, 5, 8};
  const unsigned long kPhase[] = {1, 1, 2, 2, 4};
  for (int i = 0; i < 5; ++i) {
    const unsigned long every = kEvery[i], phase = kPhase[i];
    // Each side requeues exactly once in every `every` frames...
    unsigned long team0 = 0, team1 = 0;
    for (unsigned long frame = 0; frame < every; ++frame) {
      team0 += RequeueOnFrame(frame, 0, every, phase) ? 1ul : 0ul;
      team1 += RequeueOnFrame(frame, 1, every, phase) ? 1ul : 0ul;
    }
    EXPECT_EQ(team0, 1ul) << "team 0, every " << every;
    EXPECT_EQ(team1, 1ul) << "team 1, every " << every;
    // ... and the sides are out of phase: team 0 requeues on frame 0 and team 1 does
    // not, so the two squads' requeues never land on the same frame.
    EXPECT_TRUE(RequeueOnFrame(0, 0, every, phase)) << "team 0, every " << every;
    EXPECT_FALSE(RequeueOnFrame(0, 1, every, phase)) << "team 1, every " << every;
  }
}

TEST(PesA1MatchData, TheClocksAdvanceByTheSimTick) {
  // AddPossessionTime advanced its clocks by a hardcoded 10 ms; they advance by the
  // tick (Milestone A1), so a second of ticks is a second on the possession clock.
  blunted::Database* db = SharedDb();
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";
  SetDB(db);
  MatchData data(1, 2);
  data.AddPossessionTime(0, Match::kTick_ms);
  EXPECT_EQ(data.GetPossessionTime_ms(0), 18ul) << "the tick, truncated";
  EXPECT_EQ(data.GetPossessionTime_ms(1), 0ul) << "only the given side's clock runs";
  for (int i = 1; i < 54; ++i) data.AddPossessionTime(0, Match::kTick_ms);
  EXPECT_NEAR(static_cast<double>(data.GetPossessionTime_ms(0)), 1000.0, 1.0);
}

// --- the sim's clock, live ball and AI reads on a real match ---------------

TEST(PesA1Sim, EveryGateCountsPesLogicFrames) {
  REQUIRE_PES_MATCH();
  EXPECT_EQ(match->GetSimFrame(), 0ul);
  EXPECT_TRUE(match->IsFrameSecond()) << "frame 0 opens a frame-second, as t = 0 did";
  for (unsigned long frame = 1; frame <= 3 * Match::kFramesPerSecond; ++frame) {
    match->Process();
    ASSERT_EQ(match->GetSimFrame(), frame) << "one Process call is one logic frame";
    EXPECT_EQ(match->IsFrameSecond(), frame % Match::kFramesPerSecond == 0) << "at " << frame;
    EXPECT_NEAR(match->GetSimTime_ms(), static_cast<double>(frame) * Match::kTick_ms, 1e-6);
  }
  // 3 * 54 logic frames are three seconds of sim time - on the frame clock and on
  // the millisecond clock the match exposes to everything else.
  EXPECT_NEAR(match->GetSimTime_ms(), 3000.0, 1e-3);
  EXPECT_EQ(match->GetActualTime_ms(), 3000ul) << "162 ticks, truncated";
}

TEST(PesA1Sim, RandomInfoIsRepublishedOnEveryLogicFrame) {
  // report 3.15: the callback 0x14041e2b0 runs the producer's update whenever the producer
  // exists, so the record the carrier copies changes from one logic frame to the next. A match
  // that published once would hand the carrier the same value for the whole match.
  REQUIRE_PES_MATCH();
  match->Process();
  const float first = match->GetRandomInfo().unit;
  match->Process();
  const float second = match->GetRandomInfo().unit;
  EXPECT_NE(first, second) << "two frames, two records";
  EXPECT_GE(second, 0.0f);
  EXPECT_LT(second, 1.0f) << "a RangeRaw over 999999, divided by 999999";
}

TEST(PesA1Sim, TheBallIsTheSecondRecordOfTheTrackItWrote) {
  REQUIRE_PES_MATCH();
  Ball* ball = match->GetBall();
  ASSERT_TRUE(ball);
  const Vector3 start(0.0f, 0.11f, 0.0f);
  ball->SetPosition(start);
  ball->SetMomentum(Vector3(8.0f, 0.0f, 3.0f));
  EXPECT_NEAR((ball->GetTrack()[0] - start).GetLength(), 0.0f, 1e-4f)
      << "record 0 is where the ball stands";
  const Vector3 step = ball->GetTrack()[1] - ball->GetTrack()[0];
  EXPECT_GT(step.GetLength(), 0.05f) << "one PES tick of movement at 8.5 m/s";

  ball->Process();
  EXPECT_NEAR((ball->GetTrack()[0] - start).GetLength(), 0.0f, 1e-4f);
  EXPECT_NEAR((ball->GetPositionBuffer() - ball->GetTrack()[1]).GetLength(), 0.0f, 1e-4f)
      << "the ball's position is record 1 of the track it just wrote";
  EXPECT_NEAR((ball->GetPositionBuffer() - (start + step)).GetLength(), 0.0f, 1e-3f)
      << "that record is one tick of the ball's own step on, not a 10 ms grid read";
}
// The world window's ball record is PES's layout and the ball's own writers fill it (pesworldrecord.hpp): the prediction table
// at B+0x88 + 0x28*k holds the track in PES's axes (x, height, z), and B+0x2948 the velocity. PES's sampler rebuilds the table
// every tick (docs/re/pes-re-officials-ball.md 5.3).
TEST(PesA1Sim, TheBallsTrackAndVelocityAreTheWindowsBallRecord) {
  REQUIRE_PES_MATCH();
  Ball* ball = match->GetBall();
  ASSERT_TRUE(ball);
  ball->SetPosition(Vector3(3.0f, 0.5f, 0.25f));  // GF: x, y = PES z, z = the height
  {
    // Sample 0 is where the ball stands, in PES's axes: x 3, height 0.25, z across 0.5 (read raw, not through the ball).
    const uint8_t* sample0 = match->PesWorldOf().BallRecord() + pes::kBallSamplesOffset;
    EXPECT_FLOAT_EQ(pes::LoadF32(sample0, pes::kBallSampleXOffset), 3.0f);
    EXPECT_FLOAT_EQ(pes::LoadF32(sample0, pes::kBallSampleYOffset), 0.25f) << "PES's y is the height";
    EXPECT_FLOAT_EQ(pes::LoadF32(sample0, pes::kBallSampleZOffset), 0.5f);
  }
  ball->SetMomentum(Vector3(8.0f, 2.0f, 3.0f));
  ball->Process();
  ball->Process();
  const uint8_t* record = match->PesWorldOf().BallRecord();
  const auto trackAt = [&](unsigned k) {
    const uint8_t* sample = record + pes::kBallSamplesOffset + k * pes::kBallSampleStride;
    return Vector3(pes::LoadF32(sample, pes::kBallSampleXOffset), pes::LoadF32(sample, pes::kBallSampleZOffset),
                   pes::LoadF32(sample, pes::kBallSampleYOffset));
  };
  for (const unsigned k : {0u, 1u, 120u, 249u})
    EXPECT_NEAR((trackAt(k) - ball->TrackSample(k)).GetLength(), 0.0f, 1e-6f) << "sample " << k;
  EXPECT_GT((trackAt(249) - trackAt(0)).GetLength(), 1.0f) << "the table is the flight, not a repeated position";
  const Vector3 v = ball->GetMovement();
  EXPECT_FLOAT_EQ(pes::LoadF32(record, pes::kBallVelocityOffset), v.coords[0]);
  EXPECT_FLOAT_EQ(pes::LoadF32(record, pes::kBallVelocityOffset + 4), v.coords[2]) << "PES's y is the height";
  EXPECT_FLOAT_EQ(pes::LoadF32(record, pes::kBallVelocityOffset + 8), v.coords[1]);
}

// The window's B+0x2948 is the ball's only velocity: a write to the record is the ball's movement at once, and a
// SetMomentum is in the record at once (no second store that a later Process catches up).
TEST(PesA1Sim, TheBallsVelocityHasNoStoreBesideTheWindow) {
  REQUIRE_PES_MATCH();
  Ball* ball = match->GetBall();
  ASSERT_TRUE(ball);
  match->PesWorldOf().SetBallVelocity(4.0f, 1.5f, -2.0f);  // PES: x, height, z
  const Vector3 v = ball->GetMovement();
  EXPECT_FLOAT_EQ(v.coords[0], 4.0f);
  EXPECT_FLOAT_EQ(v.coords[1], -2.0f) << "GF's y is PES's z";
  EXPECT_FLOAT_EQ(v.coords[2], 1.5f) << "GF's z is the height";
  ball->SetMomentum(Vector3(7.0f, 2.0f, 3.0f));
  const uint8_t* record = match->PesWorldOf().BallRecord();
  EXPECT_FLOAT_EQ(pes::LoadF32(record, pes::kBallVelocityOffset), 7.0f);
  EXPECT_FLOAT_EQ(pes::LoadF32(record, pes::kBallVelocityOffset + 4), 3.0f);
  EXPECT_FLOAT_EQ(pes::LoadF32(record, pes::kBallVelocityOffset + 8), 2.0f);
}

// The event store E is the window's event region at PES's offsets: a routed kind-7 record is in ring 0's slot, its count word
// is at E+0x1900, and its payload is the newest-event copy at E+0x180698 (FUN_140A3E190(E, 0)).
TEST(PesA1Sim, TheEventStoreLivesInTheWindowAtPesOffsets) {
  REQUIRE_PES_MATCH();
  pes::PesWorld& world = match->PesWorldOf();
  pes::EventStore store = world.Events();
  const pes::EventRingTable table{};  // every kind routes to ring 0
  pes::EventRecord rec;
  rec.Write(7, 100, 100, 0.0f, 1);
  rec.SetActionKind(5);
  rec.SetU32(0x18, 1);
  rec.SetU32(0x1c, 4);
  store.Route(table, rec);
  const uint8_t* e = world.Bytes() + (pes::PesWorld::EventsVa() - pes::kWorldWindowVa);
  const uint32_t head = store.Ring(0).head;
  EXPECT_EQ(pes::LoadU32(e, head * pes::EventRecord::kSize), 7u) << "ring 0's slot holds the record (its route kind word)";
  EXPECT_GE(pes::LoadU32(e, 0x1900), 1u) << "ring 0's count word is at E+0x1900";
  EXPECT_EQ(pes::LoadU32(e, pes::kEventNewestOffset), 5u) << "payload+0 of the newest event is at E+0x180698";
  EXPECT_EQ(pes::LoadU32(e, pes::kEventNewestOffset + 4), 1u);
  EXPECT_EQ(match->GetPesEvents().NewestOrRing0(0)->ActionKind(), 5u) << "the match reads the same bytes";
}

// Each side's AI record MA is written every tick: the ball copy is the ball's record 0, and the lane lists are the score
// rows' entries (slot id u32, value f32, a team's 11 entries at +0x58 per team).
TEST(PesA1Sim, TheTeamAisRecordMaIsWrittenEachTickFromTheBallAndTheRows) {
  REQUIRE_PES_MATCH();
  match->StartPlay();
  for (int i = 0; i < 3; ++i) match->Process();
  pes::PesWorld& world = match->PesWorldOf();
  float ball[3];
  world.BallSample(0, ball);
  int checked = 0;
  for (int side = 0; side < 2; ++side) {
    const uint8_t* ma = world.Ma(side);
    EXPECT_FLOAT_EQ(pes::LoadF32(ma, pes::kMaBallOffset), ball[0]);
    EXPECT_FLOAT_EQ(pes::LoadF32(ma, pes::kMaBallOffset + 4), ball[1]);
    EXPECT_FLOAT_EQ(pes::LoadF32(ma, pes::kMaBallOffset + 8), ball[2]);
    const pes::ScoreRows& rows = match->GetTeam(side)->GetController()->Rows();
    for (int list = 0; list < 2; ++list) {
      for (int team = 0; team < 2; ++team) {
        const uint8_t* block = ma + (list == 0 ? pes::kMaLaneListAOffset : pes::kMaLaneListBOffset) +
                               static_cast<std::size_t>(team) * pes::kMaListTeamStride;
        for (int rank = 0; rank < rows.list[list].Count(team); ++rank) {
          EXPECT_EQ(pes::LoadU32(block, rank * 8), static_cast<uint32_t>(rows.list[list].Entry(team, rank).id));
          EXPECT_FLOAT_EQ(pes::LoadF32(block, rank * 8 + 4), rows.list[list].Entry(team, rank).value);
          ++checked;
        }
      }
    }
  }
  EXPECT_GT(checked, 0) << "the rows list somebody";
}

// The Clear unit 0x142109EB0 runs on the window: nothing is stored while the danger byte (imageA+0x31C1) is clear; with it
// set the unit stores its answer type 5 and a corridor angle over the window's records.
TEST(PesA1Sim, TheClearUnitRunsOnTheWindowAndAnswersOnlyOnTheDangerByte) {
  REQUIRE_PES_MATCH();
  match->StartPlay();
  for (int i = 0; i < 3; ++i) match->Process();
  pes::PesWorld& world = match->PesWorldOf();
  world.SetClearDanger(0, false);
  EXPECT_EQ(world.RunClearUnit(0, 3).type, 0u);
  world.SetClearDanger(0, true);
  const pes::PesWorld::ClearAnswer answer = world.RunClearUnit(0, 3);
  EXPECT_EQ(answer.type, static_cast<uint32_t>(pes::kAnswerClear));
  EXPECT_GE(answer.angle_deg, 0.0f);
  EXPECT_LE(answer.angle_deg, 360.0f);
}

// MI+0x1198 counts logic frames, one per tick, and is the frame every reader takes (PES 0x140A128E4).
TEST(PesA1Sim, TheFrameCounterIsTheMatchInfoWordAndCountsTicks) {
  REQUIRE_PES_MATCH();
  const uint32_t before = pes::LoadU32(match->PesWorldOf().MatchInfo(), pes::kMiFrameOffset);
  for (int i = 0; i < 7; ++i) match->Process();
  const uint32_t after = pes::LoadU32(match->PesWorldOf().MatchInfo(), pes::kMiFrameOffset);
  EXPECT_EQ(after, before + 7u);
  EXPECT_EQ(match->GetPesFrame(), static_cast<int>(after));
}

// The pitch record F at G+0x2AA8: F[k] at +4k, written once from the field constants.
TEST(PesA1Sim, ThePitchRecordHoldsTheFieldConstants) {
  REQUIRE_PES_MATCH();
  const uint8_t* f = match->PesWorldOf().PitchRecord();
  EXPECT_FLOAT_EQ(pes::LoadF32(f, 0), pesFieldHalfLength);
  EXPECT_FLOAT_EQ(pes::LoadF32(f, 4), pesFieldHalfWidth);
  EXPECT_FLOAT_EQ(pes::LoadF32(f, 5 * 4), pesGoalAreaHalfWidth);
  EXPECT_FLOAT_EQ(pes::LoadF32(f, 6 * 4), pesPenaltyAreaLineX);
  EXPECT_FLOAT_EQ(pes::LoadF32(f, 17 * 4), pesCircleRadius);
}


TEST(PesA1Sim, TheAiReadsTheBallsRecordAtItsOwnTime) {
  REQUIRE_PES_MATCH();
  Ball* ball = match->GetBall();
  ASSERT_TRUE(ball);
  ball->SetPosition(Vector3(0.0f, 0.11f, 0.0f));
  ball->SetMomentum(Vector3(10.0f, 0.0f, 4.0f));
  for (int i = 0; i < 3; ++i) ball->Process();

  MentalImage image(match.get());
  image.SetTimeStampNeg_ms(0);
  image.UpdateBallPredictions();
  const auto& track = ball->GetTrack();
  // A floor read: the AI's own time in milliseconds lands on the record PES's reader
  // takes, not between two of them. The ball is flying, so an interpolated read
  // would land between records 0 and 1 of a record step apart.
  for (unsigned ms : {0u, 17u, 18u, 19u, 100u, 1000u, 2990u}) {
    const unsigned record =
        pesball::TrackIndexForAiMs(ms, static_cast<double>(ballPredictionSize_ms));
    EXPECT_NEAR((image.GetBallPrediction(ms) - track[record]).GetLength(), 0.0f, 1e-5f)
        << "at " << ms << " ms";
  }
  // The finite difference the AI takes across one record is the ball's own speed:
  // metres per PES tick times the tick rate.
  const Vector3 now = image.GetBallPredictionAtRecord(0, 0);
  const Vector3 next = image.GetBallPredictionAtRecord(0, 1);
  const float speed = ball->GetMovement().GetLength();
  EXPECT_GT(speed, 1.0f) << "the ball must be moving for the difference to mean anything";
  EXPECT_NEAR((next - now).GetLength() * pes::kPesTickHz, speed, speed * 0.05f);
}

// --- the clocks the sim keeps on the tick ----------------------------------

TEST(PesA1Sim, ThePossessionWindowCountsSecondsNotTenMillisecondSteps) {
  REQUIRE_PES_MATCH();
  MatchData* data = match->GetMatchData();
  ASSERT_TRUE(data);
  // The 60-second window holds seconds of possession, so a second of ticks moves
  // the factor by 0.5 / 60. Main's AddPossessionTime_10ms added 0.01 per 10 ms -
  // 1.0 a second - and a tick is 18.5 ms now, not 10.
  const float before = data->GetPossessionFactor_60seconds();
  for (unsigned long i = 0; i < Match::kFramesPerSecond; ++i)
    data->AddPossessionTime(1, Match::kTick_ms);
  EXPECT_NEAR(data->GetPossessionFactor_60seconds() - before, 0.5f / 60.0f, 2e-5f)
      << "one second of ticks is one second of the window";
  // Team 0 drains the same window by the same amount.
  for (unsigned long i = 0; i < Match::kFramesPerSecond; ++i)
    data->AddPossessionTime(0, Match::kTick_ms);
  EXPECT_NEAR(data->GetPossessionFactor_60seconds(), before, 2e-5f)
      << "a second of the other team's possession cancels it";
}

TEST(PesA1Sim, TheImageSomeMillisecondsAgoIsThatAgeNotGridStepsOfTen) {
  REQUIRE_PES_MATCH();
  // A snapshot per tick, so entry i of the ring is i ticks old and the milliseconds
  // a read asks for are an age, not a count on a 10 ms grid. The ball is the
  // witness: held still in the air and one metre further along x each tick, so an
  // image's ball track carries the tick that took it - the tracks are frozen at
  // their own tick, and a held ball's track does not move along x by itself.
  Ball* ball = match->GetBall();
  ASSERT_TRUE(ball);
  const int ticks = 12;
  for (int tick = 0; tick < ticks; ++tick) {
    ball->SetPosition(Vector3(static_cast<float>(tick), 5.0f, 0.0f));
    ball->SetMomentum(Vector3(0.0f, 0.0f, 0.0f));
    match->Process();
  }
  const auto ballXOfAge = [&](int ageTicks) {
    const int ms = static_cast<int>(ageTicks * Match::kTick_ms + 0.5);
    const MentalImage* image = match->GetMentalImage(ms);
    EXPECT_TRUE(image);
    return image->GetBallPrediction(0).coords[0];
  };
  // Entry i is i ticks old, so the two reads are one metre apart on the ramp. The
  // 10 ms grid would have read 19 ms as entry 2 and 37 ms as entry 4 - two metres.
  EXPECT_NEAR(ballXOfAge(1) - ballXOfAge(2), 1.0f, 0.15f)
      << "the read of 19 ms and the read of 37 ms are one entry apart, not three";
}
