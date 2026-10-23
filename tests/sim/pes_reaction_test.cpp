// PES 2021's step 9 early reaction branch (FUN_1404fdd20's early path): gates,
// flags, ball-track extension, ramp and word, over synthetic facts with the
// install's constant values. The install-read constants are checked by the
// install-gated test at the end.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "onthepitch/pesreaction.hpp"
#include "pes/exe.hpp"

namespace {

// The constants as the install holds them (see pesreaction.hpp for the VAs).
pes::EarlyReactionConstants Constants() {
  pes::EarlyReactionConstants c;
  c.draw = 54.0f;
  c.half = 0.5f;
  c.heightAbove = 3.0f;
  c.shortScale = 0.3f;
  c.thresholdLow = 0.02f;
  c.thresholdKindTwo = 0.049999997f;
  c.rampHigh = 0.15f;
  c.rampFull = 1.0f;
  return c;
}

// A receiver that stores: slot 3 (side 0), kind 1, actor slot 7, ball speed
// 0.5 (full ramp), and no ball-track extension (BPL is 0 for kind 1).
pes::EarlyReactionFacts StoreFacts() {
  pes::EarlyReactionFacts f;
  f.receiver = 3;
  f.receiverOnPitch = true;
  f.side = 0;
  f.holder = pes::kNoSlot;
  f.reactionP5 = false;
  f.currentActorSlot = 5;
  f.kind = 1;
  f.frame = 100;
  f.actorSlot = 7;
  f.subKind = 0;
  f.byte45 = 0;
  f.kind1 = 0;
  f.frame1 = 0;
  f.actorSlot1 = pes::kNoSlot;
  f.ballFlag80 = true;
  f.sampleCount = 0;
  f.ballSpeed = 0.5f;
  return f;
}

// Kind 2 with payload +0x45 set: BPL, so the ball-track extension runs and the
// threshold is 0.02. Set the heights for the scan.
pes::EarlyReactionFacts BplFacts() {
  pes::EarlyReactionFacts f = StoreFacts();
  f.kind = 2;
  f.byte45 = 1;
  f.ballFlag80 = false;
  return f;
}

pes::EarlyOutcome OutcomeOf(const pes::EarlyReactionFacts& f) {
  return pes::EvaluateEarlyReaction(f, Constants()).outcome;
}

int ValueOf(const pes::EarlyReactionFacts& f) {
  return pes::EvaluateEarlyReaction(f, Constants()).value;
}

// A height vector long enough for the scan: index j is sample j's height.
std::vector<float> Heights(int samples) { return std::vector<float>(samples + 2, 0.0f); }

// A1: kinds 1..3 and 16 store; a store with X = 27 (ramp 1 at speed 0.5) is 27.
TEST(PesReaction, StoresTheFullRampWordForKindOne) {
  const pes::EarlyResult r = pes::EvaluateEarlyReaction(StoreFacts(), Constants());
  EXPECT_EQ(r.outcome, pes::EarlyOutcome::kStore);
  EXPECT_EQ(r.value, 27);
}

TEST(PesReaction, StoresKindsTwoThreeAndSixteen) {
  for (uint32_t kind : {2u, 3u, 16u}) {
    pes::EarlyReactionFacts f = StoreFacts();
    f.kind = kind;
    if (kind == 3) f.subKind = 3;  // CL: no extension
    EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kStore) << "kind " << kind;
    EXPECT_EQ(ValueOf(f), 27) << "kind " << kind;
  }
}

// A2: the kind map. Kinds other than 1..3, 4 and 16 skip; kind 4 goes to the main pass.
TEST(PesReaction, OtherKindsSkipAndKindFourGoesToMain) {
  for (uint32_t kind : {0u, 5u, 6u, 7u, 8u, 9u, 10u, 14u, 15u}) {
    pes::EarlyReactionFacts f = StoreFacts();
    f.kind = kind;
    EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kSkip) << "kind " << kind;
  }
  pes::EarlyReactionFacts four = StoreFacts();
  four.kind = 4;
  EXPECT_EQ(OutcomeOf(four), pes::EarlyOutcome::kMain);
}

// A3: p5 and the actor's side route to the main pass before the kind map.
TEST(PesReaction, P5AndActorSideRouteToMain) {
  pes::EarlyReactionFacts p5 = StoreFacts();
  p5.reactionP5 = true;
  EXPECT_EQ(OutcomeOf(p5), pes::EarlyOutcome::kMain);

  pes::EarlyReactionFacts side = StoreFacts();
  side.currentActorSlot = 12;  // side 1; the receiver is on side 0
  EXPECT_EQ(OutcomeOf(side), pes::EarlyOutcome::kMain);
}

// A4: an off-pitch receiver takes nothing, and so does an actor that is the receiver.
TEST(PesReaction, OffPitchAndActorReceiverSkip) {
  pes::EarlyReactionFacts off = StoreFacts();
  off.receiverOnPitch = false;
  EXPECT_EQ(OutcomeOf(off), pes::EarlyOutcome::kSkip);

  pes::EarlyReactionFacts self = StoreFacts();
  self.actorSlot = self.receiver;
  EXPECT_EQ(OutcomeOf(self), pes::EarlyOutcome::kSkip);
}

// A5: kinds 11 and 12 skip only when the holder is the receiver; otherwise they
// fall through to p5 (0x1404fe0cd-0x1404fe0d5).
TEST(PesReaction, KindElevenAndTwelveSkipOnlyWhenHolderIsReceiver) {
  for (uint32_t kind : {11u, 12u}) {
    pes::EarlyReactionFacts f = StoreFacts();
    f.kind = kind;
    f.holder = f.receiver;
    f.reactionP5 = true;
    EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kSkip) << "kind " << kind;
    f.holder = 9;
    f.reactionP5 = true;
    EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kMain) << "kind " << kind;
  }
}

// A6: kind 13 skips on a valid holder (0x1404fe0bc) and inside the frame window
// (age < 54 with E1 kind 10..14); otherwise it falls through to p5.
TEST(PesReaction, KindThirteenSkipsOnHolderOrFrameWindow) {
  pes::EarlyReactionFacts f = StoreFacts();
  f.kind = 13;
  f.holder = 4;
  f.reactionP5 = true;
  EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kSkip);

  f.holder = pes::kNoSlot;
  f.reactionP5 = true;
  EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kMain);

  f.reactionP5 = true;
  f.frame = 100;
  f.frame1 = 100 - 53;  // age 53 < 54
  f.kind1 = 12;
  EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kSkip);

  f.frame1 = 100 - 54;  // age 54: the window is exclusive
  EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kMain);

  f.frame1 = 100 - 53;
  f.kind1 = 9;  // E1 kind outside 10..14
  EXPECT_EQ(OutcomeOf(f), pes::EarlyOutcome::kMain);
}

// A7: the kind-2 flag. BPL (payload +0x45 != 0) runs the extension and uses the
// 0.02 threshold; without it the threshold is 0.05 and X stays 27.
TEST(PesReaction, KindTwoFlagDecidesExtensionAndThreshold) {
  const std::vector<float> heights = [] {
    std::vector<float> h = Heights(31);
    h[31] = 5.0f;  // j = 31 is the first (from the top) above 3.0: idx = 30
    return h;
  }();
  pes::EarlyReactionFacts f = BplFacts();
  f.sampleCount = 30;
  f.sampleHeights = heights;
  f.ballSpeed = 0.5f;
  EXPECT_EQ(ValueOf(f), 30);  // idx 30 > 27: X = 27 + min(16, 3) = 30

  f.byte45 = 0;  // no BPL: no extension (X = 27), threshold 0.05
  f.ballSpeed = 0.04f;
  EXPECT_EQ(ValueOf(f), 0);  // 0.04 is at or below 0.05: zero ramp

  f.byte45 = 1;  // BPL: threshold 0.02, extension to X = 30
  f.ballSpeed = 0.04f;
  EXPECT_EQ(ValueOf(f), 4);  // (0.04 - 0.02) / (0.15 - 0.02) * 30 = 4.6 -> 4
}

// A8: kind 3 splits on the sub-kind. sub - 3 in 0..1 is CL (no BPL); otherwise
// DL and BPL hold (the unsigned sub - 3 wraps for sub < 3).
TEST(PesReaction, KindThreeSubKindSplitsTheExtension) {
  const std::vector<float> heights = [] {
    std::vector<float> h = Heights(31);
    h[31] = 5.0f;
    return h;
  }();
  pes::EarlyReactionFacts f = StoreFacts();
  f.kind = 3;
  f.ballFlag80 = false;
  f.sampleCount = 30;
  f.sampleHeights = heights;
  f.ballSpeed = 0.5f;

  f.subKind = 3;
  EXPECT_EQ(ValueOf(f), 27);
  f.subKind = 4;
  EXPECT_EQ(ValueOf(f), 27);
  f.subKind = 5;
  EXPECT_EQ(ValueOf(f), 30);
  f.subKind = 2;  // unsigned sub - 3 wraps above 1: BPL
  EXPECT_EQ(ValueOf(f), 30);
}

// A9: the ball-track scan and its cap. The scan runs j = n+1 down to 1 and takes
// the first j above 3.0 (idx = j - 1), or n when none qualifies. The step is 16.
TEST(PesReaction, BallTrackPicksTheHighestSampleAndCapsTheStep) {
  {  // j = 35 and j = 20 both qualify; the scan from the top finds 35: idx 34, X = 34.
    std::vector<float> h = Heights(40);
    h[35] = 5.0f;
    h[20] = 5.0f;
    pes::EarlyReactionFacts f = BplFacts();
    f.sampleCount = 40;
    f.sampleHeights = h;
    EXPECT_EQ(ValueOf(f), 34);
  }
  {  // idx 50 (j = 51): room 23, the step caps at 16: X = 43.
    std::vector<float> h = Heights(51);
    h[51] = 5.0f;
    pes::EarlyReactionFacts f = BplFacts();
    f.sampleCount = 50;
    f.sampleHeights = h;
    EXPECT_EQ(ValueOf(f), 43);
  }
  {  // None qualifies: idx = n = 40 (EDI keeps n): room 13, X = 40.
    std::vector<float> h = Heights(41);
    pes::EarlyReactionFacts f = BplFacts();
    f.sampleCount = 40;
    f.sampleHeights = h;
    EXPECT_EQ(ValueOf(f), 40);
  }
  {  // idx 20 is not above X = 27: no extension.
    std::vector<float> h = Heights(21);
    pes::EarlyReactionFacts f = BplFacts();
    f.sampleCount = 20;
    f.sampleHeights = h;
    EXPECT_EQ(ValueOf(f), 27);
  }
  {  // B+0x80 set: no extension even with a qualifying sample.
    std::vector<float> h = Heights(31);
    h[31] = 5.0f;
    pes::EarlyReactionFacts f = BplFacts();
    f.ballFlag80 = true;
    f.sampleCount = 30;
    f.sampleHeights = h;
    EXPECT_EQ(ValueOf(f), 27);
  }
}

// A10: the ramp is linear between the threshold and the high end, and the word
// truncates. At speed 0.086 the ramp is 0.5077 and 0.5077 * 27 = 13.71 -> 13 (rounding gives 14).
TEST(PesReaction, RampIsLinearAndTruncatesTheWord) {
  pes::EarlyReactionFacts f = StoreFacts();
  f.ballSpeed = 0.086f;
  EXPECT_EQ(ValueOf(f), 13);
}

// A11: the ramp is 0 at or below the threshold and full at or above the high end.
TEST(PesReaction, RampIsZeroAtThresholdAndFullAtHigh) {
  pes::EarlyReactionFacts f = StoreFacts();
  f.ballSpeed = 0.02f;
  EXPECT_EQ(ValueOf(f), 0);
  f.ballSpeed = 0.15f;
  EXPECT_EQ(ValueOf(f), 27);
}

// A12: FDD20's entry guard admits the starter and bench slots 0..21 only.
TEST(PesReaction, EntryAdmitsOnlySlotsZeroToTwentyOne) {
  for (int slot = 0; slot <= 21; ++slot) EXPECT_TRUE(pes::EarlyReactionAdmitted(slot)) << slot;
  EXPECT_FALSE(pes::EarlyReactionAdmitted(22));
  EXPECT_FALSE(pes::EarlyReactionAdmitted(pes::kNoSlot));
}

// The install's constants, read from the exe's .rdata at the VAs the header cites.
TEST(PesReactionInstall, LoadsTheInstallConstants) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the early branch constants come from the install";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto loaded = pes::LoadEarlyReactionConstants(*exe, &err);
  ASSERT_TRUE(loaded.has_value()) << err;
  const pes::EarlyReactionConstants want = Constants();
  EXPECT_FLOAT_EQ(loaded->draw, want.draw);
  EXPECT_FLOAT_EQ(loaded->half, want.half);
  EXPECT_FLOAT_EQ(loaded->heightAbove, want.heightAbove);
  EXPECT_FLOAT_EQ(loaded->shortScale, want.shortScale);
  EXPECT_FLOAT_EQ(loaded->thresholdLow, want.thresholdLow);
  EXPECT_FLOAT_EQ(loaded->thresholdKindTwo, want.thresholdKindTwo);
  EXPECT_FLOAT_EQ(loaded->rampHigh, want.rampHigh);
  EXPECT_FLOAT_EQ(loaded->rampFull, want.rampFull);
}

}  // namespace
