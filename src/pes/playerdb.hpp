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

struct PlayerTraits {
  int injuryResistance = 0;  // 0 C, 1 B, 2 A
  bool leftFooted = false;   // the stronger foot is the left
  int form = 0;              // raw 0..7 (UI = raw + 1)
  uint8_t condition = kConditionDefault;  // the mixer output, or the 0x46 default
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

 private:
  std::unordered_map<int, PlayerTraits> byId_;
};

}  // namespace pes
