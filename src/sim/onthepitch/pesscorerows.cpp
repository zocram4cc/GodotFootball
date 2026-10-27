#include "onthepitch/pesscorerows.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include "onthepitch/pesworldrecord.hpp"

namespace pes {

namespace {

// The mask bits FUN_14045B420 tests (`test $1`, `$2`, `$4` on its mask byte).
constexpr uint8_t kMaskScore0 = 0x01;
constexpr uint8_t kMaskScore1 = 0x02;
constexpr uint8_t kMaskPairs = 0x04;
// FUN_140457070's flag branch passes this mask (`local_res10 = 0xff`).
constexpr uint8_t kMaskAll = 0xFF;
// FUN_140457070's rotation of MI+0x1198 & 3 onto the mask (0x140457097..0x1404570C4).
constexpr uint8_t kRotation[4] = {0x01, 0x02, 0x24, 0x10};

// Slots 0..10 are the first team and 11..21 the second (the writer's `j >= 11` split).
int TeamOf(int slot) { return slot >= lane::kSideSize ? 1 : 0; }


}  // namespace

SortedRow::SortedRow() { Clear(); }

void SortedRow::Clear() {
  count_.fill(0);
  rank_.fill(-1);
}

void SortedRow::Add(int id, float value) {
  const int team = TeamOf(id);
  entry_[team][count_[team]++] = RowEntry{id, value};
}

void SortedRow::Seal() {
  for (int team = 0; team < 2; ++team) {
    auto first = entry_[team].begin();
    auto last = first + count_[team];
    // A stable sort keeps the lower slot first on a tie.
    std::stable_sort(first, last,
                     [](const RowEntry& a, const RowEntry& b) { return a.value < b.value; });
    for (int r = 0; r < count_[team]; ++r) rank_[entry_[team][r].id] = r;
  }
}

float SortedRow::ValueOf(int id) const {
  if (id < 0 || id >= lane::kSlots || rank_[id] < 0)
    return std::numeric_limits<float>::quiet_NaN();
  return entry_[TeamOf(id)][rank_[id]].value;
}

int SortedRow::RankOf(int id) const {
  if (id < 0 || id >= lane::kSlots) return -1;
  return rank_[id];
}

int SortedRow::Nearest(int side) const {
  return count_[side] > 0 ? entry_[side][0].id : lane::kNoSlot;
}

void BuildScoreRows(const RowPlayers& p, const ScoreRowConstants& k, uint8_t mask, ScoreRows& rows, bool pesPath,
                    PesWorld& world) {
  // The score, list and angle blocks of each goal: side 0 measures from (-xg, 0), side 1 from
  // (+xg, 0). Each is written only when its mask bit is set.
  for (int side = 0; side < 2; ++side) {
    const uint8_t bit = side == 0 ? kMaskScore0 : kMaskScore1;
    if ((mask & bit) == 0) continue;
    const float centre = side == 0 ? -k.goalX_m : k.goalX_m;
    SortedRow& score = rows.score[side];
    SortedRow& list = rows.list[side];
    score.Clear();
    list.Clear();
    rows.angle[side].fill(0.0f);
    for (int i = 0; i < lane::kSlots; ++i) {
      if (!p[i].onPitch) continue;
      const float dx = centre - p[i].x;
      score.Add(i, dx * dx + p[i].z * p[i].z);
      list.Add(i, std::fabs(dx));
      // A slot exactly on the goal centre has bearing 0 (the writer's zero case).
      if (!(p[i].z == 0.0f && p[i].x == centre))
        rows.angle[side][i] = world.BearingOfPoints(centre, 0.0f, p[i].x, p[i].z);
    }
    score.Seal();
    list.Seal();
  }

  if ((mask & kMaskPairs) != 0) {
    SortedRow& above = rows.pair[0];
    SortedRow& below = rows.pair[1];
    above.Clear();
    below.Clear();
    for (int i = 0; i < lane::kSlots; ++i) {
      if (!p[i].onPitch) continue;
      above.Add(i, std::fabs(k.goalY_m - p[i].z));
      below.Add(i, std::fabs(-k.goalY_m - p[i].z));
    }
    above.Seal();
    below.Seal();
  }

  // The pair-bearing table (MA+0x23F7C, FUN_14045B420's owner loop at 0x14045BB80-0x14045BD27):
  // a PES-only read (FUN_140439FE0 family), so GF mode keeps its old rows
  // exactly: off the PES path the table stays zero, while the NearestList
  // below still rebuilds (GF's own readers use it).
  if (pesPath) {
    for (auto& row : rows.bearing) row.fill(0.0f);
    for (int a = 0; a < lane::kSlots; ++a) {
      if (!p[a].onPitch) continue;
      for (int b = 0; b < lane::kSlots; ++b) {
        if (b == a || !p[b].onPitch) continue;
        const float dx = p[a].x - p[b].x;
        const float dz = p[a].z - p[b].z;
        if (dx == 0.0f && dz == 0.0f) continue;  // coincident: the row stays 0
        rows.bearing[a][b] = world.Bearing(p[b].man, p[a].man);
      }
    }
  }

  // The NearestList is rebuilt on every call, whatever the mask: each on-pitch slot lists every
  // other on-pitch slot by squared distance. An off-pitch slot has an empty list.
  for (int i = 0; i < lane::kSlots; ++i) {
    SortedRow& row = rows.nearest[i];
    row.Clear();
    if (!p[i].onPitch) continue;
    for (int j = 0; j < lane::kSlots; ++j) {
      if (j == i || !p[j].onPitch) continue;
      const float dx = p[i].x - p[j].x;
      const float dz = p[i].z - p[j].z;
      row.Add(j, dx * dx + dz * dz);
    }
    row.Seal();
  }
}

void ScoreRowWriter::Tick(const RowPlayers& players, const ScoreRowConstants& k, unsigned long frame, bool fullRefresh,
                          bool pesPath, PesWorld& world) {
  if (fullRefresh) latched_ = true;  // FUN_140457070's flag branch sets TA+0x11
  if (!latched_) return;             // and the call returns early while TA+0x11 is 0
  const uint8_t mask = fullRefresh ? kMaskAll : kRotation[frame & 3u];
  BuildScoreRows(players, k, mask, rows_, pesPath, world);
}

// The selectors of the S1 rows by attack sign (see the header): FUN_140A714C0 and FUN_140A714A0.
const SortedRow& ScoreRowForSign(const ScoreRows& rows, int k) { return k == 1 ? rows.score[0] : rows.score[1]; }
const SortedRow& GateRowForSign(const ScoreRows& rows, int k) { return k == 1 ? rows.score[1] : rows.score[0]; }
}  // namespace pes

