#pragma once

// PES 2021's ball receive, docs/re/pes-re-receiving.md (ReceivingRE).
//
// Three decisions, all deterministic (no RNG in any receive body, report §0.8):
//   - who goes to the ball as the receiver (PASS_GET, the movement selector
//     FUN_140933450 / FUN_140931c60, report §2), with the two-receiver
//     arbitration against the other team's pass target;
//   - who may touch it now (FUN_1409fa400 -> 1409fa720, report §3): the
//     re-touch lockout and the shot-through rule;
//   - which first touch (FUN_1409f8a50, report §4): the planned 5 m touch, a
//     plain control trap, the interception rule (FUN_1409fa0b0), the
//     touch-line guard and the turn clamp (FUN_1409fbce0).
//
// Every function is pure (arguments plus the constants); the caller builds the
// inputs from its own state. Angles are degrees, distances metres, times
// seconds, speeds m/s unless the name says otherwise.
//
// Where the numbers live:
//   - trap.json: the `trap.o` record of common/match/constant/
//     constant_player.bin, read through PesVfs. Unlike tackle.o / block.o,
//     its disk offsets are not the deserializer's store offsets
//     (FUN_141e681c0); pesreceive.cpp lists both.
//   - everything else: the exe's .rdata (PeImage::ReadVa) or an instruction
//     immediate with its opcode checked.

#include <array>
#include <optional>
#include <string>
#include <functional>
#include <vector>

#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "onthepitch/pesevents.hpp"

namespace pes {

struct ReceiveConstants {
  // --- the 0x5D kick branch (reaction entry 3, FUN_1409fbba0), exe .rdata ---
  float kickMaxBallHeight_m = 0.0f;  // 0x1425a9bd4: the ball's height at 10 ticks
  float kickSpeedNumerator = 0.0f;   // 0x14263e398: 38 km/h as 38000 m/h
  float kickSpeedDivisor = 0.0f;     // 0x142598590: seconds per hour (3600)
  // --- trap.o (VFS); the store offsets FUN_1409fa720/1409fbce0 read ---
  float lockDefence_s = 0.0f;         // +0x1b4 defence: loose ball
  float lockDefenceKeep_s = 0.0f;     // +0x1b8 defenceKeep: someone else holds it
  float lockOffence_s = 0.0f;         // +0x1bc offence: mine / my team's pass
  float lockShoot_s = 0.0f;           // +0x1c0 shoot
  float lockShootSpeedBall_s = 0.0f;  // +0x1c4 shootSpeedBall
  float busyTime_s = 0.0f;            // +0xd8 busyTime (turn clamp window)
  bool checkMyBallPass = false;       // +0xdc checkMyBallPass
  float runTrapStopTurnAngle_deg = 0.0f;   // +0xe0
  float stopTrapStopTurnAngle_deg = 0.0f;  // +0xe4
  bool turnClampUse = false;               // +0xe8 use
  // --- the lockout (FUN_1409fa720), exe .rdata ---
  float lockStatCoefficient_s = 0.0f;  // 0x142588200, the c·(1 - s^2) term
  float statFloor = 0.0f;              // 0x14259be2c
  float statCeil = 0.0f;               // 0x1425c48cc
  float statSpan = 0.0f;               // 0x1425cb2fc
  float highBallLock_m = 0.0f;         // 0x14259bdd4, event 3 above it = shoot window
  float shootLockSpeed_kmh = 0.0f;     // 0x142596e6c, shoot vs shootSpeedBall
  float mpsToKmh = 0.0f;               // 0x14259ce38
  float ownKickLockScale = 0.0f;       // 0x14258c264, after my own kick
  float shotThroughAngle_deg = 0.0f;   // 0x142591140
  float shotThroughSpeed_kmh = 0.0f;   // 0x14259a760
  // --- the first touch (FUN_1409f8a50 and helpers) ---
  float interceptSpeedSq_m2s2 = 0.0f;  // 0x1425f1f24
  int interceptAgePercent = 0;         // imul edx,eax,imm8 @0x1409fa1d0 (percent of fps)
  float plannedTouch_m = 0.0f;         // 0x14259222c
  float lineMargin_m = 0.0f;           // 0x14256ae1c
  float lineGuardBallAngle_deg = 0.0f;  // 0x14259be20
  float lineGuardBodyAngle_deg = 0.0f;  // 0x14259be60
  float lineGuardAhead_m = 0.0f;        // 0x14259bddc
  float clampBallSpeedSq_m2s2 = 0.0f;   // 0x14263e390
  float runningFacing_deg = 0.0f;       // 0x14259be30
  // 0x14263e394. Compared against the predicted body velocity squared; the
  // install's value is (0.9 * 54)^2, so it reads as (speed * fps)^2 (INF(med):
  // the velocity getter FUN_140a6c6d0 is not decoded).
  float runningSpeedSq = 0.0f;
  // --- the PASS_GET arbitration (FUN_140931c60) ---
  float arbitrationAngle_deg = 0.0f;  // 0x14259be60
  float arbitrationRadius_m = 0.0f;   // 0x14258c264
  float arbitrationLength_m = 0.0f;   // 0x14259222c
  // --- who may touch (FUN_1409fa400) and where to meet it (ActionPassGet) ---
  float ballDistanceTableMaxSq_m2 = 0.0f;  // 0x14253ee30, MA+0x24710 entries are squared metres
  float receivingPointMargin_m = 0.0f;     // 0x1425e5ab8 (FUN_14094e3a0)
};

std::optional<ReceiveConstants> LoadReceiveConstants(const PeImage& exe, const PesVfs& vfs,
                                                     std::string* error);

// The heading of a 2D vector in degrees (GF's pitch y is PES's z).
float HeadingDeg(float x, float y);

// The newest ball event's kind (PES ring-0 `+0`: 1-3 kicks, 5 shot).
enum class BallEventKind { kTouch, kPass, kHighPass, kShot };
// The receive gates' kind for a store action kind (PES ring-0 +0): 1 a kick, 2 a
// high kick, 5 a shot, anything else a touch.
BallEventKind ReceiveEventKindOf(uint32_t actionKind);

// --- Re-touch lockout (report §3) ---

struct LockoutInput {
  bool ballOwned = false;   // a player holds the ball (PES `*param_3 < 0x16`)
  bool ownerIsMe = false;
  BallEventKind eventKind = BallEventKind::kTouch;
  bool eventByMe = false;
  bool eventByMyTeam = false;
  bool previousEventByMe = false;   // ring 1
  float previousEventAge_s = 1e9f;
  float ballStat = 0.0f;            // PES Ball Control, 40..99
  float ballSpeed_mps = 0.0f;
  float ballHeight_m = 0.0f;
};

// L in seconds: frames since the newest ball event must reach int(fps * L).
float RetouchLockout_s(const ReceiveConstants& c, const LockoutInput& in);
bool RetouchAllowed(const ReceiveConstants& c, const LockoutInput& in, float eventAge_s,
                    float fps);

struct ShotThroughInput {
  bool ownTeamShot = false;  // newest event a shot by my team
  float ballX = 0.0f, ballY = 0.0f;
  float ballVelX = 0.0f, ballVelY = 0.0f;
  float goalX = 0.0f;  // the goal line the shooter attacks
  float goalHalfWidth = 0.0f;
};

// True: the shot is let through (no trap).
bool ShotRunsThrough(const ReceiveConstants& c, const ShotThroughInput& in);

// --- PASS_GET (report §2) ---

struct PassGetInput {
  bool iAmPassTarget = false;       // T+0x78 == me
  bool newestEventIsMyKick = false;  // FUN_140a60530
  bool inKickBeforeContact = false;  // the anime-can-trap clause
  bool mateNearBallOwnsIt = false;   // the MA+0x24840 3-slot scan
  bool arbitrationWins = false;      // ArbitrationWins: returns before the scan
};

bool PassGetSelected(const ReceiveConstants& c, const PassGetInput& in);

struct ArbitrationInput {
  bool otherTargetValid = false;  // the other team's T+0x78 < 0x16
  float otherX = 0.0f, otherY = 0.0f;
  float otherHeading_deg = 0.0f;  // FUN_140439f50 for him
  float ballHeading_deg = 0.0f;   // the ball's travel heading
  float myX = 0.0f, myY = 0.0f;
};

// The other team's pass target faces the oncoming ball (> 135 deg off its
// travel) and I stand inside the 3 m-radius, 5 m capsule ahead of him.
bool ArbitrationWins(const ReceiveConstants& c, const ArbitrationInput& in);

// --- First touch (report §4) ---

enum class TouchKind {
  kControl,      // kind 0: the plain trap along the body heading
  kDirectional,  // the planned touch, 5 m along the brain's direction
};

struct FirstTouchInput {
  bool hasPlannedDirection = false;  // cmd+0x5d4 >= 0
  float plannedHeading_deg = 0.0f;
  float bodyHeading_deg = 0.0f;
  float moveHeading_deg = 0.0f;
  float moveSpeed_mps = 0.0f;
  bool eventByMyTeam = false;
  bool eventIsKick = false;  // ring-0 kinds 1..3 / 0x10
  bool eventByMe = false;
  float eventAge_s = 0.0f;
  float ballSpeedSq_m2s2 = 0.0f;
  float ballHeading_deg = 0.0f;
  float posX = 0.0f, posY = 0.0f;
  float halfLength = 0.0f, halfWidth = 0.0f;
  bool pressOrDelay = false;  // P+0x5d0 in {0x33 DELAY, 0x34 PRESS}
};

struct FirstTouch {
  TouchKind kind = TouchKind::kControl;
  float heading_deg = 0.0f;
  float distance_m = 0.0f;
};

// FUN_1409fa400's first gate: his squared distance to the ball.
bool WithinBallDistanceTable(const ReceiveConstants& c, float ballDistanceSq_m2);

struct Point2 {
  float x = 0, y = 0;
};
// ActionPassGet's receiving point: the predicted ball point nearest the
// receiver (FUN_140a4d340 over the prediction, one point per frame), clamped
// onto the pitch along the line from him (FUN_14094e3a0).
Point2 ReceivingPoint(const ReceiveConstants& c, const std::vector<Point2>& ballPath, Point2 me,
                      float halfLength, float halfWidth);

FirstTouch ChooseFirstTouch(const ReceiveConstants& c, const FirstTouchInput& in, float fps);

// --- Designated owner (MA+0x2600C: FUN_140457070's ring-0 scan d_a.c:1248-1283; gate 4c d_b.c:91) ---

// One ring-0 entry as the scan reads it: the action kind (payload+0), the tag word (payload+0x1C) and the
// actor key (payload+4 team, payload+8 slot). An empty slot is the default entry, which owns nobody.
struct Ring0Entry {
  uint32_t kind = 0;
  uint32_t tag = 0;
  ActorKey actor;
};
inline constexpr size_t kRing0ScanCount = 9;  // NewestOrRing0(0..8)

// The owner the scan writes to MA+0x2600C, or NoActorKey() when none. ring[n] = NewestOrRing0(n).
// period9: the match period is 9 (the shoot-out), where the first kind-5 record names the owner.
// previous: MA+0x2600C before the scan; the scan stops at the first record whose owner differs from it.
ActorKey DesignatedOwnerScan(const std::array<Ring0Entry, kRing0ScanCount>& ring, bool period9,
                             const ActorKey& previous);

// Gate 4c (FUN_1409fa520, d_b.c:91): the designated owner never traps through reaction entry 3.
bool RefusedAsDesignatedOwner(const ActorKey& designatedOwner, const ActorKey& me);

// --- Kick branch 0x5D (reaction entry 3, d_a.c:610-662) ---

// FUN_1409fbb40: the anime action gate. hasAction: the anime action pointer (param_1+0x28) is set. A held AI
// stage (non-zero) decides alone, admitting only stage 5; otherwise the action byte with bit 7 masked is 0x4A.
bool KickBranchActionGate(bool hasAction, uint8_t actionByte, int aiStage);

// FUN_1409fbba0: the ball's height at 10 ticks is at most kickMaxBallHeight_m, and the 2D difference of the
// player's and the ball's samples is strictly under kickSpeedNumerator / (fps * kickSpeedDivisor).
bool KickBranchBallGate(const ReceiveConstants& c, float ballHeightAt10_m, Point2 playerSample,
                        Point2 ballSample, float fps);

// PES's carrier after one frame's ball contact (MA+0x2601C, FUN_140457070; first-touch §5.1). PES takes
// the carrier from the newest valid K=8 key, which a touch pushes. GF stand-in: a control touch (trap or
// ball control) that leaves the ball at the toucher's feet hands him the carrier; the carrier keeps it
// while he holds the ball and loses it when he does not.
// ponytail: the K=8 push flags of the touch record (FUN_140A0DFD0 +0x18/+0x19) are not decoded.
enum class CarrierChange { kKeep, kToucher, kNone };
CarrierChange CarrierAfterContact(bool controlTouch, bool toucherHoldsBall, bool carrierHoldsBall);

// PES's holder MA+0x2601C (docs/re/pes-re-press-coord.md section 4a; pes-re-first-touch.md section 5.1). Its only valid
// writer is W1 = VA 0x14045725A (MOV dword ptr [RBP+0x2601C],EDI) in FUN_140457070: it stores the actor of the newest
// ring-1 touch key when that key maps to an on-pitch slot, else 0xFF. Nothing clears it when the ball is lost.
// GF stand-in: the touch stamped this tick (Team::SetLastTouchPlayer) hands the carrier to its toucher when the touch
// leaves the ball at his feet. Differences: PES keeps the last toucher on a loose ball, while GF clears the retainer
// when the ball leaves him (humanoid.cpp, the retain block). ponytail: PES's K=7 contact needs the touch probe
// G+0x2A80 (first-touch section 5.4, OPEN); GF takes every stamped touch. W1 (press-coord section 4a) writes only
// when TA+0x11 is set; that flag's caller is not traced.
CarrierChange CarrierFromTouch(bool touchedThisTick, bool toucherHoldsBall);

// --- The Trap action's request (docs/re/pes-re-trap-inputs.md 3.1; producer FUN_1409F8A50, caller FUN_1409F8640) ---

// The dispatcher's kind (request word 2, R[1]): kinds <= 0 run the kind-0 chain 0x14079A470, the only one translated; kind 4 runs
// 0x14079B3D0, which is not. The touch kind (word 1, R[0]) is the trap type: 0 the plain control, 3 a planned touch (the
// captured calls carry 3 and 4).
// ponytail: the planned touch's R[1] (4) and R[0] (3) come from cmd+0x5c, whose writer GF has no source for; kind 4 is the kind
// PES retries as 0 and 0x14079B3D0 is not translated, so a planned touch takes that retry (RetryTrapAsControl) at once.
inline constexpr int32_t kTrapKindControl = 0;
inline constexpr int32_t kTrapKindDirectional = 4;
inline constexpr uint32_t kTrapTouchControl = 0;
inline constexpr uint32_t kTrapTouchPlanned = 3;

// The request PES's producer builds for a first touch: word 0 the touch heading (PES yaw, degrees), word 1 the touch kind, word 2
// the dispatcher's kind, word 3 zero. ponytail: the plan flag bytes (R+0x34, +0x36, +0x37, +0x38, +0x3A: forced kind 4, no
// direction, open-body touch...) have no GF source and stay 0; the producer's writers of them remove this.
PesTrapRequest TrapRequestFor(const FirstTouch& touch, float headingYaw_deg);

// FUN_1409F8640's retry: when the anime refuses with a status in {3, 7, 8} (out[0]) a kind-4 request is made once more as kind 0,
// if plan byte +0x35 is clear.
bool RetryTrapAsControl(int32_t kind, uint32_t refusal, uint8_t planByte35);
}  // namespace pes
