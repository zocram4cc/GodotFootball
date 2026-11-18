#include "pes/hud_clock_build.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "pes/exe.hpp"
#include "pes/install_sections.hpp"
#include "pes_state.hpp"

void pes_fn_1420EFEC0(PesState& s);

namespace pes {

namespace {

// Image globals the builder reads: G (MI at +0x270, E at +0x2C0), the DS object FUN_14149A470
// returns (the FUN_1420EFA10 scale), and the replay flag byte.
constexpr uint64_t kGlobalGVa = 0x1436F3DC8;
constexpr uint64_t kGlobalDsVa = 0x143704E38;
constexpr uint64_t kReplayFlagVa = 0x1437F968A;
// The controller's vtable slot 9 (+0x48) is the scorebug class's FUN_1420F04B0 (returns 6).
constexpr uint64_t kSlot9TargetVa = 0x1420F04B0;
constexpr uint64_t kControllerSlot9 = 0x48;
constexpr uint64_t kGMiOff = 0x270, kGEnvOff = 0x2C0;
// The controller: vtable at +0x0, attention object at +0x30, match env E at +0x38.
constexpr uint64_t kCAttentionOff = 0x30, kCEnvOff = 0x38;

// Host regions in PES layout, each at a PES VA outside the image and the stack.
enum Region { kC, kX, kE, kG, kM, kV, kO, kDs, kRegions };
struct RegionLayout {
  uint64_t va, size;
};
constexpr RegionLayout kLayout[kRegions] = {
    {0x20000000, 0x1000},  // kC: controller
    {0x20010000, 0x1000},  // kX: attention object
    {0x20020000, 0x4000},  // kE: match env
    {0x20030000, 0x3000},  // kG: the G object
    {0x20040000, 0x2000},  // kM: MI record
    {0x20050000, 0x1000},  // kV: controller vtable
    {0x20060000, 0x1000},  // kO: output struct (param_2)
    {0x20070000, 0x1000},  // kDs: the DS object
};

constexpr uint64_t kStackVa = 0x10000000, kStackBytes = 0x20000;
constexpr uint64_t kOutWords = 17;  // the builder writes 17 words at O+0

void PutU64(std::vector<uint8_t>& b, uint64_t off, uint64_t v) { std::memcpy(b.data() + off, &v, 8); }
void PutU32(std::vector<uint8_t>& b, uint64_t off, uint32_t v) { std::memcpy(b.data() + off, &v, 4); }
void PutU8(std::vector<uint8_t>& b, uint64_t off, uint8_t v) { b[off] = v; }
void PutF32(std::vector<uint8_t>& b, uint64_t off, float v) { std::memcpy(b.data() + off, &v, 4); }

}  // namespace

struct ClockStateBuilder::Impl {
  PesState s;
  // Host storage for each PES-layout region; the services windows point into these.
  std::vector<uint8_t> host[kRegions];
  // .data is held in s.mem.segs[0] (the builder patches the image globals there).
  size_t dataSeg = 0;
};

ClockStateBuilder::ClockStateBuilder() : impl_(std::make_unique<Impl>()) {}
ClockStateBuilder::~ClockStateBuilder() = default;

std::unique_ptr<ClockStateBuilder> ClockStateBuilder::Create(const PeImage& exe, std::string* error) {
  std::vector<uint8_t> rdata, data;
  const uint64_t rdataMapped = ReadSection(exe, kRdataVa, kRdataBytes, &rdata);
  const uint64_t dataMapped = ReadSection(exe, kDataVa, kDataBytes, &data);
  if (rdataMapped == 0 || dataMapped == 0) {
    if (error) *error = "the install does not map the sections the HUD clock builder reads";
    return nullptr;
  }

  std::unique_ptr<ClockStateBuilder> b(new ClockStateBuilder());
  Impl& m = *b->impl_;
  PesState& s = m.s;
  for (int r = 0; r < kRegions; ++r) {
    m.host[r].assign(kLayout[r].size, 0);
    s.mem.services.push_back(PesMem::Window{kLayout[r].va, kLayout[r].size, nullptr, m.host[r].data()});
  }
  s.mem.img.push_back({kRdataVa, std::move(rdata)});
  m.dataSeg = s.mem.segs.size();
  s.mem.segs.push_back({kDataVa, std::move(data)});
  std::vector<uint8_t> stack(kStackBytes, 0);
  s.mem.segs.push_back({kStackVa, std::move(stack)});
  return b;
}

ClockStateWords ClockStateBuilder::Build(const ClockEnvInputs& in) {
  Impl& m = *impl_;
  PesState& s = m.s;
  std::vector<uint8_t>& image = s.mem.segs[m.dataSeg].bytes;

  // Zero every host region, then write the PES-layout fields the builder reads.
  for (auto& h : m.host) std::fill(h.begin(), h.end(), 0);
  auto& c = m.host[kC];
  auto& e = m.host[kE];
  auto& g = m.host[kG];
  auto& mi = m.host[kM];

  PutU64(c, 0x0, kLayout[kV].va);
  PutU64(c, kCAttentionOff, kLayout[kX].va);
  PutU64(c, kCEnvOff, kLayout[kE].va);
  PutU64(m.host[kV], kControllerSlot9, kSlot9TargetVa);
  PutU64(g, kGEnvOff, kLayout[kE].va);
  PutU64(g, kGMiOff, kLayout[kM].va);
  PutU32(mi, 0x1168, in.period);
  PutU32(mi, 0x116C, in.inPlay);
  PutU8(e, 0x9E, in.limitYellow[0]);
  PutU8(e, 0x9F, in.limitYellow[1]);
  PutF32(e, 0xA0, in.liveSeconds);
  PutU8(e, 0xA4, in.liveFlag);
  PutU8(e, 0xB0, in.limitRed[0]);
  PutU8(e, 0xB1, in.limitRed[1]);
  PutU8(e, 0x114, in.clockStopped);
  PutF32(e, 0x11C, in.frozenSeconds);
  PutU8(e, 0x2DA8, in.addedMinutes);

  // The image globals the builder reads: G and DS point at their regions, and the replay flag is set.
  PutU64(image, kGlobalGVa - kDataVa, kLayout[kG].va);
  PutU64(image, kGlobalDsVa - kDataVa, kLayout[kDs].va);
  PutU8(image, kReplayFlagVa - kDataVa, in.replayFlag ? 1 : 0);

  // Enter the translated builder as the dispatch does: a return address at [rsp], rcx = C, rdx = O.
  s.gpr[4] = kStackVa + kStackBytes - 0x100;
  s.gpr[4] -= 8;
  s.wr(s.gpr[4], 0, 8);
  s.gpr[1] = kLayout[kC].va;
  s.gpr[2] = kLayout[kO].va;
  pes_fn_1420EFEC0(s);
  s.gpr[4] += 8;

  ClockStateWords out{};
  const auto& o = m.host[kO];
  for (uint64_t w = 0; w < kOutWords; ++w) std::memcpy(&out[w], o.data() + 4 * w, 4);
  return out;
}

}  // namespace pes
