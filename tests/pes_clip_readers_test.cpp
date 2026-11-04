// PES's clip-record readers (pes/translated: 0x1406AF620 blend, 0x1406AF8E0 cancel, 0x1406B1F00 RotData start, 0x1406B1D00
// RotData end) run over the motion table GF files from Mbinfo (onthepitch/pesclipreaders.hpp). Every expectation is a
// recompute from the decoded record of the same raw words (a relation, never a pinned value): the translated code and the
// decode must agree on the whole table, at several tick rates and rates.
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "onthepitch/pesclipreaders.hpp"
#include "pes/mbinfo.hpp"

namespace {

using pes::ClipReader;

template <typename T>
void Put(std::vector<uint8_t>& b, size_t at, T v) {
  if (b.size() < at + sizeof v) b.resize(at + sizeof v);
  std::memcpy(&b[at], &v, sizeof v);
}

const pes::MotionCaps kCaps{/*animation*/ 1, /*subHit*/ 6, /*cancel*/ 5, /*rot*/ 3, /*foot*/ 20, /*pivot*/ 1};

// int(2 v / rate * (ticks / 60) + 0.5) in single precision, a rate that is not positive being 1.0.
int Ticks(int v, float rate, float ticksPerSecond) {
  if (rate <= 0.0f) rate = 1.0f;
  return static_cast<int>((static_cast<float>(v) + static_cast<float>(v)) / rate * (ticksPerSecond / 60.0f) + 0.5f);
}

// What the decode says each reader answers for (motion, index) at a rate and tick rate; past the list, the fallback record.
int Expected(const pes::MotionRecord& r, ClipReader reader, int index, float rate, float tps) {
  switch (reader) {
    case ClipReader::kBlend: return Ticks(r.BlendStart(), rate, tps);
    case ClipReader::kCancel: return Ticks(r.CancelFrames(index), rate, tps);
    case ClipReader::kRotStart: return Ticks(r.Rot(index).start, rate, tps);
    case ClipReader::kRotEnd: return Ticks(r.Rot(index).end, rate, tps);
  }
  return 0;
}

constexpr ClipReader kReaders[] = {ClipReader::kBlend, ClipReader::kCancel, ClipReader::kRotStart, ClipReader::kRotEnd};
constexpr float kRates[] = {1.0f, 0.85f, 2.5f, 0.0f, -1.0f};
constexpr float kTicksPerSecond[] = {54.0f, 60.0f};

void ExpectTableAnswersTheDecode(const pes::Mbinfo& m, const pes::PesClipReaders& readers, int motion) {
  const pes::MotionRecord& r = m.Motion(motion);
  for (const ClipReader reader : kReaders)
    for (const float rate : kRates)
      for (const float tps : kTicksPerSecond)
        for (int index = 0; index < 6; ++index)  // past each list's cap too: the fallback record
          ASSERT_EQ(readers.Read(reader, motion, rate, index, tps), Expected(r, reader, index, rate, tps))
              << "motion " << motion << " reader " << int(reader) << " rate " << rate << " tps " << tps << " index " << index;
}

TEST(PesClipReaders, EveryReaderAnswersTheDecodedRecordOnSyntheticWords) {
  pes::MbinfoBins bins;
  bins.animation.assign(36, 0);
  bins.hitData.assign(108, 0);
  bins.cancelData.assign(24, 0);
  bins.rotData.assign(16, 0);
  Put<uint32_t>(bins.animation, 0, 7u | (63u << 13));
  Put<uint32_t>(bins.animation, 4, 360u << 10);                 // blend value 360
  Put<uint32_t>(bins.cancelData, 0, 7u | (40u << 13));          // cancel frame 40
  Put<uint32_t>(bins.cancelData, 12, 7u | (57u << 13));         // a second record
  Put<uint32_t>(bins.rotData, 0, 7u | (300u << 13));
  Put<uint32_t>(bins.rotData, 4, 100u | (700u << 10));          // end 101, start 700 - 517
  Put<uint32_t>(bins.rotData, 8, 7u);
  Put<uint32_t>(bins.rotData, 12, 5u | (600u << 10));           // a second RotData record
  std::string err;
  auto m = pes::ParseMbinfo(bins, kCaps, &err);
  ASSERT_TRUE(m.has_value()) << err;
  const pes::PesClipReaders readers(*m);
  ExpectTableAnswersTheDecode(*m, readers, 7);
  ExpectTableAnswersTheDecode(*m, readers, 8);                  // a motion with no record: the fallback
  ExpectTableAnswersTheDecode(*m, readers, 0x1124);             // the last entry of the block
  ExpectTableAnswersTheDecode(*m, readers, 0x1125);             // the default entry past the block
  ExpectTableAnswersTheDecode(*m, readers, 0x7fff);
}

TEST(PesClipReaders, ACancelListKeepsItsCapAndFileOrder) {
  pes::MbinfoBins bins;
  bins.animation.assign(36, 0);
  bins.hitData.assign(108, 0);
  bins.cancelData.assign(12 * 7, 0);
  for (uint32_t i = 0; i < 7; ++i) Put<uint32_t>(bins.cancelData, 12 * i, 3u | ((10u + i) << 13));
  std::string err;
  auto m = pes::ParseMbinfo(bins, kCaps, &err);
  ASSERT_TRUE(m.has_value()) << err;
  const pes::PesClipReaders readers(*m);
  EXPECT_EQ(readers.Read(ClipReader::kCancel, 3, 1.0f, 4, 60.0f), Ticks(14, 1.0f, 60.0f));  // the cap's last record
  EXPECT_EQ(readers.Read(ClipReader::kCancel, 3, 1.0f, 5, 60.0f), Ticks(0, 1.0f, 60.0f));   // past the cap: the fallback, not record 5
  ExpectTableAnswersTheDecode(*m, readers, 3);
}

TEST(PesClipReadersInstall, TheWholeInstallTableAnswersTheDecode) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto m = pes::LoadMbinfo(vfs, *exe, &err);
  ASSERT_TRUE(m.has_value()) << err;
  const pes::PesClipReaders readers(*m);
  int withRecords = 0;
  for (int motion = 0; motion < int(pes::kClipTableSlots); motion += 7) {  // every 7th motion keeps the run short
    const pes::MotionRecord& r = m->Motion(motion);
    withRecords += !r.rotWords.empty() || !r.cancelWords.empty() || !r.blendWords.empty();
    for (const ClipReader reader : kReaders)
      for (int index = 0; index < 4; ++index)
        ASSERT_EQ(readers.Read(reader, motion, 1.0f, index, 54.0f), Expected(r, reader, index, 1.0f, 54.0f))
            << "motion " << motion << " reader " << int(reader) << " index " << index;
  }
  EXPECT_GT(withRecords, 0);
}

}  // namespace
