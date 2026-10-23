// PES 2021's S1 rows (onthepitch/pesscorerows.hpp, docs/re/pes-re-s1-writer.md). The expected
// values are worked by hand from concrete positions with the goal centre at (+-52.5, 0) and
// yg = 34, so each one can be checked on paper. Nothing here recomputes the writer.
#include <gtest/gtest.h>

#include <cmath>

#include "onthepitch/pesscorerows.hpp"

namespace {

constexpr float kXg = 52.5f;  // goal-centre x for these tests
constexpr float kYg = 34.0f;  // goal y for these tests
constexpr float kPi = 3.14159265358979f;
constexpr float kBearingTol_deg = 1e-3f;

pes::ScoreRowConstants Goal() {
  pes::ScoreRowConstants k;
  k.goalX_m = kXg;
  k.goalY_m = kYg;
  k.radToDeg = 180.0f / kPi;
  k.twoPi = 2.0f * kPi;
  return k;
}

// Puts a player on the pitch at (x, z).
void Place(pes::RowPlayers& p, int slot, float x, float z) { p[slot] = pes::RowPlayer{x, z, true}; }

}  // namespace

TEST(ScoreRows, GoalDistanceIsTheSquareFromEachGoalCentre) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, -4.0f, 3.0f);
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_FLOAT_EQ(r.score[0].ValueOf(0), 3906.25f);  // (-52.5 - 10)^2 + 0^2
  EXPECT_FLOAT_EQ(r.score[1].ValueOf(0), 1806.25f);  // (52.5 - 10)^2 + 0^2
  EXPECT_FLOAT_EQ(r.score[0].ValueOf(1), 2361.25f);  // (-52.5 + 4)^2 + 3^2
  EXPECT_FLOAT_EQ(r.score[1].ValueOf(1), 3201.25f);  // (52.5 + 4)^2 + 3^2
}

TEST(ScoreRows, PairAndTouchlineRowsAreAbsoluteDistances) {
  pes::RowPlayers p{};
  Place(p, 1, -4.0f, 3.0f);
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_FLOAT_EQ(r.pair[0].ValueOf(1), 31.0f);  // |34 - 3|
  EXPECT_FLOAT_EQ(r.pair[1].ValueOf(1), 37.0f);  // |-34 - 3|
  EXPECT_FLOAT_EQ(r.list[0].ValueOf(1), 48.5f);  // |-52.5 + 4|
  EXPECT_FLOAT_EQ(r.list[1].ValueOf(1), 56.5f);  // |52.5 + 4|
}

TEST(ScoreRows, BearingsAreAboutEachGoalCentreInDegreesFoldedToZeroTo360) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 2, 0.0f, 10.0f);
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_NEAR(r.angle[0][0], 90.0f, kBearingTol_deg);        // atan2(62.5, 0)
  EXPECT_NEAR(r.angle[1][0], 270.0f, kBearingTol_deg);       // atan2(-42.5, 0), folded
  EXPECT_NEAR(r.angle[0][2], 79.215702f, kBearingTol_deg);   // atan2(52.5, 10)
  EXPECT_NEAR(r.angle[1][2], 280.784298f, kBearingTol_deg);  // 360 + atan2(-52.5, 10)
}

TEST(ScoreRows, NearestListIsSortedByDistanceWithTheTeamSplitAtSlot11) {
  pes::RowPlayers p{};
  Place(p, 0, 0.0f, 0.0f);
  Place(p, 1, 3.0f, 0.0f);   // team 0, d^2 9
  Place(p, 2, 1.0f, 0.0f);   // team 0, d^2 1
  Place(p, 11, 0.0f, 2.0f);  // team 1, d^2 4
  Place(p, 12, 0.0f, 0.5f);  // team 1, d^2 0.25
  p[3] = pes::RowPlayer{0.5f, 0.0f, false};  // off the pitch: not listed
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/true);
  const pes::ScoreRows& r = w.Rows();
  const pes::SortedRow& nl = r.nearest[0];
  EXPECT_EQ(nl.Nearest(0), 2);  // team 0: slot 2 (d^2 1) before slot 1 (d^2 9)
  EXPECT_EQ(nl.RankOf(1), 1);
  EXPECT_FLOAT_EQ(nl.ValueOf(2), 1.0f);
  EXPECT_FLOAT_EQ(nl.ValueOf(1), 9.0f);
  EXPECT_EQ(nl.Nearest(1), 12);  // team 1: slot 12 (0.25) before slot 11 (4)
  EXPECT_EQ(nl.RankOf(11), 1);
  EXPECT_TRUE(std::isnan(nl.ValueOf(3)));                    // off the pitch: not listed
  EXPECT_EQ(r.nearest[3].Nearest(0), pes::lane::kNoSlot);  // an off-pitch owner has no list
}

TEST(ScoreRows, EachCallRefreshesItsMaskAndTheBlocksAgeUpToFourFrames) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, -4.0f, 3.0f);
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/true);  // every block at the first call
  Place(p, 0, 20.0f, 0.0f);                     // slot 0 moves 10 m towards the +x goal
  w.Tick(p, Goal(), 1, /*fullRefresh=*/false);  // frame 1 -> mask 0x02: score1, list1, angle1
  EXPECT_FLOAT_EQ(w.Rows().score[1].ValueOf(0), 1056.25f);  // (52.5 - 20)^2, refreshed
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 3906.25f);  // stale: still the frame-0 value
  EXPECT_FLOAT_EQ(w.Rows().nearest[0].ValueOf(1), 585.0f);  // the NearestList is rebuilt: 24^2 + 3^2
  w.Tick(p, Goal(), 2, /*fullRefresh=*/false);  // mask 0x24: the pairs
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 3906.25f);
  w.Tick(p, Goal(), 3, /*fullRefresh=*/false);  // mask 0x10: the NearestList only
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 3906.25f);
  w.Tick(p, Goal(), 4, /*fullRefresh=*/false);  // mask 0x01: score0, four frames after frame 0
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 5256.25f);  // (-52.5 - 20)^2
}

TEST(ScoreRows, TheWriterIsLatchedByTheFirstFullRefresh) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/false);  // TA+0x11 is 0: the writer does not run
  EXPECT_FALSE(w.Latched());
  EXPECT_TRUE(std::isnan(w.Rows().score[0].ValueOf(0)));
  w.Tick(p, Goal(), 1, /*fullRefresh=*/true);  // the flag latches it and writes every block
  EXPECT_TRUE(w.Latched());
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 3906.25f);
}

TEST(ScoreRows, TheRowOfAnAttackSignIsTheGoalBehindTheAttack) {
  // FUN_140A714C0(T, k) reads score0 (MA+0x228EC, the goal at -xg) when k == 1 and score1 otherwise;
  // the Safety gate's lane length reads the other row (FUN_140A714A0). Attacking +x (k = +1), the
  // score is the distance to -xg, the goal behind the team.
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/true);
  EXPECT_FLOAT_EQ(pes::ScoreRowForSign(w.Rows(), 1).ValueOf(0), 3906.25f);   // (-52.5 - 10)^2
  EXPECT_FLOAT_EQ(pes::ScoreRowForSign(w.Rows(), -1).ValueOf(0), 1806.25f);  // (52.5 - 10)^2
  EXPECT_FLOAT_EQ(pes::GateRowForSign(w.Rows(), 1).ValueOf(0), 1806.25f);
  EXPECT_FLOAT_EQ(pes::GateRowForSign(w.Rows(), -1).ValueOf(0), 3906.25f);
}

TEST(ScoreRows, PairBearingsAreOwnerMinusOtherFoldedToZeroTo360) {
  // FUN_14045B420's pair table (MA+0x23F7C, read by FUN_140439FE0): P[a][b] is the bearing of
  // (a - b), folded to [0, 360); 0 when the two coincide.
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, 10.0f, 10.0f);
  Place(p, 2, 10.0f, 0.0f);  // coincident with slot 0: bearing 0
  pes::ScoreRowWriter w;
  w.Tick(p, Goal(), 0, /*fullRefresh=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_NEAR(r.bearing[0][1], 180.0f, kBearingTol_deg);  // atan2(0, -10)
  EXPECT_NEAR(r.bearing[1][0], 0.0f, kBearingTol_deg);    // atan2(0, 10)
  EXPECT_NEAR(r.bearing[0][2], 0.0f, kBearingTol_deg);    // coincident: 0, not NaN
}
