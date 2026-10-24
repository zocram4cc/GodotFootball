#include "pessetpiecetaker.hpp"

#include <cstdint>

namespace pes {
namespace {

// Plain .rdata floats, all four verified against the shipped exe (OBS,
// docs/re/pes-re-setpiece-choreography.md 9 item 2 and
// pes-re-setpiece-kicker.md 2.4).
constexpr uint64_t CORNER_GATE_VA = 0x14259bde8;         // 3.5
constexpr uint64_t FREEKICK_GATE_VA = 0x14258c264;       // 3.0
constexpr uint64_t ONE_VA = 0x14256ae1c;                 // 1.0
constexpr uint64_t SHORT_CORNER_WINDOW_VA = 0x142565604;  // 2.0
constexpr uint64_t SHORT_CORNER_RUNNING_VA = 0x14259bdf0;  // 7.0
constexpr uint64_t HALF_SECOND_VA = 0x142565600;       // 0.5: the goal kick ladder's windows and rounding
constexpr uint64_t TACTICS_GATE_VA = 0x142590d98;      // 0.3: the special restart's early-out (sub_142136F00 case 2)
constexpr uint64_t CENTIMETRE_VA = 0x14256ae18;        // 0.01: the Mbinfo hit records are in cm (FUN_1406E8CC0)
constexpr uint64_t PHYSIQUE_LOW_VA = 0x1425ddf78;      // 0.9166: FUN_1406E8CC0's physique clamp, low
constexpr uint64_t PHYSIQUE_HIGH_VA = 0x1425ddf80;     // 1.0833: and high
constexpr uint64_t HIT_TICK_DIVISOR_VA = 0x14259a764;  // 60: a hit's tick divisor (FUN_1406B03D0)
constexpr uint64_t THROW_IN_PICKUP_REACH_VA = 0x14259222c;  // 5.0: the throw-in pickup clip's reach (choreography 3.2)
constexpr uint64_t THROW_IN_RELEASE_SHORT_VA = 0x1425c5948;  // 13.0: the carry's release row switches to the mid row beyond it
constexpr uint64_t THROW_IN_RELEASE_LONG_VA = 0x14259be20;  // 25.0: the placed taker's idle release row switches to the long row beyond it
constexpr uint64_t THROW_IN_PUSH_INSET_VA = 0x142588200;  // 0.1: the release margin the taker keeps OUTSIDE the touchline (throwin 4.2, FUN_140792E10)
constexpr uint64_t WALK_DOWN_KMH_VA = 0x14259bdfc;  // 11.0 km/h: the MOVE filler's walk-down speed (FUN_1406b7560)
constexpr float kKmhPerMps = 3.6f;                  // a unit conversion: km/h per m/s

bool ReadFloat(const PeImage& exe, uint64_t va, float* out) {
  return exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out));
}

}  // namespace

std::optional<SetPieceTakerConstants> LoadSetPieceTakerConstants(const PesVfs& vfs,
                                                                 const PeImage& exe,
                                                                 std::string* error) {
  SetPieceTakerConstants k{};
  bool ok = true;
  for (auto [va, out] :
       {std::pair{CORNER_GATE_VA, &k.cornerGate_s}, std::pair{FREEKICK_GATE_VA, &k.freeKickGate_s},
        std::pair{ONE_VA, &k.penaltyGate_s}, std::pair{SHORT_CORNER_WINDOW_VA, &k.shortCornerWindow_s},
        std::pair{SHORT_CORNER_RUNNING_VA, &k.shortCornerRunningWindow_s},
        std::pair{HALF_SECOND_VA, &k.halfSecond_s}, std::pair{TACTICS_GATE_VA, &k.goalKickTacticsGate},
        std::pair{CENTIMETRE_VA, &k.centimetre_m}, std::pair{PHYSIQUE_LOW_VA, &k.physiqueLow},
        std::pair{PHYSIQUE_HIGH_VA, &k.physiqueHigh}, std::pair{HIT_TICK_DIVISOR_VA, &k.hitTickDivisor},
        std::pair{THROW_IN_PICKUP_REACH_VA, &k.throwInPickupReach_m},
        std::pair{THROW_IN_RELEASE_SHORT_VA, &k.throwInReleaseShort_m},
        std::pair{THROW_IN_RELEASE_LONG_VA, &k.throwInReleaseLong_m},
        std::pair{THROW_IN_PUSH_INSET_VA, &k.throwInPushInset_m}}) {
    if (!ReadFloat(exe, va, out)) {
      char buf[32];
      std::snprintf(buf, sizeof buf, "%#llx", static_cast<unsigned long long>(va));
      *error = std::string("set-piece taker constant: unreadable .rdata at ") + buf;
      ok = false;
    }
  }
  float walkDownKmh = 0.0f;
  if (!ReadFloat(exe, WALK_DOWN_KMH_VA, &walkDownKmh)) {
    *error = "set-piece taker constant: unreadable .rdata at 0x14259bdfc (the walk-down speed)";
    ok = false;
  }
  if (!ok) return std::nullopt;
  k.walkDown_ms = walkDownKmh / kKmhPerMps;
  // A gate of zero or less would release the kicker instantly; the shipped
  // values are all positive and a failed read must not look like a rule.
  if (k.cornerGate_s <= 0.0f || k.freeKickGate_s <= 0.0f || k.penaltyGate_s <= 0.0f ||
      k.shortCornerWindow_s <= 0.0f || k.shortCornerRunningWindow_s <= 0.0f || k.halfSecond_s <= 0.0f ||
      k.goalKickTacticsGate <= 0.0f || k.centimetre_m <= 0.0f || k.physiqueLow <= 0.0f ||
      k.hitTickDivisor <= 0.0f || k.walkDown_ms <= 0.0f) {
    *error = "set-piece taker constant: non-positive gate in .rdata";
    return std::nullopt;
  }
  // The VFS is not consulted for these: they are image constants, not VFS
  // objects. It is taken so the loader has the same shape as the other PES
  // loaders, which do read constant_*.bin records out of the install.
  (void)vfs;
  return k;
}

int ThrowInCarryReleaseRow(float distance_m, float shortReach_m) {
  return distance_m > shortReach_m ? kThrowInReleaseRunMid : kThrowInReleaseRunShort;
}

int ThrowInIdleReleaseRow(float distance_m, float shortReach_m, float longReach_m) {
  if (distance_m <= shortReach_m) return kThrowInReleaseIdleShort;
  return distance_m <= longReach_m ? kThrowInReleaseIdleMid : kThrowInReleaseIdleLong;
}

}  // namespace pes
