#pragma once

// FUN_1406d01d0, the GOTO_RUN entry that takes a turn row (0x56f..0x572): the helper RUN_LOOP's turn arm, GOTO_RUN's
// re-plan and STEP_TURN_RUN call before their own paths. It returns 1 after writing the request (state 7), else 0. Ported
// from the instruction stream (docs/ports/pes-port-StepAudit2.md) and proven against the real calls of the capture hook
// stepaudit_1406D01D0 (tests/pes_movestep_turn_entry_test.cpp, tools/dev/translate/handler_oracle.py).

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "pes/exe.hpp"
#include "pes/movestep.hpp"

namespace pes {

// The two byte tables the entry reads by the look bucket: the foot flag of the default and the 0x571 rows (DAT_1425dd930)
// and of the 0x56f / 0x570 rows (DAT_1425dd940).
struct TurnEntryTables {
  std::array<uint8_t, 10> footFlag{};      // 0x1425dd930
  std::array<uint8_t, 10> footFlagTurn{};  // 0x1425dd940
};
std::optional<TurnEntryTables> LoadTurnEntryTables(const PeImage& exe, std::string* error);

// FUN_1406e9f90 (+0xac0 vt+0x30): the ability weight in [0, 1] the playback functions take. `da` selects the DA ability byte
// (0x15) instead of Acceleration (0x2b); the byte is less (10 - c) for a condition c of 10 or below, then (b - 40) / 59.
float AbilityWeight(MoveHost& host, bool da);

// FUN_1406d8620 (+0xac0 vt+0x10), the request writer every entry calls to "issue" a clip: it stores the clip into the request
// words (0x2f pending, 0x30 motion, 0x31 mirror, 0x32 and 0x2e duration, 0x33 start time, 0x35 playback, 0x36 transition,
// 0x1a and 0x2d cleared) and the clip's hit seconds and distance (0x1b, 0x34; zero for a motion of 0x1125 or more).
// FUN_1406b8340's tail applies them.
void WriteIssue(MoveHost& host, MoveReq& req, const MotionRequest& issue);

// FUN_1406e7920: the clip's hit time in seconds (blend, or the cancel record at min(index, count - 1)) over the playback.
float ClipHitSeconds(const MoveHost& host, int motion, float playback, int index);

// FUN_1406d01d0(P, req).
bool EnterGotoTurn(MoveHost& host, MoveReq& req);

}  // namespace pes
