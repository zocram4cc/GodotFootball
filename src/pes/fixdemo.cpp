#include "pes/fixdemo.hpp"

#include <cstring>

namespace pes::fixdemo {
namespace {

constexpr const char* kFixDemoRoot = "common/demo/fixdemo/";

// Cut times are in 1/60 s ticks (pes-re-cutscenes.md §3.2), whatever rate the simulation runs at.
constexpr float kTicksPerSecond = 60.0f;
// A Cut record has type 0, and its maxTime is the f32 at +0x0c (§3.2).
constexpr uint8_t kCutRecordType = 0;
constexpr size_t kMaxTimeOffset = 0x0c;

// A table starts with 14 u32 words (§2.1), then four typed column streams and a string pool.
constexpr size_t kHeaderWords = 14;
constexpr size_t kWordBytes = 4;
enum Column { kF32, kU32, kU16, kU8, kColumns };
constexpr size_t kColumnWidth[kColumns] = {4, 4, 2, 1};

uint32_t Le32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

float FloatOf(uint32_t raw) {
  float value;
  std::memcpy(&value, &raw, sizeof value);
  return value;
}

// A table's bytes with one cursor per column and a global read count (§2.1). The sequential getters
// are the reader's; the section end is the read count the table declares for itself.
class Table {
 public:
  explicit Table(const std::vector<uint8_t>& bytes) : b_(bytes) {}

  const std::string& Error() const { return error_; }

  bool Open() {
    if (b_.size() < kHeaderWords * kWordBytes) return Fail("header truncated");
    uint32_t w[kHeaderWords];
    for (size_t i = 0; i < kHeaderWords; i++) w[i] = Le32(&b_[i * kWordBytes]);
    count_[kF32] = w[0];
    strings_ = w[1];
    sections_ = w[2];
    count_[kU32] = w[3];
    count_[kU16] = w[4];
    count_[kU8] = w[5];
    stringBytes_ = w[6];
    off_[kF32] = w[7];
    strEnd_ = w[8];
    sectionEnd_ = w[9];
    off_[kU32] = w[10];
    off_[kU16] = w[11];
    off_[kU8] = w[12];
    chars_ = w[13];
    if (!Fits(off_[kF32], count_[kF32], kColumnWidth[kF32]) ||
        !Fits(off_[kU32], count_[kU32], kColumnWidth[kU32]) ||
        !Fits(off_[kU16], count_[kU16], kColumnWidth[kU16]) ||
        !Fits(off_[kU8], count_[kU8], kColumnWidth[kU8]) ||
        !Fits(strEnd_, strings_, kWordBytes) || !Fits(chars_, stringBytes_, 1) ||
        !Fits(sectionEnd_, sections_, kWordBytes)) {
      return Fail("a stream lies outside the file");
    }
    if (sections_ < 1) return Fail("no section end");
    return true;
  }

  bool U8(uint8_t* v) {
    uint32_t raw;
    if (!Take(kU8, &raw)) return false;
    *v = uint8_t(raw);
    return true;
  }
  bool U16(uint16_t* v) {
    uint32_t raw;
    if (!Take(kU16, &raw)) return false;
    *v = uint16_t(raw);
    return true;
  }
  bool U32(uint32_t* v) { return Take(kU32, v); }
  bool F32(float* v) {
    uint32_t raw;
    if (!Take(kF32, &raw)) return false;
    *v = FloatOf(raw);
    return true;
  }
  // The next string of the pool: its end is a u32 per string, and its start is the previous end.
  bool Str(std::string* s) {
    if (nextString_ >= strings_) return Fail("string pool read past its end");
    const uint32_t end = Le32(&b_[strEnd_ + size_t(nextString_) * kWordBytes]);
    const uint32_t start =
        nextString_ == 0 ? 0 : Le32(&b_[strEnd_ + size_t(nextString_ - 1) * kWordBytes]);
    if (start > end || end > stringBytes_) return Fail("string bounds outside the pool");
    s->assign(reinterpret_cast<const char*>(b_.data() + chars_ + start), end - start);
    nextString_++;
    reads_++;
    return true;
  }

  // The table ends where it says it does: its declared read count is what was read, and every column
  // and string is used up.
  bool Finished() {
    if (Le32(&b_[sectionEnd_]) != reads_) return Fail("the table does not end at its section end");
    for (int c = 0; c < kColumns; c++) {
      if (pos_[c] != count_[c]) return Fail("a column is not used up");
    }
    if (nextString_ != strings_) return Fail("the string pool is not used up");
    return true;
  }

  bool Fail(const char* what) {
    error_ = what;
    return false;
  }

 private:
  bool Fits(uint32_t off, uint32_t count, size_t width) const {
    return off <= b_.size() && uint64_t(count) * width <= b_.size() - off;
  }
  bool Take(Column c, uint32_t* raw) {
    if (pos_[c] >= count_[c]) return Fail("a column is read past its end");
    const uint8_t* p = &b_[off_[c] + size_t(pos_[c]) * kColumnWidth[c]];
    switch (c) {
      case kF32:
      case kU32:
        *raw = Le32(p);
        break;
      case kU16:
        *raw = uint32_t(p[0]) | uint32_t(p[1]) << 8;
        break;
      default:
        *raw = p[0];
        break;
    }
    pos_[c]++;
    reads_++;
    return true;
  }

  const std::vector<uint8_t>& b_;
  std::string error_;
  uint32_t count_[kColumns] = {};
  uint32_t off_[kColumns] = {};
  uint32_t pos_[kColumns] = {};
  uint32_t strings_ = 0, stringBytes_ = 0, strEnd_ = 0, chars_ = 0;
  uint32_t sections_ = 0, sectionEnd_ = 0;
  uint32_t nextString_ = 0;
  uint32_t reads_ = 0;
};

// T1, the cut library (§2.2): each group is a run of variant rows, and each row lists its .fdc files.
bool ReadCutLibrary(Table& t, CutTable* out, int* version) {
  uint16_t n = 0;
  if (!t.U16(&n)) return false;
  if (n == 1) {  // the version marker: all nine shipped tables carry it
    *version = 1;
    if (!t.U16(&n)) return false;
  }
  std::vector<uint16_t> first(n), count(n);
  for (uint16_t i = 0; i < n; i++) {
    if (!t.U16(&first[i]) || !t.U16(&count[i])) return false;
  }
  for (uint16_t i = 0; i < n; i++) {
    std::string name;
    if (!t.Str(&name)) return false;
  }
  uint16_t rows = 0;
  uint8_t keys = 0, files = 0;
  if (!t.U16(&rows) || !t.U8(&keys) || !t.U8(&files)) return false;
  std::vector<Variant> variants(rows);
  for (uint16_t j = 0; j < rows; j++) {
    for (uint8_t k = 0; k < keys; k++) {
      uint16_t key;  // the condition keys pick the variant; the picker is not ported
      if (!t.U16(&key)) return false;
    }
    for (uint8_t k = 0; k < files; k++) {
      std::string fdc;
      if (!t.Str(&fdc)) return false;
      variants[j].fdc.push_back(fdc);
    }
    uint32_t b0, b1;
    float f;
    if (!t.U32(&b0) || !t.F32(&f) || !t.U32(&b1)) return false;
  }
  out->groups.assign(n, {});
  for (uint16_t i = 0; i < n; i++) {
    if (size_t(first[i]) + count[i] > size_t(rows)) return t.Fail("a cut group lies outside its rows");
    for (uint16_t j = first[i]; j < first[i] + count[i]; j++) out->groups[i].push_back(variants[j]);
  }
  return true;
}

// T2, the scenes (§2.2): each scene is a run of steps, and a step names its cut group.
bool ReadScenes(Table& t, CutTable* out, int version) {
  uint16_t n = 0;
  if (!t.U16(&n)) return false;
  std::vector<uint16_t> first(n), count(n);
  for (uint16_t i = 0; i < n; i++) {
    if (!t.U16(&first[i]) || !t.U16(&count[i])) return false;
  }
  for (uint16_t i = 0; i < n; i++) {
    std::string name;
    if (!t.Str(&name)) return false;
  }
  uint16_t steps = 0;
  if (!t.U16(&steps)) return false;
  std::vector<SceneStep> list(steps);
  for (uint16_t j = 0; j < steps; j++) {
    SceneStep& step = list[j];
    uint8_t c0, c1, triples;
    uint16_t w0, w1;
    if (!t.U8(&step.kind) || !t.F32(&step.fade_s) || !t.U32(&step.cutRef) ||
        !t.U32(&step.flags) || !t.U8(&c0) || !t.U8(&c1) || !t.U16(&w0) || !t.U16(&w1) ||
        !t.U8(&triples)) {
      return false;
    }
    for (uint8_t k = 0; k < triples; k++) {
      uint8_t x, y, z;  // the camera-triplets are not used yet
      if (!t.U8(&x) || !t.U8(&y) || !t.U8(&z)) return false;
    }
    if (version == 1) {
      uint16_t w2;  // the step's length override; the playback does not read it yet
      if (!t.U16(&w2)) return false;
    }
  }
  out->scenes.assign(n, {});
  for (uint16_t i = 0; i < n; i++) {
    if (size_t(first[i]) + count[i] > size_t(steps)) return t.Fail("a scene lies outside its steps");
    Scene& scene = out->scenes[i];
    scene.steps.assign(list.begin() + first[i], list.begin() + first[i] + count[i]);
    if (!scene.steps.empty()) scene.flags = scene.steps.front().flags;
  }
  return true;
}

// The goal-only lists (§2.2): the edit-id scenes, the player-to-scene map, and one GOAL_SCENE_INFO row
// per scene. Nothing reads them yet, but the table must consume them to end where it says.
bool ReadGoalExtras(Table& t, size_t scenes) {
  uint16_t n = 0;
  if (!t.U16(&n)) return false;
  for (uint16_t i = 0; i < n; i++) {
    uint32_t sceneId;
    if (!t.U32(&sceneId)) return false;
  }
  if (!t.U16(&n)) return false;
  for (uint16_t i = 0; i < n; i++) {
    uint32_t playerId, sceneId;
    if (!t.U32(&playerId) || !t.U32(&sceneId)) return false;
  }
  for (size_t i = 0; i < scenes; i++) {
    for (int b = 0; b < 5; b++) {
      uint8_t x;
      if (!t.U8(&x)) return false;
    }
    for (int v = 0; v < 12; v++) {
      float x;
      if (!t.F32(&x)) return false;
    }
    for (int b = 0; b < 4; b++) {
      uint8_t x;
      if (!t.U8(&x)) return false;
    }
  }
  return true;
}

// T3, the goal control groups (§2.2): read to the section end, not used yet.
bool ReadControls(Table& t) {
  uint16_t n = 0;
  if (!t.U16(&n)) return false;
  for (uint32_t i = 0; i < 2u * n; i++) {
    uint16_t x;
    if (!t.U16(&x)) return false;
  }
  for (uint16_t i = 0; i < n; i++) {
    std::string name;
    if (!t.Str(&name)) return false;
  }
  uint16_t rows = 0;
  uint8_t keys = 0;
  if (!t.U16(&rows) || !t.U8(&keys)) return false;
  for (uint16_t j = 0; j < rows; j++) {
    for (uint8_t k = 0; k < keys; k++) {
      uint16_t key;
      if (!t.U16(&key)) return false;
    }
    uint8_t floats = 0, ints = 0;
    if (!t.U8(&floats) || !t.U8(&ints)) return false;
    for (uint8_t k = 0; k < floats; k++) {
      float x;
      if (!t.F32(&x)) return false;
    }
    for (uint8_t k = 0; k < ints; k++) {
      uint32_t x;
      if (!t.U32(&x)) return false;
    }
  }
  return true;
}

// A byte range inside a file or a block of one.
struct Span {
  const uint8_t* data;
  size_t size;
};

bool Inside(const Span& s, uint64_t off, uint64_t len) {
  return off <= s.size && len <= s.size - off;
}

uint32_t WordAt(const Span& s, size_t off) { return Le32(s.data + off); }

// The maxTime, in ticks, of the first Cut record in an .fdc file (§3.1-3.2). The file is `u32 n` and
// n entries {offset, size, nameOffset} from its start, and entry 0 is the cut block. The cut block is
// the same table relative to its own start, and each entry's typeOffset names its record type.
bool CutMaxTimeTicks(const std::vector<uint8_t>& file, float* ticks, std::string* error) {
  const Span whole{file.data(), file.size()};
  if (!Inside(whole, 0, 4) || WordAt(whole, 0) == 0 || !Inside(whole, 4, 12)) {
    *error = "fdc container truncated";
    return false;
  }
  const size_t blockAt = WordAt(whole, 4), blockSize = WordAt(whole, 8);
  if (!Inside(whole, blockAt, blockSize)) {
    *error = "fdc cut block lies outside the file";
    return false;
  }
  const Span block{file.data() + blockAt, blockSize};
  if (!Inside(block, 0, 4)) {
    *error = "fdc cut block truncated";
    return false;
  }
  const uint32_t records = WordAt(block, 0);
  for (uint32_t i = 0; i < records; i++) {
    const size_t entry = 4 + size_t(i) * 12;
    if (!Inside(block, entry, 12)) {
      *error = "fdc cut table truncated";
      return false;
    }
    const size_t offset = WordAt(block, entry), size = WordAt(block, entry + 4);
    const size_t typeAt = WordAt(block, entry + 8);
    if (!Inside(block, typeAt, 1)) {
      *error = "fdc record type lies outside the block";
      return false;
    }
    if (block.data[typeAt] != kCutRecordType) continue;
    if (!Inside(block, offset, size) || size < kMaxTimeOffset + 4) {
      *error = "fdc Cut record lies outside the block";
      return false;
    }
    std::memcpy(ticks, block.data + offset + kMaxTimeOffset, sizeof *ticks);
    return true;
  }
  *error = "fdc has no Cut record";
  return false;
}

}  // namespace

const std::vector<Variant>* CutTable::Group(uint32_t cutRef) const {
  const uint32_t index = cutRef & 0xffffu;  // the low half names the group, 1-based (§2.3)
  if (index == 0 || index > groups.size()) return nullptr;
  return &groups[index - 1];
}

std::optional<CutTable> LoadCutTable(const PesVfs& vfs, const std::string& category,
                                     std::string* error) {
  if (category != "goal") {
    *error = "only the goal table is read so far";
    return std::nullopt;
  }
  const std::string path = std::string(kFixDemoRoot) + category + "/table_" + category + ".bin";
  const auto bytes = vfs.Read(path, error);
  if (!bytes) return std::nullopt;
  Table t(*bytes);
  CutTable table;
  int version = 0;
  const bool parsed = t.Open() && ReadCutLibrary(t, &table, &version) &&
                      ReadScenes(t, &table, version) && ReadGoalExtras(t, table.scenes.size()) &&
                      ReadControls(t) && t.Finished();
  if (!parsed) {
    *error = path + ": " + t.Error();
    return std::nullopt;
  }
  return table;
}

std::optional<float> CutLength_s(const PesVfs& vfs, const std::string& category,
                                 const std::string& fdc, std::string* error) {
  const auto bytes = vfs.Read(std::string(kFixDemoRoot) + category + "/cut_data/" + fdc, error);
  if (!bytes) return std::nullopt;
  float ticks = 0.0f;
  if (!CutMaxTimeTicks(*bytes, &ticks, error)) return std::nullopt;
  return ticks / kTicksPerSecond;
}

std::optional<float> SceneLength_s(const PesVfs& vfs, const std::string& category,
                                   const CutTable& table, size_t sceneIndex, std::string* error) {
  if (sceneIndex >= table.scenes.size()) {
    *error = "no such scene";
    return std::nullopt;
  }
  float total = 0.0f;
  for (const SceneStep& step : table.scenes[sceneIndex].steps) {
    if (step.cutRef == 0) break;  // the terminator (§2.3)
    const std::vector<Variant>* group = table.Group(step.cutRef);
    if (group == nullptr || group->empty() || group->front().fdc.empty()) {
      *error = "a scene step names no cut";
      return std::nullopt;
    }
    // ponytail: the variant is the group's first row. PES's matcher picks one by the match's
    // condition keys (pes-re-cutscenes.md §4.2), which is not ported.
    const std::optional<float> cut = CutLength_s(vfs, category, group->front().fdc.front(), error);
    if (!cut) return std::nullopt;
    total += *cut + step.fade_s;
  }
  return total;
}

std::optional<GoalScenes> LoadGoalScenes(const PesVfs& vfs, std::string* error) {
  const std::optional<CutTable> table = LoadCutTable(vfs, "goal", error);
  if (!table) return std::nullopt;
  if (table->scenes.empty()) {
    *error = "the goal table has no scenes";
    return std::nullopt;
  }
  GoalScenes out;
  for (size_t i = 0; i < table->scenes.size(); i++) {
    const Scene& scene = table->scenes[i];
    const bool generic = ((scene.flags >> kGenericPoolBit) & 1u) != 0;
    const bool hug = ((scene.flags >> kHugPoolBitA) & 1u) != 0 || ((scene.flags >> kHugPoolBitB) & 1u) != 0;
    if (!generic && !hug && i != 0) continue;
    const std::optional<float> length = SceneLength_s(vfs, "goal", *table, i, error);
    if (!length) return std::nullopt;
    const GoalScene goal{int(i), scene.flags, *length};
    if (i == 0) out.fallback = goal;
    if (generic && *length > 0.0f) out.generic.push_back(goal);
    if (hug && *length > 0.0f) out.hug.push_back(goal);
  }
  return out;
}

}  // namespace pes::fixdemo
