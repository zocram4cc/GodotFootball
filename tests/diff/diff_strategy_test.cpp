// Differential test: FUN_14051E970's full step vs pes::StrategyStep,
// on the minute-0 mode-0 path GF ports today.
//
// The corpus is generated at test time from the user's executable by
// tools/dev/diffharness (specs/strategy_minute0.json; full E970 input
// closure, provenance: DiffHarnessRE specs/strategy.json -- entered at
// E970 directly, not the E910 wrapper; the ticket's FUN_14051E910 is the
// thin gate, harnessed here is the step). The slot-2 GameRandom stream
// starts at the recorded rng2 state (the decider passes stream 2 in R8D;
// the LCG is slot-independent, so draw results match whatever word holds
// the seed). Strengths land at the side-strided setup words. This binary
// replays each minute-0 mode-0 case through GF's port with the same
// constants (loaded from the user's exe), same starting RNG state, same
// strengths. Bit-identical: strategy, the hold minute R[5] and the
// second-half minute R[6]. Other modes/arms are recorded in the corpus
// for future ports; the gate116c early-return case is asserted separately.
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "diff_json.hpp"
#include "onthepitch/pesrandom.hpp"
#include "onthepitch/pesstrategy.hpp"
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
  uint32_t bits = 0;
  if (v.kind == diff::Value::Kind::Float) {
    bits = v.AsBits();
  } else {
    bits = static_cast<uint32_t>(v.AsInt());
  }
  float f = 0.0f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

TEST(DiffStrategy, MinuteZeroMatchesPESBitForBit) {
  if (ExePath().empty()) GTEST_SKIP() << "PES21_ROOT not set";
  auto cases = diff::ReadCorpus(diff::CorpusPath("strategy_minute0"));
  if (cases.empty()) GTEST_SKIP() << "no corpus (generator skipped)";
  std::string err;
  auto exe = pes::PeImage::Open(ExePath(), &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadStrategyConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  auto rngK = pes::LoadRngConstants(*exe, &err);
  ASSERT_TRUE(rngK) << err;
  int checked = 0;
  for (const auto& c : cases) {
    if (c.in.At("mode").AsInt() != 0 || c.in.At("minute").AsInt() != 0 ||
        c.in.At("mi1168").AsInt() >= 2)
      continue;  // other arms ride along for future ports
    pes::Random rng(*rngK);
    rng.SetSeed(k->rngSlot, static_cast<uint32_t>(c.in.At("rng2").AsInt()));
    pes::StrategyInputs in;
    in.period = pes::StrategyPeriod::FirstHalf;
    in.minute = 0;
    in.ownStrength = BitsToFloat(c.in.At("own_float"));
    in.oppStrength = BitsToFloat(c.in.At("chal_float"));
    pes::StrategyState s;
    s.strategy = c.in.At("strat3_in").AsInt();
    pes::StrategyStep(*k, in, rng, &s);
    const int draws = c.out.Has("_draws") ? static_cast<int>(c.out.At("_draws").items.size()) : -1;
    EXPECT_EQ(s.strategy, c.out.At("strat3").AsInt())
        << "side " << c.in.At("side").AsInt() << " draws " << draws;
    EXPECT_EQ(s.holdUntilMinute, c.out.At("strat5").AsInt());
    EXPECT_EQ(s.secondHalfMinute, c.out.At("strat6").AsInt());
    ++checked;
  }
  EXPECT_GT(checked, 0) << "corpus carries no minute-0 mode-0 cases";
}

TEST(DiffStrategy, StoppedGateKeepsStrategy) {
  // MI+0x116C==1 early return (the E910 wrapper's gate, inside E970):
  // the step leaves strat3/5/6 untouched.
  if (ExePath().empty()) GTEST_SKIP() << "PES21_ROOT not set";
  auto cases = diff::ReadCorpus(diff::CorpusPath("strategy_minute0"));
  if (cases.empty()) GTEST_SKIP() << "no corpus (generator skipped)";
  int checked = 0;
  for (const auto& c : cases) {
    if (c.in.At("mi116c").AsInt() != 1) continue;
    EXPECT_EQ(c.out.At("strat3").AsInt(), c.in.At("strat3_in").AsInt());
    EXPECT_EQ(c.out.At("strat5").AsInt(), c.in.At("strat5").AsInt());
    EXPECT_EQ(c.out.At("strat6").AsInt(), c.in.At("strat6").AsInt());
    ++checked;
  }
  EXPECT_GT(checked, 0) << "corpus carries no gate116c cases";
}

}  // namespace
