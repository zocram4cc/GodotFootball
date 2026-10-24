#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "onthepitch/pesmentality.hpp"
#include "onthepitch/pesroles.hpp"
#include "onthepitch/pesteamconcepts.hpp"
#include "pes/exe.hpp"

namespace mentality = pes::mentality;

namespace {

constexpr int kPlayStateInPlay = 1;   // MI+0x116C == 1: open play
constexpr int kPlayStateStopped = 2;  // MI+0x116C == 2: a stoppage
constexpr int kRestartKindOne = 1;    // MI+0x1554 == 1
constexpr int kNoRestartKind = 0;
constexpr int kCentreForwardClass = 9;  // CF, the class the protected forward carries
constexpr int kForwardClass = 7;        // SS, a forward class above the forward bump threshold

// Opens the install and loads the fold constants; false (the caller skips) when PES21_ROOT is unset.
bool InstallFoldConstants(mentality::FoldConstants* out) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return false;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  EXPECT_TRUE(exe) << err;
  if (!exe) return false;
  auto k = mentality::LoadFoldConstants(*exe, &err);
  EXPECT_TRUE(k) << err;
  if (!k) return false;
  *out = *k;
  return true;
}

using Candidates = std::array<mentality::FoldCandidate, pes::roles::kSideSlots>;

mentality::FoldCandidate Cand(int cls, float x, float z) {
  mentality::FoldCandidate c;
  c.onPitch = true;
  c.cls = cls;
  c.x = x;
  c.z = z;
  return c;
}

}  // namespace

// Rule 1: the role level (docs/re/pes-re-role-rebuild.md §2; EC600's CMOVZ override).
TEST(MentalityRoleLevel, FollowsThePossessionLatch) {
  EXPECT_EQ(2, mentality::RoleLevel(false, kPlayStateInPlay, kNoRestartKind));
  EXPECT_EQ(1, mentality::RoleLevel(true, kPlayStateInPlay, kNoRestartKind));
}

TEST(MentalityRoleLevel, IsZeroOutOfPlayWithRestartKindOne) {
  EXPECT_EQ(0, mentality::RoleLevel(false, kPlayStateStopped, kRestartKindOne));
  EXPECT_EQ(0, mentality::RoleLevel(true, kPlayStateStopped, kRestartKindOne));
  // Another restart kind keeps the latch rule.
  EXPECT_EQ(1, mentality::RoleLevel(true, kPlayStateStopped, kNoRestartKind));
}

TEST(MentalityRoleLevel, InPlayIgnoresTheRestartKind) {
  EXPECT_EQ(2, mentality::RoleLevel(false, kPlayStateInPlay, kRestartKindOne));
}

// The role state at IsInPlay && IsInSetPiece (plan-selector §1 vs roles §2; the seed-7 run's 754
// ticks): PES's state is 1 there (the in-play arm is taken when MI+0x116C == 1), so the set-piece
// flag is not folded into the state. The EC600 override then cannot fire in play.
TEST(MentalityRolePlayState, InPlayWithASetPieceIsStillOne) {
  EXPECT_EQ(1, mentality::RolePlayState(true, true));
  EXPECT_EQ(1, mentality::RolePlayState(true, false));
  EXPECT_EQ(2, mentality::RolePlayState(false, true));
  EXPECT_EQ(2, mentality::RolePlayState(false, false));
}

// Rule 1: the role of each slot is read from the matrix row of the level.
TEST(MentalityRoleRows, EachLevelReadsItsOwnRow) {
  std::array<int, pes::roles::kRegisteredRoles> classOf{};
  for (int r = 0; r < pes::roles::kRegisteredRoles; ++r) classOf[r] = r;
  pes::TeamSheet sheet;
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot)
    for (int level = 0; level < pes::kSheetPhases; ++level)
      sheet.SetRegisteredRole(slot, level + 1, level);
  for (int level = 0; level < pes::kSheetPhases; ++level) {
    pes::roles::SideRoles roles;
    pes::roles::WriteSideRoles(classOf, sheet, level, &roles);
    for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) EXPECT_EQ(level + 1, roles.b[slot]);
  }
}

// Rule 2: the shared reclassify (EC600's table at VA 0x1405ED1F4).
TEST(MentalityReclassify, RoleGroupsShareALineAndTheGroupsDiffer) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto table = mentality::LoadReclassifyLines(*exe, &err);
  ASSERT_TRUE(table) << err;
  const auto line = [&](int cls) { return mentality::ReclassifyLine(*table, cls); };
  EXPECT_EQ(line(1), line(2));  // CB and SB
  EXPECT_EQ(line(3), line(6));  // DMF..AMF
  EXPECT_EQ(line(7), line(9));  // SS, WF, CF
  EXPECT_NE(line(2), line(3));
  EXPECT_NE(line(6), line(7));
  EXPECT_EQ(mentality::kBackLine, line(1));
  EXPECT_EQ(mentality::kMidLine, line(3));
  EXPECT_EQ(mentality::kForwardLine, line(7));
  // GK and any class past 9 take the default arm, which is the back line.
  EXPECT_EQ(mentality::kBackLine, line(0));
  EXPECT_EQ(mentality::kBackLine, line(10));
}

// Rule 3: the block choice by the possession flag (EC600 @ 0x1405ECC21).
TEST(MentalityBlock, OutOfPossessionRunsBlockAAndInPossessionRunsBlockB) {
  EXPECT_EQ(mentality::Block::kOutOfPossession, mentality::BlockFor(false));
  EXPECT_EQ(mentality::Block::kInPossession, mentality::BlockFor(true));
}

// Rule 4: block B's fold pick (67C4B0).
TEST(MentalityFold, ForwardPenaltyAndInitialBestAreInstallConstants) {
  mentality::FoldConstants k{};
  if (!InstallFoldConstants(&k)) GTEST_SKIP() << "PES21_ROOT unset";
  EXPECT_GT(k.forwardPenalty, 0.0f);
  EXPECT_GT(k.initialBest, k.forwardPenalty);
}

TEST(MentalityFold, PicksTheLowestScoreNonForwardBeforeAForward) {
  mentality::FoldConstants k{};
  if (!InstallFoldConstants(&k)) GTEST_SKIP() << "PES21_ROOT unset";
  Candidates c{};
  c[2] = Cand(3, 0.0f, 5.0f);        // DMF, score 25
  c[5] = Cand(kForwardClass, 0.0f, 1.0f);  // SS, score 1 + forward penalty
  EXPECT_EQ(std::vector<int>{2}, mentality::FoldPicks(k, c, -1, 1, false));
}

TEST(MentalityFold, NeverPicksTheProtectedSlot) {
  mentality::FoldConstants k{};
  if (!InstallFoldConstants(&k)) GTEST_SKIP() << "PES21_ROOT unset";
  Candidates c{};
  c[2] = Cand(3, 0.0f, 5.0f);  // score 25, protected
  c[4] = Cand(4, 0.0f, 10.0f);  // score 100
  EXPECT_EQ(std::vector<int>{4}, mentality::FoldPicks(k, c, 2, 1, false));
}

TEST(MentalityFold, PicksOnePerCountInScoreOrder) {
  mentality::FoldConstants k{};
  if (!InstallFoldConstants(&k)) GTEST_SKIP() << "PES21_ROOT unset";
  Candidates c{};
  c[2] = Cand(3, 0.0f, 5.0f);   // 25
  c[4] = Cand(4, 0.0f, 10.0f);  // 100
  c[6] = Cand(5, 0.0f, 20.0f);  // 400
  EXPECT_EQ((std::vector<int>{2, 4}), mentality::FoldPicks(k, c, -1, 2, false));
  EXPECT_EQ(std::vector<int>{}, mentality::FoldPicks(k, c, -1, 0, false));
}

TEST(MentalityFold, FullAttackSkipsThePass) {
  mentality::FoldConstants k{};
  if (!InstallFoldConstants(&k)) GTEST_SKIP() << "PES21_ROOT unset";
  Candidates c{};
  c[2] = Cand(3, 0.0f, 5.0f);
  EXPECT_EQ(std::vector<int>{}, mentality::FoldPicks(k, c, -1, 1, true));
}

TEST(MentalityFold, GoalkeeperAndAbsentPlayersAreNeverPicked) {
  mentality::FoldConstants k{};
  if (!InstallFoldConstants(&k)) GTEST_SKIP() << "PES21_ROOT unset";
  Candidates c{};
  c[0] = Cand(0, 0.0f, 0.0f);  // GK at the origin
  c[1] = Cand(3, 0.0f, 0.0f);
  c[1].onPitch = false;        // off the pitch at the origin
  c[3] = Cand(4, 0.0f, 9.0f);
  EXPECT_EQ(std::vector<int>{3}, mentality::FoldPicks(k, c, -1, 3, false));
}

TEST(MentalityFold, ScoreAtOrAboveTheInitialBestIsNotPicked) {
  mentality::FoldConstants k{};
  if (!InstallFoldConstants(&k)) GTEST_SKIP() << "PES21_ROOT unset";
  Candidates c{};
  c[2] = Cand(3, 400.0f, 0.0f);  // 160000 > 100000
  EXPECT_EQ(std::vector<int>{}, mentality::FoldPicks(k, c, -1, 1, false));
}

// Stand-in for T+0x110 (no writer decoded; docs/re/pes-re-mentality-blocks.md §3.1).
TEST(MentalityFold, CountIsTwoMinusTheDialBelowNeutral) {
  EXPECT_EQ(2, mentality::FoldCount(0));
  EXPECT_EQ(1, mentality::FoldCount(1));
  EXPECT_EQ(0, mentality::FoldCount(2));
  EXPECT_EQ(0, mentality::FoldCount(3));
}

TEST(MentalityFold, ProtectedSlotIsTheLastCentreForward) {
  std::array<int, pes::roles::kSideSlots> cls{};
  EXPECT_EQ(-1, mentality::ProtectedSlot(cls));
  cls[2] = kCentreForwardClass;
  cls[5] = kCentreForwardClass;
  EXPECT_EQ(5, mentality::ProtectedSlot(cls));
}
