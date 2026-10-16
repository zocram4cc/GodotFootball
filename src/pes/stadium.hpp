#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "pes/types.hpp"
#include "pes/vfs.hpp"

namespace pes {

// A model's placement in Fox world space (metres; X long axis, Z width, Y up),
// exactly as the fox2 TransformEntity says — scale, rotation (x,y,z,w) and
// translation. Identity by default (PES leaves unrotated entities this way).
struct Transform {
  Vec3 scale = {1, 1, 1};
  Quat rotation = {0, 0, 0, 1};
  Vec3 translation = {0, 0, 0};
};

// One model a stadium's fox2 files place: a fmdl inside a sibling .fpk with a
// transform. fpk is the VFS path of the .fpk; member is the FpkEntry::path of
// the .fmdl inside it (the modelFile FilePtr, leading '/' stripped).
struct PlacedModel {
  std::string entityName;  // the fox2 "name" property, e.g. "StadiumModel_C1"
  std::string className;   // e.g. "StadiumModel", "GoalnetData", "MobCharacterModel"
  std::string fpk;         // VFS path of the .fpk holding the fmdl
  std::string member;      // FpkEntry::path of the .fmdl inside it
  Transform transform;
  uint32_t kind = 0;       // fox2 "kind" (0 scene/center, 1 weather overlay, ...)
  uint32_t direction = 0;  // fox2 "direction" (0..4 compass)
  uint32_t flags = 0;      // fox2 "flags"
};

// One row of common/etc/pesdb/Stadium.bin: the stadium id. Its content folder
// is derived from the id ("st" + id zero-padded to 3 digits, e.g. "st002");
// the string at +370 is not the folder (vanilla holds names like "CAMP_NOU").
struct StadiumInfo {
  int id = 0;
};

// Pure parse of Stadium.bin (after WESYS unwrap): 392-byte records, u16 id at +4.
std::vector<StadiumInfo> ParseStadiumRecords(std::span<const uint8_t> blob,
                                             std::string* error);

// ReadStadiumList reads common/etc/pesdb/Stadium.bin from the VFS and returns
// its records. Returns an empty vector (with *error set) on failure.
std::vector<StadiumInfo> ReadStadiumList(const PesVfs& vfs, std::string* error);

// The match's weather, as PES numbers it (PesLighting reads the same values):
// fine, rain or snow. A weather-only layer is placed in its weather alone.
inline constexpr int kWeatherFine = 0;
inline constexpr int kWeatherRain = 1;
inline constexpr int kWeatherSnow = 2;

// One Team.bin record: the team id (u32 at +8) and its home stadium id (the low
// u16 at +0x3C). Records are 1532 bytes (pes-re-stadium-cloth-vfx.md 1.1).
struct TeamRecord {
  int id = 0;
  int stadiumId = 0;
};

// Pure parse of Team.bin (after WESYS unwrap): 1532-byte records.
std::vector<TeamRecord> ParseTeamRecords(std::span<const uint8_t> blob, std::string* error);

// ReadTeamList reads common/etc/pesdb/Team.bin, the effective file: a download
// pack overrides the Data/ copy. Returns an empty vector (with *error set) on failure.
std::vector<TeamRecord> ReadTeamList(const PesVfs& vfs, std::string* error);

// The stadium PES's match resolver gives a stored id: the id when it is in
// validIds, else 29 (Konami Stadium). A stored 0 is 29 too
// (pes-re-stadium-cloth-vfx-opens.md 1.1).
// ponytail: the valid set (owner+0x28) is undecoded; stand-in = the Stadium.bin
// ids whose scene pack ships in the install. Upgrade: decode the set's source.
int ResolveStadiumId(int storedId, const std::vector<int>& validIds);

// The stadium a PES team plays at home: its Team.bin stadium through
// ResolveStadiumId, 29 when the team is not listed.
int TeamStadiumId(const PesVfs& vfs, int teamId);

// The Stadium.bin ids whose scene pack ships in the install: the valid set
// ResolveStadiumId checks against. ponytail: stand-in, see ResolveStadiumId.
std::vector<int> ValidStadiumIds(const PesVfs& vfs);

// Assemble one stadium's model placements from its fox2 files, in this order:
// the scene (<folder>.fpkd), the day/fine overlay (<folder>_df.fpkd, when the
// pack ships one), the pitch (pitch/<folder>.fpkd), the goals (common goal_post
// .fpkd: post + shadow meshes), the two nets (common goalnet_{l,r}.fpkd), and
// the corner flags (a per-stadium override when present, else common). Models
// whose fmdl member is not packaged in the sibling .fpk are dropped — PES
// cannot place a model its pack does not ship. The scene, pitch and goal_post
// sources are required; the rest are best-effort. A weather-only member (its file
// name has `_rain` or `_snow`) is placed only in that weather.
std::optional<std::vector<PlacedModel>> LoadStadiumScene(const PesVfs& vfs,
                                                         int stadiumId,
                                                         int weather,
                                                         std::string* error);

}  // namespace pes
