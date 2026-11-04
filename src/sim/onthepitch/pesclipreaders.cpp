#include "pesclipreaders.hpp"

#include <cstring>

#include "pes/translated/pes_state.hpp"
#include "pes/translated/translated.hpp"
#include "pesimagetables.hpp"

namespace pes {
namespace {

// The record fallbacks the readers load when an entry has no record (the 0x1406AF620 / 0x1406AF8E0 / 0x1406B1D00 loads of
// `LEA RCX,[0x1436F2430]` and its siblings): one zero-word record each. Zero is Mbinfo's documented fallback (INF: the
// install's .data holds these objects, not captured by the proof's replays; the readers' record-absent arms are not covered).
constexpr uint64_t kBlendNoneVa = 0x1436F2430, kCancelNoneVa = 0x1436F2500, kRotNoneVa = 0x1436F23E8;
constexpr uint64_t kNoneBytes = 0x10;
constexpr uint64_t kStackBytes = 0x1000;
constexpr uint64_t kStackVa = kClipTableVa + 0x10000000ULL;
constexpr uint64_t kRecordBytes = 8;  // two words of a record: the readers read at most u32[1]

template <typename T>
void Put(std::vector<uint8_t>& image, uint64_t at, T value) {
  std::memcpy(&image[at], &value, sizeof value);
}

// One kind's list of an entry: the count byte, then a pointer per record into the pool at `*pool`.
void FileKind(std::vector<uint8_t>& image, uint64_t entry, uint64_t kind, uint64_t list,
              const std::vector<std::array<uint32_t, 2>>& words, uint64_t* pool) {
  image[entry + kind] = static_cast<uint8_t>(words.size());
  for (size_t i = 0; i < words.size(); ++i) {
    Put<uint64_t>(image, entry + list + 8 * i, kClipTableVa + *pool);
    Put<uint32_t>(image, *pool, words[i][0]);
    Put<uint32_t>(image, *pool + 4, words[i][1]);
    *pool += kRecordBytes;
  }
}

void (*Function(ClipReader reader))(PesState&) {
  switch (reader) {
    case ClipReader::kBlend: return ::pes_fn_1406AF620;
    case ClipReader::kCancel: return ::pes_fn_1406AF8E0;
    case ClipReader::kRotStart: return ::pes_fn_1406B1F00;
    case ClipReader::kRotEnd: return ::pes_fn_1406B1D00;
  }
  return nullptr;
}

}  // namespace

PesClipReaders::PesClipReaders(const Mbinfo& mbinfo) {
  // The table object (block pointer, default entry pointer), the block, the default entry, then the record pool.
  constexpr uint64_t kObject = 0, kBlock = 0x10;
  constexpr uint64_t kDefault = kBlock + uint64_t(kClipTableSlots) * kClipTableEntryBytes;
  uint64_t pool = kDefault + kClipTableEntryBytes;
  uint64_t records = 0;
  for (uint32_t motion = 0; motion < kClipTableSlots; ++motion) {
    const MotionRecord& m = mbinfo.Motion(static_cast<int>(motion));
    records += m.blendWords.size() + m.cancelWords.size() + m.rotWords.size();
  }
  table_.assign(pool + records * kRecordBytes, 0);
  Put<uint64_t>(table_, kObject, kClipTableVa + kBlock);
  Put<uint64_t>(table_, kObject + 8, kClipTableVa + kDefault);
  for (uint32_t motion = 0; motion < kClipTableSlots; ++motion) {
    const MotionRecord& m = mbinfo.Motion(static_cast<int>(motion));
    const uint64_t entry = kBlock + uint64_t(motion) * kClipTableEntryBytes;
    FileKind(table_, entry, kClipBlendKind, kClipBlendList, m.blendWords, &pool);
    FileKind(table_, entry, kClipCancelKind, kClipCancelList, m.cancelWords, &pool);
    FileKind(table_, entry, kClipRotKind, kClipRotList, m.rotWords, &pool);
  }
}

int PesClipReaders::Read(ClipReader reader, int motion, float rate, int index, float ticksPerSecond) const {
  PesState s;
  s.mem.window = {kClipTableVa, table_.size(), table_.data(), nullptr};
  for (const uint64_t va : {kBlendNoneVa, kCancelNoneVa, kRotNoneVa})
    s.mem.segs.push_back(PesMem::Seg{va, std::vector<uint8_t>(kNoneBytes)});
  // The frame-rate cells: the int at +0, the float at +4 (0x141E532D0 returns it).
  s.mem.segs.push_back(PesMem::Seg{kFrameCellsVa, std::vector<uint8_t>(kFrameCellsBytes)});
  const float rateCell = ticksPerSecond;
  std::memcpy(s.mem.segs.back().bytes.data() + sizeof(uint32_t), &rateCell, sizeof rateCell);
  s.mem.segs.push_back(PesMem::Seg{kStackVa, std::vector<uint8_t>(kStackBytes)});
  s.gpr[4] = kStackVa + kStackBytes;
  s.gpr[1] = kClipTableVa;
  s.gpr[2] = static_cast<uint32_t>(motion);
  s.set_x_f32(2, rate);
  s.gpr[9] = static_cast<uint32_t>(index);
  Function(reader)(s);
  return static_cast<int32_t>(s.gpr[0]);
}

}  // namespace pes
