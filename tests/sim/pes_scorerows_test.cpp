// PES 2021's S1 rows (onthepitch/pesscorerows.hpp, docs/re/pes-re-s1-writer.md). The expected values are worked by hand
// from concrete positions with the goal centre at (+-52.5, 0) and yg = 34, so each one can be checked on paper. The goal
// angles and the pair table are PES's bearing (0x140439EF0) over the men's records; the bit-exact checks run that same
// function on the same records.
#include <gtest/gtest.h>

#include <cmath>

#include "onthepitch/pesscorerows.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace {

constexpr float kXg = 52.5f;  // goal-centre x for these tests
constexpr float kYg = 34.0f;  // goal y for these tests
constexpr float kBearingTol_deg = 1e-3f;

pes::ScoreRowConstants Goal() {
  pes::ScoreRowConstants k;
  k.goalX_m = kXg;
  k.goalY_m = kYg;
  return k;
}

// The world the men's records live in, as Match holds it.
pes::PesWorld& World() {
  static pes::PesWorld world;
  return world;
}

// Puts a player on the pitch at (x, z): his record holds the position, and his row entry carries that record.
void Place(pes::RowPlayers& p, int slot, float x, float z) {
  World().BindSlot(slot);
  uint8_t* man = World().ManRecord(slot);
  pes::StoreF32(man, pes::kManPosXOffset, x);
  pes::StoreF32(man, pes::kManPosZOffset, z);
  p[slot] = pes::RowPlayer{x, z, true, man};
}

void Tick(pes::ScoreRowWriter& w, const pes::RowPlayers& p, unsigned long frame, bool fullRefresh, bool pesPath) {
  w.Tick(p, Goal(), frame, fullRefresh, pesPath, World());
}

}  // namespace

TEST(ScoreRows, GoalDistanceIsTheSquareFromEachGoalCentre) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, -4.0f, 3.0f);
  pes::ScoreRowWriter w;
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
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
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
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
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_NEAR(r.angle[0][0], 90.0f, kBearingTol_deg);        // atan2(62.5, 0)
  EXPECT_NEAR(r.angle[1][0], 270.0f, kBearingTol_deg);       // atan2(-42.5, 0), folded
  EXPECT_NEAR(r.angle[0][2], 79.218506f, kBearingTol_deg);   // PES's bearing (0x140439EF0); libm atan2 gives 79.215702
  EXPECT_NEAR(r.angle[1][2], 280.781494f, kBearingTol_deg);  // 360 + PES's bearing of (-52.5, 10); libm gives 280.784298
}

// The goal angles are PES's bearing from the goal centre to the man, bit for bit.
TEST(ScoreRows, GoalAngleIsPESsBearingFromTheGoalCentre) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 2, -3.0f, 17.0f);
  pes::ScoreRowWriter w;
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_EQ(r.angle[0][0], World().BearingOfPoints(-kXg, 0.0f, 10.0f, 0.0f));
  EXPECT_EQ(r.angle[1][2], World().BearingOfPoints(kXg, 0.0f, -3.0f, 17.0f));
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
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
  const pes::ScoreRows& r = w.Rows();
  const pes::SortedRow& nl = r.nearest[0];
  EXPECT_EQ(nl.Nearest(0), 2);  // team 0: slot 2 (d^2 1) before slot 1 (d^2 9)
  EXPECT_EQ(nl.RankOf(1), 1);
  EXPECT_FLOAT_EQ(nl.ValueOf(2), 1.0f);
  EXPECT_FLOAT_EQ(nl.ValueOf(1), 9.0f);
  EXPECT_EQ(nl.Nearest(1), 12);  // team 1: slot 12 (0.25) before slot 11 (4)
  EXPECT_EQ(nl.RankOf(11), 1);
  EXPECT_TRUE(std::isnan(nl.ValueOf(3)));                  // off the pitch: not listed
  EXPECT_EQ(r.nearest[3].Nearest(0), pes::lane::kNoSlot);  // an off-pitch owner has no list
}

TEST(ScoreRows, EachCallRefreshesItsMaskAndTheBlocksAgeUpToFourFrames) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, -4.0f, 3.0f);
  pes::ScoreRowWriter w;
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);  // every block at the first call
  Place(p, 0, 20.0f, 0.0f);                                 // slot 0 moves 10 m towards the +x goal
  Tick(w, p, 1, /*fullRefresh=*/false, /*pesPath=*/true);   // frame 1 -> mask 0x02: score1, list1, angle1
  EXPECT_FLOAT_EQ(w.Rows().score[1].ValueOf(0), 1056.25f);  // (52.5 - 20)^2, refreshed
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 3906.25f);  // stale: still the frame-0 value
  EXPECT_FLOAT_EQ(w.Rows().nearest[0].ValueOf(1), 585.0f);  // the NearestList is rebuilt: 24^2 + 3^2
  Tick(w, p, 2, /*fullRefresh=*/false, /*pesPath=*/true);   // mask 0x24: the pairs
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 3906.25f);
  Tick(w, p, 3, /*fullRefresh=*/false, /*pesPath=*/true);   // mask 0x10: the NearestList only
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 3906.25f);
  Tick(w, p, 4, /*fullRefresh=*/false, /*pesPath=*/true);   // mask 0x01: score0, four frames after frame 0
  EXPECT_FLOAT_EQ(w.Rows().score[0].ValueOf(0), 5256.25f);  // (-52.5 - 20)^2
}

TEST(ScoreRows, TheWriterIsLatchedByTheFirstFullRefresh) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  pes::ScoreRowWriter w;
  Tick(w, p, 0, /*fullRefresh=*/false, /*pesPath=*/true);  // TA+0x11 is 0: the writer does not run
  EXPECT_FALSE(w.Latched());
  EXPECT_TRUE(std::isnan(w.Rows().score[0].ValueOf(0)));
  Tick(w, p, 1, /*fullRefresh=*/true, /*pesPath=*/true);   // the flag latches it and writes every block
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
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
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
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_NEAR(r.bearing[0][1], 180.0f, kBearingTol_deg);  // atan2(0, -10)
  EXPECT_NEAR(r.bearing[1][0], 0.0f, kBearingTol_deg);    // atan2(0, 10)
  EXPECT_EQ(r.bearing[0][2], 0.0f);  // coincident: the zero arm, exactly 0
  EXPECT_EQ(r.bearing[2][0], 0.0f);
}

// The pair table is PES's bearing over the two men's records, bit for bit: the same function run on the same records.
TEST(ScoreRows, PairTableIsPESsBearingOfTheRecords) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, -4.0f, 3.0f);
  pes::ScoreRowWriter w;
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
  const pes::ScoreRows& r = w.Rows();
  EXPECT_EQ(r.bearing[0][1], World().Bearing(p[1].man, p[0].man));  // a - b: the bearing from b to a
  EXPECT_EQ(r.bearing[1][0], World().Bearing(p[0].man, p[1].man));
}

// The pair table reads the men's records, not the writer's copies: a stale copy must not reach it.
TEST(ScoreRows, PairTableReadsTheManRecordNotTheCopy) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, 10.0f, 10.0f);
  p[1].x = 99.0f;  // the copy moves; the record still says (10, 10)
  pes::ScoreRowWriter w;
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/true);
  EXPECT_NEAR(w.Rows().bearing[0][1], 180.0f, kBearingTol_deg);
}

// GF mode keeps its old rows exactly: BuildScoreRows takes the PES path flag,
// and off that path the pair table stays zero (no PES reader runs there).
TEST(ScoreRows, GfModeLeavesThePairTableZero) {
  pes::RowPlayers p{};
  Place(p, 0, 10.0f, 0.0f);
  Place(p, 1, 10.0f, 10.0f);
  pes::ScoreRowWriter w;
  Tick(w, p, 0, /*fullRefresh=*/true, /*pesPath=*/false);
  const pes::ScoreRows& r = w.Rows();
  for (const auto& row : r.bearing)
    for (int i = 0; i < pes::lane::kSlots; ++i) EXPECT_EQ(row[i], 0.0f);
}
