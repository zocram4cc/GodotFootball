// PES 2021's step 9 block B factor writer, FUN_140503430 (asm 0x140503430-
// 0x14050403f), which FUN_1405027A0 calls once per on-pitch slot with the
// 5-float block at 27A0's [RSP+0x68] (27A0 asm 0x140502b42-0x140502b57). The
// block is zeroed by 27A0 before the call; this writes block[0..4]:
//   block[0] = the distance ramp of the slot's closest-point reach,
//   block[1] = the ball-speed ramp (r), block[2] = the second ramp over the
//   reach, block[3] = the angle factor (times the tail's sign factor),
//   block[4] = the p5 gate (and the DL override), maxed with the p6 term.
// Pure functions over the facts the asm reads; constants and the two tables are
// read from the install's image. Unwired: nothing calls ComputeBlock9 yet.
// Two holder predicates (FUN_140a55d40, FUN_140479470) are inputs, not ported:
// ponytail: their decode (PES 0x140a55d40 / 0x140479470, the team-slot table at
// M+0x10e8) is the next step; the caller supplies their results.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace pes {

class PeImage;

namespace reactionblock {

// x and z are the ground plane; y is the height (PES +0x560 / sample +4).
struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

// Every float constant FUN_140503430 and its helpers read, with the VA each is
// loaded from (the install's .rdata). Angles are in degrees.
struct Constants {
  float turnPeriod = 0.0f;      // 0x14259858c = 360
  float turnPeriodNeg = 0.0f;   // 0x142598598 = -360
  float halfTurn = 0.0f;        // 0x142598580 = 180
  float halfTurnNeg = 0.0f;     // 0x1425ddffc = -180
  float quarterTurn = 0.0f;     // 0x14259be5c = 90
  float tableScale = 0.0f;      // 0x1426550e8 = 512 (sine table steps per quarter turn)
  float degPerRad = 0.0f;       // 0x14259be40 = 57.295776
  float twoPiRad = 0.0f;        // 0x1425795e4 = 6.2831855 (added to negative radians)
  float one = 0.0f;             // 0x14256ae1c = 1
  float minusOne = 0.0f;        // 0x142588218 = -1
  float half = 0.0f;            // 0x142565600 = 0.5
  float atanCubic = 0.0f;       // 0x1425903b4 = 1/3
  float piOver4 = 0.0f;         // 0x142656158
  float atanMidBase = 0.0f;     // 0x14265615c = atan(1.5)
  float piOver2 = 0.0f;         // 0x1425a35a0
  float minusPiOver2 = 0.0f;    // 0x14260b394
  float pi = 0.0f;              // 0x142578480
  float minusPi = 0.0f;         // 0x142609f98
  float atanSlope = 0.0f;       // 0x14259bddc = 1.5
  float closestEps = 0.0f;      // 0x14259bdc4 = 1.1920929e-07
  float drawFloat = 0.0f;       // 0x14259d624 = 54: FUN_141e532d0's cell (its only writer stores this)
  int drawInt = 0;              // DAT_14351ae50 = 0x36 (FUN_141e532f0's int store); equals drawFloat
  float speedScale = 0.0f;      // 0x14259bdd0 = 25/36: the motion scale, divided by drawFloat
  float speedK1 = 0.0f;         // 0x14259a764 = 60
  float speedK2 = 0.0f;         // 0x14256ae34 = 1000
  float ramp1Low = 0.0f;        // 0x14259be0c = 15
  float ramp1Top = 0.0f;        // 0x14259be1c = 20
  float ramp1Step = 0.0f;       // 0x14259222c = 5
  float ramp2Low = 0.0f;        // 0x142588210 = 10
  float ramp2High = 0.0f;       // 0x1425c48c0 = 22
  float ramp2Div = 0.0f;        // 0x14259be04 = 12
  float forcedScale = 0.0f;     // 0x14259bde8 = 3.5
  float forcedLow = 0.0f;       // 0x14259bdf0 = 7
  float p5Bound = 0.0f;         // 0x14259be34 = 50
  float plainBound = 0.0f;      // 0x14259be30 = 45
  float block0Low = 0.0f;       // 0x142590d98 = 0.3
  float block0Div = 0.0f;       // 0x1425c48ac = 0.9
  float block0High = 0.0f;      // 0x14259d5d4 = 1.2
  float tailFull = 0.0f;        // 0x14259be4c = 70
  float tailZero = 0.0f;        // 0x14259be64 = 140
  float tailDiv = 0.0f;         // 0x1425c4900 = -70
  float tailGate = 0.0f;        // 0x142598578 = 0.2
  float dlScale = 0.0f;         // 0x14259b89c = 0.4
  float pFull = 0.0f;           // 0x14259be20 = 25
  float pDiv = 0.0f;            // 0x1425a946c = -20
};

// The quarter-wave sine table at 0x142655950: 513 floats, T[i] = sin(i*90/512) in
// the install (the reader checks the shape, not the values).
// The anime class table at 0x1426484f0: 94 uint32 words; bit b of word c is the
// predicate FUN_140a6e1c0 (b=4), FUN_140a6e5e0 (b=5), FUN_140a6e540 (b=2),
// FUN_140a6e300 (b=15). Classes outside 0..93 read as 0.
struct ClassTable {
  std::array<uint32_t, 94> words{};
  bool Bit(int cls, int bit) const;
};

struct Tables {
  Constants k;
  std::array<float, 513> sine{};
  ClassTable classes;
};

std::optional<Tables> LoadTables(const PeImage& exe, std::string* error);

// The player record fields FUN_140a6c6d0 and the block read: position (+0x55c),
// motion mode (+0x5b0), velocity (+0x574), facing (+0x554), action (+0x5d0).
struct PlayerFacts {
  Vec3 pos;
  int mode = 0;
  Vec3 vel;
  float facing = 0.0f;
  int action = 0;
};

// The card record fields the block reads: class byte (+1), bytes +0x38 and
// +0x40, words +0x1a and +0x1c, floats +0x60 and +0x11c, and bit 11 of the dword
// at +8.
struct CardFacts {
  int cls = 0;
  int b38 = 0;
  int b40 = 0;
  int w1a = 0;
  int w1c = 0;
  float f60 = 0.0f;
  float f11c = 0.0f;
  bool bit11AtEight = false;
};

// Everything one FUN_140503430 call reads. Slots are the 27A0 values: slot is
// S[0], holder is S[1] (0..21 valid, else the substitution rule applies).
struct Block9Inputs {
  int slot = 0;
  int holder = 0;
  bool p5 = false;             // S+8 byte (27A0 param_5)
  bool p6 = false;             // S+9 byte (27A0 param_6)
  float s4 = 0.0f;             // S[4] ([RSP+0x60]): 27A0's distance when p5
  float s5 = 0.0f;             // S[5] ([RSP+0x64])
  float teamAngleRaw = 0.0f;   // FUN_140a71520(TM, T+0x254)
  float reach = 0.0f;          // sqrt(TM+0x24710 entry of the slot); 0 for slots > 21
  PlayerFacts live;            // FUN_140a48a10(G, slot)
  Vec3 anchor;                 // FUN_140a489f0(G, slot).+0x1414
  Vec3 ballSample0;            // FUN_140a57800(B) = B+0x88
  Vec3 ballV1;                 // B+0x2960 (FUN_140a57730)
  Vec3 ballV2;                 // B+0x2948 (FUN_140a57720)
  int teamSubSlot = 0;         // FUN_140a48a30(G, !side).+0x78, used when holder is invalid
  std::optional<PlayerFacts> liveHolder;   // FUN_140a48a10(G, holder), holder valid only
  std::optional<CardFacts> cardHolder;     // FUN_140a48910(G, holder), holder valid only
  std::optional<CardFacts> cardSub;        // FUN_140a48910(G, sub), sub = holder or teamSubSlot
  float holderSlotTable = 0.0f;            // FUN_140439fe0(TM, holder, slot)
  bool gate34SideOk = false;               // FUN_140a55d40(G+0x180, side, 2) != 0 (action 0x34)
  bool gate34SlotTaken = false;            // FUN_140479470(G+0x180, slot) != 0 (action 0x34)
  // The newest event E0 = FUN_140a3e190(MA+0xE0, 0): kind, payload +0x40, +0x45, +0x46.
  uint32_t kind = 0;
  int32_t sub = 0;
  uint8_t byte45 = 0;
  uint8_t byte46 = 0;
};

// block[0..4] as 27A0 sees them after the call (zero where FUN_140503430 writes nothing).
struct Block9Result {
  std::array<float, 5> block{};
};

// FUN_140503430 over its inputs.
Block9Result ComputeBlock9(const Block9Inputs& in, const Tables& t);

// The helpers, exposed for their red tests.
float Sin(float theta, float s, const Tables& t);                  // FUN_140a92c80
float Atan2Rad(float x, float z, const Tables& t);                 // FUN_140a92d40
float AtanCore(float r, const Tables& t);                          // FUN_140a92dc0
float AngleOf(float x, float z, const Tables& t);                  // FUN_1404a53e0
Vec3 Motion(const PlayerFacts& r, float s, const Tables& t);       // FUN_140a6c6d0
bool ClosestT(const Vec3& a, const Vec3& b, const Vec3& c, float eps, float* t);  // FUN_140a91120

}  // namespace reactionblock
}  // namespace pes
