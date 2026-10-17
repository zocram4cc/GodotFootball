#include "onthepitch/peslane.hpp"

#include <cstdint>

namespace pes::lane {

namespace {
// The scalars the lane core reads, by VA (see LaneConstants).
struct LaneScalar {
  uint64_t va;
  float LaneConstants::*field;
};
constexpr LaneScalar LANE_SCALARS[] = {
    {0x14259be40, &LaneConstants::radToDeg},
    {0x1425795e4, &LaneConstants::twoPi_rad},
    {0x142565600, &LaneConstants::coneScale},
    {0x142588210, &LaneConstants::coneOffset_deg},
    {0x14259be5c, &LaneConstants::coneFloor_deg},
    {0x14259bdc4, &LaneConstants::crossEps},
};
}  // namespace

std::optional<LaneConstants> LoadLaneConstants(const PeImage& exe, std::string* error) {
  auto trig = LoadSpeedClassConstants(exe, error);
  if (!trig) return std::nullopt;
  LaneConstants k{};
  k.trig = *trig;
  for (const LaneScalar& s : LANE_SCALARS)
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*s.field)))) {
      *error = "lane constants: .rdata unreadable";
      return std::nullopt;
    }
  return k;
}

// RED stub: behaviour is not written yet.
int KeeperSlot(int, const std::array<LanePlayer, kSlots>&) { return kNoSlot; }
bool IsAwaySlot(int) { return false; }
float HeadingAdd(const LaneConstants&, float heading_deg, float) { return heading_deg; }
LaneScan ScanLane(const LaneConstants&, const std::array<LanePlayer, kSlots>&, int, int,
                  const LaneRequest&, bool, int) {
  return LaneScan{};
}
bool LaneFree(const LaneConstants&, const std::array<LanePlayer, kSlots>&, int, const LaneRequest&,
              bool, int) {
  return true;
}

}  // namespace pes::lane
