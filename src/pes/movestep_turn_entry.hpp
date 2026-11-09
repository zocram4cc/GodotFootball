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

// FUN_1406d01d0(P, req).
bool EnterGotoTurn(MoveHost& host, MoveReq& req);

}  // namespace pes
