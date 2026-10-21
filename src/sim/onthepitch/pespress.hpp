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
  // Target eval, gate, cover and coordinator constants (FUN_140633940, FUN_140632780, FUN_140632c70,
  // FUN_1406324a0, FUN_140632e70). Each names its .rdata VA; the value is the image's word.
  float evalThetaBase_m;           // 0x142588210 (10): default acceptance radius; also the Block A add
  float evalThetaFar_m;            // 0x1425c48c0 (22): radius with the press timer or flag 13
  float evalThetaZ_m;              // 0x14259be0c (15): radius with flag 14; VA from pes-re-press-coord.md (eval claims none)
  float evalThetaB29_m;            // 0x1425c5948 (13): radius in the aggressive phase
  float evalIncumbentTheta_m;      // 0x14258c264 (3): radius bonus for the incumbent
  float evalIncumbentStep_m;       // 0x14259222c (5): mode-4 add for the incumbent
  float evalBlockASpeedKmh;        // 0x14259222c (5): holder speed for Block A
  float evalBlockAHeading_deg;     // 0x14259a764 (60): Block A heading window
  float evalBlockAAdd_m;           // 0x142588210 (10): radius add when Block A fires below the base
  float evalWindowSec_s;           // 0x14259222c (5): press window seconds (G_sub and accept-2)
  float evalThetaTwoLow_m;         // 0x14259bdec (6): accept-2 radius by default
  float evalThetaTwoHigh_m;        // 0x14259be04 (12): accept-2 radius at attack level 3, class 6 or aggressive
  float evalMode7Angle_deg;        // 0x14259be34 (50): mode-7 bearing from the excluded man
  float evalMode7IncAngle_deg;     // 0x14259be2c (40): the same for the incumbent
  float evalMode4Angle_deg;        // 0x14253ee30 (100): mode-4 bearing from the excluded man
  float evalMode4Margin_m;         // 0x142588210 (10): mode-4 final margin
  float coverNear_m;               // 0x14259bde4 (2.5): the cover holds inside this distance
  float coverFar_m;                // 0x1425c48b8 (14): the cover fails beyond this distance
  float coverOuter_m;              // 0x14259bdfc (11): the cone's outer radius
  float coverCone_deg;             // 0x14259be64 (140): the cone's full angle
  float gateFacing_deg;            // 0x14259a764 (60): holder facing limit against the attack bearing
  float gateBallHeight_m;          // 0x14259bddc (1.5): ball height limit
  float classFiveDist_m;           // 0x14258c264 (3): class-5 distance to the ball
  float classFiveKindTen_m;        // 0x14259222c (5): the same for kind 10
  float maintainReason5DistSq_m2;  // 0x14259d60c (9): reason-5 release distance squared
  float maintainBearing_deg;       // 0x14259be30 (45): reason 4/7 bearing band
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

// The loose-ball writer's word for one man on one tick: PRESS for the picked man and for the coordinator's
// second presser, NOTHING for the rest. PES FUN_140535C60 skips a slot whose role is already DELAY or PRESS
// ("1 < role - 0x33U"), so the writer's reset never drops the second presser, whose life the coordinator owns.
// `winner` and `second` are global men, or kPressNoMan.
int PressWriterWord(int man, int winner, int second);

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

// ---- The target eval (FUN_140633940; docs/re/pes-re-press-eval.md §2). ----
// Per man of the team pair (index 0..21), read by the caller from the match.
struct PressEvalMan {
  float x = 0.0f, z = 0.0f;   // pitch position (player +0x55c / +0x564)
  bool active = false;        // MI+0x1748[man] != 0: on the pitch
  bool excludedKind = false;  // kind in {0x11,0x14,0x50,0x52,0x57} (G5). ponytail: GF's goalkeeper role
  bool flag15 = false;        // FUN_140a63950(ctx, side, 15, man) != 0 (G6b). ponytail: not carried, false
  bool g180Reject = false;    // G4, read only while W: a G180 entry with flag 0. ponytail: not carried, false
  bool recordBit23 = false;   // record +0xB0 bit 23 (mode-4 pre-gate). ponytail: not carried, false
};
// Per slot k of the presser side: the team words read at slot k = man % 11.
struct PressEvalSlot {
  bool flagged = true;          // T+0x4C00[k] != 0 (G6a). ponytail: GF counts every on-pitch man as flagged
  bool orderTwentyFour = false; // T+0xB748[k] == 0x24 (G7). ponytail: concept orders not mapped, false
  int roleClass = 0;            // RI = T+0x87D0[8k]; class 0 is the keeper class
  int assignment = 0;           // T+0xA2A0[k] (mode-7 test: != 0x3C). ponytail: GF keeps no table, 0
  float keepX = 0.0f;           // f32 T+0x4B4C[12k] (mode-4 test). ponytail: the caller passes the man's x
};
struct PressEvalInput {
  int side = 0;                  // s: the presser side
  float sign = 1.0f;             // T+0x254: the side's attack sign, +1 or -1
  int ref = kPressNoMan;         // the holder: the pivot of the nearest list
  int excluded = kPressNoMan;    // PP[1] (X): skipped, and the bearing reference of modes 4 and 7
  int incumbent = kPressNoMan;   // PP[2]: the man the coordinator already holds
  int mode = 0;                  // the class: 7, 4 or 1 (class 5 never reaches the eval)
  bool aggressive = false;       // the coordinator's aggressive flag (A)
  bool flag13 = false;           // W0 = FUN_140a63950(ctx, s, 13, 0xFF)
  bool flag14 = false;           // FUN_140a63950(ctx, s, 14, ref): the Z test
  bool windowEvent = false;      // D_ev < 4K and MI+0x14C8[s==0] == 1: sets W, sticky across the men
  bool pressTimer = false;       // X = T+0xA53C != 0
  int pressTimerY = 0;           // Y = T+0xA544
  int attackLevel = 0;           // T+0xB8A4
  int levelClass = 0;            // CLS = FUN_140a60340(FUN_140a40bd0(T))
  bool phaseAggressive = false;  // B29 = (T+0xA298 == 3)
  int fps = 54;                  // K = FUN_141e532e0()
  float earlyLine = 0.0f;        // L = f32 MA+0x40+0x14*(s==0): the early exit while flag13
  bool blockAOppFree = false;    // P10(ctx, 1-s) == 0 (FUN_140a51d50). ponytail: its tail is OPEN, so false
  float refSpeedKmh = 0.0f;      // FUN_140477130(ref)
  float refHeadingDeg = 0.0f;    // FUN_140458740(ref, 0.6944/fps): the holder's heading of motion
  std::array<PressEvalMan, kPressTeamMen> men{};
  std::array<PressEvalSlot, kPressSideMen> slots{};
};
struct PressEvalResult {
  int man = kPressNoMan;  // the accepted man, or kPressNoMan (nothing is written then)
  int mode = 0;           // written to B88C[man % 11] on accept
};
// FUN_140633940: the first man of `in.side` in the nearest list of `in.ref` (ascending d^2, table order on ties)
// who passes every gate. There is no keep-max. The caller applies the accept writes.
PressEvalResult PressEvalPick(const PressConstants& c, const PressEvalInput& in);

// ---- The gate, the cover and the class-5 predicate (FUN_140632780, FUN_140632c70, FUN_1406324a0). ----
struct PressGateInput {
  float ballHeight_m = 0.0f;       // B+4: the raw ball height
  bool teamKickValid = false;      // T+0x78 valid: this side made the last kick, so the gate fails
  bool pressTimer = false;         // T+0xA53C != 0
  bool flag13 = false;             // FUN_140a63950(ctx, side, 0xd, 0xff)
  int attackLevel = 0;             // T+0xB8A4
  bool classFive = false;          // the class is 5
  bool cover = false;              // PressCoverByHolder
  float facingToAttack_deg = 0.0f; // |wrap180(T+0x250 - holder facing)| (FUN_140481b00)
};
bool PressGatePasses(const PressConstants& c, const PressGateInput& in);

// FUN_140632c70: the holder covers the excluded man. `dist_m` = |X - H|, `bearingDeg` = bearing(H -> X),
// `headingDeg` = the cone axis (the caller's T+0x6EC branch).
bool PressCoverByHolder(const PressConstants& c, float dist_m, float bearingDeg, float headingDeg);

// FUN_1406324a0: the class-5 man, or kPressNoMan.
struct PressClassFiveInput {
  int holder = kPressNoMan;   // c: MA+0x2601C, else the opponent's last kick
  int cand = kPressNoMan;     // the side's nearest man to c, or the next one when that is the match-up man
  int matchUp = kPressNoMan;  // FUN_140a62960. ponytail: GF has no match-up table, so none
  bool clauseOk = false;      // (B+0x18 <= 1.5 and not high) or T+0x78 > 21
  bool keeperKind = false;    // the G5 exclusion
  int roleClass = 0;          // class of cand's slot (class 0 is the keeper class)
  bool kindTen = false;       // kind 10. ponytail: GF has no kinds, so false
  float ballDist_m = 0.0f;    // FUN_14044e0d0: the ball's distance to cand
};
int PressClassFiveMan(const PressConstants& c, const PressClassFiveInput& in);

// ---- The coordinator state (PP and B88C) and maintain (FUN_140632e70; docs/re/pes-re-press-coord.md §7). ----
struct PressCoordState {
  int holder = kPressNoMan;                 // PP[0]
  int xMan = kPressNoMan;                   // PP[1]
  int second = kPressNoMan;                 // PP[2]
  std::array<int, kPressSideMen> reason{};  // B88C[k]
};
// The maintain inputs. The caller computes every distance and bearing (squared where named).
struct PressMaintainInput {
  bool inPlay = false;               // MI+0x116c == 1
  int slot = kPressNoMan;            // k
  int man = kPressNoMan;             // side * 11 + k
  int tableWord = 0;                 // T+0xA2A0[k]
  bool keeperHolds = false;          // FUN_140a53f90 != 0. ponytail: keeper hold not carried, false
  int holderNow = kPressNoMan;       // FUN_140632bc0 this tick
  int holderRecorded = kPressNoMan;  // PP[0]
  bool holderFromTouch = false;      // MA+0x2601C valid: the holder is the newest toucher
  int reason = 0;                    // B88C[k]
  int classFiveNow = kPressNoMan;    // FUN_1406324a0 this tick
  bool lifted = false;               // B+0x18 > 1.5, or the high test
  int teamKickMan = kPressNoMan;     // T+0x78: this side's last kick (kPressNoMan when invalid)
  int matchUp = kPressNoMan;         // FUN_140a62960. ponytail: none
  int xMan = kPressNoMan;            // PP[1]
  bool xCloserToTeamPoint = false;   // dist(X, TP) < dist(H, TP); TP = the ball copy (ponytail)
  float manToHolderDistSq = 0.0f;    // dist^2(M, H)
  float bearingVsCoverDeg = 0.0f;    // |fold(bearing(H->M) - bearing(H->X))|
  bool flag13 = false;               // FUN_140a63950(ctx, side, 0xd, 0xff)
  int roleClass = 0;                 // T+0x87D0[8k]
  uint32_t stampFirst = 0;           // the head stamp's first frame (rec+4)
  uint32_t stampLast = 0;            // the head stamp's last frame (rec+8)
  uint32_t windowFrames = 0;         // 2 * fps (the stamp window)
  float manToXDistSq = 0.0f;         // FUN_1404c8ea0(MA, X, M): asm-checked as a d^2 (the coord doc says sqrt)
};
// PES order; each step returns at its first decision. True keeps the presser (PES returns 0).
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
