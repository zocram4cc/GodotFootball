#include "onthepitch/pespossession.hpp"

namespace pes {
namespace {

// FUN_1404fe9c0 (0x1404fe9c0) lines 17-26: the restart state MI+0x116c.
constexpr int kRestartStateTwo = 2;
constexpr int kRestartStateThree = 3;
// FUN_140a3c950 (0x140a3c950): at state 2, the gate is open when MI+0x1574 == 1.
constexpr int kRestartGateOpen = 1;
// FUN_1404fe9c0 step 2: the window is gs / 6 frames (`uVar9 / 6`).
constexpr int kStepTwoDivisor = 6;
// FUN_1404fe9c0 step 3: the cadence is (frame - newest.frame - 2) % 5 == 0.
constexpr unsigned kCadenceOffset = 2;
constexpr unsigned kCadenceModulus = 5;
// Numbered kinds (E+0x180698 at +0). Kicks 1..3 and shot 5 are OBS in pesreceive.hpp:88;
// restart 0x10 and touch 7 are INF (the decode's names, not read from the ring writer).
// Touch 7 matters for step 2: a plain touch written as a deciding kind would make step 2
// take that touch's actor.
constexpr int kKindKickFirst = 1;
constexpr int kKindKickLast = 3;
constexpr int kKindRestart = 0x10;
constexpr int kKindShot = 5;
// Step 3's sub-3 exclusion: kind 3 with sub-kind (+0x40) 3 or 4 (FUN_1404fe9c0).
constexpr int kKindThree = 3;
constexpr int kSubKindFirst = 3;
constexpr int kSubKindLast = 4;
// Step 3a: the raw MI+0x119c sum below 3 * gs, and the flagged newest sub-kind
// (+0x1c) in 3..5 (FUN_1404fe9c0 3a).
constexpr uint32_t kSumGameSpeedMultiple = 3;
constexpr int kSubKindOneCFirst = 3;
constexpr unsigned kSubKindOneCSpan = 3;

}  // namespace

bool TeamPossessionLatch(const PossessionLatchInput& in) {
  // Rule R first: restart state 3, or state 2 with the gate shut.
  const bool restartRule =
      in.restartState == kRestartStateThree ||
      (in.restartState == kRestartStateTwo && in.restartGate != kRestartGateOpen);
  if (restartRule) return in.restartTeam == in.team;

  // Both ranked heads must be valid, or the value is kept.
  if (!in.ownHeadValid || !in.otherHeadValid) return in.old;

  // Step 1: a carrier decides.
  if (in.carrierValid) return in.carrierSide == in.team;

  // Step 2: inside the window, a step-2 kind decides by its actor's side.
  const unsigned age = static_cast<unsigned>(in.frame - in.newestFrame);
  const unsigned window = static_cast<unsigned>(in.gs / kStepTwoDivisor);
  if (age < window) {
    const bool kindDecides = (in.newestKind >= kKindKickFirst && in.newestKind <= kKindKickLast) ||
                             in.newestKind == kKindRestart;
    if (kindDecides && in.actorValid) return in.actorSide == in.team;
    return in.old;
  }

  // Step 3: only on the newest event's cadence, and not for a shot, nor a sub-3 kick.
  if ((age - kCadenceOffset) % kCadenceModulus != 0) return in.old;
  if (in.newestKind == kKindShot) return in.old;
  if (in.newestKind == kKindThree && in.newestSubKind >= kSubKindFirst &&
      in.newestSubKind <= kSubKindLast && !in.stateFlag40_80) {
    return in.old;
  }

  // 3a: a small raw sum and a flagged newest record decide by the actor's side.
  // PES parses an invalid actor as side 0.
  if (in.ballSumRaw < kSumGameSpeedMultiple * static_cast<uint32_t>(in.gs) && in.newestFlag2d &&
      static_cast<unsigned>(in.newestField1c - kSubKindOneCFirst) < kSubKindOneCSpan) {
    const int side = in.actorValid ? in.actorSide : 0;
    return side == in.team;
  }

  // 3b: the holder decides once the chase is off, with its team in possession or
  // the other head's flag clear.
  if (!in.chaseBlocked && in.holderValid && (in.holderTeamLatch || in.otherHeadFlagClear)) {
    return in.holderSide == in.team;
  }

  return in.old;
}

int PesFrameOf(uint64_t ms, int gs) {
  return static_cast<int>(ms * static_cast<uint64_t>(gs) / 1000u);
}

}  // namespace pes
