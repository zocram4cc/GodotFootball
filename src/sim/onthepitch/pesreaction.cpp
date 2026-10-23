#include "onthepitch/pesreaction.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>

#include "pes/exe.hpp"

namespace pes {
namespace {

// The payload kinds the early branch reads (FUN_1404fdd20).
constexpr uint32_t kKindStoreFirst = 1;  // 0x1404fe14c-0x1404fe157: kinds 1..3 store
constexpr uint32_t kKindStoreSpan = 2;   //   (kind - 1) <= 2
constexpr uint32_t kKindTwo = 2;         // 0x1404fe1df: DL from payload +0x45
constexpr uint32_t kKindThree = 3;       // 0x1404fe1c8: BPL from payload +0x40
constexpr uint32_t kKindMain = 4;        // 0x1404fe0fb: kind 4 always goes to the main pass
constexpr uint32_t kKindGateFirst = 11;  // 0x1404fdff8: kinds 11..13 gate before p5
constexpr uint32_t kKindGateSecond = 12;
constexpr uint32_t kKindGateThird = 13;
constexpr uint32_t kKindStoreSixteen = 16;  // 0x1404fe154
// The frame window skips when E1's kind is 10..14 (0x1404fe0b0-0x1404fe0b6).
constexpr uint32_t kKindWindowFirst = 10;
constexpr uint32_t kKindWindowSpan = 4;

// FUN_140a6cf70: sample j is at B+((j*5)+0x11)*8 for j < 250, and j >= 250
// reads the sample at B+0x2770, which is sample 249.
constexpr int kSampleLastIndex = 249;

// Kind 3 reads payload +0x40 (sub). BPL holds when sub - 3 (unsigned) is above 1;
// CL (sub - 3 <= 1, 0x1404fe1f4-0x1404fe201) clears it.
constexpr uint32_t kKindThreeSubBase = 3;
constexpr uint32_t kKindThreeSubSpan = 1;

// CVTTSS2SI truncates toward zero, which static_cast<int> matches for in-range
// values. Every value cast here is far inside the int range.
int Truncate(float v) { return static_cast<int>(v); }

bool ReadFloatVa(const PeImage& exe, uint64_t va, float* out) {
  uint8_t bytes[sizeof(float)];
  if (!exe.ReadVa(va, sizeof bytes, bytes)) return false;
  float v = 0.0f;
  std::memcpy(&v, bytes, sizeof v);
  if (!std::isfinite(v)) return false;
  *out = v;
  return true;
}

// The VAs of the early branch's float constants, cited on the struct.
constexpr uint64_t kVaDrawCell = 0x14259d624;
constexpr uint64_t kVaHalf = 0x142565600;
constexpr uint64_t kVaHeightAbove = 0x14258c264;
constexpr uint64_t kVaShortScale = 0x142590d98;
constexpr uint64_t kVaThresholdLow = 0x1425c4894;
constexpr uint64_t kVaThresholdKindTwo = 0x1425c4898;
constexpr uint64_t kVaRampHigh = 0x1425c48a0;
constexpr uint64_t kVaRampFull = 0x14256ae1c;

// Sample j's height (+4), clamped to sample 249 as FUN_140a6cf70 clamps.
float SampleHeight(const EarlyReactionFacts& f, int j) {
  const int index = std::min(j, kSampleLastIndex);
  assert(index < static_cast<int>(f.sampleHeights.size()));
  return f.sampleHeights[static_cast<size_t>(index)];
}

}  // namespace

bool EarlyReactionAdmitted(int currentActorSlot) {
  // 0x1404fdd8f: CMP EAX,0x15; JA exit (unsigned, so 0xff is refused).
  return static_cast<uint32_t>(currentActorSlot) <= static_cast<uint32_t>(kLastSlot);
}

EarlyResult EvaluateEarlyReaction(const EarlyReactionFacts& f,
                                  const EarlyReactionConstants& c) {
  const EarlyResult skip{EarlyOutcome::kSkip, 0};
  const EarlyResult main{EarlyOutcome::kMain, 0};

  // 0x1404fdfd2-0x1404fdfe8: a receiver not on the pitch takes nothing.
  if (!f.receiverOnPitch) return skip;

  // 0x1404fdff8-0x1404fe0cb: kinds 11..13 gate before the p5 test.
  if (f.kind == kKindGateFirst || f.kind == kKindGateSecond) {
    // 0x1404fe0cd-0x1404fe0cf: the holder is the receiver.
    if (f.holder == f.receiver) return skip;
  } else if (f.kind == kKindGateThird) {
    // 0x1404fe06e-0x1404fe0bc: the frame window. The draw's +0.5 truncated
    // gives the window (54 frames). Within it, E1's kind 10..14 skips.
    // (The E1 kind-7 and actor test at 0x1404fe019-0x1404fe066 only picks
    // between two skips here, because 0x1404fe0bc skips any valid holder.)
    const uint32_t window = static_cast<uint32_t>(Truncate(c.draw + c.half));
    const uint32_t age = static_cast<uint32_t>(f.frame) - static_cast<uint32_t>(f.frame1);
    if (age < window && f.kind1 - kKindWindowFirst <= kKindWindowSpan) return skip;
    // 0x1404fe0bc-0x1404fe0c5: [RSP+0x88] <= 0x15 (the holder slot 0..21) skips.
    if (f.holder >= 0 && f.holder <= kLastSlot) return skip;
  }

  // 0x1404fe0d5-0x1404fe0f5: p5 and the actor's side route to the main pass.
  if (f.reactionP5) return main;
  const int actorSide = f.currentActorSlot >= kStartersPerSide ? 1 : 0;  // local_118
  if (actorSide != f.side) return main;
  // 0x1404fe0fb-0x1404fe108: kind 4 goes to the main pass.
  if (f.kind == kKindMain) return main;
  // 0x1404fe14a-0x1404fe157: only kinds 1..3 and 16 store.
  const bool storeKind = f.kind - kKindStoreFirst <= kKindStoreSpan || f.kind == kKindStoreSixteen;
  if (!storeKind) return skip;
  // 0x1404fe15d-0x1404fe175: the actor is the receiver.
  if (f.actorSlot == f.receiver) return skip;

  // DL, CL and BPL (0x1404fe1b1-0x1404fe20c). DL for kind 2 is payload +0x45;
  // for kind 3 it is 1 when sub - 3 > 1, else payload +0x46 == 0. CL is kind 3
  // with sub - 3 <= 1, and BPL = DL && !CL. So for kind 3 BPL is sub - 3 > 1, and
  // payload +0x46 never reaches the value, so it is not read.
  const uint32_t subMinusBase = static_cast<uint32_t>(f.subKind) - kKindThreeSubBase;
  bool bpl = false;
  if (f.kind == kKindTwo) bpl = f.byte45 != 0;
  else if (f.kind == kKindThree) bpl = subMinusBase > kKindThreeSubSpan;

  // X = (float)(int)(draw * half + half) (0x1404fe20f-0x1404fe228).
  float x = static_cast<float>(Truncate(c.draw * c.half + c.half));

  // The ball-track extension (0x1404fe22d-0x1404fe2a6), only when BPL and B+0x80 == 0.
  if (bpl && !f.ballFlag80 && f.sampleCount > 0) {
    // 0x1404fe250-0x1404fe26b: scan j = n+1 down to 1 for the first height
    // above heightAbove. idx is that j - 1, or n when none qualifies (EDI keeps n).
    int idx = f.sampleCount;
    for (int i = f.sampleCount; i >= 0; --i) {
      if (SampleHeight(f, i + 1) > c.heightAbove) {
        idx = i;
        break;
      }
    }
    // 0x1404fe271-0x1404fe27b: extend only when idx > X.
    if (static_cast<float>(idx) > x) {
      const float room = static_cast<float>(idx) - x;  // XMM6 = idx - X
      const float step = static_cast<float>(Truncate(c.draw * c.shortScale + c.half));
      x += std::min(step, room);  // MINSS, 0x1404fe2a2-0x1404fe2a6
    }
  }

  // thr (0x1404fe2aa-0x1404fe2bc): 0.05 only for kind 2 without BPL.
  const float thr = (f.kind == kKindTwo && !bpl) ? c.thresholdKindTwo : c.thresholdLow;
  // The ramp (0x1404fe2c4-0x1404fe2f3) reads the ball's speed, B+0x2940.
  const float v = f.ballSpeed;
  float ramp = 0.0f;
  if (thr < v) {
    ramp = (v < c.rampHigh) ? (v - thr) / (c.rampHigh - thr) : c.rampFull;
  }
  // 0x1404fe2f3-0x1404fe30c: the word is trunc(ramp * X).
  return EarlyResult{EarlyOutcome::kStore, Truncate(ramp * x)};
}

std::optional<EarlyReactionConstants> LoadEarlyReactionConstants(const PeImage& exe,
                                                                 std::string* error) {
  EarlyReactionConstants c;
  const struct {
    uint64_t va;
    float* field;
  } table[] = {
      {kVaDrawCell, &c.draw},
      {kVaHalf, &c.half},
      {kVaHeightAbove, &c.heightAbove},
      {kVaShortScale, &c.shortScale},
      {kVaThresholdLow, &c.thresholdLow},
      {kVaThresholdKindTwo, &c.thresholdKindTwo},
      {kVaRampHigh, &c.rampHigh},
      {kVaRampFull, &c.rampFull},
  };
  for (const auto& entry : table) {
    if (!ReadFloatVa(exe, entry.va, entry.field)) {
      if (error != nullptr) {
        *error = "early reaction constant unreadable at VA 0x" + std::to_string(entry.va);
      }
      return std::nullopt;
    }
  }
  return c;
}

}  // namespace pes
