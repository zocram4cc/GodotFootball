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
  const pes::fdc::CameraRecord& cam = file.cameras[size_t(camera_index)];
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
  canm_ = std::move(canm);
  record_ = cam;
  k_ = *library->cutscene_camera_constants();
  return OK;
}

Error PesCutsceneCamera::load_scene(const Ref<PesLibrary>& library, int scene_id, int camera_index) {
  if (!library.is_valid()) return ERR_UNCONFIGURED;
  const auto& vfs = library->vfs();
  std::string err;
  const int category = scene_id >> 16;
  auto tableBytes = vfs.Read(pes::FixDemoTablePath(category), &err);
  if (!tableBytes) {
    UtilityFunctions::push_error(String("PesCutsceneCamera: cannot read the scene table: ") + err.c_str());
    return ERR_FILE_NOT_FOUND;
  }
  auto table = pes::DecodeFixDemoTable(*tableBytes, category, &err);
  if (!table) {
    UtilityFunctions::push_error(String("PesCutsceneCamera: cannot decode the scene table: ") + err.c_str());
    return ERR_PARSE_ERROR;
  }
  const std::vector<std::string> paths = pes::FixDemoSceneFdcPaths(*table, uint32_t(scene_id), &err);
  for (const std::string& path : paths) {
    if (!vfs.Exists(path)) continue;
    if (load_file(library, String(path.c_str()), camera_index) == OK) {
      UtilityFunctions::print(String("PesCutsceneCamera: scene camera from ") + path.c_str());
      return OK;
    }
  }
  UtilityFunctions::push_error(String("PesCutsceneCamera: no cut layer of the scene has the requested camera: ") + err.c_str());
  return ERR_FILE_NOT_FOUND;
}

double PesCutsceneCamera::duration_s() const {
  if (!canm_ || !k_) return 0.0;
  return double(canm_->length) / double(pes::fdc::ClipFps(*k_, *canm_));
}

void PesCutsceneCamera::update(Camera3D* camera, double t_s) {
  if (!canm_ || !k_ || !camera) return;
  // ponytail: a record's height correction (target height from 0x141F7E3C0) is 0 here: no FixDemo player
  // supplies a target. Replace it with that player's value when it is ported (the entrance records have rate 0).
  const pes::fdc::PoseConstants& k = *k_;
  const float clock = float(t_s) * k.sixty;
  const pes::fdc::CameraPose pose = pes::fdc::EvaluateCameraPose(k, *canm_, record_, clock, cutOffset_, 0.0f);
  const Viewport* vp = camera->get_viewport();
  const Vector2 size = vp ? vp->get_visible_rect().size : Vector2(1, 1);
  const pes::fdc::RenderCamera r = pes::fdc::RenderCameraParams(k, pose, record_, float(size.x / size.y), widescreenFix_);
  camera->set_keep_aspect_mode(Camera3D::KEEP_HEIGHT);  // the clip's fov is vertical
  camera->set_fov(r.fovRad * k.oneEighty / k.pi);
  camera->set_near(r.nearM);
  camera->set_far(r.farM);
  // The pose's columns are (-right, up, forward); Godot's basis is (right, up, -forward).
  const Basis basis(Vector3(-pose.m[0][0], -pose.m[1][0], -pose.m[2][0]), Vector3(pose.m[0][1], pose.m[1][1], pose.m[2][1]),
                    Vector3(-pose.m[0][2], -pose.m[1][2], -pose.m[2][2]));
  camera->set_global_transform(Transform3D(basis, Vector3(pose.eye[0], pose.eye[1], pose.eye[2])));
}

void PesCutsceneCamera::_bind_methods() {
  ClassDB::bind_method(D_METHOD("load_scene", "library", "scene_id", "camera_index"), &PesCutsceneCamera::load_scene);
  ClassDB::bind_method(D_METHOD("load_file", "library", "vfs_path", "camera_index"), &PesCutsceneCamera::load_file);
  ClassDB::bind_method(D_METHOD("duration_s"), &PesCutsceneCamera::duration_s);
  ClassDB::bind_method(D_METHOD("update", "camera", "t_s"), &PesCutsceneCamera::update);
  ClassDB::bind_method(D_METHOD("set_widescreen_fix", "on"), &PesCutsceneCamera::set_widescreen_fix);
  ClassDB::bind_method(D_METHOD("set_cut_offset", "pos", "rot_y_deg", "flip"), &PesCutsceneCamera::set_cut_offset);
}

}  // namespace godot
