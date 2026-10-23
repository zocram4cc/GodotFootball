#include "pesrunslots.hpp"

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace pes::run {
namespace {

constexpr int kTargetOffset = 0x20;  // the 32-byte target block sits at block + 0x20 (T+0x28C + 0x40k)
constexpr int kTargetBytes = 0x20;
constexpr int kReleaseClearedBytes = 0x1d;  // release clears block bytes 0..0x1c; 0x1d..0x1f are not written

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

uint32_t Age(const Ring& ring) {
  const RingEntry& head = Behind(ring, 0);
  return head.seenAt - head.enteredAt;
}

void ZeroTargetBlock(uint8_t* p, int v) {
  std::fill(p, p + kTargetBytes, uint8_t{0});
  const auto put = [p](int at, std::initializer_list<uint8_t> bytes) {
    for (uint8_t b : bytes) p[at++] = b;
  };
  if (v == 1) {
    put(16, {0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00});  // the qword 0x10100 at +16
    return;
  }
  if (v <= 0x1c) return;
  if (v < 0x20) {
    put(16, {0x00, 0x01, 0x01, 0x00});                        // the dword 0x10100 at +16
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

void Release(Slot* slot, uint32_t stamp) {
  slot->state = 0;
  Push(&slot->ring, 0, stamp);
  std::fill(slot->block.begin(), slot->block.begin() + kReleaseClearedBytes, uint8_t{0});
  ZeroTargetBlock(slot->block.data() + kTargetOffset, 0);
}

std::array<float, 3> Point(const Slot& slot) {
  std::array<float, 3> point{};
  std::memcpy(point.data(), slot.block.data(), sizeof(point));
  return point;
}

void SetPoint(Slot* slot, const std::array<float, 3>& point) {
  std::memcpy(slot->block.data(), point.data(), sizeof(point));
}

}  // namespace pes::run
