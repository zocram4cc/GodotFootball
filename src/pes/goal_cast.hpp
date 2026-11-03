#pragma once

// The goal scene's cast: the fdc Human slots the cut cameras name in their target bytes (record +0xB1..+0xB3, the registry
// slot CAMERA_TARGET_02nn is slot + 1: pes-port-ProcCamera8.md section 1) and the man each slot is. A cast entry is
// [id u32][kind][side][roster slot][flag] with id 0x1300 + the fdc slot (0x141F56600); the goal builder 0x141F5FC00 writes the
// entry of slot 0x15 as (side S ^ 1, roster slot 0) and the scorer's as the event man. Checked against the cast lists of the
// captured goals by tools/dev/cutscene/test_goal_cast.py (scorer entry side S -> entry 0x1315 side S ^ 1, roster slot 0).

namespace pes {

inline constexpr unsigned kCastScorer = 0;           // the event man (ctx+0x5214 side, ctx+0x5218 roster slot)
inline constexpr unsigned kCastConcedingKeeper = 21;  // 0x1315: the other side's roster slot 0
inline constexpr unsigned kCastReferee = 22;          // 0x1316: official kind, roster slot 0 (seq142542: registry slot 23)

// The conceding keeper's side: the side that did not score.
constexpr unsigned ConcedingSide(unsigned scorerSide) { return scorerSide ^ 1u; }
// His roster slot (the starting keeper's line).
inline constexpr unsigned kConcedingKeeperRosterSlot = 0;

}  // namespace pes
