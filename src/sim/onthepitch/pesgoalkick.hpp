#pragma once

// PES 2021's goal kick on the pes_loop path (docs/re/pes-re-goal-kick.md).
//
// The taker is the keeper: ThinkUnitSetPlay (MODE 3) gives him a lofted pass after the set-play
// wait, to the mate the lofted picker FUN_142135230 names, or a lofted kick with no receiver
// (player 0xff). The ground option needs elapsed < fps/2 frames at state 2, and state 1 only
// lets the think reach state 2 once elapsed >= fps, so the ground kick never comes out of the
// goal-kick path. The launch is solved against PES's own ball integrator (pesballphysics.hpp).

#include <vector>

#include "pesball.hpp"
#include "pesballphysics.hpp"
#include "pes/types.hpp"

namespace pes {

enum class GoalKickCommand { kLoftedToReceiver, kLoftedIntoSpace };

// The command after the set-play wait: lofted to the picked receiver, or into space.
GoalKickCommand GoalKickAfterWait(bool receiverFound);

// The lofted picker, stand-in. PES orders the mates by its per-team pass list (FUN_140a714e0,
// 0x229b4 / 0x22b9c) and filters them by lane and distance; GF has no port of that list yet, so
// the receiver is the best-rated mate other than the taker. Returns the index into `ratings`, -1
// when no mate is left.
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
