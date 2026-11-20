// PES 2021's MOVESTEP handler group A: INIT (FUN_1406ba900's case 0) and the idle family
// (IDLE, IDLE_TURN, IDLE_OTHER, IDLE_PK_PLAYER, IDLE_PK_GK, TIMINGSTEP, DEMO). Every handler is
// one PES body, ported in PES's order with PES's field writes. The install constants come from
// the exe by the loader in movestep_common.cpp; tests pass synthetic ones. The rare idle arms
// whose helper tables PES has not decoded yet keep PES's call shape and a `// ponytail:` note.
#include <array>
#include <cmath>
#include <cstdint>

#include "pes/movestep.hpp"
#include "pes/movestep_run.hpp"

namespace pes {
namespace movestep {
namespace {
// The request words group A reads or writes that the shared contract does not name yet.
constexpr int kWordBlend = 0x34;         // req[0x34]: the idle family's blend word (0.5)
constexpr int kWordClass = 0x1f;         // req[0x1f]: the live class / rate band
constexpr int kWordSpeed = 0x1d;         // req[0x1d]: the live speed word, km/h packed
constexpr int kWordBand = 0x22;          // req[0x22]: the band the request starts in
constexpr int kWordPrevBand = 0x24;      // req[0x24]: the band the request runs at
constexpr int kWordBucket = 0x20;        // req[0x20]: the 10-way angle bucket echo
// The turn lock's words (moveword::kTurnLockFlag .. kTurnLockDir) are IDLE_TURN's and TURN_STOP's toe-off writes.

// FUN_1406b8340's idle issue block, shared by the handlers. Each word is PES's store at the
// block's tail; only the row, the duration and the second dwell differ per handler.
void SetIdleClip(MoveHost& host, MoveReq& req, int row, float duration, float secondDwell) {
  const RunConstants& k = host.RunConstantsView();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const MoveCell cell = host.Cell(row, 9, false);
  bool mirror = false;
  if (kind == 3 || kind == 6) {
    mirror = host.Percent(0x32);
  } else {
    const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
    mirror = host.Cell(row, column, false).mirror;
  }
  req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
  req.SetByte(moveword::kMirror, mirror ? 1 : 0);
  req.SetF(moveword::kDuration, duration);
  req.SetU(moveword::kStartTime, 0);
  req.SetF(moveword::kPlayback, k.one);
}

// The dwell reset the handlers write before their idle issue.
void ResetDwell(MoveReq& req, float first, float firstLimit, float second, float secondLimit) {
  req.SetF(moveword::kDwellElapsed, first);
  req.SetF(moveword::kDwellLimit, firstLimit);
  req.SetF(moveword::kDwellElapsed2, second);
  req.SetF(moveword::kDwellLimit2, secondLimit);
}

// FUN_1406d1170: IDLE and IDLE_OTHER's fallback exit into IDLE_OTHER. Its row picker is the
// shared IdleRow; the dwell threshold is PES's clamp over the clip's fixed frames.
void EnterIdleOther(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
  SetIdleClip(host, req, row, k.blend3, 0.1667f);
  const int pick = host.PickN(5);
  const float playback = static_cast<float>(pick) * k.blend05 + host.StepConstants().rate0p9;
  req.SetF(moveword::kPlayback, playback);
  const int fixed =
      host.ClipTicks(pes::ClipReader::kBlend, static_cast<int>(req.U(moveword::kMotion)), k.one, 0);  // FUN_1406af620
  float limit = static_cast<float>(host.PickN(3)) / host.TicksPerSecond() + k.kmh5;
  const float frames = static_cast<float>(fixed) / host.TicksPerSecond();
  limit = (limit <= frames && 0.0f <= frames) ? limit : 0.0f;
  req.SetF(moveword::kDwellElapsed, 0.0f);
  req.SetF(moveword::kDwellLimit, limit);
  req.SetF(moveword::kDwellElapsed2, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.1667f);
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
  RequestState(req, MoveState::kIdleOther);
  req.SetByte(moveword::kTurnLockFlag, 0);
}

// The root-facing tail IDLE and IDLE_OTHER run for command 0x79.
void RootFacingTail(MoveHost& host, MoveReq& req) {
  if (req.U(moveword::kCommandId) != 0x79) return;
  const RunConstants& k = host.RunConstantsView();
  const MoveConstants& m = host.MoveConstantsView();
  const float error = req.F(moveword::kFacingError);
  const float absError = std::fabs(error);
  if (!(k.rate01 < absError && absError < k.deg135)) return;
  float side = m.lookHigh;
  if (error <= m.lookHigh && m.lookLow <= error) side = error;
  if (req.F(moveword::kDwellElapsed) <= req.F(moveword::kDwellLimit) &&
      req.F(moveword::kDwellLimit) != req.F(moveword::kDwellElapsed)) {
    const float scale = m.lookScale / host.TicksPerSecond();
    const float magnitude = std::fabs(scale);
    if (error <= magnitude && -magnitude <= error) side = error;
  }
  const float facing = FoldFull(side + req.F(moveword::kFacing), k);
  req.SetByte(moveword::kRootFacing, 1);
  req.SetF(moveword::kRootFacingAngle, facing);
}

bool StepCandidate(const MoveReq& req) {
  const float error = std::fabs(req.F(moveword::kFacingError));
  if (req.U(0x34) == 0 || req.U(0x34) == 5) return false;
  return 33.75f <= error && req.U(moveword::kCommandId) != 0x79;
}

// FUN_14042d470(local_160): a per-tick displacement in km/h.
float DisplacementKmh(const MoveHost& host, const RunConstants& k) {
  float d[3] = {0.0f, 0.0f, 0.0f};
  host.PositionAt(1, d);
  const float length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  return length * host.TicksPerSecond() * k.kmh60 * k.kmh60 / k.kmh1000;
}

}  // namespace

// FUN_1406d1390 (the IDLE_TURN entry, d_1406d1390.c): the idle family's turn exit. Two blocks and the issue.
//  * The look-bucket flip arm (open play, look bucket 4 or 9, a facing error past 168.75): when no man owns the ball, the look
//    heading folds into [0, 360) if it is 22.5 to 157.5 degrees off the facing; the ball's bearing 18 ticks ahead (20/60 s at the
//    tick rate) stands in when it is not. If that bearing sits 22.5 to 157.5 degrees to the side opposite the facing error, the
//    bucket flips (4 <-> 9), the error becomes -/+167.5 and the look heading is the facing +/- 167.5 (0x1406D1434 COMISS XMM0,[0x1425ddfcc]
//    = 168.75 .. 0x1406D15AB MOVSS XMM0,[0x1425ddff8] = -167.5; 0x1406D13DD MOVSS XMM10,[0x1425ddfc8] = 167.5).
//  * The look heading is stepped from the facing by at most 167.5 (FUN_140a92950).
//  * The turn row is DAT_1425dda30[kind] at the column DAT_1425dd988[look bucket] (the replay action, and kind 3 while the match
//    is stopped or MA+0x25930 is set, take row 0x54c), issued with the 1/6 s blend at playback 1; the cell is FUN_1406abb30 and its
//    mirror FUN_1406abd60 (host.Cell).
// The tail call (P+0x4d0 vt+0x08) is the AdjustAngle adjuster's apply, a no-op in this port (AdjustAngleApply, movestep_turn.cpp).
void EnterIdleTurn(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const TeamAi& team = host.Team();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const uint32_t lookBucket = req.U(moveword::kLookBucketAlt);
  const float facingError = req.F(moveword::kFacingError);
  if (host.MatchPhase() == 1 && (lookBucket == 4 || lookBucket == 9) && k.deg168p75 < std::fabs(facingError)) {
    float ball[3], own[3];
    host.BallSample(static_cast<int>(host.TicksPerSecond() * k.ballSampleTicks / k.kmh60 + k.half), ball);
    host.OwnPosition(own);
    const float dx = ball[0] - own[0], dz = ball[2] - own[2];
    const float lookHeading = req.F(moveword::kLookHeading);
    // FUN_140439e50: the heading of the offset, 0 when the two points coincide.
    float bearing = (dx == 0.0f && dz == 0.0f) ? 0.0f : FoldFull(std::atan2(dx, dz) * host.StyleConstantsView().radToDeg, k);
    const float lookOff = std::fabs(PesAngleDiff(req.F(moveword::kFacing), lookHeading, k));
    if (0x15 < static_cast<uint32_t>(team.BallOwnerSlot()) && k.deg225 < lookOff && lookOff < k.deg1575) {
      bearing = 0.0f <= lookHeading ? (k.full <= lookHeading ? lookHeading + k.wrapFull : lookHeading) : lookHeading + k.full;
    }
    bearing -= host.FacingDeg();
    if (k.half_turn < std::fabs(bearing)) bearing -= (bearing < 0.0f ? k.neg : k.one) * k.full;
    if (k.deg225 < std::fabs(bearing) && std::fabs(bearing) < k.deg1575) {
      const float errorSign = (facingError <= 0.0f && facingError != 0.0f) ? k.neg : k.one;
      const float sideSign = 0.0f <= bearing ? k.one : k.neg;
      if (errorSign != sideSign) {
        const uint32_t flipped = lookBucket == 4 ? 9 : 4;
        const float facing = req.F(moveword::kFacing);
        req.SetU(moveword::kLookBucketAlt, flipped);
        req.SetF(moveword::kFacingError, flipped == 4 ? k.negDeg167p5 : k.deg167p5);
        req.SetF(moveword::kLookHeading, flipped == 4 ? facing + k.deg167p5 : facing - k.deg167p5);
      }
    }
  }
  req.SetF(moveword::kLookHeading, StepHeading(req.F(moveword::kFacing), req.F(moveword::kLookHeading), k.deg167p5, k));
  int row = t.idleTurnRow.at(static_cast<std::size_t>(kind));
  if (host.IsReplay() || (kind == 3 && (host.MatchPhase() == 2 || team.MaFlag25930()))) row = kIdleTurnReplayRow;
  const MoveCell cell = host.Cell(row, t.bucketColumn.at(req.U(moveword::kLookBucketAlt)), false);
  host.Issue(MotionRequest{cell.motionNo, cell.mirror, 0, k.blend1667, k.one, 0.0f, -1, 0.0f, 0.0f, 0});
  req.SetU(moveword::kRecordStampB, req.U(moveword::kLookHeading));
  req.SetF(moveword::kDwellElapsed2, 0.0f);
  req.SetF(moveword::kDwellLimit2, k.blend1667);
  RequestState(req, MoveState::kIdleTurn);
  req.SetU(moveword::kLiveKind, static_cast<uint32_t>(kind));
  req.SetByte(moveword::kTurnLockFlag, 0);
}

// FUN_1406ba900's case 0: INIT. Its stay arm takes the idle path; the run/step arm is the
// request the entries then consume. The decompile's case body is ported in order.
void Init(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  req.SetF(moveword::kDwellElapsed, 0.0f);
  req.SetF(moveword::kDwellLimit, 0.05f);
  req.SetF(kWordBlend, k.half);
  req.SetF(moveword::kDwellElapsed2, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);

  const float speed = DisplacementKmh(host, k);
  float share = speed / k.kmh28;
  if (k.rate07 < share) share = k.rate07;
  if (share < k.rampSlope) share = k.rampSlope;
  req.SetF(moveword::kRate, share);

  const int commandClass = host.CommandSpeedClass();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const bool turnIsWide = k.deg90 < std::fabs(req.F(moveword::kTurn));
  if (commandClass == 0 || kind == 7 || (turnIsWide && commandClass < 2)) {
    if (StayDecision(host, req)) {
      const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
      const MoveCell cell = host.Cell(row, 9, false);
      bool mirror = false;
      if (kind == 3 || kind == 6) {
        mirror = host.Percent(0x32);
      } else {
        const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
        mirror = host.Cell(row, column, false).mirror;
      }
      req.SetByte(moveword::kMotionPending, 1);
      req.SetByte(moveword::kChangeState, 1);
      req.SetU(moveword::kNextState, static_cast<uint32_t>(MoveState::kIdle));
      req.SetF(moveword::kDwellElapsed, 0.0f);
      req.SetF(moveword::kDwellLimit, 0.0f);
      req.SetF(moveword::kDwellLimit2, 0.05f);
      req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
      req.SetByte(moveword::kMirror, mirror ? 1 : 0);
      req.SetF(moveword::kPlayback, k.one);
      req.SetU(moveword::kStartTime, 0);
      req.SetU(kWordClass, 0);
      req.SetU(kWordSpeed, 0);
      req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
      req.SetByte(moveword::kTurnLockFlag, 0);
    } else {
      req.SetU(kWordClass, 0xffffffffu);
      req.SetU(kWordSpeed, 0);
      EnterGotoRun(host, req, false);
    }
    return;
  }

  const float bodyTurn = PesAngleDiff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k);
  if (k.deg225 < bodyTurn && req.F(moveword::kPrecise) == 1.0f &&
      k.deg225 < std::fabs(req.F(moveword::kTurn)) &&
      EnterRunTurnStep(host, req, (host.RateCeiling() - k.rate06) * k.half)) {
    return;
  }

  if (k.deg675 <= bodyTurn ||
      (req.F(moveword::kPrecise) == 1.0f && speed <= k.kmh22 && host.CommandSpeedClass() != 4 &&
       (k.rate07 < req.F(moveword::kDistToLook) || k.rate07 == req.F(moveword::kDistToLook) ||
        speed <= k.kmh8 || host.CommandSpeedClass() < 3))) {
    int band = 0;
    if (k.deg675 < bodyTurn && k.deg225 < std::fabs(req.F(moveword::kTurn))) {
      const float ground = DisplacementKmh(host, k);
      if (k.kmh8 <= ground) {
        if (k.kmh135 <= ground) {
          req.SetU(kWordClass, 2);
          req.SetF(kWordSpeed, 16.0f);
        } else {
          req.SetU(kWordClass, 1);
          req.SetF(kWordSpeed, 11.0f);
        }
      } else {
        req.SetU(kWordClass, 0);
        req.SetF(kWordSpeed, 5.0f);
      }
      if (EnterStepTurnRun(host, req)) return;
    }
    int kindForRow = kind;
    if (kindForRow < 2) {
      band = 2;
    } else if (kindForRow == 4) {
      kindForRow = 5;
    }
    if (k.kmh8 <= speed || 2 < host.CommandSpeedClass()) {
      band = 2;
      if (speed < k.kmh16 && host.CommandSpeedClass() < 3) band = 1;
    }
    // PES's Init bucket angle is the same FUN_14043a0a0(facing, bodyDir) result as the turn gate (XMM14 at 0x1406bac24).
    const int bucket = AngleBucket(host.AngleBuckets(),
                                   PesAngleDiff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k),
                                   10, false);
    const int row = host.StepTablesView().kindBandRow[kindForRow + band * 8];
    const MoveCell cell = host.Cell(row, host.StepTablesView().bucketColumn[bucket], false);
    req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
    req.SetByte(moveword::kMirror, host.BucketIsLeft(bucket) ? 1 : 0);
    req.SetF(kWordSpeed, speed);
    req.SetU(kWordClass, static_cast<uint32_t>(band));
    req.SetU(kWordPrevBand, static_cast<uint32_t>(band));
    req.SetU(kWordBucket, static_cast<uint32_t>(bucket));
    req.SetF(moveword::kPlayback, StepPlayback(host.PhysiqueG(), host.PlaybackConstantsView()));
    req.SetF(moveword::kDwellElapsed, 0.0f);
    req.SetF(moveword::kDwellLimit, 0.0f);
    req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kindForRow));
    RequestState(req, MoveState::kStepLoop);
    return;
  }

  req.SetByte(moveword::kStyle, 0);
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
  req.SetF(moveword::kDwellLimit, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.0f);
  EnterRun(host, req, host.FootBand(), false);
}

// FUN_1406bf0c0: IDLE.
void Idle(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  req.SetF(kWordSpeed, 0.0f);
  req.SetU(kWordClass, 0xffffffffu);
  const int fixed = host.ClipTicks(pes::ClipReader::kBlend, host.CurrentMotion(), k.one, 0);  // FUN_1406af620
  if (req.F(moveword::kDwellElapsed) < req.F(moveword::kDwellLimit)) {
    RootFacingTail(host, req);
    return;
  }
  if (StayDecision(host, req)) {
    EnterGotoRun(host, req, false);
    return;
  }
  const float error = std::fabs(req.F(moveword::kFacingError));
  if (req.U(moveword::kLookBucketAlt) != 0 && req.U(moveword::kLookBucketAlt) != 5 &&
      k.deg3375 <= error && req.U(moveword::kCommandId) != 0x79) {
    EnterIdleTurn(host, req);
    return;
  }
  const int kind = static_cast<int>(req.U(moveword::kKind));
  int row = kIdleRowFallback;
  if (kind < 8) {
    if (req.U(moveword::kCommandId) == 5 && host.MatchPhase() != 3) row = kIdleRowBall;
    else row = IdleRow(host, req);
  }
  const MoveCell cell = host.Cell(row, 9, false);
  bool mirror = false;
  if (kind == 3 || kind == 6) {
    mirror = host.Percent(0x32);
  } else {
    const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
    mirror = host.Cell(row, column, false).mirror;
  }
  if (req.U(moveword::kLiveKind) == static_cast<uint32_t>(kind)) {
    if (fixed <= host.CurrentClipFrames()) {
      const int motion = host.CurrentMotion();
      const bool smallTurn = error < k.deg1125 && ((kind - 4u) & ~3u) == 0;
      if (smallTurn || motion == 0x8d4 || motion == 0x8d5 || motion == 0x8d6 || motion == 0x8d7) {
        EnterIdleOther(host, req);
      }
    }
    RootFacingTail(host, req);
    return;
  }
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
  ResetDwell(req, 0.0f, 0.0f, 0.0f, 0.25f);
  req.SetF(kWordBlend, k.half);
  req.SetByte(moveword::kMotionPending, 1);
  req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
  req.SetByte(moveword::kMirror, mirror ? 1 : 0);
  req.SetF(moveword::kDuration, 0.25f);
  RequestState(req, MoveState::kIdle);
  req.SetF(moveword::kPlayback, k.one);
  RootFacingTail(host, req);
}

// FUN_1406bfe70: IDLE_TURN.
void IdleTurn(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const int motion = host.CurrentMotion();
  const int start = host.ClipTicks(pes::ClipReader::kRotStart, motion, k.one, 0);  // FUN_1406b1f00
  const int total = host.ClipTicks(pes::ClipReader::kRotEnd, motion, k.one, 0);    // FUN_1406b1d00
  const int fixed = host.ClipTicks(pes::ClipReader::kBlend, motion, k.one, 0);     // FUN_1406af620
  const int current = host.CurrentClipFrames();
  if (!host.IsReplay() && start <= current) {
    const int span = start + total * 2;
    if (current < span / 3) {
      const int ticks = static_cast<int>(static_cast<float>(total - current) / host.RecordF2468() + k.half);
      // FUN_14043a0a0(req[2], req[0x18]): the facing against the stamp (0x1406BFF72), per tick.
      const float turn = std::fabs(PesAngleDiff(req.F(moveword::kFacing), req.F(moveword::kRecordStampB), k)) /
                         static_cast<float>(ticks);
      if (turn < k.deg45) {
        req.SetU(moveword::kTurnLockStamp, req.U(moveword::kRecordStampB));
        req.SetByte(moveword::kTurnLockFlag, 1);
        req.SetU(moveword::kTurnLockTicks, static_cast<uint32_t>(ticks));
        req.SetByte(moveword::kTurnLockDir, 0);
      }
    }
  }
  req.SetU(kWordClass, 0xffffffffu);
  req.SetF(kWordSpeed, 0.0f);
  if (!StayDecision(host, req)) {
    if (total <= current || fixed <= current) {
      const float error = std::fabs(req.F(moveword::kFacingError));
      const bool turnCandidate =
          req.U(moveword::kLookBucketAlt) != 0 && req.U(moveword::kLookBucketAlt) != 5 &&
          k.deg3375 <= error && req.U(moveword::kCommandId) != 0x79;
      if (fixed <= current || turnCandidate) {
        if (!turnCandidate) {
          const int kind = static_cast<int>(req.U(moveword::kKind));
          const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
          const MoveCell cell = host.Cell(row, 9, false);
          bool mirror = false;
          if (kind == 3 || kind == 6) {
            mirror = host.Percent(0x32);
          } else {
            const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
            mirror = host.Cell(row, column, false).mirror;
          }
          req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
          req.SetByte(moveword::kMirror, mirror ? 1 : 0);
          req.SetF(moveword::kDuration, 0.05f);
          req.SetU(moveword::kStartTime, 0);
          req.SetF(moveword::kPlayback, k.one);
          ResetDwell(req, 0.0f, 0.0f, 0.0f, 0.05f);
          req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
          RequestState(req, MoveState::kIdle);
        } else {
          EnterIdleTurn(host, req);
        }
      }
    }
    return;
  }
  EnterGotoRun(host, req, req.F(moveword::kPrecise) == 1.0f);
}

// FUN_1406bf7f0: IDLE_OTHER.
void IdleOther(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  req.SetU(kWordClass, 0xffffffffu);
  req.SetF(kWordSpeed, 0.0f);
  if (StayDecision(host, req)) {
    EnterGotoRun(host, req, false);
    return;
  }
  const float error = std::fabs(req.F(moveword::kFacingError));
  const bool turnCandidate =
      req.U(moveword::kLookBucketAlt) != 0 && req.U(moveword::kLookBucketAlt) != 5 &&
      k.deg3375 <= error && req.U(moveword::kCommandId) != 0x79;
  if (req.F(moveword::kDwellElapsed2) < req.F(moveword::kDwellLimit2)) {
    if (req.U(moveword::kCommandSpeedClass) != 0xffffffffu && req.F(moveword::kPrecise) == 1.0f &&
        StepCandidate(req)) {
      EnterIdleTurn(host, req);
      return;
    }
  } else if (turnCandidate) {
    EnterIdleTurn(host, req);
    return;
  }
  const int kind = static_cast<int>(req.U(moveword::kKind));
  int row = kIdleRowFallback;
  if (kind < 8) row = IdleRow(host, req);
  const MoveCell cell = host.Cell(row, 9, false);
  bool mirror = false;
  if (kind == 3 || kind == 6) {
    mirror = host.Percent(0x32);
  } else {
    const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
    mirror = host.Cell(row, column, false).mirror;
  }
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
  req.SetF(moveword::kDuration, 0.3f);
  req.SetU(moveword::kStartTime, 0);
  req.SetF(moveword::kPlayback, k.one);
  ResetDwell(req, 0.0f, 0.0f, 0.0f, 0.05f);
  req.SetByte(moveword::kMotionPending, 1);
  req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
  req.SetByte(moveword::kMirror, mirror ? 1 : 0);
  RequestState(req, MoveState::kIdle);
  req.SetByte(moveword::kTurnLockFlag, 0);
  RootFacingTail(host, req);
}

// FUN_1406bfd10: IDLE_PK_PLAYER.
void IdlePkPlayer(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  req.SetU(kWordClass, 0xffffffffu);
  req.SetF(kWordSpeed, 0.0f);
  if (StayDecision(host, req)) {
    EnterGotoRun(host, req, false);
    return;
  }
  const int kind = static_cast<int>(req.U(moveword::kKind));
  if (req.U(moveword::kLiveKind) != static_cast<uint32_t>(kind)) {
    ResetDwell(req, 0.0f, 0.0f, 0.0f, 0.25f);
    req.SetF(kWordBlend, k.half);
    req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
    req.SetByte(moveword::kMotionPending, 1);
    const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
    const MoveCell cell = host.Cell(row, 9, false);
    req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
    bool mirror = false;
    if (kind == 3 || kind == 6) {
      mirror = host.Percent(0x32);
    } else {
      const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
      mirror = host.Cell(row, column, false).mirror;
    }
    req.SetByte(moveword::kMirror, mirror ? 1 : 0);
    req.SetF(moveword::kDuration, 0.25f);
    req.SetF(moveword::kPlayback, k.one);
    RequestState(req, MoveState::kIdle);
  }
}

// FUN_1406bfb70: IDLE_PK_GK.
void IdlePkGk(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  req.SetU(kWordClass, 0xffffffffu);
  req.SetF(kWordSpeed, 0.0f);
  if (StayDecision(host, req)) {
    EnterGotoRun(host, req, false);
    return;
  }
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int fixed = host.ClipTicks(pes::ClipReader::kBlend, host.CurrentMotion(), k.one, 0);  // FUN_1406af620
  if ((req.U(moveword::kLiveKind) == static_cast<uint32_t>(kind) || kind == 4) &&
      host.CurrentClipFrames() < fixed) {
    return;
  }
  ResetDwell(req, 0.0f, 0.0f, 0.0f, 0.25f);
  req.SetF(kWordBlend, k.half);
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
  req.SetByte(moveword::kMotionPending, 1);
  const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
  const MoveCell cell = host.Cell(row, 9, false);
  req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
  bool mirror = false;
  if (kind == 3 || kind == 6) {
    mirror = host.Percent(0x32);
  } else {
    const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
    mirror = host.Cell(row, column, false).mirror;
  }
  req.SetByte(moveword::kMirror, mirror ? 1 : 0);
  req.SetF(moveword::kDuration, 0.3f);
  req.SetF(moveword::kPlayback, k.one);
  RequestState(req, MoveState::kIdle);
}

// FUN_1406ca6b0: TIMINGSTEP. The hit-record reads keep PES's no-hit fallback until the records
// land, so the handler's hit-ladder arm stays on its no-hit branch.
void TimingStep(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  req.SetU(kWordClass, 0xffffffffu);
  req.SetF(kWordSpeed, 0.0f);
  const int motion = host.CurrentMotion();
  const int fixed = host.ClipTicks(pes::ClipReader::kBlend, motion, k.one, 0);  // FUN_1406af620
  const pes::MotionRecord& rec = host.Motion(motion);
  const int subCount = rec.CancelCount();
  int end = fixed;
  if (0 < subCount) {
    const int last = host.ClipTicks(pes::ClipReader::kCancel, motion, k.one, subCount - 1);  // FUN_1406af8e0
    if (fixed < last) end = last;
  }
  const bool scripted = host.ActionTableOwns(host.ActionIndex());
  const int current = host.CurrentClipFrames();
  const float turn = std::fabs(PesAngleDiff(req.F(moveword::kBodyDirection), req.F(moveword::kHeading), k));
  const bool blocked = current < end && req.U(moveword::kCommandId) != 5 && scripted &&
                       (k.deg90 <= turn || req.F(moveword::kPrecise) != 0.0f ||
                        host.CommandSpeedClass() < 3);
  if (!blocked) {
    if (!StayDecision(host, req)) {
      const int kind = static_cast<int>(req.U(moveword::kKind));
      const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
      const MoveCell cell = host.Cell(row, 9, false);
      bool mirror = false;
      if (kind == 3 || kind == 6) {
        mirror = host.Percent(0x32);
      } else {
        const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
        mirror = host.Cell(row, column, false).mirror;
      }
      req.SetByte(moveword::kMotionPending, 1);
      req.SetByte(moveword::kChangeState, 1);
      req.SetU(moveword::kNextState, static_cast<uint32_t>(MoveState::kIdle));
      req.SetF(moveword::kDwellLimit, 0.0f);
      req.SetF(moveword::kDwellLimit2, 0.05f);
      req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
      req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
      req.SetByte(moveword::kMirror, mirror ? 1 : 0);
      req.SetF(moveword::kPlayback, k.one);
      req.SetU(moveword::kStartTime, 0);
      req.SetU(kWordClass, 0xffffffffu);
      req.SetF(kWordSpeed, 0.0f);
      req.SetByte(moveword::kTurnLockFlag, 0);
    } else {
      const float bodyTurn =
          std::fabs(PesAngleDiff(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k));
      if (k.deg675 < bodyTurn) {
        req.SetU(kWordClass, 0);
        req.SetF(kWordSpeed, 5.0f);
        if (EnterStepTurnRun(host, req)) return;
      }
      req.SetU(kWordClass, 0xffffffffu);
      req.SetF(kWordSpeed, 0.0f);
      EnterGotoRun(host, req, false);
    }
  }
}

// FUN_1406bdd70: DEMO.
void Demo(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const MoveConstants& m = host.MoveConstantsView();
  req.SetU(kWordClass, 0xffffffffu);
  req.SetF(kWordSpeed, 0.0f);
  const int fixed = host.ClipTicks(pes::ClipReader::kBlend, host.CurrentMotion(), k.one, 0);  // FUN_1406af620
  const float span = host.TicksPerSecond() * m.demoSpan / k.kmh60 + k.half;
  if (static_cast<float>(fixed) - span <= static_cast<float>(host.CurrentClipFrames()) ||
      req.F(moveword::kDwellLimit) <= req.F(moveword::kDwellElapsed)) {
    const int kind = static_cast<int>(req.U(moveword::kKind));
    req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
    ResetDwell(req, 0.0f, 0.0f, 0.0f, 0.25f);
    req.SetF(kWordBlend, k.half);
    req.SetByte(moveword::kMotionPending, 1);
    const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
    const MoveCell cell = host.Cell(row, 9, false);
    req.SetU(moveword::kMotion, static_cast<uint32_t>(cell.motionNo));
    bool mirror = false;
    if (kind == 3 || kind == 6) {
      mirror = host.Percent(0x32);
    } else {
      const int column = (k.negHalf < req.F(moveword::kFacingError)) ? 9 : 8;
      mirror = host.Cell(row, column, false).mirror;
    }
    req.SetByte(moveword::kMirror, mirror ? 1 : 0);
    req.SetF(moveword::kDuration, 0.3f);
    req.SetF(moveword::kPlayback, k.one);
    RequestState(req, MoveState::kIdle);
  }
}

// FUN_1406b6bb0 (StayDecision, AS 2.3): true sends the idle player to GOTO_RUN (FUN_1406bf0c0 lines 39-42), and a STOP takes its
// run re-plan on true (FUN_1406c9e20). Past the threshold distance the answer is the bearing gate: 112.5 degrees (DAT_1425cb30c),
// or 45 * share + 67.5 when the record's actions are 5 and 5 (67.5 in phase 2), and 180 when the bearing to the ball is past 90.
// A command that is not precise (req[0xe] != 1) answers true at any facing error (FUN_1406b6bb0 line 98).
bool StayDecision(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const MoveConstants& m = host.MoveConstantsView();
  if (req.U(moveword::kCommandSpeedClass) == 0xffffffffu) return false;

  // DAT_142588200 (0.1) when the precise word is set, DAT_142598578 (0.2) otherwise. rate01 is the same 0.1, at 0x1425ddf48.
  float threshold = req.U(moveword::kPrecise) != 0u ? k.rate01 : k.blend2;
  const int phase = host.MatchPhase();
  if (phase != 2 || host.MatchState11d4() != 0) {
    const int command = static_cast<int>(req.U(moveword::kCommandId)) - 1;
    switch (command) {
      case 0:
      case 1:
        if (host.RecordActionA() != 5 || host.RecordActionB() != 5) threshold = k.blend2;
        break;
      case 0x3e:
      case 0x4c:
        if (req.U(moveword::kKind) - 4u > 1u) break;
      case 0x51:
        threshold = k.rate01;
        break;
      case 0x50:
        if (((req.U(moveword::kKind) - 2u) & 0xfffffffcu) == 0 && req.U(moveword::kKind) != 3)
          threshold = k.blend2;
        break;
      default:
        break;
    }
  } else {
    threshold = k.half;  // DAT_142565600: phase 2 with MI+0x11d4 clear
  }

  const float dist = req.F(moveword::kDistToTarget);
  if (!(threshold < dist)) return false;
  float share = 0.0f;
  if (k.half < dist) {
    share = k.one;
    if (dist < m.dist3) share = (dist - k.half) / m.dist2m5;
  }
  float bearingGate = k.deg1125b;  // DAT_1425cb30c: PES's default gate (the port had the 3.0 m span here)
  if (host.RecordActionA() == 5 && host.RecordActionB() == 5) {
    bearingGate = share * k.deg45 + k.deg675;
    if (phase == 2) bearingGate = k.deg675;
  }
  float bearing = host.BallBearingDeg() - req.F(moveword::kLookHeading);
  if (bearing < 0.0f) bearing += k.full;
  if (k.full <= bearing) bearing += k.wrapFull;
  if (k.half_turn < std::fabs(bearing)) {
    bearing -= (bearing < 0.0f ? k.neg : k.one) * k.full;
  }
  if (k.deg90 < std::fabs(bearing)) bearingGate = k.half_turn;
  const float error = req.F(moveword::kFacingError);
  return req.U(moveword::kPrecise) != 1u || error * error <= bearingGate * bearingGate;
}

// FUN_1406b93a0: the idle row id for the request.
int IdleRow(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const MoveConstants& m = host.MoveConstantsView();
  const IdleRowTables& tables = host.IdleRowTablesView();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int phase = host.MatchPhase();
  const bool actionOwns = phase == 3 && host.ActionTableOwns(host.ActionIndex());

  bool populated = true;
  const bool flag = host.MatchObject3b0() && host.TrainerFlag1423e7fd0();
  const int slot = host.ActionIndex();
  if (0 <= slot && slot < 0x16 && !host.ActionSlotMarked(slot)) populated = false;

  if (host.MatchModeByte() == 0x18 || !populated) return tables.fatigue[host.PickN(4)];
  if (flag || slot > 0x15) return tables.flag[host.PickN(4)];
  switch (kind) {
    case 2:
      if (!actionOwns) return tables.kind2[host.PickN(4)];
      [[fallthrough]];
    case 5:
      return kIdleRowKind25;
    case 3: {
      int condition = 100;
      if (slot < 0x16) condition = host.RecordConditionByte(slot);
      if ((phase == 3 && host.TicksPerSecond() * m.dist3 < static_cast<float>(host.PhaseTick())) ||
          0x13 < condition) {
        if (phase == 1 || host.MatchState1554() != 6) return tables.mode[host.PickN(0xc)];
        return tables.mode2[host.PickN(4)];
      }
      int row = tables.slot[host.PickN(2)];
      if (condition < 5 && host.Percent(0x19)) row = tables.slotAlt[host.PickN(2)];
      return row;
    }
    case 4:
      return kIdleRowKind4;
    case 6:
      return tables.kind6[host.PickN(2)];
    case 7:
      return kIdleRowKind7;
    default: {
      int row = tables.near[host.PickN(4)];
      const int related = host.RelatedSlot();
      if (0 <= related && related < 0x16) {
        float other[3] = {0.0f, 0.0f, 0.0f};
        host.RecordPosition(related, other);
        float pos[3] = {0.0f, 0.0f, 0.0f};
        host.OwnPosition(pos);
        const float dx = pos[0] - other[0];
        const float dz = pos[2] - other[2];
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < m.near01) {
          const int index = host.ActionIndex();
          const bool indexOtherSide = 10 < index && index < 0x16;
          if (indexOtherSide != (10 < related) &&
              k.deg135 < std::fabs(PesAngleDiff(host.FacingDeg(), host.RecordFacing(related), k))) {
            row = tables.far[host.PickN(4)];
          }
        }
      }
      return row;
    }
  }
}

}  // namespace movestep
}  // namespace pes
