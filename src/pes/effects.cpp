#include "pes/effects.hpp"

#include <charconv>
#include <cstdint>
#include <system_error>
#include <utility>

namespace pes {
namespace {

constexpr int kNoIndex = -1;          // EffectParam::index when the attribute is absent
constexpr int kDefaultSettingIndex = 0;
constexpr double kChannelFullScale = 255.0;  // 8-bit colour channel, rs::Color
constexpr int kRgbaRedShift = 24;            // 0xRRGGBBAA: red is the high byte
constexpr int kRgbaGreenShift = 16;
constexpr int kRgbaBlueShift = 8;
constexpr uint32_t kByteMask = 0xFF;
constexpr int kHexBase = 16;

struct Tag {
  std::string name;
  std::vector<std::pair<std::string, std::string>> attrs;
  bool closing = false;
  bool selfClosing = false;
};

struct ParseState {
  bool rooted = false;
  bool inSetting = false;
};

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

void SkipSpace(std::string_view s, size_t& pos) {
  while (pos < s.size() && IsSpace(s[pos])) ++pos;
}

bool IsNameStop(char c) { return c == '/' || c == '>' || c == '=' || c == '"' || c == '<'; }

// Tag and attribute names: everything up to space, '/', '>', '=', '"' or '<'.
std::string ReadName(std::string_view s, size_t& pos) {
  const size_t start = pos;
  while (pos < s.size() && !IsSpace(s[pos]) && !IsNameStop(s[pos])) ++pos;
  return std::string(s.substr(start, pos - start));
}

// Next element tag, skipping text, <?...?> and <!--...-->. Returns 1 for a tag,
// 0 at end of input, -1 on malformed markup (with *err set).
int NextTag(std::string_view s, size_t& pos, Tag& tag, std::string* err) {
  for (;;) {
    const size_t lt = s.find('<', pos);
    if (lt == std::string_view::npos) {
      pos = s.size();
      return 0;
    }
    pos = lt;
    if (s.substr(pos, 4) == "<!--") {
      const size_t end = s.find("-->", pos + 4);
      if (end == std::string_view::npos) {
        *err = "unterminated comment";
        return -1;
      }
      pos = end + 3;
      continue;
    }
    if (s.substr(pos, 2) == "<?") {
      const size_t end = s.find("?>", pos + 2);
      if (end == std::string_view::npos) {
        *err = "unterminated declaration";
        return -1;
      }
      pos = end + 2;
      continue;
    }
    break;
  }
  ++pos;  // past '<'
  tag = Tag{};
  if (pos < s.size() && s[pos] == '/') {
    tag.closing = true;
    ++pos;
  }
  tag.name = ReadName(s, pos);
  if (tag.name.empty()) {
    *err = "markup without an element name";
    return -1;
  }
  for (;;) {
    SkipSpace(s, pos);
    if (pos >= s.size()) {
      *err = "unterminated <" + tag.name + ">";
      return -1;
    }
    if (s[pos] == '>') {
      ++pos;
      return 1;
    }
    if (s[pos] == '/' && pos + 1 < s.size() && s[pos + 1] == '>') {
      tag.selfClosing = true;
      pos += 2;
      return 1;
    }
    if (tag.closing) {
      *err = "attributes on a closing tag </" + tag.name + ">";
      return -1;
    }
    std::string key = ReadName(s, pos);
    SkipSpace(s, pos);
    if (key.empty() || pos >= s.size() || s[pos] != '=') {
      *err = "malformed attribute in <" + tag.name + ">";
      return -1;
    }
    ++pos;
    SkipSpace(s, pos);
    if (pos >= s.size() || s[pos] != '"') {
      *err = "attribute " + key + " of <" + tag.name + "> is not double-quoted";
      return -1;
    }
    const size_t close = s.find('"', pos + 1);
    if (close == std::string_view::npos) {
      *err = "unterminated attribute " + key + " of <" + tag.name + ">";
      return -1;
    }
    tag.attrs.emplace_back(std::move(key), std::string(s.substr(pos + 1, close - pos - 1)));
    pos = close + 1;
  }
}

const std::string* Attr(const Tag& tag, std::string_view name) {
  for (const auto& [k, v] : tag.attrs)
    if (k == name) return &v;
  return nullptr;
}

bool ParseInt(std::string_view t, int* out) {
  auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), *out);
  return ec == std::errc() && p == t.data() + t.size();
}

std::string_view Trim(std::string_view t) {
  while (!t.empty() && IsSpace(t.front())) t.remove_prefix(1);
  while (!t.empty() && IsSpace(t.back())) t.remove_suffix(1);
  return t;
}

bool IsHex(std::string_view t) { return t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'); }

// One decimal float ("-0.02", "84.0"); uint parses hex itself (ConvertValue).
bool ParseNumber(std::string_view t, double* out) {
  if (t.size() > 1 && t[0] == '+') t.remove_prefix(1);
  auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), *out);
  return ec == std::errc() && p == t.data() + t.size();
}

bool ParseHex(std::string_view t, uint32_t* out) {
  t.remove_prefix(2);  // "0x"
  auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), *out, kHexBase);
  return ec == std::errc() && p == t.data() + t.size();
}

// Comma-separated numbers, exactly `count` of them.
bool ParseNumbers(std::string_view value, size_t count, std::vector<double>& out) {
  size_t n = 0;
  while (true) {
    const size_t comma = value.find(',');
    const std::string_view tok = Trim(value.substr(0, comma));
    double v = 0;
    if (!ParseNumber(tok, &v)) return false;
    out.push_back(v);
    ++n;
    if (comma == std::string_view::npos) break;
    value.remove_prefix(comma + 1);
  }
  return n == count;
}

// Converts one param's raw value by its declared type (see effects.hpp).
bool ConvertValue(std::string_view value, EffectParam& p) {
  value = Trim(value);
  const std::string& t = p.type;
  if (t == "float" || t == "angle") return ParseNumbers(value, 1, p.values);
  if (t == "vector2") return ParseNumbers(value, 2, p.values);
  if (t == "vector3") return ParseNumbers(value, 3, p.values);
  if (t == "vector4") return ParseNumbers(value, 4, p.values);
  if (t == "uint") {
    if (IsHex(value)) {
      uint32_t v = 0;
      if (!ParseHex(value, &v)) return false;
      p.values.push_back(v);
      return true;
    }
    double v = 0;
    if (!ParseNumber(value, &v)) return false;
    p.values.push_back(v);
    return true;
  }
  if (t == "rgba") {
    uint32_t v = 0;
    if (!IsHex(value) || !ParseHex(value, &v)) return false;
    p.values = {
        double((v >> kRgbaRedShift) & kByteMask) / kChannelFullScale,
        double((v >> kRgbaGreenShift) & kByteMask) / kChannelFullScale,
        double((v >> kRgbaBlueShift) & kByteMask) / kChannelFullScale,
        double(v & kByteMask) / kChannelFullScale,
    };
    return true;
  }
  p.text = std::string(value);
  return true;
}

bool Apply(const Tag& tag, EffectConfig& cfg, ParseState& st, std::string* err) {
  if (tag.name == "effect") {
    if (!tag.closing) st.rooted = true;
    return true;
  }
  if (tag.name == "setting") {
    if (tag.closing) {
      st.inSetting = false;
      return true;
    }
    EffectSetting s;
    const std::string* type = Attr(tag, "type");
    if (!type) {
      *err = "<setting> without a type";
      return false;
    }
    s.type = *type;
    if (const std::string* index = Attr(tag, "index")) {
      if (!ParseInt(*index, &s.index)) {
        *err = "<setting " + s.type + "> has a bad index";
        return false;
      }
    } else {
      s.index = kDefaultSettingIndex;
    }
    cfg.settings.push_back(std::move(s));
    st.inSetting = !tag.selfClosing;
    return true;
  }
  if (tag.name == "param") {
    if (tag.closing) return true;
    if (!st.inSetting || cfg.settings.empty()) {
      *err = "<param> outside a <setting>";
      return false;
    }
    EffectParam p;
    const std::string* id = Attr(tag, "id");
    const std::string* type = Attr(tag, "type");
    const std::string* value = Attr(tag, "value");
    if (!id || !type || !value) {
      *err = "<param> needs id, type and value";
      return false;
    }
    p.id = *id;
    p.type = *type;
    if (const std::string* index = Attr(tag, "index")) {
      if (!ParseInt(*index, &p.index)) {
        *err = "<param " + p.id + "> has a bad index";
        return false;
      }
    } else {
      p.index = kNoIndex;
    }
    if (!ConvertValue(*value, p)) {
      *err = "<param " + p.id + " type=" + p.type + "> has a bad value: " + *value;
      return false;
    }
    cfg.settings.back().params.push_back(std::move(p));
    return true;
  }
  if (tag.name == "create") {
    if (tag.closing) return true;
    EffectCreate c;
    const std::string* type = Attr(tag, "type");
    const std::string* setting = Attr(tag, "setting");
    if (!type || !setting || !ParseInt(*setting, &c.setting)) {
      *err = "<create> needs type and a numeric setting";
      return false;
    }
    c.type = *type;
    for (const auto& [k, v] : tag.attrs)
      if (k != "type" && k != "setting") c.attributes.emplace_back(k, v);
    cfg.creates.push_back(std::move(c));
    return true;
  }
  return true;  // other elements carry nothing the effect model reads
}

}  // namespace

const EffectParam* EffectSetting::Find(std::string_view id, int wantIndex) const {
  for (const EffectParam& p : params)
    if (p.id == id && p.index == wantIndex) return &p;
  return nullptr;
}

const EffectSetting* EffectConfig::Setting(std::string_view type, int index) const {
  for (const EffectSetting& s : settings)
    if (s.type == type && s.index == index) return &s;
  return nullptr;
}

std::optional<EffectConfig> ParseEffectConfig(std::string_view xml, std::string* error) {
  EffectConfig cfg;
  ParseState st;
  std::string err;
  size_t pos = 0;
  Tag tag;
  auto fail = [&](const std::string& msg) -> std::optional<EffectConfig> {
    if (error) *error = msg;
    return std::nullopt;
  };
  for (;;) {
    const int got = NextTag(xml, pos, tag, &err);
    if (got == 0) break;
    if (got < 0 || !Apply(tag, cfg, st, &err)) return fail(err);
  }
  if (!st.rooted) return fail("no <effect> root");
  if (st.inSetting) return fail("unterminated <setting>");
  return cfg;
}

}  // namespace pes
