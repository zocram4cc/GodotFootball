#pragma once

// PES 2021's CPU strategy decider FUN_14051e970 and its strategy -> attack
// level map (docs/re/pes-re-match-management.md 1). It reads only the score,
// the match minute and the two teams' strength; possession, stamina and red
// cards are not inputs. Only match mode 0 (an ordinary match) is ported: modes
// 1-4 are two-leg and competition rules, 5/6 the team's always-attack /
// always-defend flag, none of which GF's fixtures carry.

#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pesrandom.hpp"

namespace pes {

// The strategy word T+0x134: 1 defensive, 2 balanced, 3 attacking, 4 all-out.
inline constexpr int kStrategyDefensive = 1;
inline constexpr int kStrategyBalanced = 2;
inline constexpr int kStrategyAttacking = 3;
inline constexpr int kStrategyAllOut = 4;

// The decider's entry and its moves by PES's play state MI+0x116C: 1 open play, 2 stopped, 3 restart
// armed or taken. FUN_14051e910 calls the decider where `(state - 1) & ~2 == 0`, its test at
// 0x14051E92E-0x14051E93C (DEC EAX; TEST EAX,0xFFFFFFFD; JNZ): states 1 and 3. FUN_14051e970 returns
// at once in state 1 (CMP [RBX+0x116C],1 at 0x14051E9CF; JZ 0x14051F3F4), so only state 3 moves it.
inline constexpr int kDeciderInPlayState = 1;
inline bool DeciderCalledIn(int playState) { return ((playState - 1) & ~2) == 0; }
inline bool DeciderMovesIn(int playState) {
  return playState != kDeciderInPlayState && DeciderCalledIn(playState);
}

// The minute-0 draw's literals, read out of FUN_14051e970's instructions.
struct StrategyConstants {
  float strengthMargin;        // 0.9 @0x14258c260
  int weightsStronger[3];      // {attacking, defensive, balanced} = {7, 0, 3}
  int weightsWeaker[3];        // {4, 4, 2}
  int weightsEven[3];          // {3, 2, 5}
  int attackHoldFrom;          // 10: attacking holds until 10 + rand(36)
  int defendHoldFrom;          // 20: defensive until 20 + rand(26)
  int holdTo;                  // 46: the hold's exclusive upper end
  int holdNever;               // 100: a hold past 45 means "for good"
  int secondHalfFrom;          // 28: R[6] = 28 + rand(10)
  int secondHalfSpan;          // 10
  int rngSlot;                 // 2: the decider's GameRandom slot
};

std::optional<StrategyConstants> LoadStrategyConstants(const PeImage& exe, std::string* error);

struct StrategyState {
  int strategy = kStrategyBalanced;
  int holdUntilMinute = 0;   // R[5]
  int secondHalfMinute = 0;  // R[6]
};

enum class StrategyPeriod { FirstHalf, SecondHalf, ExtraTime };

struct StrategyInputs {
  StrategyPeriod period = StrategyPeriod::FirstHalf;
  int minute = 0;  // the displayed match minute
  int ownGoals = 0, oppGoals = 0;
  // Each side's rating from the match setup (SetupStrength). The decider takes
  // their difference, as FUN_14051e970 does: d = strength(own) - strength(opp).
  float ownStrength = 0.0f, oppStrength = 0.0f;
};

// One call of the decider's body (FUN_14051e970), made only where DeciderMovesIn holds. `rng` is the
// decider's own GameRandom; it draws on every call at minute 0 (FUN_14051e970 l.252, `uVar7 == 0`).
void StrategyStep(const StrategyConstants& k, const StrategyInputs& in, Random& rng,
                  StrategyState* s);

// The non-pad branch of FUN_1404f8350: the attack level the strategy asks for
// (0/1/3/4 one-to-one, anything else 2); the level itself only moves while the
// ball is in play.
int StrategyAttackLevel(int strategy);

// PES's per-team rating, written once at match load (G+0x2ad0+0xc404+side*0x2804,
// docs/re/pes-re-match-management.md 1.1). GF has no such rating in its data, so
// the mean overall of the starting eleven stands in: each overall is 0..1 and
// the rating runs 0..100 (PES's 0..99 scale).
// ponytail: mean starting-eleven overall stands in for PES's setup rating.
inline constexpr float kRatingPerOverall = 100.0f;
float SetupStrength(const std::vector<float>& overalls);

}  // namespace pes
