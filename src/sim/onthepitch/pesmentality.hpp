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
// The play state the role level reads (plan-selector §1 vs roles §2): 1 while the ball is in play,
// even with a set piece armed or taken (the seed-7 run's 754 ticks take PES's in-play arm there);
// 2 in every stoppage. The set-piece flag is deliberately not an input: EC600's override tests
// MI+0x116C == 1 first, so it cannot fire in play.
inline int RolePlayState(bool inPlay, bool inSetPiece) {
  (void)inSetPiece;
  return inPlay ? kPlayStateInPlay : 2;
}

// The shared reclassify (blocks §2). FUN_1405EC600 dispatches class -> line through a jump table
// at VA 0x1405ED1F4 (nine RVAs, index class - 1, dispatch 0x1405ECB3D-47); each arm stores its
// line (MOV EDI,imm32 at 0x1405ECB49 and 0x1405ECB53, or the default arm at 0x1405ECB5D).
struct LineTable {
  std::array<int, 9> lineOfClass{};  // index = class - 1, for classes 1..9
};
std::optional<LineTable> LoadReclassifyLines(const PeImage& exe, std::string* error);
// A class 1..9 takes its line; any other class (GK, and past 9) takes the default arm, the back line.
int ReclassifyLine(const LineTable& table, int cls);

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
int ProtectedSlot(const std::array<int, kSlots>& classes);

}  // namespace pes::mentality
