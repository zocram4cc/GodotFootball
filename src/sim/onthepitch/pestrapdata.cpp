#include "onthepitch/pestrapdata.hpp"

#include <array>
#include <cstring>

#include "onthepitch/pesdefenderdecisions.hpp"
#include "pes/animetable.hpp"

namespace pes {
namespace {

// DAT_1425DC370: one u32 per Mbinfo kind, the list capacity a motion record takes (FUN_1406AC4D0 and its siblings compare the
// count byte with it).
constexpr uint64_t kCapsVa = 0x1425DC370ULL;
// The class constant::Trap: vtable, RTTI .?AVTrap@constant@@; slot 2 (+0x10) is the loader FUN_141E67B70 of the constant files'
// object trees. Checked against the exe so a changed build is refused.
constexpr uint64_t kTrapVtableVa = 0x142B53158ULL;
constexpr uint64_t kTrapLoaderFnVa = 0x141E67B70ULL;
constexpr std::size_t kVtableLoaderSlotOffset = 0x10;

constexpr std::size_t kAnimetableCellBytes = 2;  // s16
// FUN_1406AA440 stores the mirror flag bytes after the s16 cells (offset 0x1AC34 = 0xBE5 x 0x12 x 2), one byte per cell.
constexpr std::size_t kAnimetableBufferBytes = kAnimetableCellCount * (kAnimetableCellBytes + 1);  // 0x2824E

// Table singleton, anime manager, registry object, registry entry: the words the chain reads are the first (+0) and, for the
// entry, the payload pointer at +8.
constexpr std::size_t kTableBytes = 0xD8, kManagerBytes = 0x40, kRegistryBytes = 0x40, kEntryBytes = 0x20;
constexpr std::size_t kEntryPayloadOffset = 8;
constexpr std::size_t kRegistryEndOffset = 8, kRegistryCapOffset = 0x10;  // the entry vector's end and capacity pointers
constexpr std::size_t kAlign = 16;

struct TrapField {
  uint16_t dst;
  uint8_t bytes;
  uint16_t leaf;
  uint8_t depth;
  std::array<uint8_t, 5> chain;
};
constexpr TrapField kTrapFields[] = {
#include "onthepitch/pestrapconstant_fields.inc"
};

template <typename T>
T Load(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof v);
  return v;
}

class Arena {
 public:
  explicit Arena(uint64_t base) : base_(base) {}
  // Appends n bytes (zero), returns their offset.
  std::size_t Alloc(std::size_t n) {
    const std::size_t at = (bytes_.size() + kAlign - 1) & ~(kAlign - 1);
    bytes_.resize(at + n);
    return at;
  }
  uint8_t* At(std::size_t offset) { return bytes_.data() + offset; }
  uint64_t Va(std::size_t offset) const { return base_ + offset; }
  std::vector<uint8_t> Take() { return std::move(bytes_); }

 private:
  uint64_t base_;
  std::vector<uint8_t> bytes_;
};

void Store64(uint8_t* at, uint64_t v) { std::memcpy(at, &v, sizeof v); }

// FUN_141E823E0 over the object tree: word `index` of the node at `at`, as a pointer (0 = null).
bool Child(const std::vector<uint8_t>& blob, std::size_t at, uint32_t index, std::size_t* out) {
  const std::size_t word = at + 4 * std::size_t(index);
  if (word + 4 > blob.size()) return false;
  const uint32_t offset = Load<uint32_t>(&blob[word]);
  if (offset == 0 || offset >= blob.size()) return false;
  *out = offset;
  return true;
}

bool BuildTrapObject(const std::vector<uint8_t>& blob, uint8_t* object, std::string* error) {
  for (const TrapField& f : kTrapFields) {
    std::size_t node = 0;
    for (int d = 0; d < f.depth; ++d) {
      if (!Child(blob, node, f.chain[d], &node)) {
        *error = "trap.o: offset chain of runtime +0x" + std::to_string(f.dst) + " leaves the object";
        return false;
      }
    }
    if (node + f.leaf + f.bytes > blob.size()) {
      *error = "trap.o: leaf of runtime +" + std::to_string(f.dst) + " is past the end";
      return false;
    }
    std::memcpy(object + f.dst, &blob[node + f.leaf], f.bytes);
  }
  return true;
}

}  // namespace

bool TrapRuntimeByteIsBuilt(std::size_t offset) {
  if (offset < sizeof(uint64_t)) return true;  // the vtable word
  for (const TrapField& f : kTrapFields)
    if (offset >= f.dst && offset < std::size_t(f.dst) + f.bytes) return true;
  return false;
}

std::optional<PesTrapLoaderData> BuildTrapLoaderData(const PeImage& exe, const PesVfs& vfs, std::string* error) {
  // The class is the one the chain's registry entry holds: its slot-2 loader is the function the field table was read from.
  const auto loader = exe.ReadU64(kTrapVtableVa + kVtableLoaderSlotOffset);
  if (!loader || *loader != kTrapLoaderFnVa) {
    *error = "trap: constant::Trap's loader slot is not FUN_141E67B70 in this exe";
    return std::nullopt;
  }
  const auto animetable = LoadAnimetable(exe, vfs, error);
  if (!animetable) return std::nullopt;
  std::vector<uint8_t> trap;
  if (!ReadConstantPlayerObject(vfs, "trap.o", &trap, error)) return std::nullopt;

  std::vector<std::vector<uint8_t>> files;
  std::vector<uint32_t> caps;
  for (const TrapMbinfoKind& k : kTrapMbinfoKinds) {
    auto bytes = vfs.Read(k.path, error);
    uint32_t cap = 0;
    if (!bytes || !exe.ReadVa(kCapsVa + 4 * uint64_t(k.kind), sizeof cap, reinterpret_cast<uint8_t*>(&cap))) {
      if (bytes) *error = "trap: the list caps at DAT_1425DC370 are not in the exe";
      return std::nullopt;
    }
    files.push_back(std::move(*bytes));
    caps.push_back(cap);
  }

  Arena arena(kTrapLoaderVa);
  const std::size_t table = arena.Alloc(kTableBytes), manager = arena.Alloc(kManagerBytes),
                    registry = arena.Alloc(kRegistryBytes), array = arena.Alloc(8 * (kTrapRegistryId + 1)),
                    entry = arena.Alloc(kEntryBytes), object = arena.Alloc(kTrapRuntimeBytes),
                    cells = arena.Alloc(kAnimetableBufferBytes), records = arena.Alloc(kMotionSlotCount * kMotionRecordBytes);
  std::vector<std::size_t> raw;
  for (const auto& f : files) raw.push_back(arena.Alloc(f.size()));

  // The raw file buffers: the builders point the records into them and copy nothing (FUN_1415A1708 copies the file in once).
  for (std::size_t i = 0; i < files.size(); ++i) std::memcpy(arena.At(raw[i]), files[i].data(), files[i].size());

  // The motion records (kinds 0, 1, 6, 13): per file record, motion key -> count byte at rec+kind and the pointer in the list.
  for (std::size_t i = 0; i < files.size(); ++i) {
    const TrapMbinfoKind& k = kTrapMbinfoKinds[i];
    for (std::size_t n = 0; n < files[i].size() / k.recordBytes; ++n) {
      const uint32_t motion = (Load<uint16_t>(&files[i][n * k.recordBytes]) & 0x1FFFu) + k.motionBase;
      // ponytail: PES files a record past the table (motion >= 0x1125) beyond its own block; nothing reads it back.
      if (motion >= kMotionSlotCount) continue;
      uint8_t* rec = arena.At(records + motion * kMotionRecordBytes);
      if (rec[k.kind] >= caps[i]) continue;  // the builders drop a record past the list cap
      Store64(rec + k.listOffset + 8 * std::size_t(rec[k.kind]), arena.Va(raw[i] + n * k.recordBytes));
      ++rec[k.kind];
    }
  }

  // The Animetable buffer: s16 cells (motion - 1) then the mirror flag bytes.
  const int cols = animetable->Layout().cols;
  for (std::size_t c = 0; c < kAnimetableCellCount; ++c) {
    const AnimetableCell cell = animetable->BufferCell(int(c / cols), int(c % cols));
    const int16_t motion = int16_t(cell.motionNo);
    std::memcpy(arena.At(cells) + kAnimetableCellBytes * c, &motion, sizeof motion);
    arena.At(cells)[kAnimetableCellCount * kAnimetableCellBytes + c] = cell.mirror ? 1 : 0;
  }

  // constant::Trap: the class's vtable, then the slot-2 loader's stores.
  Store64(arena.At(object), kTrapVtableVa);
  if (!BuildTrapObject(trap, arena.At(object), error)) return std::nullopt;

  // The objects between the globals and the data.
  Store64(arena.At(table), arena.Va(records));
  Store64(arena.At(manager), arena.Va(cells));
  Store64(arena.At(registry), arena.Va(array));
  Store64(arena.At(registry) + kRegistryEndOffset, arena.Va(array + 8 * (kTrapRegistryId + 1)));
  Store64(arena.At(registry) + kRegistryCapOffset, arena.Va(array + 8 * (kTrapRegistryId + 1)));
  Store64(arena.At(array) + 8 * kTrapRegistryId, arena.Va(entry));
  Store64(arena.At(entry) + kEntryPayloadOffset, arena.Va(object));

  PesTrapLoaderData out;
  out.motionTableVa = arena.Va(table), out.motionRecordsVa = arena.Va(records), out.animeManagerVa = arena.Va(manager);
  out.animetableVa = arena.Va(cells), out.registryVa = arena.Va(registry), out.trapVa = arena.Va(object);
  out.globals = {{kMotionTableGlobalVa, out.motionTableVa},
                 {kAnimeManagerGlobalVa, out.animeManagerVa},
                 {kRegistryGlobalVa, out.registryVa}};
  out.segment.va = kTrapLoaderVa;
  out.segment.bytes = arena.Take();
  return out;
}

}  // namespace pes
