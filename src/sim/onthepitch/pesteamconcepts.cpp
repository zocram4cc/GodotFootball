#include "pesteamconcepts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

#include "gametypes.hpp"
#include "pes/uniform.hpp"
#include "onthepitch/pesroles.hpp"

namespace pes {
namespace {

// {VA, expected opcode bytes}: the instruction is checked before its operand
// is trusted, so another build fails loudly.
struct Code {
  uint64_t va;
  std::initializer_list<uint8_t> bytes;
};

std::string VaText(uint64_t va) {
  char s[32];
  std::snprintf(s, sizeof s, "%#llx", static_cast<unsigned long long>(va));
  return s;
}

bool Matches(const PeImage& exe, const Code& c, std::string* error) {
  uint8_t got[16] = {};
  if (!exe.ReadVa(c.va, c.bytes.size(), got) || !std::equal(c.bytes.begin(), c.bytes.end(), got)) {
    *error = "team concept constant: unexpected instruction at " + VaText(c.va);
    return false;
  }
  return true;
}

bool ReadByteImm(const PeImage& exe, const Code& c, int* out, std::string* error) {
  uint8_t v = 0;
  if (!Matches(exe, c, error) || !exe.ReadVa(c.va + c.bytes.size(), 1, &v)) {
    if (error->empty()) *error = "team concept constant: unreadable at " + VaText(c.va);
    return false;
  }
  *out = v;
  return true;
}

// A 32-bit immediate after its checked prefix (the sheet ctor's `mov dword, imm32`).
bool ReadDwordImm(const PeImage& exe, const Code& c, int* out, std::string* error) {
  uint8_t v[4] = {};
  if (!Matches(exe, c, error) || !exe.ReadVa(c.va + c.bytes.size(), sizeof v, v)) {
    if (error->empty()) *error = "team concept constant: unreadable at " + VaText(c.va);
    return false;
  }
  *out = static_cast<int>(static_cast<uint32_t>(v[0]) | (static_cast<uint32_t>(v[1]) << 8) |
                          (static_cast<uint32_t>(v[2]) << 16) | (static_cast<uint32_t>(v[3]) << 24));
  return true;
}

bool ReadF32(const PeImage& exe, uint64_t va, float* out, std::string* error) {
  if (!exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out))) {
    *error = "team concept constant: .rdata unreadable at " + VaText(va);
    return false;
  }
  return true;
}

// `lea r32,[rax+rax*s]` multiplies by s+1 (the SIB scale field, bits 6-7).
float LeaMultiplier(uint8_t sib) { return static_cast<float>((1 << (sib >> 6)) + 1); }

// --- the image ---------------------------------------------------------------
// Sheet ctor FUN_1408c5390: `mov byte [rcx+rax+k], imm8` per slider k.
constexpr Code SHEET_SUPPORT_RANGE{0x1408c5418, {0xc6, 0x44, 0x01, 0x04}};
constexpr Code SHEET_DEFENSIVE_LINE{0x1408c5454, {0xc6, 0x44, 0x01, 0x06}};
constexpr Code SHEET_COMPACTNESS{0x1408c5471, {0xc6, 0x44, 0x01, 0x07}};
constexpr Code SHEET_TOGGLES_ZERO{0x1408c53bc, {0x33, 0xf6}};  // xor esi,esi: the toggle default
// Sheet ctor stores beyond the sliders above: the inert sliders k1 and k4 (no AI reader),
// the captain, the designated loop (one store, six slots), the three takers and the
// slot-target loop (33 entries). Each immediate is read after its checked prefix.
constexpr Code SHEET_SLIDER_INERT_1{0x1408c5438, {0xc6, 0x04, 0x01}};
constexpr Code SHEET_SLIDER_INERT_4{0x1408c548e, {0xc6, 0x44, 0x01, 0x08}};
constexpr Code SHEET_CAPTAIN{0x1408c5838, {0x41, 0xc7, 0x81, 0x38, 0x14, 0x00, 0x00}};
constexpr Code SHEET_DESIGNATED{0x1408c5858, {0xc7, 0x01}};
constexpr Code SHEET_TAKER_0{0x1408c586b, {0x41, 0xc7, 0x81, 0x54, 0x14, 0x00, 0x00}};
constexpr Code SHEET_TAKER_1{0x1408c587a, {0x41, 0xc7, 0x81, 0x58, 0x14, 0x00, 0x00}};
constexpr Code SHEET_TAKER_2{0x1408c5885, {0x49, 0xc7, 0x81, 0x5c, 0x14, 0x00, 0x00}};
constexpr Code SHEET_SLOT_TARGET{0x1408c5671, {0x41, 0xc7, 0x84, 0x89, 0x44, 0x08, 0x00, 0x00}};
constexpr uint64_t END_TEST_AFTER_VA = 0x142588200;           // 0.1 (selector, x fps)

// TransitionChase start FUN_140575510 / execute FUN_140575b60.
constexpr Code CHASE_WINDOW_LEA{0x1405755c3, {0x8d, 0x0c, 0x40}};  // lea ecx,[rax+rax*2]: 3 x fps
constexpr uint64_t CHASE_THIRD_VA = 0x142588210;     // 10
constexpr uint64_t CHASE_WIDE_VA = 0x142565604;      // 2
constexpr uint64_t CHASE_RANGE_SQ_VA = 0x14253ee30;  // 100: the d^2 test and the 100 deg test
constexpr uint64_t CHASE_INSIDE_VA = 0x14259be5c;    // 90
constexpr uint64_t CHASE_SECOND_SQ_VA = 0x1425a2670; // 400
constexpr uint64_t CHASE_SLACK_VA = 0x14259be0c;     // 15
constexpr uint64_t CHASE_TARGET_VA = 0x14259bdf4;    // 8
constexpr uint64_t CHASE_TURN_VA = 0x14259be6c;      // 160

// SideCounter start FUN_140540780.
constexpr uint64_t COUNTER_WINDOW_VA = 0x14258c264;   // 3
constexpr uint64_t COUNTER_KICKOFF_VA = 0x14259bdec;  // 6
constexpr uint64_t COUNTER_FACING_VA = 0x14259be38;   // 55
constexpr uint64_t COUNTER_DEPTH_VA = 0x14259d664;    // -12
constexpr uint64_t COUNTER_LINE_VA = 0x14259be84;     // -10
constexpr uint64_t COUNTER_ABORT_LINE_VA = 0x1425c7220;  // -14
constexpr uint64_t COUNTER_ABORT_WIDE_VA = 0x1425656c0;  // 4
constexpr Code COUNTER_MAX_GOAL_SIDE{0x140540bfd, {0x3c}};  // cmp al, imm8
// Runner pick FUN_14053e9a0 and target FUN_14053f510.
constexpr uint64_t RUNNER_UNMARKED_VA = 0x1425c721c;  // 42.25
constexpr uint64_t RUNNER_SHARE_NUM_VA = 0x14259be34; // 50
constexpr uint64_t RUNNER_SHARE_DEN_VA = 0x14253ee30; // 100
constexpr uint64_t RUNNER_SLACK_VA = 0x14259bdfc;     // 11
constexpr uint64_t RUNNER_CENTRE_VA = 0x14259d5d4;    // 1.2
constexpr uint64_t RUNNER_AREA_VA = 0x14257a194;      // 0.7
constexpr uint64_t RUNNER_LANE_VA = 0x14259d614;      // 17
// Supporters FUN_140541010.
constexpr uint64_t SUPPORT_AHEAD_VA = 0x14259be0c;  // 15
constexpr uint64_t SUPPORT_SIDE_VA = 0x14259be04;   // 12
constexpr uint64_t SUPPORT_TURN_VA = 0x1425c48c0;   // 22
constexpr uint64_t SUPPORT_MIN_AHEAD_VA = 0x14259bdf4;  // 8
constexpr uint64_t SUPPORT_HOLD_VA = 0x14259be7c;   // -5
// End test FUN_1405404a0.
constexpr Code COUNTER_GRACE_ADD{0x14054056a, {0x03, 0xc0}};      // add eax,eax: 2 x fps
constexpr Code COUNTER_MAX_LEA{0x140540609, {0x8d, 0x0c, 0x40}};  // lea ecx,[rax+rax*2]
constexpr Code COUNTER_MAX_ADD{0x14054060c, {0x03, 0xc9}};        // add ecx,ecx: 6 x fps
constexpr uint64_t COUNTER_FAR_VA = 0x1425c48e4;  // 1369
constexpr uint64_t COUNTER_DX_VA = 0x142591140;   // 30
// ForeCheckPress start FUN_1405747f0 / continue FUN_140574710 / count FUN_140574200.
constexpr uint64_t FORECHECK_WINDOW_VA = 0x14259222c;  // 5 (x fps)
constexpr uint64_t FORECHECK_HOLD_VA = 0x142565604;    // 2 (x fps)
constexpr uint64_t FORECHECK_FLOOR_VA = 0x14259be84;   // -10
constexpr uint64_t FORECHECK_CORRIDOR_VA = 0x14259be04; // 12
constexpr uint64_t PRESS_GOAL_SIDE_VA = 0x14259be4c;   // 70
constexpr uint64_t PRESS_RAMP_FROM_VA = 0x14253ee30;   // 100
constexpr uint64_t PRESS_RAMP_TO_VA = 0x1425c964c;     // 170
constexpr uint64_t PRESS_RADIUS_VA = 0x14259bdf4;      // 8
constexpr uint64_t PRESS_RADIUS_RAMP_VA = 0x1425656c0; // 4
constexpr uint64_t PRESS_BONUS_ABOVE_VA = 0x142598578; // 0.2
constexpr Code PRESS_COUNT{0x140574654, {0x41, 0x80, 0xfc}};  // cmp r12b, imm8
// ForeCheckLineDown FUN_140574a30 / FUN_140574ab0 / FUN_140574940.
constexpr uint64_t LINEDOWN_DIVISOR_VA = 0x1425c74d0;  // -2.5
constexpr uint64_t LINEDOWN_MAX_VA = 0x1425c50fc;      // 27
constexpr uint64_t LINEDOWN_GAP_VA = 0x14258c264;      // 3
constexpr uint64_t LINEDOWN_FACING_VA = 0x14259be4c;   // 70
// RetreatBlockCreate end FUN_140575db0.
constexpr uint64_t RETREAT_GAP_VA = 0x14259222c;       // 5
constexpr uint64_t RETREAT_BACK_VA = 0x14259bdf4;      // 8
constexpr uint64_t RETREAT_MID_VA = 0x14259be18;       // 18
// Band push-up FUN_140687fb0 and the measured line FUN_140687820.
constexpr uint64_t PUSHUP_BEHIND_VA = 0x1425c7678;     // -4
constexpr uint64_t PUSHUP_BALL_VA = 0x14259bdec;       // 6
constexpr uint64_t PUSHUP_CAP_VA = 0x14259be7c;        // -5
constexpr uint64_t MEASURED_CLAMP_VA = 0x1425656c0;    // 4
// The press count's lane bonus FUN_140a53da0 (call 0x14057436d).
constexpr uint64_t PRESS_LANE_RADIUS_VA = 0x1425a2828; // 6.5
constexpr uint64_t PRESS_LANE_BONUS_VA = 0x14259222c;  // 5
// The uncovered carrier FUN_140574bb0.
constexpr uint64_t UNCOVERED_MARGIN_VA = 0x14259be04;  // 12
constexpr uint64_t UNCOVERED_NEAR_VA = 0x14259d60c;    // 9
constexpr uint64_t UNCOVERED_FACING_VA = 0x14259be5c;  // 90
constexpr uint64_t UNCOVERED_FROM_VA = 0x14259222c;    // 5
constexpr uint64_t UNCOVERED_TO_VA = 0x14259be1c;      // 20
constexpr uint64_t UNCOVERED_SPAN_VA = 0x14259be0c;    // 15
constexpr uint64_t UNCOVERED_RAMP_VA = 0x14258c264;    // 3
constexpr uint64_t UNCOVERED_RADIUS_VA = 0x1425656c0;  // 4
constexpr uint64_t UNCOVERED_CONE_VA = 0x142596e6c;    // 80
// ForeCheckLineUp FUN_140575210 / FUN_140574f40 / FUN_140682770.
constexpr uint64_t LINEUP_WIDE_VA = 0x14259222c;       // 5
constexpr uint64_t LINEUP_MAX_VA = 0x14259be18;        // 18
constexpr uint64_t LINEUP_MIN_VA = 0x14259be84;        // -10
constexpr uint64_t CONTAIN_FAR_VA = 0x14259d610;       // 12.25
constexpr uint64_t CONTAIN_NEAR_VA = 0x1425656c0;      // 4
constexpr uint64_t CONTAIN_NEAR_ANGLE_VA = 0x14259be58;  // 85
constexpr uint64_t CONTAIN_FACING_VA = 0x14259be60;    // 135
constexpr uint64_t CONTAIN_WIDE_FROM_VA = 0x142588210; // 10
constexpr uint64_t CONTAIN_CONE_VA = 0x14259be5c;      // 90
constexpr uint64_t CONTAIN_RADIUS_VA = 0x14258c264;    // 3
constexpr uint64_t LINEUP_BAND_VA = 0x14259bdf0;       // 7
constexpr uint64_t LINEUP_BALL_VA = 0x14259222c;       // 5

// constant_team.bin's subConcept.o (constant-team 2.3): blob u32 0 = the offset
// of the 48-entry i32 list.
constexpr const char* CONSTANT_TEAM_PATH = "common/match/constant/constant_team.bin";
constexpr const char* SUB_CONCEPT_OBJECT = "subConcept.o";

bool LoadEnables(const PesVfs& vfs, std::array<bool, kConceptCount>* out, std::string* error) {
  auto blob = vfs.Read(CONSTANT_TEAM_PATH, error);
  if (!blob) return false;
  auto table = ParseUniformTable(*blob, error);
  if (!table) return false;
  auto span = table->Find(SUB_CONCEPT_OBJECT);
  if (!span) {
    *error = std::string(SUB_CONCEPT_OBJECT) + " missing from " + CONSTANT_TEAM_PATH;
    return false;
  }
  uint32_t listAt = 0;
  if (span->size() < sizeof listAt) {
    *error = "subConcept.o truncated";
    return false;
  }
  std::memcpy(&listAt, span->data(), sizeof listAt);
  if (static_cast<size_t>(listAt) + kConceptCount * sizeof(int32_t) > span->size()) {
    *error = "subConcept.o list out of range";
    return false;
  }
  for (int id = 0; id < kConceptCount; ++id) {
    int32_t v = 0;
    std::memcpy(&v, span->data() + listAt + id * sizeof v, sizeof v);
    (*out)[id] = v > 0;  // the selector skips `<= 0` (jle, 0x140579a1f)
  }
  return true;
}

// --- geometry ----------------------------------------------------------------
constexpr float kDegPerRad = 180.0f / 3.14159265358979f;
constexpr float kFullTurn_deg = 360.0f;
constexpr float kHalfTurn_deg = 180.0f;
constexpr float kPlusX_deg = 90.0f;   // FUN_140a4f210: dir +1 -> 90
constexpr float kMinusX_deg = 270.0f; // dir -1 -> 270
constexpr float kPlusY_deg = 0.0f;

float Wrap360(float a) {
  if (a < 0.0f) return a + kFullTurn_deg;
  if (a >= kFullTurn_deg) return a - kFullTurn_deg;
  return a;
}

// FUN_14043a0a0: |a - b| folded to 0..180.
float AngleDiff(float a, float b) {
  float d = std::fabs(Wrap360(b) - Wrap360(a));
  return d > kHalfTurn_deg ? kFullTurn_deg - d : d;
}

float AttackHeading(float dir) { return dir > 0.0f ? kPlusX_deg : kMinusX_deg; }

// Bearing a -> b (FUN_140a92d40 on (dx, dy), 0 when they coincide).
float Bearing(const Vector3& a, const Vector3& b) {
  const float dx = b.coords[0] - a.coords[0], dy = b.coords[1] - a.coords[1];
  if (dx == 0.0f && dy == 0.0f) return 0.0f;
  return Wrap360(std::atan2(dx, dy) * kDegPerRad);
}

// FUN_140a92950: step `from` toward `to` by at most `max`.
float TurnToward(float from, float to, float max) {
  float d = Wrap360(to) - Wrap360(from);
  if (d > kHalfTurn_deg) d -= kFullTurn_deg;
  if (d < -kHalfTurn_deg) d += kFullTurn_deg;
  const float step = std::min(std::fabs(d), max);
  return Wrap360(Wrap360(from) + (d < 0.0f ? -step : step));
}

// FUN_140a8f7c0: p + r * (sin h, cos h).
Vector3 Advance(const Vector3& p, float heading_deg, float r) {
  const float h = heading_deg / kDegPerRad;
  return Vector3(p.coords[0] + r * std::sin(h), p.coords[1] + r * std::cos(h), 0.0f);
}

// PES's side sign: `0 > z ? -1 : 1`.
float SideSign(float y) { return y < 0.0f ? -1.0f : 1.0f; }

float DistSq(const Vector3& a, const Vector3& b) {
  const float dx = a.coords[0] - b.coords[0], dy = a.coords[1] - b.coords[1];
  return dx * dx + dy * dy;
}

// FUN_140a4c720(a, b, dir, c): `(dir*c + a)*dir < dir*b` - b lies beyond a + c
// in the team's attack frame.
bool Beyond(float a, float b, float dir, float c) { return (dir * c + a) * dir < dir * b; }

// FUN_140a4fea0 -> FUN_140a4fbc0 (run-targets-h1 4), frames 0: men with
// minDist^2 < d^2 < radius^2 from `pos` whose bearing lies strictly within
// width/2 of `center`.
int PlayersInCone(const std::vector<Vector3>& men, const Vector3& pos, float center_deg, float width_deg,
                  float radius, float minDist) {
  int count = 0;
  for (const Vector3& m : men) {
    const float d2 = DistSq(m, pos);
    if (d2 <= minDist * minDist || d2 >= radius * radius) continue;
    if (AngleDiff(center_deg, Bearing(pos, m)) < width_deg * 0.5f) ++count;
  }
  return count;
}

}  // namespace

std::optional<TeamConceptConstants> LoadTeamConceptConstants(const PeImage& exe, const PesVfs& vfs,
                                                             std::string* error) {
  TeamConceptConstants k{};
  int captain = 0, designated = 0, slotTarget = 0;
  int taker[kSheetTakers] = {};
  bool ok = ReadByteImm(exe, SHEET_SUPPORT_RANGE, &k.defaultSliders[kSliderSupportRange], error) &&
            ReadByteImm(exe, SHEET_SLIDER_INERT_1, &k.defaultSliders[1], error) &&
            ReadByteImm(exe, SHEET_DEFENSIVE_LINE, &k.defaultSliders[kSliderDefensiveLine], error) &&
            ReadByteImm(exe, SHEET_COMPACTNESS, &k.defaultSliders[kSliderCompactness], error) &&
            ReadByteImm(exe, SHEET_SLIDER_INERT_4, &k.defaultSliders[4], error) &&
            ReadDwordImm(exe, SHEET_CAPTAIN, &captain, error) &&
            ReadDwordImm(exe, SHEET_DESIGNATED, &designated, error) &&
            ReadDwordImm(exe, SHEET_TAKER_0, &taker[0], error) &&
            ReadDwordImm(exe, SHEET_TAKER_1, &taker[1], error) &&
            ReadDwordImm(exe, SHEET_TAKER_2, &taker[2], error) &&
            ReadDwordImm(exe, SHEET_SLOT_TARGET, &slotTarget, error) &&
            Matches(exe, SHEET_TOGGLES_ZERO, error) && Matches(exe, CHASE_WINDOW_LEA, error) &&
            ReadByteImm(exe, COUNTER_MAX_GOAL_SIDE, &k.counterMaxGoalSide, error) &&
            Matches(exe, COUNTER_GRACE_ADD, error) && Matches(exe, COUNTER_MAX_LEA, error) &&
            Matches(exe, COUNTER_MAX_ADD, error) && ReadByteImm(exe, PRESS_COUNT, &k.pressCount, error);
  if (!ok) return std::nullopt;
  k.defaultCaptain = captain;
  k.defaultDesignated.fill(designated);
  k.defaultTaker = {taker[0], taker[1], taker[2]};
  k.defaultSlotTarget = slotTarget;
  const float leaTriple = LeaMultiplier(*(CHASE_WINDOW_LEA.bytes.begin() + 2));
  constexpr float kAddDoubles = 2.0f;  // add r,r
  k.chaseWindow_s = leaTriple;
  k.counterRunGrace_s = kAddDoubles;
  k.counterMaxAge_s = LeaMultiplier(*(COUNTER_MAX_LEA.bytes.begin() + 2)) * kAddDoubles;

  float shareNum = 0, shareDen = 0;
  const std::pair<uint64_t, float*> floats[] = {
      {END_TEST_AFTER_VA, &k.endTestAfter_s},
      {CHASE_THIRD_VA, &k.chaseThirdMargin_m},
      {CHASE_WIDE_VA, &k.chaseWideMargin_m},
      {CHASE_RANGE_SQ_VA, &k.chaseRangeSq_m2},
      {CHASE_RANGE_SQ_VA, &k.chaseGoalSideMax_deg},
      {CHASE_INSIDE_VA, &k.chaseInsideMin_deg},
      {CHASE_SECOND_SQ_VA, &k.chaseSecondRangeSq_m2},
      {CHASE_SLACK_VA, &k.chaseSecondWidthSlack_m},
      {CHASE_TARGET_VA, &k.chaseTargetDistance_m},
      {CHASE_TURN_VA, &k.chaseTargetTurn_deg},
      {COUNTER_WINDOW_VA, &k.counterWindow_s},
      {COUNTER_KICKOFF_VA, &k.counterKickoffGuard_s},
      {COUNTER_FACING_VA, &k.counterFacingMax_deg},
      {COUNTER_DEPTH_VA, &k.counterBallMaxDepth_m},
      {COUNTER_LINE_VA, &k.counterLineMargin_m},
      {COUNTER_ABORT_LINE_VA, &k.counterAbortLineMargin_m},
      {COUNTER_ABORT_WIDE_VA, &k.counterAbortWideMargin_m},
      {RUNNER_UNMARKED_VA, &k.runnerUnmarkedSq_m2},
      {RUNNER_SHARE_NUM_VA, &shareNum},
      {RUNNER_SHARE_DEN_VA, &shareDen},
      {RUNNER_SLACK_VA, &k.runnerChannelSlack_m},
      {RUNNER_CENTRE_VA, &k.runnerCentreFactor},
      {RUNNER_AREA_VA, &k.runnerAreaFactor},
      {RUNNER_LANE_VA, &k.runnerWideLane_m},
      {SUPPORT_AHEAD_VA, &k.supportAhead_m},
      {SUPPORT_SIDE_VA, &k.supportSide_m},
      {SUPPORT_TURN_VA, &k.supportTurn_deg},
      {SUPPORT_MIN_AHEAD_VA, &k.supportMinAhead_m},
      {SUPPORT_HOLD_VA, &k.supportHoldBonus_m},
      {COUNTER_FAR_VA, &k.counterRunnerFarSq},
      {COUNTER_DX_VA, &k.counterRunnerMaxDx_m},
      {FORECHECK_WINDOW_VA, &k.foreCheckWindow_s},
      {FORECHECK_HOLD_VA, &k.foreCheckHold_s},
      {FORECHECK_FLOOR_VA, &k.foreCheckBallFloor_m},
      {FORECHECK_CORRIDOR_VA, &k.foreCheckCorridor_m},
      {PRESS_GOAL_SIDE_VA, &k.pressGoalSide_deg},
      {PRESS_RAMP_FROM_VA, &k.pressRampFrom_deg},
      {PRESS_RAMP_TO_VA, &k.pressRampTo_deg},
      {PRESS_RADIUS_VA, &k.pressRadius_m},
      {PRESS_RADIUS_RAMP_VA, &k.pressRadiusRamp_m},
      {PRESS_BONUS_ABOVE_VA, &k.pressBonusAbove},
      {LINEDOWN_DIVISOR_VA, &k.lineDownBallDivisor},
      {LINEDOWN_MAX_VA, &k.lineDownBallMax_m},
      {LINEDOWN_GAP_VA, &k.lineDownGap_m},
      {LINEDOWN_FACING_VA, &k.lineDownLongPassFacing_deg},
      {RETREAT_GAP_VA, &k.retreatGap_m},
      {RETREAT_BACK_VA, &k.retreatBackFront_m},
      {RETREAT_MID_VA, &k.retreatMidFront_m},
      {PUSHUP_BEHIND_VA, &k.pushUpBehind_m},
      {PUSHUP_BALL_VA, &k.pushUpBallGap_m},
      {PUSHUP_CAP_VA, &k.pushUpCap_m},
      {MEASURED_CLAMP_VA, &k.measuredLineClamp_m},
      {PRESS_LANE_RADIUS_VA, &k.pressLaneRadius_m},
      {PRESS_LANE_BONUS_VA, &k.pressLaneBonus_m},
      {UNCOVERED_MARGIN_VA, &k.uncoveredLineMargin_m},
      {UNCOVERED_NEAR_VA, &k.uncoveredNearSq_m2},
      {UNCOVERED_FACING_VA, &k.uncoveredFacing_deg},
      {UNCOVERED_FROM_VA, &k.uncoveredRampFrom_m},
      {UNCOVERED_TO_VA, &k.uncoveredRampTo_m},
      {UNCOVERED_SPAN_VA, &k.uncoveredRampSpan_m},
      {UNCOVERED_RAMP_VA, &k.uncoveredRadiusRamp_m},
      {UNCOVERED_RADIUS_VA, &k.uncoveredRadius_m},
      {UNCOVERED_CONE_VA, &k.uncoveredCone_deg},
      {LINEUP_WIDE_VA, &k.lineUpWideMargin_m},
      {LINEUP_MAX_VA, &k.lineUpBallMax_m},
      {LINEUP_MIN_VA, &k.lineUpBallMin_m},
      {CONTAIN_FAR_VA, &k.containFarSq_m2},
      {CONTAIN_NEAR_VA, &k.containNearSq_m2},
      {CONTAIN_NEAR_ANGLE_VA, &k.containNearAngle_deg},
      {CONTAIN_FACING_VA, &k.containFacing_deg},
      {CONTAIN_WIDE_FROM_VA, &k.containWideFrom_m},
      {CONTAIN_CONE_VA, &k.containCone_deg},
      {CONTAIN_RADIUS_VA, &k.containRadius_m},
      {LINEUP_BAND_VA, &k.lineUpBandAbove_m},
      {LINEUP_BALL_VA, &k.lineUpBallAhead_m},
  };
  for (const auto& [va, out] : floats)
    if (!ReadF32(exe, va, out, error)) return std::nullopt;
  if (shareDen == 0.0f) {
    *error = "team concept constant: zero channel divisor";
    return std::nullopt;
  }
  k.runnerChannelShare = shareNum / shareDen;
  if (!LoadEnables(vfs, &k.enabled, error)) return std::nullopt;
  // FUN_1408C51F0's class map, read from the install with the other constants (pesroles.cpp).
  auto classMap = roles::LoadRoleClassMap(exe, error);
  if (!classMap) return std::nullopt;
  k.roleClassOfRegistered = *classMap;
  // The reclassify's table and block B's fold constants, read from the install with the class map.
  auto lines = mentality::LoadReclassifyLines(exe, error);
  if (!lines) return std::nullopt;
  k.reclassifyLines = *lines;
  auto fold = mentality::LoadFoldConstants(exe, error);
  if (!fold) return std::nullopt;
  k.fold = *fold;
  // The publisher's order table and the ordinal fill's constants, read from the install with the rest.
  auto designationConstants = designation::LoadConstants(exe, error);
  if (!designationConstants) return std::nullopt;
  k.designation = *designationConstants;
  return k;
}

namespace {

// A plan, slot or phase index inside the sheet's [3][11][3] tables (PES's bounds).
bool ValidPlan(int plan) { return plan >= 0 && plan < kSheetPlans; }
bool ValidSlotPhase(int plan, int slot, int phase) {
  return ValidPlan(plan) && slot >= 0 && slot < kSheetSlots && phase >= 0 && phase < kSheetPhases;
}
bool ValidCoord(int plan, int i, int phase) {
  return ValidPlan(plan) && i >= 0 && i < kSheetOutfield && phase >= 0 && phase < kSheetPhases;
}
int SlotPhaseIndex(int plan, int slot, int phase) {
  return (plan * kSheetSlots + slot) * kSheetPhases + phase;
}
int CoordIndex(int plan, int i, int phase) { return (plan * kSheetOutfield + i) * kSheetPhases + phase; }
// A roster index below kSheetRosterBound, or the "no player" value (FUN_1408c5c50's test).
bool RosterOrNone(int v) { return (v >= 0 && v < kSheetRosterBound) || v == kSheetNone; }

// The instruction ids the writer FUN_140a42600 branches on.
constexpr int kInstrNone = 0;        // leaves the flag byte alone
constexpr int kInstrSlotFlag8 = 8;   // marks the argument's slot
constexpr int kInstrSlotFlagA = 0xa; // marks the argument's slot
constexpr int kInstrSlotFlagF = 0xf; // marks the argument's slot
constexpr int kInstrArgument = 0xe;  // stores the argument at +0x2c

}  // namespace

// --- the sheet image ---------------------------------------------------------

int TeamSheet::ResolvePlan(int plan) const { return plan == kCurrentPlan ? Plan() : plan; }

uint32_t TeamSheet::ReadU32(int offset) const {
  uint32_t v = 0;
  std::memcpy(&v, bytes_.data() + offset, sizeof v);
  return v;
}

void TeamSheet::WriteU32(int offset, uint32_t value) {
  std::memcpy(bytes_.data() + offset, &value, sizeof value);
}

uint16_t TeamSheet::ReadU16(int offset) const {
  uint16_t v = 0;
  std::memcpy(&v, bytes_.data() + offset, sizeof v);
  return v;
}

void TeamSheet::WriteU16(int offset, uint16_t value) {
  std::memcpy(bytes_.data() + offset, &value, sizeof value);
}

int TeamSheet::Plan() const { return static_cast<int>(ReadU32(kSheetPlanOffset)); }

void TeamSheet::SetPlan(int plan) {  // FUN_1408c5e20
  if (ValidPlan(plan)) WriteU32(kSheetPlanOffset, static_cast<uint32_t>(plan));
}

int TeamSheet::Slider(int k, int plan) const {  // FUN_1408c52e0: 0 when refused
  plan = ResolvePlan(plan);
  if (!ValidPlan(plan) || k < 0 || k >= kSheetSliders) return 0;
  return bytes_[kSheetSliderOffset + kSheetSliders * plan + k];
}

void TeamSheet::SetSlider(int k, int value, int plan) {  // FUN_1408c5df0
  plan = ResolvePlan(plan);
  if (!ValidPlan(plan) || k < 0 || k >= kSheetSliders) return;
  bytes_[kSheetSliderOffset + kSheetSliders * plan + k] = static_cast<uint8_t>(value);
}

int TeamSheet::Toggle(int k, int plan) const {  // FUN_1408c5030: 0 when refused
  plan = ResolvePlan(plan);
  if (!ValidPlan(plan) || k < 0 || k >= kSheetToggleCount) return 0;
  return bytes_[kSheetToggleOffset + kSheetToggleCount * plan + k];
}

void TeamSheet::SetToggle(int k, int value, int plan) {  // FUN_1408c5c70
  plan = ResolvePlan(plan);
  if (!ValidPlan(plan) || k < 0 || k >= kSheetToggleCount) return;
  // Keeps 0 and 1; anything above 1 stores 1.
  const bool keep = value >= 0 && value < 2;
  bytes_[kSheetToggleOffset + kSheetToggleCount * plan + k] = keep ? static_cast<uint8_t>(value) : 1;
}

int TeamSheet::RegisteredRole(int slot, int phase, int plan) const {  // FUN_1408c51b0
  plan = ResolvePlan(plan);
  if (!ValidSlotPhase(plan, slot, phase)) return kSheetRoleDefault;
  return static_cast<int>(ReadU32(kSheetRegisteredRoleOffset + 4 * SlotPhaseIndex(plan, slot, phase)));
}

int TeamSheet::Role(int slot, int phase, int plan) const {  // FUN_1408c5310
  plan = ResolvePlan(plan);
  if (!ValidSlotPhase(plan, slot, phase)) return kSheetRoleDefault;
  return static_cast<int>(ReadU32(kSheetRoleOffset + 4 * SlotPhaseIndex(plan, slot, phase)));
}

void TeamSheet::SetRole(int slot, int role, int phase, int plan) {  // FUN_1408c5e90
  plan = ResolvePlan(plan);
  if (!ValidSlotPhase(plan, slot, phase) || role < 0 || role >= kSheetRoleBound) return;
  WriteU32(kSheetRoleOffset + 4 * SlotPhaseIndex(plan, slot, phase), static_cast<uint32_t>(role));
}

void TeamSheet::SetRegisteredRole(int slot, int role, int phase, int plan) {
  plan = ResolvePlan(plan);
  if (!ValidSlotPhase(plan, slot, phase) || role < 0 || role >= kSheetRoleBound) return;
  WriteU32(kSheetRegisteredRoleOffset + 4 * SlotPhaseIndex(plan, slot, phase),
           static_cast<uint32_t>(role));
}

int TeamSheet::CoordX(int i, int phase, int plan) const {  // FUN_1408c5130: 0 when refused
  plan = ResolvePlan(plan);
  if (!ValidCoord(plan, i, phase)) return 0;
  return ReadU16(kSheetCoordXOffset + 2 * CoordIndex(plan, i, phase));
}

int TeamSheet::CoordY(int i, int phase, int plan) const {  // FUN_1408c5170: 0 when refused
  plan = ResolvePlan(plan);
  if (!ValidCoord(plan, i, phase)) return 0;
  return ReadU16(kSheetCoordYOffset + 2 * CoordIndex(plan, i, phase));
}

void TeamSheet::SetCoord(int i, int x, int y, int phase, int plan) {  // FUN_1408c5d50
  plan = ResolvePlan(plan);
  if (!ValidCoord(plan, i, phase) || x < 0 || x >= kSheetCoordXBound || y < 0 ||
      y >= kSheetCoordYBound)
    return;
  WriteU16(kSheetCoordXOffset + 2 * CoordIndex(plan, i, phase), static_cast<uint16_t>(x));
  WriteU16(kSheetCoordYOffset + 2 * CoordIndex(plan, i, phase), static_cast<uint16_t>(y));
}

std::array<uint32_t, 3> TeamSheet::SlotRecordA(int slot, int phase, int plan) const {  // FUN_1408c50e0
  plan = ResolvePlan(plan);
  // Refused reads hand back record 0 of the table, as PES's pointer does.
  const int index = ValidSlotPhase(plan, slot, phase) ? SlotPhaseIndex(plan, slot, phase) : 0;
  const int at = kSheetSlotRecordAOffset + 12 * index;
  return {ReadU32(at), ReadU32(at + 4), ReadU32(at + 8)};
}

std::array<uint32_t, 3> TeamSheet::SlotRecordB(int slot, int phase, int plan) const {
  plan = ResolvePlan(plan);
  if (!ValidSlotPhase(plan, slot, phase)) return {};
  const int at = kSheetSlotRecordBOffset + 12 * SlotPhaseIndex(plan, slot, phase);
  return {ReadU32(at), ReadU32(at + 4), ReadU32(at + 8)};
}

void TeamSheet::SetSlotRecordB(int slot, int phase, const std::array<uint32_t, 3>& rec,
                               int plan) {  // FUN_1408c5e30
  plan = ResolvePlan(plan);
  if (!ValidSlotPhase(plan, slot, phase)) return;
  const int at = kSheetSlotRecordBOffset + 12 * SlotPhaseIndex(plan, slot, phase);
  for (int w = 0; w < 3; ++w) WriteU32(at + 4 * w, rec[w]);
}

int TeamSheet::SlotTarget(int slot, int plan) const {  // FUN_1408c50a0: none when refused
  plan = ResolvePlan(plan);
  if (!ValidPlan(plan) || slot < 0 || slot >= kSheetSlots) return kSheetNone;
  return static_cast<int>(ReadU32(kSheetSlotTargetOffset + 4 * (plan * kSheetSlots + slot)));
}

void TeamSheet::SetSlotTarget(int slot, int target, int plan) {  // FUN_1408c5d10
  plan = ResolvePlan(plan);
  if (!ValidPlan(plan) || slot < 0 || slot >= kSheetSlots) return;
  const bool aSlot = target >= 0 && target < kSheetTargetBound;
  if (!aSlot && target != kSheetNone) return;
  WriteU32(kSheetSlotTargetOffset + 4 * (plan * kSheetSlots + slot), static_cast<uint32_t>(target));
}

int TeamSheet::Captain() const { return static_cast<int>(ReadU32(kSheetCaptainOffset)); }

void TeamSheet::SetCaptain(int roster) {  // FUN_1408c5c50
  if (RosterOrNone(roster)) WriteU32(kSheetCaptainOffset, static_cast<uint32_t>(roster));
}

int TeamSheet::Designated(int k) const {  // FUN_1408c5080: none when refused
  if (k < 0 || k >= kSheetDesignated) return kSheetNone;
  return static_cast<int>(ReadU32(kSheetDesignatedOffset + 4 * k));
}

void TeamSheet::SetDesignated(int k, int roster) {  // FUN_1408c5cf0
  if (k < 0 || k >= kSheetDesignated || !RosterOrNone(roster)) return;
  WriteU32(kSheetDesignatedOffset + 4 * k, static_cast<uint32_t>(roster));
}

int TeamSheet::Taker(int k) const {  // FUN_1408c5060: none when refused
  if (k < 0 || k >= kSheetTakers) return kSheetNone;
  return static_cast<int>(ReadU32(kSheetTakerOffset + 4 * k));
}

void TeamSheet::SetTaker(int k, int roster) {  // FUN_1408c5cb0
  if (k < 0 || k >= kSheetTakers || !RosterOrNone(roster)) return;
  WriteU32(kSheetTakerOffset + 4 * k, static_cast<uint32_t>(roster));
}

// FUN_1408c6430: record (i, plan) at +0xef8 + 0x70 * (i + 4 * plan); a refused index
// addresses the base, which is record 0.
int TeamSheet::RecordOffset(int i, int plan) const {
  plan = ResolvePlan(plan);
  if (!ValidPlan(plan) || i < 0 || i >= kSheetInstructionsPerPlan) return kSheetInstructionOffset;
  return kSheetInstructionOffset + kSheetInstructionBytes * (i + kSheetInstructionsPerPlan * plan);
}

int TeamSheet::InstructionId(int i, int plan) const {
  return static_cast<int>(ReadU32(RecordOffset(i, plan) + kInstrIdOffset));
}

int TeamSheet::InstructionSlotFlag(int i, int slot, int plan) const {
  if (slot < 0 || slot >= kSheetSlots) return 0;
  return bytes_[RecordOffset(i, plan) + kInstrSlotFlagOffset + slot];
}

int TeamSheet::InstructionArg(int i, int plan) const {
  return static_cast<int>(ReadU32(RecordOffset(i, plan) + kInstrArgOffset));
}

int TeamSheet::InstructionParam(int i, int plan) const {
  return static_cast<int>(ReadU32(RecordOffset(i, plan) + kInstrParamOffset));
}

int TeamSheet::InstructionByte30(int i, int plan) const {
  return bytes_[RecordOffset(i, plan) + kInstrByte30Offset];
}

int TeamSheet::InstructionFlag(int i, int plan) const {
  return bytes_[RecordOffset(i, plan) + kInstrFlagOffset];
}

void TeamSheet::SetInstruction(int i, int id, int arg, int param, int plan, int flag) {  // FUN_140a42600
  if (i < 0 || i >= kSheetInstructionsPerPlan || id == kSheetRefusedInstructionId) return;
  const int rec = RecordOffset(i, plan);
  WriteU32(rec + kInstrIdOffset, static_cast<uint32_t>(id));
  WriteU32(rec + kInstrParamOffset, static_cast<uint32_t>(param));
  // Dwords +4..+0x2b (the slot flags and the argument) are cleared; the argument is 0xff.
  std::memset(bytes_.data() + rec + kInstrSlotFlagOffset, 0, kInstrArgOffset - kInstrSlotFlagOffset);
  WriteU32(rec + kInstrArgOffset, kSheetNone);
  if (id == kInstrArgument) {
    if (arg < kSheetArgBound) WriteU32(rec + kInstrArgOffset, static_cast<uint32_t>(arg));
  } else if (id == kInstrSlotFlagA || id == kInstrSlotFlagF || id == kInstrSlotFlag8) {
    if (arg >= 0 && arg < kSheetArgBound) bytes_[rec + kInstrSlotFlagOffset + arg % kSheetSlots] = 1;
  }
  if (id == kInstrNone) return;  // the flag byte is untouched for id 0
  bytes_[rec + kInstrFlagOffset] = static_cast<uint8_t>(flag);
}

void TeamSheet::SetInstructionByte30(int i, int value, int plan) {  // FUN_140a42880
  bytes_[RecordOffset(i, plan) + kInstrByte30Offset] = static_cast<uint8_t>(value);
}

void TeamSheet::ResetInstruction(int i, int plan) {  // FUN_140a42430
  const int rec = RecordOffset(i, plan);
  WriteU32(rec + kInstrIdOffset, 0);
  std::memset(bytes_.data() + rec + kInstrSlotFlagOffset, 0, kInstrArgOffset - kInstrSlotFlagOffset);
  WriteU32(rec + kInstrArgOffset, kSheetNone);
  bytes_[rec + kInstrByte30Offset] = 0;
  WriteU32(rec + kInstrParamOffset, 0);
  WriteU32(rec + kInstrResetNoneOffset, kSheetNone);
  bytes_[rec + kInstrFlagOffset] = 0;
}

TeamSheet DefaultTeamSheet(const TeamConceptConstants& k) {
  // ponytail: the constructor builds its instruction-record defaults from stack locals
  // (0x1408c5600..0x1408c5636, not traced here). All twelve records start in the reset state
  // (+0x2c and +0x38 = 0xff, the rest 0), which matches the 0xff stores it makes. The inert
  // words at +0x1470..+0x1486 are not modelled, so they stay zero.
  TeamSheet s;  // the zero image: the constructor's zero stores
  // The constructor's stores are for plan 0, which is current at construction.
  for (int i = 0; i < kSheetSliders; ++i) s.SetSlider(i, k.defaultSliders[i], 0);
  s.SetCaptain(k.defaultCaptain);
  for (int d = 0; d < kSheetDesignated; ++d) s.SetDesignated(d, k.defaultDesignated[d]);
  for (int t = 0; t < kSheetTakers; ++t) s.SetTaker(t, k.defaultTaker[t]);
  for (int plan = 0; plan < kSheetPlans; ++plan) {
    for (int slot = 0; slot < kSheetSlots; ++slot) s.SetSlotTarget(slot, k.defaultSlotTarget, plan);
    for (int i = 0; i < kSheetInstructionsPerPlan; ++i) s.ResetInstruction(i, plan);
  }
  return s;
}

bool ConceptGate(int id, const TeamSheet& sheet) {
  switch (id) {
    case kConceptSideCounter:  // 0x140543f40: Attacking Style == Counter Attack
      return sheet.Toggle(kAttackingStyle) == 0;
    case kConceptForeCheckPress:  // 0x140574910: Defensive Style == Frontline Pressure
    case kConceptForeCheckLineDown:
    case kConceptForeCheckLineUp:
    case kConceptTransitionChase:
      return sheet.Toggle(kDefensiveStyle) == 0;
    case kConceptRetreatBlockCreate:  // 0x1405767f0: All-out Defence
      return sheet.Toggle(kDefensiveStyle) == 1;
    default:
      return false;
  }
}

bool ForeCheckPressCount(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  // FUN_140574200(ctx, 0): the ball not held by the opponents -> press on.
  if (!in.opponentsHoldBall) return true;
  // Bearings from the ball, against the heading toward our own goal
  // (FUN_140a4db20). Candidates (FUN_140639830 / FUN_140639a20, decoded): own
  // outfield men on the pitch within 21.5 m of the ball or inside the
  // 18..25 m sector-assignment radius; both exceed the count's largest radius
  // (12 + 5 m), so distance alone never excludes a counted man. ponytail: the
  // one-man-per-sector rule of FUN_140639600 is not applied.
  const float toOwnGoal = AttackHeading(-in.dir);
  // FUN_140a53da0 -> FUN_140a53df0: the MATCH_UP man within 6.5 m of the ball
  // and goal-side of it (bearing ball->him within 35 deg of our goal) earns
  // the +5 m on the wide-angle radii.
  float bonus = 0.0f;
  if (in.presser >= 0 && in.presser < static_cast<int>(in.mates.size())) {
    const Vector3& u = in.mates[in.presser];
    const float d2 = DistSq(u, in.ball);
    if (d2 < k.pressLaneRadius_m * k.pressLaneRadius_m && d2 > 0.0f &&
        AngleDiff(toOwnGoal, Bearing(in.ball, u)) < k.pressGoalSide_deg * 0.5f)
      bonus = k.pressLaneBonus_m;
  }
  int count = 0;
  bool close = false;
  for (const Vector3& m : in.mates) {
    const float ang = AngleDiff(Bearing(in.ball, m), toOwnGoal);
    const float w = ang <= k.pressRampFrom_deg ? 0.0f
                    : ang >= k.pressRampTo_deg ? 1.0f
                                               : (ang - k.pressRampFrom_deg) / k.pressGoalSide_deg;
    float r = w * k.pressRadiusRamp_m + k.pressRadius_m;
    if (w > k.pressBonusAbove) r += bonus;
    if (DistSq(m, in.ball) > r * r) continue;
    if (ang < k.pressGoalSide_deg) close = true;
    if (++count >= k.pressCount && close) return true;
  }
  return false;
}

bool ForeCheckPressStart(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  if (!in.inPlay || in.sincePossessionChange_s > k.foreCheckWindow_s) return false;
  // Ball in the opponents' half, and not still inside our block: within 12 m
  // of halfway and behind our team's midpoint MA+0x3c+0x14*side (written by
  // FUN_14045bfe0: the mean of our front man and our deepest outfielder).
  const float b = in.dir * in.ball.coords[0];
  if (b < 0.0f) return false;
  if (b < k.foreCheckCorridor_m && Beyond(in.teamMid, in.ball.coords[0], in.dir, 0.0f)) return false;
  return ForeCheckPressCount(k, in);  // instruction 0xd is never armed in GF
}

bool ForeCheckPressEnd(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  if (!in.inPlay || in.sincePossessionChange_s > k.foreCheckWindow_s) return true;
  if (k.foreCheckBallFloor_m > in.dir * in.ball.coords[0]) return true;
  if (in.sincePossessionChange_s <= k.foreCheckHold_s) return false;
  return !ForeCheckPressCount(k, in);
}

namespace {
// FUN_140574a30 + FUN_140574ab0 + the trigger ORs, shared by start and end.
// FUN_140574bb0: an opponent on the ball, facing our goal, with none of our
// men in the 80 deg cone toward our goal within 4..7 m of the ball.
bool UncoveredCarrier(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  if (!in.holderOpponent) return false;
  const float toOwnGoal = AttackHeading(-in.dir);
  // Far from our line (beyond it + 12 m) or wide: nobody of ours within 3 m of
  // the ball (PES checks the ball and the holder lists; GF's holder is on it).
  const bool beyond = Beyond(in.ownLine, in.ball.coords[0], in.dir, k.uncoveredLineMargin_m);
  if (beyond || std::fabs(in.ball.coords[1]) > pesPenaltyAreaHalfWidth)
    for (const Vector3& m : in.allMen)
      if (DistSq(m, in.ball) < k.uncoveredNearSq_m2) return false;
  if (AngleDiff(toOwnGoal, in.holderHeading_deg) > k.uncoveredFacing_deg) return false;
  float w = 0.0f;
  if (in.dir * in.ball.coords[0] > in.dir * in.ownLine) {
    const float d = std::fabs(in.ball.coords[0] - in.ownLine);
    w = d <= k.uncoveredRampFrom_m ? 0.0f
        : d >= k.uncoveredRampTo_m ? 1.0f
                                   : (d - k.uncoveredRampFrom_m) / k.uncoveredRampSpan_m;
  }
  const float radius = w * k.uncoveredRadiusRamp_m + k.uncoveredRadius_m;
  return PlayersInCone(in.allMen, in.ball, toOwnGoal, k.uncoveredCone_deg, radius, 0.0f) == 0;
}

bool LineDownHolds(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  const float b = in.dir * in.ball.coords[0];
  if (pitchHalfW / k.lineDownBallDivisor > b || b > k.lineDownBallMax_m) return false;
  // |T+0x89a0 - T+0x875c|: a world x against an own-frame band, as written.
  if (std::fabs(in.measuredBack - in.backBand) < k.lineDownGap_m) return false;
  // FUN_140574940: the holder in LONG_PASS facing within 70 deg of our attack heading.
  const bool longPass = in.holderLongPass &&
                        AngleDiff(in.holderHeading_deg, AttackHeading(in.dir)) <= k.lineDownLongPassFacing_deg;
  return in.attackLevelZero || UncoveredCarrier(k, in) || longPass;
}
}  // namespace

bool ForeCheckLineDownStart(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  return !in.keeperHolds && in.inPlay && LineDownHolds(k, in);
}

bool ForeCheckLineDownEnd(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  return !in.inPlay || !LineDownHolds(k, in);
}

namespace {
// FUN_140575210: the ball wide, between own-frame -10 and 18.
bool LineUpBall(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  const float b = in.dir * in.ball.coords[0];
  return std::fabs(in.ball.coords[1]) >= pesPenaltyAreaHalfWidth - k.lineUpWideMargin_m &&
         b <= k.lineUpBallMax_m && b >= k.lineUpBallMin_m;
}

// FUN_140574f40: our MATCH_UP man contains the carrier. Within 2 m and
// goal-side within 85 deg; else the carrier faces away from our goal (>= 135
// deg off it) and the man stands goal-side within 45 deg inside 3..5 m.
bool Containing(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  if (in.presser < 0 || in.presser >= static_cast<int>(in.mates.size()) || !in.holderOpponent) return false;
  const Vector3& u = in.mates[in.presser];
  const float d2 = DistSq(u, in.ball);
  if (d2 > k.containFarSq_m2) return false;
  const float toOwnGoal = AttackHeading(-in.dir);
  if (d2 < k.containNearSq_m2 && AngleDiff(Bearing(in.holder, u), toOwnGoal) < k.containNearAngle_deg) return true;
  if (AngleDiff(in.holderHeading_deg, toOwnGoal) < k.containFacing_deg) return false;
  const float z = std::fabs(in.ball.coords[1]);
  const float w = z <= k.containWideFrom_m        ? 0.0f
                  : z >= pesPenaltyAreaHalfWidth ? 1.0f
                                                 : (z - k.containWideFrom_m) / (pesPenaltyAreaHalfWidth - k.containWideFrom_m);
  constexpr float kRadiusRampScale = 2.0f;  // addss xmm6,xmm6 at 0x140575193
  return PlayersInCone({u}, in.ball, toOwnGoal, k.containCone_deg, w * kRadiusRampScale + k.containRadius_m,
                       0.0f) > 0;
}
}  // namespace

bool ForeCheckLineUpStart(const TeamConceptConstants& k, const DefenceConceptInput& in, bool lineDownRunning) {
  return !in.keeperHolds && in.inPlay && !lineDownRunning && LineUpBall(k, in) && Containing(k, in);
}

bool ForeCheckLineUpEnd(const TeamConceptConstants& k, const DefenceConceptInput& in, bool lineDownRunning) {
  return !in.inPlay || lineDownRunning || !LineUpBall(k, in) || !Containing(k, in);
}

float LineUpBackEdge(const TeamConceptConstants& k, float ballOwn, float preBand, float edge) {
  const float x = std::min(ballOwn + k.lineUpBallAhead_m, std::min(k.pushUpCap_m, preBand + k.lineUpBandAbove_m));
  return x > edge ? x : edge;
}

bool RetreatBlockCreateEnd(const TeamConceptConstants& k, const DefenceConceptInput& in) {
  if (!in.inPlay) return true;
  const float a = in.dir * in.measuredBack, b = in.dir * in.measuredMid;
  if (std::fabs(a - in.backBand) < k.retreatGap_m && std::fabs(b - in.midBand) < k.retreatGap_m) return true;
  return k.retreatBackFront_m - pesPenaltyAreaLineX > a && k.retreatMidFront_m - pesPenaltyAreaLineX > b;
}

float BandPushUp_m(const TeamConceptConstants& k, float measuredBackOwn, float ballOwn, float backBand) {
  // ponytail: the -15 m variant behind ctx+0x138 (a flag with no writer found
  // in the shape pass) is not taken; FUN_14063fdc0's kick-prediction point is
  // the current ball.
  float x = measuredBackOwn + k.pushUpBehind_m;
  x = std::min(x, ballOwn - k.pushUpBallGap_m);
  x = std::min(x, k.pushUpCap_m);
  return x > backBand ? x - backBand : 0.0f;
}

std::optional<ChasePick> TransitionChaseStart(const TeamConceptConstants& k, const TransitionChaseInput& in) {
  if (!in.inPlay || in.sincePossessionChange_s > k.chaseWindow_s || in.carrierKicking) return std::nullopt;
  const Vector3& c = in.carrier;
  // Our attacking third and wide.
  if (pesPenaltyAreaLineX - k.chaseThirdMargin_m > in.dir * c.coords[0]) return std::nullopt;
  if (pesPenaltyAreaHalfWidth - k.chaseWideMargin_m > std::fabs(c.coords[1])) return std::nullopt;
  if (in.chaser < 0 || in.chaser >= static_cast<int>(in.men.size())) return std::nullopt;
  const ChaseMan& u = in.men[in.chaser];
  if (u.falling || DistSq(u.pos, c) > k.chaseRangeSq_m2) return std::nullopt;
  // Goal-side of the carrier (toward where the opponents attack) and inside him.
  const float bearing = Bearing(c, u.pos);
  if (AngleDiff(AttackHeading(-in.dir), bearing) > k.chaseGoalSideMax_deg) return std::nullopt;
  const float outward = c.coords[1] > 0.0f ? kPlusY_deg : kHalfTurn_deg;
  if (k.chaseInsideMin_deg > AngleDiff(outward, bearing)) return std::nullopt;
  // FUN_140575930: the widest available man in the carrier's distance list,
  // wider than |u.y| - 15, until the list passes 20 m.
  int second = -1;
  float widest = std::fabs(u.pos.coords[1]) - k.chaseSecondWidthSlack_m;
  for (int i = 0; i < static_cast<int>(in.men.size()); ++i) {
    const ChaseMan& m = in.men[i];
    if (DistSq(m.pos, c) > k.chaseSecondRangeSq_m2) break;
    if (i == in.chaser || !m.available || m.falling) continue;
    const float w = std::fabs(m.pos.coords[1]);
    if (w > widest) widest = w, second = i;
  }
  if (second < 0) return std::nullopt;
  return ChasePick{in.chaser, second};
}

Vector3 TransitionChaseTarget(const TeamConceptConstants& k, const Vector3& chaser, const Vector3& carrier,
                              float dir) {
  // From the chaser (record +0x1c), the own attack heading turned 160 deg so
  // the point lies behind him on the carrier's touchline side.
  const float turn = SideSign(carrier.coords[1]) * dir < 0.0f ? 1.0f : -1.0f;
  Vector3 p = Advance(chaser, Wrap360(AttackHeading(dir) + turn * k.chaseTargetTurn_deg), k.chaseTargetDistance_m);
  if (std::fabs(p.coords[0]) > pitchHalfW) p.coords[0] = SideSign(p.coords[0]) * pitchHalfW;
  if (std::fabs(p.coords[1]) > pitchHalfH) p.coords[1] = SideSign(p.coords[1]) * pitchHalfH;
  return p;
}

std::optional<int> SideCounterStart(const TeamConceptConstants& k, const SideCounterInput& in) {
  if (!in.attackAllowed || in.sincePossessionChange_s > k.counterWindow_s ||
      in.sinceKickoff_s < k.counterKickoffGuard_s || in.holder < 0 || in.keeperHolds || in.ownKeeperPunting)
    return std::nullopt;
  if (AngleDiff(in.holderHeading_deg, AttackHeading(in.dir)) > k.counterFacingMax_deg) return std::nullopt;
  // ponytail: FUN_140a4e720's forward-cone occupancy test (150, 4 m) is not
  // reduced; the goal-side count below carries the "is it on" judgement.
  const float depth = in.dir * in.ball.coords[0];
  if (depth > k.counterBallMaxDepth_m || -pesPenaltyAreaLineX > depth) return std::nullopt;
  if (std::fabs(in.ball.coords[1]) > pesPenaltyAreaHalfWidth) return std::nullopt;
  int goalSide = 0;
  for (const Vector3& o : in.opponents) {
    if (Beyond(in.oppLineX, o.coords[0], in.dir, k.counterLineMargin_m)) ++goalSide;
    if (SideSign(o.coords[1]) == SideSign(in.ball.coords[1]) &&
        Beyond(in.oppLineX, o.coords[0], in.dir, k.counterAbortLineMargin_m) &&
        pesPenaltyAreaHalfWidth - k.counterAbortWideMargin_m <= std::fabs(o.coords[1]))
      return std::nullopt;
  }
  if (goalSide > k.counterMaxGoalSide) return std::nullopt;
  // Runner (FUN_14053e9a0). ponytail: the run-style multipliers, the
  // overridable-state vetoes and the covered-channel flags are not reduced;
  // the scorer keeps its spine - a free wide/forward man, distance to his
  // channel point, the Attacking Area lane factor - lowest wins.
  int runner = -1;
  float best = std::numeric_limits<float>::max();
  for (int i = 0; i < static_cast<int>(in.men.size()); ++i) {
    const CounterMan& m = in.men[i];
    if (i == in.holder || !m.wideOrForward || m.nearestOpponentSq_m2 <= k.runnerUnmarkedSq_m2) continue;
    float score = std::sqrt(DistSq(m.pos, SideCounterRunTarget(k, m.pos, in.oppLineX)));
    const bool wideMan = std::fabs(m.pos.coords[1]) >= k.runnerWideLane_m;
    if (wideMan) score *= in.attackingArea == 0 ? k.runnerAreaFactor : k.runnerCentreFactor;
    if (score < best) best = score, runner = i;
  }
  if (runner < 0) return std::nullopt;
  return runner;
}

Vector3 SideCounterRunTarget(const TeamConceptConstants& k, const Vector3& runner, float oppLineX) {
  const float channel =
      pesPenaltyAreaHalfWidth + (pitchHalfH - pesPenaltyAreaHalfWidth) * k.runnerChannelShare;
  float y = SideSign(runner.coords[1]) * channel;
  if (std::fabs(runner.coords[1]) > std::fabs(y)) y = runner.coords[1];
  return Vector3(oppLineX, y, 0.0f);
}

CounterSupport SideCounterSupporters(const TeamConceptConstants& k, const SideCounterInput& in, int runner,
                                     const std::array<int, 3>& previous) {
  CounterSupport s;
  const float attack = AttackHeading(in.dir);
  s.points[0] = Advance(in.ball, attack, k.supportAhead_m);
  s.points[1] = Advance(in.ball, TurnToward(kHalfTurn_deg, attack, k.supportTurn_deg), k.supportSide_m);
  s.points[2] = Advance(in.ball, TurnToward(kPlusY_deg, attack, k.supportTurn_deg), k.supportSide_m);
  constexpr float kNoneScore = 100000.0f;  // 0x1425c65fc
  for (int slot = 0; slot < 3; ++slot) {
    float best = kNoneScore;
    for (int i = 0; i < static_cast<int>(in.men.size()); ++i) {
      const CounterMan& m = in.men[i];
      if (i == in.holder || i == runner || !m.supportEligible) continue;
      if (std::find(s.men.begin(), s.men.begin() + slot, i) != s.men.begin() + slot) continue;
      if (slot == 0 && !Beyond(in.ball.coords[0], m.pos.coords[0], in.dir, k.supportMinAhead_m)) continue;
      if (slot == 1 && m.pos.coords[1] > in.ball.coords[1]) continue;
      if (slot == 2 && m.pos.coords[1] < in.ball.coords[1]) continue;
      float d = std::sqrt(DistSq(m.pos, s.points[slot]));
      if (previous[slot] == i) d += k.supportHoldBonus_m;
      if (d < best) best = d, s.men[slot] = i;
    }
  }
  return s;
}

bool SideCounterEnd(const TeamConceptConstants& k, bool inPlay, bool runnerStillRunning, float age_s,
                    const Vector3& ball, const Vector3& runner) {
  if (!inPlay || !runnerStillRunning) return true;
  if (age_s > k.counterRunGrace_s) {
    // PES compares the root distance against 1369 (37 squared) as written.
    if (std::sqrt(DistSq(ball, runner)) > k.counterRunnerFarSq ||
        std::fabs(ball.coords[0] - runner.coords[0]) > k.counterRunnerMaxDx_m)
      return true;
  }
  // ponytail: the trailing overlap test (a wider team-mate within 8 m of the
  // runner's lane, FUN_1405a4830/FUN_140596c30) is not reduced.
  return age_s > k.counterMaxAge_s;
}

}  // namespace pes
