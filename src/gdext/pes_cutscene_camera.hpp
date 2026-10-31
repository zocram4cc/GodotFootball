#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <memory>
#include <optional>
#include <vector>

#include "gdext/pes_library.hpp"
#include "pes/fdc.hpp"
#include "pes/fixdemo_camera_rig.hpp"

namespace godot {

// The cameras of one FixDemo step on a Camera3D. A step's layers (the variant's .fdc files) each carry camera records; each
// record names a CANM clip (docs/re/pes-re-cutscenes.md §3.2 type 6) and starts at its startCutTime on the step's player
// clock (1/60 s ticks, the pes::CutsceneTimeline clock). The .fdc files are read through the mounted VFS at load time, so a
// mod that wins the mount is the one played. Fox space is Godot space here (Y up, metres, pitch-centre origin), as
// PesBroadcastCamera assumes. The pose is FixDemoCamera's (docs/re/pes-re-camera-pose.md; src/pes/fdc.hpp): near/far come
// from the record, the CANM's own near/far are ignored.
class PesCutsceneCamera : public RefCounted {
  GDCLASS(PesCutsceneCamera, RefCounted)

 public:
  // Opens every camera record of every named layer of the category's cut_data folder (a variant's .fdc names, in the order
  // the FixDemo request lists them) and the Cut records they carry. OK even when no layer has a camera.
  Error load_step(const Ref<PesLibrary>& library, int category, const PackedStringArray& names);
  // Opens vfs_path (a common/demo/fixdemo/... .fdc) and takes its camera record camera_index (the --fdc-file debug track).
  Error load_file(const Ref<PesLibrary>& library, const String& vfs_path, int camera_index);
  // Poses camera for t_s seconds of the player clock: a clip camera on the clock, a mode-4 procedural camera by its last
  // advance(). False when no such camera is in force: before the first record's startCutTime, or when the record in force is
  // a procedural record of another mode (ponytail: the engine 0x141F7BFE0 is proven only for mode 4); the caller keeps its own
  // camera then.
  bool update(Camera3D* camera, double t_s);
  // The step's procedural cameras (records with no CANM clip, mode 4): PES's engine 0x141F7BFE0 runs on the translated layer
  // (pes::FixDemoCameraRig), once per logic frame. advance() steps the record in force with the step's player clock
  // (seconds, the value cutscene_state() gives) and keeps the pose update() poses the camera with; set_target() writes
  // CAMERA_TARGET_02<slot>, the record of the scene's actor the camera frames: ground position, the two body points the
  // readers 0x14089AC40 take (Fox space = Godot space), facing in degrees. A no-op without a procedural track.
  void advance(double t_s);
  void set_target(int slot, const Vector3& ground, const Vector3& point_a, const Vector3& point_b, double facing_deg);
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
  struct Track {
    pes::fdc::CameraRecord record;
    std::optional<pes::fdc::Canm> canm;  // empty: a procedural record (empty anime path)
    int rigId = -1;                      // a mode-4 procedural record: its FixDemoCamera in rig_
  };
  Error AddTrack(const Ref<PesLibrary>& library, const pes::fdc::File& file, const pes::fdc::CameraRecord& cam);
  std::vector<Track> tracks_;
  const Track* InForce(float clock) const;
  std::shared_ptr<pes::FixDemoCameraRig> rig_;
  std::optional<pes::CameraPose> pose_;  // the last engine frame of the track in force
  std::optional<pes::fdc::PoseConstants> k_;
  pes::fdc::CutOffset stageDefault_;
  pes::fdc::File cutHolder_;  // only .cuts is filled: the Cut records the player's map holds
  bool widescreenFix_ = false;
};

}  // namespace godot
