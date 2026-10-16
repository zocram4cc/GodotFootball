#include "pes/mbinfo.hpp"

#include <algorithm>
#include <cstring>

namespace pes {
namespace {

constexpr const char* ANIMATION_PATH = "common/anime/Mbinfo/bin/Animation.bin";
constexpr const char* HIT_DATA_PATH = "common/anime/Mbinfo/bin/HitData.bin";
constexpr const char* CANCEL_DATA_PATH = "common/anime/Mbinfo/bin/CancelData.bin";
constexpr const char* SUB_HIT_DATA_PATH = "common/anime/Mbinfo/bin/SubHitData.bin";
constexpr const char* ROT_DATA_PATH = "common/anime/Mbinfo/bin/RotData.bin";
constexpr const char* FOOT_TYPE_PATH = "common/anime/Mbinfo/bin/FootType.bin";
constexpr const char* PIVOTHIT_INFO_PATH = "common/anime/Mbinfo/bin/PivothitInfo.bin";

// Record sizes (animselect 2.2; HitData and CancelData measured on the files:
// 315,900 = 2,925 x 108 and 57,828 = 4,819 x 12, each record opening with a
// motion-number word). The motion table's sizes are PES's builders' divisors.
constexpr size_t ANIMATION_RECORD = 36;
constexpr size_t HIT_RECORD = 108;
constexpr size_t CANCEL_RECORD = 12;
constexpr size_t SUB_HIT_RECORD = 32;   // FUN_1406ae670: size >> 5
constexpr size_t ROT_RECORD = 8;        // FUN_1406ae4f0: size >> 3
constexpr size_t FOOT_RECORD = 4;       // FUN_1406ad170: size >> 2
constexpr size_t PIVOT_RECORD = 16;     // FUN_1406ae350: size >> 4

// Shared word layout: bits 0-12 motion number.
constexpr uint32_t MOTION_MASK = 0x1fff;
constexpr int FRAME_SHIFT = 13;
constexpr uint32_t HIT_FRAME_MASK = 0xff;      // bits 13..20
constexpr int HIT_PART_SHIFT = 27;             // bits 27..31, stored - 1
constexpr uint32_t CANCEL_FRAME_MASK = 0x3ff;  // bits 13..22
constexpr int BLEND_START_SHIFT = 10;          // u32 b bits 10..19
constexpr uint32_t BLEND_START_MASK = 0x3ff;

// HitData field offsets (anim-select 6.1).
constexpr size_t HIT_BALL_Z = 0x24, HIT_BALL_Y = 0x28, HIT_BALL_X = 0x2c;
constexpr size_t HIT_ROOT_X = 0x30, HIT_ROOT_Z = 0x38, HIT_VEL_X = 0x3c, HIT_VEL_Z = 0x40;
constexpr size_t HIT_YAW = 0x44;

// The motion table's own layouts (FUN_1406aef60's builders, see mbinfo.hpp).
constexpr uint32_t SUB_MOTION_MASK = 0x7ff;   // FUN_1406ae670: (w0 & 0x7ff) + 0x61
constexpr uint32_t SUB_MOTION_BASE = 0x61;
constexpr uint32_t FOOT_MOTION_BASE = 0x10;   // FUN_1406ad170: (w0 & 0x1fff) + 0x10
constexpr uint32_t PIVOT_MOTION_MASK = 0xfff; // FUN_1406ae350: (w0 & 0xfff) + 0x20b
constexpr uint32_t PIVOT_MOTION_BASE = 0x20b;
constexpr int ROT_OFFSET_SHIFT = 13;          // FUN_1406b1c70: w0 bits 13..23
constexpr uint32_t ROT_OFFSET_MASK = 0x7ff;
constexpr int ROT_OFFSET_BIAS = 0x36e;        // minus 878
constexpr int ROT_START_SHIFT = 10;           // FUN_1406b1f00: w1 bits 10..19
constexpr uint32_t ROT_START_MASK = 0x3ff;
constexpr int ROT_START_BIAS = 0x205;         // minus 517
constexpr uint32_t ROT_END_MASK = 0x3ff;      // FUN_1406b1d00: w1 bits 0..9, plus 1
constexpr int FOOT_PAIR_SHIFT = 22;           // FUN_1406afe20: w0 bits 22..23
constexpr uint32_t FOOT_PAIR_MASK = 3;
constexpr int PIVOT_FLAG_SHIFT = 29;          // FUN_1406b1930: w0 bit 29

// PES's motion table holds 0x1125 entries of 0x390 bytes (FUN_1406b20e0's 0x3d13d0-byte block).
// A record whose motion is past the table is not stored: PES would write past its own block.
constexpr size_t MOTION_SLOTS = 0x1125;

// DAT_1425dc370: one dword per kind, the list cap (animation, subHit, cancel, rot, foot, pivot).
constexpr uint64_t CAPS_VA = 0x1425dc370;
constexpr uint64_t CAP_ANIMATION_KIND = 0, CAP_SUB_HIT_KIND = 2, CAP_CANCEL_KIND = 5,
                   CAP_ROT_KIND = 6, CAP_FOOT_KIND = 9, CAP_PIVOT_KIND = 12;

template <typename T>
T At(const std::vector<uint8_t>& b, size_t off) {
  T v;
  std::memcpy(&v, &b[off], sizeof v);
  return v;
}

template <typename V>
V& Slot(std::vector<V>& v, uint32_t motion) {
  if (v.size() <= motion) v.resize(motion + 1);
  return v[motion];
}

const std::vector<HitRecord> kNoHits;
const MotionRecord kNoMotion;

// The count-capped append of FUN_1406aef60's builders: a record past the cap is dropped.
template <typename V, typename T>
void Admit(std::vector<V>& list, size_t cap, T value) {
  if (list.size() < cap) list.push_back(value);
}

bool ReadU32(const PeImage& exe, uint64_t va, uint32_t* out) {
  uint8_t b[sizeof *out];
  if (!exe.ReadVa(va, sizeof b, b)) return false;
  std::memcpy(out, b, sizeof *out);
  return true;
}

}  // namespace

const MotionTiming* Mbinfo::Timing(int m) const {
  return m >= 0 && size_t(m) < timing_.size() && timing_[m] ? &*timing_[m] : nullptr;
}
const std::vector<HitRecord>& Mbinfo::Hits(int m) const {
  return m >= 0 && size_t(m) < hits_.size() ? hits_[m] : kNoHits;
}
const CancelRecord* Mbinfo::Cancel(int m) const {
  return m >= 0 && size_t(m) < cancel_.size() && cancel_[m] ? &*cancel_[m] : nullptr;
}
const MotionRecord& Mbinfo::Motion(int m) const {
  return m >= 0 && size_t(m) < motions_.size() ? motions_[m] : kNoMotion;
}

int MotionRecord::CancelFrames(int i) const {
  if (i < 0 || size_t(i) >= cancel.size()) return 0;
  return cancel[i];
}
int MotionRecord::FootPair(int i) const {
  if (i < 0 || size_t(i) >= foot.size()) return 0;
  return foot[i].pair;
}
RotRecord MotionRecord::Rot(int i) const {
  if (i < 0 || size_t(i) >= rot.size()) return RotRecord{};
  return rot[i];
}
bool MotionRecord::PivotFlag(int i) const {
  if (i < 0 || size_t(i) >= pivot.size()) return false;
  return pivot[i].flag;
}

std::optional<MotionCaps> LoadMotionCaps(const PeImage& exe, std::string* error) {
  MotionCaps c;
  const auto cap = [&](uint64_t kind, uint32_t* out) {
    return ReadU32(exe, CAPS_VA + 4 * kind, out);
  };
  if (!cap(CAP_ANIMATION_KIND, &c.animation) || !cap(CAP_SUB_HIT_KIND, &c.subHit) ||
      !cap(CAP_CANCEL_KIND, &c.cancel) || !cap(CAP_ROT_KIND, &c.rot) ||
      !cap(CAP_FOOT_KIND, &c.foot) || !cap(CAP_PIVOT_KIND, &c.pivot)) {
    *error = "mbinfo: the list caps at DAT_1425dc370 are not in the exe";
    return std::nullopt;
  }
  return c;
}

std::optional<Mbinfo> ParseMbinfo(const MbinfoBins& bins, const MotionCaps& caps, std::string* error) {
  const auto& animation = bins.animation;
  const auto& hitData = bins.hitData;
  const auto& cancelData = bins.cancelData;
  if (animation.size() % ANIMATION_RECORD || hitData.size() % HIT_RECORD ||
      cancelData.size() % CANCEL_RECORD || bins.subHitData.size() % SUB_HIT_RECORD ||
      bins.rotData.size() % ROT_RECORD || bins.footType.size() % FOOT_RECORD ||
      bins.pivothitInfo.size() % PIVOT_RECORD) {
    *error = "mbinfo: a file is not a whole number of records";
    return std::nullopt;
  }
  Mbinfo m;
  m.motions_.resize(MOTION_SLOTS);
  const auto inTable = [](uint32_t motion) { return motion < MOTION_SLOTS; };
  for (size_t o = 0; o < animation.size(); o += ANIMATION_RECORD) {
    const uint32_t a = At<uint32_t>(animation, o), b = At<uint32_t>(animation, o + 4);
    MotionTiming t;
    t.endFrame = int(a >> FRAME_SHIFT) + 1;
    t.blendStart = int(b >> BLEND_START_SHIFT & BLEND_START_MASK);
    float f[7];
    std::memcpy(f, &animation[o + 8], sizeof f);
    t.rootBlendZ = f[0], t.rootBlendX = f[1], t.rootBlendY = f[2], t.rootFirstX = f[3];
    t.rootFirstZ = f[4], t.cancelSpeed = f[5], t.endMoveAngle = f[6];
    Slot(m.timing_, a & MOTION_MASK) = t;
    const uint32_t motion = a & MOTION_MASK;
    if (inTable(motion)) Admit(m.motions_[motion].blend, caps.animation, t.blendStart);
  }
  for (size_t o = 0; o < hitData.size(); o += HIT_RECORD) {
    const uint32_t w = At<uint32_t>(hitData, o);
    HitRecord h;
    h.frame = int(w >> FRAME_SHIFT & HIT_FRAME_MASK);
    h.contactPart = int(w >> HIT_PART_SHIFT) + 1;
    h.ballX = At<float>(hitData, o + HIT_BALL_X);
    h.ballY = At<float>(hitData, o + HIT_BALL_Y);
    h.ballZ = At<float>(hitData, o + HIT_BALL_Z);
    h.rootX = At<float>(hitData, o + HIT_ROOT_X);
    h.rootZ = At<float>(hitData, o + HIT_ROOT_Z);
    h.rootVelX = At<float>(hitData, o + HIT_VEL_X);
    h.rootVelZ = At<float>(hitData, o + HIT_VEL_Z);
    h.yaw_deg = At<float>(hitData, o + HIT_YAW);
    Slot(m.hits_, w & MOTION_MASK).push_back(h);
  }
  for (size_t o = 0; o < cancelData.size(); o += CANCEL_RECORD) {
    const uint32_t w = At<uint32_t>(cancelData, o);
    const uint32_t motion = w & MOTION_MASK;
    Slot(m.cancel_, motion) = CancelRecord{int(w >> FRAME_SHIFT & CANCEL_FRAME_MASK)};
    if (inTable(motion))
      Admit(m.motions_[motion].cancel, caps.cancel, int(w >> FRAME_SHIFT & CANCEL_FRAME_MASK));
  }
  for (size_t o = 0; o < bins.rotData.size(); o += ROT_RECORD) {
    const uint32_t w0 = At<uint32_t>(bins.rotData, o), w1 = At<uint32_t>(bins.rotData, o + 4);
    const uint32_t motion = w0 & MOTION_MASK;
    RotRecord r;
    r.offset = int((w0 >> ROT_OFFSET_SHIFT) & ROT_OFFSET_MASK) - ROT_OFFSET_BIAS;
    r.start = int((w1 >> ROT_START_SHIFT) & ROT_START_MASK) - ROT_START_BIAS;
    r.end = int(w1 & ROT_END_MASK) + 1;
    if (inTable(motion)) Admit(m.motions_[motion].rot, caps.rot, r);
  }
  for (size_t o = 0; o < bins.subHitData.size(); o += SUB_HIT_RECORD) {
    const uint32_t motion = (At<uint32_t>(bins.subHitData, o) & SUB_MOTION_MASK) + SUB_MOTION_BASE;
    SubRecord s;
    for (size_t w = 0; w < s.words.size(); ++w) s.words[w] = At<uint32_t>(bins.subHitData, o + 4 * w);
    if (inTable(motion)) Admit(m.motions_[motion].sub, caps.subHit, s);
  }
  for (size_t o = 0; o < bins.footType.size(); o += FOOT_RECORD) {
    const uint32_t w = At<uint32_t>(bins.footType, o);
    const uint32_t motion = (w & MOTION_MASK) + FOOT_MOTION_BASE;
    FootRecord f{int((w >> FOOT_PAIR_SHIFT) & FOOT_PAIR_MASK)};
    if (inTable(motion)) Admit(m.motions_[motion].foot, caps.foot, f);
  }
  for (size_t o = 0; o < bins.pivothitInfo.size(); o += PIVOT_RECORD) {
    const uint32_t w = At<uint32_t>(bins.pivothitInfo, o);
    const uint32_t motion = (w & PIVOT_MOTION_MASK) + PIVOT_MOTION_BASE;
    PivotRecord p{((w >> PIVOT_FLAG_SHIFT) & 1u) != 0u};
    if (inTable(motion)) Admit(m.motions_[motion].pivot, caps.pivot, p);
  }
  const size_t n = std::max({m.timing_.size(), m.hits_.size(), m.cancel_.size()});
  m.timing_.resize(n), m.hits_.resize(n), m.cancel_.resize(n);
  return m;
}

std::optional<Mbinfo> LoadMbinfo(const PesVfs& vfs, const PeImage& exe, std::string* error) {
  auto caps = LoadMotionCaps(exe, error);
  if (!caps) return std::nullopt;
  MbinfoBins bins;
  for (auto [path, out] : {std::pair{ANIMATION_PATH, &bins.animation},
                           {HIT_DATA_PATH, &bins.hitData},
                           {CANCEL_DATA_PATH, &bins.cancelData},
                           {SUB_HIT_DATA_PATH, &bins.subHitData},
                           {ROT_DATA_PATH, &bins.rotData},
                           {FOOT_TYPE_PATH, &bins.footType},
                           {PIVOTHIT_INFO_PATH, &bins.pivothitInfo}}) {
    auto bytes = vfs.Read(path, error);
    if (!bytes) return std::nullopt;
    *out = std::move(*bytes);
  }
  return ParseMbinfo(bins, *caps, error);
}

}  // namespace pes
