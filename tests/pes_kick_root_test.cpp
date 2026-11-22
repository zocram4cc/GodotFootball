// The kick geometry test's root-motion reader FUN_141AFDF10 (translated, set kickroot) run on the engine player's own anime chain (the cut,
// PesWorld::BindAnimeChain). The reader's output is a function of the chain B and the call's own inputs (the motion, the time), so each
// oracle row holds only those inputs and PES's answer: rootmotion.tsv (the reader's real calls in the kick chooser's runs) and
// rootmotion_synthetic.tsv (PES's own bytes asked other motions and times on the same memory). Rows are written by tools/dev/translate/
// rootrd.py. For every row the vector must equal PES's on every output byte.
//
// GF_PES_KICK_ROOT_ORACLE names a directory of per-capture directories (PES-derived, never committed), each holding rootmotion.tsv (and
// optionally rootmotion_synthetic.tsv). PES_EXE names the unpacked image, PES21_ROOT the install. Without any of them, skip.
// Round 6 set (17-11): pid1152106 seq003, seq004, seq005 (kick_root_141AFDF10). Rows use the reader's r8 output convention (rdx = 0, r8 =
// the output vector in the caller's frame, as RunRootMotion calls it): PES's bytes for each real state (motion and time as captured) and,
// for seq003, six synthetic states (motions 100, 3983, 2447, 3621). The real calls' own convention (rdx set, r8 = 0) writes a different
// vector at rdx; it is not compared here. 1/1 PASS on the engine chain.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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
    const char* dir = std::getenv("GF_PES_KICK_ROOT_ORACLE");
    const char* exe = std::getenv("PES_EXE");
    if (dir == nullptr || *dir == '\0' || exe == nullptr || *exe == '\0')
      GTEST_SKIP() << "GF_PES_KICK_ROOT_ORACLE / PES_EXE unset (PES-derived captures and image, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "rootmotion.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory with rootmotion.tsv under " << dir;
    std::string why;
    engine_ = pes_kick_oracle::SharedEngine(&why);
    if (engine_ == nullptr) GTEST_SKIP() << "the engine boots from PES_EXE and PES21_ROOT: " << why;
  }
  std::vector<std::string> dirs_;
  pes_engine::EngineTrap* engine_ = nullptr;
};

// The recorded and the synthetic rows of each capture (rootmotion.tsv: the reader's real calls inside the chooser's run;
// rootmotion_synthetic.tsv: PES's own bytes asked other motions and times on the same memory), on the kicker's engine chain.
TEST_F(KickRoot, TheEngineChainGivesPesRootMotionOnEveryOracleRow) {
  size_t real = 0, synthetic = 0, turning = 0;
  const char* only = std::getenv("GF_PES_KICK_ROOT_MOTION");  // one motion only (a bisect of the closure's reach)
  Built b;
  engine_->BindWorld(*b.world, -1);
  b.world->BindAnimeChain(kSlot, engine_->Player(kKickerPlayer));
  b.ext = engine_->Externals();
  for (const std::string& dir : dirs_) {
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
