#pragma once

// PES's FixDemo variant selection on GF's memory: the manager arena the condition evaluator (0x141F4D2F0) and the variant
// matcher (0x141F4B1D0) read, in PES's layout, owned by GF. The translated evaluator, matcher, history map and seeding
// run unchanged (src/pes/translated); their allocations go through PesHeap (pes_native.hpp).
//
// What is in the arena (docs/re/pes-re-cutscenes.md Appendix A; CutsceneHybrid4 report for the captures):
//   - the ctx (FixDemoEnv, 0x52E0 bytes, singleton [0x1437F5D50]) with only the fields the foul and goal conditions read,
//     written from GF facts the way the ctx setter 0x141F51630 writes them from the world;
//   - the DB's cut library, per category, in PES's row layout ({start,count} pairs at DB+cat*0x20+0x168, the 0x78-byte
//     variant rows at +0x180; the singleton [0x1436F4FB8]), built from the decoded table_<cat>.bin;
//   - the session statics: the category table at 0x1437F5D58 and the Random1000 object at 0x1437F6340 (both seeded by the
//     translated 0x141F51FD0 / 0x141F52060), and the per-category pick history at 0x1437F5DC0 + cat*0x40.
// The variant pick is the port of the wrapper 0x141F4FDA0 over the translated 0x140A7CB40 and 0x141F4B1D0.

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pes/fixdemo_table.hpp"
#include "pes_state.hpp"

namespace pes {

// Cut categories (scene id >> 16).
constexpr int kFixDemoFoul = 2;
constexpr int kFixDemoGoal = 3;

// Where PES keeps the state the selection reads (VAs of the PES image; the translated code addresses them as such).
constexpr uint64_t kFixDemoCtxSlotVa = 0x1437F5D50;       // singleton: the ctx
constexpr uint64_t kFixDemoCategoryTableVa = 0x1437F5D58;  // u32[11], read as tbl[category] by condition types 0x0D..0x16
constexpr uint64_t kFixDemoMatchRngVa = 0x1437F6340;      // Random1000 used when ctx+0x5228 == 2; +0x10 seed, +0x20 phase
constexpr uint64_t kFixDemoHistoryVa = 0x1437F5DC0;       // + cat * 0x40: the 0x43 pick history (std::map + vectors)
constexpr uint64_t kFixDemoDbSlotVa = 0x1436F4FB8;        // singleton: the fixdemo::DB
constexpr uint32_t kFixDemoCategoryTableWords = 11;

// What GF knows about an event, in the units the ctx holds them. Each field is the ctx word the setter 0x141F51630 (or the
// event builder 0x14047C500) writes it into; the rest of the ctx stays zero except the stand-ins in fixdemo_select.cpp.
struct FixDemoFacts {
  uint32_t stadium = 0;      // ctx+0x50: environment +0x00, the stadium id (condition 0x01)
  uint32_t timeOfDay = 0;    // ctx+0x54 (environment +0x04)
  uint32_t weather = 0;      // ctx+0x58 (environment +0x08; condition 0x13)
  uint32_t matchPhase = 0;   // ctx+0x51F8: 0 first half, 1 second half, 2/3 extra time, 4 penalties (condition 0x09)
  uint32_t subjectSide = 0;  // ctx+0x5204: the side the event is about, 0 home 1 away (conditions 0x06, 0x0A, 0x20, 0x45)
  float eventX = 0, eventY = 0, eventZ = 0;  // ctx+0x5208/0x520C/0x5210: pitch position in PES's space (0x07, 0x08, 0x22)
  uint32_t eventPlayerSide = 0;              // ctx+0x5214
  uint8_t eventPlayerSlot = 0;               // ctx+0x5218
  int32_t goalDiff = 0;                      // ctx+0x5160: home goals minus away goals, as of the event (0x1C, 0x1D, 0x45)
  uint8_t playersHome = 11, playersAway = 11;  // ctx+0x2910 / 0x5140: players on the pitch (0x20)
};

struct FixDemoStepRequest {
  uint32_t cutId = 0;               // the step's cut group, (cat << 16) | (index + 1)
  uint32_t row = 0;                 // the variant the matcher picked, an index into the group's rows
  std::vector<std::string> fdc;     // that variant's .fdc layers (names under the category's cut_data folder)
};

struct FixDemoRequest {
  uint32_t sceneId = 0;
  std::vector<FixDemoStepRequest> steps;  // up to the scene's terminating cut 0
};

class FixDemoSelector {
 public:
  // `tables`: the decoded table of every category the selector may be asked about (key = category).
  explicit FixDemoSelector(std::map<int, FixDemoTable> tables);
  ~FixDemoSelector();
  FixDemoSelector(const FixDemoSelector&) = delete;
  FixDemoSelector& operator=(const FixDemoSelector&) = delete;

  // The ctx constructor's seeding (0x141F4C950): 0x141F51FD0(seed) fills the category table from the session RNG, then
  // 0x141F52060(seed) seeds the match RNG, redraws the category table's words 2..6 and resets the pick histories. PES's
  // seeds are its clock modulo 10000 (0x141B34AD0); the caller supplies them.
  void Seed(uint32_t sessionSeed, uint32_t matchSeed);

  // The ctx setter for one event (0x141F514D0 -> 0x141F51630): clears the ctx and the per-event pick cache, then writes the
  // facts and sets ctx+0x5228 = 2 (the match RNG).
  void BeginEvent(const FixDemoFacts& facts);

  // 0x141F4FDA0 for a cut group id: the variant row index, cached per event; nullopt where PES answers 0 (no such group).
  std::optional<uint32_t> PickRow(uint32_t cutId);

  // The cuts a scene plays, in order, each with its picked variant (the list expansion 0x141F616F0's use of the wrapper).
  std::optional<FixDemoRequest> Request(uint32_t sceneId);

  // A scene's list finished (0x141F58FC0, state >= 5): the category's rotation word 0x1437F5D58[cat] advances (translated
  // 0x141F4D070), except for 0x60001 and 0x40011..0x4001B. ponytail: 0x141F58FC0 also ages the pick history through
  // 0x141F52D20/0x141F52D80, neither in the committed layer; until they are, a history node keeps its last pick range.
  void EndScene(uint32_t sceneId);

  // Puts back one pick-history node recorded from a live PES, the way 0x141F4FFF0 creates it: key = (group start + range
  // start) * 0x100 + column, the node's two range words, and its used-block byte vector with the vector's capacity. For tests
  // that replay a recorded event from the session state PES held before it.
  void RestoreHistoryNode(int category, uint32_t key, uint32_t rangeStart, uint32_t rangeEnd,
                          const std::vector<uint8_t>& used, uint32_t capacity);

  // The machine, for tests that inject or read PES session state (category table, RNG, history).
  PesState& State();
  struct PesHeap& Heap();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace pes
