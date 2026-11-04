#include "onthepitch/pesanimechain.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "pes/clips.hpp"
#include "pes/mtar.hpp"
#include "pes/strcode.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

namespace pes {
namespace {

// Layout of the fixed head (offsets from kAnimeChainVa; each object's size is what its readers touch).
constexpr std::size_t kLayerOffset = 0x0;                          // FUN_141AF2C20 reads *(*(A+0x88) + 0x10)
constexpr std::size_t kLayer2Offset = 0x40;                        // its +0xF8 / +0x100: the two slot containers
constexpr std::size_t kContainer0Offset = 0x160, kContainer1Offset = 0x1C0;
constexpr std::size_t kPoseSetOffset = 0x300;                      // the container entry's object (FUN_141AF2C20's *plVar5)
constexpr std::size_t kDatabaseOffset = 0x500;
constexpr std::size_t kKeyTableOffset = 0x1000;
constexpr std::size_t kDynamicOffset = 0x1100;

// Layer controller L1 (A+0x88 target): FUN_141AF2C20 reads *(*(L1 + 0x10) + 0xF8 / 0x100) (0x141AF2C34..).
constexpr std::size_t kL1ChildOffset = 0x10;
constexpr std::size_t kL2Container0Offset = 0xF8, kL2Container1Offset = 0x100;
// A container: two 0x20-byte entries (only the current one is read) {+0 the pose set, +0xC its length} from +0, the current entry's index at +0x48 and the state
// word at +0x4C, ready when its low two bits are 1 (FUN_141AF2C20's `(byte)*(lVar3 + 0x4C) & 3) == 1` tests).
constexpr std::size_t kEntryPoseSetOffset = 0, kEntryLengthOffset = 0xC;
constexpr std::size_t kContainerIndexOffset = 0x48, kContainerStateOffset = 0x4C;
constexpr uint32_t kContainerReady = 1;
// The pose set (the layers' weights and their pose-slot tables): +4 the offset of its float weights, +0xC their count (u16), +0x30
// one offset per layer to the layer record, whose +0x98 holds the pose slot table. FUN_141AF2C20 (param_7 != 0) takes the layer
// whose weight is non-zero and indexes it as the kick's chain does (0x141AFDF10: FUN_141AF2C20(.., 0, 1)).
constexpr std::size_t kPoseWeightsOffsetField = 0x4, kPoseCountOffset = 0xC, kPoseLayerOffsets = 0x30;
constexpr std::size_t kPoseWeightsOffset = 0x48, kPoseLayerOffset = 0xF0, kLayerSlotTableOffset = 0x98;
constexpr float kLayerWeight = 1.0f;
// The pose slot table (FUN_1423A3FD0's param_1): +8 the first track index, +0x14 + (track - first) * 8 the slot's offset, a slot
// is 0x60 bytes (24 dwords: the evaluator saves them, evaluates the unit into them, reads +0x20..+0x2C (type 5) or +0x10..+0x1C
// (type 6) and puts them back).
constexpr std::size_t kSlotFirstTrackOffset = 0x8, kSlotOffsetTable = 0x14, kSlotOffsetStride = 8, kSlotBytes = 0x60;
// The clip database (FUN_141B03660): +8 the lock word (FUN_1403DB2C0 takes it with a compare-exchange 0 -> 1, 0x1403DB590, and
// FUN_1403DB410 gives it back; zero = free, so the database holds zero there), +0x18 the pack count, +0x20 the pack records (0x40
// bytes, the pack object at +0x18).
constexpr std::size_t kDbPackCountOffset = 0x18, kDbPacksOffset = 0x20;
constexpr std::size_t kPackRecordBytes = 0x40, kPackRecordObjectOffset = 0x18;
// A loaded pack (FUN_1415BB730): +0x40 == 2 (loaded), +0x60 the file's bytes, +0x90 the entry array (0x40 bytes each, the position
// FUN_1415BBA50's binary search finds in the file's own sorted table at +0x20, 16 bytes each: the key, the offset and size).
constexpr std::size_t kPackObjectBytes = 0xA0, kPackStateOffset = 0x40, kPackFileOffset = 0x60, kPackEntriesOffset = 0x90;
constexpr uint32_t kPackLoaded = 2;
// A pack entry as FUN_1415B8F40 fills it: the key, the pack, the gani file, the MOTION chunk's data (+0x20: what the reader
// evaluates) and a state word 3.
constexpr std::size_t kEntryBytes = 0x40, kEntryKeyOffset = 0, kEntryPackOffset = 0x10, kEntryFileOffset = 0x18, kEntryClipOffset = 0x20,
                      kEntryStateOffset = 0x30;
constexpr uint32_t kEntryState = 3;
constexpr uint32_t kMotionChunkHash = 0xC6E937B9;  // StrCode32("MOTION"): the chunk FUN_1415B8F40 looks up
// The motion -> clip key table (FUN_141AF32C0): +0x58 the count, +0x60 the entries, 16 bytes each, the key first.
constexpr std::size_t kKeyCountOffset = 0x58, kKeyEntriesOffset = 0x60, kKeyEntryBytes = 0x10;
// The clip the reader evaluates (FUN_1423A3FD0's param_4): +0 the unit-list flag (> 0: a list), +0x14 the unit list's offset; the
// list holds the unit count (s8 at +4), its flags (+5, bit 2 = no units to evaluate) and 8-byte unit headers from +8: +4 the track
// index (s16), +6 the type in the low nibble (5 = mover position, 6 = rotation) and bit 7 = another unit follows.
constexpr std::size_t kClipListOffsetField = 0x14, kListCountOffset = 4, kListFlagsOffset = 5, kListFirstUnitOffset = 8;
constexpr std::size_t kUnitTrackOffset = 4, kUnitTypeOffset = 6, kUnitBytes = 8;
constexpr uint8_t kUnitTypeMask = 0xF, kUnitMoverPosition = 5, kUnitMoverRotation = 6, kUnitHasNext = 0x80, kListNoUnits = 0x4;
constexpr uint64_t kChunkStackVa = kAnimeChainVa - 0x10000;
constexpr std::size_t kChunkStackBytes = 0x1000;

template <typename T>
T Load(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof v);
  return v;
}
template <typename T>
void Store(std::vector<uint8_t>& seg, std::size_t at, T v) {
  std::memcpy(seg.data() + at, &v, sizeof v);
}

std::size_t Alloc(std::vector<uint8_t>& seg, std::size_t n) {
  const std::size_t at = (seg.size() + 15) & ~std::size_t(15);
  seg.resize(at + n);
  return at;
}

// FUN_14036EDE0 (translated) over the gani file at `fileVa`, as FUN_1415B8F40 (0x1415B8F40..) calls it: the chunk list starts at
// the file's +4 offset, the chunk's data at its +0xC offset. Zero when the file has no such chunk.
uint64_t MotionChunkData(const uint8_t* pack, uint64_t packVa, std::size_t packBytes, uint64_t fileVa) {
  PesState s;
  s.mem.window.base = packVa;
  s.mem.window.size = packBytes;
  s.mem.window.ro = pack;
  s.mem.segs.push_back(PesMem::Seg{kChunkStackVa, std::vector<uint8_t>(kChunkStackBytes)});
  s.gpr[4] = kChunkStackVa + kChunkStackBytes;
  const uint32_t listOffset = Load<uint32_t>(pack + (fileVa - packVa) + 4);
  if (listOffset == 0) return 0;
  s.gpr[1] = fileVa + listOffset;
  s.gpr[2] = kMotionChunkHash;
  ::pes_fn_14036EDE0(s);
  const uint64_t chunk = s.gpr[0];
  if (chunk == 0) return 0;
  const uint32_t dataOffset = Load<uint32_t>(pack + (chunk - packVa) + 0xC);
  return dataOffset == 0 ? 0 : chunk + dataOffset;
}

// The largest track index of the units FUN_1423A3FD0 evaluates into pose slots (types 5 and 6) in the clip at `clip` (< end).
int MaxSlotTrack(const uint8_t* clip, const uint8_t* end) {
  int maxTrack = -1;
  if (clip + kClipListOffsetField + 4 > end || Load<int32_t>(clip) < 1) return maxTrack;
  const uint32_t listOffset = Load<uint32_t>(clip + kClipListOffsetField);
  if (listOffset == 0) return maxTrack;
  const uint8_t* list = clip + listOffset;
  if (list + kListFirstUnitOffset > end || (list[kListFlagsOffset] & kListNoUnits)) return maxTrack;
  const uint8_t* unit = list + kListFirstUnitOffset;
  for (int i = 0; i < static_cast<int8_t>(list[kListCountOffset]) && unit + kUnitBytes <= end; ++i) {
    const uint8_t kind = unit[kUnitTypeOffset];
    if ((kind & kUnitTypeMask) == kUnitMoverPosition || (kind & kUnitTypeMask) == kUnitMoverRotation)
      maxTrack = std::max(maxTrack, static_cast<int>(Load<int16_t>(unit + kUnitTrackOffset)));
    if (!(kind & kUnitHasNext)) break;
    unit += kUnitBytes;
  }
  return maxTrack;
}

}  // namespace

std::optional<PesAnimeChain> BuildAnimeChain(const PeImage& exe, const PesVfs& vfs, std::string* error) {
  const auto names = MotionNames(exe, vfs, error);
  if (!names) return std::nullopt;

  struct Pack {
    std::vector<uint8_t> file;
    Mtar mtar;
    std::size_t fileAt = 0, objectAt = 0, entriesAt = 0;
  };
  std::vector<Pack> packs(kBodyArchiveCount);
  for (int i = 0; i < kBodyArchiveCount; ++i) {
    auto bytes = vfs.Read(BodyArchivePath(i), error);
    if (!bytes) return std::nullopt;
    auto mtar = ParseMtar(*bytes, error);
    if (!mtar) return std::nullopt;
    // FUN_1415BB730's binary search needs the table sorted by key (index << 32 | name hash).
    uint64_t previous = 0;
    for (const MtarEntry& e : mtar->entries) {
      const uint64_t key = (uint64_t(e.index) << 32) | e.nameHash;
      if (key < previous) {
        *error = "anime chain: " + BodyArchivePath(i) + " is not sorted by key";
        return std::nullopt;
      }
      previous = key;
    }
    packs[i].file = std::move(*bytes);
    packs[i].mtar = std::move(*mtar);
  }

  PesAnimeChain out;
  out.segment.va = kAnimeChainVa;
  std::vector<uint8_t>& seg = out.segment.bytes;
  seg.resize(kDynamicOffset);
  const auto va = [](std::size_t at) { return kAnimeChainVa + at; };

  // The packs, verbatim, then each pack's object and entry array.
  for (Pack& p : packs) {
    p.fileAt = Alloc(seg, p.file.size());
    std::memcpy(seg.data() + p.fileAt, p.file.data(), p.file.size());
  }
  int maxTrack = -1;
  std::unordered_map<uint32_t, uint64_t> keyOfName;  // the first pack that holds a name wins, as FindBodyClip
  for (Pack& p : packs) {
    p.objectAt = Alloc(seg, kPackObjectBytes);
    p.entriesAt = Alloc(seg, kEntryBytes * p.mtar.entries.size());
    Store<uint32_t>(seg, p.objectAt + kPackStateOffset, kPackLoaded);
    Store<uint64_t>(seg, p.objectAt + kPackFileOffset, va(p.fileAt));
    Store<uint64_t>(seg, p.objectAt + kPackEntriesOffset, va(p.entriesAt));
    for (std::size_t i = 0; i < p.mtar.entries.size(); ++i) {
      const MtarEntry& e = p.mtar.entries[i];
      const uint64_t key = (uint64_t(e.index) << 32) | e.nameHash;
      keyOfName.emplace(e.nameHash, key);
      const uint64_t fileVa = va(p.fileAt) + e.offset;
      const uint64_t clipVa = MotionChunkData(seg.data() + p.fileAt, va(p.fileAt), p.file.size(), fileVa);
      const std::size_t at = p.entriesAt + i * kEntryBytes;
      Store<uint64_t>(seg, at + kEntryKeyOffset, key);
      Store<uint64_t>(seg, at + kEntryPackOffset, va(p.objectAt));
      Store<uint64_t>(seg, at + kEntryFileOffset, fileVa);
      Store<uint64_t>(seg, at + kEntryClipOffset, clipVa);
      Store<uint32_t>(seg, at + kEntryStateOffset, kEntryState);
      if (clipVa != 0)
        maxTrack = std::max(maxTrack, MaxSlotTrack(seg.data() + (clipVa - kAnimeChainVa), seg.data() + p.fileAt + p.file.size()));
    }
  }

  // The database: one record per pack.
  const std::size_t records = Alloc(seg, kPackRecordBytes * packs.size());
  for (std::size_t i = 0; i < packs.size(); ++i)
    Store<uint64_t>(seg, records + i * kPackRecordBytes + kPackRecordObjectOffset, va(packs[i].objectAt));
  Store<uint32_t>(seg, kDatabaseOffset + kDbPackCountOffset, static_cast<uint32_t>(packs.size()));
  Store<uint64_t>(seg, kDatabaseOffset + kDbPacksOffset, va(records));

  // The key table: motion m -> the key of the clip its name hashes to.
  const std::size_t keys = Alloc(seg, kKeyEntryBytes * names->size());
  for (std::size_t m = 0; m < names->size(); ++m) {
    const auto it = keyOfName.find(StrCode32((*names)[m]));
    if (it == keyOfName.end()) {
      *error = "anime chain: motion " + std::to_string(m) + " (" + (*names)[m] + ") has no clip in the body archives";
      return std::nullopt;
    }
    Store<uint64_t>(seg, keys + m * kKeyEntryBytes, it->second);
  }
  Store<uint32_t>(seg, kKeyTableOffset + kKeyCountOffset, static_cast<uint32_t>(names->size()));
  Store<uint64_t>(seg, kKeyTableOffset + kKeyEntriesOffset, va(keys));

  // The layer chain down to the pose slot table: one layer, weight 1, whose slot table covers every track a clip evaluates.
  // ponytail: the pose slots hold zero. PES's anime player keeps the player's current pose in them (the update FUN_141AF7A90 writes
  // them each tick, SetMotion FUN_141AF61C0 builds the table); GF runs no anime player. The reader evaluates its clip into one slot
  // and puts the slot's bytes back, so only the pose's own values would differ. Remove when the player's layer objects are ported.
  const std::size_t tracks = static_cast<std::size_t>(maxTrack + 1);
  const std::size_t tableBytes = kSlotOffsetTable + kSlotOffsetStride * tracks;
  const std::size_t slotsAt = (tableBytes + 15) & ~std::size_t(15);
  const std::size_t slotTable = Alloc(seg, slotsAt + kSlotBytes * tracks);
  Store<int32_t>(seg, slotTable + kSlotFirstTrackOffset, 0);
  for (std::size_t k = 0; k < tracks; ++k)
    Store<uint32_t>(seg, slotTable + kSlotOffsetTable + kSlotOffsetStride * k, static_cast<uint32_t>(slotsAt + kSlotBytes * k));
  Store<uint64_t>(seg, kPoseSetOffset + kPoseLayerOffset + kLayerSlotTableOffset, va(slotTable));
  Store<uint32_t>(seg, kPoseSetOffset + kPoseWeightsOffsetField, static_cast<uint32_t>(kPoseWeightsOffset));
  Store<uint16_t>(seg, kPoseSetOffset + kPoseCountOffset, 1);
  Store<uint32_t>(seg, kPoseSetOffset + kPoseLayerOffsets, static_cast<uint32_t>(kPoseLayerOffset));
  Store<float>(seg, kPoseSetOffset + kPoseWeightsOffset, kLayerWeight);
  for (const std::size_t container : {kContainer0Offset, kContainer1Offset}) {
    Store<uint32_t>(seg, container + kContainerIndexOffset, 0);
    Store<uint32_t>(seg, container + kContainerStateOffset, container == kContainer0Offset ? kContainerReady : 0);
  }
  Store<uint64_t>(seg, kContainer0Offset + kEntryPoseSetOffset, va(kPoseSetOffset));
  Store<int32_t>(seg, kContainer0Offset + kEntryLengthOffset, 1);
  Store<uint64_t>(seg, kLayer2Offset + kL2Container0Offset, va(kContainer0Offset));
  Store<uint64_t>(seg, kLayer2Offset + kL2Container1Offset, va(kContainer1Offset));
  Store<uint64_t>(seg, kLayerOffset + kL1ChildOffset, va(kLayer2Offset));

  out.globals = {{kClipDatabaseGlobalVa, va(kDatabaseOffset)}};
  out.keyTableEntriesVa = va(keys);
  out.databaseVa = va(kDatabaseOffset);
  out.motionCount = names->size();
  out.trackCount = tracks;
  return out;
}

}  // namespace pes
