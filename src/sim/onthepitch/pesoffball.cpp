#include "onthepitch/pesoffball.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "gametypes.hpp"
#include "pes/uniform.hpp"
#include "onthepitch/pesroles.hpp"
#include "onthepitch/pescarrier.hpp"

namespace pes::offball {
namespace {

// --- literals the reports give without an .rdata VA (provenance per line) ---
// Minimum run time before the Chance stop may fire: int(fps·0.2+0.5) (0x140607a60).
constexpr float kChanceMinFrames = 11.0f;
// The common test's late line margin switches at int(0.8·fps+0.5) (0x140688c00 step 14).
constexpr float kLateFrames = 43.0f;
constexpr float kLineMarginEarly_m = 1.5f;  // step 14
constexpr float kLineMarginLate_m = 0.7f;   // step 14
constexpr float kTargetReached_m = 1.0f;     // head: |pos - AI target| < 1 m
constexpr float kGoalLineMargin_m = 2.5f;    // head: s·x > F4 + 2.5
constexpr float kFarBase_m = 40.0f;          // step 8: d² > (k + 40)²
constexpr float kFarBehind_m = 8.0f;         // step 8: 8 m behind the ball (13 for Counter)
constexpr float kFarBehindCounter_m = 13.0f;
constexpr float kNearDx_m = 4.0f;            // step 6: |dx| <= 4 -> k = 10
constexpr float kNearDxFar_m = 10.0f;        // step 6: ramp to 0 at 10 m
constexpr float kNearK_m = 10.0f;
constexpr float kWideDz_m = 43.0f;           // step 13: |P.z - ball.z| > 43
// Lanes (FUN_140a71990): z_k = F1 - 2 - 2k, k = 0..32.
constexpr int kLaneCount = 33;
constexpr float kLaneSpacing_m = 2.0f;
// Blocked-lane weight w: 1 within 55 deg of T.dir, linear to 0 at 88 (59ba60 pre-pass).
constexpr float kBlockFullAngle_deg = 55.0f;
constexpr float kBlockZeroAngle_deg = 88.0f;
// The S table's "empty" value (0xff counted as 100, FUN_140462470).
constexpr int kLaneEmpty = 100;
constexpr float kLaneDiscRadius_m = 1.0f;  // FUN_140a77b80 disc r <= 1
// Moving-runner heading gate: speed > 3000/(fps·3600) m/frame, dev > 130 deg skips.
constexpr float kMovingSpeed_mps = 3000.0f / 3600.0f;
constexpr float kMovingHeadingMax_deg = 130.0f;
// Receiver-side bonus (0x14059cd42): d > 2, ramp over 3 m, +50, +100.
constexpr float kRecvGap_m = 2.0f;
constexpr float kRecvRamp_m = 3.0f;
constexpr int kRecvRampScore = 50;
constexpr int kRecvBonus = 100;
// Neighbour-lane budget FUN_140592fd0: need limit >= 15; reject a neighbour
// under limit - 15 and under 85.
constexpr int kBudgetMinLimit = 15;
constexpr int kBudgetMargin = 15;
constexpr int kBudgetNeighbourMax = 85;
// Cone penalties a4e720(id, bearing, 20/40 deg, 12 m) -> -50/-25.
constexpr float kConeNarrow_deg = 20.0f, kConeWide_deg = 40.0f, kConeRadius_m = 12.0f;
constexpr int kConeNarrowPen = -50, kConeWidePen = -25;
// Three nearest opponents within 15 m (225 m²) on the lane: |dz| <= 3, dev <= 90, perp² < 1.
constexpr int kLaneOppCount = 3;
constexpr float kLaneOppRangeSq_m2 = 225.0f;
constexpr float kLaneOppDz_m = 3.0f;
constexpr float kLaneOppDev_deg = 90.0f;
constexpr float kLaneOppPerpSq_m2 = 1.0f;
constexpr float kLaneOppFull_m = 1.0f, kLaneOppZero_m = 2.5f;
constexpr int kLaneOppPen = -50;
// Wide penalty: |z_k| > 14 -> -50·ramp(s·ball.x, 10 -> F6)·ramp(|ball.z - z_k|, 15 -> 30).
constexpr float kWideLane_m = 14.0f;
constexpr float kWideBallFrom_m = 10.0f;
constexpr float kWideDzFrom_m = 15.0f, kWideDzTo_m = 30.0f;
constexpr float kWidePen = 50.0f;
// Commit FUN_140664ee0 first gates: |fold(bearing(ball->C) - T.dir)| <= 90, |ball.z - C.z| <= 48.
constexpr float kCommitDev_deg = 90.0f;
constexpr float kCommitDz_m = 48.0f;
// FUN_14066d720 (far lane in open play): outside the box zone (|ball.z| < F7
// and s·ball.x > F6 - 5), a runner more than 10 m ahead of the ball or a lane
// more than 20 m across rejects lanes over 27 m (729 m²) from the ball.
constexpr float kFarLaneBoxMargin_m = 5.0f;
constexpr float kFarLaneAhead_m = 10.0f;
constexpr float kFarLaneAcross_m = 20.0f;
constexpr float kFarLaneBallSq_m2 = 729.0f;
// Supporter (FUN_1405eb980): 0.7·angle + distance + 15 under 10 m.
constexpr float kSupporterAngleWeight = 0.7f;
constexpr float kSupporterClose_m = 10.0f;
constexpr float kSupporterClosePen = 15.0f;
// Support distance (FUN_1405e0c60, pes-re-tactics.md 4.1).
constexpr float kSupportBase_m = 5.0f;
constexpr float kSupportRangeGain_m = 3.0f;
constexpr float kSupportRangeSpan = 9.0f;  // (SR - 1)/9, 9 = 0x14259d60c
constexpr float kSupportDeep_m = 7.0f;
constexpr float kSupportLong_m = 4.0f;
constexpr float kSupportLineMargin_m = 10.0f;
constexpr float kSupportFwdFull_deg = 45.0f, kSupportFwdZero_deg = 90.0f;
constexpr float kSupportWide_m = 3.0f;
constexpr float kSupportWideBand_m = 5.0f;
constexpr float kSupportBoxBand_m = 10.0f;
constexpr float kSupportFloor_m = 6.0f;
constexpr float kSupportDead_m = 8.0f;
constexpr float kFieldHalfWidthHalf = 0.5f;  // -0.5·F1

float Fold180(float a) {
  a = std::fmod(a, 360.0f);
  if (a > 180.0f) a -= 360.0f;
  if (a < -180.0f) a += 360.0f;
  return a;
}

float Bearing(Vec2f from, Vec2f to) {
  return std::atan2(to.z - from.z, to.x - from.x) * (180.0f / 3.14159265f);
}

float Dist2(Vec2f a, Vec2f b) { return (a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z); }

// ramp(v; a -> 0, b -> 1), either direction.
float Ramp(float v, float a, float b) {
  if (a == b) return v >= b ? 1.0f : 0.0f;
  return std::clamp((v - a) / (b - a), 0.0f, 1.0f);
}

float LaneZ(int k) { return pesFieldHalfWidth - kLaneSpacing_m - kLaneSpacing_m * k; }

bool ReadF(const PeImage& exe, uint64_t va, float* out, bool* ok) {
  *ok &= exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out));
  return *ok;
}

// The S table cell (FUN_140a77130 + FUN_140462470): the 1 m disc around
// (x, z) on team t's occupancy grid, mean of the cells, 100 when empty.
int LaneFreedom(const OffBallConstants& k, const std::vector<Vec2f>& players, float x, float z) {
  const float cw = 2.0f * pesFieldHalfLength / k.gridCellsX;
  const float ch = 2.0f * pesFieldHalfWidth / k.gridCellsZ;
  const int nx = static_cast<int>(k.gridCellsX), nz = static_cast<int>(k.gridCellsZ);
  const float cx = std::clamp(x, -pesFieldHalfLength, pesFieldHalfLength);
  int sum = 0, count = 0;
  const int i0 = std::max(0, static_cast<int>((cx + pesFieldHalfLength) / cw) - 2);
  const int j0 = std::max(0, static_cast<int>((z + pesFieldHalfWidth) / ch) - 2);
  for (int i = i0; i < std::min(nx, i0 + 5); ++i) {
    for (int j = j0; j < std::min(nz, j0 + 5); ++j) {
      const Vec2f c{-pesFieldHalfLength + (i + 0.5f) * cw, -pesFieldHalfWidth + (j + 0.5f) * ch};
      if (Dist2(c, {cx, z}) > kLaneDiscRadius_m * kLaneDiscRadius_m) continue;
      int v = kLaneEmpty;
      for (const Vec2f& p : players) {
        const float d2 = Dist2(c, p);
        if (d2 >= k.gridRadiusSq_m2 || v <= 0) continue;
        v = std::max(0, static_cast<int>(v - (k.gridFalloff_m - std::sqrt(d2)) * kLaneEmpty *
                                                 k.gridScale));
      }
      sum += v;
      ++count;
    }
  }
  return count ? sum / count : kLaneEmpty;
}

// a4e720 / a4e3f0: players inside the cone (|dev| <= width/2) within radius.
int InCone(Vec2f from, float bearing_deg, float width_deg, float radius_m,
           const std::vector<Vec2f>& players) {
  int n = 0;
  for (const Vec2f& p : players) {
    if (Dist2(from, p) >= radius_m * radius_m) continue;
    if (std::fabs(Fold180(Bearing(from, p) - bearing_deg)) <= width_deg * 0.5f) ++n;
  }
  return n;
}

}  // namespace

std::optional<OffBallConstants> LoadOffBallConstants(const PeImage& exe, const PesVfs& vfs,
                                                     std::string* error) {
  OffBallConstants k{};
  bool ok = true;
  ReadF(exe, 0x14253ee30, &k.chanceScoreBase, &ok);
  ReadF(exe, 0x142598578, &k.laneAhead_m, &ok);
  ReadF(exe, 0x14257a190, &k.blockConeFlat, &ok);
  ReadF(exe, 0x14259b89c, &k.blockConeSlope, &ok);
  ReadF(exe, 0x14259bdf0, &k.blockRadius_m, &ok);
  ReadF(exe, 0x14259be04, &k.reachDhFrom_m, &ok);
  ReadF(exe, 0x142591140, &k.reachDhTo_m, &ok);
  ReadF(exe, 0x1425c7e70, &k.reachDhDiv_m, &ok);
  ReadF(exe, 0x14259be0c, &k.reachBase_m, &ok);
  ReadF(exe, 0x14259222c, &k.reachDhGain_m, &ok);
  ReadF(exe, 0x1425cd82c, &k.reachCap_m, &ok);
  ReadF(exe, 0x1425aa1bc, &k.awareFrom, &ok);
  ReadF(exe, 0x1425c594c, &k.awareTo, &ok);
  ReadF(exe, 0x14259be1c, &k.awareSpan, &ok);
  ReadF(exe, 0x14259be18, &k.awareBonus_m, &ok);
  ReadF(exe, 0x1425cb308, &k.gridCellsX, &ok);
  ReadF(exe, 0x1425cf878, &k.gridCellsZ, &ok);
  ReadF(exe, 0x14259be10, &k.gridRadiusSq_m2, &ok);
  ReadF(exe, 0x1425656c0, &k.gridFalloff_m, &ok);
  ReadF(exe, 0x142592228, &k.gridScale, &ok);
  ReadF(exe, 0x142588210, &k.fallbackSide_m, &ok);
  ReadF(exe, 0x1425c4c18, &k.predNearSq_m2, &ok);
  ReadF(exe, 0x1425cb48c, &k.predMidSq_m2, &ok);
  ReadF(exe, 0x1425cb46c, &k.predNear_s, &ok);
  ReadF(exe, 0x14259b89c, &k.predMid_s, &ok);
  ReadF(exe, 0x14257a190, &k.predFar_s, &ok);
  ReadF(exe, 0x142588200, &k.predLead_s, &ok);
  ReadF(exe, 0x142590d98, &k.predOffBall_s, &ok);
  ReadF(exe, 0x14259222c, &k.styleRunDepth_m, &ok);
  uint8_t window = 0;
  ok &= exe.ReadVa(0x143449d88, 1, &window);
  k.laneWindow = window;
  if (!ok) {
    *error = "off-ball: PES2021.exe constants unmapped";
    return std::nullopt;
  }

  auto blob = vfs.Read(kConstantTeamPath, error);
  if (!blob) return std::nullopt;
  auto table = ParseUniformTable(*blob, error);
  if (!table) return std::nullopt;
  auto vision = table->Find("selectorVision.o");
  auto space = table->Find("spaceRun.o");
  if (!vision || !space) {
    *error = std::string("selectorVision.o / spaceRun.o missing from ") + kConstantTeamPath;
    return std::nullopt;
  }
  // selectorVision.o: six u32 offsets (sorted keys) to 5 × i32 lists
  // (pes-re-constant-team.md 2.3).
  for (int r = 0; r < kCapRunCount; ++r) {
    uint32_t at = 0;
    if (vision->size() < 4u * (r + 1)) return *error = "selectorVision.o truncated", std::nullopt;
    std::memcpy(&at, vision->data() + 4 * r, 4);
    if (at + 4u * kAttackLevels > vision->size())
      return *error = "selectorVision.o list past end", std::nullopt;
    for (int l = 0; l < kAttackLevels; ++l) {
      int32_t v = 0;
      std::memcpy(&v, vision->data() + at + 4 * l, 4);
      k.offerNum[r][l] = static_cast<uint8_t>(v);
    }
  }
  // spaceRun.o chanceScore: record +0x1c = blob +0x14; the builder passes its
  // low byte (pes-re-run-targets.md 0).
  constexpr uint32_t kChanceScoreAt = 0x14;
  if (space->size() < kChanceScoreAt + 4) return *error = "spaceRun.o truncated", std::nullopt;
  k.chanceScoreLimit = (*space)[kChanceScoreAt];
  const auto compat = pes::style::LoadCompat(exe, error);
  if (!compat) return std::nullopt;
  k.styleCompat = *compat;
  return k;
}

int ActionRank(int state) {
  switch (state) {
    case 0x23: case 0x24: case 0x25: case 0x2c: case 0x2d: case 0x2e: case 0x3a: case 0x3b:
      return 0;
    case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21: case 0x27: case 0x28: case 0x2a:
    case 0x2b: case 0x33: case 0x34: case 0x35: case 0x3c:
      return 2;
    case 0x10: case 0x26: case 0x2f: case 0x36: case 0x37: case 0x69: case 0x6a: case 0x6b:
      return 3;
    case 0x22: case 0x32: case 0x3d: case 0x6d:
      return 4;
    default:
      return 5;
  }
}

bool MayTake(int run, int current) { return ActionRank(run) < ActionRank(current); }

int RunCap(const OffBallConstants& k, CapRun run, int attackLevel, bool tikiTaka) {
  if (tikiTaka) {
    if (run == kCapSecondLine || run == kCapPullAway) return 0;
    if (run == kCapChance) return 1;
  }
  return k.offerNum[run][std::clamp(attackLevel, 0, kAttackLevels - 1)];
}

std::vector<int> SelectRunners(const std::array<float, kSlots>& scores, int running, int cap,
                               int perTickCap, bool chance) {
  std::vector<int> taken;
  if (!(running < cap || chance)) return taken;
  std::array<int, kSlots> order;
  for (int i = 0; i < kSlots; ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return scores[a] > scores[b]; });
  constexpr int kMaxTakes = 10;  // the walk's own bound
  for (int s : order) {
    if (scores[s] <= 0.0f) break;
    taken.push_back(s);
    ++running;
    if (running >= cap || static_cast<int>(taken.size()) >= perTickCap ||
        static_cast<int>(taken.size()) >= kMaxTakes)
      break;
  }
  return taken;
}

float ChanceScore(const OffBallConstants& k, float s, float runnerX) {
  return k.chanceScoreBase + s * runnerX;
}


int DefenderPredictFrames(const OffBallConstants& k, float runnerBallDist2_m2) {
  // int(fps·x + 0.5) each, as the decompile truncates each term separately.
  auto frames = [](float s) { return static_cast<int>(kPesFps * s + 0.5f); };
  const float a = runnerBallDist2_m2 < k.predNearSq_m2  ? k.predNear_s
                  : runnerBallDist2_m2 < k.predMidSq_m2 ? k.predMid_s
                                                        : k.predFar_s;
  // ponytail: the branches for a controlled, action-4/5/7 or ball-carrying
  // defender and the pass-target tail are not reached by GF, whose defenders
  // carry no PES action state; the plain off-ball lead applies.
  return static_cast<uint8_t>(frames(a) + frames(k.predLead_s)) + frames(k.predOffBall_s);
}

std::optional<Vec2f> ChanceLaneTarget(const OffBallConstants& k, const LaneSearchInput& in) {
  const float s = in.s;
  // X: the deepest of the ball and the two last defenders; never behind the runner.
  float X = in.ball.x;
  for (const Vec2f& d : in.deepest)
    if (s * d.x > s * X) X = d.x;
  if (s * X <= s * in.runnerPred.x) X = in.runnerPred.x + k.laneAhead_m * s;

  // The runner's lane and the window around it.
  int k0 = 0;
  for (int i = 1; i < kLaneCount; ++i)
    if (std::fabs(in.runner.z - LaneZ(i)) < std::fabs(in.runner.z - LaneZ(k0))) k0 = i;
  const int lo = std::max(0, k0 - k.laneWindow), hi = std::min(kLaneCount - 1, k0 + k.laneWindow);

  // S row of the defending team at its own line (ball when the ball is past it).
  float lineX = in.defLineX;
  if (s * in.ball.x > s * lineX) lineX = in.ball.x;  // INF(med): attack-frame reading of the swap
  std::array<int, kLaneCount> S{};
  for (int i = 0; i < kLaneCount; ++i) S[i] = LaneFreedom(k, in.opponents, lineX, LaneZ(i));

  // Pre-pass: lanes an opponent sits across.
  // ponytail: the pre-pass gate flags (5aa480/5aa6b0/5a8df0/danger) are all
  // treated false, so the pass always runs; the marker exemption is dropped.
  std::array<bool, kLaneCount> blocked{};
  for (int i = lo; i <= hi; ++i) {
    const Vec2f C{X, LaneZ(i)};
    const float d = std::sqrt(Dist2(in.runner, C));
    const float a = std::fabs(Fold180(Bearing(in.runner, C) - in.teamDir_deg));
    const float w = a <= kBlockFullAngle_deg ? 1.0f
                    : a < kBlockZeroAngle_deg
                        ? (kBlockZeroAngle_deg - a) / (kBlockZeroAngle_deg - kBlockFullAngle_deg)
                        : 0.0f;
    const float r = (k.blockConeFlat + k.blockConeSlope * w) * k.blockRadius_m;
    const float R = d >= r ? (d - r) * (d - r) : 0.0f;
    for (const Vec2f& o : in.opponents)
      if (Dist2(o, C) < R) blocked[i] = true;
  }

  // The three nearest opponents within 15 m, for the on-lane penalty.
  std::vector<Vec2f> near = in.opponents;
  std::sort(near.begin(), near.end(), [&](Vec2f a, Vec2f b) {
    return Dist2(a, in.runnerPred) < Dist2(b, in.runnerPred);
  });
  if (near.size() > static_cast<size_t>(kLaneOppCount)) near.resize(kLaneOppCount);

  const float speed = std::hypot(in.runnerVel.x, in.runnerVel.z);
  const float heading = std::atan2(in.runnerVel.z, in.runnerVel.x) * (180.0f / 3.14159265f);
  // ponytail: FUN_140664ee0's ~95 leaves; its two head gates and the far-lane
  // leaf FUN_14066d720 are ported (the leaf's holder flags, its allClear term
  // and its possession-time accept are taken as 0).
  const bool boxZone = std::fabs(in.ball.z) < pesPenaltyAreaHalfWidth &&
                       s * in.ball.x > pesPenaltyAreaLineX - kFarLaneBoxMargin_m;
  auto commits = [&](Vec2f C) {
    if (std::fabs(Fold180(Bearing(in.ball, C) - in.teamDir_deg)) > kCommitDev_deg ||
        std::fabs(in.ball.z - C.z) > kCommitDz_m)
      return false;
    return boxZone ||
           !((s * in.ball.x + kFarLaneAhead_m < s * in.runner.x ||
              std::fabs(in.runner.z - C.z) > kFarLaneAcross_m) &&
             Dist2(in.ball, C) > kFarLaneBallSq_m2);
  };
  std::optional<Vec2f> best;
  int bestS = 0;
  for (int i = lo; i <= hi; ++i) {
    if (blocked[i]) continue;
    const Vec2f C{X, LaneZ(i)};
    if (speed > kMovingSpeed_mps &&
        std::fabs(Fold180(Bearing(in.runnerPred, C) - heading)) > kMovingHeadingMax_deg)
      continue;
    int score = S[i];
    // ponytail: the S = 100 promotions (5a7840 ... 5a7270) and the region code
    // test (T+0xb824) are not ported; the table value stands.
    if (in.hasReceiver) {
      const float d = std::fabs(C.z - in.receiver.z);
      if (d > kRecvGap_m && std::fabs(C.z - in.ball.z) < std::fabs(in.receiver.z - in.ball.z) &&
          std::fabs(C.z) < pesGoalAreaHalfWidth)
        score += static_cast<int>(std::min(1.0f, (d - kRecvGap_m) / kRecvRamp_m) * kRecvRampScore) +
                 kRecvBonus;
    }
    // FUN_1405a59d0 (decompiled for this port) needs the ball holder's block
    // int +0xd8 > 0 (holder flag f1, off-ball O5: meaning undecoded, taken as
    // 0), so the alternative accept is false; FUN_140595cb0 returns true for a
    // slot not already in SPACE_RUN_CHANCE, which every candidate is.
    if (score < in.limit) continue;
    // FUN_140592fd0, budget 1 (594470's anyHit not ported).
    if (in.limit < kBudgetMinLimit) continue;
    bool budgetFail = false;
    for (int n : {i - 1, i + 1})
      if (n >= 0 && n < kLaneCount && S[n] < in.limit - kBudgetMargin && S[n] < kBudgetNeighbourMax)
        budgetFail = true;
    if (budgetFail) continue;

    const float b = Bearing(in.runner, C);
    int pen = 0;
    if (InCone(in.runner, b, kConeNarrow_deg, kConeRadius_m, in.opponents))
      pen = kConeNarrowPen;
    else if (InCone(in.runner, b, kConeWide_deg, kConeRadius_m, in.opponents))
      pen = kConeWidePen;
    const float lx = C.x - in.runnerPred.x, lz = C.z - in.runnerPred.z;
    const float len2 = lx * lx + lz * lz;
    for (const Vec2f& o : near) {
      if (Dist2(o, in.runnerPred) > kLaneOppRangeSq_m2) break;
      if (std::fabs(o.z - C.z) > kLaneOppDz_m) continue;
      if (std::fabs(Fold180(Bearing(in.runnerPred, o) - Bearing(in.runnerPred, C))) > kLaneOppDev_deg)
        continue;
      if (len2 <= 0.0f) continue;
      const float t = ((o.x - in.runnerPred.x) * lx + (o.z - in.runnerPred.z) * lz) / len2;
      const float px = in.runnerPred.x + t * lx - o.x, pz = in.runnerPred.z + t * lz - o.z;
      const float perp2 = px * px + pz * pz;
      if (t < 0.0f || perp2 >= kLaneOppPerpSq_m2) continue;
      const float d = std::sqrt(perp2);
      const float w = d <= kLaneOppFull_m ? 1.0f
                      : d < kLaneOppZero_m ? (kLaneOppZero_m - d) / (kLaneOppZero_m - kLaneOppFull_m)
                                           : 0.0f;
      pen = std::min(pen, static_cast<int>(kLaneOppPen * w));
    }
    score += pen;
    if (std::fabs(C.z) > kWideLane_m)
      score -= static_cast<int>(kWidePen * Ramp(s * in.ball.x, kWideBallFrom_m, pesPenaltyAreaLineX) *
                                Ramp(std::fabs(in.ball.z - C.z), kWideDzFrom_m, kWideDzTo_m));
    // ponytail: the sec/595ab0 test, the teammate-claim gates and the tie-break
    // are not ported (ties keep the first lane found).
    if (best && score <= bestS) continue;
    if (!commits(C)) continue;
    best = C;
    bestS = score;
  }
  // ponytail: the builder's undecoded fallback chain (603d00 ... 604490) is
  // replaced by its one decoded target, FUN_1406067c0: the line, 10 m to the
  // runner's side of the ball, through the same commit gates.
  if (!best) {
    const Vec2f C{in.defLineX,
                  in.ball.z + (in.runner.z >= in.ball.z ? 1.0f : -1.0f) * k.fallbackSide_m};
    if (commits(C)) best = C;
  }
  return best;
}

// --- the stop test (FUN_140688c00 and its leaves; run-targets 3, -s1..-s3) ---
//
// Every angle below is a PES bearing (atan2(dx, dz), degrees, [0, 360)), so
// the reports' literals apply unchanged. Inputs PES keeps that GF has no
// counterpart for take one fixed value, named here once:
//   - AI class a580a0 (`cls`) = 0, matching none of the class-gated branches;
//   - run sub-kind T+0x28c (`sub`) = 0 and "pinned" = false (the target
//     builder's special lanes are not ported);
//   - mark count a58320 = 0; player flag bit 21, T+0x2a3, T+0xb8a2 = 0;
//   - team concept slot T+0xb748 = 0x30 (no concept on a runner);
//   - no advanced instruction (a63950) is set (teaminstructions.hpp).
namespace {

constexpr float kRad = 57.2957795f;
// pred(p, n): n = int(fps·0.3+0.5) = 16 frames (0x14059bbe4).
constexpr float kRunPredictFrames = 16.0f;
constexpr int kNoId = -1;
// FUN_140593290's focus-player distance to the ball (0x1425ab4c4).
constexpr float k593290Focus_m2 = 625.0f;
// fps-derived frame counts int(fps·x + 0.5).
constexpr float Frames(float sec) { return static_cast<float>(static_cast<int>(kPesFps * sec + 0.5f)); }
// Speeds the reports give as 100/(fps·T) m/frame (T seconds per 100 m) or as
// FUN_1404c8dc0(k) (INF km/h); here in m/s.
constexpr float Per100(float sec) { return 100.0f / sec; }
constexpr float Kmh(float k) { return k / 3.6f; }

float Wrap360(float a) {
  a = std::fmod(a, 360.0f);
  return a < 0.0f ? a + 360.0f : a;
}
float PBrg(Vec2f a, Vec2f b) {
  if (a.x == b.x && a.z == b.z) return 0.0f;
  return Wrap360(std::atan2(b.x - a.x, b.z - a.z) * kRad);
}
float PVec(Vec2f v) { return (v.x == 0.0f && v.z == 0.0f) ? 0.0f : Wrap360(std::atan2(v.x, v.z) * kRad); }
float Ad(float a, float b) { return std::fabs(Fold180(a - b)); }
float Len(Vec2f v) { return std::sqrt(v.x * v.x + v.z * v.z); }
float Sgn(float v) { return v < 0.0f ? -1.0f : 1.0f; }
int TeamOf(int id) { return id >= kSlots ? 1 : 0; }
bool Valid(int id) { return id >= 0 && id < 2 * kSlots; }
// a4f210: the attack bearing, s == -1 -> 270, else 90.
float TDir(const RunWorld& w, int t) { return w.s[t] < 0.0f ? 270.0f : 90.0f; }
// a4c720(a, b, s, k): b is more than k ahead of a in the attack direction.
bool A720(float a, float b, float s, float k) { return (s * k + a) * s < s * b; }
// a4c9a0: past the opponents' line by k.
bool Beyond(const RunWorld& w, int id, float k) {
  const int t = TeamOf(id);
  return A720(w.lineX[t ^ 1], w.p[id].pos.x, w.s[t], k);
}
// 599d90: that line, advanced to the ball when there is no carrier.
float Line599(const RunWorld& w, int t) {
  float x = w.lineX[t ^ 1];
  if (w.carrier == kNoId && w.s[t] * w.ball.x > w.s[t] * x) x = w.ball.x;
  return x;
}
bool Beyond2(const RunWorld& w, int id, float k) {
  const int t = TeamOf(id);
  return A720(Line599(w, t), w.p[id].pos.x, w.s[t], k);
}
// 5a4830(id, 0, 1) -> 5a4790: the slot is in a run state (0x27 only with c,
// 0x2f only for AI class 6/7).
bool IsRun(int st) {
  switch (st) {
    case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21: case 0x24: case 0x25: case 0x2c:
      return true;
    default:
      return false;
  }
}
// a4e720 -> a4e3f0: opponents of `id` inside the cone (|dev| <= width/2,
// minDist < d < radius), positions `frames` ahead.
int ConeOpp(const RunWorld& w, int id, float center, float width, float radius,
            float frames = 0.0f, float minDist = 0.0f) {
  const PlayerFacts& me = w.p[id];
  const Vec2f m{me.pos.x + frames / kPesFps * me.vel.x, me.pos.z + frames / kPesFps * me.vel.z};
  const int o = (TeamOf(id) ^ 1) * kSlots;
  int n = 0;
  for (int i = o; i < o + kSlots; ++i) {
    const PlayerFacts& q = w.p[i];
    if (!q.active) continue;
    const Vec2f qp{q.pos.x + frames / kPesFps * q.vel.x, q.pos.z + frames / kPesFps * q.vel.z};
    const float d2 = Dist2(qp, m);
    if (minDist * minDist < d2 && d2 < radius * radius && Ad(center, PBrg(m, qp)) <= width * 0.5f) ++n;
  }
  return n;
}
// a4fea0 -> a4fbc0: players of team `sel` (2 = both) in the cone around `pos`
// (strict half-width).
int ConeAt(const RunWorld& w, int sel, Vec2f pos, float center, float width, float radius,
           float minDist) {
  int n = 0;
  for (int i = 0; i < 2 * kSlots; ++i) {
    if (!w.p[i].active || (sel != 2 && TeamOf(i) != sel)) continue;
    const float d2 = Dist2(w.p[i].pos, pos);
    if (minDist * minDist < d2 && d2 < radius * radius &&
        Ad(center, PBrg(pos, w.p[i].pos)) < width * 0.5f)
      ++n;
  }
  return n;
}
// a4e250: an opponent within r.
bool OppNear(const RunWorld& w, int id, float r) {
  const int o = (TeamOf(id) ^ 1) * kSlots;
  for (int i = o; i < o + kSlots; ++i)
    if (Dist2(w.p[i].pos, w.p[id].pos) < r * r) return true;
  return false;
}
// a53da0: player j within (minD, maxD) of pt and within ang/2 of dir from it.
bool A53da0(const RunWorld& w, int j, Vec2f pt, float dir, float ang, float maxD, float minD) {
  const float d2 = Dist2(w.p[j].pos, pt);
  return d2 < maxD * maxD && minD * minD < d2 && Ad(dir, PBrg(pt, w.p[j].pos)) < ang * 0.5f;
}
// 69cd40: a defender D's perpendicular gap to the lane A->B if he reaches it
// before the ball (5.65·t + 0.6 m), else -1.
float LaneIntercept(Vec2f A, Vec2f B, Vec2f C, Vec2f D, float speed, bool flag) {
  Vec2f u{B.x - A.x, B.z - A.z};
  const float l = Len(u);
  if (l > 0.0f) u = {u.x / l, u.z / l};
  auto along = [&](Vec2f p) { return (p.x - A.x) * u.x + (p.z - A.z) * u.z; };
  auto lat = [&](Vec2f p) { return -u.z * (p.x - A.x) + u.x * (p.z - A.z); };
  const float aD = along(D), lD = lat(D);
  if (aD < 0.0f && std::fabs(aD) > 0.5f) return -1.0f;
  if (flag) {
    if (Dist2(A, B) < Dist2(D, A) && Dist2(B, C) < Dist2(B, D)) return -1.0f;
    const float lC = lat(C);
    const float da = Fold180(PBrg(A, B) - PBrg(A, C));
    if ((std::fabs(da) < 20.0f || lC < 0.7f) && along(C) < aD) {
      if (std::fabs(lD - lC) < 1.5f) return -1.0f;
      if (std::fabs(lC) < std::fabs(lD)) return -1.0f;
    }
  }
  if (std::fabs(lD) <= 0.5f) return std::fabs(lD);
  const float thr = 100.0f / 17.7f * (aD / speed) + 0.6f;
  return std::fabs(lD) > thr ? -1.0f : std::fabs(lD);
}

// FUN_1408CB0B0 on the Defensive Full-back bit (FUN_140a64030(id, 0x11)): his
// style bit and his role's compat row. Off the pes_loop path there is no style
// table, so GF's stand-in (a class-2 SB, LB or RB) holds.
// ponytail: the stand-in goes when pes_loop is the only path.
bool StyleLb(const RunWorld& w, const PlayerFacts& me) {
  if (!w.styleCompat) return me.roleClass == pes::roles::kSideBackClass;
  return pes::style::Gate(*w.styleCompat, me.roleClass, me.styleWord,
                          pes::style::EngineBit(PlayingStyles::Player::DefensiveFullBack));
}

struct Ctx {
  const RunWorld& w;
  int id, t, opp, c, q;
  float s, dir, tt;
  Vec2f P, B;
  const PlayerFacts& me;
  Ctx(const RunWorld& w_, int id_)
      : w(w_), id(id_), t(TeamOf(id_)), opp(TeamOf(id_) ^ 1), c(w_.carrier),
        q(w_.receiver[TeamOf(id_)]), s(w_.s[TeamOf(id_)]), dir(TDir(w_, TeamOf(id_))),
        tt(w_.p[id_].t), P(w_.p[id_].pos), B(w_.ball), me(w_.p[id_]) {}
  float D2() const { return Dist2(P, B); }
};

bool EvKick(const BallEvent& e) {
  return e.valid && (e.kind == 1 || e.kind == 2 || e.kind == 3 || e.kind == 0x10);
}
// Head H of 692810 / 692a00 / 692580: loose ball, kicked during this run and
// at most a second ago.
bool LooseKick(const Ctx& x) {
  return x.c == kNoId && EvKick(x.w.ev) && x.w.ev.age <= x.tt && x.w.ev.age <= kPesFps;
}

// s1 #1, step 4.
bool L68f600(const Ctx& x) {
  if (x.s * x.P.x > pesGoalAreaLineX + 1.5f) return true;
  if (x.s * x.P.x < pesGoalAreaLineX - 3.0f) return false;
  if (!A720(x.B.x, x.P.x, x.s, 0.2f)) return false;
  const float a = Ad(x.dir, PBrg(x.B, x.me.aim));
  if (a < 22.0f || ConeOpp(x.w, x.id, x.dir, 50.0f, 20.0f)) return true;
  if (A720(x.B.x, x.P.x, x.s, -0.1f)) return true;
  const float wd = Ad(x.dir, PBrg(x.P, x.B));
  std::vector<int> opps;
  for (int i = x.opp * kSlots; i < x.opp * kSlots + kSlots; ++i)
    if (x.w.p[i].active) opps.push_back(i);
  std::sort(opps.begin(), opps.end(),
            [&](int a, int b) { return x.s * x.w.p[a].pos.x > x.s * x.w.p[b].pos.x; });
  for (int o : opps) {
    const Vec2f op = x.w.p[o].pos;
    if (x.s * op.x < x.s * x.P.x) break;
    if (std::fabs(op.z - x.B.z) <= std::fabs(x.P.z - x.B.z) &&
        A53da0(x.w, o, x.P, x.dir, 2.0f * wd, 30.0f, 0.0f))
      return true;
  }
  return false;
}
// s1 #3 (step 8, run != Counter).
bool L692810(const Ctx& x) {
  if (!LooseKick(x)) return false;
  if (x.s * x.B.x <= x.s * x.P.x) return Dist2(x.w.ev.pos, x.B) >= 400.0f;
  return false;
}
// s1 #4 (step 9).
bool L692a00(const Ctx& x) {
  if (!LooseKick(x)) return false;
  if (!A720(x.B.x, x.P.x, x.s, 8.0f)) return false;
  const Vec2f e = x.w.ev.pos;
  if (x.s * x.B.x > x.s * e.x) return false;
  if (std::fabs(e.x - x.B.x) > 7.0f && Beyond(x.w, x.id, -4.0f)) return true;
  return std::fabs(e.x - x.B.x) >= 12.0f;
}
// s1 #5 (step 9, run != Counter).
bool L692580(const Ctx& x) {
  if (!LooseKick(x)) return false;
  if (Dist2(x.w.ev.pos, x.B) < 100.0f) return false;
  if (Ad(x.dir, PVec(x.me.vel)) > 90.0f) return true;
  return Len(x.me.vel) < Kmh(10.0f);
}
// s1 #6: exemption (the caller stops only when this is false).
bool L68a730(const Ctx& x, int run) {
  if (run < kSecondLine || run > kCounter || !Beyond(x.w, x.id, -7.0f)) return false;
  const float dz = x.P.z - x.B.z, dx = x.P.x - x.B.x;
  if (std::fabs(x.P.z) >= 15.0f) {
    if (dx * dx + dz * dz > 484.0f) return false;
  } else {
    if (std::fabs(dz) > 25.0f) return false;
    if (std::fabs(dx) > 12.0f && !A720(x.P.x, x.B.x, x.s, 0.0f)) return false;
  }
  if (OppNear(x.w, x.id, 3.2f)) return false;
  return x.s * x.P.x <= pesPenaltyAreaLineX + 2.0f;
}
// s1 #7: exemption.
bool L68a8c0(const Ctx& x, bool intercept) {
  if (intercept) return false;
  if (std::fabs(x.P.z) > pesGoalAreaHalfWidth + 2.0f || !Beyond(x.w, x.id, -2.0f)) return false;
  const Vec2f A = x.me.aim;
  if (Dist2(A, x.B) > 729.0f) return false;
  if (!Valid(x.q) || Dist2(x.w.p[x.q].pos, x.B) > 12.25f) return false;
  if (ConeOpp(x.w, x.q, PBrg(x.B, A), 80.0f, 6.5f)) return false;
  const auto near = NearestOpponents(x.w, x.id);
  for (size_t i = 0; i < near.size() && i < 4; ++i) {
    const Vec2f o = x.w.p[near[i].id].pos;
    if (Dist2(x.P, o) > 10.24f) return true;
    if (Ad(x.dir, PBrg(x.P, o)) <= 82.0f && Dist2(o, A) <= Dist2(x.P, A)) return false;
  }
  return true;
}
// s1 #9: blocks the step-12 stop.
bool L694d10(const Ctx& x) {
  if (!Valid(x.c)) return false;
  const PlayerFacts& c = x.w.p[x.c];
  if (std::fabs(x.P.x - c.pos.x) > 8.0f) return false;
  const float a = Ad(x.dir, PBrg(c.pos, x.P));
  if (a < 60.0f || a > 120.0f) return false;
  const float side = x.P.z > c.pos.z ? 0.0f : 180.0f;
  if (ConeOpp(x.w, x.c, side, 70.0f, 7.0f)) return false;
  return std::fabs(x.P.z - c.pos.z) < 20.0f || !Beyond(x.w, x.id, -6.0f);
}
// s1 #10 (step 13).
bool L68dba0(const Ctx& x) {
  if (!Beyond(x.w, x.id, -3.0f) || !Valid(x.q)) return false;
  if (x.D2() < 100.0f || !A720(x.B.x, x.P.x, x.s, 5.0f)) return false;
  const PlayerFacts& q = x.w.p[x.q];
  const float h = PVec(q.vel), f = q.facing, b = PBrg(x.B, x.P);
  const float m = std::min(Ad(h, b), Ad(f, b));
  if (m < 135.0f && (Ad(h, x.dir) < 90.0f || Ad(f, x.dir) < 90.0f) && Ad(b, x.dir) < 36.0f)
    return false;
  return m >= 120.0f;
}
// s1 #11 (step 13).
bool L68de30(const Ctx& x) {
  if (!Valid(x.c)) return false;
  const PlayerFacts& c = x.w.p[x.c];
  if (Dist2(x.P, c.pos) < 144.0f) return false;
  const Vec2f A = x.me.aim;
  const float b = PBrg(x.P, A);
  const float fwd = x.P.z <= A.z ? 0.0f : 180.0f;
  if (Ad(b, fwd) > 65.0f) return false;
  if (!A720(c.pos.x, x.P.x, x.s, 3.0f) || !Beyond(x.w, x.id, -4.0f)) return false;
  const float back = Wrap360(fwd + 180.0f);
  if (Ad(PBrg(c.pos, x.P), back) < 85.0f) return false;
  if (Ad(PVec(c.vel), back) > 65.0f) return false;
  return Ad(c.facing, PBrg(c.pos, A)) >= 95.0f;
}
// s1 #12 (step 13).
bool L68e300(const Ctx& x) {
  if (!Valid(x.c) || !Beyond(x.w, x.id, -6.0f) || x.tt < Frames(0.1f)) return false;
  const PlayerFacts& c = x.w.p[x.c];
  const float side = x.P.z <= c.pos.z ? 0.0f : 180.0f;
  if (Ad(side, PVec(c.vel)) > 25.0f || Ad(side, c.facing) > 25.0f) return false;
  const Vec2f A2 = x.me.aim;  // ponytail: 599ff0's point taken as the run target
  if (Dist2(c.pos, A2) < 144.0f || Dist2(c.pos, x.P) < 25.0f) return false;
  return Ad(c.facing, PBrg(c.pos, A2)) >= 105.0f;
}
// s3 #19, step 6: false = stop. The classes are the B array (FUN_1408c51f0, pesroles.hpp).
bool L596240(const Ctx& x) {
  if (x.me.roleClass >= 3) return true;
  const bool me = IsRun(x.me.state);
  for (int j = x.t * kSlots; j < x.t * kSlots + kSlots; ++j) {
    if (j == x.id || !x.w.p[j].active) continue;
    const int nj = x.w.p[j].roleClass;
    if (nj < 3 && nj != 0 && IsRun(x.w.p[j].state) &&
        (!me || Dist2(x.w.p[j].pos, x.B) <= x.D2()))
      return false;
  }
  return true;
}
// s1 #14 partial: 68efc0. ponytail: a4f720 and 695110 (grid-flag reader
// a72180) are not ported; the branch that needs them keeps.
bool L68efc0(const Ctx& x) {
  const float a = Ad(x.dir, PBrg(x.B, x.P));
  if (a > 22.0f || x.tt < 2.0f * kPesFps || !Beyond2(x.w, x.id, -3.0f)) return false;
  if (a > 4.0f) return false;
  if (std::fabs(x.P.x - x.B.x) > 8.0f && ConeAt(x.w, x.opp, x.B, x.dir, 90.0f, 3.0f, 0.0f))
    return true;
  const auto near = NearestOpponents(x.w, x.id);
  if (std::fabs(x.P.x - x.B.x) >= 15.0f && !near.empty()) {
    const float d = Dist2(x.w.p[near[0].id].pos, x.P);
    return (std::fabs(x.P.x - x.B.x) > 15.0f && d < 12.25f) || d <= 25.0f;
  }
  return false;
}
// s1 #16.
bool L68e900(const Ctx& x) {
  const float bb = PBrg(x.B, x.P);
  if (x.tt < kPesFps || Ad(bb, x.dir) > 22.0f || !Beyond(x.w, x.id, -0.8f) ||
      std::fabs(x.P.x - x.B.x) < 8.5f)
    return false;
  const auto near = NearestOpponents(x.w, x.id);
  if (near.empty() || Dist2(x.w.p[near[0].id].pos, x.P) > 16.0f) return false;
  if (!Valid(x.q) || x.w.p[x.q].humanControlled) return true;
  const PlayerFacts& q = x.w.p[x.q];
  if (Ad(q.facing, bb) < 40.0f) return false;
  return Ad(PBrg(q.pos, q.aim), bb) >= 40.0f;
}
// s1 #17.
bool L68ec50(const Ctx& x) {
  if (!Beyond(x.w, x.id, -3.0f) || !A720(x.B.x, x.P.x, x.s, 14.0f)) return false;
  if (Ad(x.dir, PBrg(x.B, x.P)) > 22.0f || x.D2() < 144.0f) return false;
  if (Ad(x.dir, PVec(x.me.vel)) > 22.0f || Ad(x.dir, PBrg(x.P, x.me.aim)) > 22.0f) return false;
  const int m = x.me.marker;
  if (!Valid(m)) return false;
  const PlayerFacts& o = x.w.p[m];
  if (!(x.s * x.P.x <= x.s * o.pos.x) || std::fabs(x.P.x - o.pos.x) < 0.5f ||
      std::fabs(x.P.z - o.pos.z) > 2.0f)
    return false;
  if (Ad(x.dir, PVec(o.vel)) > 22.0f) return false;
  const float vr = Len(x.me.vel), vo = Len(o.vel);
  return vr <= vo || std::fabs(vr - vo) <= Kmh(10.0f);
}
// s1 #18: true -> skip the stop branch.
bool L68d920(const Ctx& x) {
  if (!Valid(x.q)) return false;
  const PlayerFacts& q = x.w.p[x.q];
  const float f = q.facing;
  if (Ad(f, PBrg(q.pos, x.P)) < 90.0f) return false;
  if (Ad(x.dir, f) < 100.0f || !ConeOpp(x.w, x.q, x.dir, 100.0f, 5.0f)) return false;
  for (int m = x.t * kSlots; m < x.t * kSlots + kSlots; ++m) {
    if (m == x.id || m == x.q || !x.w.p[m].active) continue;
    if (Ad(f, PBrg(q.pos, x.w.p[m].pos)) <= 45.0f && Dist2(q.pos, x.w.p[m].pos) <= 100.0f)
      return true;
  }
  return false;
}
// s1 #20.
bool L68b070(const Ctx& x) {
  if (x.s * x.B.x < pesGoalAreaLineX - 3.0f) return false;
  if (std::fabs(x.P.x - x.B.x) > 6.0f || std::fabs(x.P.z - x.B.z) > 7.0f) return false;
  if (std::fabs(x.P.z) < pesGoalAreaHalfWidth) return false;
  return x.s * x.P.x >= pesPenaltySpotX + 1.5f;
}
// s1 #21. a57800's point taken as the opponents' goal centre (INF goal point).
bool L692c50(const Ctx& x) {
  if (Valid(x.c) || !EvKick(x.w.ev)) return false;
  if (Dist2(x.P, x.me.aim) < 16.0f) return false;
  const float a1 = Fold180(x.dir - PBrg(x.P, x.me.aim));
  if (std::fabs(a1) < 75.0f) return false;
  const Vec2f g{x.s * pesFieldHalfLength, 0.0f};
  const float a2 = Fold180(x.dir - PBrg(g, x.B));
  if (std::fabs(a2) < 45.0f || std::fabs(a2) > 135.0f) return false;
  if (Sgn(a1) == Sgn(a2)) return false;
  if (std::fabs(x.B.z - g.z) >= 6.5f && Sgn(g.z - x.P.z) == Sgn(x.B.z - x.P.z)) return false;
  return true;
}
// s1 #22. a72e70 bit 12 taken as "inside the penalty area".
bool InBox(float s, Vec2f p) {
  return s * p.x >= pesPenaltyAreaLineX && std::fabs(p.z) <= pesPenaltyAreaHalfWidth;
}
bool L643f50(const RunWorld& w, int team);
bool L68b920(const Ctx& x) {
  if (!InBox(x.s, x.B) || !InBox(x.s, x.P)) return false;
  if (x.D2() > 144.0f || x.s * x.B.x < pesPenaltyAreaLineX) return false;
  if (A720(x.B.x, x.P.x, x.s, 3.0f)) return false;
  return L643f50(x.w, x.t) || x.s * x.B.x >= pesPenaltySpotX;
}
// s2 #1. marks = 0, cls = 0.
bool L691ef0(const Ctx& x) {
  if (!Valid(x.c)) return false;
  if (Dist2(x.P, x.w.p[x.c].pos) > 121.0f) return false;
  const Vec2f G{x.s * pesFieldHalfLength, 0.0f};
  if (Dist2(G, x.B) >= 1024.0f) return false;
  const float gB = PBrg(x.B, G);
  if (Ad(x.dir, gB) > 47.0f || ConeOpp(x.w, x.c, gB, 50.0f, 4.5f)) return false;
  const auto near = NearestOpponents(x.w, x.id);
  if (x.D2() > 169.0f && !near.empty() && Dist2(x.w.p[near[0].id].pos, x.P) > 25.0f) return false;
  const Vec2f Q{x.P.x + x.me.vel.x, x.P.z + x.me.vel.z};  // a75740, 54 frames ahead
  const float d = Fold180(x.dir - PBrg(x.P, Q));
  if (std::fabs(d) > 35.0f && Dist2(x.B, Q) > 64.0f) {
    const PlayerFacts& c = x.w.p[x.c];
    const float bc = PBrg(c.pos, c.aim);
    if (Ad(bc, x.dir) < 15.0f) return false;
    if (Sgn(Fold180(x.dir - bc)) != Sgn(d)) return false;
  }
  const float bP = PBrg(x.B, x.P);
  const float a1 = PBrg(x.B, {G.x, pesGoalInnerHalfWidth + 0.5f}),
                 a2 = PBrg(x.B, {G.x, -(pesGoalInnerHalfWidth + 0.5f)});
  auto within = [&](float a, float b) { return bP >= std::min(a, b) && bP <= std::max(a, b); };
  return within(a1, gB) || within(a2, gB);
}
// s2 #2. Z = the slot point.
bool L692fe0(const Ctx& x) {
  if (std::fabs(x.B.z - x.P.z) < 13.0f || std::fabs(x.P.z) < pesPenaltyAreaHalfWidth - 5.0f) return false;
  if (x.s * x.P.x < pesPenaltyAreaLineX - 10.0f) return false;
  if (!Valid(x.q)) return true;
  const PlayerFacts& q = x.w.p[x.q];
  const float b = PBrg(q.pos, x.P);
  if (Ad(q.facing, b) < 55.0f || Ad(q.facing, x.dir) > 135.0f) return false;
  const Vec2f Z = x.me.slotPoint;
  if (std::fabs(Z.z) > pesGoalAreaHalfWidth || std::fabs(Z.z - x.B.z) > 12.0f) return false;
  if (!A720(x.B.x, Z.x, x.s, 3.0f)) return false;
  const auto near = NearestOpponents(x.w, x.id);
  if (!near.empty() && Dist2(x.w.p[near[0].id].pos, x.P) > 196.0f) return false;
  return ConeOpp(x.w, x.q, b, 80.0f, std::sqrt(Dist2(q.pos, x.P)) + 5.0f) > 0;
}
// s2 #3.
bool L690b90(const Ctx& x) {
  if (std::fabs(x.B.z) > pesGoalAreaHalfWidth || x.s * x.B.x < pesPenaltyAreaLineX ||
      std::fabs(x.P.z) < pesPenaltyAreaHalfWidth)
    return false;
  const float r = Ramp(std::fabs(x.P.x), pesPenaltyAreaLineX, pesPenaltySpotX);
  return std::fabs(x.B.z - x.P.z) >= 17.0f - 7.0f * r;
}
// s2 #4.
bool L690630(const Ctx& x) {
  if (x.s * x.B.x < pesPenaltySpotX || x.s * x.P.x < pesPenaltyAreaLineX) return false;
  if (Sgn(x.P.z) == Sgn(x.B.z) && std::fabs(x.B.z) < std::fabs(x.P.z) &&
      std::fabs(x.P.z) > pesGoalAreaHalfWidth &&
      std::fabs(x.P.z - x.B.z) < 14.0f)
    if (A720(x.s * pesGoalAreaLineX, x.P.x, x.s, -3.0f) ||
        A720(x.s * pesGoalAreaLineX, x.B.x, x.s, -3.0f))
      return true;
  if (x.s * x.P.x < pesPenaltySpotX && !A720(x.B.x, x.P.x, x.s, -8.0f)) {
    const float tz = x.me.aim.z;
    if (std::fabs(tz) < pesGoalAreaHalfWidth && std::fabs(tz - x.B.z) < 22.0f) return false;
  }
  const float z = x.P.z;
  return (std::fabs(z) >= pesPenaltyAreaHalfWidth || std::fabs(z - x.B.z) >= 20.0f) &&
         std::fabs(x.B.z - z) >= 10.0f && Sgn(z) == Sgn(x.B.z);
}
// s2 #5. R = the run target.
bool L690400(const Ctx& x) {
  if (x.s * x.B.x < pesPenaltySpotX || std::fabs(x.B.z) < pesGoalAreaHalfWidth - 1.0f) return false;
  if (Sgn(x.P.z) != Sgn(x.B.z) || std::fabs(x.P.z) > pesPenaltyAreaHalfWidth) return false;
  const Vec2f R = x.me.aim;
  if (Sgn(x.B.z) == Sgn(R.z) && std::fabs(x.B.z) <= std::fabs(R.z) && std::fabs(x.P.z) <= std::fabs(R.z))
    return std::fabs(R.z) >= pesGoalAreaHalfWidth + 3.0f;
  return false;
}
// s2 #6.
bool L690cf0(const Ctx& x) {
  if (x.tt < Frames(1.8f) || x.s * x.P.x < pesPenaltyAreaLineX - 2.0f) return false;
  if (Len(x.me.vel) > 0.9f * Per100(22.5f)) return false;
  if (x.D2() > 400.0f) return false;
  for (const NearOpponent& n : NearestOpponents(x.w, x.id))
    if (std::fabs(x.w.p[n.id].pos.z - x.B.z) <= std::fabs(x.P.z - x.B.z) &&
        A53da0(x.w, n.id, x.P, x.dir, 180.0f, 3.5f, 0.0f))
      return true;
  return false;
}
// s2 #7.
bool L694650(const Ctx& x) {
  if (x.s * x.P.x < pesPenaltyAreaLineX || std::fabs(x.P.z) > pesGoalInnerHalfWidth + 2.0f) return false;
  if (Ad(x.dir, PVec(x.me.vel)) > 32.0f) return false;
  const float vMin = 0.8f * Per100(12.9f);
  if (Len(x.me.vel) < vMin || !Valid(x.q)) return false;
  if (Dist2(x.w.p[x.q].pos, x.B) > 9.0f) return false;
  if (std::fabs(x.P.z - x.B.z) < 5.0f) return false;
  if (x.s * x.B.x < pesPenaltySpotX - 2.5f || x.D2() > 400.0f) return false;
  if (A720(x.B.x, x.P.x, x.s, 1.0f)) return false;
  const int m = x.me.marker;
  if (!Valid(m)) return false;
  const PlayerFacts& o = x.w.p[m];
  const float a = Ad(x.dir, PBrg(x.P, o.pos));
  if (a > 70.0f) return false;
  const float dm = Dist2(x.P, o.pos);
  if ((Sgn(o.pos.z - x.P.z) == Sgn(x.B.z - x.P.z) || a >= 80.0f || dm <= 9.0f) &&
      (a >= 30.0f || dm <= 7.29f))
    if (Ad(x.dir, PVec(o.vel)) > 32.0f || Len(o.vel) < vMin) return false;
  const float bb = PBrg(x.B, x.P), r = std::sqrt(x.D2());
  if (ConeAt(x.w, x.opp, x.B, bb, 40.0f, r, 3.0f) && !A53da0(x.w, m, x.B, bb, 40.0f, r, 3.0f))
    return false;
  const float d = x.P.z < x.B.z ? 0.0f : 180.0f;
  return !(ConeAt(x.w, x.opp, x.P, d, 180.0f, 3.0f, 0.0f) && !A53da0(x.w, m, x.P, d, 180.0f, 3.0f, 0.0f));
}
// s2 #8 / #9: 695110 (grid-flag reader) taken as false.
bool L693a20(const Ctx& x) {
  if (x.tt < Frames(1.2f) || !Beyond2(x.w, x.id, -3.5f)) return false;
  const Vec2f R = x.me.aim;
  if (std::fabs(x.P.z) < pesPenaltyAreaHalfWidth + 3.0f) {
    if (std::fabs(x.P.z - x.B.z) > 4.0f || std::fabs(x.P.z) < pesPenaltyAreaHalfWidth - 1.0f) return false;
    if (std::fabs(x.P.z - R.z) > 4.0f && Sgn(R.z - x.P.z) == Sgn(-x.P.z)) return false;
  }
  if (std::fabs(x.P.x - x.B.x) < 8.0f || !A720(x.B.x, x.P.x, x.s, 0.0f)) return false;
  if (Ad(x.dir, PBrg(x.B, x.P)) > 36.0f || !OppNear(x.w, x.id, 8.0f)) return false;
  const int m = x.me.marker;
  if (!Valid(m) || !A53da0(x.w, m, x.P, x.dir, 200.0f, 8.0f, 0.0f)) return false;
  const float dz = x.P.z - x.B.z, dm = x.w.p[m].pos.z - x.B.z;
  if (Sgn(dz) == Sgn(dm) && std::fabs(dm) > std::fabs(dz)) return false;
  const float n = Frames(0.5f) / kPesFps;
  const Vec2f Pp{x.P.x + n * x.me.vel.x, x.P.z + n * x.me.vel.z};
  return Ad(x.dir, PBrg(Pp, R)) <= 45.0f || std::fabs(R.z - x.w.p[m].pos.z) <= 5.0f;
}
bool L693730(const Ctx& x, bool flag) {
  if (!flag || x.tt < Frames(2.2f) || !Beyond2(x.w, x.id, -2.5f)) return false;
  if (x.D2() < 289.0f || std::fabs(x.P.z) < pesPenaltyAreaHalfWidth - 2.0f) return false;
  if (std::fabs(x.P.x - x.B.x) < 10.0f || !A720(x.B.x, x.P.x, x.s, 0.0f)) return false;
  const int m = x.me.marker;
  if (!Valid(m) || !A53da0(x.w, m, x.P, x.dir, 200.0f, 8.0f, 0.0f)) return false;
  const float dz = x.P.z - x.B.z, dm = x.w.p[m].pos.z - x.B.z;
  return !(Sgn(dz) == Sgn(dm) && std::fabs(dm) > std::fabs(dz));
}
// s2 #12.
bool L68e660(const Ctx& x, bool flag) {
  if (!flag || !Valid(x.c)) return false;
  const PlayerFacts& c = x.w.p[x.c];
  if (std::fabs(x.P.z - c.pos.z) < 17.0f || Dist2(c.pos, c.aim) < 4.0f) return false;
  const float bP = PBrg(c.pos, x.P);
  if (Ad(c.facing, bP) < 145.0f) return false;
  const float bt = PBrg(c.pos, c.aim);
  if (Ad(bt, bP) < 125.0f || Len(c.vel) < Per100(32.7f) || Ad(x.dir, bt) < 100.0f) return false;
  return std::fabs(x.P.x - x.B.x) >= 9.0f;
}
// s2 #13.
bool L68b440(const Ctx& x, bool flag) {
  if (!flag || x.tt < Frames(3.0f)) return false;
  if (Ad(x.dir, PBrg(x.B, x.P)) > 45.0f || Ad(x.dir, PVec(x.me.vel)) < 95.0f) return false;
  if (Ad(x.dir, PBrg(x.P, x.me.aim)) < 95.0f) return false;
  if (!Beyond(x.w, x.id, 0.2f) || x.D2() < 400.0f) return false;
  if (x.D2() <= 1369.0f && Valid(x.q))
    return Ad(x.w.p[x.q].facing, PBrg(x.w.p[x.q].pos, x.P)) >= 65.0f;
  return true;
}
// s2 #14: sqrt(D2) >= 1444 (raw) never holds on a pitch; kept as written.
bool L68c050(const Ctx& x) { return x.tt >= Frames(2.5f) && std::sqrt(x.D2()) >= 1444.0f; }
// s2 #16.
bool L692ea0(const Ctx& x) {
  const float bx = x.s * x.B.x;
  if (bx < pesPenaltyAreaLineX - 5.0f) return false;
  const float r = Ramp(bx, pesPenaltyAreaLineX - 5.0f, pesPenaltyAreaLineX + 2.0f);
  if (std::fabs(x.me.aim.z) < pesPenaltyAreaHalfWidth - (3.0f * r + 3.0f)) return false;
  return std::fabs(x.B.z - x.P.z) >= 18.0f;
}
// s2 #18 / #19: SecondLine only.
bool L691a80(const Ctx& x) {
  if (x.tt < Frames(1.0f) || x.me.state != kSecondLine) return false;
  if (A720(x.B.x, x.P.x, x.s, 8.0f) && Ad(PBrg(x.P, x.me.aim), PBrg(x.B, x.P)) < 45.0f) return true;
  if (!A720(x.B.x, x.P.x, x.s, 13.0f) || Beyond(x.w, x.id, -5.0f)) return false;
  return ConeOpp(x.w, x.id, x.dir, 70.0f, 20.0f) || ConeOpp(x.w, x.id, x.dir, 170.0f, 2.5f);
}
// s2 #21.
bool L68d0b0(const Ctx& x, bool flag) {
  if (!L643f50(x.w, x.t)) return false;
  if (x.s * x.P.x < pesPenaltyAreaLineX - 1.0f || std::fabs(x.P.z) < pesGoalAreaHalfWidth + 2.0f || std::fabs(x.me.aim.z) < pesGoalAreaHalfWidth)
    return false;
  if (flag && (std::fabs(x.P.z) < pesPenaltyAreaHalfWidth || A720(x.B.x, x.P.x, x.s, 7.0f) || x.D2() > 144.0f))
    return false;
  if (std::fabs(x.P.z - x.B.z) < 3.0f || std::fabs(x.B.z) < std::fabs(x.P.z) ||
      std::fabs(x.B.z) < pesPenaltyAreaHalfWidth + 1.5f || x.s * x.B.x > pesPenaltySpotX)
    return true;
  // ponytail: 44e020 (the T+0xc060 per-player value) is undecoded; taken as
  // 60, so this test passes on.
  const int r = x.q;
  if (!Valid(r)) return true;
  const Vec2f rp = x.w.p[r].pos;
  for (const NearOpponent& n : NearestOpponents(x.w, x.id)) {
    const Vec2f op = x.w.p[n.id].pos;
    if (Dist2(op, x.P) > 64.0f) return true;
    if (std::fabs(op.z) <= std::fabs(rp.z) && A720(rp.x, op.x, x.s, -1.5f) &&
        Ad(x.dir, PBrg(rp, op)) <= 95.0f) {
      if (x.s * rp.x < x.s * x.P.x && std::fabs(rp.x - x.P.x) > 6.0f) return true;
      if (ConeOpp(x.w, x.id, x.dir, 50.0f, 50.0f)) return true;
      for (int j = x.t * kSlots; j < x.t * kSlots + kSlots; ++j) {
        if (j == x.id || j == r || !x.w.p[j].active) continue;
        const Vec2f jp = x.w.p[j].pos;
        if (x.s * jp.x < pesPenaltyAreaLineX - 5.0f) return true;
        if (Sgn(x.P.z - x.B.z) == Sgn(jp.z - x.B.z) &&
            (x.w.p[j].state == kCenteringGet || (x.s * jp.x > pesPenaltyAreaLineX && std::fabs(jp.z) < pesGoalAreaHalfWidth)))
          return false;
      }
      return true;
    }
  }
  return true;
}
// s3 #2 (Chance only). 594a10 (path test) and 695110 not ported: that tail
// keeps.
bool L690960(const Ctx& x) {
  if (x.me.state != kChance || x.tt < Frames(0.6f) || !Beyond(x.w, x.id, -7.0f)) return false;
  const float L = x.w.lineX[x.opp];
  float k = -16.0f;
  if (Valid(x.c)) {
    if (!Beyond(x.w, x.id, -3.0f)) return false;
    k = -20.0f;
  }
  if (x.D2() < 484.0f) return false;
  if (A720(L, x.B.x, x.s, k)) return false;
  return Beyond(x.w, x.id, 0.0f);
}
// s3 #8 (cls != 0x13).
bool L693ef0(const Ctx& x) {
  if (x.s * x.P.x < pesPenaltySpotX || std::fabs(x.P.z) < pesGoalInnerHalfWidth + 1.5f) return false;
  if (std::fabs(x.B.z) > std::fabs(x.P.z) || std::fabs(x.P.z - x.B.z) < 6.0f) return false;
  std::vector<int> mates;
  for (int j = x.t * kSlots; j < x.t * kSlots + kSlots; ++j)
    if (x.w.p[j].active) mates.push_back(j);
  std::sort(mates.begin(), mates.end(),
            [&](int a, int b) { return x.s * x.w.p[a].pos.x > x.s * x.w.p[b].pos.x; });
  for (int j : mates) {
    const Vec2f jp = x.w.p[j].pos;
    if (x.s * jp.x < x.s * x.P.x) break;
    if (std::fabs(jp.z - x.B.z) <= std::fabs(x.P.z - x.B.z)) return true;
  }
  return false;
}
// s3 #10.
bool L68d730(const Ctx& x) {
  if (x.s * x.B.x < pesPenaltySpotX || std::fabs(x.B.z) < pesGoalAreaHalfWidth || std::fabs(x.B.z) > pesPenaltyAreaHalfWidth) return false;
  const float n = Frames(0.3f) / kPesFps;
  const Vec2f Q{x.P.x + n * x.me.vel.x, x.P.z + n * x.me.vel.z};
  if (Sgn(Q.z) != Sgn(x.B.z)) return false;
  auto inChannel = [&](Vec2f v) {
    return x.s * v.x > pesPenaltySpotX && std::fabs(v.z) > pesGoalAreaHalfWidth && std::fabs(v.z) < pesPenaltyAreaHalfWidth;
  };
  return inChannel(Q) || inChannel(x.me.aim);
}
// s3 #11.
bool L68fc70(const Ctx& x) {
  if (x.s * x.B.x < pesPenaltySpotX) return false;
  const Vec2f A = x.me.aim;
  if (Sgn(A.z) == Sgn(x.B.z)) return false;
  return std::fabs(A.z) >= pesGoalAreaHalfWidth && std::fabs(x.B.z - A.z) >= 8.0f;
}
// s3 #12: the c064 value taken as 0 (passes).
bool L68cde0(const Ctx& x) {
  const Vec2f A = x.me.aim;
  if (std::fabs(A.z) < pesPenaltyAreaHalfWidth - 2.0f) return false;
  for (int j = x.t * kSlots; j < x.t * kSlots + kSlots; ++j) {
    if (j == x.id || !x.w.p[j].active || x.w.p[j].state != kOverlap) continue;
    const Vec2f jp = x.w.p[j].pos;
    if (Sgn(jp.z) == Sgn(A.z)) {
      if (std::fabs(jp.x - x.P.x) >= 6.0f && !Beyond(x.w, j, -7.0f))
        return pesPenaltyAreaLineX - 10.0f < x.s * jp.x;
      return true;
    }
  }
  return false;
}
// s3 #13 (marks 0).
bool L68cc40(const Ctx& x) {
  if (x.tt < Frames(0.3f) || x.s * x.P.x < pesPenaltyAreaLineX) return false;
  if (!A720(x.B.x, x.P.x, x.s, 3.0f) || x.D2() < 144.0f) return false;
  return Beyond(x.w, x.id, -1.0f);
}
// b4 #15 -> 694ee0 (decompiled for this port).
bool L694ee0(const Ctx& x, bool flag) {
  if (!Valid(x.q) || std::fabs(x.B.z) < pesPenaltyAreaHalfWidth) return false;
  const PlayerFacts& q = x.w.p[x.q];
  const float k = flag ? 0.8f : 0.0f, a = flag ? 2.0f : 0.0f;
  if (Ad(x.dir, PVec(q.vel)) > 85.0f || Ad(x.dir, q.facing) > 85.0f) return false;
  return !ConeOpp(x.w, x.q, x.dir, a + 90.0f, k + 40.0f) &&
         !ConeOpp(x.w, x.q, x.dir, a + 160.0f, k + 4.5f) &&
         !ConeOpp(x.w, x.q, x.dir, a + 120.0f, k + 6.0f);
}
bool L68ac40(const Ctx& x, Vec2f pos, bool flag) {
  if (!Valid(x.q) || std::fabs(x.B.z) < pesPenaltyAreaHalfWidth) return false;
  if (Sgn(x.P.z) != Sgn(x.B.z) || std::fabs(x.P.z) > std::fabs(x.B.z)) return false;
  const float k = flag ? 0.8f : 0.0f;
  if (A720(x.B.x, x.P.x, x.s, 5.0f + k) && !A720(x.w.lineX[x.opp], x.B.x, x.s, -10.0f - k))
    return false;
  const float lim = x.s * x.B.x <= pesPenaltyAreaLineX - 12.0f ? pesPenaltyAreaHalfWidth + 0.8f - k : pesGoalAreaHalfWidth + 0.8f - k;
  if (std::fabs(pos.z) < lim) return false;
  return L694ee0(x, flag);
}

// 643f50 (decompiled for this port): the carrier (or last toucher of the
// team) is free to cross: in the box zone and no opponent in four cones.
bool L643a30(const RunWorld& w, int team, int id);
bool L643f50(const RunWorld& w, int team) {
  const int c = w.carrier;
  if (!Valid(c) || TeamOf(c) != team) return false;
  const float s = w.s[team];
  const Vec2f cp = w.p[c].pos;
  if ((!L643a30(w, team, c) && s * cp.x < pesPenaltyAreaLineX - 6.0f) || std::fabs(cp.z) < pesPenaltyAreaHalfWidth + 2.0f) return false;
  const float dir = TDir(w, team);
  if (ConeOpp(w, c, dir, 110.0f, 15.0f) || ConeOpp(w, c, dir, 165.0f, 5.5f) ||
      ConeOpp(w, c, dir, 220.0f, 3.0f) || ConeOpp(w, c, dir, 265.0f, 2.2f))
    return false;
  return true;  // carrier valid: the decompile returns true on this path
}
// b1 #23, carrier branch only. ponytail: a63950/held (instruction 7), a72fb0
// (the grid lookup) are not ported; the line checks without them.
bool L643a30(const RunWorld& w, int team, int) {
  const int c = Valid(w.carrier) ? w.carrier : -1;
  if (c < 0) return false;
  const float s = w.s[team];
  const float A = TDir(w, team);
  const float vb = PVec(w.p[c].vel);
  if (s * w.ball.x < pesPenaltyAreaLineX && Ad(vb, A) > 65.0f) return false;
  if (s * w.ball.x < pesPenaltyAreaLineX && std::fabs(w.ball.z) < pesPenaltyAreaHalfWidth) return false;
  return true;
}


// The cascade FUN_140688c00 (run-targets 3), steps numbered as there.
bool CommonStop(const RunWorld& w, int id, int run) {
  const Ctx x(w, id);
  const bool counter = run == kCounter;
  // Head (off-ball 2.2).
  if (!w.openPlay || !w.possession[x.t]) return true;
  if (w.carrier == id) return true;
  if (x.s * x.P.x > pesGoalAreaLineX + 2.5f) return true;
  // 1: the last ball event is a pass to him.
  if (EvKick(w.ev) && w.ev.target == id) {
    if (w.ev.kind != 2) return true;
    if (w.ev.age >= kPesFps / 3.0f) return true;
  }
  // 2: ponytail: the carrier/receiver pass-aim keeps (5a7fe0/7be0) and stop
  // (5a7d10) read PES action records (+0x1490/+0x1494) GF has no equivalent of.
  // 3.
  if (Valid(x.c) && !Beyond(w, id, 0.7f) && w.possessionAge[x.t] < Frames(0.3f)) return false;
  // 4. ponytail: 68bd90 needs T+0xb824 (the team's region code), not decoded.
  if (L68f600(x)) return true;
  // 5: event kind 4 has no GF counterpart.
  // 6.
  const bool pinned = false;
  const float dx = std::fabs(x.P.x - x.B.x);
  const float k = dx <= 4.0f ? 10.0f : dx < 10.0f ? 10.0f * (10.0f - dx) / 6.0f : 0.0f;
  if (!L596240(x)) return true;
  // 8.
  const float d2 = x.D2();
  if (d2 > (k + 40.0f) * (k + 40.0f) && !A720(x.P.x, x.B.x, x.s, counter ? 13.0f : 8.0f))
    return true;
  if (!counter && L692810(x)) return true;
  // 9.
  if (L692a00(x)) return true;
  if (!counter && L692580(x)) return true;
  // 10.
  const float n = kRunPredictFrames / kPesFps;
  const Vec2f pred{x.P.x + n * x.me.vel.x, x.P.z + n * x.me.vel.z};
  bool intercept = false;
  for (int o = x.opp * kSlots; o < x.opp * kSlots + kSlots && !intercept; ++o)
    if (w.p[o].active && Dist2(w.p[o].pos, x.B) >= 6.25f &&
        LaneIntercept(x.B, pred, x.P, w.p[o].pos, 20.833334f, true) >= 0.0f)
      intercept = true;
  // 11. 5a4b90 needs the opponents' defensive mode (T+0xa50c): false.
  if (Valid(x.c) && x.tt > 2.0f * kPesFps && !L68a730(x, run) && !L68a8c0(x, intercept) &&
      !counter && !pinned) {
    const PlayerFacts& c = w.p[x.c];
    const float a = Ad(c.facing, PBrg(x.B, x.me.aim));
    if (a > 130.0f && d2 > 169.0f && (ConeOpp(w, x.c, x.dir, 70.0f, 6.5f) || d2 > 1156.0f))
      return true;
  }
  // 12. ponytail: of the run != 0x1d skip tests only |ball.x - P.x| < 15 is
  // ported (the rest are listed, not reduced, in run-targets 3).
  if (Valid(x.c) && x.tt > kPesFps + 0.5f && !counter && !pinned && d2 >= 289.0f &&
      std::fabs(x.P.z - x.B.z) > 0.5f && A720(x.B.x, x.P.x, x.s, -5.0f) &&
      !(run != kSecondLine && dx < 15.0f)) {
    const PlayerFacts& c = w.p[x.c];
    const float a = Ad(c.facing, PBrg(c.pos, x.P));
    const float dist = std::sqrt(Dist2(c.pos, x.P));
    const float wv = dist <= 15.0f ? 1.0f : dist < 27.0f ? (27.0f - dist) / 12.0f : 0.0f;
    if (a > 95.0f + 35.0f * wv && !L694d10(x) && (!Beyond(w, id, -10.0f) || a > 80.0f)) return true;
  }
  // 13.
  if (!pinned && (L68dba0(x) || L68de30(x) || L68e300(x))) return true;
  if (!counter && std::fabs(x.P.z - x.B.z) > 43.0f) return true;
  // 14 (cls != 2).
  if (run != kPullAway) {
    const float r = x.tt > Frames(0.8f) ? 0.7f : 1.5f;
    if (Valid(x.q) && (x.q == x.c || w.p[x.q].t <= 3.0f * kPesFps)) {
      if (Beyond2(w, id, r)) return true;
    } else if (Beyond(w, id, r)) {
      return true;
    }
  }
  // 15: state byte (falls) has no GF counterpart.
  // 16.
  if (x.tt > 4.0f * kPesFps && Valid(x.c) && !L68a730(x, run) && !L68a8c0(x, intercept) &&
      Beyond2(w, id, -2.5f)) {
    const PlayerFacts& c = w.p[x.c];
    const float dev = Ad(PBrg(c.pos, x.P), c.facing);
    const float bd = std::sqrt(d2);
    const float wv = bd <= 15.0f ? 1.0f : bd < 35.0f ? (35.0f - bd) / 20.0f : 0.0f;
    if (dev > 100.0f * wv + 70.0f) return true;
  }
  // 17 (Counter): keep-alive near a running mate skips to 19.
  bool skip18 = false;
  if (counter && x.tt <= 5.0f * kPesFps)
    for (int j = x.t * kSlots; j < x.t * kSlots + kSlots; ++j)
      if (j != id && IsRun(w.p[j].state) && Dist2(w.p[j].pos, x.P) <= 56.25f &&
          Ad(x.dir, PBrg(x.P, w.p[j].pos)) <= 70.0f)
        skip18 = true;
  // 18. ponytail: 5a40a0 -> 5a3080 (partial in -b4 19) is not ported.
  (void)skip18;
  // 19. ponytail: 5a1cc0 and the carrier-action stop need the PES path/action
  // records; 694320 needs 5a8290/6940f0 (not decoded): all taken as false.
  if (counter || (!L68efc0(x) && (pinned || (!L68e900(x) && !L68ec50(x))))) {
    if (x.tt <= 3.0f * kPesFps || !L68d920(x)) {
      if (!pinned && (L68b070(x) || L692c50(x) || L68b920(x) || L691ef0(x) ||
                      (!counter && x.tt > Frames(2.0f) && L692fe0(x))))
        return true;
      // 68b1a0 (FUN_14068b1a0): the crowded-mate stop, past fps/2 frames.
      if (!pinned && x.tt > Frames(0.5f) && CrowdedMateStop(w, id)) return true;
      // 68b6e0 the ball-sorted frame list; 693340 the 643a30 grid read;
      // 694bc0 instruction 6: all false.
      const bool anyKeep = L690b90(x) || L690630(x) || L690400(x) || L690cf0(x) ||
                           L694650(x) || L693a20(x) || L693730(x, intercept) ||
                           L68e660(x, intercept) || L68b440(x, intercept) || L68c050(x) ||
                           L692ea0(x);
      if (!anyKeep) {
        // 691d10 (SecondLine sub 8) and 68fa10 (T+0x87cc) are not reached.
        if (L691a80(x)) return true;
        if (!L68d0b0(x, pinned)) {
          // 691300 (sub 2) not reached; 68aeb0 releases the run when the runner
          // beats the receiver's lead to his own route point.
          if (L690960(x)) return true;
          if (x.tt > Frames(2.0f) && PassToMeStop(w, id)) return true;
          if (L68ac40(x, x.me.aim, true) || L693ef0(x)) return true;
          // 68c1a0 (controlled path), 68baa0 (skill cards), 6910c0 (cls 0x19)
          // are not reached.
          if (!(L68d730(x) || L68fc70(x) || L68cde0(x) || L68cc40(x))) return false;
        }
      }
    }
  }
  // 20.
  return true;
}

}  // namespace

// --- FUN_140654e60, the time-to-reach (raw 0x140654e60-0x14065565c) ---
//
// PES steps the player toward the target two frames at a time. The per-frame
// distance starts at sin((dev+90)/2)·speed·align - the heading factor
// FUN_140a92c80 - and climbs across the speed tiers
// 100/(fps·{12.9, 22.5, 32.7, 72}) m/frame; while the player has not turned
// far enough toward the target (4.5417 deg a frame = 2·int(fps+0.5)/48·2) his
// distance gain is capped by the tier he is in. The loop ends when the walked
// distance reaches the target or 20 s pass, and the answer adds the turn
// penalty int(int(fps·0.25+0.5)·ramp(dev, 90→155)) scaled by the second
// heading ramp and int(fps·0.4+0.5)·u (u = the speed ramp over 11→22 km/h).
constexpr float kTurnDegPerFrame = 2.0f * 109.0f / 48.0f;  // 2·int(fps+0.5)/48·2
constexpr float kReachTier20_s = 20.0f;   // 100/(fps·20) m/frame, the walk tier
constexpr float kReachTier12_9_s = 12.9f;  // 100/(fps·12.9) m/frame, the jog tier
constexpr float kReachTier22_5_s = 22.5f;  // 100/(fps·22.5) m/frame
constexpr float kReachTier32_7_s = 32.7f;  // 100/(fps·32.7) m/frame
constexpr float kReachTier72_s = 72.0f;    // 100/(fps·72) m/frame, the sprint tier
constexpr float kReachWalkFloor = 0.8f;   // 0.8·t20: below it the player cannot turn
constexpr float kReachTurnGate_deg = 55.0f;  // dev <= 55 turns without the accel step
constexpr float kReachAccelFast_s = 1.3f;    // seconds to climb the top two tiers
constexpr float kReachAccelSlow_s = 0.15f;   // seconds to climb the bottom two
constexpr float kReachMaxFrames_s = 20.0f;   // the loop's 20 s ceiling
constexpr float kReachTurnFrom_deg = 90.0f;  // the turn penalty starts at 90 deg
constexpr float kReachTurnSpan_deg = 65.0f;  // ... and saturates 65 deg later
constexpr float kReachTurnScale_s = 0.25f;   // int(fps·0.25+0.5) frames at full
constexpr float kReachAlignFrom_deg = 65.0f; // the second heading ramp's start
constexpr float kReachAlignTo_deg = 120.0f;  // ... and its end
constexpr float kReachAlignScale_s = 0.4f;   // int(fps·0.4+0.5) frames at full
constexpr float kReachSpeedFrom_kmh = 11.0f; // the speed ramp 11 -> 22 km/h
constexpr float kReachSpeedTo_kmh = 22.0f;
constexpr float kReachAlignNear_deg = 35.0f; // 30 + 35·(1-u): the full-alignment band
constexpr float kReachAlignFar_deg = 25.0f;  // ... and the dead band past it

// FUN_140a92c80(x, scale): sign·scale·sin(fold180(x)/2), the 512-step table at
// 0x142655950 (its values are sin(i·90/512 deg); the install test reads the
// table's ends and midpoint).
float HeadingFactor(float deg, float scale) {
  const float folded = Fold180(deg);
  const float v = std::sin(folded * 0.5f * kRad);
  return folded < 0.0f ? -scale * v : scale * v;
}

// The owner's NearestList is his own team's S1 row (pesscorerows.hpp), in the copy BuildRunWorld took: the
// opponents' block, nearest first. A row's lanes map to run-world ids: lanes 0..10 are the writer's own
// team, 11..21 the other team.
namespace {
int LaneOf(int t, int id) { return TeamOf(id) == t ? id % kSlots : kSlots + id % kSlots; }
int GlobalOf(int t, int lane) {
  return lane < kSlots ? t * kSlots + lane : (1 - t) * kSlots + (lane - kSlots);
}
}  // namespace

std::vector<NearOpponent> NearestOpponents(const RunWorld& w, int id) {
  const int t = TeamOf(id);
  std::vector<NearOpponent> out;
  if (w.nearestRows[t] == nullptr) return out;
  const pes::SortedRow& row = w.nearestRows[t]->nearest[LaneOf(t, id)];
  out.reserve(row.Count(1));
  for (int r = 0; r < row.Count(1); ++r) {
    const pes::RowEntry& e = row.Entry(1, r);
    out.push_back({GlobalOf(t, e.id), e.value});
  }
  return out;
}

// FUN_140655660 + FUN_140654e60: the frames for player `id` to reach `X`.
// ponytail: FUN_140a75740 (the path) is the straight segment - GF has no
// obstacle path, so the waypoint list is the destination itself.
int FramesToReach(const RunWorld& w, int id, Vec2f X) {
  const PlayerFacts& p = w.p[id];
  const float speed = Len(p.vel);                      // metres per frame
  const float facing = p.facing;
  const float need = Bearing(p.pos, X);                // FUN_140439e50
  const float dev = std::fabs(Ad(facing, need));
  const float heading = PVec(p.vel);
  // u: 0 at 11 km/h, 1 at 22 km/h (0x140655033: 3.6·speed against 11 and 22).
  const float kmh = speed * kPesFps * 3.6f;
  const float u = Ramp(kmh, kReachSpeedFrom_kmh, kReachSpeedTo_kmh);
  const float near = kReachAlignNear_deg + 35.0f * (1.0f - u);
  const float far = near + kReachAlignFar_deg;
  const float alignRamp = dev <= near ? 1.0f : dev >= far ? 0.0f : (far - dev) / (far - near);
  const float perFrame = HeadingFactor(dev + 90.0f, 1.0f) * speed * alignRamp;
  const float t20 = Per100(kReachTier20_s) / kPesFps;
  const float t129 = Per100(kReachTier12_9_s) / kPesFps;
  const float t225 = Per100(kReachTier22_5_s) / kPesFps;
  const float t327 = Per100(kReachTier32_7_s) / kPesFps;
  const float t72 = Per100(kReachTier72_s) / kPesFps;
  const float dist = std::sqrt(Dist2(p.pos, X));
  float gain = perFrame, turned = 0.0f, walked = 0.0f;
  int frames = 0;
  while (walked < dist && frames <= static_cast<int>(Frames(kReachMaxFrames_s))) {
    if (!(turned < dev) || (t20 * kReachWalkFloor > gain) || dev <= kReachTurnGate_deg) {
      float span = 0.0f, sec = kReachAccelSlow_s;
      if (gain >= t225) {
        span = t20 - t225;
        sec = kReachAccelFast_s;
      } else if (gain >= t327) {
        span = t225 - t327;
      } else {
        span = t327 - t72;
      }
      if (t129 > gain) {
        const int n = std::max(1, static_cast<int>(kPesFps * sec + 0.5f));
        gain += 2.0f * span / static_cast<float>(n);
      }
    }
    if (dev > turned) turned += kTurnDegPerFrame;
    walked += 2.0f * gain;
    frames += 2;
  }
  if (frames > static_cast<int>(Frames(kReachMaxFrames_s)))
    frames = static_cast<int>(dist / t129);
  // The turn penalty and the second heading ramp (0x1406554b1-0x1406555ea).
  const float turnRamp = dev <= kReachTurnFrom_deg ? 0.0f
                        : dev >= kReachTurnFrom_deg + kReachTurnSpan_deg ? 1.0f
                        : (dev - kReachTurnFrom_deg) / kReachTurnSpan_deg;
  const int turnPenalty =
      static_cast<int>(Frames(kReachTurnScale_s) * turnRamp);
  const float dev2 = std::fabs(Ad(heading, need));
  const float second = dev2 >= kReachAlignFrom_deg
                           ? Ramp(dev2, kReachAlignTo_deg, kReachAlignFrom_deg)
                           : 1.0f;
  int scale = static_cast<int>(Frames(kReachAlignScale_s) * u);
  if (p.recordByte1 == kHalvedReachRecordByte)
    scale = static_cast<int>(static_cast<float>(scale) * 0.5f);  // rec[1] == 3
  return frames + static_cast<int>(turnPenalty * second + static_cast<float>(scale));
}

// FUN_14068b1a0: the crowded-mate stop, reached at step 19 with t > fps/2.
bool CrowdedMateStop(const RunWorld& w, int id) {
  const Ctx x(w, id);
  if (x.me.aiClass != 2) return false;                // FUN_140a580a0 != 2
  // a4c9a0(id, -7): the runner at least 7 m behind the opponents' line.
  if (!A720(w.lineX[x.opp], x.P.x, x.s, -7.0f)) return false;
  // Within 20 m of the ball-line point (MA+0x2482c/0x24834: the ball at the
  // snap). ponytail: GF's snap is the current ball position.
  if (Dist2(x.P, w.ball) >= 400.0f) return false;
  for (int j = x.t * kSlots; j < x.t * kSlots + kSlots; ++j) {
    if (j == id || !w.p[j].active) continue;
    if (!IsRun(w.p[j].state)) continue;               // FUN_1405a4830
    const bool chanceMate = w.p[j].state == kChance && w.p[j].aiClass == 1;
    if (chanceMate) continue;
    if (Dist2(x.P, w.p[j].pos) < 144.0f) return true;
  }
  return false;
}

// FUN_14068aeb0: the pass-to-me stop, reached at step 19 with t > 2·fps.
bool PassToMeStop(const RunWorld& w, int id) {
  if (!EvKick(w.ev)) return false;                     // a3e190 kind not a kick
  if (w.ev.target == id) return false;                 // a62e40 == id
  const int t = TeamOf(id);
  const int q = w.receiver[t];                         // FUN_1405969b0
  if (!Valid(q) || q == id) return false;
  // FUN_140a580a0: an SB or a 0x13-class runner never keeps this way.
  if (w.p[id].aiClass == 2 || w.p[id].aiClass == 0x13) return false;
  const float s = w.s[t];
  // a4c9a0(id, -4): at least 4 m behind the opponents' line, and not more than
  // 1.2 m past it (a4c9a0(id, -1.2)).
  if (!A720(w.lineX[t ^ 1], w.p[id].pos.x, s, -4.0f)) return false;
  if (A720(w.lineX[t ^ 1], w.p[id].pos.x, s, -1.2f)) return false;
  if (!OppNear(w, id, 5.0f)) return false;             // a4e250(id, 5)
  // FUN_140653480: the receiver's lead, from his distance to the snap (the
  // ball): 196/676 m² thresholds, 0.28/0.35/0.38 s (0x1425c4c18/0x1425cb48c).
  const float d2 = Dist2(w.p[q].pos, w.ball);
  const float sec = d2 < 196.0f ? 0.28f : d2 < 676.0f ? 0.35f : 0.38f;
  const int lead = static_cast<int>(kPesFps * sec + 0.5f);
  // FUN_140655660 on the runner's own route point (FUN_140596c30).
  const int mine = FramesToReach(w, id, w.p[id].aim);
  return lead >= mine + static_cast<int>(kPesFps * 0.15f + 0.5f);
}

// Step 2 of the manager tick (off-ball 1.2): the per-slot re-run block.
//
// CORRECTION, recorded rather than quietly kept: the report is explicit that
// `A[s]` is read by the **SecondLine** mask only (off-ball 2.3, b4a.c:1204), and
// a raw walk of the Chance mask FUN_140606cd0 (0x140606cd0-0x140607a60) finds no
// read of ctx+0x38 - the single `0x38` in its body is a stack slot. So the block
// is NOT a Chance gate in PES. It was wired into Chance here because it moved
// the run count toward PES, which is exactly the wrong reason to extend a gate.
// It stays as a SecondLine-shaped predicate, tested, and unused by the Chance
// path until SecondLine itself is ported.
bool RunCooldownBlocks(const RunWorld& w, int id) { return w.p[id].runCooldown > 0; }

// FUN_140608ac0 (raw 0x140608ac0-0x140608b33), the just-played-pass-to-me lock.
// The raw: the last ball event must be kind 1, its recorded target must be this
// slot, its age must be within int(fps·0.5+0.5) frames, and the slot's team-concept
// entry (FUN_140a64030(param_2, 0x12, ...)) must be >= 1; an entry of exactly 2
// resolves through the concept-0 test (FUN_140a41b30), which needs team concepts
// GF does not carry.
PassToMeResult PassToMeLock(const RunWorld& w, int id) {
  const int t = TeamOf(id);
  const int slot = id - t * kSlots;
  if (!w.ev.valid || w.ev.kind != 1) return kPassToMeNo;
  if (w.ev.target != id) return kPassToMeNo;
  // int(fps·0.5+0.5) = 28 frames (0x142565600).
  if (w.ev.age > Frames(0.5f)) return kPassToMeNo;
  // FUN_140a64030(id, 0x12): the slot's team-concept entry. ponytail: GF carries
  // no team concepts at all (teamphilosophy.hpp has no concept table), so the
  // entry is unknowable from anything GF has. The three decodable conditions
  // above are the whole of what can be checked honestly, so they decide: a pass
  // to this slot inside half a second is the lock, and the concept term is
  // reported as the reason it cannot be confirmed rather than guessed as false.
  (void)slot;
  (void)t;
  return kPassToMeUnknown;
}

namespace {
// PES's S1 list key for attack sign s: |s·xg − x| (pesscorerows.hpp:72), the float a list entry
// carries (read at 0x1405006DF). Front-first for s means ascending on this key.
float ListKey(float s, float x) { return std::fabs(s * pesFieldHalfLength - x); }
// Block `block` of the S1 list for sign s: its active men in list order (FUN_140A714E0 returns
// list[1] for s == 1 at 0x140A714E0, list[0] otherwise).
std::vector<int> ListBlock(const RunWorld& w, int block, float s) {
  std::vector<int> v;
  for (int i = block * kSlots; i < block * kSlots + kSlots; ++i)
    if (w.p[i].active) v.push_back(i);
  std::stable_sort(v.begin(), v.end(), [&](int a, int b) {
    return ListKey(s, w.p[a].pos.x) < ListKey(s, w.p[b].pos.x);
  });
  return v;
}
// The team's own list, for its own attack sign.
std::vector<int> FrontFirst(const RunWorld& w, int team) { return ListBlock(w, team, w.s[team]); }
int LineRank(const RunWorld& w, int id) {
  const std::vector<int> v = FrontFirst(w, TeamOf(id));
  for (size_t i = 0; i < v.size(); ++i)
    if (v[i] == id) return static_cast<int>(i);
  return kSlots;
}

namespace {
// T+0xa2a0 == 0x3B: the offside-trap run state. FUN_140A61CF0 (0x140A61D16) scans a side's 11 slots for it.
constexpr int kOffsideTrapState = 0x3B;
// FUN_1404F6720's constants, each read at the VA in its comment (see the report, §2).
constexpr float kTrapSweepRadius_m = 8.0f;     // 0x14259BDF4: the first closing sweep's outer radius
constexpr float kTrapSweepCone_deg = 135.0f;   // 0x14259BE60: its cone
constexpr float kTrapLineBehind_m = 10.0f;     // 0x142588210: the carrier's distance behind the trapping line
constexpr float kTrapMarkedD2_m2 = 3.0f;       // 0x14258C264: a man within 3 m^2 of another is in contact
constexpr float kTrapDesignatedD2_m2 = 20.0f;  // 0x14259BE1C: the designated man's d2 from the carrier
constexpr float kTrapDesignatedLine_m = 4.0f;  // 0x1425656C0: the designated man's distance past the line
constexpr float kTrapAimCone_deg = 45.0f;      // 0x14259BE30: the designated man's bearing to the attack
constexpr float kTrapVetoCone_deg = 80.0f;     // 0x142596E6C: a free man in the carrier's forward cone vetoes
constexpr float kTrapArmAngle_deg = 90.0f;     // 0x14259BE5C: the carrier 90 degrees off the attack arms
constexpr float kTrapFinalCone_deg = 20.0f;    // 0x14259BE1C: the final sweep's cone (the same constant)
constexpr int kTrapFinalFrames = 15;           // 0x1404F6BAB: the final sweep's frame byte, 0xF
constexpr float kNoOpponentD2 = -1.0f;         // 0x142588218: FUN_140A4DCC0's answer for a man with no opponent
// 0x14259BDD0 / fps: the speed floor of the carrier's heading (FUN_140458740 at 0x1404F69B0).
constexpr float kCarrierSpeedFloor_mps = 0.6944444179534912f;

// The man nearest to `id` on the other side: the first entry of the other block of his S1 row (FUN_140A4EDF0 with
// k = 0, which FUN_140A4DCC0 reads too). Empty when his row lists nobody.
std::optional<NearOpponent> FirstOpponent(const RunWorld& w, int id) {
  const int t = TeamOf(id);
  const pes::SortedRow& row = w.nearestRows[t]->nearest[LaneOf(t, id)];
  if (row.Count(1) == 0) return std::nullopt;
  const pes::RowEntry& e = row.Entry(1, 0);
  return NearOpponent{GlobalOf(t, e.id), e.value};
}
// FUN_14043B8B0: the squared distance of `target` in `id`'s S1 row, either side. NaN when the row does not list him.
float RowD2(const RunWorld& w, int id, int target) {
  const int t = TeamOf(id);
  return w.nearestRows[t]->nearest[LaneOf(t, id)].ValueOf(LaneOf(t, target));
}
// FUN_140458740 over the floor (FUN_140A6C6D0 squares it against |v|): a man under the floor speed reads his facing.
// ponytail: PES also tests the record's movement state; GF's run world carries no such byte, and a still or slow man
// is the only one under the floor.
float CarrierHeading(const PlayerFacts& p) {
  const float speed2 = p.vel.x * p.vel.x + p.vel.z * p.vel.z;
  if (speed2 < kCarrierSpeedFloor_mps * kCarrierSpeedFloor_mps) return p.facing;
  return PVec(p.vel);
}
}  // namespace

}  // namespace
// FUN_1404F6720 (step 5, 0x1404F6720-0x1404F6BC2): whether team `t`'s offside-trap flag T+0xE4 is set this tick. The
// gates run in PES's order and each cites its VA. They cannot all pass: 0x1404F68E4 (10 m) and 0x1404F6A5B (4 m) differ
// by (D.x - c.x) * s, so both need |D.x - c.x| > 6 m, while 0x1404F6A45 needs d2(c, D) < 20, which caps |D.x - c.x|
// below 4.5 m. So the flag stays clear on every input, and the gates after the pair are kept for the port's fidelity.
bool OffsideTrapArmed(const RunWorld& w, int t) {
  // 0x1404F6785-0x1404F6793: the sheet check FUN_1408C5000 (byte [T+0x8E08+0x1464] = T+0xA26C) and the concept gate
  // FUN_140CA1A50(0x12), which is constant 1 (MOV AL,1). ponytail: GF keeps no sheet-valid byte; the designation
  // publisher runs before play, so the sheet gate reads set.
  // 0x1404F6799: MI+0x116C == 1, open play.
  if (!w.openPlay) return false;
  // The S1 rows are the team AIs' writer output; without them there are no nearest men.
  if (w.nearestRows[0] == nullptr || w.nearestRows[1] == nullptr) return false;
  // FUN_140A61CF0 (0x1404F67B0): no man of the trapping side is already in the trap state.
  for (int k = t * kSlots; k < t * kSlots + kSlots; ++k)
    if (w.p[k].state == kOffsideTrapState) return false;
  // The carrier, FUN_140A627E0 (0x1404F67CE), is a slot 0..21 on the other side (0x1404F67D5-0x1404F67EE).
  const int c = w.carrier;
  if (c < 0 || c >= 2 * kSlots) return false;
  const int cs = c / kSlots;
  if (cs == t) return false;
  const PlayerFacts& me = w.p[c];
  const float sT = w.s[t];   // T+0x254 of the trapping side (0x1404F69E7)
  const float sC = w.s[cs];  // T+0x254 of the carrier's side (0x1404F68BF)
  // D, the carrier side's designated man: the first of that side's block in the S1 list for its sign (FUN_140A714E0
  // at 0x1404F683E, then dword [list + 0x58·cs] at 0x1404F695E).
  const std::vector<int> block = ListBlock(w, cs, sC);
  if (block.empty()) return false;
  const int D = block.front();
  if (D == c) return false;  // 0x1404F684D
  const PlayerFacts& dm = w.p[D];
  // The carrier side's attack bearing, T+0x250 (0x1404F6865, pes::AttackAngleDeg).
  const float attackDeg = pes::AttackAngleDeg(sC);
  // The first closing sweep (FUN_140A4E720 at 0x1404F6896): an opponent 0 to 8 m out in the 135-degree forward cone.
  // Both sweeps pass the on-pitch byte as 1 (0x1404F6896, 0x1404F6BB7), a test that GF's active flag carries.
  if (ConeOpp(w, c, attackDeg, kTrapSweepCone_deg, kTrapSweepRadius_m) == 0) return false;
  // ponytail: 0x1404F68EA reads G+0x2AA8+0x18, a pitch-table float (not decoded; it is not the ball). GF has no entry
  // for it, so this gate passes; the chain stays unarmed through the gates named in the header either way.
  // Q, the trapping side's man nearest the carrier (FUN_140A4EDF0 at 0x1404F6917), within 3 m^2 of him (FUN_14043B8B0
  // at 0x1404F6936; JNC at 0x1404F6948).
  const std::optional<NearOpponent> q = FirstOpponent(w, c);
  if (!q || q->d2 >= kTrapMarkedD2_m2) return false;
  // The team lines: 0x1404F68CB (MA+0x40+0x14·t) and 0x1404F6A0B (MA+0x38+0x14·cs). ponytail: GF's RunWorld::lineX
  // stands in for both; its documented offset (+0x44) is not checked against the raw reads.
  const float lineT = w.lineX[t];
  const float lineC = w.lineX[cs];
  // 0x1404F68E4: the carrier is more than 10 m behind the trapping line, on the carrier side's attack.
  if ((lineT - me.pos.x) * sC <= kTrapLineBehind_m) return false;
  // 0x1404F6A04-0x1404F6A12: the trapping line is not past the carrier side's line, on the trapping side's attack.
  if (sT * lineT > sT * lineC) return false;
  // 0x1404F6A1B-0x1404F6A3F: the designated man is on his side of the centre line.
  if (0.0f > dm.pos.x * sC) return false;
  // 0x1404F6A45: the designated man is within 20 m^2 of the carrier (his d2 in the carrier's own row).
  const float dCD = RowD2(w, c, D);
  if (dCD >= kTrapDesignatedD2_m2) return false;
  // 0x1404F6A53-0x1404F6A5B: the designated man is not 4 m or more past the trapping line.
  if ((lineT - dm.pos.x) * sC >= kTrapDesignatedLine_m) return false;
  // The carrier's bearing to D: M[c][D] (0x1404F69D2, FUN_140439FE0). ponytail: the live bearing stands in for the
  // pair table, as pescarrier.cpp does. It is within 45 degrees of the attack bearing (0x1404F6A7D, 0x1404F6A98).
  const float bearingCD = PBrg(me.pos, dm.pos);
  if (Ad(attackDeg, bearingCD) > kTrapAimCone_deg) return false;
  // The veto (0x1404F6AD0-0x1404F6B37): a carrier-side man other than D, on the pitch, whose nearest opponent is 3 m^2
  // or more away (FUN_140A4DCC0, JC at 0x1404F6B13), and who lies inside 80 degrees of the carrier's heading.
  const float heading = CarrierHeading(me);
  for (int slot = cs * kSlots; slot < cs * kSlots + kSlots; ++slot) {
    if (!w.p[slot].active || slot == D) continue;
    const std::optional<NearOpponent> near = FirstOpponent(w, slot);
    const float nearD2 = near ? near->d2 : kNoOpponentD2;
    if (!(nearD2 >= kTrapMarkedD2_m2)) continue;
    if (Ad(heading, PBrg(me.pos, w.p[slot].pos)) <= kTrapVetoCone_deg) return false;
  }
  // 0x1404F6B4C-0x1404F6B83: the carrier faces 90 degrees or more off the attack bearing: armed.
  if (!(Ad(attackDeg, me.facing) < kTrapArmAngle_deg)) return true;
  // 0x1404F6B85-0x1404F6BB9: otherwise the final sweep toward D (20 degrees, 15 frames, radius d2) must be empty.
  return ConeOpp(w, c, bearingCD, kTrapFinalCone_deg, dCD, kTrapFinalFrames) == 0;
}

namespace {
// a4f230 (FUN_140A4F230): attackers of the team in possession ahead of the opponents' line.
int AttackersAhead(const RunWorld& w) {
  const int t = w.possession[0] ? 0 : 1;
  const float s = w.s[t];
  // ponytail: PES's fallback is the holder MA+0x2483C (0x140A4F2A3), not the receiver. RunWorld
  // has no holder and BuildRunWorld is the team AI's (not this slice).
  const int owner = Valid(w.carrier) ? w.carrier : w.receiver[t];
  // The line is the opponents' block (0x140A4F2CB-0x140A4F30C reads block 1 - t) for sign s.
  float line = 0.0f;
  for (int i : ListBlock(w, 1 - t, s))
    if (s * w.p[i].pos.x <= (w.ball.x - s * kAheadBehindBall_m) * s) {
      line = w.p[i].pos.x;
      break;
    }
  const float back = w.ball.x - s * kAheadBackClamp_m;
  if (s * line <= back * s) line = back;
  int n = 0;
  for (int i = t * kSlots; i < t * kSlots + kSlots; ++i) {
    if (!w.p[i].active || i == owner) continue;
    const float x = w.p[i].pos.x;
    if (((w.ball.x - s * kAheadWindowFront_m) * s < s * x || s * line < s * x) &&
        (w.ball.x - s * kAheadWindowBack_m) * s < s * x &&
        std::fabs(w.p[i].pos.z - w.ball.z) <= kAheadLateral_m)
      ++n;
  }
  return n;
}
// h1 #23 FUN_1405a5240: the team's focus player is free and faces forward.
bool ReceiverFreeAndFacing(const RunWorld& w, int t) {
  const int id = w.receiver[t];
  if (!Valid(id)) return false;
  if (OppNear(w, id, 1.0f)) return false;
  const float dir = TDir(w, t);
  if (ConeOpp(w, id, dir, 120.0f, 4.5f)) return false;
  return Ad(PVec(w.p[id].vel), dir) < 60.0f;
}
// FUN_1405938a0(team, 0x7e) with FUN_140593290 (decompiled for this port):
// the team may start Chance runs at all.
}  // namespace

// FUN_1405938a0 (raw 0x1405938a0-0x140593930), the Chance mask's team head,
// called once per FUN_140606cd0 before that function's own 11-slot loop. The
// caller therefore evaluates it once per tick, and passes the result down.
//
// Terms GF cannot model are named and taken false: instruction 6 (a63950), the
// holder flags (a50250), FUN_1405a5240, FUN_1404c8e60's facing test, and
// T+0xa508 == 2.
MaskReject TeamChanceMayRunReject(const RunWorld& w, int team) {
  if (!w.possession[team]) return kMaskTeamMayNotRun;
  const float s = w.s[team];
  // FUN_140593290: a carrier on the team, else its focus player within 625 m²
  // (0x1425ab4c4) of the ball and the last kick at least 4 frames old.
  if (Valid(w.carrier)) {
    if (TeamOf(w.carrier) != team) return kMaskTeamMayNotRun;
  } else {
    const int q = w.receiver[team];
    if (!Valid(q) || Dist2(w.p[q].pos, w.ball) > k593290Focus_m2) return kMaskTeamMayNotRun;
    if (w.ev.valid && w.ev.age < Frames(4.0f)) return kMaskTeamMayNotRun;
  }
  // a4c930(receiver, ball.x): the ball in the team's own half. Note the raw
  // reads MA+0x2482c's x (a4f1e0's band is over `sign * pt.x` against -28/22),
  // *not* a z at the snap - the port's earlier comment was mislabelled.
  const bool ownHalf = s > 0.0f ? w.ball.x <= 0.0f : w.ball.x >= 0.0f;
  constexpr float kOwnHalfLimit_m = -15.0f;  // 0x14259d668
  const bool open = ReceiverFreeAndFacing(w, team) || !ownHalf ||
                    (s * w.ball.x >= kOwnHalfLimit_m && w.possessionAge[team] >= Frames(4.0f));
  if (!open) return kMaskTeamMayNotRun;
  // The end-band test: `s·MA+0x2482c.x <= MA+0x2aa8 - 10` (0x142588210), where
  // MA+0x2aa8's entry is the pitch half length (F0).
  constexpr float kEndBand_m = 10.0f;  // 0x142588210
  if (s * w.ball.x > pesFieldHalfLength - kEndBand_m && std::fabs(w.ball.z) > pesPenaltyAreaHalfWidth) return kMaskTeamMayNotRun;
  return kMaskPass;
}
bool TeamChanceMayRun(const RunWorld& w, int team) {
  return TeamChanceMayRunReject(w, team) == kMaskPass;
}

// FUN_140606cd0's rank > 1 angle sub-test, raw 0x140607741-0x140607814. The
// rank byte is the attacking team's line-list position of the runner
// (FUN_140a714e0's +0xb2); the cone is 80 deg about the attack direction from
// the runner, and the 20 m window is measured against the ball-line point
// (MA+0x2482c/0x24834). GF carries no instruction 2 (FUN_1405aaf00 = 0), so the
// closing a4c720's arguments are a = 0 and k = 0: the ball-line point must be
// in the attacking half.
bool RankAngleOk(const RunWorld& w, int id, int attackLevel) {
  const Ctx x(w, id);
  // The gate chain: the runner on the mask's generic path is already past the
  // line and the slot mask. The angle test needs the reach path taken
  // (attackers ahead > 3, or a forward without the LB style bit) - which
  // ChanceMaskBit has already decided - so it is entered whenever the attacker
  // count clears it.
  const int ahead = AttackersAhead(w);
  const bool styleLb = StyleLb(w, x.me);
  if (!(ahead > 3 || (x.me.roleClass < pes::roles::kReachClassBelow && !styleLb))) return true;
  // The rank byte: the attacking team's line-list position of this runner.
  if (LineRank(w, id) <= 1) return true;
  // ponytail: FUN_1405a0d20 (the "go straight for the line" duel test) and
  // FUN_140a64030(id, 9) (style bit 9: none in GF) are taken false, so the
  // style-bit branch of the sub-test is always entered.
  // The angle branch takes class >= 4, the other side of the reach gate's `class < 4`. PES's veto
  // (run2.c:642) lists that class test as a disjunct instead; that order is not re-derived here.
  if (x.me.roleClass >= pes::roles::kReachClassBelow) {
    // 0x140607747: the bearing from the ball-line point to the runner, and the
    // runner's facing must agree within 45 deg.
    const float need = Bearing(w.ball, x.P);
    if (std::fabs(Fold180(x.me.facing - need)) >= 45.0f) return true;  // 0x14259be30
    // 0x1406077d: the cone's centre is the attack direction and its radius is
    // 30 m (xmm14, 0x142591140).
    constexpr float kRankCone_deg = 80.0f;  // 0x142596e6c
    constexpr float kRankConeRadius_m = 30.0f;  // 0x142591140
    constexpr float kRankWindow_m2 = 400.0f;  // 0x1425a2670
    if (ConeOpp(w, id, x.dir, kRankCone_deg, kRankConeRadius_m)) return true;
    // 0x1406077be: past 20 m from the ball-line point the runner is rejected,
    // as is a line point that is not ahead in the attack frame.
    if (Dist2(x.P, w.ball) >= kRankWindow_m2) return false;
  }
  // 0x1406077e4-0x140607814: a4c720(a, MA+0x2482c, s, k) with GF's a = 0 and
  // k = 0 (no instruction 2) rejects when the ball-line point is not ahead in
  // the attack frame.
  (void)attackLevel;
  return w.ball.x * x.s > 0.0f;
}

uint32_t AttackSlotMask(const RunWorld& w, int team, int L) {
  constexpr uint32_t kAll = (1u << kSlots) - 1;
  if (L >= kQuotaAllFromLevel) return kAll;
  // ponytail: the 0 < L < 4 FUN_1405aaf00 branch (class > 1 admitted, no quota) needs instruction 2
  // (none): RE-7, Q4.
  const int need = kQuotaNeed[std::max(L, 0)];
  int ahead = AttackersAhead(w);
  const float s = w.s[team];
  int beyond = 0;
  for (int i : FrontFirst(w, team)) {
    if (s * w.p[i].pos.x < pesPenaltyAreaLineX) break;
    ++beyond;
  }
  ahead = std::max(ahead, beyond);
  if (ahead < need) return kAll;
  // The quota list (0x140500600-0x1405007D2): forwards (class >= 7) are admitted at once with
  // kForwardStampScore and take places like the rest. Each other man is listed by ListKey (the S1
  // list float, 0x1405006DF), less kQuotaNearBonus_m within kQuotaNearEdge_m of the ball and nothing
  // from kQuotaFarEdge_m out, and kQuotaSideTerm_m on the receiver's side while he turns. The `need`
  // lowest take the slots (0x1405007D8-0x14050088E).
  struct E { int slot; float v; };
  std::vector<E> e;
  uint32_t mask = 0;
  const int q = w.receiver[team];
  bool turning = false;
  float side = 1.0f;
  if (Valid(q)) {
    const PlayerFacts& r = w.p[q];
    // ponytail: GF's velocity-against-facing test; PES's is |team+0x250 - receiver heading| > 15
    // (0x1405004F0; RE-9, Q3).
    if (Ad(PVec(r.vel), r.facing) > kTurnThreshold_deg) {
      turning = true;
      side = r.aim.z - r.pos.z < 0.0f ? -1.0f : 1.0f;
    }
  }
  for (int i = team * kSlots; i < team * kSlots + kSlots; ++i) {
    if (!w.p[i].active) continue;
    const int slot = i - team * kSlots;
    if (w.p[i].roleClass >= pes::roles::kAlwaysAdmitClass) {
      e.push_back({slot, kForwardStampScore});
      mask |= 1u << slot;
      continue;
    }
    float v = ListKey(s, w.p[i].pos.x);
    const float d = std::sqrt(Dist2(w.p[i].pos, w.ball));
    const float near = d <= kQuotaNearEdge_m ? 1.0f
                       : d < kQuotaFarEdge_m ? (d - kQuotaFarEdge_m) / kQuotaFalloff_m
                                             : 0.0f;
    v -= near * kQuotaNearBonus_m;
    if (turning && Valid(q) && (w.p[i].pos.z - w.p[q].pos.z < 0.0f ? -1.0f : 1.0f) == side)
      v += kQuotaSideTerm_m;
    e.push_back({slot, v});
  }
  std::stable_sort(e.begin(), e.end(), [](const E& a, const E& b) { return a.v < b.v; });
  for (int i = 0; i < need && i < static_cast<int>(e.size()); ++i) mask |= 1u << e[i].slot;
  return mask;
}

bool ChanceMaskBit(const OffBallConstants& k, const RunWorld& w, int id, uint32_t slotMask,
                   int attackLevel, bool buildUpLong, float awareness, bool teamMayRun) {
  return ChanceMaskReject(k, w, id, slotMask, attackLevel, buildUpLong, awareness, teamMayRun) ==
         kMaskPass;
}

// FUN_140606cd0's gates in walk order, naming the one that rejected `id`.
MaskReject ChanceMaskReject(const OffBallConstants& k, const RunWorld& w, int id, uint32_t slotMask,
                            int attackLevel, bool buildUpLong, float awareness, bool teamMayRun) {
  const Ctx x(w, id);
  const int slot = id - x.t * kSlots;
  const PlayerFacts& me = x.me;
  // Pre-gates: rank (or OVERLAP_SB) and FUN_140593430.
  if (!(MayTake(kChance, me.state) || me.state == kOverlap)) return kMaskRankOrState;
  // FUN_140593430's head (raw 0x140593430-0x1405934d4): the slot may not be
  // the carrier, and when the carrier is invalid it may not be the holder
  // fallback either (MA+0x2483c) - the raw tests them in that order, so a valid
  // carrier does NOT also veto the fallback player. The keeper is excluded by
  // the T+0x4c00 != 0 guard that wraps 593430's admitting block.
  if (id == w.carrier) return kMaskCarrierOrReceiver;
  if (!Valid(w.carrier) && id == w.receiver[x.t]) return kMaskCarrierOrReceiver;
  if (me.roleClass == pes::roles::kKeeperClass) return kMaskCarrierOrReceiver;
  if (IsRun(me.state)) return kMaskAlreadyRunning;  // 593430: a running slot only from PULL_AWAY
  // 593430 (raw 0x1405935c4: k = -0.1 @0x14259d648): not already on the line.
  if (Beyond2(w, id, -0.1f)) return kMaskOnTheLine;
  // Inside the box zone only with an opponent within 6.5 m (raw 0x14059373d).
  if (!(x.s * x.P.x <= pesPenaltyAreaLineX - 3.0f || std::fabs(x.P.z) >= pesGoalAreaHalfWidth + 1.5f || OppNear(w, id, 6.5f)))
    return kMaskBoxZone;
  if (x.s * x.P.x > pesGoalAreaLineX) return kMaskGoalArea;
  if (!L596240(x)) return kMaskRoleClass;
  // ponytail: 5a7d10 (carrier pass-aim records) is not applied; the special
  // lanes 60a750/608570/87a0/9350/8ac0 are not ported.
  // Generic lane: the attack-slot mask and the depth limit.
  if (me.state == kOverlap || !((slotMask >> slot) & 1u)) return kMaskSlotMask;
  if (x.s * x.P.x > pesPenaltySpotX) return kMaskDepth;
  // FUN_1405938a0 is the mask's *team head*: the raw calls it once, before this
  // function's 11-slot loop (OBS mask.c:86, loop at mask.c:197-204), so its
  // verdict is shared by every slot. `teamMayRun` is the caller's result.
  if (!teamMayRun) return kMaskTeamMayNotRun;
  const int rank = LineRank(w, id);
  // ponytail: MA+0x68/+0x60 team ints unknown; taken 0 (this gate passes).
  const int cls = me.roleClass;
  // b8a3 = attack level < 1 skips positions 0..4 (counter-trigger 128).
  if (attackLevel < 1 && cls <= pes::roles::kAttackSkipClassMax) return kMaskAttackLevel;
  const bool styleLb = StyleLb(w, me);
  const int ahead = AttackersAhead(w);
  const bool reachPath = ahead > 3 || (cls < pes::roles::kReachClassBelow && !styleLb);
  const float dz = std::fabs(x.P.z - x.B.z);
  if (reachPath) {
    float dh = 0.0f;
    if (dz > k.reachDhFrom_m) dh = dz < k.reachDhTo_m ? (dz - k.reachDhTo_m) / k.reachDhDiv_m : 0.0f;
    // Build Up Long (a62180(team, 1) == 0) widens the base 15 -> 21.
    constexpr float kLongBase_m = 21.0f;  // 0x1425c84bc
    float r = (buildUpLong ? kLongBase_m : k.reachBase_m) + dh * k.reachDhGain_m;
    // a50250(2) floor 24, 5a5b20 = 26 (needs style bit 0), 60a100 clamp,
    // aaf00 floor 35, 5a9950 clamp: all off for GF's inputs (no holder flags,
    // no style bits, no instructions, AI class 0).
    float bonus = 0.0f;
    if (buildUpLong && Valid(w.carrier)) {
      if (awareness > k.awareTo)
        bonus = k.awareBonus_m;
      else if (awareness > k.awareFrom)
        bonus = (awareness - k.awareFrom) / k.awareSpan * k.awareBonus_m;
    }
    r = std::min(r + bonus, k.reachCap_m);
    if (r + bonus < std::fabs(x.P.x - x.B.x)) return kMaskReach;
  }
  constexpr float kMaxDz_m = 42.0f;  // 0x1425cd830
  if (dz > kMaxDz_m) return kMaskDz;
  // The rank > 1 angle sub-test (raw 0x140607741-0x140607814): a second-line
  // runner inside 20 m of the ball-line point, heading within 45 deg of the
  // bearing to him and with no opponent in the 80 deg cone, is rejected when
  // that point is not ahead in the attack frame - and likewise every runner
  // once four attackers are ahead of the ball band.
  if (!RankAngleOk(w, id, attackLevel)) return kMaskRankAngle;
  // ponytail: 5a8290 / 5a5400 are not reduced; 5a7d10 (the carrier pass-aim
  // stop) is not applied. The generic path goes to 79ac, where with a carrier
  // 6046b0 (style bit 3: none -> true), 609b40 (carrier valid -> false) and
  // 609e60 (receiver action 0x48 -> false) set the bit.
  (void)rank;
  return Valid(w.carrier) || !Valid(x.q) || Dist2(w.p[x.q].pos, x.B) <= 49.0f ? kMaskPass
                                                                           : kMaskGeneric;
}

namespace {
// FUN_140a51320 (raw 0x140a51320-0x140a5139f): the latest ball event's kind when
// its target is a player on the pitch, else the PES frames since the kick. With
// no event the age is its default, far past one frame.
// ponytail: PES counts whole frames; the age is continuous, so "above one"
// compares the continuous age.
float PassGetFigure(const RunWorld& w) {
  const bool targetOnPitch = w.ev.valid && w.ev.target >= 0;
  return targetOnPitch ? static_cast<float>(w.ev.kind) : w.ev.age;
}
}  // namespace

float StyleRunDepth(const OffBallConstants& k, int roleClass, uint32_t styleWord, int runState,
                    bool carrier) {
  // The carrier and a player in a run action take no term (FUN_1405d8b70: uVar12 !=
  // local_1cc, and FUN_1405a4790's run set, which is IsRun).
  // ponytail: roles 5-6 of L650-653 are gated on team+0x7b84, not decoded; they take no term.
  if (carrier || IsRun(runState) || roleClass < 7 || roleClass > 9) return 0.0f;
  using P = PlayingStyles::Player;
  const bool poacher = pes::style::Gate(k.styleCompat, roleClass, styleWord,
                                        pes::style::EngineBit(P::GoalPoacher));
  const bool fox = pes::style::Gate(k.styleCompat, roleClass, styleWord,
                                    pes::style::EngineBit(P::FoxInTheBox));
  return poacher || fox ? k.styleRunDepth_m : 0.0f;
}

bool RunStop(const RunWorld& w, int id, int run) {
  const PlayerFacts& me = w.p[id];
  // The head PES runs before anything else (0x140607a60; 0x14060b410 and
  // 0x1406118e0 open the same way): pad control, the carrier, the PASS_GET
  // receiver while the pass figure is above one, and only then the run's
  // minimum time. ponytail: gfsim has no pad-driven player (every player runs
  // through ElizaController), so the pad clause stays false.
  if (me.humanControlled) return true;
  // ponytail: PES stops the carrier only when his action byte (player record
  // +1) lacks action-property bit 16 (FUN_140a6e340). GF carries no action
  // bytes, so the carrier always stops.
  if (w.carrier == id) return true;
  if (me.passGet && PassGetFigure(w) > 1.0f) return true;
  const float minFrames = run == kSecondLine ? kPesFps : kChanceMinFrames;
  if (me.t < minFrames) return false;
  if (run == kCounter && CommonStop(w, id, run)) return true;
  if (run == kCounter) return w.s[TeamOf(id)] * me.pos.x > pesPenaltySpotX;
  // Chance's AI class 0x13 veto (0x140607a60) is not reached: GF's aiClass is
  // the role class, 0..9.
  return CommonStop(w, id, run);
}

float SupporterCost(float angleDiff_deg, float distance_m) {
  return kSupporterAngleWeight * std::fabs(angleDiff_deg) + distance_m +
         (distance_m < kSupporterClose_m ? kSupporterClosePen : 0.0f);
}

float SupportDistance_m(const SupportDistanceInput& in) {
  if (!in.inPlay) return kSupportDead_m;
  const float s = std::clamp((in.supportRange - 1) / kSupportRangeSpan, 0.0f, 1.0f);
  const float deep = Ramp(in.ballX, in.oppLineX - kSupportLineMargin_m,
                          in.ownLineX + kSupportLineMargin_m);
  const float a = std::fabs(Fold180(in.bearingFromAttack_deg));
  const float fwd = 1.0f - Ramp(a, kSupportFwdFull_deg, kSupportFwdZero_deg);
  const float back = Ramp(180.0f - a, kSupportFwdFull_deg, kSupportFwdZero_deg);
  float d = kSupportBase_m + kSupportRangeGain_m * s + kSupportDeep_m * deep +
            (in.buildUpLong ? kSupportLong_m : 0.0f) * fwd * deep +
            Ramp(in.ballX, pesPenaltyAreaLineX, -kFieldHalfWidthHalf * pesFieldHalfWidth) * back +
            kSupportWide_m *
                Ramp(std::fabs(in.ballZ), pesPenaltyAreaHalfWidth - kSupportWideBand_m,
                     pesPenaltyAreaHalfWidth + kSupportWideBand_m) *
                Ramp(in.ballX, pesPenaltyAreaLineX - kSupportBoxBand_m, pesPenaltyAreaLineX);
  // ponytail: the holder-state min 5 and the f0/f1 (+5 floor 9 / -5) flags are
  // undecoded (O5); the plain floor applies.
  return std::max(d, kSupportFloor_m);
}

}  // namespace pes::offball
