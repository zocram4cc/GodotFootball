#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
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
  // Opens the camera_index-th camera of the first of the named cut layers (a variant's .fdc names under the category's
  // cut_data folder, in the order the FixDemo request lists them) that has one.
  Error load_layers(const Ref<PesLibrary>& library, int category, const PackedStringArray& names, int camera_index);
  // Opens vfs_path (a common/demo/fixdemo/... .fdc) and takes its camera record camera_index.
  Error load_file(const Ref<PesLibrary>& library, const String& vfs_path, int camera_index);
  // Length of the clip in seconds, from its frame count and frame rate.
  double duration_s() const;
  // Poses camera for t_s seconds of the player clock (the record's startCutTime is on the same clock; the
  // clip is held at its end pose).
  void update(Camera3D* camera, double t_s);
  // The option byte G(0x141F500D0)+0x522E of PES's fov widening for displays narrower than 16:9
  // (0x141F7B250). ponytail: GF has no such option, so it defaults to off; wire it to that option's GF
  // source when the settings are ported.
  void set_widescreen_fix(bool on) { widescreenFix_ = on; }
  // The scene's stage default (player+0xAC0 position, +0xACC rotY degrees, +0xAD0 flip): the offset a cut starts
  // with unless its first Cut record carries its own (pes::fdc::CutStartOffset, 0x141F2E11C); applied when the
  // camera record's use-cut-offset byte (+0xA6) is set. Zero is the value every entrance activation read
  // (docs/ports/pes-port-FdcCamera5.md). ponytail: for goal scenes PES writes it in the scene start 0x141F5CD00
  // from the step's placement mode; GF feeds it from its own goal state until that is ported.
  void set_stage_default(const Vector3& pos, double rot_y_deg, bool flip) {
    stageDefault_.pos = {float(pos.x), float(pos.y), float(pos.z)};
    stageDefault_.rotYDeg = float(rot_y_deg);
    stageDefault_.flip = flip;
  }

 protected:
  static void _bind_methods();

 private:
  std::optional<pes::fdc::Canm> canm_;
  std::optional<pes::fdc::PoseConstants> k_;
  pes::fdc::CameraRecord record_;
  pes::fdc::CutOffset stageDefault_;
  pes::fdc::File cutHolder_;  // only .cuts is filled: the Cut records the player's map holds
  bool widescreenFix_ = false;
};

}  // namespace godot
