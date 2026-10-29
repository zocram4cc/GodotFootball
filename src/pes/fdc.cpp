#include "pes/fdc.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

namespace pes::fdc {
namespace {

constexpr size_t kNameWindow = 512;  // the name read at nameOffset is at most this long (as fdc_parse.py)
constexpr char kVfsPrefix[] = "cpk_dat/";
constexpr char kFixDemoDir[] = "common/demo/fixdemo/";
constexpr uint8_t kCanmMagic[4] = {0x01, 0x00, 0x01, 0xff};
constexpr uint64_t kSinTableVa = 0x142655950;  // the table 0x140A92C80 indexes (lea at 0x140A92D04)

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
  c.timeMode = 0x9c < n ? r[0x9c] : uint8_t(0);
  c.loop = 0x9d < n ? r[0x9d] : uint8_t(0);
  c.useCutOffset = 0xa6 < n ? r[0xa6] : uint8_t(0);
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

// A PES float the executable's code loads RIP-relative: `[prefix] [REX] 0F op modrm(mod 0, rm 5) disp32`
// at `instr`, whose operand VA must be exactly `operand`. The opcode and the operand are checked before
// the float is read from the image, so a different executable build fails instead of misreading.
struct ConstSite {
  float PoseConstants::*field;
  uint64_t instr;
  uint8_t prefix;  // 0xF3 for the scalar-single ops, 0 for comiss
  uint8_t op;      // 0x10 movss, 0x59 mulss, 0x5C subss, 0x5E divss, 0x2F comiss
  uint64_t operand;
};
constexpr ConstSite kConstSites[] = {
    {&PoseConstants::defaultFps, 0x142139E30, 0xF3, 0x10, 0x142591140},
    {&PoseConstants::pi, 0x142139B67, 0xF3, 0x10, 0x142578480},
    {&PoseConstants::oneEighty, 0x142139B70, 0xF3, 0x10, 0x142598580},
    {&PoseConstants::sixty, 0x141F7B51A, 0xF3, 0x5E, 0x14259A764},
    {&PoseConstants::one, 0x140A93299, 0xF3, 0x10, 0x14256AE1C},
    {&PoseConstants::two, 0x140A91D24, 0xF3, 0x10, 0x142565604},
    {&PoseConstants::half, 0x141F7B2A6, 0xF3, 0x59, 0x142565600},
    {&PoseConstants::minusOne, 0x141F7B87D, 0xF3, 0x10, 0x142588218},
    {&PoseConstants::degToRad, 0x141F7B8FC, 0xF3, 0x59, 0x1425A0C48},
    {&PoseConstants::snapLow, 0x14213A184, 0xF3, 0x10, 0x1425A9668},
    {&PoseConstants::snapHigh, 0x14213A1AC, 0x00, 0x2F, 0x1425E4D78},
    {&PoseConstants::slerpLinearEps, 0x140A932AA, 0x00, 0x2F, 0x1425568D0},
    {&PoseConstants::heightRef, 0x141F39F10, 0xF3, 0x5C, 0x142598580},
    {&PoseConstants::heightScale, 0x141F39F18, 0xF3, 0x59, 0x14256AE18},
    {&PoseConstants::renderFarFloor, 0x141EAF914, 0xF3, 0x10, 0x1425CB498},
    {&PoseConstants::fovMaxRad, 0x141EAF60C, 0xF3, 0x10, 0x142B672C8},
    {&PoseConstants::wideAspect, 0x141F7B294, 0xF3, 0x10, 0x142609394},
    {&PoseConstants::radToDeg, 0x140A91E7A, 0xF3, 0x59, 0x14259BE40},
    {&PoseConstants::quarterTurnDeg, 0x140A91E99, 0xF3, 0x58, 0x14259BE5C},
    {&PoseConstants::fullTurnDeg, 0x141F7B72F, 0xF3, 0x58, 0x14259858C},
};

bool ReadConstSite(const PeImage& exe, const ConstSite& s, float* out, std::string* err) {
  auto fail = [&](const char* what) {
    if (err) {
      char va[40];
      std::snprintf(va, sizeof va, "%#llx", static_cast<unsigned long long>(s.instr));
      *err = std::string("camera pose constants: ") + what + " at " + va;
    }
    return false;
  };
  uint8_t code[16] = {};
  if (!exe.ReadVa(s.instr, sizeof code, code)) return fail("unreadable code");
  size_t i = 0;
  if (s.prefix && code[i++] != s.prefix) return fail("unexpected prefix");
  if ((code[i] & 0xF0) == 0x40) ++i;  // REX, for xmm8 and up
  if (code[i++] != 0x0F || code[i++] != s.op) return fail("unexpected opcode");
  if ((code[i++] & 0xC7) != 0x05) return fail("operand is not RIP-relative");
  int32_t disp;
  std::memcpy(&disp, code + i, 4);
  i += 4;
  if (s.instr + i + int64_t(disp) != s.operand) return fail("unexpected operand");
  if (!exe.ReadVa(s.operand, 4, reinterpret_cast<uint8_t*>(out))) return fail("unreadable operand");
  return true;
}

// Key segment of a frame in a channel (ktAnimationBase::Sample 0x142139EB0, key search 0x14213A210):
// the last key with time <= the integer frame, advanced past equal times; f is the float frame's position
// between the segment's key times (not clamped: before the first key it is negative).
struct Segment {
  size_t lo, hi;
  float f;
};

Segment Locate(const Canm::Channel& ch, float frame, int iframe) {
  const auto& t = ch.frames;
  const size_t last = t.size() - 1;
  size_t a = size_t(std::upper_bound(t.begin(), t.end(), iframe, [](int v, uint16_t key) { return v < int(key); }) - t.begin());
  a = a ? a - 1 : 0;
  const uint16_t t0 = t[a];
  for (;;) {
    if (a >= last) return {a, a, 0.0f};
    const uint16_t t1 = t[a + 1];
    if (t1 != t0) return {a, a + 1, (frame - float(t0)) / float(int(t1) - int(t0))};
    ++a;
  }
}

// PES computes the angle with acosf/sinf (CRT thunks 0x1415A1798/0x1415A1792). ponytail: the UCRT float
// functions are taken as the double function rounded to float (INF, not verified); glibc's sinf may
// differ in the last bit. Replace if a recorded PES call disagrees.
float AcosF(float x) { return float(std::acos(double(x))); }
float SinF(float x) { return float(std::sin(double(x))); }

// ktQuaternion slerp 0x140A93210: shortest arc by negating the weight; weights stay linear when
// 1 - dot is at most the threshold; the output is not normalised.
std::array<float, 4> Slerp(const PoseConstants& k, const std::array<float, 4>& q0, const std::array<float, 4>& q1, float t) {
  float d = q1[0] * q0[0] + q1[1] * q0[1] + q0[2] * q1[2] + q0[3] * q1[3];
  float sign = k.one;
  if (d < 0) {
    d = -d;
    sign = -k.one;
  }
  float w0, w1;
  if (k.one - d <= k.slerpLinearEps) {
    w0 = k.one - t;
    w1 = t;
  } else {
    const float th = AcosF(d);
    const float inv = k.one / SinF(th);
    w0 = SinF((k.one - t) * th) * inv;
    w1 = SinF(th * t) * inv;
  }
  w1 = w1 * sign;
  std::array<float, 4> o{};
  for (size_t i = 0; i < 4; ++i) o[i] = w0 * q0[i] + w1 * q1[i];
  return o;
}

// 0x140A91CA0 column-vector rotation matrix of a possibly unnormalised quaternion (s = 2/|q|^2), then
// columns 0 and 2 negated (0x142139CAB..CE8).
Mat3 CameraMatrix(const PoseConstants& k, const std::array<float, 4>& q) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  const float n = x * x + y * y + z * z + w * w;
  const float s = n > 0 ? k.two / n : 0.0f;
  Mat3 m{{{k.one - (z * z * s + y * y * s), x * y * s - w * z * s, w * y * s + z * x * s},
          {w * z * s + x * y * s, k.one - (z * z * s + x * x * s), z * y * s - w * x * s},
          {z * x * s - w * y * s, w * x * s + z * y * s, k.one - (y * y * s + x * x * s)}}};
  for (auto& row : m) {
    row[0] = -row[0];
    row[2] = -row[2];
  }
  return m;
}

// 0x140A92C80 (translated, ball/pes_fn_140A92C80): sin of `deg` degrees times the amplitude in xmm1, by
// linear interpolation in the 513-entry quarter-wave table of `k`.
float SinTableDeg(const PoseConstants& k, float deg, float amp) {
  PesState s;
  s.mem.window = {kSinTableVa, sizeof k.sinTable, reinterpret_cast<const uint8_t*>(k.sinTable.data()), nullptr};
  s.set_x_f32(0, deg);
  s.set_x_f32(1, amp);
  ::pes_fn_140A92C80(s);
  return s.x_f32(0);
}

// FUN_140A91810: out = a * b, each element summed in the order k = 0, 1, 2.
Mat3 Mul(const Mat3& a, const Mat3& b) {
  Mat3 o{};
  for (size_t r = 0; r < 3; ++r)
    for (size_t c = 0; c < 3; ++c) o[r][c] = a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c];
  return o;
}

// FUN_140A91A30: a * p.
std::array<float, 3> Mul(const Mat3& a, const std::array<float, 3>& p) {
  std::array<float, 3> o{};
  for (size_t r = 0; r < 3; ++r) o[r] = a[r][0] * p[0] + a[r][1] * p[1] + a[r][2] * p[2];
  return o;
}

Canm::Kind KindOfType(uint16_t type, bool* ok) {
  *ok = true;
  switch (type) {  // table 0x143542620: 1, 3 rotation; 4, 5, 6, 10 position; 8 scalar
    case 1:
    case 3: return Canm::Kind::kQuat;
    case 4:
    case 5:
    case 6:
    case 10: return Canm::Kind::kVec;
    case 8: return Canm::Kind::kScalar;
    default: *ok = false; return Canm::Kind::kScalar;
  }
}

}  // namespace

std::optional<PoseConstants> LoadPoseConstants(const PeImage& exe, std::string* err) {
  PoseConstants k;
  for (const ConstSite& s : kConstSites)
    if (!ReadConstSite(exe, s, &(k.*s.field), err)) return std::nullopt;
  // 0x140A92D04 `lea rdx, [rip + disp]` (48 8D 15) names the table 0x140A92C80 indexes; 513 floats.
  constexpr uint64_t kLea = 0x140A92D04;
  uint8_t lea[7] = {};
  int32_t disp = 0;
  const bool ok = exe.ReadVa(kLea, sizeof lea, lea) && lea[0] == 0x48 && lea[1] == 0x8D && lea[2] == 0x15 &&
                  (std::memcpy(&disp, lea + 3, 4), kLea + sizeof lea + int64_t(disp) == kSinTableVa) &&
                  exe.ReadVa(kSinTableVa, sizeof k.sinTable, reinterpret_cast<uint8_t*>(k.sinTable.data()));
  if (!ok) {
    if (err) *err = "camera pose constants: the sine table of 0x140A92C80 is unreadable or moved";
    return std::nullopt;
  }
  return k;
}

// FUN_140A91E60, axis (0, 1, 0): the element order of the builder's stores (x*x*omc + c, ...).
Mat3 RotYMatrix(const PoseConstants& k, float rotYDeg) {
  const float deg = (rotYDeg * k.degToRad) * k.radToDeg;
  const float c = SinTableDeg(k, deg + k.quarterTurnDeg, k.one), s = SinTableDeg(k, deg, k.one);
  const float x = 0.0f, y = k.one, z = 0.0f, omc = k.one - c;
  return {{{x * x * omc + c, x * y * omc - s * z, x * z * omc + s * y},
           {x * y * omc + s * z, y * y * omc + c, y * z * omc - s * x},
           {x * z * omc - s * y, y * z * omc + s * x, z * z * omc + c}}};
}

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
  // ktCameraAnimation::SetData 0x142139D60: length (float)s16(+4); fps i32(+8) when bit 0 of byte +0xC.
  out->length = float(int16_t(Le16(b.data() + 4)));
  out->hasFps = (b[12] & 1) != 0;
  out->fps = int32_t(Le32(b.data() + 8));
  const uint64_t track = Le32(b.data() + 16);
  if (track + 8 > len) return fail("track header past the end");
  const uint64_t chCount = Le16(b.data() + track);
  if (chCount == 0 || track + 8 + 4 * chCount > len) return fail("channel table past the end");
  std::array<bool, 6> seen{};
  for (uint64_t c = 0; c < chCount; ++c) {
    // Channel offsets are relative to the track.
    const uint64_t base = track + Le32(b.data() + track + 8 + 4 * c);
    if (base + 16 > len) return fail("channel header past the end");
    const uint16_t type = Le16(b.data() + base);
    const uint64_t keys = Le16(b.data() + base + 4), index = Le16(b.data() + base + 6);
    const uint64_t timesOff = Le32(b.data() + base + 8), valuesOff = Le32(b.data() + base + 12);
    bool known;
    const Canm::Kind kind = KindOfType(type, &known);
    if (!known) return fail("unknown channel type");
    if (index >= out->channels.size()) return fail("channel index past six");
    // The semantic fixes the kind: 0 rotation, 1 position, 2..5 scalars.
    if (kind != (index == 0 ? Canm::Kind::kQuat : index == 1 ? Canm::Kind::kVec : Canm::Kind::kScalar))
      return fail("channel type does not suit its semantic");
    if (keys == 0) return fail("channel without keys");
    const size_t ncomp = kind == Canm::Kind::kScalar ? 1 : 4;
    if ((timesOff != 0 && base + timesOff + 2 * keys > len) || base + valuesOff + 4 * ncomp * keys > len)
      return fail("channel keys past the end");
    Canm::Channel ch;
    ch.kind = kind;
    for (uint64_t k = 0; k < keys; ++k) {
      ch.frames.push_back(timesOff != 0 ? Le16(b.data() + base + timesOff + 2 * k) : uint16_t(k));
      std::array<float, 4> v{};
      for (size_t i = 0; i < ncomp; ++i) v[i] = LeF32(b.data() + base + valuesOff + 4 * ncomp * k + 4 * i);
      ch.values.push_back(v);
    }
    out->channels[index] = std::move(ch);
    seen[index] = true;
  }
  for (bool s : seen)
    if (!s) return fail("a channel is missing");
  return true;
}

CameraSample SampleCanm(const PoseConstants& k, const Canm& c, float frame) {
  // 0x142139AB0, not looping (+0xB0 is never set): clamp, then the integer frame is floorf.
  frame = frame > c.length ? c.length : std::max(0.0f, frame);
  const int iframe = int(std::floor(frame));
  CameraSample s;
  {
    const auto& ch = c.channels[0];
    const Segment g = Locate(ch, frame, iframe);
    // 0x14213A180: snap to the end keys near f = 0 and f = 1, else slerp.
    s.rotation = g.f < k.snapLow ? ch.values[g.lo] : g.f > k.snapHigh ? ch.values[g.hi] : Slerp(k, ch.values[g.lo], ch.values[g.hi], g.f);
  }
  {
    const auto& ch = c.channels[1];
    const Segment g = Locate(ch, frame, iframe);
    for (size_t i = 0; i < 3; ++i) s.position[i] = g.f * ch.values[g.hi][i] + (k.one - g.f) * ch.values[g.lo][i];  // 0x14213A110
  }
  {
    const auto& ch = c.channels[2];
    const Segment g = Locate(ch, frame, iframe);
    const float deg = (k.one - g.f) * ch.values[g.lo][0] + g.f * ch.values[g.hi][0];  // 0x14213A0E0
    s.fovRad = deg * k.pi / k.oneEighty;                                             // 0x142139C19, 0x142139C22
  }
  return s;
}

float ClipFps(const PoseConstants& k, const Canm& c) { return c.hasFps ? float(c.fps) : k.defaultFps; }

float ClipFrame(const PoseConstants& k, const Canm& c, const CameraRecord& rec, float clockTicks) {
  float t = rec.timeMode == 0 ? clockTicks : std::max(0.0f, clockTicks - rec.startTime);  // 0x141F421BB
  t = t + rec.animeStartTime;                                                                 // 0x141F7B4FC
  float frame = t * (ClipFps(k, c) / k.sixty);                                                // 0x141F7B51A
  if (rec.loop != 0 && frame > c.length && int(frame / c.length) != 0) frame = frame - float(int(frame / c.length)) * c.length;
  return frame;
}

float HeightOffsetM(const PoseConstants& k, float targetHeightCm, float correctRateForHeight) {
  return ((targetHeightCm - k.heightRef) * k.heightScale) * correctRateForHeight;
}

CameraPose EvaluateCameraPose(const PoseConstants& k, const Canm& c, const CameraRecord& rec, float clockTicks,
                              const CutOffset& playerOffset, float heightOffsetM) {
  CameraPose p;
  p.frame = ClipFrame(k, c, rec, clockTicks);
  const CameraSample s = SampleCanm(k, c, p.frame);
  p.fovRad = s.fovRad;
  p.m = CameraMatrix(k, s.rotation);
  p.eye = s.position;
  p.eye[1] = p.eye[1] + heightOffsetM;  // 0x141F7B866
  CutOffset off = rec.useCutOffset != 0 ? playerOffset : CutOffset{};
  // SetCutOffset (0x141F7B700) keeps rotY wrapped once into [0, 360).
  if (off.rotYDeg < 0.0f)
    off.rotYDeg = off.rotYDeg + k.fullTurnDeg;
  else if (off.rotYDeg >= k.fullTurnDeg)
    off.rotYDeg = off.rotYDeg - k.fullTurnDeg;
  if (off.flip) {  // 0x141F7B87D: m01, m02, m10, m20 and x times -1
    p.m[0][1] *= k.minusOne;
    p.m[0][2] *= k.minusOne;
    p.m[1][0] *= k.minusOne;
    p.m[2][0] *= k.minusOne;
    p.eye[0] *= k.minusOne;
  }
  if (off.rotYDeg != 0.0f) {  // 0x141F7B8FC: Ry * M and Ry * p
    const Mat3 ry = RotYMatrix(k, off.rotYDeg);
    p.m = Mul(ry, p.m);
    p.eye = Mul(ry, p.eye);
  }
  for (size_t i = 0; i < 3; ++i) p.eye[i] += off.pos[i];  // 0x141F7B9BF/9B7/9C7
  return p;
}

RenderCamera RenderCameraParams(const PoseConstants& k, const CameraPose& pose, const CameraRecord& rec,
                                float displayAspect, bool widescreenFix) {
  float fov = pose.fovRad;
  if (widescreenFix && displayAspect < k.wideAspect) {  // 0x141F7B250
    const float t = float(std::tan(double(fov * k.half)));
    fov = float(std::atan(double(t * k.wideAspect * (k.one / displayAspect)))) * k.two;
  }
  RenderCamera r;
  r.fovRad = fov <= k.fovMaxRad ? (0.0f <= fov ? fov : 0.0f) : k.fovMaxRad;  // 0x141EAF5D0
  r.nearM = rec.nearClip;                                                    // 0x141EAF903
  r.farM = k.renderFarFloor > rec.farClip ? k.renderFarFloor : rec.farClip;   // 0x141EAF914/91C maxss
  return r;
}

}  // namespace pes::fdc
