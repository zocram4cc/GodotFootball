#include "pesmovestep.hpp"

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

#include "match.hpp"
#include "onthepitch/ball.hpp"
#include "onthepitch/player/player.hpp"
#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_turn.hpp"

namespace {

// The install constants the MOVESTEP handlers read, loaded once per process from the install.
// The group test targets pass their own synthetic sets; the production adapter reads the exe.
struct MoveHostConstants {
  pes::RunConstants run{};
  pes::RunTables runTables{};
  pes::StepScalars stepScalars{};
  pes::StepTables stepTables{};
  pes::PlaybackConstants playback{};
  pes::SpeedRateTable speedRate{};
  pes::MoveConstants move{};
  pes::IdleRowTables idleRows{};
  pes::AngleBucketConstants buckets{};
  pes::movestep::MoveTurnConstants turn{};
};

constexpr float kDegPerRad = 180.0f / 3.14159265358979323846f;  // a unit conversion, not PES data

std::optional<MoveHostConstants> LoadMoveHostConstants() {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return std::nullopt;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) return std::nullopt;

  MoveHostConstants c;
  if (auto k = pes::LoadRunConstants(*exe, &err)) c.run = *k; else return std::nullopt;
  if (auto t = pes::LoadRunTables(*exe, &err)) c.runTables = *t; else return std::nullopt;
  if (auto s = pes::LoadStepScalars(*exe, &err)) c.stepScalars = *s; else return std::nullopt;
  if (auto t = pes::LoadStepTables(*exe, &err)) c.stepTables = *t; else return std::nullopt;
  if (auto p = pes::LoadPlaybackConstants(*exe, &err)) c.playback = *p; else return std::nullopt;
  if (auto t = pes::LoadSpeedRateTable(*exe, &err)) c.speedRate = *t; else return std::nullopt;
  if (auto m = pes::LoadMoveConstants(*exe, &err)) c.move = *m; else return std::nullopt;
  if (auto t = pes::LoadIdleRowTables(*exe, &err)) c.idleRows = *t; else return std::nullopt;
  if (auto b = pes::LoadAngleBucketConstants(*exe, &err)) c.buckets = *b; else return std::nullopt;
  if (auto t = pes::movestep::LoadMoveTurnConstants(*exe, &err)) c.turn = *t; else return std::nullopt;
  return c;
}

const MoveHostConstants* Constants() {
  static const std::optional<MoveHostConstants> kConstants = LoadMoveHostConstants();
  return kConstants.has_value() ? &*kConstants : nullptr;
}

float HeadingDeg(float x, float z, const pes::RunConstants& k) {
  return pes::FoldFull(std::atan2(x, z) * kDegPerRad, k);
}

// The MoveHost implementation over GF's player and its humanoid. Every accessor names the PES
// read it stands for; the `ponytail:` notes name the site that replaces a GF stand-in.
class MoveHostAdapter final : public pes::MoveHost {
 public:
  MoveHostAdapter(Player& player, Match& match, const MoveHostConstants& c)
      : player_(player), match_(match), c_(c) {}

  pes::MoveCell Cell(int row, int column, bool walkFlag) override {
    (void)walkFlag;  // FUN_1406abb30's populated-neighbour walk is the Animetable's own picker.
    const pes::AnimetableCell cell = match_.GetPesAnimetable().Cell(row, column);
    pes::MoveCell out;
    out.motionNo = cell.motionNo;
    out.mirror = cell.mirror;
    out.found = cell.motionNo >= 0;
    return out;
  }
  int PickN(int n) override {
    if (n <= 1) return 0;
    return player_.GetPesRandom().RangeRaw(2, n);  // PES slot 2: the player's own generator.
  }
  bool Percent(int pct) override { return player_.GetPesRandom().RangeRaw(2, 100) < pct; }
  int CommandSpeedClass() const override { return player_.GetMoveStep().speedClass; }
  float RateCeiling() const override { return player_.GetMoveStep().rateCeiling; }
  float CommandSpeedKmh() const override { return CurrentSpeedKmh(); }
  float CurrentSpeedKmh() const override {
    return player_.GetMovement().GetLength() * c_.run.speed3600;
  }
  float PhysiqueG() const override { return 1.0f; }  // ponytail: FUN_141af3180 of P+0xa08.
  float TicksPerSecond() const override { return static_cast<float>(Match::kFramesPerSecond); }
  int MatchTick() const override { return match_.GetPesFrame(); }
  int RecentStamp(int index) const override {
    (void)index;
    return match_.GetPesFrame();  // ponytail: P+0x2814 / P+0x2816's own recent-tick stamps.
  }
  float MotionStartTime(int motionNo) const override {
    return pes::movestep::MotionStartSeconds(static_cast<uint16_t>(motionNo));
  }
  float RecordF2468() const override { return 1.0f; }  // ponytail: P+0x2468's record reader.
  int MoveStatePrevious() const override {
    return static_cast<int>(player_.GetMoveStep().previous);
  }
  float RecordF0bec() const override { return player_.GetMoveStep().yawLock; }
  void SetRecordF0bec(float value) override { player_.GetMoveStep().yawLock = value; }
  const pes::AngleBucketConstants& AngleBuckets() const override { return c_.buckets; }
  void PositionAt(int index, float out[3]) const override {
    const Vector3 v = index == 1 ? player_.GetMovement() : player_.GetPosition();
    out[0] = v.coords[0];
    out[1] = v.coords[1];
    out[2] = v.coords[2];
  }
  int MatchPhase() const override {
    // PES MI+0x116c is not ported (MB-E). Open play is PES's phase 1; every stopped phase is
    // the adapter's "not 1" until the phase block lands.
    return match_.IsInPlay() ? 1 : 0;
  }
  bool IsReplay() const override { return false; }
  void Issue(const pes::MotionRequest& request) override {
    issued_ = true;
    if (request.group < 0) player_.SetPlaybackRate(request.playback);
  }
  bool Issued() const { return issued_; }

  const pes::StepScalars& StepConstants() const override { return c_.stepScalars; }
  float BandSpeedKmh(int band, float g) const override {
    (void)band;
    (void)g;
    return CurrentSpeedKmh();  // ponytail: the anime info record's band speed (vt+0x60).
  }
  bool MirrorChoice(bool sameTeam) const override {
    (void)sameTeam;
    return false;  // ponytail: FUN_140a62960's side choice.
  }
  int FootBand() const override { return player_.GetMoveStep().footBand; }
  const pes::RunConstants& RunConstantsView() const override { return c_.run; }
  const pes::RunTables& RunTablesView() const override { return c_.runTables; }
  const pes::SpeedRateTable& SpeedRateTableView() const override { return c_.speedRate; }
  const pes::movestep::MoveTurnConstants& MoveTurnConstantsView() const override { return c_.turn; }
  const pes::StepTables& StepTablesView() const override { return c_.stepTables; }
  bool BucketIsLeft(int bucket) const override {
    return 0 <= bucket && bucket < static_cast<int>(c_.turn.bucketLeft.size()) &&
           c_.turn.bucketLeft[bucket] != 0;
  }
  const pes::PlaybackConstants& PlaybackConstantsView() const override { return c_.playback; }

  int CurrentMotion() const override {
    const Anim* anim = player_.GetCurrentAnim();
    return anim ? anim->id : 0;
  }
  int CurrentClipFrames() const override { return player_.GetFrameNum(); }
  int AnimeState() const override { return 0; }  // ponytail: vt+0x28 of the +0xab8 interface.
  int MotionFixedFrames(int) const override { return player_.GetFrameCount(); }
  int MotionStartFrames(int motionNo) const override {
    return static_cast<int>(MotionStartTime(motionNo) * TicksPerSecond());
  }
  int MotionTotalFrames(int) const override { return player_.GetFrameCount(); }
  int MotionSubFrames(int, int) const override { return player_.GetFrameCount(); }
  int MotionSubCount(int) const override { return 0; }  // ponytail: FUN_1406afbc0's HitData count.

  const pes::MoveConstants& MoveConstantsView() const override { return c_.move; }
  const pes::IdleRowTables& IdleRowTablesView() const override { return c_.idleRows; }
  float FacingDeg() const override {
    const Vector3 dir = player_.GetDirectionVec();
    return HeadingDeg(dir.coords[0], dir.coords[1], c_.run);
  }
  float BallBearingDeg() const override {
    const Vector3 ball = match_.GetBall()->GetPositionBuffer();
    const Vector3 pos = player_.GetPosition();
    return HeadingDeg(ball.coords[0] - pos.coords[0], ball.coords[1] - pos.coords[1], c_.run);
  }
  int ActionIndex() const override { return player_.GetID(); }
  int RelatedSlot() const override { return -1; }  // ponytail: FUN_140a627e0's related record.
  void RecordPosition(int slot, float out[3]) const override {
    (void)slot;
    out[0] = out[1] = out[2] = 0.0f;  // ponytail: the match's per-player record positions.
  }
  float RecordFacing(int slot) const override {
    (void)slot;
    return 0.0f;  // ponytail: the match's per-player record facings.
  }
  int MatchState11d4() const override { return 0; }  // ponytail: MI+0x11d4.
  int MatchState1554() const override { return 0; }  // ponytail: MI+0x1554.
  int PhaseTick() const override { return match_.GetPesFrame(); }
  int MatchModeByte() const override { return 0; }  // ponytail: MI+0x1775's mode byte.
  bool MatchObject3b0() const override { return false; }  // ponytail: the match object at +0x3b0.
  bool TrainerFlag1423e7fd0() const override { return false; }  // ponytail: DAT_1423e7fd0.
  int RecordConditionByte(int slot) const override {
    (void)slot;
    return 100;  // ponytail: FUN_140a58550's condition byte.
  }
  bool ActionSlotMarked(int slot) const override {
    (void)slot;
    return true;  // ponytail: the per-player scripted-slot marks.
  }
  bool ActionTableOwns(int slot) const override {
    (void)slot;
    return false;  // ponytail: FUN_140a638c0's action-table ownership.
  }
  int RecordActionA() const override { return 5; }  // ponytail: P+0x5d8 of the record.
  int RecordActionB() const override { return 5; }  // ponytail: P+0x5dc of the record.

 private:
  Player& player_;
  Match& match_;
  const MoveHostConstants& c_;
  bool issued_ = false;
};

}  // namespace

namespace pesmovestep {

void ProcessMoveStep(Player& player, Match& match) {
  const MoveHostConstants* constants = Constants();
  if (constants == nullptr) return;

  pes::MoveStepRecord& rec = player.GetMoveStep();
  MoveHostAdapter host(player, match, *constants);
  pes::MoveReq req;

  // The request block's copy-in words the adapter owns before the handler runs. PES's own
  // action setter fills the command block; the adapter stands its fields in from the player.
  req.SetF(pes::moveword::kFacing, host.FacingDeg());
  const float facing = req.F(pes::moveword::kFacing);
  req.SetF(pes::moveword::kHeading, facing);
  req.SetF(pes::moveword::kLookHeading, facing);
  req.SetF(pes::moveword::kBodyDirection, facing);
  req.SetF(pes::moveword::kTurn, 0.0f);
  req.SetF(pes::moveword::kFacingError, 0.0f);
  req.SetU(pes::moveword::kCommandSpeedClass,
           static_cast<uint32_t>(host.CommandSpeedClass()));
  req.SetF(pes::moveword::kDesiredSpeedKmh, host.CurrentSpeedKmh());
  req.SetU(pes::moveword::kLiveKind, static_cast<uint32_t>(rec.state));
  req.SetF(pes::moveword::kPlayback, rec.playbackSpeed);

  const pes::MoveState stateAtEntry = rec.state;
  pes::Dispatch(stateAtEntry, host, req);
  pes::movestep::ApplyTail(rec, host, req, stateAtEntry);
  pes::ApplyTransition(rec, req);
  // P+0x3758's store lands on GF's playback rate for the tick.
  player.SetPlaybackRate(rec.playbackSpeed);
}

}  // namespace pesmovestep
