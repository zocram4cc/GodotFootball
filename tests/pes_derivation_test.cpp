// PES 2021's per-player signature motions (docs/re/pes-re-special-players-decode.md §0-§2): the
// packed Derivationdatatable.bin, the two walkers that search it, and FUN_14071E570's long-kick
// selection. Synthetic files and rules pin each branch. The install tests read the user's own file
// and exe, and skip when a download/ pack ships the table: its entries are then that pack's, not the
// stock file's, and must not be pinned.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "pes/cpk.hpp"
#include "pes/derivation.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kTablePath = "common/anime/playerID/bin/Derivationdatatable.bin";

// The entry widths the walkers mask (docs §0): an 11-bit motion in and a 5-bit sub in word1, a
// 12-bit motion out at bit 17 and a 17-bit id in dword0. The bases are synthetic; the install
// tests check the exe's own bases and widths.
constexpr int kMotionInMask = 0x7FF;
constexpr int kMotionOutMask = 0xFFF;
constexpr int kPlayerIdMask = 0x1FFFF;  // 17 bits
constexpr int kSubMask = 0x1F;          // 5 bits
constexpr int kMotionOutShift = 17;
constexpr int kSubShift = 11;
constexpr int kMotionInBase = 500;
constexpr int kMotionOutBase = 480;

pes::DerivationLayout SyntheticLayout(const std::array<int, pes::DERIVATION_KINDS>& listLength) {
  pes::DerivationLayout l{};
  l.listLength = listLength;
  l.motionInMask = kMotionInMask;
  l.motionInBase = kMotionInBase;
  l.motionOutMask = kMotionOutMask;
  l.motionOutBase = kMotionOutBase;
  l.motionOutShift = kMotionOutShift;
  l.playerIdMask = kPlayerIdMask;
  l.subMask = kSubMask;
  l.subShift = kSubShift;
  return l;
}

// Synthetic rules, shaped like FUN_1406B3560's tail and FUN_14071E570 but not the stock values, so a
// test that passes only because a constant happens to match the stock fails here.
pes::DerivationRules SyntheticRules() {
  pes::DerivationRules r{};
  r.beckhamIds = {1001, 1002, 1003, 1004, 1005};
  r.tailIdA = 1005;
  r.tailIdB = 1004;
  r.freeKickIdle = 530;
  r.freeKickRunup = 540;
  r.freeKickKind = 2;
  r.beckham2002Offset = 4;
  r.curveKind = 1;
  r.curvePlacekick = 510;
  r.curveBeckham2002 = 520;
  r.curve = 500;
  r.curveSpan = 1;
  r.curveMax = 508;
  r.beckhamVariant = 9;
  r.playerKind = 3;
  r.genericKind = 1;
  r.curvePlacekickDelta = -10;  // 510 - 10 = the curve
  r.remapFrom = 800;
  r.remapDelta = 7;
  r.remapFrom2 = 820;
  r.remapTo2 = 840;
  return r;
}

// One entry as the tests state it: its list, its player id (nullopt: generic), its clips and sub.
struct Row {
  int kind;
  std::optional<int> playerId;
  int motionIn, motionOut, sub;
};

void PutEntry(std::vector<uint8_t>& out, uint32_t dword0, uint32_t dword1) {
  for (uint32_t word : {dword0, dword1})
    for (int shift = 0; shift < 32; shift += 8) out.push_back(uint8_t(word >> shift));
}

// The file for these rows (each kind's rows in order; a list's length is its row count), packed the
// way the walkers unpack it, then decoded.
pes::DerivationTable Table(const std::vector<Row>& rows, const pes::DerivationRules& rules = SyntheticRules()) {
  std::array<int, pes::DERIVATION_KINDS> lengths{};
  for (const Row& r : rows) ++lengths[r.kind];
  std::vector<uint8_t> bytes;
  for (int kind = 0; kind < pes::DERIVATION_KINDS; ++kind)
    for (const Row& r : rows) {
      if (r.kind != kind) continue;
      const uint32_t id = r.playerId ? uint32_t(*r.playerId) + 1 : 0;
      PutEntry(bytes, id | (uint32_t(r.motionOut - kMotionOutBase) << kMotionOutShift),
               uint32_t(r.motionIn - kMotionInBase) | (uint32_t(r.sub) << kSubShift));
    }
  std::string err;
  auto t = pes::ParseDerivationTable(bytes, SyntheticLayout(lengths), rules, &err);
  if (!t) {
    ADD_FAILURE() << err;
    return {};
  }
  return *t;
}

pes::LongKickInput Kick(const pes::DerivationRules& r, int playerId, int picked, bool flag = false,
                        bool substitutable = false, int stat = 0) {
  pes::LongKickInput in{};
  in.playerId = playerId;
  in.picked = picked;
  in.flag = flag;
  in.animeVariant = r.beckhamVariant;
  in.substitutable = substitutable;
  in.stat = stat;
  return in;
}

// ---------------------------------------------------------------------------------------------
// Parsing and packing (synthetic bytes, explicit bit positions)
// ---------------------------------------------------------------------------------------------

TEST(DerivationParse, RejectsAFileThatIsNotWholeEntries) {
  const pes::DerivationLayout layout = SyntheticLayout({1, 0, 1, 0, 1, 0});  // three entries
  std::string err;
  EXPECT_TRUE(pes::ParseDerivationTable(std::vector<uint8_t>(24, 0), layout, SyntheticRules(), &err)) << err;
  EXPECT_FALSE(pes::ParseDerivationTable(std::vector<uint8_t>(23, 0), layout, SyntheticRules(), &err));
  EXPECT_FALSE(pes::ParseDerivationTable(std::vector<uint8_t>(25, 0), layout, SyntheticRules(), &err));
}

TEST(DerivationPacking, IdFieldZeroIsGenericAndIdFieldOneIsPlayerZero) {
  // Two kind-0 entries, both 500 -> 520: stored id 0 (generic) and stored id 1 (player 0).
  const uint32_t motionOut = uint32_t(520 - kMotionOutBase) << kMotionOutShift;
  std::vector<uint8_t> bytes;
  PutEntry(bytes, 0u | motionOut, 0);
  PutEntry(bytes, 1u | motionOut, 0);
  std::string err;
  auto t = pes::ParseDerivationTable(bytes, SyntheticLayout({2, 0, 0, 0, 0, 0}), SyntheticRules(), &err);
  ASSERT_TRUE(t) << err;
  const auto& list = t->lists[0];
  ASSERT_EQ(list.size(), 2u);
  EXPECT_FALSE(list[0].playerId) << "a zero id field is a generic entry, not player 0";
  ASSERT_TRUE(list[1].playerId);
  EXPECT_EQ(*list[1].playerId, 0) << "stored 1 is player 0";
}

TEST(DerivationPacking, TheSeventeenBitIdFieldKeepsEveryIdUpToItsLimit) {
  // Stored 0x1FFFF is the largest id, 131070. Stored 0x10000 is 65535, which a 16-bit mask loses.
  std::vector<uint8_t> bytes;
  PutEntry(bytes, uint32_t(kPlayerIdMask) | (uint32_t(521 - kMotionOutBase) << kMotionOutShift), 0);
  PutEntry(bytes, 0x10000u | (uint32_t(522 - kMotionOutBase) << kMotionOutShift), 0);
  std::string err;
  auto t = pes::ParseDerivationTable(bytes, SyntheticLayout({2, 0, 0, 0, 0, 0}), SyntheticRules(), &err);
  ASSERT_TRUE(t) << err;
  EXPECT_EQ(pes::DerivePlayerMotion(*t, 131070, kMotionInBase, 0), 521);
  EXPECT_EQ(pes::DerivePlayerMotion(*t, 65535, kMotionInBase, 0), 522);
  EXPECT_EQ(pes::DerivePlayerMotion(*t, 65534, kMotionInBase, 0), kMotionInBase) << "no entry is 65534";
}

TEST(DerivationPacking, MotionAndSubFieldsAreMaskedAndJunkAboveThemIsIgnored) {
  // dword0: id 7, a 12-bit motion-out field of 0x2AB, and junk in bits 29 and 31 (the decoded
  // table's own entry 8 carries bit 29). word1: motion-in field 0x5A, sub 3, and junk in dword1's
  // high half, which the walkers never read.
  constexpr uint32_t kOutField = 0x2AB;
  constexpr uint32_t kJunkHigh = (1u << 29) | (1u << 31);
  std::vector<uint8_t> bytes;
  PutEntry(bytes, (7u + 1u) | (kOutField << kMotionOutShift) | kJunkHigh,
           0x5Au | (3u << kSubShift) | (0xABCDu << 16));
  std::string err;
  auto t = pes::ParseDerivationTable(bytes, SyntheticLayout({1, 0, 0, 0, 0, 0}), SyntheticRules(), &err);
  ASSERT_TRUE(t) << err;
  ASSERT_EQ(t->lists[0].size(), 1u);
  EXPECT_EQ(t->lists[0][0].motionOut, kMotionOutBase + int(kOutField));
  EXPECT_EQ(t->lists[0][0].motionIn, kMotionInBase + 0x5A);
  EXPECT_EQ(t->lists[0][0].sub, 3);
}

// ---------------------------------------------------------------------------------------------
// FUN_1406B3560: the player walk and its tail
// ---------------------------------------------------------------------------------------------

TEST(DerivationPlayerWalk, MatchesTheIdAndTheMotionInAndTheFirstChangeWins) {
  const auto t = Table({{0, 7, 600, 610, 0}, {0, 8, 600, 620, 0}, {0, 7, 600, 630, 0}});
  EXPECT_EQ(pes::DerivePlayerMotion(t, 8, 600, 0), 620);
  EXPECT_EQ(pes::DerivePlayerMotion(t, 7, 600, 0), 610) << "the first entry for the player wins";
  EXPECT_EQ(pes::DerivePlayerMotion(t, 9, 600, 0), 600) << "no entry for player 9";
  EXPECT_EQ(pes::DerivePlayerMotion(t, 7, 601, 0), 601) << "the motion in must match";
}

TEST(DerivationPlayerWalk, SkipsAnIdentityEntryWithoutStoppingTheScan) {
  // The first row substitutes 600 with itself. The scan carries on and the second row changes it.
  const auto t = Table({{1, 7, 600, 600, 0}, {1, 7, 600, 640, 0}});
  EXPECT_EQ(pes::DerivePlayerMotion(t, 7, 600, 1), 640);
}

TEST(DerivationPlayerWalk, PlayerZeroMatchesEveryEntryByMotionOnly) {
  // The walker skips the id test when the id is 0, so any entry with motion in 700 answers, a
  // player's or a generic one.
  const auto t = Table({{3, 9, 700, 750, 0}, {3, std::nullopt, 700, 760, 0}});
  EXPECT_EQ(pes::DerivePlayerMotion(t, 0, 700, 3), 750);
  EXPECT_EQ(pes::DerivePlayerMotion(t, 9, 700, 3), 750);
  EXPECT_EQ(pes::DerivePlayerMotion(t, 5, 700, 3), 700) << "a generic row never answers for a player";
}

TEST(DerivationPlayerWalk, TheTailRewritesItsTwoCardsOverTheListResult) {
  const pes::DerivationRules r = SyntheticRules();
  // The list says 999 for the tail's cards; the tail still rewrites them.
  const auto t = Table({{2, r.tailIdA, r.freeKickIdle, 999, 0}, {1, r.tailIdB, r.curvePlacekick, 999, 0}});
  EXPECT_EQ(pes::DerivePlayerMotion(t, r.tailIdA, r.freeKickIdle, r.freeKickKind), r.freeKickIdle + r.beckham2002Offset);
  EXPECT_EQ(pes::DerivePlayerMotion(t, r.tailIdB, r.curvePlacekick, r.curveKind), r.curveBeckham2002);
  EXPECT_EQ(pes::DerivePlayerMotion(t, r.tailIdA, r.freeKickRunup, r.freeKickKind), r.freeKickRunup + r.beckham2002Offset)
      << "the tail needs no list row";
  EXPECT_EQ(pes::DerivePlayerMotion(t, r.tailIdA, r.freeKickIdle, r.curveKind), r.freeKickIdle)
      << "the free-kick rewrite is kind 2 only";
  EXPECT_EQ(pes::DerivePlayerMotion(t, r.beckhamIds[0], r.freeKickIdle, r.freeKickKind), r.freeKickIdle)
      << "other cards are not rewritten";
}

// ---------------------------------------------------------------------------------------------
// FUN_1406B3780: the generic walk
// ---------------------------------------------------------------------------------------------

TEST(DerivationGenericWalk, MatchesTheStatPlusOneAgainstTheSubField) {
  // A generic row with sub 3 answers to stat 2 (its sub minus one), not stat 3.
  const auto t = Table({{5, std::nullopt, 510, 511, 3}});
  EXPECT_EQ(pes::DeriveGenericMotion(t, 2, 510, 5), 511);
  EXPECT_EQ(pes::DeriveGenericMotion(t, 3, 510, 5), 510);
}

TEST(DerivationGenericWalk, IgnoresEntriesThatNameAPlayer) {
  const auto t = Table({{5, 7, 510, 512, 2}, {5, std::nullopt, 510, 513, 4}});
  EXPECT_EQ(pes::DeriveGenericMotion(t, 1, 510, 5), 510) << "a player's row is not generic";
  EXPECT_EQ(pes::DeriveGenericMotion(t, 3, 510, 5), 513);
}

// ---------------------------------------------------------------------------------------------
// FUN_14071E570: the long-kick selection
// ---------------------------------------------------------------------------------------------

TEST(DerivationLongKick, TheBeckhamBranchNeedsAListedCardACurvePickAFlagClearAndTheAnimeVariant) {
  const pes::DerivationRules r = SyntheticRules();
  const pes::DerivationTable t = Table({}, r);
  for (int card : r.beckhamIds)
    EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, card, r.curve)), r.curveBeckham2002) << "card " << card;
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, r.beckhamIds[0], r.curve + r.curveSpan)), r.curveBeckham2002);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, r.beckhamIds[0], r.curveMax)), r.curveBeckham2002);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, r.beckhamIds[0], r.curveMax + 1)), r.curveMax + 1)
      << "one past the curve range is a plain pick";
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 2000, r.curve)), r.curve) << "an unlisted card";
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, r.beckhamIds[0], r.curve, true)), r.curve) << "the flag must be clear";
  pes::LongKickInput wrongVariant = Kick(r, r.beckhamIds[0], r.curve);
  wrongVariant.animeVariant = r.beckhamVariant + 1;
  EXPECT_EQ(pes::SelectLongKickClip(t, wrongVariant), r.curve) << "the anime state must be the Beckham variant";
}

TEST(DerivationLongKick, ASubstitutableClipTakesThePlayerWalkThenTheGenericWalk) {
  const pes::DerivationRules r = SyntheticRules();
  // Player 42 has its own row for 600; the generic row for 600 answers to stat 4 (sub 5 minus one).
  const auto t = Table({{3, 42, 600, 650, 0}, {1, std::nullopt, 600, 660, 5}}, r);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 42, 600, false, true, 4)), 650) << "the player walk wins";
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 43, 600, false, true, 4)), 660) << "the generic walk next";
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 43, 600, false, true, 3)), 600) << "no generic match";
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 43, 600, false, false, 4)), 600) << "not substitutable: no walk";
}

TEST(DerivationLongKick, TheCurveWalksTheGenericListAsThePlacekick) {
  const pes::DerivationRules r = SyntheticRules();
  // Only the placekick has a generic row here. The curve is looked up as the placekick, so the
  // curve's pick answers with it.
  const auto t = Table({{1, std::nullopt, r.curvePlacekick, 515, 4}}, r);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 43, r.curve, false, true, 3)), 515);
}

TEST(DerivationLongKick, TheCurveReturnsToItselfWhenThePlacekickWalkFindsNothing) {
  const pes::DerivationRules r = SyntheticRules();
  const auto t = Table({}, r);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 43, r.curve, false, true, 3)), r.curve);
}

TEST(DerivationLongKick, TheRemapsRewriteTheirClipsWhateverTheWalksSaid) {
  const pes::DerivationRules r = SyntheticRules();
  const auto t = Table({{3, 44, 600, r.remapFrom, 0}}, r);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 44, r.remapFrom)), r.remapFrom + r.remapDelta);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 44, r.remapFrom2)), r.remapTo2);
  EXPECT_EQ(pes::SelectLongKickClip(t, Kick(r, 44, 600, false, true, 0)), r.remapFrom + r.remapDelta)
      << "a walk result is remapped too";
}

// ---------------------------------------------------------------------------------------------
// The install: the user's own file and exe, through the VFS winner
// ---------------------------------------------------------------------------------------------

struct Install {
  std::optional<pes::PeImage> exe;
  pes::PesVfs vfs;
  std::vector<std::string> names;
  pes::DerivationTable table;
};

// True when a download/ pack ships the table: the install's entries are then that pack's.
bool DownloadShipsTheTable(const std::string& root) {
  std::error_code ec;
  for (fs::directory_iterator it(fs::path(root) / "download", ec), end; !ec && it != end; it.increment(ec)) {
    if (it->path().extension() != ".cpk") continue;
    pes::CpkArchive cpk;
    std::string err;
    if (!cpk.Open(it->path().string(), &err)) continue;
    for (const pes::CpkEntry& e : cpk.Entries())
      if (pes::PesVfs::Fold(e.path) == pes::PesVfs::Fold(kTablePath)) return true;
  }
  return false;
}

bool LoadInstall(const char* root, Install* in, std::string* err) {
  in->exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", err);
  if (!in->exe || !in->vfs.Mount(root, err)) return false;
  auto names = pes::MotionNames(*in->exe, in->vfs, err);
  if (!names) return false;
  in->names = std::move(*names);
  auto table = pes::LoadDerivationTable(*in->exe, in->vfs, err);
  if (!table) return false;
  in->table = std::move(*table);
  return true;
}

// The motion number of a clip name in the install's table, or -1.
int MotionOf(const Install& in, const std::string& name) {
  for (size_t i = 0; i < in.names.size(); ++i)
    if (in.names[i] == name) return int(i);
  return -1;
}

std::string NameOf(const Install& in, int motion) {
  return motion >= 0 && size_t(motion) < in.names.size() ? in.names[motion] : "<out of range>";
}

TEST(DerivationInstall, TheTableHoldsItsSixListsAsWholeStockEntries) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  if (DownloadShipsTheTable(root)) GTEST_SKIP() << "a download/ pack ships the table";
  Install in;
  std::string err;
  ASSERT_TRUE(LoadInstall(root, &in, &err)) << err;
  const auto layout = pes::LoadDerivationLayout(*in.exe, &err);
  ASSERT_TRUE(layout) << err;
  size_t entries = 0;
  for (int k = 0; k < pes::DERIVATION_KINDS; ++k) {
    EXPECT_EQ(in.table.lists[k].size(), size_t(layout->listLength[k])) << "list " << k;
    entries += in.table.lists[k].size();
  }
  EXPECT_EQ(entries, 106u) << "8 + 35 + 23 + 15 + 16 + 9";
  ASSERT_TRUE(in.vfs.Size(kTablePath));
  EXPECT_EQ(*in.vfs.Size(kTablePath), 8u * entries) << "eight bytes an entry, no slack";
}

TEST(DerivationInstall, EveryEntryNamesAClipInTheInstallsMotionTable) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  if (DownloadShipsTheTable(root)) GTEST_SKIP() << "a download/ pack ships the table";
  Install in;
  std::string err;
  ASSERT_TRUE(LoadInstall(root, &in, &err)) << err;
  for (const auto& list : in.table.lists)
    for (const pes::DerivationEntry& e : list) {
      ASSERT_LT(size_t(e.motionIn), in.names.size());
      ASSERT_LT(size_t(e.motionOut), in.names.size());
      EXPECT_FALSE(in.names[e.motionIn].empty());
      EXPECT_FALSE(in.names[e.motionOut].empty());
    }
}

TEST(DerivationInstall, BeckhamsFreeKicksSubstituteByName) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  if (DownloadShipsTheTable(root)) GTEST_SKIP() << "a download/ pack ships the table";
  Install in;
  std::string err;
  ASSERT_TRUE(LoadInstall(root, &in, &err)) << err;
  const int idle = MotionOf(in, "fk_idle"), runup = MotionOf(in, "fk_runup");
  const int idleB = MotionOf(in, "fk_idle_beckham_2002"), runupB = MotionOf(in, "fk_runup_beckham_2002");
  ASSERT_GE(idle, 0);
  ASSERT_GE(runup, 0);
  ASSERT_GE(idleB, 0);
  ASSERT_GE(runupB, 0);
  // Two of the five cards have no list rows: their rewrite is the tail's own.
  for (int card : in.table.rules.beckhamIds) {
    EXPECT_EQ(NameOf(in, pes::DerivePlayerMotion(in.table, card, idle, 2)), "fk_idle_beckham_2002") << "card " << card;
    EXPECT_EQ(NameOf(in, pes::DerivePlayerMotion(in.table, card, runup, 2)), "fk_runup_beckham_2002") << "card " << card;
  }
}

TEST(DerivationInstall, BeckhamsPlacekickSubstitutesByName) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  if (DownloadShipsTheTable(root)) GTEST_SKIP() << "a download/ pack ships the table";
  Install in;
  std::string err;
  ASSERT_TRUE(LoadInstall(root, &in, &err)) << err;
  const int placekick = MotionOf(in, "kick_long_3_0_inside_y0_045_curve_placekick");
  ASSERT_GE(placekick, 0);
  for (int card : in.table.rules.beckhamIds)
    EXPECT_EQ(NameOf(in, pes::DerivePlayerMotion(in.table, card, placekick, 1)),
              "kick_long_3_0_inside_y0_045_curve_beckham_2002")
        << "card " << card;
}

TEST(DerivationInstall, BeckhamsCurvedKickSelectsTheBeckham2002ClipByName) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  if (DownloadShipsTheTable(root)) GTEST_SKIP() << "a download/ pack ships the table";
  Install in;
  std::string err;
  ASSERT_TRUE(LoadInstall(root, &in, &err)) << err;
  const std::string beckham2002 = "kick_long_3_0_inside_y0_045_curve_beckham_2002";
  for (const char* picked : {"kick_long_3_0_inside_y0_045_curve", "kick_long_3_0_inside_y0_045_curve_beckham",
                             "kick_long_3_0_inside_y0_045_curve_max"}) {
    const int pick = MotionOf(in, picked);
    ASSERT_GE(pick, 0) << picked;
    for (int card : in.table.rules.beckhamIds) {
      const auto kick = Kick(in.table.rules, card, pick, false, true, 0);
      EXPECT_EQ(NameOf(in, pes::SelectLongKickClip(in.table, kick)), beckham2002) << picked << ", card " << card;
      const auto flagged = Kick(in.table.rules, card, pick, true, true, 0);
      EXPECT_NE(NameOf(in, pes::SelectLongKickClip(in.table, flagged)), beckham2002) << "the flag blocks it";
    }
  }
}

TEST(DerivationInstall, NeymarsFeintIsItsOwnDoubleTouchSoleClip) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  if (DownloadShipsTheTable(root)) GTEST_SKIP() << "a download/ pack ships the table";
  Install in;
  std::string err;
  ASSERT_TRUE(LoadInstall(root, &in, &err)) << err;
  const int feint = MotionOf(in, "feintrun_doubletouch_3_3_f090");
  ASSERT_GE(feint, 0);
  // List 0 is Neymar's eight feints (docs §1): its first row is this feint, and the row names the card.
  ASSERT_FALSE(in.table.lists[0].empty());
  const pes::DerivationEntry& row = in.table.lists[0].front();
  ASSERT_EQ(row.motionIn, feint);
  ASSERT_TRUE(row.playerId);
  EXPECT_EQ(NameOf(in, pes::DerivePlayerMotion(in.table, *row.playerId, feint, 0)),
            "feintrun_ballroll_onetime_3_3_f090_y0_Ltouch_in_ver03");
}

TEST(DerivationInstall, ListFivesPlacekickVariantsResolveByTheirSub) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  if (DownloadShipsTheTable(root)) GTEST_SKIP() << "a download/ pack ships the table";
  Install in;
  std::string err;
  ASSERT_TRUE(LoadInstall(root, &in, &err)) << err;
  const int placekick = MotionOf(in, "kick_long_3_0_inside_y0_045_curve_placekick");
  ASSERT_GE(placekick, 0);
  // A row's sub is the stat plus one: sub 2 is Silva's, sub 3 Özil's, sub 10 Coutinho's.
  EXPECT_EQ(NameOf(in, pes::DeriveGenericMotion(in.table, 1, placekick, 5)), "kick_long_3_0_inside_y0_045_curve_silva");
  EXPECT_EQ(NameOf(in, pes::DeriveGenericMotion(in.table, 2, placekick, 5)), "kick_long_3_0_inside_y0_045_curve_ozil");
  EXPECT_EQ(NameOf(in, pes::DeriveGenericMotion(in.table, 9, placekick, 5)), "kick_long_3_0_inside_y0_045_curve_coutinho");
}

}  // namespace
