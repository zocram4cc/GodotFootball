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

// PES's ground frame against GF's: PES's z is -coords[1] (referee.cpp:197 in.ballZ_m).
// Every heading the adapter feeds PES goes through here, so the sign lives in this one place.
float HeadingDeg(float x, float z, const pes::RunConstants& k) {
  return pesmovestep::PesBearingDeg(k, x, z);
}

// FUN_1406b7560's fill, from the player's current AI command (docs/re/pes-re-anim-select.md
// 1.1): req[1] is the heading to the move target, req[3] the heading to the look-at point,
// req[0x11]/[0x12] their distances in metres, req[6] the turn (target minus facing) and req[7]
// the facing error to the look-at point. Words the command does not name keep PES's init: kind
// 3 (RUN), bucket 0, precise clear, command id 0. A command with no movement reads as standing
// still at the facing, which is PES's answer for an unset target.
void FillCommandWords(pes::MoveReq& req, const float facingDeg, Player& player,
                      const pes::AngleBucketConstants& buckets) {
  const Anim* anim = player.GetCurrentAnim();
  Vector3 dir(0.0f, 0.0f, 0.0f);
  float speed = 0.0f;
  Vector3 look(0.0f, 0.0f, 0.0f);
  bool hasLook = false;
  if (anim != nullptr) {
    dir = anim->originatingCommand.desiredDirection;
    speed = anim->originatingCommand.desiredVelocityFloat;
    hasLook = anim->originatingCommand.useDesiredLookAt;
    look = anim->originatingCommand.desiredLookAt;
  }
  const Vector3 pos = player.GetPosition();
  const float hasMove = (dir.coords[0] != 0.0f || dir.coords[1] != 0.0f) && speed > 0.0f;
  // The move target a tick ahead: the command's direction at its speed. PES's target sits in
  // the command block; GF's command carries a direction and a speed, so the target is one
  // second along them.
  const float tx = pos.coords[0] + (hasMove ? dir.coords[0] * speed : 0.0f);
  const float tz = pos.coords[1] + (hasMove ? dir.coords[1] * speed : 0.0f);
  const float dx = tx - pos.coords[0];
  const float dz = tz - pos.coords[1];
  const float dist = std::sqrt(dx * dx + dz * dz);
  // FUN_1406b9b20's own constants live in the adapter's bundle; the fill needs only the fold.
  pes::RunConstants fold{};
  fold.one = 1.0f;
  fold.neg = -1.0f;
  fold.half_turn = 180.0f;
  fold.full = 360.0f;
  const float heading = dist > 0.0f
                            ? pesmovestep::PesBearingDeg(fold, dx, dz)
                            : facingDeg;
  req.SetF(pes::moveword::kHeading, heading);
  const float lx = hasLook ? look.coords[0] - pos.coords[0] : dx;
  const float lz = hasLook ? look.coords[1] - pos.coords[1] : dz;
  const float lookDist = std::sqrt(lx * lx + lz * lz);
  const float lookHeading = lookDist > 0.0f
                                ? pesmovestep::PesBearingDeg(fold, lx, lz)
                                : facingDeg;
  req.SetF(pes::moveword::kLookHeading, lookHeading);
  req.SetF(pes::moveword::kBodyDirection, facingDeg);
  req.SetF(pes::moveword::kDistToTarget, dist);
  req.SetF(pes::moveword::kDistToLook, hasLook ? lookDist : dist);
  float turn = heading - facingDeg;
  if (fold.half_turn < turn) turn -= fold.full;
  if (turn < -fold.half_turn) turn += fold.full;
  req.SetF(pes::moveword::kTurn, turn);
  float facingError = lookHeading - facingDeg;
  if (fold.half_turn < facingError) facingError -= fold.full;
  if (facingError < -fold.half_turn) facingError += fold.full;
  req.SetF(pes::moveword::kFacingError, facingError);
  // FUN_1406b9b20(angle, 10): the 10-way buckets of the turn and the facing error, with no
  // current bucket (10 = PES's miss default).
  req.SetU(pes::moveword::kMoveBucket,
           static_cast<uint32_t>(pes::movestep::AngleBucket(buckets, turn, 10, false)));
  req.SetU(pes::moveword::kLookBucket,
           static_cast<uint32_t>(pes::movestep::AngleBucket(buckets, facingError, 10, false)));
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
  // FUN_1406eebb0's store (P+0x245a), per tick: the class of the command's speed at the turn (FUN_14081e340).
  // ponytail: PES stores it at action entry; GF has no action entry to hang it on, so it is stored each tick.
  void StoreSpeedClass(float desiredKmh, float turnDeg) {
    player_.GetMoveStep().speedClass =
        static_cast<uint8_t>(pes::SpeedClassOf(desiredKmh, turnDeg, c_.speedClass));
  }
  void StoreFootBand(float turnDeg) {
    // ponytail: anime vt+0x78 of the +0xab8 interface; GF stands the foot from the turn's
    // sign, which is the side the locomotion plants on.
    player_.GetMoveStep().footBand = static_cast<uint8_t>(turnDeg < 0.0f ? 1 : 2);
  }
  // The heading of the last displacement minus the facing, in degrees (FUN_141eb50b0's index 1). Zero
  // when the player did not move.
  float DisplacementTurnDeg() const {
    const Vector3 move = player_.GetMovement();
    if (move.GetLength() <= 0.0f) return 0.0f;
    return HeadingDeg(move.coords[0], move.coords[1], c_.run) - FacingDeg();
  }
  float RateCeiling() const override { return player_.GetMoveStep().rateCeiling; }
  // The command's desired speed: the AI's last movement command (vt+0x20 index 0). Its velocity is m/s.
  float CommandSpeedKmh() const override {
    return player_.GetCurrentAnim()->originatingCommand.desiredVelocityFloat * 3.6f;
  }
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
  // The handoff (FUN_1406b8340's tail): the issued clip plays on the body. A SetMotion plays its own
  // motion at its playback. A parametric issue plays the blend group's member nearest its rate, at the
  // rate's speed (PES blends the members by rate; GF stands one member in).
  void Issue(const pes::MotionRequest& request) override {
    issued_ = true;
    ++issues_;
    pes::RecordIssue(player_.GetIssuedClip(), request, c_.parametric);
    if (request.group < 0) {
      player_.CastHumanoid()->PlayIssuedClip(request.motion, request.playback, request.mirror);
      return;
    }
    const std::vector<pes::BlendMember> members =
        pes::BlendGroup(c_.blend, c_.parametric.group[request.group]);
    if (members.empty()) return;
    const pes::BlendMember* nearest = &members.front();
    for (const pes::BlendMember& m : members)
      if (std::fabs(m.param - request.rate) < std::fabs(nearest->param - request.rate)) nearest = &m;
    const float playback =
        nearest->param > 0.0f && request.rate > 0.0f ? request.rate / nearest->param : 1.0f;
    player_.CastHumanoid()->PlayIssuedClip(nearest->motionNo, playback, request.mirror);
  }
  bool Issued() const { return issued_; }
  int IssueCount() const { return issues_; }

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

  // movestep.hpp's CurrentMotion: the PES motion the body plays, -1 when the AI's own clip plays.
  int CurrentMotion() const override { return pesmovestep::CurrentMotionOf(player_); }
  int CurrentClipFrames() const override { return player_.GetFrameNum(); }
  int AnimeState() const override {
    // ponytail: vt+0x28 of the +0xab8 interface; GF answers the MOVE action (2) for
    // movement-like functions, else the 0 stub (never the 0x4a replay action).
    switch (player_.GetCurrentFunctionType()) {
      case e_FunctionType_Movement:
      case e_FunctionType_BallControl:
      case e_FunctionType_Trap:
        return 2;
      default:
        return 0;
    }
  }
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
  int issues_ = 0;
};

}  // namespace

namespace pesmovestep {

float PesBearingDeg(const pes::RunConstants& k, float gfX, float gfZ) {
  return pes::FoldFull(std::atan2(gfX, -gfZ) * kDegPerRad, k);  // PES z = -coords[1]
}


// The single-clip word of req[0x37] (FUN_1406b68d0's init): a SetMotion. Any other value is a style index.
constexpr uint32_t kSingleClip = 0x11;

// The request the tail issues from the request block (FUN_1406b8340's tail): a SetMotion when req[0x37]
// is the single-clip word, else the SetParametric of the style req[0x37] names.
pes::MotionRequest DueRequest(const pes::MoveReq& req, int foot) {
  pes::MotionRequest r;
  r.motion = static_cast<int>(req.U(pes::moveword::kMotion));
  r.mirror = req.Flag(pes::moveword::kMirror);
  r.transition = static_cast<int>(req.U(pes::moveword::kTransitionWord));
  r.duration = req.F(pes::moveword::kDuration);
  r.playback = req.F(pes::moveword::kPlayback);
  r.startTime = req.F(pes::moveword::kStartTime);
  const uint32_t param = req.U(pes::moveword::kParamState);
  r.group = param == kSingleClip ? -1 : static_cast<int>(param);
  r.startRate = req.F(pes::moveword::kStartRate);
  r.rate = req.F(pes::moveword::kRate);
  r.foot = foot;
  return r;
}

void ProcessMoveStep(Player& player, Match& match) {
  // MOVESTEP runs only with pes_loop on. Off it leaves the player alone: no FSM tick, no clip, and
  // no playback write (the contact rates of match.cpp keep theirs).
  if (!match.PesLoop()) return;
  const MoveHostConstants* constants = &match.GetPesMoveStepConstants();

  pes::MoveStepRecord& rec = player.GetMoveStep();
  MoveHostAdapter host(player, match, *constants);
  pes::MoveReq req;

  // The request block's copy-in words the adapter owns before the handler runs. PES's own
  // action setter (FUN_1406b7560) fills them from the AI command block; the adapter stands its
  // fields in from the player's current command: the move target is the command's direction
  // scaled by its speed (req[1] heading, req[0x11]/[0x12] distances), the look-at is the
  // command's look-at point when set (req[3]/[0x12]), and the buckets follow FUN_1406b9b20.
  req.SetF(pes::moveword::kFacing, host.FacingDeg());
  const float facing = req.F(pes::moveword::kFacing);
  FillCommandWords(req, facing, player, host.AngleBuckets());
  // FUN_1406eebb0's stores, per tick: the speed class (P+0x245a, FUN_14081e340 of the
  // command's speed at the turn) and the foot band (P+0x245b, anime vt+0x78). GF has no foot
  // sensor; the band stands at the side of the last displacement against the facing.
  host.StoreSpeedClass(host.CommandSpeedKmh(), host.DisplacementTurnDeg());
  host.StoreFootBand(host.DisplacementTurnDeg());
  req.SetU(pes::moveword::kCommandSpeedClass,
           static_cast<uint32_t>(host.CommandSpeedClass()));
  req.SetF(pes::moveword::kDesiredSpeedKmh, host.CurrentSpeedKmh());
  req.SetU(pes::moveword::kLiveKind, static_cast<uint32_t>(rec.state));
  req.SetF(pes::moveword::kPlayback, rec.playbackSpeed);
  req.SetF(pes::moveword::kRate, rec.rate);
  req.SetByte(pes::moveword::kStyle, rec.style);
  req.SetU(pes::moveword::kParamState, kSingleClip);  // PES's init: a single clip (req[0x37] == 0x11)
  // FUN_1406f99f0's store (P+0x377c): the TopRate ceiling at the record's G, once per match tick.
  // ponytail: the Speed byte (0x27) has no GF stat yet; physical_velocity stands in for it.
  pes::StoreRateCeiling(rec, host.MatchTick(),
                        pes::MoveTopRate(match.GetPesStaminaConstants(),
                                         player.GetStat("physical_velocity"), player.GetStamina(),
                                         host.PhysiqueG()));

  const pes::MoveState stateAtEntry = rec.state;
  pes::movestep::RampsBeforeDispatch(host, req);
  const int issuesBefore = host.IssueCount();
  pes::Dispatch(stateAtEntry, host, req);
  // FUN_1406b8340's tail (anime-select 1, step 7): a request the handler left due (req[0x2f]) is issued
  // now, unless the handler already issued this tick.
  if (req.Flag(pes::moveword::kMotionPending) && host.IssueCount() == issuesBefore)
    host.Issue(DueRequest(req, host.FootBand()));
  pes::movestep::ApplyTail(rec, host, req, stateAtEntry);
  rec.rate = req.F(pes::moveword::kRate);
  rec.style = static_cast<uint8_t>(req.U(pes::moveword::kStyle) & 0xffu);
  pes::ApplyTransition(rec, req);
}

int CurrentMotionOf(Player& player) {
  // The PES motion number of the clip the humanoid plays: its specialvar2 when the clip carries a PES
  // family (dodge, tackle or locomotion), -1 when the AI's own clip plays (no PES numbering).
  const Anim* anim = player.GetCurrentAnim();
  if (!anim || !anim->anim) return -1;
  const int family = std::atoi(anim->anim->GetVariable("specialvar1").c_str());
  if (!pes::IsPesClipFamily(family) && !pes::IsPesLocomotionFamily(family)) return -1;
  return std::atoi(anim->anim->GetVariable("specialvar2").c_str());
}

}  // namespace pesmovestep
