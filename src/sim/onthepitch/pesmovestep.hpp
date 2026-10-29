// PES 2021's MOVESTEP sim adapter: one PES logic tick of FUN_1406ba900 per player, over GF's
// Player / Humanoid. The adapter owns the request block, the per-tick tail (the P+0xbec copy
// and the P+0x3758 playback store) and the transition. Each Issue is recorded on the player
// (Player::GetIssuedClip), which the viewer plays; GF's controller and humanoid still decide the
// sim's movement. Values PES reads from tables GF has not ported
// keep a `// ponytail:` note naming the site that replaces them.
#pragma once

#include <optional>

#include "gamedefines.hpp"
#include "pes/movestep.hpp"

class Player;
class Match;

namespace pesmovestep {

// Runs one MOVESTEP tick for an active player. It is a no-op when the PES install is absent
// (no PES21_ROOT), so a checkout without the game can still run.
void ProcessMoveStep(Player& player, Match& match);
// PES's AI think for one player, run every tick by Player::Process after the controller's process: the decision MOVESTEP
// reads on the next tick. A no-op with pes_loop off.
void Think(Player& player, Match& match);
// The STOP entry as a handler calls it (FUN_1406d5d60): the request is filled as ProcessMoveStep fills it, EnterStop writes the
// entry's words, and the record takes the requested state. The wire test uses it to put a player into STOP by PES's own entry.
void RequestStop(Player& player, Match& match);
// The request block MOVESTEP reads this tick, as ProcessMoveStep fills it before the handler: the copy-in of the dwell
// words and stamps, the facing, and the command words of the AI's decision. The wire test reads its move target.
pes::MoveReq TickRequest(Player& player, Match& match);
// PES's frame of a GF point (x, GF y, GF z) -> (x, height = GF z, ground z = -GF y), the order of P+8, P+0xc, P+0x10.
// The adapter gives MOVESTEP every position and displacement through this one function.
void PesFrame(float gfX, float gfY, float gfZ, float out[3]);
// PES's bearing (degrees, folded to [0, 360)) of a GF ground offset (gfX, gfZ) = (dx, coords[1] delta).
// PES's z is -coords[1] (referee.cpp:197), so the bearing is atan2(dx, -gfZ). Every heading the
// adapter gives PES is read through this one function.
float PesBearingDeg(const pes::RunConstants& k, float gfX, float gfZ);
// The PES motion number the humanoid plays (movestep.hpp's CurrentMotion), or -1 when the AI's own clip plays.
int CurrentMotionOf(Player& player);
// MOVESTEP's decision: the first queued command that is not a ball action. Ball actions ride the pending ball request the
// kick dispatcher consumes (docs/re/pes-re-decision-read.md 1 and 3), so MOVESTEP steers by the movement command.
std::optional<PlayerCommand> MovementDecision(const PlayerCommandQueue& queue);
// PES's command id (cmd[0], req[0x16]) for GF's movement command: MOVE (2), or 0 with no command or a ball action.
int CommandIdFor(const PlayerCommand* cmd);
// PES's move kind (cmd[0x16], req[0]) for a player's move: the near keeper kind (4) for a keeper, else the mid kind (0).
int MoveKindFor(bool keeper);
// The physique block's height in whole cm (PM+0x30; GF's physical_height is that /100, so rounding restores it).
float PhysiqueHeightCmOf(const Player& player);
// The Speed ability byte (slot record +0x97, 40..99). ponytail: no GF source for PES's base row; physical_velocity stands in.
float SpeedByteOf(const Player& player);

}  // namespace pesmovestep
