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
// The match owns the store and writes it each tick from its event stage (matchevents.cpp);
// the possession latch, the receive gates and the dodge window read it.
// No PES number table is transcribed here: the ring routing table is read from
// the user's own install through pes::PeImage::ReadVa.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

// PES's action kinds, the ring-0 payload+0 word the readers test (OBS: pesreceive.hpp's
// kind comment, FUN_1404fe9c0 step 2): 1 a short kick, 2 a high kick, 5 a shot. 7 is the
// kind GF gives every other touch (INF: the route kind of a ball-event record is 7 too).
constexpr uint32_t kActionKick = 1;
constexpr uint32_t kActionHighKick = 2;
constexpr uint32_t kActionShot = 5;
constexpr uint32_t kActionTouch = 7;

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
  uint16_t U16(size_t off) const;
  float F32(size_t off) const;
  uint8_t U8(size_t off) const;
  void SetU32(size_t off, uint32_t v);
  void SetI32(size_t off, int32_t v);
  void SetU16(size_t off, uint16_t v);
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

  // E+0x180698: the newest kind-7 record's payload. PES keeps 0x60 bytes of
  // the payload there; the record's own words are the payload copy's.
  const EventRecord& Newest() const { return newest_; }
  // E+0x1806fc: the second copy, written only while the key at rec+0x18 is
  // valid, which FUN_140a3e880 (E+0x180700) reads as the newest event's other
  // key.
  const EventRecord& NewestSecond() const { return newestSecond_; }

  // FUN_140a3e190: n == 0 is the newest event, n >= 1 walks ring 0 from its
  // head. Null is PES's empty sentinel (an out-of-range n, or a slot whose
  // kind word is the 0x37 sentinel).
  const EventRecord* NewestOrRing0(int n) const;
  // FUN_140a3e150: ring k in 0..9, the n-th newest record; null when out of
  // range (this reader has no empty-slot test: it hands back the slot).
  const EventRecord* RingRecord(int ring, int n) const;
  // FUN_140a3e940: a kind's last record, from its per-kind block.
  const EventRecord* KindLast(int kind) const;
  // FUN_140a3e5d0: the n-th newest record of a kind, walking the write log
  // backwards from its head. Null when there are fewer than n+1.
  const EventRecord* KindNth(int kind, int n) const;
  // FUN_140a3e3b0: the newest kind-5 (shot) record, searched over the newest
  // event and then ring 0. `maxFrame` is the reader's third argument (0 = any
  // frame); a record qualifies when its payload+0x10 is at or below it.
  const EventRecord* LastShot(int maxFrame) const;

  // FUN_140a3e7d0: ring 1's n-th newest record's key at rec+0x1c (payload+8).
  ActorKey Ring1Key(int n) const;
  // FUN_140a3e880: E+0x180700, the second copy's key.
  ActorKey NewestSecondKey() const;

 private:
  std::array<EventRing8, kRingCount> rings_{};
  EventRing10 overflow_{};
  EventLog log_{};
  std::array<EventKindBlock, kKindBlockCount> kindBlocks_{};
  EventRecord newest_{};
  EventRecord newestSecond_{};
  bool hasNewest_ = false;       // the newest copy has been written at least once
  bool hasNewestSecond_ = false;  // ... and so has the valid-key second copy
};

// FUN_140a62ea0 with FUN_140a3b320: the player id a {team, slot} key names,
// walking the team's 0x28-entry roster (index -> slot) and taking
// `team * 11 + index` while the index is a starter (< 11). 0xff names none.
// `side` is the team record's own first word (FUN_140a3b320's multiplier).
int ActorIdOf(const ActorKey& key, const std::array<uint32_t, 0x28>& roster, int side);

// The newest event as its later consumers read it: FUN_1404fe9c0 step 2
// (`*ev` the kind, `ev+0x10` the frame, `ev+4` the actor key), FUN_140a0b350
// step 8 (the team at `ev+4`), and step 3's `+0x40` sub-kind, `+0x2d` flag and
// `+0x1c` word (docs/re/pes-re-possession-latch.md §2, §15). `present` is false
// when the store has no newest event (FUN_140a3e190's empty sentinel).
struct NewestEventView {
  bool present = false;
  uint32_t kind = 0;    // payload+0
  ActorKey actor{};     // payload+4
  int32_t frame = 0;    // payload+0x10
  int32_t field1c = 0;  // payload+0x1c
  uint8_t flag2d = 0;   // payload+0x2d
  int32_t subKind = 0;  // payload+0x40
};
// The n-th newest ring-0 event (n = 0 the newest, n = 1 the touch before it).
NewestEventView EventAt(const EventStore& store, int n);
NewestEventView NewestEvent(const EventStore& store);

// FUN_1404fe9c0 step 2's kind set: the newest event takes its actor when the
// kind is 1..3 or 0x10 (`*piVar14 - 1U < 3 || *piVar14 == 0x10`).
bool IsLatchStepTwoKind(uint32_t kind);
// FUN_1404fe9c0 step 2's window: within gs / 6 frames of the newest event.
int LatchStepTwoWindowFrames(int gs);

// FUN_140a0b350 step 8: refuse when the newest event's team is `side` and it is
// younger than `gs * windowFactor` frames. The factor is DAT_142598578 = 0.2,
// loaded as pes::DodgeConstants::lastEventWindowFactor (pesdodge.hpp).
bool DodgeWindowRefuses(const EventStore& store, int side, int frame, int gs,
                        float windowFactor);

// FUN_140a6ce10: the busy test. `carrierIsPlayer` is FUN_140a627e0's
// MA+0x2601c < 22, and `ctxRole` the role FUN_140a4d660 returns for the third
// argument (0 when there is none).
bool BusyFor(const EventRecord& rec, bool carrierIsPlayer, int ctxRole);

// --- The constants the detectors read out of the install. ---
// FUN_140a178d0's deflection sample: DAT_142598958, the value the second
// prediction sample must equal.
// FUN_140a6cd00's short write: DAT_14259be10 (the 16.0 scale), DAT_14257234c
// (32767) and DAT_142648170 (-32768).
struct EventConstants {
  float deflectionSample = 0.f;
  float shortScale = 0.f;
  float shortUpper = 0.f;
  float shortLower = 0.f;
};
std::optional<EventConstants> LoadEventConstants(const PeImage& exe, std::string* error);

// FUN_140a6cd00: a sample's x and z as two clamped 16-bit words.
void WriteClampedShorts(EventRecord* rec, size_t off, const float* sample,
                        const EventConstants& k);

// --- The detectors. Each writes the record into `out` and returns whether it
// --- fired; the caller routes what it writes (the detector loop FUN_140a0d3f0
// --- calls FUN_140a3dc40 on the record only when a detector returns true).
// --- Every one is a pure function of its inputs; the stored state a detector
// --- keeps between frames (its "+8" word) is passed by pointer.

// K=1 (FUN_140a14280, site 0x140a142e1): the phase word changing.
struct PhaseInput {
  int phase = 0;    // MI+0x1168
  int frame = 0;    // MI+0x1198
  float state = 0.f;  // FUN_140a3c050(MI+0x1168)
  int stored = 0;   // the detector's +8 word
};
bool DetectPhase(PhaseInput* in, EventRecord* out);

// K=9 (FUN_140a178d0, site 0x140a1798c): the ball's second prediction sample
// sitting at the deflection constant while the first has fallen.
struct DeflectionInput {
  int frame = 0;
  int phase = 0;
  float state = 0.f;
  bool gateOpen = true;     // MI+0x11b4 == 0 (FUN_140a178d0 reads a char)
  float sample0 = 0.f;      // FUN_140a6cf70(ball, 0)+4
  float sample1 = 0.f;      // FUN_140a6cf70(ball, 1)+4
  float sampleConst = 0.f;  // EventConstants::deflectionSample
};
bool DetectDeflection(const DeflectionInput& in, EventRecord* out);

// K=11 (the snapshot body 0x140a158a9-0x140a15a91, site 0x140a15978): the first
// ring-0 record that is not busy and is the other team's, copied into the new
// record beside the current record and the newest event.
struct SnapshotInput {
  const EventStore* store = nullptr;
  const EventRecord* current = nullptr;  // this pass's record, before the write
  int frame = 0;
  int phase = 0;
  float state = 0.f;
};
bool DetectSnapshot(const SnapshotInput& in, EventRecord* out);

// K=12 (FUN_140a14d40, site 0x140a14db6): team 0's possession latch flipping.
struct PossessionFlipInput {
  int frame = 0;
  int phase = 0;
  float state = 0.f;
  bool team0InPossession = false;  // team record 0's +0x255
  float ballX = 0.f;               // FUN_140a57800(G+0x40)+0
  float ballZ = 0.f;               // FUN_140a57800(G+0x40)+8
  bool stored = false;             // the detector's +8 word
};
bool DetectPossessionFlip(PossessionFlipInput* in, EventRecord* out,
                          const EventConstants& k);

// K=14 (FUN_140a1a4c0, site 0x140a1a53b): the per-index interval closing.
struct IntervalInput {
  int team = 0;               // the detector's +8 word
  int idx = 0;                // the detector's +0xc word: a byte index into
                              // the team record's +0xb6e8 array
  bool latch = false;         // team record +0xb6e8[idx]
  bool latchFlag = false;     // team record +0xb718[idx], when idx < 0x30
  int frame = 0;
  int phase = 0;
  float state = 0.f;
  bool latched = false;       // the detector's +0x10 word
  int startFrame = 0;         // the detector's +0x14 word
  int startPhase = 0;         // the detector's +0x18 word
};
bool DetectInterval(IntervalInput* in, EventRecord* out);

// K=0x10 (FUN_140a14a20's tail, site 0x140a14a8b): the restart word at
// MI+0x11b2 turning nonzero.
struct RestartInput {
  int restartByte = 0;  // MI+0x11b2
  int frame = 0;
  int phase = 0;
  float state = 0.f;
  int stored = 0;  // the detector's +8 word
};
bool DetectRestart(RestartInput* in, EventRecord* out);

// K=30 (FUN_140a13bc0, sites 0x140a13c39/0x140a13c86/0x140a13cd3): the foul
// ring's three triggers, each copying the foul record MI+0x1584 into the
// payload (FUN_140a3d0c0).
struct FoulInput {
  int frame = 0;
  int phase = 0;
  float state = 0.f;
  int restartState = 1;   // MI+0x116c
  int restartGate = 0;    // MI+0x1574
  bool foulFlag = false;  // MI+0x11bb
  bool flag135c = false;  // MI+0x135c
  bool restartGate6 = false;  // MI+0x1554 == 6
  int stored = 0;         // the detector's +8 word
  // The foul record MI+0x1584, as FUN_140a3d0c0 reads it.
  int foulKind = 0;        // +0x04
  int foulWord8 = 0;       // +0x08
  int foulCulprit = -1;    // +0x0c, a player id (FUN_140a62fe0's)
  int foulVictim = -1;     // +0x10, a player id
  int foulWord14 = 0;      // +0x14
  int foulWord18 = 0;      // +0x18
  int foulWord1c = 0;      // +0x1c
  int foulWord28 = 0;      // +0x28
  uint8_t foulByte20 = 0;  // +0x20
  uint8_t foulByte21 = 0;  // +0x21
  // FUN_140a81b70(MI+0x17ac, foul+0x28): the culprit's position sample, or null
  // when the lookup finds nothing.
  const float* culpritSample = nullptr;
  bool culpritIsPair = false;    // foul+4 == 2: the second branch
  bool culpritShortFlag = false;  // that branch's entry word is 0xf
  const EventConstants* constants = nullptr;
};
bool DetectFoul(FoulInput* in, EventRecord* out);

// The ball-event family (FUN_140a17b30's dispatch over ball+0x291c:
// FUN_140a18940 in play, FUN_140a18690 for the second state, FUN_140a19120
// otherwise), filled by FUN_140a17cf0 and de-duplicated against the newest
// event. Its payload+0 is the action kind the readers test: 1..5 for a kick or
// a shot (FUN_140a18810's switch on ball+0x27dc), 8 and 0x11 for
// FUN_140a19120's two states, 0xb..0xe for FUN_140a18690's, and 0xf for
// FUN_140a0dfd0's own record.
struct BallEventInput {
  int ballState = 0;     // G+0x40+0x291c
  int actionKind = 0;    // G+0x40+0x27dc (FUN_140a18940's payload+0)
  int lastToucher = -1;     // G+0x40+0x290c
  int lastToucherSlot = 0;  // the slot FUN_140a62fe0 pairs with that id
  int holder = -1;       // G+0x40+0x2910
  int holderFlag = 0;    // G+0x40+0x2914, a byte
  int holderByte5 = 0;   // G+0x40+0x2915, a byte
  int holderClassKind = 0xd;  // FUN_140a18690's class-resolved kind (0xb/0xc/10)
  int actorByte3e = 0;   // the actor record's +0x3e, sign-extended
  int ballWord2818 = 0;  // G+0x40+0x2818
  int ballWord281c = 0;  // G+0x40+0x281c
  int kickTailWord40 = 0;  // payload+0x40, the kind 1/2 tail
  int kickTailSub = 0;     // payload+0x18, the kind 1/2 tail
  std::array<uint32_t, 0x18> roster{};  // [G+0x180] + i*0x74 + 0x20, i < 0x18
  bool kickGate = false;       // FUN_140a57950(ball): ball+0x291c == 1
  bool secondGate = false;     // FUN_140a57930(ball): ball+0x2918 >> 2 & 1
  bool state2Gate = false;     // FUN_140a57940: the ball+0x2920 == 2 branch
  int restartPlayer = -1;      // MI+0x173c (FUN_140a19120's state 2)
  bool holderPairValid = false;  // *(D+0x108)'s second byte
  int holderPairPlayer = -1;      // *(D+0x108)'s first word
  int frame = 0;
  int phase = 0;
  float state = 0.f;           // FUN_140a3c050(MI+0x1168)
  int restartState = 1;        // MI+0x116c
  int restartKind = 0;         // MI+0x1554
  const EventStore* store = nullptr;  // the dedup reads the newest event
  const EventConstants* constants = nullptr;
};
bool DetectBallEvent(const BallEventInput& in, EventRecord* out);

// FUN_140a0dfd0: the type-0x14 record's events. Its first record is a kind-8
// record gated on ring 1's newest key being valid; the second is the kind-7
// record with the 0xf payload, written only while the ball's gate at
// G+0x2a80+0x64 is 1.
struct Type14Input {
  int type = 0;              // the input record's +0 word
  bool fromKick = false;     // its +0x18 byte
  bool fromOther = false;    // its +0x19 byte
  int gate = 0;              // *(G+0x2a80 + 0x64)
  int player = -1;           // *(G+0x2a80+0x18) for the actor
  int frame = 0;
  int phase = 0;
  float state = 0.f;
  float ballX = 0.f;         // FUN_140a57800(G+0x40)+0
  float ballZ = 0.f;         // FUN_140a57800(G+0x40)+8
  const EventStore* store = nullptr;
  const EventConstants* constants = nullptr;
};
// Both records are written through this; `out` is the record the caller routes.
// Returns which record was written, or kNone when neither fires.
enum class Type14Record { kNone, kOther, kTouch };
Type14Record DetectType14(const Type14Input& in, EventRecord* out);

// --- The kind-27 accumulator (FUN_140a115a0's case 0x1b) and the walker that
// --- feeds it (FUN_140a13060). ---
// FUN_140a13060 walks the write log's newest-first entries and hands each
// record to FUN_140a115a0 with `S = E+0x180c60`; the 0x1b arm adds one to the
// count at S+0x11a0 (E+0x181e00) and the record's +0x28 float to the duration
// sum at S+0x119c (E+0x181dfc).
struct Kind27Accumulator {
  uint32_t count = 0;      // S+0x11a0
  float durationSum = 0.f;  // S+0x119c
};
// FUN_140a115a0's 0x1b arm admission: the record's +0x30 word names a player
// under 0x18 whose entry word matches FUN_140a44e200's index, and the key at
// +0x20 is a valid one.
struct Kind27Input {
  uint32_t player = 0;       // rec+0x30
  uint32_t entryWord = 0;    // [G+0x180] + player*0x74 + 0x10
  uint32_t currentIndex = 0;  // FUN_140a44e200's lowest set bit in bits 0..7
  uint32_t keyTeam = 0;       // rec+0x20
  uint32_t keySlot = 0;       // rec+0x24
  float duration = 0.f;       // rec+0x28
};
bool Kind27Admits(const Kind27Input& in);
// The arm itself: adds one and the duration when the kind is 0x1b and the
// admission holds.
void Kind27Accumulate(Kind27Accumulator* acc, uint32_t routeKind, const Kind27Input& in);

// FUN_140a61d40/FUN_140a61d90: the 14x10 zone grid's two classifiers.
int ZoneXOf(float halfX, float x);  // 14 columns
int ZoneZOf(float halfZ, float z);  // 10 rows

// K=13 (FUN_140a15f50, sites 0x140a1615f and 0x140a16377): the possession
// sequence. The detector keeps the tracked touch record (two payload copies),
// a 14x10 grid of zone visits and its position; it records a kind-13 record
// when the tracked player changes, and another at a stoppage (MI+0x116c == 2).
struct PossessionSequenceState {
  bool hasKey = false;        // FUN_140a60530(det+0xc)
  ActorKey key{};             // +0xc
  EventRecord first{};        // +0x08, the tracked record's payload
  EventRecord second{};       // +0x6c, the second copy
  std::array<uint32_t, 140> grid{};  // +0xd8, [zoneZ + zoneX*10]
  int zoneX = -1;             // +0x30c
  int zoneZ = -1;             // +0x310
  int gridCount = -1;         // +0x308
  bool stoppageFlag = false;  // +0xd7: the team record's +0xb6e0
  float state = 0.f;          // +0x314: the state word of the emitted record
  bool flagD4 = false;        // +0xd4
  void Reset();
};
struct PossessionSequenceInput {
  int frame = 0;
  int phase = 0;
  float state = 0.f;      // FUN_140a3c050(MI+0x1168)
  int restartState = 1;   // MI+0x116c
  int ballDirX = 0;       // FUN_140a48a30(G, key team)+0x254
  float ballSample0 = 0.f;  // *(FUN_140a57800(G+0x40)+0)
  float zoneHalfX = 0.f;  // *(G+0x2aa8)+0
  float zoneHalfZ = 0.f;  // *(G+0x2aa8)+4
  float ballZ = 0.f;      // the ball's z, for the zone classifier
  bool teamFlag6e0 = false;  // FUN_140a48a30(G, key team)+0xb6e0
  const EventStore* store = nullptr;
};
bool DetectPossessionSequence(PossessionSequenceInput* in, PossessionSequenceState* st,
                              EventRecord* out);

}  // namespace pes
