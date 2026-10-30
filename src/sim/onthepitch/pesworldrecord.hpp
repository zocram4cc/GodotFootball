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
#include "pesevents.hpp"
#include "pestrapworld.hpp"
#include "pesimagetables.hpp"
#include "pes_native.hpp"

struct PesState;  // the translated layer's call state (pes_state.hpp)

namespace pes {

// Where the translated code addresses G and its blocks (PesMem::window). Above PES's image and the team sheet window.
inline constexpr uint64_t kWorldWindowVa = 0x2000100000ULL;

// The image global that holds G's VA (PES's fixture cell 0x1436F3DC8: the stoppage functions load G from it,
// pes-re-ref-record.md 1.1). The translated call maps it as an 8-byte segment holding kWorldWindowVa.
inline constexpr uint64_t kWorldGlobalVa = 0x1436F3DC8ULL;

// G's header blob (pes-re-player-record.md 0 and 1.1). The per-player blocks follow it in the window.
inline constexpr std::size_t kWorldHeaderBytes = 0x2E58;

// G's pointer to the frame-random record (docs/re/pes-re-setpiece-kicker.md: *(G+0x360)+0 is the frame-random int, +4 the
// frame-random byte; the corridor angle's carrier-state builder 0x14063A490 copies *(G+0x360)+8 at 0x14063A5F8, read live
// 28-10-26 as a float in [0.25, 1)). PES's G reset 0x140A48273 stores this slot beside +0x270 and +0x2AA8.
inline constexpr std::size_t kFrameRandomPtrOffset = 0x360;
inline constexpr std::size_t kFrameRandomIntOffset = 0x0;    // u32, 0..32767
inline constexpr std::size_t kFrameRandomByteOffset = 0x4;   // u32 word whose low byte the set-piece kicker tests
inline constexpr std::size_t kFrameRandomUnitOffset = 0x8;   // f32 (INF: unit float the carrier-state builder copies)
inline constexpr std::size_t kFrameRandomBytes = 0x10;

// G's pointer to the pitch record F (pes-re-fieldinfo.md: F[k] is the f32 at +4k; 0x14063A490 and the angle load it at
// *(G+0x2AA8); PES's G reset 0x140A48273 stores this slot).
inline constexpr std::size_t kPitchPtrOffset = 0x2AA8;
inline constexpr int kPitchFieldCount = 18;  // F[0]..F[17] (gametypes.hpp)
inline constexpr std::size_t kPitchRecordBytes = 0x48;

// G's pointer to the ball record: the stoppage functions read the ball's fallback position through it (pes-re-ref-record.md
// 1.1). The corridor angle reads its prediction samples (B+0x88 + 0x28*j, the record FUN_140A6CF70 indexes), the first of
// which FUN_140A57800 names the ball's position, its velocity (FUN_140A57720 = B+0x2948) and the flag bytes at +0x4C and +0x80.
inline constexpr std::size_t kBallPtrOffset = 0x40;
// G's pointer to the match-info block: the translated stoppage functions read MI = *(G+0x270) (pes-re-ref-record.md 1.1).
inline constexpr std::size_t kMatchInfoPtrOffset = 0x270;

// The match-info block MI at PES's offsets (pes-re-ref-record.md 1.2). Its bytes run from MI+0x0 past the last field a
// translated stoppage function reads (MI+0x15B8, a byte), rounded up to 0x40. The period record is at MI+0x1168.
inline constexpr std::size_t kMatchInfoBytes = 0x1780;  // grown from 0x1770: the carrier walk 0x14210F110 reads MI+0x1775
inline constexpr std::size_t kMatchInfoPeriodOffset = 0x1168;

// The stoppage fields GF writes into MI at their writers (pes-re-ref-record.md 1.2; the values are PES's, the names are
// the decode's). Offsides are the three words the involvement reads at play state 1.
inline constexpr std::size_t kMiRestartArmedOffset = 0x11B2;    // u8: the restart-armed flag (116C == 3)
inline constexpr std::size_t kMiWalkStateOffset = 0x1775;     // u8: the FSM byte the carrier walk vetoes passes and clears on (3, 9, 10)
inline constexpr std::size_t kMiOffsideFlagOffset = 0x1544;    // u32: 0 none, 1 flagged
inline constexpr std::size_t kMiOffsideAOffset = 0x1548;       // u32
inline constexpr std::size_t kMiOffsideBOffset = 0x154C;       // u32: the offside category
inline constexpr std::size_t kMiRestartKindOffset = 0x1554;    // u32: 1 kick-off, 2 throw-in, 3 goal kick, 4 corner, 5 FK, 6 PK, 7 period end
inline constexpr std::size_t kMiRestartSideOffset = 0x1558;    // u32: the side taking the restart
inline constexpr std::size_t kMiSpotXOffset = 0x155C;          // f32: the restart spot x (m)
inline constexpr std::size_t kMiSpotZOffset = 0x1564;          // f32: the restart spot z (m)
inline constexpr std::size_t kMiStopReasonOffset = 0x1574;     // u32: 1 period end, 3 kick-off, 4 ball out, 5 foul or offside
inline constexpr std::size_t kMiFoulKindOffset = 0x1588;       // u32: 1 offside, 2 direct foul, 3 indirect
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
  std::array<uint8_t, kFrameRandomPtrOffset - kMatchInfoPtrOffset - sizeof(uint64_t)> reserved_match{};  // G+0x278 .. G+0x360
  uint64_t frameRandom = 0;                                   // G+0x360: the frame-random record (FrameRandomVa)
  std::array<uint8_t, kSideTableAOffset - kFrameRandomPtrOffset - sizeof(uint64_t)> reserved_mid{};  // G+0x368 .. G+0x1928
  std::array<PesTableEntry, kSideCount> sideA{};                      // G+0x1928 (0x140A48A30)
  std::array<PesTableEntry, kSideCount> sideB{};                      // G+0x1978 (0x140A489D0)
  std::array<PesTableEntry, kAnimeCount> anime{};                     // G+0x19C8 (0x140A48910)
  std::array<PesTableEntry, kManCount> man{};                         // G+0x1DD8 (0x140A48A10)
  std::array<PesTableEntry, kAnimeCount> reserved_run{};              // G+0x21E8: no accessor reads it
  std::array<PesTableEntry, kSlotCount> slot{};                       // G+0x25F8 (0x140A489F0)
  std::array<uint8_t, kPitchPtrOffset - kSlotTableOffset - kSlotCount * kTableEntryBytes> reserved_pitch{};
  uint64_t pitch = 0;                                         // G+0x2AA8: the pitch record F (PitchVa)
  std::array<uint8_t, kWorldHeaderBytes - kPitchPtrOffset - sizeof(uint64_t)> reserved_tail{};
};
static_assert(offsetof(PesWorldTables, ball) == kBallPtrOffset);
static_assert(offsetof(PesWorldTables, match) == kMatchInfoPtrOffset);
static_assert(offsetof(PesWorldTables, frameRandom) == kFrameRandomPtrOffset);
static_assert(offsetof(PesWorldTables, sideA) == kSideTableAOffset);
static_assert(offsetof(PesWorldTables, sideB) == kSideTableBOffset);
static_assert(offsetof(PesWorldTables, anime) == kAnimeTableOffset);
static_assert(offsetof(PesWorldTables, man) == kManTableOffset);
static_assert(offsetof(PesWorldTables, slot) == kSlotTableOffset);
static_assert(offsetof(PesWorldTables, pitch) == kPitchPtrOffset);
static_assert(sizeof(PesWorldTables) == kWorldHeaderBytes);

// One block per on-pitch slot or official, PES's stride (pes-re-player-record.md 1.2-1.3): the anime record at +0x0, the
// man record at +0x32C, the slot record at +0x10F0. The field offsets below are from each record's own start.
inline constexpr std::size_t kPlayerBlockBytes = 0x3F40;
inline constexpr std::size_t kManRecordOffset = 0x32C;
inline constexpr std::size_t kSlotRecordOffset = 0x10F0;

// Anime record (its bound is the man record's start, +0x32C).
inline constexpr std::size_t kAnimeClassOffset = 0x1;  // u8 class byte; 0x140654E60 halves its reach term on 3
// The anime entry's own pair the Clear and trap chains read (pes-re-record-ids.md 4: on the anime entry, not the slot
// record, so GF's former kSlotLo/Hi names were mis-rooted and had no reader or writer): u16 +0x1A, u16 +0x1C, and the
// f32 at +0x11C that the corridor angle reads (CONFIRMED on-path, seq021). Their writers are OPEN.
inline constexpr std::size_t kAnimeLoOffset = 0x1A;
inline constexpr std::size_t kAnimeHiOffset = 0x1C;
inline constexpr std::size_t kAnimeF11COffset = 0x11C;

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
// The corridor angle also reads these slot-record bytes (CONFIRMED on-path, seq021: +0xA1 and +0xA4; their meaning is OPEN,
// no GF writer yet), and +0x12A4 (FUN_140A58320: a state byte, clamped to 0..4).
inline constexpr std::size_t kSlotByteA1Offset = 0xA1;      // u8
inline constexpr std::size_t kSlotByteA4Offset = 0xA4;      // u8
inline constexpr std::size_t kSlotStateOffset = 0x12A4;     // u8
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
inline void StoreU64(uint8_t* record, std::size_t offset, uint64_t value) {
  std::memcpy(record + offset, &value, sizeof value);
}

// A team record (G's side tables name them). Its attack angle is an f32 at +0x250 (CONFIRMED on-path in the corridor
// angle's side record, seq021: bytes +0x250..+0x253), its attack sign a signed byte at +0x254 (A5Judge 1.2). The danger
// writer 0x140699960 reads the u32 at +0x6D4 (6 answers danger at once), and the unit updater 0x1421019D0 reads the u32 words
// at +0x4C00 + 4 * (slot % 11); the record runs to the last of them.
inline constexpr std::size_t kSideAttackAngleOffset = 0x250;
inline constexpr std::size_t kSideAttackSignOffset = 0x254;
inline constexpr std::size_t kSideModeOffset = 0x6D4;
inline constexpr std::size_t kSideUnitWordsOffset = 0x4C00;
// The carrier walk's reads of the team-info record (G's table B; 0x14063E960 skills, 0x140A3AF90): u32 side id at +0, the
// slot-in-team -> player-info index map at +4 + 4 * slot%11, the player-info array at +0xA8 (stride 0x128) with the u64 skill
// mask at +0x78 of each.
inline constexpr std::size_t kSideIdOffset = 0x0;
inline constexpr std::size_t kSideInfoIndexOffset = 0x4;
inline constexpr std::size_t kSidePlayerInfoOffset = 0xA8;
inline constexpr std::size_t kSidePlayerInfoStride = 0x128;
inline constexpr std::size_t kSideSkillMaskOffset = 0x78;
// The designated taker's slot: u32 at +0x7C of the SIDE record (G's first table, 0x140A48A30; `CMP EDX,[RAX+0x7C]` 0x1421101C7).
inline constexpr std::size_t kSideKickerOffset = 0x7C;
// The unit info's tier word BP+0x60 (0x14063C9F0 stores *0x140A40BD0(side record)): the side record's u32 at +0xA294 unless its
// byte +0xA29C is set (the match setting the menus write, 0x140A424E0; GF's match writes it once).
inline constexpr std::size_t kSideTierOffset = 0xA294;
inline constexpr std::size_t kSideTierFlagOffset = 0xA29C;
inline constexpr std::size_t kSideRecordBytes = 0xA2A0;
struct PesSideRecord {
  std::array<uint8_t, kSideRecordBytes> bytes{};
};
// The team-info record G's second side table names (0x140A489D0, a different object from the team record above): the side id,
// the slot -> info index map and the player-info array (0x28 entries) the walk and the unit info writer read.
inline constexpr int kTeamInfoCount = 0x28;
inline constexpr std::size_t kTeamInfoRecordBytes = kSidePlayerInfoOffset + kTeamInfoCount * kSidePlayerInfoStride;
struct PesTeamInfoRecord {
  std::array<uint8_t, kTeamInfoRecordBytes> bytes{};
};

// The ball record B = *(G+0x40), readable to B+0x2954 (record-layouts-chains.md 2). FUN_140A6CF70 indexes its prediction
// table: sample j (j < 250) at B+0x88+0x28*j, an index past it answering the sample at B+0x2770 (sample 249). A sample
// opens with PES's position (x, height y, z across the pitch); the other 28 bytes of the stride are not decoded.
// FUN_140A57800 is sample 0 (the ball's position); FUN_140A57720 names B+0x2948, a velocity vector (the judge reads x at
// B+0x2948 and z at B+0x2950, pesclock.hpp); +0x4C and +0x80 are the bytes FUN_140A6D0D0 and the angle's ball gate read.
inline constexpr int kBallSampleCount = 250;
inline constexpr std::size_t kBallSamplesOffset = 0x88;
inline constexpr std::size_t kBallSampleStride = 0x28;
inline constexpr std::size_t kBallSampleXOffset = 0x0;
inline constexpr std::size_t kBallSampleYOffset = 0x4;
inline constexpr std::size_t kBallSampleZOffset = 0x8;
inline constexpr std::size_t kBallVelocityOffset = 0x2948;  // f32 x, y (+4), z (+8)
// The other ball-record bytes the corridor angle reads (CONFIRMED on-path in seq021 by flipping each byte; meanings INF/OPEN,
// writers OPEN, so GF holds none of them yet): f32 +0x4 (compared with 3.0: a height), f32 pair +0x1C/+0x24 (a ground
// point), u8 +0x4C (FUN_140A6D0D0), u8 +0x80 (the angle's ball gate), u32 +0x2920 (4 = the penalty hold,
// 0x14063A490).
inline constexpr std::size_t kBallField04Offset = 0x4;
inline constexpr std::size_t kBallPointXOffset = 0x1C;
inline constexpr std::size_t kBallPointZOffset = 0x24;
inline constexpr std::size_t kBallFlag4COffset = 0x4C;
inline constexpr std::size_t kBallFlag80Offset = 0x80;
inline constexpr std::size_t kBallHoldOffset = 0x2920;
inline constexpr std::size_t kBallRecordBytes = 0x2960;
static_assert(kBallSamplesOffset + (kBallSampleCount - 1) * kBallSampleStride == 0x2770, "sample 249 is the +0x2770 fallback");
struct PesBallRecord {
  std::array<uint8_t, kBallRecordBytes> bytes{};
};

// The frame-random record (G+0x360's target) and the pitch record F (G+0x2AA8's target).
struct PesFrameRandomRecord {
  std::array<uint8_t, kFrameRandomBytes> bytes{};
};
struct PesPitchRecord {
  std::array<uint8_t, kPitchRecordBytes> bytes{};
};

// The AI context ctx the carrier units and the corridor angle take (the Clear unit's +8 pointer, one per side): +0x18 is a
// pointer to the side's AI record MA, +0xE0 a pointer to the event store E (CONFIRMED by capture, seq021: ctx+0xE0 held
// 0x7ff5dff60018 and 0x140A3E190(E, 0) returns E+0x180698). The other ctx bytes are not read on the angle's path.
inline constexpr std::size_t kCtxMaOffset = 0x18;
inline constexpr std::size_t kCtxEventsOffset = 0xE0;
inline constexpr std::size_t kCtxBytes = 0x100;
struct PesCtxRecord {
  std::array<uint8_t, kCtxBytes> bytes{};
};

// MA, a side's AI record (docs/re/pes-re-score-rows.md, pes-re-defensive-shape.md): the score rows' blocks (pesscorerows.hpp),
// the ball copy at +0x2482C (x, height, z) and the carrier id u32 at +0x2483C. Its bytes run to the last field the angle
// reads (+0x24850), rounded up. Each sorted block is 200 bytes: two teams' 11 entries of (slot id u32, value f32) at
// +0x58 per team, the rank bytes after them (INF: the count and rank layout past the entries is not read here).
inline constexpr std::size_t kMaLaneListAOffset = 0x229B4;  // list[0] (0x140A714E0 with argument != 1)
inline constexpr std::size_t kMaLaneListBOffset = 0x22B9C;  // list[1] (argument == 1)
inline constexpr std::size_t kMaListTeamStride = 0x58;
inline constexpr int kMaListEntries = 11;
inline constexpr std::size_t kMaBallOffset = 0x2482C;       // f32 x, +4 height, +8 z
inline constexpr std::size_t kMaCarrierIdOffset = 0x2483C;  // u32
// The ball owner's slot id (u32, 0xFF none): 0x140A627E0(ctx, 0) answers it to the carrier walk, which compares its side with BP+0x24.
inline constexpr std::size_t kMaOwnerOffset = 0x2601C;
inline constexpr std::size_t kMaBytes = 0x26040;
// No carrier in MA+0x2483C (INF: PES's no-holder word is not decoded; the readers gate on id < 0x16).
inline constexpr uint32_t kMaNoCarrier = 0xFFFFFFFFu;
struct PesMaRecord {
  std::array<uint8_t, kMaBytes> bytes{};
};

// A side's carrier block BP (the unit set's per-carrier object, PES offsets; docs: pes-port-MatchInfo4.md 7.1). The danger
// builder 0x142119390 takes rcx = imageA (BP+0xEC), r8 = the unit info (BP+0x1C: u32 slot at +0, u32 slot % 11 at +4, u32 side
// at +8, f32 attack heading at +0x10, f32 attack sign at +0x14, the carrier's ground position (x, y, z) at +0x18 and the same
// times the sign at +0x24), r9 = the state (BP+0x78: the ball copy (x, height, z) at +0x40, owned byte at +0x6C) and the u32
// flags BP+0x18 as its sixth argument. The pressure builder 0x14069A500 and the danger writer 0x140699960 are called from its
// tail on imageA+0x2124, and the writer's byte lands at imageA+0x31C1 (the danger flag the carrier units test: 0 or 1, live 5/5).
// The Clear unit 0x142109EB0 is a separate object (r8 = the unit; the answer record is rdx): unit+0 the VA of the unit info (its
// first u32 is the carrier's slot id, the angle's rdx), unit+8 the VA of ctx (the angle's rcx), unit+0x28 the VA of imageA. It
// stores its answer as the u32 type 5 at answer+0 and the corridor angle (an f32, degrees) at answer+8, and writes nothing
// while the danger byte is clear.
inline constexpr std::size_t kBpFlagsOffset = 0x18;
inline constexpr std::size_t kBpUnitInfoOffset = 0x1C;
inline constexpr std::size_t kBpStateOffset = 0x78;
inline constexpr std::size_t kBpImageAOffset = 0xEC;
inline constexpr std::size_t kBpBytes = 0x4300;  // the walk's list ends at BP+0x4158, its previous-answer words at BP+0x4214
inline constexpr std::size_t kImageADangerOffset = 0x31C1;
inline constexpr std::size_t kImageAPressureOffset = 0x2124;
// The unit info's fields (offsets from BP+0x1C) and the state's (from BP+0x78).
inline constexpr std::size_t kUnitSlotOffset = 0x0;
inline constexpr std::size_t kUnitSlotInTeamOffset = 0x4;
inline constexpr std::size_t kUnitSideOffset = 0x8;
inline constexpr std::size_t kUnitHeadingOffset = 0x10;
inline constexpr std::size_t kUnitSignOffset = 0x14;
inline constexpr std::size_t kUnitPositionOffset = 0x18;
inline constexpr std::size_t kUnitSignedPositionOffset = 0x24;
inline constexpr std::size_t kStateBallOffset = 0x40;
inline constexpr std::size_t kStateOwnedOffset = 0x6C;
inline constexpr std::size_t kUnitObjectBytes = 0x40;
inline constexpr std::size_t kUnitObjectOffset = kBpBytes;
inline constexpr std::size_t kClearAnswerOffset = kUnitObjectOffset + kUnitObjectBytes;  // u32 type at +0, f32 angle at +8
inline constexpr std::size_t kUnitCarrierOffset = 0x0;
inline constexpr std::size_t kUnitCtxOffset = 0x8;
inline constexpr std::size_t kUnitImageAOffset = 0x28;
inline constexpr std::size_t kAnswerAngleOffset = 0x8;
inline constexpr std::size_t kAnswerBytes = 0x10;
inline constexpr std::size_t kUnitMarkOffset = 0xC;  // u32: slot -> player-info index (0x14063C9F0 stores it from the team-info map)
inline constexpr uint32_t kBpRefreshAll = 0x1F;      // BP+0x18 in every capture
// The carrier walk 0x14210F110 (rcx = BP, rdx = the unit object {unit info VA, ctx VA}; the chosen kind comes back in eax, 0x19 =
// none). Its BP reads: the side (unit info +8) at +0x24, the tier word +0x60 (0x140A604D0: game-setting tier * 100), the ball copy
// (x, height, z) at +0xB8, the state byte +0xC8, the owned byte +0xE4, the zone record at +0xAB8 (0x140699820 reads its +0x16C,
// +0x170, +0x178, +0x17C), the team intention word +0x32B8 (0x141052E30), the turn reference angle +0x3380 (0x140481B00), the
// previous chosen kind +0x41E4, and the priority list at +0x3E20: u32 count, u32 kinds, signed rank bytes (-1 unlisted) at
// +0x68 + kind, 0x1C-byte answer records at +0x84 + 0x1C * kind.
inline constexpr std::size_t kBpSideOffset = 0x24;
inline constexpr std::size_t kBpTierOffset = 0x60;
inline constexpr std::size_t kBpDecidedOffset = 0xC8;
inline constexpr std::size_t kBpZoneOffset = 0xAB8;
inline constexpr std::size_t kZoneFlagsOffset = 0x16C;   // u32: bit 0 = close
inline constexpr std::size_t kZoneDepthOffset = 0x170;   // f32: negative = not close
inline constexpr std::size_t kZoneRowOffset = 0x178;     // s32
inline constexpr std::size_t kZoneColumnOffset = 0x17C;  // s32
inline constexpr std::size_t kBpGuardOffset = 0x32AD;
inline constexpr std::size_t kBpIntentionOffset = 0x32B8;
inline constexpr std::size_t kBpTurnReferenceOffset = 0x3380;
inline constexpr std::size_t kBpPlanOffset = 0x32B0;         // the carrier plan record (its path word is kBpIntentionOffset)
inline constexpr std::size_t kBpManagerOffset = 0x32C0;      // the carrier manager 0x14211D710 steps (+0xC0 = kBpTurnReferenceOffset)
inline constexpr std::size_t kBpProposalOffset = 0x4204;     // the dribble-proposal record (type word, id, +0xC target, +0x10 heading)
// 0x14211D710's call frame: the return address at rsp, then past the 0x20 shadow the fifth to eighth arguments.
inline constexpr std::size_t kTurnArg5Offset = 0x28;
inline constexpr std::size_t kTurnArg6Offset = 0x30;
inline constexpr std::size_t kTurnArg7Offset = 0x38;
inline constexpr std::size_t kTurnArg8Offset = 0x40;
inline constexpr std::size_t kTurnFrameBytes = 0x80;
// The unit driver 0x142110D30's step input S (rdx; the same object 0x142110C90 reads: S+4 = the unit slot, S+0x38 = the side's ctx)
// and the 72-byte step object 0x14211ED10 builds from it (the kind objects' virtual step reads it). CONFIRMED on the seq16184 capture
// (S at 0x1e00ACF8: slot 19, S+0x30 = BP+0x4258, S+0x38 = ctx). GF's S and step object live at the top of the window's stack.
inline constexpr std::size_t kStepInputBytes = 0x40;
inline constexpr std::size_t kStepInputSlotOffset = 0x4;
inline constexpr std::size_t kStepInputOutputOffset = 0x30;
inline constexpr std::size_t kStepInputCtxOffset = 0x38;
inline constexpr std::size_t kStepObjectBytes = 0x48;
inline constexpr std::size_t kBpStepOutputOffset = 0x4258;  // S+0x30: the output-buffer record, inside the BP block
inline constexpr std::size_t kBpAdoptedOffset = 0x41E8;     // the adopted answer's copy (0x1C bytes: the proposal's, BP+0x4204..)
inline constexpr std::size_t kProposalRecordBytes = 0x1C;
inline constexpr std::size_t kStepScratchBytes = 0x90;     // the step object and S, at the top of the stack (16-aligned)
inline constexpr std::size_t kSlotClassWordOffset = 0x14;  // the slot record's word 0x142110D30 tests with 0x140A6E320
inline constexpr uint32_t kProposalKindTaker = 0x13;        // 0x142110D30: the proposal kind when 0x140A638C0(ctx, slot) holds, else kBpDefaultKind
inline constexpr std::size_t kBpListOffset = 0x3E20;
inline constexpr std::size_t kListRankOffset = 0x68;
inline constexpr std::size_t kListRecordOffset = 0x84;
inline constexpr std::size_t kListRecordStride = 0x1C;
inline constexpr std::size_t kListBytes = kListRecordOffset + 0x19 * kListRecordStride;
inline constexpr std::size_t kBpChosenKindOffset = 0x41E4;
inline constexpr uint32_t kBpDefaultKind = 0x10;  // 0x142110F88: the Dribble default the driver stores while the state's word 0 is 0
inline constexpr int kWalkKindCount = 0x19;
inline constexpr uint32_t kWalkNoKind = 0x19;
inline constexpr uint8_t kRankUnlisted = 0xFF;
inline constexpr int kBpTierLive = 500;  // tier 5, zero ripeness delay (docs/re/pes-re-A4Carrier.md 3)
// The skill bits the walk tests in the carrier's mask (0x14210F110: 0x14063E960(ctx, slot, bit)).
inline constexpr int kSkillOneTouchShot = 0xB;
inline constexpr int kSkillOneTouchPass = 0xC;
inline constexpr int kSkillPinpointCross = 0xE;
// The builder's call frame (0x142110D30's call): the return address at rsp, the 5th and 6th arguments past the 0x20 shadow.
inline constexpr std::size_t kDangerArg5Offset = 0x28;
inline constexpr std::size_t kDangerArg6Offset = 0x30;
inline constexpr std::size_t kDangerFrameBytes = 0x40;
struct PesCarrierBlock {
  std::array<uint8_t, kClearAnswerOffset + kAnswerBytes> bytes{};
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

// The image's layout after the header: the blocks, the match-info block, the team records, the ball record, the pitch and
// frame-random records, the scratch and the stack.
inline constexpr std::size_t kMatchInfoOffset = kWorldHeaderBytes + static_cast<std::size_t>(kBlockCount) * kPlayerBlockBytes;
inline constexpr std::size_t kSidesOffset = kMatchInfoOffset + kMatchInfoBytes;
inline constexpr std::size_t kTeamInfosOffset = kSidesOffset + static_cast<std::size_t>(kSideCount) * kSideRecordBytes;
inline constexpr std::size_t kBallOffset = kTeamInfosOffset + static_cast<std::size_t>(kSideCount) * kTeamInfoRecordBytes;
inline constexpr std::size_t kPitchOffset = kBallOffset + kBallRecordBytes;
inline constexpr std::size_t kFrameRandomOffset = kPitchOffset + kPitchRecordBytes;
inline constexpr std::size_t kCtxOffset = kFrameRandomOffset + kFrameRandomBytes;
inline constexpr std::size_t kMaOffset = kCtxOffset + static_cast<std::size_t>(kSideCount) * kCtxBytes;
inline constexpr std::size_t kEventsOffset = kMaOffset + static_cast<std::size_t>(kSideCount) * kMaBytes;
inline constexpr std::size_t kCarrierOffset = kEventsOffset + sizeof(EventStoreImage);
inline constexpr std::size_t kTrapOffset = kCarrierOffset + static_cast<std::size_t>(kSideCount) * sizeof(PesCarrierBlock);
inline constexpr std::size_t kScratchOffset = kTrapOffset + sizeof(PesTrapImage);
inline constexpr std::size_t kStackOffset = kScratchOffset + kScratchBytes;
inline constexpr uint64_t kScratchVa = kWorldWindowVa + kScratchOffset;
inline constexpr uint64_t kStackVa = kWorldWindowVa + kStackOffset;

// The match-info block (PES's MI): the period record the clock writes in place (pesclock.hpp), at its PES offset, and the
// frame counter after it.
inline constexpr std::size_t kMiFrameOffset = 0x1198;  // u32: the logic-frame counter (0x140A128E4 increments it)
struct PesMatchInfo {
  std::array<uint8_t, kMatchInfoPeriodOffset> reserved_head{};  // MI+0x0 .. MI+0x1168: no GF writer yet
  PeriodRecord period{};                                        // MI+0x1168: PES's period record
  uint32_t frame = 0;                                           // MI+0x1198: the frame counter
  std::array<uint8_t, kMatchInfoBytes - kMatchInfoPeriodOffset - sizeof(PeriodRecord) - sizeof(uint32_t)> reserved_tail{};
};
static_assert(offsetof(PesMatchInfo, period) == kMatchInfoPeriodOffset);
static_assert(offsetof(PesMatchInfo, frame) == kMiFrameOffset);
static_assert(sizeof(PesMatchInfo) == kMatchInfoBytes);

// The whole window: G's header, then the blocks. Heap-owned by PesWorld, so the bytes never move.
struct PesWorldImage {
  PesWorldTables tables;
  std::array<PesPlayerBlock, kBlockCount> blocks{};
  PesMatchInfo match{};  // MI: the match-info block (MatchInfoVa)
  std::array<PesSideRecord, kSideCount> sides{};
  std::array<PesTeamInfoRecord, kSideCount> teamInfos{};  // the team-info records (G's side table B)
  PesBallRecord ball{};
  PesPitchRecord pitch{};
  PesFrameRandomRecord frameRandom{};
  std::array<PesCtxRecord, kSideCount> ctx{};  // each side's AI context (ctx+0x18 -> MA, ctx+0xE0 -> E)
  std::array<PesMaRecord, kSideCount> ma{};    // each side's AI record
  EventStoreImage events{};                    // E: the event store, PES's offsets (pesevents.hpp)
  std::array<PesCarrierBlock, kSideCount> carrier{};  // each side's carrier block BP, its Clear unit object and answer
  PesTrapImage trap{};                                    // P, the Trap action's owner (pestrapworld.hpp)
  std::array<uint8_t, kScratchBytes> scratch{};  // the translated bearing's points (kScratchVa)
  std::array<uint8_t, kStackBytes> stack{};      // the translated stoppage functions' stack (kStackVa)
};
static_assert(offsetof(PesWorldImage, blocks) == kWorldHeaderBytes);
static_assert(offsetof(PesWorldImage, match) == kMatchInfoOffset);
static_assert(offsetof(PesWorldImage, sides) == kSidesOffset);
static_assert(offsetof(PesWorldImage, teamInfos) == kTeamInfosOffset);
static_assert(offsetof(PesWorldImage, ball) == kBallOffset);
static_assert(offsetof(PesWorldImage, pitch) == kPitchOffset);
static_assert(offsetof(PesWorldImage, frameRandom) == kFrameRandomOffset);
static_assert(offsetof(PesWorldImage, ctx) == kCtxOffset);
static_assert(offsetof(PesWorldImage, ma) == kMaOffset);
static_assert(offsetof(PesWorldImage, events) == kEventsOffset);
static_assert(offsetof(PesWorldImage, carrier) == kCarrierOffset);
static_assert(offsetof(PesWorldImage, trap) == kTrapOffset);
static_assert(offsetof(PesWorldImage, scratch) == kScratchOffset);
static_assert(offsetof(PesWorldImage, stack) == kStackOffset);

// What the danger builder reads outside GF's window (tests: the capture's heap bytes) and the byte log of its writes.
struct PesDangerExternals {
  std::vector<PesTrapSegment> segments;
  std::vector<std::pair<uint64_t, uint8_t>>* writeLog = nullptr;
};

inline constexpr std::size_t kWorldBytes = sizeof(PesWorldImage);

class PesWorld {
 public:
  PesWorld() : image_(std::make_unique<PesWorldImage>()) {
    image_->tables.ball = BallVa();
    image_->tables.match = MatchInfoVa();
    image_->tables.frameRandom = FrameRandomVa();
    image_->tables.pitch = PitchVa();
    for (int team = 0; team < kSideCount; ++team) {
      SetEntry(WorldTable::kSideA, team, SideVa(team));
      SetEntry(WorldTable::kSideB, team, TeamInfoVa(team));
    }
    for (int side = 0; side < kSideCount; ++side) {
      uint8_t* unit = UnitObject(side);
      StoreU64(unit, kUnitCarrierOffset, CarrierVa(side) + kBpUnitInfoOffset);
      StoreU64(unit, kUnitCtxOffset, CtxVa(side));
      StoreU64(unit, kUnitImageAOffset, CarrierVa(side) + kBpImageAOffset);
    }
    for (int side = 0; side < kSideCount; ++side) {
      StoreU64(Ctx(side), kCtxMaOffset, MaVa(side));
      StoreU64(Ctx(side), kCtxEventsOffset, EventsVa());
    }
  }
  PesWorld(const PesWorld&) = delete;
  ~PesWorld();
  PesWorld& operator=(const PesWorld&) = delete;

  // The window's bytes: PesMem::window maps them at kWorldWindowVa for kWorldBytes.
  uint8_t* Bytes() { return reinterpret_cast<uint8_t*>(image_.get()); }

  // The VAs a slot's three records sit at (a table entry names one of these once bound). Officials use the same stride.
  static constexpr uint64_t BlockVa(int slot) {
    return kWorldWindowVa + kWorldHeaderBytes + static_cast<uint64_t>(slot) * kPlayerBlockBytes;
  }

  // The VA of the match-info block: G+0x270 names it.
  static constexpr uint64_t MatchInfoVa() { return kWorldWindowVa + kMatchInfoOffset; }
  static constexpr uint64_t TeamInfoVa(int side) {
    return kWorldWindowVa + kTeamInfosOffset + static_cast<uint64_t>(side) * kTeamInfoRecordBytes;
  }
  static constexpr uint64_t SideVa(int side) {
    return kWorldWindowVa + kSidesOffset + static_cast<uint64_t>(side) * kSideRecordBytes;
  }
  static constexpr uint64_t BallVa() { return kWorldWindowVa + kBallOffset; }
  static constexpr uint64_t PitchVa() { return kWorldWindowVa + kPitchOffset; }
  static constexpr uint64_t FrameRandomVa() { return kWorldWindowVa + kFrameRandomOffset; }
  // The side's AI context, its AI record MA, and the event store E (kEventNewestOffset etc. are offsets into E).
  static constexpr uint64_t CtxVa(int side) { return kWorldWindowVa + kCtxOffset + static_cast<uint64_t>(side) * kCtxBytes; }
  static constexpr uint64_t MaVa(int side) { return kWorldWindowVa + kMaOffset + static_cast<uint64_t>(side) * kMaBytes; }
  static constexpr uint64_t EventsVa() { return kWorldWindowVa + kEventsOffset; }
  uint8_t* Ctx(int side) { return image_->ctx[side].bytes.data(); }
  uint8_t* Ma(int side) { return image_->ma[side].bytes.data(); }
  EventStoreImage& EventImage() { return image_->events; }
  // E as the store's own view: every event the match routes lands in the window.
  EventStore Events() { return EventStore(image_->events); }
  // The block's state is already live (a test that loaded a capture's BP): RunCarrierUpdate continues it instead of resetting.
  void AdoptCarrierState(int side) { stateReady_[side] = constructed_[side] = true; }
  // A side's carrier block BP (kBp* offsets) and the Clear unit object beside it (the unit's r8).
  static constexpr uint64_t CarrierVa(int side) {
    return kWorldWindowVa + kCarrierOffset + static_cast<uint64_t>(side) * sizeof(PesCarrierBlock);
  }
  uint8_t* Carrier(int side) { return image_->carrier[side].bytes.data(); }
  uint8_t* UnitObject(int side) { return Carrier(side) + kUnitObjectOffset; }
  // imageA+0x31C1, the danger flag the Clear unit tests (and the Long, BackSpace and Safety units read).
  bool ClearDanger(int side) { return Carrier(side)[kBpImageAOffset + kImageADangerOffset] != 0; }
  // Pokes the flag (the Clear unit's tests: its answer depends on nothing else but the byte).
  void SetClearDanger(int side, bool danger) { Carrier(side)[kBpImageAOffset + kImageADangerOffset] = danger ? 1 : 0; }
  // Runs PES's unit builder 0x142119390 on the side's carrier block (rcx = imageA, rdx = the side's ctx, r8 = the unit info,
  // r9 = the state, the BP flags as the sixth argument): it fills imageA's lists and pressure record and stores the danger
  // byte 0x31C1 from the pressure builder 0x14069A500 and the writer 0x140699960 (its tail rule may set it too). Allocates
  // through PES's allocator on this world's PesHeap. `ext` carries bytes outside the window (tests); returns the danger byte.
  bool RunDangerBuilder(int side, const PesDangerExternals& ext = {});
  // One list entry of the carrier walk: the kind and its 0x1C-byte answer record (the unit's output).
  struct WalkEntry {
    uint32_t kind = 0;
    std::array<uint8_t, kListRecordStride> record{};
  };
  // Writes the walk's priority list into the side's BP (count, kinds, ranks = list position, the records at their kind's slot;
  // every unlisted record is zeroed: type 0 answers nothing).
  void SetUnitList(int side, const std::vector<WalkEntry>& list);
  // The walk's reads of the carrier's profile: the skill mask in the team-info record (the side id and the slot-in-team map are
  // identities of the window), the designated taker in the side record, and the carrier's height stat byte 0x31 (cm).
  void SetCarrierProfile(int side, uint32_t slot, uint64_t skillMask, uint8_t heightStatCm);
  // The unit updater's two record writers, in the driver's order (0x142110C90): the unit info BP+0x1C by 0x14063C9F0(unit info,
  // ctx, slot) and the state BP+0x78 by 0x14063A490(state, ctx, unit info), both on the window's own records (side, man, anime,
  // slot, MA, MI, ball, events). The state keeps counters across calls (tick count, owned ticks), so the block must persist.
  void RunCarrierUpdate(int side, uint32_t slot, const PesDangerExternals& ext = {});
  // Runs PES's carrier walk 0x14210F110 on the side's BP (rdx = its unit object): the chosen kind, or kWalkNoKind. `ext` carries
  // bytes outside the window (tests); allocates through the world's PesHeap like the builder.
  uint32_t RunWalk(int side, const PesDangerExternals& ext = {});
  // PES's carrier-block constructor 0x14210D320(BP, side) (proved bit-exact on three live BPs, results_dribble.json): the unit
  // info, state, imageA's lists, the carrier manager at BP+0x32C0 with its nine step objects, and the 25 unit-kind objects, vtables
  // included. Production does not run it yet (RunCarrierUpdate's ponytail); a test that loaded a capture's BP calls AdoptCarrierState.
  void RunCarrierConstruct(int side, const PesDangerExternals& ext = {});
  // PES's turn-reference builder 0x14211D710(BP+0x32C0, ctx, unit info, state, imageA, BP+0x32B0, side record, BP+0x4204): while the
  // state's word 0 is 0 it ticks the carrier manager (0x14211E2D0, 0x14211E210) and writes BP+0x3380, the angle the walk's Clear
  // gate (0x14210F4E8) measures the dribble proposal against. Call after RunDangerBuilder and the plan word BP+0x32B8
  // (RunDribbleProposal does).
  void RunTurnReference(int side, const PesDangerExternals& ext = {});
  // The unit driver 0x142110D30's proposal chain, in its order: RunTurnReference; the step object 0x14211ED10 builds (the step input S
  // lives at the top of the window's stack); while the state's word 0 is 0 the proposal writer 0x1421102D0(BP, kind, step object,
  // BP+0x4204) with kind 0x10 (0x13 when 0x140A638C0(ctx, slot) holds); then the copy BP+0x41E8 <- BP+0x4204 (0x1C bytes) and the
  // previous-kind word BP+0x41E4 = 0x10. BP+0x4204 is the dribble-proposal record the carrier walk's Clear gate reads.
  void RunDribbleProposal(int side, uint32_t slot, const PesDangerExternals& ext = {});
  // The install's own table bytes (pesimagetables.hpp) every window run reads at register-held image addresses. Required before
  // RunDangerBuilder in a real match: without them PES's anime-class gate and sine table read zeros.
  void BindImageTables(const PesImageTables& tables) { imageTables_ = tables; }
  // Runs the Clear unit 0x142109EB0 on the window for `carrierSlot` of `side`: the answer type (5 when the danger byte is
  // set, else 0: the unit stores nothing) and the corridor angle it stored (degrees, 0.0 when none).
  struct ClearAnswer {
    uint32_t type = 0;
    float angle_deg = 0.0f;
  };
  ClearAnswer RunClearUnit(int side, uint32_t carrierSlot);
  // P, the trap chain's actor object (kPActionOffset is its action). Its window VA is rdx of the chain.
  static constexpr uint64_t PVa() { return kWorldWindowVa + kTrapOffset; }
  uint8_t* P() { return image_->trap.p.data(); }
  // PES's Trap action's kind-0 chain 0x14079A470 on the window, as the action's +0x68 dispatcher (0x1407A01C0) calls it for
  // kind <= 0: the dispatcher's two action stores, its frame (the request's first word, the out word, the stack arguments), then the
  // chain. `ext` carries what GF does not hold as state yet (pestrapworld.hpp).
  PesTrapResult RunTrapKind0(const PesTrapRequest& request, const PesTrapExternals& ext);
  // MA's ball copy (x, height, z) and carrier id.
  void SetMaBall(int side, float x, float height, float z) {
    StoreF32(Ma(side), kMaBallOffset, x);
    StoreF32(Ma(side), kMaBallOffset + 4, height);
    StoreF32(Ma(side), kMaBallOffset + 8, z);
  }
  void SetMaCarrier(int side, uint32_t id) { StoreU32(Ma(side), kMaCarrierIdOffset, id); }
  // The ball owner's slot id in MA (0xFF none).
  void SetMaOwner(int side, uint32_t id) { StoreU32(Ma(side), kMaOwnerOffset, id); }
  // Entry `rank` of team `team`'s list in MA list block `list` (0 or 1): the slot id, then the value.
  void SetMaListEntry(int side, int list, int team, int rank, uint32_t id, float value) {
    uint8_t* entry = Ma(side) + (list == 0 ? kMaLaneListAOffset : kMaLaneListBOffset) +
                     static_cast<std::size_t>(team) * kMaListTeamStride + static_cast<std::size_t>(rank) * 8;
    StoreU32(entry, 0, id);
    StoreF32(entry, 4, value);
  }
  // The clock's period record, in the match-info block: PesClock writes it in place.
  PeriodRecord& Period() { return image_->match.period; }
  // The match-info block's bytes, at PES's offsets (the kMi* constants).
  uint8_t* MatchInfo() { return reinterpret_cast<uint8_t*>(&image_->match); }

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
  // The ball's prediction sample j in PES's coordinates (x along the pitch, y the height, z across): B+0x88+0x28*j.
  // FUN_140A6CF70 clamps an index past the table to sample 249.
  void SetBallSample(int j, float x, float y, float z) {
    uint8_t* sample = image_->ball.bytes.data() + kBallSamplesOffset + static_cast<std::size_t>(j) * kBallSampleStride;
    StoreF32(sample, kBallSampleXOffset, x);
    StoreF32(sample, kBallSampleYOffset, y);
    StoreF32(sample, kBallSampleZOffset, z);
  }
  void BallSample(int j, float out[3]) const {
    const uint8_t* sample = image_->ball.bytes.data() + kBallSamplesOffset + static_cast<std::size_t>(j) * kBallSampleStride;
    out[0] = LoadF32(sample, kBallSampleXOffset);
    out[1] = LoadF32(sample, kBallSampleYOffset);
    out[2] = LoadF32(sample, kBallSampleZOffset);
  }
  // The ball's velocity vector at B+0x2948 (x, y, z).
  void SetBallVelocity(float x, float y, float z) {
    uint8_t* ball = image_->ball.bytes.data();
    StoreF32(ball, kBallVelocityOffset, x);
    StoreF32(ball, kBallVelocityOffset + 4, y);
    StoreF32(ball, kBallVelocityOffset + 8, z);
  }
  void BallVelocity(float out[3]) const {
    const uint8_t* ball = image_->ball.bytes.data();
    out[0] = LoadF32(ball, kBallVelocityOffset);
    out[1] = LoadF32(ball, kBallVelocityOffset + 4);
    out[2] = LoadF32(ball, kBallVelocityOffset + 8);
  }
  // The frame counter MI+0x1198: PES's 0x140A128E4 increments it once a logic frame.
  uint32_t MatchFrame() const { return image_->match.frame; }
  void AdvanceMatchFrame() { ++image_->match.frame; }
  // The pitch record F: F[k] at +4k.
  void SetPitchField(int k, float value) {
    StoreF32(image_->pitch.bytes.data(), static_cast<std::size_t>(k) * sizeof(float), value);
  }
  // The team record's attack angle (+0x250, degrees) and sign (+0x254, s8: +1 when the team attacks +x, pes-re-ref-record.md 3.3).
  void SetAttackAngle(int team, float degrees) { StoreF32(image_->sides[team].bytes.data(), kSideAttackAngleOffset, degrees); }
  // Host bytes of the records the translated code reads through G (tests and writers beside this file).
  uint8_t* BallRecord() { return image_->ball.bytes.data(); }
  uint8_t* PitchRecord() { return image_->pitch.bytes.data(); }
  uint8_t* FrameRandomRecord() { return image_->frameRandom.bytes.data(); }
  uint8_t* SideRecord(int team) { return image_->sides[team].bytes.data(); }
  uint8_t* TeamInfoRecord(int team) { return image_->teamInfos[team].bytes.data(); }
  // The match setting the unit info's tier word reads (the side record's +0xA294; the flag byte clear).
  void SetSideTier(int team, uint32_t tier) {
    StoreU32(SideRecord(team), kSideTierOffset, tier);
    SideRecord(team)[kSideTierFlagOffset] = 0;
  }
  // Points a translated call at the window: the window itself, the image global that holds G, and the window's stack.
  // A caller that needs objects beside G (the corridor angle's ctx) adds its own segments afterwards.
  void BindCall(::PesState& call);
  // The team record's attack sign (+0x254, s8): +1 when the team attacks +x.
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
  std::unique_ptr<PesHeap> heap_;  // PES's allocator on GF memory, made by the first RunDangerBuilder; the BP lists live in it
  std::array<bool, kSideCount> stateReady_{};  // the side's BP state has been constructed (RunCarrierUpdate)
  std::array<bool, kSideCount> constructed_{};  // the side's BP was built by PES's constructor 0x14210D320 (RunCarrierConstruct)
  PesImageTables imageTables_;     // the install's table bytes the window runs read at register-held addresses (BindImageTables)
};

}  // namespace pes
