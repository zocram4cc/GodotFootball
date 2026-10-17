// PES 2021's playing-style gate, FUN_1408CB0B0 (docs/re/pes-re-playing-styles.md
// section 2, R1): a player's style bit AND the compat row of his role class
// (0x142613D20). Engine bits are the binding-table order of section 0.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>

#include "data/playingstyles.hpp"
#include "onthepitch/pesstyle.hpp"
#include "pes/exe.hpp"

namespace {

using PlayerStyle = PlayingStyles::Player;

// PES role classes (FUN_1408c51f0): GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9.
constexpr int kRoleGK = 0;
constexpr int kRoleCB = 1;
constexpr int kRoleSB = 2;
constexpr int kRoleCF = 9;

int BitIndex(uint32_t word) {
  for (int b = 0; b < 32; ++b)
    if ((word >> b) & 1u) return b;
  return -1;
}

}  // namespace

TEST(PesStyleGate, EachStyleHasItsOwnEngineBitAndNoneHasNone) {
  EXPECT_EQ(pes::style::StyleWord(PlayerStyle::None), 0u);
  std::set<int> bits;
  for (int i = 1; i < static_cast<int>(PlayerStyle::Count); ++i) {
    const auto style = static_cast<PlayerStyle>(i);
    const uint32_t word = pes::style::StyleWord(style);
    ASSERT_NE(word, 0u) << "style " << i << " has no engine bit";
    ASSERT_EQ(word & (word - 1), 0u) << "style " << i << " has more than one bit";
    const int bit = pes::style::EngineBit(style);
    EXPECT_EQ(bit, BitIndex(word)) << "style " << i;
    EXPECT_GE(bit, 0);
    EXPECT_LE(bit, 20);
    bits.insert(bit);
  }
  EXPECT_EQ(bits.size(), 21u);
}

TEST(PesStyleGate, NeedsTheStyleBit) {
  pes::style::Compat compat{};
  compat.row.fill(~0u);  // every role lists every style: only the style bit can gate
  const int bit = pes::style::EngineBit(PlayerStyle::DefensiveFullBack);
  EXPECT_FALSE(pes::style::Gate(compat, kRoleSB, 0u, bit));
  EXPECT_FALSE(pes::style::Gate(compat, kRoleSB,
                                pes::style::StyleWord(PlayerStyle::OffensiveFullBack), bit));
  EXPECT_TRUE(pes::style::Gate(compat, kRoleSB,
                               pes::style::StyleWord(PlayerStyle::DefensiveFullBack), bit));
}

TEST(PesStyleGate, NeedsTheRoleRowToListTheStyle) {
  pes::style::Compat compat{};
  const PlayerStyle style = PlayerStyle::DefensiveFullBack;
  const int bit = pes::style::EngineBit(style);
  compat.row[kRoleSB] = pes::style::StyleWord(style);
  // The style is on the player, but the row for a centre forward does not list
  // it, so PES does not let him use it there.
  EXPECT_TRUE(pes::style::Gate(compat, kRoleSB, pes::style::StyleWord(style), bit));
  EXPECT_FALSE(pes::style::Gate(compat, kRoleCF, pes::style::StyleWord(style), bit));
}

TEST(PesStyleGate, RoleOutsideTheTenRowsIsGatedOut) {
  pes::style::Compat compat{};
  compat.row.fill(~0u);
  const uint32_t word = pes::style::StyleWord(PlayerStyle::GoalPoacher);
  const int bit = pes::style::EngineBit(PlayerStyle::GoalPoacher);
  EXPECT_FALSE(pes::style::Gate(compat, -1, word, bit));
  EXPECT_FALSE(pes::style::Gate(compat, 10, word, bit));
  EXPECT_TRUE(pes::style::Gate(compat, kRoleCF, word, bit));
}

TEST(PesStyleGate, InstallRowsListTheStylesOfTheirRole) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) GTEST_SKIP() << "PES2021.exe unreadable: " << err;
  auto compat = pes::style::LoadCompat(*exe, &err);
  ASSERT_TRUE(compat.has_value()) << err;

  const uint32_t gkStyles = pes::style::StyleWord(PlayerStyle::OffensiveGoalkeeper) |
                            pes::style::StyleWord(PlayerStyle::DefensiveGoalkeeper);
  EXPECT_EQ(compat->row[kRoleGK] & gkStyles, gkStyles);
  EXPECT_EQ(compat->row[kRoleGK] & pes::style::StyleWord(PlayerStyle::GoalPoacher), 0u);

  EXPECT_NE(compat->row[kRoleSB] & pes::style::StyleWord(PlayerStyle::DefensiveFullBack), 0u);
  EXPECT_EQ(compat->row[kRoleCB] & pes::style::StyleWord(PlayerStyle::DefensiveFullBack), 0u);

  EXPECT_NE(compat->row[kRoleCF] & pes::style::StyleWord(PlayerStyle::GoalPoacher), 0u);
  EXPECT_EQ(compat->row[kRoleCF] & pes::style::StyleWord(PlayerStyle::DefensiveFullBack), 0u);
}
