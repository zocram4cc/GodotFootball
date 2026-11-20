// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not
// be used for anything important. i do not offer support, so don't ask. to be used for inspiration
// :)

#include "team.hpp"

#include "../data/formstate.hpp"
#include "../gamedefines.hpp"
#include "simcontext.hpp"
#include "../utils.hpp"
#include "AIsupport/AIfunctions.hpp"
#include "match.hpp"
#include "offsiderule.hpp"
#include "pespossession.hpp"
#include "pes/movestep.hpp"
#include "pesstamina.hpp"
#include "player/humanoid/pesplayernode.hpp"
#include "referee.hpp"

namespace {

// FUN_140931c60's MA+0x24840 scan: the loop runs over the first three slots
// of the team's ball-distance list (`uVar14 < 3`).
constexpr unsigned int kPassGetNearestSlots = 3;

bool IsKickFunction(e_FunctionType f) {
  return f == e_FunctionType_ShortPass || f == e_FunctionType_LongPass ||
         f == e_FunctionType_HighPass || f == e_FunctionType_Shot;
}

// PES's action kind of a touch made by a player running this function (pesevents.hpp).
uint32_t PesActionKindOf(e_FunctionType f) {
  switch (f) {
    case e_FunctionType_ShortPass:
    case e_FunctionType_LongPass: return pes::kActionKick;
    case e_FunctionType_HighPass: return pes::kActionHighKick;
    case e_FunctionType_Shot: return pes::kActionShot;
    default: return pes::kActionTouch;
  }
}

float HeadingDeg(const Vector3& v) { return pes::HeadingDeg(v.coords[0], v.coords[1]); }

}  // namespace

Team::Team(int id, Match* match, TeamData* teamData) : id(id), match(match), teamData(teamData) {
  assert(id == 0 || id == 1);
  assert(teamData->GetPlayerNum() >= playerNum);  // does team have enough players?

  teamController = std::make_unique<TeamAIController>(this);

  timeNeededToGetToBall_ms = 100;
  hasPossession = false;
  pesPossession = false;
  pesLatchFrame = -1;

  teamPossessionAmount = 1.0;
  fadingTeamPossessionAmount = 1.0;

  for (unsigned int i = 0; i < e_TouchType_SIZE; i++) {
    lastTouchPlayers[i] = 0;
  }
  lastTouchPlayer = 0;
  lastTouchType = e_TouchType_None;
}

Team::~Team() {}

void Team::Exit() {
  players.clear();

  teamController.reset();

  playerNode->Exit();
  playerNode.reset();
}

void Team::InitPlayers() {
  // first, build 1 instance of a player

  playerNode = BuildPlayerNode(match->GetRig());

  activePlayerCount = playerNum;

  Log(e_Notice, "Team", "Team", "Creating players");

  // PES's condition arrows (docs/re/pes-re-condition-import.md): the install's Player.bin record, overridden by
  // the edit save, deals the condition byte; FUN_140AF2DA0's band ladder and FUN_141FC1380's inverse map deal the
  // arrow; FUN_14092AF10's shift applies at stat time. No kick-off RNG: match prep is deterministic given the
  // record. There is no pre-match "Condition: Home / Away" option in PES - the old match_condition_home/away/seed
  // config is gone with the random draw it fed.
  // ponytail: the edit-save override (EDIT00000000 per-id records) lands with the save import; the level-class-6
  // gate and T+0xA298 state 3 arm with the team-setup import (docs/re/pes-re-team-status.md). Until then both
  // sides carry the stock default (class 5, mode 4), so the shift below is inert.
  // ponytail: T+0xA29C (the zero flag) is always false in GF until the setup record import lands
  // (docs/re/pes-re-team-status.md 5), so the reader is teamLevelRaw alone.
  // The shift's /100 == 6 test reads the side being shifted (FUN_14092AF10 0x14092B516 R12D^1):
  // this team's word feeds both legs - the own leg ignores it (no level gate on the state-3
  // leg), the opponent leg tests it as the shifted side. Never the other side's word.
  const bool ownLevelClassSix = match->GetTeamLevelRaw(id) / 100 == 6;
  const bool oppLevelClassSix = ownLevelClassSix;
  int arrowsDealt[(int)FormState::Arrow::Count] = {0, 0, 0, 0, 0};

  // load all players in the team, even the players who sit on the bench. aww.
  for (int i = 0; i < static_cast<int>(teamData->GetPlayerNum()); i++) {
    PlayerData* playerData = teamData->GetPlayerData(i);
    auto playerPtr = std::make_unique<Player>(this, playerData);
    Player* player = playerPtr.get();
    const FormState::Arrow arrow = FormState::ApplyTeamShift(
        FormState::ArrowForConditionByte(match->ConditionByteOf(playerData)),
        /*ownTeam=*/id == 0, /*teamModeThree=*/false, ownLevelClassSix, oppLevelClassSix);
    player->SetConditionArrow(arrow);
    // PES's stamina tank at kick-off (docs/re/pes-re-stamina-drain.md 2): the
    // Stamina ability with PES's tiered scaling, the condition arrow the same
    // draw just dealt, the match length as F, and the full 100% freshness a
    // fresh player starts a match with. The tank, the starting value and the
    // breath reserve are all set here.
    {
      // PES's Stamina ability (40..99) is GF's physical_stamina on 0..1,
      // converted by the report's own range (ponytail: the range is the
      // roster's span, not a transcribed table).
      const int staminaAbility = static_cast<int>(
          std::lround(playerData->GetStat("physical_stamina") * (99.0f - 40.0f) + 40.0f));
      // F is the match length in minutes (report 2: F = the match-length
      // setting, live 10.0 under a 10-minute setting). This engine's
      // durationMinutes plays the same role.
      // The inline getters call the constants too, so the player holds a
      // copy of them alongside his state.
      player->SetStaminaConstants(match->GetPesStaminaConstants());
      pes::InitStamina(match->GetPesStaminaConstants(), staminaAbility, static_cast<int>(arrow),
                       0, static_cast<float>(match->GetMatchDurationMinutes()),
                       100, &player->GetStamina());
      player->GetStamina().breath =
          pes::BreathCap(match->GetPesStaminaConstants(), player->GetStamina());
    }
    arrowsDealt[(int)arrow]++;
    // Add the player to 'players' before Activate(), since Activate() (via
    // Player::GetFormationEntry() -> Team::GetFormationEntry(playerID)) looks
    // the player up by scanning this same vector.
    players.push_back(std::move(playerPtr));
    // Every squad member starts with his own formation entry as his dynamic one, the bench
    // included: the set-piece suitability reads it for players who never activate.
    player->SetDynamicFormationEntry(player->GetFormationEntry());

    if (i < activePlayerCount) {
      // activate playerCount players (the starting eleven, usually)
      player->Activate(playerNode, match->GetAnimCollection());
    }
  }
  Log(e_Notice, "Team", "InitPlayers",
      std::string("condition arrows (PES): purple ") + int_to_str(arrowsDealt[0]) + ", blue " +
          int_to_str(arrowsDealt[1]) + ", green " + int_to_str(arrowsDealt[2]) + ", orange " +
          int_to_str(arrowsDealt[3]) + ", red " + int_to_str(arrowsDealt[4]));

  designatedTeamPossessionPlayer = players.at(0).get();
  // The registry and the first bulk write of the PES role arrays (setup, FUN_141FBB06A).
  teamController->SetUpRoleArrays();
}

void Team::GetBenchPlayers(std::vector<Player*>& benchPlayers) {
  for (auto& player : players) {
    if (!player->IsActive() && !HasBeenSubstituted(player->GetID()))
      benchPlayers.push_back(player.get());
  }
}

bool Team::HasBeenSubstituted(int playerID) const {
  for (int id : substitutedPlayerIDs) {
    if (id == playerID)
      return true;
  }
  return false;
}

Substitutions::SquadView Team::DescribeSwap(Player* playerOut, Player* playerIn) const {
  Substitutions::SquadView squad;
  if (!playerOut || !playerIn)
    return squad;

  squad.playerOutIsOnPitch = playerOut->IsActive();
  squad.playerOutIsSentOff = playerOut->GetCards() > 1;
  squad.playerInIsOnBench = !playerIn->IsActive();
  squad.playerInHasPlayed = HasBeenSubstituted(playerIn->GetID());
  return squad;
}

bool Team::Substitute(Player* playerOut, Player* playerIn) {
  if (!playerOut || !playerIn || playerOut == playerIn)
    return false;
  // The shootout holds pointers to the taker and the keeper; taking either off
  // mid-shootout would pull the pitch out from under it.
  if (match->GetMatchPhase() == e_MatchPhase_Penalties)
    return false;

  if (!playerOut->IsActive() || playerIn->IsActive())
    return false;
  if (HasBeenSubstituted(playerIn->GetID()))
    return false;
  // A dismissed player cannot come back on (match.cpp's auto-sub pool excludes him the same way).
  if (playerIn->GetCards() > 1)
    return false;

  int indexOut = -1;
  int indexIn = -1;
  for (int i = 0; i < static_cast<int>(players.size()); i++) {
    if (players.at(i).get() == playerOut)
      indexOut = i;
    if (players.at(i).get() == playerIn)
      indexIn = i;
  }
  if (indexOut == -1 || indexIn == -1)
    return false;

  const Vector3 replacedPosition = playerOut->GetPosition();

  playerOut->Deactivate();
  substitutedPlayerIDs.push_back(playerOut->GetID());

  // The incoming player inherits the formation slot, which is addressed by
  // position in this vector, so the two players swap places in it.
  std::swap(players.at(indexOut), players.at(indexIn));

  playerIn->Activate(playerNode, match->GetAnimCollection());
  playerIn->ResetPosition(replacedPosition, Vector3(0));

  // Nobody may be left pointing at the player who just walked off.
  if (designatedTeamPossessionPlayer == playerOut)
    designatedTeamPossessionPlayer = playerIn;
  if (passTarget == playerOut) passTarget = playerIn;
  // The last touch passes to the substitute with the other references above: left on the man who
  // walked off, a celebration aims at a player off the pitch who stands where the substitute now stands.
  for (Player*& touched : lastTouchPlayers)
    if (touched == playerOut) touched = playerIn;
  if (lastTouchPlayer == playerOut) lastTouchPlayer = playerIn;

  // Everything that cached the outgoing player has to let go of him: the match's
  // designated possession player and the mental images the AI reads.
  match->ReplacePlayerReferences(playerOut, playerIn);
  return true;
}

signed int Team::GetSide() {
  signed int side;
  if (id == 0)
    side = -1;
  else
    side = 1;

  // -1 == left, 1 == right
  e_MatchPhase phase = match->GetMatchPhase();
  if (phase == e_MatchPhase_2ndHalf || phase == e_MatchPhase_2ndExtraTime)
    side *= -1;

  return side;
}

Player* Team::GetPlayer(int player_id) {
  for (auto& player : players) {
    if (player->GetID() == player_id) {
      return player.get();
    }
  }

  // id not found
  return nullptr;
}

PlayerData* Team::GetPlayerData(int playerID) {
  for (int i = 0; i < static_cast<int>(players.size()); i++) {
    if (players.at(i)->GetID() == playerID) {
      return teamData->GetPlayerData(i);
    }
  }

  assert(1 == 2);
  return nullptr;
}

FormationEntry Team::GetFormationEntry(int playerID) {
  for (int i = 0; i < static_cast<int>(players.size()); i++) {
    if (players.at(i)->GetID() == playerID) {
      return teamData->GetFormationEntry(i);
    }
  }

  assert(1 == 2);
  FormationEntry fail;
  return fail;
}

int Team::FormationSlotOf(const Player* player) const {
  for (size_t i = 0; i < players.size(); ++i)
    if (players[i].get() == player) return static_cast<int>(i);
  return -1;
}

void Team::SetFormationEntry(int playerID, FormationEntry entry) {
  for (int i = 0; i < (signed int)players.size(); i++) {
    if (players.at(i)->GetID() == playerID) {
      teamData->SetFormationEntry(i, entry);
    }
  }
}

void Team::GetActivePlayers(std::vector<Player*>& activePlayers) {
  for (auto& player : players) {
    if (player->IsActive())
      activePlayers.push_back(player.get());
  }
}

bool Team::HasPossession() const {
  return hasPossession;
}

bool Team::HasUniquePossession() const {
  return HasPossession() && !match->GetTeam(abs(id - 1))->HasPossession();
}

int Team::GetTimeNeededToGetToBall_ms() const {
  return timeNeededToGetToBall_ms;
}

signed int Team::GetBestPossessionPlayerID() {
  return GetBestPossessionPlayer()->GetID();
}

Player* Team::GetBestPossessionPlayer() {
  // PES 2021's ball-list reader FUN_14045e0d0 (docs/re/pes-re-offside.md 4):
  // a player offside-flagged in category 0 or 1 gets the impossible score, so
  // he is never the one who goes and gets the ball. This picker is that list -
  // the team's men sorted by how soon they reach the ball - and its consumers
  // (the designated possession player, the match's possession side) all read
  // from here.
  // ponytail: PES lists category 2 as a chaser too (a tight call the referee
  // holds), and only excludes the categories the ball list does; the key-player
  // reader FUN_14050f580 also excludes 2 and is not ported.
  Player* best = nullptr;
  Player* bestUnflagged = nullptr;
  int bestTime_ms = 10000000;
  int bestUnflaggedTime_ms = 10000000;
  for (unsigned int i = 0; i < players.size(); i++) {
    Player* active = players.at(i).get();
    if (!active->IsActive()) continue;
    const int time_ms = active->GetTimeNeededToGetToBall_ms();
    if (time_ms < bestTime_ms) {
      bestTime_ms = time_ms;
      best = active;
    }
    // A flag only keeps a man off the list, it never takes the list away from
    // everyone: with nobody left to send, the closest man goes anyway (PES's
    // list would read 0xff and the caller would take whatever it found).
    if (OffsideRule::OffsideBlocksChase(match->GetReferee()->GetOffsideCategory(active))) continue;
    if (time_ms < bestUnflaggedTime_ms) {
      bestUnflaggedTime_ms = time_ms;
      bestUnflagged = active;
    }
  }

  assert(best);
  return bestUnflagged ? bestUnflagged : best;
}

float Team::GetTeamPossessionAmount() const {
  return teamPossessionAmount;
}

float Team::GetFadingTeamPossessionAmount() const {
  return fadingTeamPossessionAmount;
}

void Team::SetFadingTeamPossessionAmount(float value) {
  fadingTeamPossessionAmount = clamp(value, 0.5, 1.5);
}

void Team::SetLastTouchPlayer(Player* player, e_TouchType touchType) {
  lastTouchPlayers[touchType] = player;
  lastTouchPlayer = player;
  lastTouchType = touchType;
  player->SetLastTouchTime_ms(match->GetActualTime_ms());
  player->SetLastTouchType(lastTouchType);
  match->SetLastTouchTeamID(GetID(), touchType);
  match->QueuePesTouch(player, PesActionKindOf(player->GetCurrentFunctionType()));
}

Player* Team::GetPassTarget() const {
  if (!match->PesLoop()) return passTarget;
  return match->GetPesSlotPlayer(match->PesWorldOf().PassTarget(id));
}

// Team tick step 40c: PES's pass-target selector 0x1404FB2A0 (-> 0x1404FB380 in play, 0x1404FB780 in a stoppage, the holder tree
// 0x1404F0E50) on the world window. The selector reads records GF carries as state, staged here at PES's offsets:
//   MI+0x1748 + slot: the on-pitch byte of each man (docs/re/pes-re-holder-tree.md; captured 1 for the 22 on the pitch);
//   T+0x255: the possession latch of both sides (Team::UpdatePesPossession);
//   MA+0x2601C: the ball owner (captured 0xFF from the kick until the receiver's touch, seq732/seq850);
//   B sample 0 and the man records' positions: the ball and the men now;
//   man+0x5D0: the AI action word, 5 (PASS_GET) for the man the mover sends to the ball, else the captured idle word.
// ponytail: the owner is GF's ball retainer, not PES's W1 holder (0x14045725A: the holder persists on a loose ball in GF's port of it,
// Match::GetPesHolder, while the captured owner is 0xFF between the kick and the touch). Replace with W1's own write when it is proven
// against the captured owner. ponytail: the action word's writer FUN_140A6CA10 is not ported (pesmovestep.cpp:113): only PASS_GET is
// staged, and F0E50's other class and need reads (the slot records +0x1430, the anime class byte, the roster) are GF's stand-ins of the
// run world; the oracle test (tests/pes_teampass_oracle_test.cpp) proves the chain on PES's own records, not on these.
// The arrival-order lists MA+0x24840 / MA+0x24908 (the holder tree's default candidate and its rank scan) are PES's own writer 0x14045E0D0
// (translated, 12/12 real calls bit-exact) run on the side's AI object in the window (PesWorld::RunArrivalList), over the men's records
// staged here and by the team tick.
// ponytail: PES runs the writer from the AI tick 0x140457070 only when the frames since the newest event are 1 or 3 or the ball list ran
// (gate at 0x14045756A..0x140457602); GF has no AI tick object, so it runs every team tick. Replace with the gate once 0x14045B090 and
// 0x140A512E0 are driven from the tick. The record fields the writer reads that GF does not carry (counter A SR+0x1430, SR+0x12BC, the roles
// SR+0x1A/+0x1C, MA+0x247D4's bearings, MA+0x253C4's masks, T+0xB8B4, B+0x18) are zero.
// ponytail: counter B (SR+0x1438), the base of a resting ball's arrival time, is the AI record writer FUN_14092ED30's estimate in frames
// (21..500 captured, pes-re-player-record.md 2.1); GF's time to the ball stands in until that writer runs.
void Team::RunPesPassTarget() {
  pes::PesWorld& world = match->PesWorldOf();
  uint8_t* mi = world.MatchInfo();
  Player* const retainer = match->GetBallRetainer();
  for (int t = 0; t < pes::kSideCount; ++t) {
    Team* side = match->GetTeam(t);
    const auto& squad = side->GetAllPlayers();
    Player* const receiver = match->IsInPlay() ? side->PassGetReceiver() : nullptr;
    for (int place = 0; place < pes::kSlotsPerSide; ++place) {
      const bool onPitch = place < static_cast<int>(squad.size()) && squad[place]->IsActive();
      const int slot = t * pes::kSlotsPerSide + place;
      mi[pes::kMiOnPitchOffset + slot] = onPitch ? 1 : 0;
      if (onPitch)
        pes::StoreU32(world.ManRecord(slot), pes::kManActionOffset,
                      squad[place].get() == receiver ? pes::kActionPassGet : pes::kManActionIdle);
      if (onPitch)
        pes::StoreU32(world.SlotRecord(slot), pes::kSlotCounterBOffset,
                      static_cast<uint32_t>(static_cast<float>(squad[place]->GetTimeNeededToGetToBall_ms()) * pes::kPesTickHz / 1000.0f));
    }
    world.SetMaBallOwner(t, retainer != nullptr ? static_cast<uint32_t>(match->PesSlotOf(retainer)) : pes::kMiNoSlot);
    world.SideRecord(t)[pes::kSideLatchOffset] = side->IsPesPossession() ? 1 : 0;
    world.SetMaOwner(t, retainer != nullptr ? static_cast<uint32_t>(match->PesSlotOf(retainer)) : pes::kMiNoSlot);
  }
  const Vector3 ball = match->GetBall()->Predict(0);
  world.SetBallSample(0, ball.coords[0], ball.coords[2], ball.coords[1]);
  // B+0x2948: the ball's velocity (m/s, PES's axes: x along the pitch, y the height, z across), which the holder tree reads (FUN_140A57720)
  // to see a ball that has stopped. ponytail: the prediction samples 1..249 (FUN_140A6CF70) stay zero until the ball port writes them.
  const Vector3 ballMove = match->GetBall()->GetMovement();
  world.SetBallVelocity(ballMove.coords[0], ballMove.coords[2], ballMove.coords[1]);
  world.SetMaBall(id, ball.coords[0], ball.coords[2], ball.coords[1]);
  world.RunArrivalList(id);
  const RefereeBuffer& buffer = match->GetRefereeBuffer();
  world.RunPassTarget(id, static_cast<uint32_t>(pes::PlayStateFor(match->IsInPlay(), buffer.active,
                                                                  buffer.ReadyForTake(match->GetActualTime_ms()) || buffer.started)));
}

Player* Team::PassGetReceiver() {
  // PES's movement selector state 2 (FUN_140931c60): only the team's own pass
  // target goes to the ball as the receiver. The man tests `his slot id ==
  // [own side's record + 0x78]` (FUN_140931c60 decompile: `iVar1 == *(int *)(lVar7 + 0x78)`, lVar7 = FUN_140a48a30(G, his side)), so
  // a T+0x78 that names the other side's man matches no man of this team.
  Player* const passTarget = GetPassTarget();
  if (!passTarget || !passTarget->IsActive() || passTarget->GetTeamID() != id) return nullptr;
  const pes::ReceiveConstants& k = match->GetPesDefenderDecisionConstants().receive;
  const pes::NewestEventView newest = pes::NewestEvent(match->GetPesEvents());
  const pes::BallEventKind newestKind = pes::ReceiveEventKindOf(newest.kind);
  const bool passInFlight = newest.present && (newestKind == pes::BallEventKind::kPass ||
                                               newestKind == pes::BallEventKind::kHighPass);
  if (!passInFlight || newest.actor.team != static_cast<uint32_t>(id)) return nullptr;
  const Vector3 ballPos = match->GetBall()->Predict(0).Get2D();

  pes::PassGetInput in;
  in.iAmPassTarget = true;
  in.newestEventIsMyKick = match->GetPesActorPlayer(newest.actor) == passTarget;
  in.inKickBeforeContact = IsKickFunction(passTarget->GetCurrentFunctionType());
  // The arbitration: the other team's stored pass target standing in front
  // of mine, facing the ball.
  Player* theirTarget = match->GetTeam(1 - id)->GetPassTarget();
  pes::ArbitrationInput arb;
  arb.otherTargetValid = theirTarget && theirTarget->IsActive();
  if (arb.otherTargetValid) {
    arb.otherX = theirTarget->GetPosition().coords[0];
    arb.otherY = theirTarget->GetPosition().coords[1];
    arb.otherHeading_deg = HeadingDeg(theirTarget->GetDirectionVec());
  }
  arb.ballHeading_deg = HeadingDeg(match->GetBall()->GetMovement().Get2D());
  arb.myX = passTarget->GetPosition().coords[0];
  arb.myY = passTarget->GetPosition().coords[1];
  in.arbitrationWins = pes::ArbitrationWins(k, arb);
  // The MA+0x24840 scan: a team-mate among the three nearest the ball who
  // already owns the trap.
  std::vector<Player*> nearest;
  AI_GetClosestPlayers(this, ballPos, nearest, kPassGetNearestSlots);
  for (Player* mate : nearest)
    if (mate != passTarget && mate->GetCurrentFunctionType() == e_FunctionType_Trap)
      in.mateNearBallOwnsIt = true;
  return pes::PassGetSelected(k, in) ? passTarget : nullptr;
}

void Team::ResetSituation(const Vector3& focusPos) {
  timeNeededToGetToBall_ms = 100;
  hasPossession = false;

  teamPossessionAmount = 1.0f;
  fadingTeamPossessionAmount = 1.0f;

  for (unsigned int i = 0; i < e_TouchType_SIZE; i++) {
    lastTouchPlayers[i] = 0;
  }
  lastTouchPlayer = 0;
  lastTouchType = e_TouchType_None;
  passTarget = nullptr;

  designatedTeamPossessionPlayer = players.at(0).get();

  for (unsigned int i = 0; i < players.size(); i++) {
    if (players.at(i)->IsActive()) {
      players.at(i)->ResetSituation(focusPos);
    }
  }

  GetController()->Reset();
}

// PES's team-possession latch (FUN_1404fe9c0), evaluated once per PES logic frame, as the
// team tick runs once per frame. Each ponytail below names the PES site it stands in for.
void Team::UpdatePesPossession() {
  // ponytail: PES's frame counter (MI+0x1198) has no GF counterpart; the real-time logic
  // frame at the game speed stands in for it (Match::GetPesFrame).
  const int frame = match->GetPesFrame();
  if (frame == pesLatchFrame) return;
  pesLatchFrame = frame;
  const int gs = static_cast<int>(match->GetPesClockConstants().gameSpeed);

  pes::PossessionLatchInput in;
  in.team = id;
  in.old = pesPossession;
  in.frame = frame;
  in.gs = gs;

  // Rule R. ponytail: PES's restart state MI+0x116c (2 or 3 while a restart is awarded)
  // and MI+0x1558 (the side taking it). GF's referee buffer is active from the award to
  // the kick (referee.cpp, buffer.active and buffer.teamID). PES's 2-or-3 split and
  // MI+0x1574 have no GF counterpart: a live restart reads as state 3, which is R on.
  const RefereeBuffer& buffer = match->GetRefereeBuffer();
  if (buffer.active) {
    in.restartState = 3;
    in.restartTeam = buffer.teamID;
  }

  // Ranked-list heads. ponytail: PES's heads (u32 at MA+0x24840 + 0x58*side, valid
  // below 22) read PES's analysis object, which GF does not carry. Stand-in: a team with
  // an active player has a head.
  in.ownHeadValid = GetActivePlayerCount() > 0;
  in.otherHeadValid = match->GetTeam(1 - id)->GetActivePlayerCount() > 0;

  // Step 1. PES's carrier (MA+0x2601c, FUN_140a627e0(ctx, 0)) is the holder, carried apart from GF's retainer:
  // it persists on a loose ball, so the latch keeps the carrier's side through the flicker, as PES's does, and
  // the step-43 sweep's gate (T+0x218 from the latch flip) follows it.
  Player* carrier = match->GetPesHolder();
  in.carrierValid = carrier != nullptr;
  in.carrierSide = carrier ? carrier->GetTeamID() : 0;

  // The newest event: the store's newest ring-0 record (PES's E+0x180698). No event reads as
  // PES's zeroed record: kind 0, frame 0, no actor. Its sub-kind (+0x40), flag byte (+0x2d) and
  // field (+0x1c) are the store's own words.
  const pes::NewestEventView newest = pes::NewestEvent(match->GetPesEvents());
  if (newest.present) {
    in.newestKind = static_cast<int>(newest.kind);
    in.newestFrame = newest.frame;
  }
  in.actorValid = newest.present && newest.actor.Valid();
  in.actorSide = in.actorValid ? static_cast<int>(newest.actor.team) : 0;
  in.newestSubKind = newest.subKind;
  in.newestFlag2d = newest.flag2d != 0;
  in.newestField1c = newest.field1c;

  // ponytail: MI+0x119c, the raw kick sum that step 3a reads, has no GF counterpart. PES's
  // value at the graded flips was at least 231 (3 * gs is 162); the stand-in is 0, so the sum
  // clause holds.
  in.ballSumRaw = 0;
  // ponytail: G+0x40+0x80, no GF counterpart. Its only effect here is the sub-3 exclusion.
  in.stateFlag40_80 = false;

  // Step 3b. ponytail: the holder (MA+0x2483c) is GF's designated possession player.
  Player* holder = match->GetDesignatedPossessionPlayer();
  in.holderValid = holder != nullptr;
  in.holderSide = holder ? holder->GetTeamID() : 0;
  // Step 4b's chase predicate, PES path only (FUN_1404F4BE0 for the holder). ponytail: the holder is the
  // designated possession man, the other side's head is its designated man, and arrival is the time to
  // the ball in frames (no FUN_14044E020 table). Off the PES path the latch is not blocked, as before.
  in.chaseBlocked = false;
  if (match->PesLoop() && holder) {
    Player* otherHead = match->GetTeam(1 - in.holderSide)->GetDesignatedTeamPossessionPlayer();
    pes::ContestInput contest;
    contest.holderValid = true;
    contest.teamHasBall = match->GetTeam(in.holderSide)->IsPesPossession();
    contest.otherHeadValid = otherHead != nullptr;
    contest.holderArrival_frames =
        static_cast<float>(holder->GetTimeNeededToGetToBall_ms() / Match::kTick_ms);
    contest.otherArrival_frames =
        otherHead ? static_cast<float>(otherHead->GetTimeNeededToGetToBall_ms() / Match::kTick_ms)
                  : 0.0f;
    contest.budget_frames = static_cast<float>(gs);
    in.chaseBlocked = pes::ContestBallPredicate(contest);
  }
  // The holder's side's latch as it stands now (the other team's value is this frame's if
  // that team's update ran first).
  in.holderTeamLatch = holder != nullptr && match->GetTeam(in.holderSide)->IsPesPossession();
  // ponytail: FUN_140a489f0(G, other head).+0x1470 == 0 has no GF counterpart. Stand-in:
  // flag set, so 3b needs the holder's team latch.
  in.otherHeadFlagClear = false;

  pesPossession = pes::TeamPossessionLatch(in);
}

void Team::Process() {
  // PES's loose-ball PRESS writer runs in the team tick before the latch
  // (press-life 13.1). It self-gates on the PES path.
  teamController->PesPressTick();
  UpdatePesPossession();
  // FUN_14051D9E0's head write (0x14051DA5A): the bulk writer of the PES role arrays.
  teamController->WriteRoleArrays(true);
  // PES's designation publisher (FUN_14051E350, step 27b). GF's restart branch is stood in by
  // MatchModeByte = 0 (teamAIcontroller.cpp), so the publisher runs on every team tick.
  if (match->PesLoop()) teamController->UpdatePesDesignation();
  teamPossessionAmount =
      (float)(match->GetTeam(abs(GetID() - 1))->GetTimeNeededToGetToBall_ms() + 1500) /
      (float)(GetTimeNeededToGetToBall_ms() + 1500);
  float tmpFadingTeamPossessionAmount =
      fadingTeamPossessionAmount * 0.995f + clamp(teamPossessionAmount, 0.5f, 1.5f) * 0.005f;
  fadingTeamPossessionAmount += clamp(tmpFadingTeamPossessionAmount - fadingTeamPossessionAmount,
                                      -0.005f, 0.005f);  // maximum change per 10ms

  if (!match->IsInPlay() || match->IsInSetPiece() || match->GetBallRetainer() != 0) {
    if (match->GetBallRetainer() != 0) {
      fadingTeamPossessionAmount = teamPossessionAmount =
          (match->GetBallRetainer()->GetTeamID() == GetID()) ? 1.5f : 0.5f;
    } else {
      fadingTeamPossessionAmount = teamPossessionAmount =
          (match->GetBestPossessionTeamID() == GetID()) ? 1.5f : 0.5f;
    }
  }

  // PES's strategy decider runs every team tick and acts on dead balls, so it
  // sits outside the in-play gate below.
  teamController->UpdateStrategy();
  teamController->UpdatePesSheetPlan();
  // PES's mark table (team-tick step 35, T7a `0x1404F76AC`, after the prep steps the cone doc cites) rebuilds from the
  // rows Match::Process ticked for both sides this frame, before the in-play gate (T7a runs in every state).
  teamController->TickMarkTable();
  if (match->PesLoop()) RunPesPassTarget();
  if (match->IsInPlay() && !match->IsInSetPiece()) {
    teamController->Process();

    // PES re-decides the whole man marking every frame (report 3, 14.2); GF
    // rebuilt it every 400 ms, which let a run go unmarked for half a second.
    teamController->CalculateManMarking();

    // The cadence PES re-decides at, in logic frames (Milestone A1): GF ran it
    // every 40 ticks, phase-shifted per side.
    if ((match->GetSimFrame() + 20 * id) % 40 == 0) {
      teamController->CalculateDynamicRoles();
      // printf("dynamic roles calc team %i\n", id);
    }
  }

  for (unsigned int i = 0; i < players.size(); i++) {
    if (players.at(i)->IsActive()) {
      players.at(i)->Process();
    }
  }

  // FUN_14051E2E7 and the B swap at FUN_14051E31E: the tail write, at the end of the team tick.
  teamController->WriteRoleArrays(false);
  // Team tick step 48 (FUN_14057F600 at 0x1404F79A8): the release/claim
  // dispatch, after the run-manager takes above (PES steps 38/41 precede it)
  // and running stopped too, so outside the in-play gate.
  if (match->PesLoop()) teamController->PesStep48();
  // Team tick step 55 (FUN_140A42A70 at 0x1404F7A8F; restart arm 26c at 0x1404F6F0E): the last sim
  // write of the tick, after every step GF ports. The strategy decider above must not run it.
  if (match->PesLoop()) teamController->PesStep55();

  // PES's movement selector (pes-re-receiving.md §2) sends the PASS_GET
  // receiver to the ball while a pass is in flight; GF's time-to-ball rating
  // below covers the loose balls PES's undecoded race selectors handle.
  //
  // No set-piece gate: the movement selector picks state 2 purely from "the
  // team's pass target T+0x78 is this player" (FUN_140931c60), and the pass
  // target is cleared when a restart is set up, so a live pass target is by
  // definition not a stale one. The `!IsInSetPiece()` this replaces was a
  // workaround for the throw-in stall, whose real cause - a restart taken past
  // the lines, so no kick anim could reach the ball - is fixed elsewhere
  // (OffsideRule::RestartPosition clamps the spot inside the pitch). Keeping it
  // would have left PASS_GET ineligible exactly when PES grants it.
  if (Player* receiver = match->IsInPlay() ? PassGetReceiver() : nullptr) {
    designatedTeamPossessionPlayer = receiver;
    return;
  }

  int designatedPlayerTime_ms = designatedTeamPossessionPlayer->GetTimeNeededToGetToBall_ms();
  Player* bestPlayer = GetBestPossessionPlayer();
  int oppTime_ms = match->GetTeam(abs(GetID() - 1))->GetTimeNeededToGetToBall_ms();
  if (designatedTeamPossessionPlayer != bestPlayer) {
    // switch only if other player is somewhat better, to overcome possession-chaos
    int bestPlayerTime_ms = bestPlayer->GetTimeNeededToGetToBall_ms();
    float timeRating = (float)(bestPlayerTime_ms + 500) / (float)(designatedPlayerTime_ms + 500);

    if (bestPlayer->HasPossession())
      timeRating *= 0.5f;
    if (designatedTeamPossessionPlayer->HasPossession())
      timeRating /= 0.5f;

    // current player can get to the ball before the closest opponent: less need to switch
    // if (GetID() == 0) printf("opptime: %i, designated time: %i\n", oppTime_ms,
    // designatedPlayerTime_ms);
    if (designatedPlayerTime_ms < oppTime_ms - 100) {
      timeRating += 0.2f;
      timeRating *= 1.2f;
    }

    if (timeRating < 0.8f) {
      designatedTeamPossessionPlayer = bestPlayer;
    }
  }

  // printf("team id: %i, time: %i, other team id: %i, time: %i\n", GetID(),
  // GetTimeNeededToGetToBall_ms(), match->GetTeam(abs(GetID() - 1))->GetID(),
  // match->GetTeam(abs(GetID() - 1))->GetTimeNeededToGetToBall_ms());

  /*
    if (id == 0) {
      GetSmallDebugCircle1()->SetPosition(designatedTeamPossessionPlayer->GetPosition());
    } else {
      GetSmallDebugCircle2()->SetPosition(designatedTeamPossessionPlayer->GetPosition());
    }
  */
}

void Team::PreparePutBuffers(unsigned long snapshotTime_ms) {
  for (unsigned int i = 0; i < players.size(); i++) {
    if (players.at(i)->IsActive()) {
      players.at(i)->PreparePutBuffers(snapshotTime_ms);
    }
  }
}

void Team::FetchPutBuffers(unsigned long putTime_ms) {
  for (unsigned int i = 0; i < players.size(); i++) {
    if (players.at(i)->IsActive()) {
      players.at(i)->FetchPutBuffers(putTime_ms);
    }
  }
}

void Team::Put() {
  for (unsigned int i = 0; i < players.size(); i++) {
    if (players.at(i)->IsActive()) {
      players.at(i)->Put();
    }
  }
}

void Team::UpdatePossessionStats() {
  for (unsigned int i = 0; i < players.size(); i++) {
    if (players.at(i)->IsActive()) {
      players.at(i)->UpdatePossessionStats();
    }
  }

  // possession?

  hasPossession = false;
  timeNeededToGetToBall_ms = 100000;
  for (int i = 0; i < (signed int)players.size(); i++) {
    if (players.at(i)->IsActive()) {
      if (players.at(i)->HasPossession())
        hasPossession = true;
      if (players.at(i)->GetTimeNeededToGetToBall_ms() < timeNeededToGetToBall_ms)
        timeNeededToGetToBall_ms = players.at(i)->GetTimeNeededToGetToBall_ms();
    }
  }
}

Player* Team::GetGoalie() {
  for (unsigned int i = 0; i < players.size(); i++) {
    if (players.at(i)->IsActive()) {
      if (players.at(i)->GetFormationEntry().role == e_PlayerRole_GK)
        return players.at(i).get();
    }
  }

  return nullptr;
}

