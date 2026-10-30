// PES's kick motion pick (docs/ports/pes-port-KickMotion.md): the motion id a Kick action starts and the descriptor byte Q's
// in-flight branch reads from it. The Kick action start (FUN_140727230) does
//   row    = FUN_14071EC10 -> FUN_14071E6F0(ap, request, kind, band, side, scale)   KickRow: the clip table + fix-ups
//   motion = FUN_14071E570(ap, row, ap+0x2755, ..)                                  KickMotion: Animetable cell (row, column)
//   Q reads FUN_1406B0250(motion table, motion, 0)                                  KickMotionDescriptor
// and ap+0x2755 (the column) is FUN_140723DE0's angle bucket, KickBucket.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "onthepitch/peskickerror.hpp"
#include "pes/animetable.hpp"
#include "pes/exe.hpp"
#include "pes/mbinfo.hpp"
#include "pes/vfs.hpp"

namespace pes {

inline constexpr int kClipKinds = 0x97;  // rows of the clip table: kind 0..0x96 (kind 0 and 0x96 are never looked up)
inline constexpr int kClipBands = 12;
inline constexpr int kClipSides = 2;
inline constexpr int kNoClipRow = 0xBE5;  // "no such clip": the Animetable's row count

// The state PES's FUN_14071E6F0 reads off the player and the match (all of it only with a player, `ap != 0`).
struct KickWorld {
  bool tickOdd = false;         // MI+0x1198 & 1: the match tick counter's parity
  bool firstSide = false;       // MI+0x1554 == 1
  bool actionBit = false;       // FUN_1406EAD40(ap, 0xF): bit 0xF of the kicker's action class record
  float heading_deg = 0.0f;     // P+0x20
  float dispX = 0.0f, dispZ = 0.0f;  // FUN_141EB50B0(P, out, 1): the player's displacement over the last tick, m
  float ballX = 0.0f, ballZ = 0.0f;  // FUN_140A57450(ball, 1): the ball's record 1, m
};

struct KickMotionConstants {
  // The clip table [kind][band][side] -> Animetable row, -1 = none: FUN_1406AA590's sub-type 1 (Idletable.bin, side 1) and
  // sub-type 2 (Movetable.bin, side 0), as FUN_1406ABBB0 indexes it.
  std::array<int16_t, kClipKinds * kClipBands * kClipSides> clip{};
  // Angle cuts (degrees) of FUN_1406ABBB0 (kinds 0x77, 0x78) and FUN_140723DE0: -157.5 -112.5 -67.5 -22.5 22.5 67.5 112.5; the
  // 157.5 cut is SituationConstants::turn157 and the |angle| gate of FUN_140723DE0 is SituationConstants::flightHi (30.0).
  float n157 = 0, n112 = 0, n67 = 0, n22 = 0, p22 = 0, p67 = 0, p112 = 0;
  float cutE5B58 = 0;  // 0x1425E5B58, FUN_140723DE0's inner cut
  // FUN_14071E6F0's three row-swap tables (pairs: row, replacement) at 0x14344EEB8, 0x14344EEE0, 0x14344EF50.
  std::vector<std::pair<int, int>> swapOddTick, swapLateral, swapFirstSide;
  float lateralKmh = 0;  // 19.0 @0x1425C65E8: the lateral speed that takes a swapLateral row
  float runKmh = 0;      // 22.0 @0x1425C48C0: FUN_14071E6F0's run speed at row 0x809
  float ballRange = 0;   // 30000.0 @0x1425CD078
  float rangeDiv = 0;    // 3600.0 @0x142598590
  int defaultRecordHits = 0;  // the motion table's default record (motions past 0x1124) holds one HitData entry: FUN_1406AE940 0x1406AE9A4
};

// Reads the clip table from the install's Idletable.bin and Movetable.bin (through the VFS winner) and the constants from the exe.
std::optional<KickMotionConstants> LoadKickMotionConstants(const PeImage& exe, const PesVfs& vfs, std::string* error);

// FUN_1406ABBB0(manager, kind, band, side, angle_deg): the clip table's row, kNoClipRow when absent. Kinds 0x77 and 0x78 pick
// by the angle instead of the table.
int KickClipRow(const KickMotionConstants& k, int kind, int band, int side, float angle_deg);

// FUN_14071E6F0(ap, request, kind, band, side, scale, flagA, flagB): the row after the fix-ups. `world` null = PES's `ap == 0`
// (no fix-up). `request` is the action sub id the motion interface's vtable+0x28 returns (9 pass, 10 shot).
int KickRow(const KickMotionConstants& k, const SituationConstants& sc, int request, int kind, int band, int side,
            float scale_deg, bool flagA, bool flagB, const KickWorld* world);

// FUN_14071E570 for a player with no entry in PES's per-player swap tables: FUN_1406ABB30's cell (0 when empty), then the two
// fixed substitutions. -1 for a (row, column) outside the table.
int KickMotion(const Animetable& table, int row, int column);

// FUN_140723DE0(angle_deg, kind, req): the Animetable column (0..0x11) of a kick turned `angle_deg` off the body.
int KickBucket(const KickMotionConstants& k, const SituationConstants& sc, float angle_deg, int kind, float req);

// FUN_1406B0250(motion table, motion, 0): the motion record's byte 1 (its HitData count).
int KickMotionDescriptor(const KickMotionConstants& k, const Mbinfo& mbinfo, int motion);

}  // namespace pes
