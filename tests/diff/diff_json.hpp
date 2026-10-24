// The differential-test corpus reader: one JSONL file per function, `<fn>.jsonl`.
// Each line: {"va": "0x...", "in": {<named inputs>}, "out": {<named outputs>}}.
// Floats are {"bits": "0x...", "f": ...} (bits are authoritative); ints decimal.
// Minimal parser for exactly this shape: objects, strings, numbers, arrays.

#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace diff {

struct Value {
  enum class Kind { Int, Float, Str, Array, Object } kind = Kind::Int;
  int64_t i = 0;
  uint32_t bits = 0;  // Float: the authoritative IEEE-754 bits
  double f = 0.0;
  std::string s;
  std::vector<Value> items;
  std::map<std::string, Value> fields;

  int AsInt() const { return static_cast<int>(i); }
  uint32_t AsBits() const { return kind == Kind::Float ? bits : static_cast<uint32_t>(i); }
  float AsFloat() const {
    float v = 0.0f;
    if (kind == Kind::Float) std::memcpy(&v, &bits, sizeof v);
    return v;
  }
  const Value& At(const std::string& key) const { return fields.at(key); }
  bool Has(const std::string& key) const { return fields.count(key) != 0; }
};

class Parser {
 public:
  explicit Parser(const std::string& text) : p_(text.c_str()) {}
  Value ParseValue() {
    Skip();
    if (*p_ == '{') return ParseObject();
    if (*p_ == '[') return ParseArray();
    if (*p_ == '"') {
      Value v;
      v.kind = Value::Kind::Str;
      v.s = ParseString();
      return v;
    }
    return ParseNumber();
  }

 private:
  const char* p_;
  void Skip() {
    while (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r') ++p_;
  }
  std::string ParseString() {
    ++p_;  // "
    std::string out;
    while (*p_ && *p_ != '"') {
      if (*p_ == '\\') {
        ++p_;
        out += *p_ ? *p_++ : '\\';
      } else {
        out += *p_++;
      }
    }
    if (*p_ == '"') ++p_;
    return out;
  }
  Value ParseNumber() {
    const char* start = p_;
    if (*p_ == '-') ++p_;
    while ((*p_ >= '0' && *p_ <= '9') || *p_ == '.' || *p_ == 'e' || *p_ == 'E' ||
           ((*p_ == '+' || *p_ == '-') && (p_[-1] == 'e' || p_[-1] == 'E')) || *p_ == 'x' ||
           (*p_ >= 'a' && *p_ <= 'f') || (*p_ >= 'A' && *p_ <= 'F'))
      ++p_;
    std::string tok(start, p_);
    Value v;
    if (tok.compare(0, 2, "0x") == 0 || tok.compare(0, 2, "0X") == 0) {
      v.kind = Value::Kind::Int;
      v.i = static_cast<int64_t>(std::stoul(tok, nullptr, 16));
    } else if (tok.find_first_of(".eE") == std::string::npos) {
      v.kind = Value::Kind::Int;
      v.i = std::stoll(tok);
    } else {
      // A bare decimal float outside a bits dict: keep the value; callers that
      // need exact bits use the {"bits":...} form (the corpus always has it).
      v.kind = Value::Kind::Float;
      v.f = std::stod(tok);
      float f32 = static_cast<float>(v.f);
      std::memcpy(&v.bits, &f32, sizeof f32);
    }
    return v;
  }
  Value ParseObject() {
    Value v;
    v.kind = Value::Kind::Object;
    ++p_;  // {
    Skip();
    if (*p_ == '}') {
      ++p_;
      return v;
    }
    while (true) {
      Skip();
      std::string key = ParseString();
      Skip();  // :
      if (*p_ == ':') ++p_;
      Value val = ParseValue();
      // A {"bits": "0x...", "f": ...} dict is a float: fold it here so callers
      // see one Float value.
      if (val.kind == Value::Kind::Object && val.Has("bits")) {
        Value f;
        f.kind = Value::Kind::Float;
        const Value& b = val.At("bits");
        f.bits = b.kind == Value::Kind::Str ? static_cast<uint32_t>(std::stoul(b.s, nullptr, 16))
                                            : static_cast<uint32_t>(b.i);
        if (val.Has("f") && val.At("f").kind == Value::Kind::Float) f.f = val.At("f").f;
        float f32 = 0.0f;
        std::memcpy(&f32, &f.bits, sizeof f32);
        f.f = f32;
        val = f;
      }
      v.fields[key] = val;
      Skip();
      if (*p_ == ',') {
        ++p_;
        continue;
      }
      if (*p_ == '}') {
        ++p_;
        break;
      }
      break;
    }
    return v;
  }
  Value ParseArray() {
    Value v;
    v.kind = Value::Kind::Array;
    ++p_;  // [
    Skip();
    if (*p_ == ']') {
      ++p_;
      return v;
    }
    while (true) {
      v.items.push_back(ParseValue());
      Skip();
      if (*p_ == ',') {
        ++p_;
        continue;
      }
      if (*p_ == ']') {
        ++p_;
        break;
      }
      break;
    }
    return v;
  }
};

struct Case {
  std::string va;
  Value in, out;
};

// Reads the whole corpus; empty path or missing file yields no cases (the test
// then GTEST_SKIPs, like the install-gated tests without PES21_ROOT).
inline std::vector<Case> ReadCorpus(const std::string& path) {
  std::vector<Case> cases;
  std::ifstream f(path);
  if (!f) return cases;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    Parser parser(line);
    Value root = parser.ParseValue();
    Case c;
    c.va = root.At("va").s;
    c.in = root.At("in");
    c.out = root.At("out");
    cases.push_back(c);
  }
  return cases;
}

inline std::string CorpusPath(const std::string& name) {
  const char* dir = std::getenv("GF_DIFF_CORPUS");
  std::string base = dir ? dir : std::string(GF_DIFF_CORPUS_DIR);
  return base + "/" + name + ".jsonl";
}

}  // namespace diff
