#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <string>

#include "onthepitch/pesroles.hpp"
#include "onthepitch/pesteamconcepts.hpp"
#include "pes/exe.hpp"

// The class map is read from the install. Its relations must hold, and four role classes must
// carry the names of docs/re/pes-re-role-writer.md §5 (GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9).
constexpr int kCentreBackClass = 1;
constexpr int kDefensiveMidClass = 3;
constexpr int kAttackingMidClass = 6;
constexpr int kCentreForwardClass = 9;

TEST(PesRolesInstall, ClassMapHoldsRelationsOfTheRegisteredRoles) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto classOf = pes::roles::LoadRoleClassMap(*exe, &err);
  ASSERT_TRUE(classOf) << err;
  const auto& c = *classOf;
  for (int r = 0; r < pes::roles::kRegisteredRoles; ++r) {
    EXPECT_GE(c[r], 0);
    EXPECT_LE(c[r], 9);
  }
  EXPECT_EQ(pes::roles::kKeeperClass, c[0]);  // GK is the keeper class
  EXPECT_EQ(kCentreBackClass, c[1]);
  EXPECT_EQ(kDefensiveMidClass, c[4]);
  EXPECT_EQ(kAttackingMidClass, c[8]);
  EXPECT_EQ(kCentreForwardClass, c[12]);
  EXPECT_EQ(c[2], c[3]);                      // LB and RB share the side-back class
  EXPECT_EQ(c[6], c[7]);                      // LM and RM share a class
  EXPECT_EQ(c[9], c[10]);                     // LW and RW share a class
  EXPECT_NE(c[4], c[5]);                      // DM and CM differ
  EXPECT_NE(c[11], c[12]);                    // SS and CF differ
}

TEST(PesRoles, WriteSideRolesFillsBothArraysWithEachSlotsClass) {
  std::array<int, pes::roles::kRegisteredRoles> classOf{};
  for (int r = 0; r < pes::roles::kRegisteredRoles; ++r) classOf[r] = r % 10;
  pes::TeamSheet sheet;
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot)
    sheet.SetRegisteredRole(slot, slot + 1, 2);
  pes::roles::SideRoles roles;
  pes::roles::WriteSideRoles(classOf, sheet, 2, &roles);
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) {
    EXPECT_EQ(classOf[slot + 1], roles.a[slot]);
    EXPECT_EQ(roles.a[slot], roles.b[slot]);
  }
}

TEST(PesRoles, SwapExchangesBOnlyAndLatchesThePair) {
  pes::roles::SideRoles roles;
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) {
    roles.a[slot] = 100 + slot;
    roles.b[slot] = slot;
  }
  const auto aBefore = roles.a;
  pes::roles::SwapB(&roles, 2, 5);
  EXPECT_EQ(aBefore, roles.a);
  EXPECT_EQ(5, roles.b[2]);
  EXPECT_EQ(2, roles.b[5]);
  EXPECT_EQ(2, roles.lastSwapA);
  EXPECT_EQ(5, roles.lastSwapB);
}

TEST(PesRoles, SwapRefusesAPairPastTheLastSlot) {
  pes::roles::SideRoles roles;
  for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) roles.b[slot] = slot;
  const auto bBefore = roles.b;
  pes::roles::SwapB(&roles, 3, pes::roles::kSideSlots);
  EXPECT_EQ(bBefore, roles.b);
  EXPECT_EQ(0, roles.lastSwapA);
  EXPECT_EQ(0, roles.lastSwapB);
}

TEST(PesRoles, TailSwapNeedsBothLatchesInRange) {
  pes::roles::SideRoles roles;
  EXPECT_TRUE(pes::roles::TailSwapAllowed(roles));
  roles.lastSwapA = pes::roles::kSideSlots;
  EXPECT_FALSE(pes::roles::TailSwapAllowed(roles));
  roles.lastSwapA = 0;
  roles.lastSwapB = pes::roles::kSideSlots;
  EXPECT_FALSE(pes::roles::TailSwapAllowed(roles));
}

// FUN_1405F9CD0's team-record bytes (0x1405F9E00-0x1405F9E6C): per phase and line, the men with a non-zero class and the
// active ones among them. FUN_140A63680 reads the phase-1 forward line at T+0x87CC (decompile, `lVar4 + 0x87cc`).
namespace {
constexpr int kPossessionPhase = 1, kOutPhase = 0;
constexpr int kBackLine = 0, kMidLine = 1, kForwardLine = 2;
struct BandFixture {
  std::array<uint8_t, pes::kTeamSheetOffset> team{};
  pes::roles::SideRoles roles;
  std::array<bool, pes::roles::kSideSlots> active;
  BandFixture() {
    // GK, CB, CB, SB, DMF, CMF, SMF, AMF, SS, WF, CF
    const int classes[pes::roles::kSideSlots] = {0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (int slot = 0; slot < pes::roles::kSideSlots; ++slot) roles.b[slot] = classes[slot];
    active.fill(true);
  }
};
}  // namespace

TEST(PesBandRows, TheForwardLineOfThePossessionPhaseLivesAtT87CC) {
  EXPECT_EQ(pes::roles::BandRowOffset(kPossessionPhase, kForwardLine), 0x87CCu);
}

TEST(PesBandRows, EachLineCountsItsClassesAndTheKeeperCountsNowhere) {
  BandFixture f;
  pes::roles::WriteBandRows(f.team.data(), kPossessionPhase, f.roles, f.active);
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kPossessionPhase, kBackLine), 3);  // CB CB SB
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kPossessionPhase, kMidLine), 4);   // DMF CMF SMF AMF
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kPossessionPhase, kForwardLine), 3);  // SS WF CF
}

TEST(PesBandRows, ADeparturePastTheClassesTakesTheBackLine) {
  BandFixture f;
  f.roles.b[10] = 10;  // past CF: the switch's default arm
  pes::roles::WriteBandRows(f.team.data(), kPossessionPhase, f.roles, f.active);
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kPossessionPhase, kBackLine), 4);
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kPossessionPhase, kForwardLine), 2);
}

TEST(PesBandRows, AnInactiveManCountsAsAManButNotAsActive) {
  BandFixture f;
  f.active[10] = false;  // the CF is off the pitch
  pes::roles::WriteBandRows(f.team.data(), kPossessionPhase, f.roles, f.active);
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kPossessionPhase, kForwardLine), 3);
  EXPECT_EQ(pes::roles::BandRowActive(f.team.data(), kPossessionPhase, kForwardLine), 2);
}

TEST(PesBandRows, ABuildWritesOnlyItsOwnPhase) {
  BandFixture f;
  pes::roles::WriteBandRows(f.team.data(), kPossessionPhase, f.roles, f.active);
  for (int line = 0; line < 3; ++line) EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kOutPhase, line), 0);
  f.roles.b[10] = 4;  // the CF becomes a CMF; only the out-of-possession build sees it
  pes::roles::WriteBandRows(f.team.data(), kOutPhase, f.roles, f.active);
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kOutPhase, kForwardLine), 2);
  EXPECT_EQ(pes::roles::BandRowMen(f.team.data(), kPossessionPhase, kForwardLine), 3)
      << "the possession phase keeps what its own build wrote";
}

// 0x1404F9F0D: `test byte [MI+0x1198],1`: on an odd frame the build is the latch's phase, on an even frame the other.
TEST(PesBandRows, TheBuiltPhaseAlternatesWithTheFrameParity) {
  EXPECT_EQ(pes::roles::BuiltPhase(1, true), 1);
  EXPECT_EQ(pes::roles::BuiltPhase(1, false), 0);
  EXPECT_EQ(pes::roles::BuiltPhase(2, true), 0);
  EXPECT_EQ(pes::roles::BuiltPhase(2, false), 1);
  EXPECT_EQ(pes::roles::BuiltPhase(3, true), 1);
}
