#pragma once

// PES 2021's DODGE action (docs/re/pes-re-contact-port.md pass 4 stage B):
// the four dodge predicates (ball, goal net, post, human) and the four clip
// selectors, as a pure layer over pes::Animetable + pes::Mbinfo.
//
// The Dodge action runs the anime layer's slot 13 (FUN_14073bc70): the
// predicate that fired has set the dodge kind (cmd+0x1094), the kind picks
// the selector, and the selector picks a row/column of the Animetable, which
// gives the motion number and mirror bit. The gate FUN_14073cbc0 admits the
// action (2-10, 0x14, 0x4f through the action's own slot 0x70, 0xd after
// ap+0xae4, 0x49/0x4a always).
//
// Where the numbers live: the exe's .rdata (PeImage::ReadVa, each through an
// opcode-checked instruction that references it) and the install's
// Animetable.bin + Mbinfo bins. No PES number tables are hard-coded here
// except the row ids the kind->row and ball-grid tables themselves carry.

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "pes/animetable.hpp"
#include "pes/exe.hpp"
#include "pes/mbinfo.hpp"

namespace pes {

// The four dodge kinds, cmd+0x1094 (FUN_14073bc70's dispatch).
enum class DodgeKind : int { kBall = 1, kNet = 2, kPost = 3, kHuman = 5 };

// FUN_14073a3d0: the human dodge kind 0..7 (8 = none).
enum class HumanDodgeKind : int {
  kJumpLow = 0,
  kJump = 1,
  kJumpHigh = 2,
  kSlide = 3,
  kSlideRapidUp = 4,
  kSlideRapidDown = 5,
  kStop = 6,
  kDecelerate = 7,
  kNone = 8
};

struct DodgeConstants {
  // FUN_140739f90's kind -> row table: int[8][3], indexed [kind][speedClass].
  std::array<std::array<int, 3>, 8> kindRow{};
  // FUN_1407358e0's ball grid: rows [band][speedClass], 3 columns wide.
  std::array<std::array<int, 3>, 4> ballGrid{};
  int ballRow4A = -1, ballRow4B = -1, ballRow4C = -1;  // 0x306 / 0x30a / 0x30a
  // FUN_140a6d380's band table, 12 thresholds x scale (contact-2 §3).
  std::array<float, 12> ballHeightBands{};
  // FUN_14073c5d0's net rows: default and the fast variant.
  int netRowDefault = -1, netRowFast = -1;
  // FUN_1407368d0's post grid: 3 angular sides x the 3 speed classes.
  // [2][2] is not carried in the exe's table (the ninth int there is the next
  // constant's 8.0f); the selector's own stack value 0x316 stands in for it.
  std::array<std::array<int, 3>, 3> postGrid{{{0x2fb, 0x2fb, 0x2fc},
                                              {0x313, 0x313, 0x314},
                                              {0x315, 0x315, 0x316}}};
  // FUN_1407368d0's geometry: the 8 km/h speed-class floor, the 30-deg deep
  // split, the 45-deg near/far gate, the 135-deg column top, and the far
  // branch's 60/120 window.
  float postSpeedFloor_kmh = 0.0f;   // 8
  float postSideAngle_deg = 0.0f;    // 30
  float postColumnUp_deg = 0.0f;     // 45, above which the column steps up
  float postColumnHigh_deg = 0.0f;   // 135, the column's top band
  float postColumnBailout_deg = 0.0f;  // 60, the far branch's low edge
  float postBailout_deg = 0.0f;      // 120, the far branch's high edge
  // FUN_140735870: kind -> column.
  std::array<int, 9> kindColumn{};
  // FUN_140736ce0's look-ahead offset (seconds), drawn per frame.
  float lookAheadOffset_s = 0.0f;  // gs·12/60 when FUN_140736d30 in {0,1,2}
  // FUN_140736d30's speed classes (km/h).
  float speedClass1_kmh = 0.0f, speedClass2_kmh = 0.0f;
  // FUN_140735700's per-class clamp window (deg), and the "clamped" epsilon.
  std::array<std::array<float, 2>, 3> mirrorClamp{};  // [class][lo, hi]
  float mirrorEpsilon_deg = 0.0f;
  // FUN_14073a3d0's distances (m) and angles (deg).
  float humanDistSq = 0.0f;       // 9.0: opponent within 3 m
  float behindAngle_deg = 0.0f;   // 150
  float bearingAngle_deg = 0.0f;  // 45
  // FUN_1406e6bf0: the human selector's per-player draw modulus (stream 2).
  int humanDrawModulus = 1000;
  // The ball dodge's eligibility and its two sub-tests (FUN_140a07bf0,
  // FUN_140a07570, FUN_140a07880).
  // The class table's bit word per action class, the table FUN_140a6e470 and
  // its two siblings read (0x1426484f0, 94 u32s).
  std::array<uint32_t, 94> roleBits{};
  // FUN_140a07bf0's last-event window: the frame the eligibility waits is
  // (rng * 5) / 48, with rng the game-speed divisor (gsInt, 54).
  int lastEventFramesPerRoll = 5;   // `lea ecx,[rax+rax*4]` @0x140a07da3
  int lastEventRollDivisor = 48;    // `mul ecx, 0xaaaaaaab; shr edx,5` @0x140a07da6
  // The human gate's window factor (DAT_142598578 = 0.2, FUN_140a0b350).
  float lastEventWindowFactor = 0.0f;
  // FUN_140a07570's ball-speed floor (DAT_14263f000, m/s).
  float ballSpeedFloor_mps = 0.0f;
  // FUN_140a07570's reach test: the look-ahead height against the player's
  // reach (0x14256ae18 m per reach unit) plus the 0x142590d98 margin.
  float ballReachPerUnit_m = 0.0f;
  float ballReachMargin_m = 0.0f;
  // FUN_140a070e0's lead time: the ball dodge entry looks this far (seconds) along the ball's step (0x142590d98, the same
  // word the reach margin reads).
  float ballLead_s = 0.0f;
  // FUN_140a07570's two window tests: the bearing window off his heading
  // (0x142588210, deg) and the same window on the defend-goal axis
  // (0x14259be5c, deg).
  float ballWindowHeading_deg = 0.0f;
  float ballWindowGoal_deg = 0.0f;
  // FUN_140a07880's goal-mouth window: the three x at the mouth's edges
  // (0x14259bde8 = the post offset past the line, 0x14259bdd0 the look-ahead
  // seconds) and the two kinds that read the mouth (8..10).
  float goalMouthPostOffset_m = 0.0f;
  float goalMouthLookAhead_s = 0.0f;
  // FUN_140a07e40's and FUN_140a084d0's own edge (both gates read
  // DAT_14256ae1c = 1): the dodger stands within this of his goal line, and
  // his z within this of his post.
  float netEdgeMargin_m = 0.0f;
  // His speed has to clear 100 / (gs * 72) per frame (DAT_14253ee30 over
  // DAT_1425cb47c, gs = 54): 100 / 72 m/s once the gs cancels. Both gates.
  float netStepDivisor = 72.0f;  // DAT_1425cb47c
  float netStepNumerator = 100.0f;  // DAT_14253ee30
  // The residual bearing past the edge (DAT_14259be0c). The net rejects a
  // residual below it; the post admits a residual at or below it.
  float netEdgeRejection_deg = 0.0f;
  // FUN_140a084d0's distances: he must be within postProximity_m of the post
  // (DAT_14259222c = 5); within postResidualProximity_m of it on either axis
  // (DAT_142565600 = 0.5) admits him without the bearing test; a ball at the
  // post within postBallNear_m (DAT_142565604 = 2) keeps the designated man out.
  float postProximity_m = 0.0f;
  float postResidualProximity_m = 0.0f;
  float postBallNear_m = 0.0f;
};

std::optional<DodgeConstants> LoadDodgeConstants(const PeImage& exe, std::string* error);

// One selected clip: the Animetable cell plus its row (for the tests).
struct DodgeClip {
  int motionNo = -1;
  bool mirror = false;
  int row = -1, col = -1;
};

// FUN_1407358e0: the ball selector. The row comes from the caller's forced set
// (the row table the selector builds on the stack); the column is 9.
std::optional<DodgeClip> BallSelector(const Animetable& t, const DodgeConstants& k, int speedClass,
                                      const std::vector<int>& forcedRow);

// FUN_14073c5d0: the goal-net selector. `fast` is speed > 22 km/h.
std::optional<DodgeClip> NetSelector(const Animetable& t, const DodgeConstants& k, bool fast);

// FUN_1407368d0: the post selector. `bearing_deg` is the raw 0..360 bearing
// from the post's anchor to the player, `refAngle_deg` the post's own facing
// (the exe's two reads: XMM9 = |wrap180(bearing - refAngle)| gates the near/far
// split, the two 30-deg tests on the raw bearing pick the deep side), and
// `approachRel_deg` the angle the ball comes in at relative to the same axis,
// which picks the column (9 under 45, 13 above it, 17 above 135).
std::optional<DodgeClip> PostSelector(const Animetable& t, const DodgeConstants& k, float bearing_deg,
                                      float refAngle_deg, float approachRel_deg, float speed_kmh);

// The three angles the post dodge reads, from GF's own pitch marks. The goal
// sign is load-bearing: a team's own goal is at +side * pitchHalfW (the
// throw-in's defending branch runs `backXBound` at ballPos + 30 * side, and the
// corner's defending branch bounds the own half at side * pitchHalfW), so an
// inversion here would move both near-goal dodges to the wrong end. The post
// picked is the one on the ball's half of the goal line.
struct PostAngles {
  float ownGoalX_m = 0.0f;      // +side * pitchHalfWidth
  float bearing_deg = 0.0f;     // raw, 0..360, from the post to the player
  float refAngle_deg = 0.0f;    // the goal line's own facing
  float approachRel_deg = 0.0f;  // the ball's approach, relative to it
};
PostAngles PostAnglesFor(int side, float playerX, float playerY, float ballX, float ballY,
                         float pitchHalfWidth_m, float goalHalfWidth_m);

// FUN_140735870: kind -> column.
int KindColumn(const DodgeConstants& k, int kind);

// FUN_140739f90: kind -> row, with the ball owner's column-9 hit-window test.
// `ballHeight_m` is the ball height; the non-owner path skips the fit test.
int KindRow(const Animetable& t, const Mbinfo& mb, const DodgeConstants& k, int kind,
            float speed_mps, bool ballOwner, float ballHeight_m);

// FUN_140736d30: the speed class 0/1/2.
int SpeedClass(float speed_kmh, float headingDelta_deg);

// FUN_140a7d380: the ball-height band.
int BallBand(const DodgeConstants& k, float height_m, float bodyScale);

// FUN_140a07bf0: the ball dodge's eligibility. PES reads the player's action
// class from the role-property table and his team's records; here the class is
// an input because GF's own anime layer names it, and the two team records
// come from the caller (the designated ball-owner slot and the last-action
// player), which GF carries as the designated possession player.
struct BallEligibilityInput {
  int roleClass = -1;        // the player's action class (0..93), the table's index
  bool designatedOwner = false;  // +0x18->0x2600c == playerId: he is the team's designated owner
  int slot = -1;             // FUN_140a62d20's per-player slot; < 0 when he has none
  bool ownsBall = false;     // +0xe0->0x4: he holds the ball
  int teamIndex = -1;        // 0 or 1, which half of the pitch's record set is his
  bool perTeamActionAllowed = false;  // teamRecord->0x255: the team's own gate
  int actionSlot = -1;       // FUN_14043b870's action-list index (0x18 = none)
  int stateAction = -1;      // stateRecord->0x5d0
  bool lastActionPlayer = false;  // teamRecord->0x78 == playerId
  int framesSinceEvent = 0;  // (currentFrame - stateRecord->0x10), the last event
};

// The three PES class tests the eligibility's head runs (FUN_140a6e470 /
// 140a6e3f0 / 140a6e5e0 over the role table at 0x1426484f0), as one function
// over the raw class bit word so the caller reads the table once.
bool BallClassAdmits(const DodgeConstants& k, int roleClass);

bool BallDodgeEligible(const DodgeConstants& k, const BallEligibilityInput& in);

// FUN_140a070e0's first gate (the entry's head, before the eligibility and the sub-tests): his squared distance to the ball
// (PES reads his entry of the ball-distance table, MA+0x24710) within the ball's step over the lead time. A ball at rest
// fails it for every man who is not standing on it.
bool BallDodgeEntryGate(const DodgeConstants& k, float manDistSq_m2, float ballSpeed_mps);

// FUN_140a07570 / FUN_140a07880: the two sub-tests the ball dodge's own
// dispatcher (FUN_140a072d0) runs between the eligibility and the selector.
// The caller supplies the angles and times the exe reads itself, so the port
// stays a pure function.
struct BallSubTestInput {
  // FUN_140a07570.
  float ballStep_m = 0.0f;      // |+0x2948..+0x2950| : the ball's per-frame step
  float frameY_m = 0.0f;        // the rolled frame record's +4
  float reach_m = 0.0f;         // the player's reach byte at +0x12a7
  int recordHalf = -1;          // record0->+4 : which half the record's own team is
  bool keeperSaveFlag = false;  // record0->+0x2c
  int liveAction = -1;          // record0->+0 : the player's live anime action
  int lastTouchSlot = -1;       // the slot of the player who touched the ball last
  int selfSlot = -1;            // his own slot
  float selfHeading_deg = 0.0f;         // teamList[selfSlot + 0x247d4]
  float defendGoalAngle_deg = 0.0f;    // record->0x554 : the goal he defends
  float otherHeading_deg = 0.0f;       // teamList[lastTouchSlot + 0x247d4]
  float bearingToBall_deg = 0.0f;      // the bearing from him to the ball
  float selfTimeToBall_s = 0.0f;       // FUN_14044e0d0(teamList)
  float otherTimeToBall_s = 0.0f;      // FUN_14044e0d0(teamList, lastTouchSlot)
  bool hasAnimeSlot = false;     // FUN_1406b4580(ap, 0x15, ...): the anime layer's slot
  // FUN_140a07880's goal mouth.
  int targetAction = -1;         // the class of the player the dispatcher picked
  float ballBearing_deg = 0.0f;  // the bearing from him to the ball's look-ahead
  float mouthMidAngle_deg = 0.0f;   // the bearing to the mouth's mid sample
  float mouthFarAngle_deg = 0.0f;   // the bearing to the mouth's far sample
};

// FUN_140a07570: the ball's step and the look-ahead frame pass, and the
// heading/window geometry on the ball's last toucher admits him.
bool BallDodgeSubTestFast(const DodgeConstants& k, const BallSubTestInput& in);

// FUN_140a07880: the goal-mouth window, only for the dodge kinds whose class
// is 8, 9 or 10 (the kicks).
bool BallDodgeSubTestMouth(const DodgeConstants& k, const BallSubTestInput& in);

// PES's bearing (FUN_140439e50 over FUN_140a92d40): atan2(dx, dz) in degrees, wrapped to [0, 360), with PES z =
// -GF y: 0 deg = -GF y (PES's +z) and 90 deg = +GF x. The net gate compares bearings numerically, so this
// convention is load-bearing.
float PesBearingDeg(float fromX, float fromY, float toX, float toY);

// The goal marks both goal gates read (FieldInfo, pes-re-fieldinfo.md §1); the
// caller names them from gametypes.hpp. F[0] at 0x140a07ec5 is the goal line
// (both gates' edge), F[2] at 0x140a08013 the posts' z, and F[14] at
// 0x140a08019 the net's floor-back x (the net gate only).
struct GoalMarks {
  float goalLineX_m = 0.0f;      // F[0]: the goal line's outer edge
  float postHalfWidth_m = 0.0f;  // F[2]: the goal's inner half-width, the posts' z
  float netBackX_m = 0.0f;       // F[14]: the net's floor-back x
};

// FUN_140a07e40: the net dodge's own gate. It reads the dodger's own record
// only: his position and his movement, against the goal marks. The ball does
// not enter: the dodge is a man running into the net.
struct NetGateInput {
  bool animeSlot = false;  // FUN_1406b4c10(ap, 0x15)
  float x_m = 0.0f;        // his x (PES +0x55c: the pitch's length)
  float y_m = 0.0f;        // his z (PES +0x564: GF's y is PES's z)
  float moveX_mps = 0.0f;  // his movement (PES +0x574, per frame in the exe)
  float moveY_mps = 0.0f;  // lateral (PES +0x57c)
  float moveZ_mps = 0.0f;  // height (PES +0x578)
};

bool NetDodgeGate(const DodgeConstants& k, const GoalMarks& marks, const NetGateInput& in);

// FUN_140a084d0: the post dodge's gate. Every term reads the dodger's own
// record; the ball enters only the clause PES reads it in.
struct PostGateInput {
  bool animeSlot = false;                    // FUN_1406b4c10(ap, 0x15)
  bool keeper = false;                       // FUN_140a62690: he is his team's keeper
  bool newestTouch = false;                  // FUN_140a60300: the newest event is his
  bool designatedMan = false;                // T+0x78 == him (MA+0x2470c is not carried)
  float x_m = 0.0f;                          // his x (PES +0x55c)
  float y_m = 0.0f;                          // his z (PES +0x564)
  float moveX_mps = 0.0f;                    // his movement (PES +0x574, m/tick)
  float moveY_mps = 0.0f;                    // lateral (PES +0x57c)
  float moveZ_mps = 0.0f;                    // height (PES +0x578)
  float ballX_m = 0.0f;                      // the ball now (FUN_140a57800, ring 0)
  float ballY_m = 0.0f;
};

bool PostDodgeGate(const DodgeConstants& k, const GoalMarks& marks, const PostGateInput& in);

// FUN_140a0b350: the human dodge's own feature gate, which runs before the
// opponent scan (FUN_140a08ea0) and decides whether the man may dodge a
// challenger at all.
struct HumanGateInput {
  // The window (FUN_140a0b350, decompile lines 64-73): whether the match's
  // newest ball event is his team's, and how many logic frames ago it was.
  bool newestEventIsOwnTeam = false;
  int newestEventAgeFrames = 0;  // MI.frame - ev.frame: logic frames since his team's touch
  // The three gates ahead of it, all of which the exe reads from state GF
  // carries directly or approximates:
  bool featureOn = false;      // the behaviour's own on byte
  bool matchStateOk = true;    // the state that switches the behaviour off
  bool onRoster = false;       // his slot is on the team's own roster list
  bool teamLatchSet = false;   // teamRecord->+0x255 != 0: his team's latch, which refuses him
  bool stateRecordHalfMatches = false;  // the record's own half is his
  int rosterSlotValue = 0;     // teamRecord[+0x6a4 + (id % 11) * 4]
  bool ballOwnerIsSelf = false;  // FUN_140a6ce10's ball-owner test
};

// The window's frame count: gs * DAT_142598578 (0.2), gs = 54 from the install.
float HumanLastEventWindowFrames(const DodgeConstants& k);

bool HumanDodgeGate(const DodgeConstants& k, const HumanGateInput& in);

}  // namespace pes
