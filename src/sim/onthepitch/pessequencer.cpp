#include "pessequencer.hpp"

#include <algorithm>
#include <cmath>

#include "foulsequence.hpp"
#include "gametypes.hpp"

namespace pes {

namespace {

// (Throw-in offsets moved to RestartSpotConstants: read from the install, not transcribed.)

// The frame gates are PES's time-out, not its trigger: the release is the taker reaching the
// ball (A5Live 9.2/9.4; MayRelease), and these bounds only fire when he never gets there. Each
// value is the shortest release measured live; the DOC gate it replaces is named beside it.
constexpr int kThrowInReleaseFrames = 118;   // live 118-194; DOC counter 81-83 not reproduced
constexpr int kGoalKickReleaseFrames = 238;  // live 238-253; DOC counter 83 refuted, 246 inside
constexpr int kCornerReleaseFrames = 365;    // live 365-379; DOC 189 not reproduced
constexpr int kFreeKickReleaseFrames = 355;  // live 355-356; DOC 162 not reproduced
constexpr int kPenaltyReleaseFrames = 54;    // DOC fps*1 (setpiece 12); not observed live
// The kick-off's forced rule (throwin doc 6.2): past counter 972 = 18 s at 54 Hz the restart is
// forced, and the same rule covers kick-offs. A named frame count with its provenance: PES's
// fps times 18, not a transcribed table.
constexpr int kKickOffForcedFrames = 54 * 18;

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

int ReleaseFrames(e_SetPiece kind) {
  switch (kind) {
    case e_SetPiece_ThrowIn:
      return kThrowInReleaseFrames;
    case e_SetPiece_GoalKick:
      return kGoalKickReleaseFrames;
    case e_SetPiece_Corner:
      return kCornerReleaseFrames;
    case e_SetPiece_FreeKick:
      return kFreeKickReleaseFrames;
    case e_SetPiece_Penalty:
      return kPenaltyReleaseFrames;
    default:
      // Kick-off: PES has no timer here, only the receiver test and the forced rule (throwin
      // doc 6.2: past counter 972 = 54*18 frames the restart is forced). The gate below is
      // that bound, not a wait.
      return kKickOffForcedFrames;
  }
}

bool MayRelease(e_SetPiece kind, int framesSinceArmed, bool takerReached, float reach_m,
                float takerBallDistance_m) {
  // The trigger is the taker reaching the ball (A5Live 9.2); the kick-off receiver owning it is
  // the same check (setpiece doc 3.3, KickoffReceiver 0x140781fc0). The frame gate is only the
  // time-out both paths keep (armed handler FUN_14044d2e0, and the forced rule for kick-offs).
  if (takerReached || takerBallDistance_m <= reach_m) return true;
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
