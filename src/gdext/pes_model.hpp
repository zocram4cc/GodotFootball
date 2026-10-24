#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

#include "gdext/pes_library.hpp"

namespace godot {

// Static PES models (ball, goals, corner flags, stadium scenes, pitch): every
// .fmdl of a pack, or only `member`, as MeshInstance3Ds in Fox space with
// their techniques' shaders.
class PesModelBuilder : public RefCounted {
  GDCLASS(PesModelBuilder, RefCounted)

 public:
  Node3D* build_model(const Ref<PesLibrary>& library, const String& fpk_path, const String& member) const;
  // One stadium as PES places it: every model of its scene, its pitch, both
  // goals with their nets, and the corner flags, each built with build_model
  // under a Node3D carrying the fox2 transform. `weather` (0 fine, 1 rain, 2
  // snow) picks the weather-only layers. Null when the install has no such
  // stadium.
  Node3D* build_stadium(const Ref<PesLibrary>& library, int stadium_id, int weather) const;
  // PES's turf3d fin cover (docs/render/pes-render-TurfPlacer.md 6.1): the
  // install's turf3dtexlodu4 tile instanced over the kept placer slots of
  // the ground rect, drawn with pes3DDF_Turf3D2020 (pes_turf.gdshader).
  // Null when the install lacks the mesh, the exe grid, or the stadium.
  Node3D* build_turf_fins(const Ref<PesLibrary>& library, int stadium_id) const;

 protected:
  static void _bind_methods();
};

}  // namespace godot
