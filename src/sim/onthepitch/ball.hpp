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

#ifndef _HPP_FOOTBALL_ONTHEPITCH_BALL
#define _HPP_FOOTBALL_ONTHEPITCH_BALL

#include "../gamedefines.hpp"
#include "../utils.hpp"
#include "defines.hpp"
#include <array>

#include "pesball.hpp"
#include "pesballtrack.hpp"

using namespace blunted;

class Match;

struct BallSpatialInfo {
  BallSpatialInfo(const Vector3& momentum, const Quaternion& rotation_ms) {
    this->momentum = momentum;
    this->rotation_ms = rotation_ms;
  }
  Vector3 momentum;
  Quaternion rotation_ms;
};

class Ball {
public:
  Ball(Match* match);
  virtual ~Ball();

  // PES's track read at a GF time in milliseconds: one record per PES tick, floored
  // (Milestone A1). The live ball steps on that same tick, so this is the ball.
  Vector3 Predict(unsigned int predictTime_ms) const {
    return pesball::TrackAt(track, predictTime_ms);
  }
  // The prediction the AI copies per tick: PES's 250 records, one per PES tick.
  const std::array<Vector3, pesball::kTrackLength>& GetTrack() const { return track; }
  Vector3 GetMovement();
  void Touch(const Vector3& target);
  void SetPosition(const Vector3& target);
  void SetMomentum(const Vector3& target);
  void SetRotation(radian x, radian y, radian z,
                   float bias = 1.0);                      // radians per second for each axis
  void SetRotation(const Vector3& rot, float bias = 1.0);  // radians per second for each axis
  BallSpatialInfo CalculatePrediction();  // one PES tick of the live ball, plus the track
  Vector3 GetPositionBuffer() const { return positionBuffer; }

  bool BallTouchesNet() { return ballTouchesNet; }
  Vector3 GetAveragePosition(unsigned int duration_ms) const;

  // Weather control (roadmap 3.8). wind is an acceleration in m/s^2 applied to
  // the airborne ball; wetness is in [0, 1] and makes the pitch slicker.
  void SetWeather(const Vector3& wind, float wetness);
  Vector3 GetWeatherWind() const { return weatherWind; }
  float GetWeatherWetness() const { return weatherWetness; }

  void Process();

  // PES's dead ball at the whistle: zero the velocity in place, leaving the position for the
  // restart writer (which reads it at the stop frame). The track is rebuilt so predictions stop
  // flying; the prepare teleport still moves it to the spot.
  void Stop();

  void ResetSituation(const Vector3& focusPos);

protected:
  Vector3 momentum;
  Quaternion rotation_ms;

  // PES's prediction sampler, one record per PES tick (pesballtrack.hpp). Record 0
  // is where the ball is now and record k is k ticks of 1/54 s on.
  std::array<Vector3, pesball::kTrackLength> track;
  Quaternion orientPrediction;

  std::list<Vector3> ballPosHistory;
  Vector3 previousMomentum;
  Vector3 previousPosition;

  Vector3 positionBuffer;
  Quaternion orientationBuffer;

  Match* match;

  // PES 2021's ball physics, read from the install by the caller
  // (docs/re/pes-re-ball-contact.md, docs/re/pes-re-ball-constants.md). GF's own
  // bounce, drag and friction coefficients had no PES counterpart and are gone:
  // PES's vertical, tangential and spin response are all clamped interpolations
  // over `ball.o`'s mode columns, not single numbers.
  const pes::PesBallConstants* ballConstants = nullptr;

  // The ball's own contact-tick counter, the input the grounder kick's
  // `int(|x|*1000) mod gounnderprob[mode]` gate runs on.
  unsigned contactTicks = 0;
  // PES's recent-hit memory b (simBall+0x50, docs/re/pes-re-ball-collision.md
  // 3.1), and the value the last prediction's first step left for Process.
  float recentHit = 0.0f;
  float recentHitNext = 0.0f;

  Vector3 weatherWind;
  float weatherWetness;

  bool ballTouchesNet;
};

#endif
