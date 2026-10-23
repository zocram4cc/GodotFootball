#include "pesteamtick.hpp"

#include <algorithm>

namespace pes {
namespace {
// FUN_1404FA840's in-play arm: 8 to T+0xB8A8 (EBP = 8 at 0x1404FA853, stored at 0x1404FA870) and 4 to T+0xB8AC
// (0x1404FA87D).
constexpr int kInPlayB8A8 = 8;
constexpr int kInPlayB8AC = 4;
}  // namespace

int StrategyWantLevel(int strategy) {
  // The jump ladder at 0x1404F8619-63E: 0->0, 1->1, 3->3, 4->4, else 2.
  switch (strategy) {
    case 0:
    case 1:
    case 3:
    case 4:
      return strategy;
    default:
      return 2;
  }
}

int ManualOverrideLevel(int want, int manual, int level) {
  // The mesh at 0x1404F8640-6C8. Balanced manual never overrides, and a
  // manual that already equals the level leaves the want alone (0x1404F8663).
  if (manual == 2 || manual == level) return want;
  if (manual < 2) {
    if (manual <= want && want < 3) return manual;
  } else if (2 <= want && want <= manual) {
    return manual;
  }
  return want;
  // ponytail: the dead-ball crossing case past 0x1404F8675 (helpers
  // FUN_140A606C0, FUN_140A3FE50, MI+0x116C == 2) is not decoded; off in-play
  // the freeze below holds the level, so the arm is unreachable here.
}

void DriftAttackLevel(TeamTick* tick, int strategy, int manual, bool inPlay, int frame) {
  const int old = tick->b8a4;
  int want = ManualOverrideLevel(StrategyWantLevel(strategy), manual, old);
  // The freeze at 0x1404F86C8-6D4: off in-play the old level stands.
  if (want != old && !inPlay) want = old;
  // The clamp at 0x1404F86DE-E7: min(want, 4). No low clamp in PES.
  tick->b8a4 = std::min(want, 4);
  // The stamp at 0x1404F86ED-70C: MI+0x1198 on change only.
  if (want != old) tick->b8c0Frame = frame;
}

void WriteAttackFlags(TeamTick* tick) {
  // The inline rule at 0x1404F6E1C-58, in PES order.
  tick->b8a2 = tick->b8a4 > 3;
  tick->b8a3 = tick->b8a4 < 1;
}

void WriteRestartWord(TeamTick* tick, bool inPlay, bool possessionFlipped) {
  // Step 28 at 0x1404F6F91: MI+0x116C != 1, or the latch return (old^new
  // 0x255) nonzero.
  if (!inPlay || possessionFlipped) tick->t80 = 0xFF;
  // ponytail: T+0x6E6 = 0 (gated on FUN_140A3E190(...) != 5) has no GF
  // counterpart; the predicate is not decoded.
}

void ArmInPlayCounters(TeamTick* tick) {
  tick->b8a8 = kInPlayB8A8;
  tick->b8ac = kInPlayB8AC;
  tick->counterDone = false;  // MOV byte [RCX+0x6D44],0 at 0x1404FA887
}

}  // namespace pes
