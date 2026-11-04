// PES's kick chooser FUN_1408364D0 (translated, capture-bounded closure) run on GF's own window from the recorded
// kick_chooser_8364D0 calls. Each capture's inputs are rebuilt in a pes::PesWorld at PES's offsets (pes_kick_oracle_common.hpp): the
// kicker P and the ball, match-info, man and slot records through the window's records (the call's live-in set, manifest.tsv), the
// request through PesWorld::RunKickChooser's frame. What GF holds no state for yet (the install's image, the loader data, the
// capture's heap ranges) comes from the capture as bytes at their VAs (PesTrapExternals). The descriptor must equal PES's on every
// byte the reference call wrote.
//
// GF_PES_KICK_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/trap_oracle.py from a snapdiff case of 0x1408364D0 (see docs/ports/pes-port-KickKind2.md). PES_EXE names the
// unpacked image. Without either, skip. The request builder's side is pes_kick_header_oracle_test.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "onthepitch/peskickchooser.hpp"
#include "pes_kick_oracle_common.hpp"

namespace {

using namespace pes_kick_oracle;

// PES's descriptor: the bytes the reference call wrote into [rcx, rcx + kKickDescriptorBytes), from its write log.
std::map<size_t, uint8_t> ReferenceWrites(const Oracle& o) {
  std::map<size_t, uint8_t> bytes;
  const uint64_t rcx = o.regs.at("rcx");
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr < rcx || addr >= rcx + pes::kKickDescriptorBytes) continue;
    bytes[addr - rcx] = static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16));
  }
  return bytes;
}

class KickChooserOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_KICK_ORACLE");
    const char* exe = std::getenv("PES_EXE");
    if (dir == nullptr || *dir == '\0' || exe == nullptr || *exe == '\0')
      GTEST_SKIP() << "GF_PES_KICK_ORACLE / PES_EXE unset (PES-derived captures and image, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
    exe_ = exe;
  }
  std::vector<std::string> dirs_;
  std::string exe_;
};

TEST_F(KickChooserOracle, EveryCapturedCallOnGfsWindowGivesPesDescriptor) {
  size_t real = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    Built b;
    Build(o, image.bytes, image.size, b);
    const pes::PesKickDescriptor got = b.world->RunKickChooser(kSlot, b.request, b.ext);
    const std::map<size_t, uint8_t> want = ReferenceWrites(o);
    ASSERT_FALSE(want.empty()) << dir << ": the reference wrote no descriptor byte";
    for (const auto& [off, value] : want) EXPECT_EQ(got.bytes[off], value) << dir << " descriptor+0x" << std::hex << off;
    real += got.Kind() != pes::kKickNoKind;
  }
  RecordProperty("real_kinds", static_cast<int>(real));
}

}  // namespace
