#include "pesmovestep.hpp"

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

#include "match.hpp"
#include "onthepitch/AIsupport/mentalimage.hpp"
#include "onthepitch/ball.hpp"
#include "onthepitch/player/player.hpp"
#include "teamAIcontroller.hpp"
#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_turn.hpp"

namespace {

// The install constants the MOVESTEP handlers read: the match's own bundle (MatchSetup), read from
// the user's install by the caller. The adapter never reads the environment.
using MoveHostConstants = pes::MoveStepConstants;

constexpr float kDegPerRad = 180.0f / 3.14159265358979323846f;  // a unit conversion, not PES data

float HeadingDeg(float x, float z, const pes::RunConstants& k) {
  return pes::FoldFull(std::atan2(x, z) * kDegPerRad, k);
}

// The team-AI reads of the MOVESTEP ramp and the RUN_LOOP jostle (pes::TeamAi). GF has no team records yet,
// so each answer is a stand-in, and each names the PES read it replaces.
class AdapterTeam final : public pes::TeamAi {
 public:
  AdapterTeam(Player& player, const MoveHostConstants& c) : player_(player), c_(c) {}

  // The rank-0 opponent of the own record in the team's last run world (NearestList's NearestOpponents over
  // the RunWorld, pesoffball.hpp). Its slot is the run world's: team·11 + roster index.
  // ponytail: the run world is built on the team's parity tick, so the ranking is at most one frame old, and
  // a substitute has no slot until the next in-play build. Upgrade: PES's writer of MA+0x22e4c (pes-re-teamctx.md §2.4).
  int NearestOpponent(int own) const override {
    if (own != OwnSlot()) return kNoPlayer;
    const Player* opp = Controller().NearestOpponentOf(&player_);
    const int slot = opp ? Controller().RunSlotOf(opp) : -1;
    return slot < 0 ? kNoPlayer : slot;
  }
  // The own record's slot: team·11 + roster index in the last run world; kNoPlayer when it is not in it.
  int OwnSlot() const {
    const int slot = Controller().RunSlotOf(&player_);
    return slot < 0 ? kNoPlayer : slot;
  }
  // ponytail: GF's physical_acceleration stands in for the Acceleration byte (0x2b), scaled to the PES
  // ceiling (rec99). The DA (0x15) and PC (0x28) bytes have no GF stat yet and read 0.
  int Ability(int slot, int ability) const override {
    if (Who(slot) != &player_ || ability != pes::kAbilityAcceleration) return 0;
    return static_cast<int>(player_.GetStat("physical_acceleration") * c_.run.rec99);
  }
  // ponytail: FUN_140a581c0's condition byte: GF has no condition record, so the full value stands in.
  int Condition(int) const override { return kFullCondition; }
  void Position(int slot, float out[3]) const override {
    const Player* p = Who(slot);
    const Vector3 v = p ? p->GetPosition() : Vector3{};
    out[0] = v.coords[0];
    out[1] = v.coords[1];
    out[2] = v.coords[2];
  }
  // The per-tick velocity: GF's movement (metres a second) over the tick rate.
  void Velocity(int slot, float out[3]) const override {
    const Player* p = Who(slot);
    const Vector3 v = p ? p->GetMovement() : Vector3{};
    const float ticks = static_cast<float>(Match::kFramesPerSecond);
    out[0] = v.coords[0] / ticks;
    out[1] = v.coords[1] / ticks;
    out[2] = v.coords[2] / ticks;
  }
  float Heading(int slot) const override {
    const Player* p = Who(slot);
    if (!p) return 0.0f;
    const Vector3 dir = p->GetDirectionVec();
    return HeadingDeg(dir.coords[0], dir.coords[1], c_.run);
  }
  // ponytail: P+0x5d0, the player's sub-state byte, has no GF record yet.
  int SubState(int) const override { return 0; }
  // ponytail: hu(opp)+8 bit 27 is the action dispatcher's record (A-CLS), not decoded yet.
  bool ActionBit27(int) const override { return false; }
  // ponytail: no team records yet. Each answers "no" until the PES read it names is ported: the side keeper
  // (FUN_140a62690), the side's tackle (T+0xb6e0), the possession latch (T+0x255), the carrier (FUN_140a627e0),
  // the side slot 0x78 (T+0x78), the route row (FUN_140479470), the match-up man (FUN_140a62960), the queued
  // cover (T+0x278), the marked slot (T+0xb8c4) and the set-piece answer (FUN_140a62d90).
  bool IsSideKeeper(int) const override { return false; }
  bool SideTackled(int) const override { return false; }
  bool PossessionLatched(int) const override { return false; }
  bool Carries(int) const override { return false; }
  bool IsSideSlot78(int) const override { return false; }
  bool HasRouteRow(int) const override { return false; }
  bool IsMatchUpMan(int) const override { return false; }
  bool IsQueuedCover(int) const override { return false; }
  bool SideSlotMarked(int) const override { return false; }
  bool SetPieceAnswer(int) const override { return false; }

 private:
  static constexpr int kNoPlayer = 22;        // a slot of 22 or more is no player
  static constexpr int kFullCondition = 100;  // the condition byte's full value
  Player& player_;
  const MoveHostConstants& c_;
  // The team controller this player belongs to: its last run world is the seam's ranking and roster.
  TeamAIController& Controller() const { return *player_.GetTeam()->GetController(); }
  // The player a slot names: the own record, or the run world's player in that slot; nullptr when none.
  Player* Who(int slot) const {
    if (slot >= kNoPlayer) return nullptr;
    return slot == OwnSlot() ? &player_ : Controller().RunPlayerAt(slot);
  }
};

// The MoveHost implementation over GF's player and its humanoid. Every accessor names the PES
// read it stands for; the `ponytail:` notes name the site that replaces a GF stand-in.
class MoveHostAdapter final : public pes::MoveHost {
 public:
  MoveHostAdapter(Player& player, Match& match, const MoveHostConstants& c)
      : player_(player), match_(match), c_(c), team_(player, c) {}

  const pes::TeamAi& Team() const override { return team_; }

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
    return pes::movestep::MotionStartSeconds(c_.motionStart, static_cast<uint16_t>(motionNo));
  }
  const pes::StyleConstants& StyleConstantsView() const override { return c_.style; }
  // vt+0x20 of the +0xab8 interface: the phase byte P+0x245c, the FSM's current state.
  int CurrentState() const override { return static_cast<int>(player_.GetMoveStep().state); }
  const pes::DampingConstants& DampingConstantsView() const override { return c_.damping; }
  const pes::DampingRecord& DampingRecordView() const override { return c_.dampingRecord; }
  // ponytail: FUN_142348f80 and FUN_140a73e20 read the match's scripted-slot object (match+0x1a8);
  // GF has no scripted slot yet, so the slot is never ours and the gate answers "not open play".
  bool ScriptedSlotIsOurs() const override { return false; }
  int StaminaPercent() const override {
    // FUN_140a58550: the tank's percentage, cur * 100 / max (docs/re/pes-re-stamina-drain.md).
    const pes::StaminaState& s = player_.GetStamina();
    return s.max == 0 ? 0 : s.cur * 100 / s.max;
  }
  // FUN_1406e7380: P+0x3b18 * 180 is the height in cm.
  // ponytail: P+0x3b18 has no GF read; GF's physical_height (m) stands in.
  float PlayerHeightCm() const override { return player_.GetStat("physical_height") * 100.0f; }
  void BallSample(int index, float out[3]) const override {
    // FUN_140a6cf70: index 0 is the ball now; index k is its prediction k ticks ahead.
    // ponytail: the PES ball list is the match's per-tick ring; GF reads the mental image's
    // prediction at the same time.
    const Vector3 ball =
        index == 0 ? match_.GetBall()->GetPositionBuffer()
                   : match_.GetMentalImage(0)->GetBallPrediction(
                         static_cast<unsigned int>(index * 1000 / Match::kFramesPerSecond));
    out[0] = ball.coords[0];
    out[1] = ball.coords[1];
    out[2] = ball.coords[2];
  }
  // ctx+0x2944: ponytail: GF's ball movement heading stands in for the PES ball heading field.
  float BallMotionHeadingDeg() const override {
    const Vector3 move = match_.GetBall()->GetMovement();
    return HeadingDeg(move.coords[0], move.coords[1], c_.run);
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
    pes::RecordIssue(player_.GetIssuedClip(), request, c_.parametric);
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
  // The motion table the match loaded from the install (Mbinfo's lists, see pes/mbinfo.hpp).
  const pes::MotionRecord& Motion(int motionNo) const override {
    return match_.GetPesMbinfo().Motion(motionNo);
  }
  // ponytail: P+0x2472, the anime interface's flag byte (vt+0x30); its writer is not decoded yet.
  bool ClipFlag() const override { return false; }

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
  int ActionIndex() const override { return team_.OwnSlot(); }
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
  AdapterTeam team_;
  bool issued_ = false;
};

}  // namespace

namespace pesmovestep {

void ProcessMoveStep(Player& player, Match& match) {
  // MOVESTEP runs only with pes_loop on. Off it leaves the player alone: no FSM tick, no clip, and
  // no playback write (the contact rates of match.cpp keep theirs).
  if (!match.PesLoop()) return;
  const MoveHostConstants* constants = &match.GetPesMoveStepConstants();

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
  req.SetF(pes::moveword::kRate, rec.rate);
  req.SetByte(pes::moveword::kStyle, rec.style);

  const pes::MoveState stateAtEntry = rec.state;
  pes::movestep::RampsBeforeDispatch(host, req);
  pes::Dispatch(stateAtEntry, host, req);
  pes::movestep::ApplyTail(rec, host, req, stateAtEntry);
  rec.rate = req.F(pes::moveword::kRate);
  rec.style = static_cast<uint8_t>(req.U(pes::moveword::kStyle) & 0xffu);
  pes::ApplyTransition(rec, req);
}
int CurrentMotionOf(Player& player) {
  const Anim* anim = player.GetCurrentAnim();  // main's behaviour: the GF anim id
  return anim ? anim->id : 0;
}

}  // namespace pesmovestep
