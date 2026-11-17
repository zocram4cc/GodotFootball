#include "matchprogression.hpp"

namespace MatchProgression {

Outcome GetNext(e_MatchPhase currentPhase, bool scoresLevel, const Options& options) {
  Outcome outcome;
  outcome.nextPhase = currentPhase;

  switch (currentPhase) {
    case e_MatchPhase_1stHalf:
      outcome.nextPhase = e_MatchPhase_2ndHalf;
      break;

    case e_MatchPhase_2ndHalf:
      // PES's tie test (FUN_1404493D0): a level match takes extra time when that option is on,
      // else the shoot-out when it is on, else full time.
      if (!scoresLevel)
        outcome.gameOver = true;
      else if (options.extraTime)
        outcome.nextPhase = e_MatchPhase_1stExtraTime;
      else if (options.penaltyShootout)
        outcome.nextPhase = e_MatchPhase_Penalties;
      else
        outcome.gameOver = true;
      break;

    case e_MatchPhase_1stExtraTime:
      // Just the turn-around in extra time; both periods are always played.
      outcome.nextPhase = e_MatchPhase_2ndExtraTime;
      break;

    case e_MatchPhase_2ndExtraTime:
      // PES's tie test (FUN_1404499D0): a level match goes to the shoot-out when that option is on.
      if (scoresLevel && options.penaltyShootout)
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
