#pragma once

// PES 2021's mentality chain, as decoded in docs/re/pes-re-role-rebuild.md §2 and
// docs/re/pes-re-mentality-blocks.md §1-§3 (the port order of §7). Pure functions; every PES
// constant comes from the install (PeImage::ReadVa at the VA named beside it), none is typed here.
//
// The chain, run once per team tick by FUN_1405EC600:
//   1. the role level picks the matrix row each slot's role is read from (RoleLevel);
//   2. the shared reclassify maps each slot's class to its line (ReclassifyLine);
//   3. the possession flag F picks block A (out of possession) or block B (in possession) (BlockFor);
//   4. block B's fold pick chooses the slots it folds (FoldPicks).

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace pes {
class PeImage;
}

namespace pes::mentality {

inline constexpr int kSlots = 11;  // a side's slots (EC600's 11-entry loops)

// Lines of the reclassify (blocks §2): 0 back, 1 middle, 2 forward.
inline constexpr int kBackLine = 0;
inline constexpr int kMidLine = 1;
inline constexpr int kForwardLine = 2;

// MI+0x116C, the play state: 1 is open play.
inline constexpr int kPlayStateInPlay = 1;
// MI+0x1554 == 1, the restart kind the role level's override tests (pesclock.cpp's kRestartKindOne).
inline constexpr int kRestartKindOne = 1;

// Role level (role-rebuild §2, EC600's `NEG R9B; SBB EBP,EBP; ADD EBP,2` and the CMOVZ override):
// 2 when the team is out of possession, 1 when it has the ball, and 0 when the play state is not
// open play and MI+0x1554 == 1. The matrix row read is the level (RoleRow in pesroles).
int RoleLevel(bool possession, int playState, int restartKind);

// The shared reclassify (blocks §2). FUN_1405EC600 dispatches class -> line through a jump table
// at VA 0x1405ED1F4 (nine RVAs, index class - 1, dispatch 0x1405ECB3D-47); each arm stores its
// line (MOV EDI,imm32 at 0x1405ECB49 and 0x1405ECB53, or the default arm at 0x1405ECB5D).
struct LineTable {
  std::array<int, 9> lineOfClass{};  // index = class - 1, for classes 1..9
};
std::optional<LineTable> LoadReclassifyLines(const PeImage& exe, std::string* error);
// A class 1..9 takes its line; any other class (GK, and past 9) takes the default arm, the back line.
int ReclassifyLine(const LineTable& table, int cls);

// The line of a class through the stock reclassify (MentalityPort doc §15): 3-6 -> 1, 7-9 -> 2,
// else 0. For the lane writers, which need the class line without the install table.
int ReclassifyLineOf(int cls);
// The block choice (blocks §1): F = (T+0x255 != 0), tested at EC600 @ 0x1405ECC21. F == 0 runs
// block A (681FF0 -> 682770), F != 0 runs block B (67B640).
enum class Block { kOutOfPossession, kInPossession };
Block BlockFor(bool possession);

// Block B's fold pick (blocks §3, FUN_14067C4B0). Both constants are float operands in the image.
struct FoldConstants {
  float forwardPenalty = 0.0f;  // DAT_14259D634 = 10000.0, the MOVSS XMM12 at 0x14067CDE3
  float initialBest = 0.0f;     // DAT_1425C65FC = 100000.0, the pick's starting minimum
};
std::optional<FoldConstants> LoadFoldConstants(const PeImage& exe, std::string* error);

struct FoldCandidate {
  bool onPitch = false;
  int cls = 0;     // the B class: 0 GK .. 9 CF
  float x = 0.0f;  // the candidate position, x and z of the team frame (67C4B0's local_238)
  float z = 0.0f;
};

// The fold pick: `count` picks, each the candidate of least score, where score = x*x + z*z plus
// forwardPenalty for a class above 5 (0x14067CE72). A candidate must score below initialBest
// (0x14067CE7D). The GK, the players off the pitch, the protected slot and the slots already picked
// are never candidates (0x14067CDB3, 0x14067CD66, 0x14067CD9D). A full-attack team (T+0xB8A2) runs
// no pass (0x14067CDFB).
std::vector<int> FoldPicks(const FoldConstants& k, const std::array<FoldCandidate, kSlots>& cands,
                           int protectedSlot, int count, bool fullAttack);

// Stand-in for the fold count T+0x110, whose writer is not decoded. The live reading
// (docs/re/pes-re-mentality-reclass-live.md §2): one fold at dial -1, two at -2, none at neutral.
int FoldCount(int dial);

// Stand-in for the protected forward (planObj+0x4B40, the CF shield): the last slot of class 9, or
// -1. The full protected scan (pesforward's ProtectedSlot, R5) is not wired yet.
// The out-of-possession lane line (FUN_1405CE150/CE4F0, lane-writers §§2-3; the wrapper map at
// blockA-forward-pull §2). Pure over GF-carried inputs; every arm needing a missing PES word
// (the A64410 special predicate + F760/F490 distances, the 15/instruction table, the band rows,
// the post-step writers, the T+0x78 tree) stays a `// ponytail:` leaf at its VA.
struct LaneInput {
  std::array<int, kSlots> classes{};  // B classes 0..9 (P+0x3FB8)
  std::array<int, kSlots> active{};   // MI+0x1748, 0 = inactive; all active when all zero
  int designated = -1;                // P+0x3EF8, -1 when none (GF: the publisher has no slot id yet)
};
// FUN_1405CE4F0's stoppage step (lane-writers §2 step 3): MI+0x11D4 == 0 and (MI+0x116C == 2 or
// MI+0x11B0 < 0x15) clears the copy flag LX+0x1B0, so the copy path runs once. GF passes its
// WriteRoleArrays play state (PesPlayState: 1 in play, 2 stopped) for MI+0x116C.
bool StoppageResetsCopyFlag(int playState116c, int word11d4, int word11b0);
// CE4F0's copy gate (flag LX+0x1B0 == 0): true runs the copy path, false the recompute.
bool CopyPathRuns(bool copyFlagSet);

// FUN_1405CE150, GF-carried arms only: keeper -> 3, designated -> 2, else the reclassify of the
// slot's class. Inactive slots take their class line (the recompute path overwrites them with 3;
// CE4F0 copy path keeps the B words for all 11, including inactive).
std::array<int, kSlots> LaneBWords(const LaneInput& in);

// FUN_140A63950 (lane-post §4d): the instruction table, four (id, slot) rows per side
// (MA+0x25CF8/0x25D38). GF carries no instruction table: an all-zero table claims nothing
// (no row can match id 0xF/0x10), which is the exact rule over GF's input.
struct InstrTable {
  std::array<std::array<int, 4>, 2> ids{};
  std::array<std::array<int, 4>, 2> slots{};
};
// Nonzero iff some row j < 4 of the side holds (id, slot) (VA 0x140A63950; side < 2, id < 17).
bool InstructionClaimed(const InstrTable& table, int side, int id, int slot);

// FUN_140A64410 (lane-post §1): the SMF/WF special predicate. `slot` is the local k, `roles`
// the side's T+0x4C00 row (roles.b 1:1), `active` MI+0x1748, `slotF0` the S+0x3A9C f0 column
// (slotPoints[k].x), `secondF0` the T+0x46EC column. GF carries no T+0x46EC: pass zeros with
// the ponytail at the call (the sign arm is unreachable with an empty table, step 2).
struct SpecialInput {
  int slot = 0;                          // local k 0..10
  int side = 0;                          // 0 or 1
  InstrTable instr;                      // MA+0x25CF8/0x25D38
  std::array<int, kSlots> roles{};       // T+0x4C00
  std::array<int, kSlots> active{};      // MI+0x1748
  std::array<float, kSlots> slotF0{};    // S+0x3A9C f0
  std::array<float, kSlots> secondF0{};  // T+0x46EC
};
// 1 = special (CE150 takes the CCD50 branch), 0 = normal (CE150 takes CD540).
bool SpecialPredicate(const SpecialInput& in);

// FUN_1405CD540 (lane-post §2a): B[k] 0 -> 3; 1-2 -> 0; 3-6 -> 1; 7-9 -> 2.
int Cd540Line(int cls);

// FUN_1405CCD50's float operands (lane-post §0, script-read from the install):
// DAT_14259B89C = 0.4, DAT_142565600 = 0.5, DAT_14257A190 = 0.6.
struct LaneThresholds {
  float backZero = 0.0f;     // 0.4: the back arm's threshold when B[k] == 0
  float backNonZero = 0.0f;  // 0.5: the back arm's threshold otherwise
  float fwdOne = 0.0f;       // 0.5: the forward arm's threshold when B[k] == 1
  float fwdOther = 0.0f;     // 0.6: the forward arm's threshold otherwise
};
std::optional<LaneThresholds> LoadLaneThresholds(const PeImage& exe, std::string* error);

// FUN_1405CCD50 (lane-post §2): the distance branch. `backDist`/`fwdDist` are the
// FUN_14063F760/14063F490 scores (interiors INF, lane-post §2b); `fwdProductNeg` is the
// T+0x254-signed MA+0x2482C product < 0. The debug ring (LX+0x1B4/0x1BC/0x1C4) has no
// reader that affects the word, so only the return value ports. Returns {0, 1, 2}.
struct Ccd50Input {
  int cls = 0;              // the B class of the slot (local_148 record)
  int playState116c = 1;    // MI+0x116C
  int mi54 = 0;             // MI+0x54 (ponytail: no GF word)
  int restartKind1554 = 0;  // MI+0x1554
  float backDist = 0.0f;    // FUN_14063F760 score (ponytail: interior INF)
  float fwdDist = 0.0f;     // FUN_14063F490 score (ponytail: interior INF)
  bool fwdProductNeg = false;  // T+0x254-signed MA+0x2482C product < 0 (ponytail: analysis ball)
};
int Ccd50Word(const LaneThresholds& t, const Ccd50Input& in);

// FUN_1405CE930's head (lane-leaves §3.1): the recompute-path A-rank writer. `order` is the
// side's depth-sorted slot list (SortLaneSlots over Ce930Key values); `a` the current A-words.
// Per slot in order: active (MI+0x1748), on-pitch (class != 0) slots take the count of earlier
// same-word slots; the count starts at 0 per word. Slots off the path keep 0xA0 (the 0xA0 fill).
std::array<uint8_t, kSlots> Ce930Ranks(const std::array<int, kSlots>& a,
                                       const std::array<uint8_t, kSlots>& order,
                                       const std::array<int, kSlots>& cls,
                                       const std::array<int, kSlots>& active);
// FUN_1405CE930's key (VA 0x1405CE930, lane-leaves §3.1): a special (A64410) slot whose
// S+0x3AA4-depth row (slotPoints[k].z) is above 0 uses the +100000.0 override
// (DAT_1425C65FC); otherwise the slot keeps its |depth - ballX| key.
float Ce930Key(float depthKey, bool special, float f2);

// FUN_14063FA70 (lane-post §4): per-line on-pitch headcount. Counts active slots with class
// != 0 whose reclassified class (ReclassifyLineOf) == line.
int Fa70Count(const std::array<int, kSlots>& cls, const std::array<int, kSlots>& active, int line);
// FUN_140A4C720 (lane-post §7): (c*z + x)*c < c*y; the float (int)c sign-selects the side.
bool OpponentLine(float x, float y, int sideSign, float z);
// FUN_1405CCD00 (lane-post §4d): nonzero iff the action state word is 0x3A (58).
// GF has no 0x3A word yet (run states max at kMark 0x36): the caller passes the man's state.
inline constexpr int kActionState38 = 0x3A;
bool Is38State(int actionState);
// FUN_1405CD780 (lane-post §4d): nonzero iff out of play (MI+0x116C != 1) at restart kind 4
// (MI+0x1554 == 4) with an instruction-15 claim (FUN_140A63950) on the slot.
bool Cd780Claimed(const InstrTable& table, int side, int slot, int playState116c, int restartKind1554);

// FUN_1405CFC80's first loop (lane-post §4b): the A := 1 pass. `backTarget` is CD0F0,
// `forwardTarget` CD320; `designated` P+0x3EF8 (-1 none); `outlet` T+0x4B40 (-1 none);
// `states` the T+0xA2A0 words; `fullAttack` T+0xB8A2; `attackLevel` T+0xB8A4.
// GF carries no 0x3A states and no outlet word: pass zeros (no slot 0x3A) and -1.
struct Cfc80Input {
  std::array<int, kSlots> a{};       // the current A-words (LX+0x8 records)
  std::array<int, kSlots> b{};       // the B-words (LX+0x60 records)
  std::array<int, kSlots> cls{};     // the B classes (P+0x3FB8)
  std::array<int, kSlots> active{};  // MI+0x1748
  std::array<int, kSlots> states{};  // T+0xA2A0 action states
  int designated = -1;               // P+0x3EF8
  int outlet = -1;                   // T+0x4B40
  int forwardTarget = 0;             // CD320
  int backTarget = 0;                // CD0F0
  bool fullAttack = false;           // T+0xB8A2
  int attackLevel = 0;               // T+0xB8A4
};
// Runs the first loop only (slot order; the sorted-order second loop needs the A71500 table).
std::array<int, kSlots> Cfc80FirstLoop(const Cfc80Input& in);

// FUN_1405CE150's B-rank (lane-writers §3): per slot, the count of earlier same-B-word slots in the
// sorted order. `order` is the sorted slot list (SortLaneSlots).
std::array<uint8_t, kSlots> LaneBRank(const std::array<int, kSlots>& b,
                                      const std::array<uint8_t, kSlots>& order);

// The wrapper map (0x140682090-0x1406820B4): A 0 -> lane 1, 1 -> 4, 2 -> 7, else 0 (skipped).
int LaneOfWord(int a);

// FUN_1405CE4F0's recompute, GF-carried arms only: every word := 3; active slots with word > 1 and
// (slot == designated or B == 2) -> 2; inactive -> 3, else active with word > 2 take their B word.
// ponytail: the CF310/CFC80/CFFE0 post-steps run before this loop at CE4F0 l.65-140.
void LaneAWords(const std::array<int, kSlots>& b, std::array<int, kSlots>& a, int designated,
                const std::array<int, kSlots>& active = {});

// CE4F0's copy path (flag LX+0x1B0 == 0): A-record := B-record for all 11 slots.
void CopyLaneAWords(const std::array<int, kSlots>& b, std::array<int, kSlots>& a);

// FUN_140A79640 (lane-leaves §1): ascending by float key, stable (ties keep slot order); order[i]
// is the i-th slot, rank[slot] the index. Only param_2 == 0 is ported (no lane caller uses the
// other arm).
void SortLaneSlots(const std::array<uint8_t, kSlots>& slots, const std::array<float, kSlots>& keys,
                   std::array<uint8_t, kSlots>& order, std::array<uint8_t, kSlots>& rank);
int ProtectedSlot(const std::array<int, kSlots>& classes);

}  // namespace pes::mentality
