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
// Rule 5: the out-of-possession lane line (FUN_1405CE150's B-words, lane-writers §3).
// GF inputs only: no T+0x46EC second float (lane inputs, doc OPEN), no F760/F490 distances, so the
// special arm, the 15/instruction arm and the off-pitch sides stay unported ponytail leaves.
TEST(MentalityLaneB, KeeperIsThreeAndDesignatedIsTwo) {
  std::array<int, 11> b{0, 1, 1, 3, 4, 5, 6, 7, 8, 9, 2};
  mentality::LaneInput in;
  in.classes = b;
  in.designated = 5;
  const auto w = mentality::LaneBWords(in);
  EXPECT_EQ(3, w[0]);
  EXPECT_EQ(2, w[5]);
  for (int k = 1; k < 11; ++k) {
    if (k == 5) continue;
    EXPECT_EQ(mentality::ReclassifyLineOf(b[k]), w[k]);
  }
}

TEST(MentalityLaneB, InactiveSlotTakesItsClassLine) {
  std::array<int, 11> b{0, 1, 1, 3, 4, 5, 6, 7, 8, 9, 2};
  mentality::LaneInput in;
  in.classes = b;
  in.active[3] = false;
  EXPECT_EQ(mentality::ReclassifyLineOf(3), mentality::LaneBWords(in)[3]);
}

TEST(MentalityLaneB, WrapperMapsTheWordToTheLane) {
  EXPECT_EQ(1, mentality::LaneOfWord(0));
  EXPECT_EQ(4, mentality::LaneOfWord(1));
  EXPECT_EQ(7, mentality::LaneOfWord(2));
  EXPECT_EQ(0, mentality::LaneOfWord(3));
}

// Step 2: the CE4F0 stoppage step (MI+0x11D4 == 0 and (MI+0x116C == 2 or MI+0x11B0 < 0x15),
// lane-writers §2 step 3). GF uses WriteRoleArrays' PesPlayState mapping (agreed with SheetTriple).
TEST(MentalityLaneC, StoppageStepResetsCopyFlag) {
  // MI+0x116C == 2 clears even with MI+0x11B0 at the bound (the arms are ORed, lane-writers §2).
  EXPECT_TRUE(mentality::StoppageResetsCopyFlag(2, 0, 0x15));
  // MI+0x11B0 < 0x15 clears even in play.
  EXPECT_TRUE(mentality::StoppageResetsCopyFlag(1, 0, 0x14));
  // MI+0x11D4 != 0 blocks; both arms above the bound keep the flag.
  EXPECT_FALSE(mentality::StoppageResetsCopyFlag(2, 1, 0x14));
  EXPECT_FALSE(mentality::StoppageResetsCopyFlag(1, 0, 0x15));
}

// Step 2: the CE4F0 copy gate. A set ban flag copies; a cleared flag recomputes.
TEST(MentalityLaneC, CopyFlagSelectsCopyOrRecompute) {
  EXPECT_TRUE(mentality::CopyPathRuns(true));
  EXPECT_FALSE(mentality::CopyPathRuns(false));
}

// Step 1: the CE150 B-rank (lane-writers §3): per same-word position in the sorted order.
TEST(MentalityLaneB, RankCountsEarlierSameWordSlots) {
  std::array<int, 11> b{3, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  std::array<uint8_t, 11> slots{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::array<float, 11> keys{};
  for (int k = 0; k < 11; ++k) keys[k] = static_cast<float>(k);
  std::array<uint8_t, 11> order{};
  std::array<uint8_t, 11> rank{};
  mentality::SortLaneSlots(slots, keys, order, rank);
  const auto bRank = mentality::LaneBRank(b, order);
  EXPECT_EQ(0u, bRank[0]);
  EXPECT_EQ(0u, bRank[1]);
  EXPECT_EQ(1u, bRank[2]);
  EXPECT_EQ(5u, bRank[6]);
  EXPECT_EQ(0u, bRank[7]);
  EXPECT_EQ(3u, bRank[10]);
}

// Step 1: the CE150 B-word rule (lane-writers §3) as GF can carry it: keeper 3, designated 2,
// else the reclassify of the class. The restart-kind-4 + instruction-15 arm (FUN_140A63950,
// VA 0x140A63950) and the special arm (FUN_140A64410 -> FUN_1405CCD50 distances, VAs 0x140A64410 /
// 0x1405CCD50) are ponytail leaves: GF carries no instruction table and no distance scores.
TEST(MentalityLaneB, RestartKindFourWithoutInstructionTableKeepsClassLine) {
  std::array<int, 11> b{0, 1, 1, 3, 4, 5, 6, 7, 8, 9, 2};
  mentality::LaneInput in;
  in.classes = b;
  in.designated = -1;
  const auto w = mentality::LaneBWords(in);
  for (int k = 1; k < 11; ++k) EXPECT_EQ(mentality::ReclassifyLineOf(b[k]), w[k]);
}

// FUN_140A63950 (lane-post §4d): nonzero iff the side's instruction table holds (id, slot).
TEST(MentalityLaneSpecial, InstructionClaimNeedsIdAndSlotMatch) {
  mentality::InstrTable table{};
  table.ids[0][0] = 0xF;
  table.slots[0][0] = 7;
  EXPECT_TRUE(mentality::InstructionClaimed(table, 0, 0xF, 7));
  EXPECT_FALSE(mentality::InstructionClaimed(table, 0, 0xF, 6));
  EXPECT_FALSE(mentality::InstructionClaimed(table, 0, 0x10, 7));
  EXPECT_FALSE(mentality::InstructionClaimed(table, 1, 0xF, 7));
}

// FUN_140A64410 (lane-post §1): without a 0x10 instruction row the slot is never special.
TEST(MentalityLaneSpecial, NoSixteenRowIsNeverSpecial) {
  mentality::SpecialInput in;
  in.slot = 3;
  in.side = 0;
  in.roles[3] = 8;
  in.active[3] = 1;
  EXPECT_FALSE(mentality::SpecialPredicate(in));
}

// FUN_1405CD540 (lane-post §2a): keeper class maps to 3, the lines as the reclassify.
TEST(MentalityLaneSpecial, Cd540MapsKeeperToThree) {
  EXPECT_EQ(3, mentality::Cd540Line(0));
  EXPECT_EQ(0, mentality::Cd540Line(1));
  EXPECT_EQ(1, mentality::Cd540Line(4));
  EXPECT_EQ(2, mentality::Cd540Line(9));
}

// FUN_1405CCD50 (lane-post §2): out of play with restart kinds 3-5 returns 0.
TEST(MentalityLaneSpecial, Ccd50StoppageReturnsZero) {
  mentality::Ccd50Input in;
  in.cls = 4;
  in.playState116c = 2;
  in.mi54 = 0;
  in.restartKind1554 = 4;
  EXPECT_EQ(0, mentality::Ccd50Word(mentality::LaneThresholds(), in));
}


// FUN_1405CE930's head (lane-leaves §3): A-ranks count same-word slots ahead in depth order.
TEST(MentalityLaneRank, CountsSameWordSlotsAheadInDepthOrder) {
  // Two back-line slots (A = 0): the nearer in depth ranks 0, the farther 1.
  std::array<int, 11> a{0, 0, 1, 1, 2, 3, 3, 3, 3, 3, 3};
  std::array<uint8_t, 11> order{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::array<int, 11> cls{1, 1, 3, 4, 7, 0, 1, 2, 3, 4, 5};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  const auto rank = mentality::Ce930Ranks(a, order, cls, active);
  EXPECT_EQ(0u, rank[0]);
  EXPECT_EQ(1u, rank[1]);
  EXPECT_EQ(0u, rank[2]);
  EXPECT_EQ(1u, rank[3]);
  EXPECT_EQ(0u, rank[4]);
}

// FUN_1405CE930's special-key arm (lane-leaves §3.1): an A64410 slot with f2 > 0 uses +100000.
TEST(MentalityLaneRank, SpecialSlotWithPositiveF2SortsLast) {
  std::array<float, 11> keys{5.0f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f};
  // Slot 0 is special with f2 > 0: its key becomes the +100000 override.
  EXPECT_EQ(100000.0f, mentality::Ce930Key(keys[0], true, 1.0f));
  // A non-special slot keeps its depth key.
  EXPECT_EQ(5.0f, mentality::Ce930Key(keys[0], false, 1.0f));
  // A special slot with f2 <= 0 keeps its depth key too.
  EXPECT_EQ(5.0f, mentality::Ce930Key(keys[0], true, 0.0f));
  EXPECT_EQ(5.0f, mentality::Ce930Key(keys[0], true, -2.0f));
}

// FUN_14063FA70 (lane-post §4): counts active on-pitch slots whose reclassified class == line.
TEST(MentalityLanePost, Fa70CountsLineMembers) {
  std::array<int, 11> cls{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  EXPECT_EQ(3, mentality::Fa70Count(cls, active, 0));
  EXPECT_EQ(4, mentality::Fa70Count(cls, active, 1));
  EXPECT_EQ(3, mentality::Fa70Count(cls, active, 2));
  active[7] = 0;
  EXPECT_EQ(2, mentality::Fa70Count(cls, active, 2));
}

// FUN_140A4C720 (lane-post §7): (c*z + x)*c < c*y, the float (int)c sign-selects the side.
TEST(MentalityLanePost, OpponentLineSelectsBySide) {
  EXPECT_TRUE(mentality::OpponentLine(1.0f, 2.0f, 1, 0.0f));
  EXPECT_FALSE(mentality::OpponentLine(3.0f, 2.0f, 1, 0.0f));
  EXPECT_FALSE(mentality::OpponentLine(-3.0f, -2.0f, -1, 0.0f));
  EXPECT_TRUE(mentality::OpponentLine(-1.0f, -2.0f, -1, 0.0f));
}

// FUN_1405CCD00 (lane-post §4d): nonzero iff the man's action state is 0x3A (58).
TEST(MentalityLanePost, Is38StateNeedsTheStateWord) {
  EXPECT_TRUE(mentality::Is38State(0x3A));
  EXPECT_FALSE(mentality::Is38State(0));
  EXPECT_FALSE(mentality::Is38State(0x36));
}

// FUN_1405CD780 (lane-post §4d): instruction-15 claims count only out of play at kind 4.
TEST(MentalityLanePost, Cd780NeedsKindFourOutOfPlay) {
  mentality::InstrTable table{};
  table.ids[0][0] = 0xF;
  table.slots[0][0] = 3;
  EXPECT_TRUE(mentality::Cd780Claimed(table, 0, 3, 2, 4));
  EXPECT_FALSE(mentality::Cd780Claimed(table, 0, 3, 1, 4));
  EXPECT_FALSE(mentality::Cd780Claimed(table, 0, 3, 2, 3));
  EXPECT_FALSE(mentality::Cd780Claimed(table, 0, 4, 2, 4));
}

// FUN_1405CFC80's first loop (lane-post §4b): sets A := 1 for eligible mid slots until the
// count (CD0F0 - alreadyZeroAt0 + forwardTarget) is reached.
TEST(MentalityLanePost, Cfc80FirstLoopLiftsEligibleMids) {
  // Slot 2: active, on-pitch, A = 2 > 1, B = 1. Slot 5: designated, B = 2 (not B == 1).
  std::array<int, 11> a{3, 3, 2, 3, 3, 2, 3, 3, 3, 3, 3};
  std::array<int, 11> b{3, 0, 1, 1, 1, 2, 1, 2, 2, 2, 2};
  std::array<int, 11> cls{0, 1, 3, 1, 1, 4, 1, 7, 8, 9, 2};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> states{};
  mentality::Cfc80Input in;
  in.a = a;
  in.b = b;
  in.cls = cls;
  in.active = active;
  in.states = states;
  in.designated = 5;
  in.forwardTarget = 1;
  in.backTarget = 0;
  in.fullAttack = false;
  in.attackLevel = 0;
  in.outlet = -1;
  const auto out = mentality::Cfc80FirstLoop(in);
  // Slot 2 lifts (A 2 -> B 1 eligible, count 1 not yet reached); slot 5 is B = 2, skipped here.
  EXPECT_EQ(1, out[2]);
  EXPECT_EQ(2, out[5]);
}

// FUN_1405CFC80's count gate (lane-post §4b): no lift when the target is already met.
TEST(MentalityLanePost, Cfc80FirstLoopStopsAtCount) {
  std::array<int, 11> a{1, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3};
  std::array<int, 11> b{1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  std::array<int, 11> cls{3, 1, 3, 1, 1, 1, 1, 7, 8, 9, 2};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> states{};
  mentality::Cfc80Input in;
  in.a = a;
  in.b = b;
  in.cls = cls;
  in.active = active;
  in.states = states;
  in.designated = -1;
  in.forwardTarget = 1;
  in.backTarget = 0;
  in.fullAttack = false;
  in.attackLevel = 0;
  in.outlet = -1;
  const auto out = mentality::Cfc80FirstLoop(in);
  // Slot 0 already A == 1 fills the count (backTarget - alreadyZeroAt0 + forwardTarget = 0 - 0
  // + 1 = 1 reached by the initial bVar16 = 1): slot 2 keeps A = 2.
  EXPECT_EQ(2, out[2]);
}

// FUN_1405CF310's first half (lane-post §4a): zeroes A-words of B-rank-0 active slots that pass
// the gate and the in-play arm. B-rank comes from the caller (CE150's sort).
TEST(MentalityLanePost, Cf310ZeroesRankZeroSlotInPlay) {
  std::array<int, 11> a{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
  std::array<int, 11> b{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> cls{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> states{};
  std::array<uint8_t, 11> bRank{};
  bRank[2] = 0;  // only slot 2 has B-rank 0
  for (int k = 0; k < 11; ++k)
    if (k != 2) bRank[k] = 1;
  mentality::Cf310Input in;
  in.a = a;
  in.b = b;
  in.cls = cls;
  in.active = active;
  in.states = states;
  in.bRank = bRank;
  in.designated = -1;
  in.outlet = -1;
  in.playState116c = 1;
  in.restartKind1554 = 0;
  in.manLateral = {0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  in.manDepth = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  in.ballX = 0.0f;
  in.sideSign = 1;
  const auto out = mentality::Cf310FirstHalf(in);
  EXPECT_EQ(0, out[2]);
  for (int k = 0; k < 11; ++k)
    if (k != 2) EXPECT_EQ(3, out[k]);
}

// FUN_1405CF310's 0x3A gate (lane-post §4a): a 0x3A-state slot keeps its A-word.
TEST(MentalityLanePost, Cf310Skips38StateSlot) {
  std::array<int, 11> a{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
  std::array<int, 11> b{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> cls{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> states{};
  states[2] = 0x3A;
  std::array<uint8_t, 11> bRank{};
  for (int k = 0; k < 11; ++k) bRank[k] = 1;
  bRank[2] = 0;
  mentality::Cf310Input in;
  in.a = a;
  in.b = b;
  in.cls = cls;
  in.active = active;
  in.states = states;
  in.bRank = bRank;
  in.designated = -1;
  in.outlet = -1;
  in.playState116c = 1;
  in.restartKind1554 = 0;
  in.manLateral = {0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  in.manDepth = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  in.ballX = 0.0f;
  in.sideSign = 1;
  const auto out = mentality::Cf310FirstHalf(in);
  EXPECT_EQ(3, out[2]);
}

// FUN_1405CFFE0's keeper arm (lane-post §4c): a keeper B-word (3) keeps 3 over band input.
TEST(MentalityLanePost, Cffe80KeeperKeepsThree) {
  std::array<int, 11> a{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
  std::array<int, 11> b{3, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  std::array<int, 11> cls{0, 1, 3, 1, 1, 1, 1, 7, 8, 9, 2};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> states{};
  mentality::Cffe0Input in;
  in.a = a;
  in.b = b;
  in.cls = cls;
  in.active = active;
  in.states = states;
  in.ballDepth = -50.0f;
  in.backEdge = 0.0f;
  in.midEdge = 10.0f;
  in.fwdEdge = 20.0f;
  const auto out = mentality::Cffe0Bands(in);
  EXPECT_EQ(3, out[0]);
}

// FUN_1405CFFE0's band arms (lane-post §4c): behind the back edge an unguarded B == 1 slot
// takes (backEdge + 5 <= ballDepth) + 1.
TEST(MentalityLanePost, Cffe80BehindBackEdgeLiftsByFive) {
  std::array<int, 11> a{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
  std::array<int, 11> b{1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  std::array<int, 11> cls{3, 1, 1, 1, 1, 1, 1, 7, 8, 9, 2};
  std::array<int, 11> active{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::array<int, 11> states{};
  states[2] = 0x3A;
  states[5] = 0x3A;
  mentality::Cffe0Input in;
  in.a = a;
  in.b = b;
  in.cls = cls;
  in.active = active;
  in.states = states;
  in.ballDepth = -50.0f;
  in.backEdge = 0.0f;
  in.midEdge = 10.0f;
  in.fwdEdge = 20.0f;
  in.guarded = false;
  const auto out = mentality::Cffe0Bands(in);
  // backEdge + 5 = 5 <= ballDepth(-50)? No -> 0 + 1 = 1.
  EXPECT_EQ(1, out[2]);
  in.ballDepth = 6.0f;
  const auto out2 = mentality::Cffe0Bands(in);
  // backEdge + 5 = 5 <= 6, still behind the mid edge (10) -> 1 + 1 = 2.
  EXPECT_EQ(2, out2[2]);
}

// FUN_1404FB2A0 (lane-leaves §4): the T+0x78 keep-or-clear gate.
TEST(MentalityLaneKick, KeepAtModeByteAndClearOutOfPlay) {
  // MI+0x1775 == 0x0C keeps whatever id is set.
  EXPECT_EQ(7, mentality::KickManKeep(7, 0x0C, 1, true, true));
  // In play (116C == 1) with both gates true keeps; either gate false clears to 0xFF.
  EXPECT_EQ(7, mentality::KickManKeep(7, 0, 1, true, true));
  EXPECT_EQ(0xFF, mentality::KickManKeep(7, 0, 1, false, true));
  EXPECT_EQ(0xFF, mentality::KickManKeep(7, 0, 1, true, false));
  // Any non-play phase except 2/3 clears to 0xFF.
  EXPECT_EQ(0xFF, mentality::KickManKeep(7, 0, 4, true, true));
  EXPECT_EQ(0xFF, mentality::KickManKeep(7, 0, 0, true, true));
}

// FUN_140A63020 (lane-leaves §4): carrier -> holder -> T+0x78, each gated by id and side.
TEST(MentalityLaneKick, FallbackNeedsIdAndSideMatch) {
  // Carrier valid and side-matched: the carrier wins.
  EXPECT_EQ(3, mentality::KickManFallback(3, 0, 5, 1, 7, 1, 0));
  // Carrier valid but other side: holder leg (side-matched) wins.
  EXPECT_EQ(5, mentality::KickManFallback(3, 1, 5, 0, 7, 1, 0));
  // Carrier and holder both off-side: the current T+0x78 leg (side-matched) wins.
  EXPECT_EQ(7, mentality::KickManFallback(3, 1, 5, 1, 7, 0, 0));
  // Nothing side-matched: 0xFF.
  EXPECT_EQ(0xFF, mentality::KickManFallback(3, 1, 5, 1, 7, 1, 0));
  // An out-of-range id (0x16+) never matches.
  EXPECT_EQ(0xFF, mentality::KickManFallback(0x16, 0, 0x16, 0, 0x16, 0, 0));
}

// FUN_1404F0E50's ramp (holder-tree §4): FUN_1404F4AB0(a,b,c) clamps 0..1.
TEST(MentalityLaneKick, RampClampsZeroToOne) {
  EXPECT_FLOAT_EQ(0.0f, mentality::KickRamp(1.0f, 3.0f, 5.0f));
  EXPECT_FLOAT_EQ(1.0f, mentality::KickRamp(6.0f, 3.0f, 5.0f));
  EXPECT_FLOAT_EQ(0.5f, mentality::KickRamp(4.0f, 3.0f, 5.0f));
}

// The CE4F0 copy/recompute assembly (lane-writers §2 + lane-post §4): the copy path takes the
// B-words; otherwise the recompute runs CFFE0 -> CF310 -> CFC80 -> designated/B -> fallback.
TEST(MentalityLaneWire, CopyPathTakesBWordsVerbatim) {
  std::array<int, 11> b{3, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  std::array<int, 11> a{};
  mentality::LaneWordsInput in;
  in.b = b;
  in.copyPath = true;
  EXPECT_EQ(b, mentality::LaneWords(in, a));
}

// The recompute default (post-steps dormant on GF inputs): 3, then designated/B -> 2, B fallback.
TEST(MentalityLaneWire, RecomputeFallsBackToBWords) {
  std::array<int, 11> b{3, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  std::array<int, 11> a{};
  mentality::LaneWordsInput in;
  in.b = b;
  in.cls = {0, 1, 3, 4, 5, 6, 1, 7, 8, 9, 2};
  in.active = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  in.states = {};
  in.bRank = {};
  in.designated = 5;
  in.copyPath = false;
  const auto out = mentality::LaneWords(in, a);
  EXPECT_EQ(3, out[0]);
  EXPECT_EQ(0, out[5]);
  EXPECT_EQ(0, out[2]);
  EXPECT_EQ(0, out[10]);
}
// The sort FUN_140A79640 (lane-leaves §1): ascending, stable; rank = index in the sorted order.
TEST(MentalityLaneSort, AscendingStableWithRank) {
  std::array<uint8_t, 11> slots{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  const std::array<float, 11> keys{1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 5.0f, 5.0f, 4.0f, 3.0f, 3.0f, 3.0f};
  std::array<uint8_t, 11> order{};
  std::array<uint8_t, 11> rank{};
  mentality::SortLaneSlots(slots, keys, order, rank);
  EXPECT_EQ((std::array<uint8_t, 11>{1, 2, 4, 0, 3, 8, 9, 10, 7, 5, 6}), order);
  EXPECT_EQ(3u, rank[0]);
  EXPECT_EQ(0u, rank[1]);
  EXPECT_EQ(1u, rank[2]);
}

// The A-words FUN_1405CE4F0 (lane-writers §2): the recompute default is 3; designated or B == 2
// forces 2; otherwise the slot's B word; inactive is 3; the copy path takes the B words.
TEST(MentalityLaneA, RecomputeDefaultsDesignatedAndFallback) {
  // B words here (not classes): keeper B = 3, slot 1 holds line 1, slot 10 holds line 2.
  std::array<int, 11> b{3, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  std::array<int, 11> a{};
  mentality::LaneAWords(b, a, 5, std::array<int, 11>{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
  EXPECT_EQ(3, a[0]);
  EXPECT_EQ(2, a[5]);
  EXPECT_EQ(1, a[1]);
  EXPECT_EQ(2, a[10]);
  std::array<int, 11> active{};
  active.fill(1);
  active[2] = 0;
  mentality::LaneAWords(b, a, -1, active);
  EXPECT_EQ(3, a[2]);
}

TEST(MentalityLaneA, CopyPathTakesTheBWords) {
  std::array<int, 11> b{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 2};
  std::array<int, 11> a{};
  mentality::CopyLaneAWords(b, a);
  for (int k = 0; k < 11; ++k) EXPECT_EQ(b[k], a[k]);
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
