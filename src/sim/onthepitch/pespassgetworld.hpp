// The records PES's ActionPassGet tick (0x14094E0E0, the AI action 5's virtual +0x10) runs on, per man, in the world window:
//   the action object (rcx; vtable 0x1426301E0; 0x2B90 bytes: the action base, the route manager at +0x70 and its counters),
//   the pc record (rdx; 0x50 bytes: the man's id, the orders pointer, the side's context),
//   the command record R (r8; the AI's command record the tick's finalize 0x1405141F0 fills),
//   and the u32 the caller passes by pointer (r9: the action id).
// The object's initial bytes are PES's own constructor 0x14091DAB0 (tools/dev/translate/gen_passgetobject.py ->
// pespassgetobject_stores.inc); the route state it keeps between ticks lives in it. docs/ports/pes-port-PassReceiver3.md.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pes {

inline constexpr std::size_t kPassGetObjectBytes = 0x2B90;
inline constexpr std::size_t kPassGetPcBytes = 0x50;
inline constexpr std::size_t kPassGetRBytes = 0x1A00;
// pc: +4 the man's id (the G tables' index), +0x28 the orders pointer (a byte pointer, null when none), +0x38 the side's context.
inline constexpr std::size_t kPcIdOffset = 0x4, kPcOrdersOffset = 0x28, kPcCtxOffset = 0x38;
inline constexpr std::size_t kPcPOffset = 0x40;  // the man's P record (the tick reads pc +0x4, +0x28..+0x48 only)
// What the tick leaves in the command record R (docs/re/pes-re-decision-read.md, pes-re-press-cmd.md: the ball/pass request A+0x874..0x8C4):
// the target point (PES axes x, height, z), the speed sub-block (class A+0x8D0, use flag A+0xE39, the man's speed in km/h A+0xE3C).
inline constexpr std::size_t kRTargetOffset = 0x894, kRSpeedClassOffset = 0x8D0, kRSpeedUseOffset = 0xE39, kRSpeedKmhOffset = 0xE3C;
// The tick's result words in the object: +0x68 the target id, +0x6C the route result code.
inline constexpr std::size_t kPassGetTargetIdOffset = 0x68, kPassGetResultOffset = 0x6C;

struct PesPassGetResult {
  bool targetWritten = false;  // the tick stored R's target point this call
  float target[3] = {0, 0, 0};  // PES axes: x, height, z
  uint32_t speedClass = 0;
  bool speedUse = false;
  float speedKmh = 0;
  uint32_t routeResult = 0;  // this+0x6C: the route walk's result code
};

struct PesPassGetImage {
  std::array<uint8_t, kPassGetObjectBytes> object{};
  std::array<uint8_t, kPassGetPcBytes> pc{};
  std::array<uint8_t, kPassGetRBytes> r{};
  std::array<uint8_t, 16> arg{};  // the u32 r9 points at (the action id), padded
};

// The constructed object's rows (pespassgetobject_stores.inc): {offset, kind (0 = u64 constant, 1 = pointer into the object), value}.
struct PassGetStore {
  uint16_t offset;
  uint8_t kind;
  uint64_t value;
};
inline constexpr PassGetStore kPassGetObjectStores[] = {
#include "onthepitch/pespassgetobject_stores.inc"
};

}  // namespace pes
