#pragma once

// PES 2021's player database: common/etc/pesdb/Player.bin, the injury tier and the stronger foot of each player.
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

struct PlayerTraits {
  int injuryResistance = 0;  // 0 C, 1 B, 2 A
  bool leftFooted = false;   // the stronger foot is the left
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
