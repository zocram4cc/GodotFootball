// PES's anime joint writer FUN_141EB49B0 (translated, set kick; proven bit-exact on real full-capture states) run on GF's world window
// through PesWorld::RunAnimeJoints: the pose in the anime player's layout (model-space joint positions and rotations, the placement
// matrix of the player's heading and position) comes out as the world joint block P+0x538.. and the world rotations P+0x700..
// that FUN_141EB51A0 and the Kick gate read. The test poses a player, runs the writer, and checks the block against an independent
// recompute (position = model * heading rotation + player position; rotation = RotY(heading) * model rotation), through the joint
// map and the validity table the image holds.
//
// PES_EXE names the unpacked image. Without it, skip.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "onthepitch/pesanimechain.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes_kick_oracle_common.hpp"

namespace {

constexpr int kSlot = 3;
constexpr uint64_t kValidTableVa = 0x14351E540ULL, kJointMapVa = 0x1437F4E60ULL;  // FUN_141EB51A0's validity bytes, FUN_141EBC930's map
constexpr uint64_t kImageBaseVa = 0x140000000ULL;
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

class JointBlock : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* exe = std::getenv("PES_EXE");
    if (exe == nullptr || *exe == '\0') GTEST_SKIP() << "PES_EXE unset (the writer reads its tables from the image)";
    ASSERT_TRUE(image_.Open(exe));
    world_ = std::make_unique<pes::PesWorld>();
    ext_.image = image_.bytes;
    ext_.imageBytes = image_.size;
    pes::PesTrapSegment chain;
    chain.va = pes::kAnimeChainVa;
    chain.bytes.assign(0x1100, 0);
    pes::InitAnimeJointPose(chain.bytes);
    ext_.segments.push_back(std::move(chain));
    world_->BindAnimeChain(kSlot);
    pes::StoreU64(world_->PhysiqueA(kSlot), pes::kAChildO2Offset, pes::PesWorld::O2Va(kSlot));
    pes::StoreF32(world_->P(kSlot), pes::kPHeadingOffset, kHeadingDeg);
    for (std::size_t k = 0; k < 3; ++k) pes::StoreF32(world_->O2(kSlot), pes::kO2PointOffset + 4 * k, kPlayerAt[k]);
    pes::StoreF32(world_->O2(kSlot), pes::kO2PointOffset + 12, 1.0f);
    // A pose no two joints share: positions spread over the body, rotations about different axes.
    for (std::size_t j = 0; j < pes::kAnimeJointCount; ++j) {
      const float f = static_cast<float>(j);
      pose_.position[j] = {0.03f * f - 0.2f, 0.07f * f, 0.02f * f - 0.1f, 0.0f};
      Quat q = {0.1f * f, 0.2f - 0.01f * f, -0.15f, 1.0f};
      const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
      for (float& v : q) v /= n;
      pose_.rotation[j] = q;
    }
  }
  uint8_t ValidByte(int joint) const { return image_.bytes[kValidTableVa - kImageBaseVa + static_cast<uint64_t>(joint)]; }
  uint32_t MapEntry(int joint) const {
    uint32_t v;
    std::memcpy(&v, image_.bytes + (kJointMapVa - kImageBaseVa) + 4 * static_cast<uint64_t>(joint), sizeof v);
    return v;
  }
  pes_kick_oracle::Image image_;
  std::unique_ptr<pes::PesWorld> world_;
  pes::PesTrapExternals ext_;
  pes::PesJointPose pose_;
};

TEST_F(JointBlock, EachValidJointLandsWhereTheHeadingAndPositionPutTheModelJoint) {
  world_->RunAnimeJoints(kSlot, pose_, ext_);
  const uint8_t* p = world_->P(kSlot);
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
  world_->RunAnimeJoints(kSlot, pose_, ext_);
  const uint8_t* p = world_->P(kSlot);
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
