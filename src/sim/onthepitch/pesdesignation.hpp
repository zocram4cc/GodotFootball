#pragma once

// PES 2021's designation publisher and the set-piece ordinal fill that reads its record.
//
// Publisher (team tick step 27b): FUN_14051E350 runs FUN_140A40CA0 for the side's slots and stores
// the record {count, slot[11]} at [ctx+0xBA8]+0x7CDC, with a twin at +0x7D0C. Reader: FUN_14051B8B0,
// the corner ordinal fill, which takes the record's tail after its bit-15 and score stages.
// docs/re/pes-re-team-layer.md §2 V4; pes-re-team-layer2.md §1-§4; pes-re-extra-frontman.md §1;
// pes-re-setpiece-choreography.md §11.6.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace pes {
class PeImage;
}

namespace pes::designation {

inline constexpr int kSideSlots = 11;   // slots per side: FUN_140A40CA0 `i < 11`
inline constexpr int kClassCount = 10;  // role classes 0..9 (pesroles.hpp): the publisher's buckets
inline constexpr int kKeeperClass = 0;  // GK
inline constexpr int kNoSlot = -1;      // no slot (PES stores 0xFF)

// DAT_142643596 (FUN_140A40CA0, raw 0x140A40F24): the emission ranks. Row c, bytes 10c..10c+9, gives
// the sorted rank each of a bucket's c members is emitted from. Rows 1..10 are permutations of 0..c-1.
inline constexpr uint64_t kOrderTableVa = 0x142643596;
inline constexpr int kOrderRowStride = 10;
inline constexpr int kOrderBytes = kOrderRowStride * kSideSlots + kSideSlots;  // rows 0..11
using OrderTable = std::array<uint8_t, kOrderBytes>;

// The two float constants of FUN_14051B8B0's ranking: DAT_14259D674 (-100.0) is the rank of a man who
// is not the designated one and is not a keeper-chase, and DAT_14256AE34 (+1000.0) lifts classes 6..9.
inline constexpr uint64_t kLowRankVa = 0x14259D674;
inline constexpr uint64_t kHighClassBonusVa = 0x14256AE34;

// The install constants of the publisher and the ordinal fill, read with PeImage::ReadVa.
struct Constants {
  OrderTable order{};
  float lowRank = 0.0f;
  float highClassBonus = 0.0f;
};

std::optional<Constants> LoadConstants(const PeImage& exe, std::string* error);

// The key the publisher sorts by is the slot point's f2, read raw for both sides (FUN_140A40CA0): no side factor.

// FUN_140A40CA0's bucket of a folded class: {1,2}->1 {3,4}->3 {5,6}->6 {7,8,9}->9, else 0.
int FoldClass(int roleClass);

// The side's members of each bucket, in slot order: slot[b][0..count[b]-1] are global slot ids
// (side * 11 + i).
struct ClassBuckets {
  std::array<int, kClassCount> count{};
  std::array<std::array<int, kSideSlots>, kClassCount> slot{};
};

ClassBuckets BucketByClass(int side, const std::array<int, kSideSlots>& classB);

// FUN_140A40CA0's record: the count, then the global slot ids in emission order.
struct DesignationRecord {
  int count = 0;
  std::array<int, kSideSlots> slot{};
};

// Buckets the side's slots by folded class. Each bucket's members are sorted ascending by lateral
// key (stable), and the k-th emitted member is the sorted member at rank DAT[c][k].
DesignationRecord Publish(int side, const std::array<int, kSideSlots>& classB,
                          const std::array<float, kSideSlots>& lateral, const OrderTable& order);

// The inputs of FUN_14051B8B0 for one side. Each field names the PES reader or writer it stands for;
// the caller supplies GF's stand-in where PES's input is not carried.
struct OrdinalInput {
  int side = 0;
  // FUN_140A55CF0: the human side fills its first stage from the sheet's takers, not from bit 15.
  bool humanSide = false;
  // MI+0x1748 (and T+0x1748) on-pitch flags, per local slot.
  std::array<bool, kSideSlots> onPitch{};
  // P+0x3FB8: the role class per local slot (the publisher's B array).
  std::array<int, kSideSlots> classB{};
  // Block +0x31 + +0x2c + +0x1d: height, jump and heading, the score every ranking uses.
  std::array<float, kSideSlots> aerial{};
  // FUN_140A64030(slot, 0xF): the CPU bit-15 test (Extra Frontman).
  std::array<bool, kSideSlots> extraFrontman{};
  // FUN_1408C5060(T+0x8E08, k), k = 0..2, as local slots (kNoSlot when none).
  std::array<int, 3> takers{kNoSlot, kNoSlot, kNoSlot};
  // P+0x3EF8: the designated aerial man (FUN_1405FC860), exempt from the ranking penalty.
  int designated = kNoSlot;
  // FUN_140509890: the keeper goes up and the penalties give way to the score.
  bool keeperChase = false;
  // FUN_140A3C8F0: MI+0x173C and MI+0x1740, the restart men who may not be ranked.
  std::array<bool, kSideSlots> restartMan{};
  // T+0x6D0: the men in the box, and the count of eligible men (T+0x4C2C[k] != 0).
  int boxCount = 0;
  int eligibleCount = 0;
  // P+0x7CDC: this side's published record.
  const DesignationRecord* list = nullptr;
  float lowRank = 0.0f;
  float highClassBonus = 0.0f;
};

struct OrdinalFill {
  // SP[4+k]: the local slot at ordinal k (0 = keeper), kNoSlot where none is set.
  std::array<int, kSideSlots> ordinal{};
  // P+0x7C94: the men of the first two stages in placement order. These are the men in the box.
  std::array<int, kSideSlots> placed{};
  int placedCount = 0;
};

OrdinalFill FillOrdinals(const OrdinalInput& in);

// The 2-CF designator (FUN_140501160). Its gate is T+0x87CD == kTwoCfGate (raw 0x14050117B), and it
// pairs the first two on-pitch SS7 or CF9 men in slot order (raw 0x1405011D7-E6).
inline constexpr int kTwoCfGate = 2;
inline constexpr int kClassSS = 7;
inline constexpr int kClassCF = 9;

// The pair FUN_140501160 writes to T+0xB82C/B830: the first two on-pitch SS or CF slots in slot order,
// or kNoSlot twice when the gate is not kTwoCfGate or fewer than two such slots are on the pitch. The
// armed byte T+0xB828 is 1 on both arms and is not modelled here.
struct TwoCfPair {
  std::array<int, 2> slot{kNoSlot, kNoSlot};
};

TwoCfPair DesignateTwoCf(int side, int gate, const std::array<int, kSideSlots>& classB,
                         const std::array<bool, kSideSlots>& onPitch);

}  // namespace pes::designation
