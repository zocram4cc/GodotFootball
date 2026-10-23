#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "onthepitch/goalsequence.hpp"
#include "onthepitch/pesrandom.hpp"
#include "pes/fixdemo.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"

// The goal celebrations come from the user's install (PES21_ROOT): the FixDemo goal table and its cut
// files. Without an install there is nothing to check them against, so the tests skip.

TEST(GoalCelebrations, TheGoalTableParsesToItsSectionEnd) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  // LoadCutTable is strict: every column must be consumed and the read count must land on the section end.
  const std::optional<pes::fixdemo::CutTable> table = pes::fixdemo::LoadCutTable(vfs, "goal", &err);
  ASSERT_TRUE(table) << err;
  EXPECT_FALSE(table->scenes.empty());
  EXPECT_FALSE(table->groups.empty());
}

TEST(GoalCelebrations, EveryPoolCelebrationAndHugRunsForAPlausibleLength) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  const std::optional<pes::fixdemo::GoalScenes> scenes = pes::fixdemo::LoadGoalScenes(vfs, &err);
  ASSERT_TRUE(scenes) << err;
  ASSERT_FALSE(scenes->generic.empty());
  ASSERT_FALSE(scenes->hug.empty());
  // Cut times are 1/60 s ticks. A unit slip (ticks read as seconds, or the reverse) would fall outside
  // this range, and so would a misread length: a goal scene is a few seconds, not a fraction or minutes.
  for (const pes::fixdemo::GoalScene& scene : scenes->generic) {
    EXPECT_GE(scene.length_s, 1.0f) << "celebration " << scene.index;
    EXPECT_LE(scene.length_s, 60.0f) << "celebration " << scene.index;
    EXPECT_NE(scene.flags & (1u << pes::fixdemo::kGenericPoolBit), 0u) << "celebration " << scene.index;
  }
  for (const pes::fixdemo::GoalScene& scene : scenes->hug) {
    EXPECT_GE(scene.length_s, 1.0f) << "hug " << scene.index;
    EXPECT_LE(scene.length_s, 60.0f) << "hug " << scene.index;
  }
}

TEST(GoalCelebrations, ThePickStaysInItsPoolsAndDoesNotRepeatTheLast) {
  const pestest::PesMatchConstants& c = pestest::SharedPesMatchConstants();
  if (!c.rng || !c.goalScenes) GTEST_SKIP() << "install constants unavailable";
  const pes::fixdemo::GoalScenes& scenes = *c.goalScenes;
  std::vector<int> primaryPool;
  for (const pes::fixdemo::GoalScene& scene : scenes.generic) {
    if ((scene.flags >> (pes::fixdemo::kGoalLevelBit0 + GoalSequence::kGoalLevel)) & 1u) {
      primaryPool.push_back(scene.index);
    }
  }
  ASSERT_FALSE(primaryPool.empty()) << "no generic celebration at the goal level";
  const unsigned long bound = GoalSequence::LongestCelebration_ms(scenes);

  const auto draws = [&](unsigned int seed) {
    pes::Random rng(*c.rng);
    rng.SetSeed(pes::kRange0Slot, seed);
    std::vector<int> picked;
    int previous = -1;
    for (int i = 0; i < 200; i++) {
      const pes::fixdemo::GoalScene pick =
          GoalSequence::PickCelebration(scenes, GoalSequence::kGoalLevel, previous, rng);
      const bool inPool =
          std::find(primaryPool.begin(), primaryPool.end(), pick.index) != primaryPool.end();
      // A draw from the pool is never the celebration it repeats. The only other outcome is the fallback.
      if (inPool) EXPECT_NE(pick.index, previous) << "draw " << i << " repeated the last celebration";
      EXPECT_TRUE(inPool || pick.index == scenes.fallback.index) << "draw " << i;
      const pes::fixdemo::GoalScene hug = GoalSequence::PickHug(scenes, rng);
      EXPECT_GE(hug.index, 0) << "draw " << i << " found no hug";
      // The restart is armed after the whole sequence, so no draw may run past the bound the watch uses.
      EXPECT_LE(GoalSequence::CelebrationMs(pick.length_s) + GoalSequence::CelebrationMs(hug.length_s), bound)
          << "draw " << i;
      picked.push_back(pick.index);
      previous = pick.index;
    }
    return picked;
  };
  // Deterministic for a seed: the same match draws the same celebrations.
  EXPECT_EQ(draws(7), draws(7));
}
