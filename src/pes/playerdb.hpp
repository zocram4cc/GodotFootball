#pragma once

// PES 2021's player database: common/etc/pesdb/Player.bin, the injury tier, the stronger foot, Form and the
// condition byte of each player.
// Read-only; layout in docs/re/pes-re-matchlogic.md 1.1-1.2 (the exe's accessor table at 0x143529930).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#include "pes/editsave.hpp"

namespace pes {

class PesVfs;

inline constexpr const char* kPlayerDatabasePath = "common/etc/pesdb/Player.bin";
// 312 bytes per record, no header (matchlogic 1.1). The player id is a u32 at +8.
inline constexpr size_t kPlayerRecordBytes = 0x138;
inline constexpr size_t kPlayerIdOffset = 8;
// Injury Resistance: Player.bin id 0x2E, accessor fn 0x141F0FDD0, bit 458, 2 bits. The raw value is the tier:
// 0 C, 1 B, 2 A (matchlogic 1.2, "value = raw"). Code 3 is outside the accessor's 0..2 range and names no tier.
inline constexpr size_t kInjuryResistanceBit = 458;
inline constexpr size_t kInjuryResistanceBits = 2;
inline constexpr uint32_t kInjuryTierA = 2;
// Stronger Foot: Player.bin id 0x0F, accessor fn 0x141F103A0, bit 514, 1 bit: 0 right, 1 left (matchlogic 1.2).
inline constexpr size_t kStrongerFootBit = 514;
// Form: Player.bin id 0x2D, accessor fn 0x141F0FC60, bit 438, 3 bits, raw 0..7 (UI = raw + 1, matchlogic 1.2,
// fit 32/32). Form feeds UI display only: it does not enter the condition byte (condition-import 3).
inline constexpr size_t kFormBit = 438;
inline constexpr size_t kFormBits = 3;
// The condition byte: mixer FUN_141E5B540 over the 13 ability ids at 0x142B47AD0, (sum + 0x32)/100, clamped
// 0x28..0x6D, default 0x46 (condition-bands 6, condition-roll 4). Abilities are Player.bin 6-bit fields,
// value = raw + 40 (matchlogic 1.2).
inline constexpr uint8_t kConditionDefault = 0x46;
inline constexpr uint8_t kConditionMin = 0x28;
inline constexpr uint8_t kConditionMax = 0x6D;
// The bytes the in-match block starts from (the `base[id]` of the store step 0x14092AF10, DribbleProposal5 2). Speed is Player.bin
// id 0x1C (accessor 0x141F0F7C0: bit 306, 6 bits, raw + 40) for block byte 0x27; Jump is id 0x1F (accessor 0x141F0F760: bit 294,
// 6 bits, raw + 40) for block byte 0x2C; Height is id 0x10 (accessor 0x141F0F590: byte +0x1B, + 100 cm) for block byte 0x31
// (matchlogic 1.2/1.3). The stronger foot above is block byte 0x34 (INF: 0..1 in the clamp table, it varies per player in every
// captured roster; not checkable against Player.bin, the captured players' records are the owner's pack data, not this file's).
// pes_playerdb_test reads the accessors out of the exe and checks these constants against them.
inline constexpr size_t kSpeedBit = 306;
inline constexpr size_t kJumpBit = 294;
inline constexpr size_t kAbilityBits = 6;
inline constexpr uint8_t kAbilityBias = 40;
inline constexpr size_t kHeightBit = 216;
inline constexpr size_t kHeightBits = 8;
inline constexpr uint8_t kHeightBiasCm = 100;
// Live record byte +0x150 (the row's byte 0x3C): FUN_1414F8420, which loads a Player.bin record into the live record, stores 0x13 there
// (`mov byte [rbx+0x150],0x13`, 0x1414F85F3) when bit 18 of the record's dword at +0x3C (bit 498) is set; FUN_140AA1700 zeroed it
// before. The row builder's FUN_1408CB290(row, FUN_141FC0C40(live +0x150)) turns 0x13 into row byte 0x16 (the switch's case 0x13) and
// 0 into 0. No save field writes it, so a saved player's byte is his Player.bin record's.
inline constexpr size_t kLiveFlagBit = 498;
inline constexpr uint8_t kLiveFlagValue = 0x13;
inline constexpr uint8_t kRowByteOfLiveFlag = 0x16;

struct PlayerTraits {
  int injuryResistance = 0;  // 0 C, 1 B, 2 A
  bool leftFooted = false;   // the stronger foot is the left
  int form = 0;              // raw 0..7 (UI = raw + 1)
  uint8_t condition = kConditionDefault;  // the mixer output, or the 0x46 default
  uint8_t speed = 0;                      // block byte 0x27: Player.bin Speed, raw + 40
  uint8_t jump = 0;                       // block byte 0x2C: Player.bin Jump, raw + 40
  uint8_t heightCm = 0;                   // block byte 0x31: Player.bin Height, + 100
  uint8_t live150 = 0;                    // the live record's byte +0x150 as FUN_1414F8420 leaves it (0 or kLiveFlagValue)
  // The save's whole roster row (DecodeEditRow's image), present for a player the EDIT save holds: PES copies its first 0x50 bytes
  // into the AI record's ability block (FUN_140922bc0 -> FUN_140A58F00). Absent for a Player.bin-only player: PES's row for him
  // comes from a loader not decoded here.
  std::optional<std::array<uint8_t, kRosterRowBytes>> row;
};

class PlayerDatabase {
 public:
  // Reads the winning common/etc/pesdb/Player.bin from the install's VFS.
  static std::optional<PlayerDatabase> Load(const PesVfs& vfs, std::string* error);
  // Parses the file's bytes: whole records only. A repeated id keeps its first record.
  static std::optional<PlayerDatabase> Parse(std::span<const uint8_t> blob, std::string* error);

  // The traits of the player with this id; nullopt when no record carries that id or its tier is not C, B or A.
  std::optional<PlayerTraits> Find(int playerId) const;
  size_t Size() const { return byId_.size(); }
  // PES's roster takes a player's row bytes from the EDIT save's record when the save holds his id and from Player.bin otherwise
  // (pes-re-condition-import.md 1 step 1: the save overrides the install's Player.bin per id). Speed, Jump, Height, the stronger
  // foot, the injury tier and Form come from the save through DecodeEditRow (editsave.hpp: the record layout is not Player.bin's).
  // A save id with no Player.bin record is left out: its Parse-only traits (the condition byte) have no source for it.
  void ApplyEditSave(const EditSave& save);

 private:
  std::unordered_map<int, PlayerTraits> byId_;
};

}  // namespace pes
