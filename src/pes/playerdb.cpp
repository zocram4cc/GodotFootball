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

// The mixer's 13 input ids (0x142B47AD0: 0C 09 0B 0A 05 06 08 07 04 03 02 01 00) map onto Player.bin
// ability ids 0x12..0x2A through FUN_1414E2060; each ability is a 6-bit field, value = raw + 40. The id->bit
// table below is matchlogic 1.2's accessor table, in the mixer's own input order. The weights come from the
// WROW table at 0x142B47B10 (rows 14..36), read from the install when the mixer's port lands; until then the
// record takes the 0x46 default path (condition-import 1: checksum match or id == 0).
uint8_t MixCondition(const uint8_t* rec) {
  (void)rec;
  // ponytail: the WROW weights at 0x142B47B10 are not read yet; FUN_1414E2120's checksum gate
  // (DAT_14351AE30/34) decides default-vs-mixer and lands with the same read. Until then every
  // record takes the 0x46 default path, so every unedited player deals Normal.
  return kConditionDefault;
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
    PlayerTraits traits{int(tier), Field(rec, kStrongerFootBit, 1) == 1,
                        int(Field(rec, kFormBit, kFormBits)), MixCondition(rec)};
    traits.speed = uint8_t(Field(rec, kSpeedBit, kAbilityBits) + kAbilityBias);
    traits.jump = uint8_t(Field(rec, kJumpBit, kAbilityBits) + kAbilityBias);
    traits.heightCm = uint8_t(Field(rec, kHeightBit, kHeightBits) + kHeightBiasCm);
    db.byId_.emplace(id, traits);
  }
  return db;
}

std::optional<PlayerTraits> PlayerDatabase::Find(int playerId) const {
  auto it = byId_.find(playerId);
  if (it == byId_.end()) return std::nullopt;
  return it->second;
}

}  // namespace pes
