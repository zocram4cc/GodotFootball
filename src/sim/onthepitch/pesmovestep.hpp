// PES 2021's MOVESTEP sim adapter: one PES logic tick of FUN_1406ba900 per player, over GF's
// Player / Humanoid. The adapter owns the request block, the per-tick tail (the P+0xbec copy
// and the P+0x3758 playback store) and the transition. Each Issue is recorded on the player
// (Player::GetIssuedClip), which the viewer plays; GF's controller and humanoid still decide the
// sim's movement. Values PES reads from tables GF has not ported
// keep a `// ponytail:` note naming the site that replaces them.
#pragma once

#include "pes/movestep.hpp"

class Player;
class Match;

namespace pesmovestep {

// Runs one MOVESTEP tick for an active player. It is a no-op when the PES install is absent
// (no PES21_ROOT), so a checkout without the game can still run.
void ProcessMoveStep(Player& player, Match& match);
// PES's bearing (degrees, folded to [0, 360)) of a GF ground offset (gfX, gfZ) = (dx, coords[1] delta).
// PES's z is -coords[1] (referee.cpp:197), so the bearing is atan2(dx, -gfZ). Every heading the
// adapter gives PES is read through this one function.
float PesBearingDeg(const pes::RunConstants& k, float gfX, float gfZ);
// The PES motion number the humanoid plays (movestep.hpp's CurrentMotion), or -1 when the AI's own clip plays.
int CurrentMotionOf(Player& player);

}  // namespace pesmovestep
