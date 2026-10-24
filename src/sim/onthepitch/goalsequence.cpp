#include "goalsequence.hpp"

#include <algorithm>
#include <vector>

namespace GoalSequence {

namespace {

// FUN_140477400 draws again while the scene repeats the last celebration, and gives up after ten draws.
constexpr int kRepeatDraws = 10;

// FUN_140474b20 filters the hug pool by the level bits 12 and 13. Every hug the install carries has both
// (so the goal level does not narrow it), and the filter is kept as PES states it.
constexpr int kHugLevelBitA = 12;
constexpr int kHugLevelBitB = 13;

std::vector<const pes::fixdemo::GoalScene*> HugPool(const pes::fixdemo::GoalScenes& scenes) {
  std::vector<const pes::fixdemo::GoalScene*> pool;
  for (const pes::fixdemo::GoalScene& scene : scenes.hug) {
    if ((((scene.flags >> kHugLevelBitA) | (scene.flags >> kHugLevelBitB)) & 1u) != 0) {
      pool.push_back(&scene);
    }
  }
  return pool;
}

}  // namespace

unsigned long CelebrationMs(float length_s) {
  return static_cast<unsigned long>(length_s * 1000.0f + 0.5f);
}

pes::fixdemo::GoalScene PickCelebration(const pes::fixdemo::GoalScenes& scenes, int level,
                                        int previousIndex, pes::Random& rng) {
  std::vector<const pes::fixdemo::GoalScene*> pool;
  for (const pes::fixdemo::GoalScene& scene : scenes.generic) {
    if (((scene.flags >> (pes::fixdemo::kGoalLevelBit0 + level)) & 1u) != 0) pool.push_back(&scene);
  }
  if (pool.empty()) return scenes.fallback;
  for (int draw = 0; draw < kRepeatDraws; draw++) {
    const pes::fixdemo::GoalScene* pick = pool[size_t(rng.Range0(int(pool.size())))];
    if (pick->index != previousIndex) return *pick;
  }
  return scenes.fallback;
}

pes::fixdemo::GoalScene PickHug(const pes::fixdemo::GoalScenes& scenes, pes::Random& rng) {
  const std::vector<const pes::fixdemo::GoalScene*> pool = HugPool(scenes);
  if (pool.empty()) return pes::fixdemo::GoalScene{-1, 0, 0.0f};
  return *pool[size_t(rng.Range0(int(pool.size())))];
}

unsigned long LongestCelebration_ms(const pes::fixdemo::GoalScenes& scenes) {
  unsigned long longest = CelebrationMs(scenes.fallback.length_s);
  for (const pes::fixdemo::GoalScene& scene : scenes.generic) {
    longest = std::max(longest, CelebrationMs(scene.length_s));
  }
  unsigned long longestHug = 0;
  for (const pes::fixdemo::GoalScene* scene : HugPool(scenes)) {
    longestHug = std::max(longestHug, CelebrationMs(scene->length_s));
  }
  return longest + longestHug;
}

unsigned long ReplayFiresAt_ms(unsigned long goalTime_ms, unsigned long cutsceneEnd_ms,
                               unsigned long celebrationLength_ms) {
  const unsigned long celebrationEnd = goalTime_ms + celebrationLength_ms;
  return cutsceneEnd_ms > celebrationEnd ? cutsceneEnd_ms : celebrationEnd;
}

}  // namespace GoalSequence
