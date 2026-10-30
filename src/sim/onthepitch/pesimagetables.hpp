// The install's own image bytes the translated functions read through a register-held address, so no build-time bake covers
// them: the translator bakes a constant only where it can bound the index, and a call on GF's world window maps no image.
// Without these the window run reads zeros where PES read its tables, and PES's lane lists (imageA+0x2124..0x31C0) differ
// (MatchInfo6: man 2's anime class gate, seq4046 and seq4891). Read from the install's PES2021.exe at load, never transcribed.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pestrapworld.hpp"

namespace pes {

// The anime class table: one u32 per anime id, valid for ids 0..0x5D (pes-re-Dribble-inventory.md). The predicate 0x140A6E3C0
// indexes it with the sign-extended class byte (`MOVSX RAX,byte [RCX+1]` 0x140A6E3CE, `CMP EAX,0x5E` 0x140A6E3D3, `JL`
// 0x140A6E3D6, `MOV EAX,[RCX+RAX*4]` 0x140A6E3E2): an id below zero reads the bytes before the table, down to -128 entries.
inline constexpr uint64_t kAnimeClassTableVa = 0x1426484F0;
inline constexpr uint64_t kClassTableEntries = 0x5E;
inline constexpr uint64_t kAnimeClassBack = 0x80;
// The quarter-wave sine table 0x140A92C80 reads: sin(i * 90 / 512 deg), 513 floats (pes-re-chance-lanes.md).
inline constexpr uint64_t kQuarterSineVa = 0x142655950;
inline constexpr uint64_t kQuarterSineCount = 513;
// The frame-rate cells FUN_141E532F0 writes at start-up: the int 54 at +0 (read by 0x141E532E0) and the float 54.0 at +4 (read
// by 0x141E532D0, copied from 0x14259D624). The install's .data holds the initialised pair.
inline constexpr uint64_t kFrameCellsVa = 0x14351AE50;
inline constexpr uint64_t kFrameCellsBytes = 8;
inline constexpr uint64_t kFrameFloatSourceVa = 0x14259D624;
// The vtables of the carrier block's objects (0x14210D320 stores their VAs: the block 0x142BEF090, the plan 0x142BEED60, the 25 unit
// kinds 0x142BEED90..0x142BEF..., the carrier manager 0x142BEFFE8 and its nine step objects, the last at 0x142BF0270 + 0x48). The
// virtual calls of 0x14211D710 and 0x1421102D0 read their slots from the image's read-only data, so the window binds the span.
inline constexpr uint64_t kBpVtablesVa = 0x142BEED00;
inline constexpr uint64_t kBpVtablesBytes = 0x142BF0300 - kBpVtablesVa;
// The tier table 0x140A603C0 reads for the carrier walk's capability gate 0x140A604D0 (BP+0x60 = tier * 100): 0x22 rows (the
// `0x21 < param_2` bound) of 0x20 bytes, row r at 0x142645A40 + 0x20 * r (a flag byte, then seven f32 per tier, `+0x20 * r` in the
// `&DAT_142645a44 + (tier + r * 8) * 4` reads).
inline constexpr uint64_t kTierTableVa = 0x142645A40;
inline constexpr uint64_t kTierRows = 0x22;
inline constexpr uint64_t kTierRowBytes = 0x20;
// The event store's none record: the global every failed event query returns (`LEA RAX,[0x1436F3A40]` 0x140A3E97C in 0x140A3E940;
// the same address in 0x140A3E5D0's tail and 0x140A3E190's out-of-range arm), 0x320 bytes (`IMUL RAX,RAX,0x320` 0x140A3E971, the
// slot stride). It lies in .data's zero-initialised tail (past the exe's raw bytes), so the install holds no bytes for it: PES
// builds it at start-up with the event record's constructor 0x140A3CDD0, called by the static initialiser at 0x1401C2944
// (`LEA RCX,[0x1436F3A40]`, `CALL 0x140A3CDD0`). The constructor zeroes the record (memset, size `MOV R8D,0x320` 0x140A3CDD8) and
// stores the kind word 0x37 (`MOV dword ptr [RBX],0x37` 0x140A3CDF9) and the word +0x10 (`MOV dword ptr [RBX+0x10],0xB`
// 0x140A3CDE8); the stores are read from the exe, opcode-checked, when the tables load.
inline constexpr uint64_t kEventNoneRecordVa = 0x1436F3A40;
inline constexpr uint64_t kEventNoneRecordBytes = 0x320;
inline constexpr uint64_t kEventCtorKindStoreVa = 0x140A3CDF9;
inline constexpr uint64_t kEventCtorWordStoreVa = 0x140A3CDE8;
inline constexpr std::size_t kEventCtorWordOffset = 0x10;

struct PesImageTables {
  std::vector<PesTrapSegment> segments;
};

// Fails (nullopt, `error` set) when a range is unreadable or the frame cells disagree with their initialiser's float.
std::optional<PesImageTables> LoadPesImageTables(const PeImage& exe, std::string* error);

}  // namespace pes
