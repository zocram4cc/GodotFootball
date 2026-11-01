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

// What GF knows about a goal, in PES's units and axes (PES x, y up, z): the inputs of the goal's event position
// (0x140476C50), run radius (0x140474CF0) and scene anchor (0x140474F00) as they read them, with the PES site of each.
struct GoalAnchorFacts {
  uint32_t scorerSlot = 0;         // side * 11 + the scorer's index in the side (< 22); MI+0x15C8 = {side, index}, MI+0x1590
  std::array<float, 3> pos{};      // the scorer's man record +0x55C: metres
  std::array<float, 3> vel{};      // +0x574: metres per PES frame
  float facing = 0;                // +0x554: body facing, degrees (read instead of the velocity when moveMode == 1)
  uint32_t moveMode = 0;           // +0x5B0
  std::array<float, 18> field{};   // the field info G+0x2AA8: F0..F17, metres
  float fps = 0;                   // 0x14351AE54: PES frames per second (the rotation's speed scale divides by it)
  // The side's attacking bearing in degrees, the object behind ctx+0x18: +0x228D8 for side 0 (team info +0x254 = 1),
  // +0x228D4 for side 1 (+0x254 = 0xFF). 0x140476190 puts the corner spot at the end this bearing points to.
  std::array<float, 2> attackDeg{};
  // The scenes the goal plays. Given (`build` unset): the scene PES picked and the list it made, for goals whose list was recorded
  // (the tests' kind-3 and slim captures). Unset in GF: `build` set runs the builder 0x140477BC0 on the arena and ignores both.
  uint32_t sceneId = 0;            // A0+8: the goal scene (category 3); picks the scene row at DB+0x448
  std::vector<uint32_t> cuts;      // the scene list 0x140474CF0 sums the radius over
  // What the builder 0x140477BC0 (the pick 0x140475A90 and the list around it) reads from the match, in PES's units.
  struct SceneBuild {
    uint32_t period = 1;        // MI+0x1168: the match period (1 first half; 9 is the shoot-out)
    float clock_s = 0;          // the match clock as 0x140A3C050 returns it (protected float MI+0x1178 + MI+0x1174), split into minutes by 0x140A6BD80
    uint32_t previousPick = 0;  // A0+0x13C: the scene the DemoInfo picked for the previous goal (persists from goal to goal)
    std::optional<uint32_t> assistSlot;  // MI+0x162C = {side, index}: the scorer's team-mate who made it; none = PES's invalid pair
    std::array<uint32_t, 2> score{};     // each side's goals with this one counted: the team counters 0x140A80D10 sums (0x1408C9DF0 kind 5)
    // ponytail: the scorer's two edit-mode celebration slots (1-based indices into the goal table's edit-scene list, 0 = none: bytes 2
    // and 3 of his player record's u64 at +0x10, 0x140471280). GF's player data does not carry them; 0 takes PES's generic pick. Removed
    // with the player record's writer (the squad record builder).
    std::array<uint8_t, 2> celebration{};
    // Every man's ground position, PES axes (x, y, z): the builder reads the keepers' (the closing scene's keeper test, 0x14047A850) and the
    // assisting man's. The scorer's own comes from `pos`.
    std::array<std::array<float, 3>, 22> men{};
  };
  std::optional<SceneBuild> build;
  // ponytail: A0+0x114, the state of the generator at A0+0x108 that 0x140476190 draws its corner-spot jitter from. PES seeds
  // it where the DemoInfo is built (the 0x14047DF00 construction, from its clock); the site that removes this is that seed.
  uint32_t rngState = 0;
};

// The scene anchor: the writer 0x14047D280's result for a kind-1 goal, scene+0x110..+0x120, with the two inputs the
// translated 0x140476C50 and 0x140474CF0 computed for it.
struct GoalAnchor {
  std::array<float, 3> pos{};
  float rotYDeg = 0;  // wrapped into [0, 360)
  bool flip = false;  // 0x140478FF0
  std::array<float, 3> target{};  // A0+0x30 = the scorer's run object +0x18: the celebration spot (0x140476C50)
  float radius = 0;               // A0+0xC: the scene's run radius (0x140474CF0)
  uint32_t sceneId = 0;           // A0+8: the scene the goal plays first (the builder's pick when `build` was set)
  std::vector<uint32_t> scenes;   // the scene list the DemoInfo plays, in order (the builder's, or the given `cuts`)
};

// The scorer as the goal run object's gate 0x140472640 reads him each PES frame (man record +0x554/+0x55C/+0x564, anime record
// +1 and +0x54).
struct GoalGateFacts {
  std::array<float, 3> pos{};  // man +0x55C..: metres, PES axes
  float facing = 0;            // man +0x554: body facing, degrees
  uint8_t animeClass = 0;      // anime record +1: PES's anime class of what he is doing (0x1426484F0 tables)
  float animeSpeed = 0;        // anime record +0x54, the speed the tail compares with 13.5 ([0x1425A1FFC])
};

// What the foul run object's gate 0x14047EB00 reads each PES frame: the fouler, the fouled man and the referee (man records
// 0x14047EB00 takes through 0x140A48A10; slot 0x16 is the referee), and the fouled man's anime record +8 bit 10.
struct FoulGateFacts {
  std::array<float, 3> foulerPos{}, victimPos{}, refereePos{};  // man +0x55C..: metres, PES axes
  bool victimFlag10 = false;                                    // anime record +8 bit 10 of the fouled man
};

// A foul stop as PES's foul run object holds it at creation (the constructor 0x14047FEB0).
struct FoulGateStart {
  uint32_t sceneId = 0;       // object +0
  uint32_t foulerSlot = 0;    // MI+0x1590 (< 22)
  uint32_t victimSlot = 0;    // MI+0x1594 (< 22)
  float fps = 0;              // 0x14351AE50 / 0x14351AE54: PES frames per second
};

class PeImage;

class FixDemoSelector {
 public:
  // `tables`: the decoded table of every category the selector may be asked about (key = category). `exe`: the install's
  // PES2021.exe; the evaluator's switch tables live in PES's code (0x141F4F948.. and 0x141F508B0.., read by the translated
  // `jmp` sequences) and are mapped from it. Null with *error when the image does not map them.
  static std::unique_ptr<FixDemoSelector> Create(std::map<int, FixDemoTable> tables, const PeImage& exe, std::string* error);
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

  // Puts back one pick PES had cached for the event (ctx+0x30, cut id -> row), the way 0x141F4FDA0 inserts it. For tests
  // that replay a recorded goal from the picks PES's radius call had made.
  void RestorePick(uint32_t cutId, uint32_t row);

  // The cuts a scene plays, in order, each with its picked variant (the list expansion 0x141F616F0's use of the wrapper).
  std::optional<FixDemoRequest> Request(uint32_t sceneId);

  // A scene's list finished (0x141F58FC0, state >= 5): the category's rotation word 0x1437F5D58[cat] advances (translated
  // 0x141F4D070), except for 0x60001 and 0x40011..0x4001B. ponytail: 0x141F58FC0 also ages the pick history through
  // 0x141F52D20/0x141F52D80, neither in the committed layer; until they are, a history node keeps its last pick range.
  void EndScene(uint32_t sceneId);

  // The goal scene's anchor (writer 0x14047D280's kind-1 arm over the translated calc 0x140474F00, the wrap into [0, 360) with
  // [0x14259858C], and the flip 0x140478FF0), run on the arena: the scene row is the goal table's, laid into the DB at
  // +0x448/+0x450. Nullopt when the table has no row for the scene or the slot is not a pitch slot.
  std::optional<GoalAnchor> PlaceGoal(const GoalAnchorFacts& facts);

  // The goal's DemoInfo gate, PES's own: `0x140472640(A0, ctx, DemoInfo)` run once on the arena of the last PlaceGoal with the
  // scorer's facts of this frame (A0+0x14 counts down once per call: the timer 0x14047A710 set at PlaceGoal; the radius test,
  // the facing, anime-class and scene-flag clears are PES's), and A0+0xEC after it: true from the frame the gate opens (it
  // latches). Call it once per PES frame while DemoInfo state 2 waits. False before PlaceGoal.
  bool GoalGateReady(const GoalGateFacts& facts);

  // The foul DemoInfo's gate, PES's own: BeginFoulGate builds the object as 0x14047FEB0 does (countdown fps x 3 / the time
  // scale), FoulGateReady runs `0x14047EB00(object)` with this frame's positions and answers the ready byte (object +0xC).
  void BeginFoulGate(const FoulGateStart& start);
  bool FoulGateReady(const FoulGateFacts& facts);

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
  explicit FixDemoSelector(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace pes
