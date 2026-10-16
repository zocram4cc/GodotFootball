#include "matchprogression.hpp"

namespace MatchProgression {

Outcome GetNext(e_MatchPhase currentPhase, bool scoresLevel) {
  Outcome outcome;
  outcome.nextPhase = currentPhase;

  switch (currentPhase) {
    case e_MatchPhase_1stHalf:
      outcome.nextPhase = e_MatchPhase_2ndHalf;
      break;

    case e_MatchPhase_2ndHalf:
      // Extra time is only played when there is still nothing between them.
      if (scoresLevel)
        outcome.nextPhase = e_MatchPhase_1stExtraTime;
      else
        outcome.gameOver = true;
      break;

    case e_MatchPhase_1stExtraTime:
      // Just the turn-around in extra time; both periods are always played.
      outcome.nextPhase = e_MatchPhase_2ndExtraTime;
      break;

    case e_MatchPhase_2ndExtraTime:
      if (scoresLevel)
        outcome.nextPhase = e_MatchPhase_Penalties;
      else
        outcome.gameOver = true;
      break;

    default:
      // The shootout controller decides when the penalties phase is over.
      break;
  }

  return outcome;
}

}  // namespace MatchProgression
