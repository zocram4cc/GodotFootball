#pragma once

// PES 2021's forward line in the defensive block: sub_140682770, reached through
// FUN_140681FF0 when the team is out of possession (docs/re/pes-re-attack-level-pull.md
// §2, §5, §8, §9; docs/plans/pes-plan-mentality.md, slice F1).
//
// This file is the pure part: plain inputs in, per-slot targets out. The caller builds
// the slots from the team (F4 wires it into TeamAIController). Every number comes from
// the user's PES2021.exe through pes::PeImage::ReadVa (LoadConstants and
// LoadForwardClasses); none is transcribed here.
//
// Sign: `ballSide` is the team's attack direction times the ball's Y (PES 0x24834 times
// the team's +0x254 sign). Positive means the ball is on the side that picks the last
// member of a line.

#include <array>
#include <optional>
#include <string>

namespace pes {
class PeImage;
}

namespace pes::forward {

constexpr int kSlots = 11;  // a team's slots; sub_140682770 walks 11

// The install's floats. Each is read at the VA of the instruction that loads it.
struct Constants {
  float depthPerLevel;   // 0.25, 0x142592228: depth factor = level * this (0x140682BE4)
  float levelCeiling;    // 4.0, 0x1425656C0: levels below it take level * depthPerLevel (0x140682BD5)
  float fullHigh;        // 1.0, 0x14256AE1C: the full factor (ceiling level, Counter Target; 0x14068330E)
  float rankFirstScale;  // 0.2, 0x142598578: rank factor when mode 1 passes (0x1406832CF)
  float rankOtherScale;  // 0.65, 0x14259D5D0: rank factor when mode 1 fails (0x1406832D9)
  float cfScoreBonus;    // 30.0, 0x142591140: role 9's lineup bump in the protected scan (0x14068305C)
  float countScoreBump;  // -10.0, 0x14259BE84: bump when count > 2 (0x140683089); inert at the guard
  float scanGuard;       // -100000.0, 0x1425C6614: keep-replace guard, only NaN fails it (0x140683091)
};

// The classes the forward arm takes. Read from the jump table at 0x140683D60: class c's
// entry is index c - 1, an RVA from 0x140000000, and the arm is the entry that points at
// 0x140683027 (the arm of the pick loop at 0x140683027).
struct ForwardClasses {
  std::array<bool, 10> arm{};  // index = class (0 GK .. 9 CF)
  bool IsForward(int cls) const { return cls >= 0 && cls < 10 && arm[cls]; }
};

std::optional<Constants> LoadConstants(const PeImage& exe, std::string* error);
std::optional<ForwardClasses> LoadForwardClasses(const PeImage& exe, std::string* error);

// One record of the family sub_140682770 writes per slot: +0x7B88/+0x7B8C out of possession,
// +0x7BE0/+0x7BE4 in possession. `line` is the line the class falls in (0 back, 1 middle,
// 2 forward); `rank` is the player's index in his line, which is slot order (FUN_140682320).
struct Member {
  int line = 0;
  int rank = 0;
};

struct Slot {
  int cls = 0;              // the B class (+0x3FB8 copy); 0 = no player
  bool onPitch = false;
  float lineupScore = 0.f;  // +0x3A9C record, float 0: the protected scan's score
  float y = 0.f;            // +0x3A9C record, float 2 (+0x3AA4): the Y key of the in-possession sort
};

struct Input {
  int level = 0;                     // TeamData+0xB8A4, 0..4 (-2..+2)
  bool possession = false;           // T+0x255: picks the record family and mode 1
  float ballSide = 0.f;              // attack direction * ball Y
  float deep = 0.f;                  // the forward line's all-out-defensive depth (anchor B)
  float high = 0.f;                  // the forward line's high depth (anchor A)
  float floorRef = 0.f;              // [[ctx]]+0x44; the protected forward is floored at -floorRef
  int counterSlot = -1;              // the Counter Target holder (instruction 15); -1 for none
  int forwardCount = 0;              // the forward line's count (+0x128); equal to its active count (+0x129)
  std::array<Slot, kSlots> slots{};
  // The latch's family as PES reads it: slots already written this pass hold this pass's records,
  // the rest hold the previous pass's (FUN_140682770 reads a slot before it writes that slot).
  std::array<Member, kSlots> family{};
};

struct Result {
  int protectedSlot = -1;              // the v21 forward (-1: none)
  std::array<float, kSlots> target{};  // forward depth; meaningful for forward slots only
};

// clamp(level, 0, 4) * 0.25, the full factor at the ceiling
float DepthFactor(const Constants& k, int level);
// deep + (high - deep) * factor
float LineTarget(float factor, float deep, float high);
// deep + (highest - deep) * v108: the blend toward the highest forward target; v108 is the
// clamped position of the +0x3BA4 record in its class band (its meaning is open)
float BlendTarget(float deep, float highest, float v108);
// The forward line of one team for this pass.
Result ForwardLine(const Constants& k, const ForwardClasses& classes, const Input& in);

}  // namespace pes::forward
