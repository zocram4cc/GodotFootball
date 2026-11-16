// PES's per-player anime engine graph built in GF from the install (ticket AnimeLayer5): the translated fox load chain fills the
// ResManager from the user's own files (the FoxAni preload FUN_141EC9AF0 and the job pump FUN_141EA8430), the project registrar
// FUN_141ECA500 builds the "body" BodyAnimControlResource, and the proven player constructor FUN_141EB5250 makes each player's
// BodyAnimControl. The driver FUN_141AF4650 then runs the phases that blend the pose.
//
// PES_EXE names the unpacked image and PES21_ROOT the install. Without either, skip. Nothing here pins a value from a pack: the
// assertions are relations (a registered name resolves, a constructed object points at its parts, a pose is finite and moves with the
// motion's time).

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "onthepitch/pesfoxworld.hpp"
#include "pes/translated/translated.hpp"
#include "pes/clips.hpp"
#include "pes/exe.hpp"
#include "pes/gani.hpp"
#include "pes/vfs.hpp"
#include "rig/clip_pose.hpp"
#include "pes_state.hpp"
#include "rig/rig.hpp"

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

struct Q {
  double x = 0, y = 0, z = 0, w = 1;
};
Q Mul(const Q& a, const Q& b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Q Inv(const Q& a) { return {-a.x, -a.y, -a.z, a.w}; }
double AngleBetween(const Q& a, const Q& b) {
  const Q d = Mul(Inv(a), b);
  return 2.0 * std::acos(std::min(1.0, std::abs(d.w)));
}

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

  // The fixed frame B_j per joint that turns GF's clip pose into the engine's rotations (see the test that measures it): the engine plays
  // `motion` frozen (playback 0) at clip time 0, GF's sampler gives the same clip's chain of locals (the body joint's own rotation is taken off
  // both sides: the engine does not apply every clip's mover rotation to it), and B_j = chain_j^-1 * (R_body^-1 * R_engine,j).
  static bool BindOf(int motion, std::vector<Q>* bind, std::string* error) {
    const char* root = std::getenv("PES21_ROOT");
    const auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", error);
    if (!exe) return false;
    const auto names = pes::MotionNames(*exe, *vfs_, error);
    if (!names) return false;
    const auto rig = pesrig::BuildRig(*vfs_, error);
    if (!rig) return false;
    const auto blob = pes::FindBodyClip(*vfs_, pes::ClipId((*names)[static_cast<std::size_t>(motion)]), error);
    if (!blob) return false;
    const auto clip = pes::ParseGani(*blob, error);
    if (!clip) return false;
    const pesrig::ClipSampler sampler(*clip, *rig);
    const pesrig::ClipPose pose = sampler.Sample(0.0);
    std::vector<Q> chain(kJointCount);
    for (uint32_t j = 0; j < kJointCount; ++j) {
      const Q local = j == 0 ? Q{} : Q{pose.local[j].x, pose.local[j].y, pose.local[j].z, pose.local[j].w};
      chain[j] = rig->bones[j].parent < 0 ? local : Mul(chain[static_cast<std::size_t>(rig->bones[j].parent)], local);
    }
    const uint64_t p = NewPlayer();
    world_->ConstructAnime(p, 0);
    pes::MotionRequest request;
    request.motion = motion;
    request.playback = 0.0f;
    world_->IssueMotion(p, request);
    constexpr int kSettleFrames = 30;
    for (int f = 0; f < kSettleFrames; ++f) world_->RunFrame(p);
    const pes::PesJointPose engine = world_->ReadPose(p);
    const auto q = [&](uint32_t j) { return Q{engine.rotation[j][0], engine.rotation[j][1], engine.rotation[j][2], engine.rotation[j][3]}; };
    bind->assign(kJointCount, Q{});
    for (uint32_t j = 0; j < kJointCount; ++j) (*bind)[j] = Mul(Inv(chain[j]), Mul(Inv(q(0)), q(j)));
    return true;
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
constexpr int kBindMotion = 0x100;  // the motion the bind frames are measured on (the test above checks three more agree)

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

TEST_F(AnimeEngine, AnIssuedClipAdvancesThePoseFromFrameToFrameAndSetSpeedChangesHowFast) {
  // The first motion that leaves the idle pose: issued, then framed more times, the pose keeps moving (the clip plays); at half the playback
  // rate (FUN_141AF6E40) the same number of frames reaches a different pose than at the full rate.
  const pes::PesJointPose idle = PoseAfter(-1, 1.0f, kIssueFrames);
  int motion = -1;
  for (int m = 0x100; m <= 0x1100 && motion < 0; m += 0x100) {
    const pes::PesJointPose p = PoseAfter(m, 1.0f, kIssueFrames);
    if (std::memcmp(&idle, &p, sizeof idle) != 0) motion = m;
  }
  ASSERT_GE(motion, 0) << "no issued motion left the idle pose";
  const pes::PesJointPose early = PoseAfter(motion, 1.0f, kIssueFrames), later = PoseAfter(motion, 1.0f, 3 * kIssueFrames),
                          slow = PoseAfter(motion, 0.5f, kIssueFrames);
  EXPECT_NE(std::memcmp(&early, &later, sizeof early), 0) << "motion " << motion << ": more frames left the same pose (the clip did not advance)";
  EXPECT_NE(std::memcmp(&early, &slow, sizeof early), 0) << "motion " << motion << ": half the playback rate left the same pose";
}

TEST_F(AnimeEngine, EveryThirdMotionNumberTheMotionSetterAcceptsIsServedWithAFinitePose) {
  // One man, re-issued: SetMotion's accepted range 0..0x1124 in steps of kSweepStep, two frames each; every pose is finite with unit rotations
  // (a motion class the layer does not translate aborts in the dispatch: the survey names it).
  constexpr int kSweepStep = 3;
  const uint64_t p = NewPlayer();
  world_->ConstructAnime(p, 0);
  for (int motion = 0; motion <= 0x1124; motion += kSweepStep) {
    pes::MotionRequest request;
    request.motion = motion;
    world_->IssueMotion(p, request);
    world_->RunFrame(p);
    world_->RunFrame(p);
    const pes::PesJointPose pose = world_->ReadPose(p);
    for (uint32_t j = 0; j < kJointCount; ++j) {
      float n = 0.0f;
      for (int k = 0; k < 4; ++k) {
        ASSERT_TRUE(std::isfinite(pose.position[j][k])) << "motion " << motion << " joint " << j;
        ASSERT_TRUE(std::isfinite(pose.rotation[j][k])) << "motion " << motion << " joint " << j;
        n += pose.rotation[j][k] * pose.rotation[j][k];
      }
      ASSERT_NEAR(n, 1.0f, 1e-3f) << "motion " << motion << " joint " << j;
    }
  }
}

// The engine's joint rotations are not GF's clip rotations: PES keeps each bone in a frame of its own (the child offsets lie along the
// bone's +x; GF's skeleton is world-aligned in the T-pose, src/rig/rig.cpp). If the engine's pose is the clip's pose times a FIXED per-joint
// frame B_j, then B_j = G_clip,j^-1 * R_engine,j is the same for every motion. Measured: the engine plays motion M frozen (playback 0) at
// clip time 0 for 30 frames, GF's sampler (the same clip file, the motion table's name for M) gives G_clip,j (the chain of its locals).
TEST_F(AnimeEngine, TheEnginesJointRotationsAreTheClipsRotationsTimesAFixedFramePerJoint) {
  std::string error;
  const auto rig = pesrig::BuildRig(*vfs_, &error);
  ASSERT_TRUE(rig.has_value()) << error;
  std::vector<std::vector<Q>> bind;  // per motion, per joint
  for (const int motion : {0x100, 0x400, 0x700, 0xA00}) {
    bind.emplace_back();
    ASSERT_TRUE(BindOf(motion, &bind.back(), &error)) << motion << ": " << error;
  }
  double worst = 0.0;
  for (uint32_t j = 0; j < kJointCount; ++j) {
    double spread = 0.0;
    for (std::size_t m = 1; m < bind.size(); ++m) spread = std::max(spread, AngleBetween(bind[0][j], bind[m][j]));
    std::printf("joint %2u %-15s bind (%.4f %.4f %.4f %.4f) spread %.4f rad\n", j, rig->bones[j].gfName.c_str(), bind[0][j].x, bind[0][j].y,
                bind[0][j].z, bind[0][j].w, spread);
    worst = std::max(worst, spread);
  }
  EXPECT_LT(worst, 1e-2) << "the engine's rotations are not the clip's times one fixed frame per joint";
}

TEST_F(AnimeEngine, ThePosesTheRenderCheckPlotsAreWrittenWhenAskedFor) {
  // The AGENTS.md render check: GF_ENGINE_POSE_DUMP=<file> writes the rig's parent joints and the 20 joint positions of the idle pose and of
  // every motion probed, for tools/dev/render/plot_engine_pose.py. Nothing is written (and nothing asserted) without the variable.
  const char* path = std::getenv("GF_ENGINE_POSE_DUMP");
  if (!path || !*path) GTEST_SKIP() << "GF_ENGINE_POSE_DUMP unset";
  std::string error;
  const auto rig = pesrig::BuildRig(*vfs_, &error);
  ASSERT_TRUE(rig.has_value()) << error;
  FILE* out = std::fopen(path, "w");
  ASSERT_NE(out, nullptr);
  std::fprintf(out, "parents");
  for (uint32_t j = 0; j < kJointCount; ++j) std::fprintf(out, " %d", rig->bones[j].parent);
  std::fprintf(out, "\nnames");
  for (uint32_t j = 0; j < kJointCount; ++j) std::fprintf(out, " %s", rig->bones[j].gfName.c_str());
  std::fprintf(out, "\n");
  // One row per pose: per joint the model-space position (x y z) and rotation (x y z w), PES axes (y up, z forward).
  const auto write = [&](const char* label, const pes::PesJointPose& pose) {
    std::fprintf(out, "pose %s", label);
    for (uint32_t j = 0; j < kJointCount; ++j)
      std::fprintf(out, " %.6f %.6f %.6f %.6f %.6f %.6f %.6f", pose.position[j][0], pose.position[j][1], pose.position[j][2],
                   pose.rotation[j][0], pose.rotation[j][1], pose.rotation[j][2], pose.rotation[j][3]);
    std::fprintf(out, "\n");
  };
  // The fixed frame per joint (rotation x y z w each) between GF's clip rotations and the engine's: the viewer takes it off.
  std::vector<Q> bind;
  ASSERT_TRUE(BindOf(kBindMotion, &bind, &error)) << error;
  std::fprintf(out, "bind");
  for (const Q& q : bind) std::fprintf(out, " %.7f %.7f %.7f %.7f", q.x, q.y, q.z, q.w);
  std::fprintf(out, "\n");
  write("constructed", PoseAfter(-1, 1.0f, 0));  // before any frame: the control's own initial arrays
  write("idle", PoseAfter(-1, 1.0f, kIssueFrames));
  for (int motion = 0x100; motion <= 0x1100; motion += 0x100) write(std::to_string(motion).c_str(), PoseAfter(motion, 1.0f, kIssueFrames));
  std::fclose(out);
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
