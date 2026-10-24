// PES's per-slot run table of one team: the state word of each own slot (T+0xA2A0[k]), its history ring
// (T+0xA2CC + 0x34k: four (id, entered, seen) entries and the index at +0x30) and its block (T+0x26C + 0x40k:
// the concept point in bytes 0..11, the fields the release clears, and the 32-byte target block at +0x20).
// Decoded: the setter FUN_140A42940 (docs/re/pes-re-press-eval.md), the take FUN_140A429A0 (step 38's
// designated-supporter write, docs/re/pes-re-team-layer.md row 38), the ring push FUN_140A40200, the state age
// FUN_140A401C0 / FUN_140A401F0 (pes-re-run-targets-s1.md), the block variants FUN_140A85AA0 and the release
// FUN_140A40820 (docs/re/pes-re-tick-steps3.md, step 43). Pure: the team AI owns the table.
#pragma once

#include <array>
#include <cstdint>

namespace pes::run {

constexpr int kSlots = 11;          // T+0xA2A0[11]: the own side's slots 0..10
constexpr int kRingEntries = 4;     // the history ring: four entries of 12 bytes, the index at +0x30
constexpr int kBlockBytes = 0x40;   // T+0x26C + 0x40k
constexpr int kTargetBytes = 0x20;  // the 32-byte target block at block + 0x20 (T+0x28C + 0x40k)

// One ring entry: +0 the state id, +4 the stamp the state was entered at, +8 the stamp last seen.
struct RingEntry {
  int id = 0;
  uint32_t enteredAt = 0;
  uint32_t seenAt = 0;
};

struct Ring {
  std::array<RingEntry, kRingEntries> entry{};
  int index = 0;
};

struct Slot {
  int state = 0;  // T+0xA2A0[k]; 0 is no state
  Ring ring;
  std::array<uint8_t, kBlockBytes> block{};
};

// FUN_140A40200(ring, id, stamp): records `id` entered at `stamp`. The head already holding `id` changes
// nothing. Otherwise the index moves on (mod 4) only when the head was entered before `stamp`; the entry
// there takes the id with both stamps.
void Push(Ring* ring, int id, uint32_t stamp);

// FUN_140A401C0(ring, behind): the entry `behind` steps back from the head (0 is the head).
const RingEntry& Behind(const Ring& ring, int behind);

// FUN_140A401F0(ring, stamp): the head's last-seen stamp.
void Touch(Ring* ring, uint32_t stamp);

// The state age in frames: the head's last-seen stamp minus its entered stamp (run-targets-s1 "[+8] - [+4]").
uint32_t Age(const Ring& ring);

// FUN_140A85AA0(p, v) on the 32 bytes at p: zeroes them, then the variant of v (1, 0x1d..0x1f, 0x22, 0x27).
void ZeroTargetBlock(uint8_t* p, int v);

// FUN_140A42940(slot, id, stamp): the state word, the ring push, and the target block's variant for id.
void Assign(Slot* slot, int id, uint32_t stamp);

// FUN_140A429A0(slot, id, stamp, record): the take. The state word, the ring push and the target block's variant
// for id, then the 32-byte record copied over the target block (the copy comes last, so it replaces the variant).
void Take(Slot* slot, int id, uint32_t stamp, const std::array<uint8_t, kTargetBytes>& record);

// The record step 38 (FUN_1404F773E, 0x1404F77C9-0x1404F77ED) passes to the take for the designated supporter: the
// slot's current 32 target bytes with byte 16 set to 1.
std::array<uint8_t, kTargetBytes> SupporterRecord(const Slot& slot);

// FUN_140A40820(slot, stamp): the state word to 0, a push of (0, stamp), block bytes 0..0x1c cleared,
// and the target block zeroed. Bytes 0x1d..0x1f are not written.
void Release(Slot* slot, uint32_t stamp);

// Step 43 (FUN_1404F78FE): with T+0x218 == 0, every own slot is released (FUN_140A40820); otherwise nothing.
void ReleaseSweep(std::array<Slot, kSlots>* slots, int noBallCounter, uint32_t stamp);

// The concept point: three float32 at block bytes 0..11 (x, y, z).
std::array<float, 3> Point(const Slot& slot);
void SetPoint(Slot* slot, const std::array<float, 3>& point);

// FUN_140A42A70: per-slot gated snapshot copy of the history ring, then Touch.
void SnapshotCopy(std::array<Slot, kSlots>* dst, const std::array<Slot, kSlots>& src, uint32_t stamp);

// Step 48's release/claim dispatcher (FUN_14057F600, team-tick step 48 at
// 0x1404F79A8). MI+0x116C == 2 exits at once (0x14057F61B); the ==3 arm
// (FUN_140642670) and the else arm (FUN_14057F590 plus the carrier-record and
// frame-window gates) collapse into `stoppageClaim`, whose record-table
// inputs have no GF counterpart yet; then the 2-slot MI+0x173C scan (same
// tables) and the latch fork (0x14057F6CA): latch set runs the set loop,
// latch clear needs MI+0x116C == 1 for the clear loop.
enum class ReleaseRoute { kExit, kLatchSetLoop, kLatchClearLoop };
struct ReleaseDispatchInput {
  int playState = 0;       // MI+0x116C: 1 in play, 2 stopped, 3 set-piece ready
  bool stoppageClaim = false;  // the arm verdict plus the 2-slot scan
  bool latchSet = false;   // T+0x255 != 0
};
ReleaseRoute ReleaseDispatch(const ReleaseDispatchInput& in);

// The latch-set take loop (FUN_14057FDE0): every eligible slot takes 0x10
// (pes::offball::kMoveOnPass) with the tick's stamp via Assign (FUN_140A42940
// writes T+0xA2A0[s] = 0x10). `eligible` is the per-slot guard-ladder verdict;
// its record-table gates are OPEN, so the call site stands in until they land.
void LatchSetLoop(std::array<Slot, kSlots>* slots, uint32_t stamp,
                  const std::array<bool, kSlots>& eligible);

// The latch-clear take loop (FUN_14057F720): the head gate fVar25 <
// 2000.0/(cell * 3600.0) (0x14057F7EC-0x14057F80B; 2000.0 at 0x1425CB004,
// 3600.0 at 0x142598590, the cell at 0x14351AE54 defaulting to the install tick
// rate 54.0 at 0x14259D624, carried 1:1 as pes::kPesTickHz) returns early and
// takes nothing; below it every eligible slot takes 0x10 as above. `eligible`
// stands in for the class/state gates' record tables as above.
void LatchClearLoop(std::array<Slot, kSlots>* slots, uint32_t stamp, float headValue,
                    const std::array<bool, kSlots>& eligible);

}  // namespace pes::run
