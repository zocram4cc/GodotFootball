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

struct PesImageTables {
  std::vector<PesTrapSegment> segments;
};

// Fails (nullopt, `error` set) when a range is unreadable or the frame cells disagree with their initialiser's float.
std::optional<PesImageTables> LoadPesImageTables(const PeImage& exe, std::string* error);

}  // namespace pes
