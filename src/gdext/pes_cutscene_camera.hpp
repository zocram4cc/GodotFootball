#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <optional>

#include "gdext/pes_library.hpp"
#include "pes/fdc.hpp"

namespace godot {

// Plays one camera record of a PES FixDemo cut file (.fdc) on a Camera3D. The record names a CANM
// clip (docs/re/pes-re-cutscenes.md §3.2 type 6); the clip's six channels give the pose per frame.
// The .fdc is read through the mounted VFS at load time, so a mod that wins the mount is the one
// played. Fox space is Godot space here (Y up, metres, pitch-centre origin), as PesBroadcastCamera
// assumes. Time: the record's startCutTime is in 1/60 s ticks (docs/re/pes-re-fdc-format.md §3).
class PesCutsceneCamera : public RefCounted {
  GDCLASS(PesCutsceneCamera, RefCounted)

 public:
  // Opens the camera_index-th camera of the first .fdc layer of a FixDemo scene that has one. scene_id is
  // (category << 16) | index, e.g. 0x20009 (pes/fixdemo_table.hpp: FixDemoSceneFdcPaths).
  Error load_scene(const Ref<PesLibrary>& library, int scene_id, int camera_index);
  // Length of the clip in seconds, from its frame count and frame rate.
  double duration_s() const;
  // Poses camera for t_s seconds after the cut block's zero. Before the record's start and after the
  // clip's last frame the end pose holds.
  void update(Camera3D* camera, double t_s);

 protected:
  static void _bind_methods();

 private:
  // Opens vfs_path (a common/demo/fixdemo/... .fdc) and takes its camera record camera_index.
  Error load_file(const Ref<PesLibrary>& library, const String& vfs_path, int camera_index);

  std::optional<pes::fdc::Canm> canm_;
  float startTicks_ = 0.0f;
  float nearOverride_ = 0.0f, farOverride_ = 0.0f;  // the record's clip planes (override the clip's)
};

}  // namespace godot
