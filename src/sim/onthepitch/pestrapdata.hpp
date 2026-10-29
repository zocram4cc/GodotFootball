// The loader data PES's Trap chain (0x14079A470) reads, built from the install: the motion table's records for the four Mbinfo
// kinds the chain touches, the s16 Animetable buffer, and the constant::Trap runtime object of registry id 0x3E (trap.o), with
// the image globals that reach them (docs/re/pes-re-trap-inputs.md 2). Replaces the captured segments of
// tests/pes_trap_oracle_test.cpp; what the chain does not read is not built (listed in docs/ports/pes-port-TrapData.md).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace pes {

// Image globals (PES .data) the chain loads the loader objects from.
inline constexpr uint64_t kMotionTableGlobalVa = 0x1436F23C0ULL;  // -> the motion-table singleton (+0 = base of the records)
inline constexpr uint64_t kAnimeManagerGlobalVa = 0x1436F2378ULL; // -> the anime-table manager (+0 = the Animetable buffer)
inline constexpr uint64_t kRegistryGlobalVa = 0x1437F4808ULL;     // -> the constant registry (+0 = the entry array)

// FUN_141E5CA20(registry, id): payload = *(*(array[id]) + 8). trap.o is object 35 of constant_player.bin, which follows
// constant_match.bin's objects 0..26.
inline constexpr uint32_t kTrapRegistryId = 0x3E;
inline constexpr std::size_t kMotionSlotCount = 0x1125;   // FUN_1406B20E0: the record block is 0x1125 x 0x390 bytes
inline constexpr std::size_t kMotionRecordBytes = 0x390;
inline constexpr std::size_t kAnimetableCellCount = 0xBE5 * 0x12;  // rows x columns, FUN_1406AA440
inline constexpr std::size_t kTrapRuntimeBytes = 0x1D8;  // constant::Trap: the slot-2 loader's last store is +0x1D5

// The four Mbinfo files the chain reads, as FUN_1406AEF60 files them: kind (the file's index in the path table 0x14344DC40),
// the list offset FUN_1406AE810 gives it inside a motion record, the record size, the motion key.
struct TrapMbinfoKind {
  int kind;
  const char* path;
  std::size_t recordBytes;
  std::size_t listOffset;
  uint32_t motionBase;  // motion = (word0 & 0x1FFF) + motionBase
};
inline constexpr TrapMbinfoKind kTrapMbinfoKinds[] = {
    {0, "common/anime/Mbinfo/bin/Animation.bin", 36, 0x18, 0},    // FUN_1406AC4D0
    {1, "common/anime/Mbinfo/bin/HitData.bin", 108, 0x20, 0x11},  // FUN_1406AD300
    {6, "common/anime/Mbinfo/bin/RotData.bin", 8, 0xC8, 0},       // FUN_1406AE4F0
    {13, "common/anime/Mbinfo/bin/BallData.bin", 20, 0x210, 0},   // FUN_1406AC660
};

// Where the loader objects sit in the window: one segment from kTrapLoaderVa (above the world window, kWorldWindowVa).
inline constexpr uint64_t kTrapLoaderVa = 0x2100000000ULL;

struct PesTrapLoaderData {
  PesTrapSegment segment;                               // every loader object, at kTrapLoaderVa
  std::vector<std::pair<uint64_t, uint64_t>> globals;   // (image VA, pointer into the segment)
  uint64_t motionTableVa = 0, motionRecordsVa = 0, animeManagerVa = 0, animetableVa = 0, registryVa = 0, trapVa = 0;

  // The segment's bytes at a VA, or null when [va, va + n) is outside it.
  const uint8_t* At(uint64_t va, std::size_t n) const {
    return va >= segment.va && va + n <= segment.va + segment.bytes.size() ? segment.bytes.data() + (va - segment.va) : nullptr;
  }
  void AddTo(PesTrapExternals& ext) const {
    ext.segments.push_back(segment);
    ext.globals.insert(ext.globals.end(), globals.begin(), globals.end());
  }
};

// Reads the four Mbinfo files, Animetable.bin and trap.o through the VFS winner, and the caps and layout constants from the
// exe. Null with *error set when a file or an exe check fails.
std::optional<PesTrapLoaderData> BuildTrapLoaderData(const PeImage& exe, const PesVfs& vfs, std::string* error);

// Whether byte `offset` of the constant::Trap runtime object is built: the vtable word and every store of the slot-2 loader
// FUN_141E67B70. The other bytes of the object (past +0x1D5) are not built.
bool TrapRuntimeByteIsBuilt(std::size_t offset);

// Everything the trap chain needs from the install, built once from it (like the other Load*Constants): the actor constants P
// is made from, the loader data, and the path of the image the chain reads. The chain reads the running game's image (vtables,
// constant tables, image globals, constants kept beside the code), which the file on disk does not hold (970 ranges of .rdata
// and .data differ), so the image is the user's memory dump (PES_EXE, tools/pes21_import/README.md; file offset = VA - base).
struct PesTrapSource {
  std::string imagePath;
  uint64_t imageBytes = 0;
  PesActorConstants actor;
  PesTrapLoaderData loader;
};
// `imagePath` is the memory dump; the constants and loader data come from the install's exe and VFS.
std::optional<PesTrapSource> LoadTrapSource(const PeImage& exe, const PesVfs& vfs, const std::string& imagePath,
                                            std::string* error);

// One match's memory for the chain, built once per match: a private copy-on-write mapping of the dump (the chain writes image
// globals, and the file stays as it is; pages are shared until written, so the mapping costs nothing up front) and a copy of the
// loader segment. PesWorld::RunTrapKind0 lends the externals to each call without copying them.
class PesTrapMemory {
 public:
  explicit PesTrapMemory(const PesTrapSource& source);
  ~PesTrapMemory();
  PesTrapMemory(const PesTrapMemory&) = delete;
  PesTrapMemory& operator=(const PesTrapMemory&) = delete;
  PesTrapExternals& Externals() { return ext_; }

 private:
  PesTrapExternals ext_;
};

}  // namespace pes
