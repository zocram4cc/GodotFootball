// The refresh slice of the ball-list writer FUN_14045B090 (MA+0x2470C), the gate FUN_140457070 puts in front of it
// (docs/ports/pes-port-KickInputs.md section 1). Out of the forced state the writer runs when the AI object's slice counter
// (this+0x10), incremented once per tick, passes a limit: 4 while the newest ring-1 event is one of the first two kinds with a
// small second word (FUN_140A3E7D0 ring 1 newest, FUN_140A60530: word0 < 2 && word1 < 0x28), else 4 less the ball's speed in
// km/h over 15 (the compiler's division by 15, kept as written: multiply by 0x77777777, shift, subtract).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pes {

// ring 1 of the event store E: the count at E+0x3208, the head at E+0x320C, eight 0x320-byte records from E+0x1908.
inline constexpr std::size_t kBallListRingCountOffset = 0x3208, kBallListRingHeadOffset = 0x320C;
inline constexpr std::size_t kBallListRingRecordsOffset = 0x1908, kBallListRingRecordBytes = 800, kBallListRingRecords = 8;
// The record's kind word (0x37 = none: FUN_140A3E7D0 then answers {2, 0xFF}, which FUN_140A60530 refuses) and the pair it tests.
inline constexpr uint32_t kBallListEventNone = 0x37, kBallListEventKindLimit = 2, kBallListEventWordLimit = 0x28;
inline constexpr std::size_t kBallListEventPairOffset = 0x1C;  // two u32 words: record + 0x1C, + 0x20
inline constexpr int kBallListFastLimit = 4;                   // the limit with a fresh event, and the slow limit's start
inline constexpr int64_t kBallListDivideBy15Magic = 0x77777777;
inline constexpr uint64_t kBallListSpeedFactorVa = 0x14259D65C;  // f32 -3.6, the km/h factor

// FUN_140A60530(FUN_140A3E7D0(E ring 1, newest)): whether the newest ring-1 event is a fresh one. `eventStore` is E.
inline bool BallListFreshEvent(const uint8_t* eventStore) {
  const auto word = [&](std::size_t at) {
    uint32_t v;
    std::memcpy(&v, eventStore + at, sizeof v);
    return v;
  };
  const int32_t count = static_cast<int32_t>(word(kBallListRingCountOffset));
  if (count <= 0) return false;  // index 0 is not below the count: the default record, whose kind is none
  int32_t index = static_cast<int32_t>(word(kBallListRingHeadOffset));
  if (index < 0) index += static_cast<int32_t>(kBallListRingRecords);
  const std::size_t record = kBallListRingRecordsOffset + static_cast<std::size_t>(index) * kBallListRingRecordBytes;
  if (word(record) == kBallListEventNone) return false;
  return word(record + kBallListEventPairOffset) < kBallListEventKindLimit &&
         word(record + kBallListEventPairOffset + 4) < kBallListEventWordLimit;
}

// The slice limit (FUN_140457070's decompile): `speedKmhNegative` is (int)(|ball velocity| * DAT_14259D65C), that float being -3.6.
inline int BallListSliceLimit(bool freshEvent, int speedKmhNegative) {
  if (freshEvent) return kBallListFastLimit;
  const int hi = static_cast<int>((static_cast<int64_t>(speedKmhNegative) * kBallListDivideBy15Magic) >> 32);
  const int t = hi - speedKmhNegative;
  return kBallListFastLimit - ((t >> 3) - (t >> 31));
}

}  // namespace pes
