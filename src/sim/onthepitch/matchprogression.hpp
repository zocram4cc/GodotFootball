// Match phase progression: what follows the whistle at the end of a period.
// Extra time and the shoot-out are PES's match options (its E+0x66 and E+0x67 flags). A level
// second half goes to extra time when that option is on, else to the shoot-out when it is on,
// else to full time. Both extra periods are always completed; a level match after them goes to
// the shoot-out when that option is on, else to full time.
// See TECHNICAL_ROADMAP.md checklist item 1.

#ifndef _HPP_MATCH_PROGRESSION
#define _HPP_MATCH_PROGRESSION

#include "../gametypes.hpp"

namespace MatchProgression {

struct Options {
  bool extraTime = false;        // PES E+0x66: a level second half plays extra time
  bool penaltyShootout = false;  // PES E+0x67: a level match (after extra time, if played) goes to the shoot-out
};

struct Outcome {
  bool gameOver = false;
  e_MatchPhase nextPhase = e_MatchPhase_1stHalf;
};

Outcome GetNext(e_MatchPhase currentPhase, bool scoresLevel, const Options& options);

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
