// The one-chain cut (AnimeLayer7). The trap window's chain P+0xA08 is the engine player's own BodyAnimControl; the root point O2+0xD0 and the
// placement object are that chain's; one call of PES's per-tick driver FUN_141EB5E70 runs the body adjusters and the chain's phases and writes
// its outputs into B, which ReadAdjusterFrame reads. PES_EXE names the unpacked image and PES21_ROOT the install; without either, skip.
// The players are constructed before the window binds (the constructor runs on a memory with no window), as the match does it: the window
// binds once per suite, after the players its tests use are built.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "onthepitch/pesanimechain.hpp"
#include "onthepitch/pesfoxworld.hpp"
#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace {

constexpr uint64_t kHeapBytes = 1ull << 30;
// The player block the engine's constructor fills (matchevents.cpp kPesAnimePlayerBytes); its BodyAnimControl is at +0xA08.
constexpr uint64_t kPlayerBytes = 0x3C00;
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
    vfs_ = std::make_unique<pes::PesVfs>();
    ASSERT_TRUE(vfs_->Mount(root, &error)) << error;
    engine_ = std::make_unique<pes::PesFoxWorld>(exe, *vfs_, kHeapBytes);
    engine_->Boot();
    loaded_ = engine_->LoadBody(&loadError_);
    if (!loaded_) return;
    for (int k = 0; k < kPlayers; ++k) {
      const uint64_t p = engine_->Alloc(kPlayerBytes);
      EXPECT_NE(p, 0u);
      engine_->ConstructAnime(p, 0);
      players_.push_back(p);
    }
    pes::PesMemory& memory = engine_->Memory();
    trap_ = std::make_unique<pes::PesWorld>();
    trap_->BindMemory(memory);
    // The member adjusters' vtables and constants: the match's own ConstructActorP on the install's constants (Match ctor).
    const auto pe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &error);
    ASSERT_TRUE(pe) << error;
    const auto actor = pes::LoadActorConstants(*pe, &error);
    ASSERT_TRUE(actor) << error;
    pes::ConstructActorP(*trap_, kSlot, *actor);
    // The install's table bytes and the member vtables' slots, bound as the match binds them (Match ctor, match.cpp).
    const auto tables = pes::LoadPesImageTables(*pe, &error);
    ASSERT_TRUE(tables) << error;
    trap_->BindImageTables(*tables);
    for (int k = 0; k < 3; ++k)
      trap_->AddImageSegment(actor->memberVtable[k], actor->memberVtableSlots[k].data(), actor->memberVtableSlots[k].size());
    ext_.image = memory.Image();
    ext_.imageBase = pes::PesMemory::kImageBase;
    ext_.imageBytes = memory.ImageBytes();
    ext_.borrowed = memory.SharedWindows();
  }
  static void TearDownTestSuite() {
    trap_.reset();
    players_.clear();
    engine_.reset();
    vfs_.reset();
  }
  void SetUp() override {
    if (!engine_) GTEST_SKIP() << "PES_EXE / PES21_ROOT unset";
    ASSERT_TRUE(loaded_) << loadError_;
  }

  // The engine's player block the test `k` uses: constructed before the window bound.
  static uint64_t Player(int k) { return players_.at(static_cast<std::size_t>(k)); }
  static uint64_t ChainOf(uint64_t p) { return engine_->Read64(p + pes::kPScalePtrOffset); }

  static std::unique_ptr<pes::PesVfs> vfs_;
  static std::unique_ptr<pes::PesFoxWorld> engine_;
  static bool loaded_;
  static std::string loadError_;
  static std::vector<uint64_t> players_;
  static std::unique_ptr<pes::PesWorld> trap_;
  static pes::PesTrapExternals ext_;
};

std::unique_ptr<pes::PesVfs> AdjusterTick::vfs_;
std::unique_ptr<pes::PesFoxWorld> AdjusterTick::engine_;
bool AdjusterTick::loaded_ = false;
std::string AdjusterTick::loadError_;
std::vector<uint64_t> AdjusterTick::players_;
std::unique_ptr<pes::PesWorld> AdjusterTick::trap_;
pes::PesTrapExternals AdjusterTick::ext_;

TEST_F(AdjusterTick, WindowChainIsTheEnginePlayersBodyAnimControl) {
  const uint64_t p = Player(0);
  trap_->BindAnimeChain(kSlot, p);
  const uint64_t chain = pes::LoadU64(trap_->P(kSlot), pes::kPScalePtrOffset);
  ASSERT_NE(chain, 0u);
  EXPECT_EQ(chain, ChainOf(p));
  EXPECT_EQ(engine_->Read64(chain), kBodyAnimControlVtable);
  EXPECT_EQ(trap_->P(kSlot)[pes::kPAnimeReadyOffset], 1);
}

TEST_F(AdjusterTick, GWritesLandInTheEnginesB) {
  const uint64_t p = Player(1);
  trap_->BindAnimeChain(kSlot, p);
  trap_->WriteAnimeG(kSlot, 1.25f);
  const uint64_t b = engine_->Read64(ChainOf(p) + pes::kPhysiqueNextOffset);
  EXPECT_FLOAT_EQ(F32(engine_->Read32(b + pes::kPhysiqueGOffset)), 1.25f);
}

TEST_F(AdjusterTick, TheManPositionIsTheEnginesRootPoint) {
  const uint64_t p = Player(2);
  trap_->BindAnimeChain(kSlot, p);
  trap_->WriteAnimeRootPoint(kSlot, reinterpret_cast<const uint8_t*>(kRootPoint));
  const uint64_t o2 = engine_->Read64(ChainOf(p) + pes::kAChildO2Offset);
  EXPECT_FLOAT_EQ(F32(engine_->Read32(o2 + pes::kO2PointOffset)), kRootPoint[0]);
  EXPECT_FLOAT_EQ(F32(engine_->Read32(o2 + pes::kO2PointOffset + 4)), kRootPoint[1]);
  EXPECT_FLOAT_EQ(F32(engine_->Read32(o2 + pes::kO2PointOffset + 8)), kRootPoint[2]);
  EXPECT_FLOAT_EQ(F32(engine_->Read32(o2 + pes::kO2PointOffset + 12)), 1.0f);
}

TEST_F(AdjusterTick, OneDriverCallWritesTheYawThatTheAdjusterHolds) {
  const uint64_t p = Player(3);
  trap_->BindAnimeChain(kSlot, p);
  trap_->WriteAnimeRootPoint(kSlot, reinterpret_cast<const uint8_t*>(kRootPoint));
  trap_->ArmYaw(kSlot, 0.0f, 30.0f, 10, false);
  for (int tick = 0; tick < 3; ++tick) {
    trap_->RunAdjusters(kSlot, kTickSeconds, ext_);
    const pes::PesWorld::AdjusterFrame frame = trap_->ReadAdjusterFrame(kSlot);
    const float own = pes::LoadF32(trap_->P(kSlot), pes::kPYawAdjusterOffset + pes::kAdjYawOutOffset);
    EXPECT_TRUE(frame.yawStepped) << "tick " << tick;
    EXPECT_FALSE(frame.yawIsRate) << "tick " << tick;
    EXPECT_NEAR(frame.yawRad, own, 1e-5f) << "tick " << tick;
  }
}

}  // namespace
