#include "gdext/pes_cutscene_camera.hpp"
#include "pes/fixdemo_table.hpp"

#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <string>

namespace godot {

// The camera record's mode byte (+0xB0) of the procedural camera captured at a goal (docs/ports/pes-port-ProcCamera.md section 1).
constexpr uint8_t kProceduralMode = 4;

// A track for one camera record of `file`: the record's CANM clip (embedded, or a VFS file the resource names). A record with an
// empty anime path is procedural (PES slot +0x50 SetProcedural 0x141F7B630, update 0x141F7BFE0): a mode-4 one (the record's
// +0xB0) becomes a FixDemoCamera of the step's rig, whose engine frames pose it (advance); a procedural record of another
// mode keeps a track without a clip, and update() returns false while it is in force.
// ponytail: modes other than 4 (the foul cut 0x20007 and other cmnCam layers) are not proven on the translated engine, which
// aborts loudly on a mode it was not captured in; the caller keeps its broadcast camera for them until a capture proves them.
Error PesCutsceneCamera::AddTrack(const Ref<PesLibrary>& library, const pes::fdc::File& file, const pes::fdc::CameraRecord& cam) {
  Track track;
  track.record = cam;
  if (!cam.animePath.empty()) {
    const auto& vfs = library->vfs();
    std::string err;
    const pes::fdc::Resource* res = file.FindResource(cam.animePath);
    if (!res) {
      UtilityFunctions::push_error(String("PesCutsceneCamera: no resource for ") + cam.animePath.c_str());
      return ERR_FILE_NOT_FOUND;
    }
    std::vector<uint8_t> clip;
    if (res->external) {
      auto ext = vfs.Read(res->vfsPath, &err);
      if (!ext) return ERR_FILE_NOT_FOUND;
      clip = std::move(*ext);
    } else {
      clip = res->data;
    }
    pes::fdc::Canm canm;
    if (!pes::fdc::ParseCanm(clip, &canm, &err)) {
      UtilityFunctions::push_error(String("PesCutsceneCamera: bad clip ") + cam.animePath.c_str() + ": " + err.c_str());
      return ERR_PARSE_ERROR;
    }
    track.canm = std::move(canm);
  } else if (cam.targetBytes[0] == kProceduralMode && library->exe()) {
    if (!rig_) {
      std::string err;
      rig_ = pes::FixDemoCameraRig::Create(*library->exe(), &err);
      if (!rig_) {
        UtilityFunctions::push_error(String("PesCutsceneCamera: no procedural camera rig: ") + err.c_str());
        return ERR_UNAVAILABLE;
      }
    }
    track.rigId = int(rig_->AddCamera(cam.raw));
  }
  tracks_.push_back(std::move(track));
  return OK;
}

Error PesCutsceneCamera::load_file(const Ref<PesLibrary>& library, const String& vfs_path, int camera_index) {
  if (!library.is_valid()) return ERR_UNCONFIGURED;
  const auto& vfs = library->vfs();
  std::string err;
  const std::string path = vfs_path.utf8().get_data();
  auto bytes = vfs.Read(path, &err);
  if (!bytes) {
    UtilityFunctions::push_error(String("PesCutsceneCamera: cannot read ") + vfs_path + ": " + err.c_str());
    return ERR_FILE_NOT_FOUND;
  }
  pes::fdc::File file;
  if (!pes::fdc::Parse(*bytes, &file, &err)) {
    UtilityFunctions::push_error(String("PesCutsceneCamera: cannot parse ") + vfs_path + ": " + err.c_str());
    return ERR_PARSE_ERROR;
  }
  if (camera_index < 0 || size_t(camera_index) >= file.cameras.size()) return ERR_INVALID_PARAMETER;
  tracks_.clear();
  cutHolder_.cuts = file.cuts;
  k_ = *library->cutscene_camera_constants();
  return AddTrack(library, file, file.cameras[size_t(camera_index)]);
}

Error PesCutsceneCamera::load_step(const Ref<PesLibrary>& library, int category, const PackedStringArray& names) {
  if (!library.is_valid()) return ERR_UNCONFIGURED;
  const auto& vfs = library->vfs();
  tracks_.clear();
  cutHolder_.cuts.clear();
  k_ = *library->cutscene_camera_constants();
  for (int i = 0; i < names.size(); ++i) {
    const std::string path = pes::FixDemoCutDataPath(category, names[i].utf8().get_data());
    if (path.empty() || !vfs.Exists(path)) continue;
    std::string err;
    auto bytes = vfs.Read(path, &err);
    pes::fdc::File layer;
    if (!bytes || !pes::fdc::Parse(*bytes, &layer, &err)) {
      UtilityFunctions::push_error(String("PesCutsceneCamera: cannot read layer ") + path.c_str() + ": " + err.c_str());
      return ERR_PARSE_ERROR;
    }
    // The player's cut map holds the Cut records of every layer (the first of them starts the step: pes::fdc::FirstCut).
    cutHolder_.cuts.insert(cutHolder_.cuts.end(), layer.cuts.begin(), layer.cuts.end());
    for (const pes::fdc::CameraRecord& cam : layer.cameras) AddTrack(library, layer, cam);  // a bad clip is logged and skipped
  }
  return OK;
}

// INF (docs/ports/pes-port-CutsceneTiming.md section 5): the record in force is the last one started on the clock; records
// that start at the same tick are not told apart (ponytail: PES picks among them by the records' selectByRate, 23
// percentages; the first of them in layer order stands in until 0x141F42110 is decoded).
const PesCutsceneCamera::Track* PesCutsceneCamera::InForce(float clock) const {
  const Track* force = nullptr;
  for (const Track& t : tracks_)
    if (t.record.startTime <= clock && (!force || t.record.startTime > force->record.startTime)) force = &t;
  return force;
}

// The player time the engine takes (1/60 s ticks): the record's own clock, started at its startCutTime unless the record
// plays on the player clock as is (timeMode 0), as the clip path reads it (docs/re/pes-re-camera-pose.md).
static float RecordTicks(const pes::fdc::CameraRecord& r, float clock) { return r.timeMode ? clock - r.startTime : clock; }

void PesCutsceneCamera::set_target(int slot, const Vector3& ground, const Vector3& point_a, const Vector3& point_b, double facing_deg) {
  if (!rig_ || slot < 0 || slot >= int(pes::PesCameraRegistry::kTargets)) return;
  pes::PesCameraRegistry::Target t;
  t.valid = true;
  t.ground[0] = float(ground.x), t.ground[1] = float(ground.y), t.ground[2] = float(ground.z);
  t.pointA[0] = float(point_a.x), t.pointA[1] = float(point_a.y), t.pointA[2] = float(point_a.z);
  t.pointB[0] = float(point_b.x), t.pointB[1] = float(point_b.y), t.pointB[2] = float(point_b.z);
  t.facingDeg = float(facing_deg);
  rig_->SetTarget(unsigned(slot), t);
}

void PesCutsceneCamera::advance(double t_s) {
  if (!rig_ || !k_) return;
  const float clock = float(t_s) * k_->sixty;
  const Track* force = InForce(clock);
  if (!force || force->rigId < 0) return;
  pose_ = rig_->Step(size_t(force->rigId), RecordTicks(force->record, clock));
}

bool PesCutsceneCamera::update(Camera3D* camera, double t_s) {
  if (!camera || !k_) return false;
  const pes::fdc::PoseConstants& k = *k_;
  const float clock = float(t_s) * k.sixty;
  const Track* force = InForce(clock);
  if (force && force->rigId >= 0) {
    if (!pose_) return false;  // advance() has not run for this camera yet
    // The engine's params: eye, target and up (Fox space), the vertical fov in radians, near and far.
    const Vector3 eye(pose_->eye[0], pose_->eye[1], pose_->eye[2]), target(pose_->target[0], pose_->target[1], pose_->target[2]);
    const Vector3 back = (eye - target).normalized();
    const Vector3 right = Vector3(pose_->up[0], pose_->up[1], pose_->up[2]).cross(back).normalized();
    camera->set_keep_aspect_mode(Camera3D::KEEP_HEIGHT);
    camera->set_fov(pose_->fovRad * k.oneEighty / k.pi);
    camera->set_near(pose_->nearM);
    camera->set_far(pose_->farM);
    camera->set_global_transform(Transform3D(Basis(right, back.cross(right), back), eye));
    return true;
  }
  if (!force || !force->canm) return false;
  // ponytail: a record's height correction (target height from 0x141F7E3C0) is 0 here: no FixDemo player
  // supplies a target. Replace it with that player's value when it is ported (the entrance records have rate 0).
  const pes::fdc::CameraPose pose = pes::fdc::EvaluateCameraPose(k, *force->canm, force->record, clock,
                                                                 pes::fdc::CutStartOffset(cutHolder_, stageDefault_), 0.0f);
  const Viewport* vp = camera->get_viewport();
  const Vector2 size = vp ? vp->get_visible_rect().size : Vector2(1, 1);
  const pes::fdc::RenderCamera r = pes::fdc::RenderCameraParams(k, pose, force->record, float(size.x / size.y), widescreenFix_);
  camera->set_keep_aspect_mode(Camera3D::KEEP_HEIGHT);  // the clip's fov is vertical
  camera->set_fov(r.fovRad * k.oneEighty / k.pi);
  camera->set_near(r.nearM);
  camera->set_far(r.farM);
  // The pose's columns are (-right, up, forward); Godot's basis is (right, up, -forward).
  const Basis basis(Vector3(-pose.m[0][0], -pose.m[1][0], -pose.m[2][0]), Vector3(pose.m[0][1], pose.m[1][1], pose.m[2][1]),
                    Vector3(-pose.m[0][2], -pose.m[1][2], -pose.m[2][2]));
  camera->set_global_transform(Transform3D(basis, Vector3(pose.eye[0], pose.eye[1], pose.eye[2])));
  return true;
}

void PesCutsceneCamera::_bind_methods() {
  ClassDB::bind_method(D_METHOD("load_step", "library", "category", "names"), &PesCutsceneCamera::load_step);
  ClassDB::bind_method(D_METHOD("load_file", "library", "vfs_path", "camera_index"), &PesCutsceneCamera::load_file);
  ClassDB::bind_method(D_METHOD("update", "camera", "t_s"), &PesCutsceneCamera::update);
  ClassDB::bind_method(D_METHOD("advance", "t_s"), &PesCutsceneCamera::advance);
  ClassDB::bind_method(D_METHOD("set_target", "slot", "ground", "point_a", "point_b", "facing_deg"), &PesCutsceneCamera::set_target);
  ClassDB::bind_method(D_METHOD("set_widescreen_fix", "on"), &PesCutsceneCamera::set_widescreen_fix);
  ClassDB::bind_method(D_METHOD("set_stage_default", "pos", "rot_y_deg", "flip"), &PesCutsceneCamera::set_stage_default);
}

}  // namespace godot
