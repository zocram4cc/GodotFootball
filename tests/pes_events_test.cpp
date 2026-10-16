#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <utility>

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

// The install's routing table (OBS, docs/re/pes-re-possession-latch.md §24),
// used wherever a table is needed without the install itself.
std::array<uint32_t, 56> ObsRingTable() {
  std::array<uint32_t, 56> table{};
  table.fill(10);  // every unlisted kind goes to the overflow ring
  const std::array<std::pair<uint32_t, uint32_t>, 10> routed{{
      {7, 0}, {8, 1}, {11, 2}, {23, 3}, {30, 4},
      {37, 5}, {4, 6}, {14, 7}, {43, 9}, {55, 0}}};
  for (const auto& [kind, ring] : routed) table[kind] = ring;
  return table;
}

// A PE image of the install's routing table, for tests that need a real
// EventRingTable without the install.
std::unique_ptr<pes::PeImage> TableFromObs() {
  auto b = SectionPe(kImageBase, kTableSectionVa, 0x1000);
  std::memcpy(&b[0x400 + (kTableVa - kImageBase - kTableSectionVa)], ObsRingTable().data(),
              56 * 4);
  std::string err;
  auto pe = pes::PeImage::Parse(std::move(b), &err);
  if (!pe) return nullptr;
  return std::make_unique<pes::PeImage>(std::move(*pe));
}

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
  std::memcpy(&b[at], ObsRingTable().data(), 56 * 4);
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

// FUN_140a3dac0: the count saturates at the slot count and the head wraps.
TEST(PesEventRing, PushSaturatesTheCountAndWrapsTheHead) {
  pes::EventRing8 ring;
  for (int i = 0; i < 11; ++i) {
    pes::EventRecord rec;
    rec.Write(7, i, i, 0.f, 0);
    ring.Push(rec);
  }
  EXPECT_EQ(ring.count, 8u);
  // FUN_140a3dac0 increments the head before the copy, so the first push lands
  // in slot 1 and slot 0 is only reached on the eighth.
  EXPECT_EQ(ring.head, 3u);  // 11 pushes: head = 11 mod 8
  EXPECT_EQ(ring.Slot(3).frame1, 10);
  EXPECT_EQ(ring.Slot(2).frame1, 9);
  EXPECT_EQ(ring.Slot(1).frame1, 8);
  EXPECT_EQ(ring.Slot(0).frame1, 7);
}

// FUN_140a3db80: the overflow ring is the same shape at 0x40 slots.
TEST(PesEventRing, OverflowRingWrapsAtSixtyFour) {
  pes::EventRing10 ring;
  for (int i = 0; i < 66; ++i) {
    pes::EventRecord rec;
    rec.Write(5, i, i, 0.f, 0);
    ring.Push(rec);
  }
  EXPECT_EQ(ring.count, 0x40u);
  EXPECT_EQ(ring.head, 2u);
  EXPECT_EQ(ring.Slot(2).frame1, 65);
  EXPECT_EQ(ring.Slot(1).frame1, 64);
}

// FUN_140a3da00: the count is sticky at 1 and the head held at 0.
TEST(PesEventKindBlock, SecondPushOverwritesTheRecord) {
  pes::EventKindBlock block;
  pes::EventRecord a;
  a.Write(9, 10, 10, 0.f, 0);
  EXPECT_EQ(block.Push(a), 0);
  EXPECT_EQ(block.count, 1u);
  EXPECT_EQ(block.head, 0u);
  pes::EventRecord b;
  b.Write(9, 20, 20, 0.f, 0);
  EXPECT_EQ(block.Push(b), 0);
  EXPECT_EQ(block.count, 1u);  // still one: the count never grows past 1
  EXPECT_EQ(block.head, 0u);
  EXPECT_EQ(block.record.frame1, 20);
}

// FUN_140a3dc40's write log: 64 entries, the count saturated at 64, the head
// wrapping at 64, and the {ring, slot} pair of the record just routed.
TEST(PesEventLog, RecordsTheRingAndSlotOfEachRoute) {
  pes::EventLog log;
  for (int i = 0; i < 70; ++i) log.Push(3, static_cast<uint16_t>(i));
  EXPECT_EQ(log.count, 64u);
  EXPECT_EQ(log.head, 6u);  // 70 pushes: 70 mod 64
  EXPECT_EQ(log.entries[6].ring, 3u);
  EXPECT_EQ(log.entries[6].slot, 69);
  EXPECT_EQ(log.entries[5].slot, 68);
  log.Clear();  // FUN_140a3e140
  EXPECT_EQ(log.count, 0u);
  EXPECT_EQ(log.head, 0u);
}

// FUN_140a3dc40: the routing table picks the ring, the record lands in the
// ring's head slot, the log records the pair, and the per-kind block copies it.
TEST(PesEventStore, RouteFollowsTheInstallTable) {
  std::string err;
  auto b = SectionPe(kImageBase, kTableSectionVa, 0x1000);
  const size_t at = 0x400 + (kTableVa - kImageBase - kTableSectionVa);
  std::memcpy(&b[at], ObsRingTable().data(), 56 * 4);
  auto pe = pes::PeImage::Parse(std::move(b), &err);
  ASSERT_TRUE(pe) << err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;

  pes::EventStore store;
  pes::EventRecord touch;
  touch.Write(7, 100, 100, 0.f, 1);
  touch.SetActionKind(7);
  store.Route(*table, touch);
  EXPECT_EQ(store.Ring(0).count, 1u);
  EXPECT_EQ(store.Ring(0).head, 1u);  // the head is incremented before the copy
  EXPECT_EQ(store.Ring(0).Slot(1).frame1, 100);
  EXPECT_EQ(store.Log().count, 1u);
  EXPECT_EQ(store.Log().head, 1u);  // the log's head is incremented before the write
  EXPECT_EQ(store.Log().entries[1].ring, 0u);
  EXPECT_EQ(store.Log().entries[1].slot, 1u);
  EXPECT_EQ(store.KindBlock(7).count, 1u);
  EXPECT_EQ(store.KindBlock(7).record.frame1, 100);

  pes::EventRecord shot;
  shot.Write(5, 200, 200, 0.f, 1);
  store.Route(*table, shot);
  EXPECT_EQ(store.OverflowRing().count, 1u);  // kind 5 goes to ring 10
  EXPECT_EQ(store.OverflowRing().head, 1u);
  EXPECT_EQ(store.Log().entries[2].ring, 10);
  EXPECT_EQ(store.Log().entries[2].slot, 1u);

  pes::EventRecord foul;
  foul.Write(30, 300, 300, 0.f, 1);
  store.Route(*table, foul);
  EXPECT_EQ(store.Ring(4).count, 1u);  // the foul ring
  EXPECT_EQ(store.Ring(4).head, 1u);
  EXPECT_EQ(store.Log().entries[3].ring, 4);
}

// The kind-7 newest copy: E+0x180698 takes the payload, E+0x1806fc only while
// the key at rec+0x18 is valid, and a record of any other kind leaves both.
TEST(PesEventStore, NewestCopyFollowsKindSevenAndTheKey) {
  std::string err;
  auto b = SectionPe(kImageBase, kTableSectionVa, 0x1000);
  const size_t at = 0x400 + (kTableVa - kImageBase - kTableSectionVa);
  std::memcpy(&b[at], ObsRingTable().data(), 56 * 4);
  auto pe = pes::PeImage::Parse(std::move(b), &err);
  ASSERT_TRUE(pe) << err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;

  pes::EventStore store;
  const auto payload_word = [&store](uint32_t off) { return store.Newest().U32(off); };
  const auto second_word = [&store](uint32_t off) { return store.NewestSecond().U32(off); };

  pes::EventRecord other;
  other.Write(30, 10, 10, 0.f, 1);
  other.SetActionKind(30);
  store.Route(*table, other);
  EXPECT_EQ(store.NewestOrRing0(0), nullptr);  // no copy for a non-kind-7 record
  EXPECT_EQ(second_word(4), 0u);

  pes::EventRecord touch;
  touch.Write(7, 20, 20, 0.f, 1);
  touch.SetActionKind(7);
  touch.SetU32(0x18, 1);   // payload+4: the key's team word
  touch.SetU32(0x1c, 4);   // payload+8: the key's slot word
  touch.SetU32(0x24, 21);  // payload+0x10: the frame
  store.Route(*table, touch);
  EXPECT_EQ(payload_word(0x14), 7u);
  EXPECT_EQ(payload_word(0x18), 1u);
  EXPECT_EQ(payload_word(0x1c), 4u);
  EXPECT_EQ(payload_word(0x24), 21u);
  EXPECT_EQ(second_word(0x18), 1u);  // the second copy is in step: the key is valid

  pes::EventRecord bad;
  bad.Write(7, 30, 30, 0.f, 1);
  bad.SetActionKind(7);
  bad.SetU32(0x18, 9);  // an invalid team word: FUN_140a60530 fails
  bad.SetU32(0x24, 31);
  store.Route(*table, bad);
  EXPECT_EQ(payload_word(0x14), 7u);
  EXPECT_EQ(payload_word(0x18), 9u);  // the first copy still follows the newest
  EXPECT_EQ(second_word(0x18), 1u);  // the second copy keeps the last valid key
}

// A store that routes records the way PES's detectors write them, for the
// reader tests: the payload's action kind, its frame and its actor key.
class RoutedStore {
 public:
  explicit RoutedStore(const pes::EventRingTable& t) : table_(t) {}
  void Push(uint32_t routeKind, uint32_t actionKind, int frame, pes::ActorKey key) {
    pes::EventRecord rec;
    rec.Reset();
    rec.Write(routeKind, frame, frame, 0.f, 1);
    rec.SetActionKind(actionKind);
    rec.SetU32(0x18, key.team);
    rec.SetU32(0x1c, key.slot);
    rec.SetU32(0x24, frame);
    store_.Route(table_, rec);
  }
  pes::EventStore& store() { return store_; }

 private:
  const pes::EventRingTable& table_;
  pes::EventStore store_;
};

// A ring slot still holding the 0x37 sentinel reads as no record, which is what
// FUN_140a3e190's `*piVar2 == 0x37` test does before it hands back the payload.
TEST(PesEventReaders, AnEmptySlotReadsAsNoRecord) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  s.Push(7, 7, 10, pes::ActorKey{0, 1});
  s.Push(7, 7, 20, pes::ActorKey{0, 2});
  // Ring 0's head is 2 after two pushes: n == 1 reads slot 1, n == 2 reads the
  // slot no push has reached.
  EXPECT_EQ(s.store().NewestOrRing0(2), nullptr);  // past the count
  EXPECT_NE(s.store().NewestOrRing0(1), nullptr);
  pes::EventRecord empty;
  empty.Reset();
  s.store().Ring(0).slots[1] = empty;
  EXPECT_EQ(s.store().NewestOrRing0(1), nullptr);  // the sentinel reads as empty
}

// FUN_140a3e190: n == 0 is the newest, n >= 1 walks ring 0 back from its head,
// and both fall back to the empty sentinel.
TEST(PesEventReaders, NewestOrRing0WalksRingZero) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  pes::EventRingTable table;
  std::string err;
  auto loaded = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(loaded) << err;
  table = *loaded;
  RoutedStore s(table);

  EXPECT_EQ(s.store().NewestOrRing0(0), nullptr);  // nothing routed yet
  EXPECT_EQ(s.store().NewestOrRing0(1), nullptr);  // ring 0 is empty

  const pes::ActorKey k0{0, 3};
  const pes::ActorKey k1{1, 7};
  const pes::ActorKey k2{0, 10};
  s.Push(7, 7, 100, k0);
  s.Push(7, 7, 200, k1);
  s.Push(7, 2, 300, k2);  // a pass: the action kind is what readers see

  const pes::EventRecord* newest = s.store().NewestOrRing0(0);
  ASSERT_NE(newest, nullptr);
  EXPECT_EQ(newest->ActionKind(), 2u);
  EXPECT_EQ(newest->ActionFrame(), 300);
  EXPECT_EQ(s.store().NewestOrRing0(1)->ActionFrame(), 200);
  EXPECT_EQ(s.store().NewestOrRing0(2)->ActionFrame(), 100);
  EXPECT_EQ(s.store().NewestOrRing0(3), nullptr);  // only three in the ring
}

// FUN_140a3e150: ring k in 0..9; the overload ring (10) is not one of them.
TEST(PesEventReaders, RingRecordTakesAnyRingUnderTen) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  s.Push(30, 30, 500, pes::ActorKey{1, 2});  // the foul ring
  s.Push(5, 5, 600, pes::ActorKey{0, 1});    // the overflow ring
  ASSERT_NE(s.store().RingRecord(4, 0), nullptr);
  EXPECT_EQ(s.store().RingRecord(4, 0)->ActionFrame(), 500);
  EXPECT_EQ(s.store().RingRecord(4, 1), nullptr);
  EXPECT_EQ(s.store().RingRecord(5, 0), nullptr);  // ring 5 holds only kind 37
  EXPECT_EQ(s.store().RingRecord(10, 0), nullptr);  // FUN_140a3e150 stops at 9
}

// FUN_140a3e7d0: ring 1's record's key is at rec+0x1c (payload+8), not the
// +0x18 word rings 0..9 use.
TEST(PesEventReaders, Ring1KeyReadsPayloadPlusEight) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  // A kind-8 record routes to ring 1; its key word at payload+8 is what the
  // key reader takes.
  pes::EventRecord rec;
  rec.Reset();
  rec.Write(8, 10, 10, 0.f, 1);
  rec.SetU32(0x1c, 1);
  rec.SetU32(0x20, 4);
  s.store().Route(*table, rec);
  pes::ActorKey key = s.store().Ring1Key(0);
  EXPECT_TRUE(key.Valid());
  EXPECT_EQ(key.team, 1u);
  EXPECT_EQ(key.slot, 4u);
  EXPECT_FALSE(s.store().Ring1Key(1).Valid());  // the empty key
}

// FUN_140a3e880: the second copy's key, E+0x180700.
TEST(PesEventReaders, NewestSecondKeyFollowsTheValidCopy) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  EXPECT_FALSE(s.store().NewestSecondKey().Valid());
  s.Push(7, 7, 10, pes::ActorKey{1, 5});
  pes::ActorKey key = s.store().NewestSecondKey();
  EXPECT_TRUE(key.Valid());
  EXPECT_EQ(key.team, 1u);
  EXPECT_EQ(key.slot, 5u);
}

// FUN_140a3e940: the per-kind last record; FUN_140a3e5d0: the n-th newest
// record of a kind, through the write log.
TEST(PesEventReaders, KindLastAndKindNthUseTheBlocksAndTheLog) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  s.Push(7, 7, 10, pes::ActorKey{0, 1});
  s.Push(30, 30, 20, pes::ActorKey{1, 2});
  s.Push(7, 3, 30, pes::ActorKey{0, 4});
  s.Push(7, 2, 40, pes::ActorKey{1, 6});

  ASSERT_NE(s.store().KindLast(7), nullptr);
  EXPECT_EQ(s.store().KindLast(7)->ActionFrame(), 40);
  EXPECT_EQ(s.store().KindLast(30)->ActionFrame(), 20);
  EXPECT_EQ(s.store().KindLast(9), nullptr);  // never pushed

  ASSERT_NE(s.store().KindNth(7, 0), nullptr);
  EXPECT_EQ(s.store().KindNth(7, 0)->ActionFrame(), 40);  // newest first
  EXPECT_EQ(s.store().KindNth(7, 1)->ActionFrame(), 30);
  EXPECT_EQ(s.store().KindNth(7, 2)->ActionFrame(), 10);
  EXPECT_EQ(s.store().KindNth(7, 3), nullptr);
  EXPECT_EQ(s.store().KindNth(30, 0)->ActionFrame(), 20);
}

// FUN_140a3e3b0: the newest kind-5 (shot) record, over the newest then ring 0.
TEST(PesEventReaders, LastShotSkipsNonShotRecords) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  EXPECT_EQ(s.store().LastShot(0), nullptr);
  s.Push(7, 7, 10, pes::ActorKey{0, 1});
  s.Push(7, 5, 20, pes::ActorKey{0, 2});
  s.Push(7, 2, 30, pes::ActorKey{0, 3});
  const pes::EventRecord* shot = s.store().LastShot(0);
  ASSERT_NE(shot, nullptr);
  EXPECT_EQ(shot->ActionFrame(), 20);
  EXPECT_EQ(s.store().LastShot(15), nullptr);  // the frame filter excludes it
  EXPECT_EQ(s.store().LastShot(25)->ActionFrame(), 20);
}

// FUN_140a6ce10: the busy test over the action kind.
TEST(PesEventReaders, BusyForCoversFiveSixSevenAndNineToFourteen) {
  auto busy = [](uint32_t kind, bool carrier, int role) {
    pes::EventRecord rec;
    rec.SetActionKind(kind);
    return pes::BusyFor(rec, carrier, role);
  };
  EXPECT_TRUE(busy(9, false, 0));
  EXPECT_TRUE(busy(14, true, 0));
  EXPECT_TRUE(busy(6, false, 0));   // no carrier
  EXPECT_TRUE(busy(7, false, 0));
  EXPECT_FALSE(busy(7, true, 0));   // a carrier holds it
  EXPECT_FALSE(busy(6, true, 0));
  EXPECT_TRUE(busy(5, true, 3));    // a role-3 context
  EXPECT_FALSE(busy(5, true, 5));   // the role is itself 5
  EXPECT_FALSE(busy(5, false, 0));  // no context at all
  EXPECT_FALSE(busy(1, false, 0));
  EXPECT_FALSE(busy(15, true, 0));  // past the busy set
}

// FUN_1404fe9c0 step 2's kind set and window, and FUN_140a0b350 step 8's.
TEST(PesEventReaders, LatchStepTwoAndTheDodgeWindow) {
  EXPECT_TRUE(pes::IsLatchStepTwoKind(1));
  EXPECT_TRUE(pes::IsLatchStepTwoKind(3));
  EXPECT_TRUE(pes::IsLatchStepTwoKind(0x10));
  EXPECT_FALSE(pes::IsLatchStepTwoKind(4));
  EXPECT_FALSE(pes::IsLatchStepTwoKind(5));
  EXPECT_FALSE(pes::IsLatchStepTwoKind(7));
  EXPECT_EQ(pes::LatchStepTwoWindowFrames(54), 9);

  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  s.Push(7, 1, 100, pes::ActorKey{1, 2});  // the newest event is team 1's
  const float factor = 0.2f;               // DAT_142598578
  EXPECT_TRUE(pes::DodgeWindowRefuses(s.store(), 1, 100, 54, factor));   // same frame
  EXPECT_TRUE(pes::DodgeWindowRefuses(s.store(), 1, 110, 54, factor));   // 10 < 10.8
  EXPECT_FALSE(pes::DodgeWindowRefuses(s.store(), 1, 111, 54, factor));  // 11 >= 10.8
  EXPECT_FALSE(pes::DodgeWindowRefuses(s.store(), 0, 100, 54, factor));  // the other team
}

// FUN_140a62ea0/FUN_140a3b320: the player id a key names through its team's
// roster, and the no-actor answer otherwise.
TEST(PesEventReaders, ActorIdOfWalksTheRoster) {
  const std::array<uint32_t, 0x28> roster{};  // filled per entry below
  std::array<uint32_t, 0x28> r = roster;
  r[3] = 7;   // roster entry 3 carries slot 7
  r[12] = 9;  // entry 12 is past the starters
  EXPECT_EQ(pes::ActorIdOf(pes::ActorKey{1, 7}, r, 1), 14);   // 1*11 + 3
  EXPECT_EQ(pes::ActorIdOf(pes::ActorKey{0, 7}, r, 0), 3);
  EXPECT_EQ(pes::ActorIdOf(pes::ActorKey{1, 9}, r, 1), 0xff);  // not a starter
  EXPECT_EQ(pes::ActorIdOf(pes::ActorKey{2, 7}, r, 1), 0xff);  // no such team
  EXPECT_EQ(pes::ActorIdOf(pes::NoActorKey(), r, 0), 0xff);
}

// The newest-event view: the fields the latch and the gate read.
TEST(PesEventReaders, NewestEventViewCarriesTheLatchFields) {
  auto pe = TableFromObs();
  ASSERT_TRUE(pe);
  std::string err;
  auto table = pes::LoadEventRingTable(*pe, &err);
  ASSERT_TRUE(table) << err;
  RoutedStore s(*table);
  EXPECT_FALSE(pes::NewestEvent(s.store()).present);

  pes::EventRecord rec;
  rec.Reset();
  rec.Write(7, 42, 42, 0.f, 1);
  rec.SetActionKind(3);
  rec.SetU32(0x18, 1);  // payload+4
  rec.SetU32(0x1c, 8);
  rec.SetU32(0x24, 42);  // payload+0x10
  rec.SetI32(0x30, 5);   // payload+0x1c
  rec.SetU8(0x41, 1);    // payload+0x2d
  rec.SetI32(0x54, 4);   // payload+0x40
  s.store().Route(*table, rec);

  pes::NewestEventView v = pes::NewestEvent(s.store());
  EXPECT_TRUE(v.present);
  EXPECT_EQ(v.kind, 3u);
  EXPECT_EQ(v.frame, 42);
  EXPECT_EQ(v.actor.team, 1u);
  EXPECT_EQ(v.actor.slot, 8u);
  EXPECT_EQ(v.field1c, 5);
  EXPECT_EQ(v.flag2d, 1);
  EXPECT_EQ(v.subKind, 4);
}

}  // namespace
