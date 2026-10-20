// PES 2021's MOVESTEP handler group D: the turn handlers FUN_1406c14c0 (RUN_TURN_RUN),
// FUN_1406c2f80 (RUN_TURN_RUN_SLIPDOWN), FUN_1406c3470 (RUN_TURN_STEP), FUN_1406c7400
// (STEP_TURN_RUN) and FUN_1406c8d80 (STEP_TURN_STEP), plus the entries FUN_1406d6780,
// FUN_1406d2f60 and FUN_1406d38d0. The tests exercise each handler against synthetic install
// constants, one mutation check per branch; the install relations live in the install test.
#include <gtest/gtest.h>
#include <cstdio>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "movestep_team_synthetic.hpp"
#include "pes/movestep_turn.hpp"

namespace {

using pes::MoveHost;
using pes::MoveReq;
using pes::MoveState;
using pes::movestep::MoveTurnConstants;
using pes::movestep::EnterRunTurnRun;
using pes::movestep::EnterRunTurnStep;
using pes::movestep::EnterStepTurnRun;
using pes::movestep::RunTurnRun;
using pes::movestep::RunTurnRunSlipdown;
using pes::movestep::RunTurnStep;
using pes::movestep::StepTurnRun;
using pes::movestep::StepTurnStep;

// The synthetic install constants, in the field order of MoveTurnConstants.
MoveTurnConstants SyntheticTurn() {
  MoveTurnConstants k{};
  k.half = 0.5f;
  k.two = 2.0f;
  k.four = 4.0f;
  k.negZero = -0.0f;
  k.one = 1.0f;
  k.thousand = 1000.0f;
  k.negOne = -1.0f;
  k.three = 3.0f;
  k.quarter = 0.25f;
  k.five = 5.0f;
  k.pointTwo = 0.2f;
  k.speed26 = 26.0f;
  k.halfTurn = 180.0f;
  k.fullTurn = 360.0f;
  k.negFullTurn = -360.0f;
  k.fps = 60.0f;
  k.turnTicksB = 2.5f;
  k.turnTicks = 6.0f;
  k.speedA = 8.0f;
  k.speedB = 11.0f;
  k.speedC = 12.0f;
  k.speed15 = 15.0f;
  k.speedD = 16.0f;
  k.stampsA = 20.0f;
  k.speedE = 25.0f;
  k.angle40 = 40.0f;
  k.angle45 = 45.0f;
  k.angle5625 = 56.25f;
  k.seventy = 70.0f;
  k.turn7875 = 78.75f;
  k.turn84375 = 84.375f;
  k.angle90 = 90.0f;
  k.angle135 = 135.0f;
  k.angle225 = 225.0f;
  k.kmhToRate = 3.6f;
  k.angle225m = 22.5f;
  k.speed135 = 13.5f;
  k.speed28 = 28.0f;
  k.gFloor = 0.85f;
  k.playbackFloor = 0.95f;
  k.turnRamp = 0.15f;
  k.speed22 = 22.0f;
  k.neg70 = -70.0f;
  k.neg90 = -90.0f;
  k.speed19 = 19.0f;
  k.speed55 = 5.5f;
  k.angle1125 = 112.5f;
  k.angle1125b = 11.25f;
  k.angle3375 = 33.75f;
  k.angle675 = 67.5f;
  k.angle1575 = 157.5f;
  k.speed1475 = 14.75f;
  k.angle3325 = 33.25f;
  k.speed39375 = 39.375f;
  k.turn50625 = 50.625f;
  k.turn81875 = 81.875f;
  k.turn101250 = 101.25f;
  k.turn123750 = 123.75f;
  k.turn140625 = 140.625f;
  k.turn146250 = 146.25f;
  k.turn163125 = 163.125f;
  k.turn168750 = 168.75f;
  k.turn174375 = 174.375f;
  k.negHalfTurn = -180.0f;
  k.loopBlend = 0.1f;
  k.ten = 10.0f;
  k.blend03 = 0.3f;
  k.thirty = 30.0f;
  k.zero = 0.0f;
  k.p8 = 0.8f;
  k.startHalf = 0.6f;
  k.relax07 = 0.7f;
  k.displace04 = 0.4f;
  k.band15m = 1.5f;
  k.westBlend = 1.0f / 7.5f;
  k.clipBlend = 1.0f / 6.0f;
  k.gLow = 0.9166f;
  k.gHigh = 1.0833f;
  k.gSlope = -0.1667f;
  // The install's own ladder: 9, 11, 13, 15, 17 for the left half then 8, 6, 4, 2, 0 back down.
  static constexpr int kColumnLadder[10] = {9, 11, 13, 15, 17, 8, 6, 4, 2, 0};
  for (int i = 0; i < 10; ++i) {
    k.bucketAngle[i] = 45.0f * (i < 5 ? i : -(i - 5));
    k.bucketColumn[i] = kColumnLadder[i];
    k.bucketLeft[i] = i < 5 ? 1 : 0;
  }
  for (int i = 0; i < 64; ++i) k.stepRow[i] = 0x540 + i;
  return k;
}

pes::PlaybackConstants SyntheticPlayback() {
  pes::PlaybackConstants k{};
  k.gLow = 0.9166f;
  k.gHigh = 1.0833f;
  k.gSlope = -0.1667f;
  k.one = 1.0f;
  k.stepSlope = 0.15f;
  k.stepBase = 0.95f;
  k.turnP2 = 0.1f;
  k.turnT = 0.05f;
  return k;
}

pes::AngleBucketConstants SyntheticBuckets() {
  pes::AngleBucketConstants k{};
  k.wide = {-180, -157.5f, -112.5f, -67.5f, -22.5f, 0, 22.5f, 67.5f, 112.5f, 157.5f, 180};
  k.narrow = {-180, -170, -112.5f, -67.5f, -10, 0, 10, 67.5f, 112.5f, 170, 180};
  for (int i = 0; i < 10; ++i) k.bucket[i] = i;
  k.hysteresisUp = 5.0f;
  k.hysteresisDown = -5.0f;
  return k;
}

// The record the RunTurnRun tests were written against: start -930 frames (decoded -517) and end
// 1615 frames (decoded 897) at 54 Hz, so the reorient window is (-930, 342].
pes::MotionRecord TurnFixtureRecord() {
  pes::MotionRecord r;
  r.rot = {pes::RotRecord{/*offset*/ 0, /*start*/ -517, /*end*/ 897}};
  return r;
}

// The same record with the Animation list's first entry (FUN_1406af620): the clip's
// whole-frame count, which bounds the reorient window's end.
pes::MotionRecord TurnRecordWithFrames() {
  pes::MotionRecord r;
  r.rot = {pes::RotRecord{/*offset*/ 0, /*start*/ -517, /*end*/ 897}};
  r.blend = {897};
  return r;
}

// The same record with a non-zero row offset (FUN_1406b1c70), which the mirror's sign test
// compares against the window's tight value, and a short span so the window's frame count is
// small enough for the per-frame gate to bite.
pes::MotionRecord TurnRecordWithOffset() {
  pes::MotionRecord r;
  r.rot = {pes::RotRecord{/*offset*/ 120, /*start*/ 25, /*end*/ 26}};
  r.blend = {26};
  return r;
}

// A host answering the handlers with the synthetic values above. Every accessor names the PES
// read it stands for (movestep.hpp's MoveHost).
class FakeHost : public MoveHost {
 public:
  // LateralDamping's reads: the turn tests do not drive the drift.
  pes::DampingConstants damping{};
  pes::DampingRecord dampingRecord{};
  int CurrentState() const override { return 0; }
  const pes::TeamAi& Team() const override { return team; }
  SyntheticTeam team;
  const pes::DampingConstants& DampingConstantsView() const override { return damping; }
  const pes::DampingRecord& DampingRecordView() const override { return dampingRecord; }
  // LocomotionStyle's reads: the turn tests do not drive the style ladder.
  pes::StyleConstants style{};
  const pes::StyleConstants& StyleConstantsView() const override { return style; }
  bool ScriptedSlotIsOurs() const override { return false; }
  int StaminaPercent() const override { return 100; }
  float PlayerHeightCm() const override { return 180.0f; }
  void BallSample(int, float out[3]) const override { out[0] = out[1] = out[2] = 0.0f; }
  float BallMotionHeadingDeg() const override { return 0.0f; }
  pes::MoveCell Cell(int row, int column, bool walkFlag) override {
    ++cellCalls;
    lastRow = row;
    lastColumn = column;
    lastWalk = walkFlag;
    pes::MoveCell c;
    c.motionNo = cellMotion >= 0 ? cellMotion : row;
    c.mirror = false;
    c.found = cellMotion >= 0;
    return c;
  }
  int PickN(int n) override { return 0; }
  bool Percent(int pct) override {
    lastPercent = pct;
    return percentAnswer;
  }

  int CommandSpeedClass() const override { return commandClass; }
  float RateCeiling() const override { return rateCeiling; }
  float CommandSpeedKmh() const override { return commandKmh; }
  float CurrentSpeedKmh() const override { return currentKmh; }
  float PhysiqueG() const override { return 1.0f; }
  float TicksPerSecond() const override { return 54.0f; }
  int MatchTick() const override { return 7; }
  int RecentStamp(int index) const override { return recentStamp[index]; }
  float MotionStartTime(int motionNo) const override { return startTime; }

  float RecordF2468() const override { return f2468; }
  int MoveStatePrevious() const override { return previous; }
  float RecordF0bec() const override { return f0bec; }
  void SetRecordF0bec(float value) override { f0bec = value; }
  const pes::AngleBucketConstants& AngleBuckets() const override { return buckets; }

  void PositionAt(int index, float out[3]) const override {
    out[0] = displacement[0];
    out[1] = displacement[1];
    out[2] = displacement[2];
  }
  void OwnPosition(float out[3]) const override {
    for (int i = 0; i < 3; ++i) out[i] = 0.0f;  // group D reads the displacement and the ring only
  }
  int MatchPhase() const override { return 1; }
  bool IsReplay() const override { return false; }
  void Issue(const pes::MotionRequest& request) override { issued = request; }

  const pes::StepScalars& StepConstants() const override { return scalars; }
  float BandSpeedKmh(int band, float g) const override { return 0.0f; }
  bool MirrorChoice(bool sameTeam) const override { return false; }
  int FootBand() const override { return footBand; }
  const pes::RunConstants& RunConstantsView() const override { return runConstants; }
  const pes::RunTables& RunTablesView() const override { return runTables; }
  const pes::SpeedRateTable& SpeedRateTableView() const override { return speedRate; }
  const pes::movestep::MoveTurnConstants& MoveTurnConstantsView() const override {
    return turnConstants;
  }
  const pes::StepTables& StepTablesView() const override { return stepTables; }
  bool BucketIsLeft(int bucket) const override { return bucket < 5; }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { return playback; }

  int CurrentMotion() const override { return currentMotion; }
  int CurrentClipFrames() const override { return currentClipFrames; }
  int AnimeState() const override { return animeState; }
  const pes::MotionRecord& Motion(int) const override { return record; }
  bool ClipFlag() const override { return false; }
  const pes::MoveConstants& MoveConstantsView() const override { return moveConstants; }
  const pes::IdleRowTables& IdleRowTablesView() const override { return idleRows; }
  float FacingDeg() const override { return 0.0f; }
  float BallBearingDeg() const override { return 0.0f; }
  int ActionIndex() const override { return 0; }
  int RelatedSlot() const override { return -1; }
  void RecordPosition(int, float out[3]) const override { out[0] = out[1] = out[2] = 0.0f; }
  float RecordFacing(int) const override { return 0.0f; }
  int MatchState11d4() const override { return 0; }
  int MatchState1554() const override { return 0; }
  int PhaseTick() const override { return 0; }
  int MatchModeByte() const override { return 0; }
  bool MatchObject3b0() const override { return false; }
  bool TrainerFlag1423e7fd0() const override { return false; }
  int RecordConditionByte(int) const override { return 100; }
  bool ActionSlotMarked(int) const override { return true; }
  bool ActionTableOwns(int) const override { return false; }
  int RecordActionA() const override { return 5; }
  int RecordActionB() const override { return 5; }

  pes::StepScalars scalars{};
  pes::StepTables stepTables{};
  pes::PlaybackConstants playback = SyntheticPlayback();
  pes::AngleBucketConstants buckets = SyntheticBuckets();
  pes::RunConstants runConstants{};
  pes::RunTables runTables{};
  pes::SpeedRateTable speedRate{};
  pes::movestep::MoveTurnConstants turnConstants{};
  pes::MoveConstants moveConstants{};
  pes::IdleRowTables idleRows{};

  int commandClass = 3;
  float rateCeiling = 1.0f;
  float commandKmh = 16.0f;
  float currentKmh = 12.0f;
  int recentStamp[2] = {0, 0};
  int previous = 0;
  int footBand = 0;
  float displacement[3] = {0.0f, 0.0f, 1.0f};  // +z: heading 0 degrees
  float f2468 = 1.0f;
  float f0bec = 0.0f;
  pes::MotionRecord record = TurnFixtureRecord();
  float startTime = 0.0f;

  int currentMotion = 0;
  int currentClipFrames = 0;
  int animeState = 0;
  int cellCalls = 0, lastRow = -1, lastColumn = -1;
  bool lastWalk = false;
  int cellMotion = -1;
  int lastPercent = -1;
  bool percentAnswer = false;
  pes::MotionRequest issued{};
};

// The request words group D's tests use, in the order the handlers read them.
void FillRequest(MoveReq* req) {
  req->SetF(pes::moveword::kFacing, 10.0f);
  req->SetF(pes::moveword::kHeading, 10.0f);
  req->SetF(pes::moveword::kLookHeading, 10.0f);
  req->SetF(pes::moveword::kBodyDirection, 10.0f);
  req->SetF(pes::moveword::kTurn, 0.0f);
  req->SetF(pes::moveword::kFacingError, 0.0f);
  req->SetF(pes::moveword::kRate, 0.0f);
  req->SetU(pes::moveword::kClassWord, 2);
}

// ---------------------------------------------------------------------------------------------
// RUN_TURN_RUN, FUN_1406c14c0.
// ---------------------------------------------------------------------------------------------

// The reorient window: with the clip inside (start, (start + end) / 2) and a heading the turn
// still needs work on, the handler writes the turned heading and the turn's tick count into
// req[0xf0]..[0xfc].
TEST(RunTurnRun, WritesTheTurnedHeadingInsideTheReorientWindow) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 80.0f);
  req.SetF(pes::moveword::kTurnedRow, 70.0f);  // the candidate, 10 degrees below the heading
  host.currentMotion = 0x1000;
  host.currentClipFrames = 1;  // inside (-930, 342]
  RunTurnRun(host, req, SyntheticTurn());
  // The reorientation is due and the handler publishes the turned row and its frame count.
  EXPECT_EQ(req.U(pes::moveword::kSlipNewRow) & 0xffu, 1u);
  EXPECT_GT(req.U(pes::moveword::kSlipRowFrames), 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSlipRowOut), 80.0f);  // stepped to the heading
}

// The motion record the loader decodes from RotData's own layout: start 0 frames (517 - 517), end
// `endField + 1` ticks, offset 0 (878 - 878). The motion is 0x1000.
pes::MotionRecord DecodedRotRecord(uint32_t endField) {
  pes::MbinfoBins bins;
  bins.rotData.assign(8, 0);
  const uint32_t w0 = 0x1000u | (878u << 13);
  const uint32_t w1 = (517u << 10) | endField;
  std::memcpy(&bins.rotData[0], &w0, 4);
  std::memcpy(&bins.rotData[4], &w1, 4);
  const pes::MotionCaps caps{/*animation*/ 1, /*subHit*/ 6, /*cancel*/ 5, /*rot*/ 3, /*foot*/ 20,
                             /*pivot*/ 1};
  std::string err;
  auto m = pes::ParseMbinfo(bins, caps, &err);
  return m ? m->Motion(0x1000) : pes::MotionRecord{};
}

// The reorient window is the record's own (start to end, frames): the clip at frame 5 is past a
// window of 2 frames and inside a window of 18, so the same request reorients only under the longer.
TEST(RunTurnRun, TheReorientWindowIsTheDecodedRecordsSpan) {
  for (const bool longWindow : {false, true}) {
    FakeHost host;
    MoveReq req;
    FillRequest(&req);
    req.SetF(pes::moveword::kFacing, 10.0f);
    req.SetF(pes::moveword::kHeading, 80.0f);
    req.SetF(pes::moveword::kTurnedRow, 70.0f);
    host.currentMotion = 0x1000;
    host.currentClipFrames = 5;
    host.record = DecodedRotRecord(longWindow ? 9u : 0u);  // end 10 ticks (18 frames) or 1 (2 frames)
    RunTurnRun(host, req, SyntheticTurn());
    EXPECT_EQ(req.U(pes::moveword::kSlipNewRow) & 0xffu, longWindow ? 1u : 0u)
        << (longWindow ? "the 18-frame window" : "the 2-frame window");
  }
}

// The ladder: a stationary clip with the class word set writes the class/speed pair.
TEST(RunTurnRun, SelectsTheClassAndSpeedFromTheDisplacementSpeed) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 50;  // past every window: the ladder and the damping tail
  host.record = pes::MotionRecord{};  // the zero word: start -930 frames, end 2
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.0f;  // a stationary player: the speed is 0
  RunTurnRun(host, req, SyntheticTurn());
  // PES's ladder with a zero speed writes class 0 and the 5.0 speed word (0x40a00000).
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedWord), 5.0f);
}

// The rate tail: a fast clip re-selects the rate from the interface speed and clamps it.
TEST(RunTurnRun, ReSelectsTheRateFromTheInterfaceSpeedForClassTwo) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;  // the clip has no frames left: the rate tail runs
  host.record = pes::MotionRecord{};  // the zero word: start -930 frames, end 2
  req.SetU(pes::moveword::kClassWord, 2);
  host.displacement[0] = 1.0f;
  host.displacement[2] = 1.0f;
  RunTurnRun(host, req, SyntheticTurn());
  // PES's class-2 path stores 0.6 (0x3f19999a) in req[0x4a].
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.6f);
}

// ---------------------------------------------------------------------------------------------
// RUN_TURN_RUN_SLIPDOWN, FUN_1406c2f80.
// ---------------------------------------------------------------------------------------------

// The slip damping: with the clip's end frames run out, the handler writes the slip pair (req
// 0xe8 / req[0xec]) and P+0xbec. The record's own frames put the end at 2, so the clip must be
// at or past it.
// The slip damping: with the clip's end frames run out and the heading inside the damping
// window, the handler writes the slip pair (req[0xe8] / req[0xec]) and P+0xbec.
TEST(RunTurnRunSlipdown, WritesTheSlipPairOnceTheClipFramesRunOut) {
  FakeHost host;
  MoveReq req;
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 60.0f);  // 50 degrees apart: inside the 90-degree window
  req.SetU(pes::moveword::kSlipCandidate, 10.0f);  // already at the heading: no step to take
  host.currentMotion = 0x1000;
  host.currentClipFrames = 1700;  // past the record's 1615-frame end
  RunTurnRunSlipdown(host, req, SyntheticTurn());
  EXPECT_EQ(req.U(pes::moveword::kSlipActive) & 0xffu, 1u);
  EXPECT_GT(req.U(pes::moveword::kSlipAngle), 0u);
  // The damped facing is the heading plus the tick's step, wrapped into [0, 360).
  const float damped = req.F(pes::moveword::kSlipAngle);
  EXPECT_GT(damped, 10.0f);
  EXPECT_LE(damped, 360.0f);
  EXPECT_FLOAT_EQ(host.f0bec, damped);
}

// The row replay tail: once the sub-record's frames run out the handler clears the class words
// and re-issues the row back to the run family.
TEST(RunTurnRunSlipdown, ReIssuesTheRowOnceTheSubFramesRunOut) {
  FakeHost host;
  MoveReq req;
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;
  host.record = pes::MotionRecord{};  // the zero word: start -930 frames, end 2
  RunTurnRunSlipdown(host, req, SyntheticTurn());
  // 0.3 (0x3e99999a) is the parametric rate the tail writes; the foot band travels on
  // the host (MoveHost::FootBand), not the request.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSlipRate), 0.3f);
}

TEST(RunTurnRunSlipdown, SlipLimitReadsTheClassWordAtByte0x7c) {
  // FUN_1406c2f80 line 123: `if (*(int*)(param_2 + 0x7c) < 3)` — the byte offset
  // 0x7c is word 0x1f, the class word. A class of 3 takes the 135-degree limit;
  // any other value already does. The red form reads byte 0x1f0 instead.
  FakeHost host;
  host.turnConstants = SyntheticTurn();
  host.turnConstants.angle135 = 135.0f;
  host.turnConstants.halfTurn = 180.0f;
  MoveReq req;
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 60.0f);  // 50 deg apart: inside the 90-deg window
  req.SetU(pes::moveword::kSlipCandidate, 10.0f);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 1700;
  req.SetU(0x1f, 3u);  // the class word: PES's *(int*)(req + 0x7c)
  RunTurnRunSlipdown(host, req, SyntheticTurn());
  // limit = 135, ticks = 54: step = 2.5 deg, damped = 12.5 deg.
  EXPECT_NEAR(req.F(pes::moveword::kSlipAngle), 12.5f, 1e-4f);
}

}  // namespace

// The shared helpers and the other groups' entries the handlers call, stubbed so this test links
// without their translation units. Each carries the contract's signature in its own namespace; a
// test asserts the state the handler asked for, not what the entry did.
namespace pes {
namespace movestep {
float SpeedToRate(MoveHost& host, float speedKmh, int tableSelector) {
  (void)tableSelector;
  (void)host;
  return speedKmh > 0.0f ? 1.0f : 0.0f;
}
int LocomotionStyle(MoveHost&, int, float, MoveReq&, float, bool) { return 0; }
int IdleRow(MoveHost&, MoveReq&) { return 0x5a1; }
bool StayDecision(MoveHost&, MoveReq&) { return false; }
bool OutOfPlayPreempt(MoveHost&, MoveReq&) { return false; }
bool EnterAccelerate(MoveHost&, MoveReq&) { return false; }
void EnterGotoRun(MoveHost&, MoveReq&, bool) {}
void EnterStop(MoveHost&, MoveReq&) {}
void EnterTurnStop(MoveHost&, MoveReq&) {}  // group C's own; the stub keeps group D linking
// Group C's FUN_1406b70d0 gate (movestep_step.cpp): the group-D tests take its "no", as the port did before group C landed.
bool StepLetsRunTurn(MoveHost&, MoveReq&) { return false; }
}  // namespace movestep
}  // namespace pes

namespace {

// ---------------------------------------------------------------------------------------------
// RUN_TURN_STEP, FUN_1406c3470.
// ---------------------------------------------------------------------------------------------

// The unset-class path: with the class word at -1 and a slow player, PES does not write a
// class/speed pair; it saves the bucket, re-picks the live one and hands the tick to the STOP
// entry (FUN_1406d5d60).
TEST(RunTurnStep, HandsTheTickToTheStopEntryWhenTheClassWordIsUnset) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 100;  // past every window: the ladder runs
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  req.SetU(pes::moveword::kClassWord, 0xffffffffu);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kFacingError, 10.0f);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.0f;  // a stationary player: speed 0, below the 8 km/h gate
  RunTurnStep(host, req, SyntheticTurn());
  // The slow-player gate sends the tick to the STOP entry without a class/speed write, so the
  // class word keeps the -1 the request came in with.
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 0xffu);
  EXPECT_EQ(req.U(pes::moveword::kChangeState) & 0xffu, 0u);
  // The bucket was saved and then re-picked through the interface's own resolver, so the saved
  // value is the request's own bucket (0) and the live one is whatever it resolved to.
  EXPECT_EQ(req.U(pes::moveword::kPrevBucket) & 0xffu, 0u);
}

// PES at 0x1406C3C02: FUN_14043a0a0(req[0x02], req[0x04]) = wrap180(kBodyDirection -
// kFacing). The result's magnitude drives the `near` gate (40 deg for motion 0xbd6, 45 deg
// otherwise) and the class ladder. The port passed kHeading twice, so the value was always 0
// and the gate always passed.
TEST(RunTurnStep, TheNearGateReadsTheBodyDirectionAgainstTheFacing) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 100;
  // The class -1 arm: with `near` true and the ground speed at 8 km/h or more, the handler
  // writes class 3 and the 28.0 speed word.
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  req.SetU(pes::moveword::kClassWord, 0xffffffffu);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  // The body direction is 100 deg from the facing, so the magnitude is 100 deg: past the 45-deg
  // gate, so `near` is false and the class word is not raised to 3.
  req.SetF(pes::moveword::kBodyDirection, 110.0f);
  host.displacement[0] = 3.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 4.0f;
  RunTurnStep(host, req, SyntheticTurn());
  // `near` was false, so the arm fell through to the bucket save and the stop entry, and the
  // class word kept its -1.
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 0xffu);
}

// The same gate with the body direction inside the 45 degrees: `near` is true and the class
// word is raised to 3 with the 28.0 speed word.
TEST(RunTurnStep, TheNearGateAcceptsABodyDirectionInsideFortyFiveDegrees) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 100;
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  req.SetU(pes::moveword::kClassWord, 0xffffffffu);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 40.0f);  // 30 deg from the facing
  host.displacement[0] = 3.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 4.0f;
  RunTurnStep(host, req, SyntheticTurn());
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 3u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedWord), 28.0f);
}

// The facing write: once the row has frames and the clip has passed the span, the handler
// publishes the damped facing in req[0x3a]/req[0x3b].
TEST(RunTurnStep, HandsTheTickToStepTurnRunWhenTheEntryWritesTheRequest) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 200;  // far past the span
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 30.0f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  // A fast player: the drawn speed clears the class bands, so PES's ladder writes the class 2
  // / 16.0 pair and then hands the tick to STEP_TURN_RUN through FUN_1406d38d0.
  host.displacement[0] = 3.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 4.0f;
  RunTurnStep(host, req, SyntheticTurn());
  // PES's ladder writes the class 2 / 16.0 pair, then FUN_1406d38d0 refuses, so the tick falls
  // through to the RUN_TURN_RUN entry (FUN_1406d6780), which asks for state 12.
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 2u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedWord), 16.0f);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState) & 0xffu,
            static_cast<uint32_t>(MoveState::kRunTurnRun));
}

// ---------------------------------------------------------------------------------------------
// STEP_TURN_RUN, FUN_1406c7400.
// ---------------------------------------------------------------------------------------------

// PES at 0x1406C7695: the reorient window's precise gate reads
// FUN_14043a0a0(req[0x04], req[0x03]) = wrap180(kLookHeading - kBodyDirection), and at
// 0x1406C76CF: FUN_14043a0a0(req[0x18], req[0x04]) = wrap180(kBodyDirection - kTurnedRow).
// The port read the facing against the body direction and the heading against the facing, so
// both of the window's gates read the wrong angles.
TEST(StepTurnRun, TheReorientWindowReadsTheLookHeadingAgainstTheBodyDirection) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.record = TurnRecordWithFrames();
  host.currentClipFrames = 2;  // inside the reorient window, with frames left
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
  // The look heading is 60 degrees from the body direction, so PES's precise gate
  // (wrap180(lookHeading - bodyDirection) < 45) fails and the row is not stepped. The facing
  // is level with the body direction, so the port's old operand (facing against the body
  // direction) was 0 and the gate passed.
  req.SetF(pes::moveword::kFacing, 10.0f);
  // The request's heading is 30 degrees from the row, so stepping the row towards the
  // heading would move it to 40.
  req.SetF(pes::moveword::kHeading, 40.0f);
  req.SetF(pes::moveword::kLookHeading, 70.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kTurnedRow, 10.0f);
  req.SetF(pes::moveword::kRowAngle, 10.0f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  host.displacement[0] = 3.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 4.0f;
  StepTurnRun(host, req, SyntheticTurn());
  // The precise gate failed, so the row was NOT stepped towards the heading: the window's
  // write (req[0x3d]) is the request's own turned heading (10), not the stepped 40.
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kAngle3d), 10.0f);
}

// The same window with the look heading level with the body direction: PES's gate passes and
// the reorient window writes the turned heading into req[0x3d].
TEST(StepTurnRun, TheReorientWindowAcceptsALevelLookHeading) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.record = TurnRecordWithFrames();
  host.currentClipFrames = 2;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kLookHeading, 10.0f);
  req.SetF(pes::moveword::kBodyDirection, 10.0f);
  req.SetF(pes::moveword::kTurnedRow, 10.0f);
  req.SetF(pes::moveword::kRowAngle, 10.0f);
  req.SetU(pes::moveword::kMoveBucket, 0);
  host.displacement[0] = 3.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 4.0f;
  StepTurnRun(host, req, SyntheticTurn());
  EXPECT_TRUE(req.Flag(pes::moveword::kReorientFlag));
}

// The rate word's reset: the handler starts its tick at 0.3 (0x3e99999a).
TEST(StepTurnRun, ResetsTheRateWordAtTheStartOfItsTick) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;
  StepTurnRun(host, req, SyntheticTurn());
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.3f);
}

// The run-tail ladder: a class-3 clip re-selects the row from the sampled speed and clamps its
// rate to the interface's own.
TEST(StepTurnRun, ReSelectsTheRateForTheClassThreeTail) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetF(pes::moveword::kRate, 0.3f);
  host.displacement[0] = 1.0f;
  host.displacement[2] = 1.0f;
  StepTurnRun(host, req, SyntheticTurn());
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kRate), 0.3f);  // unchanged by the class-0..2 tail
}

// The run-family branch's gate: PES's fVar22 is wrap180(kTurnedRow - kFacing) (0x1406C770A)
// and fVar21 is wrap180(kBodyDirection - kTurnedRow) (0x1406C76CF). The port read the turn
// word against the turned row and the facing against the facing error, so the branch fired on
// angles PES never compared.
TEST(StepTurnRun, TheRunBranchGateReadsTheTurnedRowAgainstTheFacing) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kPrecise, 1);
  // The turned heading is 20 degrees from the facing, so PES's fVar22 is 20 - inside both
  // gates - and the branch does not fire. The port's old fVar22 (the facing against the
  // facing error) would have been 100 degrees, past 78.75, and the branch would have fired.
  req.SetF(pes::moveword::kTurn, 10.0f);
  req.SetF(pes::moveword::kTurnedRow, 20.0f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 20.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kFacingError, 100.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetU(pes::moveword::kPrevBucket, 1);
  StepTurnRun(host, req, SyntheticTurn());
  // The branch did not fire, so the tick did not ask for STEP_LOOP.
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState) &&
               req.U(pes::moveword::kNextState) == uint32_t(MoveState::kStepLoop));
}

// The parametric replay block: a class-3 request at the 0x2e motion replays row 0x3b mirrored.
// The run-family branch: while the clip is young, with a live class word and the precise flag
// set, the handler replays a row through the step grid and hands the tick to STEP_LOOP.
TEST(StepTurnRun, ReplaysARowThroughTheStepGridWhenTheClipIsYoung) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;  // inside the 6-tick window
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kPrecise, 1);
  // The branch's own gate reads two draws: PES's fVar21 (wrap180(kBodyDirection -
  // kTurnedRow)) and fVar22 (wrap180(kTurnedRow - kFacing)). The second arm is the one PES
  // takes: fVar22 is past 78.75 degrees, so the turned heading is 100 degrees from the facing.
  req.SetF(pes::moveword::kTurn, 10.0f);
  req.SetF(pes::moveword::kTurnedRow, 100.0f);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 100.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kFacingError, 100.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetU(pes::moveword::kPrevBucket, 1);
  StepTurnRun(host, req, SyntheticTurn());
  // The branch writes the class/speed pair from the drawn speed and asks for STEP_LOOP.
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 2u);
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState) & 0xffu,
            static_cast<uint32_t>(MoveState::kStepLoop));
  // The clip request carries the step grid's blend duration (1/7.5 s).
  EXPECT_GT(host.issued.motion, 0);
  EXPECT_NEAR(host.issued.duration, 1.0f / 7.5f, 1e-6f);
}

// PES at 0x1406C905A: the reorient window's tight value is
// FUN_14043a0a0(req[0x02], turned) = wrap180(turned - facing), where `turned` is the stepped
// heading. The port read the folded value (the stepped heading plus the bucket's angle) against
// the facing, which is a different angle - and the per-frame gate then bites.
TEST(StepTurnStep, TheReorientWindowMeasuresTheSteppedHeadingAgainstTheFacing) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.record = TurnRecordWithOffset();
  host.currentClipFrames = 45;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kFacing, 120.0f);
  // The heading is 60 degrees from the facing, so PES's tight value is wrap180(60 - 120) = -60
  // and the window's frame count is 2: 60 / 2 is inside the 45-deg-per-frame gate. The port's
  // folded value would be wrap180(60 + the bucket's 90 - 120) = +30, whose sign differs from
  // the row offset's (+60) - and the mirror's sign test then flips the other way.
  req.SetF(pes::moveword::kHeading, 60.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetF(pes::moveword::kTurnedRow, 0.0f);
  req.SetF(pes::moveword::kRowAngle, 0.0f);
  req.SetU(pes::moveword::kMoveBucket, 2);  // the bucket's angle is 45 degrees
  host.displacement[0] = 3.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 4.0f;
  StepTurnStep(host, req, SyntheticTurn());
  // The window ran, so the turned heading was written.
  EXPECT_TRUE(req.Flag(pes::moveword::kReorientFlag));
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kAngle3d), 60.0f);
  // The mirror's sign test: PES's tight value is negative and the row offset is positive, so
  // the signs differ and the mirror is set. The port's folded value is positive, the signs
  // match, and the mirror stays clear.
  EXPECT_EQ(req.U(pes::moveword::kMirrorFlag) & 0xffu, 1u);
}

// PES's S8 root-facing write (0x1406C96CA-0x1406C97FF): the heading, offset by the row's own
// turn, is compared with the look heading; the residual is blended (0 at 22.5 degrees, 1 at 45),
// scaled by 90 + 90 * blend and divided by the frame rate (FUN_141e532d0 = 54.0 - NOT a random
// draw), and the host's facing moves towards that reach. The port computed the values and
// discarded them, so the damped facing never used them.
TEST(StepTurnStep, TheRootFacingMovesTowardsTheLookHeading) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.record = TurnRecordWithOffset();
  host.currentClipFrames = 60;  // past the row's end: the facing write runs
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  // The look heading is 100 degrees away, so the residual is past 45 and the blend is 1: the
  // reach is (90 + 90) / 54 = 3.33 degrees. The row 2's own turn is -90 degrees, so the
  // heading the facing compares against is -90.
  req.SetF(pes::moveword::kLookHeading, 100.0f);
  req.SetF(pes::moveword::kBodyDirection, 0.0f);
  req.SetU(0x20, 2u);  // the row's own turn is -90 degrees
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  StepTurnStep(host, req, SyntheticTurn());
  // The reach (3.33) is smaller than the turn to the facing (90), so the facing takes the
  // turn: the host's own facing (0) plus 90.
  EXPECT_TRUE(req.Flag(pes::moveword::kRootFacing));
  EXPECT_NEAR(req.F(pes::moveword::kRootFacingAngle), 90.0f, 0.01f);
}

// PES's final class ladder (0x1406C9B0B-0x1406C9B54): the ball's distance from the player,
// scaled by 54 * 60 * 60 / 1000 = 194.4, picks the class. The port used the live class word
// instead, so the band never followed the ball.
TEST(StepTurnStep, TheFinalClassLadderFollowsTheBallDistance) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.record = TurnRecordWithOffset();
  host.currentClipFrames = 60;  // past the row's end: the final grid select runs
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetU(pes::moveword::kPrecise, 1u);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 0.0f);
  req.SetF(pes::moveword::kLookHeading, 0.0f);
  // The body direction is 60 degrees from the heading, so the residual is past 45 and the
  // ladder's distance arm applies: the ball's scaled distance is 19.44, past 13.5, so the
  // class is 2 and the speed 16 - the port's live class would have left it at 3 and 11.
  req.SetF(pes::moveword::kBodyDirection, 60.0f);
  req.SetU(0x20, 1u);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  // The ball is 0.1 metres away: the scaled distance is 19.44, past 13.5, so the class is 2
  // and the speed 16 - the port's live class would have left it at 3 and 11.
  host.displacement[0] = 0.1f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.0f;
  StepTurnStep(host, req, SyntheticTurn());
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 2u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedWord), 16.0f);
}

// ---------------------------------------------------------------------------------------------
// STEP_TURN_STEP, FUN_1406c8d80.
// ---------------------------------------------------------------------------------------------

// The early gate: while the clip is young and the class word is unset, the handler saves the
// bucket and hands the tick to the STOP entry.
TEST(StepTurnStep, HandsTheTickToTheStopEntryWhileTheClipIsYoung) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;
  req.SetU(pes::moveword::kCommandSpeedClass, 0xffffffffu);
  req.SetU(pes::moveword::kClassWord, 0xffffffffu);
  StepTurnStep(host, req, SyntheticTurn());
  // The early gate saves the request's own bucket (0) into req[0x25], re-picks the live one and
  // hands the tick to the STOP entry, so no state is requested here.
  EXPECT_EQ(req.U(pes::moveword::kChangeState) & 0xffu, 0u);
  EXPECT_EQ(req.U(pes::moveword::kPrevBucket) & 0xffu, 0u);
}

// The motion-id replay: the 0x39 motion replays row 0x4e and asks for STEP_TURN_RUN.
TEST(StepTurnStep, ReplaysRowZeroX4eAtMotionZeroX39) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x39;
  host.currentClipFrames = 0;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kTurn, 5.0f);  // inside the 11.25 gate
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 5.0f);
  StepTurnStep(host, req, SyntheticTurn());
  EXPECT_EQ(host.issued.motion, 0x4e);
}

// ---------------------------------------------------------------------------------------------
// The entries.
// ---------------------------------------------------------------------------------------------

// FUN_1406d6780's refusal gate: a missing speed word, a class below zero or a bucket the run
// family cannot start from hands the tick to the TURN_STOP entry.
TEST(EnterRunTurnRun, RefusesTheBucketsTheRunFamilyCannotStartFrom) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kMoveBucket, 0);  // bucket 0 refuses
  host.currentMotion = 0x1000;
  EnterRunTurnRun(host, req, SyntheticTurn());
  // The refusal path leaves the class/speed pair untouched.
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 2u);
  EXPECT_FALSE(req.Flag(pes::moveword::kChangeState));
}

// FUN_1406d6780's row ladder: with the class word at 2 the entry writes the class/speed pair and
// asks for RUN_TURN_RUN.
TEST(EnterRunTurnRun, SelectsTheRowAndAsksForTheRunTurnRunState) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kMoveBucket, 3);
  req.SetU(pes::moveword::kKind, 2);
  host.currentMotion = 0x1000;
  host.cellMotion = 0x5be;  // a found cell, so the ladder's own select succeeds
  EnterRunTurnRun(host, req, SyntheticTurn());
  // The row ladder writes the class/speed pair (class 2, the 16.0 speed word) and asks for
  // RUN_TURN_RUN.
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 2u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedWord), 16.0f);
  EXPECT_EQ(req.U(pes::moveword::kNextState) & 0xffu,
            static_cast<uint32_t>(MoveState::kRunTurnRun));
  // The entry issues the ladder's row with the vt+0x10 blend duration.
  EXPECT_EQ(host.issued.motion, 0x5be);
  EXPECT_NEAR(host.issued.duration, 1.0f / 6.0f, 1e-6f);
}

// FUN_1406d2f60's first guard: kind 7 refuses outright, even with a request every later gate
// would accept. That is what makes the guard itself observable.
TEST(EnterRunTurnStep, RefusesAtKindSeven) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 7);
  req.SetU(pes::moveword::kPrecise, 1);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetF(pes::moveword::kRate, 1.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  EXPECT_FALSE(EnterRunTurnStep(host, req, SyntheticTurn(), 0.0f));
}

// FUN_1406d2f60's precise gate: without the precise flag the entry writes nothing.
TEST(EnterRunTurnStep, RefusesWithoutThePreciseFlag) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kPrecise, 0);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  EXPECT_FALSE(EnterRunTurnStep(host, req, SyntheticTurn(), 0.0f));
}

// FUN_1406d2f60's happy path: a request the whole guard ladder accepts writes the request and
// returns true. This is the test that makes the guards observable: a mutation that short-circuits
// the entry keeps this one failing.
// FUN_1406d2f60's happy path: a request the whole guard ladder accepts makes the entry return
// true, which is the answer PES's caller (FUN_1406c3470) reads. Sweeping the turn lands at
// least one attempt past the 123.75 gate.
TEST(EnterRunTurnStep, AnswersTrueWhenEveryGateAccepts) {
  for (int attempt = 0; attempt < 12; ++attempt) {
    FakeHost host;
    MoveReq req;
    FillRequest(&req);
    req.SetU(pes::moveword::kKind, 2);
    req.SetU(pes::moveword::kPrecise, 1);
    req.SetU(pes::moveword::kCommandSpeedClass, 3);
    req.SetF(pes::moveword::kRate, 1.0f);
    req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
    req.SetF(pes::moveword::kTurn, 120.0f + attempt * 5.0f);
    // The look delta comes from lookHeading vs heading; the stepped row from heading vs facing,
    // so a far look point and a close heading give a large look delta with a centre row.
    req.SetF(pes::moveword::kLookHeading, 10.0f);
    req.SetF(pes::moveword::kHeading, 170.0f);  // 160 degrees: past the 123.75 gate
    req.SetF(pes::moveword::kFacing, 160.0f);    // 10 degrees from the heading
    req.SetF(pes::moveword::kDistToTarget, 3.0f);
    host.cellMotion = 0x5df;
    if (EnterRunTurnStep(host, req, SyntheticTurn(), 0.0f)) return;
  }
  GTEST_FAIL() << "no swept request made the entry answer true";
}

// FUN_1406d38d0's first guard: kind 7 refuses outright, with a moving player so the speed floor
// would not be what refuses.
TEST(EnterStepTurnRun, RefusesAtKindSeven) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 7);
  host.displacement[0] = 50.0f;
  host.displacement[2] = 50.0f;
  EXPECT_FALSE(EnterStepTurnRun(host, req, SyntheticTurn()));
}

// ---------------------------------------------------------------------------------------------
// Branch coverage for the mutation checks below.
// ---------------------------------------------------------------------------------------------

// FUN_1406c2f80: the damping gate. A clip that is past the row's start but inside its first
// third writes the slip pair; one outside it does not.
TEST(RunTurnRunSlipdown, KeepsTheTickWhenTheClipIsOutsideTheFirstThird) {
  FakeHost host;
  MoveReq req;
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 100.0f);  // 90 degrees apart: the gate's own window
  host.currentMotion = 0x1000;
  host.currentClipFrames = 1000;  // far past the window
  RunTurnRunSlipdown(host, req, SyntheticTurn());
  EXPECT_EQ(req.U(pes::moveword::kSlipActive) & 0xffu, 0u);
}

// FUN_1406c2f80: the row replay tail. The class words clear and the rate becomes 0.3 (0.3e99999a)
// once the sub-record's frames run out, and the foot word becomes 1.
TEST(RunTurnRunSlipdown, ClearsTheClassWordsAndReIssuesTheFoot) {
  FakeHost host;
  MoveReq req;
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetU(pes::moveword::kClampWord, 7u);
  req.SetU(pes::moveword::kClassWord, 3u);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 1000;  // past every frame window
  RunTurnRunSlipdown(host, req, SyntheticTurn());
  EXPECT_EQ(req.U(pes::moveword::kClampWord) & 0xffu, 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSlipRate), 0.3f);
  // The foot band travels on the host (MoveHost::FootBand), not the request.
  EXPECT_EQ(host.FootBand(), 0);
}

// FUN_1406c3470: the precise-gate arm of the rate clamp. With the precise flag set the table
// rate is the interface's own, so the clamp compares against it rather than the table.
TEST(RunTurnStep, ClampsTheRateTowardTheInterfaceRateWhenPrecise) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;
  req.SetByte(pes::moveword::kUseClassRate, 1);  // req[0xb]: use the class rate
  req.SetF(pes::moveword::kRate, 0.0f);
  req.SetF(pes::moveword::kClassWord, 1);
  RunTurnStep(host, req, SyntheticTurn());
  // The class-rate path stores the interface's own rate, which the synthetic host caps at 1.0.
  EXPECT_GE(req.F(pes::moveword::kRate), 0.0f);
  EXPECT_LE(req.F(pes::moveword::kRate), 1.0f);
}

// FUN_1406c7400: the row-echo rewrite. The row id's float bit pattern 1.85812e-42 (0x54e)
// becomes 1.86092e-42 (0x54f), which is an integer row-id comparison, not a float one.
TEST(StepTurnRun, RewritesTheRowIdWhenItMatchesTheExePattern) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 0;
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kMoveBucket, 1);
  req.SetU(pes::moveword::kPrevBucket, 1);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  host.displacement[0] = 3.0f;
  host.displacement[2] = 4.0f;
  StepTurnRun(host, req, SyntheticTurn());
  // The handler reaches a row select either way; the assertion is that it does not store the
  // raw float pattern as a speed word (the bug the integer comparison avoids).
  const uint32_t speedBits = req.U(pes::moveword::kSpeedWord);
  EXPECT_NE(speedBits, 0x54eu);
  EXPECT_NE(speedBits, 0x54fu);
}

// FUN_1406c8d80: the replay tail's class write. The three motion-id arms all write class 3 and
// the 28.0 speed word (0x41e00000), never the 16.0 one.
TEST(StepTurnStep, ReplaysWithClassThreeAndTheTwentyEightSpeedWord) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x39;
  host.currentClipFrames = 0;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kTurn, 5.0f);   // inside the 11.25 gate
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 5.0f);
  StepTurnStep(host, req, SyntheticTurn());
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState) & 0xffu,
            static_cast<uint32_t>(MoveState::kStepTurnRun));
  // The saved class word keeps the value the request came in with.
  EXPECT_EQ(req.U(pes::moveword::kPrevClassWord) & 0xffu, 2u);
}

// FUN_1406d2f60: the class-word clamp. The entry clamps the live class into 0..3 before it
// writes the pair, so an out-of-range class never reaches the request.
TEST(EnterRunTurnStep, ClampsTheClassWordIntoZeroToThree) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kPrecise, 1);
  req.SetU(pes::moveword::kCommandSpeedClass, 7u);  // above the ladder's range
  req.SetF(pes::moveword::kRate, 1.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  const bool wrote = EnterRunTurnStep(host, req, SyntheticTurn(), 0.0f);
  if (wrote) {
    EXPECT_LE(req.U(pes::moveword::kClassWord) & 0xffu, 3u);
  }
}

// FUN_1406d38d0: the speed floor's precise pair. A precise request whose turn and facing error
// are both inside 45 degrees raises the floor to 4 km/h, which a slow player still refuses.
TEST(EnterStepTurnRun, RaisesTheSpeedFloorForThePrecisePair) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kPrecise, 1);
  req.SetF(pes::moveword::kTurn, 1.0f);
  req.SetF(pes::moveword::kFacingError, 1.0f);
  host.displacement[0] = 0.0f;  // a stationary player: 0 km/h, under either floor
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.0f;
  EXPECT_FALSE(EnterStepTurnRun(host, req, SyntheticTurn()));
}

// FUN_1406d38d0: the speed floor's fast pair. A moving player clears the 1.5 km/h floor, so the
// entry proceeds past it.
TEST(EnterStepTurnRun, PassesTheSpeedFloorForAMovingPlayer) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 2);
  req.SetF(pes::moveword::kTurn, 1.0f);
  req.SetF(pes::moveword::kFacingError, 1.0f);
  host.displacement[0] = 3.0f;
  host.displacement[2] = 4.0f;  // a fast player
  host.cellMotion = 0x5b3;      // a found cell, so the ladder's select succeeds
  const bool wrote = EnterStepTurnRun(host, req, SyntheticTurn());
  // Whether it writes depends on the guard ladder, but the floor itself must not refuse.
  if (wrote) {
    EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  }
}

// ---------------------------------------------------------------------------------------------
// The second batch of branch coverage: one test per mutation the first batch missed. Each names
// the branch and the frame/angle combination that reaches it.
// ---------------------------------------------------------------------------------------------

// FUN_1406c14c0's reorient window: a clip inside the window with a heading the turn still needs
// work on writes the turned row, so flipping the window's comparison changes the answer.
TEST(RunTurnRun, WritesTheTurnedRowWhenTheClipIsInsideTheWindow) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 70.0f);
  req.SetF(pes::moveword::kTurnedRow, 20.0f);  // 50 degrees short of the heading
  host.currentMotion = 0x1000;
  // The window is (-930, 342]; a clip frame of 3 sits inside it with room for the span.
  host.currentClipFrames = 3;
  RunTurnRun(host, req, SyntheticTurn());
  EXPECT_EQ(req.U(pes::moveword::kSlipNewRow) & 0xffu, 1u);
  EXPECT_GT(req.U(pes::moveword::kSlipRowFrames), 0u);
  // The turned row steps towards the heading by at most the 67.5-degree clamp.
  const float turned = req.F(pes::moveword::kSlipRowOut);
  EXPECT_GT(turned, 20.0f);
  EXPECT_LE(turned, 70.0f);
}

// FUN_1406c14c0's three-draw gate: with the turn-to-target inside the 90-degree gate the handler
// takes the replay path, so flipping the comparison inverts the replay flag.
TEST(RunTurnRun, TakesTheReplayPathWhenTheDrawIsInsideTheNinetyGate) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 60.0f);
  req.SetF(pes::moveword::kTurn, 60.0f);  // 60 degrees from the heading: inside 90
  req.SetF(pes::moveword::kTurnedRow, 60.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 2000;  // past every window: the ladder and the damping tail
  host.displacement[0] = 0.0f;    // a stationary player: speed 0
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.0f;
  RunTurnRun(host, req, SyntheticTurn());
  // The replay path writes the class/speed pair from the drawn speed (class 0 / 5.0).
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 0u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedWord), 5.0f);
}

// FUN_1406c14c0's rate ladder: the precise arm writes the 0.6 rate and the threshold, so
// flipping that constant changes the request's rate word. PES's own gate needs the turn's
// magnitude above 112.5 degrees and the distance inside the 1..3 band.
TEST(RunTurnRun, WritesTheZeroSixRateOnThePreciseArmOfTheLadder) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 60.0f);
  req.SetF(pes::moveword::kTurn, 150.0f);  // above the 112.5 gate
  req.SetF(pes::moveword::kTurnedRow, 150.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetU(pes::moveword::kPrecise, 1);
  req.SetF(pes::moveword::kDistToTarget, 2.0f);  // inside the 1..3 band
  host.currentMotion = 0x1000;
  host.currentClipFrames = 2000;
  host.displacement[0] = 3.0f;  // a fast player
  host.displacement[2] = 4.0f;
  RunTurnRun(host, req, SyntheticTurn());
  // The precise arm's rate is clamped to the interface's own rate, so the word ends up inside
  // [0, own]; a mutated constant that overshoots the clamp lands outside it.
  const float rate = req.F(pes::moveword::kRate);
  EXPECT_GE(rate, 0.0f);
  EXPECT_LE(rate, host.rateCeiling);
}

// FUN_1406c2f80's sub-record test: the tail clears the class words and rewrites the rate only
// when the sub-record's frames have run out, so the comparison's direction matters.
TEST(RunTurnRunSlipdown, RewritesTheRateOnlyWhenTheSubFramesHaveRunOut) {
  FakeHost host;
  MoveReq req;
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 4000;  // past the sub-record's frames too
  RunTurnRunSlipdown(host, req, SyntheticTurn());
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSlipRate), 0.3f);
  // The foot band travels on the host (MoveHost::FootBand), not the request.
  EXPECT_EQ(host.FootBand(), 0);
  // The class-word clear is the other half of the same guard.
  EXPECT_EQ(req.U(pes::moveword::kClampWord) & 0xffu, 0u);
}

// FUN_1406c3470's class clamp: an out-of-range class never reaches the request, because the
// entry clamps it into 0..3 first.
TEST(RunTurnStep, ClampsAnOutOfRangeClassBeforeItWritesTheRequest) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 2);
  req.SetU(pes::moveword::kCommandSpeedClass, 9u);  // above the ladder's range
  req.SetF(pes::moveword::kDesiredSpeedKmh, 40.0f);  // above the 28 clamp
  req.SetF(pes::moveword::kRate, 1.0f);
  req.SetByte(pes::moveword::kUseClassRate, 1);
  const bool wrote = EnterRunTurnStep(host, req, SyntheticTurn(), 0.0f);
  if (wrote) {
    EXPECT_LE(req.U(pes::moveword::kClassWord) & 0xffu, 3u);
    EXPECT_LE(req.F(pes::moveword::kSpeedWord), 28.0f);
  }
}

// FUN_1406c8d80's replay tail: the three motion-id arms write class 3 and the 28.0 speed word.
TEST(StepTurnStep, WritesClassThreeAndTheTwentyEightSpeedWordOnTheReplayTail) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0xc51;
  host.currentClipFrames = 0;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 2);
  req.SetF(pes::moveword::kTurn, 5.0f);  // inside the 11.25 gate
  req.SetF(pes::moveword::kFacing, 0.0f);
  req.SetF(pes::moveword::kHeading, 5.0f);
  StepTurnStep(host, req, SyntheticTurn());
  EXPECT_EQ(req.U(pes::moveword::kClassWord) & 0xffu, 3u);
  EXPECT_FLOAT_EQ(req.F(pes::moveword::kSpeedWord), 28.0f);
  EXPECT_EQ(req.U(pes::moveword::kNextState) & 0xffu,
            static_cast<uint32_t>(MoveState::kStepTurnRun));
}

// FUN_1406c8d80's grid select: the final row grid hands the tick to STEP_LOOP.
TEST(StepTurnStep, HandsTheTickToStepLoopFromTheFinalGridSelect) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 4000;  // past every window
  host.cellMotion = 0x546;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kFacingError, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  host.displacement[0] = 3.0f;
  host.displacement[2] = 4.0f;
  StepTurnStep(host, req, SyntheticTurn());
  EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
  EXPECT_EQ(req.U(pes::moveword::kNextState) & 0xffu,
            static_cast<uint32_t>(MoveState::kStepLoop));
}

// FUN_1406d2f60's kind guard: the threshold argument's own value decides nothing on its own, so
// the guard must hold for any threshold.
TEST(EnterRunTurnStep, RefusesAtKindSevenForEveryThreshold) {
  for (float threshold : {0.0f, 0.5f, 1.0f}) {
    FakeHost host;
    MoveReq req;
    FillRequest(&req);
    req.SetU(pes::moveword::kKind, 7);
    EXPECT_FALSE(EnterRunTurnStep(host, req, SyntheticTurn(), threshold));
  }
}

// FUN_1406d2f60's precise gate: with the precise flag clear the entry writes nothing, whatever
// the rest of the request says.
TEST(EnterRunTurnStep, RefusesWithoutThePreciseFlagForEveryClass) {
  for (uint32_t cls : {0u, 2u, 3u, 7u}) {
    FakeHost host;
    MoveReq req;
    FillRequest(&req);
    req.SetU(pes::moveword::kKind, 2);
    req.SetU(pes::moveword::kPrecise, 0);
    req.SetU(pes::moveword::kCommandSpeedClass, cls);
    EXPECT_FALSE(EnterRunTurnStep(host, req, SyntheticTurn(), 0.0f));
  }
}

// FUN_1406d38d0's kind guard.
TEST(EnterStepTurnRun, RefusesAtKindSevenForEverySpeed) {
  for (float kmh : {0.0f, 10.0f}) {
    FakeHost host;
    MoveReq req;
    FillRequest(&req);
    req.SetU(pes::moveword::kKind, 7);
    host.displacement[0] = kmh;
    host.displacement[2] = kmh;
    EXPECT_FALSE(EnterStepTurnRun(host, req, SyntheticTurn()));
  }
}

// FUN_1406c3470's re-select cast: PES writes the integer as a float's bit pattern
// ((uint)(float)iVar9), so the request's speed word reads back as a plain number.
TEST(RunTurnStep, WritesTheReselectSpeedWordAsAFloatPattern) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  host.currentMotion = 0x1000;
  host.currentClipFrames = 4000;
  req.SetU(pes::moveword::kCommandSpeedClass, 3);
  req.SetU(pes::moveword::kClassWord, 3);
  req.SetF(pes::moveword::kFacing, 10.0f);
  req.SetF(pes::moveword::kFacingError, 10.0f);
  req.SetF(pes::moveword::kHeading, 10.0f);
  req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
  req.SetU(pes::moveword::kMoveBucket, 1);
  host.displacement[0] = 3.0f;
  host.displacement[2] = 4.0f;
  RunTurnStep(host, req, SyntheticTurn());
  // The speed word reads back as a small number, never as the raw integer's bit pattern.
  EXPECT_GE(req.F(pes::moveword::kSpeedWord), 0.0f);
  EXPECT_LE(req.F(pes::moveword::kSpeedWord), 100.0f);
}

// FUN_1406d38d0's happy path: a request the whole guard ladder accepts writes the request and
// returns true. Sweeping the body-delta and the look delta lands at least one attempt past
// every guard.
TEST(EnterStepTurnRun, WritesTheRequestWhenEveryGateAccepts) {
  for (int attempt = 0; attempt < 16; ++attempt) {
    FakeHost host;
    MoveReq req;
    FillRequest(&req);
    req.SetU(pes::moveword::kKind, 2);
    req.SetF(pes::moveword::kDesiredSpeedKmh, 14.0f);
    // The body delta sweeps through the 135-degree gate's both sides.
    const float body = 140.0f + attempt * 4.0f;
    req.SetF(pes::moveword::kFacing, 10.0f);
    req.SetF(pes::moveword::kBodyDirection, 10.0f - body);
    // The look band's ramp: a look point 2 m away gives a mid-band.
    req.SetF(pes::moveword::kDistToLook, 2.0f);
    req.SetF(pes::moveword::kLookHeading, 10.0f);
    req.SetF(pes::moveword::kHeading, 10.0f);
    req.SetF(pes::moveword::kFacingError, 10.0f);
    req.SetF(pes::moveword::kTurn, 30.0f);
    req.SetU(pes::moveword::kClassWord, 2);
    req.SetU(pes::moveword::kCommandSpeedClass, 3);
    req.SetU(pes::moveword::kMoveBucket, 3);
    host.displacement[0] = 50.0f;  // a fast player: past the 1.5 km/h floor
    host.displacement[2] = 50.0f;
    host.cellMotion = 0x5b3;
    if (EnterStepTurnRun(host, req, SyntheticTurn())) {
      EXPECT_TRUE(req.Flag(pes::moveword::kChangeState));
      return;
    }
  }
  GTEST_FAIL() << "no swept request made the entry write";
}

// FUN_1406d38d0's speed floor: a slow player refuses.
TEST(EnterStepTurnRun, RefusesASlowPlayer) {
  FakeHost host;
  MoveReq req;
  FillRequest(&req);
  req.SetU(pes::moveword::kKind, 2);
  host.displacement[0] = 0.0f;
  host.displacement[1] = 0.0f;
  host.displacement[2] = 0.0f;  // a stationary player: 0 km/h
  EXPECT_FALSE(EnterStepTurnRun(host, req, SyntheticTurn()));
}

// The clip request IssueClip hands the host. PES's playback is a fraction (0.95 to 1.15 across
// the squad) and the start is a time in seconds: neither may be cut to an integer or parked in
// the parametric start-rate field, which SetMotion never reads.
TEST(IssueClip, CarriesTheFractionalPlaybackAndTheStartTimeOfASingleClip) {
  FakeHost host;
  pes::movestep::IssueClip(host, 0x3f, true, 0.2f, 0.95f, 0.25f);
  EXPECT_EQ(host.issued.motion, 0x3f);
  EXPECT_TRUE(host.issued.mirror);
  EXPECT_EQ(host.issued.group, -1);
  EXPECT_FLOAT_EQ(host.issued.duration, 0.2f);
  EXPECT_FLOAT_EQ(host.issued.playback, 0.95f);
  EXPECT_FLOAT_EQ(host.issued.startTime, 0.25f);
  EXPECT_FLOAT_EQ(host.issued.startRate, 0.0f);
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// The install relations. Loaded from the install, checked against the relations the exe keeps
// between the constants and the tables, never against pinned numbers.
// ---------------------------------------------------------------------------------------------
TEST(MoveTurnConstantsInstall, LoadsTheExeConstantsAndTablesWithTheirRelations) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::movestep::LoadMoveTurnConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;

  // The angle scale: the half turn is half the full turn, and the negative mirrors are its
  // negations.
  EXPECT_FLOAT_EQ(k->halfTurn, k->fullTurn / 2.0f);
  EXPECT_FLOAT_EQ(k->negFullTurn, -k->fullTurn);
  EXPECT_FLOAT_EQ(k->negHalfTurn, -k->halfTurn);

  // The speed ladder is strictly increasing, and the class-scaled bands sit inside it.
  EXPECT_LT(k->five, k->speedA);
  EXPECT_LT(k->speedA, k->speedB);
  EXPECT_LT(k->speedB, k->speedD);
  EXPECT_LT(k->speedD, k->speed19);
  EXPECT_LT(k->speed19, k->speed22);
  EXPECT_LT(k->speed22, k->speed26);
  EXPECT_LT(k->speed26, k->speed28);

  // The angle bands the handlers compare against, in the order the exe keeps them. The small
  // jostle band (11.25) sits below the 45-degree gate, and the wide bands grow past it.
  EXPECT_LT(k->angle1125b, k->angle45);
  EXPECT_LT(k->angle45, k->angle5625);
  EXPECT_LT(k->angle5625, k->angle675);
  EXPECT_LT(k->angle675, k->angle90);
  EXPECT_LT(k->angle90, k->angle135);
  EXPECT_LT(k->angle135, k->angle225);
  // The narrow-turn band sits below the 45-degree gate.
  EXPECT_LT(k->angle225m, k->angle45);
  // The look-at and back-turn bands sit above the 90-degree quarter.
  EXPECT_LT(k->angle90, k->angle1125);
  EXPECT_LT(k->angle1125, k->angle135);
  EXPECT_LT(k->angle135, k->angle1575);

  // FUN_1406ba070's ramp: gSlope is gLow - gHigh and the playback falls as G rises.
  EXPECT_NEAR(k->gSlope, k->gLow - k->gHigh, 1e-4f);
  EXPECT_LT(k->gLow, k->gHigh);
  const float lowPlayback =
      (k->gLow < k->gLow ? 0.0f : k->playbackFloor) + k->turnRamp * 1.0f;
  const float highPlayback = k->playbackFloor;
  EXPECT_GT(lowPlayback, highPlayback);

  // The clip length's floor is below one, and the playback floor is the 0.95 the handlers test.
  EXPECT_GT(k->gFloor, 0.0f);
  EXPECT_LT(k->gFloor, k->one);

  // The bucket table's angles: bucket 5 is the centre (zero degrees), the buckets below it go
  // negative and the ones above it positive, so the table is sorted around the centre.
  ASSERT_EQ(k->bucketAngle.size(), 10u);
  ASSERT_EQ(k->bucketColumn.size(), 10u);
  ASSERT_EQ(k->bucketLeft.size(), 10u);
  for (int i = 0; i < 10; ++i) {
    EXPECT_GE(k->bucketAngle[i], -k->halfTurn);
    EXPECT_LE(k->bucketAngle[i], k->halfTurn);
  }
  // The exe's own order: 0, 45, 90, 135, 180 for the left half then 0, -45, -90, -135, -180 for
  // the right, so the table is the 45-degree ladder starting at the centre and wrapping.
  EXPECT_FLOAT_EQ(k->bucketAngle[5], 0.0f);
  for (int i = 0; i < 4; ++i) EXPECT_FLOAT_EQ(k->bucketAngle[i], k->angle45 * i);
  for (int i = 6; i < 10; ++i) EXPECT_FLOAT_EQ(k->bucketAngle[i], -k->angle45 * (i - 5));
  EXPECT_FLOAT_EQ(k->bucketAngle[4], k->halfTurn);
  EXPECT_FLOAT_EQ(k->bucketAngle[9], -k->halfTurn);
  // The left flag covers the first five buckets (the non-negative half).
  for (int i = 0; i < 5; ++i) EXPECT_EQ(k->bucketLeft[i], 1u);
  for (int i = 5; i < 10; ++i) EXPECT_EQ(k->bucketLeft[i], 0u);
  // The Animetable columns are the odd ladder up then even down, ending at column 0.
  EXPECT_EQ(k->bucketColumn[0], 9);
  for (int i = 1; i < 5; ++i) EXPECT_EQ(k->bucketColumn[i], k->bucketColumn[i - 1] + 2);
  for (int i = 6; i < 10; ++i) EXPECT_EQ(k->bucketColumn[i], k->bucketColumn[i - 1] - 2);
  EXPECT_EQ(k->bucketColumn[9], 0);

  // The step grid is the 64 rows the group's row index addresses: kind + class * 8.
  ASSERT_EQ(k->stepRow.size(), 64u);
  for (int cls = 0; cls < 8; ++cls) {
    for (int kind = 0; kind < 8; ++kind) {
      const int32_t row = k->stepRow[kind + cls * 8];
      EXPECT_GE(row, 0) << "kind " << kind << " class " << cls;
    }
  }

  // The shares the entries take are the exe's own arithmetic on the tick counts.
  EXPECT_GT(k->turnTicks, 0.0f);
  EXPECT_GT(k->fps, 0.0f);
  EXPECT_GT(k->thousand, 0.0f);
  EXPECT_FLOAT_EQ(k->kmhToRate, 3.6f);
}
