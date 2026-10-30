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

// A track for one camera record of `file`: the record's CANM clip (embedded, or a VFS file the resource names). A record with an
// empty anime path is procedural: it gets a track without a clip.
// ponytail: a procedural record (PES slot +0x50 SetProcedural 0x141F7B630, update 0x141F7BFE0) frames the record's
// targetKind/targetNo humans with its zoom stages. That update is not ported, so such a record has no pose here: when it is the
// one in force, update() returns false and the caller keeps its broadcast camera. The foul cut 0x20007 and the goal cmnCam
// layers are made of them.
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

bool PesCutsceneCamera::update(Camera3D* camera, double t_s) {
  if (!camera || !k_) return false;
  const pes::fdc::PoseConstants& k = *k_;
  const float clock = float(t_s) * k.sixty;
  // INF (docs/ports/pes-port-CutsceneTiming.md section 5): the record in force is the last one started on the clock; records
  // that start at the same tick are not told apart (ponytail: PES picks among them by the records' selectByRate, 23
  // percentages; the first of them in layer order stands in until 0x141F42110 is decoded).
  const Track* force = nullptr;
  for (const Track& t : tracks_)
    if (t.record.startTime <= clock && (!force || t.record.startTime > force->record.startTime)) force = &t;
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
  ClassDB::bind_method(D_METHOD("set_widescreen_fix", "on"), &PesCutsceneCamera::set_widescreen_fix);
  ClassDB::bind_method(D_METHOD("set_stage_default", "pos", "rot_y_deg", "flip"), &PesCutsceneCamera::set_stage_default);
}

}  // namespace godot
