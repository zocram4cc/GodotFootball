// Differential test: FUN_140AF2DA0's full function vs pes::ConditionBand.
//
// The corpus is generated at test time from the user's executable by
// tools/dev/diffharness (specs/band.json, provenance: DiffHarnessRE
// specs/band.json): the 0x17C player-edit record in REC, the cond byte as
// input b, the counter modelled. The observable is REC+0x18 bits 28-30 --
// the 0x30 writer packs the band into rec[6], the fuller boundary (EBX at
// the slice end equals it on every proven input). Bit-identical, plus the
// cond echo (REC+0x179 carries the input byte back).
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "diff_json.hpp"
#include "pes/condition.hpp"
#include "pes/exe.hpp"

namespace {

std::string ExePath() {
  const char* exe = std::getenv("PES_EXE");
  if (exe) return exe;
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return "";
  return std::string(root) + "/PES2021.exe";
}

TEST(DiffCondBand, LadderMatchesPESBitForBit) {
  if (ExePath().empty()) GTEST_SKIP() << "PES21_ROOT/PES_EXE not set";
  auto cases = diff::ReadCorpus(diff::CorpusPath("band"));
  if (cases.empty()) GTEST_SKIP() << "no corpus (generator skipped)";
  std::string err;
  auto exe = pes::PeImage::Open(ExePath(), &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadConditionConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  for (const auto& c : cases) {
    const unsigned cond = static_cast<unsigned>(c.in.At("b").AsInt());
    // The 0x30 writer: rec[6]=(rec[6]&0x8FFFFFFF)|((band&7)<<28).
    const int pesBand = (c.out.At("w6").AsInt() >> 28) & 7;
    EXPECT_EQ(pes::ConditionBand(*k, cond), pesBand) << "cond " << cond;
    EXPECT_EQ(c.out.At("cond_echo").AsInt(), static_cast<int>(cond)) << "echo " << cond;
  }
}

}  // namespace
