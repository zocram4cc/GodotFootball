#include "pes/formation.hpp"

#include <cstring>

#include "pes/exe.hpp"
#include "pes/inflate.hpp"
#include "pes/vfs.hpp"

namespace pes::formation {
namespace {

// Image addresses, read through PeImage::ReadVa (file offset = VA - 0x140000000).
constexpr uint64_t kCellCountVa = 0x14259BDF0;       // DAT_14259bdf0 = 7.0f: cells along an axis
constexpr uint64_t kSubdivisionsVa = 0x1425C5CD0;    // DAT_1425c5cd0 = 15.0f: sub-steps per cell
constexpr uint64_t kHalfStepVa = 0x142565600;        // DAT_142565600 = 0.5f: the centre offset
constexpr uint64_t kKeeperInstructionVa = 0x141FC1954;  // c7 45 88 imm32: keeper x, 0xC2480000
constexpr uint8_t kMovKeeperPrefix[3] = {0xc7, 0x45, 0x88};  // mov dword [rbp-0x78], imm32

uint32_t ReadU32(std::span<const uint8_t> blob, size_t off) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(blob[off + i]) << (8 * i);
  return v;
}

std::optional<float> ReadFloat(const pes::PeImage& exe, uint64_t va, std::string* error) {
  uint8_t b[4] = {};
  if (!exe.ReadVa(va, sizeof b, b)) {
    if (error) *error = "PE image: sheet constant unmapped";
    return std::nullopt;
  }
  float f = 0.0f;
  std::memcpy(&f, b, sizeof f);
  return f;
}

}  // namespace

std::optional<std::vector<Tactics>> ParseTactics(std::span<const uint8_t> blob, std::string* error) {
  if (blob.size() % kTacticsRecordBytes != 0) {
    if (error) *error = "Tactics.bin: not whole 12-byte records";
    return std::nullopt;
  }
  std::vector<Tactics> out;
  out.reserve(blob.size() / kTacticsRecordBytes);
  for (size_t off = 0; off < blob.size(); off += kTacticsRecordBytes)
    out.push_back(Tactics{ReadU32(blob, off), ReadU32(blob, off + 4), ReadU32(blob, off + 8)});
  return out;
}

std::optional<Plan> BuildPlan(std::span<const uint8_t> group, uint32_t expectedId, std::string* error) {
  if (group.size() != kFormationGroupBytes) {
    if (error) *error = "TacticsFormation group: not 396 bytes";
    return std::nullopt;
  }
  Plan plan{};
  for (size_t i = 0; i < kFormationEntries; ++i) {
    const size_t off = i * kFormationEntryBytes;
    const uint32_t id = ReadU32(group, off);
    const uint32_t role = ReadU32(group, off + 4);
    const uint32_t packed = ReadU32(group, off + 8);
    const uint32_t x = packed & 0xffu;
    const uint32_t y = (packed >> 8) & 0xffu;
    const uint32_t slot = (packed >> 16) & 0xfu;
    const uint32_t phase = (packed >> 20) & 0xfu;
    if (id != expectedId || role > kMaxRole || x >= kCellXBound || y >= kCellYBound ||
        slot >= static_cast<uint32_t>(kSlots) || phase >= static_cast<uint32_t>(kPhases) || (packed >> 24) != 0) {
      if (error) *error = "TacticsFormation entry out of bounds or of another record";
      return std::nullopt;
    }
    Cell cell{static_cast<uint8_t>(role), static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
    if (role == 0) {  // FUN_1414F5120 l.966: the keeper's stored cell is (3, 52)
      cell.x = kKeeperCellX;
      cell.y = kKeeperCellY;
    }
    plan[slot][phase] = cell;
  }
  return plan;
}

float GridAxis(float half, unsigned v, const SheetGrid& grid) {
  // FUN_14051B3F0, one axis, in the decompile's order: fVar9 = (F + F) / 7; (fVar9 / 15) * 0.5;
  // local_c8[j] = fVar9 * j - F; local_128[k] = (fVar9 * k) / 15 + centre, with j = v / 15, k = v % 15.
  const unsigned sub = static_cast<unsigned>(grid.subdivisions);
  const float cell = (half + half) / grid.cells;
  const float step = cell / grid.subdivisions;
  const float centre = step * grid.halfStep;
  const unsigned j = v / sub;
  const unsigned k = v % sub;
  const float cellStart = cell * static_cast<float>(j) - half;
  const float within = (cell * static_cast<float>(k)) / grid.subdivisions + centre;
  return cellStart + within;
}

Point3 SheetTriple(float halfLength, float halfWidth, const Cell& cell, const SheetGrid& grid) {
  // FUN_14051B3F0's output: [0] from the x byte over F0, [1] = 0, [2] from the y byte over F1.
  return Point3{GridAxis(halfLength, cell.x, grid), 0.0f, GridAxis(halfWidth, cell.y, grid)};
}

std::array<Point3, kSlots> SlotPointRows(const Plan& plan, int phase, float halfLength, float halfWidth,
                                         const SheetGrid& grid, const Point3& keeper) {
  std::array<Point3, kSlots> rows{};
  for (int slot = 0; slot < kSlots; ++slot)
    rows[slot] = slot == 0 ? keeper : SheetTriple(halfLength, halfWidth, plan[slot][phase], grid);
  return rows;
}

std::optional<FormationTables> FormationTables::Parse(std::span<const uint8_t> tactics,
                                                      std::span<const uint8_t> formations,
                                                      std::string* error) {
  auto records = ParseTactics(tactics, error);
  if (!records) return std::nullopt;
  if (formations.size() != records->size() * kFormationGroupBytes) {
    if (error) *error = "TacticsFormation.bin: not one 396-byte group per tactics record";
    return std::nullopt;
  }
  FormationTables tables;
  for (size_t i = 0; i < records->size(); ++i) {
    auto plan = BuildPlan(formations.subspan(i * kFormationGroupBytes, kFormationGroupBytes),
                          (*records)[i].id, error);
    if (!plan) return std::nullopt;
    tables.records_.push_back(Record{(*records)[i], *plan});
  }
  return tables;
}

std::optional<FormationTables> FormationTables::Load(const pes::PesVfs& vfs, std::string* error) {
  auto tactics = vfs.Read(kTacticsPath, error);
  if (!tactics) return std::nullopt;
  auto formations = vfs.Read(kTacticsFormationPath, error);
  if (!formations) return std::nullopt;
  auto tacticsData = UnwrapWesys(*tactics, "Tactics", error);
  if (!tacticsData) return std::nullopt;
  auto formationData = UnwrapWesys(*formations, "TacticsFormation", error);
  if (!formationData) return std::nullopt;
  return Parse(*tacticsData, *formationData, error);
}

std::optional<Plan> FormationTables::PlanOf(uint32_t teamId, int plan) const {
  for (const Record& r : records_)
    if (r.tactics.teamId == teamId && r.tactics.plan() == plan) return r.plan;
  return std::nullopt;
}

std::optional<SheetGrid> ReadSheetGrid(const pes::PeImage& exe, std::string* error) {
  auto cells = ReadFloat(exe, kCellCountVa, error);
  auto subdivisions = ReadFloat(exe, kSubdivisionsVa, error);
  auto halfStep = ReadFloat(exe, kHalfStepVa, error);
  if (!cells || !subdivisions || !halfStep) return std::nullopt;
  return SheetGrid{*cells, *subdivisions, *halfStep};
}

std::optional<SheetConstants> ReadSheetConstants(const pes::PeImage& exe, std::string* error) {
  auto grid = ReadSheetGrid(exe, error);
  if (!grid) return std::nullopt;
  auto keeper = ReadKeeperRow(exe, error);
  if (!keeper) return std::nullopt;
  return SheetConstants{*grid, *keeper};
}

std::optional<Point3> ReadKeeperRow(const pes::PeImage& exe, std::string* error) {
  uint8_t op[7] = {};
  if (!exe.ReadVa(kKeeperInstructionVa, sizeof op, op)) {
    if (error) *error = "PE image: keeper instruction unmapped";
    return std::nullopt;
  }
  if (std::memcmp(op, kMovKeeperPrefix, sizeof kMovKeeperPrefix) != 0) {
    if (error) *error = "PE image: keeper instruction is not mov dword [rbp-0x78], imm32";
    return std::nullopt;
  }
  const uint32_t bits = ReadU32(std::span<const uint8_t>(op, sizeof op), 3);
  float x = 0.0f;
  std::memcpy(&x, &bits, sizeof x);
  return Point3{x, 0.0f, 0.0f};
}

}  // namespace pes::formation
