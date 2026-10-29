// The slot record's hash-checked stamina cells (+0xC8 cur, +0xE8 max, +0x108 breath) as GF writes them from a player's StaminaState
// (WriteSlotStamina, src/sim/onthepitch/pesstamina.cpp), against PES:
//  1. PES's own readers, the translated 0x140A58550 (tank percent) and 0x140A581C0 (reserve percent: the readers the trap chain
//     calls, which abort the process on a bad hash), run on a GF-written record and answer what the state says; a corrupted cell
//     aborts them (the death test: the readers do check, so the passing runs mean something);
//  2. the cells of the full captures (tools/dev/translate/stamina_scan.py): for every player, GF's store of PES's decoded value under
//     PES's own key and ring index equals PES's cell (hash, key, index and the ring word the index selects), and PES's reader on GF's
//     cells for the same state answers what it answers on PES's cells.
// PES21_ROOT (the install's PES2021.exe: the breath cap's constants) is needed; GF_PES_STAMINA_SCAN names the scan TSV (PES-derived,
// never committed). Without them the tests skip.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"
#include "onthepitch/pesattrs.hpp"
#include "onthepitch/pesstamina.hpp"

namespace {

using pes::attrs::GuardedWordSet;

constexpr uint64_t kRecordVa = 0x2200000000ULL, kStackVa = 0x2200010000ULL;
constexpr std::size_t kRecordBytes = 0x140;

class StaminaCells : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (root == nullptr || *root == '\0') GTEST_SKIP() << "PES21_ROOT unset";
    std::string err;
    const auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    const auto k = pes::LoadStaminaConstants(*exe, &err);
    ASSERT_TRUE(k.has_value()) << err;
    k_ = *k;
  }

  // PES's reader at `fn` (rcx = the slot record), run on a copy of `record`: rax. Aborts the process on a bad hash.
  static uint64_t Run(void (*fn)(PesState&), const std::array<uint8_t, kRecordBytes>& record) {
    PesState s;
    s.mem.segs.push_back(PesMem::Seg{kRecordVa, std::vector<uint8_t>(record.begin(), record.end())});
    s.mem.segs.push_back(PesMem::Seg{kStackVa - 0x1000, std::vector<uint8_t>(0x1000)});
    s.gpr[1] = kRecordVa;
    s.gpr[4] = kStackVa - 0x800;  // the readers store into the caller's home space above rsp: the segment reaches past it
    fn(s);
    return s.gpr[0];
  }
  static uint64_t TankPct(const std::array<uint8_t, kRecordBytes>& r) { return Run(::pes_fn_140A58550, r) & 0xFFFFFFFFULL; }
  static uint64_t ReservePct(const std::array<uint8_t, kRecordBytes>& r) { return Run(::pes_fn_140A581C0, r) & 0xFFFFFFFFULL; }

  std::array<uint8_t, kRecordBytes> Written(const pes::StaminaState& s, uint32_t clock = 12345) const {
    std::array<uint8_t, kRecordBytes> r{};
    pes::WriteSlotStamina(r.data(), k_, s, clock);
    return r;
  }
  static pes::StaminaState State(int max, int cur, int breath) {
    pes::StaminaState s;
    s.max = max;
    s.cur = cur;
    s.breath = breath;
    return s;
  }

  pes::StaminaConstants k_;
};

// PES's readers accept the cells and answer what the state says, across the tank's range, the clamps (breath above the cap, cur above
// max) and a player with no tank.
TEST_F(StaminaCells, PesOwnReadersAnswerTheStatesPercents) {
  for (const int max : {96000, 130000, 16000}) {
    for (const int cur : {0, 1, max / 3, max / 2, max - 1, max}) {
      for (const int breath : {0, 1, 400, 1000, 1920, 3000}) {
        const pes::StaminaState s = State(max, cur, breath);
        const auto r = Written(s);
        const int pct = cur * 100 / max;
        EXPECT_EQ(TankPct(r), static_cast<uint64_t>(pct)) << max << " " << cur;
        // The reserve percent PES takes: the breath (clamped to the cap by the setter) over the cap, `pct` floored at 20.
        const int cap = pes::BreathCap(k_, s);
        EXPECT_EQ(ReservePct(r), static_cast<uint64_t>(std::min(breath, cap) * 100 / cap)) << max << " " << cur << " " << breath;
      }
    }
  }
  EXPECT_EQ(TankPct(Written(State(0, 0, 0))), 0u);  // a player with no tank: PES's reader answers 0
}

// cur above max is replaced by the max cell (0x140A592C0), breath above the cap by the cap (0x140A58F60): PES's readers see the clamped
// values.
TEST_F(StaminaCells, TheSettersClampLikePes) {
  const pes::StaminaState over = State(96000, 120000, 99999);
  const auto r = Written(over);
  EXPECT_EQ(TankPct(r), 100u);
  EXPECT_EQ(ReservePct(r), 100u);
  GuardedWordSet cur, max;
  std::memcpy(&cur, r.data() + pes::kSlotStaminaCurOffset, sizeof cur);
  std::memcpy(&max, r.data() + pes::kSlotStaminaMaxOffset, sizeof max);
  EXPECT_EQ(pes::attrs::ReadGuardedWord(cur).word, 96000);
  EXPECT_EQ(cur.hash, max.hash);
  EXPECT_EQ(r[pes::kSlotStaminaDirtyOffset], 1);
}

// The readers check: a flipped bit in a cell's ring word aborts them (the abort the match hit before the cells were written).
TEST_F(StaminaCells, ACorruptedCellAbortsPesReader) {
  auto r = Written(State(96000, 48000, 500));
  GuardedWordSet cur;
  std::memcpy(&cur, r.data() + pes::kSlotStaminaCurOffset, sizeof cur);
  r[pes::kSlotStaminaCurOffset + 0x10 + 4 * (cur.index & 3)] ^= 0xFF;  // the ring word the index selects
  EXPECT_DEATH(TankPct(r), "");
}

// The cells of the full captures: GF's store equals PES's cell, and PES's readers answer alike on both.
TEST_F(StaminaCells, EveryScannedPlayersCellsEqualGfs) {
  const char* scan = std::getenv("GF_PES_STAMINA_SCAN");
  if (scan == nullptr || *scan == '\0') GTEST_SKIP() << "GF_PES_STAMINA_SCAN unset";
  std::ifstream in(scan);
  ASSERT_TRUE(in.good()) << scan;
  constexpr std::array<std::size_t, 3> kCells = {pes::kSlotStaminaCurOffset, pes::kSlotStaminaMaxOffset, pes::kSlotBreathOffset};
  int players = 0, cells = 0, clamped = 0;
  for (std::string line; std::getline(in, line);) {
    std::vector<std::string> col;
    std::stringstream ss(line);
    for (std::string c; std::getline(ss, c, '\t');) col.push_back(c);
    ASSERT_EQ(col.size(), 6u) << line;
    std::array<uint8_t, kRecordBytes> pesRecord{};
    std::array<int, 3> value{};
    for (std::size_t i = 0; i < kCells.size(); ++i) {
      std::vector<uint64_t> f;
      std::stringstream cs(col[3 + i]);
      for (std::string h; std::getline(cs, h, ':');) f.push_back(std::stoull(h, nullptr, 16));
      ASSERT_EQ(f.size(), 8u) << col[3 + i];
      GuardedWordSet pc;
      pc.hash = f[1];
      pc.f = static_cast<int32_t>(f[2]);
      pc.index = static_cast<uint32_t>(f[3]);
      for (int j = 0; j < 4; ++j) pc.ring[j] = static_cast<uint32_t>(f[4 + j]);
      std::memcpy(pesRecord.data() + kCells[i], &pc, sizeof pc);
      const auto read = pes::attrs::ReadGuardedWord(pc);
      ASSERT_EQ(read.status, pes::attrs::GuardedWordStatus::kOk) << col[0] << " " << col[1] << " cell " << i;
      value[i] = read.word;
      // GF's store of PES's value under PES's key and index is PES's cell: the hash, the key, the index, the selected ring word.
      GuardedWordSet gf;
      gf.f = pc.f;
      pes::attrs::StoreGuardedWord(&gf, static_cast<uint32_t>(read.word), pc.index);
      EXPECT_EQ(gf.hash, pc.hash) << col[0] << " " << col[1] << " cell " << i;
      EXPECT_EQ(gf.f, pc.f);
      EXPECT_EQ(gf.index, pc.index);
      EXPECT_EQ(gf.ring[pc.index & 3], pc.ring[pc.index & 3]) << col[0] << " " << col[1] << " cell " << i;
      ++cells;
    }
    // The state PES holds, written by GF in a record of its own: PES's readers answer as they do on PES's record.
    const pes::StaminaState s = State(value[1], value[0], value[2]);
    clamped += s.cur > s.max || s.breath > pes::BreathCap(k_, s);
    const auto gfRecord = Written(s);
    EXPECT_EQ(TankPct(gfRecord), TankPct(pesRecord)) << col[0] << " " << col[1];
    EXPECT_EQ(ReservePct(gfRecord), ReservePct(pesRecord)) << col[0] << " " << col[1];
    ++players;
  }
  EXPECT_GT(players, 0) << "the scan holds no player";
  EXPECT_EQ(cells, 3 * players);
  // PES's own states never need the setters' clamps: GF's writer leaves a real state as it is.
  EXPECT_EQ(clamped, 0);
}

}  // namespace
