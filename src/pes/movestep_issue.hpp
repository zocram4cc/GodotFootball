#pragma once

// The anime interface calls every MOVESTEP entry makes before and while it "issues" a clip, ported from the instruction stream
// (docs/ports/pes-port-StepAudit3.md). Split from movestep_turn_entry.cpp so a test of one group does not pull the GOTO_RUN turn
// entry with them.

#include "pes/movestep.hpp"

namespace pes {

// FUN_1406e9f90 (+0xac0 vt+0x30): the ability weight in [0, 1] the playback functions take. `da` selects the DA ability byte
// (0x15) instead of Acceleration (0x2b); the byte is less (10 - c) for a condition c of 10 or below, then (b - 40) / 59.
float AbilityWeight(MoveHost& host, bool da);

// The playback every entry computes before it issues (vt+0x30 then vt+0x50): TurnPlayback(G, AbilityWeight(match-up flag)). The
// flag is true when the own slot is the match-up man (FUN_140a62960's answer is the own slot).
float EntryPlayback(MoveHost& host);

// FUN_1406d8620 (+0xac0 vt+0x10), the request writer every entry calls to "issue" a clip: it stores the clip into the request
// words (0x2f pending, 0x30 motion, 0x31 mirror, 0x32 and 0x2e duration, 0x33 start time, 0x35 playback, 0x36 transition,
// 0x1a and 0x2d cleared) and the clip's hit seconds and distance (0x1b, 0x34; zero for a motion of 0x1125 or more).
// FUN_1406b8340's tail applies them (DueRequest).
void WriteIssue(MoveHost& host, MoveReq& req, const MotionRequest& issue);

// FUN_1406e7920: the clip's hit time in seconds (blend, or the cancel record at min(index, count - 1)) over the playback.
float ClipHitSeconds(const MoveHost& host, int motion, float playback, int index);

// FUN_1406e7730: the clip's root offset at a hit, scaled by the physique scale `g` (0 for a motion of 0x1125 or more).
float ClipHitDistance(const MoveHost& host, int motion, float g, int index);

// The two copies the entries make right after WriteIssue: req[0x28] = the clip's hit seconds (FUN_1406e7920, WriteIssue's
// req[0x1b]) and req[0x27] = its hit distance (FUN_1406e7730, req[0x34]); both zero for a motion of 0x1125 or more.
void CopyHitWords(MoveReq& req, int motion);

// FUN_140a6e6b0(req + 0x144): the count byte of the request's look-point list (the byte at request offset 0x148).
int RequestLookCount(const MoveReq& req);
// FUN_140a76240(req + 0x144): the list's point the arms steer by: the block's own point (req + 0x14c) when the count is 0, else
// the entry at 12 * (count + 1) bytes into the block. Copies its three floats.
void RequestLookPoint(const MoveReq& req, float out[3]);

// FUN_1406abb30(table, &row, &column, 0) and FUN_1406abd60(table, &row, &column) as the GOTO_RUN entries call them: the clip's
// motion as a 16-bit word, and its mirror bit. With the walk argument 0 the motion's default is 0 (`NEG R9B; SBB R10W,R10W`
// at 0x1406abb30), so a row without a clip at that column answers motion 0, never -1.
struct CellPick {
  int motion = 0;
  bool mirror = false;
};
CellPick PickCell(MoveHost& host, int row, int column);

// 0x1406d04d6.. / 0x1406ceb0d..: the look bucket flip of the GOTO_RUN entries, 4 <-> 9; the look delta (word 5) is set to -180
// when the bucket it leaves is 4, else 180.
void FlipLookBucket(MoveReq& req, const RunConstants& k);

}  // namespace pes
