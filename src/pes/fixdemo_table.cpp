#include "pes/fixdemo_table.hpp"

#include <cstring>

namespace pes {
namespace {

constexpr size_t kHeaderWords = 14;
constexpr size_t kMaxKeys = 10;   // the runtime rows keep the first 10 keys and layers
constexpr int kGoalCategory = 3;

const char* const kCategoryNames[] = {"", "ent", "foul", "goal", "pk", "change", "timeup", "end", "result", "mode"};

// Bounds-checked little-endian reads of one typed array; a read past the end marks the decode bad.
template <typename T>
bool ReadArray(const std::vector<uint8_t>& b, size_t off, size_t count, std::vector<T>* out) {
  if (count > b.size() / sizeof(T) || off > b.size() - count * sizeof(T)) return false;
  out->resize(count);
  if (count) std::memcpy(out->data(), b.data() + off, count * sizeof(T));
  return true;
}

// The five typed streams and the section-end counter (pes-re-cutscenes.md §2.1).
struct Reader {
  std::vector<float> a;
  std::vector<uint32_t> b;
  std::vector<uint16_t> w;
  std::vector<uint8_t> u;
  std::vector<std::string> s;
  size_t ia = 0, ib = 0, iw = 0, iu = 0, is = 0;
  bool overrun = false;

  float f() { return next(a, ia, 0.0f); }
  uint32_t u32() { return next(b, ib, 0u); }
  uint16_t u16() { return next(w, iw, uint16_t{0}); }
  uint8_t u8() { return next(u, iu, uint8_t{0}); }
  std::string str() {
    if (is >= s.size()) { overrun = true; return {}; }
    return s[is++];
  }
  uint32_t reads() const { return static_cast<uint32_t>(ia + ib + iw + iu + is); }
  // Counts of every stream as the header declares them.
  bool ConsumedAll(const std::vector<uint32_t>& counts) const {
    return ia == counts[0] && is == counts[1] && ib == counts[3] && iw == counts[4] && iu == counts[5];
  }

 private:
  template <typename V, typename T>
  T next(const V& v, size_t& i, T zero) {
    if (i >= v.size()) { overrun = true; return zero; }
    return v[i++];
  }
};

std::vector<std::string> KeepLayers(const std::vector<std::string>& all) {
  std::vector<std::string> out;
  for (const std::string& s : all)
    if (!s.empty()) out.push_back(s);
  return out;
}

}  // namespace

std::string FixDemoTablePath(int category) {
  if (category < 1 || category > 9) return "";
  return std::string("common/demo/fixdemo/") + kCategoryNames[category] + "/table_" + kCategoryNames[category] + ".bin";
}

std::string FixDemoCutDataPath(int category, const std::string& fdcName) {
  const std::string tablePath = FixDemoTablePath(category);
  if (tablePath.empty()) return "";
  return tablePath.substr(0, tablePath.rfind('/')) + "/cut_data/" + fdcName;
}

std::optional<FixDemoTable> DecodeFixDemoTable(const std::vector<uint8_t>& bytes, int category, std::string* error) {
  auto fail = [error](const char* why) -> std::optional<FixDemoTable> {
    if (error) *error = why;
    return std::nullopt;
  };
  if (category < 1 || category > 9) return fail("category out of range");
  if (bytes.size() < kHeaderWords * 4) return fail("table shorter than its header");

  uint32_t h[kHeaderWords];
  std::memcpy(h, bytes.data(), sizeof h);
  // h: 0 nA (f32), 1 nS (strings), 2 nE (section ends), 3 nB (u32), 4 nW (u16), 5 nU (u8), 6 string bytes,
  //    7 oA, 8 oSe (string end offsets), 9 oE (section ends), 10 oB, 11 oW, 12 oU, 13 oC (string characters).
  Reader r;
  if (!ReadArray(bytes, h[7], h[0], &r.a) || !ReadArray(bytes, h[10], h[3], &r.b) ||
      !ReadArray(bytes, h[11], h[4], &r.w) || !ReadArray(bytes, h[12], h[5], &r.u))
    return fail("stream out of bounds");
  std::vector<uint32_t> ends, sectionEnds;
  if (!ReadArray(bytes, h[8], h[1], &ends) || !ReadArray(bytes, h[9], h[2], &sectionEnds))
    return fail("string or section table out of bounds");
  if (sectionEnds.empty()) return fail("no section end");
  size_t prev = 0;
  for (uint32_t e : ends) {
    if (e < prev || e > h[6] || h[13] + static_cast<size_t>(e) > bytes.size()) return fail("string out of bounds");
    r.s.emplace_back(reinterpret_cast<const char*>(bytes.data()) + h[13] + prev, e - prev);
    prev = e;
  }
  const uint32_t sectionEnd = sectionEnds[0];
  auto atEnd = [&r, sectionEnd] { return r.reads() >= sectionEnd; };

  FixDemoTable t;
  uint16_t n = r.u16();
  if (n == 1) {
    t.version = 1;
    n = r.u16();
  }
  // T1: the cut library. Groups are (start, count) slices of the row list.
  std::vector<std::pair<uint16_t, uint16_t>> groupSlices;
  for (uint16_t i = 0; i < n; ++i) groupSlices.push_back({r.u16(), r.u16()});
  std::vector<std::string> groupNames;
  for (uint16_t i = 0; i < n; ++i) groupNames.push_back(r.str());
  const uint16_t m = r.u16();
  const uint8_t nKeys = r.u8();
  const uint8_t nStr = r.u8();
  std::vector<FixDemoCutRow> rows;
  for (uint16_t i = 0; i < m; ++i) {
    FixDemoCutRow row;
    std::vector<uint16_t> keys;
    for (uint8_t k = 0; k < nKeys; ++k) {
      const uint16_t key = r.u16();
      if (k < kMaxKeys) keys.push_back(key);
    }
    row.keys = keys;
    std::vector<std::string> layers;
    for (uint8_t k = 0; k < nStr; ++k) {
      std::string s = r.str();
      if (k < kMaxKeys) layers.push_back(s);
    }
    row.fdc = KeepLayers(layers);
    row.b0 = r.u32();
    row.f = r.f();
    row.b1 = r.u32();
    rows.push_back(row);
  }
  // T2: the scene table. Scenes are (start, count) slices of the step list.
  const uint16_t nScenes = r.u16();
  std::vector<std::pair<uint16_t, uint16_t>> sceneSlices;
  for (uint16_t i = 0; i < nScenes; ++i) sceneSlices.push_back({r.u16(), r.u16()});
  std::vector<std::string> sceneNames;
  for (uint16_t i = 0; i < nScenes; ++i) sceneNames.push_back(r.str());
  const uint16_t nSteps = r.u16();
  std::vector<FixDemoStep> steps;
  for (uint16_t i = 0; i < nSteps; ++i) {
    FixDemoStep st;
    st.kind = r.u8();
    st.fade = r.f();
    st.cut = r.u32();
    st.flags = r.u32();
    st.c0 = r.u8();
    st.c1 = r.u8();
    st.w0 = r.u16();
    st.w1 = static_cast<int16_t>(r.u16());
    const uint8_t k = r.u8();
    for (uint8_t j = 0; j < k; ++j) {
      std::array<uint8_t, 3> tr{r.u8(), r.u8(), r.u8()};
      if (j < kMaxKeys) st.trip.push_back(tr);
    }
    if (t.version == 1) st.w2 = static_cast<int16_t>(r.u16());
    steps.push_back(st);
  }
  // Goal extras (pes-re-cutscenes.md §2.2): edit scenes, then playerId -> scene pairs (absent at end), then one
  // 0x3c-byte row per T2 scene, then the optional T3 control table (consumed to keep the reads exact; not kept).
  if (category == kGoalCategory) {
    const uint16_t ne = r.u16();
    for (uint16_t i = 0; i < ne; ++i) t.goalEditScenes.push_back(r.u32());
    if (!atEnd()) {
      const uint16_t np = r.u16();
      for (uint16_t i = 0; i < np; ++i) {
        FixDemoGoalPlayerScene p;
        p.playerId = r.u32();
        p.sceneId = r.u32();
        t.goalPlayerScenes.push_back(p);
      }
    }
    for (size_t i = 0; i < sceneSlices.size(); ++i) {
      for (int k = 0; k < 5; ++k) r.u8();
      for (int k = 0; k < 12; ++k) r.f();
      for (int k = 0; k < 4; ++k) r.u8();
    }
    if (!atEnd()) {
      const uint16_t ng = r.u16();
      for (uint16_t i = 0; i < ng; ++i) { r.u16(); r.u16(); }
      for (uint16_t i = 0; i < ng; ++i) r.str();
      const uint16_t cm = r.u16();
      const uint8_t cnk = r.u8();
      for (uint16_t i = 0; i < cm; ++i) {
        for (uint8_t k = 0; k < cnk; ++k) r.u16();
        const uint8_t nf = r.u8();
        const uint8_t ni = r.u8();
        for (uint8_t k = 0; k < nf; ++k) r.f();
        for (uint8_t k = 0; k < ni; ++k) r.u32();
      }
    }
  }
  if (r.overrun) return fail("stream read past its header count");
  if (r.reads() != sectionEnd) return fail("section end does not match the reads");
  if (!r.ConsumedAll({h[0], h[1], h[2], h[3], h[4], h[5]})) return fail("streams not fully consumed");

  // Assemble: a slice that runs past its list is a corrupt table.
  for (size_t gi = 0; gi < groupSlices.size(); ++gi) {
    const auto [start, count] = groupSlices[gi];
    if (static_cast<size_t>(start) + count > rows.size()) return fail("cut group slice out of range");
    FixDemoCutGroup g;
    g.id = (static_cast<uint32_t>(category) << 16) | static_cast<uint32_t>(gi + 1);
    g.firstRow = start;
    g.name = groupNames[gi];
    g.rows.assign(rows.begin() + start, rows.begin() + start + count);
    t.cutGroups.push_back(g);
  }
  for (size_t si = 0; si < sceneSlices.size(); ++si) {
    const auto [start, count] = sceneSlices[si];
    if (static_cast<size_t>(start) + count > steps.size()) return fail("scene slice out of range");
    FixDemoScene sc;
    sc.id = (static_cast<uint32_t>(category) << 16) | static_cast<uint32_t>(si + 1);
    sc.name = sceneNames[si];
    sc.steps.assign(steps.begin() + start, steps.begin() + start + count);
    t.scenes.push_back(sc);
  }
  return t;
}

}  // namespace pes
