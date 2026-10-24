// Copyright 2019 Google LLC & Bastiaan Konings
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not
// be used for anything important. i do not offer support, so don't ask. to be used for inspiration
// :)

#ifndef _HPP_TEAM_AICONTROLLER
#define _HPP_TEAM_AICONTROLLER

#include <array>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../data/formations.hpp"
#include "../data/setpiececonfig.hpp"
#include "../gamedefines.hpp"
#include "onthepitch/pesroles.hpp"
#include "pes/formation.hpp"
#include "base/properties.hpp"
#include "pesrandom.hpp"
#include "pesstrategy.hpp"
#include "pesoffball.hpp"
#include "pesgoalkick.hpp"
#include "pesscorerows.hpp"
#include "pesdefshape.hpp"
#include "teaminstructions.hpp"
#include "pesteamconcepts.hpp"
#include "pesteamtick.hpp"
#include "pesrunslots.hpp"
#include "pesmarktable.hpp"
#include "pespress.hpp"

class Match;
class Team;
class PlayerBase;

struct TacticalOpponentInfo {
  Player* player;
  float dangerFactor;
};

bool SortTacticalOpponentInfo(const TacticalOpponentInfo& a, const TacticalOpponentInfo& b);
namespace pes {
// Per-team presser state the press picker (pespress.hpp) reads and the tick
// writes. One owner: TeamAIController. Men are side-local indices 0..10 in
// each side's block (the picker's `g = side*11 + i`). Only the fields the
// picker reads are here; the rest of PES's T+0xa2a0 record lands with the
// writer.
struct TeamPressState {
  // Role-table entry per man (PES T+0xa2a0): kPresserRole marks the presser.
  int role[22] = {};
  // Per-man recent-action stamps (PES T+0xa2cc + k*0x34, FUN_140a40200).
  std::array<PressStampRing, 22> actionRing{};
  // The four press slots, as man indices; -1 when empty (PES T+0xa548).
  int pressSlot[4] = {-1, -1, -1, -1};

  // FUN_140a42940 (action part): store the man's action and stamp it at time `t`.
  // ponytail: the 0x40-byte per-man record it also clears (PES T+0x28c) has no GF
  // reader yet; add it when the writer's tail reads that record.
  void SetAction(int g, int action, uint32_t t) {
    role[g] = action;
    PressStampPush(actionRing[g], action, t);
  }
};
}  // namespace pes

class TeamAIController {
public:
  TeamAIController(Team* team);
  virtual ~TeamAIController();

  void Process();

  Vector3 GetAdaptedFormationPosition(Player* player, bool useDynamicFormationPosition = true);
  void CalculateDynamicRoles();
  void CalculateManMarking();
  // FUN_140623750's mark spot (report 14.3): where a marker stands, goal-side of
  // his opponent.
  Vector3 GetMarkSpot(Player* defender, Player* opp);
  // PES 2021's open-play defensive line (docs/re/pes-re-defensive-shape.md 6,
  // 14.1), rebuilt every tick in Process(): the back line's height in world x,
  // and the metres of length the team covers from it.
  float GetDefensiveLineHeight() const { return defensiveLineHeight; }
  float GetDefensiveTeamLength() const { return defensiveTeamLength; }
  // The presser state the press picker writes and OffBallPort reads.
  pes::TeamPressState& GetPressState() { return pressState; }
  const pes::TeamPressState& GetPressState() const { return pressState; }
  // PES arms the offside trap only on a team instruction (its `T+0xe4` flag,
  // report 14.4). GF has no such instruction: this reads the tactic that stands
  // in for it and defaults to off, PES's live value for both teams.
  bool IsOffsideTrapArmed() const;
  void ApplyOffsideTrap(Vector3& position) const;
  float GetOffsideTrapX() const { return offsideTrapX; }
  void PrepareSetPiece(e_SetPiece setPiece, int takerTeamID = -1);
  SetPieceParams GetSetPieceParams(e_SetPiece setPiece);
  Player* GetPieceTaker() { return taker; }
  // Overrides the taker the set-piece preparation picked by proximity. The
  // shootout needs its own rotation to take the kicks.
  void SetPieceTaker(Player* player) { taker = player; }
  e_SetPiece GetSetPieceType() { return setPieceType; }
  // PES's set-piece takers walk to their spot instead of being placed on it (A5 §4.7). A throw-in's taker walks to the
  // ball, picks it up and carries it to the throw spot, where he lifts it and throws (docs/re/pes-re-throwin-choreography.md
  // 3.1-3.2). A goal kick's keeper is placed on his stand point when the kick is prepared and held there until the ladder
  // releases it (pesgoalkick.hpp, docs/re/pes-re-kicker-standby.md section 4).
  bool IsPieceTakerWalking(const Player* player) const;
  // True while a placed set-piece taker holds his stand point before the release (pes-re-kicker-standby.md section 4).
  bool IsPieceTakerHeld(const Player* player) const { return pieceHeld && player == taker; }
  // True from the lift's end until the restart is over: the taker keeps the root yaw the lift left him with (PES's throw-in loop
  // keeps the lift's yaw), so his clips start from it.
  bool HoldsRoot(const Player* player) const { return pieceLifted && player == taker; }
  // The point a held taker looks at: his SetKick look point when the placement set one (a corner's heading), else the ball.
  Vector3 HeldTakerLookAt(const Vector3& ball) const { return pieceHasLookAt ? pieceLookAt : ball; }
  Vector3 GetPieceWalkTarget() const { return pieceWalkTarget; }
  // Advances the walk from the pickup, through the carry, to the lift. The referee calls it each tick before the release gate.
  void AdvancePieceWalk();
  // True from the pickup until the release: the taker holds the ball.
  bool IsCarryingBall() const { return pieceWalk && pieceCarrying; }
  // True while a pickup or lift clip plays: the taker stands and his root warps onto the predicted ball (the pickup's contact
  // frame) or onto the carry spot (the lift, 3.2 phase 4).
  bool IsPickingUp() const { return pieceWarping; }
  // True when no walk is pending, or the lift has finished and the carrying taker stands on the spot (3.2 phase 5).
  bool PieceTakerArrived() const;
  // Holds the ball in the carrying taker's hand at the release row's key offset while the touch is pending. PES keeps it
  // there until the release (FUN_140792E10 phase 1; pes-re-throwin-choreography.md 4.2, 5.3). Before the take the ball rides
  // at the taker's root (the carry pin in referee.cpp). The referee calls this from the start of the take until the touch.
  void PinBallAtRelease();
  // The receiver a throw-in is aimed at: the nearest mate inside the touchline (the throw's target; PES's ap+0x2bf4/0x2bfc).
  Player* ThrowInTargetMate(Player* thrower) const;
  // PES's goal kick (pesgoalkick.hpp): the ladder releases it (sub_142136F00) and the strike carries the keeper onto the
  // ball. StepGoalKickLadder runs one tick of the armed restart's ladder and returns true on its release.
  bool StepGoalKickLadder();
  // One tick of the strike from the release: the keeper's root moves onto the contact root.
  void AdvanceGoalKickStrike();
  // True from the strike's contact tick: the keeper may kick.
  bool GoalKickInContact() const { return goalKickStrike.contact; }
  // The mate the goal kick is passed to, or null for a kick into space.
  Player* GoalKickReceiver() const { return goalKickReceiver; }
  void CalculateSituation();

  // PES 2021's open-play defensive line, rebuilt every tick (report 6, 14.1).
  void CalculateDefensiveShape();

  void UpdateTactics();

  // PES's S1 score rows of this team's AI (pesscorerows.hpp), in the lane space: its own team at
  // 0..10, the opponents at 11..21. TickScoreRows() runs once per team tick, from Team::Process(),
  // before the in-play gate.
  const pes::ScoreRows& Rows() const { return scoreRows_.Rows(); }
  void TickScoreRows();
  pes::ScoreRowWriter scoreRows_;
  // PES 2021's mark table (team-tick step 35, FUN_1404F61E0, T7a `0x1404F76B1`):
  // rebuild markTableState from the run world each team tick on the PES path.
  void TickMarkTable();

  const Formations::Shape& GetFormationShape() const { return formationShape; }
  // Writes a shape's roles and positions into the team's formation slots. Any
  // band split totalling ten is accepted, however unorthodox.
  void ApplyFormationShape(const Formations::Shape& shape);
  // The shape the team is lined up in, counted off its own roles.
  Formations::Shape TeamFormationShape() const;
  void ApplyFormation(Formations::e_Formation newFormation);

  // Instructions the manager has set from the touchline.
  TeamInstructions::State& GetInstructions() { return instructions; }
  const TeamInstructions::State& GetInstructions() const { return instructions; }

  // The live value of a tactic slider after the whole pipeline - base
  // defaults and user modifiers.
  // Every gameplay read of a tactic belongs here; the raw team data bypasses
  // everything the manager is doing.
  float GetLiveTacticReal(const char* name, float defaultValue) const {
    return liveTeamTactics.GetReal(name, defaultValue);
  }
  // The whole live tactic set, for state read-back (remote control panel,
  // debug overlays). Values only mutate on this thread's UpdateTactics.
  const Properties& GetLiveTactics() const { return liveTeamTactics; }

  // PES's team sheet (docs/re/pes-re-tactics.md 1): the sliders and toggles
  // the AI reads, from the team's tactics with the install's sheet defaults.
  const pes::TeamSheet& GetTeamSheet() const { return teamSheet; }
  // PES's role arrays (pesroles.hpp): B holds each slot's role class, the value every class
  // reader uses; A is the stamina-sub class. Setup and the tick's head and tail write them.
  int RoleClassOfSlot(int slot) const;
  // The defensive line of a player: the reclassify of his slot's B class on the PES path (pesmentality.hpp).
  int DefensiveLineOf(Player* p, e_PlayerRole role) const;
  // The lane line's word and A-rank of a slot (pesmentality.hpp); read by the lane tests.
  int LaneWordOfSlot(int slot) const { return roles.a[slot]; }
  uint8_t LaneRankOfSlot(int slot) const { return roles.aRank[slot]; }
  // PES's designation publisher (FUN_14051E350, team tick step 27b): the side's record, rebuilt each tick.
  void UpdatePesDesignation();
  void SetUpRoleArrays();
  void WriteRoleArrays(bool head);
  void UpdateLaneLine();
  void WriteSlotPoints(int phase);
  // The B swap pair (T+0x89D4/0x89D8). No GF event sets it yet: the PES writer is not decoded.
  void RequestRoleSwap(int a, int b);
  // A team concept's order for one man (T+0xb748/b774/26c, transitions 2-3):
  // the concept id and the point it sends him to. Null when he has none.
  struct ConceptOrder {
    int conceptId;
    Vector3 target;
  };
  const ConceptOrder* GetConceptOrder(const Player* player) const;
  // PES's PRESS state, the block-radius input (defender decisions): while the
  // team's ForeCheckPress runs, its MATCH_UP man presses.
  bool IsPressing(const Player* player) const;
  // PES's presser (T+0xa548 slot 0). The writer FUN_140536fc0 is not ported,
  // so while the ForeCheckPress concept runs the team's pressure man is its
  // designated possession player - the same stand-in IsPressing uses.
  // ponytail: replace with the role table once the writer lands.
  Player* GetTeamPressurePlayer();
  // PES 2021's loose-ball PRESS writer (docs/re/pes-re-press-life.md 3/10,
  // FUN_140536fc0's loose-ball subset + FUN_140536690's head reset): picks the
  // side's presser and renews his PRESS 0x34, resetting the rest to 0. Runs
  // every team tick on the PES path, before the possession latch.
  void PesPressTick();
  // The loose-ball writer and the second presser's coordinator (FUN_140634470 with its maintain). The
  // coordinator runs first, then the writer, in the same tick (the FUN_140535C60 order). The writer keeps
  // the second presser (PressWriterWord).
  void PesLooseWriterTick();
  void PesSecondPresserTick();
  // The role table's PRESS man (T+0xa2a0 == 0x34), or null. The loose-ball
  // presser; the carrier-press stand-in above is untouched.
  Player* GetLoosePressMan();
  // True for a man who holds PRESS (0x34) in the role table. Every such man of the side takes the loose-ball approach.
  bool IsLoosePresser(const PlayerBase* p) const;
  // The first-touch flag pressOrDelay (pesreceive): the team's pressure man, or any PRESS man of the side (PES loop).
  bool PressOrDelay(const PlayerBase* p);
  // PES's strategy word T+0x134 (docs/re/pes-re-match-management.md 1).
  int GetStrategy() const { return strategy.strategy; }
  // Every team tick: the decider moves the strategy in PES state 3 (restart armed or taken), and the
  // attack level follows it once play resumes.
  void UpdateStrategy();
  // The decider body (FUN_14051E970): one StrategyStep where DeciderMovesIn holds (the caller's
  // S1 extract: the PES path runs this before the drift so the tick reads decider → copy → drift).
  void StepStrategyDecider(int playState);
  // PES's in-play plan selector (FUN_140500B90): the sheet plan, once per PES team tick after UpdateStrategy.
  void UpdatePesSheetPlan();
  // PES's per-tick team words (pesteamtick.hpp): the attack level T+0xB8A4,
  // its flags and the restart word, stepped every team tick on the PES path.
  const pes::TeamTick& GetTeamTick() const { return teamTick; }

  // PES's off-ball run or designated-supporter target for this player, if any
  // (pes-re-off-ball.md 1, 4.1).
  bool GetOffBallTarget(const Player* player, Vector3* target) const;
  int GetRunState(const Player* player) const;
  // The last run world (ProcessRunManager, on the team's parity tick) is the ranking MOVESTEP's team seam
  // reads. A player's slot is team·11 + his index in that build's roster; -1 when he is not in it.
  int RunSlotOf(const Player* player) const;
  // The own-roster index (0..10) of `player` in the run table; -1 when he is not one of this team's slots.
  int OwnRunSlot(const Player* player) const;
  Player* RunPlayerAt(int slot) const;
  // The rank-0 opponent of `own` in that world (NearestOpponents, pesoffball.hpp); nullptr when none.
  Player* NearestOpponentOf(const Player* own) const;
  // The distance from `own` to his rank-0 opponent in that world; 1000 m when he has none.
  float NearestOpponentDistance(const Player* own) const;
  // PES 2021's mark table (team-tick step 35, FUN_1404F61E0, T+0xB7EC[11]):
  // whether `player`'s own-roster slot is marked this tick. Readers R1/R2/R3
  // (docs/re/pes-re-mark-readers.md) gate the duel predicates on it.
  bool IsMarkedByTable(const Player* player) const;


  const std::vector<TacticalOpponentInfo>& GetTacticalOpponentInfo() {
    return tacticalOpponentInfo;
  }

  void Reset();

protected:
  Match* match;
  Team* team;
  Player* taker;
  bool pieceWalk = false;      // a set-piece walk is pending (PES path only): a throw-in taker's or a goal-kick keeper's
  bool pieceCarrying = false;  // a throw-in taker has the ball: pieceWalkTarget is the carry spot
  int pieceReleaseRow = 0;     // the carry's release row, chosen as the take starts (0: not yet)
  bool pieceWarping = false;   // the pickup clip is playing: the root warps onto pieceWarpTo until the contact frame
  unsigned long pieceWarpTicks = 0;  // ticks left to the pickup's contact frame
  Vector3 pieceWarpTo;         // the predicted ball at the contact frame, where the warp ends
  bool pieceLifted = false;    // the lift clip has finished: the taker stands on the spot and the release may come (3.2 phase 5)
  bool pieceHeld = false;      // the taker was placed on his stand point at prepare and holds it until the release
  bool pieceHasLookAt = false;  // the placement set pieceLookAt (a corner's SetKick heading)
  Vector3 pieceLookAt;          // where the held taker looks while he waits, when pieceHasLookAt
  Vector3 pieceWalkTarget;     // the walk's target: a throw-in's pickup point, then its carry spot; a keeper's ball spot
  Vector3 pieceCarrySpot;      // where the taker lifts the ball (3.2)
  e_SetPiece setPieceType;
  // The goal kick's ladder and strike (PES path). The strike's root is in GF metres, from the release to the contact.
  struct GoalKickStrike {
    bool started = false;       // released: the strike owns the keeper's root
    bool contact = false;       // the contact tick has passed: the keeper may kick
    unsigned long ticks = 0;    // the contact tick, counted from the release
    unsigned long elapsed = 0;  // ticks since the release
    Vector3 from;               // the keeper's root at the release
    Vector3 contactRoot;        // the root on the clip's contact
  };
  GoalKickStrike goalKickStrike;
  pes::GoalKickLadder goalKickLadder;
  Player* goalKickReceiver = nullptr;  // the mate the ladder passes to; null for a kick into space
  // Places the goal kick's stand point (SetKick's placement, pesgoalkick.hpp) and the strike's contact from the clip. Returns
  // the keeper's stand point; the caller places him there.
  Vector3 PrepareGoalKickStand(const Vector3& spot, Player* taker);
  // SetKick's placement for a corner (pesgoalkick.hpp): the stand point for the restart spot, and the look point the taker
  // faces (the corner heading). The caller places him there and holds him.
  Vector3 PrepareCornerStand(const Vector3& spot, Player* taker, Vector3* lookAt);
  // The lofted picker's mates: the taker's team's active players, rated by technical highpass. Returns the index the
  // picker names (-1 for none) and fills `mates` with the list that index refers to.
  int PickGoalKickMate(std::vector<Player*>* mates) const;

  Properties baseTeamTactics;
  Properties teamTacticsModMultipliers;
  Properties liveTeamTactics;

  Formations::Shape formationShape;
  TeamInstructions::State instructions;
  pes::TeamSheet teamSheet;
  pes::roles::SideRoles roles;
  // Block B's fold picks of this tick (pesmentality.hpp FoldPicks), PES path only. Nothing reads them yet:
  // the readers of PES's planObj+0x7CD4 are not decoded.
  std::vector<int> foldSlots;
  std::optional<std::pair<int, int>> roleSwap;
  // PES's slot-point rows P+0x3A9C (FUN_1404F5F10): each slot's sheet triple at the tick's phase.
  std::array<pes::formation::Point3, pes::formation::kSlots> slotPoints{};
  // PES's concept selector state (FUN_140579840): running flag and start time
  // per id, the men each concept has ordered, and its own record.
  std::array<bool, pes::kConceptCount> conceptRunning{};
  std::array<unsigned long, pes::kConceptCount> conceptStart_ms{};
  std::map<const Player*, ConceptOrder> conceptOrders;
  Player* chaseChaser = nullptr;
  Player* chaseSecond = nullptr;
  Player* chaseCarrier = nullptr;
  Player* counterRunner = nullptr;
  std::array<Player*, 3> counterSupporters{};
  // MI+0x11a0 (frames since the possessing team changed) and FUN_140a74ec0
  // (in-play time since a kick-off restart), in GF time.
  unsigned long possessionChange_ms = 0;
  bool lastRestartWasKickOff = false;
  unsigned long inPlaySinceRestart_ms = 0;
  unsigned long lastProcess_ms = 0;
  // FUN_140687820's measured back/midfield lines (T+0x89a0/a4, world x) and the
  // bands they are compared with (T+0x875c/8770, own frame), this tick.
  float measuredBackLine = 0, measuredMidLine = 0, backBand = 0, midBand = 0;
  // PES 2021's CPU strategy decider (pesstrategy.hpp): its state and its own
  // generator.
  pes::StrategyState strategy;
  std::optional<pes::Random> strategyRng;
  // PES's per-tick team words (pesteamtick.hpp), stepped in UpdateStrategy on
  // the PES path. Last tick's latch for the restart word's flip test.
  pes::TeamTick teamTick;
  // PES's lane copy flag LX+0x1B0 (lane-writers 2): 1 (set) recomputes the A words; a stoppage reset sets it to 0 for one
  // pass (the copy path), and that pass sets it back to 1.
  int laneCopyFlag = 1;
  bool lastTickPossession = false;
  // PES 2021's mark table (team-tick step 35, FUN_1404F61E0): T+0xB7D8's slot
  // word plus T+0xB7EC[11], rebuilt each team tick on the PES path.
  pes::marktable::MarkState markTableState;

  float offensivenessBias;

  bool teamHasPossession;
  bool teamHasUniquePossession;
  bool oppTeamHasPossession;
  bool oppTeamHasUniquePossession;
  bool teamHasBestPossession;
  float teamPossessionAmount;
  float fadingTeamPossessionAmount;
  int timeNeededToGetToBall;
  int oppTimeNeededToGetToBall;

  float depth;
  float width;

  float offsideTrapX;

  // PES 2021's open-play defensive shape (docs/re/pes-re-defensive-shape.md 6, 7,
  // 14.1): the pitch facts the chain reads, the line height it produces this
  // tick and the length the team covers, all in world terms.
  pes::ShapeField defensiveShapeField;
  float defensiveLineHeight;
  float defensiveTeamLength;
  pes::TeamPressState pressState;
  // The coordinator state (PP and B88C): the holder, the excluded man, the second presser, the reason words.
  pes::PressCoordState coordState;
  // The PES slider readings the shape uses this tick (report 14.6), mapped from
  // GF's live tactics; the band code reads them so the line and its width come
  // from one place.
  int defensiveStyle;
  int defensiveAttackLevel;
  float defensiveLineSlider;
  float defensiveCompactSlider;
  // PES's Pressuring option (team-sheet option byte 6, report 14.3): the marker
  // keeps 2 m more room.
  bool defensivePressing;
  // Last tick's own-frame line height, PES's f7a00 input (`prevFinal`).
  float previousLineHeight;
  // PES's per-slot mark state (report 3.3): the opponent a defender is switching
  // to and the tick that switch started pending, so a hand-off commits only
  // after the organiser's delay.
  std::vector<int> markPendingOpponent;
  std::vector<unsigned long> markPendingSinceTick;

  bool prevTeamHasPossession;

  void RunConcepts();
  void StopConcept(int id);
  bool StartConcept(int id);
  bool ConceptEnded(int id);
  void ExecuteConcept(int id);
  pes::TransitionChaseInput ChaseInput(Player* carrier, std::vector<Player*>* men);
  pes::SideCounterInput CounterInput(std::vector<Player*>* men);
  pes::DefenceConceptInput DefenceInput();
  float MeasuredLine(int line, float ref) const;

  // The run manager (pesoffball.hpp) keeps each own slot's run in runSlots; state 0 is no run. A slot leaves
  // a run through pes::run::Release (FUN_140A40820), so its ring records the stop.
  void ProcessOffBall();
  void ProcessRunManager(Player* carrier);
  pes::offball::RunWorld BuildRunWorld(std::array<std::vector<Player*>, 2>* ids) const;
  void ProcessDesignatedSupporter(Player* carrier);
  // The men FUN_14051B8B0 places in the box, in placement order (the PES path of the corner box).
  std::vector<Player*> PesBoxMen(int boxCount);
  pes::designation::DesignationRecord pesDesignation{};
  // PES's per-slot run table of this team (pesrunslots.hpp): the state, the history ring and the point of each slot.
  std::array<pes::run::Slot, pes::run::kSlots> runSlots{};
  long lastRunFrame = -1;
  Player* designatedSupporter = nullptr;
  Vector3 supporterTarget;
  // The last run world and the roster its slots index; empty until the first build.
  pes::offball::RunWorld lastRunWorld;
  std::array<std::vector<Player*>, 2> lastRunIds;

  std::vector<TacticalOpponentInfo> tacticalOpponentInfo;
};

#endif
