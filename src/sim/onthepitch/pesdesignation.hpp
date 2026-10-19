#pragma once

// PES 2021's designation publisher for one side (team tick step 27b: FUN_14051E350 calls
// FUN_140A40CA0 for the side's slots). It buckets the eleven slots by role class and orders each
// bucket laterally into the record the set-piece readers walk (docs/re/pes-re-team-layer.md §2 V4,
// docs/re/pes-re-team-layer2.md §1-§3).

#include <array>

#include "onthepitch/pesroles.hpp"

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

}  // namespace pes::designation
