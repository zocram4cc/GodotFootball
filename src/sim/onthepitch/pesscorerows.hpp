// PES 2021's S1 rows: the team-AI tables the carrier lane units read for goal and touchline
// distances, bearings, and each player's nearest-player list. Writer FUN_14045B420
// (docs/re/pes-re-s1-writer.md, Main audit rev 2). It runs on the listener's step 5 (FUN_140457070
// at 0x140457119). Its mask picks the blocks one call refreshes: 0x01 score0, list0 and angle0;
// 0x02 score1, list1 and angle1; 0x04 the two pair rows. The NearestList is rebuilt on every call,
// so a block is up to four logic frames old.
#ifndef _HPP_ONTHEPITCH_PESSCOREROWS
#define _HPP_ONTHEPITCH_PESSCOREROWS

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pes {
class PesWorld;
}  // namespace pes

// The lane core's slot space. The row writer owns the layout; peslane.hpp includes it for these names.
namespace pes::lane {
inline constexpr int kSlots = 22;     // both sides
inline constexpr int kSideSize = 11;  // players per side
inline constexpr int kNoSlot = 0xFF;  // PES's "no slot" result
}  // namespace pes::lane

namespace pes {

// The pitch marks the writer reads.
struct ScoreRowConstants {
  float goalX_m = 0.0f;   // xg: the goal-centre x, pitch mark G+0x2AA8+0 (gametypes pesFieldHalfLength)
  float goalY_m = 0.0f;   // yg: pitch mark +4 (gametypes pesFieldHalfWidth)
};

// The player fields the writer reads: x (+0x55C), z (+0x564), the on-pitch flag (MI+0x1748), and his man record (the
// pair table's bearings run over it).
struct RowPlayer {
  float x = 0.0f;
  float z = 0.0f;
  bool onPitch = false;
  uint8_t* man = nullptr;  // his man record (pesworldrecord.hpp): the pair table's bearings run over it on the PES path
};
using RowPlayers = std::array<RowPlayer, lane::kSlots>;

// One listed player: its slot and the value its block holds for it.
struct RowEntry {
  int id = -1;
  float value = 0.0f;
};

// A sorted block's layout in MA (pes-re-A4Carrier.md section 4: the 200-byte per-player block): team t's 11 entries of
// (slot id u32, value f32) at +0x58 t, the two count bytes at +0xB0, the rank bytes at +0xB2 + id. The unlisted fill is
// 0xFF (INF: the count and rank bytes' width is the NearestEntry layout of pesoffball.hpp).
inline constexpr std::size_t kRowBytes = 200;
inline constexpr std::size_t kRowTeamStride = 0x58;
inline constexpr std::size_t kRowCountOffset = 0xB0;
inline constexpr std::size_t kRowRankOffset = 0xB2;
inline constexpr uint8_t kRowUnlisted = 0xFF;

// A PES row, a view over its block in the world window: each team's listed players ascending by value (team 1 is slots
// 11..21), and each id's position within its team (the rank byte at +0xB2 + id). The block is the store; the view holds
// no state, and an unbound view is an empty row.
class SortedRow {
 public:
  SortedRow() = default;
  explicit SortedRow(uint8_t* block) : block_(block) {}
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
  int Count(int side) const { return block_ != nullptr ? block_[kRowCountOffset + side] : 0; }
  RowEntry Entry(int side, int rank) const;

 private:
  uint8_t* block_ = nullptr;
};

// A row of f32 in the window (a 22-slot angle or bearing row).
class FloatRow {
 public:
  FloatRow() = default;
  explicit FloatRow(uint8_t* p) : p_(p) {}
  float operator[](int i) const;
  void Set(int i, float v) const;
  void Fill(float v) const;

 private:
  uint8_t* p_ = nullptr;
};

// The S1 rows are views over the side's MA record; Bind() points them at it. A default ScoreRows owns a scratch MA of its
// own for the unit tests that build rows with no world; the match binds the window's.
struct ScoreRows {
  ScoreRows();
  ScoreRows(const ScoreRows&) = delete;
  ScoreRows& operator=(const ScoreRows&) = delete;
  void Bind(uint8_t* ma);
  // score[0]: squared distance to (-xg, 0), block MA+0x228EC. score[1]: to (+xg, 0), MA+0x22AD4.
  std::array<SortedRow, 2> score{};
  // list[0]: |-xg - x|, block MA+0x229B4. list[1]: |+xg - x|, MA+0x22B9C.
  std::array<SortedRow, 2> list{};
  // pair[0]: |yg - z|, block MA+0x22CBC. pair[1]: |yg + z|, MA+0x22D84.
  std::array<SortedRow, 2> pair{};
  // angle[0]: bearing about (-xg, 0), MA+0x22A7C. angle[1]: about (+xg, 0), MA+0x22C64. Degrees
  // in [0, 360); 0 for a slot that is off the pitch.
  std::array<FloatRow, 2> angle{};
  // bearing[a][b]: the bearing of (a - b) in degrees in [0, 360), 0 when the two coincide
  // (MA+0x23F7C + 0x58 a + 4 b, writer FUN_14045B420, reader FUN_140439FE0). Rebuilt on every
  // call, whatever the mask. Off-pitch rows stay 0.
  std::array<FloatRow, lane::kSlots> bearing{};
  // nearest[i]: every other on-pitch player by squared distance from i, MA+0x22E4C + 200 i.
  // Empty when i is off the pitch.
  std::array<SortedRow, lane::kSlots> nearest{};

 private:
  std::vector<uint8_t> scratch_;
  uint8_t* bound_ = nullptr;  // a fresh bind starts every row empty (PES's zeroed record would list slot 0 at rank 0)
};

// The row FUN_140A714C0(T, k) reads for attack sign k: score0 (MA+0x228EC, the goal at -xg) when
// k == 1, score1 otherwise. Its d^2 is the score of G5 and the DHB, and Safety gate 4 (0x140587E70).
const SortedRow& ScoreRowForSign(const ScoreRows& rows, int k);
// The row FUN_140A714A0(T, k) reads: the other goal's row, the Safety gate's lane length (0x14212B71D).
const SortedRow& GateRowForSign(const ScoreRows& rows, int k);

// The writer's body for the blocks `mask` selects. The NearestList is rebuilt on every call.
// `pesPath` gates the pair-bearing table (FUN_140439FE0 family): GF mode keeps
// its old rows exactly (all zeroes there).
void BuildScoreRows(const RowPlayers& players, const ScoreRowConstants& k, uint8_t mask, ScoreRows& rows,
                    bool pesPath, PesWorld& world);

// The writer as the team tick drives it: the TA+0x11 latch and the frame rotation.
class ScoreRowWriter {
 public:
  // One call on the step-5 path. `fullRefresh` is FUN_140457070's flag argument: it latches the
  // writer and rewrites every block (mask 0xFF). Otherwise the mask is the rotation of `frame`
  // (MI+0x1198 & 3 onto 0x01, 0x02, 0x24, 0x10), and nothing is written until the latch is set.
  // `pesPath` gates the pair-bearing table: a PES-only read (FUN_140439FE0 family), so GF mode
  // keeps its old rows exactly (all zeroes there).
  void Tick(const RowPlayers& players, const ScoreRowConstants& k, unsigned long frame, bool fullRefresh,
            bool pesPath, PesWorld& world);
  const ScoreRows& Rows() const { return rows_; }
  bool Latched() const { return latched_; }

  // Points the rows at the side's MA record in the world window.
  void Bind(uint8_t* ma) { rows_.Bind(ma); }

 private:
  ScoreRows rows_;
  bool latched_ = false;
};

}  // namespace pes

#endif
