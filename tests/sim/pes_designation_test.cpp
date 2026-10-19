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
using pes::designation::DesignationRecord;
using pes::designation::DesignateTwoCf;
using pes::designation::FoldClass;
using pes::designation::kNoSlot;
using pes::designation::kTwoCfGate;
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

// The record lists the buckets in order. Inside a bucket of c members the k-th emitted member is
// the one with sort rank DAT[10c+k] (install table, FUN_140A40CA0 0x140A40F30-F67), so the order is
// not ascending in lateral. Keys below zero sort ahead of the zero-keyed padding and are emitted.
TEST(PesDesignation, LateralOrderWithinBucket) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto order = pes::designation::LoadOrderTable(*exe, &err);
  ASSERT_TRUE(order) << err;

  // Side 0: a CB trio (bucket 1) at slots 0, 1, 2 with keys -5, -1, -3, and eight SS/WF/CF
  // (bucket 9) at slots 3..10 with keys -10 .. -0.5. Ranks in bucket 1: slot 0, 2, 1.
  const std::array<int, 11> classB = {1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9};
  const std::array<float, 11> lateral = {-5.0f, -1.0f, -3.0f, -10.0f, -9.0f, -8.0f,
                                         -7.0f, -6.0f, -4.0f, -2.0f, -0.5f};
  EXPECT_EQ((std::vector<int>{2, 1, 0, 7, 6, 8, 5, 9, 4, 10, 3}),
            Emitted(Publish(0, classB, lateral, *order)));

  // One CB at slot 0 with key +2.0 and ten SS/WF/CF at slots 1..10 with keys -10 .. -1. The
  // positive key sorts behind the padding, so bucket 1 emits no slot.
  const std::array<int, 11> classC = {1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
  const std::array<float, 11> lateralC = {2.0f, -10.0f, -9.0f, -8.0f, -7.0f, -6.0f,
                                          -5.0f, -4.0f, -3.0f, -2.0f, -1.0f};
  EXPECT_EQ((std::vector<int>{kNoSlot, 6, 5, 7, 4, 8, 3, 9, 2, 10, 1}),
            Emitted(Publish(0, classC, lateralC, *order)));
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
