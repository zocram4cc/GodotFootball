#include "pes/playerdb.hpp"

#include <cstring>

#include "pes/vfs.hpp"

namespace pes {
namespace {

// `width` bits from absolute bit `bit` of a record, LSB-first: (int.from_bytes(rec, 'little') >> bit) & mask.
uint32_t Field(const uint8_t* rec, size_t bit, size_t width) {
  uint32_t v = 0;
  for (size_t i = 0; i < width; ++i) {
    const size_t b = bit + i;
    v |= uint32_t((rec[b / 8] >> (b % 8)) & 1u) << i;
  }
  return v;
}

}  // namespace

std::optional<PlayerDatabase> PlayerDatabase::Load(const PesVfs& vfs, std::string* error) {
  auto blob = vfs.Read(kPlayerDatabasePath, error);
  if (!blob) return std::nullopt;
  return Parse(*blob, error);
}

std::optional<PlayerDatabase> PlayerDatabase::Parse(std::span<const uint8_t> blob, std::string* error) {
  if (blob.size() % kPlayerRecordBytes != 0) {
    *error = "Player.bin is not a whole number of 0x138-byte records";
    return std::nullopt;
  }
  PlayerDatabase db;
  for (size_t at = 0; at < blob.size(); at += kPlayerRecordBytes) {
    const uint8_t* rec = blob.data() + at;
    int32_t id = 0;
    std::memcpy(&id, rec + kPlayerIdOffset, sizeof id);
    const uint32_t tier = Field(rec, kInjuryResistanceBit, kInjuryResistanceBits);
    if (tier > kInjuryTierA) continue;
    db.byId_.emplace(id, PlayerTraits{int(tier), Field(rec, kStrongerFootBit, 1) == 1});
  }
  return db;
}

std::optional<PlayerTraits> PlayerDatabase::Find(int playerId) const {
  auto it = byId_.find(playerId);
  if (it == byId_.end()) return std::nullopt;
  return it->second;
}

}  // namespace pes
