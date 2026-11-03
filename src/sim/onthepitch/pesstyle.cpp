#include "onthepitch/pesstyle.hpp"

#include <cstring>

namespace pes::style {
namespace {

// The ten compat rows, 4 bytes each (census R1, pes-re-playing-styles.md §2).
constexpr uint64_t kCompatRowsVa = 0x142613d20;

}  // namespace

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
