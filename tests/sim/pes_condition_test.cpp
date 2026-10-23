// PES 2021's condition arrows (docs/re/pes-re-condition-import.md): the condition byte
// deals the band, FUN_141FC1380's inverse map deals the arrow, no kick-off RNG.
#include <gtest/gtest.h>

#include "data/formstate.hpp"

namespace {

TEST(ConditionBand, LadderEdgesArePESsSevenCompares) {
  // FUN_140AF2DA0: CMP EAX,0x41/JC then 0x46/0x4B/0x50/0x55/0x5A/0x5F.
  EXPECT_EQ(FormState::Band(0x40), 0);
  EXPECT_EQ(FormState::Band(0x41), 1);
  EXPECT_EQ(FormState::Band(0x45), 1);
  EXPECT_EQ(FormState::Band(0x46), 2);
  EXPECT_EQ(FormState::Band(0x4A), 2);
  EXPECT_EQ(FormState::Band(0x4B), 3);
  EXPECT_EQ(FormState::Band(0x4F), 3);
  EXPECT_EQ(FormState::Band(0x50), 4);
  EXPECT_EQ(FormState::Band(0x54), 4);
  EXPECT_EQ(FormState::Band(0x55), 5);
  EXPECT_EQ(FormState::Band(0x59), 5);
  EXPECT_EQ(FormState::Band(0x5A), 6);
  EXPECT_EQ(FormState::Band(0x5E), 6);
  EXPECT_EQ(FormState::Band(0x5F), 7);
  EXPECT_EQ(FormState::Band(0x6D), 7);
}

TEST(ConditionBand, InverseMapIsPESs0to4Table) {
  // FUN_141FC1380: 0->4, 1->3, 2->2, 3->1, 4->0, else 2. GF space is 4 - PES.
  EXPECT_EQ(FormState::ArrowForBand(0), FormState::Arrow::Terrible);
  EXPECT_EQ(FormState::ArrowForBand(1), FormState::Arrow::Poor);
  EXPECT_EQ(FormState::ArrowForBand(2), FormState::Arrow::Normal);
  EXPECT_EQ(FormState::ArrowForBand(3), FormState::Arrow::Great);
  EXPECT_EQ(FormState::ArrowForBand(4), FormState::Arrow::Top);
  EXPECT_EQ(FormState::ArrowForBand(5), FormState::Arrow::Normal);
  EXPECT_EQ(FormState::ArrowForBand(6), FormState::Arrow::Normal);
  EXPECT_EQ(FormState::ArrowForBand(7), FormState::Arrow::Normal);
}

TEST(ConditionBand, DefaultByteIsNormalOnBothSides) {
  // The 0x46 default path (unedited record) lands band 2 -> Normal.
  EXPECT_EQ(FormState::ArrowForConditionByte(0x46), FormState::Arrow::Normal);
}

TEST(ConditionShift, OwnTeamStateThreeForcesIndexZeroGatedByLevelClass) {
  // 0x14092B4D6-0x14092B5DB: own team with T+0xA298 state 3 -> index 0 (PES),
  // i.e. Top in GF space; gated by the /100 == 6 level-class predicate.
  EXPECT_EQ(FormState::ApplyTeamShift(FormState::Arrow::Poor, /*ownTeam=*/true,
                                     /*teamModeThree=*/true, /*levelClassSix=*/false),
            FormState::Arrow::Top);
  // Without the state-3 gate the arrow is untouched.
  EXPECT_EQ(FormState::ApplyTeamShift(FormState::Arrow::Poor, /*ownTeam=*/true,
                                     /*teamModeThree=*/false, /*levelClassSix=*/false),
            FormState::Arrow::Poor);
}

TEST(ConditionShift, OpponentShiftsOneStepWorseCappedAtTerrible) {
  // 0x14092B57B CMP EBX,4 / 0x14092B580 INC EBX: opponent min(arrow+1,4) in PES
  // space, i.e. one step towards Terrible in GF space, capped there.
  EXPECT_EQ(FormState::ApplyTeamShift(FormState::Arrow::Top, /*ownTeam=*/false,
                                     /*teamModeThree=*/false, /*levelClassSix=*/true),
            FormState::Arrow::Great);
  EXPECT_EQ(FormState::ApplyTeamShift(FormState::Arrow::Terrible, /*ownTeam=*/false,
                                     /*teamModeThree=*/false, /*levelClassSix=*/true),
            FormState::Arrow::Terrible);
  // PES index 3 shifts to 4 (Poor -> Terrible): the cap is at 4, not below it.
  EXPECT_EQ(FormState::ApplyTeamShift(FormState::Arrow::Poor, /*ownTeam=*/false,
                                     /*teamModeThree=*/false, /*levelClassSix=*/true),
            FormState::Arrow::Terrible);
  // Without the /100 == 6 gate the opponent shift does not apply.
  EXPECT_EQ(FormState::ApplyTeamShift(FormState::Arrow::Top, /*ownTeam=*/false,
                                     /*teamModeThree=*/false, /*levelClassSix=*/false),
            FormState::Arrow::Top);
}

TEST(ConditionShift, GreenAndOutOfRangeSkip) {
  // 0x14092B59B/5A0: green x1.0 and out-of-range indices write nothing.
  EXPECT_FALSE(FormState::ArrowWritesStats(FormState::Arrow::Normal));
  EXPECT_TRUE(FormState::ArrowWritesStats(FormState::Arrow::Top));
  EXPECT_TRUE(FormState::ArrowWritesStats(FormState::Arrow::Terrible));
}

TEST(ConditionChain, TheSameDataDealsTheSameArrowsWhateverTheSeed) {
  // FUN_141FE3830 is RNG-clean (no FUN_140A924E0 in its disassembly): the arrow is a pure
  // function of the condition byte and the team state, so there is no seed to pass and no
  // way for two matches over the same data to differ.
  for (unsigned int cond = 0x28; cond <= 0x6D; ++cond) {
    const FormState::Arrow home = FormState::ApplyTeamShift(FormState::ArrowForConditionByte(cond),
                                                           /*ownTeam=*/true, false, false);
    const FormState::Arrow away = FormState::ApplyTeamShift(FormState::ArrowForConditionByte(cond),
                                                           /*ownTeam=*/false, false, true);
    EXPECT_EQ(home, FormState::ApplyTeamShift(FormState::ArrowForConditionByte(cond),
                                             /*ownTeam=*/true, false, false))
        << "cond " << cond;
    EXPECT_EQ(away, FormState::ApplyTeamShift(FormState::ArrowForConditionByte(cond),
                                             /*ownTeam=*/false, false, true))
        << "cond " << cond;
  }
}

}  // namespace
