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

#include "elizacontroller.hpp"

#include <cmath>

#include "simcontext.hpp"
#include "../../AIsupport/AIfunctions.hpp"
#include "../../AIsupport/mentalimage.hpp"
#include "../../aitactics.hpp"
#include "../humanoid/humanoid_utils.hpp"
#include "../playerofficial.hpp"
#include "data/playerskills.hpp"
#include "data/playingstyles.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "onthepitch/pesballphysics.hpp"
#include "onthepitch/pesdefenderdecisions.hpp"
#include "onthepitch/pesdodge.hpp"
#include "onthepitch/matchpressure.hpp"
#include "onthepitch/peskeeper.hpp"
#include "onthepitch/setpiecelaws.hpp"
#include "onthepitch/pessetpieceteam.hpp"
#include "onthepitch/penaltyshootoutcontroller.hpp"
#include "onthepitch/teaminstructions.hpp"
#include "strategies/offtheball/default_def.hpp"
#include "strategies/offtheball/default_mid.hpp"
#include "strategies/offtheball/default_off.hpp"
#include "strategies/offtheball/goalie_default.hpp"
#include "strategies/strategy.hpp"
#include "onthepitch/goalcelebration.hpp"
#include "onthepitch/goalsequence.hpp"
#include "onthepitch/pescarrier.hpp"
#include "onthepitch/pesballphysics.hpp"
#include "onthepitch/pesgoalkick.hpp"

namespace {

// PES's angle fold (0x140481b00, report 4): wrap at 360, fold at 180.
inline float WrapDeg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

// The touch's power that lofts a goal kick onto `target` (pesgoalkick.hpp): the launch speed
// PES's lofted kick needs from the ball's spot along `direction`, at the receiving height.
float PesLoftedKickPower(Match* match, const Vector3& direction, const Vector3& target) {
  const Vector3 spot = match->GetBall()->Predict(0);
  const float distance_m = Vector3(target.coords[0] - spot.coords[0],
                                   target.coords[1] - spot.coords[1], 0.0f)
                               .GetLength();
  const float speed_mps = pes::LoftedKickSpeed(
      match->GetPesBallConstants(), pes::Vec3(spot.coords[0], spot.coords[1], spot.coords[2]),
      pes::Vec3(direction.coords[0], direction.coords[1], direction.coords[2]), distance_m,
      pes::kLoftedReceiveHeight_m);
  return pes::PowerForLaunchSpeed(speed_mps);
}

}  // namespace

ElizaController::ElizaController(Match* match) : PlayerController(match) {
  lastDesiredDirection = Vector3(0);
  lastDesiredVelocity = 0;
}

ElizaController::~ElizaController() {
  if (Verbose())
    printf("exiting elizacontroller.. ");
  if (defenseStrategy)
    delete defenseStrategy;
  if (midfieldStrategy)
    delete midfieldStrategy;
  if (offenseStrategy)
    delete offenseStrategy;
  if (goalieStrategy)
    delete goalieStrategy;
  if (Verbose())
    printf("done\n");
}

void ElizaController::RequestCommand(PlayerCommandQueue& commandQueue) {
  CastPlayer()->SetDesiredTimeToBall_ms(0);

  _CalculateSituation();

  _Preprocess();  // calculate some variables

  FormationEntry entry = CastPlayer()->GetDynamicFormationEntry();
  float mindSet = AI_GetMindSet(entry.role);

  // input

  Vector3 rawInputDirection = player->GetDirectionVec();
  float rawInputVelocityFloat = idleVelocity;
  Vector3 manualMovementDirection = player->GetDirectionVec();
  float manualMovementVelocityFloat = idleVelocity;

  bool manualMovement = false;
  bool extraHaste = false;

  // PES's loose-ball presser (press-life 3/10): his movement is the ActionPress below, not a dribble.
  const bool loosePresser = match->PesLoop() && team->GetController()->IsLoosePresser(player);

  // celebrate good times come on!

  if (!match->IsInPlay() && match->IsGoalScored()) {
    _AddCelebration(commandQueue);
    return;
  }

  // look at referee

  else if (!match->IsInPlay() && match->GetReferee()->GetBuffer().active == true &&
           (match->GetReferee()->GetCurrentFoulType() == 2 ||
            match->GetReferee()->GetCurrentFoulType() == 3) &&
           match->GetReferee()->GetBuffer().stopTime < match->GetActualTime_ms() - 1000 &&
           match->GetReferee()->GetBuffer().prepareTime > match->GetActualTime_ms()) {
    // look at referee
    PlayerCommand command;
    command.desiredFunctionType = e_FunctionType_Movement;
    command.useDesiredMovement = true;
    command.useDesiredLookAt = true;
    command.desiredDirection = CastPlayer()->GetDirectionVec();
    assert(command.desiredDirection.coords[2] == 0.0f);
    command.desiredVelocityFloat = idleVelocity;
    command.desiredLookAt = match->GetOfficials()->GetReferee()->GetPosition();
    commandQueue.push_back(command);

    return;
  }

  // stand still

  else if (!match->IsInPlay()) {  // this whole if/then/else structure is ugly and unclear

    PlayerCommand command;
    command.desiredFunctionType = e_FunctionType_Movement;
    command.useDesiredMovement = true;
    if (match->GetBallRetainer() == player) {
      command.desiredDirection =
          (Vector3(0) - player->GetPosition()).Get2D().GetNormalized(player->GetDirectionVec());
    } else {
      command.desiredDirection = player->GetDirectionVec();
    }
    command.desiredVelocityFloat = idleVelocity;
    // PES's set-piece takers run to their spot before they take the restart (A5 §4.7; for a throw-in,
    // docs/re/pes-re-throwin-choreography.md 3.1). They are not placed on it, so until one arrives his command is the run
    // to the spot.
    if (team->GetController()->IsPieceTakerWalking(CastPlayer())) {
      // PES's pickup clip plays with the body still: the root warp moves him onto the ball (docs/re/pes-re-throwin-choreography.md 3.2).
      if (team->GetController()->IsPickingUp()) {
        command.useDesiredLookAt = true;
        command.desiredLookAt = match->GetBall()->Predict(0).Get2D();
        commandQueue.push_back(command);
        return;
      }
      const Vector3 spot = team->GetController()->GetPieceWalkTarget();
      const Vector3 toSpot = (spot - player->GetPosition()).Get2D();
      command.desiredDirection = toSpot.GetNormalized(player->GetDirectionVec());
      // PES's walker is a MOVE command, and FUN_1406b7560 (the MOVE filler every MOVE handler reaches) replaces a run-class
      // walker's desired speed with FUN_140a76070(2) = 11 km/h, the install's walk-down speed (call 0x1406b77d6, .rdata
      // 0x14259bdfc; docs/re/pes-re-walk-speed.md). GF's 2 x gap rule went through FloorVelocity, which rounds anything above the
      // walk level up to the sprint level, so the walker sprinted at every gap past about 2.5 m. PES's MOVE stops him at its
      // arrival radius (StayDecision, FUN_1406b6bb0, ported), and the lift (docs/re/pes-re-throwin-choreography.md 3.2, phase 4)
      // takes him onto the spot within 0.65 m. He stands once the lift has finished.
      // ponytail: PES applies the walk-down only past its gate (in play, the route flag, the route and collision test, and the
      // distance in frames over the frame budget; match+0x116c, playerRec+0x25930, FUN_140479470 undecoded); GF applies it to
      // the whole walk. The carry is not a MOVE in PES (the root warp takes him onto the spot), so it keeps GF's 2 x gap.
      const float gap = toSpot.GetLength();
      const float walk = team->GetController()->IsCarryingBall()
                             ? FloorVelocity(pes::kPieceTakerApproachGain * gap)
                             : match->GetPesSetPieceTakerConstants().walkDown_ms;
      command.desiredVelocityFloat = team->GetController()->PieceTakerArrived() ? idleVelocity : walk;
      command.useDesiredTarget = true;
      command.desiredTarget = spot;
      command.useDesiredLookAt = true;
      // PES's carrier turns toward his carry spot (docs/re/pes-re-throwin-choreography.md 3.2, phase 4), not the ball in his
      // hand: a look point at his own feet is zero metres from him, and MOVESTEP's look distance then fails ACCELERATE's gate.
      // Once the lift has stood him on the spot (phase 5: THROWIN_LOOP), he keeps the lift's yaw: he looks along his root, not
      // at the spot, which is now under his feet and has no direction.
      constexpr float kHeldLookAhead_m = 1.0f;  // the look point's distance along the root: any length gives the same heading
      if (team->GetController()->PieceTakerArrived()) {
        command.desiredLookAt = (player->GetPosition() + player->GetRootDirectionVec() * kHeldLookAhead_m).Get2D();
      } else {
        command.desiredLookAt =
            team->GetController()->IsCarryingBall() ? spot.Get2D() : match->GetBall()->Predict(0).Get2D();
      }
      commandQueue.push_back(command);
      return;
    }
    // A placed set-piece taker holds his stand point until the kick is released (pes-re-kicker-standby.md section 4).
    // ponytail: PES holds the body in the standby clip with the default locomotion skipped; GF holds it with this idle stand
    // until the standby clip family lands (docs/re/pes-re-kicker-standby.md section 3).
    if (team->GetController()->IsPieceTakerHeld(CastPlayer())) {
      command.useDesiredLookAt = true;
      // A corner taker looks along his SetKick heading (PES's standby holds the SetKick yaw); the others look at the ball.
      command.desiredLookAt = team->GetController()->HeldTakerLookAt(match->GetBall()->Predict(0)).Get2D();
      commandQueue.push_back(command);
      return;
    }

    // Law 16: the opponents leave the penalty area before a goal kick is taken. They
    // walk out under their own steam - the taker waits for them (referee.cpp) - rather
    // than being teleported clear at the whistle. Without this a striker standing in
    // the six-yard box when the kick was awarded simply stayed there, and the kick
    // could be played to his feet.
    const e_SetPiece pending = team->GetController()->GetSetPieceType();
    const bool ourKick = team->GetController()->GetPieceTaker() != nullptr;
    // The box being kicked out of is the taker's own, which is the side we attack.
    const int defendedSide = -team->GetSide();
    if (!ourKick &&
        SetPieceLaws::ClearsThePenaltyArea(
            pending, match->GetReferee()->GetBuffer().restartPos.coords[0], defendedSide,
            pitchHalfW)) {
      const Vector3 here = player->GetPosition();
      if (SetPieceLaws::IntrudesOnPenaltyArea(here.coords[0], here.coords[1], defendedSide,
                                             pitchHalfW)) {
        float targetX = here.coords[0], targetY = here.coords[1];
        SetPieceLaws::ClearingTarget(here.coords[0], here.coords[1], defendedSide, pitchHalfW,
                                    &targetX, &targetY);
        const Vector3 target(targetX, targetY, 0);
        command.desiredDirection = (target - here).Get2D().GetNormalized(player->GetDirectionVec());
        // Jogging, not sprinting: a player asked to step out of the box is not in a
        // hurry, and a sprint reads as a scramble.
        command.desiredVelocityFloat = walkVelocity;
        command.useDesiredLookAt = true;
        command.desiredLookAt = target;
        command.useDesiredTarget = true;
        command.desiredTarget = target;
        command.preciseArrival = true;  // PES cmd[0x15]: the clearing walk stops on its point
        commandQueue.push_back(command);
        return;
      }
    }

    // Law 13: and at a free kick or a corner they stand off the ball. A defender who
    // was standing over it backs straight out of the circle - facing the ball, because
    // that is what a man retreating from a free kick does - rather than being shoved.
    if (!ourKick && SetPieceLaws::ClearsTheBallRadius(pending)) {
      const Vector3 here = player->GetPosition();
      const Vector3 ball = match->GetBall()->Predict(0).Get2D();
      if (SetPieceLaws::IntrudesOnBallRadius(here.coords[0], here.coords[1], ball.coords[0],
                                            ball.coords[1])) {
        float targetX = here.coords[0], targetY = here.coords[1];
        SetPieceLaws::RetreatTarget(here.coords[0], here.coords[1], ball.coords[0],
                                   ball.coords[1], &targetX, &targetY);
        const Vector3 target(targetX, targetY, 0);
        // 2D: a player's heading must not carry the ball's height.
        command.desiredDirection = (target - here).Get2D().GetNormalized(player->GetDirectionVec());
        command.desiredVelocityFloat = walkVelocity;
        command.useDesiredLookAt = true;
        command.desiredLookAt = ball;
        command.useDesiredTarget = true;
        command.desiredTarget = target;
        commandQueue.push_back(command);
        return;
      }
    }

    if (!match->IsInSetPiece()) {
      command.desiredDirection =
          (player->GetDirectionVec() * 0.6f +
           (Vector3(0) - player->GetPosition()).GetNormalized(player->GetDirectionVec()) * 0.4f)
              .GetNormalized(player->GetDirectionVec());
      command.desiredVelocityFloat =
          ClampVelocity(player->GetFloatVelocity() * 0.95f - random(0.0f, 3.2f));
    }
    command.useDesiredLookAt = true;
    command.desiredLookAt =
        player->GetPosition() + (match->GetBall()->Predict(0).Get2D() - player->GetPosition())
                                        .GetNormalized(command.desiredDirection) *
                                    10.0f;
    commandQueue.push_back(command);

    return;
  }

  // set piece taking

  else if ((match->IsInSetPiece() && team->GetController()->GetPieceTaker() == player) ||
           match->GetBallRetainer() == player) {
    PlayerCommand actionCommand;

    if (team->GetController()->GetSetPieceType() == e_SetPiece_Penalty) {
      actionCommand.desiredFunctionType = e_FunctionType_Shot;
      actionCommand.useDesiredMovement = false;
      actionCommand.useDesiredLookAt = false;

      // During a shootout the placement comes from the taker's stats; a penalty
      // in open play stays as unpredictable as it was.
      float aimY = random(-5, 5);
      float aimPower = random(0.4f, 1.0f);
      float aimGoalX = -team->GetSide() * pitchHalfW;
      PenaltyShootoutController* shootout = match->GetPenaltyShootout();
      if (shootout && shootout->IsStarted() && !shootout->IsFinished()) {
        aimY = shootout->GetAimY();
        aimPower = shootout->GetAimPower();
        // The whole shootout is taken at one end, which may be the end this team
        // was defending during the match.
        aimGoalX = shootout->GetGoalX();
      }

      actionCommand.touchInfo.inputDirection =
          (Vector3(aimGoalX, aimY, 0) - CastPlayer()->GetPosition())
              .GetNormalized(Vector3(-team->GetSide(), 0, 0));
      actionCommand.touchInfo.inputPower = aimPower;
      actionCommand.touchInfo.autoDirectionBias = 0.0f;
      actionCommand.touchInfo.desiredDirection = actionCommand.touchInfo.inputDirection;
      actionCommand.touchInfo.desiredPower = aimPower;
      commandQueue.push_back(actionCommand);

    } else {
      actionCommand.desiredFunctionType = e_FunctionType_ShortPass;
      actionCommand.useDesiredMovement = false;
      actionCommand.useDesiredLookAt = false;
      actionCommand.touchInfo.inputDirection = player->GetDirectionVec();
      actionCommand.touchInfo.inputPower = 0.5f;
      actionCommand.touchInfo.autoDirectionBias = 1.0f;
      actionCommand.touchInfo.autoPowerBias = 1.0f;

      actionCommand.touchInfo.forcedTargetPlayer = 0;

      Vector3 desiredTargetPosition;
      bool doCommand = true;
      bool pesGoalKick = false;

      if (team->GetController()->GetSetPieceType() == e_SetPiece_GoalKick && match->PesLoop()) {
        // PES's goal kick (pesgoalkick.hpp): the ladder's order, released by the referee. The strike owns the keeper's root
        // until its contact tick, so no kick command goes out before it.
        if (!team->GetController()->GoalKickInContact()) return;
        pesGoalKick = true;
        actionCommand.desiredFunctionType = e_FunctionType_HighPass;
        if (Player* receiver = team->GetController()->GoalKickReceiver()) {
          actionCommand.touchInfo.forcedTargetPlayer = receiver;
          desiredTargetPosition = receiver->GetPosition();
        } else {
          // ponytail: PES's receiver-less kick (player 0xff) has no decoded target. GF aims at the midfield point on the
          // attacking side, the old stand-in's x, until that is traced.
          desiredTargetPosition = Vector3((pitchHalfW * -team->GetSide()) * 0.2f,
                                          player->GetPosition().coords[1], 0.0f);
        }

      } else if (team->GetController()->GetSetPieceType() == e_SetPiece_GoalKick) {
        if (random(0.0f, 1.0f) > 0.4f) {
          actionCommand.desiredFunctionType = e_FunctionType_HighPass;
          desiredTargetPosition = Vector3((pitchHalfW * -team->GetSide()) * 0.2f,
                                          random(-pitchHalfH, pitchHalfH), 0.0f);
        } else {
          actionCommand.desiredFunctionType = e_FunctionType_ShortPass;
          desiredTargetPosition = Vector3(player->GetPosition().coords[0] * 0.9f,
                                          random(-pitchHalfH, pitchHalfH), 0.0f);
        }

      } else if (team->GetController()->GetSetPieceType() == e_SetPiece_KickOff) {
        actionCommand.desiredFunctionType = e_FunctionType_ShortPass;
        desiredTargetPosition = player->GetPosition() + player->GetDirectionVec() * 1.0f;

      } else if (team->GetController()->GetSetPieceType() == e_SetPiece_FreeKick) {
        if (random(0.0f, 1.0f) > 0.5f) {
          actionCommand.desiredFunctionType = e_FunctionType_HighPass;
          desiredTargetPosition =
              Vector3(pitchHalfW * -team->GetSide(), random(-10.0f, 10.0f), 0.0f);
        } else {
          actionCommand.desiredFunctionType = e_FunctionType_ShortPass;
          desiredTargetPosition = player->GetPosition() +
                                  Vector3(-team->GetSide() * 10.0f, random(-10.0f, 10.0f), 0.0f);
        }

      } else if (team->GetController()->GetSetPieceType() == e_SetPiece_Corner) {
        if (random(0.0f, 1.0f) > 0.3f) {
          actionCommand.desiredFunctionType = e_FunctionType_HighPass;
          desiredTargetPosition =
              Vector3((pitchHalfW * -team->GetSide()) * (0.99f - random(0.0f, 0.12f)),
                      random(-10.0f, 10.0f), 0.0f);
        } else {
          actionCommand.desiredFunctionType = e_FunctionType_ShortPass;
          desiredTargetPosition = Vector3((pitchHalfW * -team->GetSide()) * 0.8f,
                                          player->GetPosition().coords[1] * 0.8f, 0.0f);
        }

      } else if (team->GetController()->GetSetPieceType() == e_SetPiece_ThrowIn) {
        actionCommand.desiredFunctionType = e_FunctionType_ShortPass;
        desiredTargetPosition = player->GetPosition();  // closest to player
        // PES's throw is aimed at a receiver inside the touchline (docs/re/pes-re-throwin-choreography.md
        // 6.1). A mate on the line sends the ball along it, out again, so the nearest mate inside the
        // line is the target. ponytail: PES's receiver search (FUN_142135700) and course test are not
        // ported; the nearest inside mate stands in for them.
        if (match->PesLoop()) actionCommand.touchInfo.forcedTargetPlayer = team->GetController()->ThrowInTargetMate(CastPlayer());
        // Long Throw: hurled towards the box rather than to the nearest man.
        if (PlayerSkills::Has(CastPlayer()->GetPlayerData()->GetSkills(),
                              PlayerSkills::Skill::LongThrow)) {
          actionCommand.desiredFunctionType = e_FunctionType_HighPass;
          desiredTargetPosition += Vector3(-team->GetSide() * 18.0f, 0, 0);
        }

      } else if (match->GetBallRetainer() == player) {  // keeper fetched ball, probably
        // PES's after-catch distribution (ActionKeeperAfterCatchMove::exec
        // 0x1409e8880, docs/re/pes-re-keeper-port.md 2 and the corrections of
        // pes-re-goalkeeper.md 10.6.2). PES's point is where the keeper WALKS
        // while holding the ball, not the pass target - the pass target is the
        // chosen team-mate, which is what this uses it for: he walks toward
        // the free man and GF's pass goes to him.
        const pes::KeeperConstants& keeperConstants = match->GetPesKeeperConstants();
        // His own goal-area and box marks (PES F5/F6/F7). The keeper of the -x
        // goal attacks +x, so his own half is -x: the marks are magnitudes and
        // `attackDir` carries the sign.
        const int attackDir = -team->GetSide();
        const pes::KeeperFieldMarks marks = pes::KeeperFieldMarksFromFieldInfo();
        const pes::DistributionPoint carry = pes::PesAfterCatchCarry(
            player->GetPosition().coords[0], player->GetPosition().coords[1], attackDir, marks,
            keeperConstants);
        // PES's teammate (pes-re-keeper-port.md §2.2): roll over the keeper's own NearestList, his team's
        // block nearest first, and take the first teammate whose squared distance is under 100.0 (0x14253EE30,
        // the carrier constant sfCeilOpp_m). INF: block 0 holds the keeper-to-teammate d^2.
        Player* targetPlayer = nullptr;
        {
          std::vector<Player*> squad;
          team->GetActivePlayers(squad);
          const auto it = std::find(squad.begin(), squad.end(), CastPlayer());
          if (it != squad.end() && it - squad.begin() < pes::lane::kSideSize) {
            const pes::SortedRow& nl = team->GetController()->Rows().nearest[it - squad.begin()];
            const float pickD2_m2 = match->GetPesCarrierConstants().sfCeilOpp_m;
            for (int r = 0; r < nl.Count(0); ++r) {
              const pes::RowEntry& e = nl.Entry(0, r);
              if (e.value < pickD2_m2) {
                targetPlayer = squad[e.id];
                break;
              }
            }
          }
        }

        if (targetPlayer) {
          // check if this player is away from opponents, before throwing ball to him
          Player* closestOpp = AI_GetClosestPlayer(match->GetTeam(abs(team->GetID() - 1)),
                                                   targetPlayer->GetPosition(), 0);
          // PES's holder clock, not GF's possession clock: MA+0x2601C persists on a loose ball (pes-re-first-touch 5.1),
          // and GF's clock restarts with every reach flicker, so a man beside a static ball never reached the bias.
          const double heldBallMs = match->PesLoop() && match->GetPesHolder() == CastPlayer()
                                        ? match->GetActualTime_ms() - match->GetPesHolderSince_ms()
                                        : CastPlayer()->GetPossessionDuration_ms();
          if (closestOpp) {
            if (((closestOpp->GetPosition() + closestOpp->GetMovement() * 0.1f) -
                 targetPlayer->GetPosition())
                        .GetLength() > 10.0f ||
                heldBallMs > 4000) {  // last touch bias is maximum so he won't hold forever
              actionCommand.touchInfo.forcedTargetPlayer = targetPlayer;
            } else {
              doCommand = false;
            }
          }
        }

        if (doCommand) {
          // With no reachable teammate the carry point itself is the target,
          // which PES's fallback (the keeper, displaced 2 m) also lands on.
          const float targetX = targetPlayer ? targetPlayer->GetPosition().coords[0] : carry.x;
          const float targetZ = targetPlayer ? targetPlayer->GetPosition().coords[1] : carry.z;
          const pes::DistributionPoint walk = pes::PesAfterCatchWalk(
              player->GetPosition().coords[0], player->GetPosition().coords[1], carry.x, carry.z,
              targetX, targetZ, attackDir, marks, keeperConstants);
          desiredTargetPosition = Vector3(walk.x, walk.z, 0.0f);
        }
      }

      if (doCommand) {
        if (actionCommand.touchInfo.forcedTargetPlayer == 0)
          actionCommand.touchInfo.forcedTargetPlayer =
              AI_GetClosestPlayer(team, desiredTargetPosition, CastPlayer());
        AI_GetPass(CastPlayer(), actionCommand.desiredFunctionType,
                   actionCommand.touchInfo.inputDirection, actionCommand.touchInfo.inputPower,
                   actionCommand.touchInfo.autoDirectionBias, actionCommand.touchInfo.autoPowerBias,
                   actionCommand.touchInfo.desiredDirection, actionCommand.touchInfo.desiredPower,
                   actionCommand.touchInfo.targetPlayer,
                   actionCommand.touchInfo.forcedTargetPlayer);
        if (pesGoalKick) {
          // The power that lofts the ball onto the target: the launch speed PES's lofted kick
          // needs along the direction AI_GetPass aimed (pesgoalkick.hpp).
          const Vector3 target = actionCommand.touchInfo.forcedTargetPlayer
                                     ? actionCommand.touchInfo.forcedTargetPlayer->GetPosition()
                                     : desiredTargetPosition;
          actionCommand.touchInfo.desiredPower = PesLoftedKickPower(
              match, actionCommand.touchInfo.desiredDirection, target);
        }
        commandQueue.push_back(actionCommand);
      }
    }

    // remember, we are still in the 'set piece / ballretainer' if{}

    // PES's set-piece taker who does not have the ball yet. A throw-in carrier holds it from the pickup to the throw
    // (docs/re/pes-re-throwin-choreography.md 3.2), so one the walk has let go of takes it up again within the pickup reach
    // (the install's 5 m). A free kick, corner or penalty taker runs to the ball and strikes it, as PES's kicker does: GF's
    // ball-control hover stood off the ball instead (seed 7's free kick, 2.3 m away and not moving, the 1758-tick stall).
    // ponytail: PES's approach to a free kick, corner or penalty is not decoded; the run to the ball stands in for it.
    const e_SetPiece restart = team->GetController()->GetSetPieceType();
    const Vector3 ballNow = match->GetBall()->Predict(0);
    if (match->PesLoop() && match->GetBallRetainer() != player && team->GetController()->GetPieceTaker() == player) {
      const Vector3 toBall = (ballNow - player->GetPosition()).Get2D();
      if (restart == e_SetPiece_ThrowIn &&
          toBall.GetLength() <= match->GetPesSetPieceTakerConstants().throwInPickupReach_m) {
        CastPlayer()->SelectRetainAnim();
      } else if (restart == e_SetPiece_FreeKick || restart == e_SetPiece_Corner || restart == e_SetPiece_Penalty) {
        PlayerCommand run;
        run.desiredFunctionType = e_FunctionType_Movement;
        run.useDesiredMovement = true;
        run.desiredDirection = toBall.GetNormalized(player->GetDirectionVec());
        const float approach = pes::kPieceTakerApproachGain * toBall.GetLength();
        run.desiredVelocityFloat = FloorVelocity(approach);
        run.useDesiredLookAt = true;
        run.desiredLookAt = ballNow.Get2D();
        run.useDesiredTarget = true;
        run.desiredTarget = ballNow.Get2D();
        commandQueue.push_back(run);
        return;
      } else if (restart == e_SetPiece_ThrowIn) {
        // PES's throw-in taker without the ball and out of his reach stands: the walk owns his approach, and the
        // ball-control hover would run him at a ball he is not carrying.
        PlayerCommand stand;
        stand.desiredFunctionType = e_FunctionType_Movement;
        stand.useDesiredMovement = true;
        stand.desiredDirection = player->GetDirectionVec();
        stand.desiredVelocityFloat = idleVelocity;
        stand.useDesiredLookAt = true;
        stand.desiredLookAt = ballNow.Get2D();
        commandQueue.push_back(stand);
        return;
      }
    }

    if (match->GetBallRetainer() != player) {  // must be set piece taker, then
      PlayerCommand command;
      command.desiredFunctionType = e_FunctionType_Movement;
      command.useDesiredMovement = true;
      command.useDesiredLookAt = true;

      AI_GetBallControlMovement(_mentalImage, CastPlayer(), player->GetDirectionVec(), walkVelocity,
                                command.desiredDirection, command.desiredVelocityFloat,
                                command.desiredLookAt);
      assert(command.desiredDirection.coords[2] == 0.0f);

      commandQueue.push_back(command);
      return;
    } else {  // must be ball retainer
      PlayerCommand command;
      command.desiredFunctionType = e_FunctionType_Movement;
      command.useDesiredMovement = true;
      command.desiredDirection =
          (actionCommand.touchInfo.desiredDirection.GetLength() > 0.0f)
              ? actionCommand.touchInfo.desiredDirection.Get2D().GetNormalized(
                    player->GetDirectionVec())
              : (Vector3(0) - player->GetPosition()).GetNormalized(player->GetDirectionVec());
      command.desiredVelocityFloat = idleVelocity;
      command.useDesiredLookAt = true;
      command.desiredLookAt = player->GetPosition() + command.desiredDirection * 10.0f;
      commandQueue.push_back(command);
      if (team->GetController()->GetSetPieceType() == e_SetPiece_ThrowIn) {
        // printf("elizacontroller throw in doCommand (green pilon == target)\n");
        // SetGreenDebugPilon(command.desiredLookAt);
      }
      return;
    }

  }

  // default strategies for defensively, midfielders and offensively positioned players

  else if (match->IsInPlay() && !match->IsInSetPiece() && match->GetBallRetainer() != player &&
           match->GetDesignatedPossessionPlayer() != player &&
           CastPlayer()->GetFormationEntry().role !=
               e_PlayerRole_GK) {  //(match->GetDesignatedPossessionPlayer() != player ||
                                   //(CastPlayer()->GetFormationEntry().role == e_PlayerRole_GK &&
                                   //! CastPlayer()->HasPossession()))) {
    if (CastPlayer()->GetFormationEntry().role == e_PlayerRole_LB ||
        CastPlayer()->GetFormationEntry().role == e_PlayerRole_CB ||
        CastPlayer()->GetFormationEntry().role == e_PlayerRole_RB) {
      defenseStrategy->RequestInput(_mentalImage, rawInputDirection, rawInputVelocityFloat);
    } else if (CastPlayer()->GetFormationEntry().role == e_PlayerRole_DM ||
               CastPlayer()->GetFormationEntry().role == e_PlayerRole_LM ||
               CastPlayer()->GetFormationEntry().role == e_PlayerRole_CM ||
               CastPlayer()->GetFormationEntry().role == e_PlayerRole_RM ||
               CastPlayer()->GetFormationEntry().role == e_PlayerRole_AM ||
               CastPlayer()->GetFormationEntry().role == e_PlayerRole_LW ||
               CastPlayer()->GetFormationEntry().role == e_PlayerRole_RW) {
      midfieldStrategy->RequestInput(_mentalImage, rawInputDirection, rawInputVelocityFloat);
    } else if (CastPlayer()->GetFormationEntry().role == e_PlayerRole_CF ||
               CastPlayer()->GetFormationEntry().role == e_PlayerRole_SS) {
      offenseStrategy->RequestInput(_mentalImage, rawInputDirection, rawInputVelocityFloat);
    }

  }

  // dribble, pass, etcetera
  else if (match->IsInPlay() && !match->IsInSetPiece() &&
           match->GetDesignatedPossessionPlayer() == player &&
           CastPlayer()->GetFormationEntry().role != e_PlayerRole_GK) {
    if (CastPlayer()->GetTimeNeededToGetToBall_ms() < 1000) {
      // Movement first, decision second. A designated chaser standing next to
      // a still ball he does not have is the shape the 14-second stalls take:
      // inPlay 1, ball at rest, forced man 0.4 m off it, nothing queued that
      // closes the gap. Pass selection runs and the step onto the ball never
      // does. So step onto it first - once he has it, the branch below is
      // business as usual.
      const float ballSpeed_ms = match->GetBall()->GetMovement().GetLength();
      const bool hasIt = match->GetBallRetainer() == player || CastPlayer()->HasPossession();
      // Not gated on the stalemate clock. The inPlay-1 stalls this exists for
      // idle at 5-14 s, well under ShouldReleaseBall's 15, so gating on it would
      // leave the measured case untouched. Close, designated, and without it:
      // step onto it.
      if (!hasIt && ballSpeed_ms <= GameplayTuning::kStalemateBallSpeed_ms) {
        // GF's chase step, off the PES path: a PES loose ball is approached by the PRESS man's ActionPress
        // (pes-re-press-life §8), and the designated man is no chaser (pes-re-first-touch §7.1).
        if (!match->PesLoop()) {
          const Vector3 toBall =
              match->GetBall()->GetPositionBuffer() - CastPlayer()->GetPosition();
          if (toBall.GetLength() > 0.1f) {
            PlayerCommand step;
            step.desiredFunctionType = e_FunctionType_Movement;
            step.useDesiredMovement = false;
            step.useDesiredLookAt = true;
            // 2D: the ball's height must not enter the heading.
            step.desiredDirection = toBall.Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
            step.desiredVelocityFloat = sprintVelocity;
            step.desiredLookAt = match->GetBall()->GetPositionBuffer();
            commandQueue.push_back(step);
            extraHaste = true;
          }
        }
      } else if (!loosePresser || hasIt) {
        GetOnTheBallCommands(commandQueue, rawInputDirection, rawInputVelocityFloat);
        // A loose-ball presser without the ball: nothing here, the press steers him below.
      }
    } else {
      // Out of reach of the ball, so he cannot pass or dribble it - but he can
      // still run at it, and he must. This branch used to queue NOTHING: the
      // designated ball-goer stood still while every other player held his
      // shape, and a ball at rest more than a second from everyone froze the
      // whole pitch for the rest of the half (measured: 32:19 to 45:00).
      // A loose-ball presser steers through the movement command below.
      const Vector3 toBall = match->GetBall()->GetPositionBuffer() - CastPlayer()->GetPosition();
      if (match->PesLoop() && !match->GetBallRetainer()) {
        // PES's ActionPress approach (docs/re/pes-re-press-life.md sections 7 and 9): the ball now, or his own base when the
        // ball sample at k ticks sits nearer the base, at the press speed curve. PES sends a PRESS man to a loose ball; GF's
        // designated man stands in for him here. MOVESTEP steers by this command's target (pesmovestep MovementDecision).
        const pes::PressConstants& press = match->GetPesPressConstants();
        const Vector3 ballNow = match->GetBall()->Predict(0);
        const Vector3 sample = match->GetBall()->Predict(
            static_cast<unsigned int>(pes::PressTargetTick(press) * Match::kTick_ms));
        // ponytail: his base (AI+0x1414) is GF's adapted formation position.
        const Vector3 base = team->GetController()->GetAdaptedFormationPosition(CastPlayer(), true);
        const bool atBall = pes::PressTargetIsBall({sample.coords[0], sample.coords[1]},
                                                   {ballNow.coords[0], ballNow.coords[1]},
                                                   {base.coords[0], base.coords[1]});
        const Vector3 target = atBall ? ballNow.Get2D() : base.Get2D();
        const Vector3 toTarget = target - CastPlayer()->GetPosition();
        // m/s to km/h is arithmetic (3600/1000), not a PES number.
        const float speedKmh = CastPlayer()->GetMovement().GetLength() * 3.6f;
        PlayerCommand approach;
        approach.desiredFunctionType = e_FunctionType_Movement;
        approach.useDesiredMovement = false;
        approach.useDesiredLookAt = true;
        approach.desiredDirection = toTarget.Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
        approach.desiredVelocityFloat = pes::PressCommandSpeed_ms(press, toTarget.GetLength(), speedKmh,
                                                                  distanceToVelocityMultiplier);
        approach.desiredLookAt = target;
        approach.useDesiredTarget = true;
        approach.desiredTarget = target;
        commandQueue.push_back(approach);
      } else if (!match->PesLoop() && toBall.GetLength() > 0.5f) {
        PlayerCommand run;
        run.desiredFunctionType = e_FunctionType_Movement;
        run.useDesiredMovement = false;
        run.useDesiredLookAt = true;
        // 2D: a ball in the air must not tilt the run at it.
        run.desiredDirection = toBall.Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
        // Sprint: he is chasing a ball nobody else is going for.
        run.desiredVelocityFloat = sprintVelocity;
        run.desiredLookAt = match->GetBall()->GetPositionBuffer();
        run.useDesiredTarget = true;
        run.desiredTarget = match->GetBall()->GetPositionBuffer();
        commandQueue.push_back(run);
        extraHaste = true;
      }
    }
    extraHaste = false;  // todo: extra haste when touch anim is queued?
  }

  else if (CastPlayer()->GetFormationEntry().role == e_PlayerRole_GK &&
           ((match->IsInPlay() && !match->IsInSetPiece()) ||
            match->GetPenaltyShootout()->IsKeepingKick(team->GetID()))) {
    // keeper's mental image for deflections is near-instant; let's just call it premonition ;)
    const MentalImage* keeperMentalImage = _mentalImage;
    static_cast<GoalieDefaultStrategy*>(goalieStrategy)
        ->CalculateIfBallIsBoundForGoal(keeperMentalImage);
    bool boundForGoal = static_cast<GoalieDefaultStrategy*>(goalieStrategy)->IsBallBoundForGoal();
    if (boundForGoal)
      manualMovement = true;
    if (CastPlayer() != match->GetDesignatedPossessionPlayer())
      manualMovement = true;
    goalieStrategy->RequestInput(keeperMentalImage, manualMovementDirection,
                                 manualMovementVelocityFloat);

    // if (boundForGoal && hasUniquePossession) printf("has unique possession, so no deflect anims
    // (poss: %f)\n", possessionAmount);
    if (!hasUniquePossession || possessionAmount < 3.4f) {
      // A ball bound for goal is exactly when he must be able to HOLD it.
      // `boundForGoal` used to switch the pickup anims off, so the harder the
      // shot to stop, the less he was allowed to catch it - and a ball he meets
      // but does not hold is a rebound. Measured: save rate 25%, beaten gaps of
      // 1-2 m, holds rising while saves fell, 14.25 goals a match against the
      // 3.88 the owner's target asks for. Whether he can make the save at all
      // is KeeperAttemptsSave's business, one call down; this decides only
      // whether the catch he makes can stick.
      bool onlyPickupAnims = false;
      if (possessionAmount > 1.3f)
        onlyPickupAnims = true;
      _KeeperDeflectCommand(commandQueue, onlyPickupAnims);
    }

    // PES 2021's rush (docs/re/pes-re-goalkeeper.md 10.3, KP_PRESS decider
    // FUN_142115100/1421162c0): he goes for a loose ball when he wins the race
    // by the style factor - the opponent must need more than k times his own
    // frames, and once he is rushing the 0.71 s hysteresis keeps him going. A
    // ball more than 3.3 s away is never chased. This replaces the flat "within
    // 1000 ms and designated" rule, which had no idea who else was going.
    //
    // PES's style bits 19/20 are GF's Offensive/Defensive GK playing styles.
    // Where the style is neither, the no-style factors apply.
    const pes::KeeperConstants& keeperConstants = match->GetPesKeeperConstants();
    const PlayingStyles::Player gkStyle =
        CastPlayer()->GetPlayerData()->GetPlayingStyle();
    const bool offensiveStyle = gkStyle == PlayingStyles::Player::OffensiveGoalkeeper;
    const bool defensiveStyle = gkStyle == PlayingStyles::Player::DefensiveGoalkeeper;
    const float factor = pes::PesKeeperRushFactor(offensiveStyle, defensiveStyle, keeperConstants);
    const bool rushing =
        pes::PesKeeperRushRaceWins(CastPlayer()->GetTimeNeededToGetToBall_ms() * 0.001f,
                                   oppTimeNeededToGetToBall * 0.001f, keeperRushing, factor,
                                   keeperConstants);
    // The lateral limit on the point where he meets the ball: F5 for a
    // Defensive GK, F7 for an Offensive one, the midpoint for no style.
    const pes::KeeperFieldMarks marks = pes::KeeperFieldMarksFromFieldInfo();
    const float lateralLimit_m =
        pes::PesKeeperRushLateralLimit_m(offensiveStyle, defensiveStyle, marks);
    keeperRushing = rushing;
    bool keeperChases = rushing;
    // The meeting point is the ball where his own reach says he gets it; the
    // lateral limit is on that point (10.3's limits on P(kf)).
    const Vector3 meetPoint =
        _mentalImage->GetBallPrediction(CastPlayer()->GetTimeNeededToGetToBall_ms()).Get2D();
    if (std::fabs(meetPoint.coords[1]) > lateralLimit_m) keeperChases = false;
    // And the area rule (10.3): a claim outside his own penalty area is an
    // outfield presser, not a keeper's rush, so he leaves his box only through
    // the outfield branch.
    const float claimDepth = pitchHalfW - std::fabs(meetPoint.coords[0]);
    if (claimDepth > SetPieceLaws::kAreaDepth) keeperChases = false;

    if (keeperChases && match->GetDesignatedPossessionPlayer() == player) {
      GetOnTheBallCommands(commandQueue, rawInputDirection, rawInputVelocityFloat);
    }
  }

  bool forceMagnet = false;


  _SetInput(rawInputDirection, rawInputVelocityFloat);
  if (rawInputDirection.GetLength() != 0)
    lastDesiredDirection = rawInputDirection;
  else
    lastDesiredDirection = player->GetDirectionVec();
  lastDesiredVelocity = rawInputVelocityFloat;

  if (match->IsInPlay() && !match->IsInSetPiece()) {
    // ball control?
    // PES: a loose ball is reached by the reaction's TRAP (pes-re-first-touch §7.2); ball control belongs to the
    // man who has the ball. ponytail: PES's carrier brain (MA+0x2601C) is not decoded here.
    // A keeper whose PES rush has won the race (KP_PRESS, pes-re-goalkeeper.md 10.3) goes for the ball.
    // ponytail: PES moves him with its keeper mover (action 0x3e, not decoded); GF's ball control stands in for it.
    const bool keeperRush = CastPlayer()->GetFormationEntry().role == e_PlayerRole_GK && keeperRushing;
    if (!match->PesLoop() || CastPlayer()->HasPossession() || keeperRush)
      _BallControlCommand(commandQueue, false, false,
                          false);  // last param true == enable sticky run direction.

    // PES 2021's one ordered reaction table for the engaging defender
    // (docs/re/pes-re-defender-decisions.md 1): walked every tick, the first
    // predicate that returns true wins and replaces the movement command. GF's
    // own trap/interfere/sliding trio each decided for itself, with a duel
    // roll each; they are gone.
    _ReactionCommands(commandQueue);
  }

  // movement
  if (!manualMovement && match->IsInPlay() && !match->IsInSetPiece()) {
    Player* opp = match->GetTeam(abs(team->GetID() - 1))->GetDesignatedTeamPossessionPlayer();

    if (CastPlayer() != match->GetDesignatedPossessionPlayer()) {
      float mindSet = AI_GetMindSet(CastPlayer()->GetDynamicFormationEntry().role);
      float huntDistanceThreshold = 10.0f + (1.0f - mindSet) * 10.0f;  // 10 + .. * 10
      huntDistanceThreshold *= 0.5f * CastPlayer()->GetStaminaPct() +
                               0.5f * (1.0f - NormalizedClamp(CastPlayer()->GetAverageVelocity(10),
                                                              idleVelocity, sprintVelocity));
      huntDistanceThreshold *= 0.3f + GetMatch()->GetMatchDifficulty() * 0.7f;

      if (forceMagnet) {
        // make sure the movement command magnets get the best input (which is then used as 'hint'
        // for the 'toball' functions)

        inputDirection = Vector3(team->GetSide() * pitchHalfW, 0, 0) - CastPlayer()->GetPosition();
        inputVelocityFloat = idleVelocity;
        inputDirection.Normalize(CastPlayer()->GetDirectionVec());
      } else if (const TeamAIController::ConceptOrder* order =
                     team->GetController()->GetConceptOrder(CastPlayer())) {
        // A team concept's order (T+0xb774 + the T+0x26c point, transitions
        // 2.1/3.1): run to the concept's point.
        const Vector3 toTarget = order->target - CastPlayer()->GetPosition();
        inputDirection = toTarget.GetNormalized(inputDirection);
        inputVelocityFloat =
            clamp(toTarget.GetLength() * distanceToVelocityMultiplier, 0.0f, sprintVelocity);

      } else if (!teamHasBestPossession && CastPlayer()->GetManMarkingID() == -1 &&
                 ((opp->GetPosition() + opp->GetMovement() * 0.12f) -
                  (CastPlayer()->GetPosition() + CastPlayer()->GetMovement() * 0.04f))
                         .GetLength() < huntDistanceThreshold) {  // defend player

        if (player == team->GetDesignatedTeamPossessionPlayer() && possessionAmount > 0.8f) {
          forceMagnet = true;  // don't give up battles too easily
          extraHaste = true;
        }

        // or, defend an opponent, if we don't have anything better to do anyway

        Player* opp = match->GetTeam(abs(team->GetID() - 1))->GetDesignatedTeamPossessionPlayer();

        int huntingPlayersNum = 2;  // remember, this includes players with man marking id that are
                                    // not controlled by this hunting code
        std::vector<Player*> closestPlayers;
        AI_GetClosestPlayers(team, opp->GetPosition() + opp->GetMovement() * 0.1f,
                             closestPlayers, huntingPlayersNum);
        bool close = false;
        for (unsigned int i = 0; i < closestPlayers.size(); i++) {
          if (closestPlayers.at(i) == player) {
            close = true;
            break;
          }
        }

        if (close) {
          /*
          // 'easy off' method
          Vector3 defendPosition_easy = CastPlayer()->GetPosition();
          AddDefensiveComponent(defendPosition_easy, 1.0f, opp->GetID());
          inputDirection = (defendPosition -
          CastPlayer()->GetPosition()).GetNormalized(inputDirection); inputVelocityFloat =
          clamp((defendPosition - CastPlayer()->GetPosition()).GetLength() *
          distanceToVelocityMultiplier, 0.0f, sprintVelocity);
          //forceMagnet = true; // more aggressive!
          */

          // more 'hunting' method
          Vector3 defendPosition = GetDefendPosition(opp);
          if (NeedDefendingMovement(team->GetSide(), player->GetPosition(), defendPosition)) {
            inputDirection =
                (defendPosition - CastPlayer()->GetPosition()).GetNormalized(inputDirection);
            inputVelocityFloat = clamp((defendPosition - CastPlayer()->GetPosition()).GetLength() *
                                           distanceToVelocityMultiplier,
                                       0.0f, sprintVelocity);
            forceMagnet = true;  // more aggressive!
          }

        }  // close

      }  // else (!forceMagnet)

    }  // !designated

    // PES 2021's ActionPress target and pace (docs/re/pes-re-press-life.md
    // 7/9, FUN_140639100/FUN_140a75be0) for the loose-ball presser: the ball
    // now or his own base, whichever the ball sample at k ticks sits nearer,
    // at the arrival slope capped one class band above his current pace. The
    // movement command below runs it command-led (useDesiredMovement), which
    // is what lets him ramp to a sprint; GF's clip-led chase step is gone on
    // this path. The command carries the target too (useDesiredTarget), so MOVESTEP
    // steers to it. For the designated presser the movement magnets below still
    // blend toward the ball - this input is their hint.
    std::optional<Vector3> pressTarget;  // the ActionPress target, when this block computes one
    if (match->PesLoop() && team->GetController()->IsLoosePresser(player)) {
      const pes::PressConstants& press = match->GetPesPressConstants();
      const Vector3 ballNow = match->GetBall()->Predict(0);
      const int tick = pes::PressTargetTick(press);
      const Vector3 sample = match->GetBall()->Predict(
          static_cast<unsigned int>(tick * Match::kTick_ms));
      // ponytail: his base (AI+0x1414) is GF's adapted formation position.
      const Vector3 base = team->GetController()->GetAdaptedFormationPosition(CastPlayer(), true);
      const bool toBall = pes::PressTargetIsBall({sample.coords[0], sample.coords[1]},
                                                 {ballNow.coords[0], ballNow.coords[1]},
                                                 {base.coords[0], base.coords[1]});
      const Vector3 target = toBall ? ballNow.Get2D() : base.Get2D();
      pressTarget = target;
      const Vector3 toTarget = target - CastPlayer()->GetPosition();
      // m/s to km/h is arithmetic (3600/1000), not a PES number.
      const float speedKmh = CastPlayer()->GetMovement().GetLength() * 3.6f;
      inputDirection = toTarget.Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
      inputVelocityFloat = pes::PressCommandSpeed_ms(press, toTarget.GetLength(), speedKmh,
                                                     distanceToVelocityMultiplier);
    }

    inputVelocityFloat = RangeVelocity(inputVelocityFloat);  // emulate controller quantization

    // PES's PASS_GET receiver runs ActionPassGet's approach (BallFollow): to
    // the ball path's nearest point, clamped onto the pitch. GF's magnet
    // approach is for everyone else.
    if (team->PassGetReceiver() == player && !hasPossession)
      _PassGetApproach(commandQueue);
    else
      _MovementCommand(commandQueue, forceMagnet, extraHaste, pressTarget);

  } else {  // keeper?

    PlayerCommand command;
    command.desiredFunctionType = e_FunctionType_Movement;
    command.useDesiredMovement = true;
    command.desiredDirection = manualMovementDirection;
    assert(command.desiredDirection.coords[2] == 0.0f);
    command.desiredVelocityFloat = manualMovementVelocityFloat;
    command.useDesiredLookAt = true;
    command.desiredLookAt =
        player->GetPosition() +
        (_mentalImage->GetBallPrediction(40).Get2D() - player->GetPosition()).GetNormalized(0) *
            10.0f;
    commandQueue.push_back(command);
  }
}

void ElizaController::Process() {
  // The carrier's seed member ticks on every logic frame, for every active player, before this
  // player's requests (PES's driver 0x142110c90). ponytail: PES's predicate 0x14210e200 is not
  // ported; GF's stand-in is the designated possession man in play. GetOnTheBallCommands runs
  // only under this predicate (the dribble branch and the keeper's chase both sit inside it), so
  // every roll reads a copied snapshot. PES's false-frame reset (0x14210e5f0) also clears the
  // image manager (0x14211bf60) and Intention (0x1415595d0); GF clears the image only at Reset().
  const bool carrierTurn = match->IsInPlay() && !match->IsInSetPiece() &&
                           match->GetDesignatedPossessionPlayer() == player;
  pes::CarrierSeedTick(carrierSeed, carrierTurn, match->GetRandomInfo().unit);
  PlayerController::Process();
}

// --- PES 2021's engaging-defender decisions (docs/re/pes-re-defender-decisions.md) ---

namespace {

// The angle of a direction vector, in degrees. GF's pitch is (x length, y
// width); PES's is (x length, z width), so this engine's y is PES's z.
inline float HeadingDeg(const Vector3& direction) {
  return std::atan2(direction.coords[1], direction.coords[0]) * 180.0f / blunted::pi;
}

inline float BearingDeg(const Vector3& from, const Vector3& to) {
  return std::atan2(to.coords[1] - from.coords[1], to.coords[0] - from.coords[0]) * 180.0f /
         blunted::pi;
}

inline float AngleBetweenDeg(float a, float b) { return std::fabs(WrapDeg(a - b)); }

}  // namespace

// PES 2021's anime action class (the table `0x1434a5470`, 0 NOTHING,
// 2 MOVE, 3 STEP_MOVE, 4 DRIBBLE, 5/6 DRIBBLE_SIDE_BACK/STEP_MOVE, 7 TRAP,
// 8/9/10 LONG_PASS/SHORT_PASS/SHOOT, 11/12 SLIDING(_KEEP), 13 TACKLE,
// 14 BLOCK, 15 DIVE, 16 STAGGER, 17 FALL_DOWN, 18 FEINT, 19 BALL_LOST,
// 20 OVER_TAKEN, 21 DODGE, 72 JOSTLE) from GF's own function type.
// ponytail: GF's e_FunctionType names 15 kinds where PES names 94 classes,
// so the classes it does not name are approximated by the closest the class
// table carries; the dodge's eligibility only reads three class bits, all of
// which this mapping decides correctly for the kinds GF separates.
int AnimeClassOf(Player* p) {
  switch (p->GetCurrentFunctionType()) {
    case e_FunctionType_None: return 0;      // NOTHING
    case e_FunctionType_Movement: return 2;  // MOVE (STEP_MOVE is 3)
    case e_FunctionType_BallControl: return 4;  // DRIBBLE
    case e_FunctionType_Trap: return 7;          // TRAP
    case e_FunctionType_ShortPass: return 9;     // SHORT_PASS
    case e_FunctionType_LongPass:
    case e_FunctionType_HighPass: return 8;  // LONG_PASS
    case e_FunctionType_Shot: return 10;     // SHOOT
    case e_FunctionType_Header: return 8;    // a header is a pass-class kick
    case e_FunctionType_Deflect: return 14;  // BLOCK
    case e_FunctionType_Catch: return 14;    // keeper handling shares BLOCK's class
    case e_FunctionType_Interfere: return 72;  // JOSTLE
    case e_FunctionType_Trip: {
      // STAGGER is triptype 1, anything else is FALL_DOWN (match.cpp's own
      // reading of the same variable).
      return p->GetCurrentAnim()->anim->GetVariable("triptype") == "1" ? 16 : 17;
    }
    case e_FunctionType_Sliding: return 11;                     // SLIDING
    case e_FunctionType_Special: return 21;                     // DODGE
  }
  return 0;
}

namespace {

// Whether the team is playing one of its deepest mentality rungs, the only GF
// analogue the report leaves open for PES's single `T+0xa298` defensive mode
// byte (report 11 item 5: what that byte counts is unresolved).
// ponytail: one byte stands in for a whole rung, and the mode's +2 is spent on
// the two deepest of GF's five rungs rather than on one exact value.
bool DeepDefensiveMode(const TeamInstructions::State& instructions) {
  return instructions.mentality == TeamInstructions::e_Mentality_AllOutDefence ||
         instructions.mentality == TeamInstructions::e_Mentality_Defensive;
}

// PES's role class of a man: the side's array T(side)+0x4BD4[r%11] (0x14063C9F0 stores it; role-writer doc).
// RoleArrays brings that array to main; until then GF's formation role stands in: class 0 for the keeper,
// 1 for a field man. ponytail: remove when the array lands; this accessor is the one place to change.
int CarrierRoleClass(Player* p) { return p->GetFormationEntry().role == e_PlayerRole_GK ? 0 : 1; }

}  // namespace

Player* ElizaController::_Carrier() {
  // PES's FUN_140a50e00 (docs/re/pes-re-receiving-port.md): the ball's owner
  // when he is an opponent, nobody when my own team has it, and for a loose
  // ball the other team's pass target - each only while the ball is on him.
  const pes::DefenderDecisionConstants& k = match->GetPesDefenderDecisionConstants();
  Player* owner = match->GetBallRetainer();
  if (!owner) {
    Player* last = match->GetPesActorPlayer(pes::NewestEvent(match->GetPesEvents()).actor);
    if (last && last->HasPossession()) owner = last;
  }
  Team* other = match->GetTeam(1 - team->GetID());
  Player* theirTarget = other->GetPassTarget();
  const pes::ChallengeOwner who = !owner ? pes::ChallengeOwner::kNobody
                                  : owner->GetTeamID() == team->GetID() ? pes::ChallengeOwner::kMyTeam
                                                                       : pes::ChallengeOwner::kOpponent;
  Player* candidate = nullptr;
  switch (pes::ChallengeCandidate(who, theirTarget && theirTarget->IsActive())) {
    case pes::ChallengeCandidate::kOwner: candidate = owner; break;
    case pes::ChallengeCandidate::kTheirPassTarget: candidate = theirTarget; break;
    case pes::ChallengeCandidate::kNone: return nullptr;
  }
  const Vector3 ball =
      _mentalImage->GetBallPrediction(static_cast<int>(k.challengeLook_s * 1000.0f)).Get2D();
  pes::ChallengeCheck in;
  in.candX = candidate->GetPosition().coords[0];
  in.candY = candidate->GetPosition().coords[1];
  in.candHeading_deg = HeadingDeg(candidate->GetMovement().Get2D().GetNormalized(candidate->GetDirectionVec()));
  in.candSpeed_mps = candidate->GetMovement().Get2D().GetLength();
  in.ballX = ball.coords[0];
  in.ballY = ball.coords[1];
  in.myX = CastPlayer()->GetPosition().coords[0];
  in.myY = CastPlayer()->GetPosition().coords[1];
  // GF has no PRESS movement state; the team's pressure man is its presser.
  in.pressing = team->GetController()->GetTeamPressurePlayer() == player;
  return pes::ChallengeTargetValid(k, in) ? candidate : nullptr;
}

int ElizaController::_TackleAggressionLevel() {
  // PES's A: ENFORCER (+1) and the team's defensive mode (+1). The team-sheet
  // flag that zeroes it (FUN_140a3b520 / FUN_1414dab90) has no GF counterpart.
  int A = 0;
  if (CastPlayer()->GetPlayerData()->GetPlayingStyle() == PlayingStyles::Player::TheDestroyer) ++A;
  if (DeepDefensiveMode(team->GetController()->GetInstructions())) ++A;
  return A;
}

pes::SlideAggressionInput ElizaController::_SlideAggression() {
  const TeamInstructions::State& instructions = team->GetController()->GetInstructions();
  pes::SlideAggressionInput in;
  in.enforcerStyle =
      CastPlayer()->GetPlayerData()->GetPlayingStyle() == PlayingStyles::Player::TheDestroyer;
  // PES's FUN_140a62180(ctx, side, 6) == 0: the Pressuring sheet toggle is
  // Aggressive rather than Conservative (pes-re-tactics.md 2).
  in.aggressiveTeamSheet = team->GetController()->GetTeamSheet().Toggle(pes::kPressuring) == 0;
  in.deepDefensiveMode = DeepDefensiveMode(instructions);
  in.staminaUnder30 = CastPlayer()->GetStaminaPct() < 0.30f;
  return in;
}

void ElizaController::_PassGetApproach(PlayerCommandQueue& commandQueue) {
  const pes::ReceiveConstants& k = match->GetPesDefenderDecisionConstants().receive;
  // FUN_140a4d340 searches the prediction one PES frame at a time, 0..0xf9.
  constexpr int kPredictionFrames = 0xf9;
  const float frame_ms = pesball::kPesTick_s * 1000.0f;
  std::vector<pes::Point2> path;
  path.reserve(kPredictionFrames);
  for (int f = 0; f < kPredictionFrames; ++f) {
    const Vector3 b = _mentalImage->GetBallPrediction(static_cast<int>(f * frame_ms));
    path.push_back({b.coords[0], b.coords[1]});
  }
  const Vector3 me = CastPlayer()->GetPosition();
  const pes::Point2 p =
      pes::ReceivingPoint(k, path, {me.coords[0], me.coords[1]}, pitchHalfW, pitchHalfH);
  const Vector3 target(p.x, p.y, 0);
  PlayerCommand command;
  command.desiredFunctionType = e_FunctionType_Movement;
  command.useDesiredMovement = true;
  command.desiredDirection = (target - me).Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
  // ponytail: the route's speed choice (BallFollow slots 0x58/0x60, not
  // reduced) - GF's distance-to-velocity ramp stands in.
  command.desiredVelocityFloat = RangeVelocity(
      clamp((target - me).GetLength() * distanceToVelocityMultiplier, 0.0f, sprintVelocity));
  command.useDesiredLookAt = true;
  command.desiredLookAt = _mentalImage->GetBallPrediction(0).Get2D();
  commandQueue.push_back(command);
}

bool ElizaController::_AnyTeammateSliding() {
  std::vector<Player*> mates;
  match->GetTeam(team->GetID())->GetActivePlayers(mates);
  for (Player* mate : mates) {
    if (mate == player) continue;
    if (mate->GetCurrentFunctionType() == e_FunctionType_Sliding) return true;
  }
  return false;
}

bool ElizaController::_IsLastLine() {
  // PES reads the analysis table's line mark. The closest readable analogue is
  // "no team mate of mine is deeper than I am": nobody between me and my own
  // goal line, which is what a last man means on the pitch.
  const float ownX = -team->GetSide() * pitchHalfW;
  std::vector<Player*> mates;
  match->GetTeam(team->GetID())->GetActivePlayers(mates);
  for (Player* mate : mates) {
    if (mate == player) continue;
    const float mine = (player->GetPosition().coords[0] - ownX) * team->GetSide();
    const float his = (mate->GetPosition().coords[0] - ownX) * team->GetSide();
    if (his < mine - 0.01f) return false;
  }
  return true;
}

bool ElizaController::_HasCover(float towardsDeg) {
  // PES's FUN_140a4fea0(ctx, id, &pos, 10, 60): a team mate inside the cover
  // radius whose bearing is inside the cover wedge. `towardsDeg` is the bearing
  // of the thing covered - the carrier.
  const pes::DefenderDecisionConstants& k = match->GetPesDefenderDecisionConstants();
  std::vector<Player*> mates;
  match->GetTeam(team->GetID())->GetActivePlayers(mates);
  for (Player* mate : mates) {
    if (mate == player) continue;
    if ((mate->GetPosition() - player->GetPosition()).GetLength() > k.safetyCoverRadius_m) continue;
    if (AngleBetweenDeg(BearingDeg(player->GetPosition(), mate->GetPosition()), towardsDeg) <=
        k.safetyCoverAngle_deg)
      return true;
  }
  return false;
}

void ElizaController::_PushChallenge(PlayerCommandQueue& commandQueue, const Vector3& contact,
                                     e_FunctionType type) {
  PlayerCommand command;
  command.desiredFunctionType = type;
  command.useDesiredMovement = true;
  Vector3 flat = contact - CastPlayer()->GetPosition();
  if (type == e_FunctionType_Sliding) {
    // PES's slide capsule runs from where he stands towards the point, so the
    // body has to be aimed along the way there and not at the endpoint alone.
    command.desiredDirection = flat.Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
  } else {
    // 2D for every contact aimed direction: the height is carried by the velocity.
    command.desiredDirection = flat.Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
  }
  command.desiredVelocityFloat = sprintVelocity;
  command.useDesiredLookAt = true;
  command.desiredLookAt = CastPlayer()->GetPosition() + command.desiredDirection * 10.0f;
  commandQueue.push_back(command);
}

void ElizaController::_ReactionCommands(PlayerCommandQueue& commandQueue) {
  const pes::DefenderDecisionConstants& k = match->GetPesDefenderDecisionConstants();

  // The walk. PES rebuilds the per-player think context with the current frame
  // every tick, so this runs every tick; GF's RequestCommand is one tick per
  // player already.
  pes::DefenderReactionInput in;
  in.receive = _ReceiveCandidate();
  in.tackle = _TackleCandidate(k);
  in.slide = _SlideCandidate(k);
  in.block = _BlockCandidate(k);
  // Entry 11, the pass jostle (R2 reads the mark inside _PassDuelCandidate). Batch K's R3 wiring dropped it from the
  // call site on both paths. PES's R3 (FUN_1409AD830, docs/re/pes-re-mark-readers.md) skips the holder duel only for a
  // marked man, and ChooseMarkedReaction clears jostleForPass for him; an unmarked man evaluates it in both modes.
  in.jostleForPass = _PassDuelCandidate(k);
  // Each dodge predicate (entries 4-7) carries its own kind
  // (FUN_14073bc70's cmd+0x1094). They are separate entries, so the chooser's
  // winner names which kind the command runs; each predicate stores its own.
  int dodgeKindBall = 0, dodgeKindNet = 0, dodgeKindPost = 0, dodgeKindHuman = 0;
  in.dodgeBall = _DodgeBallCandidate(&dodgeKindBall);
  in.dodgeNet = _DodgeNetCandidate(&dodgeKindNet);
  in.dodgePost = _DodgePostCandidate(&dodgeKindPost);
  in.dodgeHuman = _DodgeHumanCandidate(&dodgeKindHuman);
  // Where the ball will be when he gets there is where PES's solver puts the
  // contact point, and it is the direction any of these commands runs at.
  const Vector3 contact = _mentalImage->GetBallPrediction(200).Get2D();
  // R3 (FUN_1409AD830): a marked man skips the holder duel and falls through
  // to the action dispatch. The mark bit is read on the own-side table
  // (IsMarkedByTable); the PES path only, like the R1/R2 arms.
  const bool marked = match->PesLoop() && team->GetController()->IsMarkedByTable(CastPlayer());
  const pes::DefenderReaction chosen = pes::ChooseMarkedReaction(in, marked);
  int dodgeKind = 0;
  switch (chosen) {
    case pes::DefenderReaction::kDodgeBall: dodgeKind = dodgeKindBall; break;
    case pes::DefenderReaction::kDodgeNet: dodgeKind = dodgeKindNet; break;
    case pes::DefenderReaction::kDodgePost: dodgeKind = dodgeKindPost; break;
    case pes::DefenderReaction::kDodgeHuman: dodgeKind = dodgeKindHuman; break;
    default: break;
  }
  switch (chosen) {
    case pes::DefenderReaction::kReceive:
      _TrapCommand(commandQueue);
      break;
    case pes::DefenderReaction::kTackle:
      CastPlayer()->SetPesChallenge(pes::ContactAction::kTackle);
      _TackleCommand(commandQueue, contact);
      break;
    case pes::DefenderReaction::kSlide:
      _PushChallenge(commandQueue, contact, e_FunctionType_Sliding);
      break;
    case pes::DefenderReaction::kBlock:
    case pes::DefenderReaction::kJostleForPass:
      // Both play GF's interpose clips. PES issues a BLOCK anime (0xe) and a
      // JOSTLE anime (0x48) respectively; the player carries which one, so the
      // contact reaction runs PES's BLOCK / JOSTLE rules on him.
      CastPlayer()->SetPesChallenge(chosen == pes::DefenderReaction::kBlock
                                        ? pes::ContactAction::kBlock
                                        : pes::ContactAction::kJostle);
      _PushChallenge(commandQueue, contact, e_FunctionType_Interfere);
      break;
    case pes::DefenderReaction::kDodgeBall:
    case pes::DefenderReaction::kDodgeNet:
    case pes::DefenderReaction::kDodgePost:
    case pes::DefenderReaction::kDodgeHuman:
      _DodgeCommand(commandQueue, chosen, dodgeKind);
      break;
    case pes::DefenderReaction::kNone:
      break;
  }
}

bool ElizaController::_TackleCandidate(const pes::DefenderDecisionConstants& k) {
  Player* carrier = _Carrier();
  if (!carrier || carrier->GetTeamID() == team->GetID()) return false;
  if (match->PesLoop() && team->GetController()->IsMarkedByTable(CastPlayer())) return false;

  // The anime layer has to be free of a committed challenge, and he may not
  // tackle in the first second after his own touch (PES's event-by-me gate).
  const e_FunctionType current = CastPlayer()->GetCurrentFunctionType();
  if (current == e_FunctionType_Sliding || current == e_FunctionType_Trip) return false;
  if (match->GetLastTouchPlayer() == CastPlayer() &&
      match->GetActualTime_ms() - CastPlayer()->GetLastTouchTime_ms() < 1000)
    return false;

  const Vector3 pos = CastPlayer()->GetPosition();
  const Vector3 carrierPos = carrier->GetPosition();
  const Vector3 ballAhead = _mentalImage->GetBallPrediction(200);
  const Vector3 ballNext = _mentalImage->GetBallPrediction(210);
  const float heading = HeadingDeg(CastPlayer()->GetDirectionVec());
  const float carrierHeading = HeadingDeg(carrier->GetDirectionVec());

  // Step 5's ball gate, on the strict branch. PES applies it to a defender
  // outside {FREE_RUN, WAIT_TIMER, DELAY, PRESS, SAND}; GF has no per-player
  // movement state, so every defender takes the strict branch (which is what
  // the live sample's only tackle was, report 12.1).
  if (!pes::StandingTackleBallGate(k, (ballAhead.Get2D() - pos).GetLength(),
                                   AngleBetweenDeg(heading, BearingDeg(pos, ballAhead.Get2D()))))
    return false;
  // The contact point's height. GF has no solver, so the ball's own height at
  // the look-ahead point stands in (report 12.3: it is a height, not a reach).
  if (ballAhead.coords[2] > k.tackleContactHeight_m) return false;

  const float carrierBearing = BearingDeg(carrierPos, pos);
  if (!pes::StandingTackleFromBehindAllowed(
          k, AngleBetweenDeg(carrierHeading, carrierBearing) > k.tackleBehindAngle_deg,
          AngleBetweenDeg(carrierBearing, carrierHeading),
          AngleBetweenDeg(carrierHeading, heading), _TackleAggressionLevel()))
    return false;
  if (!pes::StandingTackleEngageWindow(k, AngleBetweenDeg(heading, BearingDeg(pos, carrierPos)),
                                       (ballNext - carrierPos).GetLength()))
    return false;

  // The two safety terms. The carrier's running touch clip gives the sine;
  // without one PES's helper returns 1.0, which is above any threshold.
  float sf = 1.0f;
  if (carrier->TouchAnim() && carrier->GetTouchFrame() > 0)
    sf = pes::SafetyProgressSine(static_cast<float>(carrier->GetFrameNum()) /
                                 static_cast<float>(carrier->GetTouchFrame()));
  if (pes::StandingTackleSafetyVeto(k, sf, !_HasCover(BearingDeg(pos, carrierPos)), _IsLastLine(),
                                    static_cast<float>((match->GetActualTime_ms() -
                                                        carrier->GetLastTouchTime_ms()) /
                                                       Match::kTick_ms)))
    return false;

  // No double tackle: a team mate in a ball action and 1.5x closer to the point
  // (report 2, FUN_140970ad1).
  std::vector<Player*> mates;
  match->GetTeam(team->GetID())->GetActivePlayers(mates);
  const float mine = (ballAhead.Get2D() - pos).GetLength();
  for (Player* mate : mates) {
    if (mate == player) continue;
    if ((ballAhead.Get2D() - mate->GetPosition()).GetLength() * 1.5f > mine) continue;
    const e_FunctionType mateAction = mate->GetCurrentFunctionType();
    if (mateAction == e_FunctionType_Sliding || mateAction == e_FunctionType_Trap ||
        mateAction == e_FunctionType_Interfere || mateAction == e_FunctionType_Header)
      return false;
  }
  return true;
}

bool ElizaController::_SlideCandidate(const pes::DefenderDecisionConstants& k) {
  // CPU team only and open play only: GF is AI-vs-AI by construction, and the
  // whole block sits inside match->IsInPlay() && !match->IsInSetPiece().
  if (_AnyTeammateSliding()) return false;
  if (match->GetBallRetainer()) return false;

  // PES resolves the man he is going for before any geometry runs
  // (FUN_140a50e00), and every gate afterwards is against that player: the
  // slide is a challenge on an opponent, not a loose ball he happens to be
  // near. Without this a defender slides at any ball rolling past him with
  // nobody there.
  Player* target = _Carrier();
  if (!target || target->GetTeamID() == team->GetID()) return false;

  // The contact point, ten frames ahead, must stay below the height limit.
  const Vector3 ballAhead = _mentalImage->GetBallPrediction(100);
  if (ballAhead.coords[2] > k.slideBallHeight_m) return false;

  const bool deepMode = DeepDefensiveMode(team->GetController()->GetInstructions());
  int L = pes::SlideAggressionLevel(_SlideAggression());
  // Inside his own penalty area he may only slide at L = 3, and PES then
  // lowers L. The box is the keeper's box this engine uses everywhere.
  const float ownX = -team->GetSide() * pitchHalfW;
  const bool inOwnBox =
      std::fabs(CastPlayer()->GetPosition().coords[1]) < 20.05f &&
      (CastPlayer()->GetPosition().coords[0] - ownX) * team->GetSide() < 16.4f;
  L = pes::SlideAggressionOwnBox(k, inOwnBox, deepMode, L);
  if (L < 0) return false;

  // ponytail: PES also asks that he be his team's first man to the point, that
  // no opponent body sits in the capsule, and that the slide be lateral enough
  // to his heading. GF has no reach-and-timing solver and no capsule geometry,
  // so the reach below is the whole distance gate.
  const float reach = pes::SlideReach_m(k, L);
  if ((ballAhead.Get2D() - CastPlayer()->GetPosition()).GetLength() >= reach) return false;
  // The direction and engagement windows the raw applies after the reach
  // (FUN_140971ba0 @0x14097230c and @0x140972377): he may not slide sideways,
  // and the point he slides at has to be inside the window off his heading -
  // the same window the standing tackle uses. Without these a defender aimed
  // at a ball 45 deg to his side, or behind him, still slides at it.
  const float heading = HeadingDeg(CastPlayer()->GetDirectionVec());
  const float slideBearing = BearingDeg(CastPlayer()->GetPosition(), ballAhead.Get2D());
  if (!pes::SlideDirectionWindow(k, slideBearing, heading)) return false;
  if (!pes::SlideEngageWindow(k, slideBearing, heading)) return false;
  // Ponytail: the anime layer decides whether he is still recovering from a
  // slide; the engine's own current animation is the nearest readable gate.
  const e_FunctionType current = CastPlayer()->GetCurrentFunctionType();
  return current != e_FunctionType_Sliding && current != e_FunctionType_Trip;
}

bool ElizaController::_BlockCandidate(const pes::DefenderDecisionConstants& k) {
  // Entry 8's owner half: an opponent owner inside the block radius. The entry gates come first; R2's mark arm
  // (FUN_1409931C0) is an OR-leg of the verdict behind them.
  if (match->GetBallRetainer() == 0) return false;
  Player* carrier = _Carrier();
  if (!carrier || carrier->GetTeamID() == team->GetID()) return false;
  if (match->GetBallRetainer() != carrier) return false;
  // PES's PRESS state: the MATCH_UP man while ForeCheckPress runs.
  const bool pressing = team->GetController()->IsPressing(CastPlayer());
  const float radius = pes::BlockRadius_m(k, pressing, false, false);
  const bool inRadius = (carrier->GetPosition() - CastPlayer()->GetPosition()).GetLength() <= radius;
  // A pass or a shot clip widens the radius to 7 m once it has passed its
  // fraction. GF's clip table carries no pass clip, so the widened radius is
  // read through the requirement that the ball is already in flight towards
  // him - the late window - rather than through a clip's own progress.
  const bool marked = match->PesLoop() && team->GetController()->IsMarkedByTable(CastPlayer());
  return pes::EntryAdmitted(true, marked, inRadius);
}

bool ElizaController::_PassDuelCandidate(const pes::DefenderDecisionConstants& k) {
  // Entry 11's loose half: a kicked ball in flight, unowned, inside the block
  // radius of where it is going. It sits behind entry 8 in the table, so PES
  // reaches it only when the owner's own radius did not catch him.
  if (match->GetBallRetainer()) return false;
  Player* kicker = match->GetLastTouchPlayer();
  if (!kicker) return false;
  const float speed = match->GetBall()->GetMovement().GetLength() * 3.6f;
  const float window = pes::BlockLooseWindow_s(k, speed);
  const float age_s =
      static_cast<float>(match->GetActualTime_ms() - kicker->GetLastTouchTime_ms()) * 0.001f;
  if (age_s >= window) return false;

  const bool pressing = team->GetController()->IsPressing(CastPlayer());
  const float radius = pes::BlockRadius_m(k, pressing, false, false);
  const Vector3 ballPredict = _mentalImage->GetBallPrediction(200);
  const bool inRadius = (ballPredict - CastPlayer()->GetPosition()).GetLength() <= radius;
  const bool marked = match->PesLoop() && team->GetController()->IsMarkedByTable(CastPlayer());
  return pes::EntryAdmitted(true, marked, inRadius);
}

// The ball-height band of the ball at a look-ahead time (FUN_1406e69f0), on
// the ball's own predicted position.
static int BallBandAt(Match* match, const pes::DodgeConstants& k, int ms) {
  const float height = match->GetBall()->Predict(ms).coords[2];
  return pes::BallBand(k, height, 1.0f);  // ponytail: body scale is GF's own
}

bool ElizaController::_DodgeBallCandidate(int* kind) {
  // FUN_140a07bf0 (eligibility) + FUN_140a072d0 (the dispatcher) +
  // FUN_140a73bd80 (the selector). The ball must be unowned or owned by an
  // opponent, close, and the player has to clear the eligibility's own gates.
  if (match->GetBallRetainer() == CastPlayer()) return false;
  const Vector3 ballNear = _mentalImage->GetBallPrediction(0);
  const Vector3 ballFar = _mentalImage->GetBallPrediction(200);
  if ((ballNear.Get2D() - CastPlayer()->GetPosition()).GetLength() > 3.0f) return false;
  const pes::DodgeConstants& k = match->GetPesDodgeConstants();
  // FUN_140a07bf0: the eligibility. PES reads the player's own live anime
  // action class (0..93) out of the game's per-player record; GF's engine
  // carries its own action on the player, so the class is derived from it.
  // ponytail: GF's e_FunctionType is a smaller set than PES's 94 anime
  // classes, so the classes it does not name are approximated by the closest
  // one the table carries (see AnimeClassOf below); the table read itself is
  // the install's own, opcode-checked in LoadDodgeConstants.
  pes::BallEligibilityInput eligibility;
  eligibility.roleClass = AnimeClassOf(CastPlayer());
  // He is the team's designated ball owner: PES admits him on that alone
  // (`teamState->0x18->0x2600c == playerId`), which in GF is the designated
  // possession player the match already picks.
  eligibility.designatedOwner = match->GetDesignatedPossessionPlayer() == CastPlayer();
  eligibility.slot = CastPlayer()->GetID() % 11;
  // The ball's owner, in the same half's record set, is the only ball holder
  // the half test admits.
  eligibility.ownsBall = match->GetBallRetainer() != 0;
  Player* retainer = match->GetBallRetainer();
  eligibility.teamIndex = retainer ? (retainer->GetID() >= 11 ? 1 : 0) : eligibility.slot >= 11 ? 1 : 0;
  // The team's own gate: PES's team-possession latch on his team record (T+0x255).
  // FUN_140a07bf0 runs the ball block only while it is set (lines 46-47). The latch is
  // team.cpp's UpdatePesPossession (FUN_1404fe9c0).
  eligibility.perTeamActionAllowed = team->IsPesPossession();
  // FUN_140a62d20's per-player slot is the player's index inside his team's
  // 11-slot record set; the action-list index (0x18 = none) is the number of
  // actions the team has queued for him. GF runs one command per tick, so the
  // slot is always real and the "none" case never arises.
  eligibility.actionSlot = eligibility.slot;
  // He may not be the player the team last acted through: GF's equivalent is
  // his own last touch, which is what the team's reaction would have gone
  // through.
  eligibility.lastActionPlayer = match->GetLastTouchPlayer() == CastPlayer();
  // The last-event window: (rng * 5) / 48 frames since his own last event,
  // rng the game-speed divisor. The counter is in logic frames, one of which is
  // one tick (Milestone A1), so the elapsed ticks are the frames directly.
  const unsigned long now_ms = match->GetSimTime_ms();
  const unsigned long last_ms = CastPlayer()->GetLastTouchTime_ms();
  eligibility.framesSinceEvent =
      last_ms == 0 ? 1000 : static_cast<int>((now_ms - last_ms) / Match::kTick_ms);
  if (!pes::BallDodgeEligible(k, eligibility)) return false;

  const int speedClass = pes::SpeedClass(CastPlayer()->GetMovement().GetLength() * 3.6f, 0.0f);
  std::vector<int> forced = {k.ballGrid[0][0], k.ballGrid[0][1], k.ballGrid[0][2]};
  const int bandNear = BallBandAt(match, k, 0);
  const int bandFar = BallBandAt(match, k, 200);
  (void)bandNear;
  (void)bandFar;
  auto clip = pes::BallSelector(match->GetPesAnimetable(), k, speedClass, forced);
  if (!clip) return false;
  *kind = static_cast<int>(pes::DodgeKind::kBall);
  return true;
}

bool ElizaController::_DodgeNetCandidate(int* kind) {
  // FUN_140a07dd0 (the entry, feature 0x23) + FUN_140a07e40 (the gate) +
  // FUN_14073c5d0 (the selector). The gate reads the dodger's own record and
  // the goal marks only: no ball, and no window on his own goal line.
  const pes::DodgeConstants& k = match->GetPesDodgeConstants();
  const Vector3 pos = CastPlayer()->GetPosition();
  const Vector3 mov = CastPlayer()->GetMovement();
  pes::NetGateInput gate;
  gate.animeSlot = true;  // the command is about to issue a Special
  gate.x_m = pos.coords[0];
  gate.y_m = pos.coords[1];
  gate.moveX_mps = mov.coords[0];
  gate.moveY_mps = mov.coords[1];
  gate.moveZ_mps = mov.coords[2];
  // FieldInfo F[0], F[2] and F[14] (pes-re-fieldinfo.md §1): the goal line's outer
  // edge, the posts' inner half-width, and the net's floor-back x.
  const pes::GoalMarks marks{pesFieldHalfLength, pesGoalInnerHalfWidth, pesNetFloorBackX};
  if (!pes::NetDodgeGate(k, marks, gate)) return false;
  const float speed = mov.GetLength() * 3.6f;
  auto clip = pes::NetSelector(match->GetPesAnimetable(), k, speed > 22.0f);
  if (!clip) return false;
  *kind = static_cast<int>(pes::DodgeKind::kNet);
  return true;
}

bool ElizaController::_DodgePostCandidate(int* kind) {
  // FUN_140a08340 (the predicate) + FUN_140a084d0 (the gate) + FUN_14073c9c0
  // (the selector). The gate reads the dodger's own record and the goal marks,
  // with no window on his own goal line: PES's edge is the goal line on
  // whichever end he stands. The ball enters only the clause PES reads it in.
  const pes::DodgeConstants& k = match->GetPesDodgeConstants();
  const Vector3 pos = CastPlayer()->GetPosition();
  const Vector3 mov = CastPlayer()->GetMovement();
  pes::PostGateInput gate;
  // ponytail: FUN_1406b4c10(ap, 0x15), the anime layer's start test, is not
  // carried; the slot stands open as the net gate's does.
  gate.animeSlot = true;
  gate.keeper = CastPlayer()->GetFormationEntry().role == e_PlayerRole_GK;  // FUN_140a62690
  // ponytail: FUN_140a60300 compares the newest ring event's player; GF's last
  // touch stands in for it.
  gate.newestTouch = match->GetLastTouchPlayer() == CastPlayer();
  // ponytail: T+0x78 is GF's designated possession player; the MA+0x2470c
  // table PES also admits is not carried.
  gate.designatedMan = team->GetDesignatedTeamPossessionPlayer() == CastPlayer();
  gate.x_m = pos.coords[0];
  gate.y_m = pos.coords[1];
  gate.moveX_mps = mov.coords[0];
  gate.moveY_mps = mov.coords[1];
  gate.moveZ_mps = mov.coords[2];
  // FUN_140a57800: the ball now (ring index 0).
  const Vector3 ballNow = _mentalImage->GetBallPrediction(0).Get2D();
  gate.ballX_m = ballNow.coords[0];
  gate.ballY_m = ballNow.coords[1];
  const pes::GoalMarks marks{pesFieldHalfLength, pesGoalInnerHalfWidth, pesNetFloorBackX};
  if (!pes::PostDodgeGate(k, marks, gate)) return false;

  // The selector is unchanged in this commit: its own-post anchor and the
  // 200 ms ball approach are the next item (pes-re-NetDodge.md).
  const Vector3 ball = _mentalImage->GetBallPrediction(200).Get2D();
  const pes::PostAngles angles =
      pes::PostAnglesFor(team->GetSide(), pos.coords[0], pos.coords[1], ball.coords[0], ball.coords[1],
                         pitchHalfW, goalHalfWidth);
  const float speed = mov.GetLength() * 3.6f;
  auto clip = pes::PostSelector(match->GetPesAnimetable(), k, angles.bearing_deg, angles.refAngle_deg,
                                angles.approachRel_deg, speed);
  if (!clip) return false;
  *kind = static_cast<int>(pes::DodgeKind::kPost);
  return true;
}

bool ElizaController::_DodgeHumanCandidate(int* kind) {
  // FUN_140a0b350 (the feature gate) + FUN_140a08ea0 (the opponent scan) +
  // FUN_14073c520 (the selector). An opponent within 3 m whose dodge kind
  // resolves to a clip. The mirror side is the per-player draw (stream 2).
  const pes::DodgeConstants& k = match->GetPesDodgeConstants();
  // FUN_140a0b350's own gate first: PES runs it before the opponent scan and
  // it decides whether this man may dodge a challenger at all.
  // ponytail: GF carries no per-team roster list of the exe's shape and no
  // per-player roster-slot value, so the roster and slot terms read GF's own
  // active-player list and formation slot; the team's own latch (the exe's
  // teamRecord->0x255) is team.cpp's UpdatePesPossession, and the ball-owner
  // test stands in as the ball's retainer. The window reads the newest ball event,
  // the one term GF carries exactly.
  pes::HumanGateInput gate;
  gate.featureOn = true;  // the behaviour is always compiled in
  gate.matchStateOk = true;
  gate.onRoster = true;  // he is one of his team's active players by construction
  std::vector<Player*> mates;
  team->GetActivePlayers(mates);
  for (Player* m : mates)
    if (m == CastPlayer()) {
      gate.onRoster = true;
      break;
    }
  else
    gate.onRoster = false;
  // The state record's own half is his: the record is the one his team's own
  // decides use, so it always matches.
  gate.stateRecordHalfMatches = true;
  // The window reads the match's newest ball event, as FUN_140a0b350 does
  // (decompile lines 64-73): whether it was his team's, and how many frames old it is.
  // ponytail: the store's producers cover kinds 1, 9-14, 0x10 and 30 plus the
  // ball-event family; route kinds 2/3/5/27/28 are not produced yet (A2Events2).
  const pes::NewestEventView newest = pes::NewestEvent(match->GetPesEvents());
  gate.newestEventIsOwnTeam =
      newest.present && newest.actor.Valid() && newest.actor.team == static_cast<uint32_t>(team->GetID());
  gate.newestEventAgeFrames = match->GetPesFrame() - newest.frame;
  gate.teamLatchSet = team->IsPesPossession();
  gate.rosterSlotValue = 0;
  gate.ballOwnerIsSelf = match->GetBallRetainer() == CastPlayer();
  if (!pes::HumanDodgeGate(k, gate)) return false;

  Player* best = nullptr;
  float bestDistSq = k.humanDistSq;
  std::vector<Player*> opps;
  match->GetTeam(1 - team->GetID())->GetActivePlayers(opps);
  for (Player* opp : opps) {
    const Vector3 delta = opp->GetPosition() - CastPlayer()->GetPosition();
    const float d = delta.coords[0] * delta.coords[0] + delta.coords[1] * delta.coords[1];
    if (d < bestDistSq) bestDistSq = d, best = opp;
  }
  if (!best) return false;
  const int humanKind = _HumanDodgeKind(best);
  if (humanKind < 0 || humanKind > 7) return false;
  const int row = pes::KindRow(match->GetPesAnimetable(), match->GetPesMbinfo(), k, humanKind,
                               CastPlayer()->GetMovement().GetLength(), false, 0.0f);
  if (row < 0) return false;
  *kind = humanKind;
  return true;
}

int ElizaController::_HumanDodgeKind(Player* opp) {
  // FUN_14073a3d0. ponytail: the full kind inference reads PES's contact
  // record and the opponent's action classes, which GF does not carry per
  // part. The ported part is the geometry: an opponent closing from behind
  // (kinds 4/5) versus in front. The default kind is the jump family.
  const Vector3 toOpp = opp->GetPosition() - CastPlayer()->GetPosition();
  const float bearing = BearingDeg(CastPlayer()->GetPosition(), opp->GetPosition());
  const float heading = HeadingDeg(CastPlayer()->GetDirectionVec());
  const float off = std::fabs(AngleBetweenDeg(bearing, heading));
  const pes::DodgeConstants& k = match->GetPesDodgeConstants();
  if (off > k.behindAngle_deg) return 4;  // approaching from behind: slide_rapid_up
  if (off < k.bearingAngle_deg) return 1;  // in front: jump
  return 3;                                 // beside: slide
}

void ElizaController::_DodgeCommand(PlayerCommandQueue& commandQueue,
                                    pes::DefenderReaction reaction, int kind) {
  // FUN_14073bc70 + FUN_1406ecbb0: the Special command names the clip by
  // (family, motion). The player also carries the DODGE action so the contact
  // reaction runs PES's dodge rules on him (V5, the committed flag).
  CastPlayer()->SetPesChallenge(pes::ContactAction::kDodge);
  const pes::DodgeConstants& k = match->GetPesDodgeConstants();
  const pes::Animetable& t = match->GetPesAnimetable();
  int motionNo = -1;
  switch (reaction) {
    case pes::DefenderReaction::kDodgeBall: {
      const int speedClass = pes::SpeedClass(CastPlayer()->GetMovement().GetLength() * 3.6f, 0.0f);
      std::vector<int> forced = {k.ballGrid[0][0], k.ballGrid[0][1], k.ballGrid[0][2]};
      auto clip = pes::BallSelector(t, k, speedClass, forced);
      if (clip) motionNo = clip->motionNo;
      break;
    }
    case pes::DefenderReaction::kDodgeNet: {
      const float speed = CastPlayer()->GetMovement().GetLength() * 3.6f;
      auto clip = pes::NetSelector(t, k, speed > 22.0f);
      if (clip) motionNo = clip->motionNo;
      break;
    }
    case pes::DefenderReaction::kDodgePost: {
      // The same own-post geometry the predicate used (FUN_14073c9c0's
      // callsite, so the command's clip and the predicate's agree).
      const Vector3 pos = CastPlayer()->GetPosition();
      if (std::fabs(pos.coords[0] - team->GetSide() * pitchHalfW) > 12.0f) break;
      const Vector3 ball = _mentalImage->GetBallPrediction(200).Get2D();
      const pes::PostAngles angles =
          pes::PostAnglesFor(team->GetSide(), pos.coords[0], pos.coords[1], ball.coords[0],
                             ball.coords[1], pitchHalfW, goalHalfWidth);
      const float speed = CastPlayer()->GetMovement().GetLength() * 3.6f;
      auto clip = pes::PostSelector(t, k, angles.bearing_deg, angles.refAngle_deg,
                                    angles.approachRel_deg, speed);
      if (clip) motionNo = clip->motionNo;
      break;
    }
    case pes::DefenderReaction::kDodgeHuman: {
      const int row = pes::KindRow(t, match->GetPesMbinfo(), k, kind,
                                   CastPlayer()->GetMovement().GetLength(), false, 0.0f);
      if (row >= 0 && t.Populated(row, pes::KindColumn(k, kind)))
        motionNo = t.Cell(row, pes::KindColumn(k, kind)).motionNo;
      break;
    }
    default:
      break;
  }
  if (motionNo < 0) return;  // the selector found no clip: PES declines too
  // One per dodge the chooser actually commands, so the report can tell the
  // dodges that fire from the ones whose selector finds nothing. PES's own
  // dodge kinds 1..3 select and 4 is accepted without a selector, so the
  // tally keeps them apart by kind rather than lumping them.
  int kindIndex = -1;
  switch (reaction) {
    case pes::DefenderReaction::kDodgeBall: kindIndex = 0; break;
    case pes::DefenderReaction::kDodgeNet: kindIndex = 1; break;
    case pes::DefenderReaction::kDodgePost: kindIndex = 2; break;
    case pes::DefenderReaction::kDodgeHuman: kindIndex = 3; break;
    default: break;
  }
  if (kindIndex >= 0) match->GetMatchData()->AddDodge(team->GetID(), kindIndex);
  PlayerCommand command;
  command.desiredFunctionType = e_FunctionType_Special;
  command.useSpecialVar1 = true;
  command.specialVar1 = static_cast<int>(pes::PesClipFamily::kDodge);
  command.useSpecialVar2 = true;
  command.specialVar2 = motionNo;
  command.useDesiredMovement = true;
  command.desiredDirection = CastPlayer()->GetDirectionVec();
  command.desiredVelocityFloat = CastPlayer()->GetMovement().GetLength();
  commandQueue.push_back(command);
}

void ElizaController::_TackleCommand(PlayerCommandQueue& commandQueue, const Vector3& contact) {
  // The sub-kind pick, then the Special command naming its clip. The state it
  // reads is what GF carries; pestackle's header names each stand-in.
  const pes::TackleConstants& tk = match->GetPesTackleConstants();
  const pes::Animetable& t = match->GetPesAnimetable();
  Player* carrier = _Carrier();
  pes::TackleSituation s;
  s.speedClass = pes::SpeedClass(CastPlayer()->GetMovement().GetLength() * 3.6f, 0.0f);
  s.mayPlay = true;
  if (carrier != nullptr && carrier->GetTeamID() != team->GetID()) {
    // FUN_140477130's opposed-body distance is a per-body measure; the nearest
    // opponent's distance is GF's own stand-in.
    s.opposedBody_m = (carrier->GetPosition() - CastPlayer()->GetPosition()).GetLength();
    s.pairDistSq_m = s.opposedBody_m * s.opposedBody_m;
    s.bearingToCarrier_deg = AngleBetweenDeg(
        HeadingDeg(CastPlayer()->GetDirectionVec()),
        BearingDeg(CastPlayer()->GetPosition(), carrier->GetPosition()));
    s.ballHeight_m = _mentalImage->GetBallPrediction(0).coords[2];
  }
  s.secondMan = false;  // FUN_14078f4f0's third flag: GF has no per-tick tackle clock
  s.touching = s.opposedBody_m >= 0.0f && s.opposedBody_m < 2.0f;
  // think+0x60's "another tackle already committed", read as a team mate in an
  // Interfere clip aimed at this carrier.
  std::vector<Player*> mates;
  match->GetTeam(team->GetID())->GetActivePlayers(mates);
  for (Player* mate : mates) {
    if (mate == CastPlayer()) continue;
    if (mate->GetCurrentFunctionType() != e_FunctionType_Interfere) continue;
    if (mate->GetPesChallenge() == pes::ContactAction::kTackle) s.otherTackleCommitted = true;
  }
  const pes::TackleChoice choice = pes::ChooseTackle(tk, t, s);
  if (choice.motionNo < 0) {
    // No kind's row resolves in the install's own table: PES declines the
    // tackle's clip too, and GF's interfere clip stands.
    _PushChallenge(commandQueue, contact, e_FunctionType_Interfere);
    return;
  }
  CastPlayer()->SetPesTackleSubKind(int(choice.kind));
  match->GetMatchData()->AddTackleSubKind(team->GetID(), int(choice.kind));
  PlayerCommand command;
  command.desiredFunctionType = e_FunctionType_Special;
  command.useSpecialVar1 = true;
  command.specialVar1 = static_cast<int>(pes::PesClipFamily::kTackle);
  command.useSpecialVar2 = true;
  command.specialVar2 = choice.motionNo;
  command.useDesiredMovement = true;
  command.desiredDirection = CastPlayer()->GetDirectionVec();
  command.desiredVelocityFloat = CastPlayer()->GetMovement().GetLength();
  commandQueue.push_back(command);
}

Vector3 ElizaController::GetDirection() {
  return lastDesiredDirection;
}

float ElizaController::GetFloatVelocity() {
  return lastDesiredVelocity;
}

void ElizaController::LoadStrategies() {
  defenseStrategy = new DefaultDefenseStrategy(this);
  midfieldStrategy = new DefaultMidfieldStrategy(this);
  offenseStrategy = new DefaultOffenseStrategy(this);
  goalieStrategy = new GoalieDefaultStrategy(this);
}

bool PreRatingSortFunc(const PreRating& a, const PreRating& b) {
  return a.totalRating > b.totalRating;
}

bool SortPlayersDeepestFirst(Player* a, Player* b) {
  return a->GetPosition().coords[0] * a->GetTeam()->GetSide() <
         b->GetPosition().coords[0] * b->GetTeam()->GetSide();
}

float ElizaController::GetLazyVelocity(float desiredVelocityFloat) {
  // input is unclamped! use large values (less lazy, apparently we are more off-position)
  float adaptedDesiredVelocityFloat = desiredVelocityFloat;
  // don't use full overflow though
  if (adaptedDesiredVelocityFloat > sprintVelocity)
    adaptedDesiredVelocityFloat =
        sprintVelocity + (adaptedDesiredVelocityFloat - sprintVelocity) * 0.1f;

  float startLazinessDistance = 20.0f * (CastPlayer()->GetStaminaPct() * 0.8f + 0.2f);
  float endLazinessDistance = 65.0f * (CastPlayer()->GetStaminaPct() * 0.5f + 0.5f);

  Vector3 oppPos =
      match->GetTeam(abs(team->GetID() - 1))->GetDesignatedTeamPossessionPlayer()->GetPosition();
  float actionDistance = (player->GetPosition() - oppPos).GetLength();
  float teamPossession = clamp(GetFadingTeamPossessionAmount() - 0.5f, 0.0f, 1.0f);
  float mindSet = AI_GetMindSet(CastPlayer()->GetDynamicFormationEntry().role);

  // if mindSet is high (offensive), be lazy when teamPossession is low. and the other way round for
  // defenders. (midfielders are always half-lazy by role this way; pow() them a bit if this is
  // undesired; this will of course wear them out more than backs and strikers)
  float lazinessByRole = mindSet + teamPossession * (1.0f - mindSet * 2.0f);
  float lazinessByPosition =
      NormalizedClamp(actionDistance, startLazinessDistance, endLazinessDistance);

  float lazyFactor = lazinessByPosition * (0.5f + lazinessByRole * 0.5f);
  float resultingVelocityFloat = adaptedDesiredVelocityFloat * (1.0f - lazyFactor);

  bool clampToDribble = false;
  if (desiredVelocityFloat >= dribbleVelocity)
    clampToDribble =
        true;  // no standing still when initially undesired; it would get us further and further
               // away from our desired position *edit: no longer true with the unclamped input
  if (clampToDribble && resultingVelocityFloat < dribbleVelocity)
    resultingVelocityFloat = dribbleVelocity;

  // short term fatigue/work rate shortage ;)
  // does not heed dribble clamp above, as to simulate players having to stop to catch their breath
  float breathLeftFactor =
      1.0f - NormalizedClamp(CastPlayer()->GetAverageVelocity(10), idleVelocity, sprintVelocity);
  float workRate = CastPlayer()->GetStat("mental_workrate");
  breathLeftFactor = std::pow(breathLeftFactor, 0.8f - workRate * 0.2f);
  breathLeftFactor =
      clamp(breathLeftFactor * 1.2f, 0.0f, 1.0f);  // make sure beginning of sprint is full speed
  breathLeftFactor = breathLeftFactor * lazyFactor +
                     1.0f * (1.0f - lazyFactor);  // sometimes, we really need to force it
  resultingVelocityFloat = std::min(resultingVelocityFloat, sprintVelocity * breathLeftFactor);

  return resultingVelocityFloat;
}

Vector3 ElizaController::GetSupportPosition(const MentalImage* mentalImage,
                                            const Vector3& basePosition) {
  float offenseWeight =
      0.8f + (AI_GetMindSet(CastPlayer()->GetDynamicFormationEntry().role)) *
                 clamp((GetFadingTeamPossessionAmount() - 0.5f) * 2.0f, 0.0f, 1.0f);
  float distanceWeight = 1.0f;
  float passWeight = 0.8f;
  float movementWeight = 1.5f;
  float formationWeight =
      1.6f +
      clamp(
          (CastPlayer()->GetPosition() - mentalImage->GetBallPrediction(100).Get2D()).GetLength() /
              30.0f,
          0.0, 1.0f);
  float offsideWeight = 10.0f;

  Vector3 playerPos = player->GetPosition() + player->GetMovement() * 0.1f;  // + physics slowness

  // get closest opponents

  std::vector<Player*> opponents;
  std::vector<PlayerImage> opponentImages;
  AI_GetClosestPlayers(match->GetTeam(abs(team->GetID() - 1)), playerPos, opponents, 4);
  for (unsigned int i = 0; i < opponents.size(); i++) {
    PlayerImage oppImage = mentalImage->GetPlayerImage(opponents.at(i)->GetID());
    oppImage.position += oppImage.movement * 0.15f;
    opponentImages.push_back(oppImage);
  }

  // add candidate positions

  std::vector<Vector3> candidatePositions;
  candidatePositions.push_back(playerPos);
  float step = (2 * pi) / 12.0f;
  Vector3 pos;
  for (float rad = 0; rad < 2 * pi; rad += step) {
    pos.coords[0] = cos(rad);
    pos.coords[1] = sin(rad);
    candidatePositions.push_back(playerPos + pos * dribbleVelocity);
    candidatePositions.push_back(playerPos + pos * walkVelocity);
    candidatePositions.push_back(playerPos + pos * sprintVelocity);
  }

  // rate candidates

  Vector3 goalPos = Vector3(-team->GetSide() * pitchHalfW, playerPos.coords[1] * 0.8f,
                            0);  // y element is because goal is wider than 0
  float goalDistance = fabs((goalPos - playerPos).GetLength() - 5.0f);  // and not all too close
  float ballDistance = (playerPos - mentalImage->GetBallPrediction(100).Get2D()).GetLength();
  float formationDistance = (basePosition - playerPos).GetLength();
  float offside = AI_GetOffsideLine(match, mentalImage, abs(team->GetID() - 1));

  float passRating;  // are opponents blocking pass space to this position?

  float bestRating = 0;
  int bestCandidate = 0;

  std::vector<PreRating> preRatings;

  for (unsigned int i = 0; i < candidatePositions.size(); i++) {
    if (fabs(candidatePositions.at(i).coords[0]) < pitchHalfW &&
        fabs(candidatePositions.at(i).coords[1]) < pitchHalfH) {
      PreRating preRating;
      preRating.candidateID = i;

      // offense rating
      preRating.offenseRating =
          clamp((goalDistance - fabs((goalPos - candidatePositions.at(i)).GetLength() - 5.0)) /
                        sprintVelocity * 0.5 +
                    0.5,
                0.0, 1.0);

      // distance rating
      float candidateBallDistance =
          (candidatePositions.at(i) - mentalImage->GetBallPrediction(100).Get2D()).GetLength();
      float desiredBallDistance = 10.0f;
      // too close is bad as well..
      if (candidateBallDistance < desiredBallDistance)
        candidateBallDistance =
            desiredBallDistance + fabs(candidateBallDistance - desiredBallDistance);
      float candidateBallVsBallDistance = (ballDistance - candidateBallDistance);

      preRating.distanceRating =
          clamp((candidateBallVsBallDistance / sprintVelocity) * 0.5f + 0.5f, 0.0f, 1.0f);

      // movement rating
      preRating.movementRating =
          1.0 - clamp((player->GetMovement() - (candidatePositions.at(i) - playerPos)).GetLength() /
                          (2 * sprintVelocity),
                      0.0, 1.0);

      // formation rating
      preRating.formationRating =
          clamp(((formationDistance - (basePosition - candidatePositions.at(i)).GetLength()) /
                 sprintVelocity) *
                        0.5 +
                    0.5,
                0.0, 1.0);

      // offside rating
      bool isOffside = false;
      if (candidatePositions.at(i).coords[0] * -team->GetSide() >= offside * -team->GetSide())
        isOffside = true;
      preRating.offsideRating = isOffside ? 0.0 : 1.0;

      preRating.totalRating =
          preRating.offenseRating * offenseWeight + preRating.distanceRating * distanceWeight +
          preRating.movementRating * movementWeight + preRating.formationRating * formationWeight +
          preRating.offsideRating * offsideWeight;

      // small bonus for 'current position'
      if (i == 0)
        preRating.totalRating += 0.1f;

      preRatings.push_back(preRating);
    }
  }

  std::sort(preRatings.begin(), preRatings.end(), PreRatingSortFunc);

  for (unsigned int i = 0; i < preRatings.size() / 3; i++) {
    const PreRating& preRating = preRatings.at(i);
    int candidateID = preRating.candidateID;

    // pass rating
    passRating = 1.0;
    Vector3 checkPosition =
        candidatePositions.at(candidateID) +
        (mentalImage->GetBallPrediction(240).Get2D() - candidatePositions.at(candidateID))
                .GetNormalized(0) *
            2.0f;
    for (unsigned int o = 0; o < opponentImages.size(); o++) {
      float oppDist =
          pow(clamp((opponentImages.at(o).position - checkPosition).GetLength() / 8.0f, 0.0f, 1.0f),
              1.2f);  // 0 .. 1 == worst, best
      if (oppDist < passRating)
        passRating = oppDist;
    }

    // total rating
    float totalRating = preRating.totalRating + passRating * passWeight;
    if (totalRating > bestRating) {
      bestRating = totalRating;
      bestCandidate = candidateID;
    }
  }

  Vector3 result = candidatePositions.at(bestCandidate);

  return result;
}

Vector3 ElizaController::GetSupportPosition_ForceField(const MentalImage* mentalImage,
                                                       const Vector3& basePosition) {
  Player* designatedPlayer = team->GetDesignatedTeamPossessionPlayer();

  Vector3 currentPos = player->GetPosition() + CastPlayer()->GetMovement() * 0.1f;  // basePosition;
  Vector3 mainManPos = designatedPlayer->GetPosition() + designatedPlayer->GetMovement() * 0.1f;

  std::vector<ForceSpot> forceField;

  // support position

  float dynamicMindSet = AI_GetMindSet(CastPlayer()->GetDynamicFormationEntry().role);
  float ballDistance = (match->GetBall()->Predict(250).Get2D() - player->GetPosition()).GetLength();
  float ballDistanceX =
      fabs(match->GetBall()->Predict(250).coords[0] - player->GetPosition().coords[0]);

  bool forceNoOffside = true;

  float basePositionWeight = 0.7f;
  float overallWeight = 1.0f;
  float opponentRepelWeight = 0.3f * overallWeight;
  float teammateRepelWeight = 0.4f * overallWeight;
  float ballRepelWeight = 1.0f * overallWeight;
  float flockToPossessionPlayerWeight = 0.45f * overallWeight;

  // Support Range 1..10 as the 0..1 share PES itself uses, (SR - 1) / 9
  // (tactics 4.1). ponytail: it scales GF's support web; PES's own support
  // distance formula belongs to the off-ball port.
  constexpr float kSupportRangeSteps = 9.0f;  // 0x14259d60c
  const float webScale = AITactics::GetSupportWebScale(
      (team->GetController()->GetTeamSheet().Slider(pes::kSliderSupportRange) - 1) / kSupportRangeSteps);
#ifndef NDEBUG
  // [pass-dist] instrumentation: the mean spread of this player's nearest
  // teammates is the width of the passing network he actually lives in.
  {
    std::vector<Player*> nearMates;
    AI_GetClosestPlayers(team, currentPos, nearMates, 5);
    float mateSpreadSum = 0.0f;
    int mateCount = 0;
    for (Player* mate : nearMates) {
      if (mate == CastPlayer() || mate->GetFormationEntry().role == e_PlayerRole_GK) continue;
      mateSpreadSum += (mate->GetPosition() - player->GetPosition()).Get2D().GetLength();
      mateCount++;
    }
    if (mateCount > 0)
      match->GetMatchData()->AddSupportWebSample(team->GetID(), mateSpreadSum / mateCount);
  }
#endif

  switch (CastPlayer()->GetDynamicFormationEntry().role) {
    case e_PlayerRole_CB:
    case e_PlayerRole_LB:
    case e_PlayerRole_RB:
      opponentRepelWeight *= 2.2f;
      break;
    case e_PlayerRole_DM:
      opponentRepelWeight *= 2.0f;
      break;
    case e_PlayerRole_CM:
    case e_PlayerRole_LM:
    case e_PlayerRole_RM:
    case e_PlayerRole_LW:
    case e_PlayerRole_RW:
      opponentRepelWeight *= 1.6f;
      break;
    case e_PlayerRole_AM:
      opponentRepelWeight *= 1.2f;
      break;
    case e_PlayerRole_CF:
    case e_PlayerRole_SS:
      opponentRepelWeight *= 1.0f;
      break;
    default:
      break;
  }

  float offsideX = AI_GetOffsideLine(match, _mentalImage, abs(team->GetID() - 1));

  // actual base position
  {
    ForceSpot spot;
    spot.origin = basePosition;

    // lane version: flanks flow forward toward the ball's far lane. PES's run
    // and designated supporter (pesoffball.hpp) bypass this field entirely.
    {
      float amount = 22.0f;
      float laneY = -signSide(mainManPos.coords[1]) * 8.0f;
      amount *=
          curve(1.0f - NormalizedClamp(fabs(laneY - currentPos.coords[1]), 0.0f, 30.0f), 1.0f);
      float delta = -team->GetSide() * std::pow(dynamicMindSet, 1.5f) * amount;
      spot.origin.coords[0] += delta;
    }

    spot.magnetType = e_MagnetType_Attract;
    spot.decayType = e_DecayType_Constant;
    spot.power = 1.0f * basePositionWeight;
    // the farther away from this position, the more we are attracted to it
    spot.power *=
        0.3f + 0.7f * NormalizedClamp((spot.origin - currentPos).GetLength(), 0.0f, 20.0f);
    forceField.push_back(spot);
  }


  // stay away from opponents
  std::vector<Player*> opponents;
  AI_GetClosestPlayers(match->GetTeam(abs(team->GetID() - 1)),
                       mainManPos * 0.3f + currentPos * 0.7f, opponents, 3);
  for (unsigned int i = 0; i < opponents.size(); i++) {
    const PlayerImage& oppImg = mentalImage->GetPlayerImage(opponents.at(i)->GetID());
    ForceSpot spot;
    Vector3 oppPos = oppImg.position + oppImg.movement * 0.1f;
    spot.origin =
        oppPos + (oppPos - mainManPos).GetNormalized(0) *
                     2.0f;  // anti-magnet behind opponent, because the pass-way must be cleared
    spot.magnetType = e_MagnetType_Repel;
    spot.decayType = e_DecayType_Variable;
    spot.power = 1.0f * opponentRepelWeight;
    spot.scale = 5.0f;
    spot.exp = 0.7f;
    forceField.push_back(spot);
  }

  // stay away from teammates
  if (team->GetFadingTeamPossessionAmount() >= 1.02f) {
    std::vector<Player*> players;
    AI_GetClosestPlayers(team, currentPos, players, 6);
    for (unsigned int i = 0; i < players.size(); i++) {
      if (players.at(i) != CastPlayer()) {
        const PlayerImage& mateImg = mentalImage->GetPlayerImage(players.at(i)->GetID());
        ForceSpot spot;
        spot.origin = mateImg.position + mateImg.movement * 0.1f;
        spot.magnetType = e_MagnetType_Repel;
        spot.decayType = e_DecayType_Variable;
        spot.power = 1.0f * teammateRepelWeight;
        spot.scale = 14.0f * webScale;
        spot.exp = 1.0f;
        forceField.push_back(spot);
      }
    }
  }

  // stay away from ball (to not get in the way of passes and possessionplayer)
  if (CastPlayer() != designatedPlayer && team->GetFadingTeamPossessionAmount() >= 1.06f) {
    ForceSpot spot;
    spot.magnetType = e_MagnetType_Repel;
    spot.decayType = e_DecayType_Variable;
    spot.power = 1.0f * ballRepelWeight;
    spot.scale = 2.0f;
    spot.exp = 0.5f;
    spot.origin = mentalImage->GetBallPrediction(200).Get2D();
    forceField.push_back(spot);
    spot.origin = mentalImage->GetBallPrediction(350).Get2D();
    forceField.push_back(spot);
    spot.origin = mentalImage->GetBallPrediction(500).Get2D();
    forceField.push_back(spot);
    spot.origin = mentalImage->GetBallPrediction(650).Get2D();
    forceField.push_back(spot);
  }

  if (CastPlayer() != designatedPlayer) {
    // attract to teammate in possession
    {
      ForceSpot spot;
      spot.origin = mainManPos;
      spot.magnetType = e_MagnetType_Attract;
      spot.decayType = e_DecayType_Variable;
      spot.power = 1.0f * flockToPossessionPlayerWeight;
      spot.scale = 28.0f * webScale;
      spot.exp = 1.0f;
      forceField.push_back(spot);
    }

    // ..yet not too close
    {
      ForceSpot spot;
      spot.origin = mainManPos;
      spot.magnetType = e_MagnetType_Repel;
      spot.decayType = e_DecayType_Variable;
      spot.power = 1.0f * flockToPossessionPlayerWeight;
      spot.scale = 16.0f * webScale;
      spot.exp = 1.0f;
      forceField.push_back(spot);
    }
  }

  Vector3 forceFieldPosition =
      currentPos + AI_GetForceFieldMovement(forceField, currentPos, 7);  // 8);

  float margin = 0.08f;
  if (forceNoOffside)
    if (forceFieldPosition.coords[0] * -team->GetSide() > (offsideX * -team->GetSide()) - margin)
      forceFieldPosition.coords[0] = offsideX - (margin * -team->GetSide());

  forceFieldPosition.coords[0] = clamp(forceFieldPosition.coords[0], -pitchHalfW, pitchHalfW);
  forceFieldPosition.coords[1] = clamp(forceFieldPosition.coords[1], -pitchHalfH, pitchHalfH);

  return forceFieldPosition;
}

void ElizaController::Reset() {
  lastDesiredDirection = Vector3(0);
  lastDesiredVelocity = 0;
  // ponytail: PES clears the image selection itself when the restart state
  // gMatchContext+4460 reads 2 and +4564 is non-zero (0x14211e2d0); GF has no
  // manager-side clear, and its referee buffer takes the place of that state,
  // so the drop happens here, on the situation reset the restarts all run.
  carrierImage = pes::CarrierImageSelection();
  carrierBusyStamp = 0;
  keeperRushing = false;
}

void ElizaController::GetOnTheBallCommands(std::vector<PlayerCommand>& commandQueue,
                                           Vector3& rawInputDirection,
                                           float& rawInputVelocityFloat) {
  float oneTouchIsHard = 0.0f;
  float movementDiff = NormalizedClamp(
      (match->GetBall()->GetMovement() - CastPlayer()->GetMovement()).GetLength(), 0.0f, 10.0f);
  oneTouchIsHard =
      movementDiff - CastPlayer()->GetStat("technical_shortpass") * movementDiff * 0.8f;

  // Skills, playing style and philosophy shape how this player uses the ball.
  const PlayerSkills::Mask skills = CastPlayer()->GetPlayerData()->GetSkills();
  const PlayingStyles::ComMask comStyles = CastPlayer()->GetPlayerData()->GetComStyles();
  // The PES path (pes_loop) draws the stumble and the shot's power from the player's own
  // generator (slot 2, report 3); the GF path keeps random().
  const bool pesPath = match->PesLoop();
  const auto drawRange = [&](float lo, float hi) -> float {
    return pesPath ? lo + (hi - lo) * CastPlayer()->GetPesRandom().Unit(2) : random(lo, hi);
  };
  // A one-touch passer does not pay the control penalty on a quick release.
  oneTouchIsHard = PlayerSkills::GetQuickReleaseAccuracyPenalty(
      skills, CastPlayer()->GetPossessionDuration_ms(), oneTouchIsHard);

  std::vector<PlayerImage> opponentPlayerImages;
  _mentalImage->GetTeamPlayerImages(abs(team->GetID() - 1), -1, opponentPlayerImages);
  // A player's nearest opponent, in metres. The PES path reads the run world's sorted list (report
  // 8.5); the GF path keeps its linear scan of the mental image, measured from `at`.
  const auto nearestOppDistance = [&](const Player* p, const Vector3& at) {
    if (pesPath) return team->GetController()->NearestOpponentDistance(p);
    float best = 1000.0f;
    for (const PlayerImage& opp : opponentPlayerImages) {
      best = std::min(best, (opp.position - at).GetLength());
    }
    return best;
  };

  // Panic under pressure: count opponents closing in and let temperament decide
  // whether the player loses his composure (proposal 3B).
  {
    int pressuringOpponents = 0;
    for (const PlayerImage& oppImage : opponentPlayerImages) {
      if ((oppImage.position - CastPlayer()->GetPosition()).GetLength() < 3.5f)
        pressuringOpponents++;
    }

    // A captain on the pitch steadies everybody; fighting spirit steadies himself.
    bool captainOnPitch = false;
    std::vector<Player*> mates;
    team->GetActivePlayers(mates);
    for (Player* mate : mates) {
      if (PlayerSkills::Has(mate->GetPlayerData()->GetSkills(), PlayerSkills::Skill::Captaincy))
        captainOnPitch = true;
    }
    const float stumbleChance =
        MatchPressure::GetStumbleChance(CastPlayer()->GetStat("mental_calmness"),
                                        CastPlayer()->GetPlayerData()->GetAge(),
                                        pressuringOpponents) *
        PlayerSkills::GetStumbleChanceMultiplier(skills, captainOnPitch);
    if (MatchPressure::ShouldStumble(stumbleChance, drawRange(0.0f, 1.0f))) {
      _AddPanicPass(commandQueue, opponentPlayerImages);
      if (Verbose())
        printf("panic under pressure! chance %f, %i opponents\n", stumbleChance,
               pressuringOpponents);
    }
  }

  // panic
  float mindSet = AI_GetMindSet(CastPlayer()->GetDynamicFormationEntry().role);
  if (mindSet < 0.25f) {
    float panicProneness = 1.0f - mindSet * 2.0f;
    float goalCloseness =
        1.0f - NormalizedClamp((CastPlayer()->GetPosition() -
                                Vector3(pitchHalfW * CastPlayer()->GetTeam()->GetSide(), 0, 0))
                                   .GetLength(),
                               2.0f, 16.0f);  // 8.0f, 32.0f);
    if (CastPlayer()->GetDynamicFormationEntry().role != e_PlayerRole_GK) {
      // No pass found and no sight of goal is what a panic pass is for; the
      // possessionAmount term still keeps the carrier in control.
      if (possessionAmount < 0.9f + panicProneness * goalCloseness * 0.8f) {
        _AddPanicPass(commandQueue, opponentPlayerImages);
        if (Verbose())
          printf("panic! %f, %f, %f\n", possessionAmount, panicProneness, goalCloseness);
      }
    } else {  // keeper
      if (possessionAmount < 3.0f) {
        _AddPanicPass(commandQueue, opponentPlayerImages);
      }
    }
  }


  // PES 2021's ball-carrier decision layer (docs/re/pes-re-carrier-ai.md):
  // the plan the carrier runs, then the shoot decision, then the pass
  // decision, in the order PES adopts them. The cadence is report 2's: the
  // default dribble/keep answer is recomputed every tick, and a full think
  // only runs once the carrier has been free of a committed action for longer
  // than five PES frames (0.1 @0x142588200 + 0.5 @0x142565600 on the 54 Hz
  // game speed, converted to this engine's 10 ms tick).
  const pes::CarrierConstants& carrier = match->GetPesCarrierConstants();
  const int attackDir = -team->GetSide();
  const float attackSign = static_cast<float>(attackDir);
  const Vector3 carrierPos = CastPlayer()->GetPosition();
  const Vector3 ballPos = match->GetBall()->GetPositionBuffer();
  const Vector3 ballMove = match->GetBall()->GetMovement();
  const float ballHeight = ballPos.coords[2];
  const float degreesPerRadian = 180.0f / 3.14159265358979f;
  // PES's logic frame: the carrier's possession is counted in logic frames, which
  // is what one tick now is (Milestone A1), so the possession duration in ms
  // converts by PES's own rate.
  const int nowFrame = static_cast<int>(
      CastPlayer()->GetPossessionDuration_ms() * pes::kPesTickHz / pes::kMilliPerUnit);
  // GF's committed-action equivalent of PES's `state+8 bit0` reset: inside a
  // pass, shot, trap or ball-control action the carrier is committed, so the
  // think timer restarts every such tick (0x140a6e320's property bits 0x30000).
  const e_FunctionType currentAction = CastPlayer()->GetCurrentFunctionType();
  if (currentAction == e_FunctionType_ShortPass || currentAction == e_FunctionType_LongPass ||
      currentAction == e_FunctionType_HighPass || currentAction == e_FunctionType_Shot ||
      currentAction == e_FunctionType_Trap || currentAction == e_FunctionType_BallControl) {
    carrierBusyStamp = nowFrame;
  }
  const bool rethink = pes::RethinkDue(nowFrame, carrierBusyStamp);
  bool shooting = false;

  // The shoot zone, over the ball in PES's frame (this engine's y is PES's z).
  const float finishing = CastPlayer()->GetStat("technical_shot");
  const bool longShotSkill = PlayerSkills::Has(skills, PlayerSkills::Skill::LongRangeShooting) ||
                             PlayerSkills::Has(skills, PlayerSkills::Skill::KnuckleShot) ||
                             PlayerSkills::Has(skills, PlayerSkills::Skill::DippingShots);
  const pes::ShootZone zone = pes::ComputeShootZone(ballPos.coords[0], ballPos.coords[1],
                                                   attackSign, finishing, longShotSkill,
                                                   pesFieldHalfLength, pesGoalInnerHalfWidth, carrier);

  // The Intention ratchet (report 5, 0x1421197b0): one step a tick while he is
  // on the ball. Every gate the image units and the feint read comes from it.
  {
    const Vector3 facingNow = CastPlayer()->GetDirectionVec();
    const float facingNowAngle =
        pes::PesBearingDeg(0.0f, 0.0f, facingNow.coords[0], facingNow.coords[1]);
    pes::IntentionInput intent;
    intent.closeBand = pes::IsClose(zone);  // pred+0x9cc
    // ponytail: the pair PES diffs at a3+0x10 and a3+0x30 is not decoded; the
    // carrier's facing against the direction he is attacking stands in. Both are
    // PES bearings (0 = +z, 90 = +x), the frame T+0x250 is stored in.
    intent.headingDiff_deg = WrapDeg(facingNowAngle - pes::AttackAngleDeg(attackSign));
    // 0x142119915: `MOVSS XMM0,[R12+0x14]; MULSS XMM0,[R14+0x40]` - attackSign * K.x (K = ctx4 =
    // bp+0x78, K+0x40 is the ball x the per-tick copy carries), compared with F[6] at 0x14211991B.
    intent.attackDot = attackSign * ballPos.coords[0];
    intent.penaltyLine_m = pesFieldHalfLength - SetPieceLaws::kAreaDepth;
    // ponytail: a5+0xB58 and a5+0xB3C are the two stored ball leads PES
    // compares, one of which GF does not carry; the ball's distance from him
    // stands in, against a zero limit.
    intent.ballLead_m = (ballPos - carrierPos).GetLength();
    // ponytail: the corridor block (a6+4's guard, the two queries of
    // 0x1421b7d40 and its own gate at 0x142119c9a) has no GF counterpart, so
    // all three read false and the ratchet latches at 2 while he holds the ball.
    CastPlayer()->SetPesIntention(
        pes::NextIntention(CastPlayer()->GetPesIntention(), intent, carrier));
  }
  const int intention = CastPlayer()->GetPesIntention();

  // The carrier's pressure class from the nearest opponent: the report 4.2
  // radii are 4 m in the own third and 2 m in the attacking third, which is
  // what a nearest-opponent distance gives when the class ladder is 0
  // (inside r1), 1 (r2), 2 (r3), 3 (free). The plan block reads it (Long).
  const float pressureRadius = attackDir * carrierPos.coords[0] < -pitchHalfW / 3.0f ? 4.0f : 2.0f;
  const float nearestOpp = nearestOppDistance(CastPlayer(), carrierPos);
  int carrierPressureClass = 3;
  if (nearestOpp < pressureRadius) carrierPressureClass = 0;
  else if (nearestOpp < 2.3f * pressureRadius) carrierPressureClass = 1;
  else if (nearestOpp < 3.0f * pressureRadius) carrierPressureClass = 2;

  if (rethink) {
    // The plan (report 4.4): rolled whenever its own conditions hold, in the
    // dispatcher's priority order kPlanPriorityOrder (9, 8, 2, 3, 1, 4, 5, 6,
    // 7), else Basic. Each plan's roll is its own throw-away generator seeded
    // with int(snapshot * K) on slot 2, the seed member's snapshot (report 3.7), and
    // drawn once through RangeRaw(2, 100) (0x14211a89..0x14211aac2).
    struct PlanChoice {
      int plan;
      float seedK;
      float probability;
      bool eligible;
    };
    const float statBreakthrough = CastPlayer()->GetStat("technical_dribble");
    const bool skillBreakthrough = PlayerSkills::Has(skills, PlayerSkills::Skill::DoubleTouch);
    const bool comBreakthrough =
        PlayingStyles::Has(comStyles, PlayingStyles::Com::SpeedingBullet) ||
        PlayingStyles::Has(comStyles, PlayingStyles::Com::MazingRun) ||
        PlayingStyles::Has(comStyles, PlayingStyles::Com::IncisiveRun);
    const float carrierX = attackSign * carrierPos.coords[0];
    // MiddleShot's start takes the long-shot skills, Rising Shots and COM 5 (report 4.4).
    const bool middleShotSkill = pes::MiddleShotSkill(
        PlayerSkills::Has(skills, PlayerSkills::Skill::LongRangeShooting),
        PlayerSkills::Has(skills, PlayerSkills::Skill::KnuckleShot),
        PlayerSkills::Has(skills, PlayerSkills::Skill::DippingShots),
        PlayerSkills::Has(skills, PlayerSkills::Skill::RisingShots));
    const bool middleShotCom = PlayingStyles::Has(comStyles, PlayingStyles::Com::LongRanger);
    const PlanChoice choices[] = {
        {pes::kPlanMiddleShot, carrier.middleShotSeedK,
         pes::PlanStartProbability(pes::kPlanMiddleShot, carrier, finishing, middleShotSkill,
                                   middleShotCom, intention == 2),
         // MiddleShot needs the mid band (or the far sector past the line), and
         // an Intention below 3 (0x14211b4eb).
         pes::IsMid(zone) && intention < 3},
        {pes::kPlanBreakthrough, carrier.breakthroughSeedK,
         pes::PlanStartProbability(pes::kPlanBreakthrough, carrier, statBreakthrough,
                                   skillBreakthrough, comBreakthrough, false),
         // Breakthrough needs the mid band or a spot past the penalty-area
         // line (report 4.4's start conditions), and an Intention of 3 or more
         // (0x14211a887).
         (pes::IsMid(zone) || carrierX > SetPieceLaws::kAreaDepth) && intention >= 3},
        {pes::kPlanCross, carrier.crossSeedK,
         pes::PlanStartProbability(
             pes::kPlanCross, carrier, finishing,
             // PES's Cross start (0x14211AB10): the Crosser style, the Early
             // Cross COM or Pinpoint Crossing.
             CastPlayer()->GetPlayerData()->GetPlayingStyle() ==
                     PlayingStyles::Player::CrossSpecialist ||
                 PlayerSkills::Has(skills, PlayerSkills::Skill::PinpointCrossing),
             PlayingStyles::Has(comStyles, PlayingStyles::Com::EarlyCross), false),
         // Cross needs the carrier inside the cross zone: the close band.
         pes::IsClose(zone)},
    };
    const auto planChoice = [&](int plan) -> const PlanChoice* {
      for (const PlanChoice& c : choices) {
        if (c.plan == plan) return &c;
      }
      return nullptr;
    };
    // The curve shot (0x14211af20) is its own evaluator: its score is the
    // admission threshold and its own gates are inside the score.
    pes::CurveShotInput curveShot;
    curveShot.intention = intention;
    curveShot.incisiveRun =
        PlayingStyles::Has(comStyles, PlayingStyles::Com::IncisiveRun);
    // The engine's skill bit 7 is LONG_RANGE_DRIVE (docs/re/pes-re-carrier-ai.md
    // section 6.1, the in-match bit order), not the EDIT order's Scotch Move.
    curveShot.skill7 = PlayerSkills::Has(skills, PlayerSkills::Skill::LongRangeDrive);
    curveShot.kickingPower = CastPlayer()->GetStat("physical_shotpower");
    curveShot.finishing = finishing;
    // ponytail: a2[3]+0xB40 (0x14211afb9) and the side check's three scalars
    // (0x14211afd8..0x14211b040) read a PES record GF does not carry; the
    // trajectory is reported clear and the side scalars stay zero, which is the
    // combination that accepts.
    curveShot.trajectory_m = 0.0f;
    curveShot.fullAttack = false;  // ponytail: TeamData+0xB8A2 (47266)

    // The Long plan (0x14211b1d0): the veto and the accepts first, then the score
    // against the roll. Each input GF does not carry is a ponytail below.
    pes::LongInput longPlan;
    longPlan.intention = intention;
    // ponytail: the nearest-opponent pressure class (report 4.2) stands in for
    // sub_14069a3e0's +16, whose pressure scan is not ported.
    longPlan.engagement = carrierPressureClass;
    // ponytail: team+4 == 3 (0x14211b259) and TeamData+47266 (0x14211b279) are
    // not carried by GF; the posture writer 0x1404f6c30 is not ported.
    longPlan.teamModeThree = false;
    longPlan.fullAttack = false;
    // The team's designated possession player is the carrier (0x14211b269, PES's
    // team+0x18). GF's designation is team.cpp's time-to-ball choice, the same
    // one the team AI reads as the carrier.
    longPlan.designatedSelf = team->GetDesignatedTeamPossessionPlayer() == CastPlayer();
    // ponytail: eval+0x3c (0x14211b27f, the cached team setting 0x14063a420) and
    // eval+0x54 (BallPlayer+0x70, the team long instruction, 0x14211b303) are not
    // carried by GF: Path A at level 0 stands in.
    longPlan.teamSetting = false;
    longPlan.level = 0;
    longPlan.comLongBallExpert = PlayingStyles::Has(comStyles, PlayingStyles::Com::LongBallExpert);
    longPlan.skillWeightedPass = PlayerSkills::Has(skills, PlayerSkills::Skill::WeightedPass);
    longPlan.skillLowLoftedPass = PlayerSkills::Has(skills, PlayerSkills::Skill::LowLoftedPass);
    longPlan.lofted = CastPlayer()->GetStat("technical_highpass");

    // The image manager (report 4, 0x14211e2d0): the selected plan's own cancel
    // test runs first, then admission in the priority order and only while
    // nothing is selected, so the selection persists across rethinks.
    const auto cancel = [&](int plan) {
      const PlanChoice* c = planChoice(plan);
      return pes::ImagePlanCancelled(
          plan, intention,
          pes::PlanStillHolds(plan, c != nullptr && c->eligible, curveShot, longPlan, carrier));
    };
    const auto admit = [&](int plan) {
      if (plan == pes::kPlanCurveShot) {
        const int draw = pes::PlanRoll(carrierSeed, carrier.curveShotSeedK,
                                       match->GetPesRngConstants(), carrier);
        return pes::CurveShotAdmitted(curveShot, draw, carrier);
      }
      if (plan == pes::kPlanLong) {
        const int draw = pes::PlanRoll(carrierSeed, carrier.longSeedK,
                                       match->GetPesRngConstants(), carrier);
        return pes::LongAdmitted(longPlan, draw, carrier);
      }
      const PlanChoice* c = planChoice(plan);
      // ponytail: 7, 8 and 9 are not ported, so they are never admitted.
      if (c == nullptr || !c->eligible || c->probability <= 0.0f) return false;
      return pes::PlanRoll(carrierSeed, c->seedK, match->GetPesRngConstants(),
                           carrier) <= static_cast<int>(c->probability);
    };
    const auto image = [&](int plan) {
      // The manager copies the mode its unit published and passes the unit its
      // eval byte +0x6f (the mode-1 rule of 0x14211e0f0); the byte still stands
      // in as false. The heading is not the unit's: PES's writer (FUN_14211E650)
      // runs every frame on the plan record, so it is set below for every mode.
      return pes::PlanImage{pes::PlanImageMode(plan, carrierImage.mode, false, carrierX, carrier),
                            0.0f};
    };
    pes::AdvanceImageSelection(carrierImage, cancel, admit, image);
    // FUN_14211E650 writes plan+0xC0 every frame from the plan's mode and the
    // carrier's own position (x along the attack in PES's frame, z across it).
    carrierImage.heading_deg = pes::PlanHeadingDeg(carrierImage.mode, carrierPos.coords[0],
                                                   carrierPos.coords[1], attackSign, carrier);
  }

  // The shoot decision (report 5.1): the shoot zone, the "just shoot" rule
  // and the box/header rule.
  const Vector3 facing = CastPlayer()->GetDirectionVec();
  const float aimVsFacing = pes::AimVsFacingDeg(attackSign, facing.coords[0], facing.coords[1]);
  Player* keeper = match->GetTeam(abs(team->GetID() - 1))->GetGoalie();
  pes::ShootInput shoot;
  shoot.zone = zone;
  shoot.ballHeight_m = ballHeight;
  shoot.height_m = CastPlayer()->GetStat("physical_height") * 100.0f;
  shoot.owned = CastPlayer()->GetPossessionDuration_ms() > 0;
  shoot.planId = carrierImage.id;
  shoot.aimVsFacing_deg = aimVsFacing;
  shoot.keeperPresent = keeper != nullptr;
  shoot.keeperSettled =
      keeper != nullptr && keeper->GetCurrentFunctionType() == e_FunctionType_Movement;
  const bool shotAnswer = pes::ShootSubKind(shoot, carrier) == 1;
  const auto addShot = [&] {
    PlayerCommand command;
    command.desiredFunctionType = e_FunctionType_Shot;
    command.useDesiredMovement = false;
    command.useDesiredLookAt = false;
    command.desiredVelocityFloat = rawInputVelocityFloat;
    // The aim point: the goal centre, biased to the post the ball's sector
    // favours. ponytail: PES's aim function 0x14211f090 is not decoded (report
    // 9.1), so this engine's own aim stands in for it.
    const float y = zone.sector == pes::kSectorSideLow
                        ? -goalHalfWidth
                        : (zone.sector == pes::kSectorSideHigh ? goalHalfWidth : 0.0f);
    command.touchInfo.desiredDirection =
        (Vector3((pitchHalfW + 1.0f) * -team->GetSide(), y, 0) -
         (carrierPos + CastPlayer()->GetMovement() * 0.2f))
            .GetNormalized(Vector3(-team->GetSide(), 0, 0));
    command.touchInfo.desiredDirection =
        (command.touchInfo.desiredDirection * 0.7f +
         -CastPlayer()->GetDirectionVec() *
             (CastPlayer()->GetFloatVelocity() / sprintVelocity) * 0.3f)
            .GetNormalized();
    command.touchInfo.autoDirectionBias = 1.0f;
    const float goalDist = NormalizedClamp(
        (Vector3(pitchHalfW * -team->GetSide(), 0, 0) - carrierPos).GetLength(), 0.0f, 32.0f);
    command.touchInfo.desiredPower =
        drawRange(0.7f * (0.6f + goalDist * 0.4f), 1.0f * (0.6f + goalDist * 0.4f));
    commandQueue.push_back(command);
  };

  // The pass decision (report 4.3 + 5.2): enumerate the mates, open each
  // lane with the interception formula, and let PassForward pick the tier.
  // Enumerated on every think, gated on the same re-think clock as the plan,
  // so a carrier who has just collected the ball does not re-roll his passing
  // every tick.
  // The GoalFront unit (0x1421265C0) reads PES's per-mate receiver-record family, the same one the
  // pass layer builds, so it reads the records the pass decision leaves here (both run on the same
  // rethink).
  std::vector<pes::PassCandidate> rethinkCandidates;
  const auto passAnswer = [&]() -> pes::PassAnswer {
    // The lanes: 11 frames ahead, the owned-ball value of report 4.3's `F`.
    // One logic frame is one tick (Milestone A1), so a PES frame count is that
    // many ticks.
    constexpr float kLaneFrames = 11.0f;
    const float laneTime_s = kLaneFrames * pesball::kPesTick_s;
    const float lowPass = CastPlayer()->GetStat("technical_shortpass");
    const float carrierSpeed_kmh = CastPlayer()->GetMovement().GetLength() * 3.6f;

    pes::PassForwardInput in;
    in.owned = true;
    in.close = pes::IsClose(zone);
    in.carrierPressureClass = carrierPressureClass;
    in.attackSign = attackSign;
    in.planAngle_deg = 0.0f;  // ponytail: the plan's desired angle (+0xC0) is
                              // not decoded (report 9.3); the attack direction
                              // stands in for it.
    // The Intention ratchet itself (0x1421197b0), not a stand-in: the pass
    // layer's image-state branches read the same member the ratchet writes.
    in.imageState = intention;
    in.halfLen_m = pesFieldHalfLength;
    in.goalHalfWidth_m = pesGoalInnerHalfWidth;
    in.goalAreaHalfWidth_m = SetPieceLaws::kAreaHalfWidth;
    in.mateLaneOk = true;  // ponytail: 0x140a4c8e0 is not decoded.

    std::vector<pes::PassCandidate> candidates;
    std::vector<Player*> mates;
    team->GetActivePlayers(mates);
    // PES 2021's receiver enumeration (0x142117c30, pescarrier.hpp): the pass layer is offered the mates whose
    // status is valid, and no others. The offside line reads the opponents' S1 list (list 1 when attacking +x):
    // its first five entries, a keeper skipped, and the ball, each 16 frames ahead.
    std::vector<Player*> opponents;
    match->GetTeam(1 - team->GetID())->GetActivePlayers(opponents);
    const pes::SortedRow& oppList = team->GetController()->Rows().list[attackSign == 1.0f ? 1 : 0];
    std::vector<pes::OffsideEntry> listed;
    for (int r = 0; r < oppList.Count(1) && r < static_cast<int>(pes::kOffsideEntries); ++r) {
      const int j = oppList.Entry(1, r).id - pes::lane::kSideSize;
      if (j < 0 || j >= static_cast<int>(opponents.size())) continue;
      Player* opp = opponents[j];
      listed.push_back({opp->GetPosition().coords[0], opp->GetMovement().coords[0] * pesball::kPesTick_s,
                        opp->GetFormationEntry().role == e_PlayerRole_GK});
    }
    const float offsideLine = pes::CarrierOffsideLine(
        attackSign, listed, ballPos.coords[0], ballMove.coords[0] * pesball::kPesTick_s,
        pes::ReceiverOffsideFrames(carrier));
    const bool inSetPiece = match->IsInSetPiece();
    const e_SetPiece setPiece = team->GetController()->GetSetPieceType();
    const Player* taker = team->GetController()->GetPieceTaker();
    // The receiver record writer (0x1421174F0) for one target. Record A ("to feet") and record B ("into space") take the
    // same fields, measured from the ball: the forward angle is the bearing against the attack direction, the turn angle
    // the same bearing against the carrier's facing (report 4.3's +0xE4 and +0xE8), and the openness loop measures each
    // opponent in front of the pass against its direction.
    const auto recordFor = [&](const Vector3& tgt) {
      pes::ReceiverInput r;
      const float dist = (tgt - ballPos).GetLength();
      const float dx = tgt.coords[0] - ballPos.coords[0];
      const float dz = tgt.coords[1] - ballPos.coords[1];
      const float bearing = pes::PesBearingDeg(ballPos.coords[0], ballPos.coords[1], tgt.coords[0], tgt.coords[1]);
      const float facingAngle = pes::PesBearingDeg(0.0f, 0.0f, facing.coords[0], facing.coords[1]);
      const float fwdAngle = WrapDeg(bearing - pes::AttackAngleDeg(attackSign));
      const float turnAngle = WrapDeg(bearing - facingAngle);
      // The openness: the minimum over the opponents in front of the pass.
      float bestMargin = 150.0f;
      for (const PlayerImage& opp : opponentPlayerImages) {
        const float oppBearing = pes::PesBearingDeg(ballPos.coords[0], ballPos.coords[1], opp.position.coords[0], opp.position.coords[1]);
        if (std::fabs(WrapDeg(oppBearing - bearing)) > carrier.openInFront_deg)
          continue;
        // The opponent's predicted point along the lane.
        const Vector3 oppPred = opp.position + opp.movement * laneTime_s;
        const float odx = oppPred.coords[0] - ballPos.coords[0];
        const float odz = oppPred.coords[1] - ballPos.coords[1];
        const float laneLen = std::sqrt(dx * dx + dz * dz);
        const float along = (odx * dx + odz * dz) / std::max(laneLen, 0.001f);
        const float perp = std::fabs(odx * dz - odz * dx) / std::max(laneLen, 0.001f);
        bestMargin = std::min(bestMargin, pes::LaneOpennessMargin(dx, dz, along, perp, 1.0f, 0.0f, carrier));
      }
      r.target = {tgt.coords[0], tgt.coords[1]};
      r.passAngle_deg = fwdAngle;
      r.fwdAngle_deg = fwdAngle;
      r.turnAngle_deg = turnAngle;
      r.ballToTarget_m = dist;
      r.difficulty = pes::ReceiverDifficulty(ballMove.GetLength(), dist, 0.0f,
                                             PlayerSkills::Has(skills, PlayerSkills::Skill::OneTouchPass),
                                             carrierSpeed_kmh, false, turnAngle, ballHeight, lowPass, carrier);
      r.openness = bestMargin;
      r.opennessClass = pes::OpennessClassFromMargin(bestMargin, carrier);
      r.dirClass = pes::DirectionClass(fwdAngle, carrier);
      r.turnClass = pes::TurnClass(turnAngle, carrier);
      return r;
    };
    for (Player* mate : mates) {
      pes::ReceiverStatusInput status;
      const Vector3 matePos = mate->GetPosition();
      const Vector3 mateVel = mate->GetMovement();
      status.pos = {matePos.coords[0], matePos.coords[1]};
      status.velPerFrame = {mateVel.coords[0] * pesball::kPesTick_s,
                            mateVel.coords[1] * pesball::kPesTick_s};
      status.anime = AnimeClassOf(mate);
      status.carrier = mate == CastPlayer();
      status.setPieceTaker = inSetPiece && setPiece == e_SetPiece_FreeKick && mate == taker;
      status.offsideTested =
          !(inSetPiece && (setPiece == e_SetPiece_ThrowIn || setPiece == e_SetPiece_GoalKick ||
                           setPiece == e_SetPiece_Corner));
      status.attackSign = attackSign;
      status.offsideLine_m = offsideLine;
      status.halfLen_m = pesFieldHalfLength;
      status.halfWidth_m = pesFieldHalfWidth;
      if (pes::ReceiverStatusOf(status, carrier) != pes::kReceiverValid) continue;
      // The mate's two targets: record A ("to feet") is his position a lane-time ahead; record B ("into space") is the
      // point of carrier-ai 4.3, from his own position. Each takes the record writer above.
      const Vector3 matePred = matePos + mateVel * laneTime_s;
      const Vector3 target3(matePred.coords[0], matePred.coords[1], ballPos.coords[2]);
      const float mateKmh = mateVel.GetLength() * carrier.kmhPerMs;
      const pes::CarrierPoint intoSpace = pes::IntoSpaceTarget(
          {carrierPos.coords[0], carrierPos.coords[1]}, {matePos.coords[0], matePos.coords[1]},
          pes::PesBearingDeg(0.0f, 0.0f, mateVel.coords[0], mateVel.coords[1]), mateKmh,
          nearestOppDistance(mate, matePos), pes::AttackAngleDeg(attackSign), carrier);
      const Vector3 intoSpace3(intoSpace.x, intoSpace.z, ballPos.coords[2]);

      pes::PassCandidate c;
      c.index = mate->GetID();
      c.record = recordFor(target3);
      c.recordB = recordFor(intoSpace3);
      // The mate's own pressure: the nearest opponent to him decides both the
      // short-forward gate and the lofted tier's keeper-distance stand-in.
      const float mateNearest = nearestOppDistance(mate, matePos);
      c.nearestOppToMate_m = mateNearest;
      c.pressureClass = mateNearest < 2.0f ? 0 : (mateNearest < 4.6f ? 1 : (mateNearest < 6.0f ? 2 : 3));
      // ponytail: the pressure entry's distance to opponent slot 0 (INF the
      // keeper) is the nearest opponent's distance; the lofted tier compares a
      // linear distance against a squared one, which never passes on a full
      // pitch, so this stands in for it.
      c.pressureDistOpp0_m = mateNearest * mateNearest;
      candidates.push_back(c);
    }

    rethinkCandidates = candidates;
    return pes::PassForward(candidates, {}, in, carrier);
  };
  // The pass as GF's pass command. The through ball is GF's LongPass on the PES path; the GF path
  // keeps its collapse to ShortPass until the switch is removed.
  const auto addPass = [&](const pes::PassAnswer& pass) {
    if (pass.index < 0) return;
    Player* target = match->GetPlayer(pass.index);
    if (target == nullptr) return;
    // The PES path maps each kind by the translator (0x14210EE00); the GF path keeps its collapse of
    // the through ball to ShortPass until the switch is removed.
    const e_FunctionType passType =
        pesPath ? pes::PesPassFunctionType(pass.kind)
                : (pass.kind == pes::kPassLong ? e_FunctionType_HighPass : e_FunctionType_ShortPass);
    _AddPass(commandQueue, target, passType);
  };

  if (!pesPath) {
    // The GF order: the shot, then the pass on the rethink, then the old dribble search.
    if (shotAnswer) {
      addShot();
      shooting = true;
    }
    if (!shooting && rethink) addPass(passAnswer());
    AI_GetBestDribbleMovement(match, player->GetID(), _mentalImage, rawInputDirection,
                              rawInputVelocityFloat, team->GetTeamData()->GetTactics());
    return;
  }

  // The PES path (pes_loop): MainEval's default is the Dribble answer, recomputed every tick
  // (report 1, step f). The shot is answered every tick and the passes on the rethink; the walk
  // over the unit table (report 5) takes the first survivor, and with none the default stands.
  pes::DribbleInput search;
  search.ball = {carrierPos.coords[0], carrierPos.coords[1]};
  search.halfLen_m = pesFieldHalfLength;
  search.halfWid_m = pesFieldHalfWidth;
  // ponytail: the image-state preference W (0x14212FE70's ladder) is not ported, so the search
  // runs on threat and the touchline band alone.
  search.weight = 0.0f;
  for (const PlayerImage& opp : opponentPlayerImages) {
    pes::DribbleOpponent o;
    o.pos = {opp.position.coords[0], opp.position.coords[1]};
    // ponytail: the predicted point is his position until the look-ahead is ported.
    o.pred = o.pos;
    // ponytail: R_o (the image-state reach, 0x14212E490) is not ported; the reach floor stands in.
    o.reachRo_m = carrier.threatReachFloor_m / carrier.threatReachScale;
    o.keeper = opp.dynamicFormationEntry.role == e_PlayerRole_GK;
    search.opponents.push_back(o);
  }
  const float heading = pes::DribbleDirection(search, carrier) / degreesPerRadian;
  rawInputDirection = Vector3(std::cos(heading), std::sin(heading), 0.0f);
  // ponytail: full dribble speed; GF has no mode 3, mode 4 or feint movement to map the sub-kind to.
  rawInputVelocityFloat = sprintVelocity;

  // The Dribble default's sub-kind (report 2.1). The attack angle is the one the aim test reads:
  // the PES bearing of the attack direction (T+0x250), the same frame as the facing and velocity.
  const float attackAngle = pes::AttackAngleDeg(attackSign);
  const Vector3 move = CastPlayer()->GetMovement();
  pes::DribbleChooserInput chooser;
  chooser.plan = carrierImage.id;
  chooser.planHeading_deg = attackAngle;  // ponytail: the plan's heading (+0xC0) is the attack direction
  chooser.facing_deg = pes::PesBearingDeg(0.0f, 0.0f, facing.coords[0], facing.coords[1]);
  chooser.teamAttack_deg = attackAngle;   // ponytail: team+0x250 is the attack direction
  chooser.forwardOfHalf = attackSign * carrierPos.coords[0] > 0.0f;
  chooser.pressureClass = carrierPressureClass;
  chooser.speed_kmh = move.GetLength() * carrier.kmhPerMs;
  chooser.velZero = move.GetLength() <= 0.0f;
  chooser.velHeading_deg = pes::PesBearingDeg(0.0f, 0.0f, move.coords[0], move.coords[1]);
  chooser.room_m = nearestOpp;            // ponytail: FUN_140a4ddc0's room test is not ported
  chooser.laneFree = carrierPressureClass == 3;  // ponytail: the free-lane loop is not ported
  // ponytail: FUN_141052e30 (team mode, neutral here), the feint gate 0x14212e970, the turning
  // state 0x140a6e300 and box pressure are not decoded to GF state, so they stay off.
  [[maybe_unused]] const int dribbleSub = pes::DribbleSubKind(chooser, false, carrier);

  // The carrier block BP in the window (PES's unit updater 0x142110C90): the unit info BP+0x1C by 0x14063C9F0 and the state
  // BP+0x78 by 0x14063A490 (both translated, equal to PES on the unslimmed walk captures: pes_walk_oracle_test UnitOracle), run
  // on the window's own records (side, team info, man, anime, ball, MA, MI, events). The ball owner (MA+0x2601C) is the input
  // the state's owned byte reads, so it is written first.
  pes::PesWorld& pesWorld = match->PesWorldOf();
  const int sideIndex = team->GetID();
  Player* ballOwner = match->GetBallRetainer();
  pesWorld.SetMaOwner(sideIndex, ballOwner != nullptr ? static_cast<uint32_t>(match->PesSlotOf(ballOwner)) : pes::kMiNoSlot);
  // ponytail: the CPU tier setting (side record +0xA294, written by the menus' 0x140A424E0 callers 0x141FBE480 / 0x141FB9FB0)
  // is the live play setting, tier 5 (docs/re/pes-re-A4Carrier.md 3); GF has no such menu.
  pesWorld.SetSideTier(sideIndex, static_cast<uint32_t>(pes::kBpTierLive));
  pesWorld.RunCarrierUpdate(sideIndex, static_cast<uint32_t>(match->PesSlotOf(CastPlayer())));
  // imageA+0x31C1, the danger flag the carrier units read: PES's unit builder 0x142119390 (translated, proven on ten real
  // calls) stores it from the pressure builder 0x14069A500 and the writer 0x140699960 on the same block. The Long, BackSpace
  // and Safety units read the byte back (ClearDanger), and so does the walk (BP+0x32AD is the same byte).
  pesWorld.RunDangerBuilder(sideIndex);
  pes::PassAnswer pass;
  if (rethink) pass = passAnswer();
  // The Long unit (kind 9, 0x14212AD40) answers on the rethink too: owned or
  // danger with the ball low, target the most advanced mate.
  pes::PassAnswer longPass;
  if (rethink && (carrierImage.id == pes::kPlanLong || carrierImage.id == pes::kPlanBasic)) {
    pes::LongUnitInput longIn;
    longIn.owned = CastPlayer()->GetPossessionDuration_ms() > 0;
    longIn.danger = pesWorld.ClearDanger(sideIndex);  // imageA+0x31C1, written above
    longIn.ballHeight_m = ballHeight;
    longIn.plan = carrierImage.id;
    // ponytail: the plan target (+0xCC) and its validity (0x1421240B0) are
    // not carried by GF; the picker takes the most advanced mate.
    // ponytail: the receiver openness class (table+0xB6C) is computed for
    // PassForward candidates above, not for the Long unit's own scan.
    std::vector<Player*> longMates;
    team->GetActivePlayers(longMates);
    for (Player* mate : longMates) {
      if (mate == CastPlayer()) continue;
      if (mate->GetFormationEntry().role == e_PlayerRole_GK) continue;
      pes::LongMate m;
      m.index = mate->GetID();
      m.advance_m = attackSign * mate->GetPosition().coords[0];
      m.height_cm = mate->GetStat("physical_height") * 100.0f;
      longIn.mates.push_back(m);
    }
    longPass = pes::PassLongUnit(longIn, carrier);
  }
  // The Cross unit (kind 7, 0x142129590) answers on the rethink: the carry
  // bearing held, the lane open, the tallest mate takes the centering ball. It runs with the
  // other lane units, below, because its lane needs their slot space and the ball point.
  pes::PassAnswer crossPass;
  // The GoalFront unit (kind 2; gate 0x14212ABA0, picker 0x1421265C0): outside the central zone,
  // with the lane about the per-slot angle + 180 blocked, the most open mate in front of goal takes
  // a short ball. Its mate records are the receiver records the pass decision just built.
  // A mate's lane slot in the carrier's lane space, numbered as fillSide numbers it: his index in his
  // team's active list, the carrier's team at 0..10 and the opponents at 11..21. GetID() is the match's
  // global allocation (AllocatePlayerID), not a lane slot.
  const auto laneSlotOf = [&](const Player* p) {
    std::vector<Player*> own, opp;
    team->GetActivePlayers(own);
    match->GetTeam(1 - team->GetID())->GetActivePlayers(opp);
    for (int j = 0; j < static_cast<int>(own.size()) && j < pes::lane::kSideSize; ++j)
      if (own[j] == p) return j;
    for (int j = 0; j < static_cast<int>(opp.size()) && j < pes::lane::kSideSize; ++j)
      if (opp[j] == p) return pes::lane::kSideSize + j;
    return pes::lane::kNoSlot;
  };
  pes::PassAnswer goalFrontPass;
  if (rethink) {
    pes::GoalFrontInput gf;
    // The lane core's slot space: 0..10 for the carrier's own team and 11..21 for the opponents,
    // each side in its own squad order. Only the sides and the carrier's own index are read back.
    const auto fillSide = [&](Team* side, int base) {
      std::vector<Player*> squad;
      side->GetActivePlayers(squad);
      for (int j = 0; j < pes::lane::kSideSize && j < static_cast<int>(squad.size()); ++j) {
        Player* p = squad[j];
        if (p == CastPlayer()) gf.carrierSlot = base + j;
        const Vector3 pos = p->GetPosition();
        const Vector3 vel = p->GetMovement();
        pes::lane::LanePlayer& lp = gf.players[base + j];
        lp.pos = {pos.coords[0], pos.coords[1]};
        lp.vel = {vel.coords[0] * pesball::kPesTick_s, vel.coords[1] * pesball::kPesTick_s};
        lp.active = true;  // this list is the active players
        // 0 marks the keeper to the side record the core reads (0x14063CD90).
        lp.positionCategory = p->GetFormationEntry().role == e_PlayerRole_GK ? 0 : -1;
      }
    };
    fillSide(team, 0);
    fillSide(match->GetTeam(1 - team->GetID()), pes::lane::kSideSize);
    const Vector3 carrierGround = CastPlayer()->GetPosition();
    gf.carrierPos = {carrierGround.coords[0], carrierGround.coords[1]};
    // The lane's heading is the per-slot angle table 0x140A71260(team, side)[slot] + 180, wrapped.
    // The core's theta is dir(theta) = (sin, cos) over PES's (x, z), so the facing vector converts
    // directly (this engine's y is PES's z) and the +180 puts the lane behind the carrier.
    // ponytail: the table's writer is not decoded (docs/re/pes-re-lane-structs.md 4); the carrier's
    // own facing stands in for it.
    const Vector3 facingVec = CastPlayer()->GetDirectionVec();
    gf.laneHeading_deg =
        WrapDeg(pes::PesBearingDeg(0.0f, 0.0f, facingVec.coords[0], facingVec.coords[1]) + 180.0f);
    gf.central = pes::IsCentral(zone);
    // ponytail: the carrier's human flag (the team mode record +0x6C, the field
    // UnitListInput::humanCarrier leaves off) is not decoded to GF state.
    gf.humanCarrier = false;
    gf.carrierStat = CastPlayer()->GetStat("technical_shortpass");  // stat 0x1B Low Pass
    gf.statBonus = PlayerSkills::Has(skills, PlayerSkills::Skill::OneTouchPass);  // skill 0xC
    // ponytail: the team's 0x21D8 flag and its 0x2128 entry (the human carrier's angle raise) are
    // not decoded to GF state.
    gf.flagSet = false;
    gf.flagDistFar = false;
    for (const pes::PassCandidate& c : rethinkCandidates) {
      Player* mate = match->GetPlayer(c.index);
      if (mate == nullptr) continue;
      pes::GoalFrontMate m;
      m.index = laneSlotOf(mate);
      m.difficulty = c.record.difficulty;            // record A +0xF0
      m.distance_m = c.record.ballToTarget_m;        // record B +0xEC
      m.angleToFacing_deg = c.record.turnAngle_deg;  // record B +0xE8
      m.openness = c.record.openness;                // record A +0xF4
      m.opennessClass = c.record.opennessClass;
      // ponytail: the b6c status byte (0x142118D20(team+0xB6C, slot % 11) == 0xB) has no GF
      // counterpart; the receiver enumeration that built these records is the status filter.
      m.statusValid = true;
      // ponytail: the pressure entry's +0x10 (0x14069A3E0(team+0x2124, slot % 11)[+0x10], whose
      // meaning is OPEN) has no GF counterpart; the mate's nearest opponent stands in.
      m.pressure = c.nearestOppToMate_m;
      // His own zone (0x140698D10) at the picker's predicted point, P + frames * V with frames =
      // int(0x141E532D0() * 0x142565600 + 0x142565600) = 27 (half a game second at 54 Hz).
      const int frames =
          static_cast<int>(pes::kPesTickHz * carrier.gfPredictGain + carrier.gfPredictGain);
      const Vector3 matePred =
          mate->GetPosition() + mate->GetMovement() * (frames * pesball::kPesTick_s);
      const PlayerSkills::Mask mateSkills = mate->GetPlayerData()->GetSkills();
      const bool mateLongShotSkill =
          PlayerSkills::Has(mateSkills, PlayerSkills::Skill::LongRangeShooting) ||
          PlayerSkills::Has(mateSkills, PlayerSkills::Skill::KnuckleShot) ||
          PlayerSkills::Has(mateSkills, PlayerSkills::Skill::DippingShots);
      m.zone = pes::ComputeShootZone(matePred.coords[0], matePred.coords[1], attackSign,
                                     mate->GetStat("technical_shot"), mateLongShotSkill,
                                     pesFieldHalfLength, pesGoalInnerHalfWidth, carrier);
      gf.mates.push_back(m);
    }
    goalFrontPass = pes::GoalFrontUnit(gf, carrier);
  }
  // The lane units, BackSpace (0x142129110) and Safety (0x14212B520), read the lane core's 22-slot
  // space and the receiver records the pass decision just built. The PES fields GF does not carry are
  // stand-ins, each named at the field with the PES site that writes it.
  pes::PassAnswer backSpacePass;
  pes::PassAnswer safetyPass;
  pes::PassAnswer oneTwoPass;
  pes::PassAnswer directHighPass;
  if (rethink) {
    pes::BackSpaceInput bs;
    pes::SafetyInput sf;
    // The team's S1 score rows (pesscorerows.hpp, in the lane space) and the attacked goal's row.
    const pes::ScoreRows& rows = team->GetController()->Rows();
    const int attackK = static_cast<int>(attackSign);  // the sign k of FUN_140587E70 and FUN_140A714A0
    // The lane core's slot space, as GoalFront fills it above.
    const auto fillLaneSide = [&](Team* side, int base) {
      std::vector<Player*> squad;
      side->GetActivePlayers(squad);
      for (int j = 0; j < pes::lane::kSideSize && j < static_cast<int>(squad.size()); ++j) {
        Player* p = squad[j];
        if (p == CastPlayer()) bs.carrierSlot = base + j;
        const Vector3 pos = p->GetPosition();
        const Vector3 vel = p->GetMovement();
        pes::lane::LanePlayer& lp = bs.players[base + j];
        lp.pos = {pos.coords[0], pos.coords[1]};
        lp.vel = {vel.coords[0] * pesball::kPesTick_s, vel.coords[1] * pesball::kPesTick_s};
        lp.active = true;
        // 0 marks the keeper to the side record the core reads (0x14063CD90).
        lp.positionCategory = p->GetFormationEntry().role == e_PlayerRole_GK ? 0 : -1;
      }
    };
    fillLaneSide(team, 0);
    fillLaneSide(match->GetTeam(1 - team->GetID()), pes::lane::kSideSize);
    sf.players = bs.players;
    sf.carrierSlot = bs.carrierSlot;
    // The NearestList rank 0 of the side opposite each slot (BackSpace's pass-through validator).
    for (int q = 0; q < pes::lane::kSlots; ++q)
      bs.nearestOpponent[q] = rows.nearest[q].Nearest(pes::lane::IsAwaySlot(q) ? 0 : 1);
    // The gate lane's length: the carrier's d^2 in the row for sign !k (GateRowForSign, 0x140A714A0)
    sf.carrierGateD2_m2 = pes::GateRowForSign(rows, attackK).ValueOf(bs.carrierSlot);

    // The carrier's frame in PES's theta convention: dir(theta) = (sin, cos) over (x, z), so the
    // facing and heading vectors convert directly (this engine's y is PES's z).
    const Vector3 carrierGround = CastPlayer()->GetPosition();
    const Vector3 carrierVel = CastPlayer()->GetMovement();
    const Vector3 carrierDir = CastPlayer()->GetDirectionVec();
    const pes::CarrierPoint carrierXZ{carrierGround.coords[0], carrierGround.coords[1]};
    const pes::CarrierPoint carrierVelFrame{carrierVel.coords[0] * pesball::kPesTick_s,
                                            carrierVel.coords[1] * pesball::kPesTick_s};
    const float facingTheta =
        pes::PesBearingDeg(0.0f, 0.0f, carrierDir.coords[0], carrierDir.coords[1]);
    const float headingTheta = carrierVel.GetLength() <= 0.0f
                                   ? facingTheta
                                   : pes::PesBearingDeg(0.0f, 0.0f, carrierVel.coords[0],
                                                        carrierVel.coords[1]);
    const float carrierKmh = carrierVel.GetLength() * carrier.kmhPerMs;
    const float ballKmh = ballMove.GetLength() * carrier.kmhPerMs;
    const bool danger = pesWorld.ClearDanger(sideIndex);  // imageA+0x31C1, written above
    // ponytail: the lane heading is the angle table 0x140A71260 + 180 (writer undecoded, helpers §5);
    // the carrier's facing stands in, as GoalFront does.
    const float laneHeading = WrapDeg(facingTheta + 180.0f);

    bs.carrierPos = carrierXZ;
    bs.carrierVelPerFrame = carrierVelFrame;
    bs.carrierFacing_deg = facingTheta;
    bs.carrierHeading_deg = headingTheta;
    bs.carrierSlotStat = 0.0f;  // ponytail: imageA+0x898 (writer undecoded, helpers §5) is not carried
    bs.carrierNeed_m = nearestOpp;  // ponytail: 0x1404C8E00 (team table need) stands in with the carrier's nearest opponent
    bs.sideAngle_deg = attackAngle;  // ponytail: team+0x250 is the attack direction
    bs.attackSign = attackSign;
    bs.teamLineOpp = attackSign * pesFieldHalfLength;  // ponytail: team-table lines are not carried; the goal line stands in
    bs.pitchBound8 = pesPenaltySpotX;  // F[8]: gate 3's x limit (0x142122838, raw asm)
    bs.pitchHalfLen_m = pesFieldHalfLength;
    bs.pitchF6 = pesPenaltyAreaLineX;  // F[6]: the box line of gate 6's zone arm (helpers doc)
    bs.pitchF5 = pesGoalAreaHalfWidth;
    bs.ball = {ballPos.coords[0], ballPos.coords[1]};
    bs.ballHeight_m = ballHeight;
    bs.owned = CastPlayer()->GetPossessionDuration_ms() > 0;
    bs.receiving = false;      // ponytail: ctx4+0x6F is not decoded to GF state
    bs.humanCarrier = false;   // ponytail: ctx4+0x6C is not decoded to GF state
    bs.danger = danger;
    bs.goalMouth = false;      // ponytail: imageA+0x31C0 (0x140699960) is not ported
    bs.mid = pes::IsMid(zone);
    bs.central = pes::IsCentral(zone);
    bs.plan = carrierImage.id;
    bs.planMode = carrierImage.mode;
    // ponytail: the plan record's slot-10 target (FUN_14211CB00(plan, 10)) is not carried by GF;
    // plan 4 never names a target, so the plan-4 arm stays silent.
    bs.planRecordTarget = -1;
    bs.planRecordBytesOk = false;
    // The curve table (R2) from the opponents of the carrier's side: the BackSpace gate and G12 read it.
    // PES's psi (T+0x250) is the bearing 90 (attack +x) or 270 (attack -x); `attackAngle` is that same
    // value, so the table takes it.
    const std::optional<pes::Curve> curve =
        pes::BuildCurve(bs.players, pes::lane::IsAwaySlot(bs.carrierSlot) ? 1 : 0, attackSign,
                        attackAngle, pesFieldHalfLength, carrier);
    bs.curve = curve;
    // ponytail: the box scalars (imageA+0xB44/0xB48/0xB3C/0xB40) are not carried; zeros keep the
    // mode-1 arm's skips off.

    sf.carrierPos = carrierXZ;
    sf.carrierVelPerFrame = carrierVelFrame;
    sf.carrierFacing_deg = facingTheta;
    sf.carrierHeading_deg = headingTheta;
    sf.carrierSpeed_kmh = carrierKmh;
    sf.laneHeading_deg = laneHeading;
    sf.attackSign = attackSign;
    sf.sideAngle_deg = attackAngle;
    sf.planHeading_deg = attackAngle;  // ponytail: the plan heading (ctx7+0xC0) stands in with the attack direction
    sf.planMode = carrierImage.mode;
    // The team mode 0x141052E30 = *(u32*)(bp+0x32B0+8), the carrier's Intention the ratchet above
    // writes (writer 0x1421197B0); ctx[6] is BallPlayer+0x32B0 (carrier-ai §Unit context).
    sf.teamMode = intention;
    sf.teamLineOpp = attackSign * pesFieldHalfLength;  // ponytail: team-table lines are not carried
    sf.teamLineB = attackSign * pesFieldHalfLength;
    sf.teamLineOwn = 0.0f;  // ponytail: the own-side line (T18+0x40+side*0x14) stands in with the midfield
    sf.pitchHalfLen_m = pesFieldHalfLength;
    sf.pitchBound8 = pesPenaltySpotX;  // F[8]
    sf.pitchF6 = pesPenaltyAreaLineX;  // F[6]
    sf.pitchF5 = pesGoalAreaHalfWidth;
    sf.ball = bs.ball;
    sf.ballSpeed_kmh = ballKmh;
    // B+0x4C (0x140A6D0D0): the validity bit of the ball's sample 0, the first bounce of its track (0x140874100).
    std::vector<pes::TrackPoint> track;
    const auto& ballTrack = match->GetBall()->GetTrack();
    track.reserve(ballTrack.size());
    for (const Vector3& r : ballTrack) track.push_back({r.coords[0], r.coords[2], r.coords[1]});
    const pes::BounceSample sample0 = pes::FirstBounceSample(track, carrier.bounceLatchHeight_m);
    sf.ballTrackSet = sample0.valid;
    // K+0x40 and its height K+0x44 (FUN_14063A490, 0x14063A854..0x14063A9B5). In a set piece it is the restart display
    // point MI+0x155C, which GF takes as the live ball clamped to the pitch (the match+0x74c rule is not in GF). In open
    // play it is the carrier's AI target point P_ai+0x1414, whose meaning the docs leave open, so the carrier's own
    // ground point stands in and K+0x44 is 0. The replay/special (B+0x2920 == 4) and PASS_GET (carrier +0x5D0 == 5)
    // branches and the TRAP record branch have no GF state and are off. The set piece is MI+0x116C == 3 here, with the
    // kind 2..3 exception not mapped.
    const pes::CtxPoint ctxPoint = pes::CarrierCtxPoint(
        match->IsInSetPiece(),
        {std::clamp(ballPos.coords[0], -pesFieldHalfLength, pesFieldHalfLength), ballHeight,
         std::clamp(ballPos.coords[1], -pesFieldHalfWidth, pesFieldHalfWidth)},
        {carrierPos.coords[0], 0.0f, carrierPos.coords[1]});
    // K+0x6E (FUN_14063A490): the volatile-ball flag, from the ctx4 height K+0x44 (the point's height), the carrier's
    // height P+0xA1 / 100, and K+0x6C off (GF's carriers are AI). It feeds the Safety keeper side arm and the
    // DirectHighBall gate below.
    pes::VolatileBallInput volatileIn;
    volatileIn.ballHeight_m = ctxPoint.y;  // K+0x44
    volatileIn.carrierHeight_m = CastPlayer()->GetStat("physical_height");
    volatileIn.ballTrackFlag = sample0.valid;
    volatileIn.anchor = sample0.xz;
    volatileIn.ball = {ballPos.coords[0], ballPos.coords[1]};
    // B+0x80 (r14b) is clear: its writer is undecoded.
    volatileIn.target = {ctxPoint.x, ctxPoint.z};  // K+0x40
    const bool volatileBall = pes::VolatileBall(volatileIn);
    // The Cross unit (kind 7, 0x142129590): the carry bearing held, the lane open, the tallest
    // mate takes the centering ball. Its lane (0x14063CD90 at 0x142129804) runs from the ball
    // point, one metre along the bearing to the attacked goal's goal-area line on the pitch's
    // centre line, 5 m long with a 30 degree cone and a 2 m radius offset. The unit answers only
    // when the opposing side records nobody in it.
    if (carrierImage.id == pes::kPlanCross || carrierImage.id == pes::kPlanCurveShot ||
        carrierImage.id == pes::kPlanBasic) {
      pes::CrossInput crossIn;
      crossIn.carryVsFacing_deg = 0.0f;  // ponytail: the carry bearing
      // (ball->carrier-predicted, 0x140474C40) is not ported; facing stands in.
      const pes::lane::LaneRequest crossLane = pes::lane::CrossLane(
          carrier.lane, {ctxPoint.x, ctxPoint.z}, attackSign * pesGoalAreaLineX);
      crossIn.laneOpen = pes::lane::LaneFree(carrier.lane, bs.players, bs.carrierSlot, crossLane,
                                              /*keepKeeper*/ false, /*horizonFrames*/ 0);
      crossIn.owned = CastPlayer()->GetPossessionDuration_ms() > 0;
      std::vector<Player*> crossMates;
      team->GetActivePlayers(crossMates);
      for (Player* mate : crossMates) {
        if (mate == CastPlayer()) continue;
        if (mate->GetFormationEntry().role == e_PlayerRole_GK) continue;
        pes::LongMate m;
        m.index = mate->GetID();
        m.height_cm = mate->GetStat("physical_height") * 100.0f;
        crossIn.mates.push_back(m);
      }
      crossPass = pes::CrossUnit(crossIn, carrier);
    }
    sf.humanCarrier = false;  // K+0x6C is FUN_140A63F80's human-controller test; GF's carriers are AI, so it is 0
    sf.receivingFlag = volatileBall;
    sf.ctx44 = ctxPoint.y;  // K+0x44: the ctx4 point's height (the Safety keeper side arm reads it)
    sf.danger = danger;
    sf.goalMouth = false;  // ponytail: imageA+0x31C0 is not ported
    // ponytail: the carrier's pressure counts (+0x30/+0x34/+0x38/+0x20) have no GF state (writer
    // 0x14069A500, helpers §5); a pressed carrier stands in for the first count only.
    sf.pressCount1 = carrierPressureClass == 0 ? 1 : 0;
    sf.pressCount2 = 0;
    sf.pressCount3 = 0;
    sf.pressCount4 = 0;
    sf.carrierPressClass = carrierPressureClass;
    sf.oppPressPair = 0.0f;  // ponytail: T18+0x2470C (INF meaning, writer undecoded) is not carried

    // The OneTwo and DirectHighBall units (kinds 10, 11): the one-touch predicate's carrier inputs
    // (G1-G12), and the teammates both units test, built from the same receiver records below.
    pes::TouchInput touch;
    touch.players = bs.players;
    touch.carrierSlot = bs.carrierSlot;
    touch.carrierPos = carrierXZ;
    touch.attackSign = attackSign;
    touch.attackAngle_deg = attackAngle;
    touch.carrierFacing_deg = facingTheta;
    touch.carrierSpeed_kmh = carrierKmh;
    touch.carrierHeading_deg = headingTheta;
    touch.planHeading_deg = carrierImage.heading_deg;
    touch.planMode = carrierImage.mode;
    touch.ownLine_m = -attackSign * pesFieldHalfLength;  // G1: the own goal line stands in for T18+(sd+3)*0x14
    touch.pitchBound_m = pesFieldHalfLength;             // G2: the pitch half length stands in for F+0x18
    // G3's team level (T+0xA294 x100, reader 0x140A40BD0): the install's own stock class x 100 (the
    // §2 setup paths through the setter 0x140A424E0). GF has no match-setting selector (+0x18) and no
    // per-side +0x9200 byte, so the value is the stock default and the zero flag is false.
    touch.teamLevelRaw = carrier.teamLevelRaw;
    touch.teamLevelZero = false;
    touch.attackLevel = 0;  // G8: T+0xB8A4 is not carried (S4)
    touch.teamMode = intention;  // G4, G8: 0x141052E30 = the carrier's Intention (writer 0x1421197B0)
    touch.carrierCategory = bs.carrierSlot >= 0 ? bs.players[bs.carrierSlot].positionCategory : -1;
    touch.score = pes::ScoreRowForSign(rows, attackK).ValueOf(bs.carrierSlot);  // G5: the S1 row of the sign
    touch.closeZone = pes::IsClose(zone);
    touch.humanCarrier = false;    // K+0x6C: GF's carriers here are AI
    touch.oneTwoRunEvent = false;  // G7: the event record and the one-two run action are not carried (S7)
    touch.runCount = 0;            // G8: the teammates' one-two runs are not carried (S7)
    touch.markCount = 0.0f;        // G10: the mark count is not carried (S11); 0 is the decode's value
    touch.curve = curve;
    touch.flag15 = false;          // C[0xF] is not carried (S18)
    std::vector<pes::TouchReceiver> touchMates;
    std::vector<pes::DirectHighBallMate> dhbMates;
    for (const pes::PassCandidate& c : rethinkCandidates) {
      Player* mate = match->GetPlayer(c.index);
      if (mate == nullptr) continue;
      const int laneSlot = laneSlotOf(mate);
      const Vector3 matePos = mate->GetPosition();
      const Vector3 mateVel = mate->GetMovement();
      const pes::CarrierPoint matePt{matePos.coords[0], matePos.coords[1]};
      const pes::CarrierPoint mateVelFrame{mateVel.coords[0] * pesball::kPesTick_s,
                                           mateVel.coords[1] * pesball::kPesTick_s};
      const Vector3 mateDir = mate->GetDirectionVec();
      const float mateFacing = pes::PesBearingDeg(0.0f, 0.0f, mateDir.coords[0], mateDir.coords[1]);
      const float mateHeading =
          mateVel.GetLength() <= 0.0f
              ? mateFacing
              : pes::PesBearingDeg(0.0f, 0.0f, mateVel.coords[0], mateVel.coords[1]);
      const float mateSpeedKmh = mateVel.GetLength() * carrier.kmhPerMs;
      // The mate's zone at his 16-frame prediction, built as the GoalFront picker builds it.
      const Vector3 matePred16 = matePos + mateVel * (16.0f * pesball::kPesTick_s);
      const PlayerSkills::Mask mateSkills = mate->GetPlayerData()->GetSkills();
      const bool mateLongShotSkill =
          PlayerSkills::Has(mateSkills, PlayerSkills::Skill::LongRangeShooting) ||
          PlayerSkills::Has(mateSkills, PlayerSkills::Skill::KnuckleShot) ||
          PlayerSkills::Has(mateSkills, PlayerSkills::Skill::DippingShots);
      const pes::ShootZone mateZone = pes::ComputeShootZone(
          matePred16.coords[0], matePred16.coords[1], attackSign, mate->GetStat("technical_shot"),
          mateLongShotSkill, pesFieldHalfLength, pesGoalInnerHalfWidth, carrier);
      // The pass target's bearing in PES's theta convention: the record-B lane angle.
      const float passTheta = pes::PesBearingDeg(ballPos.coords[0], ballPos.coords[1], c.recordB.target.x, c.recordB.target.z);
      // ponytail: 0x1404C8E00 (the team table's need) is the mate's nearest-opponent distance here.
      const float mateNeed = c.nearestOppToMate_m;
      const bool mateKeeper = mate->GetFormationEntry().role == e_PlayerRole_GK;
      // The turn fields PES stores as magnitudes (INF: the decode compares them unsigned).
      const float turnAttack = std::fabs(c.record.fwdAngle_deg);
      const float turnFacing = std::fabs(c.record.turnAngle_deg);

      pes::BackSpaceMate bm;
      bm.index = laneSlot;
      bm.pos = matePt;
      bm.velPerFrame = mateVelFrame;
      bm.facing_deg = mateFacing;
      bm.heading_deg = mateHeading;
      bm.speed_kmh = mateSpeedKmh;
      // ponytail: the b6c status byte (0x142118D20 == 0xB) has no GF counterpart; the receiver
      // enumeration that built these records is the filter.
      bm.statusValid = true;
      bm.teamNeed_m = mateNeed;
      bm.teamAngle_deg = mateFacing;  // ponytail: the team table's angle (0x140A71260) is undecoded; the facing stands in
      bm.lineValue = attackSign * matePos.coords[0];  // ponytail: the team table's line value (0x14061E7C0) stands in with his x
      bm.recBTarget = c.recordB.target;
      bm.recBAngle_deg = passTheta;
      bm.recBDist_m = c.recordB.ballToTarget_m;
      bm.recBDifficulty = c.recordB.difficulty;
      bm.recBClass = c.recordB.opennessClass;
      bm.recAClass = static_cast<float>(c.record.opennessClass);
      // ponytail: the pressure entry's +0x10 (writer 0x14069A500) stands in with the mate's class.
      bm.pressureClass = static_cast<float>(c.pressureClass);
      bm.slotStat = 0.0f;  // ponytail: imageA+0x898 is not carried
      // ponytail: 0x14063E270's zone build (0x140698340/830) is undecoded; the ported zone stands in.
      bm.closeTest = pes::IsClose(mateZone);
      bm.zoneBuilt = pes::IsCentral(mateZone);  // ponytail: 0x14063E010, the same builders
      bm.zoneAtPred16 = mateZone;
      bm.roleClass = CarrierRoleClass(mate);
      bs.mates.push_back(bm);

      pes::SafetyMate sm;
      sm.index = laneSlot;
      sm.pos = matePt;
      sm.velPerFrame = mateVelFrame;
      sm.facing_deg = mateFacing;
      sm.heading_deg = mateHeading;
      sm.speed_kmh = mateSpeedKmh;
      sm.statusValid = true;  // ponytail: the b6c status byte stands in, as above
      sm.roleClass = CarrierRoleClass(mate);
      sm.nearestOppD2 = mateNeed * mateNeed;  // 0x140587E70: the team table's d2 (the record stands in)
      sm.score = pes::ScoreRowForSign(rows, attackK).ValueOf(laneSlot);  // gate 4's d^2 (S1 row of the sign)
      sm.pressureClass = static_cast<float>(c.pressureClass);
      sm.zoneMid = pes::IsMid(mateZone);
      sm.zoneAtPred16 = mateZone;
      sm.recATurnAngle_deg = turnAttack;        // record A +0xE4: the pass against the attack direction
      sm.recAVsFacing_deg = turnFacing;         // record A +0xE8: the pass against the carrier's facing
      sm.recADist_m = c.record.ballToTarget_m;  // record A +0xEC
      sm.recADifficulty = c.record.difficulty;  // record A +0xF0
      sm.recAClass = c.record.opennessClass;    // record A [0]
      sm.recBTarget = c.recordB.target;
      sm.recBAngle_deg = passTheta;
      sm.recBVsFacing_deg = std::fabs(c.recordB.turnAngle_deg);
      sm.recBDist_m = c.recordB.ballToTarget_m;
      sm.recBDifficulty = c.recordB.difficulty;
      sm.recBClass = c.recordB.opennessClass;
      sf.mates.push_back(sm);

      // The OneTwo and DirectHighBall receivers: the same record A and his own fields.
      pes::TouchReceiver tr;
      tr.index = laneSlot;
      tr.pos = matePt;
      tr.facing_deg = mateFacing;
      tr.statusValid = true;  // ponytail: the b6c status (0x142118D20 == 0xB, S5) stands in as the filter above
      // ponytail: his role class is the RoleArrays chain's T(side)+0x4BD4[r%11] (S6, writer OPEN in
      // lane-inputs §3); CarrierRoleClass stands in until that array is on main.
      tr.roleClass = CarrierRoleClass(mate);
      tr.oneTwoRun = false;  // ponytail: player(r)+0x5D0 == 0x25 (P3) has no GF state
      tr.difficulty = c.record.difficulty;
      tr.dist_m = c.record.ballToTarget_m;
      tr.turnFacing_deg = turnFacing;
      tr.attackAngle_deg = turnAttack;
      tr.opennessClass = c.record.opennessClass;
      touchMates.push_back(tr);
      pes::DirectHighBallMate dm;
      dm.receiver = tr;
      dm.velPerFrame = mateVelFrame;
      dm.heading_deg = mateHeading;
      dm.speed_kmh = mateSpeedKmh;
      // ponytail: the per-slot angle table 0x140A71260 (S10, writer OPEN) stands in with his facing.
      dm.tableAngle_deg = mateFacing;
      dm.keeper = mateKeeper;
      dm.recAClass = c.record.opennessClass;
      dm.score = pes::ScoreRowForSign(rows, attackK).ValueOf(laneSlot);  // the mate's row entry
      dm.markCount = 0;  // ponytail: AI rec2 +0x12A4 (S11) is not carried
      dhbMates.push_back(dm);
    }
    backSpacePass = pes::BackSpaceUnit(bs, carrier);
    // The carrier's NearestList row: his teammates nearest first. The Safety row and both receiver
    // lists read this order; unlisted players go last.
    const pes::SortedRow noNeighbours;
    const pes::SortedRow& carrierNl = bs.carrierSlot >= 0 ? rows.nearest[bs.carrierSlot] : noNeighbours;
    const auto nlRank = [&](int slot) {
      const int r = carrierNl.RankOf(slot);
      return r < 0 ? pes::lane::kSideSize : r;
    };
    std::stable_sort(sf.mates.begin(), sf.mates.end(),
                     [&](const pes::SafetyMate& a, const pes::SafetyMate& b) {
                       return nlRank(a.index) < nlRank(b.index);
                     });
    safetyPass = pes::SafetyUnit(sf, carrier);
    std::stable_sort(touchMates.begin(), touchMates.end(),
                     [&](const pes::TouchReceiver& a, const pes::TouchReceiver& b) {
                       return nlRank(a.index) < nlRank(b.index);
                     });
    std::stable_sort(dhbMates.begin(), dhbMates.end(),
                     [&](const pes::DirectHighBallMate& a, const pes::DirectHighBallMate& b) {
                       return nlRank(a.receiver.index) < nlRank(b.receiver.index);
                     });
    pes::OneTwoInput one;
    one.touch = touch;
    one.carrierAngle_deg = facingTheta;  // ponytail: K+0x34 (S9, INF) stands in with the carrier's facing
    one.mates = touchMates;
    oneTwoPass = pes::OneTwoUnit(one, carrier);
    pes::DirectHighBallInput dhb;
    dhb.touch = touch;
    dhb.ballHeight_m = ballHeight;
    dhb.ball = {ballPos.coords[0], ballPos.coords[1]};
    // K+0x6E: the same volatile-ball flag the Safety keeper arm reads (above).
    dhb.volatileBall = volatileBall;
    dhb.firstTimeWhileReceiving = false;  // ponytail: AI rec+1 bit 5 (S12) is not carried
    // ponytail: rec2+0x70+0x31 (S13) is the carrier's height stat 0x31, read here in metres.
    dhb.carrierHeight_m = CastPlayer()->GetStat("physical_height");
    dhb.mid = pes::IsMid(zone);
    dhb.mates = dhbMates;
    directHighPass = pes::DirectHighBallUnit(dhb, carrier);
  }
  pes::UnitListInput list;
  list.plan = carrierImage.id;
  list.central = pes::IsCentral(zone);
  list.mid = pes::IsMid(zone);
  list.close = pes::IsClose(zone);
  list.intentionMode4 = intention == 4;
  // ponytail: the plan record (+0xD8, +0xD9), its mode (+0xC4), the slot-10
  // target (FUN_14211CB00), imageA+0xB44 and the cross-line test are not
  // carried by GF; the wide-side tail latches on a wide carrier instead.
  list.wideSideLatch = std::fabs(carrierPos.coords[1]) > pitchHalfH / 3.0f;
  list.wideSector = zone.sector == pes::kSectorWide;
  list.pastCrossLine = carrierImage.id == pes::kPlanCross &&
                       attackSign * carrierPos.coords[0] > pesFieldHalfLength - 10.0f;
  // The Clear unit (kind 1, 0x142109EB0): PES's own function run on the world window. It answers only while imageA+0x31C1
  // (the danger flag written above) is set, and then runs the corridor angle 0x142108630 over the window's records.
  pes::CarrierAnswer clearAnswer;
  const pes::PesWorld::ClearAnswer cleared =
      pesWorld.RunClearUnit(sideIndex, static_cast<uint32_t>(match->PesSlotOf(CastPlayer())));
  if (cleared.type == static_cast<uint32_t>(pes::kAnswerClear)) clearAnswer = {pes::kAnswerClear, 0, cleared.angle_deg};
  // The pass commands take global player ids and the lane answers name lane slots; the walk's records name PES slots
  // (side * 11 + squad place, a man-record index): convert each answer to its PES slot for the table, then the lane
  // answers to global ids for the commands.
  std::vector<Player*> ownActive, oppActive;
  team->GetActivePlayers(ownActive);
  match->GetTeam(1 - team->GetID())->GetActivePlayers(oppActive);
  std::vector<int> ownIds, oppIds, ownPesSlots, oppPesSlots;
  for (Player* p : ownActive) {
    ownIds.push_back(p->GetID());
    ownPesSlots.push_back(match->PesSlotOf(p));
  }
  for (Player* p : oppActive) {
    oppIds.push_back(p->GetID());
    oppPesSlots.push_back(match->PesSlotOf(p));
  }
  const auto atPesSlot = [&](pes::PassAnswer answer, bool isLane) {
    if (answer.index >= 0) answer.index = pes::ReceiverPesSlot(answer.index, isLane, ownIds, oppIds, ownPesSlots, oppPesSlots);
    return answer;
  };
  const std::vector<pes::UnitSlot> table =
      pes::FillUnitTable(pes::BuildUnitList(list), shotAnswer, atPesSlot(pass, false), atPesSlot(longPass, false),
                         atPesSlot(crossPass, false), atPesSlot(goalFrontPass, true), atPesSlot(backSpacePass, true),
                         atPesSlot(safetyPass, true), atPesSlot(oneTwoPass, true), atPesSlot(directHighPass, true), clearAnswer);
  const auto toGlobalId = [&](pes::PassAnswer& answer) {
    if (answer.index >= 0) answer.index = pes::LaneSlotToGlobalId(answer.index, ownIds, oppIds);
  };
  toGlobalId(goalFrontPass);
  toGlobalId(backSpacePass);
  toGlobalId(safetyPass);
  toGlobalId(oneTwoPass);
  toGlobalId(directHighPass);
  // The carrier walk 0x14210F110 (translated; equal to PES on eight real calls and two synthetic Clear-arm states,
  // pes_walk_oracle_test) decides which unit's answer the carrier acts on, the Clear unit's replacement chain included.
  // Its inputs are the window's records; the ones GF writes here name their PES site, the rest stand in.
  uint8_t* bp = pesWorld.Carrier(sideIndex);
  // ponytail: MI+0x116C is PES's match FSM; GF derives it from its own state (3 set piece, 1 in play, 2 stopped).
  pesWorld.Period().playState = static_cast<uint32_t>(match->IsInSetPiece() ? 3 : (match->IsInPlay() ? 1 : 2));
  // MI+0x1775 (the byte the walk vetoes passes and clears on, 3, 9 and 10): PES writes it in three places only, the match-info
  // constructor (0x140A3C7AA: 0), the copy-assign (0x1404144E2) and the setter 0x140A3CCF0, whose single caller 0x141FB1880
  // (the flow state 5, mode 2, reached from 0x141FD5200) stores 3. That mode is chosen by the menus' command strings
  // (0x141FC0335), not by a match in play; the 9 and 10 vetoes have no writer. Open play reads 0 in all 9 readable captures.
  pesWorld.MatchInfo()[pes::kMiWalkStateOffset] = 0;
  pes::StoreU32(bp, pes::kBpIntentionOffset, static_cast<uint32_t>(intention));  // 0x141052E30(BP+0x32B0)
  // ponytail: BP+0x3380 (the turn reference the kind-4 tail measures the facing against) is written inside 0x14211D710's
  // sub-builders 0x14211E2D0 / 0x14211E210 / 0x14211E650 (untranslated, run only while the state's word 0 is 0): the carrier's
  // facing stands in until they land.
  const Vector3 walkDir = CastPlayer()->GetDirectionVec();
  pes::StoreF32(bp, pes::kBpTurnReferenceOffset, pes::PesBearingDeg(0.0f, 0.0f, walkDir.coords[0], walkDir.coords[1]));
  // The zone record at BP+0xAB8 is the danger builder run's own output (run above; equal to PES on the unslimmed captures).
  uint64_t skillMask = 0;
  if (PlayerSkills::Has(skills, PlayerSkills::Skill::FirstTimeShot)) skillMask |= 1ull << pes::kSkillOneTouchShot;
  if (PlayerSkills::Has(skills, PlayerSkills::Skill::OneTouchPass)) skillMask |= 1ull << pes::kSkillOneTouchPass;
  if (PlayerSkills::Has(skills, PlayerSkills::Skill::PinpointCrossing)) skillMask |= 1ull << pes::kSkillPinpointCross;
  const float heightCm = std::clamp(CastPlayer()->GetStat("physical_height") * 100.0f, 0.0f, 255.0f);
  pesWorld.SetCarrierProfile(sideIndex, static_cast<uint32_t>(match->PesSlotOf(CastPlayer())), skillMask,
                             static_cast<uint8_t>(heightCm));
  std::vector<pes::PesWorld::WalkEntry> entries;
  for (const pes::UnitSlot& slot : table) entries.push_back({static_cast<uint32_t>(slot.kind), pes::EncodeAnswerRecord(slot.answer)});
  pesWorld.SetUnitList(sideIndex, entries);
  const uint32_t wonKind = pesWorld.RunWalk(sideIndex);
  if (wonKind == 0) addShot();
  else if (wonKind == 4) addPass(pass);
  else if (wonKind == 9) addPass(longPass);
  else if (wonKind == 7) addPass(crossPass);
  else if (wonKind == 2) addPass(goalFrontPass);
  else if (wonKind == 3) addPass(backSpacePass);
  else if (wonKind == 6) addPass(safetyPass);
  else if (wonKind == 10) addPass(oneTwoPass);
  else if (wonKind == 11) addPass(directHighPass);
  else if (wonKind == 1) {
    // The Clear unit's own answer: the corridor angle (PES bearing, window coordinates: z is GF's y) is the direction.
    const float heading = clearAnswer.angle_deg / degreesPerRadian;
    _AddClearance(commandQueue, Vector3(std::sin(heading), std::cos(heading), 0.0f));
  }
}

void ElizaController::_AddPass(std::vector<PlayerCommand>& commandQueue, Player* target,
                               e_FunctionType passType) {
  PlayerCommand command;
  command.desiredFunctionType = passType;
  command.useDesiredMovement = false;
  command.useDesiredLookAt = false;
  command.touchInfo.targetPlayer = 0;
  command.touchInfo.forcedTargetPlayer = target;
  command.touchInfo.inputDirection = Vector3(0);
  command.touchInfo.inputPower = 0;
  command.touchInfo.autoDirectionBias = 1.0f;
  command.touchInfo.autoPowerBias = 1.0f;
  AI_GetPass(CastPlayer(), passType, command.touchInfo.inputDirection, command.touchInfo.inputPower,
             command.touchInfo.autoDirectionBias, command.touchInfo.autoPowerBias,
             command.touchInfo.desiredDirection, command.touchInfo.desiredPower,
             command.touchInfo.targetPlayer, command.touchInfo.forcedTargetPlayer);
  commandQueue.push_back(command);
}

void ElizaController::_AddPanicPass(std::vector<PlayerCommand>& commandQueue,
                                    const std::vector<PlayerImage>& opponentPlayerImages) {
  int yside = signSide(player->GetDirectionVec().coords[1]);  // > 0 ? 1 : -1;
  Vector3 sensibleAwayDir =
      ((player->GetDirectionVec() * Vector3(0.8f, 1.0f, 0.0f)).GetNormalized() +
       Vector3(-team->GetSide() * 0.7f, yside * 0.5f, 0))
          .GetNormalized(0) +
      Vector3(0, 0, 0.3f);
  sensibleAwayDir.Normalize(player->GetDirectionVec());

  // A blind hoof in one fixed direction was a gift to the nearest interceptor
  // at exactly the moments the carrier is most vulnerable. Probe the default
  // direction and its two neighbours; kick into the safest lane. If every
  // lane is hopeless, the desperate clearance stands.
  float laneOdds[3];
  Vector3 laneDirs[3];
  for (int probe = -1; probe <= 1; probe++) {
    laneDirs[probe + 1] = sensibleAwayDir.GetRotated2D(probe * 0.25f * pi);
    laneOdds[probe + 1] =
        _GetPassingOdds(player->GetPosition() + laneDirs[probe + 1] * 18.0f,
                        e_FunctionType_HighPass, opponentPlayerImages, 1.0f);
  }
  const Vector3 panicDir = laneDirs[GameplayTuning::GetSafestPanicLane(laneOdds)];
  _AddClearance(commandQueue, panicDir);
}

void ElizaController::_AddClearance(std::vector<PlayerCommand>& commandQueue, const Vector3& panicDir) {
  PlayerCommand command;
  command.useDesiredMovement = false;
  command.useDesiredLookAt = false;
  // No intended recipient: this is a clearance, not a pass. MatchData keeps the
  // two apart so passing accuracy measures passing.
  command.touchInfo.isClearance = true;

  command.touchInfo.inputDirection = panicDir;
  command.touchInfo.desiredDirection = panicDir;
  command.touchInfo.autoDirectionBias = 0.0f;
  command.touchInfo.autoPowerBias = 0.0f;

  command.desiredFunctionType = e_FunctionType_HighPass;
  command.touchInfo.inputPower = 0.7f;
  command.touchInfo.desiredPower = 0.7f;
  commandQueue.push_back(command);

  command.desiredFunctionType = e_FunctionType_Shot;
  command.touchInfo.inputPower = 0.6f;
  command.touchInfo.desiredPower = 0.6f;
  commandQueue.push_back(command);

  command.desiredFunctionType = e_FunctionType_LongPass;
  command.touchInfo.inputPower = 0.8f;
  command.touchInfo.desiredPower = 0.8f;
  commandQueue.push_back(command);
}

float ElizaController::_GetPassingOdds(Player* targetPlayer, e_FunctionType passType,
                                       const std::vector<PlayerImage>& opponentPlayerImages,
                                       float ballVelocityMultiplier) {
  float initialTargetDistance = (targetPlayer->GetPosition() - player->GetPosition()).GetLength();
  // A high ball needs room to come down; over a short distance it is never the
  // right pass.
  if (passType == e_FunctionType_HighPass && initialTargetDistance < 10.0f)
    return 0.0f;

  // Evaluate the pass where it will actually be aimed: AI_GetPass leads the
  // receiver by the same amount, and rating a different point than the one the
  // ball is struck at is how a pass can score well and still arrive nowhere.
  Vector3 target = targetPlayer->GetPosition() +
                   targetPlayer->GetMovement() *
                       GameplayTuning::GetReceiverLeadTime_sec(initialTargetDistance);
  if (passType == e_FunctionType_LongPass)
    target += Vector3(-team->GetSide() * initialTargetDistance * 0.2f, 0, 0);

  return _GetPassingOdds(target, passType, opponentPlayerImages, ballVelocityMultiplier);
}

float ElizaController::_GetPassingOdds(const Vector3& target, e_FunctionType passType,
                                       const std::vector<PlayerImage>& opponentPlayerImages,
                                       float ballVelocityMultiplier) {
  float secondScale = 1.0f;  // how many seconds of range to measure danger in

  Vector3 origin = player->GetPosition() + player->GetMovement() * 0.12f;

  // draw imaginary line between this and target player
  Line line(origin, target);

  float danger = 0.0f;
  for (unsigned int opp = 0; opp < opponentPlayerImages.size(); opp++) {
    Vector3 oppPos = opponentPlayerImages.at(opp).position +
                     opponentPlayerImages.at(opp).movement * 0.2f;  // + time needed to brake
    float u;  // % of line opp is closest to (0 .. 1)
    float oppDistance;
    oppDistance = line.GetDistanceToPoint(oppPos, u);

    if (u >= 0.0f && u <= 1.0f + 0.2f) {  // opp is dangerous in the first place
      if ((passType == e_FunctionType_HighPass && (u < 0.2f || u > 0.65f)) ||
          passType != e_FunctionType_HighPass) {
        float clampedU = clamp(u, 0.0f, 1.0f);
        Vector3 intersect = origin * (1.0f - clampedU) +
                            target * clampedU;  // where opp is most likely to intercept ball

        float oppToIntersect_sec =
            (oppDistance + 1.0f) / sprintVelocity;  // add some distance to correct for acceleration

        // How long the ball is in the air before it reaches the point this
        // opponent would meet it at. `intersect` is already u of the way down
        // the lane, so its distance from the origin carries u once; the old
        // line multiplied by u a second time and priced a mid-lane interception
        // at u-squared of its real flight. That is exactly where an interceptor
        // stands, so every marked lane read as faster - and safer - than it was.
        Vector3 originToBallPos = (intersect - origin);
        float penaltyTime = (passType == e_FunctionType_HighPass && u > 0.5f)
                                ? 2.5f
                                : 0.0f;  // trapping high balls takes time
        float ballToIntersect_sec = 0.7f + originToBallPos.GetLength() * 0.03f + penaltyTime;
        ballToIntersect_sec *= 1.0f / ballVelocityMultiplier;

        danger += clamp(ballToIntersect_sec - oppToIntersect_sec + (secondScale * 0.5f), 0.0f,
                        secondScale);  // add some seconds because 0 seconds doesn't mean 0 danger
      }
    }
  }

  // Pressure at the receiving end, not just along the lane: a target with a
  // marker on his shoulder used to score the same as a free man.
  float nearestOpponentDistance = 1000.0f;
  for (unsigned int opp = 0; opp < opponentPlayerImages.size(); opp++) {
    Vector3 oppPos = opponentPlayerImages.at(opp).position +
                     opponentPlayerImages.at(opp).movement * 0.2f;
    nearestOpponentDistance = std::min(nearestOpponentDistance, (oppPos - target).GetLength());
  }

  if (passType == e_FunctionType_HighPass)
    danger += 0.4f;  // just don't prefer high passing when low passing is applicable as well
  danger = NormalizedClamp(
      danger, 0.0f, secondScale);  // 1 super dangerous dude is basically the same as 100% danger
  float odds = (1.0f - danger) *
               (1.0f - GameplayTuning::GetReceiverPressureDanger(nearestOpponentDistance));
  // Danger only covers being intercepted. A pass also has to be struck and
  // received, and both get harder the further it travels, so price the distance
  // itself rather than treating an empty forty-metre channel as a certainty.
  odds *= GameplayTuning::GetPassExecutionOdds((target - origin).Get2D().GetLength());

  return odds;
}

void ElizaController::_AddCelebration(std::vector<PlayerCommand>& commandQueue) {
  signed int xSide = (match->GetBall()->Predict(0).Get2D().coords[0] > 0) ? 1 : -1;
  signed int ySide = team->GetSide();
  if (team->GetLastTouchPlayer())
    ySide = (team->GetLastTouchPlayer()->GetPosition().coords[1] > 0) ? 1 : -1;
  // A dozen metres towards the crowd in the corner of the half he scored in, not the
  // corner flag itself: he used to be sent to the exact corner and then frozen there.
  float runX = player->GetPosition().coords[0], runY = player->GetPosition().coords[1];
  GoalCelebration::RunTarget(runX, runY, xSide, ySide, pitchHalfW, pitchHalfH, &runX, &runY);
  Vector3 celebrationPosition = Vector3(runX, runY, 0);
  // The scorer runs to PES's celebration spot T where a PES goal scene gave one (Match::GetGoalCelebrationSpot, 0x140476C50's
  // answer), not the capped run above: PES's scorer runs the whole way and its goal gate opens on his arrival.
  // ponytail: the run's driver in PES (the controller that moves the scorer to T) is not decoded; this sprint stands in for it.
  // The capped RunTarget stays for the headless sim, which builds no PES goal scene.
  Vector3 pesSpot;
  if (match->GetLastGoalScorer() == player && match->GetGoalCelebrationSpot(&pesSpot)) celebrationPosition = pesSpot;

  // Flat by contract: Humanoid::SelectAnim asserts a movement direction has no
  // height. A scorer lifted off the turf by his own anim would otherwise hand it
  // a tilted direction.
  // A player already on his celebration point (or on his team-mate: a substitute stands where the man he replaced
  // stood) has no displacement, so the direction keeps his facing rather than normalising a zero vector to NaN.
  Vector3 desiredDirection =
      (celebrationPosition - player->GetPosition()).Get2D().GetNormalized(player->GetDirectionVec());
  float desiredVelocityFloat =
      ClampVelocity((celebrationPosition - player->GetPosition()).GetLength() / 4.0f);
  Vector3 desiredLookAt = player->GetPosition() + player->GetDirectionVec() * 1000;

  int celebrationType = 1;
  if (match->GetLastGoalTeamID() != team->GetID()) {
    celebrationType = 2;
    desiredVelocityFloat = idleVelocity;
  } else {
    celebrationType = 1;
  }

  int madeGoal = 1;
  if (celebrationType == 1) {
    if (team->GetLastTouchPlayer() == player) {
      madeGoal = 2;
      desiredVelocityFloat = sprintVelocity;
    } else {
      madeGoal = 1;
      if (team->GetLastTouchPlayer() != 0) {
        if ((team->GetLastTouchPlayer()->GetPosition() - player->GetPosition()).GetLength() < 20) {
          celebrationPosition = team->GetLastTouchPlayer()->GetPosition();
          desiredDirection =
              ((team->GetLastTouchPlayer()->GetPosition() * 0.5 + celebrationPosition * 0.5) -
               player->GetPosition())
                  .Get2D()
                  .GetNormalized(player->GetDirectionVec());
          desiredVelocityFloat = ClampVelocity(
              (team->GetLastTouchPlayer()->GetPosition() - player->GetPosition()).GetLength() /
              2.0);
        } else {
          desiredVelocityFloat = idleVelocity;
        }
        desiredLookAt = player->GetPosition() + team->GetLastTouchPlayer()->GetDirectionVec() * 100;
      }
    }
  }

  // celebration

  // The performance runs for as long as its clip does, rather than for a flat slice
  // of the stoppage: a 5.2 s celebration was being cut at 4 s, which dropped the
  // scorer out of his pose and into ordinary play with the camera still on him.
  // He performs when he gets there. Two seconds after the goal the performance used to
  // take over wherever he had reached, which is a man stopping dead mid-stride and
  // dancing on the spot - and a blocked scorer still celebrates, on the cap.
  const float toTarget = (celebrationPosition - player->GetPosition()).GetLength();
  const bool ready =
      match->GetLastGoalScorer() != player ||
      GoalCelebration::HasArrived(toTarget, match->GetGoalScoredTimer());
  if (ready && GoalCelebration::IsPerforming(match->GetGoalScoredTimer(),
                                             GoalSequence::kCelebration_ms)) {
    PlayerCommand command;
    command.desiredFunctionType = e_FunctionType_Special;
    command.useSpecialVar1 = true;
    command.specialVar1 = celebrationType;
    command.useSpecialVar2 = true;
    // A PES celebration is two clips: an opening, and a loop held until it is over.
    // Asking for the opening the whole way through played it and then dropped the
    // player back into whatever came next, which is why a celebration read as
    // unfinished (goalcelebration.hpp, tools/pes21_import/install_anims.py).
    // The clip lengths are GF's defaults: the per-goal choice of a filmed PES
    // celebration, which set them per scorer, was presentation and is gone.
    const int opening = madeGoal;
    const int loop = GoalCelebration::LoopVariable(opening);
    // Only ask for the loop when a loop was installed.
    const bool loopExists = match->GetAnimCollection() &&
                            match->GetAnimCollection()->HasSpecial(celebrationType, loop);
    command.specialVar2 = loopExists && GoalCelebration::Phase(
                                            match->GetGoalScoredTimer(),
                                            GoalCelebration::kIntroHold_ms) ==
                                            GoalCelebration::e_Loop
                              ? loop
                              : opening;
    command.useDesiredMovement = false;
    command.useDesiredLookAt = false;
    commandQueue.push_back(command);
  }

  // movement

  {
    PlayerCommand command;
    command.desiredFunctionType = e_FunctionType_Movement;
    command.useDesiredMovement = true;
    command.desiredDirection = desiredDirection;
    command.desiredVelocityFloat = desiredVelocityFloat;
    command.useDesiredLookAt = true;
    command.desiredLookAt = desiredLookAt;
    commandQueue.push_back(command);
  }
}
