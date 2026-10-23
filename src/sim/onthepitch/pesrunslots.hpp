// PES's per-slot run table of one team: the state word of each own slot (T+0xA2A0[k]), its history ring
// (T+0xA2CC + 0x34k: four (id, entered, seen) entries and the index at +0x30) and its block (T+0x26C + 0x40k:
// the concept point in bytes 0..11, the fields the release clears, and the 32-byte target block at +0x20).
// Decoded: the setter FUN_140A42940 (docs/re/pes-re-press-eval.md), the ring push FUN_140A40200, the state
// age FUN_140A401C0 / FUN_140A401F0 (pes-re-run-targets-s1.md), the block variants FUN_140A85AA0 and the
// release FUN_140A40820 (docs/re/pes-re-tick-steps3.md, step 43). Pure: the team AI owns the table.
#pragma once

#include <array>
#include <cstdint>

namespace pes::run {

constexpr int kSlots = 11;         // T+0xA2A0[11]: the own side's slots 0..10
constexpr int kRingEntries = 4;    // the history ring: four entries of 12 bytes, the index at +0x30
constexpr int kBlockBytes = 0x40;  // T+0x26C + 0x40k

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

// FUN_140A40820(slot, stamp): the state word to 0, a push of (0, stamp), block bytes 0..0x1c cleared,
// and the target block zeroed. Bytes 0x1d..0x1f are not written.
void Release(Slot* slot, uint32_t stamp);

// The concept point: three float32 at block bytes 0..11 (x, y, z).
std::array<float, 3> Point(const Slot& slot);
void SetPoint(Slot* slot, const std::array<float, 3>& point);

}  // namespace pes::run
