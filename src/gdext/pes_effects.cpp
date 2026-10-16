#include "gdext/pes_effects.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <string_view>

#include "pes/effects.hpp"

namespace godot {
namespace {

// The types the scene draws (godot/effects/effect_entries.gd build_entries).
constexpr std::string_view kDrawn[] = {"Vignetting", "MultiUvScrollScreen", "RainDemo"};

bool Drawn(const std::string& type) {
  for (std::string_view d : kDrawn)
    if (type == d) return true;
  return false;
}

Dictionary CreateEntry(const pes::EffectCreate& c) {
  Dictionary attributes;
  for (const auto& [key, value] : c.attributes) attributes[ToGd(key)] = ToGd(value);
  Dictionary d;
  d["type"] = ToGd(c.type);
  d["setting"] = c.setting;
  d["attributes"] = attributes;
  return d;
}

Dictionary SettingEntry(const pes::EffectSetting& s) {
  Array params;
  for (const pes::EffectParam& p : s.params) {
    Array values;
    for (double v : p.values) values.push_back(v);
    Dictionary d;
    d["id"] = ToGd(p.id);
    d["type"] = ToGd(p.type);
    d["index"] = p.index;
    d["values"] = values;
    d["text"] = ToGd(p.text);
    params.push_back(d);
  }
  Dictionary d;
  d["type"] = ToGd(s.type);
  d["index"] = s.index;
  d["params"] = params;
  return d;
}

}  // namespace

void PesEffects::_bind_methods() {
  ClassDB::bind_static_method("PesEffects", D_METHOD("placement", "library", "stadium_id", "weather", "time_zone"),
                              &PesEffects::placement);
}

Dictionary PesEffects::placement(const Ref<PesLibrary>& library, int stadium_id, int weather, int time_zone) {
  if (library.is_null()) {
    UtilityFunctions::push_error("PesEffects: no library");
    return {};
  }
  std::string err;
  auto cfg = pes::LoadEffectConfig(library->vfs(), stadium_id, time_zone != 0, weather != 0, &err);
  if (!cfg) {
    UtilityFunctions::push_error("PesEffects: ", err.c_str());
    return {};
  }
  Array placed, unplaced, orphans;
  for (const pes::EffectCreate& c : cfg->creates) {
    const pes::EffectSetting* setting = cfg->Setting(c.type, c.setting);
    if (!setting) {
      orphans.push_back(CreateEntry(c));
    } else if (!Drawn(c.type)) {
      unplaced.push_back(CreateEntry(c));
    } else {
      Dictionary entry;
      entry["type"] = ToGd(c.type);
      entry["setting"] = SettingEntry(*setting);
      placed.push_back(entry);
    }
  }
  Dictionary out;
  out["placed"] = placed;
  out["unplaced"] = unplaced;
  out["orphans"] = orphans;
  return out;
}

}  // namespace godot
