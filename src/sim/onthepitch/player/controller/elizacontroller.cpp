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
        // PES's goal kick is a lofted pass after the keeper's set-play wait (pesgoalkick.hpp): to
        // the mate the lofted picker names, or into space when there is none. Replaces the GF coin
        // below on pes_loop.
        pesGoalKick = true;
        std::vector<Player*> mates;
        team->GetActivePlayers(mates);
        std::vector<float> ratings;
        int takerIndex = -1;
        for (Player* mate : mates) {
          if (mate == CastPlayer()) takerIndex = static_cast<int>(ratings.size());
          ratings.push_back(mate->GetStat("technical_highpass"));
        }
        const int receiver = pes::PickGoalKickReceiver(ratings, takerIndex);
        actionCommand.desiredFunctionType = e_FunctionType_HighPass;
        if (pes::GoalKickAfterWait(receiver >= 0) == pes::GoalKickCommand::kLoftedToReceiver) {
          actionCommand.touchInfo.forcedTargetPlayer = mates[receiver];
          desiredTargetPosition = mates[receiver]->GetPosition();
        } else {
          // ponytail: PES's receiver-less kick (player 0xff) has no decoded target. GF aims at the
          // midfield point on the attacking side, the old stand-in's x, until that is traced.
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
        // ponytail: PES picks the mate as the first team-mate in his own
        // neighbour list with 10 m of space (MA+0x22e4c is the engine's
        // per-player sorted distance list); this engine's nearest-player rule
        // to the carry point stands in for that list.
        const pes::KeeperConstants& keeperConstants = match->GetPesKeeperConstants();
        // His own goal-area and box marks (PES F5/F6/F7). The keeper of the -x
        // goal attacks +x, so his own half is -x: the marks are magnitudes and
        // `attackDir` carries the sign.
        const int attackDir = -team->GetSide();
        const pes::KeeperFieldMarks marks = pes::KeeperFieldMarksFromFieldInfo();
        const pes::DistributionPoint carry = pes::PesAfterCatchCarry(
            player->GetPosition().coords[0], player->GetPosition().coords[1], attackDir, marks,
            keeperConstants);
        Player* targetPlayer = AI_GetClosestPlayer(team, Vector3(carry.x, carry.z, 0), CastPlayer());

        if (targetPlayer) {
          // check if this player is away from opponents, before throwing ball to him
          Player* closestOpp = AI_GetClosestPlayer(match->GetTeam(abs(team->GetID() - 1)),
                                                   targetPlayer->GetPosition(), 0);
          if (closestOpp) {
            if (((closestOpp->GetPosition() + closestOpp->GetMovement() * 0.1f) -
                 targetPlayer->GetPosition())
                        .GetLength() > 10.0f ||
                CastPlayer()->GetPossessionDuration_ms() >
                    4000) {  // last touch bias is maximum so he won't hold forever
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
      } else {
        GetOnTheBallCommands(commandQueue, rawInputDirection, rawInputVelocityFloat);
      }
    } else {
      // Out of reach of the ball, so he cannot pass or dribble it - but he can
      // still run at it, and he must. This branch used to queue NOTHING: the
      // designated ball-goer stood still while every other player held his
      // shape, and a ball at rest more than a second from everyone froze the
      // whole pitch for the rest of the half (measured: 32:19 to 45:00).
      const Vector3 toBall = match->GetBall()->GetPositionBuffer() - CastPlayer()->GetPosition();
      if (toBall.GetLength() > 0.5f) {
        PlayerCommand run;
        run.desiredFunctionType = e_FunctionType_Movement;
        run.useDesiredMovement = false;
        run.useDesiredLookAt = true;
        // 2D: a ball in the air must not tilt the run at it.
        run.desiredDirection = toBall.Get2D().GetNormalized(CastPlayer()->GetDirectionVec());
        // Sprint: he is chasing a ball nobody else is going for.
        run.desiredVelocityFloat = sprintVelocity;
        run.desiredLookAt = match->GetBall()->GetPositionBuffer();
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

    inputVelocityFloat = RangeVelocity(inputVelocityFloat);  // emulate controller quantization

    // PES's PASS_GET receiver runs ActionPassGet's approach (BallFollow): to
    // the ball path's nearest point, clamped onto the pitch. GF's magnet
    // approach is for everyone else.
    if (team->PassGetReceiver() == player && !hasPossession)
      _PassGetApproach(commandQueue);
    else
      _MovementCommand(commandQueue, forceMagnet, extraHaste);

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
  const pes::DefenderReaction chosen = pes::ChooseReaction(in);
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
  // Entry 8's owner half: an opponent owner inside the block radius.
  if (match->GetBallRetainer() == 0) return false;
  Player* carrier = _Carrier();
  if (!carrier || carrier->GetTeamID() == team->GetID()) return false;
  if (match->GetBallRetainer() != carrier) return false;
  // PES's PRESS state: the MATCH_UP man while ForeCheckPress runs.
  const bool pressing = team->GetController()->IsPressing(CastPlayer());
  const float radius = pes::BlockRadius_m(k, pressing, false, false);
  if ((carrier->GetPosition() - CastPlayer()->GetPosition()).GetLength() > radius) return false;
  // A pass or a shot clip widens the radius to 7 m once it has passed its
  // fraction. GF's clip table carries no pass clip, so the widened radius is
  // read through the requirement that the ball is already in flight towards
  // him - the late window - rather than through a clip's own progress.
  return true;
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
  return (ballPredict - CastPlayer()->GetPosition()).GetLength() <= radius;
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
    // The receiver offside prediction (report 4.3's status 6): every input to
    // the carrier's offside line, and the receiver's own position, is read
    // int(gameSpeed x 0.3) = 16 frames ahead.
    constexpr float kReceiverFrames = 16.0f;
    const float receiverPrediction_s = kReceiverFrames * pesball::kPesTick_s;
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
    for (Player* mate : mates) {
      if (mate == CastPlayer()) continue;
      if (mate->GetFormationEntry().role == e_PlayerRole_GK) continue;  // status 5
      // The mate's target is his position a lane-time ahead (record A, "to
      // feet"); record B ("into space") is not ported.
      const Vector3 matePos = mate->GetPosition();
      const Vector3 matePred = matePos + mate->GetMovement() * laneTime_s;
      // PES 2021's receiver enumeration (docs/re/pes-re-carrier-ai.md 4.3):
      // a team-mate whose position now or his position the receiver
      // prediction ahead is beyond the carrier's offside line gets status 6
      // - offside - and is never a pass receiver.
      if (AI_ReceiverIsOffside(match, _mentalImage, team->GetID(), matePos,
                               mate->GetMovement(), ballPos, ballMove,
                               receiverPrediction_s))
        continue;
      const Vector3 target3(matePred.coords[0], matePred.coords[1], ballPos.coords[2]);
      const float dist = (target3 - ballPos).GetLength();
      if (dist > in.halfLen_m) continue;  // off the pitch
      const float dx = target3.coords[0] - ballPos.coords[0];
      const float dz = target3.coords[1] - ballPos.coords[1];
      // The pass's forward angle is its bearing against the attack direction,
      // and its turn angle the same bearing against the carrier's facing
      // (report 4.3's +0xE4 and +0xE8). The absolute bearing stays local: the
      // openness loop measures each opponent against the pass direction from
      // it.
      const float bearing = pes::PesBearingDeg(ballPos.coords[0], ballPos.coords[1],
                                                target3.coords[0], target3.coords[1]);
      const float facingAngle = pes::PesBearingDeg(0.0f, 0.0f, facing.coords[0], facing.coords[1]);
      const float fwdAngle = WrapDeg(bearing - pes::AttackAngleDeg(attackSign));
      const float turnAngle = WrapDeg(bearing - facingAngle);

      // The openness: the minimum over the opponents in front of the pass.
      float bestMargin = 150.0f;
      for (const PlayerImage& opp : opponentPlayerImages) {
        const float oppDx = opp.position.coords[0] - ballPos.coords[0];
        const float oppDz = opp.position.coords[1] - ballPos.coords[1];
        if (std::fabs(WrapDeg(std::atan2(oppDz, oppDx) * degreesPerRadian - bearing)) >
            carrier.openInFront_deg)
          continue;
        // The opponent's predicted point along the lane.
        const Vector3 oppPred =
            opp.position + opp.movement * laneTime_s;
        const float odx = oppPred.coords[0] - ballPos.coords[0];
        const float odz = oppPred.coords[1] - ballPos.coords[1];
        const float laneLen = std::sqrt(dx * dx + dz * dz);
        const float along = (odx * dx + odz * dz) / std::max(laneLen, 0.001f);
        const float perp = std::fabs(odx * dz - odz * dx) / std::max(laneLen, 0.001f);
        bestMargin = std::min(bestMargin,
                              pes::LaneOpennessMargin(dx, dz, along, perp, 1.0f, 0.0f, carrier));
      }
      const float difficulty = pes::ReceiverDifficulty(
          ballMove.GetLength(), dist, 0.0f,
          PlayerSkills::Has(skills, PlayerSkills::Skill::OneTouchPass), carrierSpeed_kmh, false,
          turnAngle, ballHeight, lowPass, carrier);

      pes::PassCandidate c;
      c.index = mate->GetID();
      c.record.target = {target3.coords[0], target3.coords[1]};
      c.record.passAngle_deg = fwdAngle;
      c.record.fwdAngle_deg = fwdAngle;
      c.record.turnAngle_deg = turnAngle;
      c.record.ballToTarget_m = dist;
      c.record.difficulty = difficulty;
      c.record.openness = bestMargin;
      c.record.opennessClass = pes::OpennessClassFromMargin(bestMargin, carrier);
      c.record.dirClass = pes::DirectionClass(fwdAngle, carrier);
      c.record.turnClass = pes::TurnClass(turnAngle, carrier);
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

  pes::PassAnswer pass;
  if (rethink) pass = passAnswer();
  // The Long unit (kind 9, 0x14212AD40) answers on the rethink too: owned or
  // danger with the ball low, target the most advanced mate.
  pes::PassAnswer longPass;
  if (rethink && (carrierImage.id == pes::kPlanLong || carrierImage.id == pes::kPlanBasic)) {
    pes::LongUnitInput longIn;
    longIn.owned = CastPlayer()->GetPossessionDuration_ms() > 0;
    // ponytail: the danger flag (imageA+0x31C1, 0x140699960) is not ported;
    // the carrier's own-third pressure stands in for it.
    longIn.danger = attackSign * carrierPos.coords[0] < -pitchHalfW / 3.0f &&
                    carrierPressureClass == 0;
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
  // bearing held, the lane open, the tallest mate takes the centering ball.
  pes::PassAnswer crossPass;
  if (rethink &&
      (carrierImage.id == pes::kPlanCross || carrierImage.id == pes::kPlanCurveShot ||
       carrierImage.id == pes::kPlanBasic)) {
    pes::CrossInput crossIn;
    crossIn.carryVsFacing_deg = 0.0f;  // ponytail: the carry bearing
    // (ball->carrier-predicted, 0x140474C40) is not ported; facing stands in.
    crossIn.laneOpen = carrierPressureClass >= 2;  // ponytail: 0x14063CD90 is not ported
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
  // The GoalFront unit (kind 2; gate 0x14212ABA0, picker 0x1421265C0): outside the central zone,
  // with the lane about the per-slot angle + 180 blocked, the most open mate in front of goal takes
  // a short ball. Its mate records are the receiver records the pass decision just built.
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
        WrapDeg(std::atan2(facingVec.coords[0], facingVec.coords[1]) * degreesPerRadian + 180.0f);
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
      m.index = c.index;
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
    // ponytail: imageA+0x31C1 (danger, 0x140699960) is not ported; the carrier's own-third pressure
    // stands in, as the Long unit does.
    const bool danger =
        attackSign * carrierPos.coords[0] < -pitchHalfW / 3.0f && carrierPressureClass == 0;
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
    bs.pitchBound8 = pesFieldHalfLength;
    bs.pitchHalfLen_m = pesFieldHalfLength;
    bs.pitchF6 = pesFieldHalfLength;
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
    sf.pitchBound8 = pesFieldHalfLength;
    sf.pitchF6 = pesFieldHalfLength;
    sf.pitchF5 = pesGoalAreaHalfWidth;
    sf.ball = bs.ball;
    sf.ballSpeed_kmh = ballKmh;
    sf.ballTrackSet = false;  // ponytail: the ball-track flag (0x140A6D0D0) is not decoded to GF state
    sf.humanCarrier = false;  // ponytail: ctx4+0x6C is not decoded to GF state
    sf.receivingFlag = false;  // ponytail: ctx4+0x6E is not decoded to GF state
    sf.ctx44 = 0.0f;           // ponytail: ctx4+0x44 is not decoded to GF state
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
    const auto distSq = [&](pes::CarrierPoint p) {
      const float dx = p.x - carrierXZ.x;
      const float dz = p.z - carrierXZ.z;
      return dx * dx + dz * dz;
    };
    // ponytail: score(T18, k, s), the d2 of the score row (S1, writer OPEN), stands in with the squared
    // distance to the attacked goal's centre.
    const auto scoreStandIn = [&](pes::CarrierPoint p) {
      const float dx = p.x - attackSign * pesFieldHalfLength;
      return dx * dx + p.z * p.z;
    };
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
    // writer 0x1B5D8340, setter 0x140A424E0). GF has no match-setting selector (+0x18) and no per-side
    // +0x9200 byte, so the value is the stock default and the zero flag is false.
    touch.teamLevelRaw = carrier.teamLevelRaw;
    touch.teamLevelZero = false;
    touch.attackLevel = 0;  // G8: T+0xB8A4 is not carried (S4)
    touch.teamMode = intention;  // G4, G8: 0x141052E30 = the carrier's Intention (writer 0x1421197B0)
    touch.carrierCategory = bs.carrierSlot >= 0 ? bs.players[bs.carrierSlot].positionCategory : -1;
    touch.score = scoreStandIn(carrierXZ);
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
      const Vector3 matePos = mate->GetPosition();
      const Vector3 mateVel = mate->GetMovement();
      const pes::CarrierPoint matePt{matePos.coords[0], matePos.coords[1]};
      const pes::CarrierPoint mateVelFrame{mateVel.coords[0] * pesball::kPesTick_s,
                                           mateVel.coords[1] * pesball::kPesTick_s};
      const Vector3 mateDir = mate->GetDirectionVec();
      const float mateFacing = std::atan2(mateDir.coords[0], mateDir.coords[1]) * degreesPerRadian;
      const float mateHeading =
          mateVel.GetLength() <= 0.0f
              ? mateFacing
              : std::atan2(mateVel.coords[0], mateVel.coords[1]) * degreesPerRadian;
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
      const float passTheta = std::atan2(c.record.target.x - ballPos.coords[0],
                                         c.record.target.z - ballPos.coords[1]) *
                              degreesPerRadian;
      // ponytail: 0x1404C8E00 (the team table's need) is the mate's nearest-opponent distance here.
      const float mateNeed = c.nearestOppToMate_m;
      const bool mateKeeper = mate->GetFormationEntry().role == e_PlayerRole_GK;
      // The turn fields PES stores as magnitudes (INF: the decode compares them unsigned).
      const float turnAttack = std::fabs(c.record.fwdAngle_deg);
      const float turnFacing = std::fabs(c.record.turnAngle_deg);

      pes::BackSpaceMate bm;
      bm.index = c.index;
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
      bm.recBTarget = c.record.target;
      bm.recBAngle_deg = passTheta;
      bm.recBDist_m = c.record.ballToTarget_m;
      bm.recBDifficulty = c.record.difficulty;
      bm.recBClass = c.record.opennessClass;
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
      sm.index = c.index;
      sm.pos = matePt;
      sm.velPerFrame = mateVelFrame;
      sm.facing_deg = mateFacing;
      sm.heading_deg = mateHeading;
      sm.speed_kmh = mateSpeedKmh;
      sm.statusValid = true;  // ponytail: the b6c status byte stands in, as above
      sm.roleClass = CarrierRoleClass(mate);
      sm.nearestOppD2 = mateNeed * mateNeed;  // 0x140587E70: the team table's d2 (the record stands in)
      sm.pressureClass = static_cast<float>(c.pressureClass);
      sm.zoneMid = pes::IsMid(mateZone);
      sm.zoneAtPred16 = mateZone;
      sm.recATurnAngle_deg = turnAttack;        // record A +0xE4: the pass against the attack direction
      sm.recAVsFacing_deg = turnFacing;         // record A +0xE8: the pass against the carrier's facing
      sm.recADist_m = c.record.ballToTarget_m;  // record A +0xEC
      sm.recADifficulty = c.record.difficulty;  // record A +0xF0
      sm.recAClass = c.record.opennessClass;    // record A [0]
      sm.recBTarget = c.record.target;
      sm.recBAngle_deg = passTheta;
      sm.recBVsFacing_deg = turnFacing;
      sm.recBDist_m = c.record.ballToTarget_m;
      sm.recBDifficulty = c.record.difficulty;
      sm.recBClass = c.record.opennessClass;
      sf.mates.push_back(sm);

      // The OneTwo and DirectHighBall receivers: the same record A and his own fields.
      pes::TouchReceiver tr;
      tr.index = c.index;
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
      dm.score = scoreStandIn(matePt);
      dm.markCount = 0;  // ponytail: AI rec2 +0x12A4 (S11) is not carried
      dhbMates.push_back(dm);
    }
    backSpacePass = pes::BackSpaceUnit(bs, carrier);
    safetyPass = pes::SafetyUnit(sf, carrier);
    // The teammates nearest the carrier first, the NearestList order both units read.
    std::stable_sort(touchMates.begin(), touchMates.end(),
                     [&](const pes::TouchReceiver& a, const pes::TouchReceiver& b) {
                       return distSq(a.pos) < distSq(b.pos);
                     });
    std::stable_sort(dhbMates.begin(), dhbMates.end(),
                     [&](const pes::DirectHighBallMate& a, const pes::DirectHighBallMate& b) {
                       return distSq(a.receiver.pos) < distSq(b.receiver.pos);
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
    // ponytail: K+0x6E (a volley or header situation, no GF state) is not carried, so the unit's own
    // gate stays shut in GF.
    dhb.volatileBall = false;
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
  const std::vector<pes::UnitSlot> table =
      pes::FillUnitTable(pes::BuildUnitList(list), shotAnswer, pass, longPass, crossPass,
                         goalFrontPass, backSpacePass, safetyPass, oneTwoPass, directHighPass);
  pes::WalkContext walk;
  // The restart mode: 3 in a set piece, 1 in play, 2 stopped (report 5, MI+0x116C).
  walk.restartMode = match->IsInSetPiece() ? 3 : (match->IsInPlay() ? 1 : 2);
  // ponytail: MI+0x1775 (the FSM byte) has no GF state, and its writers are not decoded.
  // ponytail: MA+0x7C, the designated kicker, has no GF accessor yet (set pieces are pessetpiecetaker's).
  Player* ballOwner = match->GetBallRetainer();
  walk.ballOwnerTeam = ballOwner != nullptr ? ballOwner->GetTeamID() : -1;
  walk.ownTeam = team->GetID();
  walk.ballHeightOk = ballHeight >= CastPlayer()->GetStat("physical_height");
  walk.skill11 = PlayerSkills::Has(skills, PlayerSkills::Skill::FirstTimeShot);
  walk.skill12 = PlayerSkills::Has(skills, PlayerSkills::Skill::OneTouchPass);
  walk.skill14 = PlayerSkills::Has(skills, PlayerSkills::Skill::PinpointCrossing);
  walk.closeBand = pes::IsClose(zone);
  // ponytail: the tier gate (rule 3) is off: live tier 5 is zero delay and GF has no tier.
  // ponytail: the forced-clear opponent count (rule 7, 0x140A4C820) reads
  // the pressure table GF does not carry; clears always replace.
  walk.owned = CastPlayer()->GetPossessionDuration_ms() > 0;
  walk.safetyToKeeper = longPass.index >= 0 &&
                        match->GetPlayer(longPass.index) != nullptr &&
                        match->GetPlayer(longPass.index)->GetFormationEntry().role == e_PlayerRole_GK;
  walk.keeperWithinNinety = carrierPressureClass >= 2;  // ponytail: 0x140439FE0 is not ported
  // ponytail: the instant-return event scan (rule 8, 0x140A3E940) reads the
  // event table GF does not carry; Forward answers always stand.
  const pes::UnitSlot won = pes::WalkCarrierUnits(table, walk);
  if (won.kind == 0) addShot();
  else if (won.kind == 4) addPass(pass);
  else if (won.kind == 9) addPass(longPass);
  else if (won.kind == 7) addPass(crossPass);
  else if (won.kind == 2) addPass(goalFrontPass);
  else if (won.kind == 3) addPass(backSpacePass);
  else if (won.kind == 6) addPass(safetyPass);
  else if (won.kind == 10) addPass(oneTwoPass);
  else if (won.kind == 11) addPass(directHighPass);
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

  // Flat by contract: Humanoid::SelectAnim asserts a movement direction has no
  // height. A scorer lifted off the turf by his own anim would otherwise hand it
  // a tilted direction.
  Vector3 desiredDirection =
      (celebrationPosition - player->GetPosition()).Get2D().GetNormalized();
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
                  .GetNormalized();
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
