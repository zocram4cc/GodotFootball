#include "onthepitch/pesdesignation.hpp"

#include <algorithm>

#include "pes/exe.hpp"

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

std::optional<OrderTable> LoadOrderTable(const PeImage& exe, std::string* error) {
  OrderTable table{};
  if (!exe.ReadVa(kOrderTableVa, table.size(), table.data())) {
    *error = "designation order table: VA 0x142643596 is not readable";
    return std::nullopt;
  }
  for (uint8_t rank : table) {
    if (rank >= kSideSlots) {
      *error = "designation order table: a rank is past the side's slots";
      return std::nullopt;
    }
  }
  // Rows 1..10 hold each member's rank once, so each is a permutation of its count.
  for (int count = 1; count <= kClassCount; ++count) {
    unsigned seen = 0;
    for (int k = 0; k < count; ++k) {
      const unsigned bit = 1u << table[kOrderRowStride * count + k];
      if (seen & bit) {
        *error = "designation order table: a row is not a permutation";
        return std::nullopt;
      }
      seen |= bit;
    }
  }
  return table;
}

namespace {

// One element of FUN_140A40CA0's sort buffer: a slot id and its lateral key (8 bytes, slot first).
struct BufferEntry {
  int slot;
  float key;
};

}  // namespace

DesignationRecord Publish(int side, const std::array<int, kSideSlots>& classB,
                          const std::array<float, kSideSlots>& lateral, const OrderTable& order) {
  const ClassBuckets buckets = BucketByClass(side, classB);
  DesignationRecord record;
  for (int bucket = 0; bucket < kClassCount; ++bucket) {
    const int members = buckets.count[bucket];
    if (members == 0) continue;
    // Every entry starts as {kNoSlot, 0.0}; the bucket's members fill this side's block in slot
    // order. The sort runs over the whole buffer, so the padding interleaves with real keys.
    std::array<BufferEntry, kBufferSlots> sorted{};
    for (BufferEntry& entry : sorted) entry = BufferEntry{kNoSlot, 0.0f};
    for (int k = 0; k < members; ++k) {
      const int id = buckets.slot[bucket][k];
      sorted[side * kSideSlots + k] = BufferEntry{id, lateral[id % kSideSlots]};
    }
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const BufferEntry& a, const BufferEntry& b) { return a.key < b.key; });
    for (int k = 0; k < members; ++k) {
      const int rank = order[kOrderRowStride * members + k];
      record.slot[record.count++] = sorted[side * kSideSlots + rank].slot;
    }
  }
  return record;
}

TwoCfPair DesignateTwoCf(int side, int gate, const std::array<int, kSideSlots>& classB,
                         const std::array<bool, kSideSlots>& onPitch) {
  TwoCfPair pair;
  if (gate != kTwoCfGate) return pair;
  int found = 0;
  for (int i = 0; i < kSideSlots && found < 2; ++i) {
    if (!onPitch[i] || (classB[i] != kClassSS && classB[i] != kClassCF)) continue;
    pair.slot[found++] = side * kSideSlots + i;
  }
  return found == 2 ? pair : TwoCfPair{};
}

}  // namespace pes::designation
