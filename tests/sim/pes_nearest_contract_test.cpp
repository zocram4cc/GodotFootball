// PES's per-player nearest list, byte for byte (docs/re/pes-re-teamtick.md section 3.1, CONFIRMED live on 51 + 35 samples): owner i's
// block at MA + 0x22E4C + 200 i holds, per team, up to 11 {u32 slot id, f32 d^2} entries ascending by d^2 from the owner, empty
// entries as id 0xFF with d^2 0.0; the counts at +0xB0 / +0xB1; and at +0xB2 + id the rank of id in its own team's list, 10 when id
// is not listed (the owner himself and off-pitch men). The test rebuilds the expected block from the men's positions and reads
// the window bytes GF's writer leaves.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "onthepitch/pesscorerows.hpp"
#include "onthepitch/pesworldrecord.hpp"

namespace {

constexpr float kXg = 52.5f;  // goal-centre x (the writer's goal marks; the nearest list does not read them)
constexpr float kYg = 34.0f;
constexpr float kDistTol_m2 = 1e-3f;
constexpr std::size_t kEntryIdBytes = 4;
constexpr std::size_t kEntryBytes = 8;  // {u32 id, f32 d^2}

pes::ScoreRowConstants Goal() {
  pes::ScoreRowConstants k;
  k.goalX_m = kXg;
  k.goalY_m = kYg;
  return k;
}

pes::PesWorld& World() {
  static pes::PesWorld world;
  return world;
}

// Puts a man on or off the pitch at (x, z): his record holds the position the writer reads.
void Place(pes::RowPlayers& p, int slot, float x, float z, bool onPitch) {
  World().BindSlot(slot);
  uint8_t* man = World().ManRecord(slot);
  pes::StoreF32(man, pes::kManPosXOffset, x);
  pes::StoreF32(man, pes::kManPosZOffset, z);
  p[slot] = pes::RowPlayer{x, z, onPitch, man};
}

// The 22-man scene: distinct distances from every owner, two men off the pitch (one per side).
constexpr int kOffPitchA = 3;
constexpr int kOffPitchB = 14;
pes::RowPlayers Scene() {
  pes::RowPlayers p{};
  for (int s = 0; s < pes::lane::kSlots; ++s) {
    const float x = -45.0f + 4.1f * static_cast<float>(s);
    const float z = static_cast<float>((s * 7) % 13) * 1.9f - 11.4f;
    Place(p, s, x, z, s != kOffPitchA && s != kOffPitchB);
  }
  return p;
}

struct Expected {
  int id;
  float d2;
};

// The PES list of owner i for team t: the on-pitch men of team t but i, ascending by d^2 from i.
std::vector<Expected> ListOf(const pes::RowPlayers& p, int owner, int team) {
  std::vector<Expected> out;
  for (int s = team * pes::lane::kSideSize; s < (team + 1) * pes::lane::kSideSize; ++s) {
    if (s == owner || !p[s].onPitch) continue;
    const float dx = p[s].x - p[owner].x;
    const float dz = p[s].z - p[owner].z;
    out.push_back({s, dx * dx + dz * dz});
  }
  std::stable_sort(out.begin(), out.end(), [](const Expected& a, const Expected& b) { return a.d2 < b.d2; });
  return out;
}

uint32_t IdAt(const uint8_t* block, int team, int k) {
  uint32_t id;
  std::memcpy(&id, block + pes::kRowTeamStride * static_cast<std::size_t>(team) + kEntryBytes * static_cast<std::size_t>(k),
              kEntryIdBytes);
  return id;
}

float D2At(const uint8_t* block, int team, int k) {
  float d2;
  std::memcpy(&d2, block + pes::kRowTeamStride * static_cast<std::size_t>(team) + kEntryBytes * static_cast<std::size_t>(k) + kEntryIdBytes,
              sizeof d2);
  return d2;
}

class NearestListContract : public ::testing::Test {
 protected:
  void SetUp() override {
    p_ = Scene();
    writer_.Bind(World().Ma(0));
    writer_.Tick(p_, Goal(), /*frame=*/0, /*fullRefresh=*/true, /*pesPath=*/true, World());
  }
  pes::RowPlayers p_{};
  pes::ScoreRowWriter writer_;
};

// The k-th entry of each team's list is PES's k-th nearest, with its d^2; the counts give the list length.
TEST_F(NearestListContract, EachOnPitchOwnersTeamListIsSortedByDistanceFromHim) {
  const uint8_t* ma = World().Ma(0);
  for (int owner = 0; owner < pes::lane::kSlots; ++owner) {
    if (!p_[owner].onPitch) continue;
    const uint8_t* block = ma + pes::kMaNearestOffset + pes::kRowBytes * static_cast<std::size_t>(owner);
    for (int team = 0; team < 2; ++team) {
      const std::vector<Expected> want = ListOf(p_, owner, team);
      ASSERT_EQ(block[pes::kRowCountOffset + team], want.size()) << "owner " << owner << " team " << team;
      for (std::size_t k = 0; k < want.size(); ++k) {
        EXPECT_EQ(IdAt(block, team, static_cast<int>(k)), static_cast<uint32_t>(want[k].id)) << "owner " << owner << " rank " << k;
        EXPECT_NEAR(D2At(block, team, static_cast<int>(k)), want[k].d2, kDistTol_m2) << "owner " << owner << " rank " << k;
      }
    }
  }
}

// An id's rank byte is its place in its own team's list, and PES's absent rank (10) for every id that is not listed.
TEST_F(NearestListContract, RankByteIsThePlaceInTheOwnTeamListOrTenWhenAbsent) {
  const uint8_t* ma = World().Ma(0);
  for (int owner = 0; owner < pes::lane::kSlots; ++owner) {
    if (!p_[owner].onPitch) continue;
    const uint8_t* block = ma + pes::kMaNearestOffset + pes::kRowBytes * static_cast<std::size_t>(owner);
    std::vector<int> rankOf(pes::lane::kSlots, -1);
    for (int team = 0; team < 2; ++team) {
      const std::vector<Expected> want = ListOf(p_, owner, team);
      for (std::size_t k = 0; k < want.size(); ++k) rankOf[want[k].id] = static_cast<int>(k);
    }
    for (int id = 0; id < pes::lane::kSlots; ++id) {
      const int want = rankOf[id] >= 0 ? rankOf[id] : pes::kRowAbsentRank;
      EXPECT_EQ(block[pes::kRowRankOffset + id], want) << "owner " << owner << " id " << id;
    }
    EXPECT_EQ(block[pes::kRowRankOffset + owner], pes::kRowAbsentRank) << "owner " << owner << " is not in his own list";
  }
}

// PES's empty entries past a team's count are id 0xFF with d^2 0.0 (the fill the consumers read there), not NaN or zero ids.
TEST_F(NearestListContract, EmptyEntriesAreTheIdFillWithZeroDistance) {
  const uint8_t* ma = World().Ma(0);
  for (int owner = 0; owner < pes::lane::kSlots; ++owner) {
    if (!p_[owner].onPitch) continue;
    const uint8_t* block = ma + pes::kMaNearestOffset + pes::kRowBytes * static_cast<std::size_t>(owner);
    for (int team = 0; team < 2; ++team) {
      const std::size_t count = ListOf(p_, owner, team).size();
      for (std::size_t k = count; k < pes::lane::kSideSize; ++k) {
        EXPECT_EQ(IdAt(block, team, static_cast<int>(k)), static_cast<uint32_t>(pes::lane::kNoSlot))
            << "owner " << owner << " team " << team << " empty rank " << k;
        EXPECT_EQ(D2At(block, team, static_cast<int>(k)), 0.0f) << "owner " << owner << " team " << team << " empty rank " << k;
      }
    }
  }
}

}  // namespace
