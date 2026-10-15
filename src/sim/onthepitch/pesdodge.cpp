#include "onthepitch/pesdodge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "base/math/bluntmath.hpp"
#include "pes/animetable.hpp"
#include "pes/exe.hpp"
#include "pes/mbinfo.hpp"

namespace pes {
namespace {

// The exe's own 360 wrap (FUN_14043a0a0), for the dodge's angle differences.
float Wrap180Deg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a <= -180.0f) a += 360.0f;
  return a;
}

// Each constant is read through an opcode-checked instruction that references
// it, so a different executable fails the load instead of silently reading the
// wrong numbers (the animetable/mbinfo pattern, animetable.cpp's Imm table).
struct Imm {
  uint64_t va;
  std::array<uint8_t, 4> opcode;
  size_t opcodeLen, offset, width;
};

// FUN_14073a076 / 0x14073a26c: LEA R8,[RIP+disp32] -> 0x14344efb0 (kind table).
constexpr Imm KIND_TABLE = {0x14073a076, {0x4c, 0x8d, 0x05}, 3, 3, 4};
// FUN_14073593e: MOVDQA XMM0,[RIP+disp32] -> 0x1425e6650 (ball grid).
constexpr Imm BALL_GRID = {0x14073593e, {0x66, 0x0f, 0x6f, 0x05}, 4, 4, 4};
// FUN_140a6d383: LEA RCX,[RIP+disp32] -> 0x14264866c (band table).
constexpr Imm BAND_TABLE = {0x140a6d383, {0x48, 0x8d, 0x0d}, 3, 3, 4};
// FUN_140a6e470's LEA RCX,[RIP+disp32] -> 0x1426484f0 (the role table).
constexpr Imm ROLE_BITS = {0x140a6e47b, {0x48, 0x8d, 0x0d}, 3, 3, 4};
// FUN_1407368d0's post grid: the MOVDQA XMM2,[RIP+disp32] at 0x140736a72.
constexpr Imm POST_GRID = {0x140736a72, {0x66, 0x0f, 0x6f, 0x15}, 4, 4, 4};

// FUN_140735870's jump table at 0x1407358b4: kind -> case, and the case's
// MOV EDX imm. The table is 8 entries of a 32-bit offset from the image base.
constexpr uint64_t KIND_COL_JUMP_VA = 0x1407358b4;

// Speed classes (FUN_140736d30): 11 km/h and 22 km/h.
constexpr uint64_t SPEED1_VA = 0x14259bdfc, SPEED2_VA = 0x1425c48c0;
// FUN_140735700's clamp windows, per speed class (lo, hi), and the epsilon.
constexpr uint64_t CLAMP_VA[3][2] = {{0x14259d674, 0x14253ee30},
                                     {0x1425aa1c8, 0x14259be30},
                                     {0x1425cfa50, 0x14259d618}};
constexpr uint64_t EPS_VA = 0x142588200;
// FUN_14073a3d0's distances/angles.
constexpr uint64_t HUMAN_DIST_VA = 0x14259d60c, BEHIND_VA = 0x14259be68, BEARING_VA = 0x14259be30;
// FUN_140736ce0's look-ahead: 12/60 s, drawn per frame at 60 Hz.
constexpr uint64_t LOOKAHEAD_VA = 0x14259be04, FPS_REF_VA = 0x14259a764;
// FUN_14073c5d0's net fast threshold (22 km/h, DAT_1425c48c0) and the column.
// FUN_140739f90's speed-class bounds (13.5 / 22 km/h).
constexpr uint64_t SPEED_CLASS_1_VA = 0x1425a1ffc;
// FUN_1407368d0's angles: the 8 km/h speed-class floor, the 30-deg deep
// splits, the 45-deg near/far gate, the 135-deg column top, and the far
// branch's 60/120 window.
constexpr uint64_t POST_SPEED_FLOOR_VA = 0x14259bdf4, POST_SIDE_VA = 0x142591140,
                   POST_NEAR_VA = 0x14259be30, POST_COL_TOP_VA = 0x14259be60,
                   POST_FAR_LO_VA = 0x14259a764, POST_FAR_HI_VA = 0x14256ae30;
// The ball dodge's eligibility and its two sub-tests (FUN_140a07bf0,
// FUN_140a07570, FUN_140a07880).
constexpr uint64_t GAME_SPEED_VA = 0x14351ae50;           // 54, gsInt (pesstamina.cpp)
// The human gate's window factor, 0.2 (DAT_142598578, FUN_140a0b350).
constexpr uint64_t LAST_EVENT_WINDOW_VA = 0x142598578;
constexpr uint64_t BALL_SPEED_VA = 0x14263f000;           // 0.8, m/s
constexpr uint64_t BALL_REACH_VA = 0x14256ae18;           // 0.01, m per reach unit
constexpr uint64_t BALL_MARGIN_VA = 0x142590d98;          // 0.3, m
constexpr uint64_t BALL_WINDOW_HEADING_VA = 0x142588210;  // 10, deg off the heading
constexpr uint64_t BALL_WINDOW_GOAL_VA = 0x14259be5c;     // 90, deg off the goal axis
constexpr uint64_t GOAL_POST_OFFSET_VA = 0x14259bde8;     // the post past the goal line
constexpr uint64_t GOAL_LOOKAHEAD_VA = 0x14259bdd0;       // the look-ahead seconds
// FUN_140a07e40's own gate (the net dodge).
constexpr uint64_t NET_EDGE_VA = 0x14256ae1c;         // 1, the edge margin
constexpr uint64_t NET_STEP_NUM_VA = 0x14253ee30;      // 100
constexpr uint64_t NET_STEP_DIV_VA = 0x1425cb47c;      // 72
constexpr uint64_t NET_REJECT_VA = 0x14259be0c;        // the residual bearing floor
// FUN_140a084d0's distances (the post dodge's gate).
constexpr uint64_t POST_PROXIMITY_VA = 0x14259222c;  // 5, the distance to the post
constexpr uint64_t POST_RESIDUAL_VA = 0x142565600;   // 0.5, the per-axis admission
constexpr uint64_t POST_BALL_NEAR_VA = 0x142565604;  // 2, the ball near the post

bool ReadImm32(const PeImage& exe, const Imm& imm, int64_t* out) {
  uint8_t code[4] = {};
  if (!exe.ReadVa(imm.va, imm.opcodeLen, code) || std::memcmp(code, imm.opcode.data(), imm.opcodeLen))
    return false;
  uint8_t v[4] = {};
  if (!exe.ReadVa(imm.va + imm.offset, imm.width, v)) return false;
  int32_t x = 0;
  std::memcpy(&x, v, imm.width);
  *out = x;
  return true;
}

// A RIP-relative reference: the displacement at va+opcodeLen, resolved against
// va+opcodeLen+4. Returns the referenced absolute VA.
bool ReadRipTarget(const PeImage& exe, const Imm& imm, uint64_t* target) {
  uint8_t code[4] = {};
  if (!exe.ReadVa(imm.va, imm.opcodeLen, code) || std::memcmp(code, imm.opcode.data(), imm.opcodeLen))
    return false;
  int32_t disp = 0;
  if (!exe.ReadVa(imm.va + imm.offset, 4, reinterpret_cast<uint8_t*>(&disp))) return false;
  *target = imm.va + imm.offset + 4 + uint64_t(int64_t(disp));
  return true;
}

bool ReadF32(const PeImage& exe, uint64_t va, float* out) {
  return exe.ReadVa(va, 4, reinterpret_cast<uint8_t*>(out));
}

// The game-speed divisor is a plain int32 in .data (pesstamina.cpp's read).
bool ReadI32(const PeImage& exe, uint64_t va, int32_t* out) {
  return exe.ReadVa(va, 4, reinterpret_cast<uint8_t*>(out));
}

}  // namespace

std::optional<DodgeConstants> LoadDodgeConstants(const PeImage& exe, std::string* error) {
  DodgeConstants k;
  // The four table addresses, each through its referencing instruction.
  uint64_t kindVa = 0, gridVa = 0, bandVa = 0, postVa = 0;
  if (!ReadRipTarget(exe, KIND_TABLE, &kindVa) || !ReadRipTarget(exe, BALL_GRID, &gridVa) ||
      !ReadRipTarget(exe, BAND_TABLE, &bandVa) ||
      !ReadRipTarget(exe, POST_GRID, &postVa)) {
    *error = "dodge: an exe table reference is not the PES 2021 instruction this was found in";
    return std::nullopt;
  }
  // The kind -> row table: int[8][3].
  for (int kind = 0; kind < 8; ++kind)
    for (int s = 0; s < 3; ++s)
      if (!exe.ReadVa(kindVa + 4 * (kind * 3 + s), 4,
                      reinterpret_cast<uint8_t*>(&k.kindRow[kind][s]))) {
        *error = "dodge: kind table unreadable";
        return std::nullopt;
      }
  // The ball grid: rows 0..3, 3 columns wide, then the row-4 ids at +0x30.
  for (int r = 0; r < 4; ++r)
    for (int s = 0; s < 3; ++s)
      if (!exe.ReadVa(gridVa + 4 * (r * 3 + s), 4, reinterpret_cast<uint8_t*>(&k.ballGrid[r][s]))) {
        *error = "dodge: ball grid unreadable";
        return std::nullopt;
      }
  if (!exe.ReadVa(gridVa + 0x30, 4, reinterpret_cast<uint8_t*>(&k.ballRow4A)) ||
      !exe.ReadVa(gridVa + 0x34, 4, reinterpret_cast<uint8_t*>(&k.ballRow4B)) ||
      !exe.ReadVa(gridVa + 0x38, 4, reinterpret_cast<uint8_t*>(&k.ballRow4C))) {
    *error = "dodge: ball grid row 4 unreadable";
    return std::nullopt;
  }
  // The band table: 12 thresholds.
  for (int i = 0; i < 12; ++i)
    if (!ReadF32(exe, bandVa + 4 * i, &k.ballHeightBands[i])) {
      *error = "dodge: band table unreadable";
      return std::nullopt;
    }
  // FUN_140735870's jump table: 8 case offsets, resolved to a case VA, and the
  // case's MOV EDX imm is the column.
  uint8_t jt[32] = {};
  if (!exe.ReadVa(KIND_COL_JUMP_VA, 32, jt)) {
    *error = "dodge: kind column jump table unreadable";
    return std::nullopt;
  }
  for (int kind = 0; kind < 8; ++kind) {
    uint32_t off = 0;
    std::memcpy(&off, jt + 4 * kind, 4);
    const uint64_t caseVa = 0x140000000 + off;
    // Each case is MOV EDX,imm32 (ba xx xx xx xx).
    uint8_t code[5] = {};
    if (!exe.ReadVa(caseVa, 5, code) || code[0] != 0xba) {
      *error = "dodge: kind column case is not MOV EDX,imm32";
      return std::nullopt;
    }
    int32_t col = 0;
    std::memcpy(&col, code + 1, 4);
    k.kindColumn[kind] = col;
  }
  k.kindColumn[8] = 0xb;  // the default, FUN_140735870's MOV EDX,0xb
  // The net rows: FUN_14073c5d0's 0x310 default / 0x30e fast.
  k.netRowDefault = 0x310;
  k.netRowFast = 0x30e;
  // The post grid: the 8 ints the two MOVDQAs put on the stack (FUN_1407368d0
  // at 0x140736a72 / 0x140736a97). The 3x3's ninth cell is not carried in the
  // table: reading it straight from .rdata would pick up the 8.0f of the next
  // constant, so the selector's own stack value (0x316, written at 0x140736a6a)
  // stands in for it.
  for (int i = 0; i < 8; ++i)
    if (!exe.ReadVa(postVa + 4 * i, 4, reinterpret_cast<uint8_t*>(&k.postGrid[i / 3][i % 3]))) {
      *error = "dodge: post grid unreadable";
      return std::nullopt;
    }
  k.postGrid[2][2] = 0x316;
  if (!ReadF32(exe, POST_SPEED_FLOOR_VA, &k.postSpeedFloor_kmh) ||
      !ReadF32(exe, POST_SIDE_VA, &k.postSideAngle_deg) ||
      !ReadF32(exe, POST_NEAR_VA, &k.postColumnUp_deg) ||
      !ReadF32(exe, POST_COL_TOP_VA, &k.postColumnHigh_deg) ||
      !ReadF32(exe, POST_FAR_LO_VA, &k.postColumnBailout_deg) ||
      !ReadF32(exe, POST_FAR_HI_VA, &k.postBailout_deg)) {
    *error = "dodge: post angles unreadable";
    return std::nullopt;
  }
  // Speed classes and the look-ahead.
  if (!ReadF32(exe, SPEED1_VA, &k.speedClass1_kmh) || !ReadF32(exe, SPEED2_VA, &k.speedClass2_kmh)) {
    *error = "dodge: speed class bounds unreadable";
    return std::nullopt;
  }
  float lookahead = 0, fps = 0;
  if (!ReadF32(exe, LOOKAHEAD_VA, &lookahead) || !ReadF32(exe, FPS_REF_VA, &fps)) {
    *error = "dodge: look-ahead unreadable";
    return std::nullopt;
  }
  k.lookAheadOffset_s = lookahead / fps;
  for (int c = 0; c < 3; ++c)
    for (int i = 0; i < 2; ++i)
      if (!ReadF32(exe, CLAMP_VA[c][i], &k.mirrorClamp[c][i])) {
        *error = "dodge: mirror clamp unreadable";
        return std::nullopt;
      }
  if (!ReadF32(exe, EPS_VA, &k.mirrorEpsilon_deg)) {
    *error = "dodge: mirror epsilon unreadable";
    return std::nullopt;
  }
  if (!ReadF32(exe, HUMAN_DIST_VA, &k.humanDistSq) || !ReadF32(exe, BEHIND_VA, &k.behindAngle_deg) ||
      !ReadF32(exe, BEARING_VA, &k.bearingAngle_deg)) {
    *error = "dodge: human distances unreadable";
    return std::nullopt;
  }
  // The action-class table (0x1426484f0), read through FUN_140a6e470's own
  // LEA so a different executable fails instead of reading the wrong bits.
  uint64_t roleVa = 0;
  if (!ReadRipTarget(exe, ROLE_BITS, &roleVa)) {
    *error = "dodge: role table reference is not the PES 2021 instruction this was found in";
    return std::nullopt;
  }
  for (int i = 0; i < 94; ++i) {
    uint8_t b[4] = {};
    if (!exe.ReadVa(roleVa + 4 * i, 4, b)) {
      *error = "dodge: role table unreadable";
      return std::nullopt;
    }
    std::memcpy(&k.roleBits[i], b, 4);
  }
  if (!ReadI32(exe, GAME_SPEED_VA, &k.lastEventRollDivisor) || k.lastEventRollDivisor < 0) {
    *error = "dodge: game speed divisor unreadable";
    return std::nullopt;
  }
  if (!ReadF32(exe, LAST_EVENT_WINDOW_VA, &k.lastEventWindowFactor)) {
    *error = "dodge: last-event window factor unreadable";
    return std::nullopt;
  }
  k.lastEventFramesPerRoll = 5;  // `lea ecx,[rax+rax*4]` @0x140a07da3
  if (!ReadF32(exe, BALL_SPEED_VA, &k.ballSpeedFloor_mps) ||
      !ReadF32(exe, BALL_REACH_VA, &k.ballReachPerUnit_m) ||
      !ReadF32(exe, BALL_MARGIN_VA, &k.ballReachMargin_m) ||
      !ReadF32(exe, BALL_WINDOW_HEADING_VA, &k.ballWindowHeading_deg) ||
      !ReadF32(exe, BALL_WINDOW_GOAL_VA, &k.ballWindowGoal_deg) ||
      !ReadF32(exe, GOAL_POST_OFFSET_VA, &k.goalMouthPostOffset_m) ||
      !ReadF32(exe, GOAL_LOOKAHEAD_VA, &k.goalMouthLookAhead_s) ||
      !ReadF32(exe, NET_EDGE_VA, &k.netEdgeMargin_m) ||
      !ReadF32(exe, NET_STEP_NUM_VA, &k.netStepNumerator) ||
      !ReadF32(exe, NET_STEP_DIV_VA, &k.netStepDivisor) ||
      !ReadF32(exe, NET_REJECT_VA, &k.netEdgeRejection_deg) ||
      !ReadF32(exe, POST_PROXIMITY_VA, &k.postProximity_m) ||
      !ReadF32(exe, POST_RESIDUAL_VA, &k.postResidualProximity_m) ||
      !ReadF32(exe, POST_BALL_NEAR_VA, &k.postBallNear_m)) {
    *error = "dodge: ball gate constants unreadable";
    return std::nullopt;
  }
  return k;
}

int SpeedClass(float speed_kmh, float headingDelta_deg) {
  // FUN_140736d30: 0 unless the movement is within 45° of the heading and the
  // speed clears the first bound; 2 at the second.
  if (std::fabs(headingDelta_deg) > 45.0f) return 0;
  if (speed_kmh < 11.0f) return 0;
  return speed_kmh >= 22.0f ? 2 : 1;
}

int BallBand(const DodgeConstants& k, float height_m, float bodyScale) {
  // FUN_140a6d380: the first band whose scaled threshold is at or below the
  // height (the table is ascending; band 0 is below the first threshold).
  for (int i = 0; i < 12; ++i)
    if (height_m <= bodyScale * k.ballHeightBands[i]) return i;
  return 12;
}

int KindColumn(const DodgeConstants& k, int kind) {
  return kind >= 0 && kind < 9 ? k.kindColumn[kind] : 0xb;
}

std::optional<DodgeClip> BallSelector(const Animetable& t, const DodgeConstants& k, int speedClass,
                                      const std::vector<int>& forcedRow) {
  // FUN_1407358e0 with a forced row: the row table the selector builds on the
  // stack, indexed by the speed class. The column is 9.
  const int row = forcedRow[speedClass];
  const int col = 9;
  if (!t.Populated(row, col)) return std::nullopt;
  DodgeClip c;
  c.row = row, c.col = col;
  c.motionNo = t.Cell(row, col).motionNo;
  c.mirror = t.Cell(row, col).mirror;
  return c;
}

std::optional<DodgeClip> NetSelector(const Animetable& t, const DodgeConstants& k, bool fast) {
  const int row = fast ? k.netRowFast : k.netRowDefault;
  const int col = 9;  // FUN_14073c5d0's default column (0x12 only when the row fails)
  if (!t.Populated(row, col)) return std::nullopt;
  DodgeClip c;
  c.row = row, c.col = col;
  c.motionNo = t.Cell(row, col).motionNo;
  c.mirror = t.Cell(row, col).mirror;
  return c;
}

std::optional<DodgeClip> PostSelector(const Animetable& t, const DodgeConstants& k, float bearing_deg,
                                      float refAngle_deg, float approachRel_deg, float speed_kmh) {
  // FUN_1407368d0. The exe reads two angles: the bearing from the post's anchor
  // to the player (raw, 0..360) and the angle the ball comes in at, both
  // relative to the post's own facing. The near/far split is
  // |wrap180(bearing - refAngle)| >= 45; the deep side 1 needs the raw bearing
  // to clear 30 deg on both of the exe's splits (i.e. 30..150);
  // |approachRel| picks the column (9, 13, 17). The far branch (past 45) runs
  // only for an approach inside 60..120. The row is grid[side][speedClass],
  // with the speed class from the player's speed: >22 -> 2, >13.5 -> 1, >8 ->
  // 0, else 3 (which has no row at all).
  const int speedClass = speed_kmh > k.speedClass2_kmh
                              ? 2
                              : (speed_kmh > k.speedClass1_kmh
                                     ? 1
                                     : (speed_kmh > k.postSpeedFloor_kmh ? 0 : 3));
  if (speedClass >= 3) return std::nullopt;
  auto wrap180 = [](float a) {
    while (a > 180.0f) a -= 360.0f;
    while (a <= -180.0f) a += 360.0f;
    return a;
  };
  int side = 0, col = 9;
  if (std::fabs(wrap180(bearing_deg - refAngle_deg)) >= k.postColumnUp_deg) {
    // The far branch: only the window between 60 and 120 deg carries a clip.
    const float b = std::fabs(approachRel_deg);
    if (b <= k.postColumnBailout_deg || b >= k.postBailout_deg) return std::nullopt;
    side = 2;
  } else if (bearing_deg >= k.postSideAngle_deg && bearing_deg <= 180.0f - k.postSideAngle_deg) {
    // The deep side 1: the exe's 30 <= |wrap180(-A0)| and 30 <= |wrap180(180 -
    // A0)|, together the bearing between 30 and 150.
    side = 1;
    const float bb = std::fabs(approachRel_deg);
    // 13 is the exe's own 0xd and matches the install's column exactly. The
    // exe's 0x11 (17) is one past the install's populated column 16; the
    // 17-vs-16 mapping is unresolved without a live poke, so the top band
    // reads the column this install actually carries.
    col = bb > k.postColumnHigh_deg ? 16 : (bb > k.postColumnUp_deg ? 13 : 9);
  }
  const int row = k.postGrid[side][speedClass];
  if (row < 0) return std::nullopt;
  if (!t.Populated(row, col)) return std::nullopt;
  DodgeClip c;
  c.row = row, c.col = col;
  c.motionNo = t.Cell(row, col).motionNo;
  c.mirror = t.Cell(row, col).mirror;
  return c;
}

int KindRow(const Animetable& t, const Mbinfo& mb, const DodgeConstants& k, int kind,
            float speed_mps, bool ballOwner, float ballHeight_m) {
  // FUN_140739f90: speed class from the player's speed, then step down the
  // classes until the row's column 9 is populated and the clip's hit window
  // fits the ball. The 0x2f8 frame-parity and 0x2ff AI-action special cases
  // are not ported (ponytail: they need the clip's own frame parity and the
  // player's AI action id, which the Animetable cell alone does not carry).
  if (kind < 0 || kind >= 8) return -1;
  int speedClass = speed_mps >= 22.0f ? 2 : (speed_mps > 13.5f ? 1 : 0);
  for (int s = speedClass; s >= 0; --s) {
    const int row = k.kindRow[kind][s];
    if (row < 0) continue;
    if (!t.Populated(row, 9)) continue;
    if (ballOwner) {
      // The column-9 hit-window fit (FUN_140739f90's loop): the clip must have
      // a hit record whose ball height clears the ball's. GF's Mbinfo carries
      // the hit frame and the ball position, so the fit reads the hit record's
      // ballY against the ball height.
      const int motion = t.Cell(row, 9).motionNo;
      bool fits = false;
      for (const auto& h : mb.Hits(motion))
        if (h.ballY >= ballHeight_m - 0.3f) fits = true;  // 0x142590d98 margin
      if (!fits) continue;
    }
    return row;
  }
  return -1;
}

PostAngles PostAnglesFor(int side, float playerX, float playerY, float ballX, float ballY,
                         float pitchHalfWidth_m, float goalHalfWidth_m) {
  // The own goal line is on this side's end, and the post the player is
  // running into is the one on the ball's half of it.
  PostAngles a;
  a.ownGoalX_m = float(side) * pitchHalfWidth_m;
  const float postY = ballY >= 0.0f ? goalHalfWidth_m : -goalHalfWidth_m;
  const auto deg = [](float rad) { return rad * 180.0f / blunted::pi; };
  // The bearing from the post to the player, raw 0..360.
  a.bearing_deg = deg(std::atan2(playerY - postY, playerX - a.ownGoalX_m));
  if (a.bearing_deg < 0.0f) a.bearing_deg += 360.0f;
  // The goal line's own facing is its outward normal, so the reference is 180
  // at the +x end and 0 at the -x end.
  a.refAngle_deg = a.ownGoalX_m >= 0.0f ? 180.0f : 0.0f;
  // The ball's approach relative to the same axis, wrapped to +/-180.
  a.approachRel_deg = deg(std::atan2(ballY - postY, ballX - a.ownGoalX_m)) - a.refAngle_deg;
  while (a.approachRel_deg > 180.0f) a.approachRel_deg -= 360.0f;
  while (a.approachRel_deg <= -180.0f) a.approachRel_deg += 360.0f;
  return a;
}

bool BallClassAdmits(const DodgeConstants& k, int roleClass) {
  // FUN_140a07bf0's head, three predicates over the role table at 0x1426484f0:
  // FUN_140a6e470 (bit 25) must be clear, FUN_140a6e3f0 (any of bits 0,1,4,5
  // and 0x6100000) must be set, FUN_140a6e5e0 (bit 5) must be clear. Roles
  // past the 94-entry table answer false, exactly as the exe's bounds check.
  if (roleClass < 0 || roleClass >= 94) return false;
  const uint32_t bits = k.roleBits[roleClass];
  if ((bits >> 25) & 1u) return false;
  if ((bits & 0x610003bu) == 0) return false;
  if ((bits >> 5) & 1u) return false;
  return true;
}

bool BallDodgeEligible(const DodgeConstants& k, const BallEligibilityInput& in) {
  // FUN_140a07bf0, in the exe's order.
  if (!BallClassAdmits(k, in.roleClass)) return false;
  // He is the team's designated ball owner: the eligibility admits him outright.
  if (in.designatedOwner) return true;
  if (in.slot < 0) return false;
  // A ball holder has to be in the same half's record set as the slot.
  if (in.ownsBall && in.teamIndex != (in.slot >= 11 ? 1 : 0)) return false;
  if (!in.perTeamActionAllowed) return false;
  // The action-list slot must be a real one (0x18 is "none"), or his state is
  // already a dodge (5).
  if (in.actionSlot == 0x18 && in.stateAction != 5) return false;
  // He may not be the player the team last acted through.
  if (in.lastActionPlayer) return false;
  // The last-event window: the frames since his own last event have to clear
  // (rng * 5) / 48, rng being the game-speed divisor the exe rolls.
  const int framesNeeded =
      (k.lastEventRollDivisor * k.lastEventFramesPerRoll) / 48;
  return in.framesSinceEvent >= framesNeeded;
}

bool BallDodgeSubTestFast(const DodgeConstants& k, const BallSubTestInput& in) {
  // FUN_140a07570, in the exe's order.
  // The ball has to be stepping (0.8) and the rolled frame's height under his
  // reach (the reach byte * 0.01 + the 0.3 margin).
  if (in.ballStep_m < k.ballSpeedFloor_mps) return false;
  if (in.frameY_m >= in.reach_m * k.ballReachPerUnit_m + k.ballReachMargin_m) return false;
  // The record's own half has to be his, or the keeper save flag is set
  // (`record0->+4 == (id >= 11) || record0->+0x2c`).
  if (!in.keeperSaveFlag && in.recordHalf != ((in.selfSlot >= 11) ? 1 : 0)) return false;
  // Who touched the ball last picks the branch.
  if (in.lastTouchSlot == in.selfSlot) {
    // The bearing to the ball within 10 deg of his heading, or of the goal he
    // defends (rec->0x554). Either admits him.
    if (std::fabs(Wrap180Deg(in.selfHeading_deg - in.bearingToBall_deg)) < k.ballWindowHeading_deg)
      return true;
    return std::fabs(Wrap180Deg(in.defendGoalAngle_deg - in.bearingToBall_deg)) <
           k.ballWindowHeading_deg;
  }
  if (in.lastTouchSlot >= 0 && in.lastTouchSlot < 22) {
    // Somebody else touched it: he has to reach the ball no later than they
    // did, and the bearing off THEIR heading is inside 90 deg.
    if (in.selfTimeToBall_s > in.otherTimeToBall_s) return false;
    return std::fabs(Wrap180Deg(in.otherHeading_deg - in.bearingToBall_deg)) <=
           k.ballWindowGoal_deg;
  }
  // Nobody (>= 22): the live action decides. The locos and STAGGER are out;
  // everything else passes the geometry and falls through to the anime slot.
  if (in.liveAction >= 1 && in.liveAction <= 3) return false;
  return in.liveAction != 0x10 && in.hasAnimeSlot;
}

bool BallDodgeSubTestMouth(const DodgeConstants& k, const BallSubTestInput& in) {
  // FUN_140a07880. Only the kinds whose class is 8, 9 or 10 (the kicks) read
  // the goal mouth; the rest fall through to the selector.
  if (in.targetAction < 8 || in.targetAction > 10) return true;
  // The exe builds three sample x on the goal line: the mouth's edge past the
  // post (the pitch's z edge + the post offset, times his side), the pitch's
  // own far x edge, and the third on the opposite side. The ball's bearing to
  // its look-ahead position has to sit inside the wedge two of them span:
  // signedAngle(near, ball) >= 0 and signedAngle(far, ball) <= 0.
  return Wrap180Deg(in.ballBearing_deg - in.mouthMidAngle_deg) >= 0.0f &&
         Wrap180Deg(in.ballBearing_deg - in.mouthFarAngle_deg) <= 0.0f;
}

// PES's bearing (FUN_140439e50 over FUN_140a92d40); see pesdodge.hpp.
float PesBearingDeg(float fromX, float fromY, float toX, float toY) {
  float a = std::atan2(toX - fromX, toY - fromY) * (180.0f / blunted::pi);
  if (a < 0.0f) a += 360.0f;
  return a;
}

bool NetDodgeGate(const DodgeConstants& k, const GoalMarks& marks, const NetGateInput& in) {
  // FUN_140a07e40, in the exe's order. Every term reads the dodger's own record
  // and the goal marks; the ball does not enter.
  const float ax = std::fabs(in.x_m);
  // He has to stand within the edge margin of his goal line: |x| >= F[0] - 1.
  if (ax < marks.goalLineX_m - k.netEdgeMargin_m) return false;
  // The anime layer has to have the dodge slot.
  if (!in.animeSlot) return false;
  // He has to be moving. The exe's step is 100 / (gs * 72) per frame; per
  // second the gs cancels, so the floor is 100 / 72 m/s.
  // ponytail: the exe's movement-state branch (+0x5b0) is not carried; the
  // speed floor stands in for it.
  const float speed = std::sqrt(in.moveX_mps * in.moveX_mps + in.moveY_mps * in.moveY_mps +
                                in.moveZ_mps * in.moveZ_mps);
  if (speed <= k.netStepNumerator / k.netStepDivisor) return false;
  // The heading is his movement's bearing (FUN_140458740 over the record's velocity).
  const float heading = PesBearingDeg(0.0f, 0.0f, in.moveX_mps, in.moveY_mps);
  // The goal he stands at is the one on his end: the sign of his x.
  const float sx = in.x_m < 0.0f ? -1.0f : 1.0f;
  const float sz = in.y_m < 0.0f ? -1.0f : 1.0f;
  const float goalX = sx * marks.goalLineX_m;
  const float ownPost = PesBearingDeg(in.x_m, in.y_m, goalX, sz * marks.postHalfWidth_m);
  const bool wide = std::fabs(in.y_m) > marks.postHalfWidth_m;
  // Wide of the posts: his own post and the point behind it at the net's back.
  // In line: the two posts across the mouth, the back x only once he is past it.
  float a = 0.0f;
  float b = 0.0f;
  if (wide) {
    a = ownPost;
    b = PesBearingDeg(in.x_m, in.y_m, sx * marks.netBackX_m, sz * marks.postHalfWidth_m);
  } else {
    const float mouthX = ax > marks.netBackX_m ? sx * marks.netBackX_m : goalX;
    a = PesBearingDeg(in.x_m, in.y_m, mouthX, marks.postHalfWidth_m);
    b = PesBearingDeg(in.x_m, in.y_m, mouthX, -marks.postHalfWidth_m);
  }
  const float lo = std::min(a, b);
  const float hi = std::max(a, b);
  // Level with the goal line, or wide of the posts, his heading sits between
  // them; past the line it sits outside them.
  // ponytail: the exe's outside branch also yields to the anime state byte 2
  // (hu(i)+0x4c), which GF does not carry.
  const bool inside = ax <= marks.goalLineX_m || wide;
  if (inside) {
    if (heading < lo || heading > hi) return false;
  } else if (heading > lo && heading < hi) {
    return false;
  }
  // Inside the edge, his heading has to clear his own post by the rejection angle.
  if (ax < marks.goalLineX_m) {
    float residual = ownPost - heading;
    if (residual > 180.0f) residual -= 360.0f;
    if (residual < -180.0f) residual += 360.0f;
    if (std::fabs(residual) < k.netEdgeRejection_deg) return false;
  }
  return true;
}

bool PostDodgeGate(const DodgeConstants& k, const GoalMarks& marks, const PostGateInput& in) {
  // FUN_140a084d0, in the exe's order. The keeper and newest-event exits are
  // early returns; the rest is a conjunction, with the ball clause last.
  if (in.keeper || in.newestTouch) return false;
  // He stands within the edge margin of his goal line: |x| >= F[0] - 1.
  const float ax = std::fabs(in.x_m);
  if (marks.goalLineX_m - ax > k.netEdgeMargin_m) return false;
  // His z sits within the edge margin of his post: | |z| - F[2] | <= 1.
  const float dz = std::fabs(in.y_m) - marks.postHalfWidth_m;
  if (dz > k.netEdgeMargin_m || dz < -k.netEdgeMargin_m) return false;
  if (!in.animeSlot) return false;
  // He is moving above the step floor, the same 100 / (gs * 72) per frame as
  // the net gate: 100 / 72 m/s once gs cancels.
  // ponytail: the exe also reads the movement state (+0x5b0); the floor stands in.
  const float speed = std::sqrt(in.moveX_mps * in.moveX_mps + in.moveY_mps * in.moveY_mps +
                                in.moveZ_mps * in.moveZ_mps);
  if (speed <= k.netStepNumerator / k.netStepDivisor) return false;
  // The post is at the goal line of his end of the pitch and on his side (the
  // signs are his own, as the exe's `x <= 0` / `z <= 0` tests take them).
  const float sx = in.x_m < 0.0f ? -1.0f : 1.0f;
  const float sz = in.y_m < 0.0f ? -1.0f : 1.0f;
  const float postX = sx * marks.goalLineX_m;
  const float postY = sz * marks.postHalfWidth_m;
  // The residual: his bearing to the post less his heading, folded to (-180, 180].
  const float heading = PesBearingDeg(0.0f, 0.0f, in.moveX_mps, in.moveY_mps);
  float residual = PesBearingDeg(in.x_m, in.y_m, postX, postY) - heading;
  if (std::fabs(residual) > 180.0f) residual -= residual >= 0.0f ? 360.0f : -360.0f;
  // Heading at the post (within the rejection angle), or within the admission
  // distance of it on either axis.
  const bool onTheBearing = std::fabs(residual) <= k.netEdgeRejection_deg;
  const bool nearOnX = std::fabs(in.x_m - postX) <= k.postResidualProximity_m;
  const bool nearOnY = std::fabs(in.y_m - postY) <= k.postResidualProximity_m;
  if (!onTheBearing && !nearOnX && !nearOnY) return false;
  // And within postProximity_m of the post.
  const float toPost = std::hypot(in.x_m - postX, in.y_m - postY);
  if (toPost > k.postProximity_m) return false;
  // The designated man stays out of a ball that is at the post and nearer him
  // than the post is. ponytail: T+0x78 only; PES's MA+0x2470c table is not carried.
  if (in.designatedMan && std::fabs(in.ballX_m) < marks.goalLineX_m) {
    const float ballToPost = std::hypot(in.ballX_m - postX, in.ballY_m - postY);
    const float toBall = std::hypot(in.x_m - in.ballX_m, in.y_m - in.ballY_m);
    if (ballToPost < k.postBallNear_m && toPost > toBall) return false;
  }
  return true;
}

float HumanLastEventWindowFrames(const DodgeConstants& k) {
  // FUN_140a0b350 (decompile line 70): `(float)(MI.frame - ev.frame) < gs * DAT_142598578`.
  return static_cast<float>(k.lastEventRollDivisor) * k.lastEventWindowFactor;
}

bool HumanDodgeGate(const DodgeConstants& k, const HumanGateInput& in) {
  // FUN_140a0b350, in the exe's order.
  // The behaviour has to be enabled and the match not in the state that
  // switches it off.
  if (!in.featureOn) return false;
  if (!in.matchStateOk) return false;
  // He has to be on the team's own roster list.
  if (!in.onRoster) return false;
  // The state record's own half has to be his, which is the half the
  // challenger scan (FUN_140a08ea0) runs for.
  if (!in.stateRecordHalfMatches) return false;
  // The window (FUN_140a0b350, decompile lines 64-73): his team's newest ball
  // event holds him while it is younger than gs * 0.2 frames. The age is in ms
  // and the exe's frame counter runs at gs per second, so frames = ms * gs / 1000.
  const float ageFrames =
      in.newestEventAge_ms * static_cast<float>(k.lastEventRollDivisor) / 1000.0f;
  if (in.newestEventIsOwnTeam && ageFrames < HumanLastEventWindowFrames(k)) return false;
  // The team's own latch has to be closed.
  if (in.teamGateOpen) return false;
  // When his own team's roster slot carries a value, the ball-owner test
  // decides; otherwise the gate is open.
  if (in.rosterSlotValue != 0 && in.ballOwnerIsSelf) return false;
  return true;
}

}  // namespace pes
