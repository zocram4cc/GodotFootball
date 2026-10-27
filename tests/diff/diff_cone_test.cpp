// Differential test: FUN_140A53DF0's inner cone test vs
// pes::marktable::ConePasses.
//
// The corpus is generated at test time from the user's executable by
// tools/dev/diffharness (specs/cone.json; provenance: DiffHarnessRE
// specs/cone.json -- CAND/HOLD 16-byte pos structs, XMM3=head-deg, stack
// coneDeg=90.0/outerLen=12.0/innerWord, no stubs: bearing/atan2 run real).
// This binary only reads build/diff/cone.jsonl and calls GF's port.
//
// Exactness: the live bearing is PES's own (0x140439EF0 over the two men's records, run in place), so every case
// must match bit for bit; there is no atan2 margin.
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "diff_json.hpp"
#include "onthepitch/pesmarktable.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"

namespace {

// Constants come from the PACKED executable (its section table addresses
// .rdata/.text correctly); the corpus comes from the unpacked image
// (PES_EXE). PeImage cannot read the dump's stale section table.
std::string ExePath() {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return "";
  return std::string(root) + "/PES2021.exe";
}

float BitsToFloat(const diff::Value& v) {
  float f = 0.0f;
  const uint32_t bits = v.AsBits();
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// A man at (x, z) in his record; slot 0 is the candidate and slot 1 the holder.
uint8_t* Place(pes::PesWorld& world, int slot, float x, float z) {
  world.BindSlot(slot);
  uint8_t* man = world.ManRecord(slot);
  pes::StoreF32(man, pes::kManPosXOffset, x);
  pes::StoreF32(man, pes::kManPosZOffset, z);
  return man;
}

TEST(DiffCone, ConeTestMatchesPES) {
  if (ExePath().empty()) GTEST_SKIP() << "PES21_ROOT not set";
  auto cases = diff::ReadCorpus(diff::CorpusPath("cone"));
  if (cases.empty()) GTEST_SKIP() << "no corpus (generator skipped)";
  std::string err;
  auto exe = pes::PeImage::Open(ExePath(), &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::marktable::LoadMarkTableConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  pes::PesWorld world;
  for (const auto& c : cases) {
    const float cx = BitsToFloat(c.in.At("cand_x")), cz = BitsToFloat(c.in.At("cand_z"));
    const float hx = BitsToFloat(c.in.At("hold_x")), hz = BitsToFloat(c.in.At("hold_z"));
    const float head = BitsToFloat(c.in.At("headbits"));
    const float inner = BitsToFloat(c.in.At("innerbits"));
    const bool gf = pes::marktable::ConePasses(Place(world, 0, cx, cz), Place(world, 1, hx, hz), head, inner, *k, world);
    const int pes = c.out.At("cone_hit").AsInt();
    EXPECT_EQ(gf, pes != 0) << "cand (" << cx << "," << cz << ") holder (" << hx << "," << hz << ") head " << head;
  }
}

}  // namespace
