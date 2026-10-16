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

void EventLog::Push(uint32_t ring, uint16_t slot) {
  ++count;
  if (count > kEntries) count = static_cast<uint32_t>(kEntries);
  ++head;
  if (head >= kEntries) head = 0;
  entries[head].ring = ring;
  entries[head].slot = slot;
}

void EventLog::Clear() {
  count = 0;
  head = 0;
  for (auto& e : entries) e = EventLogEntry{};
}

int EventKindBlock::Push(const EventRecord& rec) {
  ++count;
  if (count > 1) count = 1;
  ++head;
  if (head != 0) head = 0;
  record = rec;
  return static_cast<int>(head);
}

void EventStore::Reset() {
  for (auto& r : rings_) r = EventRing8{};
  overflow_ = EventRing10{};
  log_ = EventLog{};
  for (auto& k : kindBlocks_) k = EventKindBlock{};
  newest_ = EventRecord{};
  newestSecond_ = EventRecord{};
  hasNewest_ = false;
  hasNewestSecond_ = false;
}

void EventStore::Route(const EventRingTable& table, const EventRecord& rec) {
  const uint32_t kind = rec.kind;
  const int ring = table.RingOf(kind);
  const int slot = (ring < 10) ? Ring(ring).Push(rec) : overflow_.Push(rec);
  log_.Push(static_cast<uint32_t>(ring), static_cast<uint16_t>(slot));
  if (kind == 7) {
    // The kind-7 path also copies the payload to the newest slots. PES keeps
    // 0x60 bytes there; only the payload half of the record is written.
    hasNewest_ = true;
    std::memcpy(reinterpret_cast<uint8_t*>(&newest_) + 0x14,
                reinterpret_cast<const uint8_t*>(&rec) + 0x14, kNewestBytes);
    const ActorKey key{rec.U32(0x18), rec.U32(0x1c)};  // payload+4
    if (key.Valid()) {
      std::memcpy(reinterpret_cast<uint8_t*>(&newestSecond_) + 0x14,
                  reinterpret_cast<const uint8_t*>(&rec) + 0x14, kNewestBytes);
      hasNewestSecond_ = true;
    }
  }
  if (kind < kKindBlockCount) kindBlocks_[kind].Push(rec);
}

const EventRecord* EventStore::NewestOrRing0(int n) const {
  // FUN_140a3e190 returns E+0x180698 for n == 0 with no empty test: the copy is
  // the store's own memory, which `hasNewest_` reports on.
  if (n == 0) return hasNewest_ ? &newest_ : nullptr;
  const EventRing8& ring = rings_[0];
  if (static_cast<uint32_t>(n) >= ring.count) return nullptr;
  int i = static_cast<int>(ring.head) - n;
  if (i < 0) i += 8;
  const EventRecord& slot = ring.slots[static_cast<size_t>(i)];
  // An empty slot reads as PES's empty-event sentinel (an action kind of 0).
  return slot.kind == EventRecord::kEmptyKind ? nullptr : &slot;
}

const EventRecord* EventStore::RingRecord(int ring, int n) const {
  if (ring < 0 || ring >= static_cast<int>(kRingCount)) return nullptr;
  const EventRing8& r = rings_[static_cast<size_t>(ring)];
  if (static_cast<uint32_t>(n) >= r.count) return nullptr;
  int i = static_cast<int>(r.head) - n;
  if (i < 0) i += 8;
  return &r.slots[static_cast<size_t>(i)];
}

const EventRecord* EventStore::KindLast(int kind) const {
  if (kind < 0 || kind >= static_cast<int>(kKindBlockCount)) return nullptr;
  const EventKindBlock& block = kindBlocks_[static_cast<size_t>(kind)];
  if (block.count == 0) return nullptr;
  // FUN_140a3da00 holds the head at 0, so the record sits in slot 0.
  return &block.record;
}

const EventRecord* EventStore::KindNth(int kind, int n) const {
  int seen = 0;
  const uint32_t walk = log_.count < EventLog::kEntries
                            ? log_.count
                            : static_cast<uint32_t>(EventLog::kEntries);
  for (uint32_t i = 0; i < walk; ++i) {
    int idx = static_cast<int>(log_.head) - static_cast<int>(i);
    if (idx < 0) idx += static_cast<int>(EventLog::kEntries);
    const EventLogEntry& entry = log_.entries[static_cast<size_t>(idx)];
    const EventRecord* rec = entry.ring < 10 ? &rings_[entry.ring].slots[entry.slot]
                                             : &overflow_.slots[entry.slot];
    if (rec->kind == static_cast<uint32_t>(kind)) {
      if (seen == n) return rec;
      ++seen;
    }
  }
  return nullptr;
}

const EventRecord* EventStore::LastShot(int maxFrame) const {
  for (int n = 0; n < 8; ++n) {
    const EventRecord* rec = NewestOrRing0(n);
    if (rec == nullptr) continue;
    if (rec->ActionKind() == 5 && (maxFrame == 0 || rec->ActionFrame() <= maxFrame)) {
      return rec;
    }
  }
  return nullptr;
}

ActorKey EventStore::Ring1Key(int n) const {
  const EventRing8& ring = rings_[1];
  if (static_cast<uint32_t>(n) >= ring.count) return NoActorKey();
  int i = static_cast<int>(ring.head) - n;
  if (i < 0) i += 8;
  const EventRecord& slot = ring.slots[static_cast<size_t>(i)];
  if (slot.kind == EventRecord::kEmptyKind) return NoActorKey();  // FUN_140a60500
  return ActorKey{slot.U32(0x1c), slot.U32(0x20)};  // payload+8
}

ActorKey EventStore::NewestSecondKey() const {
  if (!hasNewestSecond_) return NoActorKey();  // nothing valid has been routed
  uint32_t team = 0;
  uint32_t slot = 0;
  std::memcpy(&team, reinterpret_cast<const uint8_t*>(&newestSecond_) + 0x18, 4);
  std::memcpy(&slot, reinterpret_cast<const uint8_t*>(&newestSecond_) + 0x1c, 4);
  return ActorKey{team, slot};
}

int ActorIdOf(const ActorKey& key, const std::array<uint32_t, 0x28>& roster, int side) {
  if (!key.Valid()) return 0xff;
  for (size_t i = 0; i < roster.size(); ++i) {
    if (roster[i] != key.slot) continue;
    if (i > 10) return 0xff;  // FUN_140a3b320's 10 < uVar3
    return side * 11 + static_cast<int>(i);
  }
  return 0xff;
}

NewestEventView NewestEvent(const EventStore& store) {
  const EventRecord* rec = store.NewestOrRing0(0);
  if (rec == nullptr) return NewestEventView{};
  NewestEventView view;
  view.present = true;
  view.kind = rec->ActionKind();
  view.actor = ActorKey{rec->U32(0x18), rec->U32(0x1c)};
  view.frame = rec->ActionFrame();
  view.field1c = rec->I32(0x30);
  view.flag2d = rec->U8(0x41);
  view.subKind = rec->I32(0x54);
  return view;
}

bool IsLatchStepTwoKind(uint32_t kind) {
  return (kind >= 1 && kind <= 3) || kind == 0x10;
}

int LatchStepTwoWindowFrames(int gs) { return gs / 6; }

bool DodgeWindowRefuses(const EventStore& store, int side, int frame, int gs,
                        float windowFactor) {
  const EventRecord* rec = store.NewestOrRing0(0);
  if (rec == nullptr) return false;
  const uint32_t team = rec->U32(0x18);  // ev+4
  if (team != static_cast<uint32_t>(side)) return false;
  // PES's comparison is a float one: `gs * factor > age` (0x140a0b50c's mulss
  // then `ja`), with the age sign-extended before the conversion.
  const float age = static_cast<float>(static_cast<int64_t>(frame - rec->ActionFrame()));
  return static_cast<float>(gs) * windowFactor > age;
}

bool BusyFor(const EventRecord& rec, bool carrierIsPlayer, int ctxRole) {
  const uint32_t kind = rec.ActionKind();
  if (kind == 5) return ctxRole != 0 && ctxRole != 5;
  if ((kind == 6 || kind == 7) && !carrierIsPlayer) return true;
  return kind >= 9 && kind <= 14;
}

}  // namespace pes
