// PES's body-scale G (src/pes/physique.hpp) against PES itself, three ways:
//  1. the translated FUN_14195CA70 run on a block (heights from far below the floor to far above the ceiling, both slider
//     signs, the unscaled byte): the port equals it bit for bit, so every branch of the port is held by PES's own code;
//  2. the live players of the full captures (tools/dev/translate/physique_scan.py): PES's stored G (P+0xA00 and the chain's
//     G) equals the port from the player's physique block, on every player;
//  3. relations that need no capture.
// PES21_ROOT (the install: PES2021.exe for the constants) and PES_EXE (the flat image the translated code reads) are
// needed; GF_PES_PHYSIQUE_SCAN names the scan TSV (PES-derived, never committed). Without them the tests skip.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pes/exe.hpp"
#include "pes/physique.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

namespace {

uint32_t Bits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, sizeof b);
  return b;
}
float F32(uint32_t b) {
  float f;
  std::memcpy(&f, &b, sizeof f);
  return f;
}

struct Flat {
  uint8_t* bytes = nullptr;
  uint64_t size = 0;
  ~Flat() {
    if (bytes != nullptr) munmap(bytes, size);
  }
  bool Open(const char* path) {
    const int fd = open(path, O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) return false;
    size = static_cast<uint64_t>(st.st_size);
    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return false;
    bytes = static_cast<uint8_t*>(p);
    return true;
  }
};

class Physique : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (root == nullptr || *root == '\0') GTEST_SKIP() << "PES21_ROOT unset";
    std::string err;
    const auto pe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(pe.has_value()) << err;
    const auto k = pes::LoadPhysiqueConstants(*pe, &err);
    ASSERT_TRUE(k.has_value()) << err;
    k_ = *k;
  }
  pes::PhysiqueConstants k_;
};

// The translated FUN_14195CA70 on a block at a window VA: xmm0 low lane.
float TranslatedScale(const Flat& image, const pes::PhysiqueBlock& b) {
  constexpr uint64_t kBlockVa = 0x2200000000ULL, kStackVa = 0x2200010000ULL;
  PesState s;
  s.mem.flat = image.bytes;
  s.mem.flat_base = 0x140000000ULL;
  s.mem.flat_size = image.size;
  PesMem::Seg block{kBlockVa, std::vector<uint8_t>(0x44)};
  std::memcpy(block.bytes.data(), &b.heightCm, 4);
  std::memcpy(block.bytes.data() + 0x30, &b.legSlider, 4);
  block.bytes[0x40] = b.unscaled ? 1 : 0;
  s.mem.segs.push_back(std::move(block));
  s.mem.segs.push_back(PesMem::Seg{kStackVa - 0x1000, std::vector<uint8_t>(0x1000)});
  s.gpr[1] = kBlockVa;
  s.gpr[4] = kStackVa;
  ::pes_fn_14195CA70(s);
  return s.x_f32(0);
}

// Every height segment (below the floor, both inner segments, the anchor, above the ceiling), both slider signs and zero, and the
// unscaled byte: the port is PES's function bit for bit.
TEST_F(Physique, BlockScaleEqualsTheTranslatedFunctionOnEveryBranch) {
  const char* exe = std::getenv("PES_EXE");
  if (exe == nullptr || *exe == '\0') GTEST_SKIP() << "PES_EXE unset";
  Flat image;
  ASSERT_TRUE(image.Open(exe));
  int checked = 0;
  for (float h = 60.0f; h <= 300.0f; h += 0.5f) {
    for (const float slider : {-1.0f, -0.4f, 0.0f, 0.25f, 1.0f}) {
      for (const bool unscaled : {false, true}) {
        const pes::PhysiqueBlock b{h, slider, unscaled};
        EXPECT_EQ(Bits(pes::BlockScale(k_, b)), Bits(TranslatedScale(image, b))) << h << " " << slider << " " << unscaled;
        ++checked;
      }
    }
  }
  EXPECT_EQ(checked, 481 * 5 * 2);
}

// PES's stored G on every player of the scan: P+0xA00 and the chain's G equal the port from the block, and the block's height is
// the player's PM+0x30 height.
TEST_F(Physique, EveryScannedPlayersStoredGEqualsThePort) {
  const char* scan = std::getenv("GF_PES_PHYSIQUE_SCAN");
  if (scan == nullptr || *scan == '\0') GTEST_SKIP() << "GF_PES_PHYSIQUE_SCAN unset";
  std::ifstream in(scan);
  ASSERT_TRUE(in.good()) << scan;
  int players = 0, clamped = 0, agree = 0;
  for (std::string line; std::getline(in, line);) {
    std::vector<std::string> col;
    std::stringstream ss(line);
    for (std::string c; std::getline(ss, c, '\t');) col.push_back(c);
    ASSERT_EQ(col.size(), 6u) << line;
    const std::string& hex = col[3];
    ASSERT_EQ(hex.size(), 2u * 0x44) << line;
    uint8_t block[0x44];
    for (size_t i = 0; i < sizeof block; ++i) block[i] = static_cast<uint8_t>(std::stoul(hex.substr(2 * i, 2), nullptr, 16));
    pes::PhysiqueBlock b;
    std::memcpy(&b.heightCm, block, 4);
    std::memcpy(&b.legSlider, block + 0x30, 4);
    b.unscaled = block[0x40] != 0;
    const uint32_t stored = static_cast<uint32_t>(std::stoul(col[4], nullptr, 16));
    const uint32_t chain = static_cast<uint32_t>(std::stoul(col[5], nullptr, 16));
    EXPECT_FALSE(std::isnan(b.heightCm)) << line;
    EXPECT_EQ(Bits(pes::PhysiqueG(k_, b)), stored) << col[0] << " " << col[1] << " h=" << b.heightCm;
    EXPECT_EQ(stored, chain) << col[0] << " " << col[1];
    clamped += F32(stored) == k_.scaleHi || F32(stored) == k_.scaleLo;
    // The block holds the default 180 until PES fills it (a capture at the match start: every block 180, every G 1.0 whatever the
    // player's PM+0x30 height), so G follows the block; once filled the block's height is the player's.
    agree += Bits(b.heightCm) == static_cast<uint32_t>(std::stoul(col[2], nullptr, 16));
    ++players;
  }
  EXPECT_GT(players, 0) << "the scan holds no player";
  EXPECT_GT(agree, 0) << "no scanned player has a filled block";
  EXPECT_GT(clamped, 0) << "no scanned player reaches the clamp: its ends are held by the relation test only";
}

// Relations that need no capture: the anchor height is neutral, a taller body scales up and a shorter one down, the slider
// scales the same way, an unscaled block is exactly one, and the result stays inside the clamp.
TEST_F(Physique, ScaleRelations) {
  const auto g = [&](float h, float l, bool u = false) { return pes::PhysiqueG(k_, {h, l, u}); };
  EXPECT_EQ(g(k_.heightAnchorCm, 0.0f), k_.one);
  EXPECT_GT(g(k_.heightAnchorCm + 5.0f, 0.0f), g(k_.heightAnchorCm, 0.0f));
  EXPECT_LT(g(k_.heightAnchorCm - 5.0f, 0.0f), g(k_.heightAnchorCm, 0.0f));
  EXPECT_GT(g(k_.heightAnchorCm, 0.5f), g(k_.heightAnchorCm, 0.0f));
  EXPECT_LT(g(k_.heightAnchorCm, -0.5f), g(k_.heightAnchorCm, 0.0f));
  EXPECT_EQ(g(150.0f, 1.0f, true), k_.one);
  EXPECT_EQ(g(1000.0f, 1.0f), k_.scaleHi);
  EXPECT_EQ(g(10.0f, -1.0f), k_.scaleLo);
}

}  // namespace
