// PES's match-info block (MI) is the record the match clock writes and the translated stoppage functions read. G names
// it at G+0x270 (docs/re/pes-re-ref-record.md 1.1), and GF keeps it in the world window beside the player blocks, so a
// translated read of MI+0x1168 lands in the bytes the clock writes.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "gametypes.hpp"
#include "onthepitch/pesclock.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace {

using namespace pes;

uint64_t LoadU64(const uint8_t* at) {
  uint64_t value;
  std::memcpy(&value, at, sizeof value);
  return value;
}

uint32_t LoadU32(const uint8_t* at) {
  uint32_t value;
  std::memcpy(&value, at, sizeof value);
  return value;
}

// The translated accessors take G in rcx as the window's VA, so G is the window's base and G+0x270 is Bytes()+0x270.
TEST(PesMatchInfo, GNamesTheMatchInfoBlockAtPlus0x270) {
  PesWorld world;
  const uint64_t mi = PesWorld::MatchInfoVa();
  EXPECT_GE(mi, kWorldWindowVa);
  EXPECT_LE(mi + kMatchInfoBytes, kWorldWindowVa + kWorldBytes);
  EXPECT_EQ(LoadU64(world.Bytes() + kMatchInfoPtrOffset), mi);
}

// The clock's period record is the block's own bytes, not a copy: the period it writes sits at MI+0x1168.
TEST(PesMatchInfo, TheClocksPeriodRecordIsTheMatchInfoBlock) {
  PesWorld world;
  PesClockConstants constants;
  PesClock clock(constants, 90.0f, world.Period());
  clock.SetPhase(e_MatchPhase_1stHalf);
  EXPECT_EQ(&clock.Record(), &world.Period());
  // PES period 1 is the first half (docs/re/pes-re-ref-record.md 1.2).
  EXPECT_EQ(LoadU32(world.Bytes() + kMatchInfoOffset + kMatchInfoPeriodOffset), 1u);
}

}  // namespace
