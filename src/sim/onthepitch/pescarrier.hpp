#pragma once

// PES 2021's ball-carrier decision layer: match::ai::bp::BallPlayer's think
// units, docs/re/pes-re-carrier-ai.md and docs/re/pes-re-attack-decisions.md 2.
//
// This is the decision core only - the parts the reports mark "implementable
// now": the shoot zone and its "just shoot" rule, the receiver openness
// formula with its class ladder, PassForward's three tiers, and the plan
// selection order with its rolled start probabilities. Every function is pure
// (arguments plus the constants), so the whole layer is testable against
// synthetic constants without an install and without a Match; the caller
// (ElizaController) builds the inputs from its own state.
//
// Angles are degrees throughout, positions are metres. The pitch frame is
// PES's (x = length axis, z = width axis, y up) and the caller maps this
// engine's (x, y, z) onto it: this engine's y is PES's z.
//
// Every number comes from the user's own PES2021.exe through
// pes::PeImage::ReadVa: floats from .rdata, integer bases from the immediates
// of the instructions that use them (each instruction's opcode bytes are
// checked before its immediate is trusted). The few values whose instruction
// the port did not re-read carry `ponytail:` at their field with the report
// section that names them.

#include <optional>
#include <string>
#include <vector>

#include "gametypes.hpp"
#include "onthepitch/peslane.hpp"
#include "pes/exe.hpp"
#include "pesrandom.hpp"
#include "gamedefines.hpp"

namespace pes {

// Two metres on the pitch, PES frame (x along it, z across it).
struct CarrierPoint {
  float x = 0.0f;
  float z = 0.0f;
};

struct CarrierConstants {
  // --- the shoot zone (report 4.1, 0x140699610's init + 0x140698d10) ---
  float shootR0_m = 0.0f;             // 11.0, the box half-disc radius
  float shootR1_m = 0.0f;             // 12.0, the header range
  float nearCentral_m = 0.0f;         // 18.0
  float farCentral_m = 0.0f;          // 24.0
  float nearSide_m = 0.0f;            // 16.0
  float farSide_m = 0.0f;             // 22.0
  float centralHalfAngle_deg = 0.0f;  // 15.0
  float sideAngle_deg = 0.0f;         // 40.0
  float finishingBoost_m = 0.0f;      // 3.0, near range gain per clamp01 ramp
  float longShotBoost_m = 0.0f;       // 3.0, far range gain with the skill bits
  float finishingFloor = 0.0f;        // 70.0, the ramp's low stat
  float finishingTop = 0.0f;          // 99.0, the ramp's high stat
  float finishingSpan = 0.0f;         // 29.0, the ramp's width

  // --- the shoot decision (report 5.1, 0x142121920) ---
  float justShootDist_m = 0.0f;      // 20.0, "just shoot" distance
  float justShootAngle_deg = 0.0f;   // 40.0, "just shoot" angle
  float firstTimeDistSq = 0.0f;      // 400.0 = 20 m^2, no first-time shot beyond
  float firstTimeNearDistSq = 0.0f;  // 225.0 = 15 m^2
  float firstTimeIncoming_deg = 0.0f;  // 120.0, incoming ball vs facing
  float firstTimeBody_deg = 0.0f;      // 70.0, body angle at 15 m
  float firstTimePathGate_m = 0.0f;    // 7.0, the path test's distance
  float aboveHeadFactor = 0.0f;        // 0.8, ball above 0.8 * height/100
  float statDivisor = 0.0f;            // 100.0, height/100

  // --- the receiver records (report 4.3, 0x1421174f0 / 0x142118700) ---
  float movingBallFloor_kmh = 0.0f;  // 10.0, difficulty's moving-ball ramp
  float movingBallSpan_kmh = 0.0f;   // 50.0
  float kmhPerMs = 0.0f;             // 3.6
  float distFloor_m = 0.0f;          // 10.0, difficulty's distance ramp
  float distSpan_m = 0.0f;           // 20.0
  float ballAngleHigh_deg = 0.0f;    // 150.0, the incoming-ball angle ramp
  float ballAngleSpan_deg = 0.0f;    // 60.0
  float oneTouchGain = 0.0f;         // 0.5, skill 12 halves the moving-ball term
  float runFloor_kmh = 0.0f;         // 16.0, the running term
  float runSpan_kmh = 0.0f;          // 12.0
  float turnFloor_deg = 0.0f;        // 60.0, the turn term
  float turnSpan_deg = 0.0f;         // 70.0
  float turnTop_deg = 0.0f;          // 130.0
  float highFloor_m = 0.0f;          // 0.10869, the ball radius
  float highSpan_m = 0.0f;           // 0.89131, to 1 m
  float diffMovingBallWeight = 0.0f;  // 2.0
  float diffRunWeight = 0.0f;         // 1.5
  float diffTurnWeight = 0.0f;        // 1.5
  float lowPassFloor = 0.0f;          // 80.0, g(LowPass)'s low stat
  float lowPassTop = 0.0f;            // 99.0
  float lowPassSpan = 0.0f;           // 19.0
  float lowPassBase = 0.0f;           // 0.7
  float lowPassGain = 0.0f;           // 0.3
  float forwardAngle_deg = 0.0f;      // 60.0, the direction class
  float squareAngle_deg = 0.0f;       // 110.0

  // --- the openness / interception formula (report 4.3) ---
  float openNear_m = 0.0f;           // 1.0, clamp01((d-1)/29)
  float openSpan_m = 0.0f;           // 29.0
  float openInFront_deg = 0.0f;      // 90.0, opponents in front only
  float openBlockMargin_m = 0.0f;    // 1.5, body-block corridor half width
  float openLaneMargin_m = 0.0f;     // 1.0, opponents inside the lane
  float openAlongMin_m = 0.0f;       // 0.2
  float openAlongGain = 0.0f;        // 0.6
  float openAlongPenalty = 0.0f;     // 0.3
  float openMarginScale = 0.0f;      // 100.0
  float openKLowAngle_deg = 0.0f;    // 45.0, the behind-the-ball k factor
  float openKHighAngle_deg = 0.0f;   // 90.0
  float openClass1 = 0.0f;           // 10.0
  float openClass2 = 0.0f;           // 40.0
  float openClass3 = 0.0f;           // 60.0
  float openClass4 = 0.0f;           // 100.0
  float openClass5 = 0.0f;           // 130.0

  // --- PassForward's short forward (report 5.2, 0x142125c80) ---
  float fwdPlanAngleGate_deg = 0.0f;  // 60.0
  float fwdTeamFlagGate = 0.0f;       // 36.0
  float fwdCloseRange_deg = 0.0f;     // 120.0
  float fwdShortPassDistSq = 0.0f;  // 900.0 = 30 m^2, a short pass needs an open lane
  float fwdGoalCentreDist_m = 0.0f;   // 30.0
  float fwdLaneWidthGain = 0.0f;      // 0.25
  float fwdLaneWidthMin_m = 0.0f;     // 1.5
  float fwdLaneWidthMax_m = 0.0f;     // 6.0
  float fwdLaneBudget = 0.0f;         // 220.0 frames
  float fwdLineValueSq = 0.0f;        // 1225.0 = 35 m^2
  float fwdLineValueFarSq = 0.0f;     // 1600.0 = 40 m^2
  float fwdNearestOppFar_m = 0.0f;    // 8.0
  float fwdNearestOppNear_m = 0.0f;   // 5.0
  float fwdNearestOppMin_m = 0.0f;    // 1.5
  float fwdTargetAdvance_m = 0.0f;    // 35.0
  float fwdOppNearDist_m = 0.0f;      // 7.0
  float fwdOppNearFreeDist_m = 0.0f;  // 3.0
  float fwdTurnFacing_deg = 0.0f;      // 80.0
  float fwdFacingBack_deg = 0.0f;      // 135.0
  float fwdMateSpeedCap_kmh = 0.0f;    // 20.0

  // --- PassForward's lofted forward (report 5.2) ---
  float loftMinDist_m = 0.0f;       // 15.0
  float loftMaxDist_m = 0.0f;       // 25.0
  float loftDistBase_m = 0.0f;      // 10.0
  float loftDistGain_m = 0.0f;      // 5.0
  float loftKeeperNearSq = 0.0f;    // 100.0 = 10 m^2
  float loftKeeperFarSq = 0.0f;     // 225.0 = 15 m^2

  // --- the through-ball chooser (attack-decisions 2.6, 0x142127ff0) ---
  float throughMinDist_m = 0.0f;     // 5.0
  float throughCap_m = 0.0f;         // 20.0
  float throughOpenCap_m = 0.0f;     // 15.0
  float throughOpennessOpen = 0.0f;  // 100.0
  float throughClassOpen = 0.0f;     // 4
  float throughMateSpeedCap_kmh = 0.0f;  // 60.0
  float throughFacing_deg = 0.0f;         // 135.0

  // --- the plan roll (report 4.4) ---
  float breakthroughSeedK = 0.0f;  // 1100.0
  float middleShotSeedK = 0.0f;    // 1200.0
  float curveShotSeedK = 0.0f;     // 1300.0
  float crossSeedK = 0.0f;         // 1500.0
  float longSeedK = 0.0f;          // 1600.0
  int planRollModulus = 100;       // `mov edx, 0x64`
  int planRollSlot = 2;            // `mov edx, 2`
  // The stat term every rolled plan adds: gain * clamp01((x - 70)/span). The
  // span is per plan: Breakthrough's ramp saturates at stat 90 (divisor 20),
  // the other plans' at 99 (29).
  float statFloor = 0.0f;     // 70.0, 0x14211a9bb
  float statSpan = 0.0f;      // 29.0, 0x14211acc4
  float breakthroughBase = 0.0f;   // 70, 0x14211a956 `mov edx, 0x46`
  float breakthroughComBase = 0.0f;  // 20, 0x14211a991 `mov ebx, 0x14`
  float breakthroughStatSpan = 0.0f;  // 20, 0x14259be1c
  float breakthroughStatGain = 0.0f;  // 15, 0x14211a9db
  float breakthroughCap = 0.0f;       // 70, `cmp ebx, 0x46`
  float middleShotBase = 0.0f;       // 70, 0x14211b628
  float middleShotStateGain = 0.0f;  // 40, 0x14211bcf9
  float middleShotStatGain = 0.0f;   // 15, 0x14259be0c
  float middleShotFloor = 0.0f;      // 5, 0x14259be7c's -5 max
  float middleShotCap = 0.0f;        // 80, 0x14211b716
  float crossBase = 0.0f;            // 50, 0x14211ac86
  float crossStatGain = 0.0f;        // 20, 0x14259be1c
  float crossFloor = 0.0f;           // 40, 0x14211ad5e
  float crossCap = 0.0f;             // 80, 0x14211ad57
  float longBasePressed = 0.0f;      // 30, 0x14211b289
  float longBaseMid = 0.0f;          // 60, 0x14211b2c5
  float longBaseFree = 0.0f;         // 80, 0x14211b2a9
  float longStatGain = 0.0f;         // 10, report 4.4
  float longComBonus = 0.0f;         // 50, `lea ecx, [rbx + 0x32]` at 0x14211b2d9 (COM 6)
  float longSkillBonus = 0.0f;       // 50, `lea eax, [rcx + 0x32]` at 0x14211b2ec (skill 13; skill 17 repeats it)
  float longLevelStep = 0.0f;        // 20, `add edi, 0x14` at 0x14211b30b (team long instruction level)
  float longPathBSkillBonus = 0.0f;  // 10, `and ecx, 0xa` at 0x14211b36b (Path B skill)
  // --- the curve shot (report 6.4, 0x14211af20) ---
  float curveShotComBase = 0.0f;    // 30, `and ecx, 0x1e` at 0x14211b052
  float curveShotSkillBase = 0.0f;  // 30, `lea ebx, [rcx + 0x1e]` at 0x14211b05e
  float curveShotStatGain = 0.0f;   // 10, 0x142588210
  float curveShotFloor = 0.0f;      // 5, `mov eax, 5` at 0x14211b106
  float curveShotCap = 0.0f;        // 70, `cmp ebx, 0x46` at 0x14211b0fa
  // --- the S1 score rows (pesscorerows.hpp) ---
  float s1RadToDeg = 0.0f;  // 180/pi, 0x14259BE40: the writer's bearing scale
  float s1TwoPi = 0.0f;     // 2 pi, 0x1425795E4: the writer's bearing fold
  // --- the Intention ratchet (report 5, 0x1421197b0) and the image manager ---
  float intentionClose_deg = 0.0f;  // 100, 0x14253ee30
  float intentionLead_m = 0.0f;     // 10, 0x142588210
  float imageHalfLine = 0.0f;       // 0.5, 0x142565600
  // --- the dribble direction search (report 1; addresses in the loader) ---
  float dribbleEdgeCost = 0.0f;       // 100, 0x14253ee30: a band cell's cost
  float dribbleEdgeMargin_m = 0.0f;   // 0.5, 0x142565600: the touchline band
  float dribblePrefGain = 0.0f;       // 0.6, 0x14257a190: preference scale
  float dribbleRange_m = 0.0f;        // 10, 0x142588210: the search radius and falloff
  float dribbleStep_m = 0.0f;         // 2/3, 0x14258d10c: radial step, and the push-out
  float dribbleDirections = 0.0f;     // 72, 0x1425cb47c: headings in the circle
  // --- the threat term (report 1.1) ---
  float threatReachFloor_m = 0.0f;    // 5, 0x14259222c: reach floor, and full-weight distance
  float threatReachScale = 0.0f;      // 2, 0x142565604: reach = max(floor, scale * R_o)
  float threatKeeperScale = 0.0f;     // 2, 0x142565604: a keeper's threat factor
  float threatFullDist_m = 0.0f;      // 5, 0x14259222c: the carrier-opponent distance at w = 0
  float threatFalloff = 0.0f;         // -0.25, 0x1425c48ec: w = (d - 5) * falloff
  float threatPushSq_m2 = 0.0f;       // 0.444444, 0x142bf0a88: squared push-out radius
  // --- the Dribble sub-kind chooser (report 2.1, 0x14212f1c0): its gates' constants ---
  float chooserGate_deg = 0.0f;           // 30, 0x142591140: the feint's facing gate
  float chooserTurn_deg = 0.0f;           // 60, 0x14259a764: the turning band
  float chooserSideAngle_deg = 0.0f;      // 40, 0x14259be2c: the side band
  float chooserPressureScale_deg = 0.0f;  // 20, 0x14259be1c: side band per unit of pressure
  float chooserSpeedLow_kmh = 0.0f;       // 10, 0x142588210: the fast-carrier gate
  float chooserSpeedMid_kmh = 0.0f;      // 15, 0x14259be0c: the free-lane speed gate
  float chooserRoom_m = 0.0f;             // 12, 0x14259be04: the room test

  // --- the lane-gated units (BackSpace 0x142129110, GoalFront 0x14212ABA0, Safety 0x14212B520):
  // the lane request each one passes to the packer 0x14063DE20, and the GoalFront picker
  // 0x1421265C0's gates ---
  float unitLaneLen_m = 0.0f;    // 20, 0x14259be1c: BackSpace's and GoalFront's lane length
  float unitLaneCone_deg = 0.0f;  // 80, 0x142596e6c: all three lanes' cone
  float unitLaneRad_m = 0.0f;    // 1, 0x14256ae1c: BackSpace's and GoalFront's radius offset
  float gfDifficultyMax = 0.0f;  // 1, 0x14256ae1c: record A +0xF0 at or below
  float gfDistMin_m = 0.0f;      // 10, 0x142588210: the angle gate's floor on record B +0xEC
  float gfDistMax_m = 0.0f;      // 30, 0x142591140: the class gate's ceiling on it
  float gfDistMinBand_m = 0.0f;  // 5, 0x14259222c: the class gate's floor on it
  float gfClass2Dist_m = 0.0f;   // 15, 0x14259be0c: openness class 2's distance cap
  float gfAngle_deg = 0.0f;      // 90, 0x14259be5c: record B +0xE8's default limit
  float gfStatAngle_deg = 0.0f;  // 117, 0x142bf07e0: the limit when the stat byte passes the floor
  float gfFlagAngle_deg = 0.0f;  // 140, 0x14259be64: the limit when imageA+0x21D8 holds
  float gfFlagDistSq_m2 = 0.0f;  // 25, 0x14259be20: imageA+0x2128's distance for that limit
  float gfStatFloor = 0.0f;      // 70, `cmp byte [rsp+0x34],0x46` at 0x1421267ff
  float gfStatBonus = 0.0f;      // 20, `add bpl,0x14` at 0x1421266ad
  float gfPredictGain = 0.0f;    // 0.5, 0x142565600: the mate's predicted point's frame count
  float gfZoneDist_m = 0.0f;     // 15, 0x14259be0c: the zone's +0x170 at or below
  // --- the BackSpace unit (kind 3; gate 0x142129110, validator 0x1421226C0, cut-back picker
  // 0x142125140) ---
  float bsOppDistSq_m2 = 0.0f;       // 36, 0x1425a9464: the lane branch's squared distance
  float bsBallHeight_m = 0.0f;       // 1, 0x14256ae1c: block 2's ball-height limit
  float bsCornerDistSq_m2 = 0.0f;    // 225, 0x14259be70: the corner test's squared distance
  float bsPredictGain = 0.0f;        // 0.3, 0x142590d98: the 16-frame prediction's gain
  float bsBoxSkipCount = 0.0f;       // 2, imageA+0xB48's mode-1 skip level
  float bsBoxSkipMargin_m = 0.0f;    // 5, 0x14259222c: the mode-1 skip's pitch margin
  float bsAngleLimitGap_m = 0.0f;    // 6, 0x14259bdec: the mode-1 margin gap
  float bsAlignDist_m = 0.0f;        // 6, 0x14259bdec: the align gate's distance floor
  float bsAlignNeed_m = 0.0f;        // 8, 0x14259bdf4: the align gate's need ceiling
  float bsAlignAngle_deg = 0.0f;     // 45, 0x14259be30: the align gate's heading agreement
  float bsNearBox_m = 0.0f;          // 20, 0x14259be1c: the near box's x half width
  float bsNearSum_m = 0.0f;          // 40, 0x14259be2c: the near box's x+z sum
  float bsFarDist_m = 0.0f;          // 20, 0x14259be1c: the facing gate's distance floor
  float bsAngleClass = 0.0f;         // 4: the facing gate's record-B class floor
  float bsFacingLimit_deg = 0.0f;    // 90, 0x14259be5c: the facing gate's default limit
  float bsHumanFacingLimit_deg = 0.0f;  // 120, 0x14256ae30: the human carrier's limit
  float bsLane1Len_m = 0.0f;    // 5, `mov [rbp-0x30],0x40a00000` at 0x142122cc1
  float bsLane1Cone_deg = 0.0f;  // 30, `mov [rbp-0x2c],0x41f00000` at 0x142122cc8
  float bsLane1Rad_m = 0.0f;    // 2, `mov [rbp-0x28],0x40000000` at 0x142122cd8
  float bsLane2Cone_deg = 0.0f;  // 140, 0x14259be64
  float bsLane2Cap_deg = 0.0f;   // 20, 0x14259be1c: the theta clamp's cap
  float bsLane2Floor_deg = 0.0f;  // -20, 0x1425a946c: the theta clamp's floor
  float bsLane2Gain = 0.0f;      // 2, 0x142565604: the theta clamp's gain
  float bsLane2XNear_m = 0.0f;   // 3, 0x14258c264: the x term's near floor
  float bsLane2XFar_m = 0.0f;    // 15, 0x14259be0c: the x term's far ceiling
  float bsLane2XSpan_m = 0.0f;   // 12, 0x14259be04: the x term's span
  float bsLane2ZGain = 0.0f;     // 4, 0x1425656c0: the length's x-term gain
  float bsLane2Base_m = 0.0f;    // 6, 0x14259bdec: the length's base
  float bsLane2Pad_m = 0.0f;     // 2, 0x142565604: the length's pad
  float bsLane2LenMin_m = 0.0f;  // 3, 0x14258c264: the length's floor
  float bsTailDistSq_m2 = 0.0f;  // 100, 0x14253ee30: exit A's squared distance
  float bsTailHalf = 0.0f;       // 0.5, 0x142565600: exit A's half factor
  float bsTailFacing_deg = 0.0f;  // 90, 0x14259be5c: exit B's facing agreement
  float bsTailHeading_deg = 0.0f;  // 30, 0x142591140: exit B's heading agreement
  float bsTailPad_m = 0.0f;      // 1, 0x14256ae1c: exit B's z pad
  float cbDist_m = 0.0f;         // 15, 0x14259be0c: the cut-back distance gate
  float cbSpeedBranch_kmh = 0.0f;  // 10, 0x142588210: the speed branch's floor
  float cbAlign_deg = 0.0f;      // 70, 0x14259be4c: the align arm's limit
  float cbFastAlign_kmh = 0.0f;  // 14, 0x1425c48b8: the fast arm's speed floor
  float cbCrowd_m = 0.0f;        // 3, 0x14258c264: the crowd radius
  float cbBoxAngle_deg = 0.0f;   // 100, 0x14253ee30: the box branch's angle limit
  float cbSpeedGate_kmh = 0.0f;  // 5, 0x14259222c: the through picker's speed floor (shared)
  float cbAdvance_m = 0.0f;      // 20, 0x14259be1c: the advance gate's need ceiling
  // --- the Safety unit (kind 6; gate 0x14212B520, short picker 0x142126B70, through picker
  // 0x142128490) ---
  float sfFacingTurn_deg = 0.0f;    // 135, 0x14259be60: the gate's facing-vs-plan increment
  float sfLaneRad_m = 0.0f;         // 3, 0x14258c264: the gate lane's radius offset
  float sfKeeperDist_m = 0.0f;      // 15, 0x14259be0c: the keeper arm's distance floor
  float sfKeeperFar_m = 0.0f;       // 40, 0x14259be2c: the keeper arm's distance ceiling
  float sfKeeperScalar = 0.0f;      // 1.5, 0x14259bddc: the keeper arm's ctx+0x44 ceiling
  float sfKeeperSpeedCap = 0.0f;    // 60, 0x14259a764: the keeper arm's ball-speed cap
  float sfOppD2Min = 0.0f;          // 225, 0x14259be70: the mate arm's opp-table floor
  float sfScoreDist_m = 0.0f;       // 10, 0x142588210: the scored gate's distance floor
  float sfTurnOver_deg = 0.0f;      // 110, 0x14259d628: the over-110 turn mark
  float sfTurnUnder_deg = 0.0f;     // 80, 0x142596e6c: the under-80 turn mark
  float sfClass4Bonus = 0.0f;       // 20, 0x14259be1c: the need's class-4 bonus
  float sfNeedDanger = 0.0f;        // 120, 0x14256ae30: the need in danger
  float sfNeedCap = 0.0f;           // 135, 0x14259be60: the need's cap (non-keeper)
  float sfNeedFloor = 0.0f;         // 45, 0x14259be30: the need's floor
  float sfSpeedFloor_kmh = 0.0f;    // 5, 0x14259222c: the speed weight's floor
  float sfSpeedCeil_kmh = 0.0f;     // 25, 0x14259be20: the speed weight's ceiling
  float sfSpeedSpan_kmh = 0.0f;     // -20, 0x1425a946c: the weights' span
  float sfDistCeil_m = 0.0f;        // 30, 0x142591140: the distance weight's ceiling
  float sfNeedBase = 0.0f;          // 60, 0x14259a764: the need's base
  float sfNeedGain = 0.0f;          // 30, 0x142591140: the need's weight gain
  float sfClass5Bonus = 0.0f;       // 30, 0x142591140: the need's class-5 bonus
  float sfTurnAttack_deg = 0.0f;    // 90, 0x14259be5c: the over-110 attack limit
  float sfTurnFace_deg = 0.0f;      // 45, 0x14259be30: the over-110 facing limit
  float sfCeilDefault_m = 0.0f;     // 40, 0x14259be2c: the ceiling's default
  float sfCeilNear_m = 0.0f;        // 15, 0x14259be0c: the ceiling's near value
  float sfCeilFar_m = 0.0f;         // 60, 0x14259a764: the ceiling's keeper-far value
  float sfCeilOpp_m = 0.0f;         // 100, 0x14253ee30: the ceiling's opp-distance mark
  float sfMateNear_m = 0.0f;        // 5, 0x14259222c: the mate-speed branch's distance floor
  float sfMateSpeed_kmh = 0.0f;     // 20, 0x14259be1c: the mate-speed branch's speed ceiling
  float sfFrameSpeed = 0.0f;        // 80, 0x142596e6c: the frame count's speed
  float sfOppGate_m = 0.0f;         // 35, 0x14259a760: the opp-near-mate gate
  float sfLimGain = 0.0f;           // 0.4, 0x14259b89c: the limit's distance gain
  float sfLimNear_m = 0.0f;         // 10, 0x142588210: the limit's near floor
  float sfLimFar_m = 0.0f;          // 20, 0x14259be1c: the limit's far ceiling
  float sfLimMin_m = 0.0f;          // 3, 0x14258c264: the mid-arm limit's floor
  float sfOppPad_m = 0.0f;          // 2, 0x142565604: the second test's over-110 pad
  float sfKeepPad_m = 0.0f;         // 5, 0x14259222c: the second test's keeper pad
  float sfOuterTurn_deg = 0.0f;     // 50, 0x14259be34: the outer gate's turn mark
  float sfPressDist_m = 0.0f;       // 15, 0x14259be0c: the pressure gate's distance mark
  float sfLaneDivisor = 0.0f;       // 5, 0x14259222c: the one-touch lane's length divisor
  float sfLaneLenMin_m = 0.0f;      // 4, 0x1425656c0: the one-touch lane's length floor
  float sfLaneLenMax_m = 0.0f;      // 12, 0x14259be04: the one-touch lane's length ceiling
  float sfLaneCone_deg = 0.0f;      // 220, 0x1425ca310: the one-touch lane's cone
  float sfTieDiff_deg = 0.0f;       // 130, 0x1425a9bf0: the tiebreak's facing mark
  float sfTiePress = 0.0f;          // 9, 0x14259d60c: the tiebreak's pressure mark
  float sfTieCornerSq = 0.0f;       // 1600, 0x14259ce40: the tiebreak's corner distance
  float sfTieGap_m = 0.0f;          // 5, 0x14259222c: the tiebreak's opp-distance gap
  float sfThruIdle = 0.0f;          // 3, 0x14258c264: the through idle gate's frame gain
  float sfThruPress = 0.0f;         // 9, 0x14259d60c: the through idle gate's pressure mark
  float sfThruCarrierSq = 0.0f;     // 1600, 0x14259ce40: the carrier's opp-distance floor
  float sfThruRowSq = 0.0f;         // 900, 0x1425c65f4: the row scan's distance ceiling
  float sfThruAlign_deg = 0.0f;     // 70, 0x14259be4c: the speed arm's align limit
  float sfThruAim_deg = 0.0f;       // 135, 0x14259be60: the aim gate's limit
  float sfThruClass = 0.0f;         // 3: the aim gate's record-B class floor
  float sfThruFacing_deg = 0.0f;    // 60, 0x14259a764: the aim gate's record-B facing ceiling
  float sfThruNearSq = 0.0f;        // 100, 0x14253ee30: the band's near floor
  float sfThruFarSq = 0.0f;         // 900, 0x1425c65f4: the band's far ceiling
  float sfThruLaneLen_m = 0.0f;     // 10, 0x142588210: the final lane's length
  float sfThruLaneCone_deg = 0.0f;  // 60, 0x14259a764: the final lane's cone
  float sfThruLaneRad_m = 0.0f;     // 5, 0x14259222c: the final lane's radius
  float sfKeeperCone_deg = 0.0f;    // 90, 0x14259be5c: the keeper lane's cone
  float sfKeeperRad_m = 0.0f;       // 1, 0x14256ae1c: the keeper lane's radius
  float sfPadBack_m = 0.0f;         // 3, 0x14258c264: the second test's pad with the mate behind
  // --- the OneTwo and DirectHighBall units (kinds 10, 11) and the one-touch and receiver predicates
  // they share (0x142128C10, 0x142124950; lane-units2 §2, §4, §8) ---
  float oneTwoFrameGain = 0.0f;        // 0.3, 0x142590d98: the closing count's frames, int(54*0.3+0.5)
  float oneTwoClosingHalf_deg = 0.0f;  // 180, 0x142598580: the closing sweep's half turn, thr = half/2
  float oneTwoOuter_m = 0.0f;          // 6, 0x14259bdec: the closing sweep's outer radius
  float oneTwoAngle_deg = 0.0f;        // 90, 0x14259be5c: the carrier-angle gate
  float touchPad_m = 0.0f;             // 2, 0x142565604: G2's pitch-bound pad
  float touchScore_m2 = 0.0f;          // 1600, 0x14259ce40: G5's score floor
  float touchSpeed_kmh = 0.0f;         // 15, 0x14259be0c: G10's speed gate
  float touchTurn_deg = 0.0f;          // 100, 0x14253ee30: G10's heading gate
  float touchBand_m = 0.0f;            // -10, 0x14259be84: G12's band floor
  float touchLaneLen_m = 0.0f;         // 15, 0x14259be0c: G11's lane length
  float touchLaneCone_deg = 0.0f;      // 60, 0x14259a764: G11's cone
  float touchLaneRad_m = 0.0f;         // 2, 0x142565604: G11's radius offset
  float rcvDifficultyNear = 0.0f;      // 2, 0x142565604: the difficulty limit for a near, straight pass
  float rcvDifficultyFar = 0.0f;       // 1, 0x14256ae1c: the limit beyond 15 m or past 110 degrees
  float rcvDistFar_m = 0.0f;           // 15, 0x14259be0c
  float rcvTurnFar_deg = 0.0f;         // 110, 0x14259d628
  float rcvSpeedGate_kmh = 0.0f;       // 15, 0x14259be0c: the carrier speed that picks the angle limit
  float rcvAngleFast_deg = 0.0f;       // 80, 0x142596e6c
  float rcvAngleSlow_deg = 0.0f;       // 120, 0x14256ae30: also the M[r][c] test's limit
  float rcvBaseSplit_deg = 0.0f;       // 120, 0x14256ae30: the window base's pass-angle split
  float rcvBaseNear_m = 0.0f;          // 10, 0x142588210
  float rcvBaseFar_m = 0.0f;           // 5, 0x14259222c
  float rcvRampHalf_deg = 0.0f;        // 60, 0x14259a764: the ramp's full-value angle
  float rcvRampTop_deg = 0.0f;         // 130, 0x1425a9bf0
  float rcvRampSpan = 0.0f;            // -70, 0x1425c4900: the ramp's divisor
  float rcvRampGain_m = 0.0f;          // 10, 0x142588210: reach = base + 10 * ramp
  float rcvWindowFloor_m = 0.0f;       // 3, 0x14258c264
  float rcvM2_deg = 0.0f;              // 20, 0x14259be1c: the M[c][r] test
  float rcvNearSq_m2 = 0.0f;           // 36, 0x1425a9464: NL(c,r) at or above which the angle block runs
  float dhbHeightPad_m = 0.0f;         // 0.1, 0x142588200: the low-ball height pad
  float dhbNearOpp_m2 = 0.0f;          // 25, 0x14259be20: the low-ball gate's opponent (5 m)
  float dhbMateNear_m2 = 0.0f;         // 100, 0x14253ee30: branch A's opponent gap (10 m)
  float dhbPredNear_m2 = 0.0f;         // 9, 0x14259d60c: the 9-frame gate
  float dhbScoreFloor = 0.0f;          // 225, 0x14259be70
  float dhbScoreCeil = 0.0f;           // 900, 0x1425c65f4: branch A below this score
  float dhbFarGain = 0.0f;             // 0.5, 0x142565600: the 27-frame prediction
  float dhbSpeedGate_kmh = 0.0f;       // 15, 0x14259be0c: the mate's speed gate on the free lane
  float dhbLaneLen_m = 0.0f;           // 15, 0x14259be0c: the teammate lane's length
  float dhbLaneCone_deg = 0.0f;        // 60, 0x14259a764
  float dhbLaneRad_m = 0.0f;           // 5, 0x14259222c
  float dhbAdvance_m = 0.0f;           // 4, 0x1425656c0: the lane origin's advance along theta
  float dhbFacing_deg = 0.0f;          // 80, 0x142596e6c: the |X| gate

  // G3's team level (`T+0xA294`, x100; reader 0x140A40BD0, comparator 0x140A60360). PES stores
  // clamp(class x 100, 0, 700) from the match-setting selector (0x1B5D8340, class `*(int*)(setup+0x18)`,
  // default class 2 when out of 0..6, through 0x140A602B0 into the setter 0x140A424E0). GF carries no
  // selector, so the loader reads the game's own stock default class and its scale and cap.
  int teamLevelRaw = 0;

  // The lane core's own table and scalars (0x14063CD90 and its packer), loaded here so the
  // carrier units that call the core get it through the one constants bundle the match carries.
  lane::LaneConstants lane;

  // --- the curve table's builder (0x142138220; image-rows §2.3) ---
  float curveHalfTurn_deg = 0.0f;   // 180, 0x142598580: the lane angle a = wrap(psi - 180)
  float curveArmAngle_deg = 0.0f;   // 50, 0x14259be34: the two arms leave a segment point at a +- 50
  float curveArm_m = 0.0f;          // 1000, 0x14256ae34: the arm length, and the dedupe radius
  float curveWide_deg = 0.0f;       // 100, 0x14253ee30: the dedupe cone is half of it
  float curveStep_m = 0.0f;         // 1, 0x14256ae1c: a segment point's step along psi
  float curveEps = 0.0f;            // 1.1920929e-07, 0x14259bdc4: the parallel test of a crossing
  bool operator==(const CarrierConstants&) const = default;
};

std::optional<CarrierConstants> LoadCarrierConstants(const PeImage& exe, std::string* error);

// --- the shoot zone (report 4.1) ---
enum ShootBand { kBandClose = 0, kBandMid = 1, kBandFar = 2, kBandOut = 3 };
enum ShootSector { kSectorCentral = 0, kSectorSideLow = 1, kSectorSideHigh = 2, kSectorWide = 3 };

struct ShootZone {
  float d_m = -1.0f;         // distance to the goal line, or to the near post
  float angle_deg = 0.0f;    // bearing to that point against the attack direction
  int band = kBandOut;       // 0 close, 1 mid, 2 far, 3 out
  int sector = kSectorWide;  // 0 central, 1/2 side, 3 wide
  bool inBox = false;        // inside the 180 degree half-disc of radius r0
};

// The zone 0x140698d10 builds. `finishing` is stat 0x1A, `longShotSkill` the
// skill 39 | 33 | 8 mask, `halfLen_m`/`goalHalfWidth_m` the pitch marks PES
// reads from its FieldInfo (this engine's own Law-1 marks).
ShootZone ComputeShootZone(float ballX, float ballZ, float attackSign, float finishing,
                           bool longShotSkill, float halfLen_m, float goalHalfWidth_m,
                           const CarrierConstants& k);

// --- the dribble direction search (report 1) ---

// One opponent as the threat sees him: his position, the point he is moving to
// (equal to pos when static), his R_o reach figure and whether he is a keeper.
struct DribbleOpponent {
  CarrierPoint pos;
  CarrierPoint pred;
  float reachRo_m = 0.0f;
  bool keeper = false;
};

struct DribbleInput {
  CarrierPoint ball;
  float desired_deg = 0.0f;   // the heading he wants, degrees, PES frame (x, z)
  float weight = 0.0f;        // the image-state preference weight W, 0..1
  float halfLen_m = 0.0f;     // pitch half-length along x (the goal axis)
  float halfWid_m = 0.0f;     // pitch half-width along z (the touchlines)
  std::vector<DribbleOpponent> opponents;
};

// The threat term at p for a carrier at `ball` (report 1.1).
float DribbleThreat(CarrierPoint ball, CarrierPoint p, const std::vector<DribbleOpponent>& opponents,
                    const CarrierConstants& k);

// The heading (degrees, [0, 360)) the cheapest 15-step ray costs, out of
// dribbleDirections equal headings (report 1).
float DribbleDirection(const DribbleInput& in, const CarrierConstants& k);

bool InShootRange(const ShootZone& z);   // 0x1406997e0
bool IsCentral(const ShootZone& z);      // 0x140699710
bool IsMid(const ShootZone& z);           // 0x1406997a0
bool IsClose(const ShootZone& z);         // 0x140699820
bool InBox(const ShootZone& z);           // 0x1406996f0
bool InHeaderRange(const ShootZone& z, const CarrierConstants& k);  // 0x140699750

// The attack heading PES stores at `T+0x250`: the bearing of the side's attack direction in PES's
// convention (0 deg = +z, 90 deg = +x). One writer, 0x1404FEBD0, stores 0x14045DF70's MA+0x228D8 for
// attack sign +1 and MA+0x228D4 for -1 through 0x140A71520, so a side attacking +x reads 90 and one
// attacking -x reads 270. Every reader (the lane theta wrap360(T+0x250), the carrier snapshot C[4],
// the 0x140A8F7C0 advance along it, the 0x14043A0A0 bearing diff) takes it as that bearing.
float AttackAngleDeg(float attackSign);

// The shoot gate's aim-vs-facing angle (report 5.1's `angle(ball->aim, facing)`, 0x142121920, and
// central_eval's term): the attack bearing against the carrier's facing bearing, both in PES's
// convention (0 = +z, 90 = +x). A carrier facing the goal he attacks reads 0.
float AimVsFacingDeg(float attackSign, float facingX, float facingZ);

// The header-range gate PES adds to it: the ball must be over the shooter's
// head, which is `ballY > 0.8 * height/100` (0x142121a5e).
bool BallAboveHead(float ballHeight_m, float height_m, const CarrierConstants& k);

// --- the shoot decision (report 5.1) ---
struct ShootInput {
  ShootZone zone;
  float ballHeight_m = 0.0f;
  float height_m = 0.0f;  // stat 0x31
  bool owned = false;     // ball at the carrier's feet
  bool volatileShot = false;  // ctx4+0x6E, a volley/header situation
  bool receiving = false;     // ctx4+0x6F
  int planId = 0;
  float incomingVsFacing_deg = 0.0f;  // incoming ball vs the carrier's facing
  float ballToGoalDistSq = 0.0f;      // ball -> the shot's aim point
  float aimVsFacing_deg = 0.0f;       // the aim point vs the facing
  float bodyPathGate_m = 0.0f;        // 0x140a4dcc0's output
  bool keeperPresent = false;
  bool keeperSettled = false;
};

// The shot sub-kind `a` of the answer record: 0 = no shot, 1 = shoot
// (the "just shoot" rule and the box/header rule, report 5.1). The remaining
// sub-kinds (chip 2, finesse 3, knuckle 4, volley 5) are gated on capability
// rows and skill bits this engine does not carry - ponytail: they stay on the
// plain shot, the action layer picks the animation from the ball's height.
int ShootSubKind(const ShootInput& in, const CarrierConstants& k);

// --- the receiver records (report 4.3) ---
struct ReceiverInput {
  CarrierPoint target;    // the pass target (to feet, or into space)
  float openness = 150.0f;  // the record's openness (150 = the start value)
  float difficulty = 0.0f;
  int opennessClass = 0;
  int dirClass = 0;
  int turnClass = 0;
  float ballToTarget_m = 0.0f;
  float passAngle_deg = 0.0f;  // ball -> target
  float fwdAngle_deg = 0.0f;   // the pass against the attack direction
  float turnAngle_deg = 0.0f;  // the pass against the carrier's facing
};

// The direction/turn classes of report 4.3.
int DirectionClass(float fwdAngle_deg, const CarrierConstants& k);
int TurnClass(float turnAngle_deg, const CarrierConstants& k);

// The difficulty term (+0xF0), report 4.3.
float ReceiverDifficulty(float ballSpeed_mps, float dist_m, float incomingAngle_deg,
                         bool oneTouchPass, float carrierSpeed_kmh, bool running,
                         float turnAngle_deg, float ballHeight_m, float lowPass,
                         const CarrierConstants& k);

// The openness ladder: margin -> class, 0x142117b81.
int OpennessClassFromMargin(float margin, const CarrierConstants& k);

// One opponent's contribution to a lane's openness (0x142118700's scored
// tail): the perpendicular distance to the closest point of the lane against
// the time the passing opponent has to reach it.
float LaneOpennessMargin(float laneX, float laneZ, float along, float perp, float kFactor,
                         float difficulty, const CarrierConstants& k);

// --- PassForward (report 5.2) ---
enum PassKind { kPassShort = 0, kPassLong = 1, kPassCentering = 2, kPassThrough = 3 };

// One opponent the openness formula prices a lane with: his current point,
// his predicted point (position + velocity * the frames PES looks ahead) and
// the bearing of his predicted point against the pass direction.
struct LaneOpponent {
  CarrierPoint current;
  CarrierPoint predicted;
  bool sliding = false;  // action 0x33/0x34, the wider margin
};

struct PassForwardInput {
  bool owned = false;
  int carrierPressureClass = 3;  // 0 pressed .. 3 free
  bool close = false;            // the carrier is in the close shooting band
  bool danger = false;           // imageA+0x31C1
  float freeSpace = 0.0f;        // the carrier's team-table "free space" value
  int imageState = 0;            // imageA+0x32B0+8
  float planAngle_deg = 0.0f;    // plan.+0xC0
  float attackAngle_deg = 0.0f;  // team+0x250
  float attackSign = 1.0f;
  float halfLen_m = pesFieldHalfLength;
  float goalHalfWidth_m = pesGoalInnerHalfWidth;
  float goalAreaHalfWidth_m = pesGoalAreaHalfWidth;
  float planWideSide = 0;  // plan+0xC4 == 3: the same-touchline-side condition
  // 0x140a4c8e0's answer for the target mate: whether he can be reached on the
  // ground at all. ponytail: the helper itself is not decoded; the caller sets
  // it from its own lane test.
  bool mateLaneOk = false;
};

// A candidate receiver with the lanes PES scores it on.
struct PassCandidate {
  int index = -1;
  ReceiverInput record;
  float pressureDistOpp0_m = 0.0f;  // the pressure entry's distance to opponent 0
  int pressureClass = 3;
  float nearestOppToMate_m = 100.0f;  // the mate's nearest opponent
  bool mateFree = false;              // 0x14063e010 && 0x14063e270
  float mateSpeed_kmh = 0.0f;
};

struct PassAnswer {
  int index = -1;   // the mate, -1 = no answer
  int kind = kPassShort;
  bool oneTouch = false;
};

// PassForward, kind 4 (0x14212a700): its three tiers in order - short forward,
// then through when the carrier has free space, then the lofted forward.
PassAnswer PassForward(const std::vector<PassCandidate>& mates,
                       const std::vector<LaneOpponent>& opponents, const PassForwardInput& in,
                       const CarrierConstants& k);

// --- the plan (report 4.4) ---
enum CarrierPlan {
  kPlanBasic = 0,
  kPlanBreakthrough = 1,
  kPlanMiddleShot = 2,
  kPlanCurveShot = 3,
  kPlanPassThrough = 4,
  kPlanCross = 5,
  kPlanLong = 6,
  kPlanSideChange = 7,
  kPlanShotPassFromGoal = 8,
  kPlanBackDFLine = 9
};

// The priority order the manager's admission walk uses: the eight dwords at
// 0x142bf02c0 (9, 8, 2, 3, 1, 4, 5, 6) plus the 7 the dispatcher
// (0x14211e2d0) writes inline at 0x14211e35d, else Basic.
constexpr int kPlanPriorityOrder[] = {9, 8, 2, 3, 1, 4, 5, 6, 7};

// One rolled plan's start probability, the P table of report 4.4, for
// Breakthrough, MiddleShot and Cross; Long and the curve shot are scored by
// LongScore and CurveShotScore, and any other plan returns a negative value.
// `stat` is the one stat the caller passes (GF: technical_dribble for
// Breakthrough, technical_shot for MiddleShot and Cross); PES reads more, see
// docs/re/pes-re-com-styles.md section 5, items 10-13. `skill`/`com` are the
// plan's skill and COM triggers and `state2` MiddleShot's image-state-2 bonus.
float PlanStartProbability(int planId, const CarrierConstants& k, float stat, bool skill, bool com,
                           bool state2);

// The carrier's seed member, BallPlayer+0x78 (report 3.8-3.13). The driver 0x142110c90 ticks it
// on every logic frame: the update (0x14063a490) while the carrier predicate 0x14210e200 holds,
// and the reset (0x14210e5f0 -> 0x14063a3b0) on each frame it fails. An update counts itself in
// `updates` (+0x8) and copies RandomInfo+8 into `snapshot` (+0x18) when the stored value is
// negative or on the first update. So each run of predicate frames keeps one copy, and every
// roll in that run repeats.
struct CarrierSeed {
  int updates = 0;
  float snapshot = -1.0f;
};
// One frame of the member: `predicate` is 0x14210e200 for this player on this frame.
void CarrierSeedTick(CarrierSeed& seed, bool predicate, float randomInfoUnit);

// The roll itself: PES builds a throw-away generator, seeds slot 2 with
// int(snapshot * K) and draws one RangeRaw(slot 2, 100) (0x14211a89..0x14211aac2).
// The snapshot comes from the carrier's seed member (report 3.7-3.9), not from a
// possession clock, so a roll repeats while the snapshot is unchanged. Returns the
// drawn 0..99.
int PlanRoll(const CarrierSeed& seed, float seedK, const RngConstants& rng,
             const CarrierConstants& k);

// --- the curve shot (report 6.4, 0x14211af20) ---
//
// The Incisive Run (COM 4) ImageUnit: past its head gates its score is a
// percent, 0/30/30/60 from the COM card and skill 7, plus 10 per stat over the
// ramp, floored at 5 and capped at 70. The caller owns the draw, so the
// evaluator stays pure.
struct CurveShotInput {
  int intention = 1;        // the BallPlayer+0x32B0+8 ratchet
  float trajectory_m = 0.0f;  // a2[3]+0xB40: the gate wants it non-positive
  bool fullAttack = false;  // team+0xB8A2 (PES's 47266)
  float sideSelf = 0.0f;    // the scalar the two thresholds are compared against
  bool sideFlag = false;    // eval+0x48
  float sideA = 0.0f;       // eval+0x14, the two fields the side check reads
  float sideB = 0.0f;       // eval+0x20
  bool incisiveRun = false;  // +0x4C bit 4, the COM card
  bool skill7 = false;       // +0x40 bit 7
  float kickingPower = 0.0f;  // +0x2A
  float finishing = 0.0f;     // +0x1A
};

// The score, or a negative value when a head gate rejects: the Intention below
// 3 (0x14211afa1), a positive trajectory (0x14211afb9), the full-attack flag
// (0x14211afc6) or the side check (0x14211afd8).
float CurveShotScore(const CurveShotInput& in, const CarrierConstants& k);

// The admission: the draw against the score (`cmp ebx, eax; jae` at
// 0x14211b159) and the final gate, the skill 7 or the COM 4 card
// (0x14211b176..0x14211b19d).
bool CurveShotAdmitted(const CurveShotInput& in, int draw, const CarrierConstants& k);

// --- the Long plan (report 6.6: veto 0x14211a570, body 0x14211b1d0) ---
struct LongInput {
  int intention = 1;                // the BallPlayer+0x32B0+8 ratchet
  int engagement = 3;               // the carrier's pressure class, 0 pressed .. 3 free (0x14069a3e0+16)
  bool teamModeThree = false;       // team+4 == 3 (0x14211b259): an accept
  bool designatedSelf = false;      // the carrier is the team's designated player (0x14211b269): an accept
  bool fullAttack = false;          // TeamData+47266 (0x14211b279): an accept
  bool teamSetting = false;         // eval+0x3C != 0 (0x14211b27f): Path B
  int level = 0;                    // eval+0x54, the team long instruction 0..2 (0x14211b303)
  bool comLongBallExpert = false;   // COM 6 (bit 6 of +0x4C, 0x14211b2d1)
  bool skillWeightedPass = false;   // skill 13 (bit 13 of the skills QWORD, 0x14211b2e6)
  bool skillLowLoftedPass = false;  // skill 17 (bit 17, 0x14211b2f2)
  float lofted = 0.0f;              // the Lofted Pass stat (record +0x1C)
};

// The veto (0x14211a570): Intention 3 or 4, or an engagement below 1.
// ponytail: its signed-X test (0x14211a5bb..a5e4, against a PES pitch row and
// the unit's stored slot at 0x14211a601) reads PES data GF does not carry, so
// it never vetoes.
bool LongVetoed(const LongInput& in);

// The score (0x14211b1d0): Path A is the engagement's base, 50 for COM 6 and for
// each of the two skills, 20 per team level (two steps at most), and the lofted
// ramp; Path B (teamSetting) is 10 for each skill and the ramp. Truncated once.
float LongScore(const LongInput& in, const CarrierConstants& k);

// The admission: the veto, then the three accepts that skip the roll, then the
// roll itself: the draw at or below the score.
bool LongAdmitted(const LongInput& in, int draw, const CarrierConstants& k);

// --- the Intention ratchet (report 5, 0x1421197b0) ---
//
// BallPlayer+0x32B0+8, one per player, 1 on a fresh one (the reset
// 0x14210e5f0 -> 0x1415595d0 writes 1) and only ever rising within a
// possession. The gates that read it: Breakthrough and the curve shot want 3
// or more, MiddleShot wants less than 3, the feint wants exactly 3.
struct IntentionInput {
  bool closeBand = false;      // IsClose(pred+0x9cc), 0x140699820
  float headingDiff_deg = 0.0f;  // a3+0x10 minus a3+0x30, wrapped
  float attackDot = 0.0f;      // (a4+0x14) * (a5+0x40)
  float penaltyLine_m = 0.0f;   // pitch[6], the mark the caller reads
  float ballLead_m = 0.0f;     // a5+0xB58
  float ballLimit_m = 0.0f;    // a5+0xB3C
  // The corridor block (0x142119960). Its guard is `a6+4 < 2` with the state
  // not already 2 (0x142119944); inside, the first query 0x1421b7d40 must
  // answer and its second answer must exceed 1 (0x142119c91), otherwise the
  // state is 2; when both hold the state is 1 if the block's own gate holds and
  // 0 if it does not (0x142119cb8).
  bool corridorGuard = false;    // a6+4 < 2
  bool corridorAnswered = false;  // both corridor queries above their marks
  bool corridorState1 = false;   // 0x142119c9a: TeamData+308 >= 3,
                                 // 0x140a62180 != 1, or 2*GameSpeed <= a4+16
};

int NextIntention(int state, const IntentionInput& in, const CarrierConstants& k);

// --- the image manager (report 4, 0x14211e2d0) ---
//
// The carrier's selected plan, persisted across ticks at BallPlayer+0x32B0:
// +0xA0 the selected id (0 = Basic), +0xA4 frames since the last change, +0xC0
// the heading its producer published and +0xC4 the mode.
struct CarrierImageSelection {
  int id = kPlanBasic;
  int framesSinceChange = 0;
  int mode = 2;
  float heading_deg = 0.0f;
};

// What a plan's own producer answers (vtable[6], `unit+0xC`).
struct PlanImage {
  int mode = 2;
  float heading_deg = 0.0f;
};

// The selected plan's own cancel test (vtable[3]); `stillEligible` is its
// admission predicate now.
bool ImagePlanCancelled(int planId, int intention, bool stillEligible);

// What a selected plan needs to keep running: the stand-in for its own cancel
// test (vtable[3]) where that is not decoded. The curve shot keeps while its
// head gates pass (CurveShotScore), Long while its veto does not fire, and the
// rolled plans while their choice still holds.
bool PlanStillHolds(int planId, bool choiceEligible, const CurveShotInput& curve,
                    const LongInput& longPlan, const CarrierConstants& k);

// A plan's image mode (vtable[6]).
int PlanImageMode(int planId, int previousMode, bool receiving, float carrierX_m,
                  const CarrierConstants& k);

// The manager's tick. `cancel` is the selected unit's vtable[3] and `admit` its
// vtable[2], consulted only while nothing is selected - in
// `kPlanPriorityOrder`, so a plan rolls its dice only when its turn comes - and
// `image` fills the mode and heading of whichever plan ends up selected.
template <typename CancelFn, typename AdmitFn, typename ImageFn>
void AdvanceImageSelection(CarrierImageSelection& sel, CancelFn cancel, AdmitFn admit,
                           ImageFn image) {
  if (sel.id != 0 && cancel(sel.id)) sel.id = 0;
  if (sel.id == 0) {
    for (const int plan : kPlanPriorityOrder) {
      if (!admit(plan)) continue;
      sel.id = plan;
      break;
    }
  }
  if (sel.id == 0) return;
  const PlanImage now = image(sel.id);
  ++sel.framesSinceChange;
  if (now.mode != sel.mode) sel.framesSinceChange = 0;
  sel.mode = now.mode;
  sel.heading_deg = now.heading_deg;
}

// --- the answer record, the unit table and the arbitration walk (report 5, 6; MainEval f, h, i) ---

// The answer type a unit or the default writes (the 0x1C-byte record, report 6).
enum CarrierAnswerType { kAnswerNone = 0, kAnswerShoot = 1, kAnswerPass = 2, kAnswerDribble = 3 };

// What a unit answers: its type and sub-kind (a pass's PassKind, a shot's sub-kind a, a dribble's
// DribbleSub). PES has no reachable Keep (sub 2): see report §15, the chooser returns only 4-6.
struct CarrierAnswer {
  int type = kAnswerNone;
  int sub = 0;
  bool operator==(const CarrierAnswer&) const = default;
};

// One slot of the unit table: the kind (report 3) and the answer that unit stored this think.
struct UnitSlot {
  int kind = -1;
  CarrierAnswer answer;
};

// The Dribble sub-kinds the writers store (0x14212fe70 L488-525): 3 heading (FUN_142119eb0), 4 and
// 5 along with mode 3 and mode 4 (FUN_142119e20), 6 feint (FUN_142119e60).
enum DribbleSub { kDribbleHeading = 3, kDribbleMode3 = 4, kDribbleMode4 = 5, kDribbleFeint = 6 };

// The list builder (0x1421112c0): the kinds a plan lists, in the order the table holds them. The
// inputs are the plan's own flags and the carrier's zone and state, each named at its field.
struct UnitListInput {
  int plan = 0;
  bool central = false;     // IsCentral(zone), 0x140699710
  bool mid = false;         // IsMid(zone), 0x1406997a0
  bool close = false;       // IsClose(zone), 0x140699820
  // The plan record's own fields (0x1421112C0's reads): +0xD8 latches the
  // wide-side tail, +0xD9 stops plans 1-4 after their first kinds, +0xC4 mode
  // 3 latches it too, imageA+0xB44 == 3 lists the cross for plans 1-3, and
  // plan 5's cross-line test lists the cut-back.
  bool wideSideLatch = false;   // plan+0xD8
  bool stopAfterFirst = false;  // plan+0xD9
  bool oneTwoTarget = false;    // FUN_14211CB00(plan, 10) valid: PassThrough lists 4, 10
  int planMode = 0;             // plan+0xC4: 3 is the wide-side tail
  bool wideSector = false;      // imageA+0xB44 == 3
  bool pastCrossLine = false;   // plan 5: the cross-line test
  bool teamModeThree = false;   // team+4 == 3
  bool humanCarrier = false;    // team mode record +0x6C
  bool intentionMode4 = false;  // FUN_141052E30(BP+0x32B0) == 4: Basic omits its pass 4
  bool flagE7 = false;          // BP+0xE7: Basic lists nothing from the switch
};
std::vector<int> BuildUnitList(const UnitListInput& in);

// The table fill: one slot per listed kind, in list order. Kind 0 takes the
// shot answer, 4 the PassForward answer, 9 the Long answer, 7 the Cross
// answer, 2 the GoalFront answer, 3 the BackSpace answer, 6 the Safety answer, 10 the OneTwo answer
// and 11 the DirectHighBall answer; every other kind answers nothing here.
std::vector<UnitSlot> FillUnitTable(const std::vector<int>& kinds, bool shotAnswer,
                                    const PassAnswer& fwd, const PassAnswer& lng,
                                    const PassAnswer& cross, const PassAnswer& goalFront,
                                    const PassAnswer& backSpace,
                                    const PassAnswer& safety = PassAnswer{},
                                    const PassAnswer& oneTwo = PassAnswer{},
                                    const PassAnswer& directHigh = PassAnswer{});

// The walk's context (0x14210f110's reads): the match restart mode and the FSM state, the ball's
// owner and the carrier's own side, and the skill and zone bits the rules test.
struct WalkContext {
  int restartMode = 1;             // MI+0x116C: 1 in play, 2 stopped, 3 set piece
  int restartState = 0;            // MI+0x1775: 3, 9 and 10 veto passes and clears
  bool carrierIsKicker = false;    // the carrier is the designated kicker, MA+0x7C
  int ballOwnerTeam = -1;          // the team index of the ball's owner, -1 when nobody has it
  int ownTeam = 0;                 // BP+0x24: the carrier's team index
  bool passAllowedInSetPiece = true;  // 0x14063E3A0, not decoded: the caller supplies it
  bool firstTimeGate = false;      // rule 3: tier 0-1 and capability 0x14 off
  bool ballHeightOk = false;       // height/100 <= ball height (rule 3)
  bool skill11 = false;            // ONE_TOUCH_SHOOT
  bool skill12 = false;            // ONE_TOUCH_PASS
  bool skill14 = false;            // PINPOINT_CROSS (rule 9)
  bool closeBand = false;          // IsClose(shot zone): rules 6 and 8
  bool forcedClear = false;        // rule 7: the opponent-count test keeps the clear
  bool owned = false;              // BP+0xE4: the ball at the carrier's feet
  bool safetyToKeeper = false;     // rule 7: kind 6 answers a back-pass to the keeper
  bool keeperWithinNinety = false;  // rule 7: keeper within 90 deg of the body
  bool forwardIsInstantReturn = false;  // rule 8: kind 4 answers the last passer
};

// --- the Long unit (report 5.2, kind 9, 0x14212AD40) ---
//
// Needs the ball (or the danger flag with it below 1.5 m). The target is the
// plan target in plans 6/7 when 0x1421240B0 validates it, else the picker
// 0x142128A20: among valid long-ball mates the most advanced, a candidate
// within 10 m of the best replacing it when taller by more than 5 cm. Short
// (b=0) when the openness class reaches 4 (3 with 0x14063A3A0), else long
// (b=1) with f14.
struct LongMate {
  int index = -1;
  float advance_m = 0.0f;  // attack-signed x: the picker takes the maximum
  float height_cm = 0.0f;  // stat 0x31: the target-man rule
  float opennessClass = 0.0f;
};
struct LongUnitInput {
  bool owned = false;
  bool danger = false;
  float ballHeight_m = 0.0f;
  int plan = 0;
  int planTarget = -1;  // +0xCC, or -1 when the plan names none
  bool planTargetValid = false;  // 0x1421240B0
  bool favourShort = false;  // 0x14063A3A0: the short line drops to class 3
  std::vector<LongMate> mates;
};
PassAnswer PassLongUnit(const LongUnitInput& in, const CarrierConstants& k);

// --- the Cross unit (report 2.1, kind 7, 0x142129590) ---
//
// The carry bearing within 130 degrees of the facing, a lane from the aim
// (carrier + attack x half-width) into the box, and the picker 0x142125AB0:
// among valid mates the tallest (stat 0x31, last max wins). Answers a
// centering ball (b=2).
struct CrossInput {
  float carryVsFacing_deg = 0.0f;
  bool laneOpen = true;  // 0x14063CD90 + 0x1421B7D40
  bool owned = true;
  float receiverLead_m = 0.0f;  // unowned: the predicted receiver's lead
  std::vector<LongMate> mates;  // index/height reused; openness unused
  bool skill14 = false;  // f11/f12 need Pinpoint Crossing context; the unit does not gate on it
};
PassAnswer CrossUnit(const CrossInput& in, const CarrierConstants& k);

// --- the GoalFront unit (kind 2, gate 0x14212ABA0, picker 0x1421265C0) ---
//
// The gate: a carrier outside the central zone whose lane - 20 m at 80 degrees with a 1 m radius
// offset, from his own position, on the per-slot angle heading + 180 - is BLOCKED. The picker then
// takes the most open mate: status 0xB, record A's difficulty at or below 1, the angle limit on
// record B's turn angle (90, 117 or 140), the distance band (5, 30) with 10 as the angle gate's own
// floor, the openness class ladder, and the mate's own shoot zone central, within 15 m, with a
// non-zero pressure entry. Ties keep the earlier mate. The answer is a short pass (b = 0).
struct GoalFrontMate {
  int index = -1;                  // the caller's player id (PES's slot)
  bool statusValid = false;        // 0x142118D20(team+0xB6C, slot % 11) == 0xB
  float difficulty = 0.0f;         // record A +0xF0
  float distance_m = 0.0f;         // record B +0xEC (ball to target)
  float angleToFacing_deg = 0.0f;  // record B +0xE8 (the turn angle)
  float openness = 0.0f;           // record B +0xF4
  int opennessClass = 0;           // record B [0]
  float pressure = 0.0f;           // 0x14069A3E0(team+0x2124, slot % 11)[+0x10]; 0 skips
  ShootZone zone;                  // 0x140698D10 at his predicted point
};

struct GoalFrontInput {
  std::array<lane::LanePlayer, lane::kSlots> players{};  // the 22 players the lane core reads
  int carrierSlot = -1;                                  // the reference slot, 0x14043C410 sides
  CarrierPoint carrierPos;                               // the lane origin (record +0x55C/+0x564)
  float laneHeading_deg = 0.0f;  // wrap360(0x140A71260(T, carrier) [slot] + 180), 0x142598580
  bool central = false;          // IsCentral(team+0x9CC), 0x140699710
  bool humanCarrier = false;     // imageA+0x6C (0 = an AI carrier)
  float carrierStat = 0.0f;      // 0x142126696: the player-object byte the angle limit ramps on
  bool statBonus = false;        // 0x14063E960(team, carrierSlot, 0xC): adds 20 (0x1421266ad)
  bool flagSet = false;          // imageA+0x21D8[oppSide] != 0
  bool flagDistFar = false;      // imageA+0x2128[oppSide].+4 over 25 (0x14259be20)
  std::vector<GoalFrontMate> mates;
};
PassAnswer GoalFrontUnit(const GoalFrontInput& in, const CarrierConstants& k);

// --- the curve table: the image's R2 (writer 0x142138220, reader 0x142139650; image-rows §2.3, §4).
// Its 71 samples sit at lateral z_i = (i - 35) * scale (the builder's loop i < 0x47, centre 35); x[70] is
// the end value the reader returns at u = 70. ---
constexpr int kCurveSamples = 71;
constexpr int kCurveLast = kCurveSamples - 1;  // 70: the end sample, read at u == 70
constexpr float kCurveCentre = 35.0f;          // the sample with z = 0 (i = 35); the reader's offset
struct Curve {
  std::array<float, kCurveSamples> x{};  // x_i: the front line's x at z_i
  float scale = 1.0f;                    // s, the team's attack sign (+0x5E4)
};
// 0x142139650: u = clamp(z*scale + 35, 0, 70), i = floor(u), x[70] at u == 70, else the lerp.
float CurveEval(const Curve& c, float z_m);
// 0x142138220: the table from the opponents of `carrierSide` (in play, not their keeper), against the
// attack angle and sign and the pitch half length (F.x). Empty when nobody is listed (the builder then keeps
// the old rows; GF has none to keep, so the caller gets no table).
std::optional<Curve> BuildCurve(const std::array<lane::LanePlayer, lane::kSlots>& players, int carrierSide,
                                float attackSign, float attackAngle_deg, float halfLen_m,
                                const CarrierConstants& k);

// --- the BackSpace unit (kind 3, gate 0x142129110, validator 0x1421226C0, cut-back picker
// 0x142125140) ---
//
// The gate: a Mid-zone carrier whose lane - 20 m at 80 degrees with a 1 m radius offset, from
// his own position, on the side angle - is FREE checks the nearest opponent's 9-frame
// prediction against the ball point (no answer when past 6 m); then, owned or with the ball
// low, plan 4 answers the plan-record target through the validator, and other plans answer the
// cut-back picker's mate. Both answers are through balls (b = 3), dropped to short (b = 0) when
// the close test, the pressure class and the record class all exceed 1.
struct BackSpaceMate {
  int index = -1;                   // the caller's player id (PES's slot)
  CarrierPoint pos;                 // record +0x55C/+0x564
  CarrierPoint velPerFrame;          // record +0x574/+0x57C, m per PES frame
  float facing_deg = 0.0f;          // record +0x554
  int action = 0;                   // record +0x5D0 (0x1E/0x25 arm the cut-back flag)
  bool statusValid = false;         // 0x142118D20(team+0xB6C, slot % 11) == 0xB
  float teamNeed_m = 0.0f;          // 0x1404C8E00/0x1404C8EA0: the team table's need
  float teamAngle_deg = 0.0f;       // 0x140439F90: the team table's angle
  float lineValue = 0.0f;           // 0x14061E7C0: the team table's line value
  CarrierPoint recBTarget;          // record B +0xC/+0x14: the pass target
  float recBAngle_deg = 0.0f;       // record B +0xE0
  float recBDist_m = 0.0f;          // record B +0xEC (ball to target)
  float recBDifficulty = 0.0f;      // record B +0xF0
  int recBClass = 0;                // record B [0]
  float recAClass = 0.0f;           // record A [0]
  float pressureClass = 0.0f;       // 0x14069A3E0(team+0x2124, slot % 11)[+0x10]
  float slotStat = 0.0f;            // imageA+0x898 per-slot float
  float heading_deg = 0.0f;         // 0x140458740: the vel_eff heading
  float speed_kmh = 0.0f;           // 0x140477130
  ShootZone zoneAtPred16;            // 0x140698D10 at the 16-frame predicted point (mode-1 arm)
  bool closeTest = false;           // 0x14063E270(team, slot, 0)
  bool zoneBuilt = false;           // 0x14063E010(team, slot, 18): the built zone's central test
  int roleClass = -1;               // the side's +0x4C00[j] (B array, 0 = keeper)
};

struct BackSpaceInput {
  std::array<lane::LanePlayer, lane::kSlots> players{};  // the 22 players the lane core reads
  std::array<int, lane::kSlots> nearestOpponent{};  // NearestList rank 0 of each slot's opposite side
  int carrierSlot = -1;                                  // the reference slot
  bool humanCarrier = false;                             // ctx4+0x6C: the human arms (gate 10's limit)
  CarrierPoint carrierPos;                               // the lane origin (record +0x55C/+0x564)
  CarrierPoint carrierVelPerFrame;                       // record +0x574/+0x57C
  float carrierFacing_deg = 0.0f;                        // record +0x554
  float carrierHeading_deg = 0.0f;                       // 0x140458740
  float carrierSlotStat = 0.0f;                          // imageA+0x898 per-slot float
  float carrierNeed_m = 0.0f;                            // 0x1404C8E00 of the carrier
  float sideAngle_deg = 0.0f;                            // team+0x250
  float attackSign = 1.0f;                               // ctx[0][5]
  float teamLineOpp = 0.0f;                              // T18+0x40+oppSide*0x14
  float pitchBound8 = 0.0f;                              // F[8]
  float pitchHalfLen_m = 0.0f;                           // F[0], the corner test's |x|
  float pitchF6 = 0.0f;                                  // F[6]
  float pitchF5 = 0.0f;                                  // F[5]
  CarrierPoint ball;                                    // ctx4+0x40/+0x48
  float ballHeight_m = 0.0f;                            // 0x140A57800's +4
  bool owned = false;                                   // ctx4+0x6C != 0
  bool receiving = false;                               // ctx4+0x6F != 0
  bool danger = false;                                  // imageA+0x31C1
  bool goalMouth = false;                               // imageA+0x31C0
  bool mid = false;                                     // IsMid(team+0x9CC), 0x1406997A0
  bool central = false;                                 // IsCentral(team+0x9CC), 0x140699710
  int plan = 0;                                         // ctx7+0xA0
  int planMode = 0;                                     // ctx7+0xC4
  int planRecordTarget = -1;                            // 0x14211CB00(plan, 10) +0x4, -1 when none
  bool planRecordBytesOk = false;                       // its two bytes: [1] == 0 and [0] != 0
  std::optional<Curve> curve;                          // the curve table; 0x1421225E0 reads it at the mate's z
  float boxSector = 0.0f;                               // imageA+0xB44
  float boxCount = 0.0f;                                // imageA+0xB48
  float boxMargin = 0.0f;                               // imageA+0xB3C
  float boxAngle = 0.0f;                                // imageA+0xB40
  std::vector<BackSpaceMate> mates;
};
PassAnswer BackSpaceUnit(const BackSpaceInput& in, const CarrierConstants& k);

// --- the Safety unit (kind 6, gate 0x14212B520, short picker 0x142126B70, through picker
// 0x142128490) ---
//
// The gate: danger answers through the pickers with k = 3; otherwise the carrier's pressure counts
// set k (2, 1 or 0), a facing-vs-plan turn may add one, and a FREE gate lane (origin the carrier,
// the lane heading already wrap360(angle table + 180), len the nearest-opponent distance, cone 80,
// rad 3) means no answer. The short picker always runs; with k != 0 a through ball to the through
// picker's mate wins when one is found, else the short picker's mate takes the short ball.
struct SafetyMate {
  int index = -1;                   // the caller's player id (PES's slot)
  CarrierPoint pos;                 // record +0x55C/+0x564
  CarrierPoint velPerFrame;         // record +0x574/+0x57C, m per PES frame
  float facing_deg = 0.0f;          // record +0x554, in the BearingDeg convention
  float heading_deg = 0.0f;         // 0x140458740: the velocity heading (the one-touch lane's angle)
  float speed_kmh = 0.0f;           // 0x140477130
  bool statusValid = false;         // 0x142118D20(team+0xB6C, slot % 11) == 0xB
  int roleClass = -1;               // the side's +0x4C00[j] (B array): 0 = keeper
  float nearestOppD2 = 0.0f;        // 0x140587E70: the team table's nearest-opponent d2
  float pressureClass = 0.0f;       // 0x14069A3E0(team+0x2124, slot % 11)[+0x10]
  bool zoneMid = false;             // 0x14063E140(team, slot, 0): the mate's mid-zone test
  ShootZone zoneAtPred16;           // 0x140698D10 at the 16-frame prediction (the validator's mode-1 arm)
  // Record A, the short picker's: +0xE4 the attack angle (the decompile's "turn"), +0xE8 the facing
  // angle, +0xEC the distance, +0xF0 the difficulty, [0] the openness class.
  float recATurnAngle_deg = 0.0f;
  float recAVsFacing_deg = 0.0f;
  float recADist_m = 0.0f;
  float recADifficulty = 0.0f;
  int recAClass = 0;
  // Record B, the through picker's and the validator's: +0xC/+0x14 the target, +0xE0 the angle,
  // +0xE8 the facing angle, +0xEC the distance, +0xF0 the difficulty, [0] the class.
  CarrierPoint recBTarget;
  float recBAngle_deg = 0.0f;
  float recBVsFacing_deg = 0.0f;
  float recBDist_m = 0.0f;
  float recBDifficulty = 0.0f;
  int recBClass = 0;
};

struct SafetyInput {
  std::array<lane::LanePlayer, lane::kSlots> players{};  // the 22 players the lane core reads
  int carrierSlot = -1;                                  // the reference slot
  CarrierPoint carrierPos;                               // the carrier's ground point (+0x55C/+0x564)
  float carrierOwnGoalD2_m2 = 0.0f;  // S1 score row: the carrier's d^2 to his own goal (gate length is its root)
  CarrierPoint carrierVelPerFrame;                       // record +0x574/+0x57C
  float carrierFacing_deg = 0.0f;                        // ctx4+0x38, in the BearingDeg convention
  float carrierHeading_deg = 0.0f;                       // 0x140458740 of the carrier
  float carrierSpeed_kmh = 0.0f;                         // 0x140477130 of the carrier
  float laneHeading_deg = 0.0f;                          // the gate lane: wrap360(angle table + 180)
  float attackSign = 1.0f;                               // ctx[0][5]
  float sideAngle_deg = 0.0f;                            // team+0x250
  float planHeading_deg = 0.0f;                          // ctx7+0xC0
  int planMode = 0;                                      // ctx7+0xC4
  int teamMode = 0;                                      // 0x141052E30(ctx[6])
  float teamLineOpp = 0.0f;                              // T18+0x40+oppSide*0x14 (validator gate 2)
  float teamLineB = 0.0f;                                // T18+0x40+(side+3)*0x14 (through gate 2)
  float teamLineOwn = 0.0f;                              // T18+0x40+side*0x14 (keeper arm's side test)
  float pitchHalfLen_m = 0.0f;                           // F[0], the own goal corner's x
  float pitchBound8 = 0.0f;                              // F[8]
  float pitchF6 = 0.0f;                                  // F[6]
  float pitchF5 = 0.0f;                                  // F[5]
  CarrierPoint ball;                                     // ctx4+0x40/+0x48
  float ballSpeed_kmh = 0.0f;                            // 0x140A57650 at idx 0, in km/h
  bool ballTrackSet = false;                             // 0x140A6D0D0 (the keeper speed arm)
  bool humanCarrier = false;                             // ctx4+0x6C (the human arms)
  bool receivingFlag = false;                            // ctx4+0x6E (the keeper side arm)
  float ctx44 = 0.0f;                                    // ctx4+0x44 (the keeper side arm)
  bool danger = false;                                   // imageA+0x31C1
  bool goalMouth = false;                                // imageA+0x31C0
  int pressCount1 = 0;                                   // the carrier's press+0x30
  int pressCount2 = 0;                                   // the carrier's press+0x34
  int pressCount3 = 0;                                   // the carrier's press+0x38
  int pressCount4 = 0;                                   // the carrier's press+0x20
  int carrierPressClass = 3;                             // the carrier's press+0x10
  float oppPressPair = 0.0f;                             // T18+0x2470C[opp]+4 (through gate 1, tiebreak)
  float boxSector = 0.0f;                                // imageA+0xB44 (the validator's mode-1 arm)
  float boxCount = 0.0f;                                 // imageA+0xB48
  float boxMargin = 0.0f;                                // imageA+0xB3C
  float boxAngle = 0.0f;                                 // imageA+0xB40
  std::vector<SafetyMate> mates;                         // the candidate receivers, in squad order
};
PassAnswer SafetyUnit(const SafetyInput& in, const CarrierConstants& k);

// --- the one-touch predicate 0x142128C10 and the receiver predicate 0x142124950, the carrier's inputs
// to them (lane-units2 §2.2, §2.6, §8). Each stand-in is named at its field. ---
struct TouchReceiver {
  int index = -1;                // r, the caller's slot
  CarrierPoint pos;              // player(r) +0x55C/+0x564
  float facing_deg = 0.0f;       // player(r) +0x554: the M[r][c] test
  bool statusValid = false;      // 0x142118D20(X5+0xB6C, r%11) == 0xB (S5 stand-in)
  int roleClass = -1;            // T(side)+0x4BD4[r%11] (0x14063C9F0 store): his role class; 0 is the keeper, psi is class != 0
  bool oneTwoRun = false;        // player(r)+0x5D0 == 0x25 (P3)
  float difficulty = 0.0f;       // record A +0xF0
  float dist_m = 0.0f;           // record A +0xEC: ball to target
  float turnFacing_deg = 0.0f;   // record A +0xE8: the pass against the carrier's facing
  float attackAngle_deg = 0.0f;  // record A +0xE4: the pass against the attack
  int opennessClass = 0;         // record A [0]
};

// The carrier's snapshot C and kinematics K as the one-touch predicate reads them.
struct TouchInput {
  std::array<lane::LanePlayer, lane::kSlots> players{};  // the 22 players the lane core reads
  int carrierSlot = -1;            // C[0] = s
  CarrierPoint carrierPos;         // P_s, the player record's ground point
  float attackSign = 1.0f;         // k = C[5]
  float attackAngle_deg = 0.0f;    // C[4] = wrap360(T(side)+0x250)
  float carrierFacing_deg = 0.0f;  // K+0x38: the receiver's A_tab gate
  float carrierSpeed_kmh = 0.0f;   // 0x140477130 of P_s
  float carrierHeading_deg = 0.0f;  // 0x140458740 of P_s
  float planHeading_deg = 0.0f;    // ctx7+0xC0: the plan heading (G10 and the G11 lane)
  int planMode = 0;                // ctx7+0xC4 (G9)
  float ownLine_m = 0.0f;          // G1: T18+(sd+3)*0x14 (S14 stand-in: the own goal line)
  float pitchBound_m = 0.0f;       // G2: F+0x18 (S8 stand-in: the pitch half length)
  int teamLevelRaw = 0;            // +0xA294, the level x 100 (0x140A40BD0; writer 0x140A424E0, value OPEN)
  bool teamLevelZero = false;      // +0xA29C (0x140A428B0): the reader returns 0 whatever +0xA294 holds
  int attackLevel = 0;             // G8: T+0xB8A4 (S4, writer OPEN)
  int teamMode = 2;                // G4, G8 and the receiver's angle block: 0x141052E30 (S4)
  int carrierCategory = -1;        // G4: T(sd)+0x4C00[s%11]; 0 is the keeper
  float score = 0.0f;              // G5: score(T18, k, s), the carrier's d^2 to the attacked goal (S1 row)
  bool closeZone = false;          // G6: IsClose(IA+0x9CC)
  bool humanCarrier = false;       // K+0x6C
  bool oneTwoRunEvent = false;     // G7: the newest event is the carrier side's one-two run (S7)
  int runCount = 0;                // G8: teammates on a one-two run (S7)
  float markCount = 0.0f;          // G10: markCount, AI rec2 +0x12A4 (S11 stand-in 0)
  std::optional<Curve> curve;      // the curve table; G12 reads it at P_s.z (0x142138220 writer)
  bool flag15 = false;             // C[0xF] (0x140A62180, S18): the receiver's angle-block arm
};

// The one-touch predicate 0x142128C10: gates G1-G12 in decoded order, all true to answer.
bool OneTouch(const TouchInput& in, const CarrierConstants& k);

// --- OneTwo (kind 10, gate 0x14212AEB0, picker 0x142126A80) ---
//
// The gate: the closing sweep (0x140A4E720) or the carrier's angle within 90 degrees of the attack,
// then the one-touch predicate, then the first listed teammate the receiver predicate takes. The
// answer is a short ball (b = 0) with the one-touch flag set.
struct OneTwoInput {
  TouchInput touch;
  float carrierAngle_deg = 0.0f;     // K+0x34 (S9: stands in with the carrier's facing)
  std::vector<TouchReceiver> mates;  // the teammates in NearestList order from the carrier, nearest first
};
PassAnswer OneTwoUnit(const OneTwoInput& in, const CarrierConstants& k);

// K+0x6E, the volatile-ball flag the DirectHighBall gate and the Safety keeper side arm read
// (FUN_14063A490, 0x14063AA5E..0x14063AA91). It is written only while K+0x6C is 0 (0x14063AA62)
// and is set when the ctx4 height K+0x44 is above the carrier's height byte P+0xA1 / 100: the byte
// is read at 0x14063AA79 and the divisor 100.0 sits at 0x14253EE30 (0x14063AA84).
struct VolatileBallInput {
  bool humanCarrier = false;     // K+0x6C (GF's carriers are AI: false)
  float ballHeight_m = 0.0f;     // K+0x44: the ctx4 point's height
  float carrierHeight_m = 0.0f;  // P+0xA1 / 100: the carrier's physical height
};
// ponytail: the ball-track arm (B+0x4C, 0x14063AB01..0x14063AB06, with B+0x80 and B+0x1C) has no GF
// state; GF's ball track is not carried, so the arm stays off. Upgrade: decode the B+0x4C writers.
inline bool VolatileBall(const VolatileBallInput& in) {
  return !in.humanCarrier && in.ballHeight_m > in.carrierHeight_m;
}

// --- DirectHighBall (kind 11, gate 0x142129BE0) ---
//
// A teammate with the inputs the loop reads: his receiver fields (also the f10 test), his velocity,
// and the stand-in for the angle table.
struct DirectHighBallMate {
  TouchReceiver receiver;
  CarrierPoint velPerFrame;     // record +0x574/+0x57C
  float heading_deg = 0.0f;     // 0x140458740
  float speed_kmh = 0.0f;       // 0x140477130
  float tableAngle_deg = 0.0f;  // 0x140A71260 (S10 stand-in: his facing)
  bool keeper = false;          // T(side)+0x4C00[m%11] == 0
  int recAClass = 0;            // record A [0]: at least 1 to be listed
  float score = 0.0f;           // score(k, m), the mate's d^2 to the attacked goal (S1 row)
  int markCount = 0;            // AI rec2 +0x12A4 (S11): counted below 5, else 0
};
struct DirectHighBallInput {
  TouchInput touch;                       // the carrier's one-touch inputs; touch.players is the lane space
  float ballHeight_m = 0.0f;              // K+0x44
  CarrierPoint ball;                      // K+0x40/+0x48
  bool volatileBall = false;              // K+0x6E: a volley or header situation (no GF state, see the controller)
  bool firstTimeWhileReceiving = false;   // AI rec+1 state bit 5 (S12 stand-in)
  float carrierHeight_m = 0.0f;           // AI rec2+0x70+0x31 / 100 (S13 stand-in: his height stat)
  bool mid = false;                       // IsMid(IA+0x9CC)
  std::vector<DirectHighBallMate> mates;  // the carrier's NearestList teammates, nearest first
};
PassAnswer DirectHighBallUnit(const DirectHighBallInput& in, const CarrierConstants& k);

// --- the arming gate (report 3.1, 0x142111610) ---
//
// Evaluated for every listed kind before its unit runs; failing disarms the
// unit. Kind 17 needs the 0x1E team cap, kind 3 the 0x1C cap, kind 6 the 0x1D
// cap; shoot/clear/pass kinds need the ball within 5 m (dist^2 <= 25).
bool UnitArmed(int kind, bool capFeint, bool capBackSpace, bool capSafety, float ballDistSq_m2,
               const CarrierConstants& k);
UnitSlot WalkCarrierUnits(const std::vector<UnitSlot>& table, const WalkContext& c);

// The Dribble sub-kind chooser (report 2.1, 0x14212f1c0): the gates in decompiled order, returning
// kDribbleMode3 (4), kDribbleMode4 (5) or kDribbleFeint (6). Its callee-derived inputs are named at
// each field.
struct DribbleChooserInput {
  int plan = 0;
  float planHeading_deg = 0.0f;  // param_3: the plan's heading, plan+0xC0
  float facing_deg = 0.0f;       // param_4: the unit's heading, +0x2A0
  float pressure = 0.0f;         // param_5 (XMM12): the caller's pressure scalar
  float teamAttack_deg = 0.0f;   // team+0x250
  bool forwardOfHalf = false;    // side * x > 0
  bool feintGate = false;        // 0x14212E970, not decoded: the caller supplies it
  bool turning = false;          // 0x140A6E300 on the anime state
  int teamMode = 2;              // 0x141052E30(team)
  float verticalVel = 0.0f;      // the carrier's vertical velocity, P_ai+0x24
  float verticalLimit = 0.0f;    // imageA+0x244 * side
  bool boxPressed = false;       // imageA+0xB54 / +0xB3C / +0xB44 test
  int pressureClass = 3;         // 0x14069A3E0(imageA+0x2124)+0x10; negative when unknown
  float speed_kmh = 0.0f;        // 0x140477130
  bool velZero = true;           // the velocity sample is zero (0x140A6C6D0)
  float velHeading_deg = 0.0f;   // the velocity sample's heading (0x140A92D40)
  float room_m = 0.0f;           // 0x140A4DDC0's room test
  bool laneFree = true;          // the free-lane loop: 0x14063CD90 and 0x140A53DF0 per opponent
};
int DribbleChooser(const DribbleChooserInput& in, const CarrierConstants& k);

// The dispatch around the chooser (0x14212fe70 L432-465): the Long image runs no chooser and keeps
// mode 3; an override, not decoded to its callees, turns mode 3 into a heading.
int DribbleSubKind(const DribbleChooserInput& in, bool headingOverride, const CarrierConstants& k);

// MiddleShot's start skill (0x14211b628's gate): the long-shot skills 39, 8 and 33, and Rising
// Shots (34), which GF's longShotSkill omitted.
bool MiddleShotSkill(bool longRangeShooting, bool knuckleShot, bool droppingShots, bool risingShots);

// The GF pass command for a PES pass kind. The through ball is GF's LongPass; the lofted tier
// is HighPass.
e_FunctionType PesPassFunctionType(int passKind);


// Whether a think may run this tick: the carrier has been free of a committed
// action for longer than the gate.
inline bool RethinkDue(int nowFrame, int busyStampFrame, int gateFrames = 5) {
  return nowFrame - busyStampFrame > gateFrames;
}

}  // namespace pes
