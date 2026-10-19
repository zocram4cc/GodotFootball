#pragma once

// PES 2021's designation publisher for one side (team tick step 27b: FUN_14051E350 calls
// FUN_140A40CA0 for the side's slots). It buckets the eleven slots by role class and orders each
// bucket laterally into the record the set-piece readers walk (docs/re/pes-re-team-layer.md §2 V4,
// docs/re/pes-re-team-layer2.md §1-§3).

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "onthepitch/pesroles.hpp"

namespace pes {
class PeImage;
}

namespace pes::designation {

inline constexpr int kSideSlots = roles::kSideSlots;  // slots per side (FUN_140A40CA0 `i < 11`)
inline constexpr int kClassCount = 10;                // role classes 0..9, which are also bucket indices
inline constexpr int kNoSlot = 0xFF;                  // the empty id in FUN_140A40CA0's sort buffer

// The bucket a role class joins under flag 1, which the tick publisher always passes: {1,2}->1,
// {3,4}->3, {5,6}->6, {7,8,9}->9, and every other class (0, and anything past 9) -> 0. Raw jump
// table 0x140A40D54-0D69, default 0x140A40D70.
int FoldClass(int roleClass);

// The side's members of each bucket, in slot order: slot[b][0..count[b]-1] are global slot ids
// (side * 11 + i).
struct ClassBuckets {
  std::array<int, kClassCount> count{};
  std::array<std::array<int, kSideSlots>, kClassCount> slot{};
};

ClassBuckets BucketByClass(int side, const std::array<int, kSideSlots>& classB);

// FUN_140A40CA0's order table DAT_142643596 (raw 0x140A40F24: LEA R9,[R13+0x2643596], R13 =
// 0x140000000), read from the install. Row c, starting at byte 10c, gives for a bucket of c members
// the sort rank each emitted member takes; rows 1..10 are permutations of 0..c-1.
inline constexpr uint64_t kOrderTableVa = 0x142643596;
inline constexpr int kOrderRowStride = 10;  // raw 0x140A40F20-F2B: the row is 10 bytes, RAX = 5c
inline constexpr int kOrderBytes = kOrderRowStride * kSideSlots + kSideSlots;  // rows 0..11
using OrderTable = std::array<uint8_t, kOrderBytes>;

std::optional<OrderTable> LoadOrderTable(const PeImage& exe, std::string* error);

// FUN_140A40CA0's record: the count, then the slot ids in emission order (kNoSlot when the sort
// put an empty buffer entry at the rank).
struct DesignationRecord {
  int count = 0;
  std::array<int, kSideSlots> slot{};
};

// Buckets the side's slots by folded class, sorts each bucket's lateral keys (ascending, stable,
// over a 22-entry buffer padded with {kNoSlot, 0.0}), and emits each bucket in DAT row order.
// `lateral[i]` is the key of slot i: f2 of the slot-point row for i (FUN_140A40CA0 `S+0x3AA4+12*i`).
DesignationRecord Publish(int side, const std::array<int, kSideSlots>& classB,
                          const std::array<float, kSideSlots>& lateral, const OrderTable& order);

}  // namespace pes::designation
