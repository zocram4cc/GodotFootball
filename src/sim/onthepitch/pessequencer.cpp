#include "pessequencer.hpp"

#include <algorithm>
#include <cmath>

#include "foulsequence.hpp"
#include "gametypes.hpp"

namespace pes {

namespace {

// A throw-in's z sits F1 + the install's line outset outside the touchline - FUN_14050d280
// computes the point (R = ball.x, sign(ball.z) * (F1 + 0.25)) - and its x stays at most F0 minus
// the install's end inset, FUN_14050e770's own clamp (docs/re/pes-re-throwin-choreography.md 1.3;
// both values from the install, RestartSpotConstants).

// ponytail: frames from arming to release, live (A5Live 9.3). These fixed gates stand in for PES's taker reach and kick
// action (9.2, 9.4) until that action is ported. Each value is the shortest release measured live; the DOC gate it
// replaces is named beside it. A throw-in has no gate: its taker is released on arrival (MayRelease).
constexpr int kCornerReleaseFrames = 365;    // live 365-379; DOC 189 not reproduced
constexpr int kFreeKickReleaseFrames = 355;  // live 355-356; DOC 162 not reproduced
constexpr int kPenaltyReleaseFrames = 54;    // DOC fps*1 (setpiece 12); not observed live

float Sign(float v) { return v < 0.0f ? -1.0f : 1.0f; }

unsigned long ToMs(float seconds) {
  return static_cast<unsigned long>(std::lround(seconds * 1000.0f));
}

}  // namespace

e_SetPiece EndLineRestart(int lastTouchSide, float ballX) {
  return lastTouchSide * ballX > 0.0f ? e_SetPiece_Corner : e_SetPiece_GoalKick;
}

int AwardedTeamAfterBallOut(int lastTouchTeam) { return lastTouchTeam == 0 ? 1 : 0; }

Spot CornerSpot(float ballX, float ballZ, const RestartSpotConstants& k) {
  return {Sign(ballX) * (pesFieldHalfLength - k.cornerInsetX_m),
          Sign(ballZ) * (pesFieldHalfWidth - k.cornerInsetZ_m)};
}

Spot GoalKickSpot(float ballX, float ballZ, const RestartSpotConstants& k) {
  return {Sign(ballX) * (pesGoalAreaLineX + k.goalAreaPushOut_m), Sign(ballZ) * k.goalAreaPushZ_m};
}

Spot ThrowInSpot(float ballX, float ballZ, const RestartSpotConstants& k) {
  const float endX = pesFieldHalfLength - k.throwInEndInset_m;
  return {std::clamp(ballX, -endX, endX),
          Sign(ballZ) * (pesFieldHalfWidth + k.throwInLineOutset_m)};
}

Spot ThrowInCarrySpot(float restartX, float restartZ, int attackSign, const RestartSpotConstants& k) {
  const float carryZ = Sign(restartZ) * (pesFieldHalfWidth + k.throwInCarryOutset_m);
  // The approach line runs from the restart point to the centre of the goal the side attacks,
  // G = (attackSign * F0, 0) (FUN_140791ec0 phase 1). Where it crosses the carry line, the x is
  // G.x - carryZ * (G.x - restartX) / restartZ.
  const float goalX = static_cast<float>(attackSign) * pesFieldHalfLength;
  float x = goalX - carryZ * (goalX - restartX) / restartZ;
  // The x stays within the install's clamp of the restart's x (the 2 m test, COMISS at 0x1407921c2).
  x = std::clamp(x, restartX - k.throwInCarryClamp_m, restartX + k.throwInCarryClamp_m);
  // Past the end line's inset the spot falls back to the restart's x (the F0 - 0.5 test, 0x1407921f3).
  if (std::fabs(x) > pesFieldHalfLength - k.throwInCarryEndInset_m) x = restartX;
  return {x, carryZ};
}

int ReleaseFrames(e_SetPiece kind) {
  switch (kind) {
    case e_SetPiece_ThrowIn:
      return 0;  // no timer: the taker's arrival releases it (MayRelease)
    case e_SetPiece_Corner:
      return kCornerReleaseFrames;
    case e_SetPiece_FreeKick:
      return kFreeKickReleaseFrames;
    case e_SetPiece_Penalty:
      return kPenaltyReleaseFrames;
    default:
      return 0;  // kick-off and goal kick: no clock (the receiver's ball test, or the goal kick's ladder; pesgoalkick.hpp)
  }
}

bool MayRelease(e_SetPiece kind, int framesSinceArmed, bool takerArrived) {
  if (kind == e_SetPiece_GoalKick) return false;  // the goal kick's ladder releases it (pesgoalkick.hpp), never this gate
  if (kind == e_SetPiece_ThrowIn) return takerArrived;  // PES's arrival and kick (A5 9.3, §4.7)
  return framesSinceArmed >= ReleaseFrames(kind);
}

std::vector<Beat> BuildProgramme(e_SetPiece kind) {
  // Every stoppage holds, wipes, runs its UI and teleport, then the tail. The fade-in is 0xd (half
  // a second) for a throw-in and a goal kick, and 0xe (a second) for the rest (decode 4.2-4.6).
  const bool halfSecondFade = kind == e_SetPiece_ThrowIn || kind == e_SetPiece_GoalKick;
  return {Beat::Hold, Beat::Wipe, Beat::UiAndTeleport,
          halfSecondFade ? Beat::FadeHalf : Beat::FadeIn, Beat::Tail};
}

Timing RunProgramme(const std::vector<Beat>& beats, unsigned long stopTime_ms,
                    const StoppageConstants& k, bool downed, int holdKind) {
  Timing timing;
  unsigned long at_ms = stopTime_ms;
  for (const Beat beat : beats) {
    switch (beat) {
      case Beat::Hold: {
        const float hold_s = (holdKind == 1 ? k.holdKind1_s : k.holdKind5_s) +
                             (downed ? k.downedPlayerExtra_s : 0.0f);
        at_ms += ToMs(hold_s);
        break;
      }
      case Beat::Wipe:
        at_ms += ToMs(k.wipe_s);
        timing.prepareAt_ms = at_ms;
        break;
      case Beat::UiAndTeleport:
        at_ms += FoulSequence::Tail_ms();
        break;
      case Beat::FadeHalf:
        at_ms += ToMs(k.fadeHalf_s);
        break;
      case Beat::FadeIn:
        at_ms += ToMs(k.fadeIn_s);
        break;
      case Beat::Tail:
        at_ms += ToMs(k.restartTail_s);
        break;
    }
  }
  timing.armedAt_ms = at_ms;
  return timing;
}

}  // namespace pes
