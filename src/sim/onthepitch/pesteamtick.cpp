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

// Step 19's licence map: 0->0, 1->2, 2->4, 3->6, anything else 5.
int Step19Licence(int level) {
  if (level == 0) return 0;
  if (level == 1) return 2;
  if (level == 2) return 4;
  if (level == 3) return 6;
  return 5;
}

// Step 19's list branch (FUN_140500200, 0x140500544-88E); see pesteamtick.hpp.
uint32_t Step19ListPick(const Step19ListInput& in) {
  // The count arm (0x1405003ED-F1): fewer men ahead than the licence and the whole side takes the mask.
  if (in.count < in.licence) return kStep19AllSlots;
  // The sort-key constants are the quota's decoded image words: 10.0 at 0x142588210, 25.0 at 0x14259BE20,
  // -15.0 at 0x14259D668, 5.0 at 0x14259222C (0x140500746, XMM11), -3.0 at 0x14259D654 (0x14050078E, XMM10).
  constexpr float kNear_m = 10.0f;
  constexpr float kFar_m = 25.0f;
  constexpr float kFalloff_m = -15.0f;
  constexpr float kScale = 5.0f;
  constexpr float kHolderAdd = -3.0f;
  constexpr float kForwardKey = -100.0f;  // 0xC2C80000 at 0x140500693
  struct Entry {
    int slot;
    float key;
  };
  std::array<Entry, kReactionSlots> entries{};
  int listed = 0;
  uint32_t mask = 0;
  for (const Step19Slot& man : in.slots) {
    if (!man.onPitch) continue;  // 0x140500621: an off-pitch slot is not listed
    if (man.roleClass >= kStep19ForwardClass) {
      mask |= 1u << man.slot;  // BTS at 0x140500663-6A6: taken at once, and he ranks too
      entries[static_cast<size_t>(listed++)] = {man.slot, kForwardKey};
      continue;
    }
    const float dx = man.manX - in.ballX;
    const float dz = man.manZ - in.ballZ;
    const float d = std::sqrt(dx * dx + dz * dz);
    const float ramp = d <= kNear_m ? 1.0f : d >= kFar_m ? 0.0f : (d - kFar_m) / kFalloff_m;
    float key = in.tableFloat[static_cast<size_t>(man.slot)] - ramp * kScale;
    if (man.holderMatch) key += kHolderAdd;
    entries[static_cast<size_t>(listed++)] = {man.slot, key};
  }
  // FUN_140A79640 (0x1405007DF): ascending by key, stable (slot order kept on ties).
  std::stable_sort(entries.begin(), entries.begin() + listed,
                   [](const Entry& a, const Entry& b) { return a.key < b.key; });
  for (int i = 0; i < in.licence && i < listed; ++i)
    mask |= 1u << entries[static_cast<size_t>(i)].slot;
  return mask;
}

// Step 19 as a whole (FUN_140500200): level >= 4 takes every slot; then the class branch; then the list at
// the level's licence.
uint32_t Step19Mask(int level, bool classGate, const Step19ListInput& list) {
  if (level >= kStep19AllFromLevel) return kStep19AllSlots;
  if (level > 0 && classGate) {
    uint32_t mask = 0;
    for (const Step19Slot& man : list.slots)
      if (man.roleClass > 1) mask |= 1u << man.slot;
    return mask;
  }
  Step19ListInput in = list;
  in.licence = Step19Licence(level);
  return Step19ListPick(in);
}

void BuildMarkTableTick(marktable::MarkState* state, const marktable::Facts& facts,
                        const marktable::Constants& k) {
  marktable::BuildMarkTable(state, facts, k);
}

// Step 20's gates (0x1404FF8D9/8EF/946): latch set, carrier-or-kickman valid,
// in play — else the tick returns with tables untouched (0x140500134).
bool Step20GatesPass(const Step20Gates& g) {
  if (!g.latchSet) return false;
  const bool holderOk = (g.carrier >= 0 && g.carrier < 0x16) ||
                        (g.kickMan >= 0 && g.kickMan < 0x16);
  if (!holderOk) return false;
  return g.playState == kPlayStateInPlay;
}

// Step 20's band write (0x14050017F-1BE): the 0.5-select (COMISS XMM13,0.5 at
// 0x140500187 + JC, so NaN -> 1.0): below -> 1.0, above 4.0 -> 0.0, else (x-4.0)/-3.5;
// -3.5 is 0x14259D658 (0x1405001A4 DIVSS), 4.0 the 0x140500193 arm.
float Step20Band(float x) {
  constexpr float kHalf = 0.5f;   // 0x142565600 (0x140500187)
  constexpr float kTop = 4.0f;    // 0x140500193 arm
  constexpr float kSpan = -3.5f;  // 0x14259D658 (0x1405001A4)
  if (std::isnan(x)) return 1.0f;
  if (x < kHalf) return 1.0f;
  if (x > kTop) return 0.0f;
  return (x - kTop) / kSpan;
}

// Step 20's outer loop (T3InteriorRE §1): the per-slot rows T+0x8C35/T+0x8C40 at 0x140500036/03E. Per
// outer k the cand side·11 + k misses (flag 0, 0xFF) when off-pitch (0x1404FFA55); the holder keeps
// the slot's old values without storing (0x140500174). Otherwise: no blocker + in range takes
// (1, 0xFF) on exhaust (0x140500015), a blocking opp takes (0, opp) (0x1404FFE38/0x140500140), and
// out of range takes (0, 0xFF) (0x140500149). The 8D50 band write needs the running min the inner
// geometry (A8F7C0/A90FD0, 0x1404FFC97/0x1404FFD47) reduces; GF has no analogue, so 8D50 keeps its
// previous values.
void Step20Table(TeamTick* tick, const Step20Input& in) {
  for (int k = 0; k < kReactionSlots; ++k) {
    const int cand = in.side * kReactionSlots + k;
    if (cand > 0x15) continue;  // 0x1404FFA4C: cand > 0x15 misses
    const Step20Slot& s = in.slots[static_cast<size_t>(k)];
    if (!s.onPitch) {
      tick->slotFlag[static_cast<size_t>(k)] = 0;
      tick->slotTarget[static_cast<size_t>(k)] = kStep20NoTarget;
      continue;
    }
    if (cand == in.holder) continue;
    if (!s.inRange) {
      tick->slotFlag[static_cast<size_t>(k)] = 0;
      tick->slotTarget[static_cast<size_t>(k)] = kStep20NoTarget;
      continue;
    }
    if (s.blocks >= 0 && s.blocks < 0x16) {
      tick->slotFlag[static_cast<size_t>(k)] = 0;
      tick->slotTarget[static_cast<size_t>(k)] = static_cast<uint32_t>(s.blocks);
      continue;
    }
    tick->slotFlag[static_cast<size_t>(k)] = 1;
    tick->slotTarget[static_cast<size_t>(k)] = kStep20NoTarget;
  }
}

// Step 21's gates (§6): latch clear (T+0x255 == 0, 0x1404F7BB3 JNZ fail),
// roster A55D40 != 0 (0x1404F7BCC), level words A40BD0/A60340 >= 5
// (0x1404F7C05-15), side-1 flag clear (0x1404F7C1B-33), carrier-or-kickman
// < 0x16 (0x1404F7C46-72; 0x1404F7C76 JA leaves B8D0 untouched — modeled by
// the caller keeping the old value when no carrier resolves).
bool Step21GatesPass(const Step21Gates& g) {
  if (!g.latchClear) return false;
  if (!g.rosterOk) return false;
  if (!g.levelOk) return false;
  if (!g.sideFlagClear) return false;
  const bool holderOk = (g.carrier >= 0 && g.carrier < 0x16) ||
                        (g.kickMan >= 0 && g.kickMan < 0x16);
  return holderOk;
}

// Step 21's loose cone (0x1404F81E9-21E): score > 30 → 0; angle >= 160 → 50;
// else ((angle - 30)/130)*50. Floor 30.0 is XMM15 (0x140591140, 0x1404F7F64);
// span 130.0 is 0x1425A9BF0 (0x1404F820E), scale 50.0 is 0x14259BE34.
float Step21LooseCone(float score, float angleDeg) {
  constexpr float kFloor = 30.0f;    // XMM15 (0x1404F7F64)
  constexpr float kCap = 160.0f;     // 0x14259BE6C (0x1404F81FA)
  constexpr float kSpan = 130.0f;    // 0x1425A9BF0 (0x1404F820E)
  constexpr float kScale = 50.0f;    // 0x14259BE34 (0x1404F8216)
  if (score > kFloor) return 0.0f;
  if (angleDeg >= kCap) return kScale;
  return ((angleDeg - kFloor) / kSpan) * kScale;
}

// Step 21's score arms: incumbent -10.0 (0x1404F81A1), ×0.6 (0x1404F81B8 →
// 0x14257A190, NOT 0.7 per §7), +d (0x1404F81D0), support +20.0
// (0x1404F826C) when A62EA0 == cand.
float Step21Score(float base, float dist, bool incumbent, float support) {
  constexpr float kIncumbentPenalty = -10.0f;  // 0x14259BE84 (0x1404F81A1)
  constexpr float kScale = 0.6f;               // 0x14257A190 (0x1404F81B8)
  float score = base;
  if (incumbent) score += kIncumbentPenalty;
  score *= kScale;
  score += dist;
  score += support;
  return score;
}

// Step 21's best tracking (0x1404F8274-28A): NaN-safe strict-greater
// take-over (UCOMISS/JP/JZ/JBE) from -1000.0 (0x14256AE38, 0x1404F7EB1).
// No take-over leaves R15D = 0xFF (init 0x1404F7EBA); 0x15 is only the
// stamp-gate bound (0x1404F82EF).
int Step21Best(const std::array<float, kReactionSlots>& scores) {
  constexpr float kFloor = -1000.0f;  // 0x14256AE38 (0x1404F7EB1)
  float best = kFloor;
  int id = 0xFF;
  for (int k = 0; k < kReactionSlots; ++k) {
    const float s = scores[static_cast<size_t>(k)];
    if (!(s > best)) continue;  // NaN-safe: NaN never takes over
    best = s;
    id = k;
  }
  return id;
}

// Step 21's per-slot loop (T3InteriorRE §2): the scan k = 0..10 over the opposing half
// (cand = (side==0)·11 + k, 0x1404F7F75-84). Off-pitch skips; the incumbent admits without the
// tests (0x1404F802B), else the caller folds the 8C35-row/B8C4 arms into `admitted`; the carrier
// is skipped (0x1404F80DA); the (5·ramp+5)..40 m band excludes the near clump and the far field
// (0x1404F810F-2F); tight level (B8A4 ≤ 1, 0x1404F81C9) keeps angle ≤ 60 else skips; loose adds
// Step21LooseCone; a restart-kind support adds 20 (0x1404F826C). Best from −1000 (0x1404F7EB1) by
// NaN-safe strict-greater take-over (first valid takes over via JZ, 0x1404F8274-8A). Post-loop
// always writes B8D0 (0xFF on empty); the stamp writes only when best ≤ 0x15 (0x1404F82EF-FC).
void Step21Pick(TeamTick* tick, const Step21Input& in) {
  constexpr float kBestFloor = -1000.0f;  // 0x14256AE38 (0x1404F7EB1)
  constexpr float kNearBase = 5.0f;  // the 5·ramp+5 near arm (0x1404F810F)
  constexpr float kNearSpan = 5.0f;
  constexpr float kFar = 40.0f;  // 0x14259BE2C (0x1404F8128)
  constexpr float kTightCap = 60.0f;  // 0x14259A764 (0x1404F81DB)
  constexpr float kSupportAdd = 20.0f;  // 0x14259BE1C (0x1404F826C)
  constexpr int kTightLevel = 1;  // 0x1404F81C9: tight iff B8A4 ≤ 1
  float best = kBestFloor;
  int id = 0xFF;
  for (int k = 0; k < kReactionSlots; ++k) {
    const int cand = (in.side == 0 ? kReactionSlots : 0) + k;
    if (cand > 0x15) continue;
    const Step21Slot& s = in.slots[static_cast<size_t>(k)];
    if (!s.onPitch) continue;
    const bool incumbent = cand == in.incumbent;
    if (!incumbent && !s.admitted) continue;
    if (cand == in.holder) continue;
    if (s.dist < kNearSpan * in.ramp + kNearBase) continue;
    if (s.dist > kFar) continue;
    float support = 0.0f;
    float loose = 0.0f;
    if (in.level <= kTightLevel) {
      if (s.angle > kTightCap) continue;
    } else {
      loose = Step21LooseCone(Step21Score(s.base, s.dist, incumbent, 0.0f), s.angle);
    }
    if (s.isSupport) support = kSupportAdd;
    const float score = Step21Score(s.base, s.dist, incumbent, loose + support);
    if (!(score > best)) continue;  // NaN-safe: NaN never takes over
    best = score;
    id = cand;
  }
  tick->bestSlot = static_cast<uint32_t>(id);
  if (id >= 0 && id <= 0x15) tick->bestStamp = in.frame;
}

}  // namespace pes
