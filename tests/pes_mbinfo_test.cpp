// PES's Mbinfo bins (pes/mbinfo.hpp): synthetic records check each layout's bit fields and the
// builders' per-motion caps; install tests read the user's bins and check the motion table
// against the raw files.
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>

#include "pes/mbinfo.hpp"

namespace {

template <typename T>
void Put(std::vector<uint8_t>& b, size_t at, T v) {
  if (b.size() < at + sizeof v) b.resize(at + sizeof v);
  std::memcpy(&b[at], &v, sizeof v);
}

const pes::MotionCaps kSyntheticCaps{/*animation*/ 1, /*subHit*/ 6, /*cancel*/ 5,
                                     /*rot*/ 3, /*foot*/ 20, /*pivot*/ 1};

TEST(PesMbinfo, DecodesTheThreeLayouts) {
  pes::MbinfoBins bins;
  bins.animation.assign(36, 0);
  bins.hitData.assign(108, 0);
  bins.cancelData.assign(12, 0);
  Put<uint32_t>(bins.animation, 0, 5u | (63u << 13));  // motion 5, end frame 64
  Put<uint32_t>(bins.animation, 4, 360u | (50u << 10));
  Put<float>(bins.animation, 28, 1.5f);  // cancel_speed
  Put<uint32_t>(bins.hitData, 0, 5u | (12u << 13) | (9u << 27));
  Put<float>(bins.hitData, 0x24, 106.0f);
  Put<float>(bins.hitData, 0x28, 168.5f);
  Put<float>(bins.hitData, 0x2c, 60.0f);
  Put<float>(bins.hitData, 0x44, -4.0f);
  Put<uint32_t>(bins.cancelData, 0, 5u | (40u << 13));
  std::string err;
  auto m = pes::ParseMbinfo(bins, kSyntheticCaps, &err);
  ASSERT_TRUE(m.has_value()) << err;
  ASSERT_NE(m->Timing(5), nullptr);
  EXPECT_EQ(m->Timing(5)->endFrame, 64);
  EXPECT_EQ(m->Timing(5)->blendStart, 50);
  EXPECT_FLOAT_EQ(m->Timing(5)->cancelSpeed, 1.5f);
  ASSERT_EQ(m->Hits(5).size(), 1u);
  EXPECT_EQ(m->Hits(5)[0].frame, 12);
  EXPECT_EQ(m->Hits(5)[0].contactPart, 10);
  EXPECT_FLOAT_EQ(m->Hits(5)[0].ballY, 168.5f);
  EXPECT_FLOAT_EQ(m->Hits(5)[0].ballX, 60.0f);
  EXPECT_FLOAT_EQ(m->Hits(5)[0].yaw_deg, -4.0f);
  ASSERT_NE(m->Cancel(5), nullptr);
  EXPECT_EQ(m->Cancel(5)->frame, 40);
  EXPECT_EQ(m->Timing(4), nullptr);
  EXPECT_TRUE(m->Hits(4).empty());
  EXPECT_TRUE(m->Hits(99999).empty());
}

TEST(PesMbinfo, RejectsTruncatedFiles) {
  std::string err;
  pes::MbinfoBins bins;
  bins.animation.assign(35, 0);
  EXPECT_FALSE(pes::ParseMbinfo(bins, kSyntheticCaps, &err).has_value());
  pes::MbinfoBins hits;
  hits.hitData.assign(100, 0);
  EXPECT_FALSE(pes::ParseMbinfo(hits, kSyntheticCaps, &err).has_value());
}

TEST(PesMbinfoMotionTable, RotDataReadsItsThreeFieldsAndKeepsTheCap) {
  // Word 0: motion 5 at bits 0..12, offset field at bits 13..23. Word 1: start at bits 10..19,
  // end at bits 0..9. Four records on motion 5 with a cap of three: the fourth is dropped.
  pes::MbinfoBins bins;
  for (uint32_t i = 0; i < 4; ++i) {
    Put<uint32_t>(bins.rotData, 8 * i, 5u | ((700u + i) << 13));
    Put<uint32_t>(bins.rotData, 8 * i + 4, ((600u + i) << 10) | (20u + i));
  }
  std::string err;
  auto m = pes::ParseMbinfo(bins, kSyntheticCaps, &err);
  ASSERT_TRUE(m.has_value()) << err;
  const pes::MotionRecord& r = m->Motion(5);
  ASSERT_EQ(r.rot.size(), 3u);
  EXPECT_EQ(r.Rot(0).offset, 700 - 878);
  EXPECT_EQ(r.Rot(0).start, 600 - 517);
  EXPECT_EQ(r.Rot(0).end, 20 + 1);
  EXPECT_EQ(r.Rot(2).end, 22 + 1);
  // A missing record reads as PES's zero-filled fallback: its zero word gives these values.
  const pes::RotRecord fallback = r.Rot(3);
  EXPECT_EQ(fallback.offset, 0 - 878);
  EXPECT_EQ(fallback.start, 0 - 517);
  EXPECT_EQ(fallback.end, 1);
}

TEST(PesMbinfoMotionTable, FootTypeAndPivotUseTheirOwnMotionOffsets) {
  // FootType: motion = (w & 0x1fff) + 0x10, foot pair at bits 22..23.
  // PivothitInfo: motion = (w & 0xfff) + 0x20b, flag at bit 29.
  pes::MbinfoBins bins;
  bins.footType.assign(4, 0);
  bins.pivothitInfo.assign(32, 0);
  Put<uint32_t>(bins.footType, 0, 5u | (2u << 22));
  Put<uint32_t>(bins.pivothitInfo, 0, 1u | (1u << 29));
  Put<uint32_t>(bins.pivothitInfo, 16, 1u);
  std::string err;
  auto m = pes::ParseMbinfo(bins, kSyntheticCaps, &err);
  ASSERT_TRUE(m.has_value()) << err;
  EXPECT_EQ(m->Motion(5).FootCount(), 0);
  EXPECT_EQ(m->Motion(5 + 0x10).FootCount(), 1);
  EXPECT_EQ(m->Motion(5 + 0x10).FootPair(0), 2);
  EXPECT_EQ(m->Motion(5 + 0x10).FootPair(1), 0);
  EXPECT_TRUE(m->Motion(1 + 0x20b).PivotFlag(0));
  EXPECT_FALSE(m->Motion(1 + 0x20b).PivotFlag(1));
  EXPECT_EQ(m->Motion(1 + 0x20b).pivot.size(), 1u);  // cap 1: the second record is dropped
}

TEST(PesMbinfoMotionTable, SubHitDataUsesItsOwnMotionOffsetAndKeepsEachWordsRecord) {
  // SubHitData: motion = (w0 & 0x7ff) + 0x61, cap 6.
  pes::MbinfoBins bins;
  bins.subHitData.assign(32, 0);
  Put<uint32_t>(bins.subHitData, 0, 3u);
  Put<uint32_t>(bins.subHitData, 4, 0xdeadbeefu);
  std::string err;
  auto m = pes::ParseMbinfo(bins, kSyntheticCaps, &err);
  ASSERT_TRUE(m.has_value()) << err;
  ASSERT_EQ(m->Motion(3 + 0x61).sub.size(), 1u);
  EXPECT_EQ(m->Motion(3 + 0x61).sub[0].words[1], 0xdeadbeefu);
  EXPECT_TRUE(m->Motion(3).sub.empty());
}

TEST(PesMbinfoMotionTable, CancelDataFramesAndCountFeedTheReaders) {
  pes::MbinfoBins bins;
  bins.cancelData.assign(24, 0);
  Put<uint32_t>(bins.cancelData, 0, 9u | (40u << 13));
  Put<uint32_t>(bins.cancelData, 12, 9u | (57u << 13));
  std::string err;
  auto m = pes::ParseMbinfo(bins, kSyntheticCaps, &err);
  ASSERT_TRUE(m.has_value()) << err;
  EXPECT_EQ(m->Motion(9).CancelCount(), 2);
  EXPECT_EQ(m->Motion(9).CancelFrames(0), 40);
  EXPECT_EQ(m->Motion(9).CancelFrames(1), 57);
  EXPECT_EQ(m->Motion(9).CancelFrames(2), 0);  // past the end: PES's zero word
}

// The install's own bins, read with the loader and checked against the raw records. The oracle
// below decodes the raw words from the layouts in the RE report; it shares no code with the loader.
TEST(PesMbinfoInstall, TheMotionTableHoldsTheFirstRecordsOfEachMotionAsTheBinsSay) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto m = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(m.has_value()) << err;
  auto caps = pes::LoadMotionCaps(*exe, &err);
  ASSERT_TRUE(caps.has_value()) << err;

  auto rot = vfs.Read("common/anime/Mbinfo/bin/RotData.bin", &err);
  ASSERT_TRUE(rot.has_value()) << err;
  ASSERT_FALSE(rot->empty());
  // Oracle: every record of every motion, in file order, its first `cap` entries.
  std::vector<std::vector<uint32_t>> rotWords;  // per motion, the raw words of the kept records
  for (size_t o = 0; o + 8 <= rot->size(); o += 8) {
    uint32_t w0, w1;
    std::memcpy(&w0, &(*rot)[o], 4);
    std::memcpy(&w1, &(*rot)[o + 4], 4);
    const uint32_t motion = w0 & 0x1fff;
    if (rotWords.size() <= motion) rotWords.resize(motion + 1);
    if (rotWords[motion].size() < caps->rot * 2) {
      rotWords[motion].push_back(w0);
      rotWords[motion].push_back(w1);
    }
  }
  int checked = 0;
  for (size_t motion = 0; motion < rotWords.size() && motion < 0x1125; ++motion) {
    const auto& words = rotWords[motion];
    const pes::MotionRecord& r = m->Motion(int(motion));
    ASSERT_EQ(r.rot.size(), words.size() / 2) << "motion " << motion;
    for (size_t i = 0; i < r.rot.size(); ++i) {
      const uint32_t w0 = words[2 * i], w1 = words[2 * i + 1];
      EXPECT_EQ(r.Rot(int(i)).offset, int((w0 >> 13) & 0x7ff) - 878);
      EXPECT_EQ(r.Rot(int(i)).start, int((w1 >> 10) & 0x3ff) - 517);
      EXPECT_EQ(r.Rot(int(i)).end, int(w1 & 0x3ff) + 1);
    }
    if (++checked == 64) break;
  }
  EXPECT_GT(checked, 0);
}

TEST(PesMbinfoInstall, EveryKeptListIsNoLongerThanItsCap) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto m = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(m.has_value()) << err;
  auto caps = pes::LoadMotionCaps(*exe, &err);
  ASSERT_TRUE(caps.has_value()) << err;
  for (int motion = 0; motion < 0x1125; ++motion) {
    const pes::MotionRecord& r = m->Motion(motion);
    EXPECT_LE(r.blend.size(), caps->animation);
    EXPECT_LE(r.cancel.size(), caps->cancel);
    EXPECT_LE(r.rot.size(), caps->rot);
    EXPECT_LE(r.sub.size(), caps->subHit);
    EXPECT_LE(r.foot.size(), caps->foot);
    EXPECT_LE(r.pivot.size(), caps->pivot);
  }
}

}  // namespace
