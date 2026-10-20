#pragma once

// PES 2021's goal kick on the pes_loop path (docs/re/pes-re-goalkick-taker.md, audited against the
// disassembly of sub_142136F00 and GoalKickStandby::SetKick, 0x140774AE0).
//
// The taker is the keeper. SetKick gives him a stand point: a target his walk aims at, not a teleport.
// ThinkUnitSetPlay's timer ladder (sub_142136F00) releases the kick, not his arrival. At the release his
// strike clip carries him from where he stands onto the contact root of the clip's first hit record. The
// kick is a lofted pass to the mate the lofted picker names, or a lofted kick into space when none is
// found. The flight is PES's own ball integrator (pesballphysics.hpp).

#include <vector>

#include "pesball.hpp"
#include "pesballphysics.hpp"
#include "pessetpiecetaker.hpp"
#include "pes/types.hpp"

namespace pes {

enum class GoalKickCommand { kLoftedToReceiver, kLoftedIntoSpace };

// The install's constants for the goal kick. GoalKickConstantsFrom fills them from the set-piece taker constants.
struct GoalKickConstants {
  float fps = 0.0f;             // PES's logic frames per second (kPesTickHz)
  float half_s = 0.0f;          // 0.5, 0x142565600: the ladder's windows, and the rounding of every wait
  float threeSeconds_s = 0.0f;  // 3.0, 0x14258C264: the ladder's long wait (case 3)
  float tacticsGate = 0.0f;     // 0.3, 0x142590D98: the special restart's early-out (case 2)
  float centimetre_m = 0.0f;    // 0.01, 0x14256AE18: the hit record's positions are cm (FUN_1406E8CC0)
  float physiqueLow = 0.0f;     // 0x1425DDF78: the physique clamp of FUN_1406E8CC0, for x and z
  float physiqueHigh = 0.0f;    // 0x1425DDF80
  float hitTickDivisor = 0.0f;  // 60, 0x14259A764: a hit lands at 2 · frame · fps / 60 + rounding (FUN_1406B03D0)
};

GoalKickConstants GoalKickConstantsFrom(const SetPieceTakerConstants& set);

// The phases of sub_142136F00, one per case of its switch. Case 0 stamps the taker and is the ladder's first tick.
enum class GoalKickPhase : int {
  kTaker = 0,
  kWaitOne = 1,
  kShortWindow = 2,
  kWaitThree = 3,
  kLoftedWindow = 4,
  kForced = 5,
};

// The ladder's state: its phase, and the frame the phase began on (the unit's stamp, +0x44).
struct GoalKickLadder {
  GoalKickPhase phase = GoalKickPhase::kTaker;
  unsigned long stamp = 0;
};

// What the ladder reads on a tick.
struct GoalKickLadderInputs {
  // The lofted picker's receiver index, -1 when it finds none (FUN_142135230 answers above 0x15).
  int lofted = -1;
  // The restart record's +0x14C4 flag (MI+0x14C4 == 1; 0 on a normal goal kick) and the taker's tactics +0x18.
  // The tactics are read only when the flag is set: below the gate, the ladder waits three seconds first.
  bool specialRestart = false;
  float tactics = 0.0f;
  // MI+0x400 != 0 and FUN_141F9C3B0 != 0: a valid special object skips the three-second wait.
  bool specialObjectValid = false;
};

// The ladder's order for a tick: whether the kick is released, and its command when it is.
struct GoalKickOrder {
  bool released = false;
  GoalKickCommand command = GoalKickCommand::kLoftedIntoSpace;
  int receiver = -1;  // the mate for a lofted pass to a receiver; -1 for a kick into space (PES's player 0xFF)
};

// One tick of the ladder at `frame` (PES's logic frame counter). A transition tick emits nothing: the next tick is
// the phase's first evaluation, as after the decompile's stamp and `break`.
GoalKickOrder StepGoalKickLadder(GoalKickLadder& ladder, unsigned long frame, const GoalKickConstants& k,
                                 const GoalKickLadderInputs& in);

// The clip's contact offset in metres, Fox space (X forward, Y up, Z across): FUN_1406E8CC0's answer for the
// clip's first hit record, the ball's place in the clip's frame in cm. Scaled by the physique (clamped for x and
// z, not for y), and x is mirrored when the keeper strikes with his other foot (FUN_1406E6660(player, 0x34)).
// ponytail: the physique is the caller's (PES's player +0x3B18); GF passes 1.0 until that field is ported.
pes::Vec3 ClipOffsetMetres(const pes::Vec3& hitBallCm, float physique, bool mirror, const GoalKickConstants& k);

// SetKick's stand point, GF space: the spot plus the contact offset, its x and z negated, turned by the spot's
// heading (the placer sub_140732D40). The height is the spot's plus the offset's.
pes::Vec3 GoalKickStandPoint(const pes::Vec3& spot, float headingDeg, const pes::Vec3& clipOffsetFox_m);

// The root at the clip's contact, GF space: the stand point plus the clip's root motion to its first hit record (cm,
// Fox), turned by the heading and mirrored like the offset.
pes::Vec3 GoalKickContactRoot(const pes::Vec3& stand, float headingDeg, const pes::Vec3& hitRootCm, bool mirror,
                              const GoalKickConstants& k);

// The strike's contact tick counted from the release, in PES logic frames: 2 · hit frame · fps / 60 + rounding.
// The hit frame is at 30 Hz (HitData), so the tick is the frame times fps / 30 (FUN_1406B03D0, kind 0).
unsigned long GoalKickContactTicks(int hitFrame, const GoalKickConstants& k);

// The strike's root on tick `elapsed` of `ticks`: a straight warp from where the keeper stood at the release onto
// the contact root, reached on the contact tick and held there.
// ponytail: the clip's own per-frame root track is not decoded; the warp is linear to the hit (sub_140775780's
// root-motion warp, FUN_141EB35A0, names the exact curve).
pes::Vec3 GoalKickStrikeRoot(const pes::Vec3& from, const pes::Vec3& contactRoot, unsigned long elapsed,
                             unsigned long ticks);

// The lofted picker, stand-in. PES orders the mates by its per-team pass list (FUN_140a714e0, 0x229b4 / 0x22b9c) and
// filters them by lane and distance; GF has no port of that list yet, so the receiver is the best-rated mate other
// than the taker. Returns the index into `ratings`, -1 when no mate is left.
// ponytail: FUN_142135230 candidate list (per-team order, 5 m line test) replaces this rating.
int PickGoalKickReceiver(const std::vector<float>& ratings, int takerIndex);

// The launch speed is the touch's speed (humanoid.cpp: touchVec = dir * 36 * (power + 0.3)).
inline constexpr float kPassTouchSpeedPerPower_mps = 36.0f;
inline constexpr float kPassTouchPowerOffset = 0.3f;
// Receiving height of a lofted goal kick: OBS, the request's receive height ap+0x2bf8 is 1.5 m
// on long free kicks and goal kicks (pes-re-setpiece-kicker.md 4). Its writer is not decoded.
inline constexpr float kLoftedReceiveHeight_m = 1.5f;
// Upper bound of the launch-speed search. PES's own bound is the power byte (0..1); this bound
// is a GF search limit, not a PES value.
// ponytail: the PES predictor FUN_140731d80 (not in Ghidra) bounds the search itself.
inline constexpr float kLoftedSearchTop_mps = 60.0f;

// The touch's power that launches at `speed_mps`.
float PowerForLaunchSpeed(float speed_mps);

// The ground distance (metres) the ball covers when it first falls through `receiveHeight_m`,
// launched at `speed_mps` along `dir` from `start`. GF space: z is up. A ball that lands
// before reaching that height returns the distance it lands at.
float LoftedFlightDistance(const PesBallConstants& c, const Vec3& start, const Vec3& dir,
                           float speed_mps, float receiveHeight_m);

// The launch speed that makes LoftedFlightDistance equal `distance_m`. Bisection; returns
// kLoftedSearchTop_mps when the distance is out of reach.
float LoftedKickSpeed(const PesBallConstants& c, const Vec3& start, const Vec3& dir,
                      float distance_m, float receiveHeight_m);

}  // namespace pes
