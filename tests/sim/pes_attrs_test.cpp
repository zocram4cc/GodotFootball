// PES 2021's 27A0 helpers (FUN_140A603C0, FUN_140A62610, FUN_140A58450/58350): the rating
// curve over a table, the keeper starter index, and the guarded word's normal path with its
// FNV check. The expected words are written out by hand in the tests, not taken from the code.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include "onthepitch/pesattrs.hpp"
#include "pes/exe.hpp"

namespace {

using pes::attrs::GuardedWordResult;
using pes::attrs::GuardedWordSet;
using pes::attrs::GuardedWordStatus;
using pes::attrs::RatingTable;

// Row 0 flagged (interpolates), row 1 unflagged, row 33 flagged with T[0] = 5.
RatingTable SyntheticTable() {
  RatingTable t;
  for (auto& row : t.rows) {
    row.flag = 0;
    row.t = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
  }
  t.rows[0].flag = 1;
  t.rows[1].flag = 0;
  t.rows[33].flag = 1;
  t.rows[33].t[0] = 5.0f;
  return t;
}

// FNV-1a 64 over the four bytes, written out here (not shared with the code under test).
uint64_t Fnv1a64(const uint8_t bytes[4]) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (int i = 0; i < 4; ++i) {
    h ^= bytes[i];
    h *= 0x100000001b3ull;
  }
  return h;
}

// The encoding written out byte by byte from the decode: e_i = (w_i + K_i) ^ (K_i * 63),
// with K_i = byte i of (3*f + 4*i), all mod 256.
void ReferenceEncode(uint32_t word, int32_t f, uint8_t out[4]) {
  const uint32_t three_f = static_cast<uint32_t>(f) * 3u;
  for (int i = 0; i < 4; ++i) {
    const uint32_t wi = (word >> (8 * i)) & 0xffu;
    const uint32_t value = three_f + 4u * static_cast<uint32_t>(i);
    const uint32_t ki = (value >> (8 * i)) & 0xffu;
    out[i] = static_cast<uint8_t>(((wi + ki) & 0xffu) ^ ((ki * 63u) & 0xffu));
  }
}

GuardedWordSet MakeSet(uint32_t ring2, int32_t f, uint32_t index, bool goodHash) {
  GuardedWordSet set;
  set.ring = {0x11111111u, 0x22222222u, ring2, 0x44444444u};
  set.f = f;
  set.index = index;
  uint8_t e[4];
  ReferenceEncode(ring2, f, e);
  set.hash = goodHash ? Fnv1a64(e) : Fnv1a64(e) ^ 1ull;
  return set;
}

// ---- FUN_140A603C0 -------------------------------------------------------------

TEST(PesAttrsRating, ZeroBandReturnsTheFirstValue) {
  const RatingTable t = SyntheticTable();
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 0, 0), 0.0f);
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, -250, 0), 0.0f);  // q < 0 clamps to band 0
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, -50, 0), 0.0f);
}

TEST(PesAttrsRating, FlaggedRowInterpolatesBetweenBands) {
  const RatingTable t = SyntheticTable();
  // 250: q = 2, r = 50: halfway between T[2] = 20 and T[3] = 30.
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 250, 0), 25.0f);
  // 199: q = 1, r = 99: 10 + 10 * 0.99.
  EXPECT_NEAR(pes::attrs::RatingCurve(t, 199, 0), 19.9f, 1e-4f);
  // r == 0 returns the band itself.
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 300, 0), 30.0f);
}

TEST(PesAttrsRating, UnflaggedRowAndTopBandDoNotInterpolate) {
  const RatingTable t = SyntheticTable();
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 250, 1), 20.0f);  // flag clear
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 650, 0), 60.0f);  // q clamps to 6: no interpolation
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 1200, 0), 60.0f);
}

TEST(PesAttrsRating, RowsAboveThirtyThreeReadZeroAndRow33UsesItsFirstValue) {
  const RatingTable t = SyntheticTable();
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 250, 34), 0.0f);
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 120, 33), 12.0f);  // q = 1, flagged, r = 20: 10 + 0.2 * 10
  EXPECT_FLOAT_EQ(pes::attrs::RatingCurve(t, 0, 33), 5.0f);
}

// ---- FUN_140A62610 / FUN_140A62690 ----------------------------------------------

TEST(PesAttrsKeeper, FirstOnPitchStarterWithRoleClassZeroIsTheKeeper) {
  std::array<bool, 22> onPitch{};
  onPitch.fill(true);
  std::array<int, 11> role{1, 2, 0, 3, 4, 5, 6, 7, 8, 9, 0};
  EXPECT_EQ(pes::attrs::KeeperStarterIndex(0, onPitch, role), 2);
  EXPECT_EQ(pes::attrs::KeeperGlobalSlot(0, onPitch, role), 2);
  EXPECT_EQ(pes::attrs::KeeperGlobalSlot(1, onPitch, role), 13);  // side 1: 11 + 2
}

TEST(PesAttrsKeeper, OffPitchOrNoKeeperClassSkipsTheStarter) {
  std::array<bool, 22> onPitch{};
  onPitch.fill(true);
  onPitch[2] = false;  // the first class-0 starter is off the pitch
  std::array<int, 11> role{1, 2, 0, 3, 4, 5, 6, 7, 8, 9, 0};
  EXPECT_EQ(pes::attrs::KeeperStarterIndex(0, onPitch, role), 10);
  std::array<int, 11> none{1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2};
  EXPECT_EQ(pes::attrs::KeeperStarterIndex(0, onPitch, none), pes::attrs::kNoSlot);
  EXPECT_EQ(pes::attrs::KeeperGlobalSlot(0, onPitch, none), pes::attrs::kNoSlot);
}

// ---- FUN_140A58450 / FUN_140A58350: the normal path and the tamper status --------

TEST(PesAttrsGuardedWord, MatchingHashReturnsTheEncodedWord) {
  const GuardedWordSet set = MakeSet(0x01020304u, 84, 2, /*goodHash=*/true);  // 3*84 = 0xFC: byte 1 carries
  const GuardedWordResult r = pes::attrs::ReadGuardedWord(set);
  ASSERT_EQ(r.status, GuardedWordStatus::kOk);
  uint8_t e[4];
  ReferenceEncode(0x01020304u, 84, e);
  const uint32_t expected = static_cast<uint32_t>(e[0]) | (static_cast<uint32_t>(e[1]) << 8) |
                            (static_cast<uint32_t>(e[2]) << 16) |
                            (static_cast<uint32_t>(e[3]) << 24);
  EXPECT_EQ(static_cast<uint32_t>(r.word), expected);
}

TEST(PesAttrsGuardedWord, IndexSelectsTheRingEntryByItsLowTwoBits) {
  // index 6 & 3 == 2: the same ring entry as index 2.
  const GuardedWordSet a = MakeSet(0x01020304u, -77, 2, true);
  GuardedWordSet b = a;
  b.index = 6;
  EXPECT_EQ(pes::attrs::ReadGuardedWord(a).word, pes::attrs::ReadGuardedWord(b).word);
}

TEST(PesAttrsGuardedWord, MismatchedHashIsTheTamperStatus) {
  const GuardedWordSet set = MakeSet(0x01020304u, 1234, 2, /*goodHash=*/false);
  EXPECT_EQ(pes::attrs::ReadGuardedWord(set).status, GuardedWordStatus::kTampered);
}

// ---- the install table ----------------------------------------------------------

TEST(PesAttrsInstall, LoadsTheRatingTableOfThirtyFourRows) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the rating table comes from the install";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto table = pes::attrs::LoadRatingTable(*exe, &err);
  ASSERT_TRUE(table.has_value()) << err;
  EXPECT_EQ(table->rows.size(), static_cast<size_t>(pes::attrs::kRatingRows));
  for (const auto& row : table->rows) {
    EXPECT_TRUE(row.flag == 0 || row.flag == 1);
  }
}

}  // namespace
