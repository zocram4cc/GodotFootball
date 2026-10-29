#include "pesmovestep.hpp"

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

#include "match.hpp"
#include "onthepitch/AIsupport/mentalimage.hpp"
#include "onthepitch/ball.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/player/controller/playercontroller.hpp"
#include "teamAIcontroller.hpp"
#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"
#include "pes/movestep_turn.hpp"

namespace {
// PES's filler fallback distance (DAT_142598578, script-read from the install: 0.2; pes-re-decision-read.md 2 says 0.4).
constexpr float kBallFallbackDistance_m = 0.2f;

// The install constants the MOVESTEP handlers read: the match's own bundle (MatchSetup), read from
// the user's install by the caller. The adapter never reads the environment.
using MoveHostConstants = pes::MoveStepConstants;

constexpr float kDegPerRad = 180.0f / 3.14159265358979323846f;  // a unit conversion, not PES data

// PES's ground frame against GF's: PES's z is -coords[1] (referee.cpp:197 in.ballZ_m).
// Every heading the adapter feeds PES goes through here, so the sign lives in this one place.
float HeadingDeg(float x, float z, const pes::RunConstants& k) {
  return pesmovestep::PesBearingDeg(k, x, z);
}

// PES's cmd[0x17] command classes: FUN_1406b8de0 maps 1..4 to the bands walk, jog, run and dash, and 0 is none.
// ponytail: GF's quantised velocity levels stand in for the AI's writer of cmd[0x17], which is not ported. Read
// against PES's run-entry bands (5, 11, 16 and 28 km/h, docs/re/pes-re-anim-select.md 2.3): the dribble level
// (3.5 m/s, 12.6 km/h) is a jog (2), the walk level (5 m/s, 18 km/h) a run (3), the sprint level (8 m/s) a dash
// (4). GF has no class 1 (walk): nothing in GF walks at 5 km/h.
constexpr int kCommandNone = 0;
constexpr int kCommandJog = 2;
constexpr int kCommandRun = 3;
constexpr int kCommandDash = 4;
constexpr float kKmhPerMps = 3.6f;  // a unit conversion: km/h per m/s

int CommandSpeedClassOf(float velocityMps) {
  switch (FloatToEnumVelocity(velocityMps)) {
    case e_Velocity_Dribble:
      return kCommandJog;
    case e_Velocity_Walk:
      return kCommandRun;
    case e_Velocity_Sprint:
      return kCommandDash;
    default:
      return kCommandNone;
  }
}

// The team-AI reads of the MOVESTEP ramp and the RUN_LOOP jostle (pes::TeamAi). GF has no team records yet,
// so each answer is a stand-in, and each names the PES read it replaces.
class AdapterTeam final : public pes::TeamAi {
 public:
  AdapterTeam(Player& player, const MoveHostConstants& c) : player_(player), c_(c) {}

  // The rank-0 opponent of the own record in the team's last run world (NearestList's NearestOpponents over
  // the RunWorld, pesoffball.hpp). Its slot is the run world's: team·11 + roster index.
  // The rank-0 opponent reads the team AI's S1 row through the run world (NearestOpponents, read live).
  // ponytail: a substitute has no run slot until the next in-play build (RunSlotOf's roster). Upgrade: the writer's lane map.
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
  // ponytail: GF coords, not PES's frame (index 1 is GF depth = -PES z, index 2 the height). ProximityCap reads index 1 as
  // PES z, behind ActionBit27 (inert while that is false). LIVE: JostleStyle reads mirrored coordinates; the fix flips
  // AThrowInIsNotAwarded red, so investigate which is wrong (movestep_run.cpp, JostleStyle).
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
    const Vector3 dir = p->GetRootDirectionVec();  // PES's yaw of that player: his root, not his travel (FacingDeg)
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
  // FUN_1406eebb0's store (P+0x245a) at action entry: the class of the command's speed at the turn (FUN_14081e340).
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
    const PlayerCommand* cmd = Decision();
    return (cmd ? cmd->desiredVelocityFloat : idleVelocity) * kKmhPerMps;
  }
  // FUN_1406b7560's command words, from the AI's decision: the move target (cmd+0x20) and the look
  // point (cmd+0x14) in PES's ground frame (x, z = -GF y), the speed band (cmd[0x17]), the desired speed
  // (cmd[0x172]) and the precise flag (cmd[0x15]). A command with no target point projects its direction one
  // second ahead: a ponytail stand-in (see PlayerCommand::useDesiredTarget). No command: the player stands.
  void FillCommandWords(pes::MoveReq& req) const {
    const Vector3 pos = player_.GetPosition();
    const PlayerCommand* cmd = Decision();
    const float speed = cmd ? cmd->desiredVelocityFloat : idleVelocity;
    const Vector3 dir = cmd ? cmd->desiredDirection : Vector3(0.0f, 0.0f, 0.0f);
    const Vector3 projected(pos.coords[0] + dir.coords[0] * speed,
                            pos.coords[1] + dir.coords[1] * speed, pos.coords[2]);
    const Vector3 aim = cmd && cmd->useDesiredTarget ? cmd->desiredTarget : projected;
    // PES's filler: a move target within 0.2 m of the root steers by the ball's sample 0 (DAT_142598578, 0.2 in the install;
    // pes-re-decision-read.md 2 says 0.4). ponytail: GF's ball Predict(0) stands in for PES's BallInfo sample 0 (FUN_140A6CF70).
    const bool atRoot = cmd && (aim.Get2D() - pos.Get2D()).GetLength() < kBallFallbackDistance_m;
    const Vector3 target = atRoot ? match_.GetBall()->Predict(0) : aim;
    const Vector3 look = cmd && cmd->useDesiredLookAt ? cmd->desiredLookAt : aim;
    pes::movestep::MoveTargetInput in;
    in.originX = pos.coords[0];
    in.originZ = -pos.coords[1];  // PES z = -coords[1] (referee.cpp:197 in.ballZ_m)
    in.facingDeg = FacingDeg();
    in.targetX = target.coords[0];
    in.targetZ = -target.coords[1];
    in.lookX = look.coords[0];
    in.lookZ = -look.coords[1];
    in.lookValid = cmd && cmd->useDesiredLookAt;  // PES FUN_140a6e6b0: the look-at block is valid (the command carries a look point)
    in.speedBand = pes::CommandClassFor(CommandSpeedClassOf(speed), c_.runTables);
    in.desiredKmh = speed * kKmhPerMps;
    in.precise = cmd && cmd->preciseArrival;
    in.commandSpeedClass = CommandSpeedClassOf(speed);
    // The PES command id and move kind (cmd[0] and cmd[0x16]) come from the movement command: see CommandIdFor and MoveKindFor.
    in.commandId = pesmovestep::CommandIdFor(cmd);
    in.moveKind = pesmovestep::MoveKindFor(player_.GetFormationEntry().role == e_PlayerRole_GK);
    in.animeStateNonzero = static_cast<int>(player_.GetMoveStep().state) != 0;
    in.bodyBucket = pes::movestep::BodyBucketFlag(c_.run.one);  // OPEN: the per-player record (loader FUN_141194330, flag write 0x1411944E6); GF feeds PES's default producer value
    pes::movestep::FillMoveTarget(req, in, c_.run, c_.buckets);
  }
  // The request block as FUN_1406b7560 copies it in: the dwell words and stamps the record carried from the last tick,
  // the facing, and the command words of the AI's decision (FillCommandWords).
  pes::MoveReq Request() {
    pes::MoveReq req;
    pes::CarryIn(player_.GetMoveStep(), req);
    req.SetF(pes::moveword::kFacing, FacingDeg());
    FillCommandWords(req);
    return req;
  }
  // The AI's decision: the controller's command at its last think (Think). Null before the first think.
  const PlayerCommand* Decision() const {
    const std::optional<PlayerCommand>& command = player_.GetPesDecision().command;
    return command ? &*command : nullptr;
  }
  // PES's AI think for this player, every tick: the first queued movement command (MovementDecision) becomes the decision
  // MOVESTEP reads on the next tick. ponytail: PES's think cadence is not decoded. The 25-frame gate that pes-re-teamtick.md
  // 5.1 named is computed and never read (raw asm 0x92578f, 0x8bfcf0), so GF thinks every tick. SelectAnim applies a
  // candidate with side effects, so the humanoid's own choice is not read here.
  void Think() {
    // The controller decides from the mental image its process set this tick. The match has none until its first is
    // built, and the humanoid waits the same way (humanoid.cpp:238).
    if (!static_cast<PlayerController*>(player_.GetController())->GetMentalImage()) return;
    PlayerCommandQueue queue;
    player_.RequestCommand(queue);
    PesDecision& decision = player_.GetPesDecision();
    decision.frame = match_.GetPesFrame();
    decision.command = pesmovestep::MovementDecision(queue);
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
    pesmovestep::PesFrame(ball.coords[0], ball.coords[1], ball.coords[2], out);
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
  // FUN_141eb50b0: index 0 and 1 are the displacement; index >= 2 is the history ring, which GF does not keep.
  // ponytail: PES's ring at P+0x2fc is written per tick by a writer not decoded yet; until then every sample is the position.
  void PositionAt(int index, float out[3]) const override {
    const Vector3 v = index < 2 ? player_.GetMovement() : player_.GetPosition();
    pesmovestep::PesFrame(v.coords[0], v.coords[1], v.coords[2], out);
  }
  void OwnPosition(float out[3]) const override {
    const Vector3 v = player_.GetPosition();
    pesmovestep::PesFrame(v.coords[0], v.coords[1], v.coords[2], out);
  }
  int MatchPhase() const override {
    // PES MI+0x116c (pes-re-A5Judge.md): an awarded restart (buffer active) that is prepared or started is set-play ready (3);
    // any other stop is 2, and open play is 1.
    const auto& buf = match_.GetReferee()->GetBuffer();
    const bool ready = buf.ReadyForTake(match_.GetActualTime_ms()) || buf.started;
    return pes::PlayStateFor(match_.IsInPlay(), buf.active, ready);
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
    // The callers scale this by speed3600 into km/h, so it is metres a second. ponytail: PES's band speed is not decoded.
    // Its candidate source is the 11-knot rate curve at 0x1425DE010 (docs/re/pes-re-speed-inputs.md; the band-to-knot map
    // is open). vt+0x60 is clip duration (FUN_1406E7EB0), not a band speed. Every band returns the player's ground speed
    // for the last tick; the physique argument is ignored.
    return player_.GetMovement().GetLength();
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
  const pes::SpeedClassConstants& SpeedClassConstantsView() const override { return c_.speedClass; }
  const pes::IdleRowTables& IdleRowTablesView() const override { return c_.idleRows; }
  float FacingDeg() const override {
    // PES's facing (req[2], P+0x20) is the root yaw a turn clip rotates; GF's directionVec is the travel direction
    // once the body moves, so a sliding turn clip would read as a facing change (humanoidbase.hpp rootDirectionVec).
    const Vector3 dir = player_.GetRootDirectionVec();
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

// PES's frame of a GF point (see pesmovestep.hpp). 0.0f - y keeps +0.0 for a zero y, so atan2 of a stationary displacement is
// 0 degrees and not 180.
void PesFrame(float gfX, float gfY, float gfZ, float out[3]) {
  out[0] = gfX;
  out[1] = gfZ;
  out[2] = 0.0f - gfY;
}

float PesBearingDeg(const pes::RunConstants& k, float gfX, float gfZ) {
  return pes::FoldFull(std::atan2(gfX, -gfZ) * kDegPerRad, k);  // PES z = -coords[1]
}

// PES's command id (cmd[0], req[0x16]) is the AI's action id, and the copy-in stores it as req[0x16] (FUN_1406b7560 line 313).
// GF's AI has no PES action table, so a movement command is PES's MOVE action (2), the id the running trap takes
// (pes-re-transitions.md:330). A ball action rides the pass channel in PES (pes-re-decision-read.md 1), so it has no movement
// id. ponytail: PRESS (0x33), DELAY (0x34) and WAIT_TIMER (0x11) come from GF states that are not ported, so GF gives them 0.
constexpr int kPesCommandMove = 2;
int CommandIdFor(const PlayerCommand* cmd) {
  if (cmd == nullptr || cmd->desiredFunctionType != e_FunctionType_Movement) return 0;
  return kPesCommandMove;
}

// PES's move kind (cmd[0x16], req[0]) selects the anime row block: 0..2 mid, 3 far, 4..6 keeper near, mid and far, 7 keeper
// holding the ball (pes-re-anim-select.md:93). GF's AI writes no kind. A keeper's move takes the near keeper kind and an
// outfield move the mid kind. ponytail: the far kind (3), the keeper's mid and far kinds (5, 6) and the holding kind (7)
// need PES's writer of cmd+0x58, which is not decoded.
constexpr int kPesMoveKindMid = 0;
constexpr int kPesMoveKindKeeperNear = 4;
int MoveKindFor(bool keeper) { return keeper ? kPesMoveKindKeeperNear : kPesMoveKindMid; }


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

pes::MoveReq TickRequest(Player& player, Match& match) {
  MoveHostAdapter host(player, match, match.GetPesMoveStepConstants());
  return host.Request();
}

// PES's AI think for one player, run every tick by Player::Process after the controller's process, which sets the mental
// image the controller decides from.
void Think(Player& player, Match& match) {
  if (!match.PesLoop()) return;
  MoveHostAdapter host(player, match, match.GetPesMoveStepConstants());
  host.Think();
}

void ProcessMoveStep(Player& player, Match& match) {
  // MOVESTEP runs only with pes_loop on. Off it leaves the player alone: no FSM tick, no clip, and
  // no playback write (the contact rates of match.cpp keep theirs).
  if (!match.PesLoop()) return;
  // A held set-piece taker is in his standby action, and every standby but the two kick-off loops answers 1 from its slot 10,
  // which makes the default locomotion driver FUN_1406F0C20 return before any steering (call at 0x1406F0C93;
  // docs/re/pes-re-kicker-standby.md 1-2). The kick-off takers are never held (TeamAIController::pieceHeld).
  if (player.GetTeam()->GetController()->IsPieceTakerHeld(&player)) return;
  // A throw-in walker in his pickup or lift is in SEAMLESS_THROWIN (anime id 29, Update 0x140791EC0, phases 1 and 4), not in a
  // MOVE action, so the MOVESTEP entry does not run while the clip's root warp carries him (the gate below, for this action).
  if (player.GetTeam()->GetController()->IsPieceTakerWalking(&player) && player.GetTeam()->GetController()->IsPickingUp())
    return;
  // PES runs the MOVESTEP entry only in the MOVE, STEP_MOVE, DRIBBLE_STEP_MOVE and DEMO_MOVE actions (anime ids 2, 3, 6, 74):
  // their Update functions FUN_1407824A0, FUN_140789DD0, FUN_1407D1E60 and the thunk at 0x1407347B0 are the entry FUN_1406B8340's
  // only callers (call sites 0x140782552, 0x140789E42, 0x1407D23A2, jump 0x1407347B6). A dodge, tackle, trap or kick owns the body
  // in its own action, so the FSM does not tick while such a clip plays.
  // ponytail: GF has no PES action id for the body (PES's +0xAC8). IsMovementClip stands in for "in MOVE": a Movement function,
  // or a PES locomotion clip the FSM itself issued. Remove when the body carries its action id.
  if (!player.CastHumanoid()->IsMovementClip()) {
    // A clip at its last frame hands the body back. The FSM is frozen while such a clip plays, so a hold (STOP or ACCELERATE)
    // would block the end-of-clip switch (humanoid.cpp PesPoseHeld). ponytail: PES's state after the action is not decoded; IDLE
    // stands in.
    const Anim* clip = player.GetCurrentAnim();
    pes::MoveStepRecord& fsm = player.GetMoveStep();
    if (clip->frameNum >= clip->anim->GetFrameCount() - 1 && pes::HoldsBodyPose(fsm.state))
      fsm.Enter(pes::MoveState::kIdle);
    return;
  }
  const MoveHostConstants* constants = &match.GetPesMoveStepConstants();

  pes::MoveStepRecord& rec = player.GetMoveStep();
  MoveHostAdapter host(player, match, *constants);
  // The request block as FUN_1406b7560 copies it in (Request); the action-entry stores below follow the command words.
  pes::MoveReq req = host.Request();
  // FUN_1406eebb0's stores at action entry, where AnimeState changes (a new action, P+0xac8): the speed class
  // (P+0x245a, FUN_14081e340 of the command's speed at the turn) and the foot band (P+0x245b, anime vt+0x78).
  // GF has no foot sensor; the band stands at the side of the last displacement against the facing.
  const int anime = host.AnimeState();
  if (anime != rec.actionAnime) {
    rec.actionAnime = anime;
    host.StoreSpeedClass(host.CommandSpeedKmh(), host.DisplacementTurnDeg());
    host.StoreFootBand(host.DisplacementTurnDeg());
  }
  req.SetByte(pes::moveword::kLiveKind, rec.liveKind);
  req.SetF(pes::moveword::kPlayback, rec.playbackSpeed);
  req.SetF(pes::moveword::kPlaybackDefault, rec.playbackDefault);
  req.SetF(pes::moveword::kRate, rec.rate);
  req.SetByte(pes::moveword::kStyle, rec.style);
  req.SetU(pes::moveword::kParamState, kSingleClip);  // PES's init: a single clip (req[0x37] == 0x11)
  // FUN_1406f99f0's store (P+0x377c): the TopRate ceiling at the record's G, once per match tick.
  // ponytail: the Speed byte (0x27) = clamp(trunc(base[0x27] * A1..A5), 40, 99), written by FUN_14092AF10
  // (docs/re/pes-re-speed-inputs.md). Its base row has no GF source yet, so physical_velocity stands in for it.
  constexpr float kStatPerUnit = 100.0f;  // GF stores PES's 40..99 as 0.40..0.99
  pes::StoreRateCeiling(rec, host.MatchTick(),
                        pes::MoveTopRate(match.GetPesStaminaConstants(),
                                         player.GetStat("physical_velocity") * kStatPerUnit,
                                         player.GetStamina(), host.PhysiqueG()));

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
  rec.liveKind = static_cast<uint8_t>(req.U(pes::moveword::kLiveKind) & 0xffu);
  pes::CarryOut(rec, req);  // FUN_1406b6a90's copy-back: the words the next tick reads back
  pes::ApplyTransition(rec, req);
}

void RequestStop(Player& player, Match& match) {
  if (!match.PesLoop()) return;
  const MoveHostConstants* constants = &match.GetPesMoveStepConstants();
  pes::MoveStepRecord& rec = player.GetMoveStep();
  MoveHostAdapter host(player, match, *constants);
  pes::MoveReq req = host.Request();
  pes::movestep::EnterStop(host, req);
  pes::CarryOut(rec, req);
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

// MOVESTEP's decision: the first queued command that is not a ball action (pes-re-decision-read.md 1 and 3).
std::optional<PlayerCommand> MovementDecision(const PlayerCommandQueue& queue) {
  for (const PlayerCommand& command : queue) {
    if (!IsBallActionFunction(command.desiredFunctionType)) return command;
  }
  return std::nullopt;
}

}  // namespace pesmovestep
