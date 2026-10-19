// PES 2021's S1 rows: the team-AI tables the carrier lane units read for goal and touchline
// distances, bearings, and each player's nearest-player list. Writer FUN_14045B420
// (docs/re/pes-re-s1-writer.md, Main audit rev 2). It runs on the listener's step 5 (FUN_140457070
// at 0x140457119). Its mask picks the blocks one call refreshes: 0x01 score0, list0 and angle0;
// 0x02 score1, list1 and angle1; 0x04 the two pair rows. The NearestList is rebuilt on every call,
// so a block is up to four logic frames old.
#ifndef _HPP_ONTHEPITCH_PESSCOREROWS
#define _HPP_ONTHEPITCH_PESSCOREROWS

#include <array>
#include <cstdint>

// The lane core's slot space. The row writer owns the layout; peslane.hpp includes it for these names.
namespace pes::lane {
inline constexpr int kSlots = 22;     // both sides
inline constexpr int kSideSize = 11;  // players per side
inline constexpr int kNoSlot = 0xFF;  // PES's "no slot" result
}  // namespace pes::lane

namespace pes {

// The pitch marks and the angle constants the writer reads.
struct ScoreRowConstants {
  float goalX_m = 0.0f;   // xg: the goal-centre x, pitch mark G+0x2AA8+0 (gametypes pesFieldHalfLength)
  float goalY_m = 0.0f;   // yg: pitch mark +4 (gametypes pesFieldHalfWidth)
  float radToDeg = 0.0f;  // 0x14259BE40: the bearing scale, 180/pi
  float twoPi = 0.0f;     // 0x1425795E4: 2 pi, folds a negative bearing into [0, 2 pi)
};

// The player fields the writer reads: x (+0x55C), z (+0x564) and the on-pitch flag (MI+0x1748).
struct RowPlayer {
  float x = 0.0f;
  float z = 0.0f;
  bool onPitch = false;
};
using RowPlayers = std::array<RowPlayer, lane::kSlots>;

// One listed player: its slot and the value its block holds for it.
struct RowEntry {
  int id = -1;
  float value = 0.0f;
};

// A PES row: each team's listed players ascending by value (team 1 is slots 11..21), and each
// id's position within its team (the rank byte at +0xB2 + id).
class SortedRow {
 public:
  SortedRow();
  void Clear();
  // Lists `id` with `value`. Seal() sorts the teams and sets the ranks.
  void Add(int id, float value);
  void Seal();
  // The value listed for `id`; NaN when it is not listed (PES reads its 0xFF fill there).
  float ValueOf(int id) const;
  // The position of `id` in its team's list, or -1 when it is not listed.
  int RankOf(int id) const;
  // The head of team `side`'s list (its nearest), or lane::kNoSlot when that team lists nobody.
  int Nearest(int side) const;
  // The block `side` (0: lanes 0..10, 1: lanes 11..21): its count, and its entry at `rank`, nearest first.
  int Count(int side) const { return count_[side]; }
  const RowEntry& Entry(int side, int rank) const { return entry_[side][rank]; }

 private:
  std::array<std::array<RowEntry, lane::kSideSize>, 2> entry_{};
  std::array<int, 2> count_{};
  std::array<int, lane::kSlots> rank_{};
};

struct ScoreRows {
  // score[0]: squared distance to (-xg, 0), block MA+0x228EC. score[1]: to (+xg, 0), MA+0x22AD4.
  std::array<SortedRow, 2> score{};
  // list[0]: |-xg - x|, block MA+0x229B4. list[1]: |+xg - x|, MA+0x22B9C.
  std::array<SortedRow, 2> list{};
  // pair[0]: |yg - z|, block MA+0x22CBC. pair[1]: |yg + z|, MA+0x22D84.
  std::array<SortedRow, 2> pair{};
  // angle[0]: bearing about (-xg, 0), MA+0x22A7C. angle[1]: about (+xg, 0), MA+0x22C64. Degrees
  // in [0, 360); 0 for a slot that is off the pitch.
  std::array<std::array<float, lane::kSlots>, 2> angle{};
  // nearest[i]: every other on-pitch player by squared distance from i, MA+0x22E4C + 200 i.
  // Empty when i is off the pitch.
  std::array<SortedRow, lane::kSlots> nearest{};
};

// The row FUN_140A714C0(T, k) reads for attack sign k: score0 (MA+0x228EC, the goal at -xg) when
// k == 1, score1 otherwise. Its d^2 is the score of G5 and the DHB, and Safety gate 4 (0x140587E70).
const SortedRow& ScoreRowForSign(const ScoreRows& rows, int k);
// The row FUN_140A714A0(T, k) reads: the other goal's row, the Safety gate's lane length (0x14212B71D).
const SortedRow& GateRowForSign(const ScoreRows& rows, int k);

// The writer's body for the blocks `mask` selects. The NearestList is rebuilt on every call.
void BuildScoreRows(const RowPlayers& players, const ScoreRowConstants& k, uint8_t mask,
                    ScoreRows& rows);

// The writer as the team tick drives it: the TA+0x11 latch and the frame rotation.
class ScoreRowWriter {
 public:
  // One call on the step-5 path. `fullRefresh` is FUN_140457070's flag argument: it latches the
  // writer and rewrites every block (mask 0xFF). Otherwise the mask is the rotation of `frame`
  // (MI+0x1198 & 3 onto 0x01, 0x02, 0x24, 0x10), and nothing is written until the latch is set.
  void Tick(const RowPlayers& players, const ScoreRowConstants& k, unsigned long frame,
            bool fullRefresh);
  const ScoreRows& Rows() const { return rows_; }
  bool Latched() const { return latched_; }

 private:
  ScoreRows rows_{};
  bool latched_ = false;
};

}  // namespace pes

#endif
