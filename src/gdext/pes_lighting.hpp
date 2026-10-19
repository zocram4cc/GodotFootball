#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

#include "gdext/pes_library.hpp"

namespace godot {

// One stadium's match light rig (docs/re/pes-re-lighting.md §0-8): the sun
// from the rig's astronomical fit, the floodlights with their per-span
// groups, the reflection probes, the sky parameters and the shadow/fog
// recipe, as children of this node (lights and probes) plus a WorldEnvironment
// and a Camera3D whose compositor carries the PesPostEffect (godot/post), both
// found by name on siblings. The exposure and the grading pair are the post
// chain's: this hands it the rig's values for the span it sets up.
class PesLighting : public Node3D {
  GDCLASS(PesLighting, Node3D)

 public:
  // weather: 0 fine, 1 rain, 2 snow. time_zone: 0 day, 1 night. season: 0
  // summer, 1 winter. hour < 0 selects the span by time zone/season (§2.3);
  // otherwise the explicit-time span. Builds (or rebuilds) the rig nodes.
  // Returns false (and pushes an error) when the rig is missing/unreadable.
  bool setup(const Ref<PesLibrary>& library, int stadium_id, int weather, int time_zone,
             int season, int hour, int minute);

 protected:
  static void _bind_methods();
};

}  // namespace godot
