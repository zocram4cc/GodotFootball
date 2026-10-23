// PES 2021's formation tables and the sheet triple that turns a formation cell into a slot-point row.
// Decoded in docs/re/pes-re-sheet-triple.md: Tactics.bin (M1: plan), TacticsFormation.bin (M2: cells),
// FUN_14051B3F0 (M3: grid), FUN_1404F5F10 and FUN_141FC1590 (row and keeper).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pes {
class PeImage;
class PesVfs;
}  // namespace pes

namespace pes::formation {

// common/etc/pesdb/Tactics.bin: records of {u32 tacticsId, u32 teamId, u32 packed}, two per team.
// Bit 24 of packed is the plan (0 or 1) the record fills (FUN_1414F5120, decompile l.857-893).
inline constexpr const char* kTacticsPath = "common/etc/pesdb/Tactics.bin";
inline constexpr size_t kTacticsRecordBytes = 12;
inline constexpr int kPlanBit = 24;

// common/etc/pesdb/TacticsFormation.bin: one 396-byte group per tactics record, at the same index
// and with the same id. A group is 33 entries {u32 id, u32 role, u32 packed} (FUN_1414F5120 l.386-491).
// packed = x | y << 8 | slot << 16 | phase << 20 (x and y are bytes, the slot is < 11, the phase < 3).
inline constexpr const char* kTacticsFormationPath = "common/etc/pesdb/TacticsFormation.bin";
inline constexpr size_t kFormationEntryBytes = 12;
inline constexpr size_t kFormationEntries = 33;
inline constexpr size_t kFormationGroupBytes = kFormationEntries * kFormationEntryBytes;
inline constexpr int kSlots = 11;         // the sheet's slot bound (FUN_1408C50E0: slot < 0xb)
inline constexpr int kPhases = 3;         // the sheet's phase bound (FUN_1408C50E0: f < 3)
inline constexpr uint32_t kMaxRole = 12;  // registered roles 0..12 (FUN_1408C5E90: role < 0xd)
inline constexpr uint32_t kCellXBound = 49;   // FUN_1408C5D50: x < 0x31
inline constexpr uint32_t kCellYBound = 105;  // FUN_1408C5D50: y < 0x69

// The keeper's cell. FUN_1414F5120 l.966 stores the immediate 0x3403 (x = 3, y = 52) for role 0.
inline constexpr uint8_t kKeeperCellX = 3;
inline constexpr uint8_t kKeeperCellY = 52;

struct Tactics {
  uint32_t id = 0;
  uint32_t teamId = 0;
  uint32_t packed = 0;
  int plan() const { return static_cast<int>((packed >> kPlanBit) & 1u); }
};

// One sheet cell: the registered role, and the formation coordinates (x along the length, y across).
struct Cell {
  uint8_t role = 0;
  uint8_t x = 0;
  uint8_t y = 0;
  bool operator==(const Cell&) const = default;
};

// plan[slot][phase] as FUN_1414C6180/61D0/65E0 address the table: per phase, eleven roles and then
// eleven (x, y) pairs.
using Plan = std::array<std::array<Cell, kPhases>, kSlots>;

// The sheet's constants, read from PES2021.exe. FUN_14051B3F0's DAT_14259bdf0 is the cell count (7.0),
// DAT_1425c5cd0 the sub-steps per cell (15.0, lane 0 of the 15s vector), DAT_142565600 the centre offset (0.5).
struct SheetGrid {
  float cells = 0.0f;
  float subdivisions = 0.0f;
  float halfStep = 0.0f;
};

// A slot-point row: FUN_1408C50E0's record A, (x, 0, z) for the length, height and width axes.
struct Point3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  bool operator==(const Point3&) const = default;
};

// Parses Tactics.bin (already unwrapped). Whole records only.
std::optional<std::vector<Tactics>> ParseTactics(std::span<const uint8_t> blob, std::string* error);

// Turns one 396-byte group into its plan. Every entry must carry `expectedId` (its tactics record's id)
// and be in bounds; a role-0 entry is the keeper cell.
std::optional<Plan> BuildPlan(std::span<const uint8_t> group, uint32_t expectedId, std::string* error);

// FUN_14051B3F0, one axis: the coordinate of cell v (0 .. cells*subdivisions - 1) on a half-size `half`.
// Each float operation keeps the order of the decompile, so the result is bit-for-bit the image's.
float GridAxis(float half, unsigned v, const SheetGrid& grid);

// The sheet triple of a formation cell: x along the length (F0), z across the width (F1), y zero.
Point3 SheetTriple(float halfLength, float halfWidth, const Cell& cell, const SheetGrid& grid);

// The slot-point rows of one phase, in slot order. Slot 0 is the keeper row for every phase
// (FUN_141FC1590: `if (slot == 0)` stores the keeper record); slots 1..10 are their cells' triples.
std::array<Point3, kSlots> SlotPointRows(const Plan& plan, int phase, float halfLength, float halfWidth,
                                         const SheetGrid& grid, const Point3& keeper);

// The install's tables, with each record's plan built. Load reads the VFS (WESYS-unwrapped).
class FormationTables {
 public:
  static std::optional<FormationTables> Load(const pes::PesVfs& vfs, std::string* error);
  static std::optional<FormationTables> Parse(std::span<const uint8_t> tactics,
                                              std::span<const uint8_t> formations, std::string* error);
  // The plan a team's tactics record fills (bit 24 = plan), or nothing if the team has no such record.
  std::optional<Plan> PlanOf(uint32_t teamId, int plan) const;
  size_t RecordCount() const { return records_.size(); }

 private:
  struct Record {
    Tactics tactics;
    Plan plan;
  };
  std::vector<Record> records_;
};

// The grid constants at their image addresses. Fails when the image does not hold them.
std::optional<SheetGrid> ReadSheetGrid(const pes::PeImage& exe, std::string* error);

// FUN_141FC1590's keeper record: `mov dword [rbp-0x78], 0xC2480000` (c7 45 88 imm32 at 0x141FC1954)
// stores the keeper's x, -50.0; y and z are zero.
std::optional<Point3> ReadKeeperRow(const pes::PeImage& exe, std::string* error);

// The sheet constants one match reads from the exe: the grid and the keeper row.
struct SheetConstants {
  SheetGrid grid;
  Point3 keeper;
};
std::optional<SheetConstants> ReadSheetConstants(const pes::PeImage& exe, std::string* error);

}  // namespace pes::formation
