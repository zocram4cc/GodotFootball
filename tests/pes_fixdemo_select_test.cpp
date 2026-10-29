// GF's FixDemo selection (pes::FixDemoSelector: the translated evaluator 0x141F4D2F0 and matcher 0x141F4B1D0 run on a
// GF-owned ctx/DB/statics arena) against PES's own picks for live-captured events.
//
// Oracle: four events of the running game (rendezvous captures of the ctx-init hook 0x141F514D0, live2/pid1792161 and
// live2/pid1981120; the wrapper hook 0x141F4FDA0 for event A). A capture at the entry of call N+1 holds the ctx facts and
// the per-event pick cache (ctx+0x30, a std::map cut id -> row) of the event call N started; the capture at the entry of call
// N holds the session state before it (category table 0x1437F5D58, Random1000 0x1437F6340, pick history). Each case
// replays the facts and that state on GF's arena and compares every cut's row and the RNG afterwards. Pins are values read
// by script from those captures (docs/ports/pes-port-CutsceneHybrid4.md); the tests skip without the install or when its
// FixDemo tables differ from the captured run's.
//   A  goal, 3 cuts 0x3014B..0x3014D (draws, history, stadium-specific row)    pid1792161 seq16355 -> 16360
//   B  goal, 2 cuts 0x30005, 0x30103 (no draw)                                  pid1981120 seq8080  -> 8091
//   D  foul, cut 0x20007 (one 0x43 draw)                                         pid1981120 seq8469  -> 9831
//   E  goal, 2 cuts 0x30037, 0x30104 (no draw)                                  pid1981120 seq9831  -> 9843

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "pes/fixdemo_select.hpp"
#include "pes/vfs.hpp"

namespace {

float F(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

struct HistoryNode {
  int category;
  uint32_t key;
  std::vector<uint8_t> used;
  uint32_t capacity;
};

struct Event {
  const char* name;
  pes::FixDemoFacts facts;
  std::array<uint32_t, pes::kFixDemoCategoryTableWords> table;  // 0x1437F5D58 before the event
  uint32_t rngSeed, rngPhase;                                    // Random1000 +0x10 / +0x20 before
  std::vector<HistoryNode> history;
  std::vector<std::pair<uint32_t, uint32_t>> picks;              // cut group id, PES's row, in PES's call order
  uint32_t rngSeedAfter, rngPhaseAfter;
};

pes::FixDemoFacts Facts(uint32_t stadium, uint32_t subject, float x, float y, float z, uint32_t evSide, uint8_t evSlot,
                        int32_t diff) {
  pes::FixDemoFacts f;
  f.stadium = stadium;
  f.subjectSide = subject;
  f.eventX = x;
  f.eventY = y;
  f.eventZ = z;
  f.eventPlayerSide = evSide;
  f.eventPlayerSlot = evSlot;
  f.goalDiff = diff;
  return f;
}

const std::vector<Event>& Events() {
  static const std::vector<Event> events = {
      {"A goal pickup-ball", Facts(0x4A, 1, F(1113548495), 0, F(3233808384u), 0, 0, 1), {0, 8, 3, 15, 6, 13, 14, 13, 14, 0, 5},
       0x47684811, 0x3B5,
       {{3, 106752, {0, 0, 1, 1}, 4}, {3, 245761, {0, 1, 1, 0}, 4}, {3, 1284608, {1, 0, 1}, 3},
        {3, 1342976, {0, 1, 0, 1, 1}, 5}},
       {{0x3014B, 1}, {0x3014C, 220}, {0x3014D, 0}}, 0x75EE2113, 0x1A},
      {"B goal celebration + run", Facts(0x31, 0, F(3260824944u), 0, F(1101438824), 0, 0, 0),
       {10, 7, 10, 7, 15, 5, 12, 10, 1, 1, 11}, 0x39CDABE1, 0x235, {}, {{0x30005, 1}, {0x30103, 6}}, 0x39CDABE1, 0x235},
      {"D foul warning", Facts(0x31, 0, 0, 0, 0, 0, 255, 0), {10, 7, 10, 8, 15, 5, 12, 10, 1, 1, 11}, 0x3D35A763, 0x26, {},
       {{0x20007, 0}}, 0x0466C975, 0xE7},
      {"E goal celebration + run", Facts(0x31, 0, 0, 0, 0, 0, 0, 1), {10, 7, 11, 8, 15, 5, 12, 10, 1, 1, 11}, 0x0466C975, 0xE7,
       {}, {{0x30037, 0}, {0x30104, 9}}, 0x0466C975, 0xE7},
  };
  return events;
}

// The FixDemo tables through the mounted VFS (the winner of any mod is the one the game read). Empty = skip.
std::map<int, pes::FixDemoTable> LoadTables(std::string* why) {
  std::map<int, pes::FixDemoTable> tables;
  const char* root = std::getenv("PES21_ROOT");
  if (!root) {
    *why = "PES21_ROOT not set";
    return tables;
  }
  pes::PesVfs vfs;
  std::string err;
  if (!vfs.Mount(root, &err)) {
    *why = err;
    return tables;
  }
  for (int cat : {pes::kFixDemoFoul, pes::kFixDemoGoal}) {
    auto bytes = vfs.Read(pes::FixDemoTablePath(cat), &err);
    auto table = bytes ? pes::DecodeFixDemoTable(*bytes, cat, &err) : std::nullopt;
    if (!table) {
      *why = err;
      return {};
    }
    tables[cat] = *table;
  }
  return tables;
}

// The captured run's tables: every cut an event picks must name the group and variant count PES's pick was made over.
bool TablesMatchTheCapturedRun(const std::map<int, pes::FixDemoTable>& tables, const Event& e) {
  for (const auto& [cut, row] : e.picks) {
    const auto t = tables.find(static_cast<int>(cut >> 16));
    const uint32_t g = (cut & 0xFFFF) - 1;
    if (t == tables.end() || g >= t->second.cutGroups.size() || row >= t->second.cutGroups[g].rows.size()) return false;
  }
  return true;
}

// The state PES held before the event, on a fresh selector, then the event's facts.
void Prepare(pes::FixDemoSelector& sel, const Event& e) {
  PesState& s = sel.State();
  for (uint32_t i = 0; i < pes::kFixDemoCategoryTableWords; ++i) s.wr(pes::kFixDemoCategoryTableVa + i * 4, e.table[i], 4);
  s.wr(pes::kFixDemoMatchRngVa + 0x10, e.rngSeed, 4);
  s.wr(pes::kFixDemoMatchRngVa + 0x20, e.rngPhase, 4);
  for (const HistoryNode& n : e.history) sel.RestoreHistoryNode(n.category, n.key, 0, 0, n.used, n.capacity);
  sel.BeginEvent(e.facts);
}

TEST(FixDemoSelect, GfPicksEqualPesForTheCapturedEventsAndLeaveTheRngWherePesLeftIt) {
  std::string why;
  const auto tables = LoadTables(&why);
  if (tables.empty()) GTEST_SKIP() << why;
  for (const Event& e : Events()) {
    if (!TablesMatchTheCapturedRun(tables, e)) GTEST_SKIP() << "install tables differ from the captured run: " << e.name;
    pes::FixDemoSelector sel(tables);
    Prepare(sel, e);
    for (const auto& [cut, row] : e.picks) {
      const auto got = sel.PickRow(cut);
      ASSERT_TRUE(got.has_value()) << e.name << " cut " << std::hex << cut;
      EXPECT_EQ(*got, row) << e.name << " cut " << std::hex << cut;
    }
    EXPECT_EQ(sel.State().rd(pes::kFixDemoMatchRngVa + 0x10, 4), e.rngSeedAfter) << e.name;
    EXPECT_EQ(sel.State().rd(pes::kFixDemoMatchRngVa + 0x20, 4), e.rngPhaseAfter) << e.name;
  }
}

// The scene request is the list expansion over the picks: the foul scene 0x20008 plays cut 0x20007 and its variant's layers.
TEST(FixDemoSelect, TheFoulSceneRequestNamesTheLayersOfThePickedVariant) {
  std::string why;
  const auto tables = LoadTables(&why);
  if (tables.empty()) GTEST_SKIP() << why;
  const Event& e = Events()[2];  // D
  if (!TablesMatchTheCapturedRun(tables, e)) GTEST_SKIP() << "install tables differ from the captured run";
  pes::FixDemoSelector sel(tables);
  Prepare(sel, e);
  const auto req = sel.Request(0x20008);
  ASSERT_TRUE(req.has_value());
  const pes::FixDemoScene& scene = tables.at(pes::kFixDemoFoul).scenes[0x20008 - 0x20001];
  ASSERT_EQ(req->steps.size(), 1u);
  EXPECT_EQ(req->steps[0].cutId, scene.steps[0].cut);
  EXPECT_EQ(req->steps[0].row, 0u);
  const auto& group = tables.at(pes::kFixDemoFoul).cutGroups[(scene.steps[0].cut & 0xFFFF) - 1];
  EXPECT_EQ(req->steps[0].fdc, group.rows[0].fdc);
  EXPECT_FALSE(req->steps[0].fdc.empty());
}

// The pick is cached per event (0x141F4FDA0's map): a second ask answers the first pick and draws nothing; a new event
// clears it.
TEST(FixDemoSelect, APickIsCachedForTheEventAndAFreshEventDrawsAgain) {
  std::string why;
  const auto tables = LoadTables(&why);
  if (tables.empty()) GTEST_SKIP() << why;
  const Event& e = Events()[2];
  if (!TablesMatchTheCapturedRun(tables, e)) GTEST_SKIP() << "install tables differ from the captured run";
  pes::FixDemoSelector sel(tables);
  Prepare(sel, e);
  const uint32_t first = *sel.PickRow(0x20007);
  const uint64_t rngAfterFirst = sel.State().rd(pes::kFixDemoMatchRngVa + 0x10, 4);
  EXPECT_EQ(*sel.PickRow(0x20007), first);
  EXPECT_EQ(sel.State().rd(pes::kFixDemoMatchRngVa + 0x10, 4), rngAfterFirst);
  sel.BeginEvent(e.facts);
  (void)sel.PickRow(0x20007);
  EXPECT_NE(sel.State().rd(pes::kFixDemoMatchRngVa + 0x10, 4), rngAfterFirst);  // a new event draws again
}

// A group that does not exist is PES's 0 (no row): nullopt, no draw.
TEST(FixDemoSelect, AnUnknownCutGroupHasNoPick) {
  std::string why;
  const auto tables = LoadTables(&why);
  if (tables.empty()) GTEST_SKIP() << why;
  pes::FixDemoSelector sel(tables);
  sel.BeginEvent({});
  EXPECT_FALSE(sel.PickRow(0x2FFFF).has_value());
  EXPECT_FALSE(sel.PickRow(0x20000).has_value());  // index (0 - 1) wraps past the count
}

// The seeding is a function of PES's two seeds (0x141F51FD0, 0x141F52060): same seeds, same words; the words the table
// holds are draws below 16; another match seed gives another match RNG.
TEST(FixDemoSelect, SeedingIsAFunctionOfTheSeedsAndFillsTheCategoryTableBelowSixteen) {
  std::string why;
  const auto tables = LoadTables(&why);
  if (tables.empty()) GTEST_SKIP() << why;
  pes::FixDemoSelector a(tables), b(tables), c(tables);
  a.Seed(1234, 5678);
  b.Seed(1234, 5678);
  c.Seed(1234, 5679);
  const auto word = [](pes::FixDemoSelector& s, uint64_t va) { return s.State().rd(va, 4); };
  for (uint32_t i = 0; i < pes::kFixDemoCategoryTableWords; ++i) {
    EXPECT_EQ(word(a, pes::kFixDemoCategoryTableVa + i * 4), word(b, pes::kFixDemoCategoryTableVa + i * 4));
    EXPECT_LT(word(a, pes::kFixDemoCategoryTableVa + i * 4), 16u);
  }
  EXPECT_EQ(word(a, pes::kFixDemoMatchRngVa + 0x10), word(b, pes::kFixDemoMatchRngVa + 0x10));
  EXPECT_NE(word(a, pes::kFixDemoMatchRngVa + 0x10), word(c, pes::kFixDemoMatchRngVa + 0x10));
}

// A finished scene advances its category's rotation word, except 0x60001 and 0x40011..0x4001B (0x141F58FC0).
TEST(FixDemoSelect, AFinishedSceneAdvancesItsCategoryRotationExceptThePesExemptions) {
  std::string why;
  const auto tables = LoadTables(&why);
  if (tables.empty()) GTEST_SKIP() << why;
  pes::FixDemoSelector sel(tables);
  const auto word = [&](int cat) { return sel.State().rd(pes::kFixDemoCategoryTableVa + cat * 4, 4); };
  sel.EndScene(0x20008);
  EXPECT_EQ(word(2), 1u);
  sel.EndScene(0x30005);
  EXPECT_EQ(word(3), 1u);
  sel.EndScene(0x60001);
  EXPECT_EQ(word(6), 0u);
  sel.EndScene(0x40011);
  sel.EndScene(0x4001B);
  EXPECT_EQ(word(4), 0u);
  sel.EndScene(0x40010);
  sel.EndScene(0x4001C);
  EXPECT_EQ(word(4), 2u);
}

}  // namespace
