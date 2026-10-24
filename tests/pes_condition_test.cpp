// PES 2021's condition loader (docs/re/pes-re-condition-roll.md): the band ladder's
// immediates and the multiplier table, read from the user's own PES2021.exe.
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "pes/condition.hpp"
#include "pes/exe.hpp"

namespace {

TEST(ConditionInstall, BandEdgesAndTableMatchTheDecodedLadderAndRows) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadConditionConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // FUN_140AF2DA0's seven CMP immediates (condition-roll §0).
  const uint8_t edges[7] = {0x41, 0x46, 0x4B, 0x50, 0x55, 0x5A, 0x5F};
  for (int i = 0; i < 7; ++i) EXPECT_EQ(k->bandEdges[size_t(i)], edges[i]) << "edge " << i;
  // 0x142632D70, 3 tiers x 5 PES arrows (condition-arrows §Confirmed).
  const float rows[3][5] = {{1.12f, 1.06f, 1.00f, 0.92f, 0.84f},
                            {1.09f, 1.05f, 1.00f, 0.94f, 0.88f},
                            {1.06f, 1.03f, 1.00f, 0.94f, 0.88f}};
  for (int t = 0; t < 3; ++t)
    for (int a = 0; a < 5; ++a) EXPECT_FLOAT_EQ(pes::ConditionMultiplier(*k, t, a), rows[t][a])
        << "tier " << t << " arrow " << a;
}

TEST(ConditionInstall, TheLoadedLadderDealsPESsFullBandTable) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadConditionConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // The cond->band->arrow table (condition-import §4), recomputed from the loaded edges.
  const struct {
    unsigned int cond;
    int band, pes;
  } table[] = {{0x28, 0, 4}, {0x41, 1, 3}, {0x46, 2, 2}, {0x4B, 3, 1},
               {0x50, 4, 0}, {0x55, 5, 2}, {0x5A, 6, 2}, {0x5F, 7, 2}};
  for (const auto& row : table) {
    EXPECT_EQ(pes::ConditionBand(*k, row.cond), row.band) << "cond " << row.cond;
    EXPECT_EQ(pes::ConditionArrow(pes::ConditionBand(*k, row.cond)), row.pes) << "cond " << row.cond;
  }
}

}  // namespace
