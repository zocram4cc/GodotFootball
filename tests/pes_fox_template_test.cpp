// The fox world's template (OneWorld cut 2). A clone of the frozen anime world holds the bytes a fresh boot and load make on every region of
// PES's memory, the same heap bookkeeping, and runs the same engine frames to the same poses.
//
// PES_EXE names the unpacked image and PES21_ROOT the install. Without either, skip.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "onthepitch/pesboot.hpp"
#include "onthepitch/pesfoxworld.hpp"
#include "onthepitch/pesmemory.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/vfs.hpp"
#include "pes_engine_harness.hpp"

namespace {

constexpr uint64_t kHeapBytes = 768ull << 20;  // the anime engine's heap (matchevents.cpp kPesAnimeHeapBytes)
constexpr uint64_t kPlayerBytes = 0x3C00;       // the player block FUN_1406E03D0 constructs (matchevents.cpp kPesAnimePlayerBytes)
constexpr int kPlayers = 2, kFrames = 3;

// Two players constructed on `world` and run for kFrames frames: every pose read after each frame, the file serves and the heap's live bytes.
// The allocation order is the same on every world.
struct PlayResult {
  std::vector<pes::PesJointPose> poses;
  uint64_t serves = 0, live = 0;
};

PlayResult Play(pes::PesFoxWorld& world) {
  std::vector<uint64_t> players;
  for (int i = 0; i < kPlayers; ++i) {
    const uint64_t p = world.Alloc(kPlayerBytes);
    world.ConstructAnime(p, 0);
    players.push_back(p);
  }
  PlayResult result;
  for (int f = 0; f < kFrames; ++f)
    for (uint64_t p : players) {
      pes_test::RunEngineFrame(world, p);
      result.poses.push_back(world.ReadPose(p));
    }
  result.serves = world.FileServes();
  result.live = world.HeapLiveBytes();
  return result;
}

// The heap's bookkeeping, entry by entry: the blocks, the free lists and the counters.
bool SameBookkeeping(const PesHeap::Bookkeeping& a, const PesHeap::Bookkeeping& b) {
  if (a.top != b.top || a.allocs != b.allocs || a.frees != b.frees || a.live.size() != b.live.size() ||
      a.freeList.size() != b.freeList.size())
    return false;
  for (const auto& [va, block] : a.live) {
    const auto it = b.live.find(va);
    if (it == b.live.end() || it->second.bytes != block.bytes || it->second.recyclable != block.recyclable) return false;
  }
  for (const auto& [bytes, vas] : a.freeList) {
    const auto it = b.freeList.find(bytes);
    if (it == b.freeList.end() || it->second != vas) return false;
  }
  return true;
}

bool Environment(const char** exe, const char** root) {
  *exe = std::getenv("PES_EXE");
  *root = std::getenv("PES21_ROOT");
  return *exe && **exe && *root && **root;
}

}  // namespace

TEST(FoxTemplate, ACloneHoldsTheFreshBootsBytesAndBookkeepingOnEveryRegion) {
  const char* exe = nullptr;
  const char* root = nullptr;
  if (!Environment(&exe, &root)) GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  std::string error;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  pes::PesFoxWorld fresh(exe, vfs, kHeapBytes);
  fresh.Boot();
  ASSERT_TRUE(fresh.LoadBody(&error)) << error;
  // The reference is the state the template freezes (cut 3): the boot, the body and the trap loader data.
  pes::AddTrapLoaderData(fresh.Memory(), vfs, exe);
  const pes::PesFoxWorldTemplate& frozen = pes::AnimeWorldTemplate(exe, vfs, kHeapBytes);
  pes::PesFoxWorld clone(frozen, vfs);
  pes::PesMemory& a = fresh.Memory();
  pes::PesMemory& b = clone.Memory();
  ASSERT_EQ(a.ImageBytes(), b.ImageBytes());
  EXPECT_EQ(std::memcmp(a.Image(), b.Image(), a.ImageBytes()), 0) << "the image";
  EXPECT_EQ(std::memcmp(a.Heap().PageData(), b.Heap().PageData(), a.Heap().PageBytes()), 0) << "the heap's page";
  EXPECT_EQ(std::memcmp(a.Heap().ArenaData(), b.Heap().ArenaData(), a.Heap().ArenaBytes()), 0) << "the heap's arena";
  EXPECT_EQ(std::memcmp(a.Stack(), b.Stack(), pes::PesMemory::kStackBytes), 0) << "the stack";
  EXPECT_EQ(std::memcmp(a.Teb(), b.Teb(), pes::PesMemory::kTebBytes), 0) << "the thread block";
  EXPECT_EQ(std::memcmp(a.OsArena(), b.OsArena(), pes::PesMemory::kOsArenaBytes), 0) << "the OS arena";
  EXPECT_TRUE(SameBookkeeping(a.Heap().SaveBookkeeping(), b.Heap().SaveBookkeeping())) << "the heap's bookkeeping";
  EXPECT_EQ(fresh.FileServes(), clone.FileServes());
  EXPECT_EQ(fresh.InitializersRun(), clone.InitializersRun());
}

TEST(FoxTemplate, ACloneRunsTheSameFramesToTheSamePoses) {
  const char* exe = nullptr;
  const char* root = nullptr;
  if (!Environment(&exe, &root)) GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  std::string error;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  pes::PesFoxWorld fresh(exe, vfs, kHeapBytes);
  fresh.Boot();
  ASSERT_TRUE(fresh.LoadBody(&error)) << error;
  pes::AddTrapLoaderData(fresh.Memory(), vfs, exe);
  const PlayResult expected = Play(fresh);
  const pes::PesFoxWorldTemplate& frozen = pes::AnimeWorldTemplate(exe, vfs, kHeapBytes);
  pes::PesFoxWorld clone(frozen, vfs);
  const PlayResult actual = Play(clone);
  ASSERT_EQ(expected.poses.size(), static_cast<size_t>(kPlayers * kFrames));
  ASSERT_EQ(actual.poses.size(), expected.poses.size());
  for (size_t i = 0; i < expected.poses.size(); ++i)
    EXPECT_EQ(std::memcmp(&expected.poses[i], &actual.poses[i], sizeof(pes::PesJointPose)), 0) << "pose " << i;
  EXPECT_EQ(expected.serves, actual.serves);
  EXPECT_EQ(expected.live, actual.live);
}

TEST(FoxTemplate, ACloneThatRunsLeavesAnotherCloneAndTheFreshBootUnchanged) {
  const char* exe = nullptr;
  const char* root = nullptr;
  if (!Environment(&exe, &root)) GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  std::string error;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  pes::PesFoxWorld fresh(exe, vfs, kHeapBytes);
  fresh.Boot();
  ASSERT_TRUE(fresh.LoadBody(&error)) << error;
  pes::AddTrapLoaderData(fresh.Memory(), vfs, exe);
  const pes::PesFoxWorldTemplate& frozen = pes::AnimeWorldTemplate(exe, vfs, kHeapBytes);
  pes::PesFoxWorld runner(frozen, vfs);
  Play(runner);  // its writes go copy-on-write, over the frozen pages
  pes::PesFoxWorld other(frozen, vfs);
  pes::PesMemory& a = fresh.Memory();
  pes::PesMemory& b = other.Memory();
  EXPECT_EQ(std::memcmp(a.Image(), b.Image(), a.ImageBytes()), 0) << "the image of a clone made after a run";
  EXPECT_EQ(std::memcmp(a.Heap().ArenaData(), b.Heap().ArenaData(), a.Heap().ArenaBytes()), 0) << "the heap of a clone made after a run";
  EXPECT_EQ(std::memcmp(a.OsArena(), b.OsArena(), pes::PesMemory::kOsArenaBytes), 0) << "the OS arena of a clone made after a run";
  EXPECT_TRUE(SameBookkeeping(a.Heap().SaveBookkeeping(), b.Heap().SaveBookkeeping())) << "the heap's bookkeeping after a run";
}

// OneWorld cut 4: a player block (P) lives in the match's window, which the trap world's calls map. The anime engine's calls run on the same
// memory, so they must map it too: a call that reads or writes a window VA reads or writes the window's bytes.
TEST(FoxTemplate, ACallOfTheMemoryReadsAndWritesTheMatchWindow) {
  const char* exe = nullptr;
  const char* root = nullptr;
  if (!Environment(&exe, &root)) GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  std::string error;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  pes::PesFoxWorld world(pes::AnimeWorldTemplate(exe, vfs, kHeapBytes), vfs);
  pes::PesMemory& memory = world.Memory();
  uint8_t* window = memory.AddWindow(pes::kWorldWindowVa, kPlayerBytes);
  constexpr uint64_t kProbe = 0x123, kWritten = 0xFEEDFACE;
  const uint64_t seen = 0x1122334455667788ull;
  std::memcpy(window + kProbe * 8, &seen, sizeof seen);
  EXPECT_EQ(memory.Read64(pes::kWorldWindowVa + kProbe * 8), seen) << "a call reads the host window's bytes";
  memory.Write64(pes::kWorldWindowVa + kProbe * 8 + 8, kWritten);
  uint64_t stored = 0;
  std::memcpy(&stored, window + kProbe * 8 + 8, sizeof stored);
  EXPECT_EQ(stored, kWritten) << "a call's write lands in the host window";
}

// The constructor FUN_141EB5250 builds a player's control on a block anywhere in PES's memory, the window's P included: the same fields as on
// a heap block (the control's vtable, the ready flag, the name word, the constructor's none markers), written in place.
TEST(FoxTemplate, ThePlayerConstructorBuildsTheControlOnABlockInTheMatchWindow) {
  const char* exe = nullptr;
  const char* root = nullptr;
  if (!Environment(&exe, &root)) GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  std::string error;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  const pes::PesFoxWorldTemplate& frozen = pes::AnimeWorldTemplate(exe, vfs, kHeapBytes);
  pes::PesFoxWorld onHeap(frozen, vfs), inWindow(frozen, vfs);
  const uint64_t heapBlock = onHeap.Alloc(kPlayerBytes);
  onHeap.ConstructAnime(heapBlock, 0);
  uint8_t* window = inWindow.Memory().AddWindow(pes::kWorldWindowVa, kPlayerBytes);
  inWindow.ConstructAnime(pes::kWorldWindowVa, 0);
  constexpr size_t kAnimeFields = 0xA04, kAnimeFieldsEnd = 0xAB8, kControlPointer = 0xA08;
  for (size_t at = kAnimeFields; at < kAnimeFieldsEnd; at += 4) {
    if (at >= kControlPointer && at < kControlPointer + 8) continue;  // the control's address depends on the allocation order
    uint32_t inPlace = 0;
    std::memcpy(&inPlace, window + at, sizeof inPlace);
    EXPECT_EQ(inPlace, onHeap.Memory().Read32(heapBlock + at)) << "P+0x" << std::hex << at;
  }
  uint64_t control = 0;
  std::memcpy(&control, window + kControlPointer, sizeof control);
  ASSERT_NE(control, 0u) << "the constructor stored no control at P+0xA08";
  EXPECT_EQ(inWindow.Memory().Read64(control), onHeap.Memory().Read64(onHeap.Memory().Read64(heapBlock + kControlPointer)))
      << "the control's vtable";
}

// The boot builds the registry singleton (PES's engine init does, behind protection thunks GF does not run): the cell holds an object.
TEST(FoxTemplate, TheBootBuildsTheRegistrySingleton) {
  const char* exe = nullptr;
  const char* root = nullptr;
  if (!Environment(&exe, &root)) GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  std::string error;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  pes::PesFoxWorld world(pes::AnimeWorldTemplate(exe, vfs, kHeapBytes), vfs);
  EXPECT_NE(world.Memory().Read64(pes::kRegistryCellVa), 0u) << "DAT_1437F0AC8 is empty after the boot";
}

// The creator copies the config's bytes +0x28..+0x80 to registry +0x50..+0xA8 (FUN_141B56170). The registry the boot builds from the config
// rebuilt by meaning holds what PES's own creator copied from the config of the REAL call (GF_PES_REGISTRY_CONFIG: the 0x88-byte block, read by
// tools/dev/translate/registry_config.py from live2 registry_create_141B55E70 seq001; the file stays in scratch). The block's last qword
// (+0x78) is a dword of flags and a dword of stack padding (0x797F on the capture): only the flags are compared.
TEST(FoxTemplate, TheRegistryTheBootBuildsHoldsTheConfigOfPesRealCall) {
  const char* exe = nullptr;
  const char* root = nullptr;
  if (!Environment(&exe, &root)) GTEST_SKIP() << "PES_EXE and PES21_ROOT are needed";
  const char* file = std::getenv("GF_PES_REGISTRY_CONFIG");
  if (!file || !*file) GTEST_SKIP() << "GF_PES_REGISTRY_CONFIG is not set";
  std::ifstream in(file);
  std::string hex;
  ASSERT_TRUE(static_cast<bool>(std::getline(in, hex))) << file;
  ASSERT_EQ(hex.size(), 2 * pes::kRegistryConfigBytes);
  std::vector<uint8_t> block;
  for (size_t at = 0; at < hex.size(); at += 2) block.push_back(static_cast<uint8_t>(std::stoul(hex.substr(at, 2), nullptr, 16)));
  std::string error;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &error)) << error;
  pes::PesFoxWorld world(pes::AnimeWorldTemplate(exe, vfs, kHeapBytes), vfs);
  pes::PesMemory& memory = world.Memory();
  const uint64_t registry = memory.Read64(pes::kRegistryCellVa);
  ASSERT_NE(registry, 0u);
  constexpr uint64_t kCopyTo = 0x50, kLastQword = 0x78;
  for (uint64_t at = pes::kRegistryConfigCopyFrom; at < pes::kRegistryConfigCopyTo; at += 8) {
    uint64_t captured = 0;
    std::memcpy(&captured, block.data() + at, sizeof captured);
    uint64_t built = memory.Read64(registry + kCopyTo + (at - pes::kRegistryConfigCopyFrom));
    if (at == kLastQword) {
      captured &= 0xFFFFFFFFull;
      built &= 0xFFFFFFFFull;
    }
    EXPECT_EQ(built, captured) << "config +0x" << std::hex << at;
  }
}
