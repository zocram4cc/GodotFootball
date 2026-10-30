#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "onthepitch/pesgoalscene.hpp"
#include "pes/exe.hpp"

namespace {

// A goal table whose scenes carry flags on their first step only (0x140a7cce0 reads the first row).
pes::FixDemoTable Table(const std::vector<std::vector<uint32_t>>& stepFlagsPerScene) {
  pes::FixDemoTable t;
  for (const auto& steps : stepFlagsPerScene) {
    pes::FixDemoScene s;
    for (uint32_t f : steps) {
      pes::FixDemoStep st;
      st.flags = f;
      s.steps.push_back(st);
    }
    t.scenes.push_back(s);
  }
  return t;
}

constexpr uint32_t kPool = 1u << 6, kL0 = 1u << 11, kL1 = 1u << 12, kL2 = 1u << 13, kStage = 1u << 16;
constexpr uint32_t Id(uint32_t index) { return (3u << 16) | (index + 1); }

TEST(PesGoalScene, PoolIsThePoolBitThenTheLevelBitInTableOrder) {
  // scene 0: pool+L0; 1: no pool bit; 2: pool+L1; 3: pool+L0+L1; 4: pool but no level; 5: empty (no steps).
  auto t = Table({{kPool | kL0}, {kL0}, {kPool | kL1}, {kPool | kL0 | kL1}, {kPool}, {}});
  EXPECT_EQ(pes::GoalScenePool(t, {0, false}), (std::vector<uint32_t>{Id(0), Id(3)}));
  EXPECT_EQ(pes::GoalScenePool(t, {1, false}), (std::vector<uint32_t>{Id(2), Id(3)}));
  EXPECT_TRUE(pes::GoalScenePool(t, {2, false}).empty());
}

TEST(PesGoalScene, OnlyTheFirstStepRowDecides) {
  auto t = Table({{kPool | kL0, 0}, {0, kPool | kL0}});
  EXPECT_EQ(pes::GoalScenePool(t, {0, false}), (std::vector<uint32_t>{Id(0)}));
}

TEST(PesGoalScene, StageGateKeepsBit16OnlyForLevelsOneAndTwo) {
  auto t = Table({{kPool | kL1 | kStage}, {kPool | kL1}, {kPool | kL2 | kStage}, {kPool | kL0}, {kPool | kL0 | kStage}});
  EXPECT_EQ(pes::GoalScenePool(t, {1, true}), (std::vector<uint32_t>{Id(0)}));
  EXPECT_EQ(pes::GoalScenePool(t, {2, true}), (std::vector<uint32_t>{Id(2)}));
  // Level 0 never applies the gate (0x140477400 jumps past it).
  EXPECT_EQ(pes::GoalScenePool(t, {0, true}), (std::vector<uint32_t>{Id(3), Id(4)}));
  EXPECT_EQ(pes::GoalScenePool(t, {1, false}), (std::vector<uint32_t>{Id(0), Id(1)}));
}

class PesGoalSceneInstall : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) GTEST_SKIP() << "PES21_ROOT not set";
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe) << err;
    auto k = pes::LoadRngConstants(*exe, &err);
    ASSERT_TRUE(k) << err;
    k_ = *k;
  }
  pes::RngConstants k_;
};

TEST_F(PesGoalSceneInstall, DrawsRange0OfThePoolOnTheGoalObjectsSlotOne) {
  auto t = Table({{kPool | kL0}, {kPool | kL0}, {kPool | kL0}, {kPool | kL0}, {kPool | kL0}});
  for (uint32_t seed = 100; seed < 120; ++seed) {
    pes::Random a(k_), b(k_);
    for (int s = 0; s < 3; ++s) a.SetSeed(s, seed + s), b.SetSeed(s, seed + s);  // the match triple (t, t+1, t+2)
    const auto pool = pes::GoalScenePool(t, {0, false});
    const auto picked = pes::PickGoalScene(t, {0, false}, a);
    ASSERT_TRUE(picked);
    EXPECT_EQ(*picked, pool[size_t(b.Range0(int(pool.size())))]) << "seed " << seed;
    EXPECT_EQ(a.State(1), b.State(1)) << "exactly one draw, on slot 1";
    EXPECT_EQ(a.State(2), b.State(2));
  }
}

TEST_F(PesGoalSceneInstall, EmptyPoolIsPesTrapAndDrawsNothingUseful) {
  auto t = Table({{kL0}});
  pes::Random r(k_);
  EXPECT_FALSE(pes::PickGoalScene(t, {0, false}, r));
}

}  // namespace
