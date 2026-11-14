// PES's roster-entry default (pesworldrecord.hpp InitRosterEntry: FUN_140A55940 with FUN_140A55F80 and FUN_140A6E6C0). The
// constructor fills every roster entry with it. The test checks it against one captured entry (entry 2 of the roster table in
// live2/pid480109/seq001, embedded below, 116 bytes) and, when GF_PES_ROSTER_CAPTURE names the full 2784-byte roster table dump,
// against all 24 captured entries.
//
// Known departures, labelled INF in pesworldrecord.hpp and asserted here as departures rather than as matches:
//   * +0x18 of entries 2..23 holds 0x20 in the capture; the writer stores 0xFFFFFFFF there (writer of 0x20 not found).
//   * Entries 0 and 1 differ from the capture in many bytes (writer not found); only their index byte is asserted.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"

namespace {

// Entry 2 of the captured roster table (116 bytes).
constexpr uint8_t kCapturedEntry2[pes::kRosterEntryBytes] = {
    0x01, 0x02, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00};

constexpr std::size_t kSlot18 = 0x18;
constexpr std::size_t kIndexByte = 1;

std::vector<uint8_t> EntryFor(int index) {
  std::vector<uint8_t> entry(pes::kRosterEntryBytes);
  pes::InitRosterEntry(entry.data(), static_cast<uint8_t>(index));
  return entry;
}

}  // namespace

// Entry 2 of the default roster matches the captured bytes, except +0x18 (INF, see the file comment).
TEST(PesRosterEntry, EntryTwoMatchesTheCapturedBytesExceptSlot18) {
  const std::vector<uint8_t> entry = EntryFor(2);
  for (std::size_t k = 0; k < pes::kRosterEntryBytes; ++k) {
    if (k >= kSlot18 && k < kSlot18 + 4) continue;  // INF: captured 0x20, writer stores 0xFFFFFFFF
    EXPECT_EQ(entry[k], kCapturedEntry2[k]) << "entry 2 byte +" << k;
  }
  EXPECT_EQ(entry[kIndexByte], 2);
}

// Entries 3..23 are the entry-2 layout with their own index (the writer's stores do not depend on the index otherwise).
TEST(PesRosterEntry, EntriesThreeToTwentyThreeDifferFromEntryTwoOnlyInTheIndexByte) {
  const std::vector<uint8_t> two = EntryFor(2);
  for (int index = 3; index < pes::kRosterEntries; ++index) {
    const std::vector<uint8_t> entry = EntryFor(index);
    for (std::size_t k = 0; k < pes::kRosterEntryBytes; ++k) {
      if (k == kIndexByte) {
        EXPECT_EQ(entry[k], index);
      } else {
        EXPECT_EQ(entry[k], two[k]) << "entry " << index << " byte +" << k;
      }
    }
  }
}

// Entries 0 and 1 are INF (captured bytes differ, writer not found): only the index byte is asserted.
TEST(PesRosterEntry, EntriesZeroAndOneCarryTheirIndex) {
  EXPECT_EQ(EntryFor(0)[kIndexByte], 0);
  EXPECT_EQ(EntryFor(1)[kIndexByte], 1);
}

// Full comparison against a captured roster table dump (GF_PES_ROSTER_CAPTURE: 24 entries of 116 bytes, no header).
// The dump is PES's memory and is not committed; without the variable the test skips.
TEST(PesRosterEntry, MatchesEveryCapturedEntryWhenACaptureIsGiven) {
  const char* path = std::getenv("GF_PES_ROSTER_CAPTURE");
  if (path == nullptr || *path == '\0') GTEST_SKIP() << "GF_PES_ROSTER_CAPTURE not set";
  std::ifstream in(path, std::ios::binary);
  ASSERT_TRUE(in) << "cannot open " << path;
  const std::vector<uint8_t> table((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  ASSERT_EQ(table.size(), static_cast<std::size_t>(pes::kRosterEntries) * pes::kRosterEntryBytes);
  for (int index = 2; index < pes::kRosterEntries; ++index) {
    const std::vector<uint8_t> entry = EntryFor(index);
    const uint8_t* captured = table.data() + static_cast<std::size_t>(index) * pes::kRosterEntryBytes;
    for (std::size_t k = 0; k < pes::kRosterEntryBytes; ++k) {
      if (k >= kSlot18 && k < kSlot18 + 4) continue;  // INF
      EXPECT_EQ(entry[k], captured[k]) << "entry " << index << " byte +" << k;
    }
  }
}
