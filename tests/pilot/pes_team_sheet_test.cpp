// The team record T holds the team sheet S = T+0x8e08 in PES's byte layout. The
// translated sheet readers run on that memory in place; each must agree with the
// hand port it replaces on the same bytes.

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

#include "onthepitch/pesteamconcepts.hpp"

#ifdef PES_PILOT_AVAILABLE
#include "pes/pespilot.hpp"
#include "pes_state.hpp"
#endif

namespace {

using namespace pes;

TEST(TeamRecord, SheetSitsAtPesOffsetInsideTheTeamRecord) {
  EXPECT_EQ(offsetof(PesTeamRecord, sheet), kTeamSheetOffset);
  EXPECT_EQ(sizeof(PesTeamRecord), kTeamRecordBytes);
}

#ifdef PES_PILOT_AVAILABLE
using SheetBytes = std::array<uint8_t, kSheetBytes>;

// Any byte pattern is a valid input: the readers must agree on random sheets, not
// just on sheets the GF setters can write.
SheetBytes RandomSheet(std::mt19937& rng) {
  SheetBytes sheet{};
  for (auto& b : sheet) b = static_cast<uint8_t>(rng());
  return sheet;
}

// Runs one translated reader with rcx = the window's base (S), rdx = k, r8 = plan. The
// window maps the VA range onto the sheet bytes in place: no copy.
uint32_t RunSheetReader(void (*fn)(PesState&), SheetBytes& sheet, uint64_t k, uint64_t plan) {
  PesState s;
  s.gpr[1] = kSheetWindowVa;
  s.gpr[2] = k;
  s.gpr[8] = plan;
  s.mem.window = {kSheetWindowVa, kSheetBytes, sheet.data(), false};
  fn(s);
  return static_cast<uint32_t>(s.gpr[0]);
}

TEST(TeamSheetPilot, TakerAgreesWithHandPortOnRandomSheets) {
  std::mt19937 rng(7);
  for (int trial = 0; trial < 200; ++trial) {
    SheetBytes sheet = RandomSheet(rng);
    for (int k = -1; k <= 3; ++k) {
      EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C5060, sheet,
                                                static_cast<uint64_t>(k), 0)),
                sheet_port::Taker(sheet.data(), k))
          << "k=" << k;
    }
  }
}

TEST(TeamSheetPilot, DesignatedAgreesWithHandPortOnRandomSheets) {
  std::mt19937 rng(8);
  for (int trial = 0; trial < 200; ++trial) {
    SheetBytes sheet = RandomSheet(rng);
    for (int k = -1; k <= 6; ++k) {
      EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C5080, sheet,
                                                static_cast<uint64_t>(k), 0)),
                sheet_port::Designated(sheet.data(), k))
          << "k=" << k;
    }
  }
}

TEST(TeamSheetPilot, SliderAgreesWithHandPortOnRandomSheets) {
  std::mt19937 rng(9);
  for (int trial = 0; trial < 200; ++trial) {
    SheetBytes sheet = RandomSheet(rng);
    for (int plan = 0; plan <= kSheetPlans; ++plan) {
      for (int k = -1; k <= kSheetSliders; ++k) {
        EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C52E0, sheet,
                                                  static_cast<uint64_t>(k),
                                                  static_cast<uint64_t>(plan))),
                  sheet_port::Slider(sheet.data(), k, plan))
            << "k=" << k << " plan=" << plan;
      }
    }
  }
}

TEST(TeamSheetPilot, ToggleAgreesWithHandPortOnRandomSheets) {
  std::mt19937 rng(10);
  for (int trial = 0; trial < 200; ++trial) {
    SheetBytes sheet = RandomSheet(rng);
    for (int plan = 0; plan <= kSheetPlans; ++plan) {
      for (int k = -1; k <= kSheetToggleCount; ++k) {
        EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C5030, sheet,
                                                  static_cast<uint64_t>(k),
                                                  static_cast<uint64_t>(plan))),
                  sheet_port::Toggle(sheet.data(), k, plan))
            << "k=" << k << " plan=" << plan;
      }
    }
  }
}
#else
TEST(TeamSheetPilot, TranslatedReadersNeedTheImage) {
  GTEST_SKIP() << "PES_EXE unset at configure time: no translated sheet readers";
}
#endif

}  // namespace
