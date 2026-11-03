#pragma once

// PES 2021 EDIT save (EDIT00000000): decryption and the per-player appearance
// records. Read-only; layout and cipher in docs/re/pes-kit-assembly.md 8.1.
// The save path arrives at run time only (PES21_EDIT_SAVE), never as a default.

#include <array>
#include <bitset>
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

// One 0x138-byte player record of the save's data block (`+0` the u32 player id). NOT Player.bin's layout. PES reads it through the
// descriptor table at 0x1434F2B20 (122 entries {id, getter, setter, min/default, max/bits}, FUN_141402570; the getters are tiny
// leaves: byte/word/dword load, shift, mask), and builds the match's roster row (the 0x50-byte block the store step 0x14092AF10
// starts from) in FUN_141FE9B00 (called from FUN_141FE3830) out of a live 0x17C record that FUN_141F10CE0 filled from the save
// record field by field: a save field whose value is outside its descriptor's [min, max] is skipped, and each row byte is clamped
// to the block clamp table (0x142613C60, FUN_1408CAF60). The composed map is below, one entry per row byte the builder writes;
// docs/ports/pes-port-EditSaveFields.md has the VAs of every table and switch case, and tests/pes_editsave_test.cpp re-derives
// the whole table from the exe and compares.
inline constexpr size_t kEditPlayerRecordBytes = 0x138;
using EditPlayerRecord = std::array<uint8_t, kEditPlayerRecordBytes>;
inline constexpr size_t kEditIdOffset = 0;

// Row byte indices GF reads (the in-match block, docs/re/pes-re-matchlogic.md 1.3).
inline constexpr size_t kRowPosition = 0x00;
inline constexpr size_t kRowSpeed = 0x27;
inline constexpr size_t kRowJump = 0x2C;
inline constexpr size_t kRowForm = 0x2E;
inline constexpr size_t kRowInjuryResistance = 0x2F;
inline constexpr size_t kRowHeightCm = 0x31;
inline constexpr size_t kRowStrongerFoot = 0x34;
inline constexpr size_t kEditRowBytes = 0x3D;  // bytes 0x00..0x3C; 0x33 is never written by the builder

struct EditRowField {
  uint8_t row;                 // roster row byte
  uint8_t saveId;              // the save's field id (descriptor table 0x1434F2B20)
  uint16_t bit;                // LSB-first bit index in the record, from the id's getter
  uint8_t width;               // bits
  uint8_t gateMin, gateMax;    // the descriptor's range: a value outside it is not applied (FUN_141F10CE0)
  uint8_t clampMin, clampMax;  // the row byte's clamp-table bounds (0x142613C60)
};
inline constexpr EditRowField kEditRowFields[] = {
    {0x00, 0x0E, 269, 5, 0, 31, 0, 12},
    {0x01, 0x33, 333, 2, 0, 2, 0, 2},
    {0x02, 0x34, 335, 2, 0, 2, 0, 2},
    {0x03, 0x35, 337, 2, 0, 2, 0, 2},
    {0x04, 0x36, 339, 2, 0, 2, 0, 2},
    {0x05, 0x37, 341, 2, 0, 2, 0, 2},
    {0x06, 0x38, 343, 2, 0, 2, 0, 2},
    {0x07, 0x39, 345, 2, 0, 2, 0, 2},
    {0x08, 0x3A, 347, 2, 0, 2, 0, 2},
    {0x09, 0x3B, 349, 2, 0, 2, 0, 2},
    {0x0A, 0x3C, 372, 2, 0, 2, 0, 2},
    {0x0B, 0x3D, 352, 2, 0, 2, 0, 2},
    {0x0C, 0x3E, 354, 2, 0, 2, 0, 2},
    {0x0D, 0x3F, 356, 2, 0, 2, 0, 2},
    {0x0E, 0x76, 279, 5, 0, 31, 0, 19},
    {0x0F, 0x77, 315, 3, 0, 7, 0, 6},
    {0x10, 0x32, 331, 2, 0, 2, 0, 2},
    {0x11, 0x75, 220, 4, 0, 15, 0, 9},
    {0x12, 0x78, 96, 8, 0, 255, 0, 255},
    {0x13, 0x79, 104, 8, 0, 255, 0, 255},
    {0x14, 0x12, 112, 7, 40, 99, 40, 99},
    {0x15, 0x23, 288, 7, 40, 99, 40, 99},
    {0x16, 0x26, 238, 7, 40, 99, 40, 99},
    {0x17, 0x14, 320, 7, 40, 99, 40, 99},
    {0x18, 0x13, 119, 7, 40, 99, 40, 99},
    {0x19, 0x15, 128, 7, 40, 99, 40, 99},
    {0x1A, 0x18, 149, 7, 40, 99, 40, 99},
    {0x1B, 0x16, 135, 7, 40, 99, 40, 99},
    {0x1C, 0x17, 142, 7, 40, 99, 40, 99},
    {0x1D, 0x19, 302, 7, 40, 99, 40, 99},
    {0x1E, 0x24, 224, 7, 40, 99, 40, 99},
    {0x1F, 0x25, 231, 7, 40, 99, 40, 99},
    {0x20, 0x1A, 160, 7, 40, 99, 40, 99},
    {0x21, 0x1B, 167, 7, 40, 99, 40, 99},
    {0x22, 0x27, 245, 7, 40, 99, 40, 99},
    {0x23, 0x28, 295, 7, 40, 99, 40, 99},
    {0x24, 0x29, 358, 7, 40, 99, 40, 99},
    {0x25, 0x2A, 256, 7, 40, 99, 40, 99},
    {0x26, 0x2C, 318, 2, 0, 3, 0, 3},
    {0x27, 0x1C, 174, 7, 40, 99, 40, 99},
    {0x28, 0x20, 199, 7, 40, 99, 40, 99},
    {0x29, 0x21, 206, 7, 40, 99, 40, 99},
    {0x2A, 0x1E, 365, 7, 40, 99, 40, 99},
    {0x2B, 0x1D, 181, 7, 40, 99, 40, 99},
    {0x2C, 0x1F, 192, 7, 40, 99, 40, 99},
    {0x2D, 0x22, 213, 7, 40, 99, 40, 99},
    {0x2E, 0x2D, 252, 3, 0, 7, 0, 7},
    {0x2F, 0x2E, 327, 2, 0, 2, 0, 2},
    {0x30, 0x0D, 263, 6, 15, 50, 15, 63},
    {0x31, 0x10, 80, 8, 100, 250, 132, 220},
    {0x32, 0x11, 88, 8, 30, 150, 30, 129},
    {0x34, 0x0F, 381, 1, 0, 1, 0, 1},
    {0x35, 0x2B, 126, 2, 0, 3, 0, 3},
    {0x36, 0x30, 329, 2, 0, 3, 0, 3},
    {0x37, 0x2F, 284, 3, 0, 7, 0, 7},
    {0x38, 0x71, 309, 3, 0, 7, 0, 4},
    {0x39, 0x72, 156, 4, 0, 15, 0, 9},
    {0x3A, 0x73, 312, 3, 0, 7, 0, 4},
    {0x3B, 0x74, 188, 4, 0, 15, 0, 9},
};
// The row's skill bitmask (u64 at +0x40, FUN_141FEA250, table 0x14352AC70 {skill bit, key}): the save's flag for key k is the one
// bit at kEditSkillFirstBit + k (id 0x48 + k), and it sets or clears skill bit kEditSkillBit[k].
inline constexpr size_t kEditSkillFirstBit = 390;
inline constexpr uint8_t kEditSkillBit[] = {0,  37, 1,  2,  3,  38, 4,  5,  32, 6,  7,  31, 39, 8,  33, 34, 9,  10, 11, 12, 40,
                                            13, 14, 15, 16, 28, 17, 18, 35, 19, 20, 29, 30, 21, 22, 23, 36, 24, 25, 26, 27};

// The playing-style masks (FUN_141FE9F40, called by the row builder FUN_141FE9B00):
// - row +0x48 (u32), the style word PES's playing-style gate reads (pesstyle.hpp: its bit is the engine bit): save id 0x40 (5 bits at
//   kEditStyleBit, range 0..31) goes through FUN_1414C04A0 (byte table 0x142979060, a value above 0x15 stores 0) into the live record's
//   field 0x3B, back out through FUN_1414C0300 (dword table 0x142979080) and FUN_141FC0AD0 (the switch: style 1..0x15 -> mask bit, 0 and
//   the default -> no bit; the 0x15 preset it passes in never survives);
// - row +0x4C (u32), seven flags: save ids 0x41 + k (one bit each at kEditFlagFirstBit + k) set live key 60 + k (FUN_1414C0200, table
//   0x1429790D8), and the table 0x14352AC10 {mask bit, key} turns each key into its mask bit.
// Row byte +0x3C is FUN_141FC0C40 of the live record's byte +0x150. No save field writes it (FUN_141F10CE0 and the field setter
// FUN_1414C6E90 never touch it): its writer is the Player.bin loader FUN_1414F8420 (playerdb.hpp kLiveFlagBit), so the save-decoded
// image holds 0 here and PlayerDatabase::ApplyEditSave fills it from the player's Player.bin record (0 on all 34 captured rows).
// Row bytes +0x3D..+0x3F are padding the builder never writes (one player's row shows 0 in one capture and a value in another).
inline constexpr size_t kEditStyleBit = 274, kEditStyleWidth = 5;
inline constexpr uint8_t kEditStyleLive[22] = {0, 1, 2, 3, 13, 14, 4, 18, 19, 5, 6, 7, 9, 20, 8, 11, 21, 12, 15, 10, 16, 17};
inline constexpr uint8_t kEditLiveStyle[22] = {0, 1, 2, 3, 6, 9, 10, 11, 14, 12, 19, 15, 17, 4, 5, 18, 20, 21, 7, 8, 13, 16};
inline constexpr int8_t kEditStyleMaskBit[22] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 11, 16, 18, 17, 14, 15, 19, 20};
inline constexpr size_t kEditFlagFirstBit = 383;
inline constexpr uint8_t kEditFlagMaskBit[7] = {0, 2, 1, 4, 6, 3, 5};
// The style word of a roster entry that has no row of the save: FUN_140AA1700 zeroes the live record, so its style value is 0, whose
// mask bit is none (kEditStyleMaskBit[0]; FUN_141FC0AD0's default arm sets no bit).
inline constexpr uint32_t kDefaultStyleWord = 0;
static_assert(kEditStyleMaskBit[0] < 0, "style value 0 sets no mask bit");

// The roster row, the 0x50 bytes PES copies into an AI record (FUN_140922bc0 -> FUN_140A58F00, record +0x70): the layout offsets.
inline constexpr size_t kRosterRowBytes = 0x50;
inline constexpr size_t kRowStyleByte = 0x3C, kRowSkills = 0x40, kRowStyleMask = 0x48, kRowFlagMask = 0x4C;

// The row a save record yields. `set` marks the bytes (0x00..0x3B) whose field passed its gate; the others stay with the caller's
// other source (Player.bin). `image` is the whole 0x50-byte row (unwritten bytes 0).
struct EditRow {
  std::array<uint8_t, kEditRowBytes> byte{};
  std::bitset<kEditRowBytes> set;
  uint64_t skills = 0;
  uint32_t styleMask = 0, flagMask = 0;
  std::array<uint8_t, kRosterRowBytes> image{};
};
EditRow DecodeEditRow(const EditPlayerRecord& rec);

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
