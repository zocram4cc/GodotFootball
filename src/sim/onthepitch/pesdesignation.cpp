#include "onthepitch/pesdesignation.hpp"

#include <cmath>
#include <cstring>

#include "pes/exe.hpp"

namespace pes::designation {
namespace {

// One float of the image, little-endian as PE stores it.
bool ReadF32(const PeImage& exe, uint64_t va, float* out) {
  uint8_t bytes[sizeof(float)];
  if (!exe.ReadVa(va, sizeof bytes, bytes)) return false;
  std::memcpy(out, bytes, sizeof bytes);
  return std::isfinite(*out);
}

// A candidate of FUN_14051B8B0's ranking: a local slot and its score.
struct Candidate {
  int slot;
  float score;
};

// FUN_14051B8B0's score for a man (raw 0x14051B8B0 L206-249). The designated man is exempt. Class 1
// is -100 whatever the keeper-chase flag says (the flag only lifts the rest), class 2 and classes 10+
// score 0, classes 3..9 keep the score, and classes 6..9 get the high-class bonus.
float RankScore(const OrdinalInput& in, int k, bool keeperFirst) {
  const int cls = in.classB[k];
  float score = in.aerial[k];
  if (in.designated != k) {
    score = keeperFirst ? in.aerial[k] : in.lowRank;
    if (cls == 1) {
      score = in.lowRank;
    } else if (cls != kKeeperClass) {
      score = (cls >= 3 && cls <= 9) ? in.aerial[k] : 0.0f;
    }
  }
  if (cls > 5) score += in.highClassBonus;
  return score;
}

}  // namespace

std::optional<Constants> LoadConstants(const PeImage& exe, std::string* error) {
  Constants k;
  if (!exe.ReadVa(kOrderTableVa, k.order.size(), k.order.data())) {
    *error = "designation order table: VA 0x142643596 is not readable";
    return std::nullopt;
  }
  for (uint8_t rank : k.order) {
    if (rank >= kSideSlots) {
      *error = "designation order table: a rank is past the side's slots";
      return std::nullopt;
    }
  }
  // Rows 1..10 hold each member's rank once, so each is a permutation of its count.
  for (int count = 1; count <= kClassCount; ++count) {
    unsigned seen = 0;
    for (int r = 0; r < count; ++r) {
      const unsigned bit = 1u << k.order[kOrderRowStride * count + r];
      if (seen & bit) {
        *error = "designation order table: a row is not a permutation";
        return std::nullopt;
      }
      seen |= bit;
    }
  }
  if (!ReadF32(exe, kLowRankVa, &k.lowRank) || !(k.lowRank < 0.0f)) {
    *error = "designation low rank: VA 0x14259D674 is not a negative float";
    return std::nullopt;
  }
  if (!ReadF32(exe, kHighClassBonusVa, &k.highClassBonus) || !(k.highClassBonus > 0.0f)) {
    *error = "designation class bonus: VA 0x14256AE34 is not a positive float";
    return std::nullopt;
  }
  return k;
}

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

DesignationRecord Publish(int side, const std::array<int, kSideSlots>& classB,
                          const std::array<float, kSideSlots>& lateral, const OrderTable& order) {
  const ClassBuckets buckets = BucketByClass(side, classB);
  DesignationRecord record;
  for (int bucket = 0; bucket < kClassCount; ++bucket) {
    const int members = buckets.count[bucket];
    if (members == 0) continue;
    // FUN_140A40CA0 compacts the bucket to its members and sorts them ascending by key (the
    // insertion base of FUN_140A79640: a member moves left only while it is strictly below its
    // predecessor, so equal keys keep slot order).
    std::array<int, kSideSlots> sorted = buckets.slot[bucket];
    for (int i = 1; i < members; ++i) {
      const int current = sorted[i];
      int j = i;
      while (j > 0 && lateral[sorted[j - 1] % kSideSlots] > lateral[current % kSideSlots]) {
        sorted[j] = sorted[j - 1];
        --j;
      }
      sorted[j] = current;
    }
    // The emission: the k-th member is the sorted member at rank DAT[members][k] (raw 0x140A40F30).
    for (int k = 0; k < members; ++k) {
      record.slot[record.count++] = sorted[order[kOrderRowStride * members + k]];
    }
  }
  return record;
}

OrdinalFill FillOrdinals(const OrdinalInput& in) {
  OrdinalFill fill;
  fill.ordinal.fill(kNoSlot);

  // The keeper is the last class-0 slot, on the pitch or not (raw scan of P+0x3FB8, uVar20).
  int keeper = kNoSlot;
  for (int k = 0; k < kSideSlots; ++k) {
    if (in.classB[k] == kKeeperClass) keeper = k;
  }
  // FUN_14051B8B0 fills nothing when no man is eligible (bVar22 == 0).
  if (in.eligibleCount == 0) return fill;

  // local_154: the box count, capped by the eligible men.
  const int limit = in.eligibleCount < in.boxCount ? in.eligibleCount : in.boxCount;
  int next = kSideSlots - 1;  // uVar23: the free ordinal, falling from 10
  unsigned mask = 0;          // local_164: the men already ordinal-placed
  auto place = [&](int slot) {
    fill.ordinal[next--] = slot;
    mask |= 1u << slot;
  };
  auto placeInBox = [&](int slot) {
    place(slot);
    fill.placed[fill.placedCount++] = slot;
  };

  // Stage one: the CPU side's bit-15 men in slot order, or the human side's sheet takers. A taker
  // named twice is placed once here; PES stores him twice (ponytail: no duplicate in the ordinal list).
  if (in.humanSide) {
    for (int t = 0; t < 3; ++t) {
      const int k = in.takers[t];
      if (k != kNoSlot && in.onPitch[k] && !(mask & (1u << k))) placeInBox(k);
    }
  } else {
    for (int k = 0; k < kSideSlots; ++k) {
      if (in.onPitch[k] && in.extraFrontman[k]) placeInBox(k);
    }
  }

  // The keeper goes up next when the chase holds (local_166); a keeper already placed is not repeated.
  const bool keeperFirst = in.keeperChase;
  if (keeperFirst && keeper != kNoSlot && !(mask & (1u << keeper))) placeInBox(keeper);

  // Stage two: the eligible men by score, descending. The sort is stable here; PES's quicksort
  // (FUN_141E53540) leaves ties in an order that is not ported (ponytail).
  std::array<Candidate, kSideSlots> ranked{};
  int count = 0;
  for (int k = 0; k < kSideSlots; ++k) {
    if (in.onPitch[k]) ranked[count++] = Candidate{k, RankScore(in, k, keeperFirst)};
  }
  for (int i = 1; i < count; ++i) {
    const Candidate current = ranked[i];
    int j = i;
    while (j > 0 && ranked[j - 1].score < current.score) {
      ranked[j] = ranked[j - 1];
      --j;
    }
    ranked[j] = current;
  }
  for (int i = 0; i < count; ++i) {
    const int k = ranked[i].slot;
    // A man is ranked when he is not the keeper, not already placed, and not a restart man.
    if (in.classB[k] == kKeeperClass || (mask & (1u << k)) || in.restartMan[k]) continue;
    // PES stores the man, then tests the limit (raw 0x14051B8B0 L285-298).
    placeInBox(k);
    if (limit <= fill.placedCount) break;
  }

  // The remainder: the published record's tail, from element 10 down to element 1 (element 0 is never
  // read), each element counted only while its index is below the record's count.
  if (in.list != nullptr) {
    for (int e = kSideSlots - 1; e >= 1; --e) {
      if (e >= in.list->count) continue;
      const int global = in.list->slot[e];
      if (global < 0 || global >= 2 * kSideSlots) continue;
      const int k = global % kSideSlots;
      if (!in.onPitch[k] || (mask & (1u << k))) continue;
      place(k);
    }
  }

  // Ordinal 0 is the keeper unless he already holds a place (PES's SP[4] write, kNoSlot when none).
  if (keeper == kNoSlot || !(mask & (1u << keeper))) fill.ordinal[0] = keeper;
  return fill;
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
