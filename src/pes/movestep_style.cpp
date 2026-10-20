// FUN_1406ba2f0, the locomotion style ladder: which parametric group a request plays. It lives
// with the dispatcher seams, not in movestep_common.cpp, so the group test targets (which stand
// their own helpers in) do not pull the shared helper object into their links.
#include <cmath>
#include <cstdint>

#include "pes/movestep.hpp"

namespace pes {
namespace movestep {
namespace {
// FUN_1406ba2f0's values, named by its decompile. Each arm returns one of these.
constexpr int kReplayAnime = 0x4a;   // the anime state vt+0x28 == 0x4a: the replay action
constexpr int kOpenPlay = 1;         // MI+0x116c == 1
constexpr int kPitchSlots = 0x16;    // the stamina gate reads the players on the pitch
constexpr int kTiredPercent = 0x14;  // stamina below 20 %
constexpr int kDribbleCommand = 5;   // req[0x16] == 5: the dribble command
constexpr int kLastGatedCommand = 0x3e;
// Bit i set: the command i is one of 2, 5, 0x33, 0x34, 0x3e (FUN_1406ba2f0's mask).
constexpr uint64_t kGatedCommands = 0x4018000000000024ull;
constexpr int kStyleNotOpenPlay = 0xf;
constexpr int kStyleTired = 0xd;
constexpr int kStyleAccelerate = 0xa;
constexpr int kStyleDribble = 8;
constexpr int kStyleOtherwise = 0;

// The dribble arm: the ball is in front of the player, within 3 m, below half his height, and it
// is not a fast ball running along the facing. true = style 8.
bool DribbleArm(MoveHost& host, MoveReq& req) {
  const StyleConstants& s = host.StyleConstantsView();
  const RunConstants& r = host.RunConstantsView();
  if (req.U(moveword::kCommandId) != kDribbleCommand || !req.Flag(moveword::kUseClassRate) ||
      host.MatchPhase() != kOpenPlay) {
    return false;
  }
  // The ball's position 20/60 s ahead: the ball list index is int(54 * 20 / 60 + 0.5).
  const int index = static_cast<int>(host.TicksPerSecond() * s.sampleTicks / r.kmh60 + r.half);
  float ball[3] = {0.0f, 0.0f, 0.0f};
  host.BallSample(index, ball);
  float self[3] = {0.0f, 0.0f, 0.0f};
  host.OwnPosition(self);
  const float dx = ball[0] - self[0];
  const float dz = ball[2] - self[2];
  const float facing = host.FacingDeg();
  // FUN_140439e50 (the bearing to the ball) and FUN_1404583b0 (against the facing).
  const float bearing = FoldFull(std::atan2(dx, dz) * s.radToDeg, r);
  if (!(std::fabs(PesAngleDiff(facing, bearing, r)) < r.deg45)) return false;
  if (!(std::sqrt(dx * dx + dz * dz) < host.MoveConstantsView().dist3)) return false;
  if (!(ball[1] < (host.PlayerHeightCm() / s.heightDiv) * r.half)) return false;  // PES's height is index 1
  // FUN_140a57650 (the ball's displacement over one tick) through FUN_14042d470 (km/h).
  float first[3] = {0.0f, 0.0f, 0.0f};
  float second[3] = {0.0f, 0.0f, 0.0f};
  host.BallSample(1, first);
  host.BallSample(2, second);
  const float perTick = std::sqrt((second[2] - first[2]) * (second[2] - first[2]) +
                                  (second[0] - first[0]) * (second[0] - first[0]));
  const float speedKmh = perTick * host.TicksPerSecond() * r.kmh60 * r.kmh60 / s.kmhScale;
  if (s.speed16 <= speedKmh && std::fabs(PesAngleDiff(facing, host.BallMotionHeadingDeg(), r)) <= r.deg45) {
    return false;
  }
  return true;
}
}  // namespace

int LocomotionStyle(MoveHost& host, int animeState, float rateRequested, MoveReq& req, float rateMax,
                    bool rateHeld) {
  const RunConstants& r = host.RunConstantsView();
  const StyleConstants& s = host.StyleConstantsView();
  // The scripted-slot gate: outside open play, and in the replay action, only the slot's owner
  // keeps the ladder.
  if (animeState == kReplayAnime || host.MatchPhase() != kOpenPlay) {
    if (!host.ScriptedSlotIsOurs()) return kStyleNotOpenPlay;
  }
  // The stamina gate reads the player on the pitch.
  if (host.ActionIndex() < kPitchSlots && host.StaminaPercent() < kTiredPercent) return kStyleTired;
  if (animeState == kReplayAnime) return kStyleOtherwise;

  const float rate = req.F(moveword::kRate);
  const uint32_t command = req.U(moveword::kCommandId);
  // T1: the request is no more than 0.15 above the current rate, and not a speed-class-3 walk
  // with the class rate in use.
  const bool t1 = rateRequested <= rate + r.rampSlope &&
                  (req.U(moveword::kCommandSpeedClass) != 3 || !req.Flag(moveword::kUseClassRate));
  // T2: the rate is near the ceiling, the rate is held, or the command is not gated.
  const bool t2 = rateMax * s.rate0p895 <= rate || rateHeld || command > kLastGatedCommand ||
                  ((kGatedCommands >> (command & 0x3f)) & 1) == 0;
  // T3: the rate is above the dribble arm's gate.
  const bool t3 = rateMax * s.rate0p825 <= rate;
  if (!(t1 || t2 || t3)) return kStyleAccelerate;
  if (DribbleArm(host, req)) return kStyleDribble;
  return (t1 || t2) ? kStyleOtherwise : kStyleAccelerate;
}

}  // namespace movestep
}  // namespace pes
