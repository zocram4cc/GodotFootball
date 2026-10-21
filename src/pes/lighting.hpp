#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "pes/fox2.hpp"
#include "pes/stadium.hpp"  // Transform
#include "pes/types.hpp"
#include "pes/vfs.hpp"

namespace pes {

// The match light rig `light_stNNN_a{f,r}` (docs: docs/re/pes-re-lighting.md
// §0-8): the TimeOfDay spans with their settings, the sun inputs, the cloud
// dome, the floodlights, the reflection probes, the shadow and plugin
// settings. Carries only the groups the lighting uses; the colour-grading,
// demo-exposure, model-intensity and alpha-reflection groups stay in the file for the post chain.

// One lighting group of a TimeOfDaySettings entity (report §2.4): a span's
// settings replace the global entity's group only where the group's
// override<Group> flag is true.
struct LightSettings {
  bool overrideDateTime = false;
  int dateTimeHour = 0, dateTimeMinute = 0;
  enum { SUN_ALL = 0, SUN_PRECOMPUTE_ONLY = 1, SUN_DISABLE = 2, SUN_CASCADE_SHADOW_ONLY = 3 };
  bool overrideSunLightMode = false;
  int sunLightMode = SUN_ALL;
  bool overrideAtmosphere = false;
  double sunLux = 150000, moonLux = 1, skyLightSunScale = 1, enlightenOutputScale = 1;
  bool overrideGameExposure = false;
  double gameExposureCompensation = 0, gameMinExposure = -14, gameMaxExposure = -7;
  double gameKeyValue = 0.18, gameMaxLuminanceValue = 32, gameInitialExposureValue = -14;
  bool overrideGlobalFog = false;
  bool globalFogEnabled = true;
  Vec4 globalFogColor = {1, 1, 1, 1};
  double globalFogDensity = 0.03, globalFogHeightScale = 4;
  bool overrideAtmosphericFog = false;
  double fogStartDepth = 40, fogRangeDepthFar = 4000, fogRangeDepthNear = 1000;
  double fogAmpFar = 800, fogAmpNear = 1, fogWhiteness = 0;
  double objFogHeightAmpMax = 2, objFogHeightAmpMaxHeight = 200;
  Vec4 objFogRgbAmp = {1, 1, 1, 0.1};
  double objFogRgbAmpFuncScale = 1000;
  bool overrideAmbientObscurance = false;
  double saoRadius = 0, saoBias = 0, saoIntensity = 0, saoContrast = 0;
};

// The merged TimeOfDaySettings for one span: the global entity's groups with
// the span's groups replacing them where its override flags are true (§2.4).
LightSettings MergeLightSettings(const LightSettings& base, const LightSettings& span);

// The sun's astronomical inputs (report §3.1): TppAtmosphere minus the
// per-span set (sunLux etc. live in LightSettings).
struct Atmosphere {
  Vec3 rayleighScattering = {0.0058, 0.0135, 0.0331};
  Vec3 rayleighScatteringOfCloudySky = {0.0058, 0.0135, 0.0331};
  Vec3 mieScattering = {0.004, 0.004, 0.004};
  Vec3 mieScatteringOfCloudySky = {0.05, 0.05, 0.03};
  double rayleighHeightScale = 4, mieHeightScale = 1200, mieAbsorptionRate = 0.1;
  double mieAnisotropy = 0.8, mieAnisotropyOfCloudySky = 0.2;
  int multiScatteringOrder = 3, numBands = 3;
  Vec3 groundColor = {0.5, 0.5, 0.5};
  Vec3 moonColor = {0.996, 0.988, 0.843};
  Vec3 sunColorExtinctionOfClearSky = {0, 0, 0};
  Vec3 sunColorOfCloudySky = {0, 0, 0};
  double northAngle = 90, latitude = 0, longitude = 0, gmtTimeDifference = 0;
  int year = 2019, month = 3, day = 6;
  double starLight = 0, skyLightLuminanceScale = 1, daySkyAmbientScale = 1, cloudiness = 0,
         influenceOfFog = 0;
  double skyColorSunScale = 1;  // TppAtmosphere S: the sun colour's π·S peak (1 if the file omits it)
  std::string atshPath;  // .atshFilePath, e.g. the light's own AtmosphereSHCoefficients
  double shadowRange = 120, shadowRangeExtra = 80, hiResShadowRange = 35;
  double shadowProjectionRange = 30, shadowFadeRange = 0, selfShadowBias = -15;
};

// One TimeSpan of TimeOfDay.timeSpans with its span-local enable list §4.2:
// the names the TimeOfDayObjectProxy children of this span list in
// `targetObjects` (a locator name marks the whole subtree, e.g. Light_Pitch_F).
struct TimeSpanSettings {
  std::string name;  // "TimeSpan1500_day"
  int startHour = 0, startMinute = 0;
  LightSettings settings;          // merged over the global entity (§2.4)
  std::vector<std::string> groups;  // locator names enabled in this span
};

// A SpotLight or PointLight of the rig (report §4.2). `groups` is the chain of
// ancestor locator names from the `parent` links, root-first.
struct RigLight {
  std::string name;
  bool spot = true;
  Vec3 reachPoint = {0, -5, 0};
  double innerRange = 0, outerRange = 0;
  Vec4 color = {1, 1, 1, 1};
  double temperature = 6500, colorDeflection = 0, lumen = 0, lightSize = 0;
  double umbraAngle = 0, penumbraAngle = 0, attenuationExponent = 1.2;
  double shadowUmbraAngle = 0, shadowPenumbraAngle = 0, shadowAttenuationExponent = 1.2;
  double dimmer = 0, shadowBias = 0, viewBias = 0, powerScale = 1;
  bool enable = true, castShadow = false, showObject = true, showRange = true;
  bool isBounced = false, useAutoDimmer = false, hasSpecular = true;
  Transform transform;  // resolved down the parent chain to world space
  std::vector<std::string> groups;
};

// A ReflectionProbe (report Part B §6): box from the probe's own transform
// scale, origin offset from the capture position.
struct RigProbe {
  std::string name;
  bool enable = true, parallax = true;
  int priority = 0;
  double intensity = 1;
  Vec3 box = {0, 0, 0};  // the probe's own transform scale (metres)
  Vec3 origin = {0, 0, 0};
  Vec3 capture = {0, 0, 0};
  double innerScale[3][2] = {};  // [x,y,z][neg,pos]
  int captureResolution = 2;
  std::vector<std::string> groups;  // ancestor locator names, like RigLight
};

// The cloud dome state (report Part B §7): a sky_dome.fmdl scaled
// 8000x2000x8000 with these material params.
struct SkyDome {
  bool enable = false;
  double textureRepeats = 1, cloudCover = 0.3, diffusion = 0, density = 0.6;
  double dirLightGain = 2, inCloudScatterGain = 0.4, cloudWindInfluence = 0;
  int priority = 0;
};

// Forward+ shadow recipe from GrShadowSettings + TppAtmosphere (§4.2, §7.3).
struct ShadowSettings {
  std::string method;  // "PrecomputedVSM" etc.
  double shadowOpacity = 1, shadowBlurRadius = 2.5, maskedShadowBlurRadius = 3;
  double depthBias = 1e-5;
  int temporalSampleCount = 4;
};

// PesPluginSettings (report Part C §2, §4.1): one per rig. The tonemap speed is the
// adaptation rate of the eye (the post chain's law).
struct PluginSettings {
  double tonemapSpeed = 30;
};

// A parsed light rig as the lighting surface needs it.
struct LightRig {
  int timeAdvanceMode = 0;
  int previewSlot = 0;
  LightSettings global;
  Atmosphere atmosphere;
  std::vector<TimeSpanSettings> spans;  // in TimeOfDay.timeSpans order
  std::vector<RigLight> lights;
  std::vector<RigProbe> probes;
  std::vector<std::string> globalLights;  // GlobalLightArray.spotLights names
  SkyDome sky;
  ShadowSettings shadows;
  PluginSettings plugin;
  std::string stadium;  // "st002", from the rig's own paths

  // The slot mapper (report §2.3): suffix of each span in order, day/night
  // with season 0 = summer, 1 = winter. Unknown suffixes and out-of-range
  // results clamp to the last span.
  const TimeSpanSettings& SpanForDayNight(bool night, int season) const;
  // Explicit-time lookup: the last span with start <= time; index clamped.
  const TimeSpanSettings& SpanForTime(int hour, int minute) const;
};

// Parses the fox2 payload of a light rig (.fpkd member ending ".fox2").
std::optional<LightRig> ParseLightRig(const Fox2File& file, std::string* error);

// The rig's VFS path for stadium/weather: "f" = fine (report §2.2).
std::string LightRigPath(int stadiumId, bool rain);
// Reads and parses the rig. Null (error set) when missing or unreadable.
std::optional<LightRig> LoadLightRig(const PesVfs& vfs, int stadiumId, bool rain,
                                     std::string* error);

// The sun in Fox space (Y up): to-sun from the NOAA fit (report §3.3), unit,
// from the atmosphere's date with the span's hour:minute (or the merged
// settings' dateTime when overrideDateTime). `lightDir = -toSun`.
Vec3 SunToDirection(const Atmosphere& atmo, int hour, int minute);
// Sun below-horizon fade × 1 (report Part B §10): I_sun = sunLux * fadeSun.
double SunHorizonFade(const Vec3& lightDir);

// AtmosphereSHCoefficients (.atsh, report Part B §9): 0x50 type name, 0x70 two
// {u64 hash, u32 type 0x1c, u32 value} params (40 times, 2 weathers), 0xB0 u32
// time[40], 0x150 entry[2][40] of sh[9][3] radiance SH (cd/m2·√4π) and
// light[3] direct illuminance (lux), with the file's non-IEEE half decode
// (exponent 31 normal, 0x7FFF=131008).
struct AtshTable {
  uint32_t times[40] = {};       // seconds of day, step 2160
  Vec3 sh[2][40][9];             // [weather][time][coefficient]
  Vec3 light[2][40];             // [weather][time]
};
std::optional<AtshTable> ParseAtsh(std::span<const uint8_t> blob, std::string* error);
// The nearest-time entry's direct-light colour for the rig weather slot.
Vec3 AtshSunColor(const AtshTable& atsh, bool cloudy, int hour, int minute);
// The direct sun's colour (sun-radiance §1.2-§1.6): the .atsh colour normalised to its largest
// component, times π·skyColorSunScale (PES's cb3[1] = I·π·S; the intensity sunLux·fade stays with
// the light). A null atsh (the install ships none) is white.
Vec3 SunColour(const Atmosphere& atmo, const AtshTable* atsh, bool cloudy, int hour, int minute);
// The nearest-time SH9 row for the rig weather slot.
const Vec3 (*AtshSkySh(const AtshTable& atsh, bool cloudy, int hour, int minute))[9];
// Reads `memberSuffix` (e.g. an .atsh) from an .fpk already in memory.
std::optional<std::vector<uint8_t>> FpkMember(std::span<const uint8_t> fpkBlob,
                                              std::string_view memberSuffix, std::string* error);
// The .atsh beside the rig's .fpkd (same folder, .fpk): null when the install
// does not ship one.
std::optional<AtshTable> LoadAtsh(const PesVfs& vfs, int stadiumId, bool rain, std::string* error);

}  // namespace pes
