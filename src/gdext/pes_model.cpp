#include "gdext/pes_model.hpp"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdio>
#include <string_view>
#include <unordered_map>

#include "gdext/pes_mesh.hpp"
#include "gdext/pes_texture.hpp"
#include "pes/fmdl.hpp"
#include "pes/fpk.hpp"
#include "pes/pack_rules.hpp"
#include "pes/stadium.hpp"
#include "pes/turffin.hpp"

namespace godot {
namespace {

constexpr std::string_view FMDL_EXT = ".fmdl";
constexpr const char* BLIN_SHADER = "res://shaders/pes_blin.gdshader";
constexpr const char* CONSTANT_SHADER = "res://shaders/pes_constant.gdshader";
constexpr const char* CONSTANT_BLEND_SHADER = "res://shaders/pes_constant_blend.gdshader";
constexpr const char* PITCH_SHADER = "res://shaders/pes_pitch.gdshader";
constexpr const char* GGX_DECAL_SHADER = "res://shaders/pes_ggx_decal.gdshader";
constexpr const char* BLIN_DECAL_SHADER = "res://shaders/pes_blin_decal.gdshader";
constexpr const char* TURF_SHADER = "res://shaders/pes_turf.gdshader";
constexpr const char* BASE_ROLE = "Base_Tex_SRGB";
constexpr int NO_FACE = 0;  // static models have no face folder to fall back on

// Fox technique -> shader. ConstantSRGB is unlit; "_Solid" is the opaque
// variant. fox3DDC_GGX and fox3DDC_Blin are Deferred_Decal techniques (the
// pitch lines and scratch); the other fox3DD* techniques still draw as Blin
// until their disassembly is mapped (ponytail).
const char* ShaderFor(const std::string& technique) {
  if (technique == "pes3DDF_Pitch2020") return PITCH_SHADER;
  if (technique == "fox3DFW_ConstantSRGB_NDR_Solid") return CONSTANT_SHADER;
  if (technique == "fox3DDC_GGX") return GGX_DECAL_SHADER;
  if (technique == "fox3DDC_Blin") return BLIN_DECAL_SHADER;
  if (technique == "fox3DFW_ConstantSRGB_NDR") return CONSTANT_BLEND_SHADER;
  if (technique.rfind("fox3DD", 0) == 0) return BLIN_SHADER;
  return nullptr;
}

bool EndsWith(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

class StaticBuild {
 public:
  explicit StaticBuild(const pes::PesVfs& vfs) : vfs_(vfs), resolver_(vfs, NO_FACE) {}

  Ref<ShaderMaterial> Material(const pes::Fmdl& m, const pes::FmdlMaterial& fm) {
    const char* shaderPath = ShaderFor(fm.technique);
    if (!shaderPath) {
      UtilityFunctions::push_warning("PES technique without a shader, drawn as Blin: ", ToGd(fm.technique));
      shaderPath = BLIN_SHADER;
    }
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    mat->set_name(ToGd(fm.name));
    mat->set_shader(ResourceLoader::get_singleton()->load(shaderPath));  // cached by the loader
    for (const auto& [name, v] : fm.params) mat->set_shader_parameter(ToGd(name), Vector4(v.x, v.y, v.z, v.w));
    for (const auto& [role, ti] : fm.textures) {
      if (ti < 0 || ti >= int(m.textures.size())) continue;
      const auto* t = Texture(m.textures[ti]);
      if (!t) continue;
      mat->set_shader_parameter(ToGd(role), t->tex);
      mat->set_shader_parameter(ToGd("has_" + role), true);
      if (role == BASE_ROLE) mat->set_shader_parameter("alpha_tested", t->hasAlpha);
    }
    return mat;
  }

 private:
  struct Loaded {
    Ref<godot::Texture> tex;
    bool hasAlpha;
  };
  const Loaded* Texture(const pes::FmdlTexture& t) {
    const std::string path = resolver_.Resolve(t);
    if (path.empty()) {
      UtilityFunctions::push_warning("PES texture not found: ", ToGd(t.directory + t.filename));
      return nullptr;
    }
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second ? &*it->second : nullptr;
    auto& slot = cache_[path];
    std::string err;
    Ref<godot::Texture> tex = LoadPesTexture(vfs_, path, &err);
    if (tex.is_null()) {
      UtilityFunctions::push_warning("PES texture unreadable: ", ToGd(path), " (", ToGd(err), ")");
      return nullptr;
    }
    bool hasAlpha = false;
    if (Ref<Texture2D> t2 = tex; t2.is_valid()) {
      Ref<Image> img = t2->get_image();
      if (img.is_valid()) {
        if (img->is_compressed()) img->decompress();
        hasAlpha = img->detect_alpha() != Image::ALPHA_NONE;
      }
    }
    slot = Loaded{tex, hasAlpha};
    return &*slot;
  }

  const pes::PesVfs& vfs_;
  pes::TextureResolver resolver_;
  std::unordered_map<std::string, std::optional<Loaded>> cache_;  // by VFS path
};

// Every fmdl of a pack (or only `want` when set) as one Node3D per fmdl, named
// after the fmdl and holding its meshes as MeshInstance3Ds, so a caller can
// place a stadium member by name and reuse the rest.
Node3D* MeshMembers(StaticBuild& build, const pes::FpkArchive& fpk, std::span<const uint8_t> pack,
                    const std::string& name, const std::string& want = std::string()) {
  Node3D* root = memnew(Node3D);
  root->set_name(ToGd(name));
  std::string err;
  bool built = false;
  for (const auto& e : fpk.entries) {
    if (!EndsWith(e.path, FMDL_EXT) || (!want.empty() && !EndsWith(e.path, want))) continue;
    auto m = pes::ParseFmdl(pack.subspan(e.offset, e.size), &err);
    if (!m) {
      UtilityFunctions::push_warning("PES model unreadable: ", ToGd(e.path), " (", ToGd(err), ")");
      continue;
    }
    std::vector<Ref<ShaderMaterial>> materials;
    for (const auto& fm : m->materials) materials.push_back(build.Material(*m, fm));
    Node3D* group = memnew(Node3D);
    group->set_name(ToGd(e.path.substr(e.path.find_last_of('/') + 1)));
    group->set_meta("pes_member", ToGd(e.path));
    for (const auto& mesh : m->meshes) {
      if (mesh.indices.empty()) continue;
      Ref<ArrayMesh> am;
      am.instantiate();
      am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, FmdlSurfaceArrays(mesh));
      if (mesh.materialIndex >= 0 && mesh.materialIndex < int(materials.size()))
        am->surface_set_material(0, materials[mesh.materialIndex]);
      MeshInstance3D* mi = memnew(MeshInstance3D);
      mi->set_mesh(am);
      group->add_child(mi);
      built = true;
    }
    root->add_child(group);
  }
  if (!built) {
    memdelete(root);
    return nullptr;
  }
  return root;
}

}  // namespace

void PesModelBuilder::_bind_methods() {
  ClassDB::bind_method(D_METHOD("build_model", "library", "fpk_path", "member"), &PesModelBuilder::build_model);
  ClassDB::bind_method(D_METHOD("build_stadium", "library", "stadium_id", "weather"), &PesModelBuilder::build_stadium);
  ClassDB::bind_method(D_METHOD("build_turf_fins", "library", "stadium_id"), &PesModelBuilder::build_turf_fins);
}

Node3D* PesModelBuilder::build_model(const Ref<PesLibrary>& library, const String& fpk_path, const String& member) const {
  if (library.is_null()) return nullptr;
  const auto& vfs = library->vfs();
  std::string err;
  const std::string packPath = FromGd(fpk_path), want = FromGd(member);
  auto pack = vfs.Read(packPath, &err);
  if (!pack) return nullptr;
  auto fpk = pes::FpkArchive::Parse(*pack, &err);
  if (!fpk) {
    UtilityFunctions::push_error("PES pack unreadable: ", fpk_path, " (", ToGd(err), ")");
    return nullptr;
  }
  StaticBuild build(vfs);
  return MeshMembers(build, *fpk, *pack, packPath.substr(packPath.find_last_of('/') + 1), want);
}

Node3D* PesModelBuilder::build_stadium(const Ref<PesLibrary>& library, int stadium_id, int weather) const {
  if (library.is_null()) return nullptr;
  std::string err;
  const auto models = pes::LoadStadiumScene(library->vfs(), stadium_id, weather, &err);
  if (!models) {
    UtilityFunctions::push_error("PES stadium not readable: ", ToGd(err));
    return nullptr;
  }
  Node3D* root = memnew(Node3D);
  root->set_name("Stadium" + String::num_int64(stadium_id, 3));
  // Each .fpk pack is read and meshed once, then every placement of one of its
  // members re-uses that mesh tree: the st002 scene pack is 130 MB on disk and
  // two of its members are placed, and every material in it loads its textures
  // through StaticBuild's cache.
  std::unordered_map<std::string, Node3D*> meshed;  // pack member path -> its meshes
  for (const auto& m : *models) {
    // A MobCharacterModel carries sklFile (docs/re/pes-re-fox2-format.md 191):
    // the cornerflag rig is a skinned model PES positions at runtime, so its
    // fox2 identity transform is not where PES draws it. Placing it at the
    // identity puts a flag pole on the centre spot, which PES never draws, so
    // the class is skipped. Where PES draws it (the staff package) is an open
    // RE item: docs/re/pes-re-fox2-stadium.md 4.2, pes-re-stadium-camera.md.
    if (m.className == "MobCharacterModel") continue;
    auto it = meshed.find(m.member);
    if (it == meshed.end()) {
      auto pack = library->vfs().Read(m.fpk, &err);
      Node3D* group = nullptr;
      if (pack) {
        auto fpk = pes::FpkArchive::Parse(*pack, &err);
        if (!fpk) {
          UtilityFunctions::push_error("PES pack unreadable: ", ToGd(m.fpk), " (", ToGd(err), ")");
        } else {
          // One child per fmdl of the pack, in pack order, each named after it
          // so every placement of one member re-uses the same subtree.
          StaticBuild build(library->vfs());
          group = MeshMembers(build, *fpk, *pack, m.fpk.substr(m.fpk.find_last_of('/') + 1));
          if (group != nullptr)
            for (int i = 0; i < group->get_child_count(); ++i) {
              Node3D* c = Object::cast_to<Node3D>(group->get_child(i));
              if (c->has_meta("pes_member")) meshed[FromGd(c->get_meta("pes_member"))] = c;
            }
        }
      }
      it = meshed.emplace(m.member, group).first;
    }
    Node3D* placed = memnew(Node3D);
    if (it->second != nullptr) placed->add_child(it->second->duplicate());
    // The fox2 TransformEntity is the model's whole placement in Fox space,
    // which is Godot's space: scale, then the (x,y,z,w) rotation, then the
    // translation, composed as translation * rotation * scale.
    const pes::Transform& t = m.transform;
    const Quaternion q(t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w);
    Basis b(Basis(q).scaled(Vector3(t.scale.x, t.scale.y, t.scale.z)));
    placed->set_transform(Transform3D(b, Vector3(t.translation.x, t.translation.y, t.translation.z)));
    placed->set_meta("pes_class", ToGd(m.className));
    placed->set_meta("pes_member", ToGd(m.member));
    placed->set_meta("pes_rotation_y", q.get_euler(EULER_ORDER_YXZ).y);
    root->add_child(placed);
  }
  return root;
}

Node3D* PesModelBuilder::build_turf_fins(const Ref<PesLibrary>& library, int stadium_id) const {
  if (library.is_null()) return nullptr;
  const pes::PesVfs& vfs = library->vfs();
  std::string err;
  // The grid geometry from the install's turf init path (turffin.cpp).
  auto exe = pes::PeImage::Open(vfs.Root() + "/PES2021.exe", &err);
  if (!exe) {
    UtilityFunctions::push_error("PES turf grid unreadable: ", ToGd(err));
    return nullptr;
  }
  auto grid = pes::TurfGridGeometry(*exe, &err);
  if (!grid) {
    UtilityFunctions::push_error("PES turf grid unreadable: ", ToGd(err));
    return nullptr;
  }
  auto affine = pes::TurfMaskAffine(*exe, &err);
  if (!affine) {
    UtilityFunctions::push_error("PES turf mask affine unreadable: ", ToGd(err));
    return nullptr;
  }
  // The fin tile: mesh0 of the install's turf3dtexlodu4.fmdl.
  auto blob = vfs.Read(pes::kTurfFinMeshPath, &err);
  if (!blob) {
    UtilityFunctions::push_error("PES fin mesh not found: ", ToGd(err));
    return nullptr;
  }
  auto fmdl = pes::ParseFmdl(*blob, &err);
  if (!fmdl || fmdl->meshes.empty() || fmdl->meshes[0].indices.empty()) {
    UtilityFunctions::push_error("PES fin mesh unreadable: ", ToGd(err));
    return nullptr;
  }
  // PES binds the technique's textures by Fox StrCode32 role hash
  // (TurfPlacerRE 1.1); the fmdl's proxy roles do not apply. Winner per
  // slot follows TurfMask 6.3: stadium first, common fallback.
  char folder[16];
  snprintf(folder, sizeof folder, "st%03d", stadium_id);
  const std::string st = "Asset/model/bg/" + std::string(folder) + "/sourceimages/tga/#windx11/";
  const std::string common = "Asset/model/bg/common/pitch/sourceimages/#windx11/";
  const std::string commonTurf = "Asset/model/bg/common/turf3d/sourceimages/#windx11/";
  auto pick = [&](const std::string& a, const std::string& b) {
    if (!a.empty() && vfs.Exists(a)) return a;
    return b;
  };
  const std::pair<const char*, std::string> binds[] = {
      {"Base_Tex_SRGB", pick(st + "pitch_alp.ftex", common + "pitch_alp.ftex")},
      {"Grass_MaskMap_Tex_LIN", std::string()},  // ponytail: the runtime ground mask (TurfMask 1.4 OPEN)
      {"NormalMap_Tex_NRM", pick(st + "turf_grain_alp.ftex", common + "pitch_grain_alp.ftex")},
      {"SpecularMap_Tex_LIN", pick(st + "turf_srm_alp.ftex", common + "grass_slopetable_alp.ftex")},
      {"Grass_DetailDiffuseMap_Tex_SRGB",
       pick(st + "turf_bsm_alp_rgba32.ftex", common + "turf_bsm_alp_rgba32.ftex")},
      {"Grass_PlanarShadow_Tex_LIN", std::string()},  // ponytail: the runtime shadow target (TurfPlacer 1.4 OPEN)
      {"Grass_DetailNormalMap_Tex_NRM", pick(st + "turf_nrm.ftex", common + "turf_nrm.ftex")},
      {"Grass_DiffuseMap_Tex_SRGB", pick(st + "grassfin_bsm_alp.ftex", commonTurf + "grassfin_bsm_alp.ftex")},
  };
  Ref<ShaderMaterial> mat;
  mat.instantiate();
  mat->set_shader(ResourceLoader::get_singleton()->load(TURF_SHADER));
  // The two runtime globals (mask t1, planar shadow t5) are GPU-side and
  // OPEN; Godot's default_black is (0,0,0,1) but the VS reads .w, so the
  // honest no-input stub is an explicit zero texel (mask alpha 0 opens the
  // height gate, VS asm 66-70; shadow sum 0 is unshadowed, VS asm 99).
  Ref<Image> zero = Image::create(1, 1, false, Image::FORMAT_RGBA8);
  zero->fill(Color(0, 0, 0, 0));
  Ref<ImageTexture> zeroTex = ImageTexture::create_from_image(zero);
  for (const auto& [role, path] : binds) {
    if (path.empty()) {
      mat->set_shader_parameter(String::utf8(role), zeroTex);
      continue;
    }
    if (!vfs.Exists(path)) continue;
    std::string terr;
    Ref<Texture> tex = LoadPesTexture(vfs, path, &terr);
    if (tex.is_null()) {
      UtilityFunctions::push_warning("PES turf texture unreadable: ", ToGd(path), " (", ToGd(terr), ")");
      continue;
    }
    mat->set_shader_parameter(String::utf8(role), tex);
  }
  mat->set_shader_parameter("PitchOffset", Vector4(affine->x, affine->y, affine->z, affine->w));
  Ref<ArrayMesh> am;
  am.instantiate();
  am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, FmdlSurfaceArrays(fmdl->meshes[0]));
  am->surface_set_material(0, mat);
  Ref<MultiMesh> mm;
  mm.instantiate();
  mm->set_transform_format(MultiMesh::TRANSFORM_3D);
  mm->set_mesh(am);
  // One instance per kept placer slot (TurfPlacerRE 3.1): (idx-u)*0.5 over
  // the ground rect. Variant yaw: PES's per-patch pick (3.4) is live state;
  // GF replays the pick cadence as a yaw so neighbouring tiles vary.
  // ponytail: variant content is OPEN; yaw stands in for the entry bytes.
  std::vector<Transform3D> kept;
  kept.reserve(size_t(grid->countX) * size_t(grid->countZ));
  const int ux = (grid->countX - 1) / 2, uz = (grid->countZ - 1) / 2;
  for (int iz = 0; iz < grid->countZ; ++iz)
    for (int ix = 0; ix < grid->countX; ++ix) {
      const size_t i = size_t(iz) * size_t(grid->countX) + ix;
      if (!grid->kept[i]) continue;
      const float yaw = float(grid->variant[i] % 4) * 1.5707963f;
      kept.emplace_back(Basis(Vector3(0, 1, 0), yaw),
                        Vector3(float(ix - ux) * grid->stepM, 0.0f, float(iz - uz) * grid->stepM));
    }
  mm->set_instance_count(int(kept.size()));
  // One set_buffer upload: per-instance set_instance_transform does not stick
  // from the extension (readback is identity). TRANSFORM_3D packs 12 floats
  // per instance (basis rows x/y/z, then the origin).
  PackedFloat32Array buf;
  buf.resize(int(kept.size()) * 12);
  for (int i = 0; i < int(kept.size()); ++i) {
    const Transform3D& t = kept[size_t(i)];
    float* b = buf.ptrw() + i * 12;
    b[0] = t.basis[0][0];
    b[1] = t.basis[0][1];
    b[2] = t.basis[0][2];
    b[3] = t.origin.x;
    b[4] = t.basis[1][0];
    b[5] = t.basis[1][1];
    b[6] = t.basis[1][2];
    b[7] = t.origin.y;
    b[8] = t.basis[2][0];
    b[9] = t.basis[2][1];
    b[10] = t.basis[2][2];
    b[11] = t.origin.z;
  }
  mm->set_buffer(buf);
  MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
  mmi->set_multimesh(mm);
  // The tile AABB covers one slot; the instances span the ground rect, so
  // the cull bounds are the rect (half-extents + tile margin, y to 1 m).
  mmi->set_custom_aabb(AABB(Vector3(-grid->widthM / 2 - 1.0f, -1.0f, -grid->heightM / 2 - 1.0f),
                             Vector3(grid->widthM + 2.0f, 2.0f, grid->heightM + 2.0f)));
  Node3D* root = memnew(Node3D);
  root->set_name("TurfFins");
  root->add_child(mmi);
  return root;
}

}  // namespace godot
