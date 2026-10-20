// PES 2021's dodge selectors (onthepitch/pesdodge.hpp): synthetic tables check
// each layout the exe references; one install test reads the user's exe, the
// Animetable and the Mbinfo bins and checks the rows resolve to the dodge
// clip families.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "onthepitch/pesdodge.hpp"
#include "pes/mbinfo.hpp"

namespace {

pes::AnimetableLayout Layout() {
  pes::AnimetableLayout l;
  l.rows = 900;
  l.cols = 18;
  l.motionMask = 0x1fff;
  l.mirrorShift = 13;
  l.columnStep_deg = 22.5f;
  l.columnOrigin_deg = 180.0f;
  l.zeroLeft_deg = -0.001f;
  l.zeroRight_deg = 0.001f;
  l.noRow = 900;
  return l;
}

// A synthetic exe is not available (the constants are read from the real
// executable), so the selector tests build the constants by hand and only the
// install test exercises LoadDodgeConstants.
pes::DodgeConstants Constants() {
  pes::DodgeConstants k;
  // The install's kind -> row table (0x14344efb0).
  const int rows[8][3] = {{753, 754, 755}, {757, 757, 757}, {758, 758, 758},
                          {763, 763, 764}, {765, 765, 766}, {760, 760, 762},
                          {771, 771, 771}, {769, 767, 767}};
  for (int i = 0; i < 8; ++i)
    for (int s = 0; s < 3; ++s) k.kindRow[i][s] = rows[i][s];
  // The install's ball grid (0x1425e6650).
  const int grid[4][3] = {{757, 757, 757}, {775, 779, 779}, {776, 780, 780}, {777, 781, 781}};
  for (int r = 0; r < 4; ++r)
    for (int s = 0; s < 3; ++s) k.ballGrid[r][s] = grid[r][s];
  k.ballRow4A = 763, k.ballRow4B = 763, k.ballRow4C = 764;
  // The install's band table (0x14264866c).
  const float bands[12] = {0.335f, 0.56f, 0.785f, 1.01f, 1.235f, 1.46f,
                           1.685f, 1.91f, 2.135f, 2.36f, 2.585f, 3.08f};
  for (int i = 0; i < 12; ++i) k.ballHeightBands[i] = bands[i];
  k.netRowDefault = 0x310;
  k.netRowFast = 0x30e;
  // The install's speed-class bounds (0x1425a1ffc / 0x1425c48c0).
  k.speedClass1_kmh = 13.5f;
  k.speedClass2_kmh = 22.0f;
  // The install's post grid is now DodgeConstants' own default
  // (0x1425e6680's 8 loaded ints plus the selector's 0x316).
  k.postSideAngle_deg = 30.0f;     // DAT_142591140
  k.postSpeedFloor_kmh = 8.0f;     // DAT_14259bdf4
  k.postColumnUp_deg = 45.0f;      // DAT_14259be30
  k.postColumnHigh_deg = 135.0f;   // DAT_14259be60
  k.postColumnBailout_deg = 60.0f; // DAT_14259a764
  k.postBailout_deg = 120.0f;      // DAT_14256ae30
  for (int i = 0; i < 9; ++i) k.kindColumn[i] = (i < 3 ? 9 : i < 6 ? 5 : i == 6 ? 7 : i == 7 ? 8 : 11);
  return k;
}

// A table with the dodge rows populated at their real columns.
std::vector<uint8_t> Table() {
  std::vector<uint32_t> cells(900 * 18, 0);
  auto put = [&](int row, int col, int motion) { cells[row * 18 + col] = uint32_t(motion + 1); };
  for (int s : {0, 1, 2, 3, 4, 5, 6, 7}) {
    put(753 + s, 8, 18 + s);  // avoidjump family rows, col 8
    put(753 + s, 9, 18 + s);  // and col 9
  }
  put(763, 8, 24);
  put(763, 9, 24);
  put(0x310, 9, 2068);  // goalnet_stop_slow
  put(0x30e, 9, 2067);  // goalnet_stop_rapid
  put(0x2fb, 9, 24);    // avoidslide_3_3_000 (post, side 0 slow)
  put(0x2fc, 9, 28);    // avoidslide_4_4_000 (post, side 0 fast)
  put(0x313, 9, 2070);  // goalpost_stop_2_0_000 (post, side 1 slow)
  put(0x314, 9, 2074);  // goalpost_stop_3_0_000 (post, side 1 fast)
  put(0x315, 9, 2071);  // goalpost_stop_2_0_000_side (post, side 2 slow)
  put(0x313, 13, 2072); // goalpost_stop_2_0_090 (the stepped-up column)
  put(0x313, 16, 2073); // goalpost_stop_2_0_160 (the install's top column)
  put(0x316, 9, 2075);  // goalpost_stop_3_0_000_side (the grid's ninth cell)
  put(0x2f5, 9, 19);   // avoidjump_3_3
  put(0x2f1, 9, 18);   // avoidjump_2_2_low
  std::vector<uint8_t> b(cells.size() * 4);
  std::memcpy(b.data(), cells.data(), b.size());
  return b;
}

TEST(PesDodge, BallSelectorReadsTheForcedRowAtColumn9) {
  auto t = pes::ParseAnimetable(Table(), Layout(), nullptr);
  ASSERT_TRUE(t.has_value());
  std::vector<int> forced = {757, 757, 757};
  auto c = pes::BallSelector(*t, Constants(), 0, forced);
  ASSERT_TRUE(c.has_value());
  EXPECT_EQ(c->motionNo, 19);  // avoidjump_3_3
  EXPECT_EQ(c->col, 9);
  EXPECT_EQ(c->row, 757);
}

TEST(PesDodge, BallSelectorRefusesAnEmptyRow) {
  auto t = pes::ParseAnimetable(Table(), Layout(), nullptr);
  ASSERT_TRUE(t.has_value());
  std::vector<int> forced = {757, 757, 757};
  // Speed class 2 maps to a row that is not populated in the synthetic table.
  forced[2] = 500;
  EXPECT_FALSE(pes::BallSelector(*t, Constants(), 2, forced).has_value());
}

TEST(PesDodge, NetSelectorPicksTheSlowAndFastRows) {
  auto t = pes::ParseAnimetable(Table(), Layout(), nullptr);
  ASSERT_TRUE(t.has_value());
  auto slow = pes::NetSelector(*t, Constants(), false);
  ASSERT_TRUE(slow.has_value());
  EXPECT_EQ(slow->motionNo, 2068);  // goalnet_stop_slow
  auto fast = pes::NetSelector(*t, Constants(), true);
  ASSERT_TRUE(fast.has_value());
  EXPECT_EQ(fast->motionNo, 2067);  // goalnet_stop_rapid
}

TEST(PesDodge, PostSelectorPicksTheSideRowAndColumn) {
  auto t = pes::ParseAnimetable(Table(), Layout(), nullptr);
  ASSERT_TRUE(t.has_value());
  const pes::DodgeConstants k = Constants();
  // Lined up with the post's axis (bearing 0, ball coming at 0): side 0, the
  // slow row at column 9.
  auto c0 = pes::PostSelector(*t, k, 0.0f, 0.0f, 0.0f, 10.0f);
  ASSERT_TRUE(c0.has_value());
  EXPECT_EQ(c0->row, 0x2fb);
  EXPECT_EQ(c0->col, 9);
  // Inside the near branch (bearing under 45 off the axis) but past the 30-deg
  // split: the deep side 1. The ball's own angle picks the column (9 under 45,
  // 13 above it, 17 above 135).
  auto c1 = pes::PostSelector(*t, k, 35.0f, 0.0f, 60.0f, 10.0f);
  ASSERT_TRUE(c1.has_value());
  EXPECT_EQ(c1->row, 0x313);
  EXPECT_EQ(c1->col, 13);
  auto c1b = pes::PostSelector(*t, k, 35.0f, 0.0f, 10.0f, 10.0f);
  ASSERT_TRUE(c1b.has_value());
  EXPECT_EQ(c1b->row, 0x313);
  EXPECT_EQ(c1b->col, 9);
  auto c1c = pes::PostSelector(*t, k, 35.0f, 0.0f, 140.0f, 10.0f);
  ASSERT_TRUE(c1c.has_value());
  EXPECT_EQ(c1c->col, 16);  // the install's top column, not the exe's 17
  // Under the 30-deg split the near branch's shallow side 0 stands.
  auto c0b = pes::PostSelector(*t, k, 20.0f, 0.0f, 60.0f, 10.0f);
  ASSERT_TRUE(c0b.has_value());
  EXPECT_EQ(c0b->row, 0x2fb);
  // The far branch (bearing past 45 off the axis): only the window between 60
  // and 120.
  auto c2 = pes::PostSelector(*t, k, 60.0f, 0.0f, 90.0f, 10.0f);
  ASSERT_TRUE(c2.has_value());
  EXPECT_EQ(c2->row, 0x315);
  EXPECT_FALSE(pes::PostSelector(*t, k, 60.0f, 0.0f, 30.0f, 10.0f).has_value());
  EXPECT_FALSE(pes::PostSelector(*t, k, 60.0f, 0.0f, 150.0f, 10.0f).has_value());
  // The far branch's fast class reads the grid's ninth cell, which the exe
  // writes on the stack rather than carrying in its table: the 9th int at
  // 0x1425e6680 is the 8.0f of the next constant, not a row.
  auto farFast = pes::PostSelector(*t, k, 60.0f, 0.0f, 90.0f, 25.0f);
  ASSERT_TRUE(farFast.has_value());
  EXPECT_EQ(farFast->row, 0x316);
  // The speed class picks the grid's column: fast moves to the fast row.
  auto fast = pes::PostSelector(*t, k, 0.0f, 0.0f, 0.0f, 25.0f);
  ASSERT_TRUE(fast.has_value());
  EXPECT_EQ(fast->row, 0x2fc);
  auto mid = pes::PostSelector(*t, k, 0.0f, 0.0f, 0.0f, 15.0f);
  ASSERT_TRUE(mid.has_value());
  EXPECT_EQ(mid->row, 0x2fb);
  // Under the 8 km/h floor there is no row at all.
  EXPECT_FALSE(pes::PostSelector(*t, k, 0.0f, 0.0f, 0.0f, 5.0f).has_value());
  // An empty cell returns no clip.
  std::vector<uint32_t> cells(900 * 18, 0);
  cells[0x2fb * 18 + 9] = 24 + 1;
  std::vector<uint8_t> b(cells.size() * 4);
  std::memcpy(b.data(), cells.data(), b.size());
  auto t2 = pes::ParseAnimetable(b, Layout(), nullptr);
  ASSERT_TRUE(t2.has_value());
  pes::DodgeConstants k2 = k;
  k2.postGrid[0][0] = -1;  // a side/class the install does not carry
  EXPECT_FALSE(pes::PostSelector(*t2, k2, 0.0f, 0.0f, 0.0f, 25.0f).has_value());
}

TEST(PesDodge, KindColumnFollowsTheJumpTable) {
  const pes::DodgeConstants k = Constants();
  EXPECT_EQ(pes::KindColumn(k, 0), 9);
  EXPECT_EQ(pes::KindColumn(k, 1), 9);
  EXPECT_EQ(pes::KindColumn(k, 2), 9);
  EXPECT_EQ(pes::KindColumn(k, 3), 5);
  EXPECT_EQ(pes::KindColumn(k, 4), 5);
  EXPECT_EQ(pes::KindColumn(k, 5), 5);
  EXPECT_EQ(pes::KindColumn(k, 6), 7);
  EXPECT_EQ(pes::KindColumn(k, 7), 8);
  EXPECT_EQ(pes::KindColumn(k, 8), 11);  // the default
}

TEST(PesDodge, SpeedClassFollowsFUN_140736d30) {
  EXPECT_EQ(pes::SpeedClass(5.0f, 0.0f), 0);   // below the first bound
  EXPECT_EQ(pes::SpeedClass(15.0f, 0.0f), 1);  // between
  EXPECT_EQ(pes::SpeedClass(25.0f, 0.0f), 2);  // above the second
  EXPECT_EQ(pes::SpeedClass(25.0f, 90.0f), 0);  // heading off by more than 45°
}

TEST(PesDodge, BallBandFollowsFUN_140a6d380) {
  const pes::DodgeConstants k = Constants();
  EXPECT_EQ(pes::BallBand(k, 0.1f, 1.0f), 0);
  EXPECT_EQ(pes::BallBand(k, 0.5f, 1.0f), 1);
  EXPECT_EQ(pes::BallBand(k, 1.0f, 1.0f), 3);
  EXPECT_EQ(pes::BallBand(k, 3.0f, 1.0f), 11);  // above the last threshold
  EXPECT_EQ(pes::BallBand(k, 0.4f, 0.8f), 1);   // scaled by the body
}

TEST(PesDodge, KindRowStepsDownUntilTheColumnIsPopulated) {
  auto t = pes::ParseAnimetable(Table(), Layout(), nullptr);
  ASSERT_TRUE(t.has_value());
  // Kind 1 is row 757 at every speed class; the column-9 fit passes without a
  // ball owner.
  EXPECT_EQ(pes::KindRow(*t, pes::Mbinfo{}, Constants(), 1, 25.0f, false, 0.0f), 757);
  EXPECT_EQ(pes::KindRow(*t, pes::Mbinfo{}, Constants(), 1, 15.0f, false, 0.0f), 757);
  EXPECT_EQ(pes::KindRow(*t, pes::Mbinfo{}, Constants(), 1, 5.0f, false, 0.0f), 757);
  // Kind 8 is out of range.
  EXPECT_EQ(pes::KindRow(*t, pes::Mbinfo{}, Constants(), 8, 15.0f, false, 0.0f), -1);
}

TEST(PesDodge, KindRowFallsThroughAnUnpopulatedClass) {
  auto t = pes::ParseAnimetable(Table(), Layout(), nullptr);
  ASSERT_TRUE(t.has_value());
  // A kind whose fast class is empty steps down to the slow class.
  std::vector<uint32_t> cells(900 * 18, 0);
  cells[763 * 18 + 8] = 24 + 1;  // only the slow row of kind 3 populated
  cells[763 * 18 + 9] = 24 + 1;
  std::vector<uint8_t> b(cells.size() * 4);
  std::memcpy(b.data(), cells.data(), b.size());
  auto t2 = pes::ParseAnimetable(b, Layout(), nullptr);
  ASSERT_TRUE(t2.has_value());
  // kind 3 rows: 763 (slow), 763, 764 (fast). Fast is empty -> steps to 763.
  EXPECT_EQ(pes::KindRow(*t2, pes::Mbinfo{}, Constants(), 3, 25.0f, false, 0.0f), 763);
}

TEST(PesDodgeInstall, TheUsersTablesAndRows) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto k = pes::LoadDodgeConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  EXPECT_EQ(k->lastEventRollDivisor, 54);           // gs: GAME_SPEED_VA, 0x14351ae50
  EXPECT_FLOAT_EQ(k->lastEventWindowFactor, 0.2f);  // DAT_142598578
  auto t = pes::LoadAnimetable(*exe, vfs, &err);
  ASSERT_TRUE(t.has_value()) << err;
  auto mb = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(mb.has_value()) << err;

  // The kind -> row table resolves every kind to a row whose column 9 holds a
  // clip of the avoid family (the human dodges).
  int resolved = 0;
  for (int kind = 0; kind < 8; ++kind) {
    const int row = pes::KindRow(*t, *mb, *k, kind, 5.0f, false, 0.0f);
    ASSERT_GE(row, 0) << "kind " << kind;
    EXPECT_TRUE(t->Populated(row, pes::KindColumn(*k, kind)) ||
                t->Populated(row, 9))
        << "kind " << kind << " row " << row;
    ++resolved;
  }
  EXPECT_EQ(resolved, 8);

  // The net selector resolves to the goal-net family on both rows.
  auto slow = pes::NetSelector(*t, *k, false);
  ASSERT_TRUE(slow.has_value());
  EXPECT_EQ(slow->row, 0x310);
  auto fast = pes::NetSelector(*t, *k, true);
  ASSERT_TRUE(fast.has_value());
  EXPECT_EQ(fast->row, 0x30e);

  // The ball selector resolves the jump row.
  std::vector<int> forced = {757, 757, 757};
  auto ball = pes::BallSelector(*t, *k, 0, forced);
  ASSERT_TRUE(ball.has_value());
  EXPECT_EQ(ball->row, 757);

  // The post selector resolves a row of the install's own grid, and that row
  // carries a clip of the goalpost family at the selector's column.
  int postResolved = 0;
  for (int s = 0; s < 3; ++s) {
    const int row = k->postGrid[s][0];
    if (row < 0) continue;
    // Side 0 and 1 come out of the near branch, side 2 out of the far branch.
    const float bearing = s == 2 ? 90.0f : (s == 1 ? 35.0f : 0.0f);
    const float approach = s == 2 ? 90.0f : 0.0f;
    auto clip = pes::PostSelector(*t, *k, bearing, 0.0f, approach, 10.0f);
    if (!clip || clip->row != row) {
      // A grid cell the install's own table leaves empty: an install fact,
      // not a port failure.
      EXPECT_FALSE(t->Populated(row, 9)) << "post row " << row << " col 9 empty";
      continue;
    }
    EXPECT_GE(clip->motionNo, 0) << "post row " << row;
    ++postResolved;
  }
  EXPECT_GE(postResolved, 1);

  // The post grid's rows are the install's goalpost family rows.
  EXPECT_EQ(k->postGrid[0][0], 0x2fb);
  EXPECT_EQ(k->postGrid[1][0], 0x313);
  EXPECT_EQ(k->postGrid[2][0], 0x315);
  auto post0 = pes::PostSelector(*t, *k, 0.0f, 0.0f, 0.0f, 10.0f);
  ASSERT_TRUE(post0.has_value());
  EXPECT_EQ(post0->row, 0x2fb);
  EXPECT_EQ(post0->col, 9);

  // Install fact: the exe writes column 0x11 (17) in the deep branch's >135 deg
  // band and 0xd (13) below it. Column 13 is a raw index and matches the
  // install's table exactly (its angular centre is 90 deg, the "090" clip).
  // Column 17's angular centre is 180 deg, where this install has nothing; the
  // 160 clip it means sits at column 16 (centre 157.5 deg, the nearest bucket).
  // Pinned so a live poke can confirm the pre/post-shift mapping.
  EXPECT_FALSE(t->Populated(0x313, 17));  // the exe's column is empty here
  EXPECT_TRUE(t->Populated(0x313, 16));   // the install's own 160-deg clip
  EXPECT_TRUE(t->Populated(0x313, 13));   // the exe's 0xd matches

  // The grid's ninth cell is not in the exe's table: reading past the two
  // MOVDQA loads picks up the next constant's 8.0f. The selector's own stack
  // value 0x316 is the row, and it carries the far side's fast clip.
  EXPECT_EQ(k->postGrid[2][2], 0x316);
  EXPECT_TRUE(t->Populated(0x316, 9));
}

TEST(PesDodgeInstall, TheGoalGateConstantsAreTheExesOwn) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto k = pes::LoadDodgeConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;
  // FUN_140a07e40's and FUN_140a084d0's own constants: the edge margin
  // (0x14256ae1c), the residual (0x14259be0c), and the step numerator
  // (0x14253ee30) over the divisor (0x1425cb47c).
  EXPECT_FLOAT_EQ(k->netEdgeMargin_m, 1.0f);
  EXPECT_FLOAT_EQ(k->netEdgeRejection_deg, 15.0f);
  EXPECT_FLOAT_EQ(k->netStepNumerator, 100.0f);
  EXPECT_FLOAT_EQ(k->netStepDivisor, 72.0f);
  // FUN_140a084d0's distances: 5.0 (0x14259222c), 0.5 (0x142565600), 2.0 (0x142565604).
  EXPECT_FLOAT_EQ(k->postProximity_m, 5.0f);
  EXPECT_FLOAT_EQ(k->postResidualProximity_m, 0.5f);
  EXPECT_FLOAT_EQ(k->postBallNear_m, 2.0f);
}

// The dodge's goal-sign convention, pinned because it is load-bearing and a
// silent re-inversion would move both near-goal dodges to the wrong end.
//
// A team's own goal is at +side * pitchHalfW. Two independent places in the
// engine agree: the throw-in's defending branch runs `backXBound` at
// ballPos + 30 * side (backward is +side) and the corner's defending branch
// bounds the own half at side * pitchHalfW * 0.98..0.5. Pitch marks are
// gametypes.hpp's own.
TEST(PesDodgeGoalSign, TheOwnGoalIsOnTheSidesEnd) {
  const float pitchHalfW = 52.5f, goalHalfWidth = 3.7f;
  // A side +1 team defends the +x end, and the angles it hands the selector put
  // the post on the own goal line at the goal's half-width.
  const pes::PostAngles plus = pes::PostAnglesFor(1, 40.0f, 5.0f, 45.0f, 5.0f, pitchHalfW,
                                                  goalHalfWidth);
  EXPECT_NEAR(plus.ownGoalX_m, pitchHalfW, 1e-5f);
  EXPECT_EQ(plus.refAngle_deg, 180.0f);
  // The player stands inside the pitch, so the bearing from the post to him is
  // in the left half-plane: past 90 and before 270 degrees.
  EXPECT_GT(plus.bearing_deg, 90.0f);
  EXPECT_LT(plus.bearing_deg, 270.0f);

  // The other team defends the opposite end, and its reference flips.
  const pes::PostAngles minus = pes::PostAnglesFor(-1, -40.0f, 5.0f, -45.0f, 5.0f, pitchHalfW,
                                                   goalHalfWidth);
  EXPECT_NEAR(minus.ownGoalX_m, -pitchHalfW, 1e-5f);
  EXPECT_EQ(minus.refAngle_deg, 0.0f);

  // The two ends are never the same, so an inversion cannot pass by symmetry.
  EXPECT_NE(plus.ownGoalX_m, minus.ownGoalX_m);
  // The post follows the ball's half of the goal line.
  const pes::PostAngles nearPost = pes::PostAnglesFor(1, 40.0f, 5.0f, 45.0f, 10.0f, pitchHalfW,
                                                      goalHalfWidth);
  EXPECT_GE(nearPost.bearing_deg, 0.0f);
  EXPECT_LT(nearPost.bearing_deg, 360.0f);
}

}  // namespace
// (FUN_140a07570 / FUN_140a07880). GF has no synthetic exe, so the constants
// are hand-built and only the install test exercises the table reads.
namespace {

// The role-class bits, by the three predicates the exe's head runs. Built by
// hand from the install's own 0x1426484f0 table: class 4 DRIBBLE admits, class
// 7 TRAP is out on the bit-5 test, class 8 LONG_PASS is out on bit 25, and
// class 99 is out of range entirely.
pes::DodgeConstants BallConstants() {
  pes::DodgeConstants k = Constants();
  for (auto& b : k.roleBits) b = 0x610003bu;  // a play/receive class by default
  k.roleBits[4] = 0x00004008u;   // DRIBBLE: admits
  k.roleBits[7] = 0x00004020u;   // TRAP: bit 5 set -> out
  k.roleBits[8] = 0x02000000u;   // LONG_PASS: bit 25 set -> out
  // The last-event window: (54 * 5) / 48 = 5 frames.
  k.lastEventRollDivisor = 54;
  k.lastEventWindowFactor = 0.2f;  // the human gate's window (DAT_142598578)
  k.lastEventFramesPerRoll = 5;
  k.ballSpeedFloor_mps = 0.8f;
  k.ballReachPerUnit_m = 0.01f;
  k.ballReachMargin_m = 0.3f;
  k.ballWindowHeading_deg = 10.0f;
  k.ballWindowGoal_deg = 90.0f;
  // The goal gates' own constants (FUN_140a07e40, FUN_140a084d0).
  k.netEdgeMargin_m = 1.0f;
  k.netStepNumerator = 100.0f;
  k.netStepDivisor = 72.0f;
  k.netEdgeRejection_deg = 15.0f;
  k.postProximity_m = 5.0f;
  k.postResidualProximity_m = 0.5f;
  k.postBallNear_m = 2.0f;
  return k;
}

TEST(PesDodgeBallGate, TheHeadRunsTheThreeClassPredicates) {
  const pes::DodgeConstants k = BallConstants();
  EXPECT_TRUE(pes::BallClassAdmits(k, 4));    // DRIBBLE
  EXPECT_FALSE(pes::BallClassAdmits(k, 7));   // TRAP: bit 5
  EXPECT_FALSE(pes::BallClassAdmits(k, 8));   // LONG_PASS: bit 25
  EXPECT_FALSE(pes::BallClassAdmits(k, 99));  // past the table
  EXPECT_FALSE(pes::BallClassAdmits(k, -1));  // before it
  // A class whose word carries none of the play bits is out too.
  pes::DodgeConstants k2 = BallConstants();
  k2.roleBits[4] = 0x02000000u;  // bit 25 only: admits nothing
  EXPECT_FALSE(pes::BallClassAdmits(k2, 4));
}

TEST(PesDodgeBallGate, TheDesignatedOwnerIsAdmittedOutright) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallEligibilityInput in;
  in.roleClass = 4;
  in.designatedOwner = true;  // the +0x18->0x2600c test: he owns it, done
  EXPECT_TRUE(pes::BallDodgeEligible(k, in));
}

TEST(PesDodgeBallGate, TheLastEventWindowHasToClearFiveFrames) {
  const pes::DodgeConstants k = BallConstants();  // (54*5)/48 = 5
  pes::BallEligibilityInput in;
  in.roleClass = 4;
  in.slot = 3;
  in.perTeamActionAllowed = true;
  in.actionSlot = 5;
  in.framesSinceEvent = 5;
  EXPECT_TRUE(pes::BallDodgeEligible(k, in));
  // One frame short: the exe's window is inclusive of the boundary.
  in.framesSinceEvent = 4;
  EXPECT_FALSE(pes::BallDodgeEligible(k, in));
}

TEST(PesDodgeBallGate, TheTeamsOwnActionGateAndTheLastActionPlayer) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallEligibilityInput in;
  in.roleClass = 4;
  in.slot = 3;
  in.actionSlot = 5;
  in.framesSinceEvent = 20;
  // The team's own gate (teamRecord->0x255) closed: no dodge at all.
  in.perTeamActionAllowed = false;
  EXPECT_FALSE(pes::BallDodgeEligible(k, in));
  // Open, but he is the player the team last acted through.
  in.perTeamActionAllowed = true;
  in.lastActionPlayer = true;
  EXPECT_FALSE(pes::BallDodgeEligible(k, in));
}

TEST(PesDodgeBallGate, TheActionSlotHasToBeHis) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallEligibilityInput in;
  in.roleClass = 4;
  in.slot = 3;
  in.perTeamActionAllowed = true;
  in.framesSinceEvent = 20;
  // No slot at all (FUN_140a62d20's 0): the eligibility stops before the
  // window test.
  in.slot = -1;
  in.actionSlot = 5;
  EXPECT_FALSE(pes::BallDodgeEligible(k, in));
  in.slot = 3;
  in.actionSlot = 0x18;  // the "none" index
  EXPECT_FALSE(pes::BallDodgeEligible(k, in));
  // 0x18 is accepted when his own state is already a dodge (5).
  in.stateAction = 5;
  EXPECT_TRUE(pes::BallDodgeEligible(k, in));
}

TEST(PesDodgeBallGate, ABallHolderHasToBeInHisOwnHalf) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallEligibilityInput in;
  in.roleClass = 4;
  in.slot = 3;
  in.perTeamActionAllowed = true;
  in.actionSlot = 5;
  in.framesSinceEvent = 20;
  // A ball holder on the other half's record set is out (FUN_140a63830's
  // half test).
  in.ownsBall = true;
  in.teamIndex = 1;  // slot 3 is the home half
  EXPECT_FALSE(pes::BallDodgeEligible(k, in));
  in.teamIndex = 0;
  EXPECT_TRUE(pes::BallDodgeEligible(k, in));
  // Not holding the ball, the half test does not apply.
  in.ownsBall = false;
  in.teamIndex = 1;
  EXPECT_TRUE(pes::BallDodgeEligible(k, in));
}

TEST(PesDodgeBallGate, SubTestFastRunsTheStepAndTheReachFirst) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallSubTestInput in;
  in.ballStep_m = 1.0f;
  in.frameY_m = 1.0f;
  in.reach_m = 200.0f;  // 200 * 0.01 + 0.3 = 2.3 m
  in.recordHalf = 0;
  in.selfSlot = 3;
  in.lastTouchSlot = 3;
  in.selfHeading_deg = 0.0f;
  in.bearingToBall_deg = 0.0f;
  in.hasAnimeSlot = true;
  EXPECT_TRUE(pes::BallDodgeSubTestFast(k, in));
  in.ballStep_m = 0.5f;  // under the 0.8 step floor
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
  in.ballStep_m = 1.0f;
  in.frameY_m = 2.4f;  // past his reach
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
  in.frameY_m = 1.0f;
  in.recordHalf = 1;  // the wrong half's record
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
  in.recordHalf = 0;
  in.keeperSaveFlag = true;  // the save flag bypasses the half test
  EXPECT_TRUE(pes::BallDodgeSubTestFast(k, in));
  in.keeperSaveFlag = false;
  in.selfHeading_deg = 40.0f;  // the ball well off his heading
  in.defendGoalAngle_deg = 90.0f;
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
}

TEST(PesDodgeBallGate, SubTestFastReadsHisHeadingOrTheGoalHeDefends) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallSubTestInput in;
  in.ballStep_m = 1.0f;
  in.frameY_m = 1.0f;
  in.reach_m = 200.0f;
  in.recordHalf = 0;
  in.selfSlot = 3;
  in.lastTouchSlot = 3;  // he touched it last
  in.hasAnimeSlot = true;
  // Within 10 deg off his heading.
  in.selfHeading_deg = 0.0f;
  in.defendGoalAngle_deg = 170.0f;
  in.bearingToBall_deg = 9.0f;
  EXPECT_TRUE(pes::BallDodgeSubTestFast(k, in));
  // Off both, but inside the wrap: 179 deg apart is really 1 deg.
  in.bearingToBall_deg = 181.0f;
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
  // Off his heading, but on the goal he defends.
  in.selfHeading_deg = 90.0f;
  in.defendGoalAngle_deg = -170.0f;
  in.bearingToBall_deg = -179.0f;
  EXPECT_TRUE(pes::BallDodgeSubTestFast(k, in));
}

TEST(PesDodgeBallGate, SubTestFastTakesTheTimeToBallWhenSomebodyElseTouched) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallSubTestInput in;
  in.ballStep_m = 1.0f;
  in.frameY_m = 1.0f;
  in.reach_m = 200.0f;
  in.recordHalf = 1;
  in.selfSlot = 14;
  in.lastTouchSlot = 5;
  in.otherHeading_deg = 0.0f;
  in.bearingToBall_deg = 45.0f;
  in.hasAnimeSlot = true;
  EXPECT_TRUE(pes::BallDodgeSubTestFast(k, in));
  // They get there first.
  in.selfTimeToBall_s = 3.0f;
  in.otherTimeToBall_s = 1.0f;
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
  // He gets there first, but the ball is off their heading by more than 90.
  in.selfTimeToBall_s = 1.0f;
  in.otherTimeToBall_s = 3.0f;
  in.otherHeading_deg = 0.0f;
  in.bearingToBall_deg = 180.0f;
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
}

TEST(PesDodgeBallGate, SubTestFastFallsThroughWhenNobodyTouched) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallSubTestInput in;
  in.ballStep_m = 1.0f;
  in.frameY_m = 1.0f;
  in.reach_m = 200.0f;
  in.recordHalf = 0;
  in.selfSlot = 3;
  in.lastTouchSlot = 22;  // the 0x16 "no player" answer
  in.hasAnimeSlot = true;
  in.liveAction = 5;  // DRIBBLE_SIDE_BACK
  EXPECT_TRUE(pes::BallDodgeSubTestFast(k, in));
  // The locos and STAGGER are out.
  for (int a : {1, 2, 3, 0x10}) {
    in.liveAction = a;
    EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in)) << "action " << a;
  }
  in.liveAction = 0x15;  // already dodging: falls through, but no anime slot
  in.hasAnimeSlot = false;
  EXPECT_FALSE(pes::BallDodgeSubTestFast(k, in));
}

TEST(PesDodgeBallGate, SubTestMouthOnlyRunsForTheKickClasses) {
  const pes::DodgeConstants k = BallConstants();
  pes::BallSubTestInput in;
  in.mouthMidAngle_deg = -10.0f;
  in.mouthFarAngle_deg = 10.0f;
  in.ballBearing_deg = 0.0f;  // inside the wedge
  // Only 8, 9 and 10 (the kicks) read the mouth.
  for (int a : {8, 9, 10}) {
    in.targetAction = a;
    EXPECT_TRUE(pes::BallDodgeSubTestMouth(k, in)) << "action " << a;
  }
  // Outside the band the mouth is not read at all, so a ball far outside the
  // wedge still passes.
  in.ballBearing_deg = 90.0f;
  for (int a : {4, 5, 6, 7, 11, 12}) {
    in.targetAction = a;
    EXPECT_TRUE(pes::BallDodgeSubTestMouth(k, in)) << "action " << a;
  }
  // Inside the band, the wedge bounds it.
  in.ballBearing_deg = 0.0f;
  in.targetAction = 9;
  EXPECT_TRUE(pes::BallDodgeSubTestMouth(k, in));
  in.ballBearing_deg = 30.0f;  // past the far edge
  EXPECT_FALSE(pes::BallDodgeSubTestMouth(k, in));
  in.ballBearing_deg = -30.0f;  // past the mid edge
  EXPECT_FALSE(pes::BallDodgeSubTestMouth(k, in));
}


// The goal marks both gates' tests run on: a 50 goal line, 1.0 m posts and a
// net back at 52.0 (none of them PES's values; the posts sit at (+-50, +-1)).
pes::GoalMarks NetMarks() { return {50.0f, 1.0f, 52.0f}; }

pes::NetGateInput RunningAt(float x, float y, float vx, float vy) {
  pes::NetGateInput in;
  in.animeSlot = true;
  in.x_m = x;
  in.y_m = y;
  in.moveX_mps = vx;
  in.moveY_mps = vy;
  return in;
}

TEST(PesDodgeNetGate, ThePesBearingIsAtan2DxDzWithZeroAlongGfY) {
  // The gate compares bearings numerically, so the convention is pinned. PES's z is minus GF's y
  // (PesBearingDeg in pesdodge.cpp, the same convention as pesmovestep.cpp's PesBearingDeg; see
  // docs/re/pes-re-carrier-port.md, the bearing row): atan2(dx, PES z), wrapped to [0, 360). So 0 deg
  // runs along -GF y (PES's +z), 90 deg along +GF x and 180 deg along +GF y.
  EXPECT_NEAR(pes::PesBearingDeg(0.0f, 0.0f, 0.0f, -1.0f), 0.0f, 1e-3f);
  EXPECT_NEAR(pes::PesBearingDeg(0.0f, 0.0f, 1.0f, 0.0f), 90.0f, 1e-3f);
  EXPECT_NEAR(pes::PesBearingDeg(0.0f, 0.0f, 0.0f, 1.0f), 180.0f, 1e-3f);
  EXPECT_NEAR(pes::PesBearingDeg(0.0f, 0.0f, -1.0f, 0.0f), 270.0f, 1e-3f);
  EXPECT_NEAR(pes::PesBearingDeg(0.0f, 0.0f, 1.0f, -1.0f), 45.0f, 1e-3f);
  EXPECT_NEAR(pes::PesBearingDeg(0.0f, 0.0f, 1.0f, 1.0f), 135.0f, 1e-3f);
  EXPECT_NEAR(pes::PesBearingDeg(2.0f, 3.0f, 2.0f, 4.0f), 180.0f, 1e-3f);
}

TEST(PesDodgeNetGate, HeHasToStandWithinTheEdgeMarginOfHisGoalLine) {
  const pes::DodgeConstants k = BallConstants();
  // The margin is inclusive: 49.0 = 50 - 1.0.
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(48.9f, 0.0f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(30.0f, 0.0f, 2.0f, 0.0f)));
  // Either end of the pitch: the goal he stands at is the one that counts.
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(-49.0f, 0.0f, -2.0f, 0.0f)));
  // Without the anime layer's slot the gate stops before the geometry.
  pes::NetGateInput noSlot = RunningAt(49.0f, 0.0f, 2.0f, 0.0f);
  noSlot.animeSlot = false;
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), noSlot));
}

TEST(PesDodgeNetGate, HisSpeedHasToClearTheFloor) {
  // 100 / 72 m/s: the exe's 100 / (gs * 72) per frame, gs = 54, in m/s.
  const pes::DodgeConstants k = BallConstants();
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, 1.5f, 0.0f)));
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, 1.2f, 0.0f)));
}

TEST(PesDodgeNetGate, InLineWithTheGoalHisHeadingHasToBeBetweenThePosts) {
  const pes::DodgeConstants k = BallConstants();
  // From (49, 0) the posts stand at 45 and 135 deg. Running along +x heads 90
  // deg, between them; running sideways heads 0 deg, running away 270.
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, 0.0f, 2.0f)));
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, -2.0f, 0.0f)));
}

TEST(PesDodgeNetGate, InsideTheEdgeHisHeadingHasToClearFifteenDegOfHisPost) {
  const pes::DodgeConstants k = BallConstants();
  // The own post stands at 45 deg from (49, 0). Heading 50 is between the posts
  // but 5 deg off that one.
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, 1.532f, 1.286f)));
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(49.0f, 0.0f, 2.0f, 0.0f)));
  // Past the goal line the residual does not run: from (50.2, 0) heading 50 is
  // outside the posts' pair.
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(50.2f, 0.0f, 1.532f, 1.286f)));
}

TEST(PesDodgeNetGate, BehindTheGoalLineHeMustNotBeRunningBackThroughTheMouth) {
  const pes::DodgeConstants k = BallConstants();
  // Past the line at (51.5, 0) the posts stand at 236.3 and 303.7 deg. Running
  // deeper into the net (heading 90) is in; running back out through the mouth
  // (heading 270) is out.
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(51.5f, 0.0f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(51.5f, 0.0f, -2.0f, 0.0f)));
}

TEST(PesDodgeNetGate, WideOfTheGoalHisHeadingHasToBeBetweenTheNetBackAndHisPost) {
  const pes::DodgeConstants k = BallConstants();
  // At (50.0, 2.0), wide of the posts: the own post stands at 180 deg and the
  // net back behind it at 116.6 deg. Heading 150 is between them; heading 110
  // is outside, though it would be inside a 3.0 m offset to the back (108.4).
  EXPECT_TRUE(pes::NetDodgeGate(k, NetMarks(), RunningAt(50.0f, 2.0f, 1.0f, -1.732f)));
  EXPECT_FALSE(pes::NetDodgeGate(k, NetMarks(), RunningAt(50.0f, 2.0f, 1.879f, -0.684f)));
}

TEST(PesDodgeNetGate, ThePostsAreTheMarksPostHalfWidth) {
  const pes::DodgeConstants k = BallConstants();
  // With posts at 2.0 m, from (51.0, 0) the pair spans 206.6 to 333.4 deg, so
  // heading 200 is outside it. GF's 3.7 m goal would put 200 inside [195.1, 344.9].
  const pes::GoalMarks wider{50.0f, 2.0f, 52.0f};
  EXPECT_TRUE(pes::NetDodgeGate(k, wider, RunningAt(51.0f, 0.0f, -0.684f, -1.879f)));
}

// FUN_140a084d0, the post dodge's gate. The marks are NetMarks()'s, so the post
// sits at (+-50, +-1). The ball starts far from the post, so the designated
// clause stays out unless a test moves it.
static pes::PostGateInput PostAt(float x, float y, float vx, float vy) {
  pes::PostGateInput in;
  in.animeSlot = true;
  in.x_m = x;
  in.y_m = y;
  in.moveX_mps = vx;
  in.moveY_mps = vy;
  in.ballX_m = 30.0f;
  in.ballY_m = 0.0f;
  return in;
}

TEST(PesDodgePostGate, HeHasToStandWithinTheEdgeMarginOfHisGoalLine) {
  const pes::DodgeConstants k = BallConstants();
  // 49.0 = 50 - 1.0 is inside the margin; 48.9 is not.
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.0f, 0.5f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), PostAt(48.9f, 0.5f, 2.0f, 0.0f)));
  // Either end of the pitch: the goal he stands at is the one that counts.
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(-49.0f, -0.5f, -2.0f, 0.0f)));
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), PostAt(-48.9f, -0.5f, -2.0f, 0.0f)));
  // Without the anime layer's slot the gate stops before the geometry.
  pes::PostGateInput noSlot = PostAt(49.0f, 0.5f, 2.0f, 0.0f);
  noSlot.animeSlot = false;
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), noSlot));
}

TEST(PesDodgePostGate, HisZMustSitWithinOneMetreOfThePost) {
  const pes::DodgeConstants k = BallConstants();
  // ||z| - 1| <= 1: z = 0 and z = 2 are on the band's edges, 2.1 is off it.
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.5f, 0.0f, 2.0f, 0.0f)));
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.5f, 2.0f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.5f, 2.1f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.5f, -2.1f, -2.0f, 0.0f)));
}

TEST(PesDodgePostGate, TheZBandHasItsInnerEdgeToo) {
  const pes::DodgeConstants k = BallConstants();
  // With posts at 3.0 m, z = 1.5 is 1.5 m inside the band; z = 2.2 is in it.
  const pes::GoalMarks wide{50.0f, 3.0f, 52.0f};
  EXPECT_FALSE(pes::PostDodgeGate(k, wide, PostAt(49.5f, 1.5f, 2.0f, 0.0f)));
  EXPECT_TRUE(pes::PostDodgeGate(k, wide, PostAt(49.5f, 2.2f, 2.0f, 0.0f)));
}

TEST(PesDodgePostGate, HeHasToBeMovingAboveTheStepFloor) {
  const pes::DodgeConstants k = BallConstants();  // 100 / 72 m/s
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.5f, 0.5f, 1.5f, 0.0f)));
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.5f, 0.5f, 1.2f, 0.0f)));
}

TEST(PesDodgePostGate, TheKeeperAndTheNewestToucherAreLeftOut) {
  const pes::DodgeConstants k = BallConstants();
  pes::PostGateInput in = PostAt(49.5f, 0.5f, 2.0f, 0.0f);
  ASSERT_TRUE(pes::PostDodgeGate(k, NetMarks(), in));
  in.keeper = true;
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), in));
  in.keeper = false;
  in.newestTouch = true;
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), in));
}

TEST(PesDodgePostGate, HeadingAtThePostOrHalfAMetreOfItAdmitsHim) {
  const pes::DodgeConstants k = BallConstants();
  // From (49.2, 0) the post at (50, 1) lies at atan2(0.8, 1). Running that way
  // the bearing admits him; running straight along +x he is 0.8 m and 1.0 m
  // from the post on x and z, beyond the half-metre admission.
  const float c = std::sqrt(1.64f);
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.2f, 0.0f, 2.0f * 0.8f / c, 2.0f / c)));
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.2f, 0.0f, 2.0f, 0.0f)));
  // Half a metre on x admits him without the bearing: 49.5 is 0.5 m from x = 50.
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(49.5f, 0.0f, 2.0f, 0.0f)));
}

TEST(PesDodgePostGate, TheFiveMetreCapBoundsHim) {
  const pes::DodgeConstants k = BallConstants();
  // (55, 1) is 5.0 m from the post at (50, 1): inside the cap. (56, 1) is 6 m,
  // outside, though level with the post on z.
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), PostAt(55.0f, 1.0f, 2.0f, 0.0f)));
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), PostAt(56.0f, 1.0f, 2.0f, 0.0f)));
}

TEST(PesDodgePostGate, TheDesignatedManIsKeptOffABallNearerHimThanThePost) {
  const pes::DodgeConstants k = BallConstants();
  pes::PostGateInput in = PostAt(49.5f, 0.5f, 2.0f, 0.0f);
  ASSERT_TRUE(pes::PostDodgeGate(k, NetMarks(), in));
  // The ball at (49.6, 0.4) is 0.72 m from the post and 0.14 m from him, nearer
  // than the post's 0.71 m: the designated man stays out, anyone else dodges.
  in.ballX_m = 49.6f;
  in.ballY_m = 0.4f;
  in.designatedMan = true;
  EXPECT_FALSE(pes::PostDodgeGate(k, NetMarks(), in));
  in.designatedMan = false;
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), in));
}

TEST(PesDodgePostGate, TheBallClauseNeedsTheBallOnThePitchAndNearThePost) {
  const pes::DodgeConstants k = BallConstants();
  pes::PostGateInput in = PostAt(49.5f, 0.5f, 2.0f, 0.0f);
  in.designatedMan = true;
  // Past the goal line the clause does not apply.
  in.ballX_m = 50.5f;
  in.ballY_m = 0.4f;
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), in));
  // Far from the post it does not either.
  in.ballX_m = 30.0f;
  EXPECT_TRUE(pes::PostDodgeGate(k, NetMarks(), in));
}


// The human dodge's feature gate (FUN_140a0b350): the match-state gates, the
// roster, the team latch and the last-event window.
TEST(PesDodgeHumanGate, TheWindowIsGsTimesAPointTwoFramesOfTheNewestEvent) {
  const pes::DodgeConstants k = BallConstants();  // gs = 54, factor 0.2
  EXPECT_FLOAT_EQ(pes::HumanLastEventWindowFrames(k), 10.8f);
  pes::HumanGateInput in;
  in.featureOn = true;
  in.onRoster = true;
  in.stateRecordHalfMatches = true;
  in.newestEventIsOwnTeam = true;
  // Logic frames since his team's newest touch: 10 is inside the 10.8-frame window.
  in.newestEventAgeFrames = 10;
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
  // 11 frames clears it: FUN_140a0b350 compares with a strict <.
  in.newestEventAgeFrames = 11;
  EXPECT_TRUE(pes::HumanDodgeGate(k, in));
}

TEST(PesDodgeHumanGate, TheWindowOnlyHoldsHimForHisTeamsNewestEvent) {
  const pes::DodgeConstants k = BallConstants();
  pes::HumanGateInput in;
  in.featureOn = true;
  in.onRoster = true;
  in.stateRecordHalfMatches = true;
  in.newestEventAgeFrames = 2;
  // The other side's touch, however recent, does not hold him.
  in.newestEventIsOwnTeam = false;
  EXPECT_TRUE(pes::HumanDodgeGate(k, in));
  in.newestEventIsOwnTeam = true;
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
}

TEST(PesDodgeHumanGate, TheMatchStateAndTheFeatureSwitchItOff) {
  const pes::DodgeConstants k = BallConstants();
  pes::HumanGateInput in;
  in.featureOn = true;
  in.onRoster = true;
  in.stateRecordHalfMatches = true;
  EXPECT_TRUE(pes::HumanDodgeGate(k, in));
  in.featureOn = false;
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
  in.featureOn = true;
  in.matchStateOk = false;  // the state that switches the behaviour off
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
}

TEST(PesDodgeHumanGate, TheRosterAndTheTeamsLatch) {
  const pes::DodgeConstants k = BallConstants();
  pes::HumanGateInput in;
  in.featureOn = true;
  in.onRoster = true;
  in.stateRecordHalfMatches = true;
  EXPECT_TRUE(pes::HumanDodgeGate(k, in));
  in.onRoster = false;
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
  in.onRoster = true;
  in.stateRecordHalfMatches = false;
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
  in.stateRecordHalfMatches = true;
  in.teamLatchSet = true;  // the team's own latch set
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
}

TEST(PesDodgeHumanGate, TheBallOwnerTestOnlyRunsOnAPopulatedSlot) {
  const pes::DodgeConstants k = BallConstants();
  pes::HumanGateInput in;
  in.featureOn = true;
  in.onRoster = true;
  in.stateRecordHalfMatches = true;
  // An empty roster slot never runs the ball-owner test.
  in.rosterSlotValue = 0;
  in.ballOwnerIsSelf = true;
  EXPECT_TRUE(pes::HumanDodgeGate(k, in));
  // A populated one does.
  in.rosterSlotValue = 5;
  in.ballOwnerIsSelf = true;
  EXPECT_FALSE(pes::HumanDodgeGate(k, in));
  in.ballOwnerIsSelf = false;
  EXPECT_TRUE(pes::HumanDodgeGate(k, in));
}

}  // namespace
