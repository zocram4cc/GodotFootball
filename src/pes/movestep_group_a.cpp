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
// A record value in ticks at rate 1.0 (FUN_1406af620, FUN_1406b1f00, FUN_1406b1d00 and FUN_1406af8e0
// are called with DAT_14256ae1c in these handlers).
int MotionTicks(const MoveHost& host, int value, const RunConstants& k) {
  return pes::movestep::RecordTicks(host, static_cast<float>(value), k.one, k.kmh60, k.half, k.one);
}

// The request words group A reads or writes that the shared contract does not name yet.
constexpr int kWordBlend = 0x34;         // req[0x34]: the idle family's blend word (0.5)
constexpr int kWordClass = 0x1f;         // req[0x1f]: the live class / rate band
constexpr int kWordSpeed = 0x1d;         // req[0x1d]: the live speed word, km/h packed
constexpr int kWordBand = 0x22;          // req[0x22]: the band the request starts in
constexpr int kWordPrevBand = 0x24;      // req[0x24]: the band the request runs at
constexpr int kWordBucket = 0x20;        // req[0x20]: the 10-way angle bucket echo
constexpr int kWordReorient = 0x3c;      // req[0x3c] byte: a reorient / turn lock is due
constexpr int kWordTurnLockStamp = 0x3d;  // req[0x3d]: IDLE_TURN's lock stamp
constexpr int kWordTurnLockTicks = 0x3e;  // req[0x3e]: the lock's tick count
constexpr int kWordTurnLockDir = 0x3f;    // req[0x3f] byte: the lock's direction flag

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
      MotionTicks(host, host.Motion(static_cast<int>(req.U(moveword::kMotion))).BlendStart(), k);
  float limit = static_cast<float>(host.PickN(3)) / host.TicksPerSecond() + k.kmh5;
  const float frames = static_cast<float>(fixed) / host.TicksPerSecond();
  limit = (limit <= frames && 0.0f <= frames) ? limit : 0.0f;
  req.SetF(moveword::kDwellElapsed, 0.0f);
  req.SetF(moveword::kDwellLimit, limit);
  req.SetF(moveword::kDwellElapsed2, 0.0f);
  req.SetF(moveword::kDwellLimit2, 0.1667f);
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
  RequestState(req, MoveState::kIdleOther);
  req.SetByte(kWordReorient, 0);
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

// FUN_1406d1390: the idle family's turn exit into IDLE_TURN. PES's row table at 0x1425dda30 is
// not decoded; the shared IdleRow is the port's stand-in until it lands.
// ponytail: 0x1425dda30's row table removes this stand-in.
void EnterIdleTurn(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const int kind = static_cast<int>(req.U(moveword::kKind));
  const int row = kind < 8 ? IdleRow(host, req) : kIdleRowFallback;
  SetIdleClip(host, req, row, k.blend1667, 0.1667f);
  req.SetF(moveword::kDwellLimit2, 0.1667f);
  req.SetF(moveword::kDwellElapsed2, 0.0f);
  req.SetByte(moveword::kLiveKind, static_cast<uint8_t>(kind));
  RequestState(req, MoveState::kIdleTurn);
  req.SetByte(kWordReorient, 0);
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
      req.SetByte(kWordReorient, 0);
    } else {
      req.SetU(kWordClass, 0xffffffffu);
      req.SetU(kWordSpeed, 0);
      EnterGotoRun(host, req, false);
    }
    return;
  }

  const float bodyTurn = AngleDiffDeg(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k);
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
    const int bucket = AngleBucket(host.AngleBuckets(),
                                   AngleDiffDeg(req.F(moveword::kHeading), req.F(moveword::kFacing), k),
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
  const int fixed = MotionTicks(host, host.Motion(host.CurrentMotion()).BlendStart(), k);
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
  const pes::MotionRecord& rec = host.Motion(host.CurrentMotion());
  const int start = MotionTicks(host, rec.Rot(0).start, k);
  const int total = MotionTicks(host, rec.Rot(0).end, k);
  const int fixed = MotionTicks(host, rec.BlendStart(), k);
  const int current = host.CurrentClipFrames();
  if (!host.IsReplay() && start <= current) {
    const int span = start + total * 2;
    if (current < span / 3) {
      const int ticks = static_cast<int>(static_cast<float>(total - current) / host.RecordF2468() + k.half);
      const float turn =
          std::fabs(AngleDiffDeg(static_cast<float>(span), req.F(moveword::kRecordStampB), k)) /
          static_cast<float>(ticks);
      if (turn < k.deg45) {
        req.SetU(kWordTurnLockStamp, req.U(moveword::kRecordStampB));
        req.SetByte(kWordReorient, 1);
        req.SetU(kWordTurnLockTicks, static_cast<uint32_t>(ticks));
        req.SetByte(kWordTurnLockDir, 0);
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
  req.SetByte(kWordReorient, 0);
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
  const int fixed = MotionTicks(host, host.Motion(host.CurrentMotion()).BlendStart(), k);
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
  const pes::MotionRecord& rec = host.Motion(host.CurrentMotion());
  const int fixed = MotionTicks(host, rec.BlendStart(), k);
  const int subCount = rec.CancelCount();
  int end = fixed;
  if (0 < subCount) {
    const int last = MotionTicks(host, rec.CancelFrames(subCount - 1), k);
    if (fixed < last) end = last;
  }
  const bool scripted = host.ActionTableOwns(host.ActionIndex());
  const int current = host.CurrentClipFrames();
  const float turn = std::fabs(AngleDiffDeg(req.F(moveword::kBodyDirection), req.F(moveword::kHeading), k));
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
      req.SetByte(kWordReorient, 0);
    } else {
      const float bodyTurn =
          std::fabs(AngleDiffDeg(req.F(moveword::kFacing), req.F(moveword::kBodyDirection), k));
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
  const int fixed = MotionTicks(host, host.Motion(host.CurrentMotion()).BlendStart(), k);
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

// FUN_1406b6bb0: INIT's stay decision; true = the player stays idle this tick.
bool StayDecision(MoveHost& host, MoveReq& req) {
  const RunConstants& k = host.RunConstantsView();
  const MoveConstants& m = host.MoveConstantsView();
  if (req.U(moveword::kCommandSpeedClass) == 0xffffffffu) return false;

  float threshold = k.blend2;
  if (req.Flag(moveword::kPrecise)) threshold = k.rate01;
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
  }

  const float dist = req.F(moveword::kDistToTarget);
  if (!(threshold < dist)) return false;
  float share = 0.0f;
  if (k.half < dist) {
    share = k.one;
    if (dist < m.dist3) share = (dist - k.half) / m.dist2m5;
  }
  float bearingGate = m.dist3;
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
  return error * error <= bearingGate * bearingGate;
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
              k.deg135 < std::fabs(AngleDiffDeg(host.FacingDeg(), host.RecordFacing(related), k))) {
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
