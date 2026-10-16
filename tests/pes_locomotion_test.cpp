#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/locomotion.hpp"
#include "pes/vfs.hpp"

// Locomotion blend data (docs/re/pes-re-animselect.md 2.6, 4.2, 5.2).

namespace {
void Push(std::vector<uint8_t>& b, uint32_t bits, float param) {
  b.resize(b.size() + 8);
  std::memcpy(&b[b.size() - 8], &bits, 4);
  std::memcpy(&b[b.size() - 4], &param, 4);
}
uint32_t Bits(int motionNo, int group, bool mirror) {
  return uint32_t(motionNo - 0x520) | uint32_t(group) << 12 | uint32_t(mirror) << 19;
}
}  // namespace

TEST(BlendTable, DecodesRecordsAndSortsAGroupByParameter) {
  std::vector<uint8_t> b;
  Push(b, Bits(3011, 80, false), 0.3f);
  Push(b, Bits(2000, 7, true), 0.5f);
  Push(b, Bits(3633, 80, false), 0.0f);
  Push(b, Bits(3005, 80, true), 0.15f);
  auto group = pes::BlendGroup(pes::ParseBlendTable(b), 80);
  ASSERT_EQ(group.size(), 3u);
  EXPECT_EQ(group[0].motionNo, 3633);
  EXPECT_EQ(group[1].motionNo, 3005);
  EXPECT_TRUE(group[1].mirror);
  EXPECT_EQ(group[2].motionNo, 3011);
  EXPECT_FLOAT_EQ(group[2].param, 0.3f);
}

TEST(BlendTable, ATrailingPartialRecordIsIgnored) {
  std::vector<uint8_t> b;
  Push(b, Bits(3633, 80, false), 0.0f);
  b.push_back(0);
  EXPECT_EQ(pes::ParseBlendTable(b).size(), 1u);
}

// Bracketing members and the weight between them, over the parameter axis.
TEST(BlendTable, BracketsARateBetweenTwoMembers) {
  const std::vector<float> params = {0.0f, 0.15f, 0.3f, 0.5f, 0.9f, 1.0f};
  auto w = pes::Bracket(params, 0.4f);
  EXPECT_EQ(w.lo, 2);
  EXPECT_EQ(w.hi, 3);
  EXPECT_NEAR(w.t, 0.5f, 1e-6f);
  auto below = pes::Bracket(params, -1.0f), above = pes::Bracket(params, 2.0f);
  EXPECT_EQ(below.lo, 0);
  EXPECT_EQ(below.t, 0.0f);
  EXPECT_EQ(above.hi, 5);
  EXPECT_EQ(above.t, 1.0f);
}

TEST(LocomotionInstall, Group80IsTheWalkToSprintBlend) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto pe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(pe) << err;
  auto motions = pes::MotionNames(*pe, vfs, &err);
  ASSERT_TRUE(motions) << err;
  auto table = pes::LoadBlendTable(vfs, &err);
  ASSERT_TRUE(table) << err;
  EXPECT_EQ(table->size(), 449u);
  auto g = pes::BlendGroup(*table, pes::LOCOMOTION_BLEND_GROUP);
  const std::vector<std::string> want = {"walk_1_1", "run_1_1", "run_2_2", "run_3_3", "run_4_4", "run_5_5"};
  ASSERT_EQ(g.size(), want.size());
  for (size_t i = 0; i < g.size(); ++i) EXPECT_EQ((*motions)[g[i].motionNo], want[i]) << i;
  EXPECT_FLOAT_EQ(g[3].param, 0.5f);
}
