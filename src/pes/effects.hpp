#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pes {

// PES 2021 effect_config.xml (version 2): <setting> blocks of <param>s, and
// <create> lines that instantiate a setting. Comments are skipped whole, so the
// commented-out templates inside a pack are not settings. docs/re/pes-re-effects-particles.md 1.3.
//
// Numeric param types are converted at parse time: float, uint (decimal or
// 0x hex), angle (degrees, as written), vector2/3/4 -> `values`; rgba
// (0xRRGGBBAA, rs::Color order) -> `values` = r,g,b,a in [0,1]. Every other
// type (path, ...) keeps its raw text in `text`.
struct EffectParam {
  std::string id, type;
  int index = -1;  // -1: the param has no index attribute
  std::vector<double> values;
  std::string text;
};

struct EffectSetting {
  std::string type;  // effect class, e.g. "LensFlare", "SmokeBomb2"
  int index = 0;
  std::vector<EffectParam> params;
  // The param with this id and index (-1: no index attribute); null when absent.
  const EffectParam* Find(std::string_view id, int wantIndex = -1) const;
};

struct EffectCreate {
  std::string type;
  int setting = 0;
  std::vector<std::pair<std::string, std::string>> attributes;  // e.g. floor="upper", dir="back"
};

struct EffectConfig {
  std::vector<EffectSetting> settings;
  std::vector<EffectCreate> creates;
  // The setting of this class and index; null when absent.
  const EffectSetting* Setting(std::string_view type, int index) const;
};

// Parses one effect_config.xml. Malformed markup or a bad numeric value fails
// with `error` set and no partial result.
std::optional<EffectConfig> ParseEffectConfig(std::string_view xml, std::string* error);

}  // namespace pes
