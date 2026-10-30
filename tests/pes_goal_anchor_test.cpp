// GF's goal scene anchor (pes::FixDemoSelector::PlaceGoal: the translated calc 0x140474F00, the writer 0x14047D280's wrap
// and the flip 0x140478FF0, run on a GF-owned arena) against PES's own results for seven live-captured goals.
//
// Oracle: rendezvous captures of the anchor hooks (cut_anchor_calc_474F00 and cut_anchor_write_47D280, one writer call
// before each calc call), live2/pid2621613 and live2/pid132854. Each case pins what tools/dev/cutscene/anchor_cases.py reads
// from the snapshot at the call (the scorer's man record, run object, field info, the goal's scene id and radius, the scene
// row) and what PES's own bytes answered (the writer's scene+0x110..+0x120 after emulating the call). Values are pinned by
// that script, as floats' bits. The cases cover the scene row's modes (1,1,1) and (3,1,0), a push by the run radius, and
// both signs of x and z. Not answered by any capture: a true flip. The scorer's facing, velocity and move word are read by the
// calc, but no captured goal and none of the install's row kinds tried (explore: 8 rows x facing/velocity/move-word variants)
// changes the anchor with them, so no test can pin their wiring.
//
//   A pid2621613 seq1532  B pid132854 seq2152  C seq2962  D seq3783  E seq4195  F seq4721  G seq5566

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/fixdemo_select.hpp"
#include "pes/vfs.hpp"

namespace {

float F(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

constexpr size_t kRowBytes = 0x3C, kRowFloats = 8, kRowTail = 0x38;

struct Case {
  const char* name;
  const char* snap;
  uint32_t sceneId;
  uint32_t slot;
  std::array<float, 3> pos, vel;
  float facing;
  uint32_t moveMode;
  float halfLength, halfWidth, fps;
  std::array<float, 3> target;
  float radius;
  const char* rowHex;  // the scene row's 0x3C bytes as PES held them
  std::array<float, 3> wantPos;
  float wantRotY;
  int wantFlip;
};

const Case kCases[] = {
    {"A", "pid2621613/seq1532", 0x30034, 21, {F(0xC243860F), F(0x00000000), F(0x41828688)}, {F(0xBBA5C000), F(0x00000000), F(0x3E12FC00)}, F(0x43B2C5E6), 3,
     F(0x42523D70), F(0x42083D70), F(0x42580000), {F(0xC2463D70), F(0x00000000), F(0x42003D70)}, F(0x4158BD1D),
     "000001010100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
     {F(0xC22697AF), F(0x00000000), F(0x4192D98F)}, F(0x43A4FE13), 0},
    {"B", "pid132854/seq2152", 0x30005, 9, {F(0x4237FAD8), F(0x00000000), F(0xC12B77C2)}, {F(0x3CEB9000), F(0x00000000), F(0xBD873B00)}, F(0x43109120), 0,
     F(0x42523D70), F(0x42083D70), F(0x42580000), {F(0x42463D70), F(0x00000000), F(0xC1FC3225)}, F(0x41E627EB),
     "0000010101bf9d7200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
     {F(0x42097565), F(0x00000000), F(0xC0A69234)}, F(0x4315FC26), 0},
    {"C", "pid132854/seq2962", 0x30038, 10, {F(0x4228262B), F(0x00000000), F(0x4139217F)}, {F(0x3D620000), F(0x00000000), F(0x3E064880)}, F(0x41A49F00), 3,
     F(0x42523D70), F(0x42083D70), F(0x42580000), {F(0x42463D70), F(0x00000000), F(0x41FC3225)}, F(0x41ADDDA0),
     "000001010100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
     {F(0x4217522C), F(0x00000000), F(0x413353C6)}, F(0x41F01ED2), 0},
    {"D", "pid132854/seq3783", 0x300D3, 8, {F(0x4256453A), F(0x00000000), F(0x41AD46D8)}, {F(0x3D8D9200), F(0x00000000), F(0x3D3A0800)}, F(0x4270E63D), 3,
     F(0x42523D70), F(0x42083D70), F(0x42580000), {F(0x425C3D70), F(0x00000000), F(0x41A759EA)}, F(0x40A66666),
     "03000301003971c100005cc200005c4200005cc200005c4200000000000000000000000000000000000087430000b442000087430000b44200000000",
     {F(0x425C0000), F(0x00000000), F(0x41AD46D8)}, F(0x42B40000), 0},
    {"E", "pid132854/seq4195", 0x30009, 10, {F(0x42426445), F(0x00000000), F(0x411166F7)}, {F(0x3BF3E000), F(0x00000000), F(0x3E1309C0)}, F(0x401E8965), 3,
     F(0x42523D70), F(0x42083D70), F(0x42580000), {F(0x42463D70), F(0x00000000), F(0x42003D70)}, F(0x41BA5049),
     "00000101016335bb00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
     {F(0x42138C66), F(0x00000000), F(0x4121C228)}, F(0x41F01ED2), 0},
    {"F", "pid132854/seq4721", 0x300D4, 9, {F(0x42548A29), F(0x00000000), F(0x41A5BD2F)}, {F(0x3D324C00), F(0x00000000), F(0x3D4BBE00)}, F(0x423FD470), 3,
     F(0x42523D70), F(0x42083D70), F(0x42580000), {F(0x425C3D70), F(0x00000000), F(0x41A03A53)}, F(0x40CB1A5B),
     "03000301002eea98000056c200005642000056c20000564200000000000000000000000000000000000087430000b442000087430000b44200000000",
     {F(0x42560000), F(0x00000000), F(0x41A5BD2F)}, F(0x42B40000), 0},
    {"G", "pid132854/seq5566", 0x30009, 10, {F(0x422FE577), F(0x00000000), F(0xC11DCAEF)}, {F(0x3D3C2800), F(0x00000000), F(0xBDDD2400)}, F(0x4320889E), 0,
     F(0x42523D70), F(0x42083D70), F(0x42580000), {F(0x42463D70), F(0x00000000), F(0xC2003D70)}, F(0x41B9B502),
     "00000101016335bb00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
     {F(0x42136FEF), F(0x00000000), F(0xC120FCF2)}, F(0x4315FC26), 0},
};

std::vector<uint8_t> Unhex(const char* hex) {
  std::vector<uint8_t> out;
  for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) out.push_back(static_cast<uint8_t>(std::strtoul(std::string(hex + i, 2).c_str(), nullptr, 16)));
  return out;
}

pes::FixDemoSceneRow RowOf(const std::vector<uint8_t>& b) {
  pes::FixDemoSceneRow row;
  std::memcpy(row.head.data(), b.data(), row.head.size());
  std::memcpy(row.floats.data(), b.data() + kRowFloats, sizeof(float) * row.floats.size());
  std::memcpy(row.tail.data(), b.data() + kRowTail, row.tail.size());
  return row;
}

// FieldInfo F0..F8 of the captured run (the same eight floats on every goal), as bits.
constexpr uint32_t kFieldBits[] = {0x42523D70, 0x42083D70, 0x406A3D71, 0x401C28F6, 0x423C3D70,
                                   0x41128F5C, 0x42108F5C, 0x41A0F5C2, 0x42263D70};

// What goal_event_cases.py reads from each capture besides the anchor's inputs: the scene list the builder made (cuts), the
// A0 generator state before the call (rngPre, one draw of 0x140A92530 rewound), the picks PES's radius call had cached
// (cut id -> variant row), and whether the captured A0+0x30 / A0+0xC came out of the default arm: the two kind-3 goals
// (D, F) did not (A0 held a later recompute: 55.06 against the 60.06 the call gives on the same inputs).
struct Oracle {
  const char* name;
  std::vector<uint32_t> cuts;
  uint32_t rngPre;
  std::vector<std::pair<uint32_t, uint32_t>> picks;
  bool defaultArm;
};
const Oracle kOracle[] = {
    {"A", {0x30034, 0x30134, 0x3011C}, 194601516, {{0x30034, 1}, {0x3011C, 7}}, true},
    {"B", {0x3011B, 0x30005, 0x300A3, 0x30132}, 1127289236, {{0x30005, 0}, {0x300A3, 1}, {0x3011B, 9}}, true},
    {"C", {0x30111, 0x30038, 0x300A0}, 675191954, {{0x30038, 0}, {0x300A0, 1}, {0x30111, 1}}, true},
    {"D", {0x300D3}, 486850559, {}, false},
    {"E", {0x30102, 0x30009, 0x300A1, 0x30136}, 1570210949, {{0x30009, 0}, {0x300A1, 0}, {0x30102, 9}}, true},
    {"F", {0x300D4}, 511140391, {}, false},
    {"G", {0x30104, 0x30009, 0x30136, 0x3011C}, 350138786, {{0x30009, 1}, {0x30104, 5}, {0x3011C, 5}, {0x30136, 331}}, true},
};
static_assert(sizeof(kOracle) / sizeof(kOracle[0]) == sizeof(kCases) / sizeof(kCases[0]));

// The side's attacking bearing: the scorer's side attacks the end its corner spot (the captured target) is at.
pes::GoalAnchorFacts FactsOf(const Case& c, const Oracle& o) {
  pes::GoalAnchorFacts f;
  f.sceneId = c.sceneId;
  f.scorerSlot = c.slot;
  f.pos = c.pos;
  f.vel = c.vel;
  f.facing = c.facing;
  f.moveMode = c.moveMode;
  for (size_t k = 0; k < sizeof(kFieldBits) / sizeof(kFieldBits[0]); ++k) f.field[k] = F(kFieldBits[k]);
  f.fps = c.fps;
  const unsigned side = c.slot / 11;
  const float towardsPlusX = c.target[0] > 0 ? 90.0f : 270.0f;  // INF: PES's bearing of its +x end; pid132854's page is not captured
  f.attackDeg[side] = towardsPlusX;
  f.attackDeg[1 - side] = towardsPlusX > 180.0f ? 90.0f : 270.0f;
  f.cuts = o.cuts;
  f.rngState = o.rngPre;
  return f;
}
pes::GoalAnchorFacts FactsOf(const Case& c) { return FactsOf(c, kOracle[&c - kCases]); }

std::optional<pes::PeImage> OpenExe(std::string* why) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) {
    *why = "PES21_ROOT not set";
    return std::nullopt;
  }
  return pes::PeImage::Open(std::string(root) + "/PES2021.exe", why);
}

// A selector over the install's own goal table (scenes, cut groups, variant rows), as PES's DB holds it.
std::unique_ptr<pes::FixDemoSelector> MakeInstalled(const pes::PeImage& exe, pes::FixDemoTable* table, std::string* why) {
  const char* root = std::getenv("PES21_ROOT");
  pes::PesVfs vfs;
  if (!root || !vfs.Mount(root, why)) return nullptr;
  const auto bytes = vfs.Read(pes::FixDemoTablePath(pes::kFixDemoGoal), why);
  auto decoded = bytes ? pes::DecodeFixDemoTable(*bytes, pes::kFixDemoGoal, why) : std::nullopt;
  if (!decoded) return nullptr;
  *table = *decoded;
  return pes::FixDemoSelector::Create({{pes::kFixDemoGoal, *table}}, exe, why);
}

// A selector whose goal table holds only the case's scene row (the rest zero), at the scene's index.
std::unique_ptr<pes::FixDemoSelector> MakeFor(const pes::PeImage& exe, const Case& c) {
  pes::FixDemoTable t;
  t.goalSceneRows.resize(c.sceneId & 0xFFFF);
  t.goalSceneRows.back() = RowOf(Unhex(c.rowHex));
  std::string err;
  auto sel = pes::FixDemoSelector::Create({{pes::kFixDemoGoal, t}}, exe, &err);
  EXPECT_TRUE(sel) << err;
  return sel;
}

// GF's T (A0+0x30), radius (A0+0xC) and anchor on the inputs of the five captured goals that took the default arm equal
// PES's: the translated 0x140476C50, 0x140474CF0, 0x140474F00 and 0x140478FF0 on GF's arena, the install's tables, and the
// picks PES's own radius call had cached. Bit for bit.
TEST(GoalAnchor, EqualsPesOnTheFiveDefaultGoals) {
  std::string why;
  const auto exe = OpenExe(&why);
  if (!exe) GTEST_SKIP() << why;
  pes::FixDemoTable table;
  int compared = 0;
  for (const Case& c : kCases) {
    const Oracle& o = kOracle[&c - kCases];
    if (!o.defaultArm) continue;
    auto sel = MakeInstalled(*exe, &table, &why);
    if (!sel) GTEST_SKIP() << why;
    const uint32_t index = (c.sceneId & 0xFFFF) - 1;
    if (index >= table.goalSceneRows.size() || table.goalSceneRows[index].head != RowOf(Unhex(c.rowHex)).head)
      continue;  // the install's goal table is not the captured run's
    for (const auto& [cut, row] : o.picks) sel->RestorePick(cut, row);
    const auto got = sel->PlaceGoal(FactsOf(c, o));
    ASSERT_TRUE(got) << c.name;
    ++compared;
    for (int i = 0; i < 3; ++i) EXPECT_EQ(got->target[i], c.target[i]) << c.name << " " << c.snap << " T[" << i << "]";
    EXPECT_EQ(got->radius, c.radius) << c.name << " " << c.snap << " radius";
    for (int i = 0; i < 3; ++i) EXPECT_EQ(got->pos[i], c.wantPos[i]) << c.name << " " << c.snap << " pos[" << i << "]";
    EXPECT_EQ(got->rotYDeg, c.wantRotY) << c.name << " " << c.snap;
    EXPECT_EQ(got->flip, c.wantFlip != 0) << c.name << " " << c.snap;
  }
  if (compared == 0) GTEST_SKIP() << "the install's goal table matches none of the captured scenes";
}

// (OpenExe, MakeInstalled and MakeFor are defined above.)

// (the oracle test is above, beside the data it reads)

// The scorer's slot is only a table index: the same facts in another slot of the same side give the same anchor (a wrong
// table stride or a record written for another slot moves it).
TEST(GoalAnchor, DoesNotDependOnTheScorersSlot) {
  std::string why;
  const auto exe = OpenExe(&why);
  if (!exe) GTEST_SKIP() << why;
  const Case& c = kCases[1];
  auto sel = MakeFor(*exe, c);
  pes::GoalAnchorFacts f = FactsOf(c);
  const auto base = sel->PlaceGoal(f);
  ASSERT_TRUE(base);
  for (uint32_t slot : {0u, 5u, 10u}) {
    f.scorerSlot = slot;
    const auto got = sel->PlaceGoal(f);
    ASSERT_TRUE(got);
    EXPECT_EQ(got->pos, base->pos) << "slot " << slot;
    EXPECT_EQ(got->target, base->target) << "slot " << slot;
  }
}

// The anchor follows where the scorer stands, and the run radius (the translated 0x140474CF0 over the scene list) pushes it
// away from the run target (PES: the anchor closer to the target than the radius is moved out to it, along the bearing,
// before the pull-in from the pitch edge). An empty scene list has no radius.
TEST(GoalAnchor, FollowsTheScorerAndIsPushedOutByTheRunRadius) {
  std::string why;
  const auto exe = OpenExe(&why);
  if (!exe) GTEST_SKIP() << why;
  const Case& c = kCases[0];
  const Oracle& o = kOracle[0];
  pes::FixDemoTable table;
  auto sel = MakeInstalled(*exe, &table, &why);
  if (!sel) GTEST_SKIP() << why;
  for (const auto& [cut, row] : o.picks) sel->RestorePick(cut, row);
  pes::GoalAnchorFacts f = FactsOf(c, o);
  const auto distanceToTarget = [](const pes::GoalAnchor& a) {
    return std::hypot(a.pos[0] - a.target[0], a.pos[2] - a.target[2]);
  };
  f.cuts.clear();
  const auto base = sel->PlaceGoal(f);
  ASSERT_TRUE(base);
  EXPECT_EQ(base->radius, 0.0f) << "no scenes, no radius";
  f.cuts = o.cuts;
  const auto pushed = sel->PlaceGoal(f);
  ASSERT_TRUE(pushed);
  EXPECT_GT(pushed->radius, 0.0f) << "the scene list is summed";
  EXPECT_GT(distanceToTarget(*pushed), distanceToTarget(*base)) << "the radius is read";
  f.cuts.clear();
  f.pos[2] += 3.0f;
  const auto moved = sel->PlaceGoal(f);
  ASSERT_TRUE(moved);
  EXPECT_NE(moved->pos, base->pos) << "the scorer's position is read";
}

// The writer's wrap: a scene row of rotation kind 2 adds its float +0x28 (row float 8) to the anchor's rotation, so a row can
// push it out of [0, 360); the writer brings it back by one turn. Relation to the same row without the offset.
TEST(GoalAnchor, RotationIsWrappedIntoOneTurn) {
  std::string why;
  const auto exe = OpenExe(&why);
  if (!exe) GTEST_SKIP() << why;
  const Case& c = kCases[0];
  const auto rotWith = [&](float offset) {
    pes::FixDemoTable t;
    t.goalSceneRows.resize(c.sceneId & 0xFFFF);
    t.goalSceneRows.back().head = {0, 0, 1, 1, 2};
    t.goalSceneRows.back().floats[8] = offset;
    std::string err;
    auto sel = pes::FixDemoSelector::Create({{pes::kFixDemoGoal, t}}, *exe, &err);
    const auto got = sel ? sel->PlaceGoal(FactsOf(c)) : std::nullopt;
    EXPECT_TRUE(got) << err;
    return got ? got->rotYDeg : -1.0f;
  };
  const float base = rotWith(0.0f);
  ASSERT_GE(base, 0.0f);
  EXPECT_NEAR(rotWith(30.0f), base + 30.0f, 1e-3) << "inside the turn: unchanged";
  EXPECT_NEAR(rotWith(400.0f), base + 400.0f - 360.0f, 1e-3) << "past 360 loses a turn";
  EXPECT_NEAR(rotWith(-200.0f), base - 200.0f + 360.0f, 1e-3) << "below 0 gains a turn";
}

// A scene the table has no row for, or a slot off the pitch, has no anchor.
TEST(GoalAnchor, HasNoAnchorWithoutARowOrASlot) {
  std::string why;
  const auto exe = OpenExe(&why);
  if (!exe) GTEST_SKIP() << why;
  const Case& c = kCases[0];
  auto sel = MakeFor(*exe, c);
  pes::GoalAnchorFacts f = FactsOf(c);
  f.sceneId += 1;  // one past the table's last row
  EXPECT_FALSE(sel->PlaceGoal(f));
  f = FactsOf(c);
  f.scorerSlot = 22;
  EXPECT_FALSE(sel->PlaceGoal(f));
  f = FactsOf(c);
  f.sceneId = (pes::kFixDemoFoul << 16) | 1;  // not a goal category scene
  EXPECT_FALSE(sel->PlaceGoal(f));
}

// The table's scene row, laid into the DB as PES holds it (0x3C bytes: u8 x5, pad, f32 x12 at +8, u8 x4 at +0x38), is the
// row PES had for the captured goal's scene. A relation to the install's own table, not a pinned value: skips when the
// install's table is not the captured run's (a row's modes differ).
TEST(GoalAnchor, TheInstallsSceneRowIsTheRowPesHeld) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  if (!vfs.Mount(root, &err)) GTEST_SKIP() << err;
  const auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) GTEST_SKIP() << err;
  const auto bytes = vfs.Read(pes::FixDemoTablePath(pes::kFixDemoGoal), &err);
  const auto table = bytes ? pes::DecodeFixDemoTable(*bytes, pes::kFixDemoGoal, &err) : std::nullopt;
  if (!table) GTEST_SKIP() << err;
  auto sel = pes::FixDemoSelector::Create({{pes::kFixDemoGoal, *table}}, *exe, &err);
  ASSERT_TRUE(sel) << err;
  PesState& s = sel->State();
  const uint64_t rows = s.rd(s.rd(pes::kFixDemoDbSlotVa, 8) + 0x448, 8);
  ASSERT_NE(rows, 0u);
  EXPECT_EQ(table->goalSceneRows.size(), table->scenes.size()) << "one row per scene";
  int compared = 0;
  for (const Case& c : kCases) {
    const uint32_t index = (c.sceneId & 0xFFFF) - 1;
    if (index >= table->goalSceneRows.size()) continue;
    const auto want = Unhex(c.rowHex);
    ASSERT_EQ(want.size(), kRowBytes);
    if (RowOf(want).head != table->goalSceneRows[index].head) continue;  // the install's table differs from the captured run's
    ++compared;
    for (size_t b = 0; b < kRowBytes; ++b) {
      if (b >= 5 && b < kRowFloats) continue;  // compiler padding: PES's copy holds stack garbage there
      EXPECT_EQ(s.rd(rows + index * kRowBytes + b, 1), want[b]) << c.name << " scene " << std::hex << c.sceneId << " byte " << b;
    }
  }
  if (compared == 0) GTEST_SKIP() << "the install's goal table matches none of the captured scenes' rows";
}

}  // namespace
