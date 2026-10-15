#pragma once

// PES body clips played by the simulation, converted from the user's install
// at match start (nothing PES-derived is committed). The conversion is the
// verbatim one of AGENTS.md's skeleton section: the engine's skeleton IS the
// PES anim rig, so each joint's local rotation is carried across with the
// Fox -> engine basis map (x, y, z) -> (x, -z, y) and nothing else; the body
// joint takes RIG_ROOT o mover, and the player line the world translation
// (rig/clip_pose.hpp samples both, at PES_FRAME_S and the gameplay scale).
//
// Clips join the AnimCollection as `special` animations keyed by
// (specialvar1 = family, specialvar2 = the clip's motion number), so a
// command naming the pair plays exactly that clip.

#include <optional>
#include <string>
#include <vector>

#include "onthepitch/pescontact.hpp"
#include "pes/exe.hpp"
#include "pes/gani.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"

namespace pes {

// The specialvar1 families imported clips carry. Celebrations and cards use
// 1..3 (gamedefines.hpp), so imported families start well clear of them.
enum class PesClipFamily : int { kDodge = 100, kTackle = 101 };

struct PesClip {
  std::string name;
  int motionNo = -1;  // index in the exe's motion table (pes::MotionNames)
  PesClipFamily family = PesClipFamily::kDodge;
  Gani gani;
};

struct PesClipSet {
  std::vector<PesClip> clips;
};

// The clips of every family, by name prefix in the exe's motion table:
// family kDodge = the `dodge_`, `avoidjump_`, `avoidslide_`, `goalnet_`,
// `rapidstop_` and `rapid_3_2_dodge_` motions the Dodge action selects
// (docs/re/pes-re-contact-port.md pass 4 stage B);
// family kTackle = the `tacklefoot_` motions the Tackle action's sub-kind
// group tables select (stage C).
std::optional<PesClipSet> LoadPesClips(const PeImage& exe, const PesVfs& vfs, std::string* error);

// One clip as engine .anim text (the format Animation::LoadText reads), at the
// engine's 10 ms frame.
std::string PesClipAnimText(const PesClip& clip, const pesrig::Rig& rig);

// The contact action of a player whose current clip is a Special command:
// a PES clip is the action its family names (the player carries which one in
// his PES challenge - SetPesChallenge at the command), and every other special
// animation is not a challenge. Without this the contact layer sees a dodger or
// a PES tackler as OTHER, so PES's dodge rules and the challenge count skip them.
inline bool IsPesClipFamily(int specialVar1) {
  return specialVar1 == int(PesClipFamily::kDodge) || specialVar1 == int(PesClipFamily::kTackle);
}

inline ContactAction SpecialClipAction(int specialVar1, ContactAction challenge) {
  return IsPesClipFamily(specialVar1) ? challenge : ContactAction::kOther;
}

}  // namespace pes
