// PES 2021's per-tick team record words (Slice A): the attack level T+0xB8A4,
// its flags T+0xB8A2/B8A3, and the restart word T+0x80 (team-layer §1 steps
// 13/18/28, §2 V1). Pure functions over a small struct; the controller owns
// one per side and steps it every team tick on the PES path.

#pragma once

#include <cstdint>

namespace pes {

// One side's per-tick team words. `b8a4` is the attack level 0..4
// (FUN_1404F8350); `b8a2`/`b8a3` the full-attack/full-defence flags
// (FUN_1404F6C30 inline at 0x1404F6E1C-58); `t80` the restart word
// (0x1404F6F91); `b8c0Frame` the stamp of the last level change (T+0xB8C0).
struct TeamTick {
  int b8a4 = 2;  // init FUN_140A415A0 -> 2 (tactics §3)
  bool b8a2 = false;
  bool b8a3 = false;
  uint32_t t80 = 0;
  int b8c0Frame = 0;
  // Step 15's counters T+0xB8A8 and T+0xB8AC, and the done flag brain+0x6D44 (step 17's, FUN_1404FBCA0). The brain
  // keeps that byte, not T; GF keeps the controller's per-side words in this struct, so it lives here.
  int b8a8 = 0;
  int b8ac = 0;
  bool counterDone = false;
  // T+0xE4, the offside-trap arm of step 5 (FUN_1404F6720). FUN_140501260 clears it at the top of each team tick
  // (0x1405012D0); step 5 sets it. Only the chain's gates can set it (pesoffball.cpp).
  bool trapArmed = false;
};

// The strategy word T+0x134 -> wanted level map: 0/1/3/4 one-to-one,
// anything else 2 (jump ladder 0x1404F8619-63E).
int StrategyWantLevel(int strategy);

// The manual/target word T+0x210 override mesh (0x1404F8640-6C8): t replaces
// want when t != 2, t != level (0x1404F8663), and (t < 2 and t <= want < 3)
// or (t > 2 and 2 <= want <= t).
int ManualOverrideLevel(int want, int manual, int level);

// The non-pad branch of FUN_1404F8350: want from the strategy, the manual
// override, the in-play freeze (MI+0x116C == 1), the clamp to 4, and the
// frame stamp on change. `manual` is T+0x210 (init 2; no in-match writer
// found, tactics §3 O4).
void DriftAttackLevel(TeamTick* tick, int strategy, int manual, bool inPlay, int frame);

// The flags PES sets right after the drift, in FUN_1404F6C30:
// B8A2 = (L >= 4), B8A3 = (L <= 0).
void WriteAttackFlags(TeamTick* tick);

// Step 28: T+0x80 = 0xFF when the ball is not in play (MI+0x116C != 1) or
// the latch flipped this tick (FUN_1404FE9C0 returns old^new 0x255).
void WriteRestartWord(TeamTick* tick, bool inPlay, bool possessionFlipped);

// Step 15's in-play arm: B8A8 = 8, B8AC = 4, brain+0x6D44 = 0 (0x1404FA869-0x1404FA887). The stopped ladder
// (0x1404FA897 on) is not ported.
void ArmInPlayCounters(TeamTick* tick);

}  // namespace pes
