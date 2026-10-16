// PES 2021's on-pitch event store (match ctx +0xe0) and the loop detectors that
// feed it. Ported from the decompiles at the cited VAs; the stand-ins for inputs
// GF has no counterpart for carry a `ponytail:` note naming the PES address they
// replace.
//
//   writer   FUN_140a3d060  (0x140a3d060)  rec[0]=K, rec[1..2]=frames, state byte, rec[4]=p6
//   init     FUN_140a3cdd0  (0x140a3cdd0)  zeros 800 B, rec[0]=0x37, rec[4]=0xb
//   router   FUN_140a3dc40  (0x140a3dc40)  ring = DAT_142642e60[K]; K==7 copies the newest
//   ringpush FUN_140a3dac0  (0x140a3dac0)  8-slot ring, count cap 8, head wraps 0..7
//   ovfpush  FUN_140a3db80  (0x140a3db80)  ring-10 block, count cap 0x40, head wraps 0..0x3f
//   kindblck FUN_140a3da00  (0x140a3da00)  per-kind last-record block, presence flag
//   reader   FUN_140a3e190  (0x140a3e190)  n==0 -> newest copy; n>=1 -> ring 0
//   reader   FUN_140a3e7d0  (0x140a3e7d0)  ring 1, returns the 8-byte key at rec+0x1c
//   reader   FUN_140a3e880  (0x140a3e880)  = newest + 0x68 (the second key)
//   reader   FUN_140a3e150  (0x140a3e150)  ring k in 0..9, record pointer
//   reader   FUN_140a3e5d0  (0x140a3e5d0)  the n-th newest record of a kind via the log
//   reader   FUN_140a3e940  (0x140a3e940)  the per-kind last record
//   key test FUN_140a60530  (0x140a60530)  valid when key0 < 2 && key1 < 0x28
//   key cmp  FUN_140a60300  (0x140a60300)  two 4-byte keys equal
//
// Kinds (docs/re/pes-re-possession-latch.md section 11): 1-3 kicks, 5 shot, 7 touch,
// 9-14 the busy set, 0x10 restart, 0x1e (30) foul. The empty sentinel is 0x37.
#ifndef _HPP_ONTHEPITCH_PESEVENTS
#define _HPP_ONTHEPITCH_PESEVENTS

#include <array>
#include <cstdint>

namespace pes {

// The 800-byte event record. The first five words are the writer's header
// (FUN_140a3d060); the rest is a kind-specific payload (FUN_140a3cdd0 zeroes it).
struct alignas(8) EventRecord {
  static constexpr size_t kSize = 800;
  uint32_t kind;             // +0x00  0x37 = empty
  int32_t frame1;            // +0x04
  int32_t frame2;            // +0x08
  uint8_t stateByte;         // +0x0c
  int32_t p6;                // +0x10
  std::array<uint8_t, kSize - 0x14> payload;  // +0x14
  // FUN_140a3cdd0: zero, kind = 0x37, p6 = 0xb.
  void Reset();
  // Payload words (payload + off).
  uint32_t U32(size_t off) const;
  int32_t I32(size_t off) const;
  uint32_t Key0() const;  // payload +0x04: the team word (< 2)
  uint32_t Key1() const;  // payload +0x08: the slot word (< 0x28)
};

// An actor key (FUN_140a60530 / FUN_140a60300): team + slot.
struct ActorKey {
  uint32_t team = 0;
  uint32_t slot = 0;
};

// The event store: ten 8-slot rings, the ring-10 overflow block, the per-kind
// last-record blocks, the newest-event copy, and the 64-entry kind log.
class EventStore {
 public:
  // FUN_140a3dc40: route a written record by its kind through the table.
  void Route(const EventRecord& rec);
  // FUN_140a3dac0 (rings 0..9) and FUN_140a3db80 (ring 10).
  void PushRing(int ring, const EventRecord& rec);
  // FUN_140a3da00: the per-kind last-record block (kind 0..0x36).
  void PushKindBlock(int kind, const EventRecord& rec);
  // FUN_140a3e190: n==0 -> the newest copy, n>=1 -> ring 0's n-th newest record.
  // Returns false (and leaves *out empty) for an out-of-range or empty slot.
  bool NewestOrRing0(int n, EventRecord* out) const;
  // FUN_140a3e150: ring k in 0..9, the n-th newest record.
  bool Ring(int k, int n, EventRecord* out) const;
  // FUN_140a3e7d0: ring 1's n-th newest record's 8-byte key (rec+0x1c).
  ActorKey Ring1Key(int n) const;
  // FUN_140a3e940: the per-kind last record.
  bool KindLast(int kind, EventRecord* out) const;
  // FUN_140a3e5d0: the n-th newest record of a given kind, via the log.
  bool KindNth(int kind, int n, EventRecord* out) const;
  // The newest-event copy (FUN_140a3e190(E,0) = store +0x180698), and its
  // second key (FUN_140a3e880 = +0x68).
  const EventRecord& Newest() const;
  const EventRecord& NewestSecond() const;
  // The ring table, read from the install by the loader (DAT_142642e60).
  std::array<int, 56> ringTable = {10};

 private:
  std::array<EventRecord, 8> ringSlots_[10] = {};
  std::uint32_t ringCount_[10] = {};
  std::uint32_t ringHead_[10] = {};
  std::array<EventRecord, 64> ovfSlots_ = {};
  std::uint32_t ovfCount_ = 0;
  std::uint32_t ovfHead_ = 0;
  std::array<EventRecord, 56> kindRecs_ = {};
  std::array<std::uint32_t, 56> kindPresent_ = {};
  std::array<std::uint32_t, 56> kindCounter_ = {};
  EventRecord newest_ = {};
  EventRecord newestSecond_ = {};
  struct LogEntry { std::int32_t ring = -1; std::uint16_t slot = 0; };
  std::array<LogEntry, 64> log_ = {};
  std::uint32_t logCount_ = 0;
  std::uint32_t logHead_ = 0;
};

// The kind-27 (0x1b) accumulator, FUN_140a115a0 case 0x1b: on a record with a
// valid key, count++ and sum the duration word.
struct Kind27Accumulator {
  std::uint32_t count = 0;  // E+0x181e00
  float durationSum = 0.f;  // E+0x181dfc
};

// The writer (FUN_140a3d060): rec[0]=K, rec[1..2]=frames, state byte, rec[4]=p6.
void WriteRecord(EventRecord* rec, int kind, int frame1, int frame2, int stateByte, int p6);

// --- Detectors: pure functions over their cited inputs. Each fires a record
// --- into `out` and returns true when it did.
struct PhaseInput {   // MI = *(G+0x270)
  int frame = 0;      // MI+0x1198
  int phase = 0;      // MI+0x1168
  int stateByte = 0;  // FUN_140a3c050(MI+0x1168)
};
// K=1 (FUN_140a14280, site 140a142e1): a phase change.
bool DetectPhase(const PhaseInput& in, int* storedPhase, EventRecord* out);

struct DeflectionInput {  // K=9 (FUN_140a178d0, site 140a1798c)
  int frame = 0;  // MI+0x1198
  int phase = 0;  // MI+0x1168
  int stateByte = 0;
  bool ballGate = false;  // MI+0x11b4 == 0
  float sample0 = 0.f;    // *(FUN_140a6cf70(G+0x40,0)+4)
  float sample1 = 0.f;    // *(FUN_140a6cf70(G+0x40,1)+4)
  float sampleConst = 0.f;  // DAT_142598958
};
bool DetectDeflection(const DeflectionInput& in, EventRecord* out);

struct OutOfPlayInput {  // K=10 (FUN_140a17810, site 140a1788b)
  int frame = 0;
  int phase = 0;
  int stateByte = 0;
  bool gate = false;  // MI+0x11b4 == 0
  int outSide = 0;    // G+0x40+0x2798
};
bool DetectOutOfPlay(const OutOfPlayInput& in, int* stored, EventRecord* out);

struct PossessionFlipInput {  // K=12 (FUN_140a14d40, site 140a14db6)
  int frame = 0;
  int phase = 0;
  int stateByte = 0;
  bool team0InPossession = false;  // team record 0 +0x255 != 0
};
bool DetectPossessionFlip(const PossessionFlipInput& in, bool* stored, EventRecord* out);

struct IntervalInput {  // K=14 (FUN_140a1a4c0, site 140a1a53b)
  int frame = 0;
  int phase = 0;
  int stateByte = 0;
  int team = 0;      // detector +8
  int idx = 0;       // detector +0xc
  bool latch = false;  // team record +0xb6e8[idx]
  bool latchFlag = false;  // team record +0xb718[idx], when idx < 0x30
  int idxLimit = 0x30;
};
bool DetectInterval(const IntervalInput& in, int* storedFrame, int* storedPhase,
                    bool* storedLatched, EventRecord* out);

struct FoulInput {  // K=30 (FUN_140a13bc0, sites 140a13c39..140a13cd3)
  int frame = 0;
  int phase = 0;
  int stateByte = 0;
  int restartState = 1;  // MI+0x116c
  int restartGate = 0;   // MI+0x1574
  bool foulFlag = false; // MI+0x11bb
  bool flag135c = false; // MI+0x135c
  // The foul record copy (FUN_140a3d0c0 from MI+0x1584), payload-sized.
  const EventRecord* foulRecord = nullptr;
};
bool DetectFoul(const FoulInput& in, int* storedPhase, EventRecord* out);

// --- Readers for the consumers that switch to this later.
// The newest event's kind / team / frame (docs/re/pes-re-possession-latch.md
// section 2, step 8; the newest copy is E+0x180698).
int NewestKind(const EventStore& s);
int NewestTeam(const EventStore& s);  // payload +0x04
int NewestFrame(const EventStore& s);  // payload +0x10
bool NewestActorValid(const EventStore& s);  // FUN_140a62ea0: team word < 2

// The human-dodge gate's window (section 2, step 8): refuse when the newest
// event is this team's and it is younger than gs * 0.2 frames.
bool DodgeWindowRefuses(const EventStore& s, int side, int nowFrame, int gs);

// The busy test (section 6, FUN_140a6ce10): busy for kinds 9-14, and for
// 6/7 with no carrier.
bool BusyFor(const EventRecord& rec, bool carrierIsPlayer);

// The kind set the possession latch's step 2 takes an actor from
// (pespossession.cpp kStepTwoDivisor; the step-2 kinds).
bool IsLatchStepTwoKind(int kind);

}  // namespace pes

#endif
