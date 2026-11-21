// The one-chain cut (AnimeLayer7). The trap window's chain P+0xA08 is the engine player's own BodyAnimControl; the root point O2+0xD0 and the
// placement object are that chain's; one call of PES's per-tick driver FUN_141EB5E70 runs the body adjusters and the chain's phases and writes
// its outputs into B, which ReadAdjusterFrame reads. PES_EXE names the unpacked image and PES21_ROOT the install; without either, skip.
// The engine and the window are the shared fixture (pes_engine_trap.hpp): the players are built before the window binds, as the match does it.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "onthepitch/pesanimechain.hpp"
#include "onthepitch/pesfoxworld.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes_engine_trap.hpp"

namespace {

// BodyAnimControl's class (pes_anime_engine_test.cpp): the vtable of the chain the constructor builds.
constexpr uint64_t kBodyAnimControlVtable = 0x142ABA1A8;
constexpr int kSlot = 3;
constexpr float kRootPoint[3] = {38.0f, 0.0f, 2.5f};
// PES's logic frame: 54 a second (kPesTickHz); the adjuster driver's p2.
constexpr float kTickSeconds = 1.0f / 54.0f;
constexpr int kPlayers = 4;  // one per test

float F32(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

class AdjusterTick : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const char* exe = std::getenv("PES_EXE");
    const char* root = std::getenv("PES21_ROOT");
    if (!exe || !*exe || !root || !*root) return;
    std::string error;
    et_ = pes_engine::EngineTrap::Open(exe, root, kPlayers, &error);
    if (!et_) {
      loadError_ = error;
      return;
    }
    trap_ = std::make_unique<pes::PesWorld>();
    et_->BindWorld(*trap_, kSlot);
    ext_ = &et_->Externals();
  }
  static void TearDownTestSuite() {
    trap_.reset();
    et_.reset();
  }
  void SetUp() override {
    if (!et_) GTEST_SKIP() << "PES_EXE / PES21_ROOT unset";
    ASSERT_TRUE(loadError_.empty()) << loadError_;
  }

  static uint64_t Player(int k) { return et_->Player(k); }
  static uint64_t ChainOf(uint64_t p) { return et_->ChainOf(p); }

  static std::unique_ptr<pes_engine::EngineTrap> et_;
  static std::string loadError_;
  static std::unique_ptr<pes::PesWorld> trap_;
  static pes::PesTrapExternals* ext_;
};

std::unique_ptr<pes_engine::EngineTrap> AdjusterTick::et_;
std::string AdjusterTick::loadError_;
std::unique_ptr<pes::PesWorld> AdjusterTick::trap_;
pes::PesTrapExternals* AdjusterTick::ext_ = nullptr;

TEST_F(AdjusterTick, WindowChainIsTheEnginePlayersBodyAnimControl) {
  const uint64_t p = Player(0);
  trap_->BindAnimeChain(kSlot, p);
  const uint64_t chain = pes::LoadU64(trap_->P(kSlot), pes::kPScalePtrOffset);
  ASSERT_NE(chain, 0u);
  EXPECT_EQ(chain, ChainOf(p));
  EXPECT_EQ(et_->Engine().Read64(chain), kBodyAnimControlVtable);
  EXPECT_EQ(trap_->P(kSlot)[pes::kPAnimeReadyOffset], 1);
}

TEST_F(AdjusterTick, GWritesLandInTheEnginesB) {
  const uint64_t p = Player(1);
  trap_->BindAnimeChain(kSlot, p);
  trap_->WriteAnimeG(kSlot, 1.25f);
  const uint64_t b = et_->Engine().Read64(ChainOf(p) + pes::kPhysiqueNextOffset);
  EXPECT_FLOAT_EQ(F32(et_->Engine().Read32(b + pes::kPhysiqueGOffset)), 1.25f);
}

TEST_F(AdjusterTick, TheManPositionIsTheEnginesRootPoint) {
  const uint64_t p = Player(2);
  trap_->BindAnimeChain(kSlot, p);
  trap_->WriteAnimeRootPoint(kSlot, reinterpret_cast<const uint8_t*>(kRootPoint));
  const uint64_t o2 = et_->Engine().Read64(ChainOf(p) + pes::kAChildO2Offset);
  EXPECT_FLOAT_EQ(F32(et_->Engine().Read32(o2 + pes::kO2PointOffset)), kRootPoint[0]);
  EXPECT_FLOAT_EQ(F32(et_->Engine().Read32(o2 + pes::kO2PointOffset + 4)), kRootPoint[1]);
  EXPECT_FLOAT_EQ(F32(et_->Engine().Read32(o2 + pes::kO2PointOffset + 8)), kRootPoint[2]);
  EXPECT_FLOAT_EQ(F32(et_->Engine().Read32(o2 + pes::kO2PointOffset + 12)), 1.0f);
}

TEST_F(AdjusterTick, OneDriverCallWritesTheYawThatTheAdjusterHolds) {
  const uint64_t p = Player(3);
  trap_->BindAnimeChain(kSlot, p);
  trap_->WriteAnimeRootPoint(kSlot, reinterpret_cast<const uint8_t*>(kRootPoint));
  trap_->ArmYaw(kSlot, 0.0f, 30.0f, 10, false);
  for (int tick = 0; tick < 3; ++tick) {
    trap_->RunAdjusters(kSlot, kTickSeconds, *ext_);
    const pes::PesWorld::AdjusterFrame frame = trap_->ReadAdjusterFrame(kSlot);
    const float own = pes::LoadF32(trap_->P(kSlot), pes::kPYawAdjusterOffset + pes::kAdjYawOutOffset);
    EXPECT_TRUE(frame.yawStepped) << "tick " << tick;
    EXPECT_FALSE(frame.yawIsRate) << "tick " << tick;
    EXPECT_NEAR(frame.yawRad, own, 1e-5f) << "tick " << tick;
  }
}

}  // namespace
