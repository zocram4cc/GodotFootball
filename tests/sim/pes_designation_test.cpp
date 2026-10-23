#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "onthepitch/pesdesignation.hpp"
#include "pes/exe.hpp"

namespace {

using pes::designation::BucketByClass;
using pes::designation::ClassBuckets;
using pes::designation::Constants;
using pes::designation::DesignationRecord;
using pes::designation::DesignateTwoCf;
using pes::designation::FillOrdinals;
using pes::designation::FoldClass;
using pes::designation::kNoSlot;
using pes::designation::kTwoCfGate;
using pes::designation::OrdinalFill;
using pes::designation::OrdinalInput;
using pes::designation::OrderTable;
using pes::designation::Publish;

// The members of one bucket, read back from its count and slot arrays.
std::vector<int> Members(const ClassBuckets& buckets, int bucket) {
  return std::vector<int>(buckets.slot[bucket].begin(),
                          buckets.slot[bucket].begin() + buckets.count[bucket]);
}

// The record's slot ids in emission order.
std::vector<int> Emitted(const DesignationRecord& record) {
  return std::vector<int>(record.slot.begin(), record.slot.begin() + record.count);
}

// The install's constants, or a skip when PES21_ROOT is unset.
std::optional<Constants> InstallConstants() {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return std::nullopt;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  EXPECT_TRUE(exe) << err;
  if (!exe) return std::nullopt;
  auto constants = pes::designation::LoadConstants(*exe, &err);
  EXPECT_TRUE(constants) << err;
  return constants;
}

// An input for FillOrdinals with every man on the pitch, the keeper at slot 0 and the outfield in
// class 3 with a score equal to his slot, and no list. Tests override what they need.
OrdinalInput OutfieldInput() {
  OrdinalInput in;
  in.onPitch.fill(true);
  in.classB.fill(3);
  in.classB[0] = 0;
  for (int k = 0; k < pes::designation::kSideSlots; ++k) in.aerial[k] = static_cast<float>(k);
  in.boxCount = 4;
  in.eligibleCount = pes::designation::kSideSlots;
  in.lowRank = -100.0f;
  in.highClassBonus = 1000.0f;
  return in;
}

}  // namespace

// Role classes GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9 (pes-re-role-writer §5) fold into the
// buckets 0, 1, 3, 6 and 9. A bucket keeps its members in slot order.
TEST(PesDesignation, ClassesBucketed) {
  EXPECT_EQ(0, FoldClass(0));
  EXPECT_EQ(1, FoldClass(1));
  EXPECT_EQ(1, FoldClass(2));
  EXPECT_EQ(3, FoldClass(3));
  EXPECT_EQ(3, FoldClass(4));
  EXPECT_EQ(6, FoldClass(5));
  EXPECT_EQ(6, FoldClass(6));
  EXPECT_EQ(9, FoldClass(7));
  EXPECT_EQ(9, FoldClass(8));
  EXPECT_EQ(9, FoldClass(9));
  EXPECT_EQ(0, FoldClass(10));

  const std::array<int, 11> classB = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 2};
  const ClassBuckets home = BucketByClass(0, classB);
  EXPECT_EQ((std::vector<int>{0}), Members(home, 0));
  EXPECT_EQ((std::vector<int>{1, 2, 10}), Members(home, 1));
  EXPECT_EQ((std::vector<int>{3, 4}), Members(home, 3));
  EXPECT_EQ((std::vector<int>{5, 6}), Members(home, 6));
  EXPECT_EQ((std::vector<int>{7, 8, 9}), Members(home, 9));
  for (int unfolded : {2, 4, 5, 7, 8}) EXPECT_TRUE(Members(home, unfolded).empty()) << unfolded;

  // Side 1's slots are the global ids 11..21.
  const ClassBuckets away = BucketByClass(1, classB);
  EXPECT_EQ((std::vector<int>{12, 13, 21}), Members(away, 1));
  EXPECT_EQ((std::vector<int>{18, 19, 20}), Members(away, 9));
}

// A bucket of c members is emitted in the order of the install's row c (raw 0x140A40F30). Three
// defenders with keys -5, -1, -3 sort to slots 0, 2, 1, and row 3 emits ranks 1, 2, 0: slots 2, 1, 0.
// Eight forwards with keys -10 .. -0.5 sort to slots 3..10 and row 8 emits ranks 4,3,5,2,6,1,7,0.
TEST(PesDesignation, LateralOrderWithinBucket) {
  const auto constants = InstallConstants();
  if (!constants) GTEST_SKIP() << "PES21_ROOT unset";
  const OrderTable& order = constants->order;

  const std::array<int, 11> classB = {1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9};
  const std::array<float, 11> lateral = {-5.0f, -1.0f, -3.0f, -10.0f, -9.0f, -8.0f,
                                         -7.0f, -6.0f, -4.0f, -2.0f, -0.5f};
  EXPECT_EQ((std::vector<int>{2, 1, 0, 7, 6, 8, 5, 9, 4, 10, 3}),
            Emitted(Publish(0, classB, lateral, order)));

  // One defender at slot 0 and ten forwards at slots 1..10 with keys -10 .. -1: the forwards sort to
  // 1..10, and row 10 emits ranks 5,4,6,3,7,2,8,1,9,0. The defender's bucket comes first.
  const std::array<int, 11> classC = {1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
  const std::array<float, 11> lateralC = {2.0f, -10.0f, -9.0f, -8.0f, -7.0f, -6.0f,
                                          -5.0f, -4.0f, -3.0f, -2.0f, -1.0f};
  EXPECT_EQ((std::vector<int>{0, 6, 5, 7, 4, 8, 3, 9, 2, 10, 1}),
            Emitted(Publish(0, classC, lateralC, order)));
}

// A single member emits itself, whatever his key: the sort runs over the bucket's members only, so
// a positive key cannot sort him out of his side's block.
TEST(PesDesignation, SingleMemberBucketEmitsItself) {
  // Row 10 is the identity here, so a bucket of ten keepers is emitted in slot order. The lone
  // defender (bucket 1) follows them, whatever his positive key.
  OrderTable order{};
  for (int r = 0; r < 10; ++r) order[10 * 10 + r] = static_cast<uint8_t>(r);
  const std::array<int, 11> classB = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  std::array<float, 11> lateral{};
  lateral[0] = 2.0f;
  const DesignationRecord record = Publish(1, classB, lateral, order);
  ASSERT_EQ(11, record.count);
  EXPECT_EQ(11, record.slot[10]);
}

// The install's order table is the one the publisher reads, and the two ranking constants are the
// signs FUN_14051B8B0 needs: a penalty below zero and a bonus above it. The loader checks the rows.
TEST(PesDesignationInstall, ConstantsLoadFromTheInstall) {
  const auto constants = InstallConstants();
  if (!constants) GTEST_SKIP() << "PES21_ROOT unset";
  EXPECT_EQ(0, constants->order[pes::designation::kOrderRowStride * 1]);
  EXPECT_LT(constants->lowRank, 0.0f);
  EXPECT_GT(constants->highClassBonus, 0.0f);
}

// The bit-15 men (the CPU stage) take the top ordinals in slot order, then the rest of the box by
// score. Classes 3..9 keep their score; with no designated man the penalty puts nothing above them.
TEST(PesDesignation, BitFifteenMenFirstThenScore) {
  OrdinalInput in = OutfieldInput();
  in.extraFrontman[2] = true;
  in.extraFrontman[5] = true;
  const OrdinalFill fill = FillOrdinals(in);
  // Stage one: slots 2 and 5 at ordinals 10 and 9. Stage two: the highest scores 10 and 9, and the
  // limit of four stops it. The keeper, slot 0, takes ordinal 0 because he holds no place.
  EXPECT_EQ(4, fill.placedCount);
  EXPECT_EQ((std::array<int, 4>{2, 5, 10, 9}),
            (std::array<int, 4>{fill.placed[0], fill.placed[1], fill.placed[2], fill.placed[3]}));
  EXPECT_EQ((std::array<int, 11>{0, kNoSlot, kNoSlot, kNoSlot, kNoSlot, kNoSlot, kNoSlot, 9, 10, 5, 2}),
            fill.ordinal);
}

// The designated man is exempt from the penalty, so a CB with the highest score ranks first. Without
// the exemption a CB sits at the penalty and does not get in.
TEST(PesDesignation, DesignatedManIsExemptFromThePenalty) {
  OrdinalInput in = OutfieldInput();
  in.classB[1] = 1;
  in.aerial[1] = 20.0f;
  in.boxCount = 1;
  in.designated = 1;
  const OrdinalFill fill = FillOrdinals(in);
  ASSERT_EQ(1, fill.placedCount);
  EXPECT_EQ(1, fill.placed[0]);
  EXPECT_EQ(1, fill.ordinal[10]);

  in.designated = kNoSlot;
  const OrdinalFill penalised = FillOrdinals(in);
  ASSERT_GE(penalised.placedCount, 1);
  EXPECT_NE(1, penalised.placed[0]);
}

// Class 1 sits at the penalty, class 2 scores 0, classes 6..9 get the bonus. With the box of three
// the order is the bonus man, the class-3 man, the class-2 man, and the penalised CB is not taken.
TEST(PesDesignation, ClassPenaltiesAndBonusRankTheBox) {
  OrdinalInput in = OutfieldInput();
  for (int k = 1; k < pes::designation::kSideSlots; ++k) in.classB[k] = 0;
  in.classB[1] = 1;   // CB: penalised
  in.aerial[1] = 10.0f;
  in.classB[2] = 2;   // SB: scores 0
  in.aerial[2] = 9.0f;
  in.classB[3] = 6;   // AMF: score 1, then the bonus
  in.aerial[3] = 1.0f;
  in.classB[4] = 3;   // DMF: score 5
  in.aerial[4] = 5.0f;
  in.boxCount = 3;
  const OrdinalFill fill = FillOrdinals(in);
  EXPECT_EQ((std::array<int, 3>{3, 4, 2}),
            (std::array<int, 3>{fill.placed[0], fill.placed[1], fill.placed[2]}));
  EXPECT_EQ(3, fill.placedCount);
}

// The remainder comes from the published record: elements 10 down to 1, each only while its index is
// below the count. Element 0 (slot 7 here) is never read, so slot 7 is not placed.
TEST(PesDesignation, RemainderReadsTheRecordTopDownWithoutElementZero) {
  OrdinalInput in = OutfieldInput();
  for (int k = 0; k < pes::designation::kSideSlots; ++k) in.aerial[k] = 0.0f;
  in.boxCount = 1;
  DesignationRecord list;
  list.count = 4;
  list.slot = {7, 3, 9, 5, 0, 0, 0, 0, 0, 0, 0};
  in.list = &list;
  const OrdinalFill fill = FillOrdinals(in);
  EXPECT_EQ(1, fill.placedCount);
  EXPECT_EQ((std::array<int, 11>{0, kNoSlot, kNoSlot, kNoSlot, kNoSlot, kNoSlot, kNoSlot, 3, 9, 5, 1}),
            fill.ordinal);
}

// The keeper's place is a rule of the fill: when no man is eligible nothing is filled at all.
TEST(PesDesignation, NoEligibleMenLeavesTheOrdinalsEmpty) {
  OrdinalInput in = OutfieldInput();
  in.eligibleCount = 0;
  const OrdinalFill fill = FillOrdinals(in);
  EXPECT_EQ(0, fill.placedCount);
  for (int ordinal : fill.ordinal) EXPECT_EQ(kNoSlot, ordinal);
}

// The 2-CF pair is the first two on-pitch SS or CF slots, and only under the gate T+0x87CD == 2.
TEST(PesDesignation, PairArmedOnlyWhen87CD2) {
  const std::array<int, 11> classB = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 2};  // SS at 7, CF at 9
  std::array<bool, 11> onPitch{};
  onPitch.fill(true);
  const std::array<int, 2> none{kNoSlot, kNoSlot};

  EXPECT_EQ((std::array<int, 2>{7, 9}), DesignateTwoCf(0, kTwoCfGate, classB, onPitch).slot);
  for (int gate : {0, 1, 3}) {
    EXPECT_EQ(none, DesignateTwoCf(0, gate, classB, onPitch).slot) << "gate " << gate;
  }

  // One striker off the pitch leaves a single man, which is no pair.
  onPitch[9] = false;
  EXPECT_EQ(none, DesignateTwoCf(0, kTwoCfGate, classB, onPitch).slot);
  onPitch[9] = true;

  // Side 1's slots are the global ids 11..21.
  EXPECT_EQ((std::array<int, 2>{18, 20}), DesignateTwoCf(1, kTwoCfGate, classB, onPitch).slot);

  // Only the first two strikers in slot order count, and a WF (class 8) is not a striker.
  const std::array<int, 11> fourStrikers = {0, 1, 2, 3, 4, 5, 6, 9, 9, 9, 9};
  EXPECT_EQ((std::array<int, 2>{7, 8}),
            DesignateTwoCf(0, kTwoCfGate, fourStrikers, onPitch).slot);
  const std::array<int, 11> oneStriker = {0, 1, 2, 3, 4, 5, 6, 8, 9, 0, 0};
  EXPECT_EQ(none, DesignateTwoCf(0, kTwoCfGate, oneStriker, onPitch).slot);
}
