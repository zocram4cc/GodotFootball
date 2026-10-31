#pragma once

// PES 2021 EDIT save (EDIT00000000): decryption and the per-player appearance
// records. Read-only; layout and cipher in docs/re/pes-kit-assembly.md 8.1.
// The save path arrives at run time only (PES21_EDIT_SAVE), never as a default.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "pes/uniform.hpp"

namespace pes {

std::array<uint8_t, 64> Sha512(std::span<const uint8_t> data);

// MT19937 seeded with init_by_array, as the game's FUN_1414136a0.
class Mt19937 {
 public:
  explicit Mt19937(const std::vector<uint32_t>& key);
  uint32_t Next();

 private:
  std::array<uint32_t, 624> state_{};
  size_t index_ = 624;
};

// FUN_14140e0f0: MT keystream mixed with four rotated earlier draws; its own
// inverse.
std::vector<uint8_t> CryptStream(const std::array<uint8_t, 64>& key, std::span<const uint8_t> in);

inline constexpr size_t kAppearanceRecordBytes = 72;
using AppearanceRecord = std::array<uint8_t, kAppearanceRecordBytes>;
using AppearanceTable = std::unordered_map<uint32_t, AppearanceRecord>;

// One 0x138-byte player record of the save's data block. NOT Player.bin's layout (the id is the u32 at +0, abilities are plain
// 7-bit values with no +40 bias): the offsets below are the fields PES's roster row carries into a match, fitted on 34 players
// captured live (tools/dev/rendezvous/roster_rows.py output; docs/ports/pes-port-DribbleProposal7.md 1). Height is also the
// byte docs/re/pes-re-physique.md 1.1 read.
inline constexpr size_t kEditPlayerRecordBytes = 0x138;
using EditPlayerRecord = std::array<uint8_t, kEditPlayerRecordBytes>;
inline constexpr size_t kEditIdOffset = 0;
inline constexpr size_t kEditHeightOffset = 0x0A;     // u8 cm, the roster row's block byte 0x31
inline constexpr size_t kEditStrongerFootBit = 381;   // 1 bit, 1 = left, block byte 0x34
inline constexpr size_t kEditAbilityBits = 7;
// The 25 ability fields sit at bits 112 119 128 135 142 149 160 167 174 181 192 199 206 213 224 231 238 245 | 256 288 295 302 320 358 365;
// the roster row's ability bytes are 0x14..0x25 (18) then 0x27..0x2D (7), so the split lands on bit 245 | 256 and the fields run in
// bit order (INF: every one of the save's 4830 players has all 25 equal, so no data tells the order apart). Speed is block byte 0x27,
// Jump 0x2C.
inline constexpr size_t kEditSpeedBit = 256;
inline constexpr size_t kEditJumpBit = 358;

struct EditSave {
  AppearanceTable appearances;  // by player id
  std::vector<EditPlayerRecord> players;  // save order
};
// Decrypts the file, recomputes every block's SHA-512 against the header as
// the game does (FUN_14140f470), and collects the appearance records. A stale
// stored digest is reported through `error` but the block is still used: the
// game skips the check when the file's flag bit for that block is set, and
// the running game demonstrably loads such files.
std::optional<EditSave> ReadEditSave(std::span<const uint8_t> file, std::string* error);

// PlayerAppearance.bin, the game's fallback for players the save lacks:
// 60-byte {u32 id, 56-byte body} records, held as 72-byte records with zero
// boots/glove word and alternate id (pes-kit-assembly.md 8.1).
inline constexpr const char* kPlayerAppearancePath = "common/character0/model/character/appearance/PlayerAppearance.bin";
inline constexpr size_t kAppearanceFileRecordBytes = 60;
std::optional<AppearanceTable> ParsePlayerAppearanceFile(std::span<const uint8_t> blob, std::string* error);

// The fields of a 72-byte appearance record the part choice uses.
PlayerLook DecodeLook(const AppearanceRecord& record);

}  // namespace pes
