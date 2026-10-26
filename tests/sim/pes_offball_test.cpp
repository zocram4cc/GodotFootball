// PES 2021's off-ball run manager, Chance run target and stop test, and the
// designated supporter (docs/re/pes-re-off-ball.md, pes-re-run-targets.md).
// Synthetic constants everywhere except the one install test.

#include <gtest/gtest.h>

#include <cstdlib>
#include <cmath>
#include <cstring>

#include "gametypes.hpp"
#include "onthepitch/pesoffball.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace {

using namespace pes::offball;

// The rows the run world reads. In production they are the team AIs' (BuildRunWorld); a test owns them here.
// Each call rebuilds the storage from the run world's positions, and a test calls it before it reads.
void BuildNearest(RunWorld& w) {
  static std::array<pes::ScoreRows, 2> rows;
  for (int t = 0; t < 2; ++t) {
    pes::RowPlayers lanes{};
    for (int lane = 0; lane < pes::lane::kSlots; ++lane) {
      const int id = lane < kSlots ? t * kSlots + lane : (1 - t) * kSlots + (lane - kSlots);
      lanes[lane] = pes::RowPlayer{w.p[id].pos.x, w.p[id].pos.z, w.p[id].active};
    }
    pes::BuildScoreRows(lanes, pes::ScoreRowConstants{}, 0xFF, rows[t], /*pesPath=*/true);
    w.nearestRows[t] = &rows[t];
  }
}

OffBallConstants Synthetic() {
  OffBallConstants k{};
  // selectorVision.o columns: L = 0..4, one row per run in the file's key order.
  const uint8_t caps[kCapRunCount][kAttackLevels] = {
      {1, 1, 2, 2, 3}, {0, 1, 2, 2, 3}, {0, 1, 1, 2, 2},
      {1, 2, 3, 4, 5}, {2, 3, 4, 4, 4}, {0, 1, 1, 1, 1}};
  for (int r = 0; r < kCapRunCount; ++r)
    for (int l = 0; l < kAttackLevels; ++l) k.offerNum[r][l] = caps[r][l];
  k.chanceScoreLimit = 60;
  k.chanceScoreBase = 100.0f;
  k.laneAhead_m = 0.2f;
  k.blockConeFlat = 0.6f;
  k.blockConeSlope = 0.4f;
  k.blockRadius_m = 7.0f;
  k.laneWindow = 8;
  k.reachDhFrom_m = 12.0f;
  k.reachDhTo_m = 30.0f;
  k.reachDhDiv_m = -18.0f;
  k.reachBase_m = 15.0f;
  k.reachDhGain_m = 5.0f;
  k.reachCap_m = 37.0f;
  k.awareFrom = 75.0f;
  k.awareTo = 95.0f;
  k.awareSpan = 20.0f;
  k.awareBonus_m = 18.0f;
  k.gridCellsX = 104.0f;
  k.gridCellsZ = 68.0f;
  k.gridRadiusSq_m2 = 16.0f;
  k.gridFalloff_m = 4.0f;
  k.gridScale = 0.25f;
  k.fallbackSide_m = 10.0f;
  k.predNearSq_m2 = 196.0f;
  k.predMidSq_m2 = 676.0f;
  k.predNear_s = 0.28f;
  k.predMid_s = 0.4f;
  k.predFar_s = 0.6f;
  k.predLead_s = 0.1f;
  k.predOffBall_s = 0.3f;
  return k;
}


TEST(OffBallRank, RunsNeverPreemptRunsOrConceptMoves) {
  EXPECT_TRUE(MayTake(kChance, kBasePosition));
  EXPECT_TRUE(MayTake(kChance, kNothing));
  EXPECT_TRUE(MayTake(kChance, kMoveOnPass));
  EXPECT_FALSE(MayTake(kChance, kCounter));
  EXPECT_FALSE(MayTake(kCounter, kChance));
  EXPECT_FALSE(MayTake(kChance, kOneTwoPassRun));
  EXPECT_FALSE(MayTake(kChance, kTeammateMoving));
}

TEST(OffBallCaps, ColumnByAttackLevelWithTikiTakaOverride) {
  const OffBallConstants k = Synthetic();
  EXPECT_EQ(RunCap(k, kCapChance, 2, false), 3);
  EXPECT_EQ(RunCap(k, kCapCounter, 0, false), 2);
  EXPECT_EQ(RunCap(k, kCapChance, 2, true), 1);
  EXPECT_EQ(RunCap(k, kCapSecondLine, 3, true), 0);
  EXPECT_EQ(RunCap(k, kCapPullAway, 4, true), 0);
  EXPECT_EQ(RunCap(k, kCapCounter, 2, true), 4);
}

TEST(OffBallSelect, ChanceMayTakeOnePastTheCapOtherRunsStopAtIt) {
  std::array<float, kSlots> scores{};
  scores[3] = 120.0f;
  scores[5] = 140.0f;
  scores[7] = 110.0f;
  // Cap reached: an ordinary run takes nobody, Chance takes its best one.
  EXPECT_TRUE(SelectRunners(scores, 2, 2, 1, false).empty());
  const std::vector<int> c = SelectRunners(scores, 2, 2, kChancePerTick, true);
  ASSERT_EQ(c.size(), 1u);
  EXPECT_EQ(c[0], 5);
  // Room for two: best first, stops at the cap.
  const std::vector<int> two = SelectRunners(scores, 0, 2, kChancePerTick, true);
  ASSERT_EQ(two.size(), 2u);
  EXPECT_EQ(two[0], 5);
  EXPECT_EQ(two[1], 3);
  // One new runner per tick for non-Chance selectors.
  EXPECT_EQ(SelectRunners(scores, 0, 4, 1, false).size(), 1u);
  // A zero score is never taken.
  std::array<float, kSlots> none{};
  EXPECT_TRUE(SelectRunners(none, 0, 4, kChancePerTick, true).empty());
}

TEST(OffBallChance, ScoreIsBasePlusForwardX) {
  const OffBallConstants k = Synthetic();
  EXPECT_FLOAT_EQ(ChanceScore(k, -1.0f, -20.0f), 120.0f);
  EXPECT_GT(ChanceScore(k, 1.0f, 30.0f), ChanceScore(k, 1.0f, 10.0f));
}

TEST(OffBallLane, DefendersArePredictedFurtherTheFurtherTheRunnerIsFromTheBall) {
  const OffBallConstants k = Synthetic();
  // int(54·a + 0.5) + int(54·0.1 + 0.5) + int(54·0.3 + 0.5)
  EXPECT_EQ(DefenderPredictFrames(k, 100.0f), 15 + 5 + 16);
  EXPECT_EQ(DefenderPredictFrames(k, 400.0f), 22 + 5 + 16);
  EXPECT_EQ(DefenderPredictFrames(k, 900.0f), 32 + 5 + 16);
}

LaneSearchInput OpenField() {
  LaneSearchInput in;
  in.s = 1.0f;
  in.runner = {10.0f, 0.0f};
  in.runnerPred = {10.0f, 0.0f};
  in.ball = {0.0f, 0.0f};
  in.defLineX = 25.0f;
  in.opponents = {{25.0f, 0.0f}, {25.0f, 1.0f}, {40.0f, 20.0f}};
  in.deepest = {{25.0f, 0.0f}, {25.0f, 1.0f}};
  in.teamDir_deg = 0.0f;
  in.limit = 60;
  return in;
}

TEST(OffBallLane, TargetsTheLineInTheFreeLaneNotIntoTheDefenders) {
  const OffBallConstants k = Synthetic();
  const auto t = ChanceLaneTarget(k, OpenField());
  ASSERT_TRUE(t.has_value());
  EXPECT_FLOAT_EQ(t->x, 25.0f);
  // Lanes are 2 m apart from the touchline: z = F1 - 2 - 2k.
  const float lane = (pesFieldHalfWidth - 2.0f - t->z) / 2.0f;
  EXPECT_NEAR(lane, std::round(lane), 1e-4f);
  // The defenders at z = 0..1 empty the S table there: not that lane.
  EXPECT_GT(std::fabs(t->z), 2.0f);
}

TEST(OffBallLane, LineBehindTheRunnerMovesJustAheadOfHim) {
  const OffBallConstants k = Synthetic();
  LaneSearchInput in = OpenField();
  in.runner = in.runnerPred = {30.0f, 0.0f};
  in.ball = {24.0f, 0.0f};  // the runner within 10 m of it (FUN_14066d720)
  const auto t = ChanceLaneTarget(k, in);
  ASSERT_TRUE(t.has_value());
  EXPECT_FLOAT_EQ(t->x, 30.2f);
}

TEST(OffBallLane, AnOpponentAcrossTheRouteBlocksThatLane) {
  const OffBallConstants k = Synthetic();
  LaneSearchInput in = OpenField();
  const auto open = ChanceLaneTarget(k, in);
  ASSERT_TRUE(open.has_value());
  // Off the line (no S effect), out of the cones, sat on the route to it.
  in.opponents.push_back({20.0f, open->z});
  const auto t = ChanceLaneTarget(k, in);
  ASSERT_TRUE(t.has_value());
  EXPECT_NE(t->z, open->z);
}

TEST(OffBallLane, RunnerWellAheadOfTheBallTakesNoLaneFarFromIt) {
  // FUN_14066d720: outside the box, a runner 10 m+ ahead of the ball rejects
  // lanes more than 27 m (729 m²) from it.
  const OffBallConstants k = Synthetic();
  LaneSearchInput in = OpenField();
  // Exactly 10 m ahead is not "more than": the open field's z = 16 lane stands.
  ASSERT_TRUE(ChanceLaneTarget(k, in).has_value());
  // 11 m ahead: every lane past |z| = 10.2 is over 27 m from the ball, and the
  // nearer ones are blocked; the line point 10 m aside (725 m²) is left.
  in.runner = in.runnerPred = {11.0f, 0.0f};
  const auto t = ChanceLaneTarget(k, in);
  ASSERT_TRUE(t.has_value());
  EXPECT_FLOAT_EQ(t->x, 25.0f);
  EXPECT_FLOAT_EQ(t->z, 10.0f);
}

TEST(OffBallLane, UnderTheLimitFallsBackToTheLineBesideTheBall) {
  const OffBallConstants k = Synthetic();
  LaneSearchInput in = OpenField();
  in.limit = 101;
  in.runner = in.runnerPred = {10.0f, -3.0f};  // below the ball: the -z side
  const auto t = ChanceLaneTarget(k, in);
  ASSERT_TRUE(t.has_value());
  EXPECT_FLOAT_EQ(t->x, in.defLineX);
  EXPECT_FLOAT_EQ(t->z, -10.0f);
  in.ball = {-20.0f, 0.0f};  // far behind: the fallback fails the far-lane leaf too
  EXPECT_FALSE(ChanceLaneTarget(k, in).has_value());
}

// Team 0 attacks +x with the ball at its carrier (id 0) on the halfway line;
// runner id 3 is in a Chance run 10 m ahead; team 1 holds its line at 25 m.
constexpr int kRunner = 3;
RunWorld Running() {
  RunWorld w;
  for (int i = 0; i < 2 * kSlots; ++i) {
    w.p[i].active = true;
    w.p[i].roleClass = 4;
    w.p[i].pos = {i < kSlots ? -20.0f : 40.0f, -30.0f + 3.0f * (i % kSlots)};
    w.p[i].aim = w.p[i].pos;
    w.p[i].slotPoint = w.p[i].pos;
    w.p[i].facing = i < kSlots ? 90.0f : 270.0f;
  }
  w.carrier = 0;
  w.receiver = {0, -1};
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  PlayerFacts& r = w.p[kRunner];
  r.pos = {10.0f, 0.0f};
  r.aim = {25.0f, 0.0f};
  r.state = kChance;
  r.t = 30.0f;
  w.lineX = {-30.0f, 25.0f};
  w.possession = {true, false};
  w.possessionAge = {500.0f, 0.0f};
  BuildNearest(w);
  return w;
}

TEST(OffBallMask, AttackSlotsOpenToAllUntilEnoughAttackersAreForward) {
  RunWorld w = Running();
  // Classes of the slots' roles: GK, CB, the back pair, DMF, CMF, class 5 for 6..8, CF for 9, 10.
  static const int kRoles[kSlots] = {0, 1, 2, 2, 3, 4, 5, 5, 5, 9, 9};
  for (int i = 0; i < kSlots; ++i) w.p[i].roleClass = kRoles[i];
  EXPECT_EQ(AttackSlotMask(w, 0, 4), (1u << kSlots) - 1);
  // Level 2 needs 4 attackers ahead of the ball band; only the runner is.
  EXPECT_EQ(AttackSlotMask(w, 0, 2), (1u << kSlots) - 1);
  for (int i = 7; i < kSlots; ++i) w.p[i].pos = {12.0f, -10.0f + 5.0f * i - 35.0f};
  const uint32_t m = AttackSlotMask(w, 0, 2);
  EXPECT_TRUE((m >> 9) & 1u);  // forwards always, and they count toward the 4
  EXPECT_TRUE((m >> 10) & 1u);
  int others = 0;
  for (int i = 0; i < 9; ++i) others += (m >> i) & 1u;
  EXPECT_EQ(others, 2);
  EXPECT_TRUE((m >> 7) & 1u);  // the two nearest the goal line among the rest
  EXPECT_TRUE((m >> 8) & 1u);
}

// The quota's world (FUN_140500200): team 0 attacks +x from the ball at its carrier (id 0); only
// the men a test places are active. Team 1 stands on the 40 m line. No slot has a receiver, so
// nothing turns.
void Place(RunWorld& w, int id, Vec2f pos, int roleClass) {
  PlayerFacts& f = w.p[id];
  f.active = true;
  f.pos = f.aim = f.slotPoint = pos;
  f.roleClass = roleClass;
}
RunWorld QuotaWorld(Vec2f ball) {
  RunWorld w;
  w.ball = ball;
  w.carrier = 0;
  w.receiver = {-1, -1};
  w.possession = {true, false};
  Place(w, 0, ball, 4);
  Place(w, kSlots + 0, {40.0f, -10.0f}, 4);
  Place(w, kSlots + 1, {45.0f, 0.0f}, 4);
  Place(w, kSlots + 2, {50.0f, 10.0f}, 4);
  return w;
}
constexpr uint32_t kEverySlot = (1u << kSlots) - 1;
constexpr uint32_t SlotBit(int slot) { return 1u << slot; }
uint32_t SlotRange(int from, int to) {
  uint32_t m = 0;
  for (int s = from; s <= to; ++s) m |= SlotBit(s);
  return m;
}

// FUN_140A4F230 draws its offside line from the opponents' S1 list. The defenders have no man
// behind the 1.5 m band, so the line is the back clamp at ball - 7 m (13 m). Team 0's own deepest
// man behind the band stands at 18 m: a line taken from the attackers' own list would pick him.
// From the defenders' line the men at 14, 18, 22 and 30 m are ahead: 4, so level 2 (need 4) runs
// the quota. From the attackers' line there are 3 and every slot is admitted.
TEST(OffBallMask, AheadCountUsesTheOpponentsLine) {
  RunWorld w = QuotaWorld({20.0f, 0.0f});
  Place(w, 1, {14.0f, 0.0f}, 4);
  Place(w, 2, {18.0f, 0.0f}, 4);
  Place(w, 3, {22.0f, 0.0f}, 4);
  Place(w, 4, {30.0f, 0.0f}, 4);
  EXPECT_EQ(AttackSlotMask(w, 0, 1), SlotBit(3) | SlotBit(4));  // need 2: the two nearest the goal
  EXPECT_EQ(AttackSlotMask(w, 0, 2), SlotBit(0) | SlotBit(2) | SlotBit(3) | SlotBit(4));  // need 4
}

// The ladder (FUN_140500200): need 0, 2, 4, 6 for levels 0..3, and every slot from level 4.
TEST(OffBallMask, QuotaLadderByLevel) {
  RunWorld w = QuotaWorld({0.0f, 0.0f});
  for (int k = 1; k <= 8; ++k) Place(w, k, {24.0f + k, 0.0f}, 4);  // 25..32 m ahead, no bonus
  EXPECT_EQ(AttackSlotMask(w, 0, 0), 0u);
  EXPECT_EQ(AttackSlotMask(w, 0, 1), SlotRange(7, 8));
  EXPECT_EQ(AttackSlotMask(w, 0, 2), SlotRange(5, 8));
  EXPECT_EQ(AttackSlotMask(w, 0, 3), SlotRange(3, 8));
  EXPECT_EQ(AttackSlotMask(w, 0, 4), kEverySlot);
}

// A forward is admitted whatever his distance (the BTS path), and his −100 list score takes one
// of the quota's places: three forwards fill a level-1 quota and no non-forward gets in.
TEST(OffBallMask, ForwardsAdmittedAndCountTowardNeed) {
  RunWorld w = QuotaWorld({0.0f, 0.0f});
  Place(w, 1, {30.0f, 0.0f}, 7);
  Place(w, 2, {28.0f, 5.0f}, 8);
  Place(w, 3, {26.0f, -5.0f}, 9);
  for (int k = 4; k <= 8; ++k) Place(w, k, {20.0f, 0.0f}, 4);
  EXPECT_EQ(AttackSlotMask(w, 0, 1), SlotBit(1) | SlotBit(2) | SlotBit(3));
}

// The ahead gate: with fewer than `need` men ahead of the ball band (one here), level 2 (need 4)
// admits every slot. The quota starts only at four.
TEST(OffBallMask, AheadGateAdmitsEveryone) {
  RunWorld w = QuotaWorld({0.0f, 0.0f});
  Place(w, 1, {25.0f, 0.0f}, 4);
  for (int k = 2; k <= 6; ++k) Place(w, k, {-20.0f, 0.0f}, 4);
  EXPECT_EQ(AttackSlotMask(w, 0, 2), kEverySlot);
}

// The distance terms: a man within 10 m of the ball is listed 5 m nearer the goal than his
// distance, and one 25 m or more away gets nothing. The 5 m bonus puts the 10 m man (17.56) ahead
// of the 27.8 m man (18.56); with a 3 m bonus the 27.8 m man would take the second place.
TEST(OffBallMask, FalloffAndSideAreImageConstants) {
  RunWorld w = QuotaWorld({20.0f, 0.0f});
  Place(w, 1, {50.0f, 0.0f}, 4);   // 2.56, first
  Place(w, 2, {30.0f, 0.0f}, 4);   // 10 m from the ball: 22.56 - 5
  Place(w, 3, {34.0f, 24.0f}, 4);  // 27.8 m from the ball: 18.56
  EXPECT_EQ(AttackSlotMask(w, 0, 1), SlotBit(1) | SlotBit(2));
}

// The quota's constants are the install's own. Each float is the operand its instruction reads;
// the level gate, the ladder and the forward stamp are instruction immediates, opcode-checked.
TEST(OffBallQuotaInstall, QuotaConstantsAreTheExesOwn) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto read = [&](uint64_t va) {
    std::array<uint8_t, 8> b{};
    EXPECT_TRUE(exe->ReadVa(va, b.size(), b.data()));
    return b;
  };
  auto f32 = [&](uint64_t va, size_t at = 0) {
    const auto b = read(va);
    float v = 0.0f;
    std::memcpy(&v, b.data() + at, sizeof v);
    return v;
  };
  EXPECT_FLOAT_EQ(f32(0x142588210), kQuotaNearEdge_m);
  EXPECT_FLOAT_EQ(f32(0x14259BE20), kQuotaFarEdge_m);
  EXPECT_FLOAT_EQ(f32(0x14259D668), kQuotaFalloff_m);
  EXPECT_FLOAT_EQ(f32(0x14259222C), kQuotaNearBonus_m);
  EXPECT_FLOAT_EQ(f32(0x14259D654), kQuotaSideTerm_m);
  EXPECT_FLOAT_EQ(f32(0x14259BE0C), kTurnThreshold_deg);
  EXPECT_FLOAT_EQ(f32(0x14259BDDC), kAheadBehindBall_m);
  EXPECT_FLOAT_EQ(f32(0x14259BDF0), kAheadBackClamp_m);
  EXPECT_FLOAT_EQ(f32(0x14259222C), kAheadWindowFront_m);
  EXPECT_FLOAT_EQ(f32(0x14259BE04), kAheadWindowBack_m);
  EXPECT_FLOAT_EQ(f32(0x1425CDAA0), kAheadLateral_m);
  // CMP EBX,imm8 (83 FB ib) at 0x140500232.
  EXPECT_EQ(read(0x140500232)[0], 0x83);
  EXPECT_EQ(read(0x140500232)[2], kQuotaAllFromLevel);
  // MOV byte [RSP+0x20],imm8 (C6 44 24 20 ib) at 0x14050030C (3), 0x140500313 (2), 0x14050031A (1).
  for (int l = 1; l <= 3; ++l) {
    const uint64_t va = l == 1 ? 0x14050031A : l == 2 ? 0x140500313 : 0x14050030C;
    EXPECT_EQ(read(va)[0], 0xC6);
    EXPECT_EQ(read(va)[4], kQuotaNeed[l]);
  }
  // MOV dword [RSP+RAX*8+0x44],imm32 (C7 44 C4 44 id) at 0x140500693: the stamp is −100.0.
  EXPECT_EQ(read(0x140500693)[0], 0xC7);
  EXPECT_FLOAT_EQ(f32(0x140500693, 4), kForwardStampScore);
}

// T+0x4C00 holds PES's role CLASS 0..9 (docs/re/pes-re-role-writer.md §5). Each fixture sets the
// class of the slot's role, and the expectations are the class-scale answers.
TEST(OffBallMask, ADefensiveMidfielderIsClassThreeAndIsReachGated) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  w.p[kRunner].state = kBasePosition;
  w.p[kRunner].t = 1e9f;
  w.lineX[1] = 45.0f;                // the runner stays onside
  w.p[kRunner].pos = {30.0f, 0.0f};  // 30 m ahead of the ball: past the 15 m reach
  // A DMF is class 3, below the reach gate's 4 (run2.c:557-572): he takes the reach path.
  w.p[kRunner].roleClass = 3;
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // A CMF is class 4: not below the gate.
  w.p[kRunner].roleClass = 4;
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
}

// The attack-level skip is `class <= 4` (run2.c:562, `4 < iVar22` exempts): a CMF (class 4) is
// skipped at attack level 0, and the class-5 LMF above him is not.
TEST(OffBallMask, ACentralMidfielderIsClassFourAndIsSkippedAtAttackLevelZero) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  w.p[kRunner].state = kBasePosition;
  w.p[kRunner].t = 1e9f;
  w.p[kRunner].pos = {10.0f, 0.0f};
  w.p[kRunner].aim = {20.0f, 0.0f};
  w.lineX[1] = 25.0f;
  w.carrier = 0;
  w.p[kRunner].roleClass = 4;  // CMF
  EXPECT_EQ(kMaskAttackLevel, ChanceMaskReject(k, w, kRunner, ~0u, 0, false, 50.0f, true));
  w.p[kRunner].roleClass = 5;  // LMF: above the skip
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 0, false, 50.0f, true));
}

// The attack-slot mask admits class >= 7 (SS, WF, CF) outright: PES's writer scores `class < 7`
// and gives the rest score -100 with their bit (FUN_140500200, the `iVar20 < 7` else branch).
// The AMF (class 6) and the RMF (class 5) are scored like everyone else.
TEST(OffBallMask, ARightMidfielderAndAnAttackingMidfielderAreScoredNotAdmitted) {
  RunWorld w = Running();
  // Four attackers ahead of the ball band (ids 1..4; the runner is id 3), so the mask is
  // selective at attack level 2 (need 4).
  w.p[1].pos = {10.0f, -6.0f};
  w.p[2].pos = {10.0f, -3.0f};
  w.p[4].pos = {10.0f, 3.0f};
  w.p[8].roleClass = 6;   // AMF
  w.p[9].roleClass = 5;   // RMF
  w.p[10].roleClass = 7;  // SS: admitted outright
  const uint32_t m = AttackSlotMask(w, 0, 2);
  EXPECT_EQ(0u, (m >> 8) & 1u);
  EXPECT_EQ(0u, (m >> 9) & 1u);
  EXPECT_EQ(1u, (m >> 10) & 1u);
}

TEST(OffBallMask, ADefenderNeedsTheReachAForwardDoesNot) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  w.p[kRunner].state = kBasePosition;
  w.p[kRunner].t = 1e9f;
  w.lineX[1] = 45.0f;                // the runner stays onside
  w.p[kRunner].pos = {30.0f, 0.0f};  // 30 m ahead of the ball: past 15 m
  w.p[kRunner].roleClass = 1;         // CB
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.p[kRunner].roleClass = 9;        // CF: no reach path
  w.lineX[1] = 25.0f;                // ... but not from beyond the line
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.lineX[1] = 45.0f;
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~(1u << kRunner), 2, false, 50.0f, true));
  w.p[kRunner].pos = {30.0f, 43.0f};  // |dz| > 42
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.p[kRunner].pos = {12.0f, 0.0f};
  w.p[kRunner].roleClass = 1;
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.p[kRunner].pos = {19.0f, 0.0f};  // 19 m: past 15, inside Long Pass's 21
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, true, 50.0f, true));
}

TEST(OffBallMask, ALeftBackReachesOnlyWithTheDefensiveFullBackStyleOnPesLoop) {
  // styleLb is FUN_140a64030(id, 0x11): the Defensive Full-back style on his
  // role row. Off pes_loop there is no style table and GF's stand-in (any
  // registered left back is styleLb) holds.
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  w.p[kRunner].state = kBasePosition;
  w.p[kRunner].t = 1e9f;
  w.lineX[1] = 45.0f;
  w.p[kRunner].pos = {30.0f, 0.0f};  // 30 m ahead: past the 15 m reach
  w.p[kRunner].roleClass = 2;        // SB
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));  // stand-in, off pes_loop

  constexpr int kRoleSB = 2;
  pes::style::Compat compat{};
  compat.row[kRoleSB] = pes::style::StyleWord(PlayingStyles::Player::DefensiveFullBack);
  w.styleCompat = compat;
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));  // no style: reach
  w.p[kRunner].styleWord = pes::style::StyleWord(PlayingStyles::Player::DefensiveFullBack);
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));  // Defensive FB: no reach
  w.p[kRunner].styleWord = pes::style::StyleWord(PlayingStyles::Player::OffensiveFullBack);
  EXPECT_FALSE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));  // another FB style
}

TEST(OffBallMask, NoChanceRunsFromOwnHalfUntilFourSecondsOfPossession) {
  // FUN_1405938a0: in its own half a team runs only with a free forward-facing
  // focus player or after 4 s of possession. This is the mask's *team head* -
  // the raw evaluates it once, before the slot loop - so it is read here rather
  // than through the per-slot function.
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  w.p[kRunner].state = kBasePosition;
  w.p[kRunner].roleClass = 9;
  w.p[0].pos = w.ball = {-5.0f, 0.0f};
  w.p[kRunner].pos = {5.0f, 0.0f};
  EXPECT_TRUE(TeamChanceMayRun(w, 0));
  // With the shared verdict the slot itself is a candidate.
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.possessionAge[0] = 100.0f;
  EXPECT_FALSE(TeamChanceMayRun(w, 0));
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  // ... and with the verdict false the slot is refused for that reason, whatever
  // his own gates say.
  EXPECT_EQ(kMaskTeamMayNotRun,
            ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, false));
  w.p[0].vel = {5.0f, 0.0f};  // the carrier runs forward, nobody near: free
  EXPECT_TRUE(TeamChanceMayRun(w, 0));
}

TEST(OffBallStop, ChanceKeepsItsMinimumRunTimeThenTheCommonTestDecides) {
  RunWorld w = Running();
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
  w.carrier = kRunner;
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
  w.p[kRunner].t = 5.0f;
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));  // 0x140607a60: the carrier test is ahead of the minimum
  w.carrier = 0;
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));  // under 11 frames the common test cannot stop him
  w = Running();
  w.possession = {false, true};
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, ChanceStopsAPadControlledRunnerBeforeTheMinimum) {
  // 0x140607a60 tests pad control before the minimum run time.
  RunWorld w = Running();
  w.p[kRunner].humanControlled = true;
  w.p[kRunner].t = 2.0f;
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, ChanceStopsAPassGetRunnerOnceThePassFigureExceedsOne) {
  // 0x140607a60: PASS_GET (action 5) with FUN_140a51320 > 1 stops, ahead of the
  // minimum. A through pass to a mate gives the figure 2 (its kind).
  RunWorld w = Running();
  w.p[kRunner].passGet = true;
  w.p[kRunner].t = 2.0f;
  w.ev = {true, 2, 1.0f, {0.0f, 0.0f}, 5};
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
  w.ev.kind = 1;  // a short pass gives 1, which is not above one
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, ThePassGetFigureFallsBackToFramesSinceTheKickWithNoTargetPlayer) {
  // FUN_140a51320's other branch: with no valid target player the figure is the
  // PES frames since the kick.
  RunWorld w = Running();
  w.p[kRunner].passGet = true;
  w.p[kRunner].t = 2.0f;
  w.ev = {true, 1, 0.5f, {0.0f, 0.0f}, -1};
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
  w.ev.age = 3.0f;
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, OnlyTheRunnerInPassGetIsStoppedByThePassFigure) {
  RunWorld w = Running();
  w.p[kRunner].t = 2.0f;
  w.ev = {true, 2, 1.0f, {0.0f, 0.0f}, 5};
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, ReleasedPastTheLineOrNearTheGoalLine) {
  RunWorld w = Running();
  w.p[kRunner].pos = {26.0f, 0.0f};  // 1 m past: under 1.5 early, over 0.7 late
  w.p[0].pos = w.ball = {20.0f, 0.0f};
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
  w.p[kRunner].t = 50.0f;
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
  w = Running();
  w.lineX[1] = 60.0f;
  w.p[0].pos = w.ball = {45.0f, 0.0f};
  w.p[kRunner].pos = {50.0f, 0.0f};
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, FarRunnerStopsUnlessWellBehindTheBall) {
  RunWorld w = Running();
  w.p[kRunner].pos = {-35.0f, 0.0f};
  w.p[kRunner].aim = {-20.0f, 0.0f};
  w.p[0].pos = w.ball = {10.0f, 0.0f};  // 45 m away, 45 m behind: keep
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
  w.p[kRunner].pos = {10.0f, 0.0f};
  w.p[0].pos = w.ball = {10.0f, -45.0f};  // 45 m away, level: stop
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, LooseBallKickedFarPastHimEndsTheRun) {
  // FUN_140692810: no carrier, a kick within the last second carried the ball
  // 20 m+, and it is not ahead of the runner.
  RunWorld w = Running();
  w.carrier = -1;
  w.ball = {8.0f, 0.0f};
  w.ev = {true, 1, 20.0f, {-15.0f, 0.0f}, 7};
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
  w.ev.pos = {0.0f, 0.0f};  // only 8 m: no
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, ThePassToHimEndsTheRunAThroughBallAfterEighteenFrames) {
  RunWorld w = Running();
  w.ev = {true, 1, 2.0f, {0.0f, 0.0f}, kRunner};
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
  w.ev.kind = 2;
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
  w.ev.age = 18.0f;
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
}

TEST(OffBallStop, ADefenderRunsOnlyIfNoCloserDefenderIsRunning) {
  // FUN_140596240 over the role-class map: a CB/SB runner stops when another
  // CB/SB runner is nearer the ball.
  RunWorld w = Running();
  w.p[kRunner].roleClass = 2;
  BuildNearest(w);
  EXPECT_FALSE(RunStop(w, kRunner, kChance));
  w.p[5].roleClass = 1;
  w.p[5].state = kChance;
  w.p[5].pos = {5.0f, 2.0f};
  BuildNearest(w);
  EXPECT_TRUE(RunStop(w, kRunner, kChance));
}

TEST(OffBallSupport, SupporterCostPrefersInFrontAndPenalisesTooClose) {
  EXPECT_FLOAT_EQ(SupporterCost(10.0f, 12.0f), 19.0f);
  EXPECT_FLOAT_EQ(SupporterCost(0.0f, 8.0f), 23.0f);
  EXPECT_LT(SupporterCost(0.0f, 12.0f), SupporterCost(40.0f, 12.0f));
}

TEST(OffBallSupport, DistanceGrowsWithSupportRangeAndDeepBall) {
  SupportDistanceInput in;
  in.inPlay = true;
  in.supportRange = 1;
  in.ballX = 0.0f;
  in.ownLineX = -30.0f;
  in.oppLineX = 30.0f;
  in.bearingFromAttack_deg = 0.0f;
  const float low = SupportDistance_m(in);
  in.supportRange = 10;
  EXPECT_NEAR(SupportDistance_m(in) - low, 3.0f, 1e-4f);
  in.ballX = -25.0f;  // deep in own half
  EXPECT_GT(SupportDistance_m(in), low + 3.0f);
  in.inPlay = false;
  EXPECT_FLOAT_EQ(SupportDistance_m(in), 8.0f);
}

// FUN_140654e60: the frames to reach a point. A player walking at the slow
// tier covers 100/(fps·72) m per frame and a jogger 100/(fps·12.9).
TEST(OffBallReach, FramesToReachGrowsWithDistanceAndFallsWithSpeed) {
  RunWorld w = Running();
  w.p[0].pos = {0.0f, 0.0f};
  w.p[0].facing = 0.0f;               // toward +x, the target's direction
  w.p[0].roleClass = 4;                // a midfielder: no keeper's half scale
  w.p[0].vel = {0.02f, 0.0f};         // 1.08 km/h-equivalent: standing still
  const int near = FramesToReach(w, 0, {10.0f, 0.0f});
  const int far = FramesToReach(w, 0, {30.0f, 0.0f});
  EXPECT_GT(far, near);
  // A faster player reaches the same point sooner.
  w.p[0].vel = {0.15f, 0.0f};         // 8.1 km/h
  EXPECT_LT(FramesToReach(w, 0, {10.0f, 0.0f}), near);
  // Already there and facing it: only the speed-ramped term, and a player at
  // rest below 11 km/h has u = 0, so no frames at all.
  w.p[0].vel = {0.0f, 0.0f};
  EXPECT_EQ(FramesToReach(w, 0, {0.0f, 0.0f}), 0);
  // Past the 20 s ceiling FUN_140654e60 falls back to the straight-line jog
  // tier 100/(fps·12.9) m/frame, which needs 154 m of run to reach.
  w.p[0].vel = {0.02f, 0.0f};
  const int capped = FramesToReach(w, 0, {200.0f, 0.0f});
  EXPECT_EQ(capped, static_cast<int>(200.0f / (100.0f / (kPesFps * 12.9f))));
}

// FUN_14068b1a0: the crowded-mate stop. An SB (AI class 2) runner inside 20 m
// of the ball, behind the opponent line by at most 7 m, is released when
// another runner who is not a class-1 Chance mate stands within 12 m.
TEST(OffBallStop, AnSBWithAnotherRunnerTwelveMetresAwayIsReleased) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 2;             // SB
  r.state = kChance;
  r.t = kPesFps * 4.0f;      // past every early exemption
  r.pos = {20.0f, 3.0f};     // 5 m behind the 25 m line, 10 m from the ball
  r.aim = {25.0f, 3.0f};
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  w.lineX[1] = 25.0f;
  PlayerFacts& m = w.p[5];
  m.pos = {22.0f, 3.0f};     // 2 m away
  m.state = kChance;         // on a run
  m.aiClass = 5;
  EXPECT_TRUE(CrowdedMateStop(w, kRunner));
  // A CB instead of an SB: not this leaf.
  r.aiClass = 1;
  EXPECT_FALSE(CrowdedMateStop(w, kRunner));
  r.aiClass = 2;
  // More than 7 m behind the line: not this leaf.
  r.pos = {17.0f, 3.0f};
  EXPECT_FALSE(CrowdedMateStop(w, kRunner));
  r.pos = {20.0f, 3.0f};
  // Too far from the ball: not this leaf.
  r.pos = {45.0f, 3.0f};
  EXPECT_FALSE(CrowdedMateStop(w, kRunner));
}

TEST(OffBallStop, ACrowdingMateMustBeRunningAndTwelveMetresAway) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 2;
  r.state = kChance;
  r.t = kPesFps * 4.0f;
  r.pos = {20.0f, 3.0f};
  r.aim = {25.0f, 3.0f};
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  PlayerFacts& m = w.p[5];
  m.pos = {22.0f, 3.0f};
  m.state = kNothing;       // not running: he cannot crowd
  EXPECT_FALSE(CrowdedMateStop(w, kRunner));
  m.state = kChance;        // running, and inside 12 m
  EXPECT_TRUE(CrowdedMateStop(w, kRunner));
  m.pos = {40.0f, 3.0f};    // 20 m away
  EXPECT_FALSE(CrowdedMateStop(w, kRunner));
  m.pos = {22.0f, 3.0f};
  m.aiClass = 1;            // a class-1 Chance mate: exempt
  m.state = kChance;
  EXPECT_FALSE(CrowdedMateStop(w, kRunner));
}

// FUN_14068aeb0: the pass-to-me stop. With a kick in flight whose recorded
// target is not this runner, an opponent within 5 m of him and the runner
// 1.2-4 m behind the opponent line, the run is released when he reaches his
// own route point inside the receiver's lead plus int(fps·0.15+0.5): the
// receiver can no longer beat him to the point the pass is played to.
TEST(OffBallStop, TheRunnerIsReleasedWhenHeBeatsTheReceiverToHisPoint) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 5;                 // not 2 (SB) and not 0x13
  r.state = kChance;
  r.t = kPesFps * 5.0f;          // past the 2·fps gate
  r.pos = {22.0f, 3.0f};         // 3 m behind the 25 m line
  r.aim = {25.0f, 3.0f};
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  w.lineX[1] = 25.0f;
  // The kick goes to slot 7, not to the runner, and is under way.
  w.receiver[0] = 7;
  w.ev = {true, 1, 3.0f, {10.0f, 3.0f}, 7};
  // An opponent next to the runner (5 m cone) and the receiver, slower, far off.
  PlayerFacts& o = w.p[kSlots + 4];
  o.pos = {24.0f, 3.0f};
  PlayerFacts& q = w.p[7];
  q.pos = {-6.0f, 3.0f};           // 16 m from the ball: the 0.28 s lead band
  r.aim = {22.1f, 3.0f};          // he is already at his own route point
  EXPECT_TRUE(PassToMeStop(w, kRunner));
  // The runner's own target pushed far away: he needs more frames than the
  // receiver's lead, so the keep ends.
  r.aim = {2.0f, 3.0f};
  EXPECT_FALSE(PassToMeStop(w, kRunner));
  r.aim = {22.1f, 3.0f};
  // The pass is to the runner himself: never a keep.
  w.ev.target = kRunner;
  EXPECT_FALSE(PassToMeStop(w, kRunner));
  w.ev.target = 7;
  // No kick at all: never a keep.
  w.ev.valid = false;
  EXPECT_FALSE(PassToMeStop(w, kRunner));
  w.ev.valid = true;
  // No opponent within 5 m: never a keep.
  o.pos = {45.0f, 3.0f};
  EXPECT_FALSE(PassToMeStop(w, kRunner));
}

// The lead band: the receiver's lead comes from his distance to the ball,
// 196/676 m² thresholds to 0.28/0.35/0.38 s (FUN_140653480), and the runner is
// released only when he reaches his own route point inside that lead plus
// int(fps·0.15+0.5) = 8. Each pair below pins one threshold by choosing the
// runner's distance to his own point so the margin lands just inside or just
// outside the band.
TEST(OffBallStop, PassToMeStopReleasesInsideTheNearLeadBand) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 5;
  r.state = kChance;
  r.t = kPesFps * 5.0f;
  r.pos = {22.0f, 3.0f};
  r.facing = 90.0f;
  r.aim = r.pos;                 // on his own point: 2 frames, 15 >= 2 + 8
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  w.lineX[1] = 25.0f;
  w.receiver[0] = 7;
  w.ev = {true, 1, 3.0f, {10.0f, 3.0f}, 7};
  w.p[kSlots + 4].pos = {24.0f, 3.0f};
  w.p[7].pos = {-3.0f, 3.0f};    // 13 m from the ball (169 m²): int(fps·0.28+0.5) = 15
  EXPECT_TRUE(PassToMeStop(w, kRunner));
  w.p[7].pos = {3.0f, 3.0f};     // 7 m from the ball (49 m²): the same 15
  EXPECT_TRUE(PassToMeStop(w, kRunner));
  w.p[7].pos = {-17.0f, 3.0f};   // 27 m from the ball (729 m²): int(fps·0.38+0.5) = 21
  EXPECT_TRUE(PassToMeStop(w, kRunner));
}

TEST(OffBallStop, PassToMeStopKeepsOutsideTheNearLeadBand) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 5;
  r.state = kChance;
  r.t = kPesFps * 5.0f;
  r.pos = {22.0f, 3.0f};
  r.facing = 90.0f;
  r.aim = {22.25f, 3.0f};        // 0.25 m out: 11 frames, 15 < 11 + 8
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  w.lineX[1] = 25.0f;
  w.receiver[0] = 7;
  w.ev = {true, 1, 3.0f, {10.0f, 3.0f}, 7};
  w.p[kSlots + 4].pos = {24.0f, 3.0f};
  w.p[7].pos = {3.0f, 3.0f};     // 49 m²: the 15-frame band cannot reach him
  EXPECT_FALSE(PassToMeStop(w, kRunner));
  w.p[7].pos = {-17.0f, 3.0f};   // 729 m²: the 21-frame band can
  EXPECT_TRUE(PassToMeStop(w, kRunner));
}

// The lead bands, FUN_140653480's distance ramp against the ball: 0.28 s inside
// 196 m², 0.35 s inside 676 m², 0.38 s beyond it. A runner 0.25 m from his own
// point needs 12 frames, so the 15-frame near band and the 19-frame middle band
// keep him while the 21-frame far band releases him - which pins all three
// thresholds at once.
TEST(OffBallStop, PassToMeStopReleasesInTheFarAndMiddleLeadBands) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 5;
  r.state = kChance;
  r.t = kPesFps * 5.0f;
  r.pos = {22.0f, 3.0f};
  r.facing = 90.0f;
  r.aim = {22.25f, 3.0f};        // 12 frames to his own point
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  w.lineX[1] = 25.0f;
  w.receiver[0] = 7;
  w.ev = {true, 1, 3.0f, {10.0f, 3.0f}, 7};
  w.p[kSlots + 4].pos = {24.0f, 3.0f};
  w.p[7].pos = {-17.0f, 3.0f};   // 729 m²: int(fps·0.38+0.5) = 21, and 21 >= 20
  EXPECT_TRUE(PassToMeStop(w, kRunner));
  w.p[7].pos = {-15.0f, 3.0f};   // 625 m²: int(fps·0.35+0.5) = 19, and 19 < 20
  EXPECT_FALSE(PassToMeStop(w, kRunner));
  w.p[7].pos = {3.0f, 3.0f};     // 49 m²: the 15-frame near band cannot reach him
  EXPECT_FALSE(PassToMeStop(w, kRunner));
}

// The far band's ceiling: a runner 0.45 m from his own point (16 frames) is
// NOT released by a far-band receiver - 21 < 24 - so a longer far lead than
// 0.38 s would wrongly release him.
TEST(OffBallStop, PassToMeStopFarBandKeepsARunnerTooFarFromHisPoint) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 5;
  r.state = kChance;
  r.t = kPesFps * 5.0f;
  r.pos = {22.0f, 3.0f};
  r.facing = 90.0f;
  r.aim = {22.45f, 3.0f};        // 16 frames to his own point
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  w.lineX[1] = 25.0f;
  w.receiver[0] = 7;
  w.ev = {true, 1, 3.0f, {10.0f, 3.0f}, 7};
  w.p[kSlots + 4].pos = {24.0f, 3.0f};
  w.p[7].pos = {-17.0f, 3.0f};   // 729 m²: int(fps·0.38+0.5) = 21 < 24
  EXPECT_FALSE(PassToMeStop(w, kRunner));
  w.p[7].pos = {3.0f, 3.0f};     // 49 m²: 15 < 24 as well
  EXPECT_FALSE(PassToMeStop(w, kRunner));
}

TEST(OffBallStop, PassToMeStopNeedsTheRunnerNearTheLine) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.aiClass = 5;
  r.state = kChance;
  r.t = kPesFps * 5.0f;
  r.pos = {22.0f, 3.0f};
  r.aim = {22.1f, 3.0f};
  w.p[0].pos = w.ball = {10.0f, 3.0f};
  w.lineX[1] = 25.0f;
  w.receiver[0] = 7;
  w.ev = {true, 1, 3.0f, {10.0f, 3.0f}, 7};
  w.p[kSlots + 4].pos = {24.0f, 3.0f};
  PlayerFacts& q = w.p[7];
  q.pos = {-6.0f, 3.0f};
  EXPECT_TRUE(PassToMeStop(w, kRunner));
  // More than 4 m behind the line (a4c9a0 -4): not this keep.
  r.pos = {19.0f, 3.0f};
  EXPECT_FALSE(PassToMeStop(w, kRunner));
  r.pos = {22.0f, 3.0f};
  r.aim = {22.1f, 3.0f};
  // More than 1.2 m past the line (a4c9a0 -1.2): not this keep.
  r.pos = {26.5f, 3.0f};
  EXPECT_FALSE(PassToMeStop(w, kRunner));
}

// FUN_140606cd0's rank > 1 angle sub-test (raw 0x140607741-0x140607814): a
// forward on the second line or later whose route leaves him within 80 deg of
// the attack direction and within 20 m of the ball-line point is only a
// candidate while that point is in the attacking half - GF carries no
// instruction 2, so a4c720's arguments are a = 0 and k = 0.
TEST(OffBallMask, ASecondLineForwardNeedsTheLinePointInTheAttackingHalf) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;                 // CF: the reach path needs no style bit
  r.pos = {0.0f, -2.0f};           // on the halfway line, off centre
  r.aim = {4.0f, -2.0f};
  r.facing = 90.0f;                // the attack direction, +x
  w.lineX[1] = 45.0f;
  w.carrier = 0;
  // A teammate further up the pitch makes this runner the second line.
  w.p[9].pos = {40.0f, 6.0f};
  w.p[10].pos = {41.0f, -6.0f};
  w.p[8].pos = {39.0f, 10.0f};
  // The ball-line point beside the runner, 6 m to his goal side and 2 m into
  // the attacking half: the bearing from it to him is inside the 45 deg window
  // of his facing, and inside 20 m.
  w.p[0].pos = w.ball = {2.0f, -8.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
  // The same line point 2 m into the defending half: the sub-test rejects him.
  w.p[0].pos = w.ball = {-2.0f, -8.0f};
  EXPECT_FALSE(RankAngleOk(w, kRunner, 4));
}

TEST(OffBallMask, TheAngleSubTestOnlyRunsForAForwardOnTheSecondLine) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;                 // CF
  r.pos = {0.0f, -2.0f};
  r.aim = {4.0f, -2.0f};
  r.facing = 90.0f;
  w.p[0].pos = w.ball = {2.0f, -8.0f};   // 6 m to his side, in the window
  w.lineX[1] = 45.0f;
  w.carrier = 0;
  w.p[9].pos = {40.0f, 6.0f};
  w.p[10].pos = {41.0f, -6.0f};
  w.p[8].pos = {39.0f, 10.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
  // An AMF with three attackers ahead: classes 4 and up take no reach path,
  // so the sub-test is not entered at all and the line point's half does not
  // matter. (With four ahead the OR gate would let every rank in.)
  r.roleClass = 6;
  w.p[8].pos = {-20.0f, -14.0f};
  w.p[9].pos = {-20.0f, -17.0f};
  w.p[10].pos = {-20.0f, -20.0f};
  w.p[0].pos = w.ball = {-2.0f, -8.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
  r.roleClass = 9;
  // A CF on the first line: the rank gate is not entered, so no veto.
  for (int i = 0; i < kSlots; ++i) w.p[i].pos.x = -20.0f;
  r.pos = {0.0f, -2.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
  // Back on the second line, 26 m from the ball-line point: past the 20 m
  // window, so the sub-test rejects him wherever the line point sits.
  w.p[9].pos = {40.0f, 6.0f};
  w.p[10].pos = {41.0f, -6.0f};
  w.p[8].pos = {39.0f, 10.0f};
  w.p[0].pos = w.ball = {0.0f, -28.0f};
  EXPECT_FALSE(RankAngleOk(w, kRunner, 4));
  // An opponent inside the 80 deg cone about the attack direction lets him
  // through even then.
  w.p[kSlots + 3].pos = {2.0f, -2.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
}

// The sub-test's three gates, one at a time: the rank, the facing window and
// the 20 m window. Each fixture below holds the other two open.
TEST(OffBallMask, TheAngleSubTestNeedsTheSecondLineNotJustAForward) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;                 // CF
  r.pos = {0.0f, -2.0f};
  r.aim = {4.0f, -2.0f};
  r.facing = 90.0f;
  w.p[0].pos = w.ball = {-2.0f, -8.0f};   // the veto condition
  w.lineX[1] = 45.0f;
  w.carrier = 0;
  // Three teammates ahead of the ball band, and this runner behind them all:
  // he is the second line, so the sub-test runs.
  w.p[9].pos = {40.0f, 6.0f};
  w.p[10].pos = {41.0f, -6.0f};
  w.p[8].pos = {39.0f, 10.0f};
  for (int i = 0; i < kSlots; ++i)
    if (i != 8 && i != 9 && i != 10 && i != kRunner) w.p[i].pos = {-20.0f, -30.0f + 3.0f * i};
  EXPECT_FALSE(RankAngleOk(w, kRunner, 4));
  // Now the runner is the first line: the sub-test does not run.
  w.p[9].pos = {-20.0f, 6.0f};
  w.p[10].pos = {-20.0f, -6.0f};
  w.p[8].pos = {-20.0f, 10.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
}

TEST(OffBallMask, TheAngleSubTestFacingWindowIsFortyFiveDegrees) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;                 // CF
  r.pos = {0.0f, -2.0f};
  r.aim = {4.0f, -2.0f};
  w.p[9].pos = {40.0f, 6.0f};
  w.p[10].pos = {41.0f, -6.0f};
  w.p[8].pos = {39.0f, 10.0f};
  w.p[0].pos = w.ball = {-2.0f, -8.0f};
  w.lineX[1] = 45.0f;
  w.carrier = 0;
  // Facing 60 deg off the bearing to him: outside the 45 deg window, so the
  // sub-test lets him through even with the line point behind.
  r.facing = 131.6f;
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
  // Facing 18 deg off it (68.4): inside the window, so the veto applies.
  r.facing = 50.0f;
  EXPECT_FALSE(RankAngleOk(w, kRunner, 4));
}

TEST(OffBallMask, TheAngleSubTestWindowIsTwentyMetres) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;                 // CF
  r.pos = {0.0f, -2.0f};
  r.aim = {4.0f, -2.0f};
  r.facing = 90.0f;
  w.p[9].pos = {40.0f, 6.0f};
  w.p[10].pos = {41.0f, -6.0f};
  w.p[8].pos = {39.0f, 10.0f};
  w.p[7].pos = {38.0f, 14.0f};
  w.lineX[1] = 45.0f;
  w.carrier = 0;
  // 26.7 m from the line point (712 m2, past the 400 m2 window) with the point
  // in the attacking half, so only the window can reject him.
  w.p[0].pos = w.ball = {6.0f, -28.0f};
  EXPECT_FALSE(RankAngleOk(w, kRunner, 4));
  // Inside it, 6 m to his side (40 m2): the half test passes and he is fine.
  w.p[0].pos = w.ball = {2.0f, -8.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
  // 12 m out (144 m2), still inside 400 m2 but past a 100 m2 window.
  w.p[0].pos = w.ball = {6.0f, -14.0f};
  EXPECT_TRUE(RankAngleOk(w, kRunner, 4));
}

// FUN_140606cd0's gates, one reason each. The classifier names the gate that
// rejected a slot, so a live trace can be read without a debugger.
TEST(OffBallMaskTrace, TheClassifierNamesTheRejectingGate) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;
  r.pos = {10.0f, 0.0f};
  r.aim = {20.0f, 0.0f};
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  w.lineX[1] = 25.0f;
  w.carrier = 0;
  // A clean candidate: no gate rejects him.
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // The generic tail's three disjuncts need no carrier to be seen: with one,
  // the first disjunct always passes. Without one, a receiver inside
  // team-may-run's 25 m but past the tail's 49 m2 (7 m) rejects him.
  w.carrier = -1;
  w.receiver[0] = 1;
  w.p[1].pos = {10.0f, 2.0f};   // 104 m2 from the ball, 10.2 m away
  EXPECT_EQ(kMaskGeneric, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // The tail's !Valid(q) disjunct is dead in the walk order: with no carrier
  // FUN_140593290 already needs a valid focus player, and with a carrier the
  // first disjunct passes first. So a slot with neither never reaches the tail
  // - and now that the team head owns 593290, the refusal surfaces there rather
  // than per slot.
  w.carrier = -1;
  w.receiver[0] = -1;
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  // Restored: the clean candidate the test starts on.
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  w.carrier = 0;
  w.receiver[0] = 1;
  w.p[1].pos = {11.0f, 0.0f};

  // The rank/state gate: a running slot may only come from PULL_AWAY.
  r.state = kChance;
  EXPECT_EQ(kMaskRankOrState, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.state = kBasePosition;
  // The carrier and the goalkeeper are never candidates.
  EXPECT_EQ(kMaskCarrierOrReceiver, ChanceMaskReject(k, w, 0, ~0u, 2, false, 50.0f, true));
  // With a carrier the receiver is not refused here: the !Valid disjunct needs no carrier (946-950).
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, 1, ~0u, 2, false, 50.0f, true));
  w.receiver[0] = -1;
  r.roleClass = 0;  // the keeper
  EXPECT_EQ(kMaskCarrierOrReceiver, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.roleClass = 9;
  w.receiver[0] = 1;
  // Every state IsRun() accepts also loses the rank test first, so
  // 593430's own "already running" test is unreachable through Chance:
  // kMaskRankOrState fires for all of them. (0x1d..0x21 rank 2 = Chance's
  // own rank, 0x24/0x25/0x2c rank 0, so MayTake is false either way.)
  r.state = kSecondLine;
  EXPECT_EQ(kMaskRankOrState, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.state = kOneTwoPassRun;
  EXPECT_EQ(kMaskRankOrState, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.state = kChance;
  EXPECT_EQ(kMaskRankOrState, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.state = kBasePosition;
  // 593430's line test: already past the opponents' line.
  w.p[kRunner].pos = {26.0f, 0.0f};
  EXPECT_EQ(kMaskOnTheLine, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.p[kRunner].pos = {10.0f, 0.0f};
  // The slot mask.
  EXPECT_EQ(kMaskSlotMask, ChanceMaskReject(k, w, kRunner, 0u, 2, false, 50.0f, true));
  // The depth limit: past F8 (41.56 m), with the line pushed far enough ahead
  // that the line test does not fire first.
  w.lineX[1] = 60.0f;
  w.p[kRunner].pos = {44.0f, 0.0f};
  EXPECT_EQ(kMaskDepth, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.lineX[1] = 25.0f;
  w.p[kRunner].pos = {10.0f, 0.0f};
  // The attack level's class skip: level 0 skips classes 0..4.
  r.roleClass = 2;  // a full back
  EXPECT_EQ(kMaskAttackLevel, ChanceMaskReject(k, w, kRunner, ~0u, 0, false, 50.0f, true));
  r.roleClass = 9;
  // |dz| past 42 m.
  w.p[kRunner].pos = {10.0f, 43.0f};
  EXPECT_EQ(kMaskDz, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // Inside the box zone (past F6 - 3, within F5 + 1.5 of the centre) an
  // opponent within 6.5 m is required; z = 8 keeps the default defenders' z
  // ladder (steps of 3 from -30) clear of him and stays inside F5 + 1.5.
  w.p[kRunner].pos = {40.0f, 8.0f};
  w.lineX[1] = 60.0f;
  EXPECT_EQ(kMaskBoxZone, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // The goal-area gate (s·x > F4) sits behind the box-zone gate, so it is only
  // reached once the opponent is there: with one beside a runner at x = 48 the
  // box zone lets him through and the goal-area gate stops him.
  w.p[kSlots + 4].pos = {42.0f, 8.0f};
  w.p[kRunner].pos = {48.0f, 8.0f};
  EXPECT_EQ(kMaskGoalArea, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.lineX[1] = 25.0f;
  w.p[kRunner].pos = {10.0f, 0.0f};
  // FUN_140596240: a runner of class < 3 may not run when a teammate of a
  // lower class is already running and no further from the ball than him.
  w.p[5].roleClass = 1;   // a CB
  w.p[5].pos = {9.0f, 0.0f};
  w.p[5].state = kChance;
  r.roleClass = 2;
  EXPECT_EQ(kMaskRoleClass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.roleClass = 4;        // class 4 and up are exempt
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  w.p[5].state = kBasePosition;
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.roleClass = 4;
  // TeamMayRun: in the own half with young possession and no free
  // forward-facing focus player, the team may not start Chance runs.
  w.carrier = -1;
  w.receiver[0] = 1;
  w.p[1].pos = {11.0f, 0.0f};
  w.possessionAge[0] = 100.0f;
  w.p[1].vel = {0.0f, 0.0f};
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  // Old possession (or a free forward-facing focus player) opens it, and then
  // the generic tail - no carrier and a receiver far from the ball - is what
  // rejects him.
  w.possessionAge[0] = 1e6f;
  EXPECT_EQ(kMaskGeneric, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
}

// The reach path is separate because its rejection depends on the runner's
// distance to the ball and the attack direction: a defender needs it, a
// forward does not, and the reach limit grows with Build Up Long.
TEST(OffBallMaskTrace, TheClassifierNamesTheReachRejection) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.pos = {30.0f, 0.0f};          // 30 m ahead of the ball
  r.aim = {40.0f, 0.0f};
  r.roleClass = 1;                 // a CB: the reach path applies
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  w.lineX[1] = 45.0f;             // keeps him onside
  w.carrier = 0;
  EXPECT_EQ(kMaskReach, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  r.roleClass = 9;                // a CF: no reach path
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // A CB at 19 m: past the 15 m base, inside Build Up Long's 21 m.
  r.roleClass = 1;
  r.pos = {19.0f, 0.0f};
  EXPECT_EQ(kMaskReach, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, true, 50.0f, true));
}

// Step 2 of the manager tick (off-ball 1.2): A[s] = int(fps·2+0.5) frames when
// a slot newly enters a run state. PES reads it in the SecondLine mask only
// (off-ball 2.3, b4a.c:1204) - the Chance mask's raw body never touches ctx+0x38
// - so this predicate is SecondLine-shaped and the Chance path must not use it.
// It is tested here for the SecondLine port that will consume it.
TEST(OffBallMaskTrace, TheRerunBlockIsASecondLineGateNotAChanceOne) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;
  r.pos = {10.0f, 0.0f};
  r.aim = {20.0f, 0.0f};
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  w.lineX[1] = 25.0f;
  w.carrier = 0;
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // On cooldown the Chance mask must NOT stop him: the block is SecondLine's.
  r.runCooldown = 1;
  EXPECT_TRUE(RunCooldownBlocks(w, kRunner));
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  EXPECT_TRUE(ChanceMaskBit(k, w, kRunner, ~0u, 2, false, 50.0f, true));
  // Counted down to zero the predicate clears either way.
  r.runCooldown = 0;
  EXPECT_FALSE(RunCooldownBlocks(w, kRunner));
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, ~0u, 2, false, 50.0f, true));
}

// The per-team head (off-ball 2.1): T+0x255 (in possession) and a valid
// receiver/carrier are the same answer for every slot of the team in a tick, so
// the caller asks them once. The returned reason is what the histogram files
// when the mask is not walked at all.
TEST(OffBallMaskTrace, TheTeamHeadIsAskedOnceAndNamesItsReason) {
  RunWorld w = Running();
  // The fixture's team 0 is in open play, in possession, with a carrier.
  EXPECT_EQ(kMaskPass, TeamChanceMayRunReject(w, 0));
  EXPECT_TRUE(TeamChanceMayRun(w, 0));
  // Open play is the caller's own guard (MI+0x116c), not re-tested here: the
  // per-team head is about possession and the receiver only.
  // Not in possession.
  w.possession = {false, true};
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  w.possession = {true, false};
  // With no carrier, FUN_140593290 falls back to the focus player: a valid one
  // within 625 m2 of the ball keeps the team eligible, and none at all is the
  // same team-may-run refusal - PES has no separate "no receiver" head.
  w.carrier = -1;
  EXPECT_EQ(kMaskPass, TeamChanceMayRunReject(w, 0));  // w.receiver[0] = 0 is valid
  w.receiver[0] = -1;
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  w.receiver[0] = 0;
  // The other team, which holds no possession in the fixture.
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 1));
}

// FUN_1405938a0 is the Chance mask's *team head*: the raw calls it once per
// FUN_140606cd0, before that function's own 11-slot loop (OBS mask.c:86 with
// the loop at mask.c:197-204). So its verdict is one value shared by every slot,
// and the per-slot function receives it rather than recomputing it.
TEST(OffBallMaskTrace, TheTeamHeadIsSharedByEverySlot) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;
  r.pos = {10.0f, 0.0f};
  r.aim = {20.0f, 0.0f};
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  w.lineX[1] = 25.0f;
  w.carrier = 0;
  const uint32_t all = ~0u;
  // The shared verdict is true: every slot is judged on its own gates.
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, kRunner, all, 2, false, 50.0f, true));
  // With the verdict false the mask is not walked for ANY slot that reaches the
  // team gate at all: the carrier, the receiver and the keeper are caught by
  // earlier gates, and whoever is left is refused for the team's reason.
  for (int id = 0; id < 2 * kSlots; ++id) {
    const bool earlier = w.carrier == id || w.receiver[id >= kSlots ? 1 : 0] == id ||
                         w.p[id].roleClass == 0;
    if (earlier) continue;
    EXPECT_EQ(kMaskTeamMayNotRun, ChanceMaskReject(k, w, id, all, 2, false, 50.0f, false))
        << "slot " << id;
  }
  // The verdict itself follows FUN_1405938a0's own terms.
  w.possession = {false, true};
  EXPECT_FALSE(TeamChanceMayRun(w, 0));
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  // A receiver too far from the ball fails FUN_140593290's 625 m2 fallback the
  // same way, which is why there is no separate no-receiver reason.
  w.possession = {true, false};
  w.carrier = -1;
  w.receiver[0] = 2;
  w.p[2].pos = {40.0f, 0.0f};
  EXPECT_FALSE(TeamChanceMayRun(w, 0));
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  // The end band (0x142588210 = 10 m): a ball deep in the attacking third but
  // wide of the box is refused, while the same ball inside the box width is not.
  w.carrier = 0;
  w.receiver[0] = 0;
  w.possessionAge[0] = 1e6f;
  w.p[0].pos = w.ball = {45.0f, 25.0f};   // past F0 - 10 and beyond F7
  EXPECT_FALSE(TeamChanceMayRun(w, 0));
  EXPECT_EQ(kMaskTeamMayNotRun, TeamChanceMayRunReject(w, 0));
  w.p[0].pos = w.ball = {45.0f, 15.0f};   // inside the box width: allowed
  EXPECT_TRUE(TeamChanceMayRun(w, 0));
  EXPECT_EQ(kMaskPass, TeamChanceMayRunReject(w, 0));
}

// FUN_140593430's head (raw 0x140593430-0x1405934d4) tests the carrier first
// and only falls back to the holder (MA+0x2483c) when the carrier is invalid, so
// a valid carrier does not also veto the fallback player.
TEST(OffBallMaskTrace, TheCarrierGateDoesNotAlsoVetoTheFallbackPlayer) {
  const OffBallConstants k = Synthetic();
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;
  r.pos = {10.0f, 0.0f};
  r.aim = {20.0f, 0.0f};
  w.p[0].pos = w.ball = {0.0f, 0.0f};
  w.lineX[1] = 25.0f;
  // With a valid carrier, only that carrier is vetoed: the fallback player
  // (w.receiver, which BuildRunWorld sets to the last toucher when nobody holds)
  // stays a candidate.
  for (int i = 0; i < kSlots; ++i) w.p[i].roleClass = 9;  // no keepers in this fixture
  w.carrier = 0;
  w.receiver[0] = 1;
  EXPECT_EQ(kMaskCarrierOrReceiver, ChanceMaskReject(k, w, 0, ~0u, 2, false, 50.0f, true));
  EXPECT_EQ(kMaskPass, ChanceMaskReject(k, w, 1, ~0u, 2, false, 50.0f, true));
  // With no carrier, the fallback player is vetoed as well.
  w.carrier = -1;
  EXPECT_EQ(kMaskCarrierOrReceiver, ChanceMaskReject(k, w, 1, ~0u, 2, false, 50.0f, true));
  // The keeper is excluded either way.
  w.receiver[0] = -1;
  w.p[2].roleClass = 0;
  EXPECT_EQ(kMaskCarrierOrReceiver, ChanceMaskReject(k, w, 2, ~0u, 2, false, 50.0f, true));
}

// FUN_140608ac0 (raw 0x140608ac0-0x140608b33), the just-played-pass-to-me lock
// PES applies between FUN_140593430 and the T+0xb8b8 slot mask. Its three
// decodable conditions are all observable in GF; the fourth, the slot's
// team-concept entry (FUN_140a64030 kind 0x12), is not, because GF carries no
// concept table. So a pass to this slot inside half a second reads as UNKNOWN
// rather than a guessed yes or no, and every other case reads as no.
TEST(OffBallCorridor, APassToMeInsideHalfASecondIsSeenButNotDecidable) {
  RunWorld w = Running();
  PlayerFacts& r = w.p[kRunner];
  r.state = kBasePosition;
  r.t = 1e9f;
  r.roleClass = 9;
  r.pos = {10.0f, 0.0f};
  r.aim = {20.0f, 0.0f};
  // No event at all: no lock.
  EXPECT_EQ(kPassToMeNo, PassToMeLock(w, kRunner));
  // A ground pass (kind 1) to this runner, 10 frames old: the decodable part
  // holds, so the result is the concept term's unknown.
  w.ev = {true, 1, 10.0f, {0.0f, 0.0f}, kRunner};
  EXPECT_EQ(kPassToMeUnknown, PassToMeLock(w, kRunner));
  // A through pass (kind 2) is not the lock's kind: no.
  w.ev.kind = 2;
  EXPECT_EQ(kPassToMeNo, PassToMeLock(w, kRunner));
  w.ev.kind = 1;
  // Aged past int(fps·0.5+0.5) = 28 frames: no.
  w.ev.age = 40.0f;
  EXPECT_EQ(kPassToMeNo, PassToMeLock(w, kRunner));
  // A pass to somebody else: no.
  w.ev.age = 10.0f;
  w.ev.target = 7;
  EXPECT_EQ(kPassToMeNo, PassToMeLock(w, kRunner));
  // Back to this runner: unknown again.
  w.ev.target = kRunner;
  EXPECT_EQ(kPassToMeUnknown, PassToMeLock(w, kRunner));
}

// PES's nearest lists (the 200-byte entries at MA+0x22e4c + 200·i). Team 0 is ids
// 0..10, team 1 is 11..21. Each entry sorts both teams by squared distance from
// its owner, the owner's own team included.
RunWorld Spread() {
  RunWorld w;
  for (int i = 0; i < 2 * kSlots; ++i) {
    w.p[i].active = true;
    w.p[i].pos = {50.0f, 50.0f + static_cast<float>(i)};
  }
  w.p[0].pos = {0.0f, 0.0f};
  return w;
}

TEST(NearestList, OpponentsAreSortedNearestFirst) {
  RunWorld w = Spread();
  const float d[kSlots] = {7.0f, 3.0f, 10.0f, 1.0f, 5.0f, 9.0f, 2.0f, 8.0f, 4.0f, 6.0f, 11.0f};
  for (int s = 0; s < kSlots; ++s) w.p[kSlots + s].pos = {0.0f, d[s]};
  BuildNearest(w);
  const std::vector<int> want{14, 17, 12, 19, 15, 20, 11, 18, 16, 13, 21};
  std::vector<int> got;
  for (const auto& n : NearestOpponents(w, 0)) got.push_back(static_cast<int>(n.id));
  EXPECT_EQ(want, got);
  ASSERT_FALSE(NearestOpponents(w, 0).empty());
  EXPECT_FLOAT_EQ(NearestOpponents(w, 0)[0].d2, 1.0f);
}

TEST(NearestList, EqualDistancesKeepSlotOrder) {
  RunWorld w = Spread();
  for (int s = 0; s < kSlots; ++s) w.p[kSlots + s].pos = {0.0f, 10.0f + static_cast<float>(s)};
  w.p[kSlots + 2].pos = {0.0f, 4.0f};
  w.p[kSlots + 7].pos = {-4.0f, 0.0f};
  w.p[kSlots + 9].pos = {0.0f, -4.0f};
  BuildNearest(w);
  const std::vector<int> want{13, 18, 20, 11, 12, 14, 15, 16, 17, 19, 21};
  std::vector<int> got;
  for (const auto& n : NearestOpponents(w, 0)) got.push_back(static_cast<int>(n.id));
  EXPECT_EQ(want, got);
}

TEST(NearestList, CountsHoldOnlyTheActivePlayersOfEachTeam) {
  RunWorld w = Spread();
  w.p[kSlots + 4].active = false;
  w.p[kSlots + 6].active = false;
  BuildNearest(w);
  const auto opp = NearestOpponents(w, 0);
  EXPECT_EQ(9u, opp.size());
  for (const auto& n : opp) EXPECT_TRUE(w.p[n.id].active);
  const pes::SortedRow& nl = w.nearestRows[0]->nearest[0];
  EXPECT_EQ(kSlots - 1, nl.Count(0));  // own block excludes the owner (pes-re-teamtick.md §3.2)
  EXPECT_EQ(9, nl.Count(1));
}

TEST(NearestList, RanksGiveTheSlotWithinItsTeamsListAndAbsentForInactive) {
  RunWorld w = Spread();
  w.p[kSlots + 4].active = false;
  BuildNearest(w);
  const auto opp = NearestOpponents(w, 0);
  const pes::SortedRow& nl = w.nearestRows[0]->nearest[0];
  for (int k = 0; k < static_cast<int>(opp.size()); ++k) EXPECT_EQ(k, nl.RankOf(opp[k].id));
  EXPECT_EQ(-1, nl.RankOf(kSlots + 4));  // inactive: absent from the list
  EXPECT_EQ(-1, nl.RankOf(0));           // the owner is absent from his own block
}

TEST(NearestList, ATeamOneOwnerListsTeamZeroAsOpponents) {
  RunWorld w = Spread();
  w.p[kSlots + 3].pos = {0.0f, 0.0f};
  for (int s = 0; s < kSlots; ++s) w.p[s].pos = {0.0f, 13.0f - static_cast<float>(s)};
  BuildNearest(w);
  const std::vector<int> want{10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  std::vector<int> got;
  for (const auto& n : NearestOpponents(w, kSlots + 3)) got.push_back(static_cast<int>(n.id));
  EXPECT_EQ(want, got);
}

TEST(NearestList, OwnersOwnTeamBlockExcludesTheOwner) {
  // pes-re-teamtick.md §3.2, CONFIRMED live: self excluded (owner 0: rank[0] = 10).
  RunWorld w = Spread();
  BuildNearest(w);
  const pes::SortedRow& nl = w.nearestRows[0]->nearest[0];
  for (int k = 0; k < nl.Count(0); ++k) EXPECT_NE(nl.Entry(0, k).id, 0);
  EXPECT_EQ(nl.Count(0), kSlots - 1);
  EXPECT_EQ(-1, nl.RankOf(0));
  // The opponents' block is untouched: all 11 opponents listed.
  EXPECT_EQ(nl.Count(1), kSlots);
}


TEST(OffBallInstall, CapsAndLimitFromTheInstall) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto k = LoadOffBallConstants(*exe, vfs, &err);
  ASSERT_TRUE(k) << err;
  for (int r = 0; r < kCapRunCount; ++r)
    for (int l = 1; l < kAttackLevels; ++l) EXPECT_GE(k->offerNum[r][l], k->offerNum[r][l - 1]);
  EXPECT_GT(k->chanceScoreLimit, 0);
  EXPECT_LE(k->chanceScoreLimit, 255);
  EXPECT_GT(k->reachCap_m, k->reachBase_m);
  EXPECT_GT(k->awareTo, k->awareFrom);
  EXPECT_EQ(k->laneWindow, 8);
  EXPECT_FLOAT_EQ(k->styleRunDepth_m, 5.0f);  // 0x14259222c, FUN_1405d8b70 L650-657
}

// FUN_1405d8b70 L650-657: a Goal Poacher (bit 0) or a Fox in the Box (bit 2) in a
// forward role (PES kinds 7-9), neither carrying the ball nor in a run state, has his
// run target pushed 5 m toward the attack. The style must be on the player AND listed
// in his role's compat row (FUN_1408cb0b0). The rows below are synthetic.
pes::style::Compat StyleRows() {
  pes::style::Compat c{};
  c.row[5] = (1u << 0) | (1u << 2);
  c.row[6] = (1u << 0) | (1u << 2);
  c.row[7] = (1u << 0) | (1u << 4) | (1u << 8);  // lists the Poacher, not the Fox
  c.row[8] = (1u << 4) | (1u << 5) | (1u << 6);  // lists neither
  c.row[9] = (1u << 0) | (1u << 2);              // lists both
  return c;
}
OffBallConstants WithStyles() {
  OffBallConstants k = Synthetic();
  k.styleCompat = StyleRows();
  k.styleRunDepth_m = 5.0f;
  return k;
}
const uint32_t kPoacherWord = pes::style::StyleWord(PlayingStyles::Player::GoalPoacher);
const uint32_t kFoxWord = pes::style::StyleWord(PlayingStyles::Player::FoxInTheBox);

TEST(OffBallStyleRunDepth, GoalPoacherInACentreForwardRoleGetsFiveMetres) {
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 9, kPoacherWord, kNothing, false), 5.0f);
}
TEST(OffBallStyleRunDepth, FoxInTheBoxInACentreForwardRoleGetsFiveMetres) {
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 9, kFoxWord, kNothing, false), 5.0f);
}
TEST(OffBallStyleRunDepth, CompatRowDecidesWhichForwardRoleKeepsTheStyle) {
  const OffBallConstants k = WithStyles();
  EXPECT_FLOAT_EQ(StyleRunDepth(k, 7, kPoacherWord, kNothing, false), 5.0f);
  EXPECT_FLOAT_EQ(StyleRunDepth(k, 7, kFoxWord, kNothing, false), 0.0f);
  EXPECT_FLOAT_EQ(StyleRunDepth(k, 8, kPoacherWord, kNothing, false), 0.0f);
}
TEST(OffBallStyleRunDepth, CarrierGetsNoRunTerm) {
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 9, kPoacherWord, kNothing, true), 0.0f);
}
TEST(OffBallStyleRunDepth, ARunningPlayerGetsNoRunTerm) {
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 9, kPoacherWord, kChance, false), 0.0f);
}
TEST(OffBallStyleRunDepth, AStandingPlayerOfTheSameStyleStillGetsIt) {
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 9, kPoacherWord, kBasePosition, false), 5.0f);
}
TEST(OffBallStyleRunDepth, NoStyleNoTerm) {
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 9, 0u, kNothing, false), 0.0f);
}
TEST(OffBallStyleRunDepth, RolesFiveAndSixWaitForTheUndecodedTeamByte) {
  // ponytail: roles 5-6 are gated on team+0x7b84 (FUN_1405d8b70 L650-653), not decoded
  // yet; until it is, they get no term. Remove this when team+0x7b84 is ported.
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 5, kFoxWord, kNothing, false), 0.0f);
  EXPECT_FLOAT_EQ(StyleRunDepth(WithStyles(), 6, kPoacherWord, kNothing, false), 0.0f);
}

// T1a: the offside-trap arm (FUN_1404F6720, step 5, 0x1404F6720-0x1404F6BC2). The carrier is team 1's slot 0 (id 11)
// at (2, 0), facing -x and standing still. Team 0, the trapping side, has its nearest man (id 0) 1 m in front of him.
// Team 1's designated man (id 12) is his most advanced player and within 3 m. Every other man is out of the cones the
// chain tests. Each scene passes the chain up to the one gate it names. The 10 m line gate, the 4 m line gate and the
// 20 m^2 designated-man gate cannot all pass: the first two differ by (D.x - c.x) * s, and the third caps |D.x - c.x|
// below 4.5 m. So the arm is unreachable; these scenes check the gates that stop it.
RunWorld TrapScene(float trapLine_m, Vec2f designated) {
  RunWorld w;
  for (int i = 0; i < 2 * kSlots; ++i) w.p[i].active = true;
  w.p[0].pos = {1.0f, 0.0f};
  static const float kFarZ[] = {-30.0f, -25.0f, -20.0f, -15.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f, 15.0f};
  for (int k = 1; k < kSlots; ++k) w.p[k].pos = {-50.0f, kFarZ[k - 1]};
  w.p[kSlots].pos = {2.0f, 0.0f};
  w.p[kSlots].facing = 270.0f;
  w.p[kSlots + 1].pos = designated;
  static const float kBackZ[] = {-20.0f, -15.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f, 15.0f, 20.0f};
  for (int k = 2; k < kSlots; ++k) w.p[kSlots + k].pos = {12.0f, kBackZ[k - 2]};
  w.carrier = kSlots;
  w.lineX = {trapLine_m, 0.0f};
  BuildNearest(w);
  return w;
}

TEST(OffBallTrap, TrapArmedOnlyWhenFullPassChain) {
  // 0x1404F68E4 (10 m): the carrier is 5 m short of the trapping line, not 10 m behind it.
  EXPECT_FALSE(OffsideTrapArmed(TrapScene(-3.0f, {-0.598f, 1.5f}), 0));
  // 0x1404F6A5B (4 m): the trapping line is 9.4 m beyond the designated man.
  EXPECT_FALSE(OffsideTrapArmed(TrapScene(-10.0f, {-0.598f, 1.5f}), 0));
  // 0x1404F6A45 (20 m^2): the designated man is 8 m away (64 m^2) and both line gates pass.
  EXPECT_FALSE(OffsideTrapArmed(TrapScene(-8.5f, {-4.928f, 4.0f}), 0));
}

// T7a walks NearestList block `side` (0x1404F63E0: entry side*11+k over its eleven entries). A holder on that side
// has his own team's list in that block, so the mark walk reads his mates. NearestOpponents keeps the other block.
TEST(NearestTeamBlock, TheMarkingSideBlockIsThatSidesListWhoeverHolds) {
  pes::ScoreRows rows;
  // Holder: global 2 (team 0, lane 2). Mates at lanes 0, 1, 3; opponents at lanes 11, 12.
  pes::SortedRow& row = rows.nearest[2];
  row.Add(0, 4.0f);
  row.Add(1, 9.0f);
  row.Add(3, 1.0f);
  row.Add(11, 2.0f);
  row.Add(12, 16.0f);
  row.Seal();
  pes::offball::RunWorld w;
  w.nearestRows[0] = &rows;
  const auto mates = pes::offball::NearestTeamBlock(w, 2, 0);
  ASSERT_EQ(mates.size(), 3u);
  EXPECT_EQ(mates[0].id, 3);
  EXPECT_EQ(mates[1].id, 0);
  EXPECT_EQ(mates[2].id, 1);
  const auto opponents = pes::offball::NearestTeamBlock(w, 2, 1);
  ASSERT_EQ(opponents.size(), 2u);
  EXPECT_EQ(opponents[0].id, 11);
  EXPECT_EQ(opponents[1].id, 12);
  EXPECT_EQ(pes::offball::NearestOpponents(w, 2).size(), opponents.size());
}

// PES's man record holds the run code's kinematics (pesworldrecord.hpp): position in m (+0x55C, +0x564), velocity in
// m per frame (+0x574, +0x57C) and facing in degrees (+0x554). A fact reads them in place; its velocity is in m/s.
TEST(OffBallRecord, FactsReadTheirManRecord) {
  pes::PesWorld world;
  PlayerFacts f;
  f.man = world.Block(4) + pes::kManRecordOffset;
  pes::StoreF32(f.man, pes::kManPosXOffset, 12.5f);
  pes::StoreF32(f.man, pes::kManPosZOffset, -7.0f);
  pes::StoreF32(f.man, pes::kManVelXOffset, 0.25f);
  pes::StoreF32(f.man, pes::kManVelZOffset, -0.5f);
  pes::StoreF32(f.man, pes::kManFacingOffset, 90.0f);
  EXPECT_FLOAT_EQ(f.pos().x, 12.5f);
  EXPECT_FLOAT_EQ(f.pos().z, -7.0f);
  EXPECT_FLOAT_EQ(f.vel().x, 0.25f * kPesFps);
  EXPECT_FLOAT_EQ(f.vel().z, -0.5f * kPesFps);
  EXPECT_FLOAT_EQ(f.facing(), 90.0f);
}

// A player with no record reads zeros.
TEST(OffBallRecord, WithoutARecordTheKinematicsReadZero) {
  const PlayerFacts none;
  EXPECT_EQ(none.pos().x, 0.0f);
  EXPECT_EQ(none.pos().z, 0.0f);
  EXPECT_EQ(none.vel().x, 0.0f);
  EXPECT_EQ(none.facing(), 0.0f);
}

// The setters write PES's units: velocity in m per frame, so 5.4 m/s at 54 frames a second is 0.1 m a frame.
TEST(OffBallRecord, SettersWriteTheManRecordInPesUnits) {
  pes::PesWorld world;
  PlayerFacts f;
  f.man = world.Block(4) + pes::kManRecordOffset;
  f.SetPos({3.0f, 4.0f});
  f.SetVel({5.4f, -2.7f});
  f.SetFacing(270.0f);
  EXPECT_FLOAT_EQ(pes::LoadF32(f.man, pes::kManPosXOffset), 3.0f);
  EXPECT_FLOAT_EQ(pes::LoadF32(f.man, pes::kManPosZOffset), 4.0f);
  EXPECT_NEAR(pes::LoadF32(f.man, pes::kManVelXOffset), 0.1f, 1e-6f);
  EXPECT_NEAR(pes::LoadF32(f.man, pes::kManVelZOffset), -0.05f, 1e-6f);
  EXPECT_FLOAT_EQ(pes::LoadF32(f.man, pes::kManFacingOffset), 270.0f);
  EXPECT_NEAR(f.vel().x, 5.4f, 1e-4f);
  EXPECT_NEAR(f.vel().z, -2.7f, 1e-4f);
}

}  // namespace
