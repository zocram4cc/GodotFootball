#include "gdext/pes_cutscene_camera.hpp"
#include "pes/fixdemo_table.hpp"

#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <algorithm>
#include <string>

namespace godot {

// The camera record's mode byte (+0xB0) of the procedural camera captured at a goal (docs/ports/pes-port-ProcCamera.md section 1).
constexpr uint8_t kProceduralMode = 4;

// A track for one camera record of `file`: the record's CANM clip (embedded, or a VFS file the resource names). A record with an
// empty anime path is procedural (PES slot +0x50 SetProcedural 0x141F7B630, update 0x141F7BFE0): a mode-4 one (the record's
// +0xB0) becomes a FixDemoCamera of the step's rig, whose engine frames pose it (advance); a procedural record of another
// mode keeps a track without a clip, and update() returns false while it is in force.
// ponytail: the translated engine is proven only on the captured calls (pes::FixDemoCameraRig::Step): a record or a target
// motion that reaches an uncaptured function kills that camera (advance reports the VA) and update() returns false for it, so the
// caller keeps its broadcast camera, until a capture proves the call. Modes other than 4 (the foul cut 0x20007, other cmnCam
// layers) are not given a rig at all.
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

Error PesCutsceneCamera::begin_scene(const Ref<PesLibrary>& library, int64_t seed) {
  if (!library.is_valid() || !library->exe()) return ERR_UNCONFIGURED;
  std::string err;
  const auto k = pes::LoadFixDemoRandomConstants(*library->exe(), &err);
  if (!k) {
    UtilityFunctions::push_error(String("PesCutsceneCamera: ") + err.c_str());
    return ERR_UNAVAILABLE;
  }
  // The context constructor passes its clock % 10000 to the global seeder (0x141F4C950 -> 0x141F51FD0).
  // ponytail: PES's global generator lives across scenes (each scene's A seed is its next draw); GF builds a fresh one per
  // scene from the match seed, so every scene of a match draws the same sequence until GF keeps the global between scenes.
  scene_ = std::make_shared<SceneState>(*k, uint32_t(seed) % 10000u);
  return OK;
}

void PesCutsceneCamera::continue_scene(const Ref<PesCutsceneCamera>& previous) {
  if (previous.is_valid()) scene_ = previous->scene_;
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
  layers_.clear();
  cutHolder_.cuts = file.cuts;
  k_ = *library->cutscene_camera_constants();
  // The debug track plays its one record: no PES layer list is built for a single file.
  layers_.push_back({0, {file.cameras[size_t(camera_index)]}, {0}});
  return AddTrack(library, file, file.cameras[size_t(camera_index)]);
}

Error PesCutsceneCamera::load_step(const Ref<PesLibrary>& library, int category, const PackedStringArray& names) {
  if (!library.is_valid() || !scene_) return ERR_UNCONFIGURED;
  const auto& vfs = library->vfs();
  tracks_.clear();
  layers_.clear();
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
    Layer built;
    built.first = tracks_.size();
    built.records = layer.cameras;
    for (const pes::fdc::CameraRecord& cam : layer.cameras) {
      const size_t before = tracks_.size();
      AddTrack(library, layer, cam);  // a bad clip is logged and skipped
      if (tracks_.size() == before) tracks_.push_back(Track{cam, {}, -1});  // keeps tracks_ aligned with the layer's records
    }
    // The layer player's creation: its set word (0x141F5D2E0), then at its cut start the list (0x141F2DD80).
    // ponytail: the word's mode byte (player+0xAE8 key 0, +0x48) is 0 in all 16 captured layers; its loader is not located, so
    // every layer takes mode 0 (a fresh pick); modes 1 and 2 are ported in FixDemoScene::LayerSetWord.
    const uint32_t word = scene_->scene.LayerSetWord(0, pes::CameraSetCount(built.records));
    built.list = pes::BuildCameraList(built.records, word, scene_->scene.listRandom());
    layers_.push_back(std::move(built));
  }
  return OK;
}

// Each layer plays its built list (PES's layer player 0x141F42110: the next record takes over when the player clock reaches its
// start). The step shows one of the layers' records: the one whose start is latest on the clock, the first layer on a tie.
// ponytail: which layer's camera PES shows when several run is not decoded (the layers' cameras write one params block, 0x1437F4A40,
// through FixDemoCamera::Update 0x141F7B770; the camera stack 0x141EB0570 only activates the scene's camera); this stands in.
const PesCutsceneCamera::Track* PesCutsceneCamera::InForce(float clock) const {
  const Track* force = nullptr;
  for (const Layer& layer : layers_) {
    if (layer.list.empty()) continue;
    const Track& t = tracks_[layer.first + layer.list[pes::CameraListIndex(layer.records, layer.list, clock)]];
    if (t.record.startTime <= clock && (!force || t.record.startTime > force->record.startTime)) force = &t;
  }
  return force;
}

// The player time the engine takes (1/60 s ticks): the record's own clock, started at its startCutTime unless the record
// plays on the player clock as is (timeMode 0), as the clip path reads it (docs/re/pes-re-camera-pose.md).
static float RecordTicks(const pes::fdc::CameraRecord& r, float clock) { return r.timeMode ? clock - r.startTime : clock; }

void PesCutsceneCamera::set_target(int slot, const Vector3& ground, const Vector3& point_a, const Vector3& point_b, double facing_deg) {
  if (!rig_ || slot < 0 || slot >= int(PesCameraRegistry::kTargets)) return;
  PesCameraRegistry::Target t;
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
  const int id = force->rigId;
  if (std::find(deadRigs_.begin(), deadRigs_.end(), id) != deadRigs_.end()) return;
  uint64_t missing = 0;
  pose_ = rig_->Step(size_t(id), RecordTicks(force->record, clock), &missing);
  if (!pose_) {
    deadRigs_.push_back(id);
    UtilityFunctions::push_error(String("PesCutsceneCamera: procedural camera ") + String::num_int64(id) +
                                 " stopped: the engine 0x141F7BFE0 called untranslated 0x" + String::num_int64(int64_t(missing), 16) +
                                 "; the broadcast camera stays");
  } else if (std::find(ranRigs_.begin(), ranRigs_.end(), id) == ranRigs_.end()) {
    ranRigs_.push_back(id);
    UtilityFunctions::print(String("PesCutsceneCamera: procedural camera ") + String::num_int64(id) + " engine ran: first pose at clock " +
                            String::num(RecordTicks(force->record, clock)) + " ticks");
  }
}

bool PesCutsceneCamera::update(Camera3D* camera, double t_s) {
  if (!camera || !k_) return false;
  const pes::fdc::PoseConstants& k = *k_;
  const float clock = float(t_s) * k.sixty;
  const Track* force = InForce(clock);
  if (force && force->rigId >= 0) {
    if (!pose_) return false;  // advance() has not run for this camera yet
    // The engine's params: eye, target and up (Fox space), the vertical fov in radians, near and far.
    const pes::GodotCameraPose g = pes::ToGodotCamera(*pose_, k.oneEighty / k.pi);
    camera->set_keep_aspect_mode(Camera3D::KEEP_HEIGHT);
    camera->set_fov(g.vfovDeg);
    camera->set_near(g.nearM);
    camera->set_far(g.farM);
    const auto v = [](const std::array<float, 3>& a) { return Vector3(a[0], a[1], a[2]); };
    camera->set_global_transform(Transform3D(Basis(v(g.x), v(g.y), v(g.z)), v(g.origin)));
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
  ClassDB::bind_method(D_METHOD("begin_scene", "library", "seed"), &PesCutsceneCamera::begin_scene);
  ClassDB::bind_method(D_METHOD("continue_scene", "previous"), &PesCutsceneCamera::continue_scene);
  ClassDB::bind_method(D_METHOD("load_step", "library", "category", "names"), &PesCutsceneCamera::load_step);
  ClassDB::bind_method(D_METHOD("load_file", "library", "vfs_path", "camera_index"), &PesCutsceneCamera::load_file);
  ClassDB::bind_method(D_METHOD("update", "camera", "t_s"), &PesCutsceneCamera::update);
  ClassDB::bind_method(D_METHOD("advance", "t_s"), &PesCutsceneCamera::advance);
  ClassDB::bind_method(D_METHOD("set_target", "slot", "ground", "point_a", "point_b", "facing_deg"), &PesCutsceneCamera::set_target);
  ClassDB::bind_method(D_METHOD("set_widescreen_fix", "on"), &PesCutsceneCamera::set_widescreen_fix);
  ClassDB::bind_method(D_METHOD("set_stage_default", "pos", "rot_y_deg", "flip"), &PesCutsceneCamera::set_stage_default);
}

}  // namespace godot
