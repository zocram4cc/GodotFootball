// The loader data PES's Trap chain reads (pestrapdata.hpp), built from the install, against the bytes the chain read in captured
// calls. Needs PES21_ROOT (the install: PES2021.exe and the VFS) and GF_PES_TRAP_READS, a directory of per-capture files made by
// tools/dev/translate/trap_reads.py (PES-derived, never committed):
//   <capture>.reads.tsv  every loader byte the call read, in logical coordinates (motion record, file record, cell, field)
//   <capture>.full.tsv   the whole Animetable buffer, the Trap runtime object and every kind-0/1/6/13 list of the capture
// Without either, skip. The captures' heap addresses differ from the window's, so a pointer is compared by what it reaches.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "onthepitch/pestrapdata.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace {

namespace fs = std::filesystem;
constexpr uint64_t kWindowGlobalVa = 0x1436F3DC8ULL;  // G's cell: window state, not loader data (the oracle binds it)
constexpr std::size_t kListCellBytes = 8;

using Row = std::vector<std::string>;

std::vector<Row> ReadTsv(const fs::path& path) {
  std::vector<Row> rows;
  std::ifstream f(path);
  for (std::string line; std::getline(f, line);) {
    Row row;
    std::stringstream ss(line);
    for (std::string col; std::getline(ss, col, '\t');) row.push_back(col);
    if (!row.empty()) rows.push_back(std::move(row));
  }
  return rows;
}

uint64_t Num(const std::string& s) { return std::strtoull(s.c_str(), nullptr, 10); }

std::vector<uint8_t> FromHex(const std::string& hex) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) out.push_back(uint8_t(std::strtoul(hex.substr(i, 2).c_str(), nullptr, 16)));
  return out;
}

class TrapData : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const char* root = std::getenv("PES21_ROOT");
    const char* reads = std::getenv("GF_PES_TRAP_READS");
    if (root == nullptr || *root == '\0' || reads == nullptr || *reads == '\0') return;
    std::string err;
    auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe.has_value()) << err;
    ASSERT_TRUE(vfs_.Mount(root, &err)) << err;
    exe_ = std::move(exe);
    data_ = pes::BuildTrapLoaderData(*exe_, vfs_, &err);
    ASSERT_TRUE(data_.has_value()) << err;
    for (const auto& e : fs::directory_iterator(reads))
      if (e.path().string().ends_with(".reads.tsv")) captures_.push_back(e.path().string().substr(0, e.path().string().size() - 10));
    std::sort(captures_.begin(), captures_.end());
  }
  void SetUp() override {
    if (!data_) GTEST_SKIP() << "PES21_ROOT / GF_PES_TRAP_READS unset (install and PES-derived captures, not committed)";
    ASSERT_FALSE(captures_.empty());
  }

  // Our byte at a window VA.
  static uint8_t Byte(uint64_t va) {
    const uint8_t* p = data_->At(va, 1);
    return p == nullptr ? 0xEE : *p;
  }
  static uint64_t Qword(uint64_t va) {
    const uint8_t* p = data_->At(va, 8);
    uint64_t v = 0;
    if (p != nullptr) std::memcpy(&v, p, sizeof v);
    return v;
  }
  static uint64_t Record(uint64_t motion) { return data_->motionRecordsVa + motion * pes::kMotionRecordBytes; }
  static const pes::TrapMbinfoKind* Kind(int kind) {
    for (const auto& k : pes::kTrapMbinfoKinds)
      if (k.kind == kind) return &k;
    return nullptr;
  }
  // The pointer cell of list(kind)[idx] in a motion record.
  static uint64_t ListPointer(uint64_t motion, int kind, uint64_t idx) {
    return Qword(Record(motion) + Kind(kind)->listOffset + 8 * idx);
  }

  static pes::PesVfs vfs_;
  static std::optional<pes::PeImage> exe_;
  static std::optional<pes::PesTrapLoaderData> data_;
  static std::vector<std::string> captures_;
};
pes::PesVfs TrapData::vfs_;
std::optional<pes::PeImage> TrapData::exe_;
std::optional<pes::PesTrapLoaderData> TrapData::data_;
std::vector<std::string> TrapData::captures_;

// Every byte of loader data a captured call read is produced from the install and equals the capture's.
TEST_F(TrapData, EveryLoaderByteTheChainReadIsProducedFromTheInstall) {
  size_t compared = 0;
  for (const std::string& capture : captures_) {
    SCOPED_TRACE(capture);
    // A pointer is compared by what it reaches: the capture's pointer bytes are heap addresses. A list pointer is non-null in
    // ours exactly when the capture's is, and every raw-record byte read through it (F rows) is compared below.
    std::map<std::pair<uint64_t, int>, bool> capturedListPointerNonNull;  // (motion, kind) -> list[0] read non-null
    for (const Row& r : ReadTsv(capture + ".reads.tsv")) {
      const std::string& tag = r[0];
      if (tag == "M") {
        const uint64_t motion = Num(r[1]), off = Num(r[2]);
        ASSERT_LT(motion, pes::kMotionSlotCount);
        bool inList = false;
        for (const auto& k : pes::kTrapMbinfoKinds) {
          if (off >= k.listOffset && off < k.listOffset + kListCellBytes) {
            inList = true;
            if (Num(r[3]) != 0) capturedListPointerNonNull[{motion, k.kind}] = true;
          }
        }
        if (!inList) {
          bool isCount = false;
          for (const auto& k : pes::kTrapMbinfoKinds) isCount |= off == uint64_t(k.kind);
          ASSERT_TRUE(isCount) << "motion " << motion << " record byte +" << off << " is read but not built";
          EXPECT_EQ(Byte(Record(motion) + off), Num(r[3])) << "motion " << motion << " count byte +" << off;
          ++compared;
        }
      } else if (tag == "F") {
        const uint64_t motion = Num(r[1]), idx = Num(r[3]), off = Num(r[4]);
        const int kind = int(Num(r[2]));
        ASSERT_NE(Kind(kind), nullptr) << "kind " << kind << " is read but not built";
        const uint64_t ptr = ListPointer(motion, kind, idx);
        ASSERT_NE(data_->At(ptr, Kind(kind)->recordBytes), nullptr) << "motion " << motion << " kind " << kind << " idx " << idx;
        EXPECT_EQ(Byte(ptr + off), Num(r[5])) << "motion " << motion << " kind " << kind << " idx " << idx << " +" << off;
        ++compared;
      } else if (tag == "C") {
        const uint64_t row = Num(r[1]), col = Num(r[2]), part = Num(r[3]), cell = row * 0x12 + col;
        const uint64_t at = part < 2 ? data_->animetableVa + 2 * cell + part
                                     : data_->animetableVa + 2 * pes::kAnimetableCellCount + cell;
        EXPECT_EQ(Byte(at), Num(r[4])) << "Animetable (" << row << ", " << col << ") part " << part;
        ++compared;
      } else if (tag == "P") {
        const uint64_t off = Num(r[1]);
        ASSERT_TRUE(pes::TrapRuntimeByteIsBuilt(off)) << "Trap +0x" << std::hex << off << " is read but not built";
        EXPECT_EQ(Byte(data_->trapVa + off), Num(r[2])) << "Trap +0x" << std::hex << off;
        ++compared;
      } else if (tag == "T" || tag == "G" || tag == "R" || tag == "E" || tag == "A") {
        // The pointers between the globals and the data: each is read as an 8-byte cell and must reach the next object.
        const uint64_t off = Num(r[1]);
        if (tag == "T") {
          ASSERT_LT(off, 8u) << "motion-table singleton +" << off << " is read but not built";
          EXPECT_EQ(Qword(data_->motionTableVa), data_->motionRecordsVa);
        } else if (tag == "G") {
          ASSERT_LT(off, 8u) << "anime manager +" << off << " is read but not built";
          EXPECT_EQ(Qword(data_->animeManagerVa), data_->animetableVa);
        } else if (tag == "R") {
          ASSERT_LT(off, 8u) << "registry +" << off << " is read but not built";
        } else if (tag == "A") {
          ASSERT_EQ(Num(r[1]), pes::kTrapRegistryId) << "registry entry " << r[1] << " is read but not built";
        } else {
          ASSERT_TRUE(off >= 8 && off < 16) << "registry entry +" << off << " is read but not built";
        }
        ++compared;
      } else if (tag == "D") {
        ADD_FAILURE() << "the default motion record is read (+" << r[1] << "): not built";
      } else if (tag == "X") {
        const uint64_t va = Num(r[1]);
        bool known = va >= kWindowGlobalVa && va < kWindowGlobalVa + 8;
        for (const auto& [gva, value] : data_->globals) known |= va >= gva && va < gva + 8;
        EXPECT_TRUE(known) << "image global 0x" << std::hex << va << " is read, differs from the file and is not built";
      }
    }
    // list pointers: ours non-null where the capture read a non-null one
    for (const auto& [key, nonNull] : capturedListPointerNonNull)
      EXPECT_EQ(ListPointer(key.first, key.second, 0) != 0, nonNull) << "motion " << key.first << " kind " << key.second;
  }
  EXPECT_GT(compared, 500u);
}

// The three image globals reach the objects the way the chain walks them (FUN_141E5CA20: payload = *(*(array[id]) + 8)).
TEST_F(TrapData, TheGlobalsReachTheLoaderObjects) {
  std::map<uint64_t, uint64_t> global(data_->globals.begin(), data_->globals.end());
  ASSERT_EQ(global.size(), 3u);
  EXPECT_EQ(Qword(global.at(pes::kMotionTableGlobalVa)), data_->motionRecordsVa);  // singleton +0: the record block
  EXPECT_EQ(Qword(global.at(pes::kAnimeManagerGlobalVa)), data_->animetableVa);
  const uint64_t array = Qword(global.at(pes::kRegistryGlobalVa)), entry = Qword(array + 8 * pes::kTrapRegistryId);
  EXPECT_EQ(Qword(entry + 8), data_->trapVa);
}

// The whole structures equal the capture's, not only what the call read: the Animetable buffer, every list of kinds 0, 1, 6, 13
// (counts and the raw records behind the pointers) and every byte of the Trap runtime object the loader stores.
TEST_F(TrapData, TheWholeStructuresEqualTheCapture) {
  for (const std::string& capture : captures_) {
    SCOPED_TRACE(capture);
    std::map<std::pair<uint64_t, int>, std::vector<std::vector<uint8_t>>> lists;
    for (const Row& r : ReadTsv(capture + ".full.tsv")) {
      if (r[0] == "anime") {
        const std::vector<uint8_t> want = FromHex(r[1]);
        ASSERT_EQ(want.size(), pes::kAnimetableCellCount * 3);
        ASSERT_NE(data_->At(data_->animetableVa, want.size()), nullptr);
        EXPECT_EQ(std::memcmp(data_->At(data_->animetableVa, want.size()), want.data(), want.size()), 0);
      } else if (r[0] == "trap") {
        const std::vector<uint8_t> want = FromHex(r[1]);
        ASSERT_EQ(want.size(), pes::kTrapRuntimeBytes);
        size_t built = 0;
        for (size_t i = 0; i < want.size(); ++i) {
          if (!pes::TrapRuntimeByteIsBuilt(i)) continue;
          ++built;
          if (i < 8) continue;  // the vtable word: the image's own pointer, equal to the capture's
          EXPECT_EQ(Byte(data_->trapVa + i), want[i]) << "Trap +0x" << std::hex << i;
        }
        EXPECT_GT(built, 400u);
      } else if (r[0] == "rec") {
        lists[{Num(r[1]), int(Num(r[2]))}].push_back(FromHex(r[4]));
      }
    }
    for (uint64_t motion = 0; motion < pes::kMotionSlotCount; ++motion) {
      for (const auto& k : pes::kTrapMbinfoKinds) {
        const auto it = lists.find({motion, k.kind});
        const size_t want = it == lists.end() ? 0 : it->second.size();
        ASSERT_EQ(Byte(Record(motion) + k.kind), want) << "motion " << motion << " kind " << k.kind;
        for (size_t i = 0; i < want; ++i) {
          const uint64_t ptr = ListPointer(motion, k.kind, i);
          ASSERT_NE(data_->At(ptr, k.recordBytes), nullptr);
          EXPECT_EQ(std::memcmp(data_->At(ptr, k.recordBytes), it->second[i].data(), k.recordBytes), 0)
              << "motion " << motion << " kind " << k.kind << " idx " << i;
        }
      }
    }
  }
}

// An install that is missing a file is refused with the file named, not built half-way.
TEST(TrapDataRefusal, AnUnmountedInstallBuildsNothing) {
  const char* root = std::getenv("PES21_ROOT");
  if (root == nullptr || *root == '\0') GTEST_SKIP() << "PES21_ROOT unset";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  pes::PesVfs empty;
  EXPECT_FALSE(pes::BuildTrapLoaderData(*exe, empty, &err).has_value());
  EXPECT_FALSE(err.empty());
}

}  // namespace
