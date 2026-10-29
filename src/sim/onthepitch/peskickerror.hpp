#pragma once

// PES 2021's kick execution error (docs/re/pes-re-kick-accuracy.md,
// pes-re-kick-scatter.md): the kick-type quality FUN_14071d770, the three
// {scale, sign, amp} scatter records FUN_14083cc30 builds from soft-min
// qualities, and the yaw / pitch / speed draws FUN_1408398d5 makes from them.
// Every draw is slot 2 of the kicker's own math::Random (player+0x32d0).
// Every constant is read from the user's PES2021.exe at runtime.

#include <array>
#include <optional>
#include <string>

#include "pes/exe.hpp"
#include "pesrandom.hpp"
#include "pespenaltyscatter.hpp"

namespace pes {

inline constexpr int kKickRngSlot = 2;  // Normal 0x140a92590 and 0x1406ebf30 hard-wire slot 2
inline constexpr int kNormalTableSize = 513;

// The situation factor Q of FUN_14083cc30 (0x14083DF54..0x14083E69C; docs/re/pes-re-kick-kinds.md,
// pes-re-kick-scatter-inputs.md (a), docs/ports/pes-port-KickQ.md): Base(class, subclass, gate) x G. Every field is
// the kicker record's own (ap = rcx of the scatter), in PES units. Q needs only the class and subclass of the kick kind
// (rows of the install's 151-row kind table 0x14344E380) and four gate predicates that are pure functions of the kind.
struct SituationConstants {
  // Base: class base, subclass multiplier, gate multiplier.
  float p90 = 0.0f;       // 0.9   @0x14258c260 class 0 base; subclass 7/0xB; gate multiplier (class 2 and 4); sub-0/1 tail
  float p88 = 0.0f;       // 0.88  @0x1425ddf70 class 1 base
  float p86 = 0.0f;       // 0.86  @0x1425eb850 class 2 and 3 base
  float one = 0.0f;       // 1.0   @0x14256ae1c class 4 base, ramp top
  float p95 = 0.0f;       // 0.95  @0x1425ab124 subclass 1; gate multiplier (class 0, 1)
  float p80 = 0.0f;       // 0.8   @0x142572348 subclass 5
  float p85 = 0.0f;       // 0.85  @0x1425ab120 subclass 6; sub-0/1 tail (first-time)
  float p70 = 0.0f;       // 0.7   @0x14257a194 subclass 8
  float p925 = 0.0f;      // 0.925 @0x1425ef9dc gate multiplier (class 3)
  // G: speed term.
  float tickRate = 0.0f;  // 54.0  @0x14259d624 (init of slot 0x14351ae54 by 0x141e532f0; getter 0x141e532d0)
  float sixty = 0.0f;     // 60.0  @0x14259a764 (km/h = tick rate x 60 x 60 / 1000 x m per tick)
  float thousand = 0.0f;  // 1000.0 @0x14256ae34
  float stopLow = 0.0f;   // 11.0  @0x14259bdfc speed threshold floor (km/h)
  float hundred = 0.0f;   // 100.0 @0x14253ee30
  float fifty = 0.0f;     // 50.0  @0x14259be34 kick-angle ramp start
  float c0Gain = 0.0f;    // 2.5   @0x14259bde4 class 0 threshold gain
  float c13Gain = 0.0f;   // 5.0   @0x14259222c class 1, 3 threshold gain
  float speedTop = 0.0f;  // 28.0  @0x1425a2004
  float staminaK = 0.0f;  // 0.01  @0x14256ae18
  float turnLo = 0.0f;    // 45.0  @0x14259be30 body-turn ramp
  float turnHi = 0.0f;    // 135.0 @0x14259be60
  float turnDiv = 0.0f;   // 90.0  @0x14259be5c (0x14083DF47 MOVSS XMM15; the ramp's divisor at 0x14083E278 / E354)
  float lowKnee = 0.0f;   // 22.0  @0x1425c48c0
  float lowGain = 0.0f;   // 8.5   @0x1425c48b4
  float topGain = 0.0f;   // 6.0   @0x14259bdec
  float effK = 0.0f;      // 0.6   @0x14257a190
  float turnSq = 0.0f;    // 0.3   @0x142590d98 sub 0/1/9/10: 1 - t^2 * 0.3
  float tenth = 0.0f;     // 0.1   @0x142588200 (uVar14 4/0xD branch)
  float c2Base = 0.0f;    // 0.05  @0x1425a8a28
  float c2Gain = 0.0f;    // 0.15  @0x1425c48a0
  double aGain = 0.0;     // 0.3   @0x14259d5e0 (f64) uVar14 0/2/0xF/0x11 branch
  double aPlusFirst = 0.0;  // 0.3 @0x14259d5e0 (f64) first-time byte 0
  double aPlusLater = 0.0;  // 0.2 @0x1425ef868 (f64) first-time byte != 0
  // G: first-time window (0x14083CDCD..0x14083CF07).
  float windowLimit = 0.0f;  // 360.0 @0x14259858c (also the heading wrap)
  float halfTurn = 0.0f;     // 180.0 @0x142598580
  float negOne = 0.0f;       // -1.0  @0x142588218
  float turn157 = 0.0f;      // 157.5 @0x1425cca3c
  float turn130 = 0.0f;      // 130.0 @0x1425a9bf0 class 0, first-time byte 0
  float turn125 = 0.0f;      // 125.0 @0x1425c5950 class 0, first-time byte != 0
  float turn120 = 0.0f;      // 120.0 @0x14256ae30 class 1, first-time byte 0
  float turn115 = 0.0f;      // 115.0 @0x1425c48d4 class 1 (first-time byte != 0) and class 2 (byte 0)
  float turn110 = 0.0f;      // 110.0 @0x14259d628 class 2, first-time byte != 0
  float noWindow = 0.0f;     // 720.0: the immediate W1 (0x1406EB6FC) stores in ap+0x2764
  // In-flight branch (motion descriptor byte 1, subclass 2..12, 0x14083E42E..0x14083E607): Q *= 1 - (ramp(ball speed) * 0.9 +
  // 0.1) * (1 - g^2) * m, g the ball height's ramp (1 at or under ballLow, 0 at or over ballHigh), m by class.
  float ballLow = 0.0f;      // 0.3260578 @0x1425cb2a8
  float ballHeld = 0.0f;     // -0.2173719 @0x1425e5b40: added to the height for the 0x1407240F0 / 0x1407241A0 kinds and the AI bit
  float ballHigh = 0.0f;     // 0.6521156 @0x1425e6060
  float ballSpan = 0.0f;     // -0.3260578 @0x1425efa20: the height ramp's divisor
  float flightLo = 0.0f;     // 10.0 @0x142588210 ball speed (km/h) where the speed ramp starts
  float flightHi = 0.0f;     // 30.0 @0x142591140 where it ends (classes 2 and up; class 0 reads `fifty`, class 1 flightHi1)
  float flightHi1 = 0.0f;    // 40.0 @0x14259be2c
  float flightGain0 = 0.0f;  // 0.4 @0x14259b89c class 0 multiplier (class 1 reads p80, the rest effK)
  // The kind table 0x14344E380 (16 B per row, row = kind) and the four gate predicates 0x1407240F0, 0x140724590,
  // 0x140726610, 0x140726640 evaluated over the kinds from the immediates and tables in their code.
  static constexpr int kKinds = 151;  // rows 0..0x96
  std::array<uint8_t, kKinds * 16> kindTable{};
  std::array<uint8_t, 16> outOfRangeRow{};  // 0x14344E620
  std::array<uint8_t, kKinds> gateKinds{};  // bit 0: 0x1407240F0, 1: 0x140724590, 2: 0x140726610, 3: 0x140726640, 4: 0x1407241A0
};

// The kicker-record fields Q reads (ap+offset), the launch vector's horizontal components (param_6 of the scatter)
// and the kick quality `a` (FUN_14071d760). Distances in metres per PES tick (not per second).
struct KickSituation {
  int kind = 0;                // ap+0x2752: row of the kind table, 1..0x96
  int8_t b2754 = 0;            // ap+0x2754: result[1] of FUN_140829E80
  int8_t entry2755 = 0;        // ap+0x2755: result[2], the kick-angle bucket (before the motion override)
  int8_t firstTime = 0;        // ap+0x2756
  float window = 0.0f;         // ap+0x2764: an angle in degrees; >= 360 means no window
  bool aiBit = false;          // ap+0x2460 bit 29
  uint16_t motion = 0;         // the u16 behind vtable+0x18 of the motion object at ap+0xAB8
  bool touch = false;          // vtable+0x30 of the motion object
  float velX = 0.0f, velZ = 0.0f;  // ap+0x2480, ap+0x2488
  float stamina = 0.0f;        // ap+0x281C
  float heading_deg = 0.0f;    // ap+0x247C
  float body_deg = 0.0f;       // ap+0xBE4
  float launchX = 0.0f, launchZ = 0.0f;  // param_6[0], param_6[2] (PES axes: z = -GF y)
  float quality = 0.0f;        // a
  // The in-flight branch's inputs (0x14083E42E..0x14083E607). `descriptor` is FUN_1406B0250(motion table, motion id, 0): byte 1 of
  // the motion's runtime record; 1 runs the branch, anything else skips it. The ball records are FUN_140A6CF70(ball, 1) and (ball, 2)
  // (ball+0xB0, ball+0xD8: x, height, z; the ball's predicted position one and two ticks ahead), in metres.
  int descriptor = 0;
  float ball1X = 0.0f, ball1Y = 0.0f, ball1Z = 0.0f;
  float ball2X = 0.0f, ball2Z = 0.0f;
  bool actionBit = false;      // FUN_1406EAD40(ap, 0xB or 0xC): bit b of the kicker's action class record (b = 0xB when vtable+0x28 returns 10)
};

// PES's bearing of (x, z) in degrees [0, 360): FUN_14046F230 over FUN_14046F260 (translated).
float PesHeadingDeg(float x, float z);

// Base(class, subclass, gate) of the kind. A kind outside 0..0x96 reads the out-of-range row, like PES.
float KickBase(const SituationConstants& c, const KickSituation& s);
// Q = Base x G.
float SituationFactor(const SituationConstants& c, const KickSituation& s);

struct KickErrorConstants {
  // --- FUN_14071d770 quality curve q(S) (accuracy 2, contact 3.3) ---
  float qFloorStat = 0.0f;   // 40.0  @0x14259be2c
  float qMidStat = 0.0f;     // 60.0  @0x14259a764
  float qTopStat = 0.0f;     // 95.0  @0x1425c594c
  float qLowGain = 0.0f;     // 0.1   @0x142588200 (also q at 60)
  float qLowSpan = 0.0f;     // 20.0  @0x14259be1c
  float qMidGain = 0.0f;     // 0.85  @0x1425ab120
  float qMidSpan = 0.0f;     // 35.0  @0x14259a760
  float qTopGain = 0.0f;     // 0.05  @0x1425a8a28
  float qTopQuarter = 0.0f;  // 0.25  @0x142592228
  float qTopBase = 0.0f;     // 0.95  @0x1425ab124
  float skillBonus = 0.0f;   // +5.0  @0x14259222c

  // --- FUN_14083cc30 records (scatter 1, 6) ---
  float angleScale_deg = 0.0f;  // 22.5 @0x14259d618 (Y and P scale)
  float speedScale = 0.0f;      // 0.3  @0x142590d98 (S scale)
  float ampGain = 0.0f;         // 4.25 @0x1425ef9f8
  float ampBase = 0.0f;         // 0.75 @0x14259bdd4 (also A's stat gain)
  float half = 0.0f;            // 0.5  @0x142565600: default pSign, k, negative-pitch halving
  // A = 0.75*a + 0.25 (0x14083e610..633)
  float aBase = 0.0f;           // 0.25 @0x142592228

  // --- I, non-SHOOT (0x14083dd32..0x14083df70) ---
  float minute_s = 0.0f;        // 60.0 @0x14259a764, squared: m/s -> km/h with /1000
  float km_m = 0.0f;            // 1000 @0x14256ae34
  float rampPerQuality = 0.0f;  // 10.0 @0x142588210 (also the pitch ramp span)
  float rampLo_kmh = 0.0f;      // 60.0 @0x14259a764
  float groundHi_kmh = 0.0f;    // 85.0 @0x14259be58 (kick byte 1..3)
  float otherHi_kmh = 0.0f;     // 80.0 @0x142596e6c
  float wQuality = 0.0f;        // 0.95 @0x1425ab124
  float wGround = 0.0f;         // 0.3  @0x142590d98
  float wOther = 0.0f;          // 0.375 @0x1425e4a30
  float pitchLo_deg = 0.0f;     // 20.0 @0x14259be1c
  float pitchHi_deg = 0.0f;     // 30.0 @0x142591140
  // --- I, SHOOT (0x14083db8a..0x14083dd27, action byte 10) ---
  float powerLo = 0.0f;         // 0.2  @0x142598578
  float powerSpan = 0.0f;       // 0.8  @0x142572348
  float shootPowerWeight = 0.0f;  // 0.25 @0x142592228

  // --- R pressure (0x14083ec70..0x14083ee46) ---
  float rWeight = 0.0f;         // 0.4  @0x14259b89c (also D's contact slope)
  float dBase = 0.0f;           // 0.8  @0x142572348
  float contactLo = 0.0f;       // -20  @0x1425a946c
  float contactSpan = 0.0f;     // 40.0 @0x14259be2c
  float contactHi = 0.0f;       // 20.0 @0x14259be1c
  float nearOpp_m = 0.0f;       // 1.0  @0x14256ae1c (xmm12)
  float farOpp_m = 0.0f;        // 2.0  @0x142565604
  float dWeight = 0.0f;         // 1.5  @0x14259bddc

  // --- Normal 0x140a92590 (rng 1.4/1.5, scatter 2) ---
  float normalDiv = 0.0f;       // 3.53223 @0x1426550e4
  float normalCentre = 0.0f;    // 50.0 @0x14259be34
  float normalTabScale = 0.0f;  // 2^-12 @0x1426550dc
  float normalNegTwo = 0.0f;    // -2.0 @0x14259a768
  float normalTabSteps = 0.0f;  // 512 @0x1426550e8
  float normalDraw = 0.0f;      // 1000 @0x14256ae34
  float fullTurn_deg = 0.0f;    // 360 @0x14259858c
  float quarterTurn_deg = 0.0f; // 90  @0x14259be5c
  std::array<float, kNormalTableSize> logTable{};  // 0x1426548c0, raw: -ln(512/i)*4096
  std::array<float, kNormalTableSize> sinTable{};  // 0x142655950: sin(i*90/512 deg), 513 entries, read at load

  // --- Penalty aim scatter (FUN_1406b4000, FUN_1406e3210; pespenaltyscatter.*) ---
  PenaltyScatterConstants penaltyScatter{};

  // --- Situation factor Q (FUN_14083cc30) ---
  SituationConstants situation{};
};

std::optional<KickErrorConstants> LoadKickErrorConstants(const PeImage& exe, std::string* error);

// FUN_14071d770's tail: the quality of a kick from the stat it uses (0..99,
// any skill bonuses already added).
float KickQuality(const KickErrorConstants& k, float stat);

// FUN_1408413c0: the four factors sorted descending, then folded from 1.0 by
// x = min(x,v) * (max(x,v) + 1) / 2, once per factor: a soft minimum that leans to the worst.
float SoftMin4(float a, float b, float c, float d);

// Normal(rng, amp, 0) 0x140a92590: an int in 0..99 around 50.
int Normal(Random& rng, const KickErrorConstants& k, float amp);
// 0x140a92c80: sin of `deg` in degrees, times `amp`, PES's translated sine (ball/pes_fn_140A92C80) over the
// install's quarter-wave table `k.sinTable`.
float SinTableDeg(const KickErrorConstants& k, float deg, float amp);

// The launch-power penalty I (rsp+0x58). Pass: launch speed and elevation
// against a stat-raised window; shot: the strike's power 0..1.
float PassLaunchPenalty(const KickErrorConstants& k, float quality, float speed_mps,
                        float pitch_deg, bool groundPass);
float ShotLaunchPenalty(const KickErrorConstants& k, float power);
// R = 1 - 0.4*1.5*D, D the nearest opponent's pressure. Contacts are PES
// 0..99 Physical Contact; no opponent within reach gives 1.
float PressureFactor(const KickErrorConstants& k, float contactSelf, float contactOpp,
                     float oppDistance_m);

struct ScatterRecord {
  float scale = 0.0f;
  float pSign = 0.0f;
  float amp = 0.0f;
};
struct KickScatter {
  ScatterRecord yaw, pitch, speed;
};
// `situation` is Q (SituationFactor of the kick); F1/F2 stay a stand-in inside.
KickScatter BuildScatter(const KickErrorConstants& k, float quality, float launchPenalty,
                         float pressure, float situation);

struct KickError {
  float yaw_deg = 0.0f;
  float pitch_deg = 0.0f;
  float speedFactor = 1.0f;
};
// FUN_1408398d5 path B (mode 3): yaw, pitch, speed, each Normal then a sign roll.
// `halveNegativePitch`: action bytes 9/10/12..14 halve a downward pitch error.
KickError DrawKickError(const KickErrorConstants& k, Random& rng, const KickScatter& s,
                        bool halveNegativePitch);

// Applies the error to a launch velocity (x, y horizontal, z up): heading turn,
// elevation change, speed factor - the FUN_140470d60 heading add and the
// pitch/|v| rebuild.
void ApplyKickError(const KickError& e, float v[3]);

}  // namespace pes
