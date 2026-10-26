// PES's world object G keeps its record tables as runs of 40-byte entries; each entry's first qword is the VA of a
// record. The translated table accessors read those entries in place through PesMem::window, and GF's world is one
// window that holds G and the per-player blocks. Each accessor must answer with the record GF bound at that index, and
// a block field must sit at its PES offset from the record the accessor returns.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>

#include "onthepitch/pesworldrecord.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

namespace {

using namespace pes;

constexpr int kTestSlot = 5;

// The machine state with the world mapped in place: translated code addresses G and the blocks by VA.
PesState MappedState(PesWorld& world) {
  PesState s;
  s.mem.window.base = kWorldWindowVa;
  s.mem.window.size = kWorldBytes;
  s.mem.window.rw = world.Bytes();
  return s;
}

// Runs one translated table accessor with rcx = G and rdx = index. An int argument in edx zero-extends into rdx.
uint64_t RunAccessor(void (*fn)(PesState&), PesWorld& world, uint64_t index) {
  PesState s = MappedState(world);
  s.gpr[1] = kWorldWindowVa;
  s.gpr[2] = static_cast<uint32_t>(index);
  fn(s);
  return s.gpr[0];
}

struct TableCase {
  void (*fn)(PesState&);
  WorldTable table;
  int count;
};

constexpr TableCase kTables[] = {
    {&pes_fn_140A48A30, WorldTable::kSideA, 2},
    {&pes_fn_140A489D0, WorldTable::kSideB, 2},
    {&pes_fn_140A48910, WorldTable::kAnime, kAnimeCount},
    {&pes_fn_140A48A10, WorldTable::kMan, kManCount},
    {&pes_fn_140A489F0, WorldTable::kSlot, kSlotCount},
};

// A distinct record VA per entry. The accessors return the pointer; nothing dereferences it here.
constexpr uint64_t RecordVa(int index) {
  return 0x3000000000ULL + 0x1000ULL * static_cast<uint64_t>(index);
}

TEST(WorldTables, EachAccessorReturnsTheEntryItsIndexNames) {
  for (const TableCase& t : kTables) {
    auto world = std::make_unique<PesWorld>();
    for (int i = 0; i < t.count; ++i) world->SetEntry(t.table, i, RecordVa(i));
    for (int i = 0; i < t.count; ++i)
      EXPECT_EQ(RunAccessor(t.fn, *world, static_cast<uint64_t>(i)), RecordVa(i))
          << "table " << static_cast<int>(t.table) << " index " << i;
  }
}

TEST(WorldTables, AccessorsFallBackToTheFirstEntryPastTheirCount) {
  for (const TableCase& t : kTables) {
    auto world = std::make_unique<PesWorld>();
    for (int i = 0; i < t.count; ++i) world->SetEntry(t.table, i, RecordVa(i));
    EXPECT_EQ(RunAccessor(t.fn, *world, static_cast<uint64_t>(t.count)), RecordVa(0))
        << "table " << static_cast<int>(t.table);
  }
}

// GF writes a slot's fields into its block; the translated accessors return the records, and the bytes read through
// them at PES's offsets are the bytes GF wrote.
TEST(WorldRecords, FieldsSitAtTheirPesOffsetsFromTheRecordTheAccessorReturns) {
  auto world = std::make_unique<PesWorld>();
  world->BindSlot(kTestSlot);
  uint8_t* block = world->Block(kTestSlot);

  const uint8_t animeClass = 0x11;
  block[kAnimeClassOffset] = animeClass;
  const float manPosX = 12.5f;
  std::memcpy(block + kManRecordOffset + kManPosXOffset, &manPosX, sizeof manPosX);
  const uint32_t slotCounterA = 7;
  std::memcpy(block + kSlotRecordOffset + kSlotCounterAOffset, &slotCounterA, sizeof slotCounterA);

  const uint64_t anime = RunAccessor(&pes_fn_140A48910, *world, kTestSlot);
  const uint64_t man = RunAccessor(&pes_fn_140A48A10, *world, kTestSlot);
  const uint64_t slot = RunAccessor(&pes_fn_140A489F0, *world, kTestSlot);
  EXPECT_EQ(anime, world->AnimeVa(kTestSlot));
  EXPECT_EQ(man, world->ManVa(kTestSlot));
  EXPECT_EQ(slot, world->SlotVa(kTestSlot));

  PesState s = MappedState(*world);
  EXPECT_EQ(s.rd(anime + kAnimeClassOffset, 1), animeClass);
  EXPECT_FLOAT_EQ(s.rdf(man + kManPosXOffset), manPosX);
  EXPECT_EQ(s.rd(slot + kSlotCounterAOffset, 4), slotCounterA);
}

}  // namespace
