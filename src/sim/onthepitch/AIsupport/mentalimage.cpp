// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not
// be used for anything important. i do not offer support, so don't ask. to be used for inspiration
// :)

#include "mentalimage.hpp"

#include "../match.hpp"
#include <stdexcept>
#include <string>

MentalImage::MentalImage(Match* match) : match(match) {
  timeStampNeg_ms = 0;
  maxDistanceDeviation =
      2.5f;  // if reality is this much (or more) off from mental image, enforce as maximum offset
  maxMovementDeviation = walkVelocity;
}

MentalImage::~MentalImage() {}

void MentalImage::TakeSnapshot() {
  players.clear();

  std::vector<Player*> allPlayers;
  match->GetTeam(0)->GetActivePlayers(allPlayers);
  match->GetTeam(1)->GetActivePlayers(allPlayers);

  for (auto* player : allPlayers) {
    PlayerImage playerImage;
    playerImage.teamID = player->GetTeamID();
    playerImage.side = player->GetTeam()->GetSide();
    playerImage.playerID = player->GetID();
    playerImage.player = player;
    playerImage.position = player->GetPosition();
    playerImage.directionVec = player->GetDirectionVec();
    playerImage.bodyDirectionVec = player->GetBodyDirectionVec();
    playerImage.velocity = player->GetFloatVelocity();
    playerImage.movement = player->GetMovement();
    playerImage.formationEntry = player->GetFormationEntry();
    playerImage.dynamicFormationEntry = player->GetDynamicFormationEntry();
    players.push_back(playerImage);
  }

  UpdateBallPredictions();
}

PlayerImage MentalImage::GetPlayerImage(int playerID) const {
  for (const auto& playerImage : players) {
    if (playerImage.playerID == playerID) {
      PlayerImage newImage = playerImage;
      Vector3 extrapolation = playerImage.movement * GetTimeStampNeg_ms() * 0.001f;
      newImage.position = playerImage.position + extrapolation;
      newImage.position = newImage.position.EnforceMaximumDeviation(newImage.player->GetPosition(),
                                                                    maxDistanceDeviation);
      newImage.movement = newImage.movement.EnforceMaximumDeviation(newImage.player->GetMovement(),
                                                                    maxMovementDeviation);
      return newImage;
    }
  }

  // The caller asks only for a player the snapshot holds. The old failsafe returned players[0],
  // another player's image, or threw out_of_range from an empty snapshot. PES's mental image
  // build is not located (docs/re/pes-re-tick-order.md 4), so there is no PES guard to decode.
  throw std::runtime_error("MentalImage::GetPlayerImage: player " + std::to_string(playerID) +
                           " is not in the snapshot");
}

void MentalImage::GetTeamPlayerImages(int teamID, int exceptPlayerID,
                                      std::vector<PlayerImage>& playerImages) const {
  for (const auto& playerImage : players) {
    Player* player = match->GetPlayer(playerImage.playerID);
    assert(player);
    if (player->IsActive() && player->GetTeamID() == teamID && player->GetID() != exceptPlayerID) {
      PlayerImage newImage = playerImage;
      Vector3 extrapolation = playerImage.movement * GetTimeStampNeg_ms() * 0.001f;
      newImage.position = playerImage.position + extrapolation;
      newImage.position = newImage.position.EnforceMaximumDeviation(newImage.player->GetPosition(),
                                                                    maxDistanceDeviation);
      newImage.movement = newImage.movement.EnforceMaximumDeviation(newImage.player->GetMovement(),
                                                                    maxMovementDeviation);
      playerImages.push_back(newImage);
    }
  }
}

void MentalImage::UpdateBallPredictions() {
  ballPredictions = match->GetBall()->GetTrack();
}

Vector3 MentalImage::GetBallPredictionAtRecord(unsigned int time_ms, unsigned records) const {
  // The record the AI's own time reads, plus `records` more: the difference over one
  // PES tick is the ball's own speed, so a record difference times the tick's rate
  // in Hz is a velocity in m/s (Milestone A1).
  const unsigned index = pesball::TrackIndexForAiMs(
      static_cast<double>(time_ms) + timeStampNeg_ms, ballPredictionSize_ms);
  return pesball::TrackRecord(ballPredictions, index + records);
}

Vector3 MentalImage::GetBallPrediction(unsigned int time_ms) const {
  // The track is read the way PES's own readers read it: floor to a record. The live
  // ball steps on the same 1/54 s tick, so a record is the ball's actual state
  // (Milestone A1); the interpolated read that stood in while the ball ran on GF's
  // 10 ms integrator is gone.
  Vector3 mentalResult = pesball::TrackRecord(
      ballPredictions, pesball::TrackIndexForAiMs(static_cast<double>(time_ms) + timeStampNeg_ms,
                                                 ballPredictionSize_ms));
  // The match tears its ball down before the graphics thread has finished its
  // last put, so on the way out there is nothing real left to blend towards.
  if (!match->GetBall()) return mentalResult;
  Vector3 realResult = pesball::TrackRecord(
      match->GetBall()->GetTrack(),
      pesball::TrackIndexForAiMs(static_cast<double>(time_ms), ballPredictionSize_ms));

  // let there be a maximum difference between the two. why?
  // when a ball gets a wholly new movement, this prediction is obviously far off reality, while
  // some variables are not, like the player->gettimeneededtogettoball, since that is based on
  // non-delayed vars. a solution would be to have a reaction-time-corrected version of everything,
  // but that is, for now, too complicated. maybe one day rebuild the whole timeneeded/tactics
  // calculations system

  // ponytail: this 2.5 m blend has no PES counterpart; PES's MA ball copy is a plain
  // per-tick copy. Kept until the MA ball cluster (MB-B) settles what the AI reads.
  Vector3 result = mentalResult.EnforceMaximumDeviation(realResult, maxDistanceDeviation);

  return result;
}
