// PES 2021's formation tables (pesdb Tactics.bin, TacticsFormation.bin) and the sheet triple grid
// (FUN_14051B3F0) that turns a formation cell into a slot-point row (FUN_1404F5F10 -> record A).
#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/formation.hpp"
#include "pes/vfs.hpp"

namespace {

using pes::formation::Cell;
using pes::formation::FormationTables;
using pes::formation::Point3;
using pes::formation::SheetGrid;

// The install's three grid constants, as the install tests read them. The synthetic tests use the
// same values as an input and check the rules, not the install.
constexpr SheetGrid kGrid{7.0f, 15.0f, 0.5f};
constexpr float kHalfLength = 50.0f;  // synthetic pitch half-length: a test input, not a PES value
constexpr float kHalfWidth = 30.0f;
constexpr unsigned kCellCount = 105;  // 7 cells x 15 sub-steps: the y range of a formation cell

void PutU32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

std::vector<uint8_t> Concat(std::vector<uint8_t> a, const std::vector<uint8_t>& b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

// One Tactics.bin record: {tacticsId, teamId, packed}.
std::vector<uint8_t> TacticsRecord(uint32_t id, uint32_t team, uint32_t packed) {
  std::vector<uint8_t> r;
  PutU32(r, id);
  PutU32(r, team);
  PutU32(r, packed);
  return r;
}

// One TacticsFormation.bin entry: {id, role, packed}, packed = x | y << 8 | slot << 16 | phase << 20.
std::vector<uint8_t> Entry(uint32_t id, uint32_t role, uint32_t phase, uint32_t slot, uint32_t x,
                           uint32_t y) {
  std::vector<uint8_t> e;
  PutU32(e, id);
  PutU32(e, role);
  PutU32(e, x | (y << 8) | (slot << 16) | (phase << 20));
  return e;
}

// A 396-byte group of 33 entries. Slot 0 is the keeper (role 0) with the given stored coordinates;
// the other slots have role == slot, x == 5 + slot and y == 10 + 5 * phase + slot.
std::vector<uint8_t> Group(uint32_t id, uint32_t keeperX = 3, uint32_t keeperY = 52) {
  std::vector<uint8_t> g;
  for (uint32_t phase = 0; phase < 3; ++phase)
    for (uint32_t slot = 0; slot < 11; ++slot) {
      if (slot == 0)
        g = Concat(g, Entry(id, 0, phase, 0, keeperX, keeperY));
      else
        g = Concat(g, Entry(id, slot, phase, slot, 5 + slot, 10 + 5 * phase + slot));
    }
  return g;
}

std::optional<pes::PesVfs> MountedVfs(std::string* err) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return std::nullopt;
  pes::PesVfs vfs;
  if (!vfs.Mount(root, err)) return std::nullopt;
  return vfs;
}

}  // namespace

TEST(TacticsRecord, PlanIsBit24OfThePackedWord) {
  std::string err;
  const auto blob = Concat(TacticsRecord(0x1af6, 1, 0), TacticsRecord(0x1af7, 1, 1u << 24));
  auto records = pes::formation::ParseTactics(blob, &err);
  ASSERT_TRUE(records.has_value()) << err;
  ASSERT_EQ(records->size(), 2u);
  EXPECT_EQ((*records)[0].plan(), 0);
  EXPECT_EQ((*records)[1].plan(), 1);
}

TEST(TacticsRecord, RejectsAPartialRecord) {
  std::string err;
  auto blob = TacticsRecord(0x1af6, 1, 0);
  blob.push_back(0);
  EXPECT_FALSE(pes::formation::ParseTactics(blob, &err).has_value());
  EXPECT_FALSE(err.empty());
}

TEST(FormationGroup, EachEntryLandsInItsPhaseAndSlotCell) {
  std::string err;
  auto plan = pes::formation::BuildPlan(Group(0x1af6), 0x1af6, &err);
  ASSERT_TRUE(plan.has_value()) << err;
  for (int phase = 0; phase < 3; ++phase)
    for (int slot = 1; slot < 11; ++slot) {
      const Cell& c = (*plan)[slot][phase];
      EXPECT_EQ(c.role, slot);
      EXPECT_EQ(c.x, 5 + slot);
      EXPECT_EQ(c.y, 10 + 5 * phase + slot);
    }
}

TEST(FormationGroup, AKeeperCellDoesNotDependOnItsStoredCoordinates) {
  std::string err;
  auto a = pes::formation::BuildPlan(Group(0x1af6, 40, 7), 0x1af6, &err);
  auto b = pes::formation::BuildPlan(Group(0x1af6, 3, 90), 0x1af6, &err);
  ASSERT_TRUE(a.has_value()) << err;
  ASSERT_TRUE(b.has_value()) << err;
  for (int phase = 0; phase < 3; ++phase) EXPECT_EQ((*a)[0][phase], (*b)[0][phase]);
}

TEST(FormationGroup, AnEntryWithAnIdOtherThanItsRecordIsRefused) {
  std::string err;
  EXPECT_FALSE(pes::formation::BuildPlan(Group(0x1af7), 0x1af6, &err).has_value());
  EXPECT_FALSE(err.empty());
}

TEST(SheetGrid, AxisIsTheCellGridOfTheFieldHalfSize) {
  // Cell v of 105 covers the half-size in steps of 2h/105 and sits at its centre, v + 0.5 (FUN_14051B3F0).
  for (unsigned v = 0; v < kCellCount; ++v) {
    const float want = (static_cast<float>(v) + 0.5f) * (2.0f * kHalfLength / kCellCount) - kHalfLength;
    EXPECT_NEAR(pes::formation::GridAxis(kHalfLength, v, kGrid), want, 1e-3f) << "v=" << v;
  }
}

TEST(SheetGrid, AxisIsStrictlyIncreasingAcrossTheWholeRange) {
  for (unsigned v = 1; v < kCellCount; ++v)
    EXPECT_GT(pes::formation::GridAxis(kHalfLength, v, kGrid),
              pes::formation::GridAxis(kHalfLength, v - 1, kGrid));
}

TEST(SheetGrid, TripleTakesXAlongTheLengthAndYAcrossTheWidthWithNoHeight) {
  const Cell c{3, 3, 52};  // role, x, y
  const Point3 p = pes::formation::SheetTriple(kHalfLength, kHalfWidth, c, kGrid);
  EXPECT_FLOAT_EQ(p.x, pes::formation::GridAxis(kHalfLength, 3, kGrid));
  EXPECT_FLOAT_EQ(p.y, 0.0f);
  EXPECT_FLOAT_EQ(p.z, pes::formation::GridAxis(kHalfWidth, 52, kGrid));
}

TEST(SlotPointRows, SlotZeroIsTheKeeperRowAndTheOthersAreTheCellOfThePhase) {
  std::string err;
  auto plan = pes::formation::BuildPlan(Group(0x1af6), 0x1af6, &err);
  ASSERT_TRUE(plan.has_value()) << err;
  const Point3 keeper{-50.0f, 0.0f, 0.0f};  // a synthetic keeper row
  for (int phase = 0; phase < 3; ++phase) {
    const auto rows = pes::formation::SlotPointRows(*plan, phase, kHalfLength, kHalfWidth, kGrid, keeper);
    EXPECT_FLOAT_EQ(rows[0].x, keeper.x);
    EXPECT_FLOAT_EQ(rows[0].z, keeper.z);
    for (int slot = 1; slot < 11; ++slot) {
      const Point3 want = pes::formation::SheetTriple(kHalfLength, kHalfWidth, (*plan)[slot][phase], kGrid);
      EXPECT_FLOAT_EQ(rows[slot].x, want.x) << "slot " << slot << " phase " << phase;
      EXPECT_FLOAT_EQ(rows[slot].z, want.z) << "slot " << slot << " phase " << phase;
    }
  }
}

TEST(FormationInstall, EachTeamHasOneRecordPerPlanAndTheTablesParse) {
  std::string err;
  auto vfs = MountedVfs(&err);
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  auto tables = FormationTables::Load(*vfs, &err);
  ASSERT_TRUE(tables.has_value()) << err;
  ASSERT_GT(tables->RecordCount(), 0u);
  // Team 1 is a vanilla team with a record for each plan (the fixture's home side, GF team 1).
  EXPECT_TRUE(tables->PlanOf(1, 0).has_value());
  EXPECT_TRUE(tables->PlanOf(1, 1).has_value());
}

TEST(FormationInstall, TheGridAndTheKeeperRowAreReadFromTheExe) {
  std::string err;
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto grid = pes::formation::ReadSheetGrid(*exe, &err);
  ASSERT_TRUE(grid.has_value()) << err;
  // The cell count times the sub-steps is the 105-step y range the formation tables address.
  EXPECT_EQ(static_cast<unsigned>(grid->cells * grid->subdivisions), kCellCount);
  EXPECT_FLOAT_EQ(grid->halfStep, 0.5f);
  auto keeper = pes::formation::ReadKeeperRow(*exe, &err);
  ASSERT_TRUE(keeper.has_value()) << err;
  EXPECT_LT(keeper->x, -40.0f);  // the keeper stands on his own goal line, not in the field
  EXPECT_FLOAT_EQ(keeper->y, 0.0f);
  EXPECT_FLOAT_EQ(keeper->z, 0.0f);
}
