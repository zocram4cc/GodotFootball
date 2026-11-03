#pragma once

// PES's FixDemo selection tables, `common/demo/fixdemo/<cat>/table_<cat>.bin`, decoded from the mounted VFS.
// Layout: docs/re/pes-re-cutscenes.md §2.1-2.2 (container: 14 u32 header, then column-split typed streams A f32,
// B u32, W u16, U u8 and one string stream; the parse order is FUN_141f40c00). A table is consumed exactly or the
// decode fails: every stream is read to its header count and the section-end read counter must land on its value.
//
// Ids: scene id = (cat << 16) | (index + 1); a step's cut is a cut-group id of the same category; cut == 0 ends
// the scene's steps (pes-re-cutscenes.md §2.3).

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pes {

struct FixDemoCutRow {
  std::vector<uint16_t> keys;   // condition keys (condType << 8 | value), first 10 kept
  std::vector<std::string> fdc; // the .fdc layers that play together, first 10 kept (empty names dropped)
  uint32_t b0 = 0;
  float f = 0;
  uint32_t b1 = 0;
};

struct FixDemoCutGroup {
  uint32_t id = 0;  // (cat << 16) | (index + 1)
  uint32_t firstRow = 0;  // the group's slice start in the table's row array: the DB's {start, count} pair (0x140A7CB40)
  std::string name;
  std::vector<FixDemoCutRow> rows;  // the variants, in table order
};

struct FixDemoStep {
  uint8_t kind = 0;
  float fade = 0;
  uint32_t cut = 0;   // cut-group id; 0 terminates
  uint32_t flags = 0;
  uint8_t c0 = 0, c1 = 0;
  uint16_t w0 = 0;
  int16_t w1 = 0;
  std::vector<std::array<uint8_t, 3>> trip;  // first 10 kept
  int16_t w2 = -1;                           // -1 when the table is version 0
};

struct FixDemoScene {
  uint32_t id = 0;
  std::string name;
  std::vector<FixDemoStep> steps;
};

struct FixDemoGoalPlayerScene {
  uint32_t playerId = 0;
  uint32_t sceneId = 0;
};

// A goal scene's placement row, one per scene (table stream order: 5 u8, 12 f32, 4 u8). PES lays it out in the DB as
// 0x3C bytes: the u8s at +0, the floats at +8, the tail at +0x38 (the bytes between are compiler padding). The anchor
// 0x140474F00 reads bytes +2/+3/+4 (x, z and rotation modes) and floats; the event-position builder 0x140476C50 switches on +0.
struct FixDemoSceneRow {
  std::array<uint8_t, 5> head{};
  std::array<float, 12> floats{};
  std::array<uint8_t, 4> tail{};
};

struct FixDemoTable {
  int version = 0;
  std::vector<FixDemoCutGroup> cutGroups;
  std::vector<FixDemoScene> scenes;
  // Goal category only (empty otherwise): edit-scene ids and the playerId -> scene pairs.
  std::vector<uint32_t> goalEditScenes;
  std::vector<FixDemoGoalPlayerScene> goalPlayerScenes;
  std::vector<FixDemoSceneRow> goalSceneRows;  // one per scene, in scene order
};

// The VFS path of a cut layer a variant names: the category's cut_data folder + the name. Empty for an unknown category.
// (Which variant plays is the matcher's choice: FixDemoSelector, pes/fixdemo_select.hpp.)
std::string FixDemoCutDataPath(int category, const std::string& fdcName);

// Decodes one table_<cat>.bin. `category` is 1..9 (ent..mode). Returns nullopt with *error on any inconsistency.
std::optional<FixDemoTable> DecodeFixDemoTable(const std::vector<uint8_t>& bytes, int category, std::string* error);

// The install path of a category's table, from its folder name ("ent", "foul", ...), or empty for an unknown category.
std::string FixDemoTablePath(int category);

}  // namespace pes
