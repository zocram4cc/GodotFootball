// diffparse.hpp: minimal JSONL reader for the generated diff mains.
// Each line: {"va": ..., "in": {name: int | {"bits": "0x..", "f": ..}}, ...}.
// Only the value shapes the Map specs emit are parsed: integers, {"bits"},
// strings, nested objects/arrays (skipped structurally). Build tree only.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct JVal {
  enum T { NUM, BITS, STR } t = NUM;
  int64_t n = 0;
  uint32_t bits = 0;
  std::string s;
};

struct JLine {
  std::map<std::string, JVal> in, out;
  long long inst = 0;  // bank instance this case ran on (run.py --seed-bank); 0 when unbanked
};

// Parse one flat-ish line: finds "in":{...} and "out":{...} top objects,
// then k: v pairs where v is a number, "string", or {"bits": "0x..", ...}.
static inline const char* skip_ws(const char* p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
  return p;
}

static inline const char* parse_val(const char* p, JVal& v) {
  p = skip_ws(p);
  if (*p == '"') {
    ++p;
    const char* e = strchr(p, '"');
    v.t = JVal::STR;
    v.s.assign(p, e ? e : p);
    return e ? e + 1 : p;
  }
  if (*p == '{') {
    // object: look for "bits" member
    const char* e = strchr(p, '}');
    const char* b = strstr(p, "\"bits\"");
    v.t = JVal::BITS;
    v.bits = 0;
    if (b && (!e || b < e)) {
      const char* q = strchr(b + 6, '"');
      if (q) v.bits = (uint32_t)strtoul(q + 1, 0, 0);
    }
    return e ? e + 1 : p + 1;
  }
  if (*p == '[') {
    // array: skip balanced
    int d = 0;
    do {
      if (*p == '[') ++d;
      if (*p == ']') --d;
      ++p;
    } while (*p && d > 0);
    v.t = JVal::NUM;
    v.n = 0;
    return p;
  }
  char* end = 0;
  // floats at top level: keep raw bits via strtod+cast? inputs use ints or
  // {bits}; a bare float means f (rare) -> store truncated int.
  if (strchr(p, '.') || strchr(p, 'e') || strchr(p, 'E')) {
    v.t = JVal::NUM;
    v.n = (int64_t)strtod(p, &end);
    return end ? end : p;
  }
  v.t = JVal::NUM;
  v.n = strtoll(p, &end, 0);
  return end ? end : p;
}

static inline const char* parse_obj(const char* p,
                                    std::map<std::string, JVal>& m) {
  // p at '{'
  ++p;
  for (;;) {
    p = skip_ws(p);
    if (*p == '}' || *p == 0) return *p ? p + 1 : p;
    if (*p == ',') {
      ++p;
      continue;
    }
    if (*p != '"') return p;  // malformed; stop
    ++p;
    const char* e = strchr(p, '"');
    if (!e) return p;
    std::string key(p, e);
    p = skip_ws(e + 1);
    if (*p == ':') ++p;
    JVal v;
    p = parse_val(p, v);
    m[key] = v;
  }
}

static inline bool parse_line(const char* line, JLine& jl) {
  const char* pin = strstr(line, "\"in\"");
  const char* pout = strstr(line, "\"out\"");
  if (!pin || !pout) return false;
  if (const char* pi = strstr(line, "\"inst\":")) {  // written by run.py for banked cases
    for (pi += 7; *pi == ' '; ++pi) {}
    for (; *pi >= '0' && *pi <= '9'; ++pi) jl.inst = jl.inst * 10 + (*pi - '0');
  }
  pin = strchr(pin + 4, '{');
  pout = strchr(pout + 5, '{');
  if (!pin || !pout) return false;
  parse_obj(pin, jl.in);
  parse_obj(pout, jl.out);
  return true;
}
