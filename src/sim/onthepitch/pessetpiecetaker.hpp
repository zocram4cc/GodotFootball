#pragma once

// PES 2021's set-piece taker timing: how long the AI holds a restart before the
// kicker takes it, docs/re/pes-re-setpiece-choreography.md 9 item 2.
//
// The set-piece AI (match::ai::bp::ThinkUnitSetPlay, think 0x142135c50) counts
// elapsed logic frames from the restart being armed (`MI+0x1198` minus the AI
// context's stored base frame) and releases the kicker once the count reaches a
// frame-rate multiple:
//
//   corner, long cross   3.5 x fps   189 frames at 54 Hz -> CORNERKICK (50)
//   free kick, long pass 3.0 x fps   162 frames at 54 Hz -> FREEKICK_LONGPASS (32)
//   penalty              1.0 x fps    54 frames at 54 Hz
//
// Measured live, counted from the set-play counter's first frame: corner 379 and
// 191 (the 189-frame window seen twice), free kick 164 and 164, penalty not
// observed, kick-off 28 exactly (three times).
//
// The kick-off is not governed by these numbers. Its release is the taker's kick (A5Judge 9.4, live capture 23-10-26:
// arming to release 118-139 frames), and the KickoffReceiver pass gate (0x140781fc0) is not that release: it needs
// MI+0x11AC > 972, and the release comes at 47-56. Neither is a fps multiple. Throw-ins are released on the taker's
// arrival and readiness, goal kicks on the timer ladder of sub_142136F00 (pesgoalkick.hpp). `waitTimerMin` in
// constant::BallplayerSetplay (+0x28) is NOT read on any of these paths: the gates are the fps multiples above, and the JSON value
// is recorded so that nobody ports it.
//
// Every value is a plain .rdata float in the install's PES2021.exe, read through
// pes::PeImage::ReadVa.

#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

// The wait gates in seconds of logic time, and the short-corner window.
struct SetPieceTakerConstants {
  float cornerGate_s = 3.5f;    // 3.5, .rdata 0x14259bde8
  float freeKickGate_s = 3.0f;  // 3.0, .rdata 0x14258c264
  float penaltyGate_s = 1.0f;   // 1.0, .rdata 0x14256ae1c
  // The short corner may be played inside this window instead (ThinkUnitSetPlay
  // 0x142135feb): 2 x fps, or 7 x fps while a team-mate is already running the
  // corner (byte 0x14259bdf0), and once the partner check passes.
  float shortCornerWindow_s = 2.0f;
  float shortCornerRunningWindow_s = 7.0f;
  // The goal kick's ladder (sub_142136F00, pesgoalkick.hpp) and its placement (SetKick 0x140774AE0). freeKickGate_s is
  // also the ladder's three-second wait: the same .rdata word.
  float halfSecond_s = 0.5f;         // 0.5, .rdata 0x142565600: the ladder's windows, and the rounding of its waits
  float goalKickTacticsGate = 0.3f;  // 0.3, .rdata 0x142590d98: the special restart's early-out
  float centimetre_m = 0.01f;        // 0.01, .rdata 0x14256ae18: the Mbinfo hit records are in cm
  float physiqueLow = 0.9166f;       // .rdata 0x1425ddf78: FUN_1406e8cc0's physique clamp, low (x and z)
  float physiqueHigh = 1.0833f;      // .rdata 0x1425ddf80: and high
  float hitTickDivisor = 60.0f;      // 60, .rdata 0x14259a764: a hit's tick divisor (FUN_1406b03d0)
  // The throw-in's pickup reach: PES starts the pickup clip inside this (DAT_14259222c, docs/re/pes-re-throwin-choreography.md
  // 3.2). Until the loader reads it, the old GF stand-in of 1 m stands here.
  float throwInPickupReach_m = 1.0f;
  // PES's carry release row (FUN_140792E10, carry branch; docs/re/pes-re-throwin-choreography.md 4.2): the short run row while the
  // taker is within this ground distance of the throw's target, the mid row beyond it. 13.0 from .rdata 0x1425c5948.
  float throwInReleaseShort_m = 0.0f;
  // The MOVE command filler's walk-down (FUN_1406b7560, call 0x1406b77d6 -> FUN_140a76070(2)): a run-class set-piece walker far
  // from his target has his desired speed replaced by this one, 11 km/h from .rdata 0x14259bdfc
  // (docs/re/pes-re-walk-speed.md). Held in m/s; the loader converts.
  float walkDown_ms = 0.0f;
};

// PES's throw-in carry release rows (throwin 4.2): throwin_release_run_000_short and _mid, the 000 column.
inline constexpr int kThrowInReleaseRunShort = 0x9bb;
inline constexpr int kThrowInReleaseRunMid = 0x9bc;
// The carry's release row for a taker `distance_m` from the throw's target: the short row up to `shortReach_m`, the mid row
// beyond it (FUN_140792E10: the compare at 0x140792f13 selects 0x9bc when the distance is above the reach).
int ThrowInCarryReleaseRow(float distance_m, float shortReach_m);

std::optional<SetPieceTakerConstants> LoadSetPieceTakerConstants(const PesVfs& vfs,
                                                                const PeImage& exe,
                                                                std::string* error);

}  // namespace pes
