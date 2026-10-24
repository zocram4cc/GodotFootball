// PES 2021's per-tick team record words (Slice A): the attack level T+0xB8A4,
// its flags T+0xB8A2/B8A3, and the restart word T+0x80 (team-layer §1 steps
// 13/18/28, §2 V1). Pure functions over a small struct; the controller owns
// one per side and steps it every team tick on the PES path.

#pragma once

#include <array>
#include <cstdint>

#include "gametypes.hpp"

namespace pes {

// One side's per-tick team words. `b8a4` is the attack level 0..4
// (FUN_1404F8350); `b8a2`/`b8a3` the full-attack/full-defence flags
// (FUN_1404F6C30 inline at 0x1404F6E1C-58); `t80` the restart word
// (0x1404F6F91); `b8c0Frame` the stamp of the last level change (T+0xB8C0);
// `b6c40` the stopped-select word (brain+0x6C40, step 8: FUN_1405008E0); `reactionDelay` and
// `reactionDelayMirror` the step 9 banks T+0x6A4[11] and T+0xB860[11];
// `noBallCounter`, `supportWeight`, `supportBank` and `supportStamp` the step 11 words (T+0x218,
// T+0x224[11], brain+0x6CC4[11] and brain+0x6CF4).
// The players per side that step 9 keeps a reaction delay for (the count 0xB at 0x14050225B).
constexpr int kReactionSlots = 11;

// The unit value of step 11's bank and base weights: the float 1.0 at 0x14256AE1C, read from the
// image (bytes 00 00 80 3F).
constexpr float kUnitValue = 1.0f;
// The latch-clear sign of step 11's bank: the float at 0x142588218, -1.0 (read from the image).
constexpr float kLatchClearSign = -1.0f;

// The base weights start at 1.0. PES fills T+0x224..0x24C from the team sheet (OPEN, its loader is
// not ported). Step 11's score-off fill sets them to 1.0 on every call, so 1.0 is the stand-in
// until the loader lands.
inline std::array<float, kReactionSlots> UnitWeights() {
  std::array<float, kReactionSlots> w;
  w.fill(kUnitValue);
  return w;
}

// The mode word T+0x134 and its setup source +0xBCC (plan-selector §5: the team tick copies
// +0xBCC to T+0x134 at 0x1404F6DBD, between the decider and the drift, so the drift reads the
// previous tick's word). +0xBCC's writer is OPEN (plan-selector §2), so GF carries −1 (keep)
// until it lands; the copy below mirrors PES's order now, the drift reads 134 once the writer
// does. ponytail: modeBcc stands in for +0xBCC (0x140501076); remove when its writer lands.
struct TeamTick {
  int b8a4 = 2;  // init FUN_140A415A0 -> 2 (tactics §3)
  bool b8a2 = false;
  bool b8a3 = false;
  int modeBcc = -1;  // setup +0xBCC: −1/absent keeps
  int word134 = -1;  // T+0x134: the 0x1404F6DBD copy of modeBcc
  uint32_t t80 = 0;
  int b8c0Frame = 0;
  int b6c40 = 0;
  int reactionDelay[kReactionSlots] = {};
  int reactionDelayMirror[kReactionSlots] = {};
  int noBallCounter = 0;                                            // T+0x218 (step 11)
  std::array<float, kReactionSlots> supportWeight = UnitWeights();  // T+0x224[11] (step 11)
  std::array<float, kReactionSlots> supportBank{};                  // brain+0x6CC4[11] (step 11)
  int supportStamp = -1;                                            // brain+0x6CF4 (step 11)
  // Step 15's counters T+0xB8A8 and T+0xB8AC, and the done flag brain+0x6D44 (step 17's, FUN_1404FBCA0). The brain
  // keeps that byte, not T; GF keeps the controller's per-side words in this struct, so it lives here.
  int b8a8 = 0;
  int b8ac = 0;
  bool counterDone = false;
  // T+0xE4, the offside-trap arm of step 5 (FUN_1404F6720). FUN_140501260 clears it at the top of each team tick
  // (0x1405012D0); step 5 sets it. Only the chain's gates can set it (pesoffball.cpp).
  bool trapArmed = false;
};

// MI+0x116C, the play state (pes-re-A5Judge.md): 1 is play in progress.
constexpr int kPlayStateInPlay = 1;
// MI+0x116C == 2: stopped but no restart is armed or taken (plan-selector §1's stoppage arm tests
// for exactly this; PlayStateFor in pes/movestep.hpp produces it).
inline constexpr int kPlayStateStopped = 2;

// MI+0x1554, the restart kind (pes-re-A5Judge.md's JR table): 1 kick-off, 2 throw-in,
// 3 goal kick, 4 corner, 5 free kick, 6 penalty kick, 7 period end (plan-selector §4: the
// period-end thunk 0x143F559F0 behind gate 0x14050C3E0); 0 is no restart.
constexpr int kRestartFreeKick = 5;
constexpr int kRestartPeriodEnd = 7;

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

// Step 11 (FUN_1404FEDC0) head, 0x1404FEDC0-0x1404FEF9B, and its score-off fill (0x1404FF760).
// `latchSet` is T+0x255 != 0, `flipped` is R8B (the latch changed this tick, FUN_1404FE9C0's
// return), `playState` is MI+0x116C and `scoreFlag` is DL (the per-slot scores, OPEN). Sets the
// counter T+0x218, the stamp and the bank; fills the base weights when `scoreFlag` is off.
void SupportWeights(TeamTick* tick, bool latchSet, bool flipped, int playState, bool scoreFlag);

// The 0x1404F6DBD copy (plan-selector §5): T+0x134 = +0xBCC, between the decider and the drift.
void CopyModeWord(TeamTick* tick);

// The stoppage plan-2 rule (plan-selector §1: 0x140500FA9-DB7): when the stoppage arm reaches the
// plan write with the sheet's current plan, the setup word keeps it when zero, plan 2 keeps
// itself, and anything else becomes plan 2.
int StoppagePlan2(int currentPlan, bool setupWordNonZero);

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
