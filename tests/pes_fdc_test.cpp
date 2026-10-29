// Reader tests for FixDemo cut files (.fdc): the container, the cut block's records and the
// embedded CANM camera streams. Synthetic files pin the layout; the install tests run only with
// PES21_ROOT and check consumption over every install file (relations and counts printed, not pinned).

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "pes/fdc.hpp"
#include "pes/translated/translated.hpp"
#include "pes/vfs.hpp"
#include "pes_state.hpp"

namespace {

using pes::fdc::Canm;
using pes::fdc::File;

void Put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
}
void PutF(std::vector<uint8_t>& b, float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  Put32(b, u);
}
void PutAt(std::vector<uint8_t>& b, size_t off, const std::string& s) {
  for (size_t i = 0; i < s.size() && off + i < b.size(); ++i) b[off + i] = uint8_t(s[i]);
}
void PutF32At(std::vector<uint8_t>& b, size_t off, float f) {
  std::vector<uint8_t> t;
  PutF(t, f);
  std::copy(t.begin(), t.end(), b.begin() + off);
}
void PutU32At(std::vector<uint8_t>& b, size_t off, uint32_t v) {
  std::vector<uint8_t> t;
  Put32(t, v);
  std::copy(t.begin(), t.end(), b.begin() + off);
}

struct Rec {
  uint8_t type;
  std::vector<uint8_t> bytes;
};

// The cut block: m, the m table rows, the records, then the type pool, then `tail` zero bytes.
std::vector<uint8_t> MakeBlock(const std::vector<Rec>& recs, size_t tail = 0) {
  const size_t m = recs.size();
  std::vector<uint8_t> blk;
  Put32(blk, uint32_t(m));
  std::vector<uint32_t> offs;
  size_t cur = 4 + 12 * m;
  blk.resize(cur, 0);
  for (const Rec& r : recs) {
    offs.push_back(uint32_t(cur));
    blk.insert(blk.end(), r.bytes.begin(), r.bytes.end());
    cur += r.bytes.size();
  }
  const uint32_t poolStart = uint32_t(cur);
  blk.resize(cur + m, 0);
  for (size_t i = 0; i < m; ++i) {
    PutU32At(blk, 4 + 12 * i, offs[i]);
    PutU32At(blk, 4 + 12 * i + 4, uint32_t(recs[i].bytes.size()));
    PutU32At(blk, 4 + 12 * i + 8, poolStart + uint32_t(i));
    blk[poolStart + i] = recs[i].type;
  }
  blk.resize(blk.size() + tail, 0);
  return blk;
}

struct Res {
  std::string name;
  bool external;
  std::vector<uint8_t> embedded;  // used when !external
};

// A whole .fdc: entry 0 is the cut block named by the file's path, then the resources.
std::vector<uint8_t> MakeFile(const std::string& name, const std::vector<Rec>& recs, const std::vector<Res>& res,
                              size_t blockTail = 0) {
  std::vector<uint8_t> blk = MakeBlock(recs, blockTail);
  std::vector<std::vector<uint8_t>> datas{blk};
  std::vector<std::string> names{name};
  for (const Res& r : res) {
    if (r.external) {
      std::vector<uint8_t> four;
      Put32(four, 0);
      datas.push_back(four);
    } else {
      datas.push_back(r.embedded);
    }
    names.push_back(r.name);
  }
  const size_t n = datas.size();
  std::vector<uint8_t> out;
  Put32(out, uint32_t(n));
  size_t cur = 4 + 12 * n;
  std::vector<uint32_t> offs;
  for (const auto& d : datas) {
    offs.push_back(uint32_t(cur));
    cur += d.size();
  }
  std::vector<uint32_t> nameOffs;
  std::vector<uint8_t> pool;
  for (const auto& s : names) {
    nameOffs.push_back(uint32_t(cur + pool.size()));
    pool.insert(pool.end(), s.begin(), s.end());
    pool.push_back(0);
  }
  for (size_t i = 0; i < n; ++i) {
    Put32(out, offs[i]);
    Put32(out, uint32_t(datas[i].size()));
    Put32(out, nameOffs[i]);
  }
  for (const auto& d : datas) out.insert(out.end(), d.begin(), d.end());
  out.insert(out.end(), pool.begin(), pool.end());
  return out;
}

// A 284-byte camera record (type 6) with the fields the reader decodes set.
std::vector<uint8_t> CameraBytes(uint32_t key, float start, const std::string& anime, float near, float far) {
  std::vector<uint8_t> r(284, 0);
  PutU32At(r, 0x00, key);
  PutF32At(r, 0x04, start);
  PutAt(r, 0x0c, anime);
  PutF32At(r, 0x90, near);
  PutF32At(r, 0x94, far);
  PutU32At(r, 0xb8, 0xffffffffu);  // zoom[0] = -1 (off)
  return r;
}

std::vector<uint8_t> TimingBytes(float start, float end, const std::string& action) {
  std::vector<uint8_t> r(92, 0);
  PutF32At(r, 0x04, start);
  PutF32At(r, 0x08, end);
  PutAt(r, 0x1c, action);
  return r;
}

// A CANM v1 stream with six channels: rotation (quat), position (vec4), fov, near, far, aspect.
struct CanmChannelSpec {
  uint32_t type;
  std::vector<uint16_t> frames;
  std::vector<std::vector<float>> values;
  bool noTimes = false;  // times offset 0: one key per frame, no table
};
std::vector<uint8_t> MakeCanm(uint32_t frameCount, const std::vector<CanmChannelSpec>& channels) {
  const size_t trackOff = 0x18;
  std::vector<uint8_t> b(trackOff, 0);
  b[0] = 1;   // version
  b[2] = 0x01;
  b[3] = 0xff;
  PutU32At(b, 4, frameCount);
  PutU32At(b, 8, 30);
  PutU32At(b, 12, 1);
  PutU32At(b, 16, uint32_t(trackOff));
  // track header: u16 channelCount, u16 0xff01, u32 pad, offsets[channelCount]
  const size_t chCount = channels.size();
  std::vector<uint8_t> track;
  track.push_back(uint8_t(chCount));
  track.push_back(0);
  track.push_back(0x01);
  track.push_back(0xff);
  Put32(track, 0);
  const size_t offTable = track.size();
  track.resize(offTable + 4 * chCount, 0);
  std::vector<uint32_t> chanOffs;
  size_t cur = track.size();
  for (size_t c = 0; c < chCount; ++c) {
    const auto& ch = channels[c];
    chanOffs.push_back(uint32_t(cur));
    const size_t ncomp = (ch.type == 1 || ch.type == 4) ? 4 : 1;
    const size_t chanStart = cur;
    const size_t tableBytes = ch.noTimes ? 0 : 2 * ch.frames.size();
    std::vector<uint8_t> body(16 + tableBytes, 0);
    const uint32_t timesOff = ch.noTimes ? 0 : 16;
    const uint32_t valuesOff = uint32_t(16 + tableBytes);
    std::vector<uint8_t> valuesBytes;
    for (const auto& v : ch.values)
      for (size_t k = 0; k < ncomp; ++k) PutF(valuesBytes, k < v.size() ? v[k] : 0.0f);
    body.resize(valuesOff, 0);
    body.insert(body.end(), valuesBytes.begin(), valuesBytes.end());
    PutU32At(body, 0, ch.type);
    body[4] = uint8_t(ch.frames.size());
    body[5] = uint8_t(ch.frames.size() >> 8);
    body[6] = uint8_t(c);
    body[7] = 0;
    PutU32At(body, 8, timesOff);
    PutU32At(body, 12, valuesOff);
    for (size_t k = 0; k < ch.frames.size() && !ch.noTimes; ++k) {
      body[timesOff + 2 * k] = uint8_t(ch.frames[k]);
      body[timesOff + 2 * k + 1] = uint8_t(ch.frames[k] >> 8);
    }
    track.insert(track.end(), body.begin(), body.end());
    cur = chanStart + body.size();
  }
  // Channel offsets are relative to the track start, which is trackOff in the file.
  for (size_t c = 0; c < chCount; ++c) PutU32At(track, offTable + 4 * c, chanOffs[c]);
  b.insert(b.end(), track.begin(), track.end());
  return b;
}

std::vector<uint8_t> OneRecordFile() {
  return MakeFile("cpk_dat/common/demo/fixdemo/goal/cut_data/a.fdc",
                  {{6, CameraBytes(10, 270.0f, "goal/canm/a.canm", 5.0f, 300.0f)}, {2, TimingBytes(12.5f, 40.0f, "demo_goal_level3")}},
                  {{"cpk_dat/common/x.gani", true, {}}, {"goal/canm/a.canm", false, MakeCanm(0, {})}});
}

}  // namespace

TEST(PesFdc, DecodesCameraAndTimingRecordsAndConsumesEveryByte) {
  const auto file = OneRecordFile();
  File f;
  std::string err;
  ASSERT_TRUE(pes::fdc::Parse(file, &f, &err)) << err;
  EXPECT_TRUE(f.Consumed());
  EXPECT_EQ(f.coverage.gapBytes, 0u);
  EXPECT_EQ(f.coverage.overlapBytes, 0u);
  EXPECT_EQ(f.name, "cpk_dat/common/demo/fixdemo/goal/cut_data/a.fdc");
  ASSERT_EQ(f.cameras.size(), 1u);
  EXPECT_EQ(f.cameras[0].key, 10u);
  EXPECT_FLOAT_EQ(f.cameras[0].startTime, 270.0f);
  EXPECT_EQ(f.cameras[0].animePath, "goal/canm/a.canm");
  EXPECT_FLOAT_EQ(f.cameras[0].nearClip, 5.0f);
  EXPECT_FLOAT_EQ(f.cameras[0].farClip, 300.0f);
  EXPECT_EQ(f.cameras[0].zoom[0], -1);
  ASSERT_EQ(f.timings.size(), 1u);
  EXPECT_FLOAT_EQ(f.timings[0].startTime, 12.5f);
  EXPECT_FLOAT_EQ(f.timings[0].endTime, 40.0f);
  EXPECT_EQ(f.timings[0].actionName, "demo_goal_level3");
  ASSERT_EQ(f.resources.size(), 2u);
  EXPECT_TRUE(f.resources[0].external);
  EXPECT_EQ(f.resources[0].vfsPath, "common/x.gani");
  EXPECT_FALSE(f.resources[1].external);
  ASSERT_NE(f.FindResource("goal/canm/a.canm"), nullptr);
}

TEST(PesFdc, StrayBytesAfterTheFileAreReportedAsGaps) {
  auto file = OneRecordFile();
  file.push_back(0xaa);
  file.push_back(0xaa);
  file.push_back(0xaa);
  File f;
  std::string err;
  ASSERT_TRUE(pes::fdc::Parse(file, &f, &err)) << err;
  EXPECT_FALSE(f.Consumed());
  EXPECT_EQ(f.coverage.gapBytes, 3u);
}

TEST(PesFdc, ZeroBytesAfterTheTypePoolInTheBlockAreCountedAsPadding) {
  const auto file = MakeFile("cpk_dat/common/demo/fixdemo/goal/cut_data/b.fdc",
                             {{2, TimingBytes(0.0f, 1.0f, "x")}}, {}, /*blockTail=*/3);
  File f;
  std::string err;
  ASSERT_TRUE(pes::fdc::Parse(file, &f, &err)) << err;
  EXPECT_TRUE(f.Consumed());
  EXPECT_EQ(f.coverage.padBytes, 3u);
}

TEST(PesFdc, RejectsARecordTypeTheLoaderDoesNotKnow) {
  const auto file = MakeFile("cpk_dat/x.fdc", {{42, std::vector<uint8_t>(16, 0)}}, {});
  File f;
  std::string err;
  EXPECT_FALSE(pes::fdc::Parse(file, &f, &err));
  EXPECT_FALSE(err.empty());
}

TEST(PesFdc, RejectsAFileShorterThanItsEntryTable) {
  const std::vector<uint8_t> file{3, 0, 0, 0, 1, 2};
  File f;
  std::string err;
  EXPECT_FALSE(pes::fdc::Parse(file, &f, &err));
}

// ---- CANM sampling and the FixDemoCamera pose (docs/re/pes-re-camera-pose.md) ----

namespace {

using pes::fdc::CameraRecord;
using pes::fdc::PoseConstants;

// The constants the pose maths reads from the executable; null without an install.
const PoseConstants* Constants() {
  static const std::optional<PoseConstants> k = []() -> std::optional<PoseConstants> {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return std::nullopt;
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    if (!exe) return std::nullopt;
    return pes::fdc::LoadPoseConstants(*exe, &err);
  }();
  return k ? &*k : nullptr;
}
#define NEED_CONSTANTS(k)                                                         \
  const PoseConstants* k = Constants();                                           \
  if (!k) GTEST_SKIP() << "PES21_ROOT not set or PES2021.exe pose constants unreadable"

// A clip with the given rotation and position channels; fov/near/far/aspect are one held key each.
std::vector<uint8_t> ClipBytes(uint32_t length, const std::vector<uint16_t>& rotFrames,
                               const std::vector<std::vector<float>>& rot, const std::vector<uint16_t>& posFrames,
                               const std::vector<std::vector<float>>& pos, bool posNoTimes = false) {
  return MakeCanm(length, {
      {1, rotFrames, rot},
      {4, posFrames, pos, posNoTimes},
      {8, {0}, {{40}}},
      {8, {0}, {{0.5f}}},
      {8, {0}, {{100}}},
      {8, {0}, {{1.5f}}},
  });
}

Canm Clip(uint32_t length, const std::vector<uint16_t>& rotFrames, const std::vector<std::vector<float>>& rot,
          const std::vector<uint16_t>& posFrames, const std::vector<std::vector<float>>& pos, bool posNoTimes = false) {
  Canm c;
  std::string err;
  EXPECT_TRUE(pes::fdc::ParseCanm(ClipBytes(length, rotFrames, rot, posFrames, pos, posNoTimes), &c, &err)) << err;
  return c;
}

Canm HeldClip(const std::vector<float>& q, const std::vector<float>& pos) { return Clip(10, {0}, {q}, {0}, {pos}); }

float Norm4(const std::array<float, 4>& q) { return std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]); }

}  // namespace

TEST(PesFdcCanm, ReadsLengthAndTheFpsOnlyWhenTheHeaderFlagsIt) {
  auto bytes = ClipBytes(7, {0}, {{0, 0, 0, 1}}, {0}, {{0, 0, 0, 1}});
  Canm c;
  std::string err;
  ASSERT_TRUE(pes::fdc::ParseCanm(bytes, &c, &err)) << err;
  EXPECT_EQ(c.length, 7.0f);
  EXPECT_TRUE(c.hasFps);
  EXPECT_EQ(c.fps, 30);
  bytes[12] = 0;  // bit 0 of +0xC clear: the stream has no fps (0x142139D60)
  ASSERT_TRUE(pes::fdc::ParseCanm(bytes, &c, &err)) << err;
  EXPECT_FALSE(c.hasFps);
}

TEST(PesFdcSample, RotationBelowTheLowSnapIsTheFirstKeyAndAboveTheHighSnapTheSecond) {
  NEED_CONSTANTS(k);
  // Unnormalised keys: any blend or renormalise would change them.
  const Canm c = Clip(1000, {0, 1000}, {{0, 0, 0, 2}, {0, 3, 0, 0}}, {0}, {{0, 0, 0, 1}});
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, 0.5f).rotation, (std::array<float, 4>{0, 0, 0, 2}));    // f = 0.0005
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, 999.5f).rotation, (std::array<float, 4>{0, 3, 0, 0}));  // f = 0.9995
}

TEST(PesFdcSample, KeysWithOneMinusDotAtMostTheThresholdBlendLinearlyAndStayUnnormalised) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {0, 10}, {{0, 0, 0, 1}, {0, 0.001f, 0, 1}}, {0}, {{0, 0, 0, 1}});
  const auto q = pes::fdc::SampleCanm(*k, c, 5.0f).rotation;  // d = 1: 1 - d = 0
  EXPECT_EQ(q[1], 0.5f * 0.001f);
  EXPECT_EQ(q[3], 1.0f);  // (1-f)*1 + f*1; a renormalise would give 1/|q| != 1
}

TEST(PesFdcSample, KeysJustPastTheLinearThresholdSlerpToAUnitQuaternion) {
  NEED_CONSTANTS(k);
  const float th = 0.015f;
  const float s = std::sin(th), co = std::cos(th);
  ASSERT_GT(1.0f - co, k->slerpLinearEps);  // the fixture is past the threshold
  const Canm c = Clip(10, {0, 10}, {{0, 0, 0, 1}, {0, s, 0, co}}, {0}, {{0, 0, 0, 1}});
  EXPECT_NEAR(Norm4(pes::fdc::SampleCanm(*k, c, 5.0f).rotation), 1.0f, 1e-6f);  // linear weights give cos(th/2)
}

TEST(PesFdcSample, SlerpDoesNotRenormaliseItsOutput) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {0, 10}, {{0, 0, 0, 0.5f}, {0, 0.5f, 0, 0}}, {0}, {{0, 0, 0, 1}});
  const auto q = pes::fdc::SampleCanm(*k, c, 5.0f).rotation;  // d = 0, half way: 0.5 * sin(45 deg) each
  EXPECT_NEAR(q[1], 0.5f * std::sqrt(0.5f), 1e-6f);
  EXPECT_NEAR(q[3], 0.5f * std::sqrt(0.5f), 1e-6f);
}

TEST(PesFdcSample, SlerpTakesTheShortArcByNegatingTheSecondKey) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {0, 10}, {{0, 0, 0, 1}, {0, 0.6f, 0, -0.8f}}, {0}, {{0, 0, 0, 1}});
  const auto q = pes::fdc::SampleCanm(*k, c, 5.0f).rotation;  // d = -0.8: the key is flipped to (0,-.6,0,.8)
  const double th = std::acos(0.8), w = std::sin(0.5 * th) / std::sin(th);
  EXPECT_NEAR(q[1], -w * 0.6, 1e-6);
  EXPECT_NEAR(q[3], w + w * 0.8, 1e-6);
}

TEST(PesFdcSample, PositionIsFTimesSecondPlusOneMinusFTimesFirst) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {0}, {{0, 0, 0, 1}}, {0, 10}, {{-43.9f, 0, 0, 1}, {41.7f, 0, 0, 1}});
  const float v0 = -43.9f, v1 = 41.7f, f = 0.1f;  // frame 1 of keys 0 and 10: f = 1/10
  ASSERT_NE(f * v1 + (1.0f - f) * v0, v0 + (v1 - v0) * f);  // the fixture tells the two forms apart
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, 1.0f).position[0], -35.340004f);
}

TEST(PesFdcSample, FramesPastTheLengthOrBelowZeroHoldTheEndPoses) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {0}, {{0, 0, 0, 1}}, {0, 10}, {{-4, 0, 0, 1}, {6, 0, 0, 1}});
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, 12.0f).position[0], 6.0f);
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, -3.0f).position[0], -4.0f);  // unclamped, f = -0.3 would extrapolate
}

TEST(PesFdcSample, BeforeTheFirstKeyTimePositionExtrapolatesAndRotationIsTheFirstKey) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {2, 10}, {{0, 0, 0, 2}, {0, 3, 0, 0}}, {2, 10}, {{0, 0, 0, 1}, {8, 0, 0, 1}});
  const auto s = pes::fdc::SampleCanm(*k, c, 1.0f);  // key 0 is at 2: f = (1 - 2) / 8 = -0.125
  EXPECT_EQ(s.position[0], -1.0f);
  EXPECT_EQ(s.rotation, (std::array<float, 4>{0, 0, 0, 2}));
}

TEST(PesFdcSample, EqualKeyTimesUseTheLastOfTheRunAsTheSegmentStart) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {0}, {{0, 0, 0, 1}}, {0, 5, 5, 10}, {{0, 0, 0, 1}, {1, 0, 0, 1}, {2, 0, 0, 1}, {3, 0, 0, 1}});
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, 5.0f).position[0], 2.0f);
  // A run at the first key time, sampled before it: the segment starts at the run's last key.
  const Canm d = Clip(10, {0}, {{0, 0, 0, 1}}, {2, 2, 10}, {{0, 0, 0, 1}, {5, 0, 0, 1}, {8, 0, 0, 1}});
  EXPECT_EQ(pes::fdc::SampleCanm(*k, d, 1.0f).position[0], 4.625f);  // f = -1/8: f*8 + (1-f)*5
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, 7.5f).position[0], 2.5f);
}

TEST(PesFdcSample, AChannelWithoutATimesTableHasOneKeyPerFrame) {
  NEED_CONSTANTS(k);
  const Canm c = Clip(10, {0}, {{0, 0, 0, 1}}, {0, 1, 2}, {{0, 0, 0, 1}, {10, 0, 0, 1}, {20, 0, 0, 1}}, /*posNoTimes=*/true);
  EXPECT_EQ(pes::fdc::SampleCanm(*k, c, 1.5f).position[0], 15.0f);
}

TEST(PesFdcSample, FovIsTheChannelDegreesToRadians) {
  NEED_CONSTANTS(k);
  const Canm c = HeldClip({0, 0, 0, 1}, {0, 0, 0, 1});
  EXPECT_NEAR(pes::fdc::SampleCanm(*k, c, 0.0f).fovRad, 40.0 * std::acos(-1.0) / 180.0, 1e-6);
}

TEST(PesFdcPose, ClipFrameIsClockMinusStartPlusAnimeStartTimesFpsOverSixty) {
  NEED_CONSTANTS(k);
  const Canm c = HeldClip({0, 0, 0, 1}, {0, 0, 0, 1});  // 30 fps stream
  CameraRecord rec;
  rec.startTime = 540.0f;
  rec.timeMode = 1;
  rec.animeStartTime = 6.0f;
  EXPECT_EQ(pes::fdc::ClipFrame(*k, c, rec, 570.0f), 36.0f * (30.0f / k->sixty));
  EXPECT_EQ(pes::fdc::ClipFrame(*k, c, rec, 100.0f), 6.0f * (30.0f / k->sixty));  // before the start: t = 0
  rec.timeMode = 0;                                                               // the player clock as is
  EXPECT_EQ(pes::fdc::ClipFrame(*k, c, rec, 100.0f), 106.0f * (30.0f / k->sixty));
  // fps / 60 is formed first, then multiplied by the time (0x141F7B51A, 0x141F7B526): at 24 fps and t = 9
  // the other order differs in the last bit.
  Canm c24 = c;
  c24.fps = 24;
  rec.animeStartTime = 0.0f;
  ASSERT_NE(9.0f * (24.0f / k->sixty), 9.0f * 24.0f / k->sixty);
  EXPECT_EQ(pes::fdc::ClipFrame(*k, c24, rec, 9.0f), 9.0f * (24.0f / k->sixty));
}

TEST(PesFdcPose, ClipFrameWrapsAtTheLengthOnlyForALoopingRecord) {
  NEED_CONSTANTS(k);
  const Canm c = HeldClip({0, 0, 0, 1}, {0, 0, 0, 1});  // 30 fps, length 10
  CameraRecord rec;
  rec.timeMode = 0;
  const float clock = k->sixty * 0.5f;  // half a second: 15 frames at 30 fps
  EXPECT_EQ(pes::fdc::ClipFrame(*k, c, rec, clock), 15.0f);
  rec.loop = 1;
  EXPECT_EQ(pes::fdc::ClipFrame(*k, c, rec, clock), 5.0f);
}

TEST(PesFdcPose, AStreamWithoutAnFpsRunsAtTheDefault) {
  NEED_CONSTANTS(k);
  Canm c = HeldClip({0, 0, 0, 1}, {0, 0, 0, 1});
  c.hasFps = false;
  c.fps = 99;
  EXPECT_EQ(pes::fdc::ClipFps(*k, c), k->defaultFps);
  CameraRecord rec;
  rec.timeMode = 0;
  EXPECT_EQ(pes::fdc::ClipFrame(*k, c, rec, 10.0f), 10.0f * (k->defaultFps / k->sixty));
}

TEST(PesFdcPose, IdentityRotationLooksDownMinusZWithColumnsZeroAndTwoNegated) {
  NEED_CONSTANTS(k);
  const auto p = pes::fdc::EvaluateCameraPose(*k, HeldClip({0, 0, 0, 1}, {1, 2, 3, 1}), CameraRecord{}, 0.0f, {}, 0.0f);
  EXPECT_EQ(p.forward(), (std::array<float, 3>{0, 0, -1}));
  EXPECT_EQ(p.up(), (std::array<float, 3>{0, 1, 0}));
  EXPECT_EQ(p.m[0][0], -1.0f);  // column 0 is minus the camera's right
  EXPECT_EQ(p.eye, (std::array<float, 3>{1, 2, 3}));
}

TEST(PesFdcPose, TheMatrixNormalisesAnUnnormalisedQuaternion) {
  NEED_CONSTANTS(k);
  const float h = std::sqrt(0.5f);
  const auto unit = pes::fdc::EvaluateCameraPose(*k, HeldClip({0, h, 0, h}, {0, 0, 0, 1}), CameraRecord{}, 0.0f, {}, 0.0f);
  const auto big = pes::fdc::EvaluateCameraPose(*k, HeldClip({0, 3 * h, 0, 3 * h}, {0, 0, 0, 1}), CameraRecord{}, 0.0f, {}, 0.0f);
  for (int r = 0; r < 3; ++r)
    for (int col = 0; col < 3; ++col) EXPECT_NEAR(big.m[r][col], unit.m[r][col], 1e-6f);
  // A unit quaternion for +90 deg about Y turns +Z to +X; the camera looks down -R*z.
  EXPECT_NEAR(unit.forward()[0], -1.0f, 1e-6f);
  EXPECT_NEAR(unit.forward()[2], 0.0f, 1e-6f);
}

TEST(PesFdcPose, HeightOffsetRaisesTheEye) {
  NEED_CONSTANTS(k);
  const auto p = pes::fdc::EvaluateCameraPose(*k, HeldClip({0, 0, 0, 1}, {1, 2, 3, 1}), CameraRecord{}, 0.0f, {}, 0.25f);
  EXPECT_EQ(p.eye, (std::array<float, 3>{1, 2.25f, 3}));
}

TEST(PesFdcPose, HeightOffsetIsTargetHeightMinusTheReferenceTimesScaleTimesRate) {
  NEED_CONSTANTS(k);
  EXPECT_EQ(pes::fdc::HeightOffsetM(*k, k->heightRef + 20.0f, 2.0f), (20.0f * k->heightScale) * 2.0f);
  EXPECT_EQ(pes::fdc::HeightOffsetM(*k, k->heightRef - 10.0f, 0.0f), 0.0f);
}

TEST(PesFdcPose, FlipMirrorsXOfTheEyeAndTheOrientation) {
  NEED_CONSTANTS(k);
  const Canm c = HeldClip({0.2f, 0.3f, 0.1f, 0.9f}, {4, 2, 3, 1});  // every off-diagonal element is non-zero
  const auto plain = pes::fdc::EvaluateCameraPose(*k, c, CameraRecord{}, 0.0f, {}, 0.0f);
  pes::fdc::CutOffset flip;
  flip.flip = true;
  CameraRecord rec;
  rec.useCutOffset = 1;
  const auto mirrored = pes::fdc::EvaluateCameraPose(*k, c, rec, 0.0f, flip, 0.0f);
  ASSERT_NE(plain.m[1][2], 0.0f);
  EXPECT_EQ(mirrored.eye, (std::array<float, 3>{-4, 2, 3}));
  for (size_t r = 0; r < 3; ++r)  // S M S with S = diag(-1, 1, 1): m01, m02, m10, m20 change sign
    for (size_t col = 0; col < 3; ++col)
      EXPECT_EQ(mirrored.m[r][col], ((r == 0) != (col == 0)) ? -plain.m[r][col] : plain.m[r][col]) << r << col;
}

TEST(PesFdcPose, RotYIsAWorldRotationNotACameraLocalOne) {
  NEED_CONSTANTS(k);
  const float a = 0.3f;  // pitch the camera about X, so a local and a world yaw differ
  const Canm c = HeldClip({std::sin(a), 0, 0, std::cos(a)}, {0, 0, 0, 1});
  const auto plain = pes::fdc::EvaluateCameraPose(*k, c, CameraRecord{}, 0.0f, {}, 0.0f);
  CameraRecord rec;
  rec.useCutOffset = 1;
  pes::fdc::CutOffset off;
  off.rotYDeg = 90.0f;  // Ry(90) * v = (v2, v1, -v0)
  const auto turned = pes::fdc::EvaluateCameraPose(*k, c, rec, 0.0f, off, 0.0f);
  const auto f = plain.forward(), u = plain.up();
  ASSERT_NE(f[1], 0.0f);
  EXPECT_NEAR(turned.forward()[0], f[2], 1e-6f);
  EXPECT_NEAR(turned.forward()[1], f[1], 1e-6f);
  EXPECT_NEAR(turned.forward()[2], -f[0], 1e-6f);
  EXPECT_NEAR(turned.up()[0], u[2], 1e-6f);
  EXPECT_NEAR(turned.up()[1], u[1], 1e-6f);
  EXPECT_NEAR(turned.up()[2], -u[0], 1e-6f);
}

TEST(PesFdcPose, RotYTurnsTheWholeShotAboutWorldYAndTheOffsetIsAddedAfterFlipAndRotY) {
  NEED_CONSTANTS(k);
  const Canm c = HeldClip({0, 0, 0, 1}, {1, 0, 0, 1});
  CameraRecord rec;
  rec.useCutOffset = 1;
  pes::fdc::CutOffset off;
  off.rotYDeg = 90.0f;
  auto p = pes::fdc::EvaluateCameraPose(*k, c, rec, 0.0f, off, 0.0f);
  EXPECT_NEAR(p.eye[0], 0.0f, 1e-6f);  // (1,0,0) -> (cos, 0, -sin)
  EXPECT_NEAR(p.eye[2], -1.0f, 1e-6f);
  EXPECT_NEAR(p.forward()[0], -1.0f, 1e-6f);  // (0,0,-1) -> (-sin, 0, cos * -1)
  EXPECT_NEAR(p.forward()[2], 0.0f, 1e-6f);
  EXPECT_NEAR(p.up()[1], 1.0f, 1e-6f);

  off.flip = true;  // flip, then turn, then add: (1,0,0) -> (-1,0,0) -> (0,0,1) -> (10,0,1)
  off.pos = {10, 0, 0};
  p = pes::fdc::EvaluateCameraPose(*k, c, rec, 0.0f, off, 0.0f);
  EXPECT_NEAR(p.eye[0], 10.0f, 1e-6f);
  EXPECT_NEAR(p.eye[2], 1.0f, 1e-6f);

  rec.useCutOffset = 0;  // the record does not ask for the player's offset
  p = pes::fdc::EvaluateCameraPose(*k, c, rec, 0.0f, off, 0.0f);
  EXPECT_EQ(p.eye, (std::array<float, 3>{1, 0, 0}));
}

TEST(PesFdcRender, NearIsTheRecordsAndFarIsAtLeastTheFloor) {
  NEED_CONSTANTS(k);
  const auto pose = pes::fdc::EvaluateCameraPose(*k, HeldClip({0, 0, 0, 1}, {0, 0, 0, 1}), CameraRecord{}, 0.0f, {}, 0.0f);
  CameraRecord rec;
  rec.nearClip = 0.05f;  // below any GF clamp: PES passes the record's value on
  rec.farClip = k->renderFarFloor * 0.1f;
  auto r = pes::fdc::RenderCameraParams(*k, pose, rec, k->wideAspect, false);
  EXPECT_EQ(r.nearM, 0.05f);
  EXPECT_EQ(r.farM, k->renderFarFloor);
  rec.farClip = k->renderFarFloor * 2.0f;
  EXPECT_EQ(pes::fdc::RenderCameraParams(*k, pose, rec, k->wideAspect, false).farM, k->renderFarFloor * 2.0f);
}

TEST(PesFdcRender, FovIsClampedToZeroAndTheMaximum) {
  NEED_CONSTANTS(k);
  auto pose = pes::fdc::EvaluateCameraPose(*k, HeldClip({0, 0, 0, 1}, {0, 0, 0, 1}), CameraRecord{}, 0.0f, {}, 0.0f);
  pose.fovRad = k->fovMaxRad + 0.5f;
  EXPECT_EQ(pes::fdc::RenderCameraParams(*k, pose, CameraRecord{}, k->wideAspect, false).fovRad, k->fovMaxRad);
  pose.fovRad = -0.2f;
  EXPECT_EQ(pes::fdc::RenderCameraParams(*k, pose, CameraRecord{}, k->wideAspect, false).fovRad, 0.0f);
}

TEST(PesFdcRender, WidescreenFixKeepsTheSixteenByNineHorizontalFovOnlyBelowThatAspect) {
  NEED_CONSTANTS(k);
  auto pose = pes::fdc::EvaluateCameraPose(*k, HeldClip({0, 0, 0, 1}, {0, 0, 0, 1}), CameraRecord{}, 0.0f, {}, 0.0f);
  pose.fovRad = 0.8f;
  const float narrow = k->wideAspect * 0.75f;
  const double want = 2.0 * std::atan(std::tan(0.4) * double(k->wideAspect) / double(narrow));
  EXPECT_NEAR(pes::fdc::RenderCameraParams(*k, pose, CameraRecord{}, narrow, true).fovRad, want, 1e-6);
  EXPECT_EQ(pes::fdc::RenderCameraParams(*k, pose, CameraRecord{}, narrow, false).fovRad, 0.8f);  // option byte clear
  EXPECT_EQ(pes::fdc::RenderCameraParams(*k, pose, CameraRecord{}, k->wideAspect * 1.2f, true).fovRad, 0.8f);  // wider
}

// The worked example of docs/re/pes-re-camera-pose.md §3 (tools/dev/cutscene/camera_pose_example.py, float32):
// ent_001_view_st000_cam2.fdc, camera record 0 (key 20, start 540 ticks, cam0021.canm), the cut offset
// taken as zero. eye, forward (= -R*z), up (= R*y), fov (radians), clip frame.
struct WorkedRow {
  float dTicks;
  std::array<float, 3> eye, forward, up;
  float fovRad, frame;
};
const WorkedRow kWorked[] = {
    {30.0f, {-55.0f, 0.400000006f, 9.69999981f}, {0.494217366f, 0.241652727f, -0.83507669f},
     {-0.123076625f, 0.970362782f, 0.207961977f}, 0.809797525f, 15.0f},
    {91.0f, {-55.0f, 0.400000006f, 9.69999981f}, {0.528505206f, 0.244651943f, -0.81291306f},
     {-0.133352309f, 0.969610989f, 0.205113947f}, 0.809797525f, 45.5f},  // the slerp's linear-weight branch
    {180.0f, {-55.0f, 0.400000006f, 9.69999981f}, {0.625869215f, 0.251175195f, -0.738375783f},
     {-0.162409395f, 0.967941642f, 0.191604182f}, 0.809797525f, 90.0f},
};

TEST(PesFdcPoseInstall, LoadsEveryConstantFromItsInstruction) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::fdc::LoadPoseConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  EXPECT_NEAR(k->degToRad * k->oneEighty, k->pi, 1e-6f);
  EXPECT_LT(k->snapLow, k->snapHigh);
  EXPECT_LT(k->snapHigh, k->one);
  EXPECT_GT(k->defaultFps, 0.0f);
  EXPECT_GT(k->renderFarFloor, 0.0f);
}

TEST(PesFdcPoseInstall, EntranceCutPoseMatchesTheWorkedExampleBitForBit) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::fdc::LoadPoseConstants(*exe, &err);
  ASSERT_TRUE(k) << err;

  std::string fdcPath;
  for (const auto& p : pes::fdc::ListCutFiles(vfs))
    if (pes::PesVfs::Fold(p).find("ent_001_view_st000_cam2.fdc") != std::string::npos) fdcPath = p;
  if (fdcPath.empty()) GTEST_SKIP() << "the install has no entrance cut";
  auto bytes = vfs.Read(fdcPath, &err);
  ASSERT_TRUE(bytes) << err;
  File f;
  ASSERT_TRUE(pes::fdc::Parse(*bytes, &f, &err)) << err;
  ASSERT_FALSE(f.cameras.empty());
  const CameraRecord& rec = f.cameras[0];
  const pes::fdc::Resource* res = f.FindResource(rec.animePath);
  ASSERT_NE(res, nullptr);
  auto clip = res->external ? vfs.Read(res->vfsPath, &err) : std::optional<std::vector<uint8_t>>(res->data);
  ASSERT_TRUE(clip) << err;
  Canm c;
  ASSERT_TRUE(pes::fdc::ParseCanm(*clip, &c, &err)) << err;
  // The oracle is the worked example of this install's record: skip when a mod changed it.
  if (rec.startTime != 540.0f || rec.timeMode != 1 || rec.animeStartTime != 0.0f || c.length != 330.0f ||
      !c.hasFps || c.fps != 30)
    GTEST_SKIP() << "the installed entrance record differs from the worked example";

  for (const WorkedRow& w : kWorked) {
    const auto p = pes::fdc::EvaluateCameraPose(*k, c, rec, rec.startTime + w.dTicks, {}, 0.0f);
    SCOPED_TRACE(w.dTicks);
    EXPECT_EQ(p.eye, w.eye);
    EXPECT_EQ(p.forward(), w.forward);
    EXPECT_EQ(p.up(), w.up);
    EXPECT_EQ(p.fovRad, w.fovRad);
    EXPECT_EQ(p.frame, w.frame);
    const auto r = pes::fdc::RenderCameraParams(*k, p, rec, k->wideAspect, false);
    EXPECT_EQ(r.nearM, rec.nearClip);
    EXPECT_EQ(r.farM, std::max(k->renderFarFloor, rec.farClip));
  }
}

TEST(PesFdcCanm, RejectsAStreamWithoutTheMagic) {
  auto bytes = MakeCanm(0, {});
  bytes[2] = 0;
  Canm c;
  std::string err;
  EXPECT_FALSE(pes::fdc::ParseCanm(bytes, &c, &err));
}

// Install tests: PES21_ROOT only. Every .fdc the VFS lists must be parsed; a file the coverage check
// rejects must carry a type-99 (Disabled) record, the one OPEN class (docs/re/pes-re-fdc-format.md §5).
TEST(PesFdcInstall, EveryCutFileParsesAndConsumesItsBytesOrHasTheKnownDisabledRecord) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  const auto files = pes::fdc::ListCutFiles(vfs);
  ASSERT_FALSE(files.empty());
  size_t parsed = 0, consumed = 0, openDisabled = 0, errors = 0, unexplained = 0;
  for (const auto& path : files) {
    auto bytes = vfs.Read(path, &err);
    if (!bytes) {
      ++errors;
      continue;
    }
    File f;
    if (!pes::fdc::Parse(*bytes, &f, &err)) {
      ++errors;
      continue;
    }
    ++parsed;
    if (f.Consumed()) {
      ++consumed;
    } else if (f.HasRecordType(pes::fdc::kDisabled)) {
      ++openDisabled;
    } else {
      ++unexplained;
    }
  }
  std::cout << "[fdc install] files " << files.size() << " parsed " << parsed << " consume every byte " << consumed
            << " open (type-99 gap) " << openDisabled << " errors " << errors << "\n";
  EXPECT_EQ(errors, 0u);
  EXPECT_EQ(unexplained, 0u);
}

TEST(PesFdcInstall, EveryEmbeddedCameraStreamIsAWellFormedUnitRotationStream) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  size_t streams = 0, bad = 0;
  for (const auto& path : pes::fdc::ListCutFiles(vfs)) {
    auto bytes = vfs.Read(path, &err);
    File f;
    if (!bytes || !pes::fdc::Parse(*bytes, &f, &err)) continue;
    for (const auto& r : f.resources) {
      if (r.external || r.data.size() < 4 || r.data[2] != 0x01 || r.data[3] != 0xff) continue;
      Canm c;
      ++streams;
      if (!pes::fdc::ParseCanm(r.data, &c, &err)) {
        ++bad;
        continue;
      }
      for (const auto& q : c.channels[0].values) {
        const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (std::fabs(n - 1.0f) > 1e-3f) {
          ++bad;
          break;
        }
      }
      if (c.channels[0].frames.size() != size_t(c.length) + 1u) ++bad;
    }
  }
  std::cout << "[fdc canm install] embedded streams " << streams << " bad " << bad << "\n";
  EXPECT_GT(streams, 0u);
  EXPECT_EQ(bad, 0u);
}

// ---------------------------------------------------------------------------------------------------
// The cut offset's rotY (FixDemoCamera::Update 0x141F7B8FC): the axis-angle builder 0x140A91E60 takes its
// sine and cosine from PES's 512-step degree table (0x140A92C80), not from libm.

namespace {

constexpr uint64_t kScratchBase = 0x60000000;

// The translated builder 0x140A91E60 about the axis (x, y, z), the angle in radians, run on the install's
// sine table; the 3x3 it stores (rows 0x10 apart).
pes::fdc::Mat3 TranslatedBuilder(const PoseConstants& k, float x, float y, float z, float angleRad) {
  PesState s;
  s.mem.window = {0x142655950, sizeof k.sinTable, reinterpret_cast<const uint8_t*>(k.sinTable.data()), nullptr};
  s.mem.segs.push_back({kScratchBase, std::vector<uint8_t>(0x1000)});
  const uint64_t out = kScratchBase, top = kScratchBase + 0x800;
  s.gpr[1] = out;
  s.gpr[4] = top;
  s.set_x_f32(1, x);
  s.set_x_f32(2, y);
  s.set_x_f32(3, z);
  s.wrf(top + 0x28, angleRad);  // the fifth argument slot: [entry rsp + 0x28] = [rsp + 0xB0] after the prologue
  ::pes_fn_140A91E60(s);
  pes::fdc::Mat3 m{};
  for (size_t r = 0; r < 3; ++r)
    for (size_t c = 0; c < 3; ++c) m[r][c] = s.rdf(out + 0x10 * r + 4 * c);
  return m;
}

}  // namespace

TEST(PesFdcCutOffset, RotYMatrixIsTheTranslatedBuildersBitForBit) {
  NEED_CONSTANTS(k);
  // Degrees across the quadrants, off-table values (the table has 512 steps per quarter turn) and a
  // negative angle; each goes through PES's own deg -> rad multiply before the builder's rad -> deg.
  for (float deg : {1.0f, 30.6f, 40.3f, 89.9f, 123.4f, 179.0f, 271.0f, 359.9f, -45.0f, -200.5f}) {
    SCOPED_TRACE(deg);
    const pes::fdc::Mat3 want = TranslatedBuilder(*k, 0.0f, k->one, 0.0f, deg * k->degToRad);
    const pes::fdc::Mat3 got = pes::fdc::RotYMatrix(*k, deg);
    for (size_t r = 0; r < 3; ++r)
      for (size_t c = 0; c < 3; ++c) EXPECT_EQ(got[r][c], want[r][c]) << "m[" << r << "][" << c << "]";
  }
}

TEST(PesFdcCutOffset, RotYTurnsTheEyeByTheAngleAboutWorldY) {
  NEED_CONSTANTS(k);
  // Relation, not a pinned table: a unit eye on +X turned by a degrees lands at (cos a, 0, -sin a).
  const Canm c = HeldClip({0, 0, 0, 1}, {1, 0, 0, 1});
  CameraRecord rec;
  rec.useCutOffset = 1;
  for (float deg : {17.0f, 123.4f, 250.0f}) {
    SCOPED_TRACE(deg);
    pes::fdc::CutOffset off;
    off.rotYDeg = deg;
    const auto p = pes::fdc::EvaluateCameraPose(*k, c, rec, 0.0f, off, 0.0f);
    const double a = double(deg) * M_PI / 180.0;
    EXPECT_NEAR(p.eye[0], std::cos(a), 2e-5);
    EXPECT_NEAR(p.eye[2], -std::sin(a), 2e-5);
  }
}

TEST(PesFdcCutOffset, RotYOutsideZeroToThreeSixtyTurnsTheShotLikeItsWrappedAngle) {
  NEED_CONSTANTS(k);
  // FixDemoCamera slot +0x60 (0x141F7B700) stores rotY wrapped once into [0, 360): the shot cannot tell.
  const Canm c = HeldClip({0, 0, 0, 1}, {1, 0, 0, 1});
  CameraRecord rec;
  rec.useCutOffset = 1;
  auto eyeAt = [&](float deg) {
    pes::fdc::CutOffset off;
    off.rotYDeg = deg;
    return pes::fdc::EvaluateCameraPose(*k, c, rec, 0.0f, off, 0.0f).eye;
  };
  EXPECT_EQ(eyeAt(-90.0f), eyeAt(270.0f));
  EXPECT_EQ(eyeAt(450.0f), eyeAt(90.0f));
  EXPECT_EQ(eyeAt(360.0f), eyeAt(0.0f));
  EXPECT_NE(eyeAt(-90.0f), eyeAt(90.0f));
}

// ---------------------------------------------------------------------------------------------------
// The live camera of the entrance (rendezvous capture 008 of the cut_offset hook 0x141F39CC0, taken the
// frame ent_001_order_st000_cam0001 replaced ent_001_view_st000_cam0000 at clock 180 ticks). At the call
// the FixDemo player's camera holds cam0000 evaluated at the capture clock c (SetTime has run, Update has
// not), and the render parameter block 0x1437F4A40 holds the previous Update's result, at c - dt
// (dt = player+0x6C: the 54 Hz step). The cut offset read at the same call is zero (player+0xA08..+0xA18).

namespace {

float Bits(uint32_t u) { return std::bit_cast<float>(u); }

struct LiveEntrance {
  File file;
  Canm clip;
  CameraRecord rec;
};

std::optional<LiveEntrance> LoadEntranceRecord(const std::string& clipName) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return std::nullopt;
  pes::PesVfs vfs;
  std::string err;
  if (!vfs.Mount(root, &err)) return std::nullopt;
  std::string path;
  for (const auto& p : pes::fdc::ListCutFiles(vfs))
    if (pes::PesVfs::Fold(p).find("ent_001_view_st000_cam1.fdc") != std::string::npos) path = p;
  if (path.empty()) return std::nullopt;
  auto bytes = vfs.Read(path, &err);
  if (!bytes) return std::nullopt;
  LiveEntrance e;
  if (!pes::fdc::Parse(*bytes, &e.file, &err)) return std::nullopt;
  bool found = false;
  for (const CameraRecord& r : e.file.cameras)
    if (r.animePath.find(clipName) != std::string::npos) e.rec = r, found = true;
  if (!found) return std::nullopt;
  const pes::fdc::Resource* res = e.file.FindResource(e.rec.animePath);
  if (!res || res->external) return std::nullopt;
  if (!pes::fdc::ParseCanm(res->data, &e.clip, &err)) return std::nullopt;
  return e;
}

}  // namespace

TEST(PesFdcLiveEntrance, ViewCam0PoseAtTheCaptureClockHasTheLiveCamerasPositionAndFov) {
  NEED_CONSTANTS(k);
  auto e = LoadEntranceRecord("ent_001_view_st000_cam0000");
  if (!e) GTEST_SKIP() << "the install has no ent_001_view_st000_cam1.fdc";
  const float clock = Bits(0x43340016);  // player+0x80 at the capture, 180.00034 ticks
  const auto p = pes::fdc::EvaluateCameraPose(*k, e->clip, e->rec, clock, {}, 0.0f);
  // The camera's pose block (cam+0x168): position and fov at the capture clock, before Update.
  EXPECT_EQ(p.eye, (std::array<float, 3>{Bits(0x42554c33), Bits(0x3ebd4ef4), Bits(0x420e8a04)}));
  EXPECT_EQ(p.fovRad, Bits(0x3f6d643a));
}

TEST(PesFdcLiveEntrance, ViewCam0RenderParametersAtThePreviousClockMatchTheLiveBlock) {
  NEED_CONSTANTS(k);
  auto e = LoadEntranceRecord("ent_001_view_st000_cam0000");
  if (!e) GTEST_SKIP() << "the install has no ent_001_view_st000_cam1.fdc";
  const float prev = Bits(0x4332e3a4);  // c - dt, dt = player+0x6C = 1.1111145 ticks
  const auto p = pes::fdc::EvaluateCameraPose(*k, e->clip, e->rec, prev, {}, 0.0f);
  const auto r = pes::fdc::RenderCameraParams(*k, p, e->rec, k->wideAspect, false);
  // Block 0x1437F4A40: [3..5] eye, [6..8] up, [0..2] target, [0xC] fov, [0xD] near, [0xE] far.
  const std::array<float, 3> eye{Bits(0x42554db3), Bits(0x3ebd48d0), Bits(0x420e8ab2)};
  const std::array<float, 3> up{Bits(0x3e0161e4), Bits(0x3f79472b), Bits(0x3e41eacd)};
  const std::array<float, 3> target{Bits(0x41ceb4e4), Bits(0x4123c9a2), Bits(0x404f5400)};
  const float dist = Bits(0x422e778d);  // pose block +0x30: the target's distance along the view direction
  EXPECT_EQ(p.eye, eye);
  EXPECT_EQ(r.fovRad, Bits(0x3f6d643a));
  EXPECT_EQ(r.nearM, 0.5f);
  // Block [0xE] is Update's far, the record's own (400). The renderer then floors it at 4000 (0x141EAF91C).
  EXPECT_EQ(e->rec.farClip, 400.0f);
  EXPECT_EQ(r.farM, std::max(k->renderFarFloor, e->rec.farClip));
  // Up and target go through PES's yaw/pitch/roll round trip, which GF does not emulate (INF): rounding only.
  const auto f = p.forward();
  float worstUp = 0, worstTarget = 0;
  for (size_t i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    worstUp = std::max(worstUp, std::fabs(p.up()[i] - up[i]));
    worstTarget = std::max(worstTarget, std::fabs(eye[i] + dist * f[i] - target[i]));
    EXPECT_NEAR(p.up()[i], up[i], 2e-5f);
    EXPECT_NEAR(eye[i] + dist * f[i], target[i], 2e-4f);
  }
  std::cout << "[live entrance] worst |up - live| " << worstUp << ", worst |eye + dist*forward - live target| " << worstTarget
            << "\n";
}

// The same player's last pose of the entrance: capture 009 (hook 0x141F41CE0) was taken after the cut had
// ended (the player clock read 534 ticks, 8.9 s, and no longer advanced; its camera was released), so the
// render block 0x1437F4A40 still holds ent_001_order_st000_cam0001's last Update, at that clock.
TEST(PesFdcLiveEntrance, OrderCam1AtTheCutsLastClockMatchesTheLiveBlock) {
  NEED_CONSTANTS(k);
  auto e = LoadEntranceRecord("ent_001_order_st000_cam0001");
  if (!e) GTEST_SKIP() << "the install has no ent_001_view_st000_cam1.fdc";
  const float clock = 534.0f;  // player+0x80 at capture 009
  const auto p = pes::fdc::EvaluateCameraPose(*k, e->clip, e->rec, clock, {}, 0.0f);
  EXPECT_EQ(p.frame, 177.0f) << "(534 - 180) ticks at 30 fps; the clip is 180 frames long";
  const std::array<float, 3> eye{Bits(0xc2900000), Bits(0x41400000), Bits(0x00000000)};
  const std::array<float, 3> up{Bits(0x3e0beaac), Bits(0x3f7d9958), Bits(0xb1cd37a2)};
  const std::array<float, 3> target{Bits(0xc1e656b0), Bits(0x40c14a50), Bits(0xb5fd7e1e)};
  const float dist = Bits(0x422e778d);
  EXPECT_EQ(p.eye, eye);
  EXPECT_EQ(p.fovRad, Bits(0x3f7fab6f));
  const auto f = p.forward();
  for (size_t i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    EXPECT_NEAR(p.up()[i], up[i], 2e-5f);
    EXPECT_NEAR(eye[i] + dist * f[i], target[i], 2e-4f);
  }
}
