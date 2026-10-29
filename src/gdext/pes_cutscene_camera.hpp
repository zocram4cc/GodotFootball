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
// assumes. The pose is FixDemoCamera's (docs/re/pes-re-camera-pose.md; src/pes/fdc.hpp): time is the
// player clock in 1/60 s ticks, near/far come from the record, the CANM's own near/far are ignored.
class PesCutsceneCamera : public RefCounted {
  GDCLASS(PesCutsceneCamera, RefCounted)

 public:
  // Opens the camera_index-th camera of the first .fdc layer of a FixDemo scene that has one. scene_id is
  // (category << 16) | index, e.g. 0x20009 (pes/fixdemo_table.hpp: FixDemoSceneFdcPaths).
  Error load_scene(const Ref<PesLibrary>& library, int scene_id, int camera_index);
  // Length of the clip in seconds, from its frame count and frame rate.
  double duration_s() const;
  // Poses camera for t_s seconds of the player clock (the record's startCutTime is on the same clock; the
  // clip is held at its end pose).
  void update(Camera3D* camera, double t_s);
  // The option byte G(0x141F500D0)+0x522E of PES's fov widening for displays narrower than 16:9
  // (0x141F7B250). ponytail: GF has no such option, so it defaults to off; wire it to that option's GF
  // source when the settings are ported.
  void set_widescreen_fix(bool on) { widescreenFix_ = on; }

 protected:
  static void _bind_methods();

 private:
  // Opens vfs_path (a common/demo/fixdemo/... .fdc) and takes its camera record camera_index.
  Error load_file(const Ref<PesLibrary>& library, const String& vfs_path, int camera_index);

  std::optional<pes::fdc::Canm> canm_;
  std::optional<pes::fdc::PoseConstants> k_;
  pes::fdc::CameraRecord record_;
  bool widescreenFix_ = false;
};

}  // namespace godot
