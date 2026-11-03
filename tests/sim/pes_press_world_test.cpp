// PES's press team step (the writer 0x140536FC0, the coordinator 0x140634470 and the target eval 0x140633940, translated and
// proven bit-exact on live captures: results_press.json) run on the world window through PesWorld::StagePress. GF's retired
// hand ports (pespress.cpp: RunPresserPass1 / PressWinner / PressEvalPick) picked the loose-ball presser from GF's own
// positions; these cases follow PES's own decompile (docs/re/pes-re-press-life.md 3, pes-re-press-coord.md): the writer
// enrols a man of a side with no designated man, in play, whose d^2 to the ball is under the candidate radius, and writes
// PRESS (0x34) into T+0xA2A0.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include "onthepitch/pesclock.hpp"
#include "onthepitch/pesimagetables.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"

namespace {

using namespace pes;

// The install's table bytes the window runs read (the event store's none record among them), as GF's loaders read them.
const PesImageTables& InstallTables() {
  static const PesImageTables tables = [] {
    std::string error;
    auto exe = PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", &error);
    EXPECT_TRUE(exe) << error;
    auto loaded = exe ? LoadPesImageTables(*exe, &error) : std::nullopt;
    EXPECT_TRUE(loaded) << error;
    return loaded ? *loaded : PesImageTables{};
  }();
  return tables;
}

constexpr uint32_t kPress = 0x34;
constexpr std::size_t kBearingBypassOffset = 0xB709;  // docs/re/pes-re-press-life.md 3.3
constexpr uint32_t kLateFrame = 5000;  // far past any ball event: the writer's pressure score is at its cap
constexpr float kBallX_m = 10.0f;
constexpr float kBallZ_m = 3.0f;
constexpr uint32_t kEventKick = 1;
constexpr float kNear_m = 4.0f;    // inside the writer's 9 m pick radius
constexpr float kFar_m = 60.0f;    // outside the candidate table's 21.5 m
constexpr int kNearMan = 4;
constexpr int kSide = 0;

struct Staged {
  PesWorld world;
  PesWorld::PressFacts facts;

  Staged() {
    world.BindImageTables(InstallTables());
    for (int slot = 0; slot < kSlotCount; ++slot) world.BindSlot(slot);
    StoreU32(world.MatchInfo(), kMiFrameOffset, kLateFrame);
    for (int j = 0; j < kBallSampleCount; ++j) world.SetBallSample(j, kBallX_m, 0.0f, kBallZ_m);
    // The writer's loose-ball arm (0x140537xxx) needs the newest ball event to be a kick kind (1, 2 or 3; E+0x180698 payload+0).
    StoreU32(world.EventImage().newest.data(), 0, kEventKick);
    world.SetMaBall(kSide, kBallX_m, 0.0f, kBallZ_m);
    world.SetMaBall(1 - kSide, kBallX_m, 0.0f, kBallZ_m);
    for (int slot = 0; slot < kSlotCount; ++slot) Place(slot, kFar_m, slot * 2.0f);
    Place(kNearMan, kBallX_m - kNear_m, kBallZ_m);
    facts.playState = kMiPlayStateLive;
    facts.frame = kLateFrame;
    facts.armed = 1;
    facts.attackSign = kSide == 0 ? -1 : 1;
    facts.onPitch.fill(true);
    for (int k = 0; k < kSlotsPerSide; ++k) facts.roleClass[k] = 3;  // an outfield role class (0 is the keeper)
    facts.roleClass[0] = 0;
  }

  void Place(int slot, float x, float z) {
    uint8_t* man = world.ManRecord(slot);
    StoreF32(man, kManPosXOffset, x);
    StoreF32(man, kManPosZOffset, z);
  }

  // PRESS from the last tick: the candidate table keeps a man whose action word is PRESS (FUN_140639830).
  void MarkPress(int k) { StoreU32(world.SideRecord(kSide), kTeamActionOffset + sizeof(uint32_t) * k, kPress); }

  void Run() {
    MarkPress(kNearMan);
    world.StagePress(kSide, facts);
    world.SideRecord(kSide)[kBearingBypassOffset] = 1;  // T+0xB709: the writer skips its bearing window (0x140537700)
    world.RunPressWriter(kSide);
  }

  // T+0xA548[0]: the man the writer enrolled this tick (0xFF none: the writer clears the four entries first).
  uint32_t Enrolled() { return LoadU32(world.SideRecord(kSide), kTeamPressSlotsOffset); }
};

constexpr uint32_t kNobody = 0xFF;
constexpr uint32_t kNearId = kSide * kSlotsPerSide + kNearMan;

class PesPressWorld : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set: the window runs read the install's tables";
  }
};

// The writer's loose-ball arm (FUN_140536FC0): a side with no designated man, a kicked ball, a PRESS man inside the candidate
// radius: the candidate table keeps him, the writer renews PRESS (0x34) and enrols him in T+0xA548[0].
TEST_F(PesPressWorld, ThePressManNearALooseBallIsRenewedAndEnrolledWhenTheSideHasNoDesignatedMan) {
  Staged s;
  s.Run();
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), kPress);
  EXPECT_EQ(s.Enrolled(), kNearId);
}

TEST_F(PesPressWorld, NobodyIsEnrolledWhileTheSideHasADesignatedMan) {
  Staged s;
  s.facts.designated = kNearMan;  // T+0x78 valid: the writer leaves before the search (0x140537116)
  s.Run();
  EXPECT_EQ(s.Enrolled(), kNobody);
}

TEST_F(PesPressWorld, NobodyIsEnrolledWhenTheBallIsNotInPlay) {
  Staged s;
  s.facts.playState = 2;  // MI+0x116C != 1: the writer leaves
  s.Run();
  EXPECT_EQ(s.Enrolled(), kNobody);
}

TEST_F(PesPressWorld, APressManOffThePitchIsNotEnrolled) {
  Staged s;
  s.facts.onPitch[kNearMan] = false;  // MI+0x1748: the candidate table skips a man who is not on the pitch
  s.Run();
  EXPECT_EQ(s.Enrolled(), kNobody);
}

TEST_F(PesPressWorld, APressManOutsideTheCandidateRadiusIsNotEnrolled) {
  Staged s;
  s.Place(kNearMan, kBallX_m - kFar_m, kBallZ_m);
  s.Run();
  EXPECT_EQ(s.Enrolled(), kNobody);
}

// The coordinator's class-5 arm (docs/re/pes-re-press-coord.md 2.10, 8.8): an opponent holds the ball; the side's man at entry 0 of
// the holder's nearest-list row, inside the class-5 radius of the ball, becomes the second presser: PRESS (0x34) and PP[2].
constexpr std::size_t kMaNearestRowsOffset = 0x22E4C;  // MA: 200-byte rows per man, {u32 id, f32 d^2} entries, side 1 at +88
constexpr std::size_t kMaNearestRowBytes = 200;
constexpr std::size_t kMaBallRankOffset = 0x247BE;     // MA+0x247BE+man: the man's rank in the ball-distance list (FUN_14044E0D0)
constexpr std::size_t kMaBallListOffset = 0x24710;     // MA: 8-byte {f32 d^2, ..} per rank
constexpr uint32_t kLevelTier3 = 300;                  // T+0xA294: tier * 100; the coordinator's level flag needs tier >= 3
constexpr int kHolderSlot = 3;
constexpr uint32_t kHolderId = kSlotsPerSide + kHolderSlot;
constexpr float kInsideRadiusDistSq = 1.0f;            // under the 3 m class-5 radius (outfield; 5 m for kind 10)

struct HolderScene : Staged {
  HolderScene() {
    facts.holder = kHolderId;  // MA+0x2601C
    StoreU32(world.SideRecord(kSide), kSideTierOffset, kLevelTier3);
    uint8_t* row = world.Ma(kSide) + kMaNearestRowsOffset + kMaNearestRowBytes * kHolderId;
    StoreU32(row, 0, kNearId);
    StoreF32(row, sizeof(uint32_t), 4.0f);
    world.Ma(kSide)[kMaBallRankOffset + kNearId] = 0;
    StoreF32(world.Ma(kSide) + kMaBallListOffset, 0, kInsideRadiusDistSq);
  }
  void RunCoordinator() {
    world.StagePress(kSide, facts);
    world.RunPressCoordinator(kSide);
  }
};

TEST_F(PesPressWorld, TheNearestManToAnOpponentHolderIsMadeTheSecondPresser) {
  HolderScene s;
  s.RunCoordinator();
  EXPECT_EQ(s.world.PressSecond(kSide), kNearId);
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), kPress);
}

TEST_F(PesPressWorld, TheKeeperIsNotMadeTheSecondPresser) {
  HolderScene s;
  s.facts.roleClass[kNearMan] = 0;  // T+0x87D0 + 8k: role class 0 is the keeper; the class-5 predicate refuses a zero word
  s.RunCoordinator();
  EXPECT_EQ(s.world.PressSecond(kSide), kNobody);
  EXPECT_NE(s.world.PressAction(kSide, kNearMan), kPress);
}

TEST_F(PesPressWorld, NobodyIsMadeThePresserWhileTheBallIsNotInPlay) {
  HolderScene s;
  s.facts.playState = 2;
  s.RunCoordinator();
  EXPECT_EQ(s.world.PressSecond(kSide), kNobody);
}

// The dispatcher's order-word loop (FUN_140535C60 0x140536011-0x140536059) and the slot reset (FUN_140536690): a slot whose order
// word T+0xB748 is 0x26 starts a press through FUN_14062AF10: DELAY when another man of the side, in the side's ball-distance list
// (MA+0x2470C), is in action 5 and the opponents have no designated man (0x14062B...: the helper's list scan), PRESS otherwise.
constexpr std::size_t kMaPairListOffset = 0x2470C;  // MA: {u32 man id, f32 d^2} per rank
constexpr uint32_t kDelay = 0x33;
constexpr uint32_t kOrderPress = 0x26;
constexpr uint32_t kOrderIdle = 0x30;               // the order word of a man no concept holds (every capture)
constexpr uint32_t kActionPressed = 5;              // P+0x5D0 of the list scan's "already pressing" test
constexpr int kOtherMan = 7;
constexpr int kOpponentSide = 1 - kSide;
constexpr std::size_t kTeamResetTargetOffset = 0x214;  // T+0x214: FUN_140536690 stores 0xFF at entry

struct OrderScene : Staged {
  explicit OrderScene(int side = kSide) : orderSide(side) {
    facts.holder = static_cast<uint32_t>(side * kSlotsPerSide + kHolderSlot);  // a man of the side: the helper's holder block is skipped
    world.StagePress(side, facts);
    for (int k = 0; k < kSlotsPerSide; ++k) SetOrder(side, k, kOrderIdle);
    StoreU32(world.SideRecord(1 - side), kTeamDesignatedOffset, kNobody);  // the opponents have no designated man (T+0x78 > 0x15)
  }
  void SetOrder(int side, int k, uint32_t word) {
    StoreU32(world.SideRecord(side), kTeamOrderWordOffset + sizeof(uint32_t) * k, word);
  }
  // Another man of the side, first in the ball-distance list, in action 5: the helper finds a presser with the geometry.
  void AnotherPresserListed() {
    StoreU32(world.Ma(orderSide) + kMaPairListOffset, 0, orderSide * kSlotsPerSide + kOtherMan);
    StoreU32(world.ManRecord(orderSide * kSlotsPerSide + kOtherMan), kManActionOffset, kActionPressed);
  }
  void Enrol(int side, uint32_t id) { StoreU32(world.SideRecord(side), kTeamPressSlotsOffset, id); }
  uint32_t Word(int k) { return world.PressAction(orderSide, k); }
  int orderSide;
};

TEST_F(PesPressWorld, AnOrderWord26SlotStartsAPressWhenNobodyElseHasTheGeometry) {
  OrderScene s;
  s.SetOrder(kSide, kNearMan, kOrderPress);
  s.world.RunPressOrders(kSide);
  EXPECT_EQ(s.Word(kNearMan), kPress);
}

TEST_F(PesPressWorld, AnOrderWord26SlotIsDelayedWhenAnotherListedManIsAlreadyPressing) {
  OrderScene s;
  s.SetOrder(kSide, kNearMan, kOrderPress);
  s.AnotherPresserListed();
  s.world.RunPressOrders(kSide);
  EXPECT_EQ(s.Word(kNearMan), kDelay);
}

TEST_F(PesPressWorld, AnEnrolledManIsPressedEvenWhenAnotherListedManIsPressing) {
  OrderScene s;
  s.SetOrder(kSide, kNearMan, kOrderPress);
  s.AnotherPresserListed();
  s.Enrol(kSide, kNearId);  // FUN_140A41190 finds him in T+0xA548: the helper answers 0
  s.world.RunPressOrders(kSide);
  EXPECT_EQ(s.Word(kNearMan), kPress);
}

TEST_F(PesPressWorld, ASlotWithoutOrderWord26IsLeftAlone) {
  OrderScene s;
  s.SetOrder(kSide, kNearMan, kOrderIdle);
  s.world.RunPressOrders(kSide);
  EXPECT_EQ(s.Word(kNearMan), 0u);
}

TEST_F(PesPressWorld, ASlotAlreadyDelayedKeepsItsWordWhateverTheHelperWouldSay) {
  OrderScene s;
  s.SetOrder(kSide, kNearMan, kOrderPress);
  StoreU32(s.world.SideRecord(kSide), kTeamActionOffset + sizeof(uint32_t) * kNearMan, kDelay);
  s.world.RunPressOrders(kSide);  // the helper would say PRESS (nobody else listed): the loop skips a DELAY slot
  EXPECT_EQ(s.Word(kNearMan), kDelay);
}

TEST_F(PesPressWorld, ASlotAlreadyPressedKeepsItsWordWhateverTheHelperWouldSay) {
  OrderScene s;
  s.SetOrder(kSide, kNearMan, kOrderPress);
  s.AnotherPresserListed();
  StoreU32(s.world.SideRecord(kSide), kTeamActionOffset + sizeof(uint32_t) * kNearMan, kPress);
  s.world.RunPressOrders(kSide);  // the helper would say DELAY: the loop skips a PRESS slot
  EXPECT_EQ(s.Word(kNearMan), kPress);
}

// The helper takes the man's global index (side * 11 + slot): the opponents' side keys its own tables.
TEST_F(PesPressWorld, TheSecondSidesOrderWordIsHandedToTheHelperAsItsGlobalIndex) {
  OrderScene s(kOpponentSide);
  s.SetOrder(kOpponentSide, kNearMan, kOrderPress);
  s.AnotherPresserListed();
  s.Enrol(kOpponentSide, kOpponentSide * kSlotsPerSide + kNearMan);  // enrolled under his global index only
  s.world.RunPressOrders(kOpponentSide);
  EXPECT_EQ(s.Word(kNearMan), kPress);
}

// FUN_140536690's first loop: a PRESS or DELAY slot that is neither the second presser PP[2] nor enrolled goes back to 0.
TEST_F(PesPressWorld, APressSlotNobodyKeptIsResetBeforeTheWriter) {
  Staged s;
  s.MarkPress(kNearMan);
  s.world.StagePress(kSide, s.facts);
  s.world.ResetPress(kSide);
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), 0u);
}

TEST_F(PesPressWorld, ADelaySlotNobodyKeptIsResetToo) {
  Staged s;
  StoreU32(s.world.SideRecord(kSide), kTeamActionOffset + sizeof(uint32_t) * kNearMan, kDelay);
  s.world.StagePress(kSide, s.facts);
  s.world.ResetPress(kSide);
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), 0u);
}

TEST_F(PesPressWorld, AnEnrolledPressSlotSurvivesTheReset) {
  Staged s;
  s.MarkPress(kNearMan);
  s.world.StagePress(kSide, s.facts);
  StoreU32(s.world.SideRecord(kSide), kTeamPressSlotsOffset, kNearId);
  s.world.ResetPress(kSide);
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), kPress);
}

TEST_F(PesPressWorld, TheCoordinatorsSecondPresserSurvivesTheReset) {
  HolderScene s;
  s.RunCoordinator();  // PP[2] = the near man, PRESS
  ASSERT_EQ(s.world.PressSecond(kSide), kNearId);
  s.world.ResetPress(kSide);  // not enrolled in T+0xA548, but PP[2]: PES keeps him (the dispatcher's param_1 + 0x5C)
  EXPECT_EQ(s.world.PressAction(kSide, kNearMan), kPress);
}

TEST_F(PesPressWorld, TheResetClearsTheDispatchersLastTargetWord) {
  Staged s;
  StoreU32(s.world.SideRecord(kSide), kTeamResetTargetOffset, 7);
  s.world.StagePress(kSide, s.facts);
  s.world.ResetPress(kSide);
  EXPECT_EQ(LoadU32(s.world.SideRecord(kSide), kTeamResetTargetOffset), kNobody);  // T+0x214 = 0xFF (0x140536690 entry)
}

}  // namespace
