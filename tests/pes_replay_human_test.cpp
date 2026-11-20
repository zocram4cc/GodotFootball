// The ReplayHuman frame read (FUN_14217DEF0) run from the translated layer on PES-layout state
// (src/pes/replay_human.*). The install supplies the constants and globals the function reads, so the
// tests skip without PES21_ROOT.

#include <gtest/gtest.h>

#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/replay_human.hpp"
#include "pes_state.hpp"

namespace {

constexpr uint64_t kCapturePage = 0x1000;

// The capture the copy tests take their pre-state from: GF_PES_COPY_CAPTURE names an unpacked rendezvous capture of a
// frame that copies. tools/dev/translate/copy_livein.py writes its maps.txt (the VA runs) and livein.tsv: the pages the
// frame reads before writing them (plus the blocks its allocator answers), the copy sources, and the outermost allocator
// answers in call order. PES-derived and never committed: the tests skip without it.
struct CopyCapture {
  SnapView snap;
  std::vector<pes::PreStatePage> pages;
  std::vector<uint64_t> sources;
  std::vector<pes::AllocAnswer> answers;
  std::vector<uint8_t> record;  // the frame's record: kCapturePage bytes from the record's address
  std::vector<uint8_t> after;   // PES's record palette entry after the call (after.bin)
  std::vector<uint8_t> palette_entry;  // the palette entry the call's human selects (palette.bin)
  uint32_t human = 0, guard = 0;  // the call's own inputs (copy_livein.py's I line)
  uint8_t active = 1, palette = 0;
  uint16_t frameIndex = 0;  // the frame index: the low word of the frame key (the unit frame the palette byte is read from)
};

bool LoadCopyCapture(const std::string& dir, CopyCapture& cap, std::string* error) {
  std::ifstream paletteFile(dir + "/palette.bin", std::ios::binary);
  cap.palette_entry.assign(std::istreambuf_iterator<char>(paletteFile), std::istreambuf_iterator<char>());
  std::ifstream afterFile(dir + "/after.bin", std::ios::binary);
  cap.after.assign(std::istreambuf_iterator<char>(afterFile), std::istreambuf_iterator<char>());
  if (cap.after.size() != pes::kPaletteEntryBytes) {
    *error = "no after.bin in " + dir;
    return false;
  }
  cap.snap.fd = open((dir + "/mem.bin").c_str(), O_RDONLY);
  if (cap.snap.fd < 0) {
    *error = "no mem.bin in " + dir;
    return false;
  }
  std::ifstream maps(dir + "/maps.txt");
  unsigned long long va = 0, size = 0, off = 0;
  std::string line;
  while (std::getline(maps, line)) {
    std::istringstream row(line);
    if (row >> std::hex >> va >> size >> off) cap.snap.runs.push_back({va, size, off});
  }
  std::sort(cap.snap.runs.begin(), cap.snap.runs.end(),
            [](const SnapView::Run& a, const SnapView::Run& b) { return a.va < b.va; });
  std::ifstream livein(dir + "/livein.tsv");
  while (std::getline(livein, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    char kind = 0;
    row >> kind;
    if (kind == 'I') {
      unsigned human = 0, guard = 0, active = 0, palette = 0, frameIndex = 0;
      row >> human >> std::hex >> guard >> std::dec >> active >> palette >> frameIndex;
      cap.human = human;
      cap.guard = guard;
      cap.active = static_cast<uint8_t>(active);
      cap.palette = static_cast<uint8_t>(palette);
      cap.frameIndex = static_cast<uint16_t>(frameIndex);
      continue;
    }
    unsigned long long value = 0;
    row >> std::hex >> value;
    if (kind == 'S') {
      cap.sources.push_back(value);
      continue;
    }
    if (kind == 'R') {
      cap.record.resize(kCapturePage);
      for (uint64_t i = 0; i < kCapturePage; ++i) {
        const uint8_t* b = cap.snap.at(value + i);
        cap.record[i] = b != nullptr ? *b : 0;
      }
      continue;
    }
    if (kind == 'A') {
      unsigned long long va = 0;
      row >> va;
      cap.answers.push_back({value, va});
      continue;
    }
    if (kind != 'P') continue;
    if (cap.snap.at(value) == nullptr) continue;  // a page the capture does not map: the frame reads nothing there
    pes::PreStatePage page;
    page.va = value;
    page.bytes.resize(kCapturePage);
    for (uint64_t i = 0; i < kCapturePage; ++i) {
      const uint8_t* b = cap.snap.at(value + i);
      page.bytes[i] = b != nullptr ? *b : 0;
    }
    cap.pages.push_back(std::move(page));
  }
  return true;
}

// The frame over the pre-state of the capture in dir (a copy_livein.py directory), or over no capture when dir is null (the
// synthetic frame). Returns null without PES21_ROOT, or when the capture cannot be read.
std::unique_ptr<pes::ReplayHumanFrame> MakeFrameFrom(const char* dir, CopyCapture* out) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return nullptr;
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  if (!exe) return nullptr;
  CopyCapture cap;
  if (dir != nullptr && !LoadCopyCapture(dir, cap, &err)) return nullptr;
  auto f = pes::ReplayHumanFrame::Create(*exe, &err, cap.pages, cap.answers, cap.record);
  if (out != nullptr) *out = std::move(cap);
  return f;
}

// The frame over GF_PES_COPY_CAPTURE's pre-state when out is set (the copy tests: the caller skips when the variable is unset);
// without out, the synthetic frame the gating tests use.
std::unique_ptr<pes::ReplayHumanFrame> MakeFrame(CopyCapture* out = nullptr) {
  if (out == nullptr) return MakeFrameFrom(nullptr, nullptr);
  const char* dir = std::getenv("GF_PES_COPY_CAPTURE");
  if (dir == nullptr || *dir == '\0') return nullptr;
  return MakeFrameFrom(dir, out);
}

// A palette entry whose compare word (+0x4, the dword at entry+0x84) is non-zero, so the copy is taken.
pes::ReplayFrameInputs PresentHuman() {
  pes::ReplayFrameInputs in;
  for (uint32_t i = 0; i < pes::kPaletteEntryBytes; ++i) in.entry[i] = static_cast<uint8_t>(i * 7 + 3);
  in.entry[4] = 1;  // the compare word
  in.entry[5] = in.entry[6] = in.entry[7] = 0;
  return in;
}

// The call's own inputs (copy_livein.py's I line: the human, its guard, the record's active byte and the human's palette
// index), with the palette entry PresentHuman gives (the copy takes its source from the registry).
pes::ReplayFrameInputs FromCapture(const CopyCapture& cap) {
  pes::ReplayFrameInputs in = PresentHuman();
  in.human = cap.human;
  in.guard = cap.guard;
  in.active = cap.active;
  in.paletteIndex = cap.palette;
  in.frameIndex = cap.frameIndex;
  if (!cap.palette_entry.empty()) std::copy(cap.palette_entry.begin(), cap.palette_entry.end(), in.entry.begin());
  return in;
}

// The record's bytes when nothing was copied.
const std::array<uint8_t, pes::kPaletteEntryBytes> kZero{};

std::vector<uint8_t> AsVector(const std::array<uint8_t, pes::kPaletteEntryBytes>& a) { return {a.begin(), a.end()}; }
}  // namespace

// The copy the capture's call makes. PES's own post-call record (after.bin) is the expected value, not the palette source:
// the copy routine writes parts of the entry it copies (zeroes some fields, takes others), so the source bytes differ.
TEST(PesReplayHuman, CopiesThePaletteEntryWhenTheHumanIsPresent) {
  if (!std::getenv("GF_PES_COPY_CAPTURE")) GTEST_SKIP() << "GF_PES_COPY_CAPTURE not set: the copy needs the seq205 pre-state";
  CopyCapture cap;
  auto f = MakeFrame(&cap);
  if (!f) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::ReplayHumanRecord rec = f->Read(FromCapture(cap));
  EXPECT_EQ(AsVector(rec.bytes), cap.after);
  // The release at the end of the copy leaves the record in its released state (-1).
  EXPECT_EQ(rec.word1c, 0xFFFFFFFFu);
}

TEST(PesReplayHuman, AnAbsentHumanLeavesTheRecordAlone) {
  auto f = MakeFrame();
  if (!f) GTEST_SKIP() << "PES21_ROOT not set";
  pes::ReplayFrameInputs in = PresentHuman();
  in.guard = 0x7FFF;  // the frame's marker for an absent human
  const pes::ReplayHumanRecord rec = f->Read(in);
  EXPECT_EQ(rec.bytes, kZero);
  EXPECT_EQ(rec.word1c, 0xFFFFFFFFu);  // the idle state every real entry holds (-1); an absent human leaves it alone
}

TEST(PesReplayHuman, AnInactiveRecordLeavesItAlone) {
  auto f = MakeFrame();
  if (!f) GTEST_SKIP() << "PES21_ROOT not set";
  pes::ReplayFrameInputs in = PresentHuman();
  in.active = 0;
  const pes::ReplayHumanRecord rec = f->Read(in);
  EXPECT_EQ(rec.bytes, kZero);
}

TEST(PesReplayHuman, APaletteIndexBeyondTheCountIsNotCopied) {
  auto f = MakeFrame();
  if (!f) GTEST_SKIP() << "PES21_ROOT not set";
  pes::ReplayFrameInputs in = PresentHuman();
  in.paletteIndex = 0x27;  // the first index past the palette count
  const pes::ReplayHumanRecord rec = f->Read(in);
  EXPECT_EQ(rec.bytes, kZero);
}

TEST(PesReplayHuman, AnEntryWithoutTheCompareWordIsNotCopied) {
  auto f = MakeFrame();
  if (!f) GTEST_SKIP() << "PES21_ROOT not set";
  pes::ReplayFrameInputs in = PresentHuman();
  in.entry[4] = 0;  // the compare word is zero: the function takes its no-copy exit
  const pes::ReplayHumanRecord rec = f->Read(in);
  EXPECT_EQ(rec.bytes, kZero);
}

// The record keeps the copied entry across the reads that do not copy, as PES's record does: an absent human and an inactive
// record leave it alone. The expected bytes are PES's own post-call record (after.bin). After the copy the release leaves
// +0x1C at -1 and +0x21 at 0.
TEST(PesReplayHuman, ASequenceKeepsTheCopiedEntryAcrossTheReadsThatDoNotCopy) {
  if (!std::getenv("GF_PES_COPY_CAPTURE")) GTEST_SKIP() << "GF_PES_COPY_CAPTURE not set: the copy needs the seq205 pre-state";
  CopyCapture cap;
  auto f = MakeFrame(&cap);
  if (!f) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::ReplayFrameInputs first = FromCapture(cap);
  pes::ReplayHumanRecord rec = f->Read(first);
  EXPECT_EQ(AsVector(rec.bytes), cap.after);
  EXPECT_EQ(rec.word1c, 0xFFFFFFFFu);
  EXPECT_EQ(rec.byte21, 0);

  pes::ReplayFrameInputs absent = first;
  absent.guard = 0x7FFF;
  rec = f->Read(absent);
  EXPECT_EQ(AsVector(rec.bytes), cap.after);

  pes::ReplayFrameInputs inactive = first;
  inactive.active = 0;
  rec = f->Read(inactive);
  EXPECT_EQ(AsVector(rec.bytes), cap.after);
}

// A second read of a record that holds a copied entry, with a changed palette entry: the copy gate takes the copy, the release
// runs on the record's object (R+0x48), and the new entry replaces the stored one. No real call of this path is captured yet
// (the five second reads of pid2998104 skip the copy: their palette entry did not change). GF_PES_RELEASE_CAPTURE names a
// PES replay of such a state: pid2998104/seq567 with its stored entry's first byte flipped (synth.py), run through
// copy_livein.py, which writes its after.bin from PES's own bytes.
TEST(PesReplayHuman, AChangedEntryOnASecondReadReplacesTheCopiedEntryThroughTheRelease) {
  const char* dir = std::getenv("GF_PES_RELEASE_CAPTURE");
  if (dir == nullptr || *dir == '\0') {
    GTEST_SKIP() << "GF_PES_RELEASE_CAPTURE not set: the release path needs a second read with a changed entry (the rig hook)";
  }
  CopyCapture cap;
  auto f = MakeFrameFrom(dir, &cap);
  if (!f) GTEST_SKIP() << "PES21_ROOT not set, or the release capture is unreadable";
  const pes::ReplayHumanRecord rec = f->Read(FromCapture(cap));
  EXPECT_EQ(AsVector(rec.bytes), cap.after);
  EXPECT_EQ(rec.word1c, 0xFFFFFFFFu);
  EXPECT_EQ(rec.byte21, 0);
}

// The frame table holds 300-byte human records (FUN_14217DEF0 strides by 300). Human 0 sits at offset 0, so only another
// index shows the stride. Human 2 is marked absent in its own record; a port that wrote it at another stride would leave
// the function reading an unmarked record there, take the copy, and fail.
TEST(PesReplayHuman, AnAbsentHumanAtAnIndexOtherThanZeroIsFoundAtItsOwnFrameRecord) {
  auto f = MakeFrame();
  if (!f) GTEST_SKIP() << "PES21_ROOT not set";
  pes::ReplayFrameInputs in = PresentHuman();
  in.human = 2;
  in.guard = 0x7FFF;
  const pes::ReplayHumanRecord rec = f->Read(in);
  EXPECT_EQ(rec.bytes, kZero);
}
