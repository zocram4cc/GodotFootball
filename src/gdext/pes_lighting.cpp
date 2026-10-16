// The match light rig as Godot nodes (docs: docs/re/pes-re-lighting.md §0-8).
// Every number comes from the rig file: the sun direction from the §3.3 fit,
// the sky coefficients from TppAtmosphere, the floodlights (§4.2) and probes
// Exposure, bloom and the film curve are the post chain's (godot/post); the
// tonemapper stays LINEAR here.

#include "gdext/pes_lighting.hpp"
#include <cmath>

#include <godot_cpp/classes/compositor.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/reflection_probe.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "pes/lighting.hpp"
#include "pes/tonemap.hpp"

namespace godot {
namespace {

constexpr const char* SKY_SHADER = "res://shaders/pes_sky.gdshader";

// A fox-space (x,y,z,w) quat as a Godot quaternion; fox space is Godot space.
Quaternion ToGd(const pes::Quat& q) { return Quaternion(float(q.x), float(q.y), float(q.z), float(q.w)); }
Vector3 ToGd(const pes::Vec3& v) { return Vector3(v.x, v.y, v.z); }
// Fox→Godot basis: translation * rotation * scale (pes_model.cpp).
Transform3D Placement(const pes::Transform& t) {
  Basis b(ToGd(t.rotation));
  b.scale(Vector3(t.scale.x, t.scale.y, t.scale.z));
  return Transform3D(b, ToGd(t.translation));
}
// Spot axis: the rig's spots shine along local −Y (report §4.2); Godot's
// SpotLight3D shines along −Z, so a +90° X rotation maps −Y to −Z.
Basis SpotBasis(const Quaternion& q) { return Basis(Vector3(1, 0, 0), 1.57079632679f) * Basis(q); }

bool GroupOn(const std::vector<std::string>& chain, const std::vector<std::string>& on) {
  for (const auto& g : chain)
    for (const auto& e : on)
      if (g == e) return true;
  return chain.empty();  // ungrouped (root-level) lights stay as authored
}

void ClearChildren(Node3D* n) {
  for (int i = n->get_child_count() - 1; i >= 0; --i) {
    Node* c = n->get_child(i);
    n->remove_child(c);
    c->queue_free();
  }
}

// The post chain's constants (docs/re/pes-re-lighting.md Part C §1, OBS FUN_1416c7930):
// the adaptation step uses 0.98^(60·dt) with dt = 1/60 (the live tod+0x120).
constexpr double kAdaptDecayBase = 0.98;
constexpr double kAdaptRatePerSecond = 60.0;
constexpr double kAdaptDtSeconds = 1.0 / 60.0;

// The post chain's GPU parameters, in the layout godot/post/pes_post.glsl reads
// (32 floats): white balance columns, the film curve, the exposure law, the bloom.
PackedFloat32Array PostParams(const pes::LightSettings& s, const pes::PluginSettings& plugin) {
  PackedFloat32Array p;
  p.resize(32);
  const pes::Mat3 wb = pes::WhiteBalanceRgb(s.whiteTemperature, s.whiteTint);
  for (int col = 0; col < 3; ++col)
    for (int row = 0; row < 3; ++row) p[4 * col + row] = float(wb[row][col]);
  const pes::FilmCurve f = pes::MakeFilmCurve(s.acesFilmSlope, s.acesFilmToe, s.acesFilmShoulder,
                                              s.acesFilmBlackClip, s.acesFilmWhiteClip);
  const double film[8] = {f.toeMatch, f.straightMatch, f.shoulderMatch, f.slope,
                          f.black,    f.white,         f.toeScale,      f.shoulderScale};
  for (int i = 0; i < 8; ++i) p[12 + i] = float(film[i]);
  p[20] = float(s.gameKeyValue);
  p[21] = float(s.gameExposureCompensation);
  p[22] = float(s.gameMinExposure);
  p[23] = float(s.gameMaxExposure);
  p[24] = float(plugin.tonemapSpeed);
  // The adaptation step gain 1 − 0.98^(60·dt), dt = 1/60 per rendered frame.
  p[25] = float(1.0 - std::pow(kAdaptDecayBase, kAdaptRatePerSecond * kAdaptDtSeconds));
  p[26] = float(plugin.bloomBrightnessExtraction);
  p[27] = float(plugin.bloomWeight);
  p[28] = float(pes::BloomKernelCount(plugin.bloomSize));
  // ponytail: the 0.1–0.2 size copy branch (decoded in shape only) runs as one
  // zero-offset Kawase pass with its weight applied once; the offset scale 0 marks it.
  p[29] = pes::BloomCopyBranch(plugin.bloomSize) ? 0.0f : 1.0f;
  return p;
}

}  // namespace

void PesLighting::_bind_methods() {
  ClassDB::bind_method(D_METHOD("setup", "library", "stadium_id", "weather", "time_zone",
                                "season", "hour", "minute"),
                       &PesLighting::setup);
}

bool PesLighting::setup(const Ref<PesLibrary>& library, int stadium_id, int weather,
                        int time_zone, int season, int hour, int minute) {
  if (library.is_null()) {
    UtilityFunctions::push_error("PesLighting: no library");
    return false;
  }
  const bool rain = weather != 0;  // rain and snow both load the rain rig (§2.2)
  std::string err;
  auto rig = pes::LoadLightRig(library->vfs(), stadium_id, rain, &err);
  if (!rig) {
    UtilityFunctions::push_error("PesLighting: ", err.c_str());
    return false;
  }
  const pes::TimeSpanSettings& span =
      hour < 0 ? rig->SpanForDayNight(time_zone != 0, season) : rig->SpanForTime(hour, minute);
  const pes::LightSettings& s = span.settings;
  const int useHour = s.overrideDateTime ? s.dateTimeHour : span.startHour;
  const int useMinute = s.overrideDateTime ? s.dateTimeMinute : span.startMinute;

  // The sun: direction from the §3.3 fit, colour from the .atsh light colour
  // for this time (report Part B §9: sunLux × transmittance), off when the
  // span disables it. Its basis −Z is the light direction (report §3.3).
  const pes::Vec3 toSun = pes::SunToDirection(rig->atmosphere, useHour, useMinute);
  const pes::Vec3 lightDir = {-toSun.x, -toSun.y, -toSun.z};
  const double fade = pes::SunHorizonFade(lightDir);
  const bool sunOn = s.sunLightMode != pes::LightSettings::SUN_DISABLE && fade > 0;
  pes::Vec3 sunColour = {1, 1, 1};
  if (auto atsh = pes::LoadAtsh(library->vfs(), stadium_id, rain, &err)) {
    const pes::Vec3 c = pes::AtshSunColor(*atsh, rain, useHour, useMinute);
    const float norm = std::max({c.x, c.y, c.z, 1e-6f});
    sunColour = {c.x / norm, c.y / norm, c.z / norm};
  }
  ClearChildren(this);

  DirectionalLight3D* sun = memnew(DirectionalLight3D);
  sun->set_name("Sun");
  sun->set_transform(Transform3D(Basis().looking_at(Vector3(lightDir.x, lightDir.y, lightDir.z)),
                                 Vector3()));
  sun->set("light_intensity_lux", float(s.sunLux * fade));
  sun->set_color(Color(sunColour.x, sunColour.y, sunColour.z));
  sun->set("light_angular_distance", 2.0f);  // Fox's 1° disk radius (§3.3)
  sun->set_shadow(true);
  sun->set("directional_shadow_mode", DirectionalLight3D::SHADOW_PARALLEL_4_SPLITS);
  sun->set("directional_shadow_max_distance", float(rig->atmosphere.shadowRange));  // ≈ shadowRange (§7.3)
  sun->set_param(Light3D::PARAM_SHADOW_BLUR, float(rig->shadows.shadowBlurRadius));
  sun->set_param(Light3D::PARAM_SHADOW_OPACITY, float(rig->shadows.shadowOpacity));
  sun->set_visible(sunOn);
  add_child(sun);

  // The moon takes over when the sun is out (Part B §10): straight down,
  // moonLux × moonColor. Negligible next to the floodlights; kept for the sky.
  DirectionalLight3D* moon = memnew(DirectionalLight3D);
  moon->set_name("Moon");
  moon->set_rotation(Vector3(-1.57079632679f, 0, 0));  // straight down
  moon->set("light_intensity_lux", float(s.moonLux));
  moon->set_color(Color(rig->atmosphere.moonColor.x, rig->atmosphere.moonColor.y,
                        rig->atmosphere.moonColor.z));
  moon->set_visible(!sunOn);
  add_child(moon);

  // Floodlights (§4.2): physical lumens, temperature, ranges, cone and
  // falloff from the file; visibility from the span's proxy groups.
  for (const pes::RigLight& l : rig->lights) {
    if (!l.enable) continue;
    Light3D* node = nullptr;
    if (l.spot) {
      SpotLight3D* spot = memnew(SpotLight3D);
      spot->set("spot_range", float(l.outerRange));
      spot->set("spot_angle", float(l.umbraAngle) / 2.0f);
      spot->set("spot_attenuation", float(l.attenuationExponent));
      spot->set_transform(Transform3D(SpotBasis(ToGd(l.transform.rotation)),
                                      ToGd(l.transform.translation)));
      node = spot;
    } else {
      OmniLight3D* omni = memnew(OmniLight3D);
      omni->set("omni_range", float(l.outerRange));
      omni->set("omni_attenuation", float(l.attenuationExponent));
      node = omni;
    }
    node->set_name(String(l.name.c_str()));
    node->set("light_intensity_lumens", float(l.lumen * l.powerScale));
    node->set_color(Color(float(l.color.x), float(l.color.y), float(l.color.z)));
    node->set_temperature(float(l.temperature));
    node->set_param(Light3D::PARAM_SPECULAR, l.hasSpecular ? 1.0f : 0.0f);
    node->set_param(Light3D::PARAM_SIZE, float(l.lightSize));
    node->set_shadow(l.castShadow);
    node->set_visible(GroupOn(l.groups, span.groups));
    if (!l.spot) node->set_transform(Transform3D(Basis(), ToGd(l.transform.translation)));
    add_child(node);
  }

  // Reflection probes (Part B §6): box from the file, capture offset from the
  // capture position, symmetric blend from the tightest inner scale, ONCE —
  // the baked .rpd cannot load into a Godot probe (report §6).
  for (const pes::RigProbe& p : rig->probes) {
    if (!p.enable) continue;
    ReflectionProbe* probe = memnew(ReflectionProbe);
    probe->set_name(String(p.name.c_str()));
    probe->set_position(ToGd(p.origin));
    probe->set_size(ToGd(p.box));
    probe->set_origin_offset(Vector3(p.capture.x - p.origin.x, p.capture.y - p.origin.y,
                                     p.capture.z - p.origin.z));
    float inner = 1.0f;
    for (int a = 0; a < 3; ++a)
      for (int e = 0; e < 2; ++e) inner = std::min(inner, float(p.innerScale[a][e]));
    const float tight = std::min({p.box.x, p.box.y, p.box.z});
    probe->set_blend_distance((1.0f - inner) * tight / 2.0f);
    probe->set_intensity(float(p.intensity));
    probe->set_enable_box_projection(true);
    probe->set_as_interior(false);
    probe->set_update_mode(ReflectionProbe::UPDATE_ONCE);
    probe->set_visible(GroupOn(p.groups, span.groups));
    add_child(probe);
  }

  // Sky + environment on the sibling WorldEnvironment named by the scene; the
  // shader carries the decoded TppAtmosphere coefficients and the sun state.
  Node* parent = get_parent();
  WorldEnvironment* wenv = parent ? Object::cast_to<WorldEnvironment>(parent->get_node_or_null("WorldEnvironment")) : nullptr;
  if (wenv) {
    Ref<Environment> env = wenv->get_environment();
    if (env.is_valid()) {
      Ref<Shader> skyShader = ResourceLoader::get_singleton()->load(SKY_SHADER);
      Ref<ShaderMaterial> skyMat;
      skyMat.instantiate();
      skyMat->set_shader(skyShader);
      const pes::Atmosphere& a = rig->atmosphere;
      skyMat->set_shader_parameter("beta_rayleigh", Vector3(a.rayleighScattering.x, a.rayleighScattering.y, a.rayleighScattering.z));
      skyMat->set_shader_parameter("height_rayleigh", float(a.rayleighHeightScale));
      skyMat->set_shader_parameter("beta_mie", Vector3(a.mieScattering.x, a.mieScattering.y, a.mieScattering.z));
      skyMat->set_shader_parameter("height_mie", float(a.mieHeightScale / 1000.0));
      skyMat->set_shader_parameter("mie_g", float(a.mieAnisotropy));
      skyMat->set_shader_parameter("mie_absorption", float(a.mieAbsorptionRate));
      skyMat->set_shader_parameter("sun_direction", Vector3(toSun.x, toSun.y, toSun.z));
      skyMat->set_shader_parameter("sun_intensity",
                                    float(s.sunLux * fade * M_PI * s.skyLightSunScale));
      skyMat->set_shader_parameter("star_intensity",
                                    float(s.moonLux * M_PI * s.skyLightSunScale));
      skyMat->set_shader_parameter("ground_color",
                                    Vector3(a.groundColor.x, a.groundColor.y, a.groundColor.z));
      Ref<Sky> sky;
      sky.instantiate();
      sky->set_material(skyMat);
      env->set_sky(sky);
      env->set_background(Environment::BG_SKY);
      env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
      env->set_ambient_light_energy(float(a.daySkyAmbientScale));
      // Global height fog (§7.5): density/1000, height scale/1000.
      env->set_fog_enabled(s.globalFogEnabled);
      env->set_fog_mode(Environment::FOG_MODE_EXPONENTIAL);
      env->set_fog_light_color(
          Color(float(s.globalFogColor.x), float(s.globalFogColor.y), float(s.globalFogColor.z)));
      env->set_fog_density(float(s.globalFogDensity / 1000.0));
      env->set_fog_height_density(float(s.globalFogHeightScale / 1000.0));
      env->set_fog_aerial_perspective(1.0f);
      env->set_fog_sky_affect(1.0f);
      // Tonemap stays LINEAR: the post chain (PesPostEffect) does the film, so
      // Godot's own tonemapper is an identity here.
      env->set_tonemapper(Environment::TONE_MAPPER_LINEAR);
      env->set_glow_enabled(false);
    }
  }
  // Exposure, bloom and the film curve belong to the post chain: the PesPostEffect
  // on the camera's compositor takes the rig's values (godot/post/pes_post_effect.gd).
  Camera3D* camera = parent ? Object::cast_to<Camera3D>(parent->get_node_or_null("Camera3D")) : nullptr;
  if (camera && camera->get_compositor().is_valid()) {
    Dictionary post;
    post["params"] = PostParams(s, rig->plugin);
    post["initial_ev"] = s.gameInitialExposureValue;
    Array effects = camera->get_compositor()->get("compositor_effects");
    for (int i = 0; i < effects.size(); ++i) {
      Object* effect = effects[i];
      if (effect && effect->has_method("configure")) effect->call("configure", post);
    }
  }
  return true;
}

}  // namespace godot
