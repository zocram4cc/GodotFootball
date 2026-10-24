// PES's play state MI+0x116c (pes-re-A5Judge.md): 1 in play, 2 stopped, 3 set-play ready. A restart the referee has
// awarded and armed or started is 3; any other stop is 2. These are the values the MOVESTEP phase reads.
#include <gtest/gtest.h>

#include "pes/movestep.hpp"

TEST(PlayState, InPlayIsOne) {
  EXPECT_EQ(pes::PlayStateFor(true, false, false), 1);
  EXPECT_EQ(pes::PlayStateFor(true, true, true), 1);
}

TEST(PlayState, AnAwardedRestartArmedOrTakenIsSetPlayReady) {
  EXPECT_EQ(pes::PlayStateFor(false, true, true), 3);
}

TEST(PlayState, AStopWithoutAnArmedAwardedRestartIsStopped) {
  EXPECT_EQ(pes::PlayStateFor(false, true, false), 2);
  EXPECT_EQ(pes::PlayStateFor(false, false, false), 2);
  EXPECT_EQ(pes::PlayStateFor(false, false, true), 2);
}

// The seed-7 run's 754 ticks (plan-selector §1 vs roles §2): in play with a set piece armed or
// taken, PES's team-tick state is 1 however the award flags read. The role state agrees.
TEST(PlayState, InPlayWithASetPieceIsOneWhateverTheAwardReads) {
  EXPECT_EQ(pes::PlayStateFor(true, false, false), 1);
  EXPECT_EQ(pes::PlayStateFor(true, true, false), 1);
  EXPECT_EQ(pes::PlayStateFor(true, false, true), 1);
}
