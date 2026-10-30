#include "pesgoalscene.hpp"

namespace pes {
namespace {

// Flag bits of a scene's first step row (row +0xc, 0x140a7cce0 / 0x140a7ce90). Immediates of the calls in 0x140477400:
// the pool bit is the `6` pushed before the call at 0x1404774xx, the level bits are 0xb/0xc/0xd and the stage bit 0x10.
constexpr uint32_t kGoalCategory = 3;
constexpr int kPoolBit = 6;
constexpr int kLevelBit[3] = {11, 12, 13};
constexpr int kStageBit = 16;

bool Has(const FixDemoScene& scene, int bit) { return (scene.steps.front().flags >> bit) & 1u; }

}  // namespace

std::vector<uint32_t> GoalScenePool(const FixDemoTable& goalTable, const GoalSceneRule& rule) {
  std::vector<uint32_t> pool;
  if (rule.level < 0 || rule.level > 2) return pool;  // 0x140477400 skips every filter and draws from an empty pool
  for (size_t i = 0; i < goalTable.scenes.size(); ++i) {
    const FixDemoScene& scene = goalTable.scenes[i];
    if (scene.steps.empty() || !Has(scene, kPoolBit) || !Has(scene, kLevelBit[rule.level])) continue;
    if (rule.level != 0 && rule.stageGate && !Has(scene, kStageBit)) continue;
    pool.push_back((kGoalCategory << 16) | static_cast<uint32_t>(i + 1));
  }
  return pool;
}

std::optional<uint32_t> PickGoalScene(const FixDemoTable& goalTable, const GoalSceneRule& rule, Random& rng) {
  const std::vector<uint32_t> pool = GoalScenePool(goalTable, rule);
  if (pool.empty()) return std::nullopt;  // PES: the draw index is out of range, 0x140477400 traps
  return pool[static_cast<size_t>(rng.Range0(static_cast<int>(pool.size())))];
}

}  // namespace pes
