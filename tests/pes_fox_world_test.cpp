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
#include "pes/translated/translated.hpp"
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

// The whole chain, PES's own: the job maker FUN_141EA7190 and the job FUN_141EA7640 (translated) open a handle on a body skeleton, the
// handle's reader FUN_1414A9280 reads it through the six CRI Fs services (the install's VFS), and the job parses it into the ResManager.
// The per-type adder (FUN_141B01660..) answers the resource's key, which the job keeps at +0x40 (16 bytes: key, valid byte at +8).
constexpr char kSkeletonJobPath[] = "cpk_dat/common/anime/FoxAnim/Body/CharacterAssets/body_skel.frig";
constexpr uint64_t kResManagerVa = 0x1437CEAA0, kNoKey = 0xB8A0BF169F98;
constexpr uint64_t kJobStateOffset = 8, kJobKeyOffset = 0x40, kJobDone = 2;

// Every call of the chain is translated: boot constructors, job maker, job, reader and decode stages with the CRI services, and the
// job's lock release 0x1415A1F7C (the CRT leave-lock helper the file handle's delete reaches).
TEST_F(FoxSeam, TheTranslatedJobLoadsAnInstallFileIntoTheResManager) {
  world_->Boot();
  ASSERT_NE(world_->Read64(kResManagerVa), 0u) << "the boot built no ResManager";
  const uint64_t job = world_->StartJob(kSkeletonJobPath);
  ASSERT_NE(job, 0u) << "the extension names no loader";
  std::string error;
  ASSERT_TRUE(world_->Finish(job, &error)) << error;
  EXPECT_EQ(world_->FileServes(), 1u);
  EXPECT_EQ(world_->Read8(job + kJobStateOffset), kJobDone);
  EXPECT_NE(world_->Read64(job + kJobKeyOffset), kNoKey);
  EXPECT_NE(world_->Read8(job + kJobKeyOffset + 8), 0);
}

// PES's own allocator (jemalloc: FUN_1403CF740 under FUN_14029D370) boots from the zeroed state PES starts in: FUN_14038EE80 (malloc_init_hard)
// sets the "initialised" byte at 0x1436DCEE0 that FUN_14038E3E0 tests, and the allocator then hands out distinct, aligned blocks from the OS
// arena the VirtualAlloc model serves. The dump holds that byte set (the dead game's), so Boot zeroes the allocator's state first: a Boot that
// then skips the init leaves it clear, and one that skips the zeroing walks the dead trees (the first allocation never returns).
constexpr uint64_t kAllocatorInitFlagVa = 0x1436DCEE0, kOsArenaVa = 0x3000000000ULL, kOsArenaBytes = 0x100000000ULL;
constexpr uint64_t kAllocSize = 0x100, kAllocAlign = 0x10, kAllocFlags = 0x30001;  // FUN_141B04850's call shape: (size, 0x10, 0x30001)

TEST_F(FoxSeam, TheBootRunsPesMallocInitAndItsAllocatorHandsOutDistinctAlignedBlocks) {
  world_->Boot();
  EXPECT_EQ(world_->Read8(kAllocatorInitFlagVa), 1);
  const uint64_t a = world_->Call(::pes_fn_1403CF740, {kAllocSize, kAllocAlign, kAllocFlags});
  const uint64_t b = world_->Call(::pes_fn_1403CF740, {kAllocSize, kAllocAlign, kAllocFlags});
  for (uint64_t block : {a, b}) {
    EXPECT_GE(block, kOsArenaVa);
    EXPECT_LT(block, kOsArenaVa + kOsArenaBytes);
    EXPECT_EQ(block % kAllocAlign, 0u);
  }
  EXPECT_NE(a, b);
  EXPECT_TRUE(a + kAllocSize <= b || b + kAllocSize <= a) << "the two blocks overlap";
}

// Process-start state of the CRT's thread-safe statics: the dump holds the dead game's guards (the string table's, a function-local static
// at 0x1436EDCA8, reads 0x80000001 = built) and global epoch; the reset clears every guard and sets the epoch to INT_MIN. The string table's
// guard stands for the 856 the scan finds; a second guard (the anime type object's, 0x1437CEEB8) is read too, from another function.
constexpr uint64_t kStringTableGuardVa = 0x1436EDCA8, kAnimeTypeGuardVa = 0x1437CEEB8, kGlobalEpochVa = 0x14350B358;

TEST_F(FoxSeam, TheStaticInitResetClearsEveryGuardAndStartsTheEpochAtIntMin) {
  ASSERT_NE(world_->Read32(kStringTableGuardVa), 0u) << "the dump no longer holds the guard set: the test proves nothing";
  ASSERT_NE(world_->Read32(kAnimeTypeGuardVa), 0u);
  world_->ResetStaticInitState();
  EXPECT_EQ(world_->Read32(kStringTableGuardVa), 0u);
  EXPECT_EQ(world_->Read32(kAnimeTypeGuardVa), 0u);
  EXPECT_EQ(world_->Read32(kGlobalEpochVa), 0x80000000u);
}

// A constant-initialized static (the type table's hash map, FUN_14026A2B0's) holds the dead game's runtime state in the dump; the restore
// brings back the install's own exe bytes for exactly that object.
constexpr uint64_t kTypeTableVa = 0x14339B010, kTypeTableBytes = 0x30;

TEST_F(FoxSeam, TheInitialDataRestoreBringsBackTheExesBytesOfAConstantInitializedStatic) {
  std::string error;
  const auto exe = pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", &error);
  ASSERT_TRUE(exe.has_value()) << error;
  std::vector<uint8_t> initial(kTypeTableBytes);
  ASSERT_TRUE(exe->ReadVa(kTypeTableVa, initial.size(), initial.data()));
  std::vector<uint8_t> dump(kTypeTableBytes);
  for (uint64_t i = 0; i < dump.size(); ++i) dump[i] = world_->Read8(kTypeTableVa + i);
  ASSERT_NE(dump, initial) << "the dump no longer holds runtime state there: the test proves nothing";
  const uint8_t neighbourBefore = world_->Read8(kTypeTableVa + kTypeTableBytes);
  world_->RestoreInitialData();
  std::vector<uint8_t> restored(kTypeTableBytes);
  for (uint64_t i = 0; i < restored.size(); ++i) restored[i] = world_->Read8(kTypeTableVa + i);
  EXPECT_EQ(restored, initial);
  EXPECT_EQ(world_->Read8(kTypeTableVa + kTypeTableBytes), neighbourBefore) << "the byte after the object was touched";
}

// The stand-ins for the packer's virtualized functions (stubs.json gf-service): their effects were read from PES's own bytes on a real
// pre-state (the entries' evidence): 0x143E98DA0(pool, size) stored size at pool+0x60 and returned the old value; 0x143DFA780 =
// lua_atpanic(L, f) stored f at L+0x160 (with the global state at L+0xB8) and returned the old value. Each test checks the one store, the
// return, and that no other byte of the object moved.
struct ObjectState : PesState {
  static constexpr uint64_t kBase = 0x40000, kBytes = 0x400;
  ObjectState() { mem.segs.push_back({kBase, std::vector<uint8_t>(kBytes, 0x5A)}); }
  std::vector<uint8_t> Bytes() { std::vector<uint8_t> out(kBytes); for (uint64_t i = 0; i < kBytes; ++i) out[i] = static_cast<uint8_t>(rd(kBase + i, 1)); return out; }
};

TEST(PackerStandIns, PoolSetMaxStoresTheSizeAtPlus0x60AndReturnsTheOldValue) {
  ObjectState s;
  const uint64_t pool = ObjectState::kBase + 0x100, oldMax = 0x2FFFF0, newMax = 0x4000;
  s.wr(pool + 0x60, oldMax, 8);
  const std::vector<uint8_t> before = s.Bytes();
  s.gpr[1] = pool;
  s.gpr[2] = newMax;
  pes_service_packer_pool_set_max(s);
  EXPECT_EQ(s.gpr[0], oldMax);
  EXPECT_EQ(s.rd(pool + 0x60, 8), newMax);
  std::vector<uint8_t> after = s.Bytes();
  for (uint64_t i = 0; i < 8; ++i) after[pool + 0x60 - ObjectState::kBase + i] = before[pool + 0x60 - ObjectState::kBase + i];
  EXPECT_EQ(after, before) << "a byte outside the max-size qword changed";
}

TEST(PackerStandIns, LuaAtPanicStoresTheFunctionInTheGlobalStatesPanicSlotAndReturnsTheOldOne) {
  ObjectState s;
  // PES's layout as the real run and the live game show it: the global state follows the thread at L+0xB8 (l_G at L+0x20), its panic slot is
  // L+0x160.
  const uint64_t L = ObjectState::kBase + 0x40, G = L + 0xB8, oldPanic = 0, panicFn = 0x14029E220;
  s.wr(L + 0x20, G, 8);
  s.wr(L + 0x160, oldPanic, 8);
  const std::vector<uint8_t> before = s.Bytes();
  s.gpr[1] = L;
  s.gpr[2] = panicFn;
  pes_service_packer_lua_atpanic(s);
  EXPECT_EQ(s.gpr[0], oldPanic);
  EXPECT_EQ(s.rd(L + 0x160, 8), panicFn);
  std::vector<uint8_t> after = s.Bytes();
  for (uint64_t i = 0; i < 8; ++i) after[L + 0x160 - ObjectState::kBase + i] = before[L + 0x160 - ObjectState::kBase + i];
  EXPECT_EQ(after, before) << "a byte outside the panic slot changed";
  // A second call returns the first call's function (the slot is the state).
  s.gpr[2] = 0x1234;
  pes_service_packer_lua_atpanic(s);
  EXPECT_EQ(s.gpr[0], panicFn);
}

TEST(PackerStandIns, LuaSetAllocFStoresTheAllocatorAndItsUserDataInTheGlobalState) {
  ObjectState s;
  const uint64_t L = ObjectState::kBase + 0x40, G = L + 0xB8, allocFn = 0x14029EB40, ud = 0x1FB0180;
  s.wr(L + 0x20, G, 8);
  const std::vector<uint8_t> before = s.Bytes();
  s.gpr[1] = L;
  s.gpr[2] = allocFn;
  s.gpr[8] = ud;
  pes_service_packer_lua_setallocf(s);
  EXPECT_EQ(s.rd(L + 0xC8, 8), allocFn);  // G + 0x10 as the real run's state holds it (L + 0xC8 = frealloc, L + 0xD0 = ud)
  EXPECT_EQ(s.rd(L + 0xD0, 8), ud);
  EXPECT_EQ(s.gpr[0], G);
  std::vector<uint8_t> after = s.Bytes();
  for (uint64_t i = 0; i < 16; ++i) after[L + 0xC8 - ObjectState::kBase + i] = before[L + 0xC8 - ObjectState::kBase + i];
  EXPECT_EQ(after, before) << "a byte outside the two stores changed";
}

}  // namespace
