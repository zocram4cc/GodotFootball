// PES 2021's step 9 early reaction branch (FUN_1404fdd20, asm 0x1404fdf20-
// 0x1404fe314): the per-receiver gates and the table word T+0x6a4[k]. Pure
// functions over the facts the gates read, verified against the disassembly.
// Nothing calls this yet (T2b slice A): the wiring slice supplies the facts
// from the match, and the block-B loop that calls it is not exact yet.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace pes {

class PeImage;

// PES's "no slot" value (0xff): no holder, no actor, or a key that names none.
inline constexpr int kNoSlot = 0xff;
// The global slots are 0..21: side * 11 + starter index. FUN_1404fdd20 admits
// the current actor only up to this slot (0x1404fdd8f).
inline constexpr int kLastSlot = 0x15;
inline constexpr int kStartersPerSide = 11;

// The float constants of the early branch, read from the install's .rdata by
// LoadEarlyReactionConstants. Each names its VA.
struct EarlyReactionConstants {
  // 0x14259d624 = 54.0. FUN_141e532d0 returns the cell DAT_14351ae54, and
  // FUN_141e532f0 is its only writer (it stores this constant). The early branch
  // reads it as its one "draw": the frame window (int)(draw + half), the ramp
  // scale (int)(draw * half + half), and the ball-track step (int)(draw * 0.3 + half).
  float draw = 0.0f;
  float half = 0.0f;             // 0x142565600 = 0.5
  float heightAbove = 0.0f;      // 0x14258c264 = 3.0: the ball sample height test
  float shortScale = 0.0f;       // 0x142590d98 = 0.3
  float thresholdLow = 0.0f;     // 0x1425c4894 = 0.02
  float thresholdKindTwo = 0.0f; // 0x1425c4898 = 0.049999997: kind 2 without BPL
  float rampHigh = 0.0f;         // 0x1425c48a0 = 0.15
  float rampFull = 0.0f;         // 0x14256ae1c = 1.0
};

// The facts one receiver's pass reads. Slots are global (0..21, 0xff = none).
struct EarlyReactionFacts {
  int receiver = 0;               // gl = ESI, the receiver's global slot
  bool receiverOnPitch = false;   // MI+gl+0x1748 != 0 (0x1404fdfd2)
  int side = 0;                   // FDD20's side parameter
  int holder = kNoSlot;           // FUN_140a627e0(MA, 0): the holder slot (0x1404fde1d)
  bool reactionP5 = false;        // FDD20's p5 ([RSP+0x1c0])
  int currentActorSlot = kNoSlot; // FUN_140a62ea0(MA, E+0x180700); admitted 0..21

  // E0 = EventAt(0), the newest event.
  uint32_t kind = 0;              // payload +0
  int32_t frame = 0;              // payload +0x10
  int actorSlot = kNoSlot;        // the slot of payload +4 (FUN_140a62ea0)
  int32_t subKind = 0;            // payload +0x40
  uint8_t byte45 = 0;             // payload +0x45 (kind 2's DL flag)

  // E1 = EventAt(1). Absent, it reads the static record whose kind is 0
  // (DAT_1436f3d60 +0), which the frame test can never match.
  uint32_t kind1 = 0;             // payload +0
  int32_t frame1 = 0;             // payload +0x10
  int actorSlot1 = kNoSlot;       // the slot of payload +4 (0xff when no valid key)

  // The ball record B = G+0x40.
  bool ballFlag80 = false;        // B+0x80 != 0
  int sampleCount = 0;            // B+0x40 (FUN_140a6cef0(B, 0))
  // Sample j's +4 float, the height, for j = 0..249 (FUN_140a6cf70 puts sample
  // j at B+((j*5)+0x11)*8 and clamps j >= 250 to 249). The scan reads j = 1..n+1,
  // so the span must hold min(n+1, 249) + 1 entries.
  std::span<const float> sampleHeights;
  float ballSpeed = 0.0f;         // B+0x2940
};

// What FDD20 does with one receiver.
enum class EarlyOutcome : uint8_t {
  kSkip,   // 0x1404fe90a: no table word and no main pass for this receiver
  kMain,   // 0x1404fe319: the receiver goes to the main branch (not ported here)
  kStore,  // 0x1404fe304: T+0x6a4[k] = value
};

struct EarlyResult {
  EarlyOutcome outcome = EarlyOutcome::kSkip;
  int value = 0;  // the table word, when kStore
};

// FDD20's entry guard (0x1404fdd8f): the current actor must map to a slot
// 0..21, or FDD20 returns with no writes at all.
bool EarlyReactionAdmitted(int currentActorSlot);

// One receiver's pass through FDD20's early branch (0x1404fdfc4-0x1404fe314).
EarlyResult EvaluateEarlyReaction(const EarlyReactionFacts& facts,
                                  const EarlyReactionConstants& constants);

// The constants from the install's .rdata (PeImage::ReadVa, 4-byte floats).
std::optional<EarlyReactionConstants> LoadEarlyReactionConstants(const PeImage& exe,
                                                                 std::string* error);

}  // namespace pes
