#pragma once

// PES 2021's presser picker (FUN_140639a20):
// the pass-1 score of each man of a side against the carrier point, the
// pass-2 carrier-distance window and angle offset, the stable bucket sort, and
// the angle classification (FUN_140639600) that gives a candidate its press
// code 0..3 for the carrier's bearing.
//
// Pure: arguments plus the constants. The caller builds the men's positions,
// slots and role entries from its own state. Angles are degrees, distances
// metres. Constants come from the install's PES2021.exe image, read by
// LoadPressConstants at the addresses named below.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"

namespace pes {

// Every float here is a .rdata constant of PES2021.exe (verified in the image).
struct PressConstants {
  float rampStart_deg;        // 0x14253ee30 (100): pass-1 ramp start; also the score outside the cone
  float rampEnd_deg;          // 0x14259be6c (160)
  float rampSpan_deg;         // 0x14259a764 (60)
  float radiusBase_m;         // 0x142588210 (10): cone radius and the in-cone scale
  float halfTurn_deg;         // 0x142598580 (180)
  float fullTurn_deg;         // 0x14259858c (360)
  float radToDeg;             // 0x14259be40 (57.2957763671875)
  float notSlotPenalty;       // 0x14259d634 (10000): the man holds no press slot
  float notRolePenalty;       // 0x1425cee24 (250000): role is not the presser role
  // Pass-2 gates and adds (FUN_140639a20).
  float pass2DelayDistSq;     // 0x1425a9464 (36): squared distance for the DELAY slot step
  float pass2ZeroRoleDistSq;  // 0x1425c6780 (81): squared distance for the +pass2SameCodeAdd role case
  float pass2SameCodeAdd;     // 0x14259bdf4 (8): score add when no code change is made
  float pass2ActionAdd;       // 0x1425656c0 (4.0): score add for a man not in DELAY (0x33/0x34)
  float carrierRampStart_m;   // 0x14259be18 (18)
  float carrierRampEnd_m;     // 0x14259be20 (25)
  float carrierRampSpan_m;    // 0x14259bdf0 (7)
  float pass2Lo;              // 0x14258c264 (3): sqrt(score) at or below -> offset ramp 1
  float pass2Hi;              // 0x14259bdec (6): sqrt(score) at or above -> offset ramp 0
  float pass2Div;             // 0x14259d654 (-3): ramp = (root - hi) / div between them
  float offsetBase_deg;       // 0x14259be0c (15)
  float offsetSpan_deg;       // 0x1425a1ff8 (12.5)
  // Angle-code bands (FUN_140639600).
  float slot1Lo_deg;          // 0x14259be38 (55)
  float slot1Hi_deg;          // 0x1425cb488 (145)
  float lowBand_deg;          // 0x14259be30 (45)
  float highBand_deg;         // 0x14259be60 (135)
  float slot2Band_deg;        // 0x1425c48d4 (115)
  float slot3Lo_deg;          // 0x1425c6608 (-55)
  float slot3Hi_deg;          // 0x1425cee28 (-145)
  // Speed and angle-band helpers (FUN_14042d470, FUN_140a92b90).
  float secPerMinute;         // 0x14259a764 (60)
  float metresPerKm;          // 0x14256ae34 (1000)
  float framesPerSec;         // 0x14351ae54 (54): .data initializer, read as the image holds it
  // Loose-ball chase additions (FUN_140a75be0 class, FUN_140639100 target;
  // press-life 7/9). The §5 table radius serves the skipped §3.2 multi-press
  // only: the winner's (18+7r) pick radius implies it.
  float speedCut0_kmh;     // 0x1425a8a28 (0.05): class 0/1 cut
  float speedCut1_kmh;     // 0x14259bdf4 (8): class 1/2 cut (== pass2SameCodeAdd's VA)
  float speedCut2_kmh;     // 0x1425a1ffc (13.5): class 2/3 cut
  float speedCut3_kmh;     // 0x1425c48c0 (22): class 3/4 cut
  float targetTickRate;    // 0x142588200 (0.1): k = fps*rate + bias ticks
  float targetTickBias;    // 0x142565600 (0.5)
};

// Role-class table at 0x1426484f0 (94 u32 entries). Bits read by the writer:
// 3 = FUN_140a6e150, 5 = FUN_140a6e5e0, 6 = FUN_140a6e580, 16 = FUN_140a6e340.
inline constexpr int kRoleCount = 94;
inline constexpr int kRoleBitA = 3;
inline constexpr int kRoleBitB = 5;
inline constexpr int kRoleBitC = 6;
inline constexpr int kRoleBitJostle = 16;
struct PressRoleTable {
  std::array<uint32_t, kRoleCount> bits{};
};

std::optional<PressRoleTable> LoadPressRoleTable(const PeImage& exe, std::string* error);

// True when `role` (0..93) carries `bit`. Roles past the table give false (FUN_140a6e150 and friends).
bool PressRoleHas(const PressRoleTable& t, int role, int bit);

// FUN_140a40200's 4-deep ring of recent (id, time) stamps. `head` is the newest slot.
struct PressStampRing {
  struct Stamp {
    int id = 0;
    uint32_t first = 0;
    uint32_t last = 0;
  };
  std::array<Stamp, 4> slots{};
  int head = 0;
};

// FUN_140a40200: a new id stamps the head slot; it advances the head first when the
// old head's first stamp is older than `t`. Same id as the head is a no-op.
void PressStampPush(PressStampRing& ring, int id, uint32_t t);

// True when the signed angle `deg` wraps to an |angle| outside [eps, halfTurn - eps]
// (FUN_140a92b90 returns 1). Used for the boundary test on a bearing.
bool PressAngleOutsideBand(const PressConstants& c, float deg, float eps);

// Speed in km/h from a per-frame distance: (framesPerSec * 60 * 60 / 1000) * perFrame
// (FUN_14042d470). // ponytail: unit reading is inferred from the constants; the
// caller's threshold is the only check of the scale.
float PressSpeedKmh(const PressConstants& c, float perFrame);

// A bucket entry: a man index (0xff = empty) and the score the sort orders by.
struct PressCandidate {
  int man;
  float score;
};

inline constexpr int kPressNoMan = 0xff;
// The role-table value that marks the presser (FUN_140639a20 compares to 0x3a).
inline constexpr int kPresserRole = 0x3a;
// The team-state values the loose-ball writer writes (FUN_140536fc0/36690):
// PRESS 0x34 for the picked man, NOTHING 0 for the reset rest.
inline constexpr int kPressState = 0x34;
inline constexpr int kNothingState = 0;

std::optional<PressConstants> LoadPressConstants(const PeImage& exe, std::string* error);

// Stable ascending sort by score (the picker's FUN_140a79440 insertion branch).
void SortPressBucket(std::vector<PressCandidate>& bucket);

// Bearing of `to` seen from `from` in [0, 360): atan2(dx, dz) in degrees, with
// the picker's wrap (FUN_140439e50). Coincident points give 0.
float PressBearingDeg(const PressConstants& c, float fromX, float fromZ, float toX, float toZ);

// Wraps an angle into [0, 360).
float PressWrapDeg(const PressConstants& c, float deg);

// Pass-1 angle: bearing minus the side reference (FUN_140a71540), both in
// [0, 360), folded to a signed value in [-180, 180].
float PressSideAngleDeg(const PressConstants& c, float bearingDeg, float referenceDeg);

// Pass-2 carrier ramp: 0 up to carrierRampStart_m, linear to 1 at carrierRampEnd_m.
float PressCarrierRamp(const PressConstants& c, float carrierDist_m);

// Pass-2 angle offset in degrees for a candidate whose sqrt(score) is `root`.
float PressOffsetDeg(const PressConstants& c, float root);

// Pass-1 score of one man: squared distance, plus the in-cone term (or
// rampStart outside the cone), plus the role and slot penalties. `angleDeg` is
// PressSideAngleDeg's signed result. `slot` is the press slot index, -1 if none.
float PressManScore(const PressConstants& c, float distSq, float angleDeg, int role, int slot);

// Angle code 0..3 of FUN_140639600 for a signed angle `angleDeg` (the carrier
// bearing offset) and the press slot index 0..3, or -1 for none.
int PressAngleCode(const PressConstants& c, float angleDeg, int slot);

// Pass-2 state of FUN_140639a20 (`param_3`): codeTaken marks an angle code as
// used, pressCode[man % 11] is the code a man holds (-1 none), and
// zeroCodeFlag is set once a man holds code 0.
struct PressPickState {
  std::array<int, 4> codeTaken{};
  std::array<int, 11> pressCode{{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
  bool zeroCodeFlag = false;
};

// Inputs of pass 2 for one side. Men are team indices 0..21 (side*11 + slot);
// role is the side's role table indexed by man % 11.
struct PressPickInput {
  float ballX = 0, ballZ = 0;                         // MA ball copy, MA+0x2482c / +0x24834 (stand-in: the ball now)
  float sampleX = 0, sampleZ = 0;                     // BallInfo sample 0, FUN_140a57800(G+0x40): the ball now
  float refX = 0, refZ = 0;                           // func_0x140a57800: carrier reference
  float referenceDeg = 0;                             // FUN_140a71540: side reference, [0, 360)
  bool presserFlag = false;                           // team+0xf1
  std::array<float, 22> posX{}, posZ{};               // player +0x55c / +0x564
  std::array<bool, 22> onField{};                     // MI+0x1748 on-pitch flag
  std::array<int, 22> action{};                       // player +0x5d0
  int excludedMan = -1;                               // FUN_140a62690 result
  std::array<int, 11> role{};                         // team+0xa2a0
  std::array<int, 4> pressSlotMan{{-1, -1, -1, -1}};  // team+0xa548: man in each slot
};

// Pass 1 of FUN_140639a20 for one side (`side` is the 11-man block index).
// Builds the side's pass-1 bucket: on-field men in the radius-independent
// score, sorted ascending by score. Uses `in.role`, `in.pressSlotMan`, and
// `in.onField` (the side's 11-man block). Ball and reference come from `in`.
std::vector<PressCandidate> RunPresserPass1(const PressConstants& c, const PressPickInput& in,
                                            int side);

// FUN_140a62610 via FUN_140a62690: the first on-field man of `side` whose team
// flag is zero, as a team index, or kPressNoMan.
int PressExcludedMan(int side, const std::array<bool, 22>& onField, const std::array<int, 11>& flag);

// Pass 2 of FUN_140639a20 for one side. `pass1Bucket` is the side's pass-1
// bucket sorted ascending by score. Returns the side's output bucket, sorted
// ascending by score.
std::vector<PressCandidate> RunPresserPicker(const PressConstants& c, const PressPickInput& in,
                                             const std::vector<PressCandidate>& pass1Bucket,
                                             PressPickState& state);

// One man as the candidate fill (FUN_140639830) sees him. Every gate is
// precomputed by the caller from match/team state; the fill only tallies.
struct PressFillMan {
  bool onField = false;        // team+0x1748[man] != 0
  bool excluded = false;       // man == FUN_140a62690(team, side)
  bool inDistance = false;     // carrier-to-man distSq <= 462.25 (0x1425cee20)
  bool carrierTagged = false;  // carrier+0x5d0 == 0x34
  bool inFirstTen = false;     // local index k <= 10
  bool teamFlagZero = false;   // team+0xb88c[k] == 0 (the k>10 branch)
  bool inStampedSet = false;   // FUN_140a41190(team, man) >= 0
  bool notInA558 = false;      // team+0xa558 != man
  bool gate479470 = false;     // FUN_140479470(team, man) != 0
  int angleCode = -1;          // FUN_140639600 code, 0..3
  // Table d^2 (m^2) of the man: MA+0x24710 + (rank + bucket)*8, float. FUN_14044e0d0
  // returns sqrtf of it (the import behind _DAT_1425301f0 is ucrtbase sqrtf), so the
  // fill compares the distance in metres.
  float tableDistSq = 0;
};

struct PressFillResult {
  int count = 0;               // men that passed every gate
  bool flagA = false;          // a passer has sqrt(tableDistSq) < 4.0 (0x1425656c0)
  bool flagB = false;          // a passer has angleCode 0
  std::array<bool, 4> slotSet{};  // angle codes used by passers
};

// Gate and tally for FUN_140639830 over the side's 11 men (local index k).
PressFillResult PressCandidateFill(const PressConstants& c, const std::array<PressFillMan, 11>& mans);

// ---- Second presser: the coordinator's holder, class and maintain gates (PES FUN_140632bc0,
// FUN_140632ae0, FUN_140632e70; coordinator FUN_140634470). ----

// Team-man and side-man counts of the coordinator's tests (PES compares men with 0x16, slots with 0xb).
inline constexpr int kPressTeamMen = 22;
inline constexpr int kPressSideMen = 11;
// The assignment-table word of a presser: PES T+0xa2a0[slot] == 0x34 (state 52, PRESS).
inline constexpr int kPressAction = 0x34;
// Slot flags in T+0xb88c[slot] that the maintain gate reads (PES FUN_140632e70: 3 and 5).
inline constexpr int kPressSlotFlagThree = 3;
inline constexpr int kPressSlotFlagFive = 5;
// Classes FUN_140632ae0 returns.
inline constexpr int kPressClassOne = 1;
inline constexpr int kPressClassFour = 4;
inline constexpr int kPressClassFive = 5;
inline constexpr int kPressClassSix = 6;
inline constexpr int kPressClassSeven = 7;

// True for the PES player kinds that cannot hold the press: 0x11, 0x14 and 0x50 (a bitmask over
// kind - 0x11), plus 0x52 and 0x57 (FUN_140632bc0, FUN_1406324a0).
bool PressHolderKindExcluded(int pesKind);

// PES FUN_140632bc0: the side's holder for the press pass. The possession man (MA+0x2601C through
// FUN_140a627e0(ctx, 0)) wins when he is a team man; otherwise the other side's carrier (+0x78)
// stands in unless his kind is excluded. kPressNoMan when neither holds.
int PressHolder(int possessionMan, int otherCarrier, int otherCarrierKind);

// Inputs of FUN_140632ae0, in the order it tests them. Each callee result is the caller's.
struct PressClassInput {
  bool flagFiveMan = false;     // FUN_1406324a0(ctx, team) names a team man
  bool flagDNone = false;       // FUN_140a63950(ctx, side, 0xd, 0xff)
  bool testFun632850 = false;   // FUN_140632850(...) != 0 (not decoded: caller)
  bool teamB716 = false;        // ctx+0xb716 != 0
  bool testFun632320 = false;   // FUN_140632320(...) (not decoded: caller)
};

// PES FUN_140632ae0: the press class of a side.
int PressClass(const PressClassInput& in);

// Inputs of FUN_140632e70, the coordinator's maintain test for one side-local slot. The caller reads
// every field from the side's state; `man` is side * 11 + slot.
struct PressMaintainInput {
  bool inPlay = false;                // MI+0x116c == 1
  int man = kPressNoMan;              // side * 11 + slot
  int slot = kPressNoMan;             // side-local slot (param_4)
  int tableWord = 0;                  // T+0xa2a0[slot]
  int flag = 0;                       // T+0xb88c[slot] (0 for slots past 10)
  bool keeperHoldsBall = false;       // FUN_140a53f90(ctx, 0xff) != 0 (not decoded: caller)
  int holderNow = kPressNoMan;        // FUN_140632bc0(side, ctx)
  int holderRecorded = kPressNoMan;   // the coordinator's holder (param_1[0])
  int flag5Man = kPressNoMan;         // FUN_1406324a0(ctx, team) (partly decoded: caller)
  int carrier = kPressNoMan;          // T+0x78
  bool ballLifted = false;            // (ball+0x18 > 1.5) or the +4 height term, DAT_14259bddc
  int matchUpMan = kPressNoMan;       // FUN_140a62960(side), the MATCH_UP man (not decoded: caller)
  int carrierRecorded = kPressNoMan;  // the coordinator's carrier (param_1[1], team+0x7c)
  int flag3Stamp = kPressNoMan;       // T+0x4b10
  bool refused = false;               // FUN_140633350(...) != 0 (not decoded: caller)
  bool distanceRefused = false;       // the distance sub-branch verdict (not ported: caller)
  uint32_t recordAgeFrames = 0;       // T+0xa2cc[slot] stamp: last - first
  uint32_t windowFrames = 0;          // FUN_141e532e0() * 2
  float scoreToCarrier = 0.0f;        // FUN_1404c8ea0(team, carrierRecorded, man) (not decoded: caller)
};

// PES FUN_140632e70: true keeps the presser (PES returns 0), false releases him (PES returns 1).
// PARTIAL: the distance sub-branch is the caller's verdict until its helpers are ported.
bool PressPresserKept(const PressConstants& c, const PressMaintainInput& in);
// --- Loose-ball chase (press-life 6/7/9) ---

// FUN_140639a20's pick radius in metres: carrierRampStart_m + carrierRampSpan_m
// times the score ramp of the winner's sqrt(score) `root` (1 at or below
// pass2Lo, 0 at or above pass2Hi, linear on pass2Div between).
float PressPickRadius_m(const PressConstants& c, float root);

// The writer's gate on the ball (FUN_140536FC0, 0x140537332; S1Watch's decode, local://pes-re-far-chase.md §1): with no
// carrier, the ball sample (BallInfo sample 0) must lie within kPressBallGate_m of the MA ball copy, or the writer picks
// and renews nobody. The gate compares squared distance with 169 = 13^2 (0x1425C6788).
inline constexpr float kPressBallGate_m = 13.0f;
bool PressBallGatePasses(const PressPickInput& in);

// The writer's winner: -1 when the gate refuses; else the top pass-1 man when he is within the pick radius
// (PressPickRadius_m). A fresh pick and a renewal are the same test.
int PressWinner(const PressConstants& c, const PressPickInput& in, const std::vector<PressCandidate>& bucket);

// FUN_140639100's no-carrier arm: true when the ball sample at k ticks is at
// least as near the ball now as the presser's base (squared metres, ties go
// to the ball). Points are (x, z) pitch metres. No constants: the comparison
// is pure arithmetic on the caller's points.
struct PressPoint {
  float x = 0, z = 0;
};
bool PressTargetIsBall(PressPoint sample, PressPoint ball, PressPoint base);

// The sample index: int(framesPerSec * targetTickRate + targetTickBias) (54*0.1+0.5 = 5).
int PressTargetTick(const PressConstants& c);

// FUN_140a75be0's speed class 0..4 of `speedKmh` over the four cuts.
int PressSpeedClass(const PressConstants& c, float speedKmh);

// The chase command speed in m/s: the arrival slope (dist_m * slope) capped
// one band above the current class, the caps in m/s from the cuts (class 4 is
// uncapped). The +1 lead is GF's: PES's solver paces AT the class
// (FUN_1406b9270, undecoded), which would stand a standing presser still.
float PressCommandSpeed_ms(const PressConstants& c, float dist_m, float speedKmh, float slope);

}  // namespace pes
