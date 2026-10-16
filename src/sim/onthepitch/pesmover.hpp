#ifndef _HPP_ONTHEPITCH_PESMOVER
#define _HPP_ONTHEPITCH_PESMOVER

#include <cstdint>

#include "pes/types.hpp"

namespace pesmover {

// PES's player-velocity accessors, read from FUN_140a6c6d0 (floored velocity, F4),
// FUN_140a6c990 (mover velocity, F6) and FUN_140458740 (heading of F4). The record
// fields are PES's: movement state +0x5b0, facing in degrees +0x554, and velocity in
// m per tick at +0x574, +0x578, +0x57c.
inline constexpr int kMovementStill = 1;  // the player stands still
inline constexpr int kMovementSlow = 5;   // a slow walk, floored below the speed floor

// PES's speed floor, 0.6944444 (2.5 km/h), at .rdata 0x14259bdd0. The caller divides
// it by the game speed and passes it as minSpeed.
inline constexpr uint64_t kSpeedFloorVa = 0x14259bdd0;
// 2*pi at .rdata 0x1425795e4, the fold of a negative heading.
inline constexpr uint64_t kTwoPiVa = 0x1425795e4;

struct MoverRecord {
  int movementState;   // +0x5b0
  float facingDeg;     // +0x554
  pes::Vec3 velocity;  // +0x574, +0x578, +0x57c
};

// FUN_140a6c6d0: the velocity with the floor. A still player, or a slow walker whose
// speed is below minSpeed, moves at minSpeed along his facing; anyone else keeps his
// velocity.
pes::Vec3 FloorVelocity(const MoverRecord& m, float minSpeed);

// FUN_140a6c990: the mover's velocity. A still player, or a slow walker below
// minSpeed, has none.
pes::Vec3 MoverVelocity(const MoverRecord& m, float minSpeed);

// FUN_140458740: the heading of FloorVelocity, atan2(x, z) in degrees folded into
// [0, 360). A zero vector has heading 0.
float FloorBearingDeg(const MoverRecord& m, float minSpeed);

}  // namespace pesmover

#endif
