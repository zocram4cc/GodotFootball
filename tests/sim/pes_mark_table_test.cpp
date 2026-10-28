// PES 2021's mark table (team-tick step 35, FUN_1404F61E0): the 11-byte opponent mask
// T+0xB7EC[11]. Pure behaviour tests with synthetic men; no install needed except
// the constants check. MarkGateRE's four open inputs arrive as Facts booleans.
#include <gtest/gtest.h>

#include <cstdlib>
#include <cmath>
#include <utility>
#include <string>

#include "onthepitch/pesmarktable.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"
#include <memory>
#include <vector>

#include "base/properties.hpp"
#include "data/matchdata.hpp"
#include "gamedefines.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "onthepitch/pesscorerows.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/vfs.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"
#include "rig/rig.hpp"

#include "pes_match_constants.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

using pes::marktable::Candidate;
using pes::marktable::Constants;
using pes::marktable::Facts;
using pes::marktable::MarkState;

// The men's records the mark table reads: one world for the file, each man placed by slot. Slot 5 is the holder, slot 7
// the candidate, slots 12 and up the holder-side mates.
pes::PesWorld& World() {
  static pes::PesWorld world;
  return world;
}

// Places a man at (x, z) in his record and returns the record.
uint8_t* Man(int slot, float x, float z) {
  World().BindSlot(slot);
  uint8_t* man = World().ManRecord(slot);
  pes::StoreF32(man, pes::kManPosXOffset, x);
  pes::StoreF32(man, pes::kManPosZOffset, z);
  return man;
}

Constants TestConstants() {
  Constants k;
  k.rampBase = 7.0f;
  k.rampTop = 3.0f;
  k.coneOffset = 15.0f;
  k.ceilD2 = 1369.0f;
  k.wrapCap = 105.0f;
  k.wrapSide = 80.0f;
  k.coneDeg = 90.0f;
  k.coneLen = 12.0f;
  k.gateFrac = 64.0f;
  k.gateRadius = 12.0f;
  k.gateWidth = 2.0f;
  k.half = 0.5f;
  return k;
}

// A passing scene: side 0 marks slot 3, holder is slot 5, one candidate at slot 7 with
// bearings that clear the 105/80 wrap gates. The ramp wind puts the cone at 0.5 - 15.
Facts PassingFacts() {
  Facts f;
  f.world = &World();
  f.side = 0;
  f.holder = 5;
  f.sign = 1.0f;
  f.holderMan = Man(5, 14.6984631f, 1.7101007f);  // bearing(cand -> holder) = 70 deg, agreeing with pairToHolder
  f.goalAreaHalfWidth_m = 9.16f;  // F[5]: lo = 9.16 - 7 = 2.16, ramp (0 - 2.16 + 3)/3 > 1 -> cone = 1 - 15
  Candidate c;
  c.id = 7;
  c.d2 = 100.0f;
  c.actionWord = 1;
  c.man = Man(7, 10.0f, 0.0f);  // side line at cone + holderX = 0.70 < 10 passes
  // Facing 0 (the wrap tests' subtrahend): |10 - 0| = 10 <= 105, |70 - 0| = 70 <= 80, floor |70 + 14| = 84 >= 80.
  c.pairToTarget = 10.0f;
  c.pairToHolder = 70.0f;
  c.head = 0.0f;  // the facing word [cand+0x554]: the wrap tests' subtrahend
  f.candidates[0] = c;
  return f;
}
// RED (step 1): the team tick must rebuild marks on the PES path (T7a
// FUN_1404F61E0 at team-tick `0x1404F76AC`), so that the readers (R1/R2/R3)
// see live marks instead of a zeroed table. A live PES-loop match must show a
// marked slot on some tick (the pre-wiring run left every mark zero; a live
// table proves the builder call is wired to the run world).
TEST(MarkTableTick, LiveMatchTickSetsAMark) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the tick feed runs a live match";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql"));
  SetDB(&db);
  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = 1;
  setup->awayTeamDbId = 2;
  setup->seed = 7;
  setup->durationMinutes = 2;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";
  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(1, 2), *setup, config);
  bool marked = false;
  for (unsigned long t = 0; !match.IsGameOver() && t < 6ul * 60ul * 54ul && !marked; ++t) {
    match.Process();
    for (int team = 0; team < 2 && !marked; ++team) {
      std::vector<Player*> players;
      match.GetTeam(team)->GetActivePlayers(players);
      for (Player* p : players) {
        if (match.GetTeam(team)->GetController()->IsMarkedByTable(p)) {
          marked = true;
          break;
        }
      }
    }
  }
  EXPECT_TRUE(marked);
}

TEST(MarkTable, ZeroedOnEntry) {
  MarkState s;
  s.markSlot = 3;
  s.mark.fill(1);
  Facts f = PassingFacts();
  f.holder = 0xFF;  // gated out: mask keeps only the zero-init
  pes::marktable::BuildMarkTable(&s, f, TestConstants());
  for (uint8_t b : s.mark) EXPECT_EQ(b, 0);
}

TEST(MarkTable, EmptySlotMarksNothing) {
  MarkState s;  // markSlot = 0xFF
  Facts f = PassingFacts();
  pes::marktable::BuildMarkTable(&s, f, TestConstants());
  for (uint8_t b : s.mark) EXPECT_EQ(b, 0);
}

TEST(MarkTable, HolderKeeperOrTargetMarksNothing) {
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.holderIsKeeper = true;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
  f = PassingFacts();
  f.holder = 3;  // == tgt (side 0 * 11 + 3)
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

TEST(MarkTable, MarksIdModElevenUpToThree) {
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 9;  // tgt 9: none of the ids 1..5 is skipped
  Facts f = PassingFacts();
  for (int i = 0; i < 5; ++i) {
    f.candidates[i].id = i + 1;
    f.candidates[i].d2 = 100.0f;
    f.candidates[i].actionWord = 1;
    f.candidates[i].man = Man(6 + i, 10.0f, 0.0f);
    f.candidates[i].pairToTarget = 10.0f;
    f.candidates[i].pairToHolder = 70.0f;
    f.candidates[i].head = 0.0f;
  }
  pes::marktable::BuildMarkTable(&s, f, k);
  int total = 0;
  for (uint8_t b : s.mark) total += b;
  EXPECT_EQ(total, 3);
  EXPECT_EQ(s.mark[1], 1);
  EXPECT_EQ(s.mark[2], 1);
  EXPECT_EQ(s.mark[3], 1);
  EXPECT_EQ(s.mark[4], 0) << "the fourth passing candidate stops at the cap";
  EXPECT_EQ(s.mark[5], 0);
}

TEST(MarkTable, GlobalIdFoldsModEleven) {
  // The commit index is id % 11 (0x1404F65AA MUL 0xBA2E8BA3): id 12 commits slot 1.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 9;
  Facts f = PassingFacts();
  f.candidates[0].id = 12;
  f.candidates[0].d2 = 100.0f;
  f.candidates[0].actionWord = 1;
  f.candidates[0].pairToTarget = 10.0f;
  f.candidates[0].pairToHolder = 70.0f;
  f.candidates[0].head = 0.0f;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[1], 1);
  EXPECT_EQ(s.mark[0], 0);
}

TEST(MarkTable, FarCandidateHardExits) {
  // d^2 > 1369 returns (0x1404F643D JA), so a later passing candidate is never reached.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.candidates[0].id = 1;
  f.candidates[0].d2 = 2000.0f;
  f.candidates[1].id = 2;
  f.candidates[1].d2 = 100.0f;
  f.candidates[1].actionWord = 1;
  f.candidates[1].man = Man(8, 10.0f, 0.0f);
  f.candidates[1].pairToTarget = 10.0f;
  f.candidates[1].pairToHolder = 70.0f;
  f.candidates[1].head = 0.0f;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[2], 0);
}

TEST(MarkTable, WrapBoundaryPasses) {
  // 0x1404F64D9 JA: d = 105.0 exactly does not skip (strictly-greater exit).
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 9;
  Facts f = PassingFacts();
  f.candidates[0].pairToTarget = 105.0f;
  f.candidates[0].pairToHolder = 70.0f;
  f.candidates[0].head = 0.0f;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 1);
}

TEST(MarkTable, SideGateRejectsFarSide) {
  // FUN_140A4C720: (sign*cone + holderX)*sign < sign*candX, strict. Cone is -14 here,
  // so the line is at 0.70: a candidate at x = 0 fails, one at 10 passes.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.candidates[0].man = Man(7, 0.0f, 0.0f);
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
  f = PassingFacts();
  f.candidates[0].man = Man(7, 10.0f, 0.0f);
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 1);
  // Edge note: exactly on the line is |cone| >= 14 m from the holder, outside the 12 m
  // cone, so the cone subsumes the side edge (mutation >= to > survives; PES JA confirmed
  // at 0x1404F647C/47E by asm).
}

TEST(MarkTable, ActionWordRejects) {
  // pl+0x5D0 admits iff 1 or 0x3F (0x1404F6484-492).
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.candidates[0].actionWord = 2;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
  f = PassingFacts();
  f.candidates[0].actionWord = 0x3F;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 1);
}

TEST(MarkTable, ConeAllowsOneMateInside) {
  // count = 1, cone passes: 1 <= 1 commits with one agreeing mate nearby.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.gateMan[0] = Man(12, 19.3969269f, 3.4202013f);
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 1);
}

TEST(MarkTable, ConeRejectsOutside) {
  // FUN_140A53DF0: touching the 12 m circle fails (JBE). Candidate at d2 = 144 exactly.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  // Exactly 12 m from the holder (f32 d2 == 144.0): the JBE arm fails the touch.
  // live = 120 agrees with pairToHolder = 120, so only the annulus rejects.
  // With the agreeing mate (count = 1) and coneHit = false, 1 > 0 rejects.
  f.gateMan[0] = Man(12, 19.0285912f, -0.7898993f);
  f.candidates[0].man = Man(7, 4.3061590f, 7.7101007f);
  f.candidates[0].d2 = 100.0f;
  f.candidates[0].pairToHolder = 120.0f;
  f.candidates[0].head = 100.0f;  // the facing word agrees with the pairs: 10 and 120 clear 105 and 80 around it
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

TEST(MarkTable, CountBandEdgeCounts) {
  // |b - u| one ulp above 32.0 still counts (JA taken only when strictly above, 0x140A4EA45).
  // Cone-edge candidate (coneHit = false) + this single mate: exact 1 > 0 rejects.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.gateMan[0] = Man(12, 17.0458202f, -2.7046375f);
  f.candidates[0].man = Man(7, 4.3061590f, 7.7101007f);
  f.candidates[0].d2 = 100.0f;
  f.candidates[0].pairToHolder = 120.0f;
  f.candidates[0].head = 100.0f;  // the facing word agrees with the pairs (see ConeRejectsOutside)
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

TEST(MarkTable, CountGateRejectsCrowd) {
  // FUN_140A4EAF0: count <= (cone != 0). Two mates within 12 m agreeing with u push count to 2.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.gateMan[0] = Man(12, 14.6984631f + 5.0f, 1.7101007f);
  f.gateMan[1] = Man(13, 19.3969262f, 3.4202014f);
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

TEST(MarkTable, WrapTestsSubtractTheFacingWord) {
  // PES's wrap tests subtract the facing word [cand+0x554] (0x1404F6498 MOVSS into XMM7, read by
  // FUN_14043A0A0 at 0x1404F64C5 and 0x1404F64F9). FUN_1404AB8E0's output (0x1404F64A8) is never read
  // in T7a. Facing 0 against pairToHolder 170 is |170| > 80: rejected. The stored point's bearing (90)
  // would have passed it: |170 - 90| = 80.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.candidates[0].pairToHolder = 170.0f;
  f.candidates[0].head = 0.0f;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

TEST(MarkTable, RampMidPinsConeValue) {
  // holderX = 7.16 is mid-ladder: ramp = (7.16 - 2.16)/10 = 0.5, cone = -14.5.
  // Floor |70 + 14.5| = 84.5 >= 80 passes; without the -15.0 offset it would fail.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.holderMan = Man(5, 7.16f, 1.71f);
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 1);
}

TEST(MarkTable, WrapGatesReject) {
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.candidates[0].pairToTarget = 200.0f;  // |200 - 90| = 110 > 105
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

// The install's constants, read from the exe's .rdata at the VAs the header cites.
TEST(MarkTableInstall, LoadsTheInstallConstants) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the mark constants come from the install";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto loaded = pes::marktable::LoadMarkTableConstants(*exe, &err);
  ASSERT_TRUE(loaded.has_value()) << err;
  const Constants want = TestConstants();
  EXPECT_FLOAT_EQ(loaded->rampBase, want.rampBase);
  EXPECT_FLOAT_EQ(loaded->rampTop, want.rampTop);
  EXPECT_FLOAT_EQ(loaded->coneOffset, want.coneOffset);
  EXPECT_FLOAT_EQ(loaded->ceilD2, want.ceilD2);
  EXPECT_FLOAT_EQ(loaded->wrapCap, want.wrapCap);
  EXPECT_FLOAT_EQ(loaded->wrapSide, want.wrapSide);
  EXPECT_FLOAT_EQ(loaded->coneDeg, want.coneDeg);
  EXPECT_FLOAT_EQ(loaded->coneLen, want.coneLen);
  EXPECT_FLOAT_EQ(loaded->gateRadius, want.gateRadius);
  EXPECT_FLOAT_EQ(loaded->half, want.half);
}

// PES's S1 writer runs at listener step 5, before any team tick, so both sides' score rows are one snapshot of the
// frame start: a side's row for an enemy holds the same squared distance as the enemy's row for him. GF ticked team
// 1's writer after team 0's players had moved in the same tick, so the two rows disagreed. Checked on frames not in
// play at either end, where both writers rewrite every block.
TEST(MarkTableTick, BothSidesRowsAgreeOnTheFrameSnapshot) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the tick feed runs a live match";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql"));
  SetDB(&db);
  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = 1;
  setup->awayTeamDbId = 2;
  setup->seed = 7;
  setup->durationMinutes = 2;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";
  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(1, 2), *setup, config);
  int compared = 0;
  for (unsigned long t = 0; !match.IsGameOver() && t < 2ul * 60ul * 54ul && compared < 2000; ++t) {
    const bool stopped = !match.IsInPlay();
    match.Process();
    if (!stopped || match.IsInPlay()) continue;
    // Lanes: a side's own players at 0..10, the enemy's at 11..21 (TickScoreRows).
    const pes::ScoreRows& home = match.GetTeam(0)->GetController()->Rows();
    const pes::ScoreRows& away = match.GetTeam(1)->GetController()->Rows();
    for (int i = 0; i < 11; ++i) {
      for (int j = 0; j < 11; ++j) {
        const float fromHome = home.nearest[i].ValueOf(11 + j);  // home's writer: home i, away j
        const float fromAway = away.nearest[j].ValueOf(11 + i);  // away's writer: away j, home i
        if (std::isnan(fromHome) || std::isnan(fromAway)) continue;  // not listed: off the pitch
        EXPECT_NEAR(fromHome, fromAway, 1e-3f * (fromHome > 1.0f ? fromHome : 1.0f))
            << "tick " << t << ": home " << i << " against away " << j;
        ++compared;
      }
    }
    if (testing::Test::HasFailure()) break;
  }
  EXPECT_GT(compared, 0);
}

}  // namespace
