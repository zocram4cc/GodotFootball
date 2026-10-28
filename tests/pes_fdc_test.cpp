// Reader tests for FixDemo cut files (.fdc): the container, the cut block's records and the
// embedded CANM camera streams. Synthetic files pin the layout; the install tests run only with
// PES21_ROOT and check consumption over every install file (relations and counts printed, not pinned).

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "pes/fdc.hpp"
#include "pes/vfs.hpp"

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
    std::vector<uint8_t> body(16, 0);
    body.resize(16 + 2 * ch.frames.size(), 0);
    const uint32_t timesOff = 16;
    const uint32_t valuesOff = uint32_t(timesOff + 2 * ch.frames.size());
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
    for (size_t k = 0; k < ch.frames.size(); ++k) {
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

TEST(PesFdcCanm, SamplesLinearPositionAndSlerpsRotationBetweenKeys) {
  const float s = std::sqrt(0.5f);
  const auto bytes = MakeCanm(2, {
      {1, {0, 1, 2}, {{0, 0, 0, 1}, {0, s, 0, s}, {0, s, 0, s}}},   // rotation: id, 90 deg about Y, held
      {4, {0, 1, 2}, {{0, 1, 0, 1}, {2, 1, 0, 1}, {2, 1, 4, 1}}},   // position
      {8, {0, 1, 2}, {{40}, {50}, {50}}},                            // vertical fov
      {8, {0}, {{0.5f}}},                                             // near
      {8, {0}, {{100}}},                                              // far
      {8, {0}, {{1.5f}}},                                             // aspect
  });
  Canm c;
  std::string err;
  ASSERT_TRUE(pes::fdc::ParseCanm(bytes, &c, &err)) << err;
  EXPECT_EQ(c.frameCount, 2u);
  EXPECT_EQ(c.frameRate, 30u);

  const auto mid = pes::fdc::SampleCanm(c, 0.5f);
  EXPECT_NEAR(mid.position[0], 1.0f, 1e-5f);
  EXPECT_NEAR(mid.position[2], 0.0f, 1e-5f);
  EXPECT_NEAR(mid.fovDeg, 45.0f, 1e-4f);
  EXPECT_NEAR(mid.rotation[1], std::sin(0.5f * std::acos(-1.0f) / 4.0f), 1e-4f);  // half of 90 deg about Y
  EXPECT_NEAR(mid.rotation[3], std::cos(0.5f * std::acos(-1.0f) / 4.0f), 1e-4f);
  EXPECT_NEAR(mid.nearM, 0.5f, 1e-6f);
  EXPECT_NEAR(mid.farM, 100.0f, 1e-6f);
  EXPECT_NEAR(mid.aspect, 1.5f, 1e-6f);

  const auto late = pes::fdc::SampleCanm(c, 1.5f);
  EXPECT_NEAR(late.position[0], 2.0f, 1e-5f);
  EXPECT_NEAR(late.position[2], 2.0f, 1e-5f);
  EXPECT_NEAR(late.rotation[1], s, 1e-5f);
  // Past the last key the pose is held.
  const auto past = pes::fdc::SampleCanm(c, 9.0f);
  EXPECT_NEAR(past.position[2], 4.0f, 1e-5f);
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
      if (c.channels[0].frames.size() != c.frameCount + 1u) ++bad;
    }
  }
  std::cout << "[fdc canm install] embedded streams " << streams << " bad " << bad << "\n";
  EXPECT_GT(streams, 0u);
  EXPECT_EQ(bad, 0u);
}
