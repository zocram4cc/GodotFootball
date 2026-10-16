#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "onthepitch/pesevents.hpp"

namespace {

// A PE with one section: file offset 0x400 holds the section's virtual address.
std::vector<uint8_t> SectionPe(uint64_t imageBase, uint32_t sectionVa, uint32_t rawSize) {
  std::vector<uint8_t> b(0x600, 0);
  b[0] = 'M', b[1] = 'Z';
  constexpr uint32_t PE_AT = 0x80;
  auto put16 = [&](size_t at, uint16_t v) { std::memcpy(&b[at], &v, 2); };
  auto put32 = [&](size_t at, uint32_t v) { std::memcpy(&b[at], &v, 4); };
  auto put64 = [&](size_t at, uint64_t v) { std::memcpy(&b[at], &v, 8); };
  put32(0x3C, PE_AT);
  put32(PE_AT, 0x00004550);
  put16(PE_AT + 6, 1);
  put16(PE_AT + 20, 0xF0);
  put16(PE_AT + 24, 0x20B);
  put64(PE_AT + 24 + 24, imageBase);
  const uint32_t sec = PE_AT + 24 + 0xF0;
  std::memcpy(&b[sec], ".rdata\0\0", 8);
  put32(sec + 8, rawSize);        // virtual size
  put32(sec + 12, sectionVa);     // virtual address (RVA)
  put32(sec + 16, rawSize);       // raw size
  put32(sec + 20, 0x400);         // raw offset
  b.resize(0x400 + rawSize, 0);
  return b;
}

constexpr uint64_t kImageBase = 0x140000000;
constexpr uint64_t kTableVa = 0x142642e60;
// The section that carries the table in this buffer: the RVA the table sits at.
constexpr uint32_t kTableSectionVa = 0x2642000;

// FUN_140a3cdd0: the record comes back as the empty record, not zeroed-and-done.
TEST(PesEventRecord, ResetFillsTheHeader) {
  pes::EventRecord rec;
  rec.Write(7, 100, 100, 1.5f, 3);
  rec.SetU32(0x40, 0xdeadbeef);
  rec.Reset();
  EXPECT_EQ(rec.kind, pes::EventRecord::kEmptyKind);
  EXPECT_EQ(rec.frame1, 0);
  EXPECT_EQ(rec.frame2, 0);
  EXPECT_EQ(rec.stateByte, 0);
  EXPECT_EQ(rec.p6, pes::EventRecord::kResetP6);
  EXPECT_EQ(rec.U32(0x40), 0u);  // the payload is zeroed too
}

// FUN_140a3d060: rec[0]=K, rec[1..2]=frames, rec[4]=p6.
TEST(PesEventRecord, WriteFillsTheFiveHeaderWords) {
  pes::EventRecord rec;
  rec.Write(9, 1234, 1234, 0.0f, 5);
  EXPECT_EQ(rec.kind, 9u);
  EXPECT_EQ(rec.frame1, 1234);
  EXPECT_EQ(rec.frame2, 1234);
  EXPECT_EQ(rec.p6, 5);
}

// FUN_140a6bd60/FUN_140a6be70: the state byte is the word's /3600, plus one
// when (word / 60) leaves a remainder mod 60. Word 60.0 -> 1, 3600.0 -> 1,
// 3600.0 + 60 -> 2, 0 -> 0.
TEST(PesEventRecord, StateByteFoldsTheWordPerMinuteAndHour) {
  EXPECT_EQ(pes::StateByteOf(0.0f), 0);
  EXPECT_EQ(pes::StateByteOf(59.0f), 0);      // under a minute: no +1
  EXPECT_EQ(pes::StateByteOf(60.0f), 1);      // one minute in
  EXPECT_EQ(pes::StateByteOf(3599.0f), 1);    // 59 minutes in, still hour 0
  EXPECT_EQ(pes::StateByteOf(3600.0f), 1);    // exactly one hour in
  EXPECT_EQ(pes::StateByteOf(3660.0f), 2);    // one hour plus one minute
  EXPECT_EQ(pes::StateByteOf(7199.0f), 2);
  EXPECT_EQ(pes::StateByteOf(7200.0f), 2);
}

// FUN_140a3c070: the word is the selected word's bytes each offset by the
// counter's bytes and folded by 0x3f; the check word is FNV-1a over them
// (seed 0xcbf29ce484222325, prime 0x100000001b3).
TEST(PesStateWord, WordIsTheFoldedBytesAndTheCheckHolds) {
  pes::StateWordInput in;
  in.selected = 0x11223344;
  in.counter = 5;  // tripled to 15 inside FUN_140a3c070
  in.check = 0x764a3c5e1d56d23ull;
  EXPECT_TRUE(pes::StateWordCheckHolds(in));
  EXPECT_EQ(pes::StateWord(in), 0x112233e2u);

  in.counter = 1000;  // tripled to 3000: every byte of the counter is used
  in.check = 0x4a8d0e0688aa6e0dull;
  EXPECT_TRUE(pes::StateWordCheckHolds(in));
  EXPECT_EQ(pes::StateWord(in), 0x11228bb4u);

  in.check ^= 1u;  // a tampered check word is reported, not folded away
  EXPECT_FALSE(pes::StateWordCheckHolds(in));
}

// FUN_140a3c050: the float word is the state word plus x+0xc.
TEST(PesStateWord, FloatWordAddsTheOffset) {
  pes::StateWordInput in;
  in.selected = 0x11223344;
  in.counter = 5;
  in.check = 0x764a3c5e1d56d23ull;
  in.offset = 0.5f;
  EXPECT_FLOAT_EQ(pes::StateWordFloat(in), 0.5f + 0x112233e2);
}

// The routing table is read from the install, never transcribed (pes/exe.hpp).
TEST(PesEventRingTable, ReadsFiftySixRingsFromTheExe) {
  auto b = SectionPe(kImageBase, kTableSectionVa, 0x1000);
  const size_t at = 0x400 + (kTableVa - kImageBase - kTableSectionVa);
  std::memcpy(&b[at],
              (const uint32_t[]){
                  10, 10, 10, 10, 6, 10, 10, 0, 1, 10, 10,
                  2, 10, 10, 7, 10, 10, 10, 10, 10, 10, 10,
                  10, 3, 10, 10, 10, 10, 10, 10, 4, 10, 10,
                  10, 10, 10, 10, 5, 10, 10, 10, 10, 10, 9,
                  10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                  0},
              56 * 4);
  std::string err;
  auto exe = pes::PeImage::Parse(std::move(b), &err);
  ASSERT_TRUE(exe) << err;
  auto table = pes::LoadEventRingTable(*exe, &err);
  ASSERT_TRUE(table) << err;
  EXPECT_EQ(table->ring[7], 0);   // touch
  EXPECT_EQ(table->ring[8], 1);   // the other touch
  EXPECT_EQ(table->ring[30], 4);  // the foul ring
  EXPECT_EQ(table->ring[14], 7);
  EXPECT_EQ(table->ring[5], 10);  // shot: the overflow ring
  EXPECT_EQ(table->RingOf(0), 10);
  EXPECT_EQ(table->RingOf(31), 10);
  EXPECT_EQ(table->ring.size(), 56u);
}

// The install's own table (the OBS values, docs/re/pes-re-possession-latch.md
// §24). Skipped without PES21_ROOT.
TEST(PesEventRingTable, TheInstallRoutesTheKindsItRoutes) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto table = pes::LoadEventRingTable(*exe, &err);
  ASSERT_TRUE(table) << err;
  EXPECT_EQ(table->RingOf(7), 0);
  EXPECT_EQ(table->RingOf(8), 1);
  EXPECT_EQ(table->RingOf(11), 2);
  EXPECT_EQ(table->RingOf(23), 3);
  EXPECT_EQ(table->RingOf(30), 4);
  EXPECT_EQ(table->RingOf(37), 5);
  EXPECT_EQ(table->RingOf(4), 6);
  EXPECT_EQ(table->RingOf(14), 7);
  EXPECT_EQ(table->RingOf(43), 9);
  EXPECT_EQ(table->RingOf(55), 0);
  for (int k = 0; k < 56; ++k) {
    const int r = table->RingOf(k);
    EXPECT_GE(r, 0);
    EXPECT_LE(r, 10);
  }
}

// The actor key: FUN_140a60530 accepts team < 2 and slot < 0x28; FUN_140a60500
// is the {2, 0xff} stand-in.
TEST(PesActorKey, ValidityFollowsFUN_140a60530) {
  pes::ActorKey k;
  k.team = 1;
  k.slot = 5;
  EXPECT_TRUE(k.Valid());
  k.team = 2;
  EXPECT_FALSE(k.Valid());
  k = pes::NoActorKey();
  EXPECT_FALSE(k.Valid());
  EXPECT_EQ(k.team, pes::EventRecord::kNoActorTeam);
  EXPECT_EQ(k.slot, pes::EventRecord::kNoActorSlot);
}

}  // namespace
