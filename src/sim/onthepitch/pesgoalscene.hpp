#pragma once

// Which FixDemo goal scene PES plays for a goal: the pool rule and the one draw of FUN_140477400 (0x140477400,
// docs/re/pes-re-scene-choice.md 2a). The builder collects every goal-category scene whose first step row has flag
// bit 6 (FUN_140a7cce0(mgr, pool, 3, 6)), keeps those with the level's bit (0 -> 11, 1 -> 12, 2 -> 13;
// FUN_140a7ce90 drops a scene without the bit), and for levels 1 and 2 with the stage gate on keeps those with bit 16.
// Then it draws Range0(pool size) (0x140a92530, slot 1 of the goal object) and plays pool[draw].
// The scene id is (category 3 << 16) | (index + 1) (0x140a7cce0: `param_3*0x10000 + 1 + index`).

#include <cstdint>
#include <optional>
#include <vector>

#include "onthepitch/pesrandom.hpp"
#include "pes/fixdemo_table.hpp"

namespace pes {

// ponytail: the level (goal ctx [0], 0..2) and the stage gate (DAT_143446058 and the not-in-mode flag of vtable
// +0x88 of 0x1434a2d40) are the builder's inputs from the goal context; GF has no goal context yet. The PES sites
// that remove this: the context writer that fills param_1[0] of 0x140477400 (callers of 0x140477400).
struct GoalSceneRule {
  int level = 0;
  bool stageGate = false;
};

// The pool in PES's order (table order), as scene ids.
std::vector<uint32_t> GoalScenePool(const FixDemoTable& goalTable, const GoalSceneRule& rule);

// The scene id PES plays, or nullopt where PES traps (an empty pool: the draw index is out of range).
std::optional<uint32_t> PickGoalScene(const FixDemoTable& goalTable, const GoalSceneRule& rule, Random& rng);

}  // namespace pes
