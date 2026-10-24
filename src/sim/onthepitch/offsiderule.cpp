#include "offsiderule.hpp"

#include <algorithm>
#include <cmath>

namespace OffsideRule {

float OffsideLineX(int defendingSide, const std::vector<blunted::Vector3>& defenderCentres,
                   float ballX, float halfLength_m, const pes::JudgeConstants& k) {
  const float s = static_cast<float>(defendingSide);
  if (s == 0.0f) return 0.0f;

  // PES 2021's line (report 3), in the defending team's own attack frame with
  // its own goal at -halfLength: the second-rearmost defender's rear surface,
  // capped by the ball and by halfway, clamped to the goal line. Both running
  // minima start at 0, so a defender beyond halfway never deepens the line, and
  // fewer than two eligible defenders makes the second one the goal line.
  float rearmost = 0.0f;
  float second = 0.0f;
  int count = 0;
  for (const blunted::Vector3& centre : defenderCentres) {
    // The rear surface: the centre shifted towards his own goal by his torso
    // radius (PES: x + s*(-r), its constant already the negated radius), which
    // in this frame is one shift because s*s = 1.
    // ponytail: no per-limb points; arms and toes not modelled - the torso
    // shift stands in for every body point's, so a defender's rear surface is
    // his centre plus the torso shift and no deeper.
    const float v = s * centre.coords[0] + k.offsideRearShiftTorso_m;
    ++count;
    if (v < rearmost) {
      second = rearmost;
      rearmost = v;
    } else if (v < second) {
      second = v;
    }
  }
  // Fewer than two eligible defenders makes the second one the goal line, and
  // the ball caps the line from there. The halfway cap is not a separate step:
  // both running minima start at 0 and only ever go down, so `second` is never
  // positive - PES's `second > 0 -> 0` is the same invariant, stated again.
  if (count < 2) second = -halfLength_m;
  second = std::min(second, s * ballX);
  if (std::fabs(second) > halfLength_m) second = -halfLength_m;
  return s * second;
}

int OffsideCategory(float attackerCentreX, float lineX, int attackingSide,
                    const pes::JudgeConstants& k) {
  const float s = static_cast<float>(attackingSide);

  // PES 2021's attacker test (report 4): s*P.x + m > s*L + tol, with P the
  // leading body point and m its surface margin, and the category from
  // d = |P.x - L| at that point's centre.
  // ponytail: GF has centre positions only - no per-limb points; arms and toes
  // are not modelled. The torso point stands in for every body point, so the
  // leading point is the centre shifted by the torso margin (its margin is
  // already inside that surface) and no arm ever leads it: the 0.30 m tolerance
  // cannot apply, and the centre itself must pass the line, exactly as PES's
  // own head/torso point does. What this misses is PES's per-limb strictness:
  // a toe leading gives it 0.05 m, an elbow 0.24 m, a hand tip 0.30 m.
  const float leadingX = attackerCentreX + s * k.offsideMarginTorso_m;
  // The non-arm tolerance is PES's own shared float with the torso margin.
  const float tolerance = k.offsideMarginTorso_m;
  const float d = std::fabs(attackerCentreX - lineX);

  if (s * leadingX > s * lineX + tolerance)
    return d < k.offsideNearBand_m ? 2 : (d <= k.offsideClearBand_m ? 1 : 0);
  return d < k.offsideNearBand_m ? 3 : -1;
}

bool OffsideBlocksChase(int category) {
  // PES 2021's ball-list reader FUN_14045e0d0 (docs/re/pes-re-offside.md 4):
  // the score of a player offside-flagged in category 0 or 1 is the impossible
  // one, so he never appears in his team's ball list.
  return category >= 0 && category < 2;
}

bool CanCreateOffside(e_SetPiece restart) {
  // PES 2021's gate (docs/re/pes-re-offside.md 2.2): the delivery of its staged
  // set 1..4 - a kick-off, a throw-in, a goal kick or a corner - never arms a
  // flag. A free kick or a penalty can put its receivers offside.
  // Kind 7 (the period end) is taken as a kick-off (docs/re/pes-re-plan-selector.md §4), so it never arms one either.
  return restart != e_SetPiece_KickOff && restart != e_SetPiece_PeriodEnd && restart != e_SetPiece_ThrowIn &&
         restart != e_SetPiece_GoalKick && restart != e_SetPiece_Corner;
}

bool ShouldSnapshot(bool inPlay, bool restartQueued, e_SetPiece queuedRestart) {
  if (!inPlay)
    return false;
  if (restartQueued && !CanCreateOffside(queuedRestart))
    return false;
  return true;
}

bool IsDeliberatePlay(e_TouchType touchType) {
  return touchType == e_TouchType_Intentional_Kicked;
}

bool TouchResetsPhase(bool opponentOfFlagged, e_TouchType touchType) {
  if (!opponentOfFlagged)
    return true;
  return IsDeliberatePlay(touchType);
}

blunted::Vector3 RestartPosition(const blunted::Vector3& spot, int attackSign, e_SetPiece restart,
                                 const pes::RestartSpotConstants& k) {
  // PES's restart-spot writer FUN_14050e770 (VA 0x14050e770; TouchReach's decode), explicit-spot
  // path, in the writer's order. The offsets come from the install (k); F0, F1 and F4 are
  // gametypes.hpp's FieldInfo constants.
  const auto sign = [](float v) { return v < 0.0f ? -1.0f : 1.0f; };
  blunted::Vector3 out = spot;
  float& x = out.coords[0];
  float& z = out.coords[1];
  // A spot inside the restart team's own goal area is pushed out of it.
  if (std::fabs(z) < pesGoalAreaHalfWidth && attackSign * x < -pesGoalAreaLineX) {
    x = sign(x) * (pesGoalAreaLineX + k.goalAreaPushOut_m);
    z = sign(z) * k.goalAreaPushZ_m;
  }
  // Clamped inside the outer lines, with no margin.
  if (std::fabs(x) > pesFieldHalfLength) x = sign(x) * pesFieldHalfLength;
  if (std::fabs(z) > pesFieldHalfWidth) z = sign(z) * pesFieldHalfWidth;
  // A free kick is kept inside the touchline by the writer's inset.
  if (restart == e_SetPiece_FreeKick && std::fabs(z) > pesFieldHalfWidth - k.freeKickInset_m)
    z = sign(z) * (pesFieldHalfWidth - k.freeKickInset_m);
  return out;
}

}  // namespace OffsideRule
