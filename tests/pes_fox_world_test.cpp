// The fox file seam (ticket AnimeLayer4): PES's file handle reader asks the CRI Fs API for a file's size and reads it into the handle's
// buffer; GF answers those six calls (pes_service_fs_*) from the install's VFS. The test drives the services the way FUN_1414A9280 does
// (arguments and stack slots as its disassembly passes them) over a real install file and compares the buffer with the VFS's own bytes.
//
// PES_EXE names the unpacked image and PES21_ROOT the install. Without either, skip.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "onthepitch/pesfoxworld.hpp"
#include "pes_stubs.hpp"
#include "pes/vfs.hpp"

namespace {

// A file every install has: the first body animation pack (the file layer's own first customer).
constexpr char kPackPath[] = "common/anime/FoxAnim/Body/body_anime_file0.mtar";
constexpr uint64_t kHeapBytes = 256ull << 20;
// FUN_1415A71B8's arguments in the caller's frame (FUN_1414A9280 at 0x1414A93B4..0x1414A93BE): the service runs in place of the call,
// so [rsp+0x20] is the fifth argument.
constexpr uint64_t kLoadSizeSlot = 0x20, kBufferSlot = 0x28, kBufferSizeSlot = 0x30;
constexpr uint32_t kStatusComplete = 2, kStatusError = 3;

class FoxSeam : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* exe = std::getenv("PES_EXE");
    const char* root = std::getenv("PES21_ROOT");
    if (!exe || !*exe || !root || !*root) GTEST_SKIP() << "PES_EXE / PES21_ROOT unset";
    std::string error;
    ASSERT_TRUE(vfs_.Mount(root, &error)) << error;
    world_ = std::make_unique<pes::PesFoxWorld>(exe, vfs_, kHeapBytes);
    const auto bytes = vfs_.Read(kPackPath, &error);
    ASSERT_TRUE(bytes.has_value()) << error;
    file_ = *bytes;
  }

  // FUN_1415A2FD0(fs, path, int64* size): the size the reader tests.
  int64_t Stat(const std::string& path) {
    PesState s = world_->NewCall();
    const uint64_t cell = world_->Alloc(8);
    s.gpr[2] = world_->NewCString(path);
    s.gpr[8] = cell;
    pes_service_fs_stat(s);
    return static_cast<int64_t>(s.rd(cell, 8));
  }

  // The reader's sequence: create, callback, read, status, destroy. Returns the status the reader's test sees.
  uint32_t Load(const std::string& path, uint64_t buffer, uint64_t loadSize, uint64_t bufferSize) {
    PesState s = world_->NewCall();
    const uint64_t opCell = world_->Alloc(8), statusCell = world_->Alloc(8);
    s.gpr[1] = opCell;
    pes_service_fs_op_create(s);
    const uint64_t op = s.rd(opCell, 8);
    EXPECT_NE(op, 0u);
    s.gpr[1] = op;
    s.gpr[2] = 0x1414A8F00;  // the completion callback
    s.gpr[8] = world_->Alloc(8);
    pes_service_fs_op_callback(s);
    s.gpr[1] = op;
    s.gpr[2] = 0;  // fs
    s.gpr[8] = world_->NewCString(path);
    s.gpr[9] = 0;  // offset
    s.wr(s.gpr[4] + kLoadSizeSlot, loadSize, 8);
    s.wr(s.gpr[4] + kBufferSlot, buffer, 8);
    s.wr(s.gpr[4] + kBufferSizeSlot, bufferSize, 8);
    pes_service_fs_op_read(s);
    s.gpr[1] = op;
    s.gpr[2] = statusCell;
    pes_service_fs_op_status(s);
    const uint32_t status = static_cast<uint32_t>(s.rd(statusCell, 4));
    s.gpr[1] = op;
    pes_service_fs_op_destroy(s);
    return status;
  }

  std::vector<uint8_t> ReadBack(uint64_t va, uint64_t n) {
    PesState s = world_->NewCall();
    std::vector<uint8_t> out(n);
    for (uint64_t i = 0; i < n; ++i) out[i] = static_cast<uint8_t>(s.rd(va + i, 1));
    return out;
  }

  pes::PesVfs vfs_;
  std::unique_ptr<pes::PesFoxWorld> world_;
  std::vector<uint8_t> file_;
};

TEST_F(FoxSeam, StatAnswersTheInstallFilesSize) { EXPECT_EQ(Stat(kPackPath), static_cast<int64_t>(file_.size())); }

TEST_F(FoxSeam, StatOfAMissingFileIsMinusOne) { EXPECT_EQ(Stat("common/anime/FoxAnim/Body/no_such_file.mtar"), -1); }

TEST_F(FoxSeam, ReadFillsTheBufferWithTheFilesBytesAndCompletes) {
  const uint64_t buffer = world_->Alloc(file_.size());
  ASSERT_NE(buffer, 0u);
  EXPECT_EQ(Load(kPackPath, buffer, file_.size(), file_.size()), kStatusComplete);
  EXPECT_EQ(ReadBack(buffer, file_.size()), file_);
}

TEST_F(FoxSeam, ReadStopsAtTheSmallerOfTheLoadSizeAndTheBufferSize) {
  // The loader never writes past the buffer: a load size over the buffer size reads the buffer size, and the bytes after stay zero.
  constexpr uint64_t kSlice = 4096;
  const uint64_t buffer = world_->Alloc(2 * kSlice);
  ASSERT_NE(buffer, 0u);
  EXPECT_EQ(Load(kPackPath, buffer, 2 * kSlice, kSlice), kStatusComplete);
  EXPECT_EQ(ReadBack(buffer, kSlice), std::vector<uint8_t>(file_.begin(), file_.begin() + kSlice));
  EXPECT_EQ(ReadBack(buffer + kSlice, kSlice), std::vector<uint8_t>(kSlice, 0));
}

TEST_F(FoxSeam, ReadOfAMissingFileEndsInTheErrorStatus) {
  const uint64_t buffer = world_->Alloc(64);
  EXPECT_EQ(Load("common/anime/FoxAnim/Body/no_such_file.mtar", buffer, 64, 64), kStatusError);
}

}  // namespace
