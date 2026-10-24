#include "pesrunslots.hpp"

#include "pesball.hpp"
#include "pesoffball.hpp"

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace pes::run {
namespace {

constexpr int kTargetOffset = 0x20;         // the target block sits at block + 0x20 (T+0x28C + 0x40k)
constexpr int kReleaseClearedBytes = 0x1d;  // release clears block bytes 0..0x1c; 0x1d..0x1f are not written
constexpr int kSupporterFlagAt = 0x10;      // the byte step 38 sets in its record (0x1404F77ED)

static_assert(sizeof(float) * 3 == 12, "the concept point is three float32 at block bytes 0..11");

}  // namespace

void Push(Ring* ring, int id, uint32_t stamp) {
  int at = ring->index;
  if (ring->entry[at].id == id) return;
  if (ring->entry[at].enteredAt < stamp) {
    at = at + 1 > kRingEntries - 1 ? 0 : at + 1;
    ring->index = at;
  }
  ring->entry[at] = RingEntry{id, stamp, stamp};
}

const RingEntry& Behind(const Ring& ring, int behind) {
  const int at = ring.index - behind;
  return ring.entry[at >= 0 ? at : at + kRingEntries];
}

void Touch(Ring* ring, uint32_t stamp) { ring->entry[ring->index].seenAt = stamp; }

void SnapshotCopy(std::array<Slot, kSlots>* dst, const std::array<Slot, kSlots>& src, uint32_t stamp) {
  for (int s = 0; s < kSlots; ++s) {
    if ((*dst)[s].state == 0x25) continue;
    const RingEntry& d = Behind((*dst)[s].ring, 1);
    const RingEntry& r = Behind(src[s].ring, 1);
    if (d.id != r.id || d.enteredAt != r.enteredAt) continue;
    (*dst)[s].ring = src[s].ring;
    Touch(&(*dst)[s].ring, stamp);
  }
}

uint32_t Age(const Ring& ring) {
  const RingEntry& head = Behind(ring, 0);
  return head.seenAt - head.enteredAt;
}

void ZeroTargetBlock(uint8_t* p, int v) {
  std::fill(p, p + kTargetOffset, uint8_t{0});
  const auto put = [p](int at, std::initializer_list<uint8_t> bytes) {
    for (uint8_t b : bytes) p[at++] = b;
  };
  if (v == 1) {
    put(16, {0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00});  // the qword 0x10100 at +16
    return;
  }
  if (v <= 0x1c) return;
  if (v < 0x20) {
    put(16, {0x00, 0x01, 0x01, 0x00});                          // the dword 0x10100 at +16
    put(20, {0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00});  // the qword 0x10101 at +20
    return;
  }
  if (v == 0x22) {
    put(4, {0x00, 0x01});  // the word 0x100 at +4
    p[6] = 0x01;           // the byte 1 at +6
    return;
  }
  // 0x27 writes only zeros (the word at +4 and the dword at +0), which the fill already did.
}

void Assign(Slot* slot, int id, uint32_t stamp) {
  slot->state = id;
  Push(&slot->ring, id, stamp);
  ZeroTargetBlock(slot->block.data() + kTargetOffset, id);
}

void Take(Slot* slot, int id, uint32_t stamp, const std::array<uint8_t, kTargetBytes>& record) {
  slot->state = id;
  Push(&slot->ring, id, stamp);
  ZeroTargetBlock(slot->block.data() + kTargetOffset, id);
  std::copy(record.begin(), record.end(), slot->block.begin() + kTargetOffset);
}

std::array<uint8_t, kTargetBytes> SupporterRecord(const Slot& slot) {
  std::array<uint8_t, kTargetBytes> record{};
  std::copy(slot.block.begin() + kTargetOffset, slot.block.begin() + kTargetOffset + kTargetBytes,
            record.begin());
  record[kSupporterFlagAt] = 1;
  return record;
}

void Release(Slot* slot, uint32_t stamp) {
  slot->state = 0;
  Push(&slot->ring, 0, stamp);
  std::fill(slot->block.begin(), slot->block.begin() + kReleaseClearedBytes, uint8_t{0});
  ZeroTargetBlock(slot->block.data() + kTargetOffset, 0);
}

void ReleaseSweep(std::array<Slot, kSlots>* slots, int noBallCounter, uint32_t stamp) {
  if (noBallCounter != 0) return;
  for (Slot& slot : *slots) Release(&slot, stamp);
}

std::array<float, 3> Point(const Slot& slot) {
  std::array<float, 3> point{};
  std::memcpy(point.data(), slot.block.data(), sizeof(point));
  return point;
}

void SetPoint(Slot* slot, const std::array<float, 3>& point) {
  std::memcpy(slot->block.data(), point.data(), sizeof(point));
}

ReleaseRoute ReleaseDispatch(const ReleaseDispatchInput& in) {
  // 0x14057F61B: MI+0x116C == 2 exits at once.
  if (in.playState == 2) return ReleaseRoute::kExit;
  // The ==3 arm (FUN_140642670) and the else arm (FUN_14057F590 plus the
  // carrier-record and frame-window gates) plus the 2-slot MI+0x173C scan:
  // no claim, no dispatch.
  if (!in.stoppageClaim) return ReleaseRoute::kExit;
  // The latch fork (0x14057F6CA): latch set runs the set loop; latch clear
  // needs MI+0x116C == 1 for the clear loop.
  if (in.latchSet) return ReleaseRoute::kLatchSetLoop;
  if (in.playState == 1) return ReleaseRoute::kLatchClearLoop;
  return ReleaseRoute::kExit;
}

void LatchSetLoop(std::array<Slot, kSlots>* slots, uint32_t stamp,
                  const std::array<bool, kSlots>& eligible) {
  for (int s = 0; s < kSlots; ++s) {
    if (!eligible[s]) continue;
    // FUN_140A42940(T, slot, 0x10): the state word, the ring push, and the
    // target block's variant for id (pes::offball::kMoveOnPass is 0x10).
    Assign(&(*slots)[s], pes::offball::kMoveOnPass, stamp);
  }
}

void LatchClearLoop(std::array<Slot, kSlots>* slots, uint32_t stamp, float headValue,
                    const std::array<bool, kSlots>& eligible) {
  // The head gate (0x14057F7EC-0x14057F80B): fVar25 < 2000.0/(cell * 3600.0).
  // The cell at 0x14351AE54 defaults to the install tick rate, carried 1:1 as
  // pes::kPesTickHz (docs/re/pes-re-team-helpers.md sect 3.1).
  constexpr float kHeadGateNumerator = 2000.0f;    // 0x1425CB004
  constexpr float kHeadGateDivisor = 3600.0f;      // 0x142598590
  if (!(headValue < kHeadGateNumerator / (pes::kPesTickHz * kHeadGateDivisor))) return;
  for (int s = 0; s < kSlots; ++s) {
    if (!eligible[s]) continue;
    Assign(&(*slots)[s], pes::offball::kMoveOnPass, stamp);
  }
}

}  // namespace pes::run
