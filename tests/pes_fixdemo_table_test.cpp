#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pes/fixdemo_table.hpp"
#include "pes/vfs.hpp"

namespace {

// A table built from the container layout (pes-re-cutscenes.md §2.1-2.2): one version-1 cut group with one
// variant, one scene with one step. `sectionEnd` is the header's read count; the decoder must reject any value
// other than the true read total (28 here).
struct Built {
  std::vector<uint8_t> bytes;
};

Built BuildTable(uint32_t sectionEnd, uint32_t nU = 6) {
  const std::vector<float> A = {2.5f, 0.5f};
  const std::vector<uint32_t> B = {7, 9, 0x30001, 0};
  // W: version marker, group count, group slice (0,1), row count, row key, scene count, scene slice (0,1),
  // step count, step w0, step w1, step w2.
  const std::vector<uint16_t> W = {1, 1, 0, 1, 1, 0x0E01, 1, 0, 1, 1, 60, 0xFFFF, 0xFFFF};
  // U: row nKeys, nStr, then the step's kind, c0, c1, trip count.
  const std::vector<uint8_t> U = {1, 1, 3, 0, 0, 0};
  // S: group name, row layer, scene name.
  const std::vector<std::string> S = {"G", "f.fdc", "SC"};
  std::string chars;
  std::vector<uint32_t> ends;
  for (const std::string& s : S) {
    chars += s;
    ends.push_back(static_cast<uint32_t>(chars.size()));
  }
  const uint32_t nW = static_cast<uint32_t>(W.size());

  Built out;
  std::vector<uint8_t>& b = out.bytes;
  auto put = [&b](const void* p, size_t n) {
    const auto* c = static_cast<const uint8_t*>(p);
    b.insert(b.end(), c, c + n);
  };
  const uint32_t header[14] = {
      static_cast<uint32_t>(A.size()), static_cast<uint32_t>(S.size()), 1, static_cast<uint32_t>(B.size()), nW,
      nU, static_cast<uint32_t>(chars.size()), 0, 0, 0, 0, 0, 0, 0};
  b.resize(sizeof header);
  uint32_t h[14];
  std::memcpy(h, header, sizeof h);
  h[7] = static_cast<uint32_t>(b.size());
  put(A.data(), A.size() * 4);
  h[8] = static_cast<uint32_t>(b.size());
  put(ends.data(), ends.size() * 4);
  h[9] = static_cast<uint32_t>(b.size());
  put(&sectionEnd, 4);
  h[10] = static_cast<uint32_t>(b.size());
  put(B.data(), B.size() * 4);
  h[11] = static_cast<uint32_t>(b.size());
  put(W.data(), W.size() * 2);
  h[12] = static_cast<uint32_t>(b.size());
  put(U.data(), U.size());
  h[13] = static_cast<uint32_t>(b.size());
  put(chars.data(), chars.size());
  std::memcpy(b.data(), h, sizeof h);
  return out;
}

constexpr uint32_t kTrueReads = 2 + 4 + 13 + 6 + 3;  // A + B + W + U + S

}  // namespace

TEST(FixDemoTable, DecodesVersionOneGroupsScenesAndSteps) {
  const Built t = BuildTable(kTrueReads);
  std::string err;
  auto d = pes::DecodeFixDemoTable(t.bytes, 2, &err);
  ASSERT_TRUE(d) << err;
  EXPECT_EQ(d->version, 1);
  ASSERT_EQ(d->cutGroups.size(), 1u);
  EXPECT_EQ(d->cutGroups[0].id, 0x20001u);
  EXPECT_EQ(d->cutGroups[0].name, "G");
  ASSERT_EQ(d->cutGroups[0].rows.size(), 1u);
  const pes::FixDemoCutRow& row = d->cutGroups[0].rows[0];
  EXPECT_EQ(row.keys, std::vector<uint16_t>{0x0E01});
  EXPECT_EQ(row.fdc, std::vector<std::string>{"f.fdc"});
  EXPECT_EQ(row.b0, 7u);
  EXPECT_EQ(row.f, 2.5f);
  EXPECT_EQ(row.b1, 9u);
  ASSERT_EQ(d->scenes.size(), 1u);
  EXPECT_EQ(d->scenes[0].id, 0x20001u);
  EXPECT_EQ(d->scenes[0].name, "SC");
  ASSERT_EQ(d->scenes[0].steps.size(), 1u);
  const pes::FixDemoStep& st = d->scenes[0].steps[0];
  EXPECT_EQ(st.kind, 3);
  EXPECT_EQ(st.fade, 0.5f);
  EXPECT_EQ(st.cut, 0x30001u);
  EXPECT_EQ(st.w0, 60);
  EXPECT_EQ(st.w1, -1);
  EXPECT_EQ(st.w2, -1);
}

// Mutation: the header's section-end count one read short of the true total. The decoder must refuse it.
TEST(FixDemoTable, RejectsSectionEndThatIsNotTheReadTotal) {
  const Built t = BuildTable(kTrueReads - 1);
  std::string err;
  EXPECT_FALSE(pes::DecodeFixDemoTable(t.bytes, 2, &err));
  EXPECT_FALSE(err.empty());
}

TEST(FixDemoTable, RejectsAStreamThatIsNotFullyConsumed) {
  const Built t = BuildTable(kTrueReads, /*nU=*/7);
  std::string err;
  EXPECT_FALSE(pes::DecodeFixDemoTable(t.bytes, 2, &err));
}

TEST(FixDemoTable, RejectsTruncatedFile) {
  Built t = BuildTable(kTrueReads);
  t.bytes.resize(t.bytes.size() - 4);
  std::string err;
  EXPECT_FALSE(pes::DecodeFixDemoTable(t.bytes, 2, &err));
}

// Install gate: decodes every category's table through the mounted VFS (the winner of any mod is the one read).
// Relations only, no pinned counts: each table must decode, and every step's cut must name a group of its own
// category or be 0 (the terminator).
TEST(FixDemoTable, InstallTablesDecodeExactlyAndCutsStayInCategory) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  for (int cat = 1; cat <= 9; ++cat) {
    auto bytes = vfs.Read(pes::FixDemoTablePath(cat), &err);
    ASSERT_TRUE(bytes) << pes::FixDemoTablePath(cat) << ": " << err;
    auto d = pes::DecodeFixDemoTable(*bytes, cat, &err);
    ASSERT_TRUE(d) << "cat " << cat << ": " << err;
    EXPECT_FALSE(d->scenes.empty()) << "cat " << cat;
    for (const pes::FixDemoScene& sc : d->scenes) {
      EXPECT_EQ(sc.id >> 16, static_cast<uint32_t>(cat));
      for (const pes::FixDemoStep& st : sc.steps) {
        if (st.cut == 0) break;
        EXPECT_EQ(st.cut >> 16, static_cast<uint32_t>(cat)) << sc.name;
      }
    }
  }
}

TEST(FixDemoTable, CutDataPathsAreUnderTheCategoryCutDataAndMountedInTheInstall) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  // Scene 0x20009 is FOUL_PINCH_CARD_Y (category 2, foul; pes-re-cutscenes.md §2.3): the layers its steps' variants name.
  auto bytes = vfs.Read(pes::FixDemoTablePath(2), &err);
  ASSERT_TRUE(bytes) << err;
  auto table = pes::DecodeFixDemoTable(*bytes, 2, &err);
  ASSERT_TRUE(table) << err;
  size_t checked = 0;
  for (const pes::FixDemoStep& step : table->scenes[0x20009 - 0x20001].steps) {
    if (step.cut == 0) break;
    for (const pes::FixDemoCutRow& row : table->cutGroups[(step.cut & 0xFFFF) - 1].rows) {
      for (const std::string& name : row.fdc) {
        const std::string p = pes::FixDemoCutDataPath(2, name);
        EXPECT_EQ(p.rfind("common/demo/fixdemo/foul/cut_data/", 0), 0u) << p;
        EXPECT_TRUE(vfs.Exists(p)) << p;
        ++checked;
      }
    }
  }
  EXPECT_GT(checked, 0u);
  EXPECT_TRUE(pes::FixDemoCutDataPath(0, "x.fdc").empty());
}
