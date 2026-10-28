#include "gdext/pes_cutscene_camera.hpp"
#include "pes/fixdemo_table.hpp"

#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <algorithm>
#include <string>

namespace godot {
namespace {

// The record's startCutTime counts 1/60 s ticks (docs/re/pes-re-fdc-format.md §3; the clip is 30 fps,
// so one frame is two ticks, as the corpus statistic in pes-re-cutscenes.md §3.2 gives).
constexpr double kTicksPerSecond = 60.0;
// PES21_CAMERA_TRACE.md §6.4: GF derives shadow depth from the near plane, so sub-10 cm near planes are clamped.
constexpr float kMinNearM = 0.1f;

}  // namespace

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
  startTicks_ = cam.startTime;
  nearOverride_ = cam.nearClip;
  farOverride_ = cam.farClip;
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
  if (!canm_ || canm_->frameRate == 0) return 0.0;
  return double(canm_->frameCount) / double(canm_->frameRate);
}

void PesCutsceneCamera::update(Camera3D* camera, double t_s) {
  if (!canm_ || !camera) return;
  const double ticksPerFrame = kTicksPerSecond / double(canm_->frameRate);
  const double frame = std::clamp((t_s * kTicksPerSecond - startTicks_) / ticksPerFrame, 0.0, double(canm_->frameCount));
  const pes::fdc::CameraSample s = pes::fdc::SampleCanm(*canm_, float(frame));
  const float near = nearOverride_ > 0.0f ? nearOverride_ : s.nearM;
  const float far = farOverride_ > 0.0f ? farOverride_ : s.farM;
  camera->set_keep_aspect_mode(Camera3D::KEEP_HEIGHT);  // the clip's fov is vertical
  camera->set_fov(s.fovDeg);
  camera->set_near(std::max(near, kMinNearM));
  camera->set_far(far);
  const Quaternion q(s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3]);
  camera->set_global_transform(Transform3D(Basis(q), Vector3(s.position[0], s.position[1], s.position[2])));
}

void PesCutsceneCamera::_bind_methods() {
  ClassDB::bind_method(D_METHOD("load_scene", "library", "scene_id", "camera_index"), &PesCutsceneCamera::load_scene);
  ClassDB::bind_method(D_METHOD("duration_s"), &PesCutsceneCamera::duration_s);
  ClassDB::bind_method(D_METHOD("update", "camera", "t_s"), &PesCutsceneCamera::update);
}

}  // namespace godot
