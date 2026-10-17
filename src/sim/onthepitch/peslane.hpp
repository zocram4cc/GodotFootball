#pragma once

// PES 2021's pass-lane blocker test (docs/re/pes-re-lane-core.md 3-5). The core 0x14063CD90 takes
// one side's eleven players, predicts each as P + h*V, and records those that sit in the
// annulus-and-cone of the lane (0x140A53DF0) or whose predicted path crosses the lane segment
// (0x140A90D80). It returns the recorded player nearest the lane origin and the count. The lane is
// free when the opposing side records nobody (0x14063DC10).
//
// Angles are degrees. Positions are the ground plane (x, z); height is not read by the test.
// Every constant comes from the install through LoadLaneConstants. Nothing in the carrier calls
// this yet.

#include <array>
#include <optional>
#include <string>

#include "onthepitch/pesoffball.hpp"
#include "pes/exe.hpp"
#include "pes/movestep.hpp"

namespace pes::lane {

inline constexpr int kSlots = 22;     // both sides
inline constexpr int kSideSize = 11;  // players per side
inline constexpr int kNoSlot = 0xFF;  // PES's "no slot" result

struct LaneConstants {
  SpeedClassConstants trig;   // the sine table 0x142655950 and its scalars (0x140A92C80)
  float radToDeg;             // 57.29578 (0x14259BE40): radians to degrees
  float twoPi_rad;            // 6.2831855 (0x1425795E4): the atan2 negative wrap
  float coneScale;            // 0.5 (0x142565600): half-cone is coneScale * cone
  float coneOffset_deg;       // 10 (0x142588210): added to the half-cone for the opponent test
  float coneFloor_deg;        // 90 (0x14259BE5C): the opponent test never allows less than this
  float crossEps;             // 1.1920929e-7 (0x14259BDC4): floor on the crossing denominator
};

std::optional<LaneConstants> LoadLaneConstants(const PeImage& exe, std::string* error);

struct LanePlayer {
  offball::Vec2f pos;          // m (record +0x55C, +0x564)
  offball::Vec2f vel;          // m per PES frame (record +0x574, +0x57C)
  bool active = false;         // MI+0x1748[slot] != 0
  int positionCategory = -1;   // the side's +0x4C00[j]; 0 marks the keeper (spec O1, INF)
};

// The lane (the struct 0x14063DE20 builds): origin O, heading theta, length, cone c and the radius
// offset of the cone apex behind O.
struct LaneRequest {
  offball::Vec2f origin;
  float heading_deg = 0.0f;
  float len_m = 0.0f;
  float cone_deg = 0.0f;
  float rad_m = 0.0f;
};

struct LaneScan {
  int nearest = kNoSlot;  // recorded slot with the least |D - O|^2 (first scanned wins a tie)
  int count = 0;          // recorded players
};

// 0x140A62690: the side's keeper, the first active player with position category 0; kNoSlot if none.
int KeeperSlot(int side, const std::array<LanePlayer, kSlots>& players);

// 0x14043C410: away side, 11 <= slot <= 21.
bool IsAwaySlot(int slot);

// 0x140470D60: wrap360(wrap360(heading) + add).
float HeadingAdd(const LaneConstants& k, float heading_deg, float add_deg);

// 0x14063CD90. `reference` is excluded from the scan. `side` is 0 or 1; the PES guard q < 22 is
// unreachable for those. `keepKeeper` false excludes the side's keeper. `horizonFrames` is h.
LaneScan ScanLane(const LaneConstants& k, const std::array<LanePlayer, kSlots>& players, int reference,
                  int side, const LaneRequest& lane, bool keepKeeper, int horizonFrames);

// 0x14063DC10 with 0x14063DE20: scan the side opposite `reference`; the lane is free when nobody
// is recorded.
bool LaneFree(const LaneConstants& k, const std::array<LanePlayer, kSlots>& players, int reference,
              const LaneRequest& lane, bool keepKeeper, int horizonFrames);

}  // namespace pes::lane
