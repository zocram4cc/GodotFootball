// The fox world's template (OneWorld cut 2). A clone of the frozen anime world holds the bytes a fresh boot and load make on every region of
// PES's memory, the same heap bookkeeping, and runs the same engine frames to the same poses.
//
// PES_EXE names the unpacked image and PES21_ROOT the install. Without either, skip.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "onthepitch/pesfoxworld.hpp"
#include "onthepitch/pesmemory.hpp"
#include "pes/vfs.hpp"

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
      world.RunFrame(p);
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
