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

}  // namespace MatchProgression

#endif
