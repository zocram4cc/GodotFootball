#include "pes/fdc.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pes::fdc {
namespace {

constexpr size_t kNameWindow = 512;  // the name read at nameOffset is at most this long (as fdc_parse.py)
constexpr char kVfsPrefix[] = "cpk_dat/";
constexpr char kFixDemoDir[] = "common/demo/fixdemo/";
constexpr uint8_t kCanmMagic[4] = {0x01, 0x00, 0x01, 0xff};

uint32_t Le32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint16_t Le16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
float LeF32(const uint8_t* p) {
  const uint32_t u = Le32(p);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

// The NUL-terminated string in the n-byte record r at off, at most max bytes.
std::string Str(const uint8_t* r, size_t n, size_t off, size_t max) {
  if (off >= n) return {};
  const size_t lim = std::min(max, n - off);
  size_t end = 0;
  while (end < lim && r[off + end] != 0) ++end;
  return std::string(reinterpret_cast<const char*>(r + off), end);
}

// Fields past the record's end read as zero, so a short record decodes without reading past it.
CameraRecord DecodeCamera(const uint8_t* r, size_t n) {
  auto u32 = [&](size_t off) { return off + 4 <= n ? Le32(r + off) : 0u; };
  auto f32 = [&](size_t off) { return off + 4 <= n ? LeF32(r + off) : 0.0f; };
  CameraRecord c;
  c.key = u32(0x00);
  c.startTime = f32(0x04);
  c.originalCameraId = 0x08 + 2 <= n ? Le16(r + 0x08) : uint16_t(0);
  c.envDataId = 0x0a + 2 <= n ? Le16(r + 0x0a) : uint16_t(0);
  c.animePath = Str(r, n, 0x0c, 128);
  c.animeStartTime = f32(0x8c);
  c.nearClip = f32(0x90);
  c.farClip = f32(0x94);
  c.nearClipScale = f32(0x98);
  c.correctRateForHeight = f32(0xa0);
  c.functionNo = u32(0xa8);
  for (size_t i = 0; i < 4; ++i)
    if (0xb0 + i < n) c.targetBytes[i] = r[0xb0 + i];
  c.zoomingTimeRate = u32(0xb4);
  for (size_t i = 0; i < 8; ++i) c.zoom[i] = int32_t(u32(0xb8 + 4 * i));
  return c;
}

TimingRecord DecodeTiming(const uint8_t* r, size_t n) {
  auto u32 = [&](size_t off) { return off + 4 <= n ? Le32(r + off) : 0u; };
  auto f32 = [&](size_t off) { return off + 4 <= n ? LeF32(r + off) : 0.0f; };
  TimingRecord t;
  t.word0 = u32(0x00);
  t.startTime = f32(0x04);
  t.endTime = f32(0x08);
  t.targetNo = int32_t(u32(0x0c));
  t.paramNo1 = int32_t(u32(0x10));
  t.paramNo2 = int32_t(u32(0x14));
  t.value = f32(0x18);
  t.actionName = Str(r, n, 0x1c, 64);
  return t;
}

std::string StripVfsPrefix(const std::string& s) {
  constexpr size_t kLen = sizeof(kVfsPrefix) - 1;
  return s.rfind(kVfsPrefix, 0) == 0 ? s.substr(kLen) : s;
}

// Index pair around a frame, and the blend weight from the first to the second.
struct KeyPair {
  size_t lo, hi;
  float t;
};

KeyPair Locate(const std::vector<uint16_t>& frames, float f) {
  const size_t last = frames.size() - 1;
  if (f <= float(frames.front())) return {0, 0, 0.0f};
  if (f >= float(frames[last])) return {last, last, 0.0f};
  size_t lo = 0, hi = last;
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) / 2;
    if (float(frames[mid]) <= f) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  const float f0 = float(frames[lo]), f1 = float(frames[hi]);
  return {lo, hi, f1 == f0 ? 0.0f : (f - f0) / (f1 - f0)};
}

std::array<float, 4> Lerp4(const std::array<float, 4>& a, const std::array<float, 4>& b, float t) {
  std::array<float, 4> o{};
  for (size_t i = 0; i < 4; ++i) o[i] = a[i] + (b[i] - a[i]) * t;
  return o;
}

std::array<float, 4> Channel(const Canm& c, size_t idx, float f) {
  const auto& ch = c.channels[idx];
  const KeyPair p = Locate(ch.frames, f);
  return Lerp4(ch.values[p.lo], ch.values[p.hi], p.t);
}

std::array<float, 4> Normalize(std::array<float, 4> q) {
  const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (n > 0) {
    for (float& v : q) v /= n;
  }
  return q;
}

// Shortest-arc slerp; nlerp when the keys are nearly equal (the sin of the angle vanishes).
std::array<float, 4> Slerp(std::array<float, 4> a, std::array<float, 4> b, float t) {
  float d = 0;
  for (size_t i = 0; i < 4; ++i) d += a[i] * b[i];
  if (d < 0) {
    for (float& v : b) v = -v;
    d = -d;
  }
  if (d > 0.9995f) return Normalize(Lerp4(a, b, t));
  const float th = std::acos(d);
  const float s = std::sin(th);
  const float w0 = std::sin((1.0f - t) * th) / s, w1 = std::sin(t * th) / s;
  std::array<float, 4> o{};
  for (size_t i = 0; i < 4; ++i) o[i] = a[i] * w0 + b[i] * w1;
  return Normalize(o);
}

}  // namespace

uint32_t NominalSize(uint8_t type) {
  switch (type) {
    case kCut: return 64;
    case kEnv: return 156;
    case kTiming: return 92;
    case kEffect: return 188;
    case kHuman: return 364;
    case kObject: return 444;
    case kCamera: return 284;
    case kFunction: return 324;
    case kHumanControl: return 96;
    case kDisabled: return 444;
    default: return 0;
  }
}

bool File::HasRecordType(uint8_t type) const {
  return std::find(recordTypes.begin(), recordTypes.end(), type) != recordTypes.end();
}

const Resource* File::FindResource(const std::string& path) const {
  const std::string want = StripVfsPrefix(path);
  for (const Resource& r : resources)
    if (StripVfsPrefix(r.name) == want) return &r;
  return nullptr;
}

bool Parse(const std::vector<uint8_t>& b, File* out, std::string* err) {
  auto fail = [&](const std::string& msg) {
    if (err) *err = msg;
    return false;
  };
  *out = File{};
  const uint64_t len = b.size();
  if (len < 4) return fail("shorter than the 4-byte entry count");
  const uint64_t n = Le32(b.data());
  const uint64_t hdrEnd = 4 + 12 * n;
  if (hdrEnd > len) return fail("entry table past the end of the file");
  if (n == 0) return fail("no entries");

  // Every structure claims a byte range; the coverage check counts bytes claimed twice or not at all.
  struct Claim {
    uint64_t lo, hi;
  };
  std::vector<Claim> claims;
  auto claim = [&](uint64_t lo, uint64_t hi) {
    if (hi > lo) claims.push_back({lo, hi});
  };
  claim(0, hdrEnd);

  struct Entry {
    uint64_t off, size;
    std::string name;
  };
  std::vector<Entry> entries;
  for (uint64_t i = 0; i < n; ++i) {
    const uint8_t* e = b.data() + 4 + 12 * i;
    const uint64_t off = Le32(e), size = Le32(e + 4), nameOff = Le32(e + 8);
    if (off + size > len) return fail("entry region past the end of the file");
    if (nameOff > len) return fail("name past the end of the file");
    const uint64_t window = std::min<uint64_t>(kNameWindow, len - nameOff);
    uint64_t nlen = 0;
    while (nlen < window && b[nameOff + nlen] != 0) ++nlen;
    if (nameOff + nlen >= len || b[nameOff + nlen] != 0) return fail("unterminated name");
    claim(nameOff, nameOff + nlen + 1);
    entries.push_back({off, size, std::string(b.begin() + nameOff, b.begin() + nameOff + nlen)});
  }

  out->name = entries[0].name;
  for (uint64_t i = 1; i < n; ++i) {
    const Entry& e = entries[i];
    claim(e.off, e.off + e.size);
    Resource r;
    r.name = e.name;
    if (e.size == 4) {
      r.external = true;
      r.vfsPath = StripVfsPrefix(e.name);
    } else {
      r.data.assign(b.begin() + e.off, b.begin() + e.off + e.size);
    }
    out->resources.push_back(std::move(r));
  }

  // The cut block: m, then m rows of { offset, size, typeOffset } relative to the block; the type
  // byte sits at the block's pool, and the record is its own slice of the block.
  const uint64_t base = entries[0].off, cutSize = entries[0].size;
  if (base + 4 > len) return fail("cut block header past the end of the file");
  const uint64_t m = Le32(b.data() + base);
  const uint64_t recHdrEnd = base + 4 + 12 * m;
  if (recHdrEnd > base + cutSize) return fail("cut block table past its size");
  claim(base, recHdrEnd);
  std::vector<uint64_t> refs;
  for (uint64_t i = 0; i < m; ++i) {
    const uint8_t* row = b.data() + base + 4 + 12 * i;
    const uint64_t off = Le32(row), size = Le32(row + 4), to = Le32(row + 8);
    if (to + 1 > cutSize || off + size > cutSize) return fail("record outside the cut block");
    const uint8_t t = b[base + to];
    claim(base + to, base + to + 1);
    claim(base + off, base + off + size);
    refs.push_back(base + to);
    if (NominalSize(t) == 0) return fail("record with unknown type " + std::to_string(int(t)));
    out->recordTypes.push_back(t);
    const uint8_t* r = b.data() + base + off;
    if (t == kCamera) out->cameras.push_back(DecodeCamera(r, size));
    if (t == kTiming) out->timings.push_back(DecodeTiming(r, size));
  }

  std::vector<uint8_t> owned(len, 0);
  size_t overlaps = 0;
  for (const Claim& c : claims)
    for (uint64_t k = c.lo; k < c.hi; ++k) {
      if (owned[k]) ++overlaps;
      owned[k] = 1;
    }

  // Unclaimed bytes are padding only when they are zero, unreferenced, and in the pool tail of the
  // cut block (from the first type byte to the block's declared end). Any other unclaimed byte is a gap.
  const uint64_t blockEnd = base + cutSize;
  std::vector<uint8_t> isRef(len, 0);
  uint64_t poolStart = blockEnd;
  for (uint64_t r : refs) {
    isRef[r] = 1;
    poolStart = std::min(poolStart, r);
  }
  Coverage cov;
  cov.overlapBytes = overlaps;
  for (uint64_t k = 0; k < len; ++k) {
    if (owned[k]) continue;
    if (!isRef[k] && poolStart <= k && k < blockEnd && b[k] == 0) {
      ++cov.padBytes;
    } else {
      ++cov.gapBytes;
      if (cov.gapOffsets.size() < 8) cov.gapOffsets.push_back(size_t(k));
    }
  }
  out->coverage = cov;
  return true;
}

std::vector<std::string> ListCutFiles(const PesVfs& vfs) {
  std::vector<std::string> out;
  for (const std::string& p : vfs.List(kFixDemoDir)) {
    const std::string f = PesVfs::Fold(p);
    if (f.find("/cut_data/") != std::string::npos && f.size() >= 4 && f.compare(f.size() - 4, 4, ".fdc") == 0)
      out.push_back(p);
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool ParseCanm(const std::vector<uint8_t>& b, Canm* out, std::string* err) {
  auto fail = [&](const std::string& msg) {
    if (err) *err = msg;
    return false;
  };
  *out = Canm{};
  const uint64_t len = b.size();
  if (len < 0x18 || std::memcmp(b.data(), kCanmMagic, 4) != 0) return fail("not a canm stream");
  if (Le16(b.data()) != 1) return fail("unsupported canm version");
  if (Le32(b.data() + 12) != 1) return fail("expected one track");
  out->frameCount = Le32(b.data() + 4);
  out->frameRate = Le32(b.data() + 8);
  const uint64_t track = Le32(b.data() + 16);
  if (track + 8 > len) return fail("track header past the end");
  const uint64_t chCount = Le16(b.data() + track);
  if (chCount == 0 || track + 8 + 4 * chCount > len) return fail("channel table past the end");
  for (uint64_t c = 0; c < chCount; ++c) {
    // Channel offsets are relative to the track.
    const uint64_t base = track + Le32(b.data() + track + 8 + 4 * c);
    if (base + 16 > len) return fail("channel header past the end");
    const uint32_t type = Le32(b.data() + base);
    const uint64_t keys = Le16(b.data() + base + 4), index = Le16(b.data() + base + 6);
    const uint64_t timesOff = Le32(b.data() + base + 8), valuesOff = Le32(b.data() + base + 12);
    if (type != 1 && type != 4 && type != 8) return fail("unknown channel type");
    if (index >= out->channels.size()) return fail("channel index past six");
    if (keys == 0) return fail("channel without keys");
    const size_t ncomp = type == 8 ? 1 : 4;
    if (base + timesOff + 2 * keys > len || base + valuesOff + 4 * ncomp * keys > len)
      return fail("channel keys past the end");
    Canm::Channel ch;
    for (uint64_t k = 0; k < keys; ++k) {
      ch.frames.push_back(Le16(b.data() + base + timesOff + 2 * k));
      std::array<float, 4> v{};
      for (size_t i = 0; i < ncomp; ++i) v[i] = LeF32(b.data() + base + valuesOff + 4 * ncomp * k + 4 * i);
      ch.values.push_back(v);
    }
    out->channels[index] = std::move(ch);
  }
  for (const auto& ch : out->channels)
    if (ch.frames.empty()) return fail("a channel is missing");
  return true;
}

CameraSample SampleCanm(const Canm& c, float frame) {
  CameraSample s;
  const auto pos = Channel(c, 1, frame);
  s.position = {pos[0], pos[1], pos[2]};
  s.fovDeg = Channel(c, 2, frame)[0];
  s.nearM = Channel(c, 3, frame)[0];
  s.farM = Channel(c, 4, frame)[0];
  s.aspect = Channel(c, 5, frame)[0];
  const auto& rot = c.channels[0];
  const KeyPair p = Locate(rot.frames, frame);
  s.rotation = Slerp(rot.values[p.lo], rot.values[p.hi], p.t);
  return s;
}

}  // namespace pes::fdc
