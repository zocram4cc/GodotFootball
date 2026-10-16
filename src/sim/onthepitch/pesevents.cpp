#include "onthepitch/pesevents.hpp"

#include <cstring>

namespace pes {
namespace {

// FUN_140a6bd60/FUN_140a6be70's divisors, read off the instruction immediates:
// 0xe10 = 3600 and 0x3c = 60 at 0x140a6bd65/0x140a6bd6a and 0x140a6be7a.
constexpr int kSecondsPerStateHour = 3600;
constexpr int kSecondsPerMinute = 60;
// FUN_140a3c070's byte fold (0x140a3c0ac...0x140a3c120): `byte * 0x3f`.
constexpr int kStateFold = 0x3f;
// FUN_140a3c070's FNV-1a constants (0x140a3c111/0x140a3c0e7).
constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ull;
constexpr uint64_t kFnvPrime = 0x100000001b3ull;
// FUN_140a3dc40's routing table: 56 ints at 0x142642e60.
constexpr uint64_t kRingTableVa = 0x142642e60;
constexpr size_t kRingTableLen = 56;

}  // namespace

void EventRecord::Reset() {
  std::memset(this, 0, sizeof *this);
  p6 = kResetP6;
  kind = kEmptyKind;
}

void EventRecord::Write(uint32_t k, int32_t f1, int32_t f2, float state, int32_t p) {
  kind = k;
  frame1 = f1;
  frame2 = f2;
  stateByte = StateByteOf(state);
  p6 = p;
}

uint32_t EventRecord::U32(size_t off) const {
  uint32_t v = 0;
  std::memcpy(&v, reinterpret_cast<const uint8_t*>(this) + off, sizeof v);
  return v;
}

int32_t EventRecord::I32(size_t off) const {
  int32_t v = 0;
  std::memcpy(&v, reinterpret_cast<const uint8_t*>(this) + off, sizeof v);
  return v;
}

float EventRecord::F32(size_t off) const {
  float v = 0.f;
  std::memcpy(&v, reinterpret_cast<const uint8_t*>(this) + off, sizeof v);
  return v;
}

uint8_t EventRecord::U8(size_t off) const {
  return reinterpret_cast<const uint8_t*>(this)[off];
}

void EventRecord::SetU32(size_t off, uint32_t v) {
  std::memcpy(reinterpret_cast<uint8_t*>(this) + off, &v, sizeof v);
}

void EventRecord::SetI32(size_t off, int32_t v) {
  std::memcpy(reinterpret_cast<uint8_t*>(this) + off, &v, sizeof v);
}

void EventRecord::SetF32(size_t off, float v) {
  std::memcpy(reinterpret_cast<uint8_t*>(this) + off, &v, sizeof v);
}

void EventRecord::SetU8(size_t off, uint8_t v) {
  reinterpret_cast<uint8_t*>(this)[off] = v;
}

bool ActorKey::Valid() const { return team < 2 && slot < EventRecord::kSlotLimit; }

bool ActorKey::operator==(const ActorKey& o) const {
  return team == o.team && slot == o.slot;
}

ActorKey NoActorKey() { return ActorKey{EventRecord::kNoActorTeam, EventRecord::kNoActorSlot}; }

uint32_t StateWord(const StateWordInput& in) {
  const uint32_t k = in.selected;
  const uint32_t counter = static_cast<uint32_t>(in.counter * 3);
  const uint32_t c0 = counter & 0xffu;
  const uint32_t c1 = (counter + 4u) >> 8 & 0xffu;
  const uint32_t c2 = (counter + 8u) >> 16 & 0xffu;
  const uint32_t c3 = (counter + 0xcu) >> 24 & 0xffu;
  const uint32_t b0 = ((k & 0xffu) + c0) ^ (c0 * kStateFold);
  const uint32_t b1 = (((k >> 8) & 0xffu) + c1) ^ (c1 * kStateFold);
  const uint32_t b2 = (((k >> 16) & 0xffu) + c2) ^ (c2 * kStateFold);
  const uint32_t b3 = (((k >> 24) & 0xffu) + c3) ^ (c3 * kStateFold);
  return (b0 & 0xffu) | (b1 & 0xffu) << 8 | (b2 & 0xffu) << 16 | (b3 & 0xffu) << 24;
}

bool StateWordCheckHolds(const StateWordInput& in) {
  const uint32_t k = in.selected;
  const uint32_t counter = static_cast<uint32_t>(in.counter * 3);
  const uint32_t c0 = counter & 0xffu;
  const uint32_t c1 = (counter + 4u) >> 8 & 0xffu;
  const uint32_t c2 = (counter + 8u) >> 16 & 0xffu;
  const uint32_t c3 = (counter + 0xcu) >> 24 & 0xffu;
  const uint8_t bytes[4] = {
      static_cast<uint8_t>(((k & 0xffu) + c0) ^ (c0 * kStateFold)),
      static_cast<uint8_t>((((k >> 8) & 0xffu) + c1) ^ (c1 * kStateFold)),
      static_cast<uint8_t>((((k >> 16) & 0xffu) + c2) ^ (c2 * kStateFold)),
      static_cast<uint8_t>((((k >> 24) & 0xffu) + c3) ^ (c3 * kStateFold))};
  uint64_t h = kFnvOffsetBasis;
  for (uint8_t b : bytes) h = (h ^ b) * kFnvPrime;
  return h == in.check;
}

float StateWordFloat(const StateWordInput& in) {
  return static_cast<float>(StateWord(in)) + in.offset;
}

uint8_t StateByteOf(float state) {
  // cvttss2si at 0x140a6bd60/0x140a6be70: the word is truncated toward zero.
  const int word = static_cast<int>(state);
  const int hours = word / kSecondsPerStateHour;
  const int minuteRemainder = word / kSecondsPerMinute % kSecondsPerMinute;
  return static_cast<uint8_t>(hours + (minuteRemainder != 0 ? 1 : 0));
}

int EventRingTable::RingOf(uint32_t kind) const {
  if (kind >= ring.size()) return 10;  // the overflow ring FUN_140a3dc40 uses
  return ring[kind];
}

std::optional<EventRingTable> LoadEventRingTable(const PeImage& exe, std::string* error) {
  std::array<uint8_t, kRingTableLen * sizeof(int32_t)> raw{};
  if (!exe.ReadVa(kRingTableVa, raw.size(), raw.data())) {
    if (error) *error = "the ring table at 0x142642e60 is not mapped";
    return std::nullopt;
  }
  EventRingTable table;
  for (size_t i = 0; i < kRingTableLen; ++i) {
    std::memcpy(&table.ring[i], &raw[i * sizeof(int32_t)], sizeof(int32_t));
  }
  return table;
}

}  // namespace pes
