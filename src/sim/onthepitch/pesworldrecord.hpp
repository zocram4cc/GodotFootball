// PES's world object G (the match's record tables) and the per-player blocks GF owns beside it, in PES's byte layout.
//
// G's record tables are runs of 40-byte entries whose first qword is the VA of a record (docs/re/pes-re-player-record.md
// 1.1). The translated accessors 0x140A48A30, 0x140A489D0, 0x140A48910, 0x140A48A10 and 0x140A489F0 read them in place
// through PesMem::window. GF's world is one window: G's header, then the per-player and official blocks, the match-info
// block, the team records, the ball record and the translated code's stack, so every pointer a table entry holds
// addresses bytes GF owns.

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

// The image global that holds G's VA (PES's fixture cell 0x1436F3DC8: the stoppage functions load G from it,
// pes-re-ref-record.md 1.1). The translated call maps it as an 8-byte segment holding kWorldWindowVa.
inline constexpr uint64_t kWorldGlobalVa = 0x1436F3DC8ULL;

// G's header blob (pes-re-player-record.md 0 and 1.1). The per-player blocks follow it in the window.
inline constexpr std::size_t kWorldHeaderBytes = 0x2E58;

// G's pointer to the ball record: the stoppage functions read the ball's fallback position through it (pes-re-ref-record.md 1.1).
inline constexpr std::size_t kBallPtrOffset = 0x40;

// G's pointer to the match-info block: the translated stoppage functions read MI = *(G+0x270) (pes-re-ref-record.md 1.1).
inline constexpr std::size_t kMatchInfoPtrOffset = 0x270;

// The match-info block MI at PES's offsets (pes-re-ref-record.md 1.2). Its bytes run from MI+0x0 past the last field a
// translated stoppage function reads (MI+0x15B8, a byte), rounded up to 0x40. The period record is at MI+0x1168.
inline constexpr std::size_t kMatchInfoBytes = 0x15C0;
inline constexpr std::size_t kMatchInfoPeriodOffset = 0x1168;

// The stoppage fields GF writes into MI at their writers (pes-re-ref-record.md 1.2; the values are PES's, the names are
// the decode's). Offsides are the three words the involvement reads at play state 1.
inline constexpr std::size_t kMiRestartArmedOffset = 0x11B2;    // u8: the restart-armed flag (116C == 3)
inline constexpr std::size_t kMiOffsideFlagOffset = 0x1544;    // u32: 0 none, 1 flagged
inline constexpr std::size_t kMiOffsideAOffset = 0x1548;       // u32
inline constexpr std::size_t kMiOffsideBOffset = 0x154C;       // u32: the offside category
inline constexpr std::size_t kMiRestartKindOffset = 0x1554;    // u32: 1 kick-off, 2 throw-in, 3 goal kick, 4 corner, 5 FK, 6 PK, 7 period end
inline constexpr std::size_t kMiRestartSideOffset = 0x1558;    // u32: the side taking the restart
inline constexpr std::size_t kMiSpotXOffset = 0x155C;          // f32: the restart spot x (m)
inline constexpr std::size_t kMiSpotZOffset = 0x1564;          // f32: the restart spot z (m)
inline constexpr std::size_t kMiStopReasonOffset = 0x1574;     // u32: 1 period end, 3 kick-off, 4 ball out, 5 foul or offside
inline constexpr std::size_t kMiFoulKindOffset = 0x1588;       // u32: 1 offside, 2 direct foul, 3 indirect
inline constexpr uint32_t kFoulKindDirect = 2;                  // MI+0x1588 and the foul event's +0x18: a direct foul
inline constexpr std::size_t kMiCardLevelOffset = 0x158C;      // u32: 0 none, 1 foul, 2 foul + warning, 3 yellow, 4 red
inline constexpr std::size_t kMiFoulerOffset = 0x1590;         // u32: the fouler's slot (0..21, 0xFF none)
inline constexpr std::size_t kMiVictimOffset = 0x1594;         // u32: the victim's slot (0..21, 0xFF none)
inline constexpr std::size_t kMiNoSlot = 0xFF;

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
// G's two pointers the sanction tree 0x14047F170 reads past the tables (docs/ports/pes-port-FoulSanction.md 2): the field
// info (two f32: the pitch's half length and half width, line half-width included, in metres) and the competition rules
// object read by the card-history reader 0x140A81230 (null here: GF has no aggregate-tie object, 0x140A80D10 answers 0).
inline constexpr std::size_t kFieldInfoPtrOffset = 0x2AA8;
inline constexpr std::size_t kCompetitionPtrOffset = 0x2AD0;
inline constexpr int kSlotsPerSide = 11;  // a side's squad places 0..10 (pesdesignation.hpp kSideSlots)
static_assert(kSlotCount == 2 * kSlotsPerSide);

// The officials' blocks follow the players' (PES's official indices 22..25; the table counts above already name them).
// The label helper 0x141E53520 (replay/pes_fn_141E53520.cpp) gives index minus 22 for 22..24: the referee is 22, the
// assistants are 23 and 24 (CONFIRMED from the helper's code; the assistants' order is INF). 25, the fourth official,
// is not a GF role.
inline constexpr int kOfficialCount = 4;
inline constexpr int kRefereeOfficialIndex = 22;
inline constexpr int kBlockCount = kSlotCount + kOfficialCount;

enum class WorldTable { kSideA, kSideB, kAnime, kMan, kSlot };

struct PesTableEntry {
  uint64_t record = 0;  // VA of the record the entry names
  std::array<uint8_t, kTableEntryBytes - sizeof(uint64_t)> unnamed{};
};
static_assert(sizeof(PesTableEntry) == kTableEntryBytes);

// G's header as its tables sit in it. Bytes no accessor reads are reserved space.
struct PesWorldTables {
  std::array<uint8_t, kBallPtrOffset> reserved_head{};        // G+0x0 .. G+0x40
  uint64_t ball = 0;                                          // G+0x40: the ball record (BallVa)
  std::array<uint8_t, kMatchInfoPtrOffset - kBallPtrOffset - sizeof(uint64_t)> reserved_ball{};  // G+0x48 .. G+0x270
  uint64_t match = 0;                                         // G+0x270: the VA of the match-info block (MatchInfoVa)
  std::array<uint8_t, kSideTableAOffset - kMatchInfoPtrOffset - sizeof(uint64_t)> reserved_mid{};  // G+0x278 .. G+0x1928
  std::array<PesTableEntry, kSideCount> sideA{};                      // G+0x1928 (0x140A48A30)
  std::array<PesTableEntry, kSideCount> sideB{};                      // G+0x1978 (0x140A489D0)
  std::array<PesTableEntry, kAnimeCount> anime{};                     // G+0x19C8 (0x140A48910)
  std::array<PesTableEntry, kManCount> man{};                         // G+0x1DD8 (0x140A48A10)
  std::array<PesTableEntry, kAnimeCount> reserved_run{};              // G+0x21E8: no accessor reads it
  std::array<PesTableEntry, kSlotCount> slot{};                       // G+0x25F8 (0x140A489F0)
  std::array<uint8_t, kFieldInfoPtrOffset - kSlotTableOffset - kSlotCount * kTableEntryBytes> reserved_run2{};  // G+0x2968 .. G+0x2AA8
  uint64_t fieldInfo = 0;                                                                                       // G+0x2AA8: the field info
  std::array<uint8_t, kCompetitionPtrOffset - kFieldInfoPtrOffset - sizeof(uint64_t)> reserved_field{};          // G+0x2AB0 .. G+0x2AD0
  uint64_t competition = 0;                                                                                     // G+0x2AD0: the competition object (null)
  std::array<uint8_t, kWorldHeaderBytes - kCompetitionPtrOffset - sizeof(uint64_t)> reserved_tail{};            // G+0x2AD8 .. G+0x2E58
};
static_assert(offsetof(PesWorldTables, ball) == kBallPtrOffset);
static_assert(offsetof(PesWorldTables, match) == kMatchInfoPtrOffset);
static_assert(offsetof(PesWorldTables, sideA) == kSideTableAOffset);
static_assert(offsetof(PesWorldTables, sideB) == kSideTableBOffset);
static_assert(offsetof(PesWorldTables, anime) == kAnimeTableOffset);
static_assert(offsetof(PesWorldTables, man) == kManTableOffset);
static_assert(offsetof(PesWorldTables, slot) == kSlotTableOffset);
static_assert(offsetof(PesWorldTables, fieldInfo) == kFieldInfoPtrOffset);
static_assert(offsetof(PesWorldTables, competition) == kCompetitionPtrOffset);
static_assert(sizeof(PesWorldTables) == kWorldHeaderBytes);

// One block per on-pitch slot or official, PES's stride (pes-re-player-record.md 1.2-1.3): the anime record at +0x0, the
// man record at +0x32C, the slot record at +0x10F0. The field offsets below are from each record's own start.
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

// The official's per-frame record (the involvement's A0, pes-re-sanction-caller.md 5.2): the official index is a u32 at +4.
// It sits in the official's block past the slot record's last field, where no accessor reads.
inline constexpr std::size_t kOfficialFrameOffset = 0x3000;
inline constexpr std::size_t kOfficialFrameIndexOffset = 0x4;

static_assert(kManRecordOffset + kManRepeatOffset + sizeof(int32_t) <= kSlotRecordOffset);
static_assert(kSlotRecordOffset + kSlotCounterBOffset + sizeof(int32_t) <= kOfficialFrameOffset);
static_assert(kOfficialFrameOffset + kOfficialFrameIndexOffset + sizeof(uint32_t) <= kPlayerBlockBytes);

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
inline uint32_t LoadU32(const uint8_t* record, std::size_t offset) {
  uint32_t value;
  std::memcpy(&value, record + offset, sizeof value);
  return value;
}
inline void StoreU32(uint8_t* record, std::size_t offset, uint32_t value) {
  std::memcpy(record + offset, &value, sizeof value);
}

// A team record (G's side tables name them). Its attack sign is a signed byte at +0x254 (A5Judge 1.2).
inline constexpr std::size_t kSideRecordBytes = 0x260;
inline constexpr std::size_t kSideAttackSignOffset = 0x254;
struct PesSideRecord {
  std::array<uint8_t, kSideRecordBytes> bytes{};
};

// The ball record: its fallback position is a f32 pair at +0x2770 (x) and +0x2778 (z) (pes-re-ref-record.md 1.3).
inline constexpr std::size_t kBallRecordBytes = 0x2780;
inline constexpr std::size_t kBallXOffset = 0x2770;
inline constexpr std::size_t kBallZOffset = 0x2778;
struct PesBallRecord {
  std::array<uint8_t, kBallRecordBytes> bytes{};
};

// The translated bearing's scratch: the image's tail, past the last block (kScratchVa). The translated code runs on its
// stack, which grows down from the end (the atan2 polynomial spills to rsp+0x20), and BearingOfPoints writes its two
// points there. A ground point is laid out as the translated code reads it: x at +0, z at +kPointZOffset.
inline constexpr std::size_t kScratchBytes = 256;
// The involvement and the other stoppage functions run their own stack in the window's tail (kStackVa).
inline constexpr std::size_t kStackBytes = 0x2000;
inline constexpr std::size_t kPointXOffset = 0;
inline constexpr std::size_t kPointZOffset = 8;
inline constexpr std::size_t kPointBytes = 16;
static_assert(kManPosZOffset - kManPosXOffset == kPointZOffset, "a man record's position is laid out as a point");

// The image's layout after the header: the blocks, the match-info block, the team records, the ball record, the scratch
// and the stack.
inline constexpr std::size_t kMatchInfoOffset = kWorldHeaderBytes + static_cast<std::size_t>(kBlockCount) * kPlayerBlockBytes;
inline constexpr std::size_t kSidesOffset = kMatchInfoOffset + kMatchInfoBytes;
inline constexpr std::size_t kBallOffset = kSidesOffset + static_cast<std::size_t>(kSideCount) * kSideRecordBytes;
inline constexpr std::size_t kScratchOffset = kBallOffset + kBallRecordBytes;
inline constexpr std::size_t kStackOffset = kScratchOffset + kScratchBytes;
inline constexpr uint64_t kScratchVa = kWorldWindowVa + kScratchOffset;
inline constexpr uint64_t kStackVa = kWorldWindowVa + kStackOffset;

// ---- The sanction tree's records: PES's 0x14047F170 reads them in place (docs/ports/pes-port-FoulSanction.md 2). ----
// A0, the request (Flow+0x6D8): the tree's answer, the scene id, is stored at +0 by the builder 0x140480910; the tree
// reads the played-scene counter at +0xD (0 on the stop's first decision).
inline constexpr std::size_t kRequestBytes = 0x40;
inline constexpr std::size_t kRequestSceneOffset = 0x0;   // u32
inline constexpr std::size_t kRequestPlayedOffset = 0xD;  // u8
// A1, the ANALYZE object (owner+0x228): its +0x18 names a block whose +0x24A0C holds two 0x14-byte per-side records
// (0x14042D5E0 reads one; its dword +8 == 3 is read at 0x14047F70D), its +0xE0 names the event store E.
inline constexpr std::size_t kAnalyzeBytes = 0x100;
inline constexpr std::size_t kAnalyzeBlockPtrOffset = 0x18;
inline constexpr std::size_t kAnalyzeEventsPtrOffset = 0xE0;
inline constexpr std::size_t kAnalyzeSideRecordsOffset = 0x24A0C;
inline constexpr std::size_t kAnalyzeSideRecordBytes = 0x14;
inline constexpr std::size_t kAnalyzeBlockBytes = kAnalyzeSideRecordsOffset + kSideCount * kAnalyzeSideRecordBytes;
// E, the event store: ten rings of eight 0x320-byte records at E + ring * 0x1908, the count at +0x1900 and the head at
// +0x1904 (0x140A3E150 reads them; 0x140A3DAC0 writes them). Ring 4 holds the foul events (the tree reads the newest).
// E is the window's last member: the card-count tables the card-history reader 0x140A80D10 reads at E+0x750D0 and
// E+0xCE720 lie past the window's end and answer zero (a ponytail: PES's writer of those counters is not decoded;
// until it is, the leading-team late-foul scenes never fire).
inline constexpr std::size_t kEventRingStride = 0x1908;
inline constexpr std::size_t kEventRecordBytes = 0x320;
inline constexpr std::size_t kEventFoulRing = 4;
inline constexpr std::size_t kEventStoreBytes = (kEventFoulRing + 1) * kEventRingStride;
// The foul event record at PES's offsets (docs/ports/pes-port-FoulSanction.md 2.2, read from seven foul captures).
inline constexpr uint32_t kFoulEventKind = 0x1E;            // +0x00: the routing kind of a foul event (30 in every record)
inline constexpr std::size_t kFoulEventKindOffset = 0x00;
inline constexpr std::size_t kFoulEventFrame1Offset = 0x04;  // u32: the played frame
inline constexpr std::size_t kFoulEventFrame2Offset = 0x08;  // u32: the played frame (equal to +4 in every capture)
inline constexpr std::size_t kFoulEventPeriodOffset = 0x10;  // u32: the period state (MI+0x1168)
inline constexpr std::size_t kFoulEventFoulKindOffset = 0x18;  // u32: 2 direct, 3 indirect (the tree's iStack_340)
inline constexpr std::size_t kFoulEventCardOffset = 0x1C;      // u32: the card level (= MI+0x158C in every capture)
inline constexpr std::size_t kFoulEventSideOffset = 0x28;      // u32: the fouling side's restart side (= MI+0x1558)
inline constexpr std::size_t kFoulEventInjuredOffset = 0x42;   // u8: the injury veto the tree reads at 0x14047F364
// The field info G+0x2AA8 names: two f32.
inline constexpr std::size_t kFieldInfoBytes = 8;
inline constexpr std::size_t kFieldInfoHalfLengthOffset = 0;
inline constexpr std::size_t kFieldInfoHalfWidthOffset = 4;
// The per-side team info (G+0x1978 table, 0x140A489D0): +0 the side, +4 a u32 per squad place naming his record, the
// records from +0xA8 at stride 0x128 (0x140A3AF90). A record's +0 is his yellow-card count, his id is the u32 at +0x34
// (0x140A3B520 reads it as present while the image global 0x14351AE34 is 0, which it is in every capture).
inline constexpr std::size_t kTeamInfoSideOffset = 0x0;
inline constexpr std::size_t kTeamInfoPlaceMapOffset = 0x4;
inline constexpr std::size_t kTeamInfoRecordsOffset = 0xA8;
inline constexpr std::size_t kTeamInfoRecordBytes = 0x128;
inline constexpr int kTeamInfoRecordCount = 0x28;
inline constexpr std::size_t kTeamInfoBytes = kTeamInfoRecordsOffset + kTeamInfoRecordCount * kTeamInfoRecordBytes;
inline constexpr std::size_t kTeamInfoYellowsOffset = 0x0;  // u8, counting the card being decided
inline constexpr std::size_t kTeamInfoIdOffset = 0x34;      // u32, nonzero for a player on the pitch

struct PesSanctionImage {
  std::array<uint8_t, kRequestBytes> request{};
  std::array<uint8_t, kAnalyzeBytes> analyze{};
  std::array<uint8_t, kAnalyzeBlockBytes> analyzeBlock{};
  std::array<uint8_t, kFieldInfoBytes> fieldInfo{};
  std::array<std::array<uint8_t, kTeamInfoBytes>, kSideCount> teamInfo{};
  std::array<uint8_t, kEventRecordBytes> foulSource{};  // the record 0x140A3DAC0 copies into the ring
  std::array<uint8_t, kEventStoreBytes> events{};       // E: last, so the counter tables past it read zero
};
inline constexpr std::size_t kSanctionOffset = kStackOffset + kStackBytes;

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
  std::array<PesPlayerBlock, kBlockCount> blocks{};
  PesMatchInfo match{};  // MI: the match-info block (MatchInfoVa)
  std::array<PesSideRecord, kSideCount> sides{};
  PesBallRecord ball{};
  std::array<uint8_t, kScratchBytes> scratch{};  // the translated bearing's points (kScratchVa)
  std::array<uint8_t, kStackBytes> stack{};      // the translated stoppage functions' stack (kStackVa)
  PesSanctionImage sanction{};                   // the sanction tree's records (kSanctionOffset): E is the window's last bytes
};
static_assert(offsetof(PesWorldImage, blocks) == kWorldHeaderBytes);
static_assert(offsetof(PesWorldImage, match) == kMatchInfoOffset);
static_assert(offsetof(PesWorldImage, sides) == kSidesOffset);
static_assert(offsetof(PesWorldImage, ball) == kBallOffset);
static_assert(offsetof(PesWorldImage, scratch) == kScratchOffset);
static_assert(offsetof(PesWorldImage, stack) == kStackOffset);
static_assert(offsetof(PesWorldImage, sanction) == kSanctionOffset);
static_assert(offsetof(PesWorldImage, sanction) + offsetof(PesSanctionImage, events) + kEventStoreBytes == sizeof(PesWorldImage),
              "E is the window's last member: the tree's counter tables past it read zero");

inline constexpr std::size_t kWorldBytes = sizeof(PesWorldImage);

class PesWorld {
 public:
  PesWorld() : image_(std::make_unique<PesWorldImage>()) {
    image_->tables.ball = BallVa();
    image_->tables.match = MatchInfoVa();
    for (int team = 0; team < kSideCount; ++team) SetEntry(WorldTable::kSideA, team, SideVa(team));
    BindSanction();
  }
  PesWorld(const PesWorld&) = delete;
  PesWorld& operator=(const PesWorld&) = delete;

  // The window's bytes: PesMem::window maps them at kWorldWindowVa for kWorldBytes.
  uint8_t* Bytes() { return reinterpret_cast<uint8_t*>(image_.get()); }

  // The VAs a slot's three records sit at (a table entry names one of these once bound). Officials use the same stride.
  static constexpr uint64_t BlockVa(int slot) {
    return kWorldWindowVa + kWorldHeaderBytes + static_cast<uint64_t>(slot) * kPlayerBlockBytes;
  }

  // The VA of the match-info block: G+0x270 names it.
  static constexpr uint64_t MatchInfoVa() { return kWorldWindowVa + kMatchInfoOffset; }
  static constexpr uint64_t SideVa(int side) {
    return kWorldWindowVa + kSidesOffset + static_cast<uint64_t>(side) * kSideRecordBytes;
  }
  static constexpr uint64_t BallVa() { return kWorldWindowVa + kBallOffset; }
  static constexpr uint64_t SanctionVa(std::size_t member) { return kWorldWindowVa + kSanctionOffset + member; }
  // The clock's period record, in the match-info block: PesClock writes it in place.
  PeriodRecord& Period() { return image_->match.period; }
  // The match-info block's bytes, at PES's offsets (the kMi* constants).
  uint8_t* MatchInfo() { return reinterpret_cast<uint8_t*>(&image_->match); }

  // ---- PES's sanction tree 0x14047F170 on the window (docs/ports/pes-port-FoulSanction.md). ----
  // The pitch's half length and half width in metres, line half-width included (the field info G+0x2AA8 names).
  void SetFieldInfo(float halfLength_m, float halfWidth_m);
  // A slot's team-info record: his id (nonzero: on the pitch) and his yellow-card count, the card being decided included.
  void SetPlayerInfo(int slot, uint32_t id, int yellows);
  // The foul event the judge pushes into E's ring 4 (0x140A3DAC0 runs on the window), at the offsets the tree reads.
  struct FoulEvent {
    uint32_t periodState = 0;  // MI+0x1168
    uint32_t foulKind = 0;   // 2 direct, 3 indirect
    uint32_t cardLevel = 0;  // 1 foul, 2 foul + warning, 3 yellow, 4 red
    uint32_t side = 0;       // the restart side
    bool injured = false;
  };
  void PushFoulEvent(const FoulEvent& event);
  // Runs the tree on the stop the match-info block, the foul ring and the team info hold; the scene id it returns is stored
  // in the request at +0 (as the builder does) and answered. 0x20022 is PES's "no scene".
  uint32_t DecideSanction();
  uint32_t SanctionScene() { return LoadU32(image_->sanction.request.data(), kRequestSceneOffset); }
  void SetScenePlayed(uint8_t count) { image_->sanction.request[kRequestPlayedOffset] = count; }

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
  // Binds an official's anime and man entries (the tables count the officials; the slot table covers players only).
  void BindOfficial(int official) {
    SetEntry(WorldTable::kAnime, official, AnimeVa(official));
    SetEntry(WorldTable::kMan, official, ManVa(official));
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
  // The ball's ground position in metres (PES x, PES z = GF y): the ball record's fallback position at +0x2770 and +0x2778,
  // which the stoppage functions read for the spot the ball stands on (pes-re-ref-record.md 1.3).
  void SetBall(float x, float z) {
    StoreF32(image_->ball.bytes.data(), kBallXOffset, x);
    StoreF32(image_->ball.bytes.data(), kBallZOffset, z);
  }
  // The team record's attack sign (+0x254, s8): +1 when the team attacks +x (pes-re-ref-record.md 3.3).
  void SetAttackSign(int team, int sign) {
    image_->sides[team].bytes[kSideAttackSignOffset] = static_cast<uint8_t>(static_cast<int8_t>(sign));
  }

  // PES's officials' involvement 0x140A3A690 (pes-re-ref-record.md 1.5): whether `official` is involved in the stoppage
  // the match-info block holds. Runs the translated function on the window.
  bool Involved(int official);

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
  // Points G's field info, the side-B team info, A1 and its blocks at the sanction image (the ctor).
  void BindSanction();
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
