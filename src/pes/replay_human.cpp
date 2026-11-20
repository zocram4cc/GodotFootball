#include "pes/replay_human.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include "pes/exe.hpp"
#include "pes/install_sections.hpp"
#include "pes_native.hpp"
#include "pes_state.hpp"

void pes_fn_14217DEF0(PesState& s);
void pes_fn_1403AFBA0(PesState& s);
void pes_fn_1403AC690(PesState& s);
void pes_fn_1403B07D0(PesState& s);

namespace pes {

namespace {

// The image's own pointers into the replay: the manager global M (its +0x6A8 is the unit U), and the TLS
// block the record's release path reads through gs:[0x58] (index DAT_1437067A0 = 0).
constexpr uint64_t kManagerGlobalVa = 0x1436F5118;
constexpr uint64_t kReplayVtableVa = 0x142BF7D58;  // the ReplayHuman vtable (image .rdata)
constexpr uint64_t kManagerUnitOff = 0x6A8;
constexpr uint64_t kRecordVtableOff = 0x0, kRecordActiveOff = 0x18, kRecordHumanOff = 0x28;
constexpr uint64_t kRecordBytesOff = 0x50, kRecordWord1c = 0x1C, kRecordByte21 = 0x21;
constexpr uint64_t kListFramesOff = 0x10;
constexpr uint64_t kUnitPaletteOff = 0x20, kUnitFramesOff = 0x40;
constexpr uint64_t kFrameStride = 300, kFrameGuardOff = 0x30C, kFramePaletteOff = 0x36B;
constexpr uint64_t kPaletteEntriesOff = 0x80, kPaletteEntryStride = kPaletteEntryBytes;
// The valid palette indices are below this count (FUN_1420B6A30 tests the byte against 0x27).
constexpr uint8_t kPaletteCount = 0x27;
constexpr uint64_t kTebTlsOff = 0x58;
constexpr uint64_t kStackVa = 0x10000000, kStackBytes = 0x20000;

// Host regions in PES layout, each at a PES VA outside the image, the stack and the heap arena.
enum Region { kR, kL, kF, kK, kU, kP, kG2, kM, kTeb, kTls, kSlot, kRegions };
struct RegionLayout {
  uint64_t va, size;
};
constexpr RegionLayout kLayout[kRegions] = {
    {0x20000000, 0x1000},   // kR: the ReplayHuman record
    {0x20010000, 0x1000},   // kL: the frame list (+0x10 = frames)
    {0x20020000, 0x10000},  // kF: the frames of the list (300-byte human records)
    {0x20030000, 0x1000},   // kK: the frame key (two words)
    {0x20040000, 0x1000},   // kU: the unit (+0x20 palette, +0x40 frame table)
    {0x20050000, 0x4000},   // kP: the palette (0xA0-byte entries from +0x80)
    {0x20060000, 0x10000},  // kG2: the unit's frame table (0x1FB0-byte frames)
    {0x20070000, 0x1000},   // kM: the replay manager (+0x6A8 = the unit)
    {0x20080000, 0x1000},   // kTeb: the thread block (+0x58 = the TLS array)
    {0x20090000, 0x1000},   // kTls: the TLS array (index 0 = the slot block)
    {0x200A0000, 0x1000},   // kSlot: the slot block (+0x58 = the saved word)
};

void PutU64(std::vector<uint8_t>& b, uint64_t off, uint64_t v) { std::memcpy(b.data() + off, &v, 8); }
void PutU32(std::vector<uint8_t>& b, uint64_t off, uint32_t v) { std::memcpy(b.data() + off, &v, 4); }
void PutU8(std::vector<uint8_t>& b, uint64_t off, uint8_t v) { b[off] = v; }

}  // namespace

struct ReplayHumanFrame::Impl {
  PesState s;
  std::vector<uint8_t> host[kRegions];
  std::unique_ptr<PesHeap> heap;
  size_t dataSeg = 0;
};

ReplayHumanFrame::ReplayHumanFrame() : impl_(std::make_unique<Impl>()) {}
ReplayHumanFrame::~ReplayHumanFrame() = default;

std::unique_ptr<ReplayHumanFrame> ReplayHumanFrame::Create(const PeImage& exe, std::string* error) {
  std::vector<uint8_t> rdata, data;
  const uint64_t rdataMapped = ReadSection(exe, kRdataVa, kRdataBytes, &rdata);
  const uint64_t dataMapped = ReadSection(exe, kDataVa, kDataBytes, &data);
  if (rdataMapped == 0 || dataMapped == 0) {
    if (error) *error = "the install does not map the sections the replay frame read reads";
    return nullptr;
  }

  std::unique_ptr<ReplayHumanFrame> f(new ReplayHumanFrame());
  Impl& m = *f->impl_;
  PesState& s = m.s;
  // PesHeap::Attach replaces s.mem.services, so it runs before the host windows are pushed (the reverse order dropped
  // every region: the record read as zero and the function returned at its first gate).
  m.heap = std::make_unique<PesHeap>();
  m.heap->Attach(s);
  for (int r = 0; r < kRegions; ++r) {
    m.host[r].assign(kLayout[r].size, 0);
    s.mem.services.push_back(PesMem::Window{kLayout[r].va, kLayout[r].size, nullptr, m.host[r].data()});
  }
  s.mem.img.push_back({kRdataVa, std::move(rdata)});
  m.dataSeg = s.mem.segs.size();
  s.mem.segs.push_back({kDataVa, std::move(data)});
  std::vector<uint8_t> stack(kStackBytes, 0);
  s.mem.segs.push_back({kStackVa, std::move(stack)});
  // The allocator entry 0x14038E3E0 is a boundary service (pes_native_region_alloc): the region manager, its thresholds and
  // its VirtualAlloc boot are not run, so the replay needs no boot window.

  // The thread block: gs:[0x58] is the TLS array; its index 0 is the slot block.
  s.gs_base = kLayout[kTeb].va;
  return f;
}

ReplayHumanRecord ReplayHumanFrame::Read(const ReplayFrameInputs& in) {
  Impl& m = *impl_;
  PesState& s = m.s;
  std::vector<uint8_t>& image = s.mem.segs[m.dataSeg].bytes;
  auto& r = m.host[kR];
  auto& l = m.host[kL];
  auto& f = m.host[kF];
  auto& k = m.host[kK];
  auto& u = m.host[kU];
  auto& p = m.host[kP];
  auto& g2 = m.host[kG2];
  auto& mg = m.host[kM];
  auto& teb = m.host[kTeb];
  auto& tls = m.host[kTls];

  // The record: vtable, active flag and human. The host record persists between reads, so an object at +0x48 does too.
  PutU64(r, kRecordVtableOff, kReplayVtableVa);
  PutU8(r, kRecordActiveOff, in.active);
  PutU32(r, kRecordHumanOff, in.human);
  // The idle state the five real entries hold (live2/pid2998104, replay_frame_14217DEF0): +0x1C released (-1), +0x21 set.
  PutU32(r, kRecordWord1c, 0xFFFFFFFFu);
  PutU8(r, kRecordByte21, 1);

  // The frame list, the key (frame 0, word 0), and the frame's human record.
  PutU64(l, kListFramesOff, kLayout[kF].va);
  PutU32(k, 0, 0);
  const uint64_t humanOff = static_cast<uint64_t>(in.human) * kFrameStride;
  PutU32(f, humanOff + kFrameGuardOff, in.guard);
  PutU8(g2, humanOff + kFramePaletteOff, in.paletteIndex);

  // The unit: the palette and the frame table; the palette entry the index selects.
  PutU64(u, kUnitPaletteOff, kLayout[kP].va);
  PutU64(u, kUnitFramesOff, kLayout[kG2].va);
  PutU64(mg, kManagerUnitOff, kLayout[kU].va);
  // PES reads an entry only for indices below its palette count (FUN_1420B6A30: byte < 0x27); above it the
  // function returns before the entry is touched, so the entry is not written.
  if (in.paletteIndex < kPaletteCount) {
    std::copy(in.entry.begin(), in.entry.end(),
              p.begin() + kPaletteEntriesOff + static_cast<uint64_t>(in.paletteIndex) * kPaletteEntryStride);
  }

  // The thread block and the TLS array (index 0 is the slot block, whose +0x58 the release saves to).
  PutU64(teb, kTebTlsOff, kLayout[kTls].va);
  PutU64(tls, 0, kLayout[kSlot].va);

  // The image global M: the manager object the record reads through (the .data copy holds the pointer).
  PutU64(image, kManagerGlobalVa - kDataVa, kLayout[kM].va);

  // Enter the translated function as the dispatch does: return address at [rsp], rcx = R, rdx = L, r8 = K.
  s.gpr[4] = kStackVa + kStackBytes - 0x100;
  s.gpr[4] -= 8;
  s.wr(s.gpr[4], 0, 8);
  s.gpr[1] = kLayout[kR].va;
  s.gpr[2] = kLayout[kL].va;
  s.gpr[8] = kLayout[kK].va;
  pes_fn_14217DEF0(s);
  s.gpr[4] += 8;

  ReplayHumanRecord out;
  std::copy_n(r.begin() + kRecordBytesOff, kPaletteEntryBytes, out.bytes.begin());
  std::memcpy(&out.word1c, r.data() + kRecordWord1c, 4);
  out.byte21 = r[kRecordByte21];
  return out;
}

}  // namespace pes
