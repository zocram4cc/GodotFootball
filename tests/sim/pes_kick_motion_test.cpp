// PES 2021's kick motion pick (docs/ports/pes-port-KickMotion.md): the clip table, the row fix-ups, the Animetable cell, the
// angle bucket and the descriptor byte Q's in-flight branch reads. The oracle tests compare against PES's own functions called
// over a kick_scatter snapshot (tools/dev/rendezvous/kick_motion_replay.py): its TSVs hold PES bytes, stay in scratch and reach
// the tests through PES_KICK_MOTION_DIR. Every relation test builds its data from the install, never from a pinned value.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "onthepitch/peskickerror.hpp"
#include "onthepitch/peskickmotion.hpp"
#include "pes/animetable.hpp"
#include "pes/exe.hpp"
#include "pes/mbinfo.hpp"
#include "pes/vfs.hpp"

namespace {

struct Install {
  std::optional<pes::PeImage> exe;
  pes::PesVfs vfs;
  pes::KickMotionConstants k;
  pes::SituationConstants sc;
  std::optional<pes::Animetable> table;
  std::optional<pes::Mbinfo> mbinfo;
};

// One install per process: the loaders read ~2 MB of files.
const Install* LoadInstall(std::string* err) {
  static std::optional<Install> install;
  static std::string failure;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const char* root = std::getenv("PES21_ROOT");
    if (root) {
      Install i;
      i.exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &failure);
      if (i.exe && i.vfs.Mount(root, &failure)) {
        auto k = pes::LoadKickMotionConstants(*i.exe, i.vfs, &failure);
        auto e = pes::LoadKickErrorConstants(*i.exe, &failure);
        i.table = pes::LoadAnimetable(*i.exe, i.vfs, &failure);
        i.mbinfo = pes::LoadMbinfo(i.vfs, *i.exe, &failure);
        if (k && e && i.table && i.mbinfo) {
          i.k = *k;
          i.sc = e->situation;
          install = std::move(i);
        }
      }
    }
  }
  if (!install) *err = failure.empty() ? "PES21_ROOT not set" : failure;
  return install ? &*install : nullptr;
}

#define NEED_INSTALL(var)                                   \
  std::string err_;                                         \
  const Install* var = LoadInstall(&err_);                  \
  if (!var) GTEST_SKIP() << err_

std::optional<std::vector<std::vector<std::string>>> ReadTsv(const char* name) {
  const char* dir = std::getenv("PES_KICK_MOTION_DIR");
  if (!dir) return std::nullopt;
  std::ifstream in(std::string(dir) + "/" + name);
  if (!in) return std::nullopt;
  std::vector<std::vector<std::string>> rows;
  std::string line;
  std::getline(in, line);  // header
  while (std::getline(in, line)) {
    std::vector<std::string> cells;
    std::stringstream ss(line);
    std::string c;
    while (std::getline(ss, c, '\t')) cells.push_back(c);
    rows.push_back(std::move(cells));
  }
  return rows;
}

#define NEED_TSV(var, name)                                                    \
  auto var##_opt = ReadTsv(name);                                              \
  if (!var##_opt) GTEST_SKIP() << "PES_KICK_MOTION_DIR/" name " not available"; \
  const auto& var = *var##_opt;                                                \
  ASSERT_FALSE(var.empty()) << name

float FromBits(const std::string& hex) {
  const uint32_t bits = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// ---- oracle: PES's own functions ---------------------------------------------------------------------------------------

TEST(PesKickMotionOracle, TheClipTableIsPesTableByteForByte) {
  NEED_INSTALL(in);
  const char* dir = std::getenv("PES_KICK_MOTION_DIR");
  if (!dir) GTEST_SKIP() << "PES_KICK_MOTION_DIR not set";
  std::ifstream f(std::string(dir) + "/cliptable.hex");
  std::string hex;
  ASSERT_TRUE(std::getline(f, hex)) << "cliptable.hex";
  ASSERT_EQ(hex.size(), in->k.clip.size() * 4);
  int wrong = 0;
  for (size_t i = 0; i < in->k.clip.size(); ++i) {
    const uint16_t want = static_cast<uint16_t>(std::stoul(hex.substr(4 * i, 2), nullptr, 16) |
                                                std::stoul(hex.substr(4 * i + 2, 2), nullptr, 16) << 8);
    if (static_cast<uint16_t>(in->k.clip[i]) != want && ++wrong <= 10)
      ADD_FAILURE() << "cell " << i << " (kind " << i / 24 << " band " << i / 2 % 12 << " side " << i % 2 << "): got "
                    << in->k.clip[i] << " want " << static_cast<int16_t>(want);
  }
  EXPECT_EQ(wrong, 0);
}

TEST(PesKickMotionOracle, TheClipRowWithoutAPlayerEqualsPes) {
  NEED_INSTALL(in);
  NEED_TSV(rows, "rows.tsv");
  int compared = 0, wrong = 0;
  for (const auto& r : rows) {
    if (r[0] != "1") continue;  // ap null: the table lookup alone
    const int got = pes::KickRow(in->k, in->sc, std::stoi(r[1]), std::stoi(r[2]), std::stoi(r[3]), std::stoi(r[4]),
                                 FromBits(r[5]), false, false, nullptr);
    ++compared;
    if (got != std::stoi(r[6]) && ++wrong <= 10)
      ADD_FAILURE() << "kind " << r[2] << " band " << r[3] << " side " << r[4] << " scale " << r[5] << ": got " << got << " want " << r[6];
  }
  EXPECT_GT(compared, 1000);
  EXPECT_EQ(wrong, 0) << wrong << " of " << compared;
}

// The fix-ups run on the snapshot's match: an odd tick (the capture's own counter), not the first side, no lateral run, no
// action bit. Rows that end on 0x809 read the ball and the player's displacement, which the snapshot does not give as scalars:
// they are compared only where the table itself does not name that row.
TEST(PesKickMotionOracle, TheRowWithAPlayerEqualsPesAtTheCapturesMatch) {
  NEED_INSTALL(in);
  NEED_TSV(rows, "rows.tsv");
  NEED_TSV(cap, "capture.tsv");
  pes::KickWorld world;
  world.tickOdd = std::stoul(cap[0][10]) & 1;
  world.firstSide = std::stoi(cap[0][11]) == 1;
  int compared = 0, wrong = 0, swapped = 0;
  for (const auto& r : rows) {
    if (r[0] != "0") continue;
    const int kind = std::stoi(r[2]), band = std::stoi(r[3]), side = std::stoi(r[4]), request = std::stoi(r[1]);
    const int table = pes::KickClipRow(in->k, kind, band, side, FromBits(r[5]));
    if (table == 0x809) continue;
    const int got = pes::KickRow(in->k, in->sc, request, kind, band, side, FromBits(r[5]), false, false, &world);
    ++compared;
    swapped += got != table;
    if (got != std::stoi(r[6]) && ++wrong <= 10)
      ADD_FAILURE() << "request " << request << " kind " << kind << " band " << band << " side " << side << ": got " << got << " want " << r[6];
  }
  EXPECT_GT(compared, 1000);
  EXPECT_GT(swapped, 0) << "the capture's match must exercise a fix-up";
  EXPECT_EQ(wrong, 0) << wrong << " of " << compared;
}

TEST(PesKickMotionOracle, TheMotionOfEveryRowAndColumnEqualsPes) {
  NEED_INSTALL(in);
  NEED_TSV(rows, "motions.tsv");
  int wrong = 0;
  for (const auto& r : rows) {
    const int motion = pes::KickMotion(*in->table, std::stoi(r[0]), std::stoi(r[1]));
    if (motion != std::stoi(r[3]) && ++wrong <= 10) ADD_FAILURE() << "row " << r[0] << " col " << r[1] << ": got " << motion << " want " << r[3];
  }
  EXPECT_EQ(wrong, 0) << wrong << " of " << rows.size();
}

TEST(PesKickMotionOracle, TheDescriptorOfEveryMotionEqualsPes) {
  NEED_INSTALL(in);
  NEED_TSV(rows, "desc.tsv");
  int ones = 0, wrong = 0;
  for (const auto& r : rows) {
    const int got = pes::KickMotionDescriptor(in->k, *in->mbinfo, std::stoi(r[0]));
    ones += got == 1;
    if (got != std::stoi(r[1]) && ++wrong <= 10) ADD_FAILURE() << "motion " << r[0] << ": got " << got << " want " << r[1];
  }
  EXPECT_GT(ones, 100) << "the in-flight branch needs motions that run it";
  EXPECT_EQ(wrong, 0) << wrong << " of " << rows.size();
}

TEST(PesKickMotionOracle, TheBucketEqualsPesOverAnAngleGrid) {
  NEED_INSTALL(in);
  NEED_TSV(rows, "bucket.tsv");
  int wrong = 0;
  for (const auto& r : rows) {
    const int got = pes::KickBucket(in->k, in->sc, FromBits(r[0]), std::stoi(r[1]), FromBits(r[2]));
    if (got != std::stoi(r[3]) && ++wrong <= 10) ADD_FAILURE() << "kind " << r[1] << " angle " << FromBits(r[0]) << " req " << FromBits(r[2]) << ": got " << got << " want " << r[3];
  }
  EXPECT_EQ(wrong, 0) << wrong << " of " << rows.size();
}

// The captured kick: PES's fields at the kick start give the motion the kicker is playing and the descriptor Q read.
TEST(PesKickMotionOracle, TheCapturedKickGivesPesMotionAndDescriptor) {
  NEED_INSTALL(in);
  NEED_TSV(cap, "capture.tsv");
  const auto& c = cap[0];
  pes::KickWorld world;
  world.tickOdd = std::stoul(c[10]) & 1;
  world.firstSide = std::stoi(c[11]) == 1;
  const int row = pes::KickRow(in->k, in->sc, std::stoi(c[5]), std::stoi(c[0]), std::stoi(c[1]), std::stoi(c[3]), 0.0f, false, false, &world);
  EXPECT_EQ(row, std::stoi(c[8]));
  const int motion = pes::KickMotion(*in->table, row, std::stoi(c[2]));
  EXPECT_EQ(motion, std::stoi(c[6])) << "the motion PES's kicker plays";
  EXPECT_EQ(pes::KickMotionDescriptor(in->k, *in->mbinfo, motion), std::stoi(c[7]));
}

// ---- relations that hold on any install ------------------------------------------------------------------------------------

// A kick the clip table has no row for gets no motion: the descriptor is then the missing record's, never the in-flight one.
TEST(PesKickMotionInstall, AnAbsentClipGivesRowNoneAndNoDescriptorOne) {
  NEED_INSTALL(in);
  int absent = 0;
  for (int kind = 1; kind < 0x97; ++kind)
    for (int band = 0; band < pes::kClipBands; ++band) {
      const int row = pes::KickClipRow(in->k, kind, band, 0, 0.0f);
      if (row != pes::kNoClipRow) continue;
      ++absent;
      EXPECT_EQ(pes::KickMotion(*in->table, row, 9), -1) << "row none is outside the Animetable";
    }
  EXPECT_GT(absent, 0) << "ground kinds have no clip in the upper bands";
}

TEST(PesKickMotionInstall, EveryAnimetableCellMapsToTheMotionPesGivesOrTheTwoFixes) {
  NEED_INSTALL(in);
  const int cols = in->table->Layout().cols;
  for (int row = 0; row < pes::kNoClipRow; ++row)
    for (int col = 0; col < cols; ++col) {
      const int cell = in->table->Cell(row, col).motionNo;
      const int got = pes::KickMotion(*in->table, row, col);
      if (cell < 0) {
        ASSERT_EQ(got, 0) << row << "," << col;
      } else if (got != cell) {
        ASSERT_TRUE((cell == 0xA5E && got == 0xA7B) || (cell == 0xABE && got == 0xADC)) << row << "," << col;
      }
    }
}

// ---- no install --------------------------------------------------------------------------------------------------------------

pes::KickMotionConstants Synthetic() {
  pes::KickMotionConstants k;
  k.clip.fill(-1);
  k.n157 = -157.5f, k.n112 = -112.5f, k.n67 = -67.5f, k.n22 = -22.5f, k.p22 = 22.5f, k.p67 = 67.5f, k.p112 = 112.5f;
  k.swapOddTick = {{100, 101}, {110, 111}};
  k.lateralKmh = 19.0f;
  return k;
}

pes::SituationConstants SyntheticSituation() {
  pes::SituationConstants c;
  c.tickRate = 54.0f, c.sixty = 60.0f, c.thousand = 1000.0f, c.turn157 = 157.5f, c.flightHi = 30.0f;
  return c;
}

TEST(PesKickMotion, AnOddTickTakesTheFirstMatchingSwapRowOnly) {
  const pes::KickMotionConstants k = Synthetic();
  const pes::SituationConstants sc = SyntheticSituation();
  pes::KickWorld w;
  w.tickOdd = true;
  EXPECT_EQ(pes::KickRow(k, sc, 9, 1, 0, 0, 0.0f, false, false, nullptr), pes::kNoClipRow);
  pes::KickMotionConstants k2 = k;
  k2.clip[size_t((1 * pes::kClipBands + 0) * pes::kClipSides + 0)] = 100;
  EXPECT_EQ(pes::KickRow(k2, sc, 9, 1, 0, 0, 0.0f, false, false, &w), 101);
  EXPECT_EQ(pes::KickRow(k2, sc, 9, 1, 0, 0, 0.0f, false, false, nullptr), 100) << "no player: no fix-up";
  w.tickOdd = false;
  EXPECT_EQ(pes::KickRow(k2, sc, 9, 1, 0, 0, 0.0f, false, false, &w), 100);
  EXPECT_EQ(pes::KickRow(k2, sc, 9, 1, 0, 0, 0.0f, false, true, &w), 100) << "flag B skips the fix-ups";
}

TEST(PesKickMotion, ThePassAndShotRowsSwapOnlyForTheirOwnRequest) {
  pes::KickMotionConstants k = Synthetic();
  const pes::SituationConstants sc = SyntheticSituation();
  k.clip[size_t((5 * pes::kClipBands + 0) * pes::kClipSides + 0)] = 0x863;
  k.clip[size_t((6 * pes::kClipBands + 0) * pes::kClipSides + 0)] = 0x865;
  pes::KickWorld w;
  EXPECT_EQ(pes::KickRow(k, sc, 9, 5, 0, 0, 0.0f, false, false, &w), 0x864);
  EXPECT_EQ(pes::KickRow(k, sc, 10, 5, 0, 0, 0.0f, false, false, &w), 0x863);
  EXPECT_EQ(pes::KickRow(k, sc, 10, 6, 0, 0, 0.0f, false, false, &w), 0x866);
  EXPECT_EQ(pes::KickRow(k, sc, 9, 6, 0, 0, 0.0f, false, false, &w), 0x865);
}

// Row 0x809 (FUN_14071E6F0): the ball within 30000 / (tick x 3600) m of the player's displacement keeps it at a walk on an odd tick,
// takes 0x875 at a walk on an even tick and 0x877 at a run; a ball beyond it takes 0x874.
TEST(PesKickMotion, TheBallRowSplitsOnTheBallDistanceTheTickAndTheRunSpeed) {
  pes::KickMotionConstants k = Synthetic();
  const pes::SituationConstants sc = SyntheticSituation();
  k.ballRange = 30000.0f, k.rangeDiv = 3600.0f, k.runKmh = 22.0f;
  k.clip[size_t((7 * pes::kClipBands + 0) * pes::kClipSides + 0)] = 0x809;
  const auto row = [&](const pes::KickWorld& w) { return pes::KickRow(k, sc, 9, 7, 0, 0, 0.0f, false, false, &w); };
  pes::KickWorld w;  // ball and player at the same point, standing
  EXPECT_EQ(row(w), 0x875) << "even tick, walking";
  w.tickOdd = true;
  EXPECT_EQ(row(w), 0x809) << "odd tick, walking: no assignment";
  w.dispX = w.ballX = 0.2f;  // 0.2 m per tick = 38.9 km/h at 54 Hz
  EXPECT_EQ(row(w), 0x877) << "running";
  w.ballX = 5.0f;
  EXPECT_EQ(row(w), 0x874) << "ball out of reach";
}

// The lateral table swaps when the displacement in the player's heading frame reaches 19 km/h; the first-side table on the match side.
TEST(PesKickMotion, TheLateralAndFirstSideTablesTakeTheirOwnGate) {
  pes::KickMotionConstants k = Synthetic();
  const pes::SituationConstants sc = SyntheticSituation();
  k.swapLateral = {{200, 201}};
  k.swapFirstSide = {{210, 211}};
  k.clip[size_t((7 * pes::kClipBands + 0) * pes::kClipSides + 0)] = 200;
  k.clip[size_t((8 * pes::kClipBands + 0) * pes::kClipSides + 0)] = 210;
  const auto row = [&](int kind, const pes::KickWorld& w) { return pes::KickRow(k, sc, 9, kind, 0, 0, 0.0f, false, false, &w); };
  pes::KickWorld w;
  w.dispZ = 0.05f;  // 9.7 km/h
  EXPECT_EQ(row(7, w), 200);
  w.dispZ = 0.2f;  // 38.9 km/h forward of heading 0
  EXPECT_EQ(row(7, w), 201);
  w.heading_deg = 90.0f;  // the same step is now sideways: the frame's z axis is -x
  EXPECT_EQ(row(7, w), 200);
  EXPECT_EQ(row(8, w), 210);
  w.firstSide = true;
  EXPECT_EQ(row(8, w), 211);
}

TEST(PesKickMotion, ADescriptorOfAMotionPastTheTableOrWithTooManyHitsIsTheCappedCount) {
  // HitData record: 108 bytes, word 0 bits 0..12 = motion - 0x11. Three records on one motion, cap 2.
  pes::MbinfoBins bins;
  bins.hitData.assign(3 * 108, 0);
  for (int i = 0; i < 3; ++i) bins.hitData[size_t(i) * 108] = 0x20;  // motion 0x31
  pes::MotionCaps caps;
  caps.hit = 2;
  std::string err;
  auto m = pes::ParseMbinfo(bins, caps, &err);
  ASSERT_TRUE(m) << err;
  pes::KickMotionConstants k;
  k.defaultRecordHits = 1;
  EXPECT_EQ(pes::KickMotionDescriptor(k, *m, 0x31), 2) << "PES's builder drops the record past the cap";
  EXPECT_EQ(pes::KickMotionDescriptor(k, *m, 0x32), 0);
  EXPECT_EQ(pes::KickMotionDescriptor(k, *m, -1), 0);
  EXPECT_EQ(pes::KickMotionDescriptor(k, *m, 0x1125), 1) << "past the table: the default record's one entry";
  EXPECT_EQ(pes::KickMotionDescriptor(k, *m, 0xE42), 0x22) << "FUN_1406B0250 answers these two motions without a record";
  EXPECT_EQ(pes::KickMotionDescriptor(k, *m, 0xF7C), 0x18);
}

}  // namespace
