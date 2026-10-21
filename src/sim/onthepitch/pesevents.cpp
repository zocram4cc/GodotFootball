#include "onthepitch/pesevents.hpp"

#include <cmath>
#include <cstring>

#include "gametypes.hpp"

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

uint16_t EventRecord::U16(size_t off) const {
  uint16_t v = 0;
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

void EventRecord::SetU16(size_t off, uint16_t v) {
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

NewestEventView EventAt(const EventStore& store, int n) {
  const EventRecord* rec = store.NewestOrRing0(n);
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

NewestEventView NewestEvent(const EventStore& store) { return EventAt(store, 0); }

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

std::optional<EventConstants> LoadEventConstants(const PeImage& exe, std::string* error) {
  EventConstants k;
  if (!exe.ReadVa(0x142598958, sizeof k.deflectionSample,
                  reinterpret_cast<uint8_t*>(&k.deflectionSample)) ||
      !exe.ReadVa(0x14259be10, sizeof k.shortScale,
                  reinterpret_cast<uint8_t*>(&k.shortScale)) ||
      !exe.ReadVa(0x14257234c, sizeof k.shortUpper,
                  reinterpret_cast<uint8_t*>(&k.shortUpper)) ||
      !exe.ReadVa(0x142648170, sizeof k.shortLower,
                  reinterpret_cast<uint8_t*>(&k.shortLower))) {
    if (error) *error = "an event constant is not mapped in the install";
    return std::nullopt;
  }
  return k;
}

void WriteClampedShorts(EventRecord* rec, size_t off, const float* sample,
                        const EventConstants& k) {
  auto clamp = [&k](float v) {
    v *= k.shortScale;
    if (v > k.shortUpper) return k.shortUpper;
    return v > k.shortLower ? v : k.shortLower;
  };
  const int16_t x = static_cast<int16_t>(static_cast<int>(clamp(sample[0])));
  const int16_t z = static_cast<int16_t>(static_cast<int>(clamp(sample[2])));
  rec->SetU8(off, static_cast<uint8_t>(x & 0xff));
  rec->SetU8(off + 1, static_cast<uint8_t>((static_cast<uint16_t>(x) >> 8) & 0xff));
  rec->SetU8(off + 2, static_cast<uint8_t>(z & 0xff));
  rec->SetU8(off + 3, static_cast<uint8_t>((static_cast<uint16_t>(z) >> 8) & 0xff));
}

// FUN_140a60300: two 4-byte key words equal.
bool KeysEqual(const EventRecord& a, const EventRecord& b) {
  return a.U32(0x18) == b.U32(0x18) && a.U32(0x1c) == b.U32(0x1c);
}

bool DetectPhase(PhaseInput* in, EventRecord* out) {
  if (in->stored == in->phase) return false;
  out->Write(1, in->frame, in->frame, in->state, in->phase);
  out->SetU32(0x14, static_cast<uint32_t>(in->phase));  // FUN_140a3d4f0
  out->SetU32(0x18, static_cast<uint32_t>(in->stored));
  in->stored = in->phase;
  return true;
}

bool DetectDeflection(const DeflectionInput& in, EventRecord* out) {
  if (!in.gateOpen) return false;
  if (in.sample1 != in.sampleConst) return false;
  if (0.0f <= in.sample1 - in.sample0) return false;
  out->Write(9, in.frame, in.frame, in.state, in.phase);
  return true;
}

int OutOfPlayCode(float x, float z, float radius) {
  // sgn(v) = -1 if v < 0 else +1: the point moves toward the centre by one radius, so a bit fires
  // once the whole ball is past the line (pes-re-out-side.md 3).
  const float xs = x - (x < 0.0f ? -radius : radius);
  const float zs = z - (z < 0.0f ? -radius : radius);
  const float ax = std::fabs(xs), az = std::fabs(zs);
  // The band edges 12 and 8 metres (0x14259be04, 0x14259bdf4; pes-re-out-side.md 4).
  constexpr float kBandNear_m = 12.0f;
  constexpr float kBandInset_m = 8.0f;
  const float penaltyLine = pesPenaltyAreaLineX, penaltyHalf = pesPenaltyAreaHalfWidth;
  int code = 0;
  code |= (ax <= pesFieldHalfLength && az <= pesFieldHalfWidth) ? 0x1 : 0x2;
  if (az > pesFieldHalfWidth && ax <= pesFieldHalfLength) code |= 0x4;
  if (ax > pesFieldHalfLength) code |= 0x8;
  code |= xs <= 0.0f ? 0x10 : 0x20;
  if (zs > 0.0f) code |= 0x40;
  if (zs < 0.0f) code |= 0x80;
  if (az >= penaltyHalf) code |= 0x100;
  if (az >= penaltyHalf && ax > kBandNear_m && ax < penaltyLine - kBandInset_m) code |= 0x2000;
  if (az >= penaltyHalf && ax > penaltyLine - kBandInset_m && ax < pesFieldHalfLength) code |= 0x1000;
  if (ax >= penaltyLine && az < penaltyHalf) code |= xs > 0.0f ? 0x800 : 0x400;
  if (ax >= pesGoalAreaLineX && az < pesGoalAreaHalfWidth) code |= 0x200;
  if (ax < pesCircleRadius && az < pesCircleRadius) code |= 0x4000;
  return code;
}

bool DetectOutOfPlay(OutOfPlayInput* in, EventRecord* out) {
  if (in->stored == in->code) return false;
  in->stored = in->code;
  // The new code is the record's first payload word (pes-re-out-side.md 5, the K=10 payload).
  out->Write(0x0a, in->frame, in->frame, in->code, 0);
  return true;
}

bool DetectSnapshot(const SnapshotInput& in, EventRecord* out) {
  if (in.store == nullptr || in.current == nullptr) return false;
  if (BusyFor(*in.current, true, 0)) return false;  // FUN_140a6ce10(payload, 1, 0)
  const uint32_t kind = in.current->ActionKind();
  if (kind == 4 || kind == 0x11) return false;
  const EventRecord* found = nullptr;
  for (int n = 1; n < 8; ++n) {
    const EventRecord* rec = in.store->NewestOrRing0(n);
    if (rec == nullptr) continue;
    if (rec->ActionKind() == 0x11) return false;
    if (!ActorKey{rec->U32(0x18), rec->U32(0x1c)}.Valid()) continue;
    if (!BusyFor(*rec, true, 0)) {
      found = rec;
      break;
    }
  }
  if (found == nullptr) return false;
  if (found->ActionKind() - 4 <= 1) return false;  // kinds 4 and 5
  if (found->U32(0x18) == in.current->U32(0x18)) return false;
  out->Write(11, in.frame, in.frame, in.state, in.phase);
  // FUN_140a156b0 resets three 0x64-byte blocks; the three payload copies
  // follow: the current record, the newest event, and the record found.
  auto copy = [out](size_t to, const EventRecord& from) {
    std::memcpy(reinterpret_cast<uint8_t*>(out) + to,
                reinterpret_cast<const uint8_t*>(&from) + 0x14, 0x64);
  };
  copy(0x14, *in.current);
  copy(0x78, *in.store->NewestOrRing0(0));
  copy(0xdc, *found);
  return true;
}

bool DetectPossessionFlip(PossessionFlipInput* in, EventRecord* out,
                          const EventConstants& k) {
  if (in->stored == in->team0InPossession) return false;
  in->stored = in->team0InPossession;
  out->Write(12, in->frame, in->frame, in->state, in->phase);
  out->SetU32(0x14, 2);  // overwritten below, as PES does
  out->SetU32(0x14, in->stored ? 0u : 1u);
  out->SetU32(0x18, 0);  // FUN_140a6cc70
  const float sample[3] = {in->ballX, 0.f, in->ballZ};
  WriteClampedShorts(out, 0x18, sample, k);
  return true;
}

bool DetectInterval(IntervalInput* in, EventRecord* out) {
  if (!in->latched) {
    if (!in->latch) return false;
    in->latched = true;
    in->startFrame = in->frame;
    in->startPhase = in->phase;
    return false;
  }
  if (in->latch) return false;
  in->latched = false;
  out->Write(14, in->startFrame, in->frame, in->state, in->startPhase);
  out->SetU32(0x18, static_cast<uint32_t>(in->team));  // payload+4
  out->SetU32(0x14, static_cast<uint32_t>(in->idx));   // payload+0
  out->SetU8(0x20, in->idx < 0x30 && in->latchFlag ? 1 : 0);  // payload+0x1c
  return true;
}

bool DetectRestart(RestartInput* in, EventRecord* out) {
  if (in->stored == in->restartByte) return false;
  in->stored = in->restartByte;
  if (in->restartByte == 0) return false;
  out->Write(0x10, in->frame, in->frame, in->state, in->phase);
  return true;
}

bool DetectFoul(FoulInput* in, EventRecord* out) {
  bool fired = false;
  if (in->restartState == 2) {
    if (in->stored == 1 && in->restartGate == 5) fired = true;
    if (in->foulFlag) fired = true;
    if (in->flag135c) fired = true;
  }
  in->stored = in->restartState;
  if (!fired) return false;
  out->Write(30, in->frame, in->frame, in->state, in->phase);
  // FUN_140a3d0c0's copy of the foul record MI+0x1584 into the payload.
  const EventConstants& k = *in->constants;
  out->SetU8(0x42, 0);  // payload+0x2e
  if (in->culpritSample != nullptr) {
    WriteClampedShorts(out, 0x14, in->culpritSample, k);
    if (in->culpritIsPair && in->culpritShortFlag) out->SetU8(0x42, 1);
  }
  out->SetU8(0x43, in->restartGate6 ? 1 : 0);  // payload+0x2f: MI+0x1554 == 6
  out->SetU32(0x18, static_cast<uint32_t>(in->foulKind));    // payload+4
  out->SetU32(0x1c, static_cast<uint32_t>(in->foulWord8));   // payload+8
  out->SetU32(0x20, static_cast<uint32_t>(in->foulCulprit));  // payload+0xc key
  out->SetU32(0x28, static_cast<uint32_t>(in->foulVictim));   // payload+0x14 key
  out->SetU32(0x30, static_cast<uint32_t>(in->foulWord14));   // payload+0x1c
  out->SetU32(0x34, static_cast<uint32_t>(in->foulWord18));  // payload+0x20
  out->SetU32(0x38, static_cast<uint32_t>(in->foulWord28));  // payload+0x24
  out->SetU32(0x3c, static_cast<uint32_t>(in->foulWord1c));  // payload+0x28
  out->SetU8(0x40, in->foulByte20);                          // payload+0x2c
  out->SetU8(0x41, in->foulByte21);                          // payload+0x2d
  return true;
}

namespace {

// The roster lookup both FUN_140a19120 and FUN_140a18940 do: the index i with
// [G+0x180] + i*0x74 + 0x20 == id, or 0x18 when none carries it.
constexpr int kRosterSlots = 0x18;
int RosterIndexOf(const std::array<uint32_t, kRosterSlots>& roster, int id) {
  for (int i = 0; i < kRosterSlots; ++i) {
    if (static_cast<int>(roster[static_cast<size_t>(i)]) == id) return i;
  }
  return kRosterSlots;
}

// FUN_140a19120: the ball's state 2 and state 4 records.
bool BuildDefaultBallEvent(const BallEventInput& in, EventRecord* out) {
  if (in.ballState == 2) {
    if (!in.state2Gate || in.restartPlayer < 0 || in.restartPlayer >= 0x16) return false;
    out->SetActionKind(0x11);
    out->SetU32(0x20, static_cast<uint32_t>(in.restartPlayer));  // payload+0xc
    return true;
  }
  if (in.ballState == 4) {
    if (!in.holderPairValid || in.holderPairPlayer < 0 || in.holderPairPlayer >= 0x16) {
      return false;
    }
    out->SetActionKind(8);
    out->SetU32(0x20, static_cast<uint32_t>(in.holderPairPlayer));
    return true;
  }
  return false;
}

// FUN_140a18690: the ball's second state.
bool BuildSecondBallEvent(const BallEventInput& in, EventRecord* out) {
  const int holder = in.holder;
  if (holder > 0x15) {
    out->SetActionKind(0xe);
    out->SetU8(0x4c, static_cast<uint8_t>(in.holderByte5));  // payload+0x38
    return true;
  }
  int kind = 0xd;
  if (in.holderFlag != 0) {
    // The two anime-class tests (FUN_140a6e4b0 / FUN_140a6e580 / FUN_140a6e070)
    // are not decoded; the caller supplies the kind the class resolves to,
    // defaulting to PES's 0xd.
    // ponytail: the three class tests, which pick 0xb, 0xc or 10 from 0xd.
    kind = in.holderClassKind;
  }
  out->SetActionKind(static_cast<uint32_t>(kind));
  out->SetU32(0x20, static_cast<uint32_t>(in.holder));   // payload+0xc
  out->SetU32(0x34, static_cast<uint32_t>(in.holderFlag));  // payload+0x20
  return true;
}

// FUN_140a18940: in play. The action kind is the ball's +0x27dc word, which is
// where the kinds 1..5 come from (FUN_140a18810's switch on it decides the
// actor and whether to record at all).
bool BuildInPlayBallEvent(const BallEventInput& in, EventRecord* out) {
  if (in.lastToucher < 0 || in.lastToucher > 0x15) return false;
  out->SetActionKind(static_cast<uint32_t>(in.actionKind));
  // FUN_140a62fe0's key: the side the id belongs to, and its roster slot.
  out->SetU32(0x18, static_cast<uint32_t>(in.lastToucher > 10 ? 1 : 0));
  out->SetU32(0x1c, static_cast<uint32_t>(in.lastToucherSlot));
  out->SetU32(0x20, static_cast<uint32_t>(RosterIndexOf(in.roster, in.lastToucher)));
  out->SetU32(0x34, static_cast<uint32_t>(in.holder == in.lastToucher ? in.holderFlag : 0));
  out->SetI32(0x38, static_cast<int8_t>(in.actorByte3e));  // payload+0x24
  out->SetU32(0x44, static_cast<uint32_t>(in.ballWord2818));  // payload+0x30
  out->SetU32(0x48, static_cast<uint32_t>(in.ballWord281c));  // payload+0x34
  // The kind 1/2 tail (FUN_140a17e10's struct, the second key, and the
  // payload+0x40 word the latch's step 3 reads) is filled by the caller's
  // `in.kickTail`, which FUN_140a43b870 and FUN_140a17e10 would compute.
  // ponytail: those two helpers, not decoded.
  if (in.actionKind == 1 || in.actionKind == 2) {
    out->SetU32(0x54, static_cast<uint32_t>(in.kickTailWord40));  // payload+0x40
    out->SetU32(0x2c, static_cast<uint32_t>(in.kickTailSub));    // payload+0x18
  }
  return true;
}

// FUN_140a17b30's de-duplication against the newest event, in the order the
// disassembly reads it.
bool DuplicatesNewest(const BallEventInput& in, const EventRecord& out) {
  if (!ActorKey{out.U32(0x18), out.U32(0x1c)}.Valid()) {
    // The invalid-key branch: a kind-14 record repeating the newest kind-14
    // record's 6..11 word is dropped.
    if (out.ActionKind() != 0xe || in.restartState == 1) return false;
    const EventRecord* newest = in.store->NewestOrRing0(0);
    if (newest == nullptr || newest->ActionKind() != 0xe) return false;
    const auto inband = [](int w) { return static_cast<unsigned>(w - 6) <= 5u; };
    return inband(static_cast<int>(newest->U32(0x54))) && inband(static_cast<int>(out.U32(0x54)));
  }
  const EventRecord* newest = in.store->NewestOrRing0(0);
  if (newest == nullptr) return false;
  if (!KeysEqual(*newest, out)) return false;
  if (newest->ActionKind() != out.ActionKind()) return false;
  if (newest->U8(0x41) != out.U8(0x41)) return false;  // payload+0x2c byte
  if (out.ActionKind() == 0x11 && newest->U32(0x30) != out.U32(0x30)) return false;
  if (in.restartState != 1) return false;
  if (out.ActionKind() == 0xb) return true;
  if (out.ActionKind() == 8) return true;
  return in.restartState == 1 && in.frame - newest->ActionFrame() < 3;
}

}  // namespace

bool DetectBallEvent(const BallEventInput& in, EventRecord* out) {
  if (in.store == nullptr) return false;
  out->Reset();
  bool built = false;
  if (in.ballState == 1 && in.kickGate) {
    built = BuildInPlayBallEvent(in, out);
  } else if (in.ballState == 2 && in.secondGate) {
    built = BuildSecondBallEvent(in, out);
  } else {
    built = BuildDefaultBallEvent(in, out);
  }
  if (!built) return false;
  // FUN_140a17cf0's fill: the frame, the state word, the phase, the restart
  // gate's word, the positions and the emit decision the fill adds.
  out->SetI32(0x24, in.frame);                                   // payload+0x10
  out->SetF32(0x28, in.state);                                   // payload+0x14
  out->SetU32(0x2c, static_cast<uint32_t>(in.phase));             // payload+0x18
  out->SetU32(0x30, static_cast<uint32_t>(out->U8(0x41) == 0 ? 0 : in.restartKind));
  const uint32_t kind = out->ActionKind();
  if (kind >= 1 && kind <= 5 && in.restartState == 1 && out->U32(0x30) == 0) {
    const EventRecord* newest = in.store->NewestOrRing0(0);
    if (newest != nullptr &&
        (KeysEqual(*newest, *out) || newest->ActionKind() == 0xd)) {
      out->SetU8(0x42, 1);  // payload+0x2e
    }
  }
  if (DuplicatesNewest(in, *out)) return false;
  out->Write(7, in.frame, in.frame, in.state, in.phase);
  return true;
}

Type14Record DetectType14(const Type14Input& in, EventRecord* out) {
  if (in.store == nullptr || in.type != 0x14) return Type14Record::kNone;
  if (in.fromKick) {
    // FUN_140a0dfd0's first record: a kind-8 record gated on ring 1's newest
    // key being valid (FUN_140a3e7d0).
    if (!in.store->Ring1Key(0).Valid()) return Type14Record::kNone;
    out->Write(8, in.frame, in.frame, in.state, in.phase);
    // FUN_140a867a0/FUN_140a867d0's payload: the empty key at payload+8 and
    // the ball sample at payload+0x14.
    out->SetU32(0x1c, EventRecord::kNoActorTeam);
    out->SetU32(0x20, EventRecord::kNoActorSlot);
    const float sample[3] = {in.ballX, 0.f, in.ballZ};
    WriteClampedShorts(out, 0x28, sample, *in.constants);
    return Type14Record::kOther;
  }
  if (!in.fromOther) return Type14Record::kNone;
  const bool gate = in.gate == 1;
  if (!in.store->Ring1Key(0).Valid()) return Type14Record::kNone;
  // The second kind-8 record, written only when the gate allows.
  if (gate) {
    out->Write(8, in.frame, in.frame, in.state, in.phase);
    out->SetU32(0x1c, EventRecord::kNoActorTeam);
    out->SetU32(0x20, EventRecord::kNoActorSlot);
    const float sample[3] = {in.ballX, 0.f, in.ballZ};
    WriteClampedShorts(out, 0x28, sample, *in.constants);
  }
  if (!gate) return Type14Record::kNone;
  // The kind-7 record with the 0xf payload and the tracked player.
  EventRecord touch;
  touch.Reset();
  touch.Write(7, in.frame, in.frame, in.state, in.phase);
  touch.SetActionKind(0xf);
  touch.SetU32(0x18, static_cast<uint32_t>(in.player));  // payload+4 key
  touch.SetU32(0x24, in.frame);                         // payload+0x10
  touch.SetF32(0x28, in.state);
  touch.SetU32(0x2c, static_cast<uint32_t>(in.phase));
  const EventRecord* newest = in.store->NewestOrRing0(0);
  if (newest != nullptr && KeysEqual(*newest, touch)) return Type14Record::kNone;
  if (newest != nullptr && newest->ActionKind() == touch.ActionKind()) {
    return Type14Record::kNone;
  }
  *out = touch;
  return Type14Record::kTouch;
}


bool Kind27Admits(const Kind27Input& in) {
  if (in.player >= 0x18) return false;               // T1
  if (in.entryWord != in.currentIndex) return false;  // T2
  return in.keyTeam < 2 && in.keySlot < EventRecord::kSlotLimit;  // T3
}

void Kind27Accumulate(Kind27Accumulator* acc, uint32_t routeKind, const Kind27Input& in) {
  if (routeKind != 0x1b || !Kind27Admits(in)) return;
  acc->count += 1;
  acc->durationSum += in.duration;
}

// FUN_140a61d40/FUN_140a61d90: the grids are the ball's halves divided by the
// install's DAT_1425c48b8 (x) and DAT_142588210 (z), clamped at the ends.
constexpr int kZoneColumns = 14;  // FUN_140a61d40's grid width
constexpr int kZoneRows = 10;     // FUN_140a61d90's grid height

int ZoneXOf(float halfX, float x) {
  if (halfX <= 0.f) return 0;
  const float v = x > -halfX ? (x < halfX ? x : halfX) : -halfX;
  const int i = static_cast<int>((v + halfX) / (2.f * halfX / 14.f));
  return i < 0 ? 0 : (i > kZoneColumns - 1 ? kZoneColumns - 1 : i);
}

int ZoneZOf(float halfZ, float z) {
  if (halfZ <= 0.f) return 0;
  const float v = z > -halfZ ? (z < halfZ ? z : halfZ) : -halfZ;
  const int i = static_cast<int>((v + halfZ) / (2.f * halfZ / kZoneRows));
  return i < 0 ? 0 : (i > kZoneRows - 1 ? kZoneRows - 1 : i);
}

void PossessionSequenceState::Reset() {
  *this = PossessionSequenceState{};
  // FUN_140a16930: the grid's count starts at 0 and its position at -1.
  gridCount = 0;
  zoneX = -1;
  zoneZ = -1;
}

bool DetectPossessionSequence(PossessionSequenceInput* in, PossessionSequenceState* st,
                              EventRecord* out) {
  if (in->store == nullptr || st == nullptr) return false;
  if (!st->hasKey) {
    // FUN_140a15f50's first branch: adopt the newest kind-7 record.
    if (in->phase == 9) return false;
    if (in->restartState != 1 && in->restartState != 3) return false;
    const EventRecord* rec = in->store->KindNth(7, 0);
    if (rec == nullptr) return false;
    if (!ActorKey{rec->U32(0x18), rec->U32(0x1c)}.Valid()) return false;
    // FUN_140a3e7d0's key decides `carrierIsPlayer` for the busy test; it is not
    // an admission gate of its own.
    if (BusyFor(*rec, in->store->Ring1Key(0).Valid(), 0)) return false;
    st->Reset();
    st->hasKey = true;
    st->key = ActorKey{rec->U32(0x18), rec->U32(0x1c)};
    // FUN_140a16930/FUN_140a17690: the tracked record's payload lands in the
    // detector's two copies.
    std::memcpy(reinterpret_cast<uint8_t*>(&st->first) + 0x14,
                reinterpret_cast<const uint8_t*>(rec) + 0x14, 0x68);
    std::memcpy(reinterpret_cast<uint8_t*>(&st->second) + 0x14,
                reinterpret_cast<const uint8_t*>(rec) + 0x14, 0x68);
    return false;
  }
  // The zone grid: FUN_140a61d40's x from the team's possession word, and
  // FUN_140a61d90's z from the ball.
  const int zoneX = ZoneXOf(in->zoneHalfX, in->ballDirX * in->ballSample0);
  const int zoneZ = ZoneZOf(in->zoneHalfZ, in->ballZ);
  if (zoneX != st->zoneX || zoneZ != st->zoneZ) {
    st->zoneX = zoneX;
    st->zoneZ = zoneZ;
    if (st->gridCount != -1 && zoneX < kZoneColumns && zoneZ < kZoneRows) {
      st->grid[static_cast<size_t>(zoneZ) + static_cast<size_t>(zoneX) * kZoneRows] += 1;
      st->gridCount += 1;
    }
  }
  if (in->teamFlag6e0) st->stoppageFlag = true;
  st->state = in->state;
  const EventRecord* rec = in->store->KindNth(7, 0);
  bool playerChanged = false;
  if (rec != nullptr) {
    if (ActorKey{rec->U32(0x18), rec->U32(0x1c)}.Valid() &&
        !BusyFor(*rec, in->store->Ring1Key(0).Valid(), 0)) {
      playerChanged = rec->U32(0x2c) != static_cast<uint32_t>(st->key.team);
      if (!playerChanged) {
        // The same tracked player: the second copy follows it.
        std::memcpy(reinterpret_cast<uint8_t*>(&st->second) + 0x14,
                    reinterpret_cast<const uint8_t*>(rec) + 0x14, 0x68);
      }
    }
  }
  if (!playerChanged && in->restartState != 2) return false;
  out->Reset();
  out->Write(13, in->frame, in->frame, st->state, in->phase);
  // FUN_140a156f0 resets the payload; the detector's tracked state follows.
  out->SetU32(0x14, st->key.team);  // payload+0: the tracked player's team word
  auto copy = [out](size_t to, const EventRecord& from, size_t bytes) {
    std::memcpy(reinterpret_cast<uint8_t*>(out) + to,
                reinterpret_cast<const uint8_t*>(&from) + 0x14, bytes);
  };
  copy(0x20, st->first, 0x60);
  copy(0x84, st->second, 0x48);
  out->SetU32(0xe4, static_cast<uint32_t>(st->gridCount));
  out->SetU32(0x18, static_cast<uint32_t>(in->phase));
  out->SetU8(0x318, st->flagD4 ? 1 : 0);
  out->SetU8(0x319, st->stoppageFlag ? 1 : 0);
  out->SetU8(0x31a, 0);
  out->SetU8(0x31b, st->stoppageFlag ? 1 : 0);
  for (int x = 0; x < kZoneColumns; ++x) {
    for (int z = 0; z < kZoneRows; ++z) {
      out->SetU32(0xe8 + static_cast<size_t>(z + x * kZoneRows) * 4,
                  st->grid[static_cast<size_t>(z) + static_cast<size_t>(x) * kZoneRows]);
    }
  }
  st->Reset();
  if (rec != nullptr) {
    st->hasKey = true;
    st->key = ActorKey{rec->U32(0x18), rec->U32(0x1c)};
    std::memcpy(reinterpret_cast<uint8_t*>(&st->first) + 0x14,
                reinterpret_cast<const uint8_t*>(rec) + 0x14, 0x68);
    std::memcpy(reinterpret_cast<uint8_t*>(&st->second) + 0x14,
                reinterpret_cast<const uint8_t*>(rec) + 0x14, 0x68);
  }
  return true;
}

}  // namespace pes
