// PES's world object G (the match's record tables) and the per-player blocks GF owns beside it, in PES's byte layout.
//
// G's record tables are runs of 40-byte entries whose first qword is the VA of a record (docs/re/pes-re-player-record.md
// 1.1). The translated accessors 0x140A48A30, 0x140A489D0, 0x140A48910, 0x140A48A10 and 0x140A489F0 read them in place
// through PesMem::window. GF's world is one window: G's header, then the per-player blocks, so every pointer a table
// entry holds addresses bytes GF owns.

#pragma once

#include <cassert>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "pesclock.hpp"

namespace pes {

// Where the translated code addresses G and its blocks (PesMem::window). Above PES's image and the team sheet window.
inline constexpr uint64_t kWorldWindowVa = 0x2000100000ULL;

// G's header blob (pes-re-player-record.md 0 and 1.1). The per-player blocks follow it in the window.
inline constexpr std::size_t kWorldHeaderBytes = 0x2E58;

// G's pointer to the match-info block: the translated stoppage functions read MI = *(G+0x270) (pes-re-ref-record.md 1.1).
inline constexpr std::size_t kMatchInfoPtrOffset = 0x270;

// The match-info block MI at PES's offsets (pes-re-ref-record.md 1.2). Its bytes run from MI+0x0 past the last field a
// translated stoppage function reads (MI+0x15B8, a byte), rounded up to 0x40. The period record is at MI+0x1168.
inline constexpr std::size_t kMatchInfoBytes = 0x15C0;
inline constexpr std::size_t kMatchInfoPeriodOffset = 0x1168;

// A table entry: the record's VA, then PES bytes this layout does not name. The accessors step 0x28 per index.
inline constexpr std::size_t kTableEntryBytes = 0x28;

// G's tables at PES's offsets (accessor VA in the comment). Each count is the accessor's bound: an index at or past it
// answers with entry 0, the accessor's fallback *(G+base).
inline constexpr std::size_t kSideTableAOffset = 0x1928;  // 0x140A48A30
inline constexpr std::size_t kSideTableBOffset = 0x1978;  // 0x140A489D0
inline constexpr std::size_t kAnimeTableOffset = 0x19C8;  // 0x140A48910
inline constexpr std::size_t kManTableOffset = 0x1DD8;    // 0x140A48A10
inline constexpr std::size_t kSlotTableOffset = 0x25F8;   // 0x140A489F0
inline constexpr int kSideCount = 2;
inline constexpr int kAnimeCount = 26;
inline constexpr int kManCount = 26;
inline constexpr int kSlotCount = 22;
inline constexpr int kSlotsPerSide = 11;  // a side's squad places 0..10 (pesdesignation.hpp kSideSlots)
static_assert(kSlotCount == 2 * kSlotsPerSide);

enum class WorldTable { kSideA, kSideB, kAnime, kMan, kSlot };

struct PesTableEntry {
  uint64_t record = 0;  // VA of the record the entry names
  std::array<uint8_t, kTableEntryBytes - sizeof(uint64_t)> unnamed{};
};
static_assert(sizeof(PesTableEntry) == kTableEntryBytes);

// G's header as its tables sit in it. Bytes no accessor reads are reserved space.
struct PesWorldTables {
  std::array<uint8_t, kMatchInfoPtrOffset> reserved_head{};  // G+0x0 .. G+0x270
  uint64_t match = 0;                                         // G+0x270: the VA of the match-info block (MatchInfoVa)
  std::array<uint8_t, kSideTableAOffset - kMatchInfoPtrOffset - sizeof(uint64_t)> reserved_mid{};  // G+0x278 .. G+0x1928
  std::array<PesTableEntry, kSideCount> sideA{};                      // G+0x1928 (0x140A48A30)
  std::array<PesTableEntry, kSideCount> sideB{};                      // G+0x1978 (0x140A489D0)
  std::array<PesTableEntry, kAnimeCount> anime{};                     // G+0x19C8 (0x140A48910)
  std::array<PesTableEntry, kManCount> man{};                         // G+0x1DD8 (0x140A48A10)
  std::array<PesTableEntry, kAnimeCount> reserved_run{};              // G+0x21E8: no accessor reads it
  std::array<PesTableEntry, kSlotCount> slot{};                       // G+0x25F8 (0x140A489F0)
  std::array<uint8_t, kWorldHeaderBytes - kSlotTableOffset - kSlotCount * kTableEntryBytes> reserved_tail{};
};
static_assert(offsetof(PesWorldTables, sideA) == kSideTableAOffset);
static_assert(offsetof(PesWorldTables, sideB) == kSideTableBOffset);
static_assert(offsetof(PesWorldTables, anime) == kAnimeTableOffset);
static_assert(offsetof(PesWorldTables, man) == kManTableOffset);
static_assert(offsetof(PesWorldTables, slot) == kSlotTableOffset);
static_assert(sizeof(PesWorldTables) == kWorldHeaderBytes);
static_assert(offsetof(PesWorldTables, match) == kMatchInfoPtrOffset);

// One block per on-pitch slot, PES's stride (pes-re-player-record.md 1.2-1.3): the anime record at +0x0, the man record
// at +0x32C, the slot record at +0x10F0. The field offsets below are from each record's own start.
inline constexpr std::size_t kPlayerBlockBytes = 0x3F40;
inline constexpr std::size_t kManRecordOffset = 0x32C;
inline constexpr std::size_t kSlotRecordOffset = 0x10F0;

// Anime record (its bound is the man record's start, +0x32C).
inline constexpr std::size_t kAnimeClassOffset = 0x1;  // u8 class byte; 0x140654E60 halves its reach term on 3

// Man record (its bound is the slot record's start, +0x10F0).
inline constexpr std::size_t kManFacingOffset = 0x554;      // f32, body facing in degrees (INF: GF's bearing unit)
inline constexpr std::size_t kManPosXOffset = 0x55C;        // f32, PES x
inline constexpr std::size_t kManPosYOffset = 0x560;        // f32, PES y (height; GF coords[2], INF)
inline constexpr std::size_t kManPosZOffset = 0x564;        // f32, PES z (GF y)
inline constexpr std::size_t kManVelXOffset = 0x574;        // f32, m per frame
inline constexpr std::size_t kManVelYOffset = 0x578;        // f32
inline constexpr std::size_t kManVelZOffset = 0x57C;        // f32
inline constexpr std::size_t kManActionOffset = 0x5D0;      // s32 action word
inline constexpr std::size_t kManActionPrevOffset = 0x5D4;  // s32
inline constexpr std::size_t kManTouchingOffset = 0x661;    // u8 bool
inline constexpr std::size_t kManRepeatOffset = 0x6B8;      // s32 repeat count
// The man record's bound: the slot record starts after it (pes-re-player-record.md 1.3).
inline constexpr std::size_t kManRecordBytes = kSlotRecordOffset - kManRecordOffset;

// Slot record (its bound is the next block, +0x3F40).
inline constexpr std::size_t kSlotLoOffset = 0x1A;          // u16
inline constexpr std::size_t kSlotHiOffset = 0x1C;          // u16
inline constexpr std::size_t kSlotCapOffset = 0x22;         // u16
inline constexpr std::size_t kSlotAnchorXOffset = 0x1414;   // f32, AI target point x
inline constexpr std::size_t kSlotCounterAOffset = 0x1430;  // s32 counter A
inline constexpr std::size_t kSlotCounterBOffset = 0x1438;  // s32 counter B

static_assert(kManRecordOffset + kManRepeatOffset + sizeof(int32_t) <= kSlotRecordOffset);
static_assert(kSlotRecordOffset + kSlotCounterBOffset + sizeof(int32_t) <= kPlayerBlockBytes);

struct PesPlayerBlock {
  std::array<uint8_t, kPlayerBlockBytes> bytes{};
};

// A PES record's f32 field, `offset` bytes from the record's start (the offsets above are from the man record's start).
inline float LoadF32(const uint8_t* record, std::size_t offset) {
  float value;
  std::memcpy(&value, record + offset, sizeof value);
  return value;
}
inline void StoreF32(uint8_t* record, std::size_t offset, float value) {
  std::memcpy(record + offset, &value, sizeof value);
}

// The translated bearing's scratch: the image's tail, past the last block (kScratchVa). The translated code runs on its
// stack, which grows down from the end (the atan2 polynomial spills to rsp+0x20), and BearingOfPoints writes its two
// points there. A ground point is laid out as the translated code reads it: x at +0, z at +kPointZOffset.
inline constexpr std::size_t kScratchBytes = 256;
// The match-info block sits after G's header and the player blocks, and before the scratch.
inline constexpr std::size_t kMatchInfoOffset =
    kWorldHeaderBytes + static_cast<std::size_t>(kSlotCount) * kPlayerBlockBytes;
inline constexpr uint64_t kScratchVa = kWorldWindowVa + kMatchInfoOffset + kMatchInfoBytes;
inline constexpr std::size_t kPointXOffset = 0;
inline constexpr std::size_t kPointZOffset = 8;
inline constexpr std::size_t kPointBytes = 16;
static_assert(kManPosZOffset - kManPosXOffset == kPointZOffset, "a man record's position is laid out as a point");

// The match-info block (PES's MI): the period record the clock writes in place (pesclock.hpp), at its PES offset.
struct PesMatchInfo {
  std::array<uint8_t, kMatchInfoPeriodOffset> reserved_head{};  // MI+0x0 .. MI+0x1168: no GF writer yet
  PeriodRecord period{};                                        // MI+0x1168: PES's period record
  std::array<uint8_t, kMatchInfoBytes - kMatchInfoPeriodOffset - sizeof(PeriodRecord)> reserved_tail{};
};
static_assert(offsetof(PesMatchInfo, period) == kMatchInfoPeriodOffset);
static_assert(sizeof(PesMatchInfo) == kMatchInfoBytes);

// The whole window: G's header, then the blocks. Heap-owned by PesWorld, so the bytes never move.
struct PesWorldImage {
  PesWorldTables tables;
  std::array<PesPlayerBlock, kSlotCount> blocks{};
  PesMatchInfo match{};  // MI: the match-info block (MatchInfoVa)
  std::array<uint8_t, kScratchBytes> scratch{};  // the translated bearing's points and stack (kScratchVa)
};
static_assert(offsetof(PesWorldImage, blocks) == kWorldHeaderBytes);
static_assert(offsetof(PesWorldImage, match) == kMatchInfoOffset);
static_assert(offsetof(PesWorldImage, scratch) == kScratchVa - kWorldWindowVa);

inline constexpr std::size_t kWorldBytes = sizeof(PesWorldImage);

class PesWorld {
 public:
  PesWorld() : image_(std::make_unique<PesWorldImage>()) { image_->tables.match = MatchInfoVa(); }
  PesWorld(const PesWorld&) = delete;
  PesWorld& operator=(const PesWorld&) = delete;

  // The window's bytes: PesMem::window maps them at kWorldWindowVa for kWorldBytes.
  uint8_t* Bytes() { return reinterpret_cast<uint8_t*>(image_.get()); }

  // The VAs a slot's three records sit at (a table entry names one of these once bound).
  static constexpr uint64_t BlockVa(int slot) {
    return kWorldWindowVa + kWorldHeaderBytes + static_cast<uint64_t>(slot) * kPlayerBlockBytes;
  }

  // The VA of the match-info block: G+0x270 names it.
  static constexpr uint64_t MatchInfoVa() { return kWorldWindowVa + kMatchInfoOffset; }
  // The clock's period record, in the match-info block: PesClock writes it in place.
  PeriodRecord& Period() { return image_->match.period; }

  static constexpr uint64_t AnimeVa(int slot) { return BlockVa(slot); }
  static constexpr uint64_t ManVa(int slot) { return BlockVa(slot) + kManRecordOffset; }
  static constexpr uint64_t SlotVa(int slot) { return BlockVa(slot) + kSlotRecordOffset; }

  // Host bytes of a slot's block (its anime record starts at its first byte).
  uint8_t* Block(int slot) { return image_->blocks[slot].bytes.data(); }

  // Writes one table entry: the record VA it names.
  void SetEntry(WorldTable table, int index, uint64_t recordVa) { Entry(table, index).record = recordVa; }

  // Binds a slot's three table entries to its records: the anime, man and slot tables name its block.
  void BindSlot(int slot) {
    SetEntry(WorldTable::kAnime, slot, AnimeVa(slot));
    SetEntry(WorldTable::kMan, slot, ManVa(slot));
    SetEntry(WorldTable::kSlot, slot, SlotVa(slot));
  }

  // The record a slot's table entry names, as PES's accessor answers it: the translated 0x140A48910 (anime) and
  // 0x140A48A10 (man), run on the window. Null when the answer lies outside the window.
  uint8_t* AnimeRecord(int slot);
  uint8_t* ManRecord(int slot);

  // The contact flag: the man record's byte at +0x661.
  bool Touching(int slot) { return ManRecord(slot)[kManTouchingOffset] != 0; }
  void SetTouching(int slot, bool touching) { ManRecord(slot)[kManTouchingOffset] = touching ? 1 : 0; }

  // The anime class byte: the anime record's +0x1.
  uint8_t AnimeClass(int slot) { return AnimeRecord(slot)[kAnimeClassOffset]; }
  void SetAnimeClass(int slot, uint8_t value) { AnimeRecord(slot)[kAnimeClassOffset] = value; }

  // PES's bearing 0x140439EF0 run in place on the window: the heading in [0, 360) from man record `from` to man record
  // `to`, over their ground positions. Coincident records read 0.
  float Bearing(const uint8_t* from, const uint8_t* to);
  // The same bearing over two ground points that are not records (a goal centre), copied into the scratch first.
  float BearingOfPoints(float fromX, float fromZ, float toX, float toZ);
  // The window address of a man record of this world (the pointer ManRecord answers), as the translated code sees it.
  uint64_t VaOf(const uint8_t* record) {
    assert(record >= Bytes() && record < Bytes() + kWorldBytes);
    return kWorldWindowVa + static_cast<uint64_t>(record - Bytes());
  }

 private:
  PesTableEntry& Entry(WorldTable table, int index) {
    switch (table) {
      case WorldTable::kSideA: return image_->tables.sideA.at(index);
      case WorldTable::kSideB: return image_->tables.sideB.at(index);
      case WorldTable::kAnime: return image_->tables.anime.at(index);
      case WorldTable::kMan: return image_->tables.man.at(index);
      case WorldTable::kSlot: default: return image_->tables.slot.at(index);
    }
  }

  std::unique_ptr<PesWorldImage> image_;
};

}  // namespace pes
