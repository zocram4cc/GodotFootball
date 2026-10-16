// Match phase progression: what follows the whistle at the end of a period.
// Extra time is only played when the match is level at full time, both extra
// periods are always completed, and penalties only follow a draw after them.
// See TECHNICAL_ROADMAP.md checklist item 1.

#ifndef _HPP_MATCH_PROGRESSION
#define _HPP_MATCH_PROGRESSION

#include "../gametypes.hpp"

namespace MatchProgression {

struct Outcome {
  bool gameOver = false;
  e_MatchPhase nextPhase = e_MatchPhase_1stHalf;
};

Outcome GetNext(e_MatchPhase currentPhase, bool scoresLevel);

// Does this period change ends? Team::GetSide() flips for the second half and the
// second period of extra time and for nothing else, so these are the two moments
// both teams must be set out again on the ends they have just swapped to. Half time
// never did it: the referee prepares the kickoff and only then fires the phase
// change, so the marks belonged to the ends the teams were leaving.
inline bool SwapsEnds(e_MatchPhase phase) {
  return phase == e_MatchPhase_2ndHalf || phase == e_MatchPhase_2ndExtraTime;
}

}  // namespace MatchProgression

#endif
