#include "onthepitch/pesstyle.hpp"

#include <cstring>

namespace pes::style {
namespace {

// The ten compat rows, 4 bytes each (census R1, pes-re-playing-styles.md §2).
constexpr uint64_t kCompatRowsVa = 0x142613d20;

}  // namespace

int EngineBit(PlayingStyles::Player style) {
  using P = PlayingStyles::Player;
  switch (style) {
    case P::GoalPoacher: return 0;
    case P::DummyRunner: return 1;
    case P::FoxInTheBox: return 2;
    case P::TargetMan: return 3;
    case P::CreativePlaymaker: return 4;
    case P::ProlificWinger: return 5;
    case P::RoamingFlank: return 6;
    case P::CrossSpecialist: return 7;
    case P::ClassicNo10: return 8;
    case P::HolePlayer: return 9;
    case P::BoxToBox: return 10;
    case P::AnchorMan: return 11;
    case P::TheDestroyer: return 12;
    case P::Orchestrator: return 13;
    case P::BuildUp: return 14;
    case P::ExtraFrontman: return 15;
    case P::OffensiveFullBack: return 16;
    case P::DefensiveFullBack: return 17;
    case P::FullBackFinisher: return 18;
    case P::OffensiveGoalkeeper: return 19;
    case P::DefensiveGoalkeeper: return 20;
    case P::None:
    case P::Count:
      break;
  }
  return -1;
}

uint32_t StyleWord(PlayingStyles::Player style) {
  const int bit = EngineBit(style);
  return bit < 0 ? 0u : 1u << bit;
}

std::optional<Compat> LoadCompat(const PeImage& exe, std::string* error) {
  std::array<uint8_t, sizeof(Compat)> raw{};
  if (!exe.ReadVa(kCompatRowsVa, raw.size(), raw.data())) {
    *error = "playing-style compat rows unreadable at 0x142613d20";
    return std::nullopt;
  }
  Compat compat{};
  std::memcpy(compat.row.data(), raw.data(), raw.size());
  // An all-zero table would gate every style out; a failed read must not look like a rule.
  bool any = false;
  for (uint32_t r : compat.row) any = any || r != 0;
  if (!any) {
    *error = "playing-style compat rows are all zero at 0x142613d20";
    return std::nullopt;
  }
  return compat;
}

bool Gate(const Compat& compat, int roleClass, uint32_t styleWord, int bit) {
  if (bit < 0 || bit > 31 || roleClass < 0 || roleClass >= static_cast<int>(compat.row.size()))
    return false;
  const uint32_t mask = 1u << bit;
  return (styleWord & mask) != 0 && (compat.row[roleClass] & mask) != 0;
}

}  // namespace pes::style
