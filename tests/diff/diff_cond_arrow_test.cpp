// Differential test: FUN_141FC1380's inverse map vs pes::ConditionArrow.
//
// The corpus is generated at test time from the user's executable by
// tools/dev/diffharness (specs/cond_arrow.json); this binary only reads
// build/diff/cond_arrow.jsonl and calls GF's port. Bit-identical.
#include <gtest/gtest.h>

#include "diff_json.hpp"
#include "pes/condition.hpp"

namespace {

TEST(DiffCondArrow, InverseMapMatchesPESBitForBit) {
  auto cases = diff::ReadCorpus(diff::CorpusPath("cond_arrow"));
  if (cases.empty()) GTEST_SKIP() << "no corpus (generator skipped)";
  for (const auto& c : cases) {
    const int band = c.in.At("band").AsInt();
    const int arrow = pes::ConditionArrow(band);
    EXPECT_EQ(arrow, c.out.At("arrow").AsInt()) << "band " << band;
    // PES's function also stores the arrow through FUN_1408C7710 ([RCX+0x58]);
    // the store carries the same value as the return (condition-bands section 2).
    EXPECT_EQ(c.out.At("stored").AsInt(), c.out.At("arrow").AsInt()) << "band " << band;
  }
}

}  // namespace
