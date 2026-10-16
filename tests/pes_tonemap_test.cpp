#include <gtest/gtest.h>

#include <cmath>

#include "pes/tonemap.hpp"

namespace {

// The film helper widens the toe and shoulder endpoints by the clip values
// (docs/re/pes-re-lighting.md Part C §3; PES FUN_14175f430).
TEST(FilmCurve, EndpointScalesFromClipValues) {
  const pes::FilmCurve f = pes::MakeFilmCurve(0.88, 0.45, 0.55, 0.0, 0.02);
  EXPECT_NEAR(f.toeScale, 0.55, 1e-12);       // 1 + black − toe
  EXPECT_NEAR(f.shoulderScale, 0.47, 1e-12);  // 1 + white − shoulder
  EXPECT_DOUBLE_EQ(f.slope, 0.88);
  EXPECT_DOUBLE_EQ(f.black, 0.0);
  EXPECT_DOUBLE_EQ(f.white, 0.02);
}

// The straight segment and the shoulder match are placed from the toe match.
TEST(FilmCurve, StraightAndShoulderMatchesFollowFromToeMatch) {
  const pes::FilmCurve f = pes::MakeFilmCurve(0.88, 0.45, 0.55, 0.0, 0.02);
  EXPECT_NEAR(f.straightMatch, (1.0 - 0.45) / 0.88 - f.toeMatch, 1e-12);
  EXPECT_NEAR(f.shoulderMatch, 0.55 / 0.88 - f.straightMatch, 1e-12);
}

// Regression oracle: the report's values for the global set (live, toe +
// shoulder = 1, so the blend is a hard switch) and for the span set, which the
// decompiled helper reproduces (docs/re/pes-re-lighting.md Part C §3 table).
TEST(FilmCurve, ReproducesReportedGlobalAndSpanSets) {
  const pes::FilmCurve global = pes::MakeFilmCurve(0.88, 0.45, 0.55, 0.0, 0.02);
  EXPECT_NEAR(global.toeMatch, -0.23491, 1e-4);
  EXPECT_NEAR(global.straightMatch, 0.85991, 1e-4);
  EXPECT_NEAR(global.shoulderMatch, -0.23491, 1e-4);
  const pes::FilmCurve span = pes::MakeFilmCurve(0.88, 0.55, 0.26, 0.0, 0.04);
  EXPECT_NEAR(span.toeMatch, -0.39028, 1e-4);
  EXPECT_NEAR(span.straightMatch, 0.90164, 1e-4);
  EXPECT_NEAR(span.shoulderMatch, -0.60619, 1e-4);
}

// A toe above the logarithmic branch's threshold still gives finite matches.
TEST(FilmCurve, HighToeUsesLinearBranchAndStaysFinite) {
  const pes::FilmCurve f = pes::MakeFilmCurve(0.88, 0.9, 0.1, 0.0, 0.0);
  EXPECT_TRUE(std::isfinite(f.toeMatch));
  EXPECT_TRUE(std::isfinite(f.straightMatch));
  EXPECT_TRUE(std::isfinite(f.shoulderMatch));
}

// At the daylight locus with no tint the white balance is the identity to
// about 1e-3: the 6500 K source is within a few thousandths of D65.
TEST(WhiteBalance, NoTintAtSixThousandFiveHundredIsNearIdentity) {
  const pes::Mat3 m = pes::WhiteBalanceRgb(6500.0, 0.0);
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) EXPECT_NEAR(m[i][j], i == j ? 1.0 : 0.0, 1e-3) << i << "," << j;
}

// Positive tint at 6500 K lowers green and raises red and blue, the report's
// magenta direction (docs/re/pes-re-lighting.md Part C §3).
TEST(WhiteBalance, PositiveTintLowersGreenAndRaisesRedAndBlue) {
  const pes::Mat3 m = pes::WhiteBalanceRgb(6500.0, 1.0);
  EXPECT_GT(m[0][0], 1.0);
  EXPECT_LT(m[1][1], 1.0);
  EXPECT_GT(m[2][2], 1.0);
}

// Regression oracle: the report's live matrix at 6500 K, tint 1, read from
// TonemapFilterDx11+0x10 during a match (docs/re/pes-re-lighting.md Part C §3).
// The stored rows are the transpose of the column-convention matrix the
// shader applies, so m[i][j] == stored[j][i].
TEST(WhiteBalance, MatchesReportedLiveMatrixAtTintOne) {
  const pes::Mat3 m = pes::WhiteBalanceRgb(6500.0, 1.0);
  const double stored[3][3] = {
      {1.234, -0.0223, 0.01165}, {0.4398, 0.8271, 0.06639}, {-0.007835, -0.01944, 1.679}};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) EXPECT_NEAR(m[i][j], stored[j][i], 2e-3) << i << "," << j;
}

// PES runs floor(10·size) Kawase passes, at least one (docs/re/pes-re-lighting.md Part C
// §2, FUN_141804af0 with floorf).
TEST(BloomPasses, KernelCountIsFloorOfTenTimesSize) {
  EXPECT_EQ(pes::BloomKernelCount(0.8), 8);
  EXPECT_EQ(pes::BloomKernelCount(0.4), 4);
  EXPECT_EQ(pes::BloomKernelCount(0.3), 3);
  EXPECT_EQ(pes::BloomKernelCount(0.05), 1);
}

// 0.1 <= size < 0.2 takes PES's single copy branch, not the Kawase loop.
TEST(BloomPasses, CopyBranchCoversTenthToFifthOfSize) {
  EXPECT_TRUE(pes::BloomCopyBranch(0.1));
  EXPECT_TRUE(pes::BloomCopyBranch(0.15));
  EXPECT_FALSE(pes::BloomCopyBranch(0.2));
  EXPECT_FALSE(pes::BloomCopyBranch(0.8));
}
}  // namespace
