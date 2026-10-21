#include "penaltyshootout.hpp"

#include <algorithm>

namespace PenaltyShootout {

namespace {

int Other(int team) {
  return team == 0 ? 1 : 0;
}

int RemainingRegulationKicks(const State& state, int team) {
  return std::max(0, regulationKicks - state.taken[team]);
}

}  // namespace

KickAim AimPenaltyKick(const pes::PenaltyScatterConstants& k, pes::Random& rng,
                       const pes::PenaltyScatterInput& taker) {
  // ponytail: the base height is bar/2. PES's FUN_1408abc30 gives (bar - (tune[+0x2C] * C +
  // geom[+0x3C] * 0.5)) * 0.5, and the kind-54 tuning entry and geom +0x3C are runtime values
  // not yet read. The lateral is zero: the penalty branch of FUN_1408b7550 passes a zero
  // lateral scale to FUN_1408abc30.
  constexpr float kBaseHeightShare = 0.5f;
  pes::PenaltyScatterInput base = taker;
  base.aimLateral = 0.0f;
  base.aimHeight = taker.barScale * kBaseHeightShare;

  KickAim aim;
  aim.radius_m = pes::PenaltyScatterRadius(k, base);
  const pes::PenaltyAim jittered = pes::ApplyPenaltyJitter(
      rng, k, aim.radius_m, pes::PenaltyAim{base.aimLateral, base.aimHeight});
  aim.lateral_m = jittered.lateral_m;
  aim.height_m = jittered.height_m;
  return aim;
}

e_Outcome ObserveKick(const KickObservation& observation, unsigned long elapsedSinceKick_ms) {
  // The net is the last word, even if the keeper got a hand to it.
  if (observation.goalDetected)
    return e_Outcome_Goal;
  if (observation.keeperHasBall)
    return e_Outcome_Save;
  // The ball's fate is settled once it has stopped, gone out, or the kick has run
  // out of time. Until then a keeper touch is not yet a verdict: the ball may still
  // go in off him, and that is a goal.
  const bool settled = observation.ballLeftField || observation.ballStopped ||
                       elapsedSinceKick_ms >= kickTimeout_ms;
  if (!settled)
    return e_Outcome_Pending;
  // Kept out by the keeper: his touch stands however the ball then went, including a
  // deflection that rolled past the kick's time limit.
  if (observation.keeperTouchedBall)
    return e_Outcome_Save;
  return e_Outcome_Miss;
}

unsigned long GetPhaseDuration_ms(e_Phase phase) {
  switch (phase) {
    case e_Phase_Positioning:
      return positioningDuration_ms;
    case e_Phase_Execution:
      return executionDuration_ms;
    case e_Phase_Resolution:
      return resolutionDuration_ms;
    default:
      return 0;
  }
}

int SelectTakerIndex(const std::vector<float>& shotRatings, unsigned int takenMask) {
  const int count = static_cast<int>(shotRatings.size());
  if (count == 0)
    return -1;

  // Everyone has taken one already: start a fresh round rather than nobody.
  const unsigned int allTaken = count >= 32 ? ~0u : (1u << count) - 1u;
  if ((takenMask & allTaken) == allTaken)
    takenMask = 0;

  int best = -1;
  for (int i = 0; i < count; i++) {
    if (takenMask & (1u << i))
      continue;
    if (best == -1 || shotRatings[i] > shotRatings[best])
      best = i;
  }
  return best;
}

unsigned int MarkTaken(unsigned int takenMask, int index, int candidateCount) {
  if (index < 0 || candidateCount <= 0 || index >= candidateCount)
    return takenMask;

  takenMask |= 1u << index;
  const unsigned int allTaken = candidateCount >= 32 ? ~0u : (1u << candidateCount) - 1u;
  return (takenMask & allTaken) == allTaken ? 0u : takenMask;
}

State Create(int firstShootingTeam) {
  State state;
  state.shootingTeam = firstShootingTeam == 1 ? 1 : 0;
  return state;
}

void BeginKick(State& state) {
  if (state.phase != e_Phase_Positioning)
    return;
  state.phase = e_Phase_Execution;
  state.lastOutcome = e_Outcome_Pending;
}

void ApplyOutcome(State& state, e_Outcome outcome) {
  if (state.phase != e_Phase_Execution)
    return;

  state.taken[state.shootingTeam]++;
  if (outcome == e_Outcome_Goal)
    state.score[state.shootingTeam]++;

  state.lastOutcome = outcome;
  state.phase = e_Phase_Resolution;
}

void NextKick(State& state) {
  if (state.phase != e_Phase_Resolution)
    return;

  if (IsDecided(state)) {
    state.phase = e_Phase_Finished;
    return;
  }

  // Both teams have taken this round's kick: on to the next one.
  if (state.taken[0] == state.taken[1])
    state.round++;

  state.shootingTeam = Other(state.shootingTeam);
  state.phase = e_Phase_Positioning;
}

bool IsDecided(const State& state) {
  return GetWinner(state) != -1;
}

bool IsSuddenDeath(const State& state) {
  return state.taken[0] >= regulationKicks && state.taken[1] >= regulationKicks &&
         !IsDecided(state);
}

int GetWinner(const State& state) {
  const bool regulationComplete =
      state.taken[0] >= regulationKicks && state.taken[1] >= regulationKicks;

  if (!regulationComplete) {
    // A lead bigger than what the trailing team can still salvage from its
    // remaining kicks ends the shootout early.
    for (int team = 0; team < 2; team++) {
      const int opponent = Other(team);
      if (state.score[team] > state.score[opponent] + RemainingRegulationKicks(state, opponent))
        return team;
    }
    return -1;
  }

  // Regulation over: both after the fifth kick and in sudden death, a lead only
  // counts once both teams have taken the same number of kicks.
  if (state.taken[0] == state.taken[1] && state.score[0] != state.score[1])
    return state.score[0] > state.score[1] ? 0 : 1;

  return -1;
}

}  // namespace PenaltyShootout
