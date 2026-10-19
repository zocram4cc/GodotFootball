#pragma once

// MOVESTEP handler group B (contract v0.10, owner: MovestepRun): the running phase. RUN_LOOP,
// GOTO_RUN and its entry, STOP and its entry, ACCELERATE and its entry, DECELERATE and its
// entry, plus the run entry FUN_1406d2030 that INIT and the other groups call. Every PES site is
// named; no field is written without a VA. The install constants (RunConstants) and the
// interface tables (RunTables) live in the shared header; this file's loader reads them by VA.
//
// Decompiles (AnimSelectRE-2 scratch, d_<VA>.c):
//   FUN_1406c0170 RUN_LOOP, FUN_1406bdf10 GOTO_RUN, FUN_1406ce820 EnterGotoRun,
//   FUN_1406c9e20 STOP, FUN_1406d5d60 EnterStop, FUN_1406bb570 ACCELERATE,
//   FUN_1406ccd00 EnterAccelerate, FUN_1406bcd20 DECELERATE, FUN_1406cdaf0 EnterDecelerate,
//   FUN_1406d2030 EnterRun. Helpers: 1406b9ff0, 1406cbb00, 1406cba60, 1406d8730, 1406b70d0,
//   1406b6ee0, 1406d01d0 (decompiled for this port in gf-MovestepRun-scratch).
#include <cstdint>

#include "pes/movestep.hpp"

namespace pes {
namespace moveword {
// The request words group B reads or writes that the shared contract does not name yet. Each is
// a PES req word; the byte flags are the low byte of the word.
inline constexpr int kRateBand = 0x1f;       // req[0x1f]: the running speed band 0..2 (OBS: FUN_1406bdf10/FUN_1406bb570 write 0/1/2 here; FUN_1406c9e20 reads it)
inline constexpr int kSpeedBand = 0x22;      // req[0x22]: the band the run starts in (OBS: FUN_1406ce820 writes clamp(req[0xa], 5, 16); RUN_LOOP copies it to 0x1d)
inline constexpr int kBandTarget = 0x24;     // req[0x24]: the band to run at (OBS: FUN_1406ce820 writes clamp(req[9], 0, 3))
// FUN_1406bdf10 writes word 0x1d before its STOP entry (param_2[0x1d] = 5.0 at lines 226-228 of its decompile). The live
// speed word is also kSpeedWord in movestep_turn.hpp; both name word 0x1d.
inline constexpr int kStopSpeedWord = 0x1d;
inline constexpr int kHitMotion = 0x27;      // req[0x27]: the last hit record's motion (OBS: FUN_1406ce820)
inline constexpr int kHitFrame = 0x28;       // req[0x28]: the last hit record's frame (OBS: FUN_1406ce820)
inline constexpr int kTurnLockFlag = 0x3d;   // req[0x3d] byte: a turn lock is active (OBS: FUN_1406bdf10)
inline constexpr int kTurnLockTicks = 0x3e;  // req[0x3e]: the lock's tick count (OBS: FUN_1406bdf10)
inline constexpr int kTurnLockDir = 0x3f;    // req[0x3f] byte: the lock's direction flag (OBS: FUN_1406bdf10)
}  // namespace moveword

// The interface's table readers, declared here so the tests can check their arithmetic directly.
int CommandClassFor(int command, const RunTables& t);      // FUN_1406b8de0, 0x1425ddcd8
float BandRateFor(int band, int kind, const RunTables& t);  // FUN_1406b9e50, 0x1425dde10
int ClassForRateFor(float rate, const RunTables& t);        // FUN_1406ba030, 0x1425dde20

// The interface's row pickers, declared here so the tests can check the index order directly
// (FUN_1406ce820 writes the same order inline at d_1406ce820.c:321). Defined in movestep_run.cpp.
int StopRowFor(int kind, int cls, const RunTables& t);    // FUN_1406b9d80, 0x1425ddad0
int GotoRowFor(int kind, int cls, const RunTables& t);    // FUN_1406b9bd0, 0x1425dda50
int TurnStopRowFor(int kind, int cls, const RunTables& t);  // FUN_1406ba0d0, 0x1425ddb50

// The step rows are read beside RunTables and kept with the other install constants, so a host
// that supplies RunConstantsView() supplies these too: LoadStepRows fills the same handle the
// adapter keeps. ponytail: adding `std::array<int32_t, 32> stepRow;` to RunTables removes this
// table, its loader and the handle.

// The group-B file-local helpers, defined in namespace pes beside the loaders (their callers
// are the handlers in namespace pes::movestep, which see them through pes::).
bool TurnLockEligible(MoveHost& host, MoveReq& req);       // FUN_1406d01d0
float FirstAngle(float a, float b, const RunConstants& k);  // FUN_140a92950
int HitRecordCount();                                      // FUN_1406afbc0

// This group's loaders, defined in movestep_run.cpp. They read the shared header's RunConstants
// and RunTables from the exe by the VAs in the file; tests skip without PES21_ROOT.
std::optional<RunConstants> LoadRunConstants(const PeImage& exe, std::string* error);
std::optional<RunTables> LoadRunTables(const PeImage& exe, std::string* error);

}  // namespace pes
