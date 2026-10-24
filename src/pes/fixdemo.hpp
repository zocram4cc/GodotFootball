#pragma once

// PES 2021's FixDemo cut tables (common/demo/fixdemo/<category>/table_<category>.bin) and cut files
// (cut_data/*.fdc), read from the user's install through the VFS. Every number here is read; none is
// transcribed. Formats: docs/re/pes-re-cutscenes.md §2 (tables) and §3 (cut files).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pes/vfs.hpp"

namespace pes::fixdemo {

// A T2 scene's attribute bits, tested on its first step (pes-re-cutscenes.md §2.3). Bit 6 is the
// generic celebration pool, and bits 11, 12 and 13 are the goal level 0, 1 and 2 (FUN_140477400,
// pes-re-scene-choice.md §2a).
inline constexpr int kGenericPoolBit = 6;
inline constexpr int kGoalLevelBit0 = 11;

// One step of a scene (a T2 row). `cutRef` names a cut group in T1, and 0 ends the scene.
struct SceneStep {
  uint8_t kind = 0;
  float fade_s = 0.0f;  // the transition in seconds (0.5 in 2,164 of 2,267 steps)
  uint32_t cutRef = 0;  // (category << 16) | (group + 1)
  uint32_t flags = 0;
};

// A T2 scene: its steps in play order.
struct Scene {
  std::vector<SceneStep> steps;
  uint32_t flags = 0;  // the first step's attribute word
};

// A T1 variant row: the .fdc files that play together. fdc[0] is the scene's cut: its Cut record
// gives the step's length.
struct Variant {
  std::vector<std::string> fdc;
};

struct CutTable {
  std::vector<Scene> scenes;                 // T2, in scene-index order
  std::vector<std::vector<Variant>> groups;  // T1: cut group -> its variants

  // The variants of a step's cut group, or nullptr when `cutRef` names no group.
  const std::vector<Variant>* Group(uint32_t cutRef) const;
};

// Reads table_<category>.bin (§2.2). Only the goal table is read so far. The reader must consume
// every column stream and land on the table's section end; anything else is an error.
std::optional<CutTable> LoadCutTable(const PesVfs& vfs, const std::string& category,
                                     std::string* error);

// The length, in seconds, of the Cut record in cut_data/<fdc> (its maxTime, in 1/60 s ticks).
std::optional<float> CutLength_s(const PesVfs& vfs, const std::string& category,
                                 const std::string& fdc, std::string* error);

// A scene's length in seconds: each step plays its cut (fdc[0] of its first variant), then its
// fade. The terminating step (cutRef 0) and anything after it are not played.
std::optional<float> SceneLength_s(const PesVfs& vfs, const std::string& category,
                                   const CutTable& table, size_t sceneIndex, std::string* error);

// A goal scene PES can pick, with its T2 index, its attribute word and its length.
struct GoalScene {
  int index = 0;
  uint32_t flags = 0;
  float length_s = 0.0f;
};

// The hug scenes: the second scene of a goal is drawn from the scenes with bit 2 or 3 (FUN_140474b20,
// pes-re-cutscenes.md §4.3 item 5). Their attribute bits are checked in GoalSequence.
inline constexpr int kHugPoolBitA = 2;
inline constexpr int kHugPoolBitB = 3;

// The goal celebrations: `generic` holds every scene with the generic-pool bit whose length is
// positive (FUN_140477400's pool). `fallback` is scene 0, which PES plays when repeated draws keep
// landing on the last celebration.
struct GoalScenes {
  std::vector<GoalScene> hug;  // scenes with bit 2 or 3, positive length (FUN_140474b20's pool)
  std::vector<GoalScene> generic;
  GoalScene fallback;
};

std::optional<GoalScenes> LoadGoalScenes(const PesVfs& vfs, std::string* error);

}  // namespace pes::fixdemo
