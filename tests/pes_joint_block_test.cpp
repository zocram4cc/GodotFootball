// PES's anime joint writer FUN_141EB49B0 (translated, set kick; proven bit-exact on real full-capture states) run on GF's world window
// through PesWorld::RunAnimeJoints, over the engine player's own chain (the cut's shared fixture, pes_engine_trap.hpp). The pose in the anime
// player's layout (model-space joint positions and rotations, the placement matrix of the player's heading and position) comes out as the
// world joint block P+0x538.. and the world rotations P+0x700.. that FUN_141EB51A0 and the Kick gate read. The test poses the engine chain's
// joint arrays, runs the writer, and checks the block against an independent recompute (position = model * heading rotation + player
// position; rotation = RotY(heading) * model rotation), through the joint map and the validity table the install's image holds.
//
// PES_EXE names the unpacked image and PES21_ROOT the install. Without either, skip.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "onthepitch/pesanimechain.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes_engine_trap.hpp"

namespace {

constexpr int kSlot = 3;
constexpr uint64_t kPlayerIndex = 0;
constexpr uint64_t kValidTableVa = 0x14351E540ULL, kJointMapVa = 0x1437F4E60ULL;  // FUN_141EB51A0's validity bytes, FUN_141EBC930's map
constexpr uint64_t kImageBaseVa = 0x140000000ULL;
// BodyAnimControl's layer and placement pointers (pesfoxworld.cpp ReadPose): A+0x88 names the layer controller L1, whose +0x50 is the joint
// position array (pesanimechain.hpp kL1PoseOffset); A+0x20 is the joint rotation array (kAQuatOffset).
constexpr uint64_t kControlOffset = 0x88;
constexpr std::size_t kPositionBlockOffset = 0x538, kRotationBlockOffset = 0x700, kPositionStride = 12, kRotationStride = 16;
constexpr int kBlockJoints = 0x13;  // the writer's loop bound
constexpr float kTolerance = 1e-5f;
constexpr float kHeadingDeg = 94.0f;
constexpr std::array<float, 3> kPlayerAt = {38.0f, 0.0f, 2.5f};

using Quat = std::array<float, 4>;

Quat Mul(const Quat& a, const Quat& b) {  // Hamilton product, (x, y, z, w)
  return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
          a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}

uint32_t Bits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, sizeof b);
  return b;
}

class JointBlock : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const char* exe = std::getenv("PES_EXE");
    const char* root = std::getenv("PES21_ROOT");
    if (!exe || !*exe || !root || !*root) return;
    std::string error;
    et_ = pes_engine::EngineTrap::Open(exe, root, 1, &error);
    if (!et_) {
      loadError_ = error;
      return;
    }
    trap_ = std::make_unique<pes::PesWorld>();
    et_->BindWorld(*trap_, kSlot);
  }
  static void TearDownTestSuite() {
    trap_.reset();
    et_.reset();
  }
  void SetUp() override {
    if (!et_) GTEST_SKIP() << "PES_EXE / PES21_ROOT unset (the writer reads its tables from the image)";
    ASSERT_TRUE(loadError_.empty()) << loadError_;
    // A pose no two joints share: positions spread over the body, rotations about different axes.
    for (std::size_t j = 0; j < pes::kAnimeJointCount; ++j) {
      const float f = static_cast<float>(j);
      pose_.position[j] = {0.03f * f - 0.2f, 0.07f * f, 0.02f * f - 0.1f, 0.0f};
      Quat q = {0.1f * f, 0.2f - 0.01f * f, -0.15f, 1.0f};
      const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
      for (float& v : q) v /= n;
      pose_.rotation[j] = q;
    }
    // The engine chain of the player: its position and rotation arrays hold the pose the phases would have left.
    p_ = et_->Player(kPlayerIndex);
    trap_->BindAnimeChain(kSlot, p_);
    pes::PesFoxWorld& engine = et_->Engine();
    const uint64_t a = engine.Read64(p_ + pes::kPScalePtrOffset);
    const uint64_t positions = engine.Read64(engine.Read64(a + kControlOffset) + pes::kL1PoseOffset);
    const uint64_t rotations = engine.Read64(a + pes::kAQuatOffset);
    for (uint32_t j = 0; j < pes::kAnimeJointCount; ++j)
      for (uint32_t k = 0; k < 4; ++k) {
        engine.Write32(positions + j * pes::kJointEntryBytes + 4 * k, Bits(pose_.position[j][k]));
        engine.Write32(rotations + j * pes::kJointEntryBytes + 4 * k, Bits(pose_.rotation[j][k]));
      }
    pes::StoreF32(trap_->P(kSlot), pes::kPHeadingOffset, kHeadingDeg);
    trap_->WriteAnimeRootPoint(kSlot, reinterpret_cast<const uint8_t*>(kPlayerAt.data()));
  }
  // The image's validity byte for joint `joint` and its map entry (the engine's image, the same bytes the writer reads).
  static uint8_t ValidByte(int joint) { return et_->Externals().image[kValidTableVa - kImageBaseVa + static_cast<uint64_t>(joint)]; }
  static uint32_t MapEntry(int joint) {
    uint32_t v;
    std::memcpy(&v, et_->Externals().image + (kJointMapVa - kImageBaseVa) + 4 * static_cast<uint64_t>(joint), sizeof v);
    return v;
  }

  static std::unique_ptr<pes_engine::EngineTrap> et_;
  static std::string loadError_;
  static std::unique_ptr<pes::PesWorld> trap_;
  pes::PesJointPose pose_;
  uint64_t p_ = 0;
};

std::unique_ptr<pes_engine::EngineTrap> JointBlock::et_;
std::string JointBlock::loadError_;
std::unique_ptr<pes::PesWorld> JointBlock::trap_;

TEST_F(JointBlock, EachValidJointLandsWhereTheHeadingAndPositionPutTheModelJoint) {
  trap_->RunAnimeJoints(kSlot, et_->Externals());
  const uint8_t* p = trap_->P(kSlot);
  const float h = kHeadingDeg * pes::kDegToRad, c = std::cos(h), s = std::sin(h);
  int checked = 0;
  for (int i = 0; i < kBlockJoints; ++i) {
    const std::array<float, 4>& m = pose_.position[MapEntry(i)];
    for (std::size_t k = 0; k < 3; ++k) {
      const float want = ValidByte(i) == 0 ? 0.0f
                         : k == 0          ? m[0] * c + m[2] * s + kPlayerAt[0]
                         : k == 1          ? m[1] + kPlayerAt[1]
                                           : -m[0] * s + m[2] * c + kPlayerAt[2];
      EXPECT_NEAR(pes::LoadF32(p, kPositionBlockOffset + kPositionStride * i + 4 * k), want, kTolerance) << "joint " << i << " axis " << k;
    }
    checked += ValidByte(i) != 0;
  }
  EXPECT_GE(checked, 1) << "the image's validity table marks no joint";
}

TEST_F(JointBlock, TheWorldRotationIsTheHeadingTurnedModelRotation) {
  trap_->RunAnimeJoints(kSlot, et_->Externals());
  const uint8_t* p = trap_->P(kSlot);
  const Quat heading = {0.0f, std::sin(0.5f * kHeadingDeg * pes::kDegToRad), 0.0f, std::cos(0.5f * kHeadingDeg * pes::kDegToRad)};
  for (int i = 0; i < kBlockJoints; ++i) {
    if (ValidByte(i) == 0) continue;
    const Quat want = Mul(heading, pose_.rotation[MapEntry(i)]);
    Quat got;
    for (std::size_t k = 0; k < 4; ++k) got[k] = pes::LoadF32(p, kRotationBlockOffset + kRotationStride * i + 4 * k);
    float same = 0.0f, flipped = 0.0f;  // q and -q are one rotation
    for (std::size_t k = 0; k < 4; ++k) {
      same += (got[k] - want[k]) * (got[k] - want[k]);
      flipped += (got[k] + want[k]) * (got[k] + want[k]);
    }
    EXPECT_LT(std::fmin(same, flipped), kTolerance) << "joint " << i;
  }
}

}  // namespace
