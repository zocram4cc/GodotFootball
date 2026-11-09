// PES's per-player anime engine graph built in GF from the install (ticket AnimeLayer5): the translated fox load chain fills the
// ResManager from the user's own files (the FoxAni preload FUN_141EC9AF0 and the job pump FUN_141EA8430), the project registrar
// FUN_141ECA500 builds the "body" BodyAnimControlResource, and the proven player constructor FUN_141EB5250 makes each player's
// BodyAnimControl. The driver FUN_141AF4650 then runs the phases that blend the pose.
//
// PES_EXE names the unpacked image and PES21_ROOT the install. Without either, skip. Nothing here pins a value from a pack: the
// assertions are relations (a registered name resolves, a constructed object points at its parts, a pose is finite and moves with the
// motion's time).

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "onthepitch/pesfoxworld.hpp"
#include "pes/translated/translated.hpp"
#include "pes/vfs.hpp"
#include "pes_state.hpp"

namespace {

constexpr uint64_t kHeapBytes = 1ull << 30;
// The registry singleton cell (FUN_141EC8C40) and its map lookup FUN_141EC8BD0(registry, std::string*) -> the resource.
constexpr uint64_t kRegistryVa = 0x1437F4F78;
// BodyAnimControlResource's vtable (FUN_141EB5250 decompile: R = registry["body"]; AnimeLayer2 section 3) and BodyAnimControl's.
constexpr uint64_t kResourceVtable = 0x142ABA190, kBodyAnimControlVtable = 0x142ABA1A8;
// The player block: FUN_1406E03D0 constructs a P of at least 0x3BC0 bytes (its last store is at param_1 + 0x772 * 8 + 0x28).
constexpr uint64_t kPlayerBytes = 0x3C00;
constexpr uint64_t kAnimeOffset = 0xA08, kAnimeReadyOffset = 0xA04, kAnimeNameOffset = 0xA10;
// A (BodyAnimControl): the AnimControl L1 at +0x88, the joint rotation array pointer at +0x20; L1: the joint position array pointer at +0x50.
constexpr uint64_t kControlOffset = 0x88, kQuatPointerOffset = 0x20, kPosePointerOffset = 0x50;
constexpr uint32_t kJointCount = 20;

class AnimeEngine : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const char* exe = std::getenv("PES_EXE");
    const char* root = std::getenv("PES21_ROOT");
    if (!exe || !*exe || !root || !*root) return;
    std::string error;
    vfs_ = std::make_unique<pes::PesVfs>();
    ASSERT_TRUE(vfs_->Mount(root, &error)) << error;
    world_ = std::make_unique<pes::PesFoxWorld>(exe, *vfs_, kHeapBytes);
    world_->Boot();
    loaded_ = world_->LoadBody(&loadError_);
  }
  static void TearDownTestSuite() {
    world_.reset();
    vfs_.reset();
  }
  void SetUp() override {
    if (!world_) GTEST_SKIP() << "PES_EXE / PES21_ROOT unset";
    ASSERT_TRUE(loaded_) << loadError_;
  }

  // A std::string (MSVC layout: 16-byte buffer, size at +0x10, capacity at +0x18) of a short name in the world's heap.
  static uint64_t NewString(const std::string& text) {
    const uint64_t va = world_->Alloc(0x20);
    EXPECT_LE(text.size(), 15u);
    for (std::size_t i = 0; i < text.size(); ++i) world_->Write8(va + i, static_cast<uint8_t>(text[i]));
    world_->Write64(va + 0x10, text.size());
    world_->Write64(va + 0x18, 15);
    return va;
  }

  static uint64_t NewPlayer() {
    const uint64_t p = world_->Alloc(kPlayerBytes);
    EXPECT_NE(p, 0u);
    return p;
  }

  static std::vector<float> Floats(uint64_t va, std::size_t n) {
    std::vector<float> out(n);
    for (std::size_t i = 0; i < n; ++i) {
      const uint32_t bits = world_->Read32(va + 4 * i);
      std::memcpy(&out[i], &bits, sizeof bits);
    }
    return out;
  }

  // A player constructed, issued `motion` (when >= 0) and framed `frames` times: his pose.
  static pes::PesJointPose PoseAfter(int motion, float playback, int frames) {
    const uint64_t p = NewPlayer();
    world_->ConstructAnime(p, 0);
    if (motion >= 0) {
      pes::MotionRequest request;
      request.motion = motion;
      request.playback = playback;
      world_->IssueMotion(p, request);
    }
    for (int f = 0; f < frames; ++f) world_->RunFrame(p);
    return world_->ReadPose(p);
  }

  static std::unique_ptr<pes::PesVfs> vfs_;
  static std::unique_ptr<pes::PesFoxWorld> world_;
  static bool loaded_;
  static std::string loadError_;
};

std::unique_ptr<pes::PesVfs> AnimeEngine::vfs_;
std::unique_ptr<pes::PesFoxWorld> AnimeEngine::world_;
bool AnimeEngine::loaded_ = false;
std::string AnimeEngine::loadError_;

TEST_F(AnimeEngine, TheBodyProjectResolvesInTheRegistryTheLoadBuilt) {
  const uint64_t registry = world_->Read64(kRegistryVa);
  ASSERT_NE(registry, 0u) << "the registrar made no registry";
  const uint64_t resource = world_->Call(::pes_fn_141EC8BD0, {registry, NewString("body")});
  ASSERT_NE(resource, 0u) << "\"body\" is not registered";
  EXPECT_EQ(world_->Read64(resource), kResourceVtable);
  EXPECT_EQ(world_->Call(::pes_fn_141EC8BD0, {registry, NewString("no_such_proj")}), 0u) << "an unregistered name resolves";
}

TEST_F(AnimeEngine, TheBodyArchivesAreInTheResManagerAtTheKeyTheirNamesGive) {
  // Every archive the install holds is in the ResManager; a name the install lacks is not (FUN_141B021A0 answers the key at +0, a valid
  // byte at +8; PES's own "absent" key is 0xB8A0BF169F98).
  constexpr uint64_t kNoKey = 0xB8A0BF169F98;
  const uint64_t resmanager = world_->Call(::pes_fn_141B034B0, {});
  ASSERT_NE(resmanager, 0u);
  int present = 0;
  for (int i = 0; i < 10; ++i) {
    const std::string name = "common/anime/FoxAnim/Body/body_anime_file" + std::to_string(i) + ".mtar";
    std::string error;
    const bool inInstall = vfs_->Size(name).has_value();
    const uint64_t out = world_->Alloc(0x10);
    const std::string cri = "cpk_dat/" + name;
    const uint64_t path = world_->NewCString(cri);
    world_->Call(::pes_fn_141B021A0, {resmanager, out, path});
    const bool resolved = world_->Read8(out + 8) != 0 && world_->Read64(out) != kNoKey;
    EXPECT_EQ(resolved, inInstall) << name;
    present += resolved ? 1 : 0;
  }
  EXPECT_GT(present, 0) << "the install holds no body archive";
}

TEST_F(AnimeEngine, TheConstructorBuildsAPlayersControlAndItsParts) {
  const uint64_t p = NewPlayer();
  world_->ConstructAnime(p, 0);
  const uint64_t a = world_->Read64(p + kAnimeOffset);
  ASSERT_NE(a, 0u);
  EXPECT_EQ(world_->Read64(a), kBodyAnimControlVtable);
  EXPECT_EQ(world_->Read8(p + kAnimeReadyOffset), 1);
  const uint64_t l1 = world_->Read64(a + kControlOffset);
  ASSERT_NE(l1, 0u);
  EXPECT_NE(world_->Read64(l1 + kPosePointerOffset), 0u);
  EXPECT_NE(world_->Read64(a + kQuatPointerOffset), 0u);
  EXPECT_EQ(world_->ReadCString(p + kAnimeNameOffset), "body");
  // Two players do not share a control.
  const uint64_t q = NewPlayer();
  world_->ConstructAnime(q, 0);
  EXPECT_NE(world_->Read64(q + kAnimeOffset), a);
}

TEST_F(AnimeEngine, AFrameLeavesAFinitePoseOfTwentyJointsWithUnitRotations) {
  const uint64_t p = NewPlayer();
  world_->ConstructAnime(p, 0);
  world_->RunFrame(p);
  const pes::PesJointPose pose = world_->ReadPose(p);
  bool any = false;
  for (uint32_t j = 0; j < kJointCount; ++j) {
    float n = 0.0f;
    for (int k = 0; k < 4; ++k) {
      ASSERT_TRUE(std::isfinite(pose.position[j][k])) << "joint " << j;
      ASSERT_TRUE(std::isfinite(pose.rotation[j][k])) << "joint " << j;
      any = any || pose.position[j][k] != 0.0f;
      n += pose.rotation[j][k] * pose.rotation[j][k];
    }
    EXPECT_NEAR(n, 1.0f, 1e-3f) << "joint " << j;
  }
  EXPECT_TRUE(any) << "the idle motion posed nothing";
}

TEST_F(AnimeEngine, TwoPlayersBuiltAndFramedTheSameWayHoldTheSamePose) {
  // The frame's scratch arena and the shared resources carry no state from one player's frame into the next player's.
  const uint64_t p = NewPlayer(), q = NewPlayer();
  world_->ConstructAnime(p, 0);
  world_->ConstructAnime(q, 0);
  world_->RunFrame(p);
  world_->RunFrame(q);
  const pes::PesJointPose a = world_->ReadPose(p), b = world_->ReadPose(q);
  EXPECT_EQ(std::memcmp(&a, &b, sizeof a), 0);
}
constexpr int kIssueFrames = 6;

TEST_F(AnimeEngine, AMotionSetMotionDropsLeavesThePoseOfAControlNeverIssued) {
  // FUN_1406F0F70 returns at once for a motion above 0x1124: nothing reaches the control.
  const pes::PesJointPose idle = PoseAfter(-1, 1.0f, kIssueFrames), dropped = PoseAfter(0x1125, 1.0f, kIssueFrames);
  EXPECT_EQ(std::memcmp(&idle, &dropped, sizeof idle), 0);
}

TEST_F(AnimeEngine, AnIssuedMotionMovesThePoseOffTheIdleOneAndTheSameIssueMovesItTheSameWay) {
  const pes::PesJointPose idle = PoseAfter(-1, 1.0f, kIssueFrames);
  bool moved = false;
  for (int motion = 0x100; motion <= 0x1100 && !moved; motion += 0x100) {
    const pes::PesJointPose first = PoseAfter(motion, 1.0f, kIssueFrames), again = PoseAfter(motion, 1.0f, kIssueFrames);
    EXPECT_EQ(std::memcmp(&first, &again, sizeof first), 0) << "motion " << motion << " is not deterministic";
    moved = std::memcmp(&idle, &first, sizeof idle) != 0;
  }
  EXPECT_TRUE(moved) << "no issued motion left the idle pose";
}

TEST_F(AnimeEngine, TheHeapHoldsTheLoadAndTwentyTwoPlayers) {
  std::vector<uint64_t> players;
  for (int i = 0; i < 22; ++i) {
    players.push_back(NewPlayer());
    world_->ConstructAnime(players.back(), 0);
    world_->RunFrame(players.back());
  }
  std::printf("anime world live heap bytes: %llu\n", static_cast<unsigned long long>(world_->HeapLiveBytes()));
  EXPECT_LT(world_->HeapLiveBytes(), kHeapBytes);
}

}  // namespace
