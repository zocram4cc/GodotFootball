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
#include "onthepitch/peskickmotion.hpp"
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
    std::string why;
    chain_ = GfAnimeChain(&why);
    if (chain_ == nullptr) GTEST_SKIP() << "the anime chain is built from the install: " << why;
  }
  std::vector<std::string> dirs_;
  std::string exe_;
  const pes::PesAnimeChain* chain_ = nullptr;
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
    Build(o, image.bytes, image.size, b, chain_);
    const pes::PesKickDescriptor got = b.world->RunKickChooser(kSlot, b.request, b.ext);
    const std::map<size_t, uint8_t> want = ReferenceWrites(o);
    ASSERT_FALSE(want.empty()) << dir << ": the reference wrote no descriptor byte";
    for (const auto& [off, value] : want) EXPECT_EQ(got.bytes[off], value) << dir << " descriptor+0x" << std::hex << off;
    real += got.Kind() != pes::kKickNoKind;
  }
  RecordProperty("real_kinds", static_cast<int>(real));
}

// The request GF builds (KickRequestFor: the action, the subtype and the target) puts those fields where PES's builder does, and the
// chooser runs to an answer on it: a descriptor kind in the clip table's range or none, never a loud abort of a path the closure lacks.
TEST_F(KickChooserOracle, AGfBuiltRequestPutsTheFieldsWherePesDoesAndTheChooserAnswers) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    Built b;
    Build(o, image.bytes, image.size, b, chain_);
    // The builder FUN_140833CD0 writes its request as words: param_1[0] the action, [1] the subtype, [0x19..0x1b] the target. The
    // words are read here by those indices, not by the header's offsets, so a wrong offset there shows.
    const auto word = [](const pes::PesKickRequest& r, size_t i) {
      uint32_t v;
      std::memcpy(&v, r.bytes.data() + 4 * i, sizeof v);
      return v;
    };
    constexpr size_t kActionWord = 0, kSubtypeWord = 1, kTargetWord = 0x19;
    int32_t action;
    float target[3];
    std::memcpy(&action, &b.request.bytes[4 * kActionWord], sizeof action);
    std::memcpy(target, &b.request.bytes[4 * kTargetWord], sizeof target);
    const pes::PesKickRequest gf = pes::KickRequestFor(action, target);
    for (const size_t i : {kActionWord, kTargetWord, kTargetWord + 1, kTargetWord + 2})
      EXPECT_EQ(word(gf, i), word(b.request, i)) << dir << " request word " << i;
    // A ground pass (9) with no order flag set has subtype 1 in PES's own request (FUN_1407204E0's default branch); the lofted
    // captures carry an order flag GF has no state for.
    if (action == 9) EXPECT_EQ(word(gf, kSubtypeWord), word(b.request, kSubtypeWord)) << dir;
    const pes::PesKickDescriptor got = b.world->RunKickChooser(kSlot, gf, b.ext);
    EXPECT_TRUE(got.Kind() == pes::kKickNoKind || (got.Kind() >= 1 && got.Kind() < pes::kClipKinds)) << dir << " kind " << got.Kind();
  }
}

}  // namespace
