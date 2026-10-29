// PES-layout record headers for the four port chains: KickInputs (the kicker ap), TrapClipPort (the anime and action
// objects), EndClear2 (the Clear unit, the corridor-angle ctx, its E0 ring, G+0x180), SanctionSwitch (A1 and its E0
// ring). Offsets are PES's; every one below was read from live captures by tools/dev/translate/recordmap_pin.py
// (verdicts in docs/re/pes-re-record-layouts-chains.md). Sizes are lower bounds (the readable extent), not allocation
// sizes. Unwired: nothing in GF includes this header yet. The padding member names are offsets, not meaning.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../sim/onthepitch/pesworldrecord.hpp"

namespace pes {

// G's record tables, measured in every live capture (pin: "G anime table 0..20", "G man table", "G slot table").
// Entries 0..20 of the anime table are one slot block each; the man record is at anime + 0x32C for those entries and the
// slot record at anime + 0x10F0 for entries 0..19. Entries from 20 (slot) and 21 (anime) on are irregular: OPEN.
inline constexpr std::size_t kUniformAnimeBlocks = 21;
inline constexpr std::size_t kUniformSlotBlocks = 20;
static_assert(kUniformSlotBlocks <= kUniformAnimeBlocks);
static_assert(kPlayerBlockBytes == 0x3F40, "a slot block is 0x3F40 bytes (live: anime entries stride 0x3F40)");
static_assert(kManRecordOffset == 0x32C, "man record at anime + 0x32C (live: entries 0..20)");
static_assert(kSlotRecordOffset == 0x10F0, "slot record at anime + 0x10F0 (live: entries 0..19)");

// E0 ring (live: count at E0 + 0x1900 is 8 once full, head at E0 + 0x1904 is 0..7; 6/6 angle captures). The ring holds
// eight 800-byte (0x320) slots, the same ring GF's pesevents.hpp describes at +0x1900/+0x1904.
inline constexpr std::size_t kRingSlots = 8;
inline constexpr std::size_t kRingSlotBytes = 0x320;

// The kicker ap: PES P, rcx of 0x14083CC30 (KickInputs). Live: P is NOT a G block start (0/12); the fields below are
// readable within the first 0x32E8 bytes (12/12). Field meanings are from the kick doc and are OPEN where it says so.
struct PesKickerRecord {
  std::array<std::uint8_t, 0xAB8> reserved_0000{};
  std::uint64_t interface_ab8;             // +0xAB8: image-resident interface object, vtable in the image (live 12/12)
  std::array<std::uint8_t, 0x2752 - 0xAC0> reserved_0ac0{};
  std::int16_t kick_table_index;            // +0x2752: s16 151-row table index or -1 (live: 42, 97; in [-1,150] 12/12)
  std::uint8_t kick_flag_2754;             // +0x2754 u8 read by the scatter
  std::uint8_t kick_flag_2755;             // +0x2755 u8 read by the scatter (uVar14 in the kick doc)
  std::uint8_t kick_flag_2756;             // +0x2756 u8 first-time flag (writers OPEN)
  std::array<std::uint8_t, 0x2758 - 0x2757> reserved_2757{};
  float kick_window_2758;                  // +0x2758 f32 (live: finite 12/12)
  std::array<std::uint8_t, 0x2764 - 0x275C> reserved_275c{};
  float kick_window_2764;                  // +0x2764 f32, the window the kick doc tests against 360 (live: 720, 275.95)
  float kick_2768;                         // +0x2768 f32 (live: finite 12/12)
  std::array<std::uint8_t, 0x281C - 0x276C> reserved_276c{};
  float kick_281c;                         // +0x281C f32 (live: finite 12/12)
  std::array<std::uint8_t, 0x2BC8 - 0x2820> reserved_2820{};
  float contact_2bc8;                      // +0x2BC8 f32 (live: finite 12/12)
  std::array<std::uint8_t, 0x2BD0 - 0x2BCC> reserved_2bcc{};
  float contact_2bd0;                      // +0x2BD0 f32 (live: finite 12/12)
  std::array<std::uint8_t, 0x2BF8 - 0x2BD4> reserved_2bd4{};
  float contact_2bf8;                      // +0x2BF8 f32 (live: finite 12/12); contact vector x/y/z at 2BF8/2BFC/2C00 (kick doc)
  std::array<std::uint8_t, 0x2D14 - 0x2BFC> reserved_2bfc{};
  std::uint8_t kick_flag_2d14;             // +0x2D14 u8 read by the scatter
  std::array<std::uint8_t, 0x32E4 - 0x2D15> reserved_2d15{};
  std::int32_t kick_rw_32e4;               // +0x32E4 i32 read and written by the scatter (live readable 12/12)
};
static_assert(offsetof(PesKickerRecord, interface_ab8) == 0xAB8);
static_assert(offsetof(PesKickerRecord, kick_table_index) == 0x2752);
static_assert(offsetof(PesKickerRecord, kick_flag_2754) == 0x2754);
static_assert(offsetof(PesKickerRecord, kick_flag_2755) == 0x2755);
static_assert(offsetof(PesKickerRecord, kick_flag_2756) == 0x2756);
static_assert(offsetof(PesKickerRecord, kick_window_2758) == 0x2758);
static_assert(offsetof(PesKickerRecord, kick_window_2764) == 0x2764);
static_assert(offsetof(PesKickerRecord, kick_2768) == 0x2768);
static_assert(offsetof(PesKickerRecord, kick_281c) == 0x281C);
static_assert(offsetof(PesKickerRecord, contact_2bc8) == 0x2BC8);
static_assert(offsetof(PesKickerRecord, contact_2bd0) == 0x2BD0);
static_assert(offsetof(PesKickerRecord, contact_2bf8) == 0x2BF8);
static_assert(offsetof(PesKickerRecord, kick_flag_2d14) == 0x2D14);
static_assert(offsetof(PesKickerRecord, kick_rw_32e4) == 0x32E4);
static_assert(sizeof(PesKickerRecord) == 0x32E8, "lower bound: the furthest read is kick_rw_32e4 + 4");

// The action interface a record holds at +0xAB8 (the kick doc's `vtbl+0x28` object). Live: one static object shared by
// every kick capture (0x1425E3508), vtable at +0 in the image. Only the vtable is pinned.
struct PesActionInterface {
  std::uint64_t vtable;  // +0: in the image (live 12/12)
};
static_assert(offsetof(PesActionInterface, vtable) == 0);

// The corridor-angle ctx (EndClear2): A0 of 0x142108630, Clear unit+8, and A1 of the sanction tree (same +0x18/+0xE0
// pair; identity A0 = A1 = unit+8 is INF). Live: both pointers readable over the extents below (6/6).
struct PesCtxRecord {
  std::array<std::uint8_t, 0x18> reserved_0000{};
  std::uint64_t object_18;                 // +0x18: readable to +0x2601C+4 (6/6)
  std::array<std::uint8_t, 0xE0 - 0x20> reserved_0020{};
  std::uint64_t e0_ring;                   // +0xE0: E0 ring object (readable to +0x180698+4, 6/6)
};
static_assert(offsetof(PesCtxRecord, object_18) == 0x18);
static_assert(offsetof(PesCtxRecord, e0_ring) == 0xE0);

// The E0 ring header at E0 (its count and head). Live: count 8 (saturated), head 0..7 (6/6).
struct PesE0RingHeader {
  std::array<std::uint8_t, 0x1900> reserved_0000{};
  std::uint32_t count;                     // +0x1900 (saturates at kRingSlots)
  std::uint32_t head;                      // +0x1904 (0..kRingSlots-1)
};
static_assert(offsetof(PesE0RingHeader, count) == 0x1900);
static_assert(offsetof(PesE0RingHeader, head) == 0x1904);

// The Clear unit (r8 of 0x142109EB0, EndClear2). Live: +0 carrier record pointer, +8 ctx, +0x28 imageA (5/5).
struct PesClearUnit {
  std::uint64_t carrier_record;            // +0 (a readable pointer; not a G table entry, 0/5)
  std::uint64_t ctx;                       // +8 (the ctx above)
  std::array<std::uint8_t, 0x28 - 0x10> reserved_0010{};
  std::uint64_t image_a;                   // +0x28 (readable to +0x31C2, 5/5)
};
static_assert(offsetof(PesClearUnit, carrier_record) == 0x0);
static_assert(offsetof(PesClearUnit, ctx) == 0x8);
static_assert(offsetof(PesClearUnit, image_a) == 0x28);

// The carrier record (unit+0): live u32 at +0 is a carrier id in 0..21 (5/5). +4 equals +0 in one capture (OPEN).
struct PesCarrierRecord {
  std::uint32_t carrier_id;                // +0
  std::uint32_t id_copy_4;                 // +4 (OPEN)
};
static_assert(offsetof(PesCarrierRecord, carrier_id) == 0);
static_assert(offsetof(PesCarrierRecord, id_copy_4) == 4);

// imageA (unit+0x28): the danger byte at +0x31C1 is 0 or 1 (live 5/5). GF reads it as `danger` in pescarrier.hpp.
struct PesImageA {
  std::array<std::uint8_t, 0x31C1> reserved_0000{};
  std::uint8_t danger;                     // +0x31C1
};
static_assert(offsetof(PesImageA, danger) == 0x31C1);

// G+0x180: a table of entries at stride 0x74, indexed by carrier id (EndClear2 port doc). Live: readable at [id] to
// +0x6E (6/6). The stride is the port doc's; the bytes at +0x6B/+0x6D are read by the kick scatter (semantics OPEN).
inline constexpr std::size_t kCarrierTableStride = 0x74;
struct PesCarrierTableEntry {
  std::array<std::uint8_t, 0x6B> reserved_0000{};
  std::uint8_t field_6b;                   // +0x6B
  std::uint8_t reserved_6c;
  std::uint8_t field_6d;                   // +0x6D
};
static_assert(offsetof(PesCarrierTableEntry, field_6b) == 0x6B);
static_assert(offsetof(PesCarrierTableEntry, field_6d) == 0x6D);
static_assert(sizeof(PesCarrierTableEntry) == 0x6E, "lower bound: the furthest read is +0x6D");

}  // namespace pes
