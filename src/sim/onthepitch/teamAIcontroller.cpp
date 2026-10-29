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

#include "teamAIcontroller.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <cstring>
#include <map>
#include <utility>

#include "../data/playerskills.hpp"
#include "../data/playingstyles.hpp"
#include "pes/movestep.hpp"
#include "pesmovestep.hpp"
#include "pestrapactor.hpp"
#include "pessetpieceteam.hpp"
#include "pessequencer.hpp"
#include "pes/mbinfo.hpp"
#include "setpiecelaws.hpp"
#include "simcontext.hpp"
#include "../misc/hungarian.h"
#include "AIsupport/AIfunctions.hpp"
#include "aitactics.hpp"
#include "match.hpp"
#include "team.hpp"

bool ReverseSortTacticalOpponentInfo(const TacticalOpponentInfo& a, const TacticalOpponentInfo& b) {
  return a.dangerFactor > b.dangerFactor;
}

TeamAIController::TeamAIController(Team* team) : team(team) {
  match = team->GetMatch();
  taker = 0;

  // Whatever the game plan set before kick-off. These used to start at their
  // defaults every match, so the only way to reach them was from the touchline once
  // play had begun - no use at all for a side nobody is shouting at.
  instructions = TeamInstructions::Load(team->GetTeamData()->GetTactics().userProperties);
  // PES's team sheet: a team's tactics may set any sheet value; the rest are
  // the install's sheet-ctor defaults (FUN_1408c5390).
  {
    const Properties& t = team->GetTeamData()->GetTactics().userProperties;
    teamRecord.sheet = pes::DefaultTeamSheet(match->GetPesTeamConceptConstants());
    const auto real = [&t](const char* key, int current) {
      return static_cast<int>(t.GetReal(key, current));
    };
    teamRecord.sheet.SetSlider(pes::kSliderSupportRange,
                               real("support_range", teamRecord.sheet.Slider(pes::kSliderSupportRange)));
    teamRecord.sheet.SetSlider(pes::kSliderDefensiveLine,
                               real("defensive_line", teamRecord.sheet.Slider(pes::kSliderDefensiveLine)));
    teamRecord.sheet.SetSlider(pes::kSliderCompactness,
                               real("compactness", teamRecord.sheet.Slider(pes::kSliderCompactness)));
    static const char* kToggleKeys[pes::kSheetToggleCount] = {
        "attacking_style", "build_up",        "attacking_area", "positioning",
        "defensive_style", "containment_area", "pressuring"};
    for (int k = 0; k < pes::kSheetToggleCount; ++k)
      teamRecord.sheet.SetToggle(k, real(kToggleKeys[k], teamRecord.sheet.Toggle(k)));
  }

  depth = 0.45f;
  width = 0.95f;

  setPieceType = e_SetPiece_None;

  offsideTrapX = 0;

  defensiveShapeField = pes::ShapeFieldFromPitch();
  defensiveLineHeight = 0.0f;
  defensiveTeamLength = 0.0f;
  previousLineHeight = 0.0f;
  defensiveStyle = 0;
  defensiveAttackLevel = 2;
  defensiveLineSlider = 4.0f;
  defensiveCompactSlider = 4.0f;
  defensivePressing = false;


  prevTeamHasPossession = false;

  baseTeamTactics.Set("position_offense_depth_factor", 0.9f);
  baseTeamTactics.Set("position_defense_depth_factor", 0.75f);
  baseTeamTactics.Set("position_offense_width_factor", 0.9f);
  baseTeamTactics.Set("position_defense_width_factor", 0.8f);
  baseTeamTactics.Set("position_offense_ownhalf_factor", 0.52f);
  baseTeamTactics.Set("position_defense_ownhalf_factor", 0.54f);
  baseTeamTactics.Set("position_offense_midfieldfocus", 0.6f);
  baseTeamTactics.Set("position_defense_midfieldfocus", 0.5f);
  baseTeamTactics.Set("position_offense_midfieldfocus_strength", 0.35f);
  baseTeamTactics.Set("position_defense_midfieldfocus_strength", 0.35f);
  baseTeamTactics.Set("position_offense_sidefocus_strength",
                      0.1f);  // take possession factor more seriously (high) or ball position (low)
  baseTeamTactics.Set("position_defense_sidefocus_strength", 0.4f);
  baseTeamTactics.Set("position_offense_microfocus_strength", 0.7f);
  baseTeamTactics.Set("position_defense_microfocus_strength", 0.8f);

  // The non-positional sliders travel through the same pipeline. Their neutral
  // base is 0.5; the user's slider offsets it.
  baseTeamTactics.Set("dribble_offensiveness", 0.5f);
  baseTeamTactics.Set("dribble_centermagnet", 0.5f);
  // PES 2021's offside-trap instruction (report 14.4's `T+0xe4`): it arms the
  // trap only while a manager has asked for it, and both live teams ran with it
  // off. GF has no touchline button for it yet, so the base is PES's value and a
  // team's own tactics data can still set it.
  baseTeamTactics.Set("offside_trap", 0.0f);

  // how much [-1 * this value .. +1 * this value] offset we can add to the above tactics.
  teamTacticsModMultipliers.Set("position_offense_depth_factor", 0.1f);
  teamTacticsModMultipliers.Set("position_defense_depth_factor", 0.1f);
  teamTacticsModMultipliers.Set("position_offense_width_factor", 0.1f);
  teamTacticsModMultipliers.Set("position_defense_width_factor", 0.1f);
  teamTacticsModMultipliers.Set("position_offense_midfieldfocus", 0.3f);
  teamTacticsModMultipliers.Set("position_defense_midfieldfocus", 0.3f);
  teamTacticsModMultipliers.Set("position_offense_sidefocus_strength", 0.1f);
  teamTacticsModMultipliers.Set("position_defense_sidefocus_strength", 0.1f);
  teamTacticsModMultipliers.Set("position_offense_microfocus_strength", 0.15f);
  teamTacticsModMultipliers.Set("position_defense_microfocus_strength", 0.15f);
  // These four used to be missing, so their user sliders were multiplied by 0.
  teamTacticsModMultipliers.Set("position_offense_ownhalf_factor", 0.1f);
  teamTacticsModMultipliers.Set("position_defense_ownhalf_factor", 0.1f);
  teamTacticsModMultipliers.Set("position_offense_midfieldfocus_strength", 0.15f);
  teamTacticsModMultipliers.Set("position_defense_midfieldfocus_strength", 0.15f);
  // At 0.5 the offset formula (user - 0.5) * 2 * multiplier makes the user's
  // slider land exactly as-is on the neutral 0.5 base.
  teamTacticsModMultipliers.Set("dribble_offensiveness", 0.5f);
  teamTacticsModMultipliers.Set("dribble_centermagnet", 0.5f);
  // The offside-trap instruction is a flag, not a slider: its whole [0, 1] range
  // travels through so a team's tactics data can set it either way.
  teamTacticsModMultipliers.Set("offside_trap", 1.0f);

  offensivenessBias = 0.5f;
  // The shape the team is actually lined up in, read off the lineup. This used
  // to come from a "formation" string in the tactics properties, but tactics_xml
  // is parsed with atof, so that property can only ever hold a number - every
  // team came out a 4-4-2 however it was really set up, and a side loaded as a
  // 5-4-1 got reshaped by an AI that thought it was a flat four.
  formationShape = TeamFormationShape();
  Log(e_Notice, "TeamAIController", "TeamAIController",
      "team " + int_to_str(team->GetID()) + " lined up as " +
          Formations::ShapeName(formationShape));

  UpdateTactics();
}

TeamAIController::~TeamAIController() {
  if (Verbose())
    printf("exiting teamcontroller.. ");
  if (Verbose())
    printf("done\n");
}

void TeamAIController::TickScoreRows() {
  // The lane space of the team's carrier controller: its own active players at 0..10 and the
  // opponents at 11..21, each side in squad order (ElizaController's fillSide).
  pes::RowPlayers players{};
  pes::PesWorld& world = match->PesWorldOf();
  const bool pesPath = match->PesLoop();
  const auto fillSide = [&](Team* side, int base) {
    std::vector<Player*> squad;
    side->GetActivePlayers(squad);
    for (int j = 0; j < pes::lane::kSideSize && j < static_cast<int>(squad.size()); ++j) {
      uint8_t* man = world.ManRecord(match->PesSlotOf(squad[j]));
      // The PES path reads the men's records, written at the tick's start; GF mode reads the live positions.
      const Vector3 pos = squad[j]->GetPosition();
      const float x = pesPath ? pes::LoadF32(man, pes::kManPosXOffset) : pos.coords[0];
      const float z = pesPath ? pes::LoadF32(man, pes::kManPosZOffset) : pos.coords[1];
      players[base + j] = pes::RowPlayer{x, z, true, man};
    }
  };
  fillSide(team, 0);
  fillSide(match->GetTeam(1 - team->GetID()), pes::lane::kSideSize);
  const pes::ScoreRowConstants k{pesFieldHalfLength, pesFieldHalfWidth};
  // PES's writer writes nothing until a stoppage latches it: the flag is the stage's (pesscorerows.hpp,
  // FUN_140457070's argument; Match::Process). A match opens stopped (inPlay starts false), so its first
  // tick latches the writer. A seam that starts play with no stoppage (Match::StartPlay) would leave the
  // rows unwritten until one came, so until the writer is latched every tick is its full refresh.
  scoreRows_.Tick(players, k, match->GetSimFrame(), !match->IsInPlay() || !scoreRows_.Latched(), pesPath, world);
  WriteMaRecord();
}

void TeamAIController::WriteMaRecord() {
  // The side's AI record MA in the world window (PES layout, pesworldrecord.hpp): the sorted lane lists the score-row
  // writer builds (the rows are the writer's own state, this is the block the carrier units' angle reads), the ball copy,
  // and the holder's id.
  // ponytail: MA+0x2482C's writer is not decoded (it changes every frame, pes-re-advance.md); the ball's record 0 stands
  // in for it, as the press gate's stand-in does (teamAIcontroller.cpp's MA ball copy). Replace with the PES writer.
  // ponytail: MA+0x2483C is the T7a holder (FUN_140A627E0 else MA+0x2483C): the match's holder slot stands in for it.
  pes::PesWorld& world = match->PesWorldOf();
  const int side = team->GetID();
  float ball[3];
  world.BallSample(0, ball);
  world.SetMaBall(side, ball[0], ball[1], ball[2]);
  const Player* holder = match->GetPesHolder();
  world.SetMaCarrier(side, holder != nullptr ? static_cast<uint32_t>(match->PesSlotOf(holder)) : pes::kMaNoCarrier);
  // MA+0x2601C (FUN_140457070's holder store, 0x14045725A) and the NearestList (MA+0x22E4C + 200 slot, FUN_14045B420): the cells
  // the Trap chain reads through P+0x3AC0 (docs/ports/pes-port-TrapOpen2.md). The holder's slot is the match's (0xFF: none).
  world.SetMaHolder(side, holder != nullptr ? static_cast<uint32_t>(match->PesSlotOf(holder)) : 0xFFu);
  for (int slot = 0; slot < pes::lane::kSlots; ++slot) {
    const pes::SortedRow& row = scoreRows_.Rows().nearest[slot];
    for (int teamSide = 0; teamSide < 2; ++teamSide) {
      for (int rank = 0; rank < pes::kMaListEntries; ++rank) {
        if (rank < row.Count(teamSide))
          world.SetMaNearestEntry(side, slot, teamSide, rank, static_cast<uint32_t>(row.Entry(teamSide, rank).id),
                                  row.Entry(teamSide, rank).value);
        else
          world.ClearMaNearestEntry(side, slot, teamSide, rank);
      }
    }
  }
  const pes::ScoreRows& rows = scoreRows_.Rows();
  for (int list = 0; list < 2; ++list) {
    for (int teamSide = 0; teamSide < 2; ++teamSide) {
      const pes::SortedRow& row = rows.list[list];
      for (int rank = 0; rank < pes::kMaListEntries; ++rank) {
        if (rank < row.Count(teamSide))
          world.SetMaListEntry(side, list, teamSide, rank, static_cast<uint32_t>(row.Entry(teamSide, rank).id),
                               row.Entry(teamSide, rank).value);
        else
          world.ClearMaListEntry(side, list, teamSide, rank);
      }
    }
  }
}

void TeamAIController::TickMarkTable() {
  // PES 2021's step 35 (FUN_1404F61E0, T7a `0x1404F76B1`): rebuild the mark table from the run world each team tick on
  // the PES path. T7a runs inside the team tick (after the prep steps, before step 38/WriteRoleArrays), reading the rows
  // Match::Process ticked for both sides this frame. It is active in every match state, not only in play. The GF path
  // keeps its own man-marking and never reads markTableState, so the build is PES-path only.
  markTableState.mark.fill(0);
  markTableState.markSlot = pes::marktable::kNoSlot;
  if (!match->PesLoop()) return;
  // ponytail: B7D8's writers (FUN_140546FB0 at 0x140547814, FUN_140549D30 at
  // 0x140549E34) store [param_1+0x1C/0x20/0x24 words] % 11, which no decoded GF
  // state names (mark-cone.md section 1: the designated possession player is
  // not that value, only the closest decoded engaging-man carrier).
  {
    Player* const engaging = team->GetDesignatedTeamPossessionPlayer();
    markTableState.markSlot = engaging != nullptr ? RunSlotOf(engaging) : -1;
  }
  // The T7a holder is FUN_140A627E0 else MA+0x2483C (PES path: MA+0x2601C); the split landed it, so the stand-in
  // is gone: the holder (Match::RecomputePesHolder) feeds the slot directly. The run slot is team*11 + roster
  // index (RunSlotOf; -1 when he is not in it).
  Player* const owner = match->GetPesHolder();
  if (owner == nullptr) return;
  const int holder = RunSlotOf(owner);
  if (holder < 0 || holder >= 2 * pes::marktable::kSlots) return;
  // This tick's facts: the candidate rows, the pair bearings, the positions
  // and the holder point, straight out of the run world the manager builds on
  // the team's parity tick.
  const pes::offball::RunWorld& w = lastRunWorld;
  pes::marktable::Facts facts;
  facts.side = team->GetID();
  facts.holder = holder;
  const pes::offball::PlayerFacts& hf = w.p[holder];
  facts.holderIsKeeper = hf.goalie;
  facts.sign = w.s[team->GetID()];
  facts.holderMan = hf.man;
  facts.world = &match->PesWorldOf();
  facts.goalAreaHalfWidth_m = pesGoalAreaHalfWidth;  // FieldInfo F[5], readers doc section 4
  int out = 0;
  for (const pes::offball::NearOpponent& near : pes::offball::NearestTeamBlock(w, holder, facts.side)) {
    if (out >= pes::marktable::kSlots) break;
    const pes::offball::PlayerFacts& cf = w.p[near.id];
    pes::marktable::Candidate& dst = facts.candidates[out++];
    dst.id = near.id;
    dst.d2 = near.d2;
    dst.actionWord = cf.passGet ? 1 : 0x3F;
    dst.man = cf.man;
    dst.pairToTarget =
        pes::offball::RowBearing(w, near.id, facts.side * pes::marktable::kSlots + markTableState.markSlot);
    dst.pairToHolder = pes::offball::RowBearing(w, near.id, holder);
    dst.head = cf.facing();
  }
  for (int i = 0; i < pes::offball::kSlots; ++i) {
    const pes::offball::PlayerFacts& mf = w.p[(holder / pes::offball::kSlots) * pes::offball::kSlots + i];
    facts.gateMan[i] = mf.man;
  }
  pes::marktable::BuildMarkTable(&markTableState, facts, match->GetPesMarkTableConstants());
}

void TeamAIController::Process() {
  if (match->IsFrameSecond())
    UpdateTactics();

  CalculateSituation();

  float startDistance =
      30.0f + 20.0f * offensivenessBias;  // distance from goal where we start holding the opponents
                                          // (but are likely to fallback somewhat)
  float forceDistance = 6.0f;  // minimum distance from goal - try to 'hold' the opp there

  float deepestDanger = (pitchHalfW - startDistance) * team->GetSide();

  // ball as max
  float adaptedBallX = match->GetBall()->Predict(0).coords[0];
  // when far away from our goal (startDistance), drop back more easily. closer to forceDistance,
  // don't buckle.
  adaptedBallX *= team->GetSide();  // > 0 == on our half (easier to work with)
  float offsetX =
      20.0f +
      10.0f * (1.0f - offensivenessBias);  // when ball is this distance away from startDistance, we
                                           // start falling back more towards forceDistance.
  float startToForcedBias = NormalizedClamp(
      adaptedBallX, pitchHalfW - startDistance - offsetX,
      pitchHalfW -
          forceDistance);  // where, between startDistance and forceDistance, is the ball? 0 .. 1
  adaptedBallX +=
      offsetX * (1.0f - startToForcedBias);  // the fall back intensity gradually diminishes when
                                             // getting closer to forceDistance
  adaptedBallX *= team->GetSide();           // back to absolute space
  if (adaptedBallX * team->GetSide() > deepestDanger * team->GetSide())
    deepestDanger = adaptedBallX;

  // ballfuture as max
  if (match->GetBall()->Predict(700).coords[0] * team->GetSide() > deepestDanger * team->GetSide())
    deepestDanger = match->GetBall()->Predict(700).coords[0];

  // opp as max
  Player* opp = match->GetTeam(abs(team->GetID() - 1))->GetDesignatedTeamPossessionPlayer();
  float cautionDistance = 4.0f * team->GetSide();
  if ((opp->GetPosition().coords[0] + opp->GetMovement().coords[0] * 0.15f + cautionDistance) *
          team->GetSide() >
      deepestDanger * team->GetSide())
    deepestDanger =
        opp->GetPosition().coords[0] + opp->GetMovement().coords[0] * 0.1f + cautionDistance;

  // slacking teammate as max
  float lineX = AI_GetOffsideLine(match, match->GetMentalImage(0), abs(team->GetID()));
  float allowSlackDistance =
      4.0f;  // despite teammates slacking behind line this much, just hold the line
  if (lineX * team->GetSide() - allowSlackDistance > deepestDanger * team->GetSide()) {
    // printf("previous: %f\n", deepestDanger);
    deepestDanger = lineX - allowSlackDistance * team->GetSide();
    // printf("now:      %f\n", deepestDanger);
  }

  /*
  if (team->GetID() == 0) SetRedDebugPilon(Vector3(deepestDanger, 0, 0));
  if (team->GetID() == 1) SetYellowDebugPilon(Vector3(deepestDanger, 0, 0));
  */

  offsideTrapX = deepestDanger;
  // disable
  // offsideTrapX = pitchHalfW * team->GetSide();

  CalculateDefensiveShape();

  // calculate who's dangerous

  std::vector<Player*> players;
  match->GetActiveTeamPlayers(abs(team->GetID() - 1), players);

  Vector3 mostDangerousPos = Vector3((pitchHalfW - 2.0) * team->GetSide(), 0, 0);
  mostDangerousPos = mostDangerousPos * 0.8f + match->GetBall()->Predict(100).Get2D() * 0.2f;

  tacticalOpponentInfo.clear();

  for (auto* player : players) {
    TacticalOpponentInfo info;
    info.player = player;

    info.dangerFactor =
        1.0 -
        NormalizedClamp((player->GetPosition() - mostDangerousPos).GetLength(), 0, pitchHalfW * 2);

    // player on ball is most dangerous
    info.dangerFactor *= 0.95f;
    if (player == match->GetTeam(abs(team->GetID() - 1))->GetDesignatedTeamPossessionPlayer())
      info.dangerFactor += 0.05f;

    tacticalOpponentInfo.push_back(info);
  }

  // sort list
  std::sort(tacticalOpponentInfo.begin(), tacticalOpponentInfo.end(),
            ReverseSortTacticalOpponentInfo);

  // MI+0x11a0: time since the possessing team changed. PES counts every frame,
  // stoppages included (transitions 1.1).
  const unsigned long now = match->GetActualTime_ms();
  if (teamHasPossession != prevTeamHasPossession) possessionChange_ms = now;
  prevTeamHasPossession = teamHasPossession;
  // FUN_140a74ec0: in-play time since the newest restart, meaningful only when
  // that restart was a kick-off (transitions 1.2).
  if (match->IsInPlay() && !match->IsInSetPiece()) inPlaySinceRestart_ms += now - lastProcess_ms;
  lastProcess_ms = now;

  // PES's team concepts (FUN_140579840): the counter-press trapper and the
  // counter-attack replace GF's zone pressure and counter roll.
  RunConcepts();

  ProcessOffBall();
  // PES's step 43 (FUN_1404F78FE, the gate at 0x1404F78DB CMP [T+0x218],0): with T+0x218 == 0 every own slot is
  // released through FUN_140A40820. It follows the designation and the run manager (steps 38 and 41) of the same
  // tick, and it runs on every in-play tick, where ProcessOffBall runs only on the team's parity frames. The gate
  // and the release are ReleaseSweep's; the stamp is the frame clock T+0x21C (GetPesFrame on the PES path).
  if (match->PesLoop())
    pes::run::ReleaseSweep(&runSlots, teamTick.noBallCounter,
                           static_cast<uint32_t>(match->GetPesFrame()));
}

float mixup(float base, const std::string& varname, e_PlayerRole role, float counterAttack) {
  float value = -100.0f;

  if (role == e_PlayerRole_CB) {
    if (varname == "position_offense_width_factor")
      value = 0.2f;  // wider defense
  }

  if (role == e_PlayerRole_LB || role == e_PlayerRole_RB) {
    if (varname == "position_defense_ownhalf_factor")
      value = -0.075f;  // go forward
    if (varname == "position_offense_width_factor")
      value = 0.2f;  // wider defense
    if (varname == "position_offense_ownhalf_factor")
      value = -0.1f;  // go forward
  }

  if (role == e_PlayerRole_LM || role == e_PlayerRole_RM || role == e_PlayerRole_LW ||
      role == e_PlayerRole_RW) {
    // wingers stay high up to offer counter-attack support
    if (varname == "position_defense_ownhalf_factor")
      value = -(0.02f + 0.06f * counterAttack);
    if (varname == "position_offense_ownhalf_factor")
      value = -0.1f;  // go forward
  }

  if (role == e_PlayerRole_AM) {
    // attackers stay high up to offer counter-attack support
    if (varname == "position_defense_depth_factor")
      value = 0.05f + 0.15f * counterAttack;
  }

  if (role == e_PlayerRole_CF || role == e_PlayerRole_SS) {
    // strikers stay high up to offer counter-attack support
    if (varname == "position_defense_depth_factor")
      value = 0.05f + 0.15f * counterAttack;
  }

  if (value > -100.0f) {
    // bias version
    // float rolebias = 0.5f;
    // return clamp(value * rolebias + base * (1.0f - rolebias), 0.0f, 1.0f);

    // offset version
    return clamp(base + value, 0.0f, 1.0f);

  } else {
    return base;
  }
}

// PES's slot-lines (report 7's `ctx+0x128[slot]`): role class 3-6 is the
// midfield line, 7-9 the forward line, everything else the back line. GF's own
// roles fall into the same three groups.
namespace {

int PesLineIndexForRole(e_PlayerRole role) {
  switch (role) {
    case e_PlayerRole_DM:
    case e_PlayerRole_CM:
    case e_PlayerRole_LM:
    case e_PlayerRole_RM:
    case e_PlayerRole_AM:
      return 1;
    case e_PlayerRole_CF:
    case e_PlayerRole_SS:
    case e_PlayerRole_LW:
    case e_PlayerRole_RW:
      return 2;
    default:
      return 0;  // GK, CB, LB, RB
  }
}

}  // namespace

void TeamAIController::CalculateDefensiveShape() {
  // FUN_1405f9eb0 with FUN_1405f7a00's cushion and FUN_1405ef910's step-up
  // (report 6, 14.1), then FUN_1405efbd0's midfield catch-up (report 14.1). No
  // smoothing: the target is rebuilt from this tick's ball.
  const pes::DefensiveShapeConstants& k = match->GetPesDefensiveShapeConstants();
  const pes::ShapeField& field = defensiveShapeField;
  // Own frame: +x toward the opponentsents' goal, own goal at -halfLength.
  const float dir = static_cast<float>(-team->GetSide());
  const Vector3 ball = match->GetBall()->Predict(0);
  const Vector3 ballVel = match->GetBall()->GetMovement();

  pes::LineFacts facts;
  facts.ballDepth_m = ball.coords[0] * dir;
  const Vector3 backToOwnGoal(-1.0f, 0.0f, 0.0f);
  const Vector3 ballVel2D(ballVel.coords[0] * dir, ballVel.coords[1], 0.0f);
  const float ballSpeed = ballVel2D.GetLength();
  facts.ballSpeed_mps = ballSpeed;
  facts.ballHeadingFromGoal_deg =
      ballSpeed > 0.01f ? std::acos(std::clamp(backToOwnGoal.GetDotProduct(ballVel2D) / ballSpeed, -1.0f, 1.0f)) *
                             180.0f / pi
                        : 180.0f;

  Player* carrier = match->GetDesignatedPossessionPlayer();
  const bool hasCarrier = carrier != nullptr;
  // The cushion and the step-up both key off the opponent on the ball; when our
  // own man carries it, PES's cushion collapses entirely.
  const bool carrierIsOpponent = hasCarrier && carrier->GetTeam() != team;
  facts.carrierDepth_m = hasCarrier ? carrier->GetPosition().coords[0] * dir : 0.0f;
  if (hasCarrier) {
    const Vector3 facing = carrier->GetDirectionVec();
    const Vector3 facing2D(facing.coords[0] * dir, facing.coords[1], 0.0f);
    const float facingLength = facing2D.GetLength();
    facts.carrierFacingFromGoal_deg =
        facingLength > 0.01f ? std::acos(std::clamp(backToOwnGoal.GetDotProduct(facing2D) / facingLength,
                                                    -1.0f, 1.0f)) *
                                  180.0f / pi
                             : 0.0f;
  }

  // PES's attack level T+0xB8A4: the per-tick struct on the PES path, level 2
  // (PES's live value for both teams) off it.
  const int attackLevel = match->PesLoop() ? teamTick.b8a4 : 2;
  // The team sheet's own bytes (tactics 1, 4): Defensive Style, Defensive Line,
  // Compactness, and Pressuring = Conservative, which gives the marker 2 m more
  // room (FUN_140623750).
  const int style = teamRecord.sheet.Toggle(pes::kDefensiveStyle);
  const float lineSlider = static_cast<float>(teamRecord.sheet.Slider(pes::kSliderDefensiveLine));
  const float compactSlider = static_cast<float>(teamRecord.sheet.Slider(pes::kSliderCompactness));

  defensiveStyle = style;
  defensiveAttackLevel = attackLevel;
  defensiveLineSlider = lineSlider;
  defensiveCompactSlider = compactSlider;
  defensivePressing = teamRecord.sheet.Toggle(pes::kPressuring) == 1;

  // PES's defence-row dispatch (FUN_1405EC600 @ 0x1405ECC21): the possession flag picks block B (67B640),
  // which runs the fold pick on the PES path. Block A's forward pull (682770) is not ported yet (F2-F4), so
  // out of possession the GF target stands. The fold's consumer (the readers of planObj+0x7CD4) is not decoded.
  foldSlots.clear();
  if (match->PesLoop() &&
      pes::mentality::BlockFor(team->IsPesPossession()) == pes::mentality::Block::kInPossession) {
    std::array<pes::mentality::FoldCandidate, pes::roles::kSideSlots> cands{};
    std::array<int, pes::roles::kSideSlots> classes{};
    const auto& players = team->GetAllPlayers();
    for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) {
      Player* p = players[slot].get();
      classes[slot] = roles.b[slot];
      cands[slot].onPitch = p->IsActive();
      cands[slot].cls = roles.b[slot];
      // ponytail: the candidate position (67C4B0's local_238) has no decoded writer; the pitch position stands.
      cands[slot].x = p->GetPosition().coords[0];
      cands[slot].z = p->GetPosition().coords[1];
    }
    foldSlots = pes::mentality::FoldPicks(match->GetPesTeamConceptConstants().fold, cands,
                                          pes::mentality::ProtectedSlot(classes),
                                          pes::mentality::FoldCount(teamTick.b8a4), teamTick.b8a2);
  }

  float ownFrameLine =
      pes::DefensiveLineHeight(k, field, facts, lineSlider, style, attackLevel, match->IsInPlay(),
                               carrierIsOpponent, previousLineHeight,
                               match->PesLoop() ? teamTick.b8a3 : false);

  // FUN_1405efbd0's catch-up: the back line steps a quarter of the gap to the
  // midfield band upfield when the midfield has been caught upfield of it.
  const float gap = k.midGap_m;
  std::vector<float> midScores;
  std::vector<Player*> active;
  team->GetActivePlayers(active);
  for (Player* p : active) {
    if (DefensiveLineOf(p, p->GetFormationEntry().role) != pes::mentality::kMidLine) continue;
    const float x = p->GetPosition().coords[0] * dir;
    const float midLower = ownFrameLine + gap;
    midScores.push_back(x <= midLower + 2.0f ? 100.0f
                                             : 100.0f * (x - midLower - 8.0f) / -6.0f);
  }
  std::sort(midScores.begin(), midScores.end(), std::greater<float>());
  float score = 0.0f;
  for (size_t i = 0; i < midScores.size() && i < 3; ++i) score += midScores[i];
  ownFrameLine += pes::ShapeRamp(score, k.midLineScoreAdjustMax, 1.0f, 200.0f, 0.0f) *
                  std::fabs(gap) / k.midCatchUpDivisor;

  previousLineHeight = ownFrameLine;

  // FUN_140687820's measured lines and FUN_140687fb0's band push-up. The
  // push-up edits the slot-target bands only, so prevFinal stays unshifted.
  const pes::TeamConceptConstants& ck = match->GetPesTeamConceptConstants();
  backBand = ownFrameLine;
  midBand = ownFrameLine + gap;
  measuredBackLine = MeasuredLine(0, AI_GetOffsideLine(match, match->GetMentalImage(0), team->GetID()));
  measuredMidLine = MeasuredLine(1, measuredBackLine + dir * std::fabs(midBand - backBand));
  if (match->IsInPlay() && !conceptRunning[pes::kConceptRetreatBlockCreate] &&
      !conceptRunning[pes::kConceptForeCheckLineDown])
    ownFrameLine += pes::BandPushUp_m(ck, measuredBackLine * dir, ball.coords[0] * dir, ownFrameLine);
  // FUN_140682770, right after the push-up: while ForeCheckLineUp runs the back
  // line's lower edge rises toward the ball. ponytail: PES raises that edge
  // only; GF's bands all hang off the line height, so the block follows.
  if (conceptRunning[pes::kConceptForeCheckLineUp])
    ownFrameLine = pes::LineUpBackEdge(ck, ball.coords[0] * dir, backBand, ownFrameLine);

  defensiveLineHeight = ownFrameLine * dir;
  defensiveTeamLength = pes::TeamLength_m(k, style, attackLevel, compactSlider);
}

float TeamAIController::MeasuredLine(int line, float ref) const {
  // Mean world x of the line's men, each clamped to within 4 m of `ref`; none
  // -> the band itself (an own-frame value returned as world x, as PES does).
  // ponytail: PES's slot line T+0x7b88 is GF's role line; the keeper is left
  // out (INF: PES gives him no outfield line).
  const float clamp_m = match->GetPesTeamConceptConstants().measuredLineClamp_m;
  std::vector<Player*> active;
  team->GetActivePlayers(active);
  float sum = 0.0f;
  int n = 0;
  for (Player* p : active) {
    const e_PlayerRole role = p->GetFormationEntry().role;
    if (role == e_PlayerRole_GK || DefensiveLineOf(p, role) != line) continue;
    sum += std::clamp(p->GetPosition().coords[0], ref - clamp_m, ref + clamp_m);
    ++n;
  }
  return n ? sum / n : (line == 0 ? backBand : midBand);
}

bool TeamAIController::IsOffsideTrapArmed() const {
  // PES arms the trap on its team flag T+0xe4 (step 5, FUN_1404F6720; report 14.4). On the PES path the team tick holds
  // that bit, set from the run world on the run frame.
  if (match->PesLoop()) return teamTick.trapArmed;
  // Off the PES path GF has no touchline button for it, so the tactic stands in (PES's live value is off).
  return GetLiveTacticReal("offside_trap", 0.0f) > 0.5f;
}

Vector3 TeamAIController::GetAdaptedFormationPosition(Player* player,
                                                      bool useDynamicFormationPosition) {
  bool toggle_yFocus = true;
  bool toggle_microFocus = true;
  bool toggle_midfieldFocus = true;

  e_PlayerRole role;
  if (useDynamicFormationPosition)
    role = player->GetDynamicFormationEntry().role;
  else
    role = player->GetFormationEntry().role;

  const float counterAttack = AITactics::ClampSetting(
      liveTeamTactics.GetReal("counter_attack", 0.5f));
  Vector3 focalPoint = match->GetDesignatedPossessionPlayer()->GetPosition();
  float urgencyBias =
      1.0f - NormalizedClamp((focalPoint - player->GetPosition()).GetLength(), 2.0f, 30.0f);
  float ballX = match->GetBall()->GetAveragePosition(3500 * (1.0f - urgencyBias * 0.7f)).coords[0];
  float ballY = match->GetBall()->GetAveragePosition(4000 * (1.0f - urgencyBias * 0.5f)).coords[1];

  float offense_depthFactor = mixup(liveTeamTactics.GetReal("position_offense_depth_factor"),
                                    "position_offense_depth_factor", role, counterAttack);
  float defense_depthFactor = mixup(liveTeamTactics.GetReal("position_defense_depth_factor"),
                                    "position_defense_depth_factor", role, counterAttack);
  float offense_widthFactor = mixup(liveTeamTactics.GetReal("position_offense_width_factor"),
                                    "position_offense_width_factor", role, counterAttack);
  float defense_widthFactor = mixup(liveTeamTactics.GetReal("position_defense_width_factor"),
                                    "position_defense_width_factor", role, counterAttack);
  float offense_ownHalfFactor = mixup(liveTeamTactics.GetReal("position_offense_ownhalf_factor"),
                                      "position_offense_ownhalf_factor", role, counterAttack);
  float defense_ownHalfFactor = mixup(liveTeamTactics.GetReal("position_defense_ownhalf_factor"),
                                      "position_defense_ownhalf_factor", role, counterAttack);
  float offense_midfieldFocus = mixup(liveTeamTactics.GetReal("position_offense_midfieldfocus"),
                                      "position_offense_midfieldfocus", role, counterAttack);
  float defense_midfieldFocus = mixup(liveTeamTactics.GetReal("position_defense_midfieldfocus"),
                                      "position_defense_midfieldfocus", role, counterAttack);
  float offense_midfieldFocusStrength =
      mixup(liveTeamTactics.GetReal("position_offense_midfieldfocus_strength"),
            "position_offense_midfieldfocus_strength", role, counterAttack);
  float defense_midfieldFocusStrength =
      mixup(liveTeamTactics.GetReal("position_defense_midfieldfocus_strength"),
            "position_defense_midfieldfocus_strength", role, counterAttack);
  float offense_sideFocusStrength =
      mixup(liveTeamTactics.GetReal("position_offense_sidefocus_strength"),
            "position_offense_sidefocus_strength", role, counterAttack);
  float defense_sideFocusStrength =
      mixup(liveTeamTactics.GetReal("position_defense_sidefocus_strength"),
            "position_defense_sidefocus_strength", role, counterAttack);
  float offense_microFocusStrength =
      mixup(liveTeamTactics.GetReal("position_offense_microfocus_strength"),
            "position_offense_microfocus_strength", role, counterAttack);
  float defense_microFocusStrength =
      mixup(liveTeamTactics.GetReal("position_defense_microfocus_strength"),
            "position_defense_microfocus_strength", role, counterAttack);

  offense_sideFocusStrength += (-0.5 + AI_GetMindSet(role)) * 0.2f;
  defense_sideFocusStrength += (0.5 - AI_GetMindSet(role)) * 0.2f;
  offense_sideFocusStrength =
      clamp(offense_sideFocusStrength + -0.3f + (offensivenessBias) * 0.3f, 0.0f, 1.0f);
  defense_sideFocusStrength =
      clamp(defense_sideFocusStrength + -0.3f + (1.0f - offensivenessBias) * 0.3f, 0.0f, 1.0f);
  if (player->GetDebug())
    printf("offense: %f, defense: %f\n", offense_sideFocusStrength, defense_sideFocusStrength);

  float possessionAmountBias = NormalizedClamp(fadingTeamPossessionAmount - 0.5f, 0.3f, 0.7f);
  // also take the ball's position as part of the possessionBias equation
  float ballBias = NormalizedClamp(((ballX / pitchHalfW) * -team->GetSide()), -0.7f,
                                   0.7f);  // 0 == own half, 1 == opponent half
  // if possessionBias is unclear (near 0.5), take ballBias more seriously as indicator of
  // possession.
  float ballBiasBias = 1.0f - fabs(possessionAmountBias * 2.0f - 1.0f);  // biasception
  ballBiasBias *= 0.6f;  // don't take ballBias too serious - we need defenders to somewhat keep
                         // watch when possession team is unclear, even when the ball is forward
  float possessionBias = possessionAmountBias * (1.0f - ballBiasBias) + ballBias * ballBiasBias;

  possessionBias = clamp(possessionBias + (offensivenessBias - 0.5f) * 0.3f, 0.0f, 1.0f);

  // offense can be a bit more relaxed
  focalPoint = (match->GetBall()->GetAveragePosition(3000).Get2D() * 1.0f + focalPoint * 0.0f) *
                   possessionBias +
               (match->GetBall()->GetAveragePosition(2000).Get2D() * 0.5f + focalPoint * 0.5f) *
                   (1.0f - possessionBias);

  float adaptedDepth = depth * (offense_depthFactor * possessionBias +
                                defense_depthFactor * (1.0f - possessionBias));
  float adaptedWidth = width * (offense_widthFactor * possessionBias +
                                defense_widthFactor * (1.0f - possessionBias));

  float offsetX = pitchHalfW * team->GetSide() *
                  ((offense_ownHalfFactor * 2.0f - 1.0f) * possessionBias +
                   (defense_ownHalfFactor * 2.0f - 1.0f) * (1.0f - possessionBias));

  float sideFocusStrength = (offense_sideFocusStrength * possessionBias +
                             defense_sideFocusStrength * (1.0f - possessionBias));

  float sideFocus = possessionBias * 2.0f - 1.0f;  // -1 == own side, 1 == opp side

  float sideX = 0.2f * sideFocus * -team->GetSide() * pitchHalfW +
                0.8f * -match->GetAveragePossessionSide(6000) * pitchHalfW;
  // exaggerate: sideX = clamp(sideX * 2.0f, -pitchHalfW, pitchHalfW);
  // if (player->GetDebug()) printf("sideX: %f\n", sideX);
  float centerX = clamp((ballX * (1.0f - sideFocusStrength) + sideX * sideFocusStrength) + offsetX,
                        -pitchHalfW, pitchHalfW);

  // center of width
  float centerY = clamp(ballY, -pitchHalfH, pitchHalfH);

  // leave space for actual depth (50% depth on both sides of center)
  float adaptCenterToFitDepthBias = 0.95f;  // the lower this value, the more players will move with
                                            // the centerX, and be clamped to the edges of the pitch
  float adaptCenterToFitWidthBias = 0.9f;   // the lower this value, the more players will move with
                                            // the centerY, and be clamped to the edges of the pitch
  centerX *= ((1.0f - adaptedDepth) / 1.0f) * adaptCenterToFitDepthBias +
             (1.0f - adaptCenterToFitDepthBias);
  centerY *= ((1.0f - adaptedWidth) / 1.0f) * adaptCenterToFitWidthBias +
             (1.0f - adaptCenterToFitWidthBias);

  float backXBound, frontXBound, lowYBound, highYBound;
  if (!teamHasPossession) {
    // PES 2021's open-play defensive shape (docs/re/pes-re-defensive-shape.md 6,
    // 7, 14.1): the whole team shape is three bands off one line height, so a
    // defender's depth and width terms both come from the line he plays in. PES
    // rebuilds both shapes every tick and the out-of-possession one is what a
    // defending team plays in.
    const int lineIndex = DefensiveLineOf(player, role);
    const float line = defensiveLineHeight;
    const float len = defensiveTeamLength;
    // The back line sits on the line height itself (report 14.7: a back-line
    // slot's target is the line height, 82% within 5 cm live); the midfield a
    // gap above it, the forwards at the far end of the team length.
    float back = lineIndex == 0 ? line
                : lineIndex == 1
                    ? line + match->GetPesDefensiveShapeConstants().midGap_m
                    : std::max(line + len - 5.0f,
                               line + match->GetPesDefensiveShapeConstants().midGap_m);
    float front = lineIndex == 0 ? back + 3.0f
                 : lineIndex == 1 ? back + 2.0f
                                  : std::max(line + len, back + 2.0f);
    back = clamp(back, -pitchHalfW, pitchHalfW);
    front = clamp(front, -pitchHalfW, pitchHalfW);
    backXBound = std::min(back, front);
    frontXBound = std::max(back, front);

    // Each line's width comes from its own head count (15/19/23 m for 3/4/5
    // players), and the block is shifted toward the ball's touchline in
    // proportion to how wide the ball is played.
    const pes::DefensiveShapeConstants& shapeConstants =
        match->GetPesDefensiveShapeConstants();
    std::vector<Player*> teammates;
    team->GetActivePlayers(teammates);
    int headCount = 0;
    for (Player* mate : teammates)
      if (mate->GetFormationEntry().role != e_PlayerRole_GK &&
          DefensiveLineOf(mate, mate->GetFormationEntry().role) == lineIndex)
        ++headCount;
    const Vector3 currentBall = match->GetBall()->Predict(0);
    const float dir = static_cast<float>(-team->GetSide());
    const float half = pes::LineHalfWidth_m(
        shapeConstants, defensiveShapeField, headCount, defensiveCompactSlider,
        defensiveAttackLevel, true, currentBall.coords[1], currentBall.coords[0] * dir,
        line * dir);
    const float lateral = std::clamp((std::fabs(currentBall.coords[1]) -
                                      shapeConstants.lateralRampNear_m) /
                                         (shapeConstants.lateralRampFar_m -
                                          shapeConstants.lateralRampNear_m),
                                     0.0f, 1.0f);
    const float coefficient =
        lineIndex == 0 && headCount >= 3 ? 0.7f : 0.6f;  // report 7's per-line c table
    const float reach = pitchHalfH - 0.5f - half;
    const float shift = reach <= 0.0f
                            ? 0.0f
                            : std::clamp((currentBall.coords[1] > 0.0f ? 1.0f : -1.0f) * pitchHalfH *
                                             lateral * coefficient,
                                         -reach, reach);
    lowYBound = shift - half;
    highYBound = shift + half;
  } else {
    backXBound = centerX - adaptedDepth * pitchHalfW * -team->GetSide();
    frontXBound = centerX + adaptedDepth * pitchHalfW * -team->GetSide();
    lowYBound = centerY - adaptedWidth * pitchHalfH;
    highYBound = centerY + adaptedWidth * pitchHalfH;
    // maybe setting offside trap this way doesn't work too well: after all,
    // defensive positioning stuff is done after this, thereby diminishing trap
    // sometimes. on the other hand, maybe this is a good basic trap setup, and we
    // could apply the trap with the applyoffsidetrap function after the
    // defensive positioning as well. (done from the def/mid strategies code)
    if (backXBound * team->GetSide() > GetOffsideTrapX() * team->GetSide())
      backXBound = GetOffsideTrapX();
  }

  float xFocus = 0.0f;
  float xFocusStrength = 0.0f;
  float yFocus = ballY * 1.0f;
  float yFocusStrength =
      0.5f * possessionBias + 0.2f * (1.0f - possessionBias);  // todo: make dynamic/configurable

  float defendFactor = (1.0f - AI_GetMindSet(role)) * (1.0f - possessionBias);
  Vector3 microFocus = focalPoint;  // * Vector3(1.0f, 0.8f + (1.0f - defendFactor) * 0.2f, 0.0f);
                                    // // defenders stay in the middle more
  Vector3 defensiveFocusPos = Vector3(clamp(microFocus.coords[0] + team->GetSide() * 2.0f,
                                            -pitchHalfW, backXBound * team->GetSide()),
                                      microFocus.coords[1] * 0.9f, 0);
  microFocus =
      (Vector3(clamp(microFocus.coords[0] - team->GetSide() * 1.0f, -pitchHalfW, pitchHalfW),
               microFocus.coords[1] * 0.9f, 0)) *
          possessionBias +
      (defensiveFocusPos) * (1.0f - possessionBias);
  float microFocusStrength = (offense_microFocusStrength * possessionBias +
                              defense_microFocusStrength * (1.0f - possessionBias));

  // on own half in possession: little microfocus strength. on own half not in possession: lots of
  // microfocus. other half: the other way around.
  float microFocusSideBias = NormalizedClamp((ballX / pitchHalfW) * -team->GetSide(), -0.7f,
                                             0.7f);  // ball on own half == lower values
  microFocusSideBias = microFocusSideBias * 0.7f + 0.3f;
  float autoMicroFocusStrength =
      (std::pow(microFocusSideBias, 0.8f) * possessionBias +
       (std::pow(1.0f - microFocusSideBias, 0.6f)) * (1.0f - possessionBias));
  microFocusStrength = microFocusStrength * (0.2f + 0.8f * autoMicroFocusStrength);
  // if (team->GetID() == 0) printf("microfocusstrength: %f\n", microFocusStrength);

  // midfield focus. higher == more on opponent's half
  float manualMidfieldFocus =
      (offense_midfieldFocus * possessionBias + defense_midfieldFocus * (1.0f - possessionBias));
  float autoMidfieldFocus = NormalizedClamp((ballX / pitchHalfW) * -team->GetSide(), -0.8f,
                                            0.8f);  // midfield follows ball
  // autoMidfieldFocus *= 0.5f + 0.5f * possessionBias; // auto defensive stance on no possession
  // (deemed too defensive)
  float midfieldFocus = manualMidfieldFocus * 0.7f + autoMidfieldFocus * 0.3f;
  // if (player->GetDebug()) printf("midfieldfocus: %f\n", midfieldFocus);
  float midfieldFocusStrength = (offense_midfieldFocusStrength * possessionBias +
                                 defense_midfieldFocusStrength * (1.0f - possessionBias));
  if (!toggle_yFocus)
    yFocusStrength = 0.0f;
  if (!toggle_microFocus)
    microFocusStrength = 0.0f;
  if (!toggle_midfieldFocus)
    midfieldFocusStrength = 0.0f;

  Vector3 desiredPos = AI_GetAdaptedFormationPosition(
      team->GetMatch(), player, backXBound, frontXBound, lowYBound, highYBound, xFocus,
      xFocusStrength, yFocus, yFocusStrength, microFocus, microFocusStrength, midfieldFocus,
      midfieldFocusStrength, useDynamicFormationPosition);

  // The playing style bends the formation spot: a hole player pushes on while
  // his team attacks, an anchor man sits, a roaming flank tucks inside, and a
  // goal poacher ignores his depth altogether to lurk on the opponent's
  // offside line (roadmap 4D, docs/PES21_IMPORT.md).
  const PlayingStyles::Player style = player->GetPlayerData()->GetPlayingStyle();
  float styleDepth = PlayingStyles::GetDepthOffset(style, teamHasPossession);
  // pes_loop: PES's run-depth term for the two run styles decoded so far (FUN_1405d8b70
  // L650-657); every other style keeps the GF stand-in above.
  if (match->PesLoop() &&
      (style == PlayingStyles::Player::GoalPoacher || style == PlayingStyles::Player::FoxInTheBox))
    styleDepth = pes::offball::StyleRunDepth(
        match->GetPesOffBallConstants(),
        RoleClassOfSlot(team->FormationSlotOf(player)),
        pes::style::StyleWord(style), GetRunState(player),
        match->GetDesignatedPossessionPlayer() == player);
  // Track Back: a midfielder or forward with the card drops deeper when his
  // team loses the ball.
  desiredPos.coords[0] +=
      -team->GetSide() * (styleDepth +
                          PlayerSkills::GetTrackBackDepth(player->GetPlayerData()->GetSkills(),
                                                          teamHasPossession));
  desiredPos.coords[1] += signSide(desiredPos.coords[1]) *
                          PlayingStyles::GetWidthOffset(style, teamHasPossession);
  if (style == PlayingStyles::Player::GoalPoacher && teamHasPossession &&
      AI_GetMindSet(role) > 0.5f) {
    const float oppOffsideLineX =
        AI_GetOffsideLine(match, match->GetMentalImage(0), abs(team->GetID() - 1));
    desiredPos.coords[0] = PlayingStyles::GetPoacherTargetX(style, desiredPos.coords[0],
                                                            oppOffsideLineX, team->GetSide());
  }
  desiredPos.coords[0] = clamp(desiredPos.coords[0], -pitchHalfW, pitchHalfW);
  desiredPos.coords[1] = clamp(desiredPos.coords[1], -pitchHalfH, pitchHalfH);

  return desiredPos;
}

void TeamAIController::CalculateDynamicRoles() {
  std::vector<Player*> players;
  team->GetActivePlayers(players);

  auto iter = players.begin();
  while (iter != players.end()) {
    if ((*iter)->GetFormationEntry().role == e_PlayerRole_GK) {
      players.erase(iter);
      break;
    }
  }

  unsigned int playerNum = players.size();

  // collect adapted formation positions
  std::vector<Vector3> adaptedFormationPositions;
  for (auto* player : players) {
    adaptedFormationPositions.push_back(GetAdaptedFormationPosition(player, false));
  }

  // first make a sorted list on all possible distances between players and formation targets
  std::vector<int> distances;
  for (unsigned int x = 0; x < playerNum; x++) {
    for (unsigned int y = 0; y < playerNum; y++) {
      const Vector3& playerPos = players.at(x)->GetPosition() + players.at(x)->GetMovement() * 0.5;
      const Vector3& formationPos = adaptedFormationPositions.at(y);
      float distance = (playerPos - formationPos).GetLength();
      distances.push_back(int(std::round(distance * 10)));
    }
  }

  std::sort(distances.begin(), distances.end());

  for (unsigned int i = playerNum; i < distances.size(); i += 5) {
    // libhungarian by Cyrill Stachniss, 2004

    hungarian_problem_t p;

    std::vector<int> r(playerNum * playerNum);

    for (unsigned int x = 0; x < playerNum; x++) {
      for (unsigned int y = 0; y < playerNum; y++) {
        const Vector3& playerPos =
            players.at(x)->GetPosition() + players.at(x)->GetMovement() * 0.5;
        const Vector3& formationPos = adaptedFormationPositions.at(y);
        float cost = (playerPos - formationPos).GetLength();
        int intCost = int(std::round(cost * 10));
        if (intCost >= distances.at(i))
          intCost = 50000;
        r[x + y * playerNum] = intCost;
      }
    }

    int** m = array_to_matrix(r.data(), playerNum, playerNum);

    /* initialize the hungarian_problem using the cost matrix*/
    int matrix_size = hungarian_init(&p, m, playerNum, playerNum, HUNGARIAN_MODE_MINIMIZE_COST);

    // fprintf(stderr, "assignment matrix has %d rows and %d columns.\n\n", matrix_size,
    // matrix_size);

    /* some output */
    // fprintf(stderr, "cost-matrix:");
    // hungarian_print_costmatrix(&p);

    /* solve the assignement problem */
    int totalCost = hungarian_solve(&p);

    /* some output */
    // fprintf(stderr, "assignment:");
    // hungarian_print_assignment(&p);

    bool ready = false;
    if (totalCost != -1 && (totalCost < 50000 || i >= distances.size() - 5)) {
      // printf("total cost: %i\n", totalCost);
      //  assign dynamic role with best cost
      for (unsigned int x = 0; x < playerNum; x++) {
        for (unsigned int y = 0; y < playerNum; y++) {
          if ((&p)->assignment[y][x] == 1) {
            FormationEntry formationEntry = players.at(y)->GetFormationEntry();
            players.at(x)->SetDynamicFormationEntry(formationEntry);
          }
        }
      }

      ready = true;
    }

    /* free used memory */
    hungarian_free(&p);
    for (unsigned int row = 0; row < playerNum; row++) {
      free(m[row]);
    }
    free(m);

    if (ready)
      break;
  }
}

void TeamAIController::CalculateManMarking() {
  // FUN_140534a20's assignment (report 3, 14.2), run every frame: PES re-decides
  // the marking each tick, GF re-decided it every 400 ms and capped it at three
  // opponents. Opponents are ordered most-advanced first, projected a sixth of a
  // second ahead; the passes are ported as report 14.2 reduces them - pass 0
  // (who may mark), pass 1 (the first defender on the carrier), pass 2b (who
  // needs a marker at all), pass 6 (continuity) and pass 7 (nearest free
  // defender, stealing from a further one). Passes 3, 4 and 5 (the sheet-driven
  // plan marks and the stopped corner/FK pairing) have no GF counterpart and are
  // left to the set-piece code; PES's at-most-three-opponents cap goes with them.
  std::vector<Player*> own;
  team->GetActivePlayers(own);

  // PES clears every mark target before the passes run.
  for (Player* p : own) p->SetManMarkingID(-1);

  if (!match->IsInPlay() || match->IsInSetPiece()) {
    for (size_t slot = 0; slot < markPendingOpponent.size(); ++slot)
      markPendingOpponent[slot] = -1;
    return;
  }

  const float dir = static_cast<float>(-team->GetSide());

  // The opponents, most advanced first: a man running goal-side sorts in front of
  // where he stands.
  std::vector<Player*> opponents;
  match->GetActiveTeamPlayers(abs(team->GetID() - 1), opponents);
  std::vector<std::pair<float, Player*>> order;
  order.reserve(opponents.size());
  for (Player* o : opponents)
    order.emplace_back(
        pes::OpponentOrderKey(o->GetPosition().coords[0] * dir,
                              o->GetMovement().coords[0] * dir),
        o);
  std::sort(order.begin(), order.end(),
            [](const std::pair<float, Player*>& a, const std::pair<float, Player*>& b) {
              return a.first > b.first;
            });

  // Pass 0: who may take a mark. The keeper never marks and the man carrying
  // the ball is busy with it.
  Player* carrier = match->GetTeam(abs(team->GetID() - 1))->GetDesignatedTeamPossessionPlayer();
  std::vector<Player*> available;
  for (Player* p : own)
    if (p->GetFormationEntry().role != e_PlayerRole_GK && p != carrier) available.push_back(p);

  // Who marked whom last frame, for continuity.
  std::vector<int> previous(markPendingOpponent.size(), -1);
  for (Player* p : own)
    if (p->GetManMarkingID() >= 0 && static_cast<size_t>(p->GetManMarkingID()) < previous.size())
      previous[p->GetManMarkingID()] = p->GetID();

  // This frame's candidate pairing per own player, before the hand-off delay
  // commits it.
  std::vector<int> candidate(own.size(), -1);
  auto candidateSlot = [&own](Player* p) {
    for (size_t i = 0; i < own.size(); ++i)
      if (own[i] == p) return static_cast<int>(i);
    return -1;
  };

  // Pass 1: the first defender on the carrier. The chaser is the man nearest the
  // ball; PES keeps him while he is within 8 m of the carrier, 10 m once he
  // already marks him, which for a ball-side search is the nearest man.
  if (carrier) {
    const Vector3 ballPos = match->GetBall()->Predict(0);
    Player* chaser = nullptr;
    float bestDistance = 1000.0f;
    for (Player* p : available) {
      const float distance = (p->GetPosition() - ballPos).GetLength();
      if (distance < bestDistance) bestDistance = distance, chaser = p;
    }
    if (chaser) {
      candidate[candidateSlot(chaser)] = carrier->GetID();
      available.erase(std::remove(available.begin(), available.end(), chaser), available.end());
    }
  }

  const float stealRadius = defensiveAttackLevel < 3
                                ? match->GetPesDefensiveShapeConstants().stealRadiusLowLevel_m
                                : match->GetPesDefensiveShapeConstants().stealRadius_m;
  for (const std::pair<float, Player*>& entry : order) {
    Player* opp = entry.second;
    if (opp == carrier) continue;
    Player* chosen = nullptr;
    // Pass 6: continuity. His own marker keeps him unless another free defender
    // is already closer to him than the marker is.
    const int lastId = static_cast<size_t>(opp->GetID()) < previous.size()
                           ? previous[opp->GetID()]
                           : -1;
    Player* lastMarker = lastId >= 0 ? match->GetPlayer(lastId) : nullptr;
    if (lastMarker && std::find(available.begin(), available.end(), lastMarker) != available.end()) {
      chosen = lastMarker;
      for (Player* p : available) {
        if (p == lastMarker) continue;
        if ((p->GetPosition() - opp->GetPosition()).GetLength() <
            (lastMarker->GetPosition() - opp->GetPosition()).GetLength()) {
          chosen = nullptr;
          break;
        }
      }
    }
    // Pass 7: the nearest free defender, up to the steal radius.
    if (!chosen) {
      float best = stealRadius;
      for (Player* p : available) {
        const float distance = (p->GetPosition() - opp->GetPosition()).GetLength();
        if (distance < best) best = distance, chosen = p;
      }
    }
    if (chosen) {
      candidate[candidateSlot(chosen)] = opp->GetID();
      available.erase(std::remove(available.begin(), available.end(), chosen), available.end());
    }
  }

  // The hand-off hysteresis (report 3.3): a changed target is committed only once
  // the same new one has been pending for the organiser's delay.
  const unsigned long now = match->GetActualTime_ms();
  const pes::DefensiveShapeConstants& shape = match->GetPesDefensiveShapeConstants();
  const unsigned long delay_ms =
      static_cast<unsigned long>(pes::MarkHandOffFrames(shape, 0)) * 1000 /
      static_cast<unsigned long>(shape.cushionFps);
  for (size_t i = 0; i < own.size(); ++i) {
    const int playerId = own[i]->GetID();
    if (markPendingOpponent.size() <= static_cast<size_t>(playerId)) {
      markPendingOpponent.resize(playerId + 1, -1);
      markPendingSinceTick.resize(playerId + 1, 0);
    }
    const int want = candidate[i];
    if (want == own[i]->GetManMarkingID()) continue;  // unchanged: nothing pending
    if (markPendingOpponent[playerId] != want) {
      markPendingOpponent[playerId] = want;
      markPendingSinceTick[playerId] = now;
    } else if (now - markPendingSinceTick[playerId] >= delay_ms) {
      own[i]->SetManMarkingID(want);
      markPendingOpponent[playerId] = -1;
    }
  }
}

Vector3 TeamAIController::GetMarkSpot(Player* defender, Player* opp) {
  // FUN_140623750 (report 14.3): the goal-side point 6 - 4*ramp(depth) metres
  // from the opponent's predicted position, 2 m further when our team is
  // pressing.
  const pes::DefensiveShapeConstants& k = match->GetPesDefensiveShapeConstants();
  const PlayerImage oppImage = match->GetMentalImage(0)->GetPlayerImage(opp->GetID());
  const Vector3 oppPos = oppImage.position + oppImage.movement * 0.2f;
  const float dir = static_cast<float>(-team->GetSide());
  const float distance =
      pes::MarkDistance_m(k, defensiveShapeField, oppPos.coords[0] * dir, defensivePressing);
  return pes::MarkSpot(oppPos, distance, Vector3(pitchHalfW * team->GetSide(), 0.0f, 0.0f));
}

void TeamAIController::ApplyOffsideTrap(Vector3& position) const {
  // binary 'hard clamp' method
  // if (position.coords[0] * team->GetSide() > offsideTrapX * team->GetSide()) position.coords[0] =
  // offsideTrapX;

  // smooth version

  // area, centered around offsideTrapX, that will be compressed.
  // so the area from [offsideTrapX - areaLength .. offsideTrapX + areaLength] will be compressed
  // into [offsideTrapX .. offsideTrapX + areaLength]
  float areaHalfLength = 2.0f;  // 4.0f;
  float absPosX = position.coords[0] * team->GetSide();
  float absOffsideTrapX = offsideTrapX * team->GetSide();

  if (absPosX > absOffsideTrapX - areaHalfLength) {
    float areaFront = absOffsideTrapX - areaHalfLength;
    float posFromAreaFront = absPosX - areaFront;
    float posFactor = posFromAreaFront / (areaHalfLength * 2.0f);
    posFactor = clamp(posFactor, 0.0f, 1.0f);  // 0.0f == most forward, 1.0f == deepest players
    // posFactor = pow(posFactor, 0.5f); // don't impact players on the non-offside side of the
    // offsideline as much as the other way around

    float absResultPosX =
        areaFront +
        areaHalfLength *
            posFactor;  // this compresses the 2 * areaHalfLength into 1 * areaHalfLength

    position.coords[0] = absResultPosX * team->GetSide();
  }
}

// PES's goal kick: the keeper walks to his stand point before he takes it. The walk is pending until the ladder releases
// the kick (A5 §4.7; pesgoalkick.hpp).
// PES's set-piece takers walk to their spot before they take the restart (A5 §4.7; for a throw-in,
// docs/re/pes-re-throwin-choreography.md 3.1-3.2). The walk is pending until the taker has arrived.
bool TeamAIController::IsPieceTakerWalking(const Player* player) const {
  return pieceWalk && player == taker;
}

// PES's goal kick ladder (sub_142136F00): one tick of it for the armed restart. Its release starts the strike and ends the
// keeper's walk.
bool TeamAIController::StepGoalKickLadder() {
  std::vector<Player*> mates;
  pes::GoalKickLadderInputs in;
  in.lofted = PickGoalKickMate(&mates);
  // ponytail: the restart record's +0x14C4 flag (MI+0x14C4, 0 on a normal goal kick) and the valid special object
  // (MI+0x400) are not in GF's restart record yet, so a goal kick takes the default route: the lofted window at frame 81.
  const pes::GoalKickOrder order = pes::StepGoalKickLadder(
      goalKickLadder, match->GetSimFrame(), pes::GoalKickConstantsFrom(match->GetPesSetPieceTakerConstants()), in);
  if (!order.released) return false;
  goalKickReceiver = order.receiver >= 0 ? mates[order.receiver] : nullptr;
  goalKickStrike.started = true;
  goalKickStrike.from = taker ? taker->GetPosition() : Vector3();
  return true;
}

// The strike (sub_140775780's root-motion warp, carried to the clip's contact): from the release, the keeper's root moves
// onto the contact root and holds it. The kick may go out once the contact tick has passed.
void TeamAIController::AdvanceGoalKickStrike() {
  GoalKickStrike& s = goalKickStrike;
  if (!s.started || s.contact || taker == nullptr) return;
  ++s.elapsed;
  const pes::Vec3 from(s.from.coords[0], s.from.coords[1], s.from.coords[2]);
  const pes::Vec3 to(s.contactRoot.coords[0], s.contactRoot.coords[1], s.contactRoot.coords[2]);
  const pes::Vec3 at = pes::GoalKickStrikeRoot(from, to, s.elapsed, s.ticks);
  taker->OffsetPosition(Vector3(at.x, at.y, at.z) - taker->GetPosition());
  s.contact = s.elapsed >= s.ticks;
}

// The lofted picker's mates: the taker's team's active players, rated by technical highpass (the stand-in for the picker,
// pesgoalkick.hpp). Returns the index the picker names, -1 for none.
int TeamAIController::PickGoalKickMate(std::vector<Player*>* mates) const {
  team->GetActivePlayers(*mates);
  std::vector<float> ratings;
  int takerIndex = -1;
  for (Player* mate : *mates) {
    if (mate == taker) takerIndex = static_cast<int>(ratings.size());
    ratings.push_back(mate->GetStat("technical_highpass"));
  }
  return pes::PickGoalKickReceiver(ratings, takerIndex);
}

namespace {
// The keeper faces the pitch from his end: 90 degrees for the end at -X, turned at the other end (the live keeper's yaw).
// ponytail: the restart record's heading (PES's +0xC, FUN_14050E770) is not decoded, so this constant stands for it.
constexpr float kKeeperFacingPitch_deg = 90.0f;
// The unit direction of that heading at a side, in GF ground coordinates.
Vector3 KeeperFacingPitch(int side) {
  const float radians = -kKeeperFacingPitch_deg * static_cast<float>(side) * (std::acos(-1.0f) / 180.0f);
  return Vector3(0, -1, 0).GetRotated2D(radians);
}
}  // namespace

// SetKick's placement (0x140774AE0) and the strike's contact, from the clip's first hit record (gkgoalkick_instep_0_0,
// 0x6B1). The keeper is placed on the returned stand point when the kick is prepared (pes-re-kicker-standby.md section 4).
Vector3 TeamAIController::PrepareGoalKickStand(const Vector3& spot, Player* taker) {
  constexpr int kGoalKickStrikeMotion = 0x6b1;
  // The keeper strikes with his stronger foot (FUN_1406E6660(player, 0x34) == 1): the mirror of the clip offset and root.
  const bool mirror = match->LeftFootedOf(taker ? taker->GetPlayerData() : nullptr);
  // ponytail: the physique (PES's player +0x3B18) is taken as 1.0.
  constexpr float kPhysique = 1.0f;
  const std::vector<pes::HitRecord>& hits = match->GetPesMbinfo().Hits(kGoalKickStrikeMotion);
  const pes::HitRecord hit = hits.empty() ? pes::HitRecord() : hits.front();
  const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match->GetPesSetPieceTakerConstants());
  // PES's GoalKickStandby::SetKick places him through the shared placer at yaw 0 (docs/re/pes-re-goalkick-taker.md, "He doesn't
  // walk to the ball"), so the stand and the contact root sit in one frame at both ends. The keeper's body facing is a separate
  // stand-in (KeeperFacingPitch), not this placer heading.
  const float heading = 0.0f;
  const pes::Vec3 spotPes(spot.coords[0], spot.coords[1], spot.coords[2]);
  const pes::Vec3 offset = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), kPhysique, mirror, k);
  const pes::Vec3 stand = pes::GoalKickStandPoint(spotPes, heading, offset);
  const pes::Vec3 contact = pes::GoalKickContactRoot(stand, heading, pes::Vec3(hit.rootX, 0.0f, hit.rootZ), mirror, k);
  // ponytail: the placer's height (the spot's plus the clip's) is not carried: GF's players stand on the ground.
  goalKickStrike.contactRoot = Vector3(contact.x, contact.y, 0.0f);
  goalKickStrike.ticks = pes::GoalKickContactTicks(hit.frame, k);
  return Vector3(stand.x, stand.y, 0.0f);
}

// SetKick's placement for a corner (sub_1407331A0; docs/re/pes-re-kicker-standby.md section 4). The run-up clip is the anime
// cell at row 0x509, column 9. Its first ball hit gives the contact offset, and the stand is the spot plus that offset turned
// by the corner heading: the side yaw plus the restart bearing to the penalty spot (FUN_1408B4560, restart kind 4).
Vector3 TeamAIController::PrepareCornerStand(const Vector3& spot, Player* taker, Vector3* lookAt) {
  constexpr int kCornerRunUpRow = 0x509;
  constexpr int kCornerRunUpColumn = 9;
  // The taker strikes with his stronger foot (FUN_1406E6660(player, 0x34) == 1): the mirror of the run-up's offset and
  // heading (Match::LeftFootedOf).
  const bool mirror = match->LeftFootedOf(taker->GetPlayerData());
  // ponytail: the physique (PES's player +0x3B18) is taken as 1.0.
  constexpr float kPhysique = 1.0f;
  const int motion = match->GetPesAnimetable().Cell(kCornerRunUpRow, kCornerRunUpColumn).motionNo;
  const std::vector<pes::HitRecord>& hits = match->GetPesMbinfo().Hits(motion);
  const pes::HitRecord hit = hits.empty() ? pes::HitRecord() : hits.front();
  const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match->GetPesSetPieceTakerConstants());
  const pes::Vec3 spotPes(spot.coords[0], spot.coords[1], spot.coords[2]);
  const pes::Vec3 offset = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), kPhysique, mirror, k);
  const float heading = pes::CornerSetKickHeadingDeg(pes::CornerRestartYawDeg(spotPes, pesPenaltySpotX), mirror);
  const pes::Vec3 stand = pes::GoalKickStandPoint(spotPes, heading, offset);
  const pes::Vec3 look = pes::PlacerLookPoint(stand, heading);
  *lookAt = Vector3(look.x, look.y, 0.0f);
  // ponytail: the placer's height (the spot's plus the clip's) is not carried: GF's players stand on the ground.
  return Vector3(stand.x, stand.y, 0.0f);
}

void TeamAIController::AdvancePieceWalk() {
  if (!pieceWalk || taker == nullptr) return;
  // PES's SeamlessThrowin phase 1 (docs/re/pes-re-throwin-choreography.md 3.2): the pickup clip 0xCF5 plays when the ball, at the
  // clip's contact frame, is within its reach of the taker (the 5 m abort). Until then the walk runs to that predicted ball.
  constexpr int kPickupClipMotion = 0xcf5;
  // PES's lift (phase 4): the lift clip roots him onto the spot over its 14 frames (throwin_seamless_liftupball).
  constexpr unsigned long kLiftTicks = 14;
  const std::vector<pes::HitRecord>& hits = match->GetPesMbinfo().Hits(kPickupClipMotion);
  const unsigned long contactTicks =
      hits.empty() ? 0ul
                   : pes::GoalKickContactTicks(hits.front().frame,
                                               pes::GoalKickConstantsFrom(match->GetPesSetPieceTakerConstants()));
  if (!pieceWarping) {
    if (!pieceCarrying) {
      // PES's reach (FUN_140791EC0 phase 1): the pickup clip plays when its ball-contact root point is less than the reach (5 m,
      // DAT_14259222C) from the taker's root. The root then warps onto that point, so the hands meet the ball at the contact frame.
      // Until then the walk runs to the predicted ball.
      const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match->GetPesSetPieceTakerConstants());
      const pes::HitRecord hit = hits.empty() ? pes::HitRecord() : hits.front();
      // ponytail: the physique (PES's player +0x3B18) is taken as 1.0, and the clip is taken unmirrored (the 000 column).
      constexpr float kPhysique = 1.0f;
      constexpr bool kMirror = false;
      const Vector3 ball = match->GetBall()->Predict(static_cast<int>(contactTicks));
      pieceWalkTarget = ball.Get2D();
      const Vector3 facing = taker->GetDirectionVec();
      const pes::Vec3 offset = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), kPhysique, kMirror, k);
      const pes::Vec3 root = pes::PickupContactRoot(
          pes::Vec3(ball.coords[0], ball.coords[1], ball.coords[2]),
          pes::FacingHeadingDeg(pes::Vec3(facing.coords[0], facing.coords[1], facing.coords[2])), offset);
      const Vector3 rootPoint(root.x, root.y, 0.0f);
      if ((rootPoint - taker->GetPosition()).Get2D().GetLength() >= match->GetPesSetPieceTakerConstants().throwInPickupReach_m)
        return;
      pieceWarping = true;
      pieceWarpTicks = contactTicks > 0 ? contactTicks : 1;
      pieceWarpTo = rootPoint;
    } else {
      // PES's lift trigger (phase 4): |ap - S| < 0.65 m (DAT_14259d5d0) and |x| inside the end line's inset. Until then the carry
      // walks on: PES's MOVE stops him at its arrival radius.
      if (pieceLifted) return;
      const pes::RestartSpotConstants& rk = match->GetPesRestartSpotConstants();
      const Vector3 pos = taker->GetPosition();
      if ((pieceCarrySpot - pos).Get2D().GetLength() >= rk.throwInCarryStop_m ||
          std::fabs(pos.coords[0]) >= pesFieldHalfLength - rk.throwInCarryEndInset_m)
        return;
      pieceWarping = true;
      pieceWarpTicks = kLiftTicks;
      pieceWarpTo = pieceCarrySpot;
    }
  }
  // The root warp of the clip: an even step per tick onto the warp's end (the contact point, or the carry spot). It is the clip's
  // root moving, not a step, so it goes through WarpRoot, not OffsetPosition (a bump booked as movement).
  const float ticksLeft = static_cast<float>(pieceWarpTicks);
  const Vector3 step = (pieceWarpTo - taker->GetPosition()).Get2D() * (1.0f / ticksLeft);
  float yawStep = 0.0f;
  if (pieceCarrying) {
    // The lift's yaw warp (phase 4, docs/re/pes-re-throwin-choreography.md 3.2): over the clip the root turns to PES's heading
    // 180 degrees, or 0 when the spot's PES z is negative, i.e. to face the pitch. PES's z is GF's -y, and a PES heading h is
    // GF's direction (sin h, -cos h); GF's angle of a direction v is atan2(v.x, -v.y). The turn is the same even step per tick.
    constexpr float kLiftHeadingFar_deg = 180.0f;  // the lift's yaw when the spot's PES z is not negative
    constexpr float kLiftHeadingNear_deg = 0.0f;   // and when it is
    const float spotPesZ = -pieceCarrySpot.coords[1];
    const float heading_rad =
        (spotPesZ < 0.0f ? kLiftHeadingNear_deg : kLiftHeadingFar_deg) * static_cast<float>(pi) / 180.0f;
    const Vector3 root = taker->GetRootDirectionVec();
    const float target = std::atan2(std::sin(heading_rad), std::cos(heading_rad));  // atan2(x, -y) of (sin h, -cos h)
    const float now = std::atan2(root.coords[0], -root.coords[1]);
    yawStep = ModulateIntoRange(-pi, pi, target - now) * (1.0f / ticksLeft);
  }
  taker->WarpRoot(step, yawStep);
  --pieceWarpTicks;
  if (pieceWarpTicks == 0) {
    pieceWarping = false;
    if (pieceCarrying) {
      pieceLifted = true;
    } else {
      pieceCarrying = true;
      pieceWalkTarget = pieceCarrySpot;
    }
  }
}

bool TeamAIController::PieceTakerArrived() const {
  if (!pieceWalk || taker == nullptr) return true;
  // The release follows the lift (docs/re/pes-re-throwin-choreography.md 3.2, phase 5: at the lift clip's end the loop starts).
  return pieceLifted;
}

Player* TeamAIController::ThrowInTargetMate(Player* thrower) const {
  // PES aims a throw-in at a receiver inside the touchline (docs/re/pes-re-throwin-choreography.md 6.1): the nearest mate
  // inside the line. ponytail: PES's receiver search (FUN_142135700) and course test are not ported; the nearest mate stands in.
  Player* inside = nullptr;
  float nearest = 10000.0f;
  for (const auto& mate : team->GetAllPlayers()) {
    if (!mate->IsActive() || mate.get() == thrower || std::fabs(mate->GetPosition().coords[1]) >= pesFieldHalfWidth)
      continue;
    const float d = (mate->GetPosition() - thrower->GetPosition()).GetLength();
    if (d < nearest) {
      nearest = d;
      inside = mate.get();
    }
  }
  return inside;
}

void TeamAIController::PinBallAtRelease() {
  if (!IsCarryingBall()) return;
  // PES chooses the carry's release row as the take starts, from the distance to the throw's target (FUN_140792E10, throwin 4.2).
  if (pieceReleaseRow == 0) {
    Player* mate = ThrowInTargetMate(taker);
    const Vector3 to = mate ? mate->GetPosition() : taker->GetPosition();
    pieceReleaseRow = pes::ThrowInCarryReleaseRow((taker->GetPosition().Get2D() - to.Get2D()).GetLength(),
                                                  match->GetPesSetPieceTakerConstants().throwInReleaseShort_m);
  }
  constexpr float kPhysique = 1.0f;  // ponytail: the physique (PES's player +0x3B18) is taken as 1.0, as the pickup does
  constexpr bool kMirror = false;    // ponytail: the 000 column; PES's column is the angle class of the throw (FUN_1406de990)
  const std::vector<pes::HitRecord>& hits = match->GetPesMbinfo().Hits(pieceReleaseRow);
  const pes::HitRecord hit = hits.empty() ? pes::HitRecord() : hits.front();
  const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match->GetPesSetPieceTakerConstants());
  const pes::Vec3 offset = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), kPhysique, kMirror, k);
  const Vector3 pos = taker->GetPosition();
  const Vector3 facing = taker->GetDirectionVec();
  const float heading = pes::FacingHeadingDeg(pes::Vec3(facing.coords[0], facing.coords[1], facing.coords[2]));
  const pes::Vec3 hand = pes::HandBallFromRoot(pes::Vec3(pos.coords[0], pos.coords[1], 0.0f), heading, offset);
  match->GetBall()->ResetSituation(Vector3(hand.x, hand.y, hand.z));
}

namespace {

// PES's role class of a GF role (docs/re/pes-re-throwin-choreography.md 2.1, INF): 0 GK, 1 CB, 2 SB, 3 DMF, 4 CMF, 5 SMF,
// 6 AMF, 7 the forwards. ponytail: stands for RoleClassOfSlot(FormationSlotOf(player)) until RoleArrays lands.
int CeremonyRoleClass(e_PlayerRole role) {
  switch (role) {
    case e_PlayerRole_GK:
      return 0;
    case e_PlayerRole_CB:
      return 1;
    case e_PlayerRole_LB:
    case e_PlayerRole_RB:
      return 2;
    case e_PlayerRole_DM:
      return 3;
    case e_PlayerRole_CM:
      return 4;
    case e_PlayerRole_LM:
    case e_PlayerRole_RM:
      return 5;
    case e_PlayerRole_AM:
      return 6;
    default:
      return 7;
  }
}

// A placed throw-in taker's stand point (PES's ThrowinLoop SetKick, 0x140794280, docs/re/pes-re-throwin-choreography.md 4.1):
// the release key's ball offset from clip 0xcfa's second record (FUN_1406E8CC0 at index 1), its horizontal length outside
// the touchline over the restart point. ponytail: the body scale (PES's player +0x3B18) is taken as 1.
constexpr int kThrowInKeyMotion = 0xcfa;
float ThrowInStandLength_m(Match& match) {
  const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(kThrowInKeyMotion);
  const pes::HitRecord key = hits.size() > 1 ? hits[1] : pes::HitRecord();
  const pes::Vec3 offset = pes::ClipOffsetMetres(pes::Vec3(key.ballX, key.ballY, key.ballZ), 1.0f, false,
                                                 pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants()));
  return std::hypot(offset.x, offset.z);
}

}  // namespace

void TeamAIController::PrepareSetPiece(e_SetPiece setPiece, int takerTeamID) {
  setPieceType = setPiece;
  pieceHeld = false;
  pieceHasLookAt = false;
  pieceWalk = false;
  pieceCarrying = false;
  pieceReleaseRow = 0;
  pieceWarping = false;
  pieceWarpTicks = 0;
  pieceLifted = false;
  bool seamlessRestart = false;
  goalKickLadder = pes::GoalKickLadder();
  goalKickStrike = GoalKickStrike();
  goalKickReceiver = nullptr;
  // Ring 5 records every restart but the throw-in (transitions 1.2): the
  // kick-off timer runs only after a kick-off record.
  if (setPiece != e_SetPiece_None && setPiece != e_SetPiece_ThrowIn) {
    lastRestartWasKickOff = setPiece == e_SetPiece_KickOff;
    inPlaySinceRestart_ms = 0;
  }

  if (takerTeamID == -1)
    assert(setPieceType == e_SetPiece_None);
  if (setPieceType == e_SetPiece_None) {
    // No restart pending: the kicker committed for the last one is released with it. The
    // stand-still branch reads "our kick" from GetPieceTaker (elizacontroller), so a stale
    // keeper here made the side that did not take a later restart skip its walk-out.
    taker = 0;
    return;
  }

  Match* match = team->GetMatch();
  const Vector3 ballPos = match->GetBall()->Predict(0).Get2D();
  const pes::SetPieceTeamConstants& spk = match->GetPesSetPieceTeamConstants();
  // The counts that live in instruction immediates rather than in .rdata.
  const pes::SetPieceTeamCounts& spc = pes::SetPieceTeamCounts();

  const bool isTakerTeam = takerTeamID == team->GetID() ? true : false;
  Team* takerTeam = isTakerTeam ? team : match->GetTeam(abs(team->GetID() - 1));
  // The side whose box a restart may require everyone out of, and the direction
  // that team attacks in - PES's `dir` (choreography §2).
  const int restartingSide = takerTeam->GetSide();
  const int dir = -restartingSide;
  // PES's attack level T+0xB8A4 (choreography 11.6): the per-tick struct on the
  // PES path, the mentality rung off it.
  const int attackLevel =
      match->PesLoop() ? teamTick.b8a4 : pes::AttackLevel((int)GetInstructions().mentality);

  std::vector<Player*> players;
  team->GetActivePlayers(players);
  // Where everyone stood when play stopped. Every placement move starts there.
  std::map<Player*, Vector3> started;
  for (auto* player : players) started[player] = player->GetPosition();


  // PES's placement is a jog (choreography 11.3): a player ends up as far as
  // 16 km/h covers inside the kind's time budget, from where he stood at the
  // stop - never a teleport across the pitch.
  auto place = [this, &spk, &started, ballPos](Player* player, const Vector3& mark,
                                               float budget_s) {
    const Vector3 from = started[player];
    const Vector3 delta = mark - from;
    player->ResetPosition(from + delta * pes::JogFraction(budget_s, delta.GetLength(), spk),
                          ballPos);
  };

  // Law 16, PES's way (choreography 11.3, raw 0x1409297df): at a goal kick, and
  // at a free kick in the restarting team's own box, everyone except the kicker
  // and that side's two back roles is pushed out of the box being kicked from.
  // PES applies this to team-mates too, which is what keeps a striker from
  // standing in the six-yard box waiting for his own goal kick.
  auto pushOutOfBox = [this, ballPos, restartingSide](Player* player, const Vector3& mark,
                                                     bool keepHim) {
    Vector3 at = mark;
    if (SetPieceLaws::IntrudesOnPenaltyArea(at.coords[0], at.coords[1], restartingSide,
                                            pitchHalfW)) {
      float outX = at.coords[0], outZ = at.coords[1];
      SetPieceLaws::ClearingTarget(at.coords[0], at.coords[1], restartingSide, pitchHalfW, &outX,
                                  &outZ);
      at = Vector3(outX, outZ, 0);
    }
    if (!keepHim) return;  // the kicker and the two back roles stay put
    player->ResetPosition(at, ballPos);
  };

  // Keepers are not in the placement list, but they are placed too, and PES
  // gives the restarting side's own keeper an unlimited budget (choreography
  // 11.3) while the other side's is a back-class one.
  std::vector<Player*> keepers;
  team->GetActivePlayers(keepers);
  for (auto* player : keepers) {
    if (player->GetFormationEntry().role != e_PlayerRole_GK) continue;
    const float budget = pes::PlacementBudget_s(
        spk, (int)setPiece, isTakerTeam, e_PlayerRole_GK, false, false, false, isTakerTeam, false,
        false);
    place(player, Vector3(pitchHalfW * team->GetSide() * 0.98, 0, 0), budget);
  }
  players.clear();
  team->GetActivePlayers(players);
  auto iter = players.begin();
  while (iter != players.end()) {
    if ((*iter)->GetFormationEntry().role == e_PlayerRole_GK) {
      iter = players.erase(iter);
    } else {
      iter++;
    }
  }
  started.clear();
  for (auto* player : players) started[player] = player->GetPosition();

  if (isTakerTeam)
    team->SetFadingTeamPossessionAmount(1.5f);
  else
    team->SetFadingTeamPossessionAmount(0.5f);

  // Load custom set-piece formation parameters
  SetPieceParams spParams = SetPieceConfig::Load(team->GetTeamData()->GetDatabaseID(), setPiece);
  float depth = spParams.formation_depth;
  float width = spParams.formation_width;

  // PES's per-kind budget (choreography 11.3, FUN_140927ab0).
  auto budgetFor = [&](Player* player) {
    const e_PlayerRole role = player->GetFormationEntry().role;
    return pes::PlacementBudget_s(spk, (int)setPiece, isTakerTeam, role, false, false, false,
                                  role == e_PlayerRole_GK, false, /*isMarking=*/false);
  };

  switch (setPiece) {
    case e_SetPiece_PeriodEnd:  // schedules exactly like the kick-off it breaks for (§4)
      [[fallthrough]];
    case e_SetPiece_KickOff:
      for (auto* player : players) {
        Vector3 basePos =
            player->GetFormationEntry().position *
            Vector3(-team->GetSide() * pitchHalfW * 0.6, -team->GetSide() * pitchHalfH * 0.6, 0);
        basePos.coords[1] +=
            random(-2.0f, 2.0f);  // to stop people from bumping into each other and such
        basePos.coords[0] *= 0.5;
        basePos.coords[0] += (pitchHalfW * 0.2) * team->GetSide();
        if (basePos.coords[0] * team->GetSide() < 0.5)
          basePos.coords[0] = 0.5 * team->GetSide();  // not allowed to stand on opp side
        if (basePos.GetLength() < 9.4) {              // not allowed to stand in center spot
          basePos.Normalize(Vector3(team->GetSide(), 0, 0));
          basePos *= 9.4;
        }
        place(player, basePos, budgetFor(player));
      }
      break;

    case e_SetPiece_GoalKick:
      for (auto* player : players) {
        float backXBound, frontXBound, lowYBound, highYBound;
        if (isTakerTeam) {
          backXBound = team->GetSide() * pitchHalfW * 0.5;
          frontXBound = -team->GetSide() * pitchHalfW * 0.2;
        } else {
          backXBound = team->GetSide() * pitchHalfW * 0.4;
          frontXBound = -team->GetSide() * pitchHalfW * 0.1;
        }
        lowYBound = -pitchHalfH * 0.7;
        highYBound = pitchHalfH * 0.7;
        Vector3 basePos =
            AI_GetAdaptedFormationPosition(match, player, backXBound, frontXBound, lowYBound,
                                           highYBound, 0, 0, 0, 0, 0, 0, 0, 0, false);
        place(player, basePos, budgetFor(player));
      }
      break;

    case e_SetPiece_Corner: {
      // The defending side's post men (choreography 11.5): the near-post man is
      // the corner-side role 2-4 player with the lowest height + jump + heading,
      // and a far-post man joins him only when the defending team's attack level
      // is 0. PES reads the three as b[0x31] + b[0x2c] + b[0x1d].
      auto aerial = [](Player* player) {
        return player->GetPlayerData()->GetHeight() + player->GetStat("physical_jump") +
               player->GetStat("technical_header");
      };
      Player* nearPost = 0;
      Player* farPost = 0;
      if (!isTakerTeam) {
        int bestIndex = -1;
        float bestScore = 1e9f;
        for (unsigned int i = 0; i < players.size(); i++) {
          Player* player = players.at(i);
          const e_PlayerRole role = player->GetFormationEntry().role;
          const bool sameSide =
              ballPos.coords[1] * player->GetFormationEntry().position.coords[1] > 0.0f;
          const bool narrow = role == e_PlayerRole_LB || role == e_PlayerRole_RB ||
                              role == e_PlayerRole_DM || role == e_PlayerRole_CM;
          const bool wide = narrow || role == e_PlayerRole_LM || role == e_PlayerRole_RM ||
                            role == e_PlayerRole_AM;
          if (!sameSide ? !wide : !narrow) continue;
          if (aerial(player) < bestScore) {
            bestScore = aerial(player);
            nearPost = player;
            bestIndex = static_cast<int>(i);
          }
        }
        (void)bestIndex;
        if (attackLevel < 1) {
          float bestFar = 1e9f;
          for (unsigned int i = 0; i < players.size(); i++) {
            Player* player = players.at(i);
            if (player == nearPost || player->GetFormationEntry().role == e_PlayerRole_GK) continue;
            if (aerial(player) < bestFar) {
              bestFar = aerial(player);
              farPost = player;
            }
          }
        }
      }
      // The attacking side's men in the box. PES path: the men FUN_14051B8B0 places in stages one and
      // two, in placement order, so the box rank is PES's. Switch-off keeps GF's stand-in: the top men
      // by the same aerial score.
      std::vector<Player*> boxAttackers;
      if (isTakerTeam && match->PesLoop()) {
        boxAttackers = PesBoxMen(pes::BoxAttackerCount(attackLevel, spc));
      } else if (isTakerTeam) {
        std::vector<Player*> outfield = players;
        std::sort(outfield.begin(), outfield.end(), [&aerial](Player* a, Player* b) {
          return aerial(a) > aerial(b);
        });
        const int count = std::min<int>(pes::BoxAttackerCount(attackLevel, spc),
                                        static_cast<int>(outfield.size()));
        boxAttackers.assign(outfield.begin(), outfield.begin() + count);
      }
      // The two post spots: 0.5 m off the goal line, the near post 0.31 m inside
      // it and the far post 0.7 m wide of it (live, choreography 11.5).
      const float goalSign = ballPos.coords[0] >= 0.0f ? 1.0f : -1.0f;
      const float zSign = ballPos.coords[1] >= 0.0f ? 1.0f : -1.0f;
      const Vector3 nearMark(goalSign * (pitchHalfW - 0.5f), zSign * 3.35f, 0);
      const Vector3 farMark(goalSign * (pitchHalfW - 0.5f),
                            zSign * -(pesGoalInnerHalfWidth - 0.7f), 0);

      for (unsigned int i = 0; i < players.size(); i++) {
        if (players.at(i) == nearPost) {
          place(players.at(i), nearMark, spk.budgetPostMan_s);
          continue;
        }
        if (players.at(i) == farPost) {
          place(players.at(i), farMark, spk.budgetPostMan_s);
          continue;
        }
        if (std::find(boxAttackers.begin(), boxAttackers.end(), players.at(i)) !=
            boxAttackers.end()) {
          const int rank = static_cast<int>(
              std::find(boxAttackers.begin(), boxAttackers.end(), players.at(i)) -
              boxAttackers.begin());
          // ponytail: PES's authored positionCK_N_V spots are not shipped, so GF
          // lays its own box men out inside the goal area: three across, two
          // deep. The count is PES's; the geometry is this engine's.
          place(players.at(i),
                Vector3(goalSign * (pitchHalfW - 7.0f - 6.0f * static_cast<float>(rank / 3)),
                        (static_cast<float>(rank % 3) - 1.0f) * 4.5f, 0),
                budgetFor(players.at(i)));
          continue;
        }
        float backXBound, frontXBound, lowYBound, highYBound, xFocus, xFocusStrength, yFocus,
            yFocusStrength, midfieldFocus, midfieldFocusStrength;
        if (isTakerTeam) {
          backXBound = -team->GetSide() * pitchHalfW * 0.2;
          frontXBound = -team->GetSide() * pitchHalfW * 0.96;
          xFocus = frontXBound * 0.85;
          xFocusStrength = 0.7f;
          yFocus = ballPos.coords[1] * 0.1f;
          yFocusStrength = 0.7f;
          midfieldFocus = 0.9f;
          midfieldFocusStrength = 0.5f;
        } else {
          backXBound = team->GetSide() * pitchHalfW * 0.98;
          frontXBound = team->GetSide() * pitchHalfW * 0.5;
          xFocus = backXBound * 0.94;
          xFocusStrength = 0.8f;
          yFocus = ballPos.coords[1] * 0.1f;
          yFocusStrength = 0.9f;
          midfieldFocus = 0.1f;
          midfieldFocusStrength = 0.7f;
        }
        lowYBound = -pitchHalfH * 0.6 * width;
        highYBound = pitchHalfH * 0.6 * width;
        Vector3 basePos = AI_GetAdaptedFormationPosition(
            match, players.at(i), backXBound, frontXBound, lowYBound, highYBound, xFocus,
            xFocusStrength, yFocus, yFocusStrength,
            Vector3(ballPos.coords[0] * 0.95f, ballPos.coords[1] * 0.1f, 0), 0.9, midfieldFocus,
            midfieldFocusStrength, false);
        place(players.at(i), basePos, budgetFor(players.at(i)));
      }
      break;
    }

    case e_SetPiece_ThrowIn:
      for (unsigned int i = 0; i < players.size(); i++) {
        float backXBound, frontXBound, lowYBound, highYBound, xFocus, xFocusStrength, yFocus,
            yFocusStrength;
        Vector3 ballPos = match->GetBall()->Predict(0).Get2D();
        if (isTakerTeam) {
          backXBound = clamp(ballPos.coords[0] + 30 * team->GetSide(), -pitchHalfW, pitchHalfW);
          frontXBound = clamp(ballPos.coords[0] + 20 * -team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocus = clamp(ballPos.coords[0] + 4 * -team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocusStrength = 0.4;
          yFocus = ballPos.coords[1] * 0.996;
          yFocusStrength = 0.6;
        } else {
          backXBound = clamp(ballPos.coords[0] + 30 * team->GetSide(), -pitchHalfW, pitchHalfW);
          frontXBound = clamp(ballPos.coords[0] + 15 * -team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocus = clamp(ballPos.coords[0] + 16 * team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocusStrength = 0.2;
          yFocus = ballPos.coords[1] * 0.95;
          yFocusStrength = 0.5;
        }
        lowYBound = -pitchHalfH * 0.75f + ballPos.coords[1] * 0.25f;
        highYBound = pitchHalfH * 0.75f + ballPos.coords[1] * 0.25f;
        Vector3 basePos = AI_GetAdaptedFormationPosition(
            match, players.at(i), backXBound, frontXBound, lowYBound, highYBound, xFocus,
            xFocusStrength, yFocus, yFocusStrength, ballPos, 0.7, 0, 0, false);
        place(players.at(i), basePos, budgetFor(players.at(i)));
      }
      break;

    case e_SetPiece_FreeKick:
      for (unsigned int i = 0; i < players.size(); i++) {
        float backXBound, frontXBound, lowYBound, highYBound, xFocus, xFocusStrength, yFocus,
            yFocusStrength;
        Vector3 ballPos = match->GetBall()->Predict(0).Get2D();
        if (isTakerTeam) {
          float xOffset =
              clamp((ballPos.coords[0] * -team->GetSide()) / pitchHalfW, -1.0, 1.0) * 0.5 +
              0.5;  // 0 == close to our goal, 1 == far from our goal
          backXBound = clamp(team->GetSide() * pitchHalfW * (depth - xOffset * depth), -pitchHalfW,
                             pitchHalfW);
          frontXBound = clamp(team->GetSide() * pitchHalfW * (-0.43f * depth - xOffset * depth),
                              -pitchHalfW, pitchHalfW);
          // -0.43 pushes attackers forward past centre into the opponent half for deep free kicks
          // printf("fb: %f %f\n", backXBound, frontXBound);
          xFocus = clamp(ballPos.coords[0] + 10 * -team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocusStrength = 0.5 + xOffset * 0.2;
          yFocus = ballPos.coords[1] * 0.4;
          yFocusStrength = 0.6 + xOffset * 0.2;
        } else {
          float xOffset =
              clamp((ballPos.coords[0] * -team->GetSide()) / pitchHalfW, -1.0, 1.0) * 0.5 +
              0.5;  // 0 == close to our goal, 1 == far from our goal
          // xOffset *= 40.0;
          backXBound =
              clamp(team->GetSide() * pitchHalfW * (1.0 - xOffset * 0.6), -pitchHalfW, pitchHalfW);
          frontXBound =
              clamp(team->GetSide() * pitchHalfW * (0.5 - xOffset * 0.8), -pitchHalfW, pitchHalfW);
          xFocus = clamp(ballPos.coords[0] + 20 * team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocusStrength = 0.6 - xOffset * 0.4;
          yFocus = ballPos.coords[1] * 0.2;
          yFocusStrength = 0.8 - xOffset * 0.4;
        }
        lowYBound = -pitchHalfH * 0.7 * width;
        highYBound = pitchHalfH * 0.7 * width;
        Vector3 basePos = AI_GetAdaptedFormationPosition(
            match, players.at(i), backXBound, frontXBound, lowYBound, highYBound, xFocus,
            xFocusStrength, yFocus, yFocusStrength, ballPos, 0.4, 0, 0, false);

        // keep distance
        if (!isTakerTeam) {
          if ((basePos - match->GetBall()->Predict(0).Get2D()).GetLength() < 9.15) {
            basePos = match->GetBall()->Predict(0).Get2D() +
                      (basePos - match->GetBall()->Predict(0).Get2D()).GetNormalized() * 9.15;
          }
        }

        place(players.at(i), basePos, budgetFor(players.at(i)));
      }

      // The wall (choreography 5.3 and 11.4, kicker report 3.1): as many men as
      // the size table wants, standing 9.15 m from the ball toward the aim point
      // and half a metre apart along the wall line.
      if (!isTakerTeam) {
        const float goalX = dir * pitchHalfW;
        const float goalDx = goalX - ballPos.coords[0], goalDz = -ballPos.coords[1];
        const float goalD = std::sqrt(goalDx * goalDx + goalDz * goalDz);
        const float ballAngle = std::atan2(std::fabs(ballPos.coords[1]), std::fabs(goalDx)) *
                                180.0f / static_cast<float>(M_PI);
        const int wallSize = pes::WallSize(goalD, ballAngle, ballPos.coords[0],
                                          ballPos.coords[1], dir, pitchHalfW,
                                          pitchHalfW - SetPieceLaws::kAreaDepth,
                                          SetPieceLaws::kAreaHalfWidth, spk, spc);
        // The aim point: the goal centre for a one-man wall, else 1.3 m outside
        // the near post on the ball's side (0x14259bdd8).
        const int ballSide = ballPos.coords[1] >= 0.0f ? 1 : -1;
        Vector3 aim(goalX, 0, 0);
        if (wallSize > 1) aim = Vector3(goalX, ballSide * (pesGoalInnerHalfWidth + spk.wallNearPost_m), 0);
        Vector3 toAim = aim - ballPos;
        toAim.Normalize(0);
        // The wall's yaw is perpendicular to the ball's bearing, and the members
        // are spread 0.5 m apart about the centre with the first on it.
        Vector3 along(-toAim.coords[1], toAim.coords[0], 0);
        std::vector<Player*> closest;
        AI_GetClosestPlayers(team, ballPos, closest, wallSize);
        for (unsigned int i = 0; i < closest.size(); i++) {
          const float offset = (static_cast<float>(i) - (closest.size() - 1) * 0.5f) * spk.wallSpacing_m;
          place(closest.at(i),
                ballPos + toAim * spk.wallDistance_m + along * offset,
                // a wall man has no budget at all (PES passes FLT_MAX)
                -1.0f);
        }
      }

      break;

    case e_SetPiece_Penalty:
      for (unsigned int i = 0; i < players.size(); i++) {
        float backXBound, frontXBound, lowYBound, highYBound, xFocus, xFocusStrength, yFocus,
            yFocusStrength;
        Vector3 ballPos = match->GetBall()->Predict(0).Get2D();
        if (isTakerTeam) {
          backXBound = clamp(ballPos.coords[0] + 50 * team->GetSide(), -pitchHalfW, pitchHalfW);
          frontXBound = clamp(ballPos.coords[0] + 10 * -team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocus = ballPos.coords[0];
          xFocusStrength = 0.6;
          yFocus = 0.0;
          yFocusStrength = 0.8;
        } else {
          backXBound = clamp(ballPos.coords[0] + 11 * team->GetSide(), -pitchHalfW, pitchHalfW);
          frontXBound = clamp(ballPos.coords[0] + 20 * -team->GetSide(), -pitchHalfW, pitchHalfW);
          xFocus = ballPos.coords[0];
          xFocusStrength = 1.0;
          yFocus = 0.0;
          yFocusStrength = 1.0;
        }
        lowYBound = -pitchHalfH * 0.8;
        highYBound = pitchHalfH * 0.8;
        Vector3 basePos = AI_GetAdaptedFormationPosition(
            match, players.at(i), backXBound, frontXBound, lowYBound, highYBound, xFocus,
            xFocusStrength, yFocus, yFocusStrength, 0, 0, 0, 0, false);

        // outside the box
        signed int penaltySide = (match->GetBall()->Predict(0).coords[0] < 0) ? -1 : 1;
        if (basePos.coords[0] * penaltySide > pitchHalfW - 16.5 - 0.5)
          basePos.coords[0] = (pitchHalfW - 16.5 - 0.5) * penaltySide;

        // outside penalty arc as well
        if ((basePos - match->GetBall()->Predict(0).Get2D()).GetLength() < 9.15 + 0.5) {
          basePos = match->GetBall()->Predict(0).Get2D() +
                    (basePos - match->GetBall()->Predict(0).Get2D()).GetNormalized() * (9.15 + 0.5);
        }

        place(players.at(i), basePos, budgetFor(players.at(i)));
      }
      break;

    default:
      for (unsigned int i = 0; i < players.size(); i++) {
        Vector3 basePos =
            players.at(i)->GetFormationEntry().position *
            Vector3(-team->GetSide() * pitchHalfW * 0.7, -team->GetSide() * pitchHalfH * 0.7, 0);

        place(players.at(i), basePos, budgetFor(players.at(i)));
      }
      break;
  }

  if (isTakerTeam) {
    // Who takes it (kicker report 1): the free-kick chooser reads the sheet's designated table
    // (TeamSheet::Designated, T+0xa244), not the takers at T+0x1454, which it never reads. GF
    // has no per-player set-piece ratings (playerdata+0x96), so the suitability below stands in.
    // ponytail: PES rates six kinds per player. GF's nearest stats stand in -
    // long free kick -> curl, short -> short pass, the second kicker -> tight
    // possession, both corners -> curl, penalty -> set piece - so a player who
    // is a good long-range curler is also GF's best corner taker. The ceiling
    // is that GF cannot tell a left-footed corner taker from a right-footed one.
    auto suitability = [this](Player* player, e_SetPiece kind) {
      if (kind == e_SetPiece_FreeKick) return player->GetStat("technical_curl");
      if (kind == e_SetPiece_Penalty) return player->GetStat("technical_setpiece");
      if (kind == e_SetPiece_Corner) return player->GetStat("technical_curl");
      return player->GetStat("technical_setpiece");
    };
    std::vector<Player*> onPitch;
    team->GetActivePlayers(onPitch);
    std::vector<Player*>::const_iterator keep = onPitch.begin();
    while (keep != onPitch.end()) {
      if ((*keep)->GetFormationEntry().role == e_PlayerRole_GK)
        keep = onPitch.erase(keep);
      else
        keep++;
    }
    // A corner never goes to a striker (kicker report 1.1: role != 9), and a
    // penalty never to a keeper, which the list above already excludes.
    if (setPiece == e_SetPiece_Corner) {
      std::vector<Player*>::const_iterator drop = onPitch.begin();
      while (drop != onPitch.end()) {
        if ((*drop)->GetFormationEntry().role == e_PlayerRole_CF)
          drop = onPitch.erase(drop);
        else
          drop++;
      }
    }

    if (setPiece == e_SetPiece_FreeKick) {
      // FUN_1404a8ad0 over every player of this side, keeper included. A candidate's index is
      // his slot in the team's player list, the id the designated values name.
      const std::vector<std::unique_ptr<Player>>& roster = team->GetAllPlayers();
      std::vector<pes::SetPieceCandidate> candidates;
      for (unsigned int i = 0; i < roster.size(); i++) {
        Player* player = roster.at(i).get();
        pes::SetPieceCandidate c;
        c.index = static_cast<int>(i);
        c.role = player->GetFormationEntry().role;
        c.suitability = suitability(player, setPiece);
        // ponytail: PES's slot target (T+0x4b4c+12i) has no decoded writer; the formation point
        // the default placement uses stands in for it.
        const Vector3 target = player->GetFormationEntry().position *
                               Vector3(-team->GetSide() * pitchHalfW * 0.7,
                                       -team->GetSide() * pitchHalfH * 0.7, 0);
        c.targetX_m = target.coords[0];
        c.targetZ_m = target.coords[1];
        c.onPitch = player->IsActive();
        candidates.push_back(c);
      }
      pes::FreeKickInput in;
      in.x_m = ballPos.coords[0];
      in.z_m = ballPos.coords[1];
      in.dir = static_cast<float>(dir);
      in.halfLength_m = pitchHalfW;
      in.penaltyX_m = pitchHalfW - SetPieceLaws::kAreaDepth;
      in.penaltyHalfZ_m = SetPieceLaws::kAreaHalfWidth;
      // ponytail: PES's tactics import writes the designated roster values (S+0x143c, T+0xa244).
      // GF's sheet keeps the install's default (DefaultTeamSheet) until that import is ported.
      in.designated = {teamRecord.sheet.Designated(0), teamRecord.sheet.Designated(1), teamRecord.sheet.Designated(2)};
      // ponytail: PES's MI+0x116c == 3, the restart phase that reuses the published kicker, has no
      // GF field, so that step stays off.
      const int pick = pes::FreeKickTaker(candidates, in, spk);
      taker = pick >= 0 ? roster.at(pick).get() : nullptr;
    } else if (setPiece == e_SetPiece_GoalKick) {
      // PES gives a goal kick to the keeper outright (choreography §2, the slot
      // whose role is 0); the fallback below is only for a team with no keeper
      // registered in goal.
      std::vector<Player*> keepers2;
      team->GetActivePlayers(keepers2);
      for (unsigned int i = 0; i < keepers2.size(); i++) {
        if (keepers2.at(i)->GetFormationEntry().role == e_PlayerRole_GK) {
          taker = keepers2.at(i);
          break;
        }
      }
    } else if (setPiece == e_SetPiece_ThrowIn && match->PesLoop()) {
      // PES's seamless throw-in taker (FUN_14044e230, docs/re/pes-re-throwin-choreography.md 2.2): the ball-arrival order,
      // the keeper test, the fps/3 bias and the ball and reach gates (EV-TI-0007, 3.1). When no candidate qualifies, the
      // ceremony taker takes it (FUN_1404a93a0, 2.1), placed rather than walking.
      // ponytail: PES's arrival list (AIctx+0x2470c) is not decoded: GF's time-to-ball is clamped to one second for every
      // man (UpdatePossessionStats), so the arrival here is the distance to the ball at the man's top speed.
      const std::vector<std::unique_ptr<Player>>& roster = team->GetAllPlayers();
      const float fps = static_cast<float>(Match::kFramesPerSecond);
      const Vector3 restart = match->GetReferee()->GetBuffer().restartPos;
      const Vector3 ball3 = match->GetBall()->Predict(0);
      std::vector<pes::ArrivalRank> ranked;
      std::vector<pes::CeremonyMan> men;
      for (unsigned int i = 0; i < roster.size(); i++) {
        Player* player = roster.at(i).get();
        if (!player->IsActive()) continue;
        pes::ArrivalRank r;
        r.slot = static_cast<int>(i);
        // ponytail: stands for RoleClassOfSlot(FormationSlotOf(player)) == 0 (RoleArrays class 0, the keeper).
        // Becomes that line when MergeRA (RoleArrays) lands; GF's formation role stands in until then.
        r.keeper = player->GetFormationEntry().role == e_PlayerRole_GK;
        r.distanceToBall_m = (player->GetPosition() - ballPos).GetLength();
        const float topSpeed_mps = std::max(player->GetMaxVelocity(), 0.1f);
        r.arrivalFrames = r.distanceToBall_m / topSpeed_mps * fps;
        ranked.push_back(r);
        // ponytail: the flank is the man's current side (PES's slot base z is not decoded here).
        pes::CeremonyMan man;
        man.slot = static_cast<int>(i);
        man.roleClass = CeremonyRoleClass(player->GetFormationEntry().role);
        man.distanceToRestart_m = (player->GetPosition() - restart).Get2D().GetLength();
        man.sameFlank = (player->GetPosition().coords[1] > 0.0f) == (restart.coords[1] > 0.0f);
        men.push_back(man);
      }
      std::stable_sort(ranked.begin(), ranked.end(),
                       [](const pes::ArrivalRank& a, const pes::ArrivalRank& b) {
                         return a.arrivalFrames < b.arrivalFrames;
                       });
      const bool ballGate = pes::SeamlessBallGate(match->GetBall()->GetMovement().GetLength() * 3.6f, ball3.coords[2],
                                                  restart.coords[0] - ball3.coords[0]);
      const int pick = ballGate ? pes::SeamlessThrowInTaker(ranked, fps, spc) : -1;
      seamlessRestart = pick >= 0;
      const int slot = seamlessRestart ? pick : pes::CeremonyThrowInTaker(men);
      taker = slot >= 0 ? roster.at(slot).get() : nullptr;
    } else {
      // A corner, a penalty or anything else without a sheet: the best
      // suitability, first on ties (FUN_140a621f0's shape).
      float best = -1.0f;
      taker = onPitch.empty() ? 0 : onPitch.front();
      for (unsigned int i = 0; i < onPitch.size(); i++) {
        const float rating = suitability(onPitch.at(i), setPiece);
        if (rating > best) {
          best = rating;
          taker = onPitch.at(i);
        }
      }
    }

    // The closest man stands in for a restart with no taker, except the PES throw-in, which has its ceremony pick above.
    if (taker == 0 && !(setPiece == e_SetPiece_ThrowIn && match->PesLoop()))
      taker = AI_GetClosestPlayer(team, ballPos);
    // A keeper who ends up with a restart is placed where that restart wants him, not where the outfield placement
    // put him. On the PES path a goal kick keeper runs to the ball instead (below).
    if (taker && taker->GetFormationEntry().role == e_PlayerRole_GK &&
        ((setPiece == e_SetPiece_GoalKick && !match->PesLoop()) || setPiece == e_SetPiece_FreeKick)) {
      taker->ResetPosition(
          Vector3(ballPos.coords[0] - team->GetSide() * 1.5f, ballPos.coords[1], 0), ballPos);
    }

    if (setPiece == e_SetPiece_GoalKick && match->PesLoop()) {
      // PES's goal kick (SetKick 0x140774AE0, pesgoalkick.hpp): the keeper is placed on his stand point when the kick is
      // prepared and held there until the ladder releases the kick (pes-re-kicker-standby.md section 4). The strike's
      // contact is fixed here.
      // ponytail: the stand point is set from the restart spot at prepare; a ball moved in the stoppage is not followed.
      const Vector3 stand = PrepareGoalKickStand(ballPos, taker);
      if (taker) {
        // The keeper faces the pitch from his end and holds that heading while he waits (PES's standby plays with the
        // SetKick yaw).
        const Vector3 facing = stand + KeeperFacingPitch(team->GetSide());
        taker->ResetPosition(stand, facing);
        pieceHeld = true;
        pieceHasLookAt = true;
        pieceLookAt = facing;
      }
    } else if (setPiece == e_SetPiece_ThrowIn && match->PesLoop()) {
      // PES's throw-in taker. A seamless candidate (path A) walks to the ball, picks it up and carries it to the throw
      // spot (3.1-3.2). A ceremony taker (path B) is placed on his SetKick stand point outside the line and holds it until
      // the release (pes-re-throwin-choreography.md 4.1).
      const pes::Spot carry =
          pes::ThrowInCarrySpot(ballPos.coords[0], ballPos.coords[1], dir, match->GetPesRestartSpotConstants());
      pieceWalk = seamlessRestart;
      pieceCarrying = false;
      pieceWalkTarget = ballPos;
      pieceCarrySpot = Vector3(carry.x, carry.z, 0.0f);
      if (!seamlessRestart && taker) {
        const Vector3 restart = match->GetReferee()->GetBuffer().restartPos;
        const float outward = restart.coords[1] > 0.0f ? 1.0f : -1.0f;
        const float standIn_m = ThrowInStandLength_m(*match);
        taker->ResetPosition(Vector3(restart.coords[0], restart.coords[1] + outward * standIn_m, 0.0f), ballPos);
        pieceHeld = true;
      }
    } else if (setPiece == e_SetPiece_ThrowIn || setPiece == e_SetPiece_KickOff) {
      taker->ResetPosition(ballPos + ballPos.GetNormalized(Vector3(0, -team->GetSide(), 0)) * 0.3f,
                           ballPos);
    } else if (setPiece == e_SetPiece_FreeKick) {
      taker->ResetPosition(ballPos + Vector3(team->GetSide(), 0, 0) * 2.3f, ballPos);
    } else if (setPiece == e_SetPiece_Corner && match->PesLoop()) {
      // PES's corner taker (SetKick, pesgoalkick.hpp): placed on his stand point when the corner is prepared, facing the
      // corner heading, and held there until the release (pes-re-kicker-standby.md section 4).
      // ponytail: the stand point is set from the restart spot at prepare; a ball moved in the stoppage is not followed.
      if (taker) {
        Vector3 lookAt;
        const Vector3 stand = PrepareCornerStand(ballPos, taker, &lookAt);
        taker->ResetPosition(stand, lookAt);
        pieceHeld = true;
        pieceHasLookAt = true;
        pieceLookAt = lookAt;
      }
    } else {
      taker->ResetPosition(ballPos + ballPos.GetNormalized(Vector3(0, -team->GetSide(), 0)) * 2.3f,
                           ballPos);
    }
    if (setPiece == e_SetPiece_ThrowIn && taker) {
      taker->SelectRetainAnim();
    }
    if (setPiece == e_SetPiece_Penalty) {
      taker->ResetPosition(ballPos + Vector3(team->GetSide(), 0, 0) * 3.0, ballPos);
    }

  } else
    taker = 0;

  // Law 16, PES's way (choreography 11.3): at a goal kick, and at a free kick
  // in the restarting team's own box, everyone but the kicker and that side's
  // two back roles ends up outside the box being kicked from. This satisfies
  // the referee's own clearing gate on both teams at once, which is what PES
  // does - and what GF used to leave to a walk.
  for (auto* player : players) {
    const e_PlayerRole role = player->GetFormationEntry().role;
    // PES's roles 0 and 1 are the keeper and the centre back; GF splits the
    // full backs off the centre backs, and PES's role 2 (the full back) is
    // pushed out, so only GF's GK and CB may stay in the box.
    const bool keepHim = player == taker ||
                         (isTakerTeam && (role == e_PlayerRole_GK || role == e_PlayerRole_CB));
    pushOutOfBox(player, player->GetPosition(), keepHim);
  }

  // keep distance from ball
  float minDistance = 2.0;
  if (!isTakerTeam)
    minDistance = 5.0;
  for (auto* player : players) {
    if (player != taker) {
      Vector3 toBall = player->GetPosition() - ballPos;
      float ballDistance = (toBall).GetLength();
      if (ballDistance < minDistance)
        player->ResetPosition(player->GetPosition() + toBall.GetNormalized(0) * minDistance, ballPos);
    }
  }
}

// --- PES's team concepts (docs/re/pes-re-tactics.md 2.1, pes-re-transitions.md 2-3)

namespace {

constexpr float kMsPerSecond = 1000.0f;
// FUN_140a74ec0 returns 0xffffffff frames when the newest restart was not a
// kick-off; the caller reads it unsigned, i.e. "forever".
constexpr float kNoKickOff_s = 1e9f;

// The PES concept ids this port runs, in the selector's ascending walk.
constexpr int kPortedConcepts[] = {pes::kConceptSideCounter, pes::kConceptForeCheckPress,
                                   pes::kConceptForeCheckLineDown, pes::kConceptForeCheckLineUp,
                                   pes::kConceptTransitionChase,
                                   pes::kConceptRetreatBlockCreate};

// P(c)+0x5d0 in {LONG_PASS, CENTERING, EARLY_CROSS, SHORT_PASS, THROUGH_PASS,
// SHOOT}: GF's kick function types.
bool IsKicking(Player* p) {
  switch (p->GetCurrentFunctionType()) {
    case e_FunctionType_ShortPass:
    case e_FunctionType_LongPass:
    case e_FunctionType_HighPass:
    case e_FunctionType_Shot:
      return true;
    default:
      return false;
  }
}

// PES's DIVE / STAGGER / FALL_DOWN anime ids. ponytail: GF's only on-the-ground
// function is the slide; a stagger has no GF state to read.
bool IsDown(Player* p) { return p->GetCurrentFunctionType() == e_FunctionType_Sliding; }

// Heading in PES's convention (0 = +y, 90 = +x).
float HeadingOf(const Vector3& v) { return std::atan2(v.coords[0], v.coords[1]) * 180.0f / pi; }


}  // namespace

const TeamAIController::ConceptOrder* TeamAIController::GetConceptOrder(const Player* player) const {
  auto it = conceptOrders.find(player);
  return it == conceptOrders.end() ? nullptr : &it->second;
}

void TeamAIController::RunConcepts() {
  // FUN_140579840. ponytail: the registry-15 stop flag, the difficulty row 0x21
  // cap and the MI+0x116c == 2 early-out have no GF counterpart; every ported
  // concept tests "in play" itself.
  const pes::TeamConceptConstants& k = match->GetPesTeamConceptConstants();
  const unsigned long now = match->GetActualTime_ms();
  const bool own = teamHasPossession;
  int runningAttack = -1;
  if (own)
    for (int id : kPortedConcepts)
      if (id <= pes::kLastAttackingConcept && conceptRunning[id]) {
        runningAttack = id;
        break;
      }
  for (int id : kPortedConcepts) {
    if (!k.enabled[id]) continue;
    const bool attacking = id <= pes::kLastAttackingConcept;
    if (own != attacking) {
      StopConcept(id);
      continue;
    }
    // One attacking concept at a time: every PES concept's slot 6
    // (interruptible) is the false stub 0x141863350.
    if (attacking && runningAttack >= 0 && runningAttack != id) continue;
    if (!pes::ConceptGate(id, teamRecord.sheet)) {
      StopConcept(id);
      continue;
    }
    if (!conceptRunning[id]) {
      if (!StartConcept(id)) continue;
      conceptRunning[id] = true;
      conceptStart_ms[id] = now;
    } else if ((now - conceptStart_ms[id]) / kMsPerSecond > k.endTestAfter_s && ConceptEnded(id)) {
      StopConcept(id);
      continue;
    }
    ExecuteConcept(id);
  }
}

void TeamAIController::StopConcept(int id) {
  // FUN_14057a3e0: clear the flag and release every man on this concept's order.
  conceptRunning[id] = false;
  for (auto it = conceptOrders.begin(); it != conceptOrders.end();)
    it = it->second.conceptId == id ? conceptOrders.erase(it) : std::next(it);
  if (id == pes::kConceptTransitionChase) chaseChaser = chaseSecond = chaseCarrier = nullptr;
  if (id == pes::kConceptSideCounter) counterRunner = nullptr, counterSupporters = {};
}

pes::TransitionChaseInput TeamAIController::ChaseInput(Player* carrier, std::vector<Player*>* men) {
  pes::TransitionChaseInput in;
  in.inPlay = match->IsInPlay() && !match->IsInSetPiece();
  in.sincePossessionChange_s = (match->GetActualTime_ms() - possessionChange_ms) / kMsPerSecond;
  in.dir = static_cast<float>(-team->GetSide());
  if (!carrier) return in;
  in.carrier = carrier->GetPosition();
  in.carrierKicking = IsKicking(carrier);
  // FUN_140575390: our MATCH_UP man, else the man nearest the carrier. GF's
  // engaging defender is its designated possession player.
  Player* chaser = team->GetDesignatedTeamPossessionPlayer();
  team->GetActivePlayers(*men);
  std::stable_sort(men->begin(), men->end(), [&](Player* a, Player* b) {
    return (a->GetPosition() - in.carrier).GetLength() < (b->GetPosition() - in.carrier).GetLength();
  });
  for (Player* p : *men) {
    pes::ChaseMan m;
    m.pos = p->GetPosition();
    m.falling = IsDown(p);
    // BASE_POSITION / MARK / PASS_COURSE_CUT: not the keeper, not the engaging
    // defender, not on another concept's order.
    const ConceptOrder* order = GetConceptOrder(p);
    m.available = p != chaser && p->GetFormationEntry().role != e_PlayerRole_GK &&
                  (!order || order->conceptId == pes::kConceptTransitionChase);
    if (p == chaser) in.chaser = static_cast<int>(in.men.size());
    in.men.push_back(m);
  }
  return in;
}

pes::SideCounterInput TeamAIController::CounterInput(std::vector<Player*>* men) {
  pes::SideCounterInput in;
  const unsigned long now = match->GetActualTime_ms();
  // T+0xa508 != 0 (counter-trigger 1.3): in play and in possession. ponytail:
  // the stale-ball veto and the keeper-hold restart path are not modelled.
  in.attackAllowed = match->IsInPlay() && !match->IsInSetPiece() && teamHasPossession;
  in.sincePossessionChange_s = (now - possessionChange_ms) / kMsPerSecond;
  in.sinceKickoff_s = lastRestartWasKickOff ? inPlaySinceRestart_ms / kMsPerSecond : kNoKickOff_s;
  in.dir = static_cast<float>(-team->GetSide());
  in.ball = match->GetBall()->Predict(0).Get2D();
  Player* retainer = match->GetBallRetainer();
  // FUN_140a53f90: a keeper holds the ball. ponytail: GF's keeper retaining the
  // ball stands in for the hold.
  in.keeperHolds = retainer && retainer->GetFormationEntry().role == e_PlayerRole_GK;
  Player* holder = retainer && retainer->GetTeam() == team ? retainer : team->GetDesignatedTeamPossessionPlayer();
  const int opp = abs(team->GetID() - 1);
  in.oppLineX = AI_GetOffsideLine(match, match->GetMentalImage(0), opp);
  std::vector<Player*> opponents;
  match->GetActiveTeamPlayers(opp, opponents);
  for (Player* o : opponents)
    if (o->GetFormationEntry().role != e_PlayerRole_GK && std::fabs(o->GetPosition().coords[1]) <= pitchHalfH)
      in.opponents.push_back(o->GetPosition());
  in.attackingArea = teamRecord.sheet.Toggle(pes::kAttackingArea);
  team->GetActivePlayers(*men);
  for (Player* p : *men) {
    pes::CounterMan m;
    m.pos = p->GetPosition();
    const int cls = RoleClassOfSlot(team->FormationSlotOf(p));
    m.wideOrForward = cls > pes::roles::kRunnerClassAbove;
    m.supportEligible = cls > pes::roles::kSupporterClassAbove;
    float nearest = std::numeric_limits<float>::max();
    for (Player* o : opponents) {
      const Vector3 d = o->GetPosition() - m.pos;
      nearest = std::min(nearest, d.coords[0] * d.coords[0] + d.coords[1] * d.coords[1]);
    }
    m.nearestOpponentSq_m2 = nearest;
    if (p == holder) {
      in.holder = static_cast<int>(in.men.size());
      in.holderHeading_deg = HeadingOf(p->GetDirectionVec());
    }
    in.men.push_back(m);
  }
  return in;
}

pes::DefenceConceptInput TeamAIController::DefenceInput() {
  pes::DefenceConceptInput in;
  in.inPlay = match->IsInPlay() && !match->IsInSetPiece();
  in.sincePossessionChange_s = (match->GetActualTime_ms() - possessionChange_ms) / kMsPerSecond;
  in.dir = static_cast<float>(-team->GetSide());
  in.ball = match->GetBall()->Predict(0).Get2D();
  Player* retainer = match->GetBallRetainer();
  // FUN_140a55d40(ball, side, 2): the opponents own the ball.
  in.opponentsHoldBall = retainer ? retainer->GetTeam() != team : oppTeamHasPossession;
  in.keeperHolds = retainer && retainer->GetFormationEntry().role == e_PlayerRole_GK;
  in.attackLevelZero = match->PesLoop()
                           ? teamTick.b8a3
                           : instructions.mentality == TeamInstructions::e_Mentality_AllOutDefence;
  if (retainer && retainer->GetTeam() != team) {
    // Anime 8 LONG_PASS: GF's long and high pass functions.
    const e_FunctionType f = retainer->GetCurrentFunctionType();
    in.holderLongPass = f == e_FunctionType_LongPass || f == e_FunctionType_HighPass;
    in.holderHeading_deg = HeadingOf(retainer->GetDirectionVec());
    in.holderOpponent = true;
    in.holder = retainer->GetPosition();
  }
  // FUN_14045bfe0's per-team block: front = the most advanced man, back
  // (MA+0x40) = the deepest outfielder, mid (MA+0x3c) = their mean. Default
  // back is our goal line. ponytail: PES walks its depth list
  // (FUN_140a71500); GF takes the extremes of the same positions.
  Player* presser = team->GetDesignatedTeamPossessionPlayer();
  std::vector<Player*> men;
  team->GetActivePlayers(men);
  float front = -in.dir * pitchHalfW, back = -in.dir * pitchHalfW;
  bool anyFront = false, anyBack = false;
  for (Player* p : men) {
    const float x = p->GetPosition().coords[0];
    if (!anyFront || in.dir * x > in.dir * front) front = x, anyFront = true;
    in.allMen.push_back(p->GetPosition());
    if (p->GetFormationEntry().role == e_PlayerRole_GK) continue;
    if (!anyBack || in.dir * x < in.dir * back) back = x, anyBack = true;
    if (p == presser) in.presser = static_cast<int>(in.mates.size());
    in.mates.push_back(p->GetPosition());
  }
  in.ownLine = back;
  in.teamMid = (front + back) * 0.5f;
  in.measuredBack = measuredBackLine, in.measuredMid = measuredMidLine;
  in.backBand = backBand, in.midBand = midBand;
  return in;
}

bool TeamAIController::IsPressing(const Player* player) const {
  return conceptRunning[pes::kConceptForeCheckPress] && player == team->GetDesignatedTeamPossessionPlayer();
}

Player* TeamAIController::GetTeamPressurePlayer() {
  return conceptRunning[pes::kConceptForeCheckPress] ? team->GetDesignatedTeamPossessionPlayer()
                                                     : nullptr;
}

Player* TeamAIController::GetLoosePressMan() {
  const int side = team->GetID();
  const auto& players = team->GetAllPlayers();
  for (int k = 0; k < 11 && k < static_cast<int>(players.size()); ++k) {
    if (pressState.role[side * 11 + k] == pes::kPressState) return players[k].get();
  }
  return nullptr;
}

bool TeamAIController::IsLoosePresser(const PlayerBase* p) const {
  const auto& players = team->GetAllPlayers();
  for (int k = 0; k < 11 && k < static_cast<int>(players.size()); ++k) {
    if (players[k].get() == p) return pes::PressIsLoosePresser(pressState.role, team->GetID(), k);
  }
  return false;
}

bool TeamAIController::PressOrDelay(const PlayerBase* p) {
  const bool teamPressure = GetTeamPressurePlayer() == p;
  if (!match->PesLoop()) return teamPressure;
  const auto& players = team->GetAllPlayers();
  for (int k = 0; k < 11 && k < static_cast<int>(players.size()); ++k) {
    if (players[k].get() == p) return pes::PressOrDelayFlag(teamPressure, pressState.role, team->GetID(), k);
  }
  return teamPressure;
}

void TeamAIController::PesPressTick() {
  // FUN_140535C60 order: the coordinator (FUN_140634470) runs before the sweep, so the writer sees this
  // tick's second presser and keeps him (PressWriterWord).
  PesSecondPresserTick();
  PesLooseWriterTick();
}

void TeamAIController::PesLooseWriterTick() {
  const int side = team->GetID();
  const uint32_t stamp = static_cast<uint32_t>(match->GetSimFrame());
  // Renewal + reset is the lifecycle (press-life 10): whoever is not picked below is reset to NOTHING,
  // so a presser stops the tick the writer drops him, except the coordinator's second presser, whose life
  // the coordinator owns (PressWriterWord: PES FUN_140535C60 skips a slot already PRESS). PES path only:
  // the GF path keeps its designated chase untouched.
  auto resetAll = [&]() {
    for (int k = 0; k < 11; ++k) {
      const int man = side * 11 + k;
      pressState.SetAction(man, pes::PressWriterWord(man, pes::kPressNoMan, coordState.second), stamp);
    }
    for (int i = 0; i < 4; ++i) pressState.pressSlot[i] = -1;
  };
  if (!match->PesLoop() || !match->IsInPlay() || match->IsInSetPiece() ||
      match->GetBallRetainer() != nullptr) {
    resetAll();
    return;
  }
  // Owned is not loose. ponytail: PES reads its carrier (MA+0x2601c); GF's
  // retainer misses dribblers, so ownership is proximity plus the newest touch
  // being his. Any owner's ball stops both sides' loose pick.
  const pes::NewestEventView newest = pes::NewestEvent(match->GetPesEvents());
  Player* newestMan = newest.present ? match->GetPesActorPlayer(newest.actor) : nullptr;
  for (int s = 0; s < 2; ++s) {
    for (const auto& p : match->GetTeam(s)->GetAllPlayers()) {
      if (p->HasPossession() && p.get() == newestMan) {
        resetAll();
        return;
      }
    }
  }
  // The pick: the §6 pass-1 score over the enrolled men, winner within the
  // (18+7r) radius. Enrolment is the §5 table's shape (on-pitch, outfield,
  // not owning); the §5 tally itself serves the skipped §3.2 multi-press.
  const pes::PressConstants& c = match->GetPesPressConstants();
  const Vector3 ball = match->GetBall()->Predict(0);
  const auto& players = team->GetAllPlayers();
  pes::PressPickInput in;
  // ponytail: the side reference (FUN_140a71540, straight-back FUN_140a4db20
  // or the ball-goal bearing under T+0xb712) is not decoded; the ball-to-goal
  // bearing stands in, preferring goal-side pressers.
  const float goalX = -static_cast<float>(team->GetSide()) * pitchHalfW;
  // The writer's gate (FUN_140536FC0, 0x140537332; S1Watch's decode): the sample is BallInfo sample 0, the ball now, and the
  // point is the MA ball copy (MA+0x2482C/+0x24834). GF stand-in: both are the ball now (Predict(0)).
  in.ballX = ball.coords[0];
  in.ballZ = ball.coords[1];
  in.sampleX = ball.coords[0];
  in.sampleZ = ball.coords[1];
  in.referenceDeg = pes::PressBearingDeg(c, ball.coords[0], ball.coords[1], goalX, 0.0f);
  for (int k = 0; k < 11 && k < static_cast<int>(players.size()); ++k) {
    const int man = side * 11 + k;
    Player* p = players[k].get();
    in.posX[man] = p->GetPosition().coords[0];
    in.posZ[man] = p->GetPosition().coords[1];
    // The keeper's rush owns him (the keeper branch); an owning man is owned.
    const bool keeper = p->GetFormationEntry().role == e_PlayerRole_GK;
    in.onField[man] =
        p->IsActive() && !keeper && !(p->HasPossession() && p == newestMan);
    in.role[k] = pressState.role[man];
  }
  for (int i = 0; i < 4; ++i) in.pressSlotMan[i] = pressState.pressSlot[i];
  const std::vector<pes::PressCandidate> bucket = pes::RunPresserPass1(c, in, side);
  // The gate (FUN_140536FC0, 0x140537332) and the pick: the same test for a fresh pick and a renewal.
  const int winner = pes::PressWinner(c, in, bucket);
  for (int k = 0; k < 11; ++k) {
    const int man = side * 11 + k;
    pressState.SetAction(man, pes::PressWriterWord(man, winner, coordState.second), stamp);
  }
  pressState.pressSlot[0] = winner;
  for (int i = 1; i < 4; ++i) pressState.pressSlot[i] = -1;
}

void TeamAIController::PesSecondPresserTick() {
  const int side = team->GetID();
  // T+0x254, the attack sign: the team attacks towards -side (the goalX convention PesLooseWriterTick uses).
  const float sign = -static_cast<float>(team->GetSide());
  if (!match->PesLoop()) {
    coordState = pes::PressCoordState{};
    return;
  }
  const pes::PressConstants& c = match->GetPesPressConstants();
  const uint32_t stamp = static_cast<uint32_t>(match->GetSimFrame());
  const int fps = static_cast<int>(c.framesPerSec);
  const Vector3 ball = match->GetBall()->Predict(0);
  // B+0x18 and the high test, both read as GF's ball height (ponytail: PES's two ball words are not split).
  const bool lifted = ball.coords[2] > c.gateBallHeight_m;
  auto dist2 = [](float ax, float az, float bx, float bz) {
    return (bx - ax) * (bx - ax) + (bz - az) * (bz - az);
  };

  // MA+0x2601C (FUN_140457070, W1 0x14045725A; the per-tick recompute Match::RecomputePesHolder): the holder,
  // carried apart from GF's retainer (Match::GetPesHolder) - it persists on a loose ball, so the reaction keeps
  // its man while the ball is in transit.
  const Player* holderPlayer = match->GetPesHolder();
  int holder = pes::kPressNoMan;
  if (holderPlayer != nullptr) {
    const pes::ActorKey key = match->PesActorKeyOf(holderPlayer);
    if (key.Valid() && key.team < 2 && key.slot < pes::kPressSideMen)
      holder = static_cast<int>(key.team) * pes::kPressSideMen + static_cast<int>(key.slot);
  }
  const bool holderValid = holder < pes::kPressTeamMen;
  // The newest valid ring-0 touch: the T+0x78 stand-in for this side's last kick (the real writer is OPEN; the
  // opponent-kick fallback T_opp+0x78 is absent).
  pes::ActorKey touch{};
  bool touchValid = false;
  for (int n = 0; n < 8 && !touchValid; ++n) {
    const pes::NewestEventView view = pes::EventAt(match->GetPesEvents(), n);
    touchValid = view.present && view.actor.Valid() && view.actor.team < 2 && view.actor.slot < pes::kPressSideMen;
    if (touchValid) touch = view.actor;
  }
  const bool kickValid = touchValid && static_cast<int>(touch.team) == side;
  const int kickMan = kickValid ? static_cast<int>(touch.team) * pes::kPressSideMen + static_cast<int>(touch.slot)
                                : pes::kPressNoMan;

  // The men by team index (side * 11 + slot), and this side's slot words.
  pes::PressEvalInput in;
  in.side = side;
  in.sign = sign;
  in.fps = fps;
  std::array<Player*, pes::kPressTeamMen> players{};
  for (int t = 0; t < 2; ++t) {
    const auto& roster = match->GetTeam(t)->GetAllPlayers();
    for (int k = 0; k < pes::kPressSideMen && k < static_cast<int>(roster.size()); ++k) {
      const int man = t * pes::kPressSideMen + k;
      Player* p = roster[k].get();
      players[man] = p;
      in.men[man].x = p->GetPosition().coords[0];
      in.men[man].z = p->GetPosition().coords[1];
      in.men[man].active = p->IsActive();
      // ponytail: the goalkeeper role stands in for the PES kinds the eval excludes.
      in.men[man].excludedKind = p->GetFormationEntry().role == e_PlayerRole_GK;
      if (t == side) {
        in.slots[k].roleClass = RoleClassOfSlot(k);  // ponytail: the B class stands in for T+0x87D0
        in.slots[k].keepX = in.men[man].x;           // ponytail: T+0x4B4C is not carried; the margin reads his x
      }
    }
  }
  float hx = 0.0f, hz = 0.0f, holderFacing = 0.0f;
  if (holderValid && players[holder] != nullptr) {
    hx = in.men[holder].x;
    hz = in.men[holder].z;
    const Vector3 dir = players[holder]->GetDirectionVec();
    holderFacing = pes::PressBearingDeg(c, 0.0f, 0.0f, dir.coords[0], dir.coords[1]);  // H+0x554 stand-in
  }

  // The side's active man nearest the holder: the entry-0 man of the holder's list (the match-up man is not carried).
  auto nearestOwn = [&]() {
    int best = pes::kPressNoMan;
    float bestD2 = 0.0f;
    for (int k = 0; k < pes::kPressSideMen; ++k) {
      const int man = side * pes::kPressSideMen + k;
      if (man == holder || !in.men[man].active) continue;
      const float d = dist2(hx, hz, in.men[man].x, in.men[man].z);
      if (best == pes::kPressNoMan || d < bestD2) {
        best = man;
        bestD2 = d;
      }
    }
    return best;
  };
  const int nearest = holderValid ? nearestOwn() : pes::kPressNoMan;

  // FUN_1406324a0 (class 5) and FUN_140632ae0 (the class). Flags 0xd, 4, 6 and 1 are stand-ins false.
  pes::PressClassFiveInput cin;
  cin.holder = holder;
  cin.cand = nearest;
  cin.clauseOk = !lifted || !kickValid;  // (B+0x18 <= 1.5 and not high) or T+0x78 > 21
  if (nearest < pes::kPressTeamMen) {
    cin.keeperKind = in.men[nearest].excludedKind;
    cin.roleClass = RoleClassOfSlot(nearest % pes::kPressSideMen);
    cin.ballDist_m = std::sqrt(dist2(ball.coords[0], ball.coords[1], in.men[nearest].x, in.men[nearest].z));
  }
  const int classFiveMan = holderValid ? pes::PressClassFiveMan(c, cin) : pes::kPressNoMan;
  pes::PressClassInput clsIn;
  clsIn.flagFiveMan = classFiveMan < pes::kPressTeamMen;
  const int cls = pes::PressClass(clsIn);

  // Step 2: the reason words are cleared, except the second presser's slot (PP[2]).
  const int prevSecond = coordState.second;
  const int prevX = coordState.xMan;
  for (int k = 0; k < pes::kPressSideMen; ++k) {
    if (!(prevSecond < pes::kPressTeamMen && k == prevSecond % pes::kPressSideMen)) coordState.reason[k] = 0;
  }
  // Step 3: maintain (FUN_140632e70) the second presser: keep him as PRESS, or release him.
  if (prevSecond < pes::kPressTeamMen) {
    const int k = prevSecond % pes::kPressSideMen;
    const bool xOk = prevX < pes::kPressTeamMen;
    pes::PressMaintainInput mi;
    mi.inPlay = match->IsInPlay() && !match->IsInSetPiece() && !match->GetRefereeBuffer().active;
    mi.slot = k;
    mi.man = prevSecond;
    mi.tableWord = pressState.role[prevSecond];
    mi.holderNow = holder;
    mi.holderRecorded = coordState.holder;
    mi.holderFromTouch = holderValid;  // MA+0x2601C valid: the holder is the newest toucher (W1's rule)
    mi.reason = coordState.reason[k];
    mi.classFiveNow = classFiveMan;
    mi.lifted = lifted;
    mi.teamKickMan = kickMan;
    mi.xMan = prevX;
    mi.roleClass = RoleClassOfSlot(k);
    if (holderValid) {
      mi.manToHolderDistSq = dist2(hx, hz, in.men[prevSecond].x, in.men[prevSecond].z);
      if (xOk) {
        mi.bearingVsCoverDeg = std::fabs(pes::PressSideAngleDeg(
            c, pes::PressBearingDeg(c, hx, hz, in.men[prevSecond].x, in.men[prevSecond].z),
            pes::PressBearingDeg(c, hx, hz, in.men[prevX].x, in.men[prevX].z)));
      }
      mi.xCloserToTeamPoint =
          xOk && dist2(ball.coords[0], ball.coords[1], in.men[prevX].x, in.men[prevX].z) <
                     dist2(ball.coords[0], ball.coords[1], hx, hz);
    }
    if (xOk) mi.manToXDistSq = dist2(in.men[prevX].x, in.men[prevX].z, in.men[prevSecond].x, in.men[prevSecond].z);
    const pes::PressStampRing& ring = pressState.actionRing[prevSecond];
    mi.stampFirst = ring.slots[ring.head].first;
    mi.stampLast = ring.slots[ring.head].last;
    mi.windowFrames = static_cast<uint32_t>(2 * fps);
    if (pes::PressPresserKept(c, mi)) {
      pressState.SetAction(prevSecond, pes::kPressState, stamp);    // FUN_140a42940: keep, PRESS
    } else {
      pressState.SetAction(prevSecond, pes::kNothingState, stamp);  // FUN_140a40820: the slot reset
      coordState.second = pes::kPressNoMan;
    }
  }

  // Step 4: PP[0] is the holder. No holder ends the pass.
  coordState.holder = holder;
  if (!holderValid) return;
  // Step 5: PP[1] (T+0x7C stand-in: the side's man nearest the holder), then the in-play test.
  coordState.xMan = nearest;
  if (!match->IsInPlay() || match->IsInSetPiece() || match->GetRefereeBuffer().active) return;  // a restart pending (referee buffer) is not PES's state 1

  // Step 7: the gate (FUN_140632780). Classes other than 5 need PP[1].
  if (cls != pes::kPressClassFive && coordState.xMan >= pes::kPressTeamMen) return;
  pes::PressGateInput g;
  g.ballHeight_m = ball.coords[2];
  g.teamKickValid = kickValid;
  g.classFive = cls == pes::kPressClassFive;
  if (coordState.xMan < pes::kPressTeamMen) {
    const float xx = in.men[coordState.xMan].x;
    const float xz = in.men[coordState.xMan].z;
    g.cover = pes::PressCoverByHolder(c, std::sqrt(dist2(hx, hz, xx, xz)), pes::PressBearingDeg(c, hx, hz, xx, xz),
                                      sign > 0.0f ? 180.0f : 0.0f);  // T+0x6EC stand-in: heading by the sign
  }
  // FUN_140481B00: the holder's facing against the attack bearing (T+0x250; ponytail: 90 forward, 270 back).
  g.facingToAttack_deg = std::fabs(pes::PressSideAngleDeg(c, sign > 0.0f ? 90.0f : 270.0f, holderFacing));
  if (!pes::PressGatePasses(c, g)) return;

  // Step 9: the aggressive flag A. The press point stands in as the ball (FUN_14063FDC0 is OPEN); the frontline
  // style is the defensive-style toggle (FUN_140a62180(ctx, side, 4)).
  bool aggressive = teamRecord.sheet.Toggle(pes::kDefensiveStyle) == 0 && sign * ball.coords[0] > 0.0f;
  aggressive = aggressive || (sign * ball.coords[0] < c.evalThetaBase_m - pesPenaltyAreaLineX &&
                              std::fabs(ball.coords[1]) < pesPenaltyAreaHalfWidth);

  // Step 10: the dispatch. Class 5 takes the holder's nearest side man; class 7 needs A; the eval picks the rest.
  if (cls == pes::kPressClassFive) {
    if (nearest < pes::kPressTeamMen) {
      coordState.second = nearest;
      coordState.reason[nearest % pes::kPressSideMen] = pes::kPressClassFive;
      pressState.SetAction(nearest, pes::kPressState, stamp);
    }
    return;
  }
  if (cls == pes::kPressClassSix || (cls == pes::kPressClassSeven && !aggressive)) return;
  pes::PressEvalInput ein = in;
  ein.ref = holder;
  ein.excluded = coordState.xMan;
  ein.incumbent = coordState.second;
  ein.mode = cls;
  ein.aggressive = aggressive;
  const pes::PressEvalResult r = pes::PressEvalPick(c, ein);
  coordState.second = r.man;  // FUN_140633940 returns PP[2]: the man, or none
  if (r.man < pes::kPressTeamMen) {
    coordState.reason[r.man % pes::kPressSideMen] = r.mode;
    pressState.SetAction(r.man, pes::kPressState, stamp);
  }
}

bool TeamAIController::StartConcept(int id) {
  const pes::TeamConceptConstants& k = match->GetPesTeamConceptConstants();
  std::vector<Player*> men;
  // The four with an empty execute slot (0x141794410) act through their
  // running flag only.
  if (id == pes::kConceptForeCheckPress) return pes::ForeCheckPressStart(k, DefenceInput());
  if (id == pes::kConceptForeCheckLineDown) return pes::ForeCheckLineDownStart(k, DefenceInput());
  if (id == pes::kConceptForeCheckLineUp)
    return pes::ForeCheckLineUpStart(k, DefenceInput(), conceptRunning[pes::kConceptForeCheckLineDown]);
  if (id == pes::kConceptRetreatBlockCreate) {
    const pes::DefenceConceptInput in = DefenceInput();  // FUN_140575ea0
    return in.inPlay && !pes::RetreatBlockCreateEnd(k, in);
  }
  if (id == pes::kConceptTransitionChase) {
    // The carrier: the opponent on the ball, else their designated man (T'+0x78).
    Player* retainer = match->GetBallRetainer();
    Player* carrier = retainer && retainer->GetTeam() != team
                          ? retainer
                          : match->GetTeam(abs(team->GetID() - 1))->GetDesignatedTeamPossessionPlayer();
    const auto pick = pes::TransitionChaseStart(k, ChaseInput(carrier, &men));
    if (!pick) return false;
    chaseChaser = men[pick->chaser];
    chaseSecond = men[pick->second];
    chaseCarrier = carrier;
    return true;
  }
  if (id == pes::kConceptSideCounter) {
    const pes::SideCounterInput in = CounterInput(&men);
    const auto runner = pes::SideCounterStart(k, in);
    if (!runner) return false;
    counterRunner = men[*runner];
    conceptOrders[counterRunner] = {id, pes::SideCounterRunTarget(k, counterRunner->GetPosition(), in.oppLineX)};
    return true;
  }
  return false;
}

bool TeamAIController::ConceptEnded(int id) {
  const pes::TeamConceptConstants& k = match->GetPesTeamConceptConstants();
  const bool inPlay = match->IsInPlay() && !match->IsInSetPiece();
  if (id == pes::kConceptForeCheckPress) return pes::ForeCheckPressEnd(k, DefenceInput());
  if (id == pes::kConceptForeCheckLineDown) return pes::ForeCheckLineDownEnd(k, DefenceInput());
  if (id == pes::kConceptForeCheckLineUp)
    return pes::ForeCheckLineUpEnd(k, DefenceInput(), conceptRunning[pes::kConceptForeCheckLineDown]);
  if (id == pes::kConceptRetreatBlockCreate) return pes::RetreatBlockCreateEnd(k, DefenceInput());
  if (id == pes::kConceptTransitionChase) {
    // FUN_140575410: in play, the carrier unchanged (or loose), the chooser
    // unchanged, the second man still off the ball.
    Player* retainer = match->GetBallRetainer();
    const bool carrierHeld = !retainer || retainer == chaseCarrier;
    return !inPlay || !carrierHeld || team->GetDesignatedTeamPossessionPlayer() != chaseChaser ||
           !chaseSecond->IsActive() || IsDown(chaseSecond) || chaseSecond == chaseChaser;
  }
  if (id == pes::kConceptSideCounter) {
    // The runner is still on SPACE_RUN_CHANCE until he has the ball.
    const bool running = counterRunner->IsActive() && match->GetBallRetainer() != counterRunner;
    return pes::SideCounterEnd(k, inPlay, running,
                               (match->GetActualTime_ms() - conceptStart_ms[id]) / kMsPerSecond,
                               match->GetBall()->Predict(0).Get2D(), counterRunner->GetPosition());
  }
  return true;
}

void TeamAIController::ExecuteConcept(int id) {
  const pes::TeamConceptConstants& k = match->GetPesTeamConceptConstants();
  if (id == pes::kConceptTransitionChase) {
    // FUN_140575b60: the second man to the trap point, every tick.
    conceptOrders[chaseSecond] = {id, pes::TransitionChaseTarget(k, chaseChaser->GetPosition(),
                                                                 chaseCarrier->GetPosition(),
                                                                 static_cast<float>(-team->GetSide()))};
    return;
  }
  if (id == pes::kConceptSideCounter) {
    // FUN_1405415b0 re-picks the three PASS_SUPPORT men every tick.
    // ponytail: its carrier pass plan (T+0x6f4) is not fed to GF's carrier.
    std::vector<Player*> men;
    const pes::SideCounterInput in = CounterInput(&men);
    std::array<int, 3> previous{-1, -1, -1};
    int runner = -1;
    for (int i = 0; i < static_cast<int>(men.size()); ++i) {
      if (men[i] == counterRunner) runner = i;
      for (int s = 0; s < 3; ++s)
        if (men[i] == counterSupporters[s]) previous[s] = i;
    }
    const pes::CounterSupport support = pes::SideCounterSupporters(k, in, runner, previous);
    for (Player* p : counterSupporters)
      if (p) conceptOrders.erase(p);
    for (int s = 0; s < 3; ++s) {
      counterSupporters[s] = support.men[s] >= 0 ? men[support.men[s]] : nullptr;
      if (counterSupporters[s]) conceptOrders[counterSupporters[s]] = {id, support.points[s]};
    }
  }
}

void TeamAIController::CalculateSituation() {
  teamHasPossession = team->HasPossession();
  teamHasUniquePossession = team->HasUniquePossession();
  oppTeamHasPossession = match->GetTeam(abs(team->GetID() - 1))->HasPossession();
  oppTeamHasUniquePossession = match->GetTeam(abs(team->GetID() - 1))->HasUniquePossession();
  teamHasBestPossession = match->GetBestPossessionTeamID() == team->GetID();
  teamPossessionAmount = team->GetTeamPossessionAmount();
  fadingTeamPossessionAmount = team->GetFadingTeamPossessionAmount();
  timeNeededToGetToBall = team->GetTimeNeededToGetToBall_ms();
  oppTimeNeededToGetToBall = match->GetTeam(abs(team->GetID() - 1))->GetTimeNeededToGetToBall_ms();
}

void TeamAIController::ApplyFormation(Formations::e_Formation newFormation) {
  ApplyFormationShape(Formations::GetShape(newFormation));
}

Formations::Shape TeamAIController::TeamFormationShape() const {
  std::vector<e_PlayerRole> roles;
  TeamData* teamData = team->GetTeamData();
  for (int i = 0; i < playerNum; i++)
    roles.push_back(teamData->GetFormationEntry(i).role);
  return Formations::ShapeFromRoles(roles);
}

void TeamAIController::ApplyFormationShape(const Formations::Shape& shape) {
  const std::vector<Formations::Slot> layout = Formations::GetLayoutForShape(shape);
  TeamData* teamData = team->GetTeamData();

  for (int i = 0; i < static_cast<int>(layout.size()) && i < playerNum; i++) {
    FormationEntry entry = teamData->GetFormationEntry(i);
    entry.role = layout.at(i).role;
    entry.databasePosition = layout.at(i).position;
    // Same blend the database loader applies, so switching shape mid-match ends
    // up with exactly the positions a fresh load of that shape would give.
    entry.position = entry.databasePosition * 0.6f + GetDefaultRolePosition(entry.role) * 0.4f;
    teamData->SetFormationEntry(i, entry);
  }

  formationShape = Formations::IsValidShape(shape) ? shape : Formations::MakeShape(4, 4, 2);
}

void TeamAIController::UpdateTactics() {
  const TeamTactics& teamTactics = team->GetTeamData()->GetTactics();
  TeamTactics& teamTacticsWritable = team->GetTeamData()->GetTacticsWritable();

  const Properties& userTacticsModifiers = teamTactics.userProperties;

  int goals = match->GetMatchData()->GetGoalCount(team->GetID());
  int oppGoals = match->GetMatchData()->GetGoalCount(abs(team->GetID() - 1));

  liveTeamTactics = baseTeamTactics;

  // The user's own tactic modifiers are applied on top of the base as offsets.
  // PES's CPU manager never switches style or shape on the clock or the score
  // (docs/re/pes-re-match-management.md 3-4): it moves the attack level
  // (UpdateStrategy) and nothing else, so the sheet's choices stand.
  // ponytail: PES's reactive plan queue (report 3, kinds 1-9) is not ported.
  // Likewise the shape to hold: the lineup, not an unreadable property.
  const Formations::Shape preferredShape = TeamFormationShape();
  if (Formations::ShapeName(preferredShape) != Formations::ShapeName(formationShape))
    ApplyFormationShape(preferredShape);
  const Properties baseTactics = liveTeamTactics;

  // when trailing, we need goals. when leading, defend lead
  float goalFactor = clamp(0.5 + (oppGoals - goals) * 0.25f, 0.0f, 1.0f);
  // time still to play matters - get more desperate towards the end
  float timeFactor = 0.5f + 0.5f * clamp(match->GetMatchTime_ms() / 6300000.0f, 0.0f, 1.0f);
  // printf("timefactor: %f\n", timeFactor);

  float offenseBias = clamp(0.5f + (goalFactor - 0.5f) * (timeFactor * 1.0f), 0.0f, 1.0f);
  // todo: add skills/opp skills and difficulty as factors
  // todo: add off/def slider/tactics
  // if (team->GetHumanGamerCount() == 0) offenseBias = offenseBias * 0.8f + 0.2f; // let's assume
  // human gamers want more offensive play
  //                                     offenseBias = offenseBias * 0.8f + 0.2f; // let's assume AI
  //                                     gamers want more defensive play

  float possessionFactor = match->GetMatchData()->GetPossessionFactor_60seconds();
  float recentPossessionBias = 1.0f - fabs(possessionFactor - team->GetID());
  // if (team->GetID() == 0) printf("team 0 recentPossessionBias: %f (possessionFactor: %f)\n",
  // recentPossessionBias, possessionFactor); if (team->GetID() == 1) printf("team 1
  // recentPossessionBias: %f (possessionFactor: %f)\n", recentPossessionBias, possessionFactor);

  offensivenessBias = offenseBias * 0.5f + recentPossessionBias * 0.5f;
  // if (team->GetID() == 0) printf("T1 offensivenessBias: %f\n", offensivenessBias);
  // if (team->GetID() == 1) printf("T2 offensivenessBias: %f\n", offensivenessBias);

  // autoBias = clamp(fabs(offenseBias - 0.5f) * 2.0f, 0.0f, 1.0f); // don't use too much autobias
  // when things are calm (= offenseBias being around 0.5)

  const map_Properties* userMods = userTacticsModifiers.GetProperties();
  map_Properties::const_iterator iter = userMods->begin();
  while (iter != userMods->end()) {
    float multiplier = teamTacticsModMultipliers.GetReal(iter->first.c_str(), 0.0f);

    float userOffset = atof(iter->second.c_str());
    float offset = userOffset;

    // feed back to teamdata (todo: hasn't this system been effectively disabled? if so, delete it
    // to avoid confusion)
    // teamTacticsWritable.liveProperties.Set(iter->first.c_str(), offset);

    offset = (offset - 0.5f) * 2.0f * multiplier;

    float baseValue = baseTactics.GetReal(iter->first.c_str(), -1.0f);
    // printf("value: %s\n", iter->first.c_str());
    if (baseValue >= 0.0f) {  // not all user mods from teamdata are used in this class (for
                              // example, individual settings like dribble stuff), ignore them
      liveTeamTactics.Set(iter->first.c_str(), clamp(baseValue + offset, 0.0f, 1.0f));
    }
    iter++;
  }
}

// PES 2021's CPU strategy decider (docs/re/pes-re-match-management.md 1): the strategy moves in
// PES state 3 (restart armed or taken; pesstrategy.hpp DeciderMovesIn), and the attack level
// follows it once play resumes. The level is GF's mentality rung (one-to-one, see pes::AttackLevel),
// so every reader of the mentality sees PES's level.
void TeamAIController::UpdateStrategy() {
  if (!match->AIManagerRunsTeam(team->GetID())) return;
  // PES path: the per-tick team words (pesteamtick.hpp) step every team tick in FUN_1404F6C30's
  // order: the decider (0x1404F6DB8), the mode-word copy (0x1404F6DBD: T+0x134 = +0xBCC), the drift
  // (FUN_1404F8350), the flags (0x1404F6E1C-58), the restart word (0x1404F6F91) - and the strategy
  // decider below moves the strategy in state 3 underneath them. Off the path the level is the
  // mentality rung.
  const bool pesPath = match->PesLoop();
  // PES's state reads MI+0x116C: 1 while the ball is in play, even with a set piece armed or
  // taken (plan-selector §1 vs roles §2). Only the in-play-only work below uses the narrower gate.
  const bool pesInPlay = match->IsInPlay();
  const bool inPlay = pesInPlay && !match->IsInSetPiece();
  const RefereeBuffer& buffer = match->GetRefereeBuffer();
  const int playState = pes::PlayStateFor(
      match->IsInPlay(), buffer.active, buffer.ReadyForTake(match->GetActualTime_ms()) || buffer.started);
  if (pesPath) {
    // Steps 8 and 9 first, as in PES's order: the stopped select (8) and the out-of-play clear of
    // the reaction delays (9) read the latch, MI+0x116C and MI+0x1554 before the drift reads anything.
    const bool latch = team->IsPesPossession();
    pes::StoppedSelect(&teamTick, !latch, playState, match->PesRestartKind());
    pes::ClearReactionDelays(&teamTick, playState);
    // Step 11's head (FUN_1404FEDC0): R8B is the latch change this tick, the flip the restart word
    // reads too. ponytail: DL (0x1404F6D89-0x1404F6D97) needs MI+0x119C, the raw kick sum, which
    // has no GF counterpart (team.cpp), so the score-on branch stays off.
    const bool flipped = latch != lastTickPossession;
    pes::SupportWeights(&teamTick, latch, flipped, playState, /*scoreFlag=*/false);
    // S1 (audit-2): PES's tick runs the decider (0x1404F6DB8) before the copy and the drift, so a
    // state-3 move is already in the strategy when the drift reads it. States 1/2 no-op here.
    StepStrategyDecider(playState);
    // The 0x1404F6DBD copy, between the decider-source and the drift.
    pes::CopyModeWord(&teamTick);
    // ponytail: T+0x210 (the manual/target word) has no GF counterpart and no
    // decoded in-match writer (tactics §3 O4); balanced stands in, so the
    // manual arm never fires until the writer lands.
    constexpr int kManualBalanced = 2;
    pes::DriftAttackLevel(&teamTick, strategy.strategy, kManualBalanced, pesInPlay,
                           match->GetPesFrame());
    pes::WriteAttackFlags(&teamTick);
    pes::WriteRestartWord(&teamTick, pesInPlay, flipped);
    lastTickPossession = latch;
    // Step 15's in-play arm (FUN_1404FA840, 0x1404FA869-0x1404FA887).
    if (inPlay) pes::ArmInPlayCounters(&teamTick);
    // FUN_140501260 clears T+0xE4 every team tick (0x1405012D0); step 5 can only set it in play.
    if (!inPlay) teamTick.trapArmed = false;
  }
  if (inPlay) {
    if (!pesPath) {
      instructions.mentality = static_cast<TeamInstructions::e_Mentality>(
          pes::StrategyAttackLevel(strategy.strategy));
    }
    return;
  }
  // The PES path already stepped the decider above (S1: before the drift); the GF path steps it here.
  if (!pesPath) StepStrategyDecider(playState);
}

// PES 2021's CPU strategy decider body (FUN_14051E970, reached from the team tick at 0x1404F6DB8):
// one StrategyStep where the entry gate holds (states 1 and 3) and only state 3 moves anything
// (DeciderMovesIn; the body returns at once in state 1).
void TeamAIController::StepStrategyDecider(int playState) {
  if (!match->AIManagerRunsTeam(team->GetID())) return;
  // PES's decider moves the strategy in state 3 only; the PES path returns in every other state.
  if (match->PesLoop() && !pes::DeciderMovesIn(playState)) return;
  pes::StrategyInputs in;
  switch (match->GetMatchPhase()) {
    case e_MatchPhase_PreMatch:
    case e_MatchPhase_1stHalf: in.period = pes::StrategyPeriod::FirstHalf; break;
    case e_MatchPhase_2ndHalf: in.period = pes::StrategyPeriod::SecondHalf; break;
    case e_MatchPhase_1stExtraTime:
    case e_MatchPhase_2ndExtraTime: in.period = pes::StrategyPeriod::ExtraTime; break;
    default: return;  // penalties: nothing left to manage
  }
  // The minute within the current half. INF(high), not observed: PES's clock
  // getter takes the period (FUN_140a3c050(MI+0x1168)), and the second half's
  // R[6] = 28..37 would be dead code if the minute ran on from 45.
  constexpr unsigned long kMsPerMinute = 60000;
  constexpr unsigned long kHalfLength_ms = 45 * kMsPerMinute;
  const unsigned long elapsed = match->GetMatchTime_ms();
  in.minute = static_cast<int>(
      (in.period == pes::StrategyPeriod::SecondHalf && elapsed >= kHalfLength_ms
           ? elapsed - kHalfLength_ms
           : elapsed) /
      kMsPerMinute);
  const int own = team->GetID(), opp = 1 - own;
  in.ownGoals = match->GetMatchData()->GetGoalCount(own);
  in.oppGoals = match->GetMatchData()->GetGoalCount(opp);
  // PES reads each side's rating from the match setup, written once at match
  // load (Match's constructor). Replaces the per-tick mean-overall lambda.
  in.ownStrength = match->GetTeamSetupStrength(own);
  in.oppStrength = match->GetTeamSetupStrength(opp);
  // PES seeds the decider's GameRandom once per match from the clock
  // (0x14051e400); GF seeds it from the match seed so a replay replays.
  if (!strategyRng) {
    strategyRng.emplace(match->GetPesRngConstants());
    strategyRng->SetSeed(match->GetPesStrategyConstants().rngSlot, match->GetMatchSeed() + own);
  }
  pes::StrategyStep(match->GetPesStrategyConstants(), in, *strategyRng, &strategy);
  teamTick.deciderRan = true;
}

void TeamAIController::PesStep55() {
  pes::run::SnapshotCopy(&runSlots, runSlots, static_cast<uint32_t>(match->GetPesFrame()));
}

pes::run::ReleaseRoute TeamAIController::PesStep48() {
  const RefereeBuffer& buffer = match->GetRefereeBuffer();
  pes::run::ReleaseDispatchInput in;
  in.playState = pes::PlayStateFor(match->IsInPlay(), buffer.active,
                                    buffer.ReadyForTake(match->GetActualTime_ms()) || buffer.started);
  // ponytail: the ==3 arm (FUN_140642670), the else arm (FUN_14057F590), the
  // carrier-record gate (FUN_140A3E190), the MI+0x1198 frame window and the
  // MI+0x173C 2-slot scan read record tables with no GF counterpart
  // (docs/re/pes-re-team-helpers.md sect 1-2); the arm verdict stands in as
  // off until they land, so the dispatcher exits.
  in.stoppageClaim = false;
  in.latchSet = team->IsPesPossession();
  const pes::run::ReleaseRoute route = pes::run::ReleaseDispatch(in);
  if (route == pes::run::ReleaseRoute::kExit) return route;
  const uint32_t stamp = static_cast<uint32_t>(match->GetPesFrame());
  // ponytail: per-slot eligibility reads the same record tables
  // (docs/re/pes-re-team-helpers.md sect 3); no slot takes until they land.
  const std::array<bool, pes::run::kSlots> eligible{};
  if (route == pes::run::ReleaseRoute::kLatchSetLoop) {
    pes::run::LatchSetLoop(&runSlots, stamp, eligible);
  } else {
    // ponytail: the loop-carried fVar25 (0x14057F7EC) is register dataflow with
    // no GF counterpart (helpers sect 3.1); above the bar stands in, so the
    // head gate returns early.
    constexpr float kHeadValueAboveBar = 1.0f;
    pes::run::LatchClearLoop(&runSlots, stamp, kHeadValueAboveBar, eligible);
  }
  return route;
}

SetPieceParams TeamAIController::GetSetPieceParams(e_SetPiece setPiece) {
  return SetPieceConfig::Load(team->GetTeamData()->GetDatabaseID(), setPiece);
}

void TeamAIController::Reset() {
  match->GetMatchData()->RecordPassRestart();
  taker = 0;

  setPieceType = e_SetPiece_None;

  offsideTrapX = 0;

  for (int id = 0; id < pes::kConceptCount; ++id) StopConcept(id);
  for (pes::run::Slot& slot : runSlots) {
    if (slot.state != pes::offball::kNothing) pes::run::Release(&slot, static_cast<uint32_t>(match->GetPesFrame()));
  }
  lastRunFrame = -1;
  designatedSupporter = nullptr;

  teamHasPossession = false;
  teamHasUniquePossession = false;
  oppTeamHasPossession = false;
  oppTeamHasUniquePossession = false;
  teamHasBestPossession = false;
  teamPossessionAmount = 1.0f;
  fadingTeamPossessionAmount = 1.0f;
  timeNeededToGetToBall = 100;
  oppTimeNeededToGetToBall = 100;
}

// --- PES's off-ball runs and designated supporter (pesoffball.hpp) ---

namespace {

namespace ob = pes::offball;

constexpr float kPesFps = 54.0f;  // FUN_141e532d0 = [0x14351ae54]
// Positions the target builder predicts: int(fps·0.3+0.5) = 16 frames (0x14059bbe4).
constexpr float kRunPredictFrames = 16.0f;
// The supporter search predicts int(fps·0.35+0.5) = 19 frames (FUN_1405eb980).
constexpr float kSupporterPredictFrames = 19.0f;

// GF mode keeps the pre-cutover run clock: at 989dd94^ the chance runs stamped unsigned milliseconds and aged as
// (now - start_ms) * kPesFps / 1000 (teamAIcontroller.cpp:3188). The frame clock is used under PesLoop() only.
// ponytail: GF-only clock on the switch-off path. PES site: the frame stamp (T+0x21C, FUN_140A42580) is PesLoop-only.
uint32_t RunStampOf(const Match* match) {
  if (match->PesLoop()) return static_cast<uint32_t>(match->GetPesFrame());
  return static_cast<uint32_t>(match->GetActualTime_ms());
}
float RunAgeOf(const pes::run::Ring& ring, const Match* match, unsigned long nowMs) {
  const uint32_t entered = pes::run::Behind(ring, 0).enteredAt;
  if (match->PesLoop()) return static_cast<float>(static_cast<uint32_t>(match->GetPesFrame()) - entered);
  return static_cast<float>(nowMs - entered) * kPesFps / 1000.0f;
}

ob::Vec2f Flat(const Vector3& v) { return {v.coords[0], v.coords[1]}; }
Vector3 Predict(const Player* p, float frames) {
  return p->GetPosition() + p->GetMovement() * (frames / kPesFps);
}
bool IsGoalie(Player* p) { return p->GetFormationEntry().role == e_PlayerRole_GK; }

}  // namespace

int TeamAIController::GetRunState(const Player* player) const {
  const int k = OwnRunSlot(player);
  return k < 0 ? ob::kNothing : runSlots[k].state;
}

int TeamAIController::OwnRunSlot(const Player* player) const {
  const int s = RunSlotOf(player);
  const int k = s - team->GetID() * ob::kSlots;
  return k >= 0 && k < pes::run::kSlots ? k : -1;
}

// The point a run aims at: the concept point of the run's slot (T+0x26C).
static Vector3 RunPoint(const pes::run::Slot& slot) {
  const std::array<float, 3> p = pes::run::Point(slot);
  return Vector3(p[0], p[1], p[2]);
}
static_assert(pes::run::kSlots == ob::kSlots, "the run table holds one slot per own roster slot");

Player* TeamAIController::NearestOpponentOf(const Player* own) const {
  const int slot = RunSlotOf(own);
  if (slot < 0) return nullptr;
  const auto near = ob::NearestOpponents(lastRunWorld, slot);
  return near.empty() ? nullptr : RunPlayerAt(near.front().id);
}

float TeamAIController::NearestOpponentDistance(const Player* own) const {
  const int slot = RunSlotOf(own);
  if (slot < 0) return 1000.0f;
  const auto near = ob::NearestOpponents(lastRunWorld, slot);
  return near.empty() ? 1000.0f : std::sqrt(near.front().d2);
}

bool TeamAIController::IsMarkedByTable(const Player* player) const {
  const int k = OwnRunSlot(player);  // 0..10 within-side (R1 `slotOf(me)`, 0x140969970 RAX)
  if (k < 0 || k >= pes::marktable::kSlots) return false;
  return markTableState.mark[static_cast<size_t>(k)] != 0;
}

int TeamAIController::RunSlotOf(const Player* player) const {
  for (int t = 0; t < 2; ++t) {
    const std::vector<Player*>& roster = lastRunIds[t];
    const auto it = std::find(roster.begin(), roster.end(), player);
    if (it != roster.end()) return t * ob::kSlots + static_cast<int>(it - roster.begin());
  }
  return -1;
}

Player* TeamAIController::RunPlayerAt(int slot) const {
  if (slot < 0 || slot >= 2 * ob::kSlots) return nullptr;
  const std::vector<Player*>& roster = lastRunIds[slot / ob::kSlots];
  const int i = slot % ob::kSlots;
  return i < static_cast<int>(roster.size()) ? roster[i] : nullptr;
}

bool TeamAIController::GetOffBallTarget(const Player* player, Vector3* target) const {
  const int k = OwnRunSlot(player);
  if (k >= 0 && runSlots[k].state != ob::kNothing && runSlots[k].state != ob::kBasePosition) {
    *target = RunPoint(runSlots[k]);
    return true;
  }
  if (player == designatedSupporter) {
    *target = supporterTarget;
    return true;
  }
  return false;
}

void TeamAIController::ProcessOffBall() {
  // The manager ticks only on the team's own parity frame (raw 0x1404f78a5):
  // each team every other PES frame, the two alternating.
  const long frame = static_cast<long>(match->GetActualTime_ms() * kPesFps / 1000.0f);
  if (frame == lastRunFrame) return;
  lastRunFrame = frame;
  if ((frame & 1) != team->GetID()) return;

  // PES's team possession flag T+0x255 changes only when the other team takes
  // the ball. ponytail: GF has no such flag; the last team to touch the ball
  // stands in (stable through a pass, unlike GF's per-tick HasPossession).
  Player* carrier = match->GetLastTouchTeamID() == team->GetID()
                        ? team->GetDesignatedTeamPossessionPlayer()
                        : nullptr;
  // ponytail: PES's support pipeline runs every team tick; here it shares the
  // manager's parity tick.
  ProcessDesignatedSupporter(carrier);
  ProcessRunManager(carrier);
}

void TeamAIController::ProcessDesignatedSupporter(Player* carrier) {
  // ponytail: T+0xc12b (the "supporter wanted" flag) is undecoded; a supporter
  // exists whenever the team has a carrier in open play (the pipeline's
  // open-play branch, pes-re-off-ball.md 4.1).
  if (!carrier || !match->IsInPlay() || match->IsInSetPiece()) {
    designatedSupporter = nullptr;
    return;
  }
  // FUN_1405eb980 keeps the supporter (+0x7b4) and searches again only when
  // there is none or T+0xc12c asks for it. ponytail: c12c's writer is not
  // decoded; a supporter who became the passer or left the pitch is replaced.
  if (!designatedSupporter || designatedSupporter == carrier || !designatedSupporter->IsActive()) {
    designatedSupporter = nullptr;
    const Vector3 carrierPred = Predict(carrier, kSupporterPredictFrames);
    const Vector3 facing = carrier->GetDirectionVec();
    const float facing_deg = std::atan2(facing.coords[1], facing.coords[0]) * 180.0f / pi;
    float bestCost = 0.0f;
    for (const auto& p : team->GetAllPlayers()) {
      Player* mate = p.get();
      if (!mate->IsActive() || mate == carrier || IsGoalie(mate)) continue;
      const Vector3 d = Predict(mate, kSupporterPredictFrames) - carrierPred;
      const float bearing = std::atan2(d.coords[1], d.coords[0]) * 180.0f / pi;
      const float diff = std::fmod(bearing - facing_deg + 540.0f, 360.0f) - 180.0f;
      const float cost = ob::SupporterCost(diff, d.Get2D().GetLength());
      if (!designatedSupporter || cost < bestCost) {
        bestCost = cost;
        designatedSupporter = mate;
      }
    }
    if (!designatedSupporter) return;
  }
  const Vector3 carrierPred = Predict(carrier, kSupporterPredictFrames);
  // Forced back to BASE_POSITION: the supporter never runs (b2a.c:657-699). Step 38 (FUN_1404F773E) writes it
  // through FUN_140A429A0 with the record of his target block and byte 16 set. A slot that stops being the
  // supporter is released (ponytail: the step's per-slot conditions, 0x1404F76E0-0x1404F7807, are not all decoded).
  if (match->PesLoop()) {
    const uint32_t takeStamp = static_cast<uint32_t>(match->GetPesFrame());
    const int supporterSlot = OwnRunSlot(designatedSupporter);
    for (int j = 0; j < pes::run::kSlots; ++j) {
      if (j != supporterSlot && runSlots[j].state == ob::kBasePosition) pes::run::Release(&runSlots[j], takeStamp);
    }
    if (supporterSlot >= 0) {
      pes::run::Take(&runSlots[supporterSlot], ob::kBasePosition, takeStamp,
                     pes::run::SupporterRecord(runSlots[supporterSlot]));
    }
  }

  // FUN_1405ebca0: the carrier's predicted point moved by the support distance
  // along the bearing carrier -> supporter.
  const float s = -team->GetSide();
  const int opp = abs(team->GetID() - 1);
  const Vector3 toMate = (Predict(designatedSupporter, kSupporterPredictFrames) - carrierPred).Get2D();
  const float bearing = std::atan2(toMate.coords[1], toMate.coords[0] * s) * 180.0f / pi;
  ob::SupportDistanceInput in;
  // Support Range and Build Up from the team sheet (tactics 4.1).
  in.supportRange = teamRecord.sheet.Slider(pes::kSliderSupportRange);
  in.buildUpLong = teamRecord.sheet.Toggle(pes::kBuildUp) == 0;
  const Vector3 ball = match->GetBall()->Predict(0);
  in.ballX = s * ball.coords[0];
  in.ballZ = ball.coords[1];
  in.ownLineX = s * AI_GetOffsideLine(match, match->GetMentalImage(0), team->GetID());
  in.oppLineX = s * AI_GetOffsideLine(match, match->GetMentalImage(0), opp);
  in.bearingFromAttack_deg = bearing;
  supporterTarget = carrierPred.Get2D() + toMate.GetNormalized(Vector3(s, 0, 0)) *
                                              ob::SupportDistance_m(in);
}

// PES's role arrays (pesroles.hpp). GF's registry holds each slot's formation role in all three
// phases; PES's own writer of the sheet registry is not decoded. The setup write is FUN_1404F5F10
// with flag `i == 0` for team i (the setup loop at 0x141FBB06A).
void TeamAIController::SetUpRoleArrays() {
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) {
    const e_PlayerRole formation = team->GetTeamData()->GetFormationEntry(slot).role;
    const int role = pes::roles::RegisteredRoleOf(formation);
    for (int phase = 0; phase < pes::kSheetPhases; ++phase)
      teamRecord.sheet.SetRegisteredRole(slot, role, phase);
  }
  pes::roles::WriteSideRoles(match->GetPesTeamConceptConstants().roleClassOfRegistered, teamRecord.sheet,
                             pes::mentality::RoleLevel(
                                 team->GetID() == 0,
                                 pes::mentality::RolePlayState(match->IsInPlay(), match->IsInSetPiece()),
                                 match->PesRestartKind()),
                             &roles);
}

// The bulk writer FUN_1404F5F10 at the team tick's head and tail (FUN_14051D9E0). Both take the level of
// the possession latch T+0x255 (RoleLevel). PES's head flag is F, the latch on the usual path (its parity
// sub-path is RE-25, not ported). Both are gated on MI+0x1775 == 0; GF has
// no such byte and its stand-in (pesmovestep.cpp's MatchModeByte) is 0, so the gate is open.
// ponytail: PES's registry is the sheet's role per phase. GF has no sheet phases, so each write
// mirrors the role its slot's player holds now (the dynamic formation entry the readers used).
void TeamAIController::WriteRoleArrays(bool head) {
  const bool possession = team->IsPesPossession();
  const int level = pes::mentality::RoleLevel(
      possession,
      pes::mentality::RolePlayState(match->IsInPlay(), match->IsInSetPiece()), match->PesRestartKind());
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) {
    const e_PlayerRole role = team->GetAllPlayers()[slot]->GetDynamicFormationEntry().role;
    teamRecord.sheet.SetRegisteredRole(slot, pes::roles::RegisteredRoleOf(role), level);
  }
  pes::roles::WriteSideRoles(match->GetPesTeamConceptConstants().roleClassOfRegistered, teamRecord.sheet,
                             level, &roles);
  // ponytail: `level` is the role level, the matrix row of WriteSideRoles. WriteSlotPoints takes the sheet phase
  // (FUN_1404F5F10: f = 2 - (F != 0), 0 when MI+0x116C != 1 && MI+0x1554 == 1; 0x1404F5F38-4A). They coincide on every
  // reachable path today (audit F6); split them when a level-0 phase lands.
  WriteSlotPoints(level);
  if (head) return;
  if (match->PesLoop()) UpdateLaneLine();
  // The tail swap (FUN_14051E31E) also needs FUN_140A41BE0's latch (TailSwapAllowed).
  if (roleSwap && possession && pes::roles::TailSwapAllowed(roles))
    pes::roles::SwapB(&roles, roleSwap->first, roleSwap->second);
}

// FUN_1404F5F10's rows: each slot's sheet triple at the tick's phase, through FUN_1408C50E0's record A.
// The plan is the sheet's (FUN_1421B7D40); the in-play selector moves it (UpdatePesSheetPlan). The
// sheet starts at plan 0 (DefaultTeamSheet): PES's only writer of setup+0xB34 is the reset (MOV
// [RBX+0xB34/0xBDC],EDI at 0x1408ADEA5/0x1408ADFAD with EDI = 0), so exhibitions start at 0
// (plan-selector §3; the reader is FUN_1408E16D0).
void TeamAIController::WriteSlotPoints(int phase) {
  if (!match->PesLoop()) return;  // the PES path only: the install's tables exist on it
  const pes::formation::FormationTables* tables = match->GetPesFormations();
  const pes::formation::SheetConstants* sheet = match->GetPesSheetConstants();
  assert(tables && sheet);  // the PES loop is built with the install's tables (gfsim_run loads them)
  const auto plan = tables->PlanOf(static_cast<uint32_t>(team->GetTeamData()->GetDatabaseID()),
                                   teamRecord.sheet.Plan());
  assert(plan);  // every team has a tactics record for each plan (Tactics.bin: two per team)
  slotPoints = pes::formation::SlotPointRows(*plan, phase, pesFieldHalfLength, pesFieldHalfWidth,
                                             sheet->grid, sheet->keeper);
}

// PES's in-play plan selector (FUN_140500B90, reached from the team tick at 0x1404F6EAD): once per PES
// team tick, after the decider and the full-defence flag of this tick.
void TeamAIController::UpdatePesSheetPlan() {
  if (!match->PesLoop() || !match->AIManagerRunsTeam(team->GetID())) return;
  // The stoppage arm (l.291-320): the gate byte +0x8D (FUN_140A606C0, one byte at G+0x750D0+0x8D),
  // the MI+0x116C == 2 test, the G+0x1F8 chain and the in-play-override gate (FUN_140A55CF0 == 0),
  // then the plan-2 rule (§1). ponytail: +0x8D has no GF carrier and no decoded meaning (its 37
  // callers are sheet/AI/match uses); the gate stands closed until the byte's meaning lands. The
  // setup word (setup+0x1FC) likewise stands at 0 (keep) until its writer is found.
  constexpr bool kRestartGate8D = false;
  constexpr bool kSetupWordNonZero = false;
  const RefereeBuffer& buffer = match->GetRefereeBuffer();
  const int playState = pes::PlayStateFor(
      match->IsInPlay(), buffer.active, buffer.ReadyForTake(match->GetActualTime_ms()) || buffer.started);
  if (playState == pes::kPlayStateStopped && kRestartGate8D) {
    teamRecord.sheet.SetPlan(pes::StoppagePlan2(teamRecord.sheet.Plan(), kSetupWordNonZero));
    return;
  }
  // The in-play mode table (l.272-289): the mode word +0xBCC picks the plan (§2). +0xBCC's writer
  // is OPEN, so the word is −1 (keep, PES's < 0 arm) until the decider first runs, then the
  // strategy the decider just moved (S2). The table reads the word, never the strategy direct.
  teamRecord.sheet.SetPlan(pes::formation::InPlayPlan(pes::SelectorMode(teamTick.deciderRan, strategy.strategy),
                                                     teamTick.b8a3, teamRecord.sheet.Plan()));
}

int TeamAIController::RoleClassOfSlot(int slot) const {
  assert(slot >= 0 && slot < pes::roles::kSideSlots);
  return roles.b[slot];
}

void TeamAIController::UpdateLaneLine() {
  if (!match->PesLoop()) return;
  pes::mentality::LaneWordsInput in;
  const auto& players = team->GetAllPlayers();
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) {
    in.b[slot] = roles.b[slot];
    in.cls[slot] = roles.b[slot];
    in.active[slot] = (slot < (int)players.size() && players[slot] && players[slot]->IsActive()) ? 1 : 0;
  }
  in.designated = -1;
  in.copyPath = false;
  in.playState116c = pes::mentality::RolePlayState(match->IsInPlay(), match->IsInSetPiece());
  roles.a = pes::mentality::LaneWords(in, roles.a);
}

int TeamAIController::DefensiveLineOf(Player* p, e_PlayerRole role) const {
  if (!match->PesLoop()) return PesLineIndexForRole(role);
  // PES's line of a slot is the reclassify of its B class (EC600's line store at 0x1405ECBB0).
  const auto& players = team->GetAllPlayers();
  for (int slot = 0; slot < pes::roles::kSideSlots && slot < static_cast<int>(players.size()); ++slot)
    if (players[slot].get() == p)
      return roles.a[slot];
  return PesLineIndexForRole(role);
}

void TeamAIController::UpdatePesDesignation() {
  // The lateral key is the slot point's f2, read raw for both sides (FUN_140A40CA0; pes-re-sheet-triple §2).
  std::array<float, pes::designation::kSideSlots> lateral{};
  for (int slot = 0; slot < pes::designation::kSideSlots; ++slot) lateral[slot] = slotPoints[slot].z;
  pesDesignation = pes::designation::Publish(
      team->GetID(), roles.b, lateral,
      team->GetMatch()->GetPesTeamConceptConstants().designation.order);
}

std::vector<Player*> TeamAIController::PesBoxMen(int boxCount) {
  // FUN_140A64030(slot, 0xF): the bit-15 style (Extra Frontman) on the man, and his class's compat row.
  constexpr int kExtraFrontmanBit = 15;
  Match* match = team->GetMatch();
  const pes::designation::Constants& designation = match->GetPesTeamConceptConstants().designation;
  const pes::style::Compat& compat = match->GetPesOffBallConstants().styleCompat;
  const std::vector<std::unique_ptr<Player>>& all = team->GetAllPlayers();
  pes::designation::OrdinalInput in;
  in.side = team->GetID();
  for (int slot = 0; slot < pes::designation::kSideSlots; ++slot) {
    Player* player = all[slot].get();
    auto* data = player->GetPlayerData();
    in.onPitch[slot] = player->IsActive();
    in.classB[slot] = RoleClassOfSlot(slot);
    in.aerial[slot] = data->GetHeight() + player->GetStat("physical_jump") +
                      player->GetStat("technical_header");
    in.extraFrontman[slot] = pes::style::Gate(compat, in.classB[slot],
                                              pes::style::StyleWord(data->GetPlayingStyle()),
                                              kExtraFrontmanBit);
    if (in.onPitch[slot]) ++in.eligibleCount;
  }
  in.boxCount = boxCount;
  in.list = &pesDesignation;
  in.lowRank = designation.lowRank;
  in.highClassBonus = designation.highClassBonus;
  const pes::designation::OrdinalFill fill = pes::designation::FillOrdinals(in);
  std::vector<Player*> men;
  for (int i = 0; i < fill.placedCount; ++i) men.push_back(all[fill.placed[i]].get());
  return men;
}

// ponytail: the swap pair T+0x89D4/0x89D8 has no decoded writer and no GF event supplies one, so
// GF never swaps B. The writer of that pair removes this stand-in.
void TeamAIController::RequestRoleSwap(int a, int b) { roleSwap = std::make_pair(a, b); }

namespace {

// PES's yaw of a ground vector in degrees, [0, 360); 0 for a zero vector. The man record's facing word (+0x554).
float PesYawDeg(const Vector3& v) {
  return (v.coords[0] == 0.0f && v.coords[1] == 0.0f)
             ? 0.0f
             : std::fmod(std::atan2(v.coords[0], v.coords[1]) * 180.0f / pi + 360.0f, 360.0f);
}

// The movement fields of a player's man record: position from GF's coordinates (x = coords[0], z = coords[1] = PES z,
// coords[2] = height), velocity in m per PES frame (GF m/s over kPesFps), facing in degrees.
void WriteManMovement(Player& pl, uint8_t* man) {
  const Vector3 pos = pl.GetPosition();
  const Vector3 mov = pl.GetMovement();
  // The previous position (+0x568) is last tick's: the record still holds it until the new one is written below (P+0x14 is a
  // copy of it).
  std::memcpy(man + pes::kManPrevPosXOffset, man + pes::kManPosXOffset, 3 * sizeof(float));
  pes::StoreF32(man, pes::kManPosXOffset, pos.coords[0]);
  pes::StoreF32(man, pes::kManPosZOffset, pos.coords[1]);
  // PES's man record holds the height too (+0x560, +0x578 m per frame). GF's coords[2] is the height (INF).
  pes::StoreF32(man, pes::kManPosYOffset, pos.coords[2]);
  pes::StoreF32(man, pes::kManVelXOffset, mov.coords[0] / ob::kPesFps);
  pes::StoreF32(man, pes::kManVelZOffset, mov.coords[1] / ob::kPesFps);
  pes::StoreF32(man, pes::kManVelYOffset, mov.coords[2] / ob::kPesFps);
  pes::StoreF32(man, pes::kManFacingOffset, PesYawDeg(pl.GetDirectionVec()));
}

// The facts P's fields are made from (pestrapactor.hpp), read from the player's own state.
pes::PesActorFacts TrapFactsOf(Player& pl, int slot) {
  pes::PesActorFacts f;
  f.slot = slot;
  f.side = slot / pes::kSlotsPerSide;
  f.heightCm = pesmovestep::PhysiqueHeightCmOf(pl);
  const pes::MoveStepRecord& rec = pl.GetMoveStep();
  f.phase = static_cast<uint8_t>(rec.state);  // P+0x245C is the MOVESTEP record's state
  // ponytail: GF has no PES action id for the body (P+0xAC8; MOVESTEP's AnimeState answers 2 for movement-like functions too).
  // The receiving man is in MOVE (2) or STEP_MOVE (3) when entry 3 fires (the six kind-0 captures show both with the same
  // phases), and nothing in GF tells them apart, so MOVE stands in for the pair and the previous pair. A body action that has an
  // id replaces this.
  f.action = f.sub = f.prevAction = f.prevSub = pes::kAnimeActionMove;
  // P+0xAE0: the PES motion number of the clip the humanoid plays; the AI's own clip has none, so the no-motion sentinel.
  const int motion = pesmovestep::CurrentMotionOf(pl);
  f.motion = motion < 0 ? pes::kPNoMotion : static_cast<uint16_t>(motion);
  f.kickApproach = 0;  // the kick approach (adjuster A) is never armed in GF
  // P+0x277C / +0x2780: the first dwell's elapsed and limit, MOVESTEP's carried words req[0x1a] / req[0x1b].
  const auto carried = [&rec](int word) {
    float v;
    std::memcpy(&v, &rec.carried[word - pes::moveword::kRecordStampA], sizeof v);
    return v;
  };
  f.word277C = carried(pes::moveword::kDwellElapsed);
  f.word2780 = carried(pes::moveword::kDwellLimit);
  f.speedByte = pesmovestep::SpeedByteOf(pl);
  f.legSlider = 0.0f;  // ponytail: FUN_141AD98B0 has no GF source (the same stand-in as PesMovestepHost::PhysiqueG)
  f.stamina = pl.GetStamina();
  return f;
}

}  // namespace

// Every active player's movement record at the tick's start (Match::Process, PES mode). The cap is BuildRunWorld's: at
// most kSlots active players a side, in roster order.
void WritePesMovementRecords(Match& match) {
  for (int t = 0; t < 2; ++t) {
    int written = 0;
    for (const auto& p : match.GetTeam(t)->GetAllPlayers()) {
      if (!p->IsActive() || written == ob::kSlots) continue;
      ++written;
      WriteManMovement(*p, match.PesWorldOf().ManRecord(match.PesSlotOf(p.get())));
    }
  }
}

// Every active player's P at the tick's start, after the movement records it is made from (Match::Process, PES mode).
// ponytail: SBn, the squad-order record of table B (G+0x1978, read by FUN_140A3B320 in 9 of 24 captured calls), has no decoded
// writer and no GF source: table B stays unbound, so its reads answer zero. FUN_140A3B320's table-B writer removes this.
void WritePesTrapActors(Match& match) {
  for (int t = 0; t < 2; ++t) {
    int written = 0;
    for (const auto& p : match.GetTeam(t)->GetAllPlayers()) {
      if (!p->IsActive() || written == ob::kSlots) continue;
      ++written;
      const int slot = match.PesSlotOf(p.get());
      pes::PesActorOpenInputs open;
      const uint8_t* man = match.PesWorldOf().ManRecord(slot);
      for (std::size_t k = 0; k < pes::kVelocityRingEntries; ++k)
        std::memcpy(open.velocityRing.data() + k * pes::kVelocityRingEntryBytes, man + pes::kManVelXOffset,
                    pes::kVelocityRingEntryBytes);
      pes::WriteActorP(match.PesWorldOf(), TrapFactsOf(*p, slot), open, match.GetPesActorConstants(),
                       match.GetPesStaminaConstants(), match.GetPesMoveStepConstants().physique);
    }
  }
}

// The world as the run code reads it (pesoffball.hpp RunWorld), global ids
// team·11 + slot over each team's active players in roster order.
pes::offball::RunWorld TeamAIController::BuildRunWorld(
    std::array<std::vector<Player*>, 2>* ids) const {
  ob::RunWorld w;
  const unsigned long now = match->GetActualTime_ms();
  const Vector3 ball = match->GetBall()->Predict(0);
  w.ball = Flat(ball);
  // The style gate is a pes_loop input: off that path the GF stand-in holds (StyleLb).
  if (match->PesLoop()) w.styleCompat = match->GetPesOffBallConstants().styleCompat;
  w.pesPath = match->PesLoop();
  w.openPlay = match->IsInPlay() && !match->IsInSetPiece();
  for (int t = 0; t < 2; ++t) {
    Team* tm = match->GetTeam(t);
    for (const auto& p : tm->GetAllPlayers())
      if (p->IsActive() && (*ids)[t].size() < ob::kSlots) (*ids)[t].push_back(p.get());
    w.s[t] = -static_cast<float>(tm->GetSide());
    w.lineX[t] = AI_GetOffsideLine(match, match->GetMentalImage(0), t);
    w.possession[t] = match->GetLastTouchTeamID() == t;
    TeamAIController* ctl = tm->GetController();
    w.possessionAge[t] = (now - ctl->possessionChange_ms) * kPesFps / 1000.0f;
  }
  auto idOf = [&](const Player* pl) {
    for (int t = 0; t < 2; ++t)
      for (size_t i = 0; i < (*ids)[t].size(); ++i)
        if ((*ids)[t][i] == pl) return t * ob::kSlots + static_cast<int>(i);
    return -1;
  };
  for (int t = 0; t < 2; ++t) {
    TeamAIController* ctl = match->GetTeam(t)->GetController();
    // PES action 5 (PASS_GET): the team's receiver, gated on play as Team's own use is.
    Player* passGetReceiver = match->IsInPlay() ? match->GetTeam(t)->PassGetReceiver() : nullptr;
    for (size_t i = 0; i < (*ids)[t].size(); ++i) {
      Player* pl = (*ids)[t][i];
      ob::PlayerFacts& f = w.p[t * ob::kSlots + i];
      f.active = true;
      f.goalie = pl->GetFormationEntry().role == e_PlayerRole_GK;
      f.passGet = pl == passGetReceiver;
      // ponytail: GF mode keeps a stand-in for PES's per-tick movement writer (not decoded; docs/re/pes-re-player-record.md
      // 2.2): GF's movement is written here, once per build. PES mode writes it at the tick's start instead.
      // The man record is keyed by PES's slot (his squad place): the block the translated accessors read.
      const int recordSlot = match->PesSlotOf(pl);
      f.man = match->PesWorldOf().ManRecord(recordSlot);
      if (!match->PesLoop()) WriteManMovement(*pl, f.man);
      f.roleClass = ctl->RoleClassOfSlot(match->GetTeam(t)->FormationSlotOf(pl));
      f.styleWord = pes::style::StyleWord(pl->GetPlayerData()->GetPlayingStyle());
      // ponytail: PES's writer of the record's +1 byte (lane-units2 S12) is not decoded; the keeper class stands in for it.
      // The byte sits in the player's anime record (pesworldrecord.hpp), keyed by his PES slot (his squad place).
      match->PesWorldOf().SetAnimeClass(
          recordSlot, static_cast<uint8_t>(f.roleClass == pes::roles::kKeeperClass ? ob::kHalvedReachRecordByte : 0));
      f.recordByte1 = match->PesWorldOf().AnimeClass(recordSlot);
      // ponytail: FUN_140a580a0's AI class (player +0xc0, a copy of +0x40) is
      // PES's AI behaviour class, which GF does not carry. The role class is
      // the closest analogue: the class the AI treats the player as.
      f.aiClass = f.roleClass;
      f.slotPoint = Flat(ctl->GetAdaptedFormationPosition(pl, false));
      const int k = ctl->OwnRunSlot(pl);
      if (k >= 0 && ctl->runSlots[k].state == ob::kBasePosition) {
        // The designated supporter at BASE_POSITION (step 38): the age of that state. His aim is the stand-in a
        // non-runner gets (ponytail, a76240).
        f.state = ob::kBasePosition;
        f.t = RunAgeOf(ctl->runSlots[k].ring, match, now);
      } else if (k >= 0 && ctl->runSlots[k].state != ob::kNothing) {
        const pes::run::Slot& run = ctl->runSlots[k];
        f.state = run.state;
        f.t = RunAgeOf(run.ring, match, now);
        f.aim = Flat(RunPoint(run));
      } else {
        f.state = ob::kBasePosition;
        f.t = 1e9f;
        // ponytail: a non-runner's AI target (a76240) is where he is heading,
        // one second out.
        f.aim = Flat(pl->GetPosition() + pl->GetMovement());
      }
    }
  }
  // Markers: the opponent whose man-marking assignment is this player.
  for (int t = 0; t < 2; ++t)
    for (Player* pl : (*ids)[t]) {
      const int mark = pl->GetManMarkingID();
      if (mark < 0) continue;
      const int target = idOf(match->GetPlayer(mark));
      if (target >= 0) w.p[target].marker = idOf(pl);
    }
  Player* owner = match->GetDesignatedPossessionPlayer();
  if (owner && owner->HasPossession()) w.carrier = idOf(owner);
  // 5969b0: the ball owner on the team, else its last toucher, else its
  // designated player.
  for (int t = 0; t < 2; ++t) {
    Team* tm = match->GetTeam(t);
    if (w.carrier >= 0 && w.carrier / ob::kSlots == t)
      w.receiver[t] = w.carrier;
    else if (tm->GetLastTouchPlayer() && idOf(tm->GetLastTouchPlayer()) >= 0)
      w.receiver[t] = idOf(tm->GetLastTouchPlayer());
    else
      w.receiver[t] = idOf(tm->GetDesignatedTeamPossessionPlayer());
  }
  const Match::PassEvent& ev = match->GetLastPassEvent();
  if (ev.valid) {
    w.ev.valid = true;
    w.ev.kind = ev.kind;
    w.ev.age = (now - ev.time_ms) * kPesFps / 1000.0f;
    w.ev.pos = Flat(ev.from);
    w.ev.target = ev.target ? idOf(ev.target) : -1;
  }
  // PES's NearestList is each team AI's S1 row (MA+0x22E4C + 200 i), read live through the run world.
  w.nearestRows = {&match->GetTeam(0)->GetController()->Rows(), &match->GetTeam(1)->GetController()->Rows()};
  return w;
}

void TeamAIController::ProcessRunManager(Player* carrier) {
  const pes::offball::OffBallConstants& k = match->GetPesOffBallConstants();
  const unsigned long now = match->GetActualTime_ms();
  const uint32_t frame = RunStampOf(match);
  const float s = -team->GetSide();
  const int opp = abs(team->GetID() - 1);
  const bool openPlay = match->IsInPlay() && !match->IsInSetPiece();
  const Vector3 ball = match->GetBall()->Predict(0);
  const float oppLineX = AI_GetOffsideLine(match, match->GetMentalImage(0), opp);

  std::array<std::vector<Player*>, 2> ids;
  lastRunWorld = BuildRunWorld(&ids);
  lastRunIds = std::move(ids);
  // Step 5 (FUN_1404F6720) arms T+0xE4 from this run world. ponytail: the run world is built on the team's run frame,
  // so the bit holds for the PES frame between; the chain cannot set it (pesoffball.cpp).
  if (match->PesLoop()) teamTick.trapArmed = ob::OffsideTrapArmed(lastRunWorld, team->GetID());
  const ob::RunWorld& world = lastRunWorld;
  const std::vector<Player*>& slots = lastRunIds[team->GetID()];
  const int base = team->GetID() * ob::kSlots;

  // Stop pass: every *running* slot whose stop test says so goes back to
  // NOTHING and its record is dropped, exactly as pass 2 had it. The 2 s
  // re-run block A[s] that step 2 also maintains is SecondLine's gate in PES
  // (off-ball 2.3: the Chance mask's raw body never reads ctx+0x38), so it is
  // not kept or applied on the Chance path - see RunCooldownBlocks's note and
  // the fitness test that pins it to SecondLine.
  for (int k = 0; k < pes::run::kSlots; ++k) {
    pes::run::Slot& slot = runSlots[k];
    if (slot.state == ob::kNothing) continue;
    if (k >= static_cast<int>(slots.size())) {
      pes::run::Release(&slot, frame);
      continue;
    }
    if (slot.state == ob::kBasePosition) continue;  // not a selector state: the stop test never reads it (off-ball 1.2)
    if (ob::RunStop(world, base + k, slot.state)) {
      match->GetMatchData()->AddStopFire(team->GetID());
      pes::run::Release(&slot, frame);
    }
  }

  // Select pass. Overlap (registered first) and the selectors after Chance are
  // not ported (report: pes-re-off-ball-port.md); Chance is the only one.
  if (!openPlay || !carrier) return;
  const int level = match->PesLoop() ? teamTick.b8a4 : static_cast<int>(GetInstructions().mentality);
  // Instruction 6 (TIKI_TAKA): GF carries no advanced instructions (their
  // in-match writer is not found, teaminstructions.hpp), so it is never set.
  constexpr bool tikiTaka = false;
  const int cap = ob::RunCap(k, ob::kCapChance, level, tikiTaka);
  int running = 0;
  for (const pes::run::Slot& slot : runSlots) running += slot.state == ob::kChance;
  const int runningAtStart = running;

  std::vector<Player*> opponents;
  match->GetActiveTeamPlayers(opp, opponents);
  ob::LaneSearchInput lane;
  lane.s = s;
  lane.ball = Flat(ball);
  lane.defLineX = oppLineX;
  lane.teamDir_deg = s > 0.0f ? 0.0f : 180.0f;  // ponytail: T+0x250 taken as the attack direction
  // FUN_140644fe0 (decompiled for this port) is false unless the ball is past
  // F6 + 2 in the attack direction, and false for a runner within 10 m of the
  // ball's z while the ball is inside the box width (both applied per runner
  // below). ponytail: its remaining tests (the role-58 receiver FUN_140a62850
  // and its 120°/7 m and 120°/5 m cones) are not ported, and the receiver
  // the lane bonus measures from is the carrier (GF's carrier is the pass's
  // target while the ball travels).
  constexpr float kRecvBoxMargin_m = 2.0f;  // 0x142565604
  constexpr float kRecvSideBand_m = 10.0f;  // 0x142588210
  const bool ballPastBox = s * ball.coords[0] >= pesPenaltyAreaLineX + kRecvBoxMargin_m;
  lane.receiver = Flat(carrier->GetPosition());
  lane.limit = k.chanceScoreLimit;
  std::vector<Player*> outfield;
  for (Player* o : opponents) {
    lane.opponents.push_back(Flat(o->GetPosition()));
    if (!IsGoalie(o)) outfield.push_back(o);
  }
  // The two deepest outfield defenders (FUN_14063fbc0), predicted per runner
  // by FUN_1405997f0's length below.
  std::sort(outfield.begin(), outfield.end(), [s](Player* a, Player* b) {
    return s * a->GetPosition().coords[0] > s * b->GetPosition().coords[0];
  });

  // At or over the cap PES searches with p8 = 1: a lane is taken only by
  // out-ranking a Chance runner it then releases (claim gates 5abcc0/5ab380/
  // 5ab990). ponytail: those gates are undecoded, so no swap happens; the
  // cap simply holds.
  const uint32_t slotMask = ob::AttackSlotMask(world, team->GetID(), level);
  const bool buildUpLong = teamRecord.sheet.Toggle(pes::kBuildUp) == 0;
  // FUN_1405938a0, the Chance mask's team head. The raw calls it once per
  // FUN_140606cd0, before that function's own loop over the 11 slots
  // (OBS mask.c:86, loop at mask.c:197-204), so its verdict is shared by every
  // slot: it is evaluated once here and passed down. When it fails the mask is
  // not walked at all, the histogram files the reason once, and the lowered-cap
  // release still runs.
  match->GetMatchData()->AddTeamTick(team->GetID());
  const int teamGate = ob::TeamChanceMayRunReject(world, team->GetID());
  const bool teamMayRun = teamGate == ob::kMaskPass;
  if (!teamMayRun) {
    match->GetMatchData()->AddMaskReject(team->GetID(), teamGate);
    // A lowered cap still releases runners, exactly as the select pass does.
    for (int excess = runningAtStart - cap; excess > 0;) {
      auto it = std::find_if(slots.begin(), slots.end(),
                             [&](Player* p) { return GetRunState(p) == ob::kChance; });
      if (it == slots.end()) break;
      pes::run::Release(&runSlots[it - slots.begin()], frame);
      --excess;
    }
    return;
  }
  std::array<float, ob::kSlots> scores{};
  std::array<Vector3, ob::kSlots> targets;
  for (size_t i = 0; running < cap && i < slots.size() && i < ob::kSlots; ++i) {
    Player* p = slots[i];
    if (p == designatedSupporter) continue;
    const Vector3 pos = p->GetPosition();
    // ponytail: the reach byte's label is unresolved (attack-leftovers 1.10);
    // GF's offensive awareness on PES's 0..99 scale stands in, without the
    // Weighted Pass +10.
    const float awareness = p->GetStat("mental_offensivepositioning") * 100.0f;
    const int reject = ob::ChanceMaskReject(k, world, base + static_cast<int>(i), slotMask, level,
                                            buildUpLong, awareness, teamMayRun);
    match->GetMatchData()->AddMaskWalk(team->GetID());
    match->GetMatchData()->AddMaskReject(team->GetID(), reject);
    if (reject != ob::kMaskPass) continue;
    lane.runner = Flat(pos);
    lane.runnerPred = Flat(Predict(p, kRunPredictFrames));
    lane.runnerVel = Flat(p->GetMovement());
    lane.hasReceiver =
        ballPastBox && !(std::fabs(ball.coords[1]) < pesPenaltyAreaHalfWidth &&
                         std::fabs(pos.coords[1] - ball.coords[1]) < kRecvSideBand_m);
    const int predFrames = ob::DefenderPredictFrames(
        k, (pos - ball).Get2D().GetLength() * (pos - ball).Get2D().GetLength());
    lane.deepest.clear();
    for (size_t d = 0; d < outfield.size() && d < 2; ++d)
      lane.deepest.push_back(Flat(Predict(outfield[d], static_cast<float>(predFrames))));
    const std::optional<ob::Vec2f> t = ob::ChanceLaneTarget(k, lane);
    if (!t) continue;  // y = -100: score 0
    targets[i] = Vector3(t->x, t->z, 0.0f);
    scores[i] = ob::ChanceScore(k, s, pos.coords[0]);
  }
  for (int i : ob::SelectRunners(scores, running, cap, ob::kChancePerTick, true)) {
    const bool reselected = runSlots[i].state == ob::kChance;
    pes::run::Assign(&runSlots[i], ob::kChance, frame);
    // ponytail: pre-cutover GF mode rebuilt the run record on every chance pick (teamAIcontroller.cpp:3374 at 989dd94^,
    // runs[slots[i]] = RunRecord{kChance, now, ...}), so a re-pick restarted the clock. PES keeps the entered stamp on
    // a re-pick (pes::run::Assign), so the refresh is GF-mode only.
    if (reselected && !match->PesLoop()) runSlots[i].ring.entry[runSlots[i].ring.index].enteredAt = frame;
    pes::run::SetPoint(&runSlots[i], {targets[i].coords[0], targets[i].coords[1], targets[i].coords[2]});
    match->GetMatchData()->AddChanceRun(team->GetID());
    ++running;
  }
  // Excess Chance runners from a lowered cap are released.
  // ponytail: PES walks only runners pinned in sel+0x280 (the claim gates are
  // not ported); here the first ones in slot order.
  for (int excess = runningAtStart - cap; excess > 0; ) {
    auto it = std::find_if(slots.begin(), slots.end(),
                           [&](Player* p) { return GetRunState(p) == ob::kChance; });
    if (it == slots.end()) break;
    pes::run::Release(&runSlots[it - slots.begin()], frame);
    --excess;
  }
}
