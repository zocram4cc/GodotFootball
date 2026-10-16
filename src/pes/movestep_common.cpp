// The MOVESTEP helpers the dispatcher port and the handler groups share. Each body names its PES
// site; constants come from the install and the install test checks their relations, while the
// synthetic tests pass their own.
#include "pes/movestep.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

// FUN_14043a0a0(a, b): the angle difference a - b wrapped into (-180, 180].
float AngleDiffDeg(float a, float b, const RunConstants& k) {
  float d = a - b;
  if (d > 0.0f) {
    if (d >= k.full) d += k.wrapFull;
  } else {
    d += k.full;
  }
  if (k.half_turn < d) {
    const float sign = d < 0.0f ? k.neg : k.one;
    d -= sign * k.full;
  }
  return d;
}

// The 360 fold of a heading into [0, 360).
float FoldFull(float a, const RunConstants& k) {
  if (a >= 0.0f) {
    if (k.full <= a) return a + k.wrapFull;
    return a;
  }
  return a + k.full;
}

namespace {
// FUN_1406b9e80's table: ten records at 0x1425de010, stored low speed first; PES walks them high
// speed first, so the loader reverses the record order. The kind scale is 0x1425ddf5c.
constexpr uint64_t SPEED_RATE_TABLE_VA = 0x1425de010;
constexpr uint64_t SPEED_RATE_KIND_SCALE_VA = 0x1425ddf5c;

// Group A's install scalars, by VA (StayDecision FUN_1406b6bb0, IdleRow FUN_1406b93a0 and the
// idle tails).
struct MoveScalar {
  uint64_t va;
  float MoveConstants::* field;
};
constexpr MoveScalar MOVE_SCALARS[] = {
    {0x14259bde4, &MoveConstants::dist2m5},
    {0x14258c264, &MoveConstants::dist3},
    {0x142588210, &MoveConstants::near01},
    {0x14259bdf0, &MoveConstants::ball1p4},
    {0x1425c5100, &MoveConstants::lookScale},
    {0x1425ddff0, &MoveConstants::lookLow},
    {0x1425ddf68, &MoveConstants::lookHigh},
    {0x1425ddfe4, &MoveConstants::rootLow},
    {0x1425ddf58, &MoveConstants::rootHigh},
    {0x142591140, &MoveConstants::demoSpan},
};

// The idle row tables, by VA, each four row ids (FUN_1406b93a0's exits).
struct IdleRowSite {
  uint64_t va;
  std::array<int32_t, 4> IdleRowTables::* row;
};
constexpr IdleRowSite IDLE_ROW_SITES[] = {
    {0x1425ddc58, &IdleRowTables::fatigue},
    {0x1425ddc48, &IdleRowTables::flag},
    {0x1425ddbf0, &IdleRowTables::kind2},
    {0x1425ddc10, &IdleRowTables::mode},
    {0x1425ddc00, &IdleRowTables::mode2},
    {0x1425ddcc8, &IdleRowTables::slot},
    {0x1425ddcd0, &IdleRowTables::slotAlt},
    {0x1425ddc40, &IdleRowTables::kind6},
    {0x1425ddbe0, &IdleRowTables::near},
    {0x1425ddbd0, &IdleRowTables::far},
};
}  // namespace

std::optional<MoveConstants> LoadMoveConstants(const PeImage& exe, std::string* error) {
  MoveConstants k{};
  for (const MoveScalar& s : MOVE_SCALARS) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*(s.field))))) {
      *error = "move constants: .rdata unreadable";
      return std::nullopt;
    }
  }
  return k;
}

std::optional<IdleRowTables> LoadIdleRowTables(const PeImage& exe, std::string* error) {
  IdleRowTables t{};
  for (const IdleRowSite& s : IDLE_ROW_SITES) {
    if (!exe.ReadVa(s.va, sizeof(std::array<int32_t, 4>),
                    reinterpret_cast<uint8_t*>(&(t.*(s.row))))) {
      *error = "idle row tables: .rdata unreadable";
      return std::nullopt;
    }
  }
  return t;
}

std::optional<SpeedRateTable> LoadSpeedRateTable(const PeImage& exe, std::string* error) {
  std::array<std::array<float, 4>, 10> stored{};
  if (!exe.ReadVa(SPEED_RATE_TABLE_VA, sizeof stored,
                  reinterpret_cast<uint8_t*>(stored.data()))) {
    *error = "speed->rate table: .rdata unreadable";
    return std::nullopt;
  }
  SpeedRateTable t;
  for (size_t i = 0; i < stored.size(); ++i) t.record[i] = stored[stored.size() - 1 - i];
  if (!exe.ReadVa(SPEED_RATE_KIND_SCALE_VA, sizeof(float),
                  reinterpret_cast<uint8_t*>(&t.kindScale))) {
    *error = "speed->rate table: kind scale unreadable";
    return std::nullopt;
  }
  return t;
}

namespace movestep {

// FUN_1406b9e80(ap, speed km/h, kind): the playback rate for a ground speed. The speed is a
// m/s value divided by the physique scale, then the first record from the top whose vLo holds
// it gives the piecewise-linear rate.
float SpeedToRate(MoveHost& host, float speedKmh, int kind) {
  const RunConstants& k = host.RunConstantsView();
  const SpeedRateTable& t = host.SpeedRateTableView();
  const float v = (speedKmh / k.speed3600) / host.PhysiqueG();
  for (size_t i = 0; i < t.record.size(); ++i) {
    const float vHi = t.record[i][0];
    const float vLo = t.record[i][1];
    if (vLo <= v) {
      float share = (v - vLo) / (vHi - vLo);
      if (k.one < share) share = k.one;
      float rate = 0.0f;
      if (i + 1 == t.record.size() && (kind == 3 || kind == 6)) {
        rate = share * t.kindScale;
      } else {
        const float rHi = t.record[i][2];
        const float rLo = t.record[i][3];
        rate = rLo + share * (rHi - rLo);
      }
      if (k.one < rate) return k.one;
      return 0.0f <= rate ? rate : 0.0f;
    }
  }
  return 0.0f;
}

}  // namespace movestep
}  // namespace pes
