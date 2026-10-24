#include "pesteamtick.hpp"

#include <algorithm>
#include <cmath>

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

void CopyModeWord(TeamTick* tick) { tick->word134 = tick->modeBcc; }

int StoppagePlan2(int currentPlan, bool setupWordNonZero) {
  if (!setupWordNonZero) return currentPlan;  // MOV EDX,2 is past the setup+0x1FC == 0 return
  if (currentPlan == 2) return currentPlan;  // CMP EAX,2; JZ return keeps plan 2
  return 2;
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

void StoppedSelect(TeamTick* tick, bool latchClear, int playState, int restartKind) {
  // The gate at 0x1405008F8-0x140500927: T+0x255 clear, MI+0x116C != 1, MI+0x1554 == 5.
  if (!latchClear || playState == kPlayStateInPlay || restartKind != kRestartFreeKick) return;
  // The store at 0x14050095B. The roster scan after it only calls FUN_140A66FF0 (a pure getter)
  // and discards the result, so nothing else changes.
  tick->b6c40 = 2;
}

bool ClearReactionDelays(TeamTick* tick, int playState) {
  // The branch at 0x14050247E-0x140502740: MI+0x116C != 1 zeroes T+0xB860 and T+0x6A4 together
  // (0x140502726-0x140502740, one loop of 11 stores to each bank).
  if (playState == kPlayStateInPlay) return false;
  // ponytail: the in-play half of step 9 (the event switch at 0x140502758, the seed-10 tail
  // gated on registry entry 15, and block B at 0x140502260) is not ported. Until it lands the
  // banks hold their values in play.
  for (int slot = 0; slot < kReactionSlots; ++slot) {
    tick->reactionDelay[slot] = 0;
    tick->reactionDelayMirror[slot] = 0;
  }
  return true;
}

void SupportWeights(TeamTick* tick, bool latchSet, bool flipped, int playState, bool scoreFlag) {
  // The sign XMM14 at 0x1404FEE10-0x1404FEE28: 1.0 when T+0x255 is set, else -1.0.
  const float sign = latchSet ? kUnitValue : kLatchClearSign;
  if (flipped) {
    // 0x1404FEE33-0x1404FEE39: the counter is reset and the stamp is -1.
    tick->noBallCounter = 0;
    tick->supportStamp = -1;
    for (int slot = 0; slot < kReactionSlots; ++slot) {
      // In play the bank is the sign times the base weight (0x1404FEE58-0x1404FEF52, eight floats
      // of T+0x224..0x240 and three of T+0x244..0x24C). Out of play it is 1.0 (0x1404FEF56-0x1404FEF89).
      tick->supportBank[slot] =
          playState == kPlayStateInPlay ? sign * tick->supportWeight[slot] : kUnitValue;
    }
  } else {
    // 0x1404FEF8B: no flip counts the frames the counter has run; the bank is left alone.
    tick->noBallCounter += 1;
  }
  // 0x1404FEF91-0x1404FEF9B: out of play the stamp is -1.
  if (playState != kPlayStateInPlay) tick->supportStamp = -1;
  if (!scoreFlag) {
    // The score-off fill (0x1404FF760-0x1404FF776): each base weight becomes XMM7 = 1.0, after the
    // bank above has read the old weights (the PES order).
    for (int slot = 0; slot < kReactionSlots; ++slot) tick->supportWeight[slot] = kUnitValue;
    return;
  }
  // ponytail: the score-on branch (0x1404FEFDF-0x1404FF75F) is not ported: the per-slot scores,
  // the recovery toward 1.0 while the stamp is above 0, and the stamp write from MI+0x1198 are
  // OPEN. The weights keep their values. The caller passes scoreFlag = false: PES's flag needs
  // MI+0x119C, the raw kick sum, which has no GF counterpart (team.cpp).
}

int RestartKindOf(e_SetPiece setPiece) {
  // The JR restart kinds (pes-re-A5Judge.md): 1 kick-off, 2 throw-in, 3 goal kick, 4 corner,
  // 5 free kick, 6 penalty kick, 7 period end (plan-selector §4: raised when the period ends and
  // held until the next award overwrites it). A referee with no set piece gives 0.
  switch (setPiece) {
    case e_SetPiece_KickOff: return 1;
    case e_SetPiece_ThrowIn: return 2;
    case e_SetPiece_GoalKick: return 3;
    case e_SetPiece_Corner: return 4;
    case e_SetPiece_FreeKick: return 5;
    case e_SetPiece_Penalty: return 6;
    case e_SetPiece_PeriodEnd: return kRestartPeriodEnd;
    case e_SetPiece_None: return 0;
  }
  return 0;
}

void ArmInPlayCounters(TeamTick* tick) {
  tick->b8a8 = kInPlayB8A8;
  tick->b8ac = kInPlayB8AC;
  tick->counterDone = false;  // MOV byte [RCX+0x6D44],0 at 0x1404FA887
}

// Step 19 (FUN_140500200) head: the level ladder at 0x14050022C-240 (CMP
// EBX,0x4; JL past the BTS loop) and the licence map at 0x1405002F2-327.
bool Step19Head(TeamTick* tick) {
  if (tick->b8a4 >= 4) {
    tick->eligibleMask = kStep19AllSlots;
    return true;
  }
  return false;
}

int Step19Licence(int level) {
  if (level == 0) return 0;
  if (level == 1) return 2;
  if (level == 2) return 4;
  if (level == 3) return 6;
  return 5;
}

// Step 19's class branch: 11-scan from side*11 (0x140500266-2BC); slot<0x16,
// fold slot%11, T+0x4C00[fold] > 1 admits (0x1405002A7/2AA).
// GF-side indexing already folds: roles.b[k] is T+0x4C00[(side*11+k)%11], so
// the caller's classes[k] is the folded word (RoleClassOfSlot reads b[slot]).
bool Step19Class(TeamTick* tick, bool gate, const std::array<int, kReactionSlots>& classes) {
  if (tick->b8a4 <= 0 || !gate) return false;
  uint32_t mask = 0;
  for (int k = 0; k < kReactionSlots; ++k)
    if (classes[static_cast<size_t>(k)] > 1) mask |= 1u << k;
  tick->eligibleMask = mask;
  return true;
}

// Step 19's sort-key ramp (0x140500726-746): the quota's decoded 10/25/-15
// ramp (pesoffball.hpp: kQuotaNearEdge/FarEdge/Falloff) times 5.0
// (0x140500746, XMM11); -3.0 on the holder match (0x14050078E, XMM10).
uint32_t Step19ListPick(const Step19ListInput& in) {
  // The distances reuse the quota's decoded edges (pesoffball constants are
  // the same image words: 10.0 at 0x142588210, 25.0 at 0x14259BE20,
  // -15.0 at 0x14259D668, 5.0 at 0x14259222C, -3.0 at 0x14259D654).
  constexpr float kNear_m = 10.0f;    // 0x142588210
  constexpr float kFar_m = 25.0f;     // 0x14259BE20
  constexpr float kFalloff_m = -15.0f;  // 0x14259D668
  constexpr float kScale = 5.0f;      // 0x14259222C (0x140500746)
  constexpr float kHolderAdd = -3.0f;  // 0x14259D654 (0x14050078E)
  constexpr float kForwardKey = -100.0f;  // 0xC2C80000 at 0x140500693
  struct Entry { int slot; float key; };
  std::array<Entry, kReactionSlots> entries{};
  for (int k = 0; k < kReactionSlots; ++k) {
    entries[static_cast<size_t>(k)] = {in.slots[static_cast<size_t>(k)].slot, 0.0f};
    if (in.slots[static_cast<size_t>(k)].roleClass >= 7) {
      entries[static_cast<size_t>(k)].key = kForwardKey;
      continue;
    }
    const float dx = in.slots[static_cast<size_t>(k)].manX - in.ballX;
    const float dz = in.slots[static_cast<size_t>(k)].manZ - in.ballZ;
    const float d = std::sqrt(dx * dx + dz * dz);
    const float ramp = d <= kNear_m ? 1.0f : d >= kFar_m ? 0.0f : (d - kFar_m) / kFalloff_m;
    float key = in.tableFloat[static_cast<size_t>(k)] - ramp * kScale;
    if (in.slots[static_cast<size_t>(k)].holderMatch) key += kHolderAdd;
    entries[static_cast<size_t>(k)].key = key;
  }
  // FUN_140A79640 (0x1405007DF): ascending by key, stable (slot order kept).
  std::stable_sort(entries.begin(), entries.end(),
                   [](const Entry& a, const Entry& b) { return a.key < b.key; });
  uint32_t mask = 0;
  for (int i = 0; i < in.licence && i < kReactionSlots; ++i) mask |= 1u << entries[static_cast<size_t>(i)].slot;
  return mask;
}

void BuildMarkTableTick(marktable::MarkState* state, const marktable::Facts& facts,
                        const marktable::Constants& k) {
  marktable::BuildMarkTable(state, facts, k);
}

}  // namespace pes
