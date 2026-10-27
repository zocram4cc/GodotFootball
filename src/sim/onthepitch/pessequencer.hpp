#pragma once

// PES 2021's referee restart sequencer as pure functions over the install's constants. The
// source is the A5 judge decode (docs/re/pes-re-A5Judge.md, with its live 9 corrections): the
// end-line restart (FUN_14050e020), the restart-spot writer (FUN_14050e770), the programme
// builder (FUN_1404526a0) and runner (FUN_14044c880), and the taker release gates (4.7, live 9.3).
// The referee uses these when pes_loop is on; the GF path keeps its own timings.

#include <vector>

#include "../gamedefines.hpp"
#include "pesstoppage.hpp"

namespace pes {

struct Spot {
  float x = 0.0f;
  float z = 0.0f;
};

// A ball out over an end line (FUN_14050e020). `lastTouchSide` is the last toucher's
// Team::GetSide(), which points at his own goal line: a ball out at his own end is a corner, and
// a ball out at the end he attacks is a goal kick (PES: goal kick iff dir * x >= 0).
e_SetPiece EndLineRestart(int lastTouchSide, float ballX);

// The team awarded a ball out is the one that did not touch it last (side = (T == 0)).
int AwardedTeamAfterBallOut(int lastTouchTeam);

// Where the restart is taken from. The corner is the flag pulled in by the install's insets; a
// goal kick sits on the goal-area line; a throw-in sits outside the touchline, in the install's
// writer form.
Spot CornerSpot(float ballX, float ballZ, const RestartSpotConstants& k);
Spot GoalKickSpot(float ballX, float ballZ, const RestartSpotConstants& k);
Spot ThrowInSpot(float ballX, float ballZ, const RestartSpotConstants& k);
// The throw-in carry spot (FUN_140791ec0 phase 1, docs/re/pes-re-throwin-choreography.md 3.2): where the
// taker lifts the ball, on the carry line outside the touchline. It lies on the line from the restart
// point to the centre of the goal the restarting side attacks (attackSign: +1 or -1), its x stays within
// the clamp of the restart's x, and it falls back to the restart's x near the end line. The restart
// point's z is a touchline's z, so it is never 0.
Spot ThrowInCarrySpot(float restartX, float restartZ, int attackSign, const RestartSpotConstants& k);

// The taker may release once this many frames have passed since the restart was armed. A kick-off and a goal kick are
// not timed (the receiver's ball test, or the goal kick's ladder in pesgoalkick.hpp): their gate is zero.
int ReleaseFrames(e_SetPiece kind);
// A throw-in is released once its taker has arrived (`takerArrived`): PES releases it on the taker's arrival and kick
// action (docs/re/pes-re-A5Judge.md 9.3; A5 §4.7). A goal kick is never released by this gate.
bool MayRelease(e_SetPiece kind, int framesSinceArmed, bool takerArrived);

// A restart programme (decode 3.2): the stoppage beats in the order they run, from the whistle
// to the armed restart. Hold is the players' hold; Wipe prepares the restart; UiAndTeleport is
// the stop stamp, UI window and teleport; FadeHalf or FadeIn is the fade-in; Tail is 0x34/0x19.
enum class Beat { Hold, Wipe, UiAndTeleport, FadeHalf, FadeIn, Tail };
std::vector<Beat> BuildProgramme(e_SetPiece kind);

struct Timing {
  unsigned long prepareAt_ms = 0;  // the wipe has run: the taker may be placed
  unsigned long readyAt_ms = 0;    // the set-play ready state (MI+0x116c = 3): after the UI and the teleport window
  unsigned long armedAt_ms = 0;    // the programme has reached the armed step
};

// Runs the beats from the whistle at `stopTime_ms`. `holdKind` is the hold table index: 1 for a
// foul (table[1]), 5 for an out-of-play stop (table[5]); a downed player adds his extra hold.
Timing RunProgramme(const std::vector<Beat>& beats, unsigned long stopTime_ms,
                    const StoppageConstants& k, bool downed, int holdKind);

}  // namespace pes
