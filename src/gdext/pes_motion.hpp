#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <cstdint>
#include <map>
#include <vector>

#include "gdext/pes_animation.hpp"
#include "gdext/pes_library.hpp"
#include "pes/locomotion.hpp"

namespace godot {

// Plays the clip MOVESTEP last issued for each player (GfMatch.player_clip): a single PES clip
// (SetMotion) or a parametric blend group (SetParametric), from the issue's start time, advanced
// at the playback rate PES keeps on the player (docs/re/pes-re-anim-blend.md 1-5). An issue
// crossfades from the clip that was playing over its blend time, and the outgoing clip keeps
// playing until the blend ends.
class PesMotionPlayer : public RefCounted {
  GDCLASS(PesMotionPlayer, RefCounted)

 public:
  Error load(const Ref<PesLibrary>& library);
  // Poses `skeleton` for the player at `index`, then advances that player's clock by
  // `delta_seconds`. A player with no issue yet keeps the pose its skeleton has (the bind pose).
  Error update(int index, Skeleton3D* skeleton, const Dictionary& clip, double delta_seconds);

 protected:
  static void _bind_methods();

 private:
  struct Command {
    int motion = -1;
    int group = -1;          // -1: SetMotion; else the PES blend group
    bool mirror = false;
    double startTime = 0.0;  // seconds into a single clip
    double startRate = 0.0;  // the parametric phase at the issue, in cycles
    double rate = 0.0;       // the parametric group's bracket position
    double duration = 0.0;   // crossfade seconds from the outgoing clip
  };
  // A clip playing: its command and its clock (seconds of clip, or cycles of a parametric group).
  struct Layer {
    Command command;
    double clock = 0.0;
  };
  struct Sampled {
    pesrig::ClipPose pose;
    Ref<PesAnimation> clip;  // the clip whose hands the pose takes
    double handTime = 0.0;
    bool mirror = false;
  };
  struct Track {
    uint32_t serial = 0;
    bool issued = false;
    Layer current;
    Layer previous;
    bool hasPrevious = false;
    double blendElapsed = 0.0;
    double blendTotal = 0.0;
    double playback = 1.0;
  };
  struct Group {
    std::vector<pes::BlendMember> members;  // sorted by parameter
    std::vector<float> params;
  };

  static Command CommandOf(const Dictionary& clip);
  Ref<PesAnimation> ClipFor(int motion);
  const Group& GroupOf(int group);
  // The two members a parametric command plays, and the weight of the higher one.
  bool Members(const Command& c, pes::Bracketed& b, Ref<PesAnimation>& lo, Ref<PesAnimation>& hi);
  bool Sample(const Layer& layer, Sampled& out);
  void Advance(Layer& layer, double seconds);

  Ref<PesLibrary> library_;
  std::vector<pes::BlendMember> blend_;     // the parametric blend table
  std::map<int, Ref<PesAnimation>> clips_;  // by motion number, loaded on first use
  std::map<int, Group> groups_;             // by blend group
  std::vector<Track> tracks_;               // by player index
};

}  // namespace godot
