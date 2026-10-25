// The team record T holds the team sheet S = T+0x8e08 in PES's byte layout. The translated sheet readers run on
// that memory in place: each returns the record's field at its layout offset, and refuses what PES refuses.

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>

#include "onthepitch/pesteamconcepts.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

namespace {

using namespace pes;

using SheetBytes = std::array<uint8_t, kSheetBytes>;

TEST(TeamRecord, SheetSitsAtPesOffsetInsideTheTeamRecord) {
  EXPECT_EQ(offsetof(PesTeamRecord, sheet), kTeamSheetOffset);
  EXPECT_EQ(sizeof(PesTeamRecord), kTeamRecordBytes);
}

// Any byte pattern is a valid sheet: the readers must answer for whatever the bytes hold.
SheetBytes RandomSheet(std::mt19937& rng) {
  SheetBytes sheet{};
  for (auto& b : sheet) b = static_cast<uint8_t>(rng());
  return sheet;
}

uint32_t WordAt(const SheetBytes& sheet, int offset) {
  uint32_t v = 0;
  std::memcpy(&v, sheet.data() + offset, sizeof v);
  return v;
}

// Runs one translated reader with rcx = the window's base (S), rdx = k, r8 = plan. The window maps the VA range onto
// the sheet's bytes in place: no copy.
uint32_t RunSheetReader(void (*fn)(PesState&), const SheetBytes& sheet, uint64_t k, uint64_t plan) {
  PesState s;
  s.gpr[1] = kSheetWindowVa;
  s.gpr[2] = static_cast<uint32_t>(k);  // an int argument in edx zero-extends into rdx
  s.gpr[8] = static_cast<uint32_t>(plan);
  s.mem.window.base = kSheetWindowVa;
  s.mem.window.size = kSheetBytes;
  s.mem.window.ro = sheet.data();
  fn(s);
  return static_cast<uint32_t>(s.gpr[0]);
}

TEST(TeamSheetReaders, TakerReadsItsWordAndRefusesPastTheLastTaker) {
  std::mt19937 rng(1);
  for (int round = 0; round < 8; ++round) {
    const SheetBytes sheet = RandomSheet(rng);
    for (int k = -1; k <= kSheetTakers + 2; ++k) {
      const int want = k >= kSheetTakers ? kSheetNone : static_cast<int>(WordAt(sheet, kSheetTakerOffset + 4 * k));
      EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C5060, sheet, static_cast<uint64_t>(k), 0)), want)
          << "k=" << k;
    }
  }
}

TEST(TeamSheetReaders, DesignatedReadsItsWordAndRefusesOutsideTheSix) {
  std::mt19937 rng(2);
  for (int round = 0; round < 8; ++round) {
    const SheetBytes sheet = RandomSheet(rng);
    for (int k = -1; k <= kSheetDesignated + 1; ++k) {
      const bool refused = k < 0 || k >= kSheetDesignated;
      const int want = refused ? kSheetNone : static_cast<int>(WordAt(sheet, kSheetDesignatedOffset + 4 * k));
      EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C5080, sheet, static_cast<uint64_t>(k), 0)), want)
          << "k=" << k;
    }
  }
}

// Slider and Toggle take a plan: 0..2 picks a stored plan, kCurrentPlan reads the plan word at S+0, and anything
// else is refused. A refused read is 0; a valid one is the byte at the plan's row.
int ExpectedRow(const SheetBytes& sheet, int offset, int row_len, int count, int k, int plan) {
  const int p = plan == kCurrentPlan ? static_cast<int>(WordAt(sheet, kSheetPlanOffset)) : plan;
  if (p < 0 || p >= kSheetPlans || k < 0 || k >= count) return 0;
  return sheet[offset + row_len * p + k];
}

TEST(TeamSheetReaders, SliderReadsItsPlanRowAndRefusesOutOfRange) {
  std::mt19937 rng(3);
  for (int round = 0; round < 8; ++round) {
    const SheetBytes sheet = RandomSheet(rng);
    for (int plan : {0, 1, 2, kSheetPlans, kCurrentPlan}) {
      for (int k = -1; k <= kSheetSliders; ++k) {
        const int want = ExpectedRow(sheet, kSheetSliderOffset, kSheetSliders, kSheetSliders, k, plan);
        EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C52E0, sheet, static_cast<uint64_t>(k),
                                                  static_cast<uint64_t>(plan))),
                  want)
            << "k=" << k << " plan=" << plan;
      }
    }
  }
}

TEST(TeamSheetReaders, ToggleReadsItsPlanRowAndRefusesOutOfRange) {
  std::mt19937 rng(4);
  for (int round = 0; round < 8; ++round) {
    const SheetBytes sheet = RandomSheet(rng);
    for (int plan : {0, 1, 2, kSheetPlans, kCurrentPlan}) {
      for (int k = -1; k <= kSheetToggleCount; ++k) {
        const int want = ExpectedRow(sheet, kSheetToggleOffset, kSheetToggleCount, kSheetToggleCount, k, plan);
        EXPECT_EQ(static_cast<int>(RunSheetReader(&::pes_fn_1408C5030, sheet, static_cast<uint64_t>(k),
                                                  static_cast<uint64_t>(plan))),
                  want)
            << "k=" << k << " plan=" << plan;
      }
    }
  }
}

}  // namespace
