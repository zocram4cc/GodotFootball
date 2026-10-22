// The match light rig: TimeOfDay spans + settings, the sun model, the cloud
// dome, floodlights, reflection probes, shadow settings (docs:
// docs/re/pes-re-lighting.md §0-8). EntityLink targets (fox2 type 22) resolve
// through ByAddress; dangling authoring refs stay empty.

#include "pes/lighting.hpp"

#include "pes/fpk.hpp"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace pes {
namespace {

constexpr double kPi = 3.14159265358979323846;

const Fox2Property* Prop(const Fox2Entity& e, std::string_view name) {
  return e.Find(name);
}
// A numeric property in any of the file's numeric slots (int64/uint64/double).
double Num(const Fox2Entity& e, std::string_view name, double dflt) {
  const Fox2Property* p = Prop(e, name);
  if (!p || p->values.empty()) return dflt;
  const Fox2Value& v = p->values[0];
  if (p->type == 8 || p->type == 9 || p->type == 6 || p->type == 7) return v.d;  // float slots
  return p->type == 5 || p->type == 1 || p->type == 3 ? double(v.u) : double(v.i);  // int slots
}
bool Flag(const Fox2Entity& e, std::string_view name) {
  const Fox2Property* p = Prop(e, name);
  return p && !p->values.empty() && (p->values[0].b || p->values[0].i != 0);
}
int Int(const Fox2Entity& e, std::string_view name, int dflt) {
  return int(Num(e, name, dflt));
}
Vec3 V3(const Fox2Entity& e, std::string_view name, Vec3 dflt) {
  const Fox2Property* p = Prop(e, name);
  if (!p || p->values.empty()) return dflt;
  const Fox2Value& v = p->values[0];
  if (p->type == 14) return v.v3;
  if (p->type == 15 || p->type == 19) return {v.v4.x, v.v4.y, v.v4.z};
  return dflt;
}
Vec4 V4(const Fox2Entity& e, std::string_view name, Vec4 dflt) {
  const Fox2Property* p = Prop(e, name);
  if (!p || p->values.empty()) return dflt;
  const Fox2Value& v = p->values[0];
  if (p->type == 15 || p->type == 19 || p->type == 14) return v.v4;
  return dflt;
}
std::string Str(const Fox2Entity& e, std::string_view name) {
  const Fox2Property* p = Prop(e, name);
  return (p && !p->values.empty()) ? p->values[0].s : std::string();
}
// A link property's target address (EntityPtr/Handle u64, or EntityLink's
// 4th slot): 0 when absent or null.
uint64_t Link(const Fox2File& file, const Fox2Entity& e, std::string_view name, size_t i = 0) {
  (void)file;
  const Fox2Property* p = Prop(e, name);
  if (!p || i >= p->values.size()) return 0;
  return p->values[i].u;
}
const Fox2Entity* Target(const Fox2File& file, const Fox2Entity& e, std::string_view name,
                         size_t i = 0) {
  return file.ByAddress(Link(file, e, name, i));
}

LightSettings ReadSettings(const Fox2Entity& e) {
  LightSettings s;
  s.overrideDateTime = Flag(e, "overrideDateTime");
  s.dateTimeHour = Int(e, "dateTimeHour", 0);
  s.dateTimeMinute = Int(e, "dateTimeMinute", 0);
  s.overrideSunLightMode = Flag(e, "overrideSunLightMode");
  s.sunLightMode = Int(e, "sunLightMode", 0);
  s.overrideAtmosphere = Flag(e, "overrideAtmosphere");
  s.sunLux = Num(e, "sunLux", 150000);
  s.moonLux = Num(e, "moonLux", 1);
  s.skyLightSunScale = Num(e, "skyLightSunScale", 1);
  s.enlightenOutputScale = Num(e, "enlightenOutputScale", 1);
  s.overrideGameExposure = Flag(e, "overrideGameExposure");
  s.gameExposureCompensation = Num(e, "gameExposureCompensation", 0);
  s.gameMinExposure = Num(e, "gameMinExposure", -14);
  s.gameMaxExposure = Num(e, "gameMaxExposure", -7);
  s.gameKeyValue = Num(e, "gameKeyValue", 0.18);
  s.gameMaxLuminanceValue = Num(e, "gameMaxLuminanceValue", 32);
  s.gameInitialExposureValue = Num(e, "gameInitialExposureValue", -14);
  s.overrideGlobalFog = Flag(e, "overrideGlobalFog");
  s.globalFogEnabled = Flag(e, "globalFogEnabled");
  s.globalFogColor = V4(e, "globalFogColor", {1, 1, 1, 1});
  s.globalFogDensity = Num(e, "globalFogDensity", 0.03);
  s.globalFogHeightScale = Num(e, "globalFogHeightScale", 4);
  s.overrideAtmosphericFog = Flag(e, "overrideAtmosphericFog");
  s.fogStartDepth = Num(e, "startDepth", 40);
  s.fogRangeDepthFar = Num(e, "rangeDepthFar", 4000);
  s.fogRangeDepthNear = Num(e, "rangeDepthNear", 1000);
  s.fogAmpFar = Num(e, "fogAmpFar", 800);
  s.fogAmpNear = Num(e, "fogAmpNear", 1);
  s.fogWhiteness = Num(e, "whiteness", 0);
  s.objFogHeightAmpMax = Num(e, "objFogHeightAmpMax", 2);
  s.objFogHeightAmpMaxHeight = Num(e, "objFogHeightAmpMaxHeight", 200);
  s.objFogRgbAmp = V4(e, "objFogRgbAmp", {1, 1, 1, 0.1});
  s.objFogRgbAmpFuncScale = Num(e, "objFogRgbAmpFuncScale", 1000);
  s.overrideAmbientObscurance = Flag(e, "overrideAmbientObscurance");
  s.saoRadius = Num(e, "saoRadius", 0);
  s.saoBias = Num(e, "saoBias", 0);
  s.saoIntensity = Num(e, "saoIntensity", 0);
  s.saoContrast = Num(e, "saoContrast", 0);
  return s;
}

Transform ReadTransform(const Fox2File& file, const Fox2Entity& owner) {
  Transform t;
  if (const Fox2Entity* te = Target(file, owner, "transform")) {
    t.scale = V3(*te, "transform_scale", {1, 1, 1});
    const Fox2Property* q = te->Find("transform_rotation_quat");
    if (q && !q->values.empty()) t.rotation = {q->values[0].q.x, q->values[0].q.y,
                                               q->values[0].q.z, q->values[0].q.w};
    t.translation = V3(*te, "transform_translation", {0, 0, 0});
  }
  return t;
}

// Fox-space composition: translation * rotation * scale of the parent chain,
// innermost (the light's own) last — the rig's child transforms are local to
// the parent (report §4.2).
Transform Compose(const Transform& outer, const Transform& inner) {
  Transform t;
  t.scale = {outer.scale.x * inner.scale.x, outer.scale.y * inner.scale.y,
             outer.scale.z * inner.scale.z};
  const Quat& a = outer.rotation;
  const Quat& b = inner.rotation;
  t.rotation = {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
  // outer.rotation * (outer.scale * inner.translation) + outer.translation.
  Vec3 s = {outer.scale.x * inner.translation.x, outer.scale.y * inner.translation.y,
            outer.scale.z * inner.translation.z};
  const Quat& q = outer.rotation;
  Vec3 u = {q.y * s.z - q.z * s.y, q.z * s.x - q.x * s.z, q.x * s.y - q.y * s.x};
  Vec3 w = {q.y * u.z - q.z * u.y, q.z * u.x - q.x * u.z, q.x * u.y - q.y * u.x};
  t.translation = {s.x + 2 * (q.w * u.x + w.x) + outer.translation.x,
                   s.y + 2 * (q.w * u.y + w.y) + outer.translation.y,
                   s.z + 2 * (q.w * u.z + w.z) + outer.translation.z};
  return t;
}

// The locator-name chain from the entity up to the root, root-first, with the
// composed world transform.
void ResolvePlacement(const Fox2File& file, const Fox2Entity& e, std::vector<std::string>* chain,
                      Transform* world) {
  const Fox2Entity* cur = &e;
  Transform t = ReadTransform(file, *cur);
  *world = t;
  std::vector<const Fox2Entity*> line;
  for (;;) {
    line.push_back(cur);
    const Fox2Entity* parent = Target(file, *cur, "parent");
    if (!parent || parent == cur) break;
    cur = parent;
    t = ReadTransform(file, *cur);
    *world = Compose(t, *world);
  }
  for (auto it = line.rbegin(); it != line.rend(); ++it) {
    const Fox2Property* n = (*it)->Find("name");
    std::string name = (n && !n->values.empty()) ? n->values[0].s : std::string();
    if ((*it) != &e && !name.empty()) chain->push_back(name);
  }
}

}  // namespace

LightSettings MergeLightSettings(const LightSettings& base, const LightSettings& span) {
  LightSettings m = base;
  if (span.overrideDateTime) {
    m.overrideDateTime = true;
    m.dateTimeHour = span.dateTimeHour;
    m.dateTimeMinute = span.dateTimeMinute;
  }
  if (span.overrideSunLightMode) {
    m.overrideSunLightMode = true;
    m.sunLightMode = span.sunLightMode;
  }
  if (span.overrideAtmosphere) {
    m.overrideAtmosphere = true;
    m.sunLux = span.sunLux;
    m.moonLux = span.moonLux;
    m.skyLightSunScale = span.skyLightSunScale;
    m.enlightenOutputScale = span.enlightenOutputScale;
  }
  if (span.overrideGameExposure) {
    m.overrideGameExposure = true;
    m.gameExposureCompensation = span.gameExposureCompensation;
    m.gameMinExposure = span.gameMinExposure;
    m.gameMaxExposure = span.gameMaxExposure;
    m.gameKeyValue = span.gameKeyValue;
    m.gameMaxLuminanceValue = span.gameMaxLuminanceValue;
    m.gameInitialExposureValue = span.gameInitialExposureValue;
  }
  if (span.overrideGlobalFog) {
    m.overrideGlobalFog = true;
    m.globalFogEnabled = span.globalFogEnabled;
    m.globalFogColor = span.globalFogColor;
    m.globalFogDensity = span.globalFogDensity;
    m.globalFogHeightScale = span.globalFogHeightScale;
  }
  if (span.overrideAtmosphericFog) {
    m.overrideAtmosphericFog = true;
    m.fogStartDepth = span.fogStartDepth;
    m.fogRangeDepthFar = span.fogRangeDepthFar;
    m.fogRangeDepthNear = span.fogRangeDepthNear;
    m.fogAmpFar = span.fogAmpFar;
    m.fogAmpNear = span.fogAmpNear;
    m.fogWhiteness = span.fogWhiteness;
    m.objFogHeightAmpMax = span.objFogHeightAmpMax;
    m.objFogHeightAmpMaxHeight = span.objFogHeightAmpMaxHeight;
    m.objFogRgbAmp = span.objFogRgbAmp;
    m.objFogRgbAmpFuncScale = span.objFogRgbAmpFuncScale;
  }
  if (span.overrideAmbientObscurance) {
    m.overrideAmbientObscurance = true;
    m.saoRadius = span.saoRadius;
    m.saoBias = span.saoBias;
    m.saoIntensity = span.saoIntensity;
    m.saoContrast = span.saoContrast;
  }
  return m;
}

std::optional<LightRig> ParseLightRig(const Fox2File& file, std::string* error) {
  const Fox2Entity* tod = nullptr;
  const Fox2Entity* atmo = nullptr;
  const Fox2Entity* skyEnt = nullptr;
  const Fox2Entity* shadowEnt = nullptr;
  const Fox2Entity* arrayEnt = nullptr;
  for (const Fox2Entity& e : file.entities) {
    if (e.className == "TimeOfDay") tod = &e;
    if (e.className == "TppAtmosphere") atmo = &e;
    if (e.className == "TppSky") skyEnt = &e;
    if (e.className == "GrShadowSettings") shadowEnt = &e;
    if (e.className == "GlobalLightArray") arrayEnt = &e;
  }
  if (!tod) {
    if (error) *error = "no TimeOfDay";
    return std::nullopt;
  }
  LightRig rig;
  rig.timeAdvanceMode = Int(*tod, "timeAdvanceMode", 0);
  rig.previewSlot = Int(*tod, "previewSlot", 0);
  if (const Fox2Entity* g = Target(file, *tod, "globalSettings"))
    rig.global = ReadSettings(*g);

  // Span -> the proxy names it enables (§4.2): proxies are TimeSpan children;
  // the span holds them in `children` (EntityHandles), the proxies are also
  // reachable through their own `parent` link.
  std::vector<std::vector<std::string>> spanGroups;
  const Fox2Property* tsp = tod->Find("timeSpans");
  if (!tsp || tsp->values.empty()) {
    if (error) *error = "TimeOfDay has no timeSpans";
    return std::nullopt;
  }
  for (const Fox2Value& v : tsp->values) {
    const Fox2Entity* span = file.ByAddress(v.u);
    if (!span) continue;  // a dangling authoring ref: not a span we can use
    TimeSpanSettings s;
    s.name = Str(*span, "name");
    s.startHour = Int(*span, "startHour", 0);
    s.startMinute = Int(*span, "startMinute", 0);
    if (const Fox2Entity* set = Target(file, *span, "settings"))
      s.settings = MergeLightSettings(rig.global, ReadSettings(*set));
    else
      s.settings = rig.global;
    for (const Fox2Entity& e : file.entities) {
      if (e.className != "TimeOfDayObjectProxy") continue;
      if (Link(file, e, "parent") != span->address) continue;
      const Fox2Property* tg = e.Find("targetObjects");
      if (!tg) continue;
      for (const Fox2Value& t : tg->values) {
        if (const Fox2Entity* target = file.ByAddress(t.u)) {
          std::string name = Str(*target, "name");
          if (!name.empty()) s.groups.push_back(name);
        }
      }
    }
    rig.spans.push_back(std::move(s));
  }

  for (const Fox2Entity& e : file.entities) {
    if (e.className != "PesPluginSettings") continue;
    rig.plugin.tonemapSpeed = Num(e, "tonemapSpeed", rig.plugin.tonemapSpeed);
    break;  // one per rig
  }

  if (atmo) {
    Atmosphere& a = rig.atmosphere;
    a.rayleighScattering = V3(*atmo, "rayleighScatteringCoefficient", a.rayleighScattering);
    a.rayleighScatteringOfCloudySky =
        V3(*atmo, "rayleighScatteringCoefficientOfCloudySky", a.rayleighScattering);
    a.mieScattering = V3(*atmo, "mieScatteringCoefficient", a.mieScattering);
    a.mieScatteringOfCloudySky =
        V3(*atmo, "mieScatteringCoefficientOfCloudySky", a.mieScatteringOfCloudySky);
    a.rayleighHeightScale = Num(*atmo, "rayleighHeightScale", a.rayleighHeightScale);
    a.mieHeightScale = Num(*atmo, "mieHeightScale", a.mieHeightScale);
    a.mieAbsorptionRate = Num(*atmo, "mieAbsorptionRate", a.mieAbsorptionRate);
    a.mieAnisotropy = Num(*atmo, "mieAnisotropy", a.mieAnisotropy);
    a.mieAnisotropyOfCloudySky = Num(*atmo, "mieAnisotropyOfCloudySky", a.mieAnisotropyOfCloudySky);
    a.multiScatteringOrder = Int(*atmo, "multiScatteringOrder", a.multiScatteringOrder);
    a.numBands = Int(*atmo, "numBands", a.numBands);
    a.groundColor = V3(*atmo, "groundColor", a.groundColor);
    a.moonColor = V3(*atmo, "moonColor", a.moonColor);
    a.sunColorExtinctionOfClearSky = V3(*atmo, "sunColorExtinctionOfClearSky", {0, 0, 0});
    a.sunColorOfCloudySky = V3(*atmo, "sunColorOfCloudySky", {0, 0, 0});
    a.northAngle = Num(*atmo, "northAngle", a.northAngle);
    a.latitude = Num(*atmo, "latitude", a.latitude);
    a.longitude = Num(*atmo, "longitude", a.longitude);
    a.gmtTimeDifference = Num(*atmo, "gmtTimeDifference", a.gmtTimeDifference);
    a.year = Int(*atmo, "year", a.year);
    a.month = Int(*atmo, "month", a.month);
    a.day = Int(*atmo, "day", a.day);
    a.starLight = Num(*atmo, "starLight", a.starLight);
    a.skyColorSunScale = Num(*atmo, "skyColorSunScale", a.skyColorSunScale);
    a.skyLightLuminanceScale = Num(*atmo, "skyLightLuminanceScale", a.skyLightLuminanceScale);
    a.daySkyAmbientScale = Num(*atmo, "daySkyAmbientScale", a.daySkyAmbientScale);
    a.cloudiness = Num(*atmo, "cloudiness", a.cloudiness);
    a.influenceOfFog = Num(*atmo, "influenceOfFog", a.influenceOfFog);
    a.atshPath = Str(*atmo, "atshFilePath");
    const std::string::size_type st = a.atshPath.find("st");
    if (st != std::string::npos && st + 5 <= a.atshPath.size())
      rig.stadium = a.atshPath.substr(st, 5);
    a.shadowRange = Num(*atmo, "shadowRange", a.shadowRange);
    a.shadowRangeExtra = Num(*atmo, "shadowRangeExtra", a.shadowRangeExtra);
    a.hiResShadowRange = Num(*atmo, "hiResShadowRange", a.hiResShadowRange);
    a.shadowProjectionRange = Num(*atmo, "shadowProjectionRange", a.shadowProjectionRange);
    a.shadowFadeRange = Num(*atmo, "shadowFadeRange", a.shadowFadeRange);
    a.selfShadowBias = Num(*atmo, "selfShadowBias", a.selfShadowBias);
  }

  if (skyEnt) {
    SkyDome& d = rig.sky;
    d.enable = Flag(*skyEnt, "enable");
    d.textureRepeats = Num(*skyEnt, "textureRepeats", d.textureRepeats);
    d.cloudCover = Num(*skyEnt, "cloudCover", d.cloudCover);
    d.diffusion = Num(*skyEnt, "diffusion", d.diffusion);
    d.density = Num(*skyEnt, "density", d.density);
    d.dirLightGain = Num(*skyEnt, "dirLightGain", d.dirLightGain);
    d.inCloudScatterGain = Num(*skyEnt, "inCloudScatterGain", d.inCloudScatterGain);
    d.cloudWindInfluence = Num(*skyEnt, "cloudWindInfluence", d.cloudWindInfluence);
    d.priority = Int(*skyEnt, "priority", d.priority);
  }

  if (shadowEnt) {
    ShadowSettings& s = rig.shadows;
    s.method = Str(*shadowEnt, "method");
    s.shadowOpacity = Num(*shadowEnt, "shadowOpacity", s.shadowOpacity);
    s.shadowBlurRadius = Num(*shadowEnt, "shadowBlurRadius", s.shadowBlurRadius);
    s.maskedShadowBlurRadius = Num(*shadowEnt, "maskedShadowBlurRadius", s.maskedShadowBlurRadius);
    s.depthBias = Num(*shadowEnt, "depthBias", s.depthBias);
    s.temporalSampleCount = Int(*shadowEnt, "temporalSampleCount", s.temporalSampleCount);
  }

  for (const Fox2Entity& e : file.entities) {
    if (e.className != "SpotLight" && e.className != "PointLight") continue;
    RigLight l;
    l.name = Str(e, "name");
    l.spot = e.className == "SpotLight";
    l.reachPoint = V3(e, "reachPoint", l.reachPoint);
    l.innerRange = Num(e, "innerRange", l.innerRange);
    l.outerRange = Num(e, "outerRange", l.outerRange);
    l.color = V4(e, "color", l.color);
    l.temperature = Num(e, "temperature", l.temperature);
    l.colorDeflection = Num(e, "colorDeflection", l.colorDeflection);
    l.lumen = Num(e, "lumen", l.lumen);
    l.lightSize = Num(e, "lightSize", l.lightSize);
    l.umbraAngle = Num(e, "umbraAngle", l.umbraAngle);
    l.penumbraAngle = Num(e, "penumbraAngle", l.penumbraAngle);
    l.attenuationExponent = Num(e, "attenuationExponent", l.attenuationExponent);
    l.shadowUmbraAngle = Num(e, "shadowUmbraAngle", l.shadowUmbraAngle);
    l.shadowPenumbraAngle = Num(e, "shadowPenumbraAngle", l.shadowPenumbraAngle);
    l.shadowAttenuationExponent = Num(e, "shadowAttenuationExponent", l.shadowAttenuationExponent);
    l.dimmer = Num(e, "dimmer", l.dimmer);
    l.shadowBias = Num(e, "shadowBias", l.shadowBias);
    l.viewBias = Num(e, "viewBias", l.viewBias);
    l.powerScale = Num(e, "powerScale", l.powerScale);
    l.enable = Flag(e, "enable");
    l.castShadow = Flag(e, "castShadow");
    l.showObject = Flag(e, "showObject");
    l.showRange = Flag(e, "showRange");
    l.isBounced = Flag(e, "isBounced");
    l.useAutoDimmer = Flag(e, "useAutoDimmer");
    l.hasSpecular = Flag(e, "hasSpecular");
    ResolvePlacement(file, e, &l.groups, &l.transform);
    rig.lights.push_back(std::move(l));
  }

  for (const Fox2Entity& e : file.entities) {
    if (e.className != "ReflectionProbe") continue;
    RigProbe p;
    p.name = Str(e, "name");
    p.enable = Flag(e, "enable");
    p.parallax = Flag(e, "enableParallaxCorrection");
    p.priority = Int(e, "priority", p.priority);
    p.intensity = Num(e, "intensity", p.intensity);
    p.innerScale[0][0] = Num(e, "innerScaleXNegative", 0.8);
    p.innerScale[0][1] = Num(e, "innerScaleXPositive", 0.8);
    p.innerScale[1][0] = Num(e, "innerScaleYNegative", 0.8);
    p.innerScale[1][1] = Num(e, "innerScaleYPositive", 0.8);
    p.innerScale[2][0] = Num(e, "innerScaleZNegative", 0.8);
    p.innerScale[2][1] = Num(e, "innerScaleZPositive", 0.8);
    p.captureResolution = Int(e, "captureResolution", p.captureResolution);
    std::vector<std::string> chain;
    Transform world;
    ResolvePlacement(file, e, &chain, &world);
    p.groups = chain;
    p.origin = world.translation;
    p.box = world.scale;  // the probe's transform scale, composed up the chain
    p.capture = p.origin;
    if (const Fox2Entity* cap = Target(file, e, "capturePosition")) {
      // capturePosition points at a locator: its own transform holds the offset.
      if (const Fox2Entity* ct = Target(file, *cap, "transform"))
        p.capture = V3(*ct, "transform_translation", p.capture);
      else
        p.capture = V3(*cap, "transform_translation", p.capture);
    }
    rig.probes.push_back(std::move(p));
  }

  if (arrayEnt) {
    if (const Fox2Property* sl = arrayEnt->Find("spotLights"))
      for (const Fox2Value& v : sl->values)
        if (const Fox2Entity* l = file.ByAddress(v.u)) rig.globalLights.push_back(Str(*l, "name"));
  }
  return rig;
}

std::string LightRigPath(int stadiumId, bool rain) {
  char buf[96];
  std::snprintf(buf, sizeof buf, "Asset/model/bg/st%03d/light/#Win/light_st%03d_a%c.fpkd",
                stadiumId, stadiumId, rain ? 'r' : 'f');
  return buf;
}

namespace {
// The .fox2 payload of an .fpkd: the member whose path ends ".fox2" (same as
// stadium.cpp's reader).
std::optional<std::span<const uint8_t>> Fox2In(const FpkArchive& fpkd,
                                               std::span<const uint8_t> blob,
                                               std::string* error) {
  for (const auto& e : fpkd.entries)
    if (e.path.size() > 5 && e.path.compare(e.path.size() - 5, 5, ".fox2") == 0) {
      if (uint64_t(e.offset) + e.size > blob.size()) {
        if (error) *error = "fox2 member out of bounds";
        return std::nullopt;
      }
      return std::span<const uint8_t>(blob.data() + e.offset, e.size);
    }
  if (error) *error = "no .fox2 member";
  return std::nullopt;
}
}  // namespace

std::optional<LightRig> LoadLightRig(const PesVfs& vfs, int stadiumId, bool rain,
                                     std::string* error) {
  const std::string path = LightRigPath(stadiumId, rain);
  auto blob = vfs.Read(path, error);
  if (!blob) return std::nullopt;
  auto fpkd = FpkArchive::Parse(*blob, error);
  if (!fpkd) return std::nullopt;
  auto payload = Fox2In(*fpkd, *blob, error);
  if (!payload) {
    if (error) *error = path + ": " + *error;
    return std::nullopt;
  }
  auto fox2 = ParseFox2(*payload, error);
  if (!fox2) {
    if (error) *error = path + ": " + *error;
    return std::nullopt;
  }
  auto rig = ParseLightRig(*fox2, error);
  if (!rig && error) *error = path + ": " + *error;
  return rig;
}

namespace {
// Standard NOAA solar position (report §3.3): Julian centuries, geometric
// mean longitude, mean anomaly, equation of centre, apparent longitude,
// obliquity, declination and the equation of time (minutes).
void SolarNoaa(double jd, double* declDeg, double* eotMin) {
  const double t = (jd - 2451545.0) / 36525.0;
  const double rad = kPi / 180.0;
  double l0 = 280.46646 + t * (36000.76983 + t * 0.0003032);
  l0 -= 360.0 * std::floor(l0 / 360.0);
  double m = 357.52911 + t * (35999.05029 - 0.0001537 * t);
  m -= 360.0 * std::floor(m / 360.0);
  const double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  const double c = std::sin(m * rad) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
                   std::sin(2 * m * rad) * (0.019993 - 0.000101 * t) +
                   std::sin(3 * m * rad) * 0.000289;
  const double th = l0 + c;
  const double om = 125.04 - 1934.136 * t;
  const double lam = th - 0.00569 - 0.00478 * std::sin(om * rad);
  const double eps0 =
      23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
  const double eps = eps0 + 0.00256 * std::cos(om * rad);
  *declDeg = std::asin(std::sin(eps * rad) * std::sin(lam * rad)) / rad;
  const double y = std::pow(std::tan(eps * rad / 2.0), 2.0);
  const double eqt = y * std::sin(2 * l0 * rad) - 2 * e * std::sin(m * rad) +
                     4 * e * y * std::sin(m * rad) * std::cos(2 * l0 * rad) -
                     0.5 * y * y * std::sin(4 * l0 * rad) -
                     1.25 * e * e * std::sin(2 * m * rad);
  *eotMin = 4.0 * eqt / rad;
}
double JulianDay(int y, int mo, int d, double utHours) {
  int yy = y, mm = mo;
  if (mm <= 2) {
    yy -= 1;
    mm += 12;
  }
  const int a = yy / 100;
  const int b = 2 - a + a / 4;
  return int(365.25 * (yy + 4716)) + int(30.6001 * (mm + 1)) + d + b - 1524.5 +
         utHours / 24.0;
}
}  // namespace

Vec3 SunToDirection(const Atmosphere& atmo, int hour, int minute) {
  // Fitted model (report §3.3): NOAA with the latitude negated and the hour
  // angle mirrored (lon − GHA), UT = local − gmt.
  const double ut = hour + minute / 60.0 - atmo.gmtTimeDifference;
  double decl, eot;
  SolarNoaa(JulianDay(atmo.year, atmo.month, atmo.day, ut), &decl, &eot);
  const double h = atmo.longitude - 15.0 * (ut - 12.0) - eot / 4.0;  // degrees
  const double rad = kPi / 180.0;
  const double phi = -atmo.latitude * rad;
  const double sinEl =
      std::sin(phi) * std::sin(decl * rad) + std::cos(phi) * std::cos(decl * rad) * std::cos(h * rad);
  const double el = std::asin(std::max(-1.0, std::min(1.0, sinEl)));
  double az = std::atan2(std::sin(h * rad), std::cos(h * rad) * std::sin(phi) -
                                                  std::tan(decl * rad) * std::cos(phi)) /
              rad;
  az = std::fmod(az + 180.0 + 360.0, 360.0);  // clockwise from north
  const double a = (atmo.northAngle - az) * rad;
  const double cosEl = std::cos(el);
  return {float(cosEl * std::sin(a)), float(std::sin(el)), float(cosEl * std::cos(a))};
}

double SunHorizonFade(const Vec3& lightDir) {
  // Report Part B §10: the up vector in the engine is (0,−1,0) and the stored
  // dir points from the light, so e = −lightDir.y.
  const double e = -lightDir.y;
  if (e >= -0.137444) return 1.0;
  double f = 1.0 - (-0.137444 - e) / 0.00518455;
  return f < 0.0 ? 0.0 : f;
}

namespace {
const TimeSpanSettings kEmptySpan;
}  // namespace
const TimeSpanSettings& LightRig::SpanForDayNight(bool night, int season) const {
  // The slot mapper (report §2.3): each span name's suffix sets the slots it
  // fills; later spans overwrite earlier ones.
  int daySummer = -1, dayWinter = -1, nightSummer = -1, nightWinter = -1;
  auto endsWith = [](const std::string& n, const char* s) {
    const size_t a = n.size(), b = std::strlen(s);
    return a >= b && n.compare(a - b, b, s) == 0;
  };
  for (size_t i = 0; i < spans.size(); ++i) {
    const std::string& n = spans[i].name;
    if (endsWith(n, "_day_s"))
      daySummer = int(i);
    else if (endsWith(n, "_day_w"))
      dayWinter = int(i);
    else if (endsWith(n, "_night_s"))
      nightSummer = int(i);
    else if (endsWith(n, "_night_w"))
      nightWinter = int(i);
    else if (endsWith(n, "_day")) {
      daySummer = dayWinter = int(i);
    } else if (endsWith(n, "_night")) {
      nightSummer = nightWinter = int(i);
    }
  }
  int idx = 0;
  if (!night)
    idx = season == 1 ? dayWinter : daySummer;
  else
    idx = season == 1 ? nightWinter : nightSummer;
  if (idx < 0) idx = 0;
  if (spans.empty()) return kEmptySpan;
  if (size_t(idx) >= spans.size()) idx = int(spans.size()) - 1;
  return spans[size_t(idx)];
}

const TimeSpanSettings& LightRig::SpanForTime(int hour, int minute) const {
  if (spans.empty()) return kEmptySpan;
  size_t idx = 0;
  const int t = hour * 60 + minute;
  for (size_t i = 0; i < spans.size(); ++i)
    if (spans[i].startHour * 60 + spans[i].startMinute <= t) idx = i;
  return spans[idx];
}

namespace {
// The .atsh fp16 is non-IEEE (report Part B §9): exponent 31 is a normal
// exponent (no Inf/NaN), saturating at 0x7FFF = 131008.
float AtshHalf(uint16_t h) {
  const uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, f = h & 1023;
  if (e == 31) return (s ? -131008.0f : 131008.0f);
  if (e == 0) return (s ? -1.0f : 1.0f) * f * 5.960464477539063e-08f;  // 2^-24
  uint32_t b = (s << 31) | ((e + 112) << 23) | (f << 13);
  float out;
  std::memcpy(&out, &b, sizeof out);
  return out;
}
uint32_t Load32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, sizeof v);
  return v;
}
uint16_t Load16(const uint8_t* p) {
  uint16_t v;
  std::memcpy(&v, p, sizeof v);
  return v;
}
}  // namespace

std::optional<AtshTable> ParseAtsh(std::span<const uint8_t> blob, std::string* error) {
  auto fail = [&](const std::string& m) {
    if (error) *error = m;
    return std::optional<AtshTable>();
  };
  if (blob.size() < 0x150) return fail("atsh too small");
  if (Load32(blob.data()) != 1 || Load32(blob.data() + 8) != blob.size())
    return fail("atsh header mismatch");
  // Params at 0x70: {u64 name hash, u32 type 0x1c, u32 value} records (the
  // type name "AtmosphereSHCoefficients" sits at 0x50). numDiv and
  // numWeathers are found by their low-48-bit hashes.
  const uint8_t* params = blob.data() + 0x70;
  uint32_t numDiv = 0, numWeathers = 0;
  for (; params + 16 <= blob.data() + 0xb0; params += 16) {
    if (Load32(params + 8) != 0x1c) continue;
    // Fox hashes are the low 48 bits of StrCode64 (fox2 format doc §6).
    const uint64_t h =
        (uint64_t(Load32(params)) | (uint64_t(Load32(params + 4)) << 32)) & 0xffffffffffffull;
    if (h == 0x0a4a00100000ull)
      numDiv = Load32(params + 12);
    else if (h == 0x0a9b00000000ull)
      numWeathers = Load32(params + 12);
  }
  if (numDiv != 40 || numWeathers != 2) return fail("atsh dims not 40x2");
  if (blob.size() != 0x150 + 2u * 40u * 60u) return fail("atsh size mismatch");
  AtshTable t;
  for (int k = 0; k < 40; ++k) t.times[k] = Load32(blob.data() + 0xb0 + 4 * k);
  const uint8_t* p = blob.data() + 0x150;
  for (int w = 0; w < 2; ++w)
    for (int k = 0; k < 40; ++k) {
      for (int c = 0; c < 9; ++c) {
        t.sh[w][k][c] = {0, 0, 0};
        for (int ch = 0; ch < 3; ++ch) {
          (&t.sh[w][k][c].x)[ch] = AtshHalf(Load16(p));
          p += 2;
        }
      }
      t.light[w][k] = {AtshHalf(Load16(p)), AtshHalf(Load16(p + 2)), AtshHalf(Load16(p + 4))};
      p += 6;
    }
  return t;
}

namespace {
int AtshSlot(const AtshTable& t, int hour, int minute) {
  const uint32_t s = uint32_t(hour * 3600 + minute * 60);
  int best = 0;
  uint32_t bestD = 1u << 31;
  for (int k = 0; k < 40; ++k) {
    const uint32_t d = t.times[k] > s ? t.times[k] - s : s - t.times[k];
    if (d < bestD) {
      bestD = d;
      best = k;
    }
  }
  return best;
}
}  // namespace

Vec3 AtshSunColor(const AtshTable& atsh, bool cloudy, int hour, int minute) {
  // The two weather slots are identical in the shipped files; the cloudy slot
  // is read when the rain rig (cloudiness 0.7) is active.
  return atsh.light[cloudy ? 1 : 0][AtshSlot(atsh, hour, minute)];
}

Vec3 SunColour(const Atmosphere& atmo, const AtshTable* atsh, bool cloudy, int hour, int minute) {
  Vec3 c = {1, 1, 1};
  if (atsh) c = AtshSunColor(*atsh, cloudy, hour, minute);
  // The PES sun row's magnitude is I·π·S (sun-radiance §1.6, live E-A): the .atsh chromaticity at
  // unit length, times π·S.
  const float len = std::max(std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z), 1e-6f);
  const float scale = float(kPi * atmo.skyColorSunScale) / len;
  return {c.x * scale, c.y * scale, c.z * scale};
}

SkyIntensities SkyRadianceScales(const LightSettings& s, const Atmosphere& atmo, double sunFade) {
  return {s.sunLux * sunFade * kPi * atmo.skyColorSunScale, s.moonLux * kPi * atmo.skyColorSunScale};
}

const Vec3 (*AtshSkySh(const AtshTable& atsh, bool cloudy, int hour, int minute))[9] {
  return &atsh.sh[cloudy ? 1 : 0][AtshSlot(atsh, hour, minute)];
}

std::optional<std::vector<uint8_t>> FpkMember(std::span<const uint8_t> fpkBlob,
                                              std::string_view memberSuffix, std::string* error) {
  auto fpk = FpkArchive::Parse(fpkBlob, error);
  if (!fpk) return std::nullopt;
  for (const auto& e : fpk->entries)
    if (e.path.size() >= memberSuffix.size() &&
        e.path.compare(e.path.size() - memberSuffix.size(), memberSuffix.size(), memberSuffix) ==
            0 &&
        uint64_t(e.offset) + e.size <= fpkBlob.size())
      return std::vector<uint8_t>(fpkBlob.data() + e.offset, fpkBlob.data() + e.offset + e.size);
  if (error) *error = "no fpk member ends with " + std::string(memberSuffix);
  return std::nullopt;
}

std::optional<AtshTable> LoadAtsh(const PesVfs& vfs, int stadiumId, bool rain, std::string* error) {
  // The packed rig: same folder, .fpk instead of .fpkd (report Part B §9).
  std::string path = LightRigPath(stadiumId, rain);
  path.resize(path.size() - 1);  // ".fpkd" -> ".fpk"
  auto blob = vfs.Read(path, error);
  if (!blob) return std::nullopt;
  char member[16];
  std::snprintf(member, sizeof member, "_a%c.atsh", rain ? 'r' : 'f');
  auto atshBlob = FpkMember(*blob, member, error);
  if (!atshBlob) {
    if (error) *error = path + ": " + *error;
    return std::nullopt;
  }
  auto atsh = ParseAtsh(*atshBlob, error);
  if (!atsh && error) *error = path + ": " + *error;
  return atsh;
}

}  // namespace pes
