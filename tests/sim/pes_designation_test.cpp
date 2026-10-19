#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "onthepitch/pesdesignation.hpp"

namespace {

using pes::designation::BucketByClass;
using pes::designation::ClassBuckets;
using pes::designation::FoldClass;

// The members of one bucket, read back from its count and slot arrays.
std::vector<int> Members(const ClassBuckets& buckets, int bucket) {
  return std::vector<int>(buckets.slot[bucket].begin(),
                          buckets.slot[bucket].begin() + buckets.count[bucket]);
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
