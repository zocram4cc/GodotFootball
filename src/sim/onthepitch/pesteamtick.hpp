// PES 2021's per-tick team record words (Slice A): the attack level T+0xB8A4,
// its flags T+0xB8A2/B8A3, and the restart word T+0x80 (team-layer §1 steps
// 13/18/28, §2 V1). Pure functions over a small struct; the controller owns
// one per side and steps it every team tick on the PES path.

#pragma once

#include <array>
#include <cstdint>

#include "gametypes.hpp"
#include "onthepitch/pesmarktable.hpp"

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

// T+0x8C40's none value (FUN_1404FF8B0: miss stores 0xFF at 0x1405001E7).
inline constexpr uint32_t kStep20NoTarget = 0xFF;
inline std::array<uint32_t, kReactionSlots> NoTargetIds() {
  std::array<uint32_t, kReactionSlots> ids{};
  ids.fill(kStep20NoTarget);
  return ids;
}
// T+0xB8D0's none value (FUN_1404F7BA0: fail writes 0xFF at 0x1404F7BE3).
inline constexpr uint32_t kStep21None = 0xFF;
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
  bool deciderRan = false;  // S2: the decider has moved the word at least once
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
  // Step 19's eligible mask T+0xB8B8 (FUN_140500200): the slots the Chance
  // mask may take. Port spec §8; init mask 0 (no reader names a nonzero reset).
  uint32_t eligibleMask = 0;
  // Step 20's receiver tables (FUN_1404FF8B0): T+0x8C35[11] found flags,
  // T+0x8C40[11] target ids (0xFF none), T+0x8D50[11] write-only scores.
  std::array<uint8_t, kReactionSlots> slotFlag{};  // T+0x8C35
  std::array<uint32_t, kReactionSlots> slotTarget = NoTargetIds();  // T+0x8C40
  std::array<float, kReactionSlots> slotScore{};  // T+0x8D50
  // Step 21's pick + stamp (FUN_1404F7BA0): T+0xB8D0 (0xFF none, 0x15 row
  // sentinel) and brain+0x6D40 (MI+0x1198, written only when best <= 0x15).
  uint32_t bestSlot = kStep21None;  // T+0xB8D0
  int bestStamp = -1;  // brain+0x6D40
};
// T+0xB8B8 all-ones: step 19's level >= 4 arm sets every slot bit (11 BTS
// iterations at 0x140500240, stored at 0x1405008BD).
inline constexpr uint32_t kStep19AllSlots = (1u << kReactionSlots) - 1;

// Step 19 (FUN_140500200) head, 0x14050022C-240: level >= 4 stores the
// all-ones mask at T+0xB8B8 and claims the tick; lower levels continue into
// the class/list branches. Returns true when the head claimed the mask.
bool Step19Head(TeamTick* tick);
// Step 19's licence map (0x1405002F2-327: TEST + SUB/JZ chain): 0->0, 1->2,
// 2->4, 3->6, anything else 5. Meaningful below level 4 (level >= 4 takes the
// all-ones arm above); negative levels land on the else arm.
int Step19Licence(int level);
// Step 19's class branch (FUN_140500200, 0x14050024F-2C5): with 0 < level and
// the FUN_1405AAF00 gate true, slots < 0x16 whose folded (slot%11) T+0x4C00
// class > 1 (0x1405002A7 CMP EAX,0x1) take the mask at 0x1405002BE and the
// tick is claimed. `classes` is the caller's T+0x4C00 row (SideRoles::b 1:1);
// `gate` is the 5AAF00 verdict, whose roster/run-state/RNG/ball arms ride a
// // ponytail at the controller call. Returns true when the branch claimed it.
bool Step19Class(TeamTick* tick, bool gate, const std::array<int, kReactionSlots>& classes);
// Step 19's list-branch sort + pick (FUN_140500200, 0x140500544-88E): per
// own slot on the pitch, class >= 7 ranks with -100 (BTS at 0x1405006A2);
// else key = tableFloat - ramp(d)*5 (+ -3.0 on the holder match), ramp(d) =
// d<=10 ? 1 : d>=25 ? 0 : (d-25)/-15, d = man-to-ball; ascending stable sort
// (FUN_140A79640 at 0x1405007DF); the first `licence` entries take the mask
// (0x14050086E/885, stored at 0x14050088E). Table floats + holder-match ride
// a // ponytail at the controller call (selected-table writers OPEN,
// §2/§4 item 3); the distances reuse the quota's decoded ramp.
struct Step19Slot {
  int slot = 0;        // own-side slot 0..10
  int roleClass = 0;   // T+0x4C00 row (SideRoles::b 1:1)
  float manX = 0.0f;   // the man's x (man-to-ball distance arm)
  float manZ = 0.0f;   // the man's z
  bool holderMatch = false;  // the [RSP+0x22] holder-match flag for this slot
};
struct Step19ListInput {
  int licence = 0;  // [RSP+0x20], from Step19Licence
  std::array<Step19Slot, kReactionSlots> slots{};
  float ballX = 0.0f;  // MA+0x2482C (analysis ball; GF: the ball now)
  float ballZ = 0.0f;  // MA+0x24834
  std::array<float, kReactionSlots> tableFloat{};  // the A714E0 selected-table floats
};
uint32_t Step19ListPick(const Step19ListInput& in);
// Step 20 (FUN_1404FF8B0): the receiver tables T+0x8C35[11] (u8 found flag,
// 0x140500036; miss stores 0 at 0x1405001CD/1DE), T+0x8C40[11] (u32 target id,
// 0xFF = none at 0x14050003E/1E7) and T+0x8D50[11] (f32 score, 0x1405001BE —
// write-only diagnostic; ported to keep the write stream 1:1). Init: flags 0,
// ids 0xFF, scores 0. Only the flag table has readers (DOC team-tickets
// §5.5); the other two keep the stores without a reader.
// Gates (0x1404FF8D9/8EF/946): T+0x255 != 0, carrier 627E0 < 0x16 else T+0x78
// < 0x16, MI+0x116C == 1 — else the tick returns with tables untouched.
struct Step20Gates {
  bool latchSet = false;  // T+0x255 != 0
  int carrier = 0xFF;     // 627E0, 0xFF = none
  int kickMan = 0xFF;     // T+0x78 fallback (lane-leaves §4 rule)
  int playState = 0;      // MI+0x116C (1 = open play)
};
bool Step20GatesPass(const Step20Gates& g);
// Step 20's band write (0x14050017F-1BE): XMM13 = clamp01((x - 4.0)/-3.5)
// with the 0.5-select (below 0.5 -> 1.0, above 4.0 -> 0.0, else scaled);
// T+0x8D50[k] := XMM13 on the hit path only (0x1405001BE).
float Step20Band(float x);
// Step 21 (FUN_1404F7BA0): the supporter pick T+0xB8D0 (0x1404F82E8;
// 0xFF = none, 0x15 row sentinel) and its stamp brain+0x6D40
// (MI+0x1198 frame, 0x1404F82FC, only when best <= 0x15). Init 0xFF / -1.
// Gates (fail writes 0xFF at 0x1404F7BE3): T+0x255 == 0 clear, roster
// A55D40 == 0, A40BD0/A60340 < 5, side-1 flag set. The carrier reads 627E0
// else T+0x78 (0x1404F7C53-6E); still > 0x15 leaves B8D0 untouched.
struct Step21Gates {
  bool latchClear = false;  // T+0x255 == 0 (the no-possession arm)
  bool rosterOk = false;    // FUN_140A55D40 != 0
  bool levelOk = false;     // FUN_140A60340(A40BD0) >= 5
  bool sideFlagClear = false;  // MA+0x24C68+side == 0
  int carrier = 0xFF;       // 627E0, 0xFF = none
  int kickMan = 0xFF;       // T+0x78 fallback
};
bool Step21GatesPass(const Step21Gates& g);
// Step 21's score arms (0x1404F81A1/1B8/1D0/216/26C): incumbent -10.0, x0.6
// (0x14257A190, NOT 0.7), +d, loose cone (score > 30 ? 0 : angle >= 160 ?
// 50 : ((angle-30)/130)*50; 30.0 = XMM15 floor at 0x1404F7F64), support
// +20.0 (A62EA0 == cand). Tight cone (level <= 1): 43A020 <= 60.0 else skip.
float Step21LooseCone(float score, float angleDeg);
float Step21Score(float base, float dist, bool incumbent, float support);
// Step 21's best tracking (0x1404F8274-28A): NaN-safe strict-greater
// take-over from -1000.0 (0x14256AE38). Returns the winning id.
int Step21Best(const std::array<float, kReactionSlots>& scores);

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

// S2 (audit-2): the selector's mode input. +0xBCC is −1 (keep) until the decider first runs
// (PES's < 0 arm, TEST EAX,EAX; JS at 0x14050107C), then the strategy the decider just moved.
inline int SelectorMode(bool deciderRan, int strategy) { return deciderRan ? strategy : -1; }

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

// Step 35 (FUN_1404F61E0): rebuild the mark table from the tick's facts.
// Thin wrapper over marktable::BuildMarkTable so the team tick owns the call
// site (T7a `0x1404F76B1`); the facts come from the controller's run world.
void BuildMarkTableTick(marktable::MarkState* state, const marktable::Facts& facts,
                        const marktable::Constants& k);

}  // namespace pes
