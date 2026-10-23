// PES 2021's per-tick team record words (Slice A): the attack level T+0xB8A4,
// its flags T+0xB8A2/B8A3, and the restart word T+0x80 (team-layer §1 steps
// 13/18/28, §2 V1). Pure functions over a small struct; the controller owns
// one per side and steps it every team tick on the PES path.

#pragma once

#include <cstdint>

#include "gametypes.hpp"

namespace pes {

// One side's per-tick team words. `b8a4` is the attack level 0..4
// (FUN_1404F8350); `b8a2`/`b8a3` the full-attack/full-defence flags
// (FUN_1404F6C30 inline at 0x1404F6E1C-58); `t80` the restart word
// (0x1404F6F91); `b8c0Frame` the stamp of the last level change (T+0xB8C0);
// `b6c40` the stopped-select word (brain+0x6C40, step 8: FUN_1405008E0); `reactionDelay` and
// `reactionDelayMirror` the step 9 banks T+0x6A4[11] and T+0xB860[11].
// The players per side that step 9 keeps a reaction delay for (the count 0xB at 0x14050225B).
constexpr int kReactionSlots = 11;

struct TeamTick {
  int b8a4 = 2;  // init FUN_140A415A0 -> 2 (tactics §3)
  bool b8a2 = false;
  bool b8a3 = false;
  uint32_t t80 = 0;
  int b8c0Frame = 0;
  int b6c40 = 0;
  int reactionDelay[kReactionSlots] = {};
  int reactionDelayMirror[kReactionSlots] = {};
};

// MI+0x116C, the play state (pes-re-A5Judge.md): 1 is play in progress.
constexpr int kPlayStateInPlay = 1;

// MI+0x1554, the restart kind (pes-re-A5Judge.md's JR table): 1 kick-off, 2 throw-in,
// 3 goal kick, 4 corner, 5 free kick, 6 penalty kick; 0 is no restart.
constexpr int kRestartFreeKick = 5;

// The restart kind of a referee set piece (MI+0x1554). The referee's desired set piece is the last
// award, kept until CancelSetPiece clears it (referee.cpp), which stands in for PES's JR word.
int RestartKindOf(e_SetPiece setPiece);

// Step 8 (FUN_1405008E0) at 0x1405008F8-0x14050095B: brain+0x6C40 = 2 when the latch (T+0x255) is
// clear, play is stopped (MI+0x116C != 1) and the restart is a free kick (MI+0x1554 == 5). Its
// roster scan (0x140500970-0x1405009D6) calls FUN_140A66FF0, a pure getter whose result is
// discarded, so the store is the whole effect. Nothing else in this slice reads b6c40 yet: step 9
// (T2b) does, and it is OPEN.
void StoppedSelect(TeamTick* tick, bool latchClear, int playState, int restartKind);

// Step 9 (FUN_140502190) out-of-play branch at 0x14050247E-0x140502740: when MI+0x116C != 1 both
// banks are zeroed on every call. Returns whether it cleared them. The in-play half of step 9 is
// OPEN: the event switch (jump table 0x140502758), the seed-10 tail (gated on registry entry 15,
// X+0x38) and block B. Until those land, the banks keep their values in play.
bool ClearReactionDelays(TeamTick* tick, int playState);

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

}  // namespace pes
