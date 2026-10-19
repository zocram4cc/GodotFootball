#include "onthepitch/pesdesignation.hpp"

namespace pes::designation {

// Role classes: GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9 (pesroles.hpp).
int FoldClass(int roleClass) {
  switch (roleClass) {
    case 1:  // CB
    case 2:  // SB
      return 1;
    case 3:  // DMF
    case 4:  // CMF
      return 3;
    case 5:  // SMF
    case 6:  // AMF
      return 6;
    case 7:  // SS
    case 8:  // WF
    case 9:  // CF
      return 9;
    default:  // GK, and anything outside the class map
      return 0;
  }
}

ClassBuckets BucketByClass(int side, const std::array<int, kSideSlots>& classB) {
  ClassBuckets buckets;
  for (int i = 0; i < kSideSlots; ++i) {
    const int bucket = FoldClass(classB[i]);
    buckets.slot[bucket][buckets.count[bucket]++] = side * kSideSlots + i;
  }
  return buckets;
}

}  // namespace pes::designation
