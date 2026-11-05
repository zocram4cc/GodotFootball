// The player skeleton the simulation animates, built from the PES animation
// rig instead of GF's media/objects/players/player.object.

#ifndef _HPP_ONTHEPITCH_PESPLAYERNODE
#define _HPP_ONTHEPITCH_PESPLAYERNODE

#include <map>
#include <string>

#include "onthepitch/pesanimechain.hpp"
#include "scene/scene3d/node.hpp"

namespace pesrig {
struct Rig;
}

// Node("player"), absolute, holding the rig's joints in joint-ID order under
// their GF names, at the rig's anim-bind local positions in GF space
// (Fox (x, y, z) -> GF (x, -z, y)), identity rotations. The body parts GF's
// player.object gave a collision mesh carry a Geometry with that mesh's
// bounding box, which is all the simulation reads of them.
boost::intrusive_ptr<blunted::Node> BuildPlayerNode(const pesrig::Rig& rig);

// The player's current pose as the anime player's joint arrays (pes::PesJointPose): each body joint's world position relative to the
// player node and its world rotation, taken out of the heading (`headingDeg`, the PES yaw P+0x20) and into PES axes (GF (x, y, z) ->
// PES (x, z, -y)). `nodes` is the humanoid's node map (it holds "player" and the rig's gfNames).
void BuildJointPose(const std::map<const std::string, boost::intrusive_ptr<blunted::Node>>& nodes, const pesrig::Rig& rig,
                    float headingDeg, pes::PesJointPose& out);

#endif
