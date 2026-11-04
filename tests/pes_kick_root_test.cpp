// The kick geometry test's root-motion reader FUN_141AFDF10 (translated, set kickroot) on the anime chain GF builds from the install
// (pesanimechain.hpp): the body archives as PES's loaded packs, the motion -> clip key table, one layer whose pose slot the reader
// evaluates the clip into. For every recorded call of the kick chooser's runs the vector must equal the one PES's own bytes returned
// on the capture's chain (rootmotion.tsv beside each oracle directory: motion, the time's float bits, the 16 output bytes).
//
// GF_PES_KICK_ORACLE names a directory of per-capture oracle directories (PES-derived, never committed), PES_EXE the unpacked image,
// PES21_ROOT the install. Without any of them, skip.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "pes_kick_oracle_common.hpp"

namespace {

using namespace pes_kick_oracle;

struct ReaderCall {
  uint32_t motion = 0;
  uint32_t secondsBits = 0;
  std::array<uint8_t, 16> out{};
};

std::vector<ReaderCall> LoadCalls(const std::string& path) {
  std::vector<ReaderCall> calls;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream row(line);
    std::string motion, bits, out;
    std::getline(row, motion, '\t');
    std::getline(row, bits, '\t');
    std::getline(row, out, '\t');
    ReaderCall c;
    c.motion = static_cast<uint32_t>(std::stoul(motion));
    c.secondsBits = static_cast<uint32_t>(std::stoul(bits, nullptr, 16));
    for (size_t i = 0; i < c.out.size(); ++i) c.out[i] = static_cast<uint8_t>(std::stoul(out.substr(2 * i, 2), nullptr, 16));
    calls.push_back(c);
  }
  return calls;
}

class KickRoot : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_KICK_ORACLE");
    const char* exe = std::getenv("PES_EXE");
    if (dir == nullptr || *dir == '\0' || exe == nullptr || *exe == '\0')
      GTEST_SKIP() << "GF_PES_KICK_ORACLE / PES_EXE unset (PES-derived captures and image, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv") && std::filesystem::exists(e.path() / "rootmotion.tsv"))
        dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory with rootmotion.tsv under " << dir;
    exe_ = exe;
    std::string why;
    chain_ = GfAnimeChain(&why);
    if (chain_ == nullptr) GTEST_SKIP() << "the anime chain is built from the install: " << why;
  }
  std::vector<std::string> dirs_;
  std::string exe_;
  const pes::PesAnimeChain* chain_ = nullptr;
};

// The recorded calls of each capture (rootmotion.tsv: the reader's real calls inside the chooser's run) and, beside the first
// capture, PES's own bytes asked other motions and times on the same memory (rootmotion_synthetic.tsv, SYNTHETIC: 72 motions at
// four times, the clips that turn the mover included).
TEST_F(KickRoot, TheChainGfBuildsGivesPesRootMotionOnEveryCapturedKick) {
  size_t real = 0, synthetic = 0, turning = 0;
  const char* only = std::getenv("GF_PES_KICK_ROOT_MOTION");  // one motion only (a bisect of the closure's reach)
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    Built b;
    Build(o, image.bytes, image.size, b, chain_);
    for (const char* file : {"rootmotion.tsv", "rootmotion_synthetic.tsv"}) {
      for (const ReaderCall& c : LoadCalls(dir + "/" + file)) {
        if (only != nullptr && c.motion != static_cast<uint32_t>(std::atoi(only))) continue;
        float seconds;
        std::memcpy(&seconds, &c.secondsBits, sizeof seconds);
        const std::array<float, 4> got = b.world->RunRootMotion(kSlot, c.motion, seconds, b.ext);
        EXPECT_EQ(std::memcmp(got.data(), c.out.data(), c.out.size()), 0)
            << dir << "/" << file << " motion " << c.motion << " at " << seconds << " s: got " << got[0] << " " << got[1] << " "
            << got[2] << " " << got[3];
        (std::string(file) == "rootmotion.tsv" ? real : synthetic)++;
        turning += got[1] != 0.0f || got[3] != 1.0f;
      }
    }
  }
  EXPECT_GT(real, 0u);
  // The synthetic rows hold the motions whose clip turns the mover: PES returns a rotation there, not the identity (0, 0, 0, 1),
  // and the vector comes from the evaluator's callee 0x1423BAEB0 (in the translated closure since docs/ports/pes-port-KickBranch.md).
  // A file with none would leave that branch unchecked.
  EXPECT_GT(turning, 0u) << "no row of rootmotion_synthetic.tsv turns the mover";
  RecordProperty("non_identity_vectors", static_cast<int>(turning));
  RecordProperty("real_calls", static_cast<int>(real));
  RecordProperty("synthetic_calls", static_cast<int>(synthetic));
}

}  // namespace
