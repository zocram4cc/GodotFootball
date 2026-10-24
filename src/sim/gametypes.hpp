// SDL-free game types and pitch geometry.
//
// gamedefines.hpp pulls in SDL for key ids, which makes it unusable from the
// pure-logic simulation modules that the headless test job builds without SDL
// installed. Everything here is plain data: the roles, the match phases and the
// dimensions of the pitch. gamedefines.hpp includes this header, so game code
// sees the same definitions it always did.

#ifndef _HPP_GAMETYPES
#define _HPP_GAMETYPES

// PES's thirteen positions: the original ten plus the wingers and the second
// striker. Appended, not interleaved, because the index is used as an array
// offset in a few switches; nothing persists a role as a number (career and
// match state both round-trip through GetRoleName/GetRoleFromString text).
enum e_PlayerRole {
  e_PlayerRole_GK,
  e_PlayerRole_CB,
  e_PlayerRole_LB,
  e_PlayerRole_RB,
  e_PlayerRole_DM,
  e_PlayerRole_CM,
  e_PlayerRole_LM,
  e_PlayerRole_RM,
  e_PlayerRole_AM,
  e_PlayerRole_CF,
  e_PlayerRole_LW,
  e_PlayerRole_RW,
  e_PlayerRole_SS,
};

enum e_SetPiece {
  e_SetPiece_None,
  e_SetPiece_KickOff,
  e_SetPiece_GoalKick,
  e_SetPiece_FreeKick,
  e_SetPiece_Corner,
  e_SetPiece_ThrowIn,
  e_SetPiece_Penalty,
  // The period-end restart (plan-selector §4): PES raises restart kind 7 when the period ends
  // and holds it until the next award overwrites it with its own kind. It schedules exactly like
  // a kick-off (the break's own restart is the next period's kick-off); only the JR kind differs.
  e_SetPiece_PeriodEnd,
};

enum e_TouchType {
  e_TouchType_Intentional_Kicked,     // goalies can't touch this
  e_TouchType_Intentional_Nonkicked,  // headers and such
  e_TouchType_Accidental,             // collisions
  e_TouchType_None,
  e_TouchType_SIZE
};

enum e_MatchPhase {
  e_MatchPhase_PreMatch,
  e_MatchPhase_1stHalf,
  e_MatchPhase_2ndHalf,
  e_MatchPhase_1stExtraTime,
  e_MatchPhase_2ndExtraTime,
  e_MatchPhase_Penalties,
};

// PES's pitch, not GF's. Every PES-derived thing on this pitch - the goal
// director's choreography marks, the camera tracks that film them, the
// entrance staging - was authored for a 105 x 68 m field, which is also the
// real-world standard. GF shipped 110 x 72, so an imported shot aimed at a
// mark near a line missed it by two to three metres, and a 2-degree PES
// telephoto lens covers 1.4 m at 40 m: the shot filmed grass (owner, 14-09).
// The markings, the goals and the AI zones are all expressed in these two
// numbers, so they move together.
const float pitchHalfW = 52.5;  // only inside side- and backlines
const float pitchHalfH = 34;
const float pitchFullHalfW = 60;  // including 'rim'
const float pitchFullHalfH = 40;
const float lineHalfW = 0.06f;

// PES 2021's FieldInfo: the 18 floats F0..F17 the Denuvo VM builds at match
// start. This is the one table: every PES-derived pitch, goal and net mark reads
// these names, and no copy is kept elsewhere. The record and its live proof are in
// docs/re/pes-re-fieldinfo.md section 1. Metres; x runs along the pitch from the
// centre, z across it. Each comment gives the index PES's code uses (F[k] sits at
// block + 4 + 4k).
constexpr float pesFieldHalfLength = 52.5599976f;  // F0 (0x42523D70): centre to the outer edge of the goal line
constexpr float pesFieldHalfWidth = 34.0599976f;   // F1 (0x42083D70): centre to the outer edge of the touchline
constexpr float pesGoalInnerHalfWidth = 3.66f;    // F2: centre to the inside of a post
constexpr float pesGoalInnerHeight = 2.44f;       // F3: ground to the underside of the bar
constexpr float pesGoalAreaLineX = 47.0599976f;    // F4 (0x423C3D70): goal-area line, F0 - 5.5
constexpr float pesGoalAreaHalfWidth = 9.16f;     // F5: goal-area half-width, F2 + 5.5
constexpr float pesPenaltyAreaLineX = 36.14f;     // F6: penalty-area line, 16.42 m from the goal line
constexpr float pesPenaltyAreaHalfWidth = 20.1199989f;  // F7 (0x41A0F5C2): penalty-area half-width
constexpr float pesPenaltySpotX = 41.5599976f;      // F8 (0x42263D70): penalty mark, F0 - 11
constexpr float pesNetHeight = 2.5f;              // F9: net height, F3 + F15 / 2
// F10..F12: open. No reader in the image yet; see docs/re/pes-re-fieldinfo.md FI-01.
constexpr float pesFieldF10 = 1.109375f;
constexpr float pesFieldF11 = 2.33984375f;
constexpr float pesFieldF12 = 51.171875f;
constexpr float pesNetRoofBackX = 54.3203125f;    // F13: the net's roof-back x
constexpr float pesNetFloorBackX = 54.43359375f;    // F14 (0x4259BC00): the net's floor-back x, the net gate's behind-the-post point
constexpr float pesCrossbarThickness = 0.12f;     // F15
constexpr float pesPostThickness = 0.12f;         // F16
constexpr float pesCircleRadius = 9.15f;          // F17: centre circle and the penalty arc

// The goal's post centreline, PES's F2 + F16/2 (3.66 + 0.06 = 3.72): the ball, keeper and net gates use the posts' centres.
constexpr float goalHalfWidth = pesGoalInnerHalfWidth + pesPostThickness * 0.5f;
// ponytail: GF's net depth. PES's net backs are F13 and F14, 1.76 m and 1.87 m behind
// the goal line; replaced by the table in the net-model commit.
const float goalDepth = 2.55f;
// The crossbar top: F3 + F15 / 2, which equals F9.
constexpr float goalHeight = pesGoalInnerHeight + pesCrossbarThickness * 0.5f;

#endif
