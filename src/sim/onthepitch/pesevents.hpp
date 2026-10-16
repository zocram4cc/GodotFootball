#pragma once

// PES 2021's on-pitch event store and the detector loop that feeds it. The
// store is the match context's event block `E = *(D+0xbd08)` (D = the detector
// context, the loop's param_1); the 223 detector objects sit at `D+0xb600`.
//
//   ring writer   FUN_140a3dac0  (0x140a3dac0)  8 slots, count cap 8, head mod 8
//   ring writer   FUN_140a3db80  (0x140a3db80)  ring 10: 0x40 slots, head mod 0x40
//   kind block    FUN_140a3da00  (0x140a3da00)  one slot, count sticky at 1
//   record writer FUN_140a3d060  (0x140a3d060)  rec[0]=K, rec[1..2]=frames,
//                                               byte rec[3]=state, rec[4]=p6
//   record reset  FUN_140a3cdd0  (0x140a3cdd0)  800 B zero, rec[0]=0x37, rec[4]=0xb
//   router        FUN_140a3dc40  (0x140a3dc40)  ring = table[K]; K=7 also copies
//                                               the payload to E+0x180698
//   write log     FUN_140a3e140  (0x140a3e140)  E+0x26ff0, 64 x {u32 ring, u16 slot}
//   readers       FUN_140a3e190 / 3e150 / 3e7d0 / 3e880 / 3e940 / 3e5d0
//   state word    FUN_140a3c070 / FUN_140a3c050 / FUN_140a6bd60 / FUN_140a6be70
//
// Layout (OBS, from the decompiles): ten rings at `E + r*0x1908` (r < 10), each
// eight 800-byte slots with the count at +0x1900 and the head at +0x1904; ring
// 10 is the 0x40-slot overflow block at E+0xfa50 (count +0xc800, head +0xc804);
// the newest-event copy is E+0x180698; the 56 per-kind blocks are at
// `E + 0x1c258 + K*0x328` (record, count at +0x320, head at +0x324); the kind-27
// accumulator sub-block is E+0x180c60.
//
// `ring`, `slot`, `count`, `head` are the store's own words; every offset above
// is a byte offset inside E.
//
// The store is not wired into the match tick yet: GF's `Match::BallEvent` still
// carries the four kinds it always carried (A2 scope, tasks/15-10-26.md §A2).
// No PES number table is transcribed here: the ring routing table is read from
// the user's own install through pes::PeImage::ReadVa.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

// The 800-byte event record. The ring slot *is* the record: FUN_140a3dac0
// copies 800 bytes from the detector's record to `ringBase + head*800`, and the
// readers (FUN_140a3e190/3e7d0/3e3b0/3e5d0) return `slot + 0x14`, so the word at
// +0x14 is what every consumer of a routed record sees. FUN_140a3d060's five
// header words are +0x00..+0x13; +0x14 onward is the detector's payload.
struct EventRecord {
  static constexpr size_t kSize = 800;
  // 0x37 = the empty sentinel (FUN_140a3e190's `*piVar2 == 0x37`).
  static constexpr uint32_t kEmptyKind = 0x37;
  // FUN_140a3cdd0's rec[4] = 0xb.
  static constexpr int32_t kResetP6 = 0xb;
  // FUN_140a60520/FUN_140a867a0: the {2, 0xff} "no actor" key, and the
  // {team, slot} shape FUN_140a60530 accepts (team < 2, slot < 0x28).
  static constexpr uint32_t kNoActorTeam = 2;
  static constexpr uint32_t kNoActorSlot = 0xff;
  static constexpr uint32_t kSlotLimit = 0x28;

  uint32_t kind = kEmptyKind;  // +0x00 the routing kind
  int32_t frame1 = 0;          // +0x04
  int32_t frame2 = 0;          // +0x08
  uint8_t stateByte = 0;       // +0x0c
  int32_t p6 = 0;              // +0x10
  std::array<uint8_t, kSize - 0x14> payload{};  // +0x14

  // FUN_140a3cdd0: 800 B zero, then rec[4] = 0xb, rec[0] = 0x37.
  void Reset();
  // FUN_140a3d060. `state` is the float word FUN_140a3c050 produces; the byte
  // folded at +0x0c is FUN_140a6bd60/FUN_140a6be70 applied to it.
  void Write(uint32_t k, int32_t f1, int32_t f2, float state, int32_t p6);

  // Word/byte views at a record offset (0 = the kind word, 0x14 = payload+0).
  uint32_t U32(size_t off) const;
  int32_t I32(size_t off) const;
  float F32(size_t off) const;
  uint8_t U8(size_t off) const;
  void SetU32(size_t off, uint32_t v);
  void SetI32(size_t off, int32_t v);
  void SetF32(size_t off, float v);
  void SetU8(size_t off, uint8_t v);

  // The action kind at payload+0: the word FUN_140a3e190's consumers test
  // (`*piVar14 - 1U < 3 || *piVar14 == 0x10` in FUN_1404fe9c0 step 2, the busy
  // set at +0 in FUN_140a6ce10, `*piVar12 == 5` in the shot reader
  // FUN_140a3e3b0). The route kind is +0x00.
  uint32_t ActionKind() const { return U32(0x14); }
  void SetActionKind(uint32_t k) { SetU32(0x14, k); }
  // The action's frame, payload+0x10 (FUN_1404fe9c0 step 2 reads ev+0x10).
  int32_t ActionFrame() const { return I32(0x24); }
  void SetActionFrame(int32_t f) { SetI32(0x24, f); }
};

// An actor key: two words FUN_140a60530 accepts when the first is < 2.
struct ActorKey {
  uint32_t team = EventRecord::kNoActorTeam;
  uint32_t slot = EventRecord::kNoActorSlot;
  bool Valid() const;  // FUN_140a60530
  bool operator==(const ActorKey& o) const;
  bool operator!=(const ActorKey& o) const { return !(*this == o); }
};

// FUN_140a60500's {2, 0xff}: the "no actor" key.
ActorKey NoActorKey();

// The state word's inputs. FUN_140a3c050(x) = (float)FUN_140a3c070(x) +
// *(float*)(x+0xc); the detectors call it with x = MI+0x1168, so every offset
// below is relative to the match-info phase block's +0x1168.
struct StateWordInput {
  uint32_t selected = 0;  // x+0x20 + (*(uint32*)(x+0x1c) & 3) * 4
  int32_t counter = 0;    // x+0x18, tripled inside FUN_140a3c070
  uint64_t check = 0;     // x+0x10: the FNV-1a check word
  float offset = 0.f;     // x+0xc
};

// FUN_140a3c070: the state word, verified against `check` by FNV-1a over its
// four bytes. PES aborts when the check fails (the call at 0x140a3c171); the
// port returns the word either way and reports the check separately.
uint32_t StateWord(const StateWordInput& in);
bool StateWordCheckHolds(const StateWordInput& in);
// FUN_140a3c050.
float StateWordFloat(const StateWordInput& in);

// FUN_140a6bd60/FUN_140a6be70: the byte FUN_140a3d060 folds at rec+0x0c.
uint8_t StateByteOf(float state);

// The ring-routing table at 0x142642e60: 56 ints, one per kind. OBS values:
// 7->0, 8->1, 11->2, 14->7, 23->3, 30->4, 37->5, 4->6, 43->9, 55->0, the rest
// 10 (docs/re/pes-re-possession-latch.md §24).
struct EventRingTable {
  std::array<int, 56> ring{};
  int RingOf(uint32_t kind) const;
};
std::optional<EventRingTable> LoadEventRingTable(const PeImage& exe, std::string* error);

// FUN_140a3dac0's ring (rings 0..9 at E + r*0x1908) and FUN_140a3db80's
// overflow ring (ring 10 at E+0xfa50): the same shape at two slot counts.
template <size_t N>
struct EventRing {
  static constexpr size_t kSlots = N;
  std::array<EventRecord, N> slots{};
  uint32_t count = 0;  // +kSlots*800 in the store's own words
  uint32_t head = 0;
  // FUN_140a3dac0/FUN_140a3db80: count+1 (saturated at the slot count), head+1
  // (wrapping at the slot count), then the 800-byte copy. Returns the slot.
  int Push(const EventRecord& rec) {
    ++count;
    if (count > kSlots) count = static_cast<uint32_t>(kSlots);
    ++head;
    if (head >= kSlots) head = 0;
    slots[head] = rec;
    return static_cast<int>(head);
  }
  const EventRecord& Slot(int i) const { return slots[static_cast<size_t>(i)]; }
};
using EventRing8 = EventRing<8>;
using EventRing10 = EventRing<0x40>;

// FUN_140a3dc40's per-record write log at E+0x26ff0: 64 entries of
// {u32 ring, u16 slot}, the count at E+0x271f0 saturated at 64, the head at
// E+0x271f4 wrapping at 64. FUN_140a3e140 clears it at each detector pass.
struct EventLogEntry {
  uint32_t ring = 0;
  uint16_t slot = 0;
};
struct EventLog {
  static constexpr size_t kEntries = 64;
  std::array<EventLogEntry, kEntries> entries{};
  uint32_t count = 0;
  uint32_t head = 0;
  void Push(uint32_t ring, uint16_t slot);
  void Clear();  // FUN_140a3e140
};

// FUN_140a3da00's per-kind block at E + 0x1c258 + K*0x328: the last record of
// that kind, with the count at +0x320 sticky at 1 and the head at +0x324 held
// at 0 (a second push overwrites the record in place).
struct EventKindBlock {
  EventRecord record{};
  uint32_t count = 0;
  uint32_t head = 0;
  int Push(const EventRecord& rec);
};

// The store `E`. The rings, the overflow ring, the newest-event copy, the
// per-kind blocks and the write log, in E's own offsets.
class EventStore {
 public:
  static constexpr size_t kRingCount = 10;
  static constexpr size_t kKindBlockCount = 56;
  // FUN_140a3dc40's kind-7 newest copy: 0x60 bytes of the payload at
  // E+0x180698, plus a second copy at E+0x1806fc written only when the key at
  // rec+0x18 is valid (FUN_140a60530), which FUN_140a3e880 (E+0x180700) reads.
  static constexpr size_t kNewestBytes = 0x60;

  void Reset();

  // FUN_140a3dc40: route a written record by its kind. The kind-0x1f special
  // path (the per-index archive at E+0x27400 and the E+0x74990 log) is not
  // ported: no detector this port produces carries that kind.
  void Route(const EventRingTable& table, const EventRecord& rec);

  // The rings, indexed by ring number (0..9).
  EventRing8& Ring(int r) { return rings_[static_cast<size_t>(r)]; }
  const EventRing8& Ring(int r) const { return rings_[static_cast<size_t>(r)]; }
  EventRing10& OverflowRing() { return overflow_; }
  const EventRing10& OverflowRing() const { return overflow_; }

  EventLog& Log() { return log_; }
  const EventLog& Log() const { return log_; }
  EventKindBlock& KindBlock(int k) { return kindBlocks_[static_cast<size_t>(k)]; }
  const EventKindBlock& KindBlock(int k) const {
    return kindBlocks_[static_cast<size_t>(k)];
  }

  // E+0x180698: the newest kind-7 record's payload.
  const std::array<uint8_t, kNewestBytes>& Newest() const { return newest_; }
  // E+0x1806fc: the second copy, kept only while the key at rec+0x18 is valid.
  const std::array<uint8_t, kNewestBytes>& NewestSecond() const { return newestSecond_; }

 private:
  std::array<EventRing8, kRingCount> rings_{};
  EventRing10 overflow_{};
  EventLog log_{};
  std::array<EventKindBlock, kKindBlockCount> kindBlocks_{};
  std::array<uint8_t, kNewestBytes> newest_{};
  std::array<uint8_t, kNewestBytes> newestSecond_{};
};

}  // namespace pes
