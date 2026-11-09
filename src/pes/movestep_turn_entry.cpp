// FUN_1406d01d0: see movestep_turn_entry.hpp. Every compare and store cites the instruction it comes from (asm in
// gf-StepAudit2-scratch/a_1406d01d0.txt; the decompile's reuse of float variables is resolved there).
#include "pes/movestep_turn_entry.hpp"

#include <cmath>

#include "pes/movestep_issue.hpp"
#include "pes/movestep_run.hpp"

namespace pes {
namespace {

constexpr uint64_t kFootFlagVa = 0x1425dd930;
constexpr uint64_t kFootFlagTurnVa = 0x1425dd940;

constexpr int kRowTurnA = 0x56f;    // the lock byte 0x3d arm
constexpr int kRowTurnB = 0x570;    // the lock byte 0x3e arm
constexpr int kRowTurnC = 0x571;    // the carrier-contest arm
constexpr int kRowTurnD = 0x572;    // the swing arm
constexpr int kRowStamina = 0x565;  // the tired-man row replacing 0x564 / 0x561
constexpr int kRowTired0 = 0x564;
constexpr int kRowTired1 = 0x561;
constexpr int kMotionLo = 0x8ac, kMotionSkip = 0x8af, kMotionHi = 0x8b1;  // 0x1406d021c, 0x1406d0228, 0x1406d0234
constexpr uint32_t kSlots = 22;            // a slot below 0x16 is a player
constexpr uint32_t kBucketA = 4, kBucketB = 9;  // the look buckets the arms flip between
constexpr int kStaminaTired = 0x14;        // 0x1406d0940 CMP AL,0x14
constexpr int kPercentTiredKeeper = 0x32, kPercentTired = 10;
constexpr int kPercentTrainerKeeper = 0x5f, kPercentTrainer = 0x46;
constexpr int kPercentClose3 = 0x1e, kPercentClose = 0x14;
constexpr int kAnimeClassKeeper = 0x12;
constexpr int kStateStop = 6, kStateGoto = 8;       // the state byte values of the long-duration test, 0x1406d0eeb
constexpr uint32_t kCmdWhistleA = 0x2f, kCmdFirst = 0x1d;  // req[0x16] == 0x2f, or 0x1d..0x1f
constexpr int kRequestLookBlockValidByte = 0x148;  // FUN_140a6e6b0(req + 0x144): the byte at +4
constexpr int kRequestLookPointWord = 0x53;        // FUN_140a76240(req + 0x144) of a clear index: req + 0x144 + 8


float Sign(float v) { return v < 0.0f ? -1.0f : 1.0f; }

}  // namespace

std::optional<TurnEntryTables> LoadTurnEntryTables(const PeImage& exe, std::string* error) {
  TurnEntryTables t;
  if (!exe.ReadVa(kFootFlagVa, t.footFlag.size(), t.footFlag.data()) ||
      !exe.ReadVa(kFootFlagTurnVa, t.footFlagTurn.size(), t.footFlagTurn.data())) {
    *error = "turn entry tables: .rdata unreadable";
    return std::nullopt;
  }
  return t;
}

namespace {

// The shared look-bucket flip of the arms: bucket 4 <-> 9, the look delta set to -180 / +180 by the bucket it LEAVES
// (0x1406d04d6..0x1406d04f0: the delta is the old bucket's, 4 gives -180, the new bucket is 9).
void FlipLookBucket(MoveReq& req, const RunConstants& k) {
  using namespace moveword;
  const bool four = req.U(kLookBucket) == kBucketA;
  req.SetF(kLookDelta, four ? k.wrapHalf : k.half_turn);
  req.SetU(kLookBucket, four ? kBucketB : kBucketA);
}

}  // namespace

bool EnterGotoTurn(MoveHost& host, MoveReq& req) {
  using namespace moveword;
  const RunConstants& k = host.RunConstantsView();
  const RunTables& t = host.RunTablesView();
  const TurnEntryTables& tt = host.TurnEntryTablesView();
  const float tps = host.TicksPerSecond();

  if (req.U(kCommandSpeedClass) == 0xffffffffu) req.SetU(kCommandSpeedClass, 1u);  // 0x1406d01e4
  if (req.U(kPrecise) != 0u) return false;                                          // 0x1406d01f7
  const int motion = host.CurrentMotion();
  if (motion < kMotionLo || motion == kMotionSkip || motion > kMotionHi) return false;

  // 0x1406d02b8..0x1406d0313: the match-up flag picks the DA byte for the playback.
  const float playback = EntryPlayback(host);
  req.SetF(kPlayback, playback);  // 0x1406d0377

  bool forced = false;    // local_res18's low byte: an arm flipped the look bucket or took the swing
  bool swingArm = false;  // local_res20

  // The ball sample the arms steer by: (clock * 20 / 60 + 0.5) ticks ahead (0x1406d0570..0x1406d0597), as a heading.
  const auto ballHeading = [&]() {
    float ball[3];
    host.BallSample(static_cast<int>(tps * k.reserveLow / k.kmh60 + k.half), ball);
    return host.HeadingToPointDeg(ball);
  };
  // |FUN_14043a0a0(req[2], req[3])|: the look point's turn from the facing.
  const auto lookOffFacing = [&]() { return std::fabs(PesAngleDiff(req.F(kFacing), req.F(kLookHeading), k)); };
  // The carrier is out of the roster, or the look point lies 22.5..157.5 degrees off the facing.
  const auto lookOverride = [&](int carrier) {
    const float off = lookOffFacing();
    return static_cast<uint32_t>(carrier) > 0x15u && k.deg225 < off && off < k.deg1575;
  };
  // The ball arm's heading less the facing P+0x20, wrapped to 180: `reference` is the ball heading, or the folded look
  // heading when the override holds.
  const auto turnFrom = [&](float reference) {
    float turn = reference - host.FacingDeg();
    if (k.half_turn < std::fabs(turn)) turn -= Sign(turn) * k.full;
    return turn;
  };

  if (req.U(kLookBucket) == kBucketA || req.U(kLookBucket) == kBucketB) {  // 0x1406d0387..0x1406d038f
    float look = req.F(kHeading);
    const float facingError = std::fabs(req.F(kFacingError));
    if (req.FlagAtByte(kRequestLookBlockValidByte)) {
      float point[3];
      for (int i = 0; i < 3; ++i) point[i] = req.F(kRequestLookPointWord + i);
      look = host.HeadingToPointDeg(point);  // 0x1406d03d1: FUN_140439e50(P+8, the block's point)
    }
    const float lookTurn = PesAngleDiff(req.F(kHeading), look, k);  // 0x1406d03da..0x1406d0413 (FUN_14043a0a0 inlined)
    const float lookMag = std::fabs(lookTurn);
    bool taken = false;
    if (k.deg1125 < facingError && facingError < k.deg1575) {  // 0x1406d043d, 0x1406d0447
      if (Sign(lookTurn) == Sign(req.F(kFacingError)) || k.deg135 < lookMag || lookMag < k.deg45) {
        if (Sign(req.F(kLookDelta)) != Sign(req.F(kFacingError))) {
          forced = true;
          FlipLookBucket(req, k);
        }
        taken = true;
      }
    }
    if (!taken) {
      const float ballDeg = ballHeading();
      const int carrier = host.CarrierSlot();
      const uint32_t cmd = req.U(kCommandId);
      const bool always = cmd == kCmdWhistleA || cmd - kCmdFirst < 3u;
      const float reference = (always || lookOverride(carrier)) ? FoldFull(req.F(kLookHeading), k) : ballDeg;
      const float turn = turnFrom(reference);
      const float turnMag = std::fabs(turn);
      if (k.turn16875 < turnMag && turnMag < k.deg163p125) {
        if (Sign(lookTurn) == Sign(turn) || k.deg135 < lookMag || lookMag < k.deg45) {
          if (Sign(req.F(kLookDelta)) != Sign(turn)) {
            forced = true;
            FlipLookBucket(req, k);
          }
        }
      }
    }
  }

  // Arm 2 (0x1406d050c): no lock bytes, a wide look delta, a class above 1 and the bucket 3 or 8.
  const uint32_t bucketNow = req.U(kLookBucket);
  if (!req.FlagAtByte(0x3d) && !req.FlagAtByte(0x3e) && k.deg1125b < std::fabs(req.F(kLookDelta)) &&
      1 < static_cast<int>(req.U(kCommandSpeedClass)) && (bucketNow == 3 || bucketNow == 8)) {
    const float ballDeg = ballHeading();
    const int carrier = host.CarrierSlot();
    const uint32_t cmd = req.U(kCommandId);
    const bool always = cmd == kCmdWhistleA || cmd - kCmdFirst < 3u;
    const float reference = (always || lookOverride(carrier)) ? FoldFull(req.F(kLookHeading), k) : ballDeg;
    const float turn = turnFrom(reference);
    if (k.deg45 < std::fabs(turn) && Sign(req.F(kLookDelta)) != Sign(turn)) {
      forced = true;
      swingArm = true;
    }
  }

  // The default cell (0x1406d0871..0x1406d099e): the goto row of (kind, class) at the look bucket's column.
  const int kind = static_cast<int>(req.U(kKind));
  const int cls = static_cast<int>(req.U(kCommandSpeedClass));
  int row = t.gotoRow[kind + cls * 8];
  const int column = t.bucketColumn[req.U(kLookBucket)];
  if (swingArm) {
    row = kRowTurnD;
  } else {
    const int slot = host.ActionIndex();
    if (static_cast<uint32_t>(slot) < kSlots && (row == kRowTired0 || row == kRowTired1) &&
        static_cast<uint32_t>(column - 8) < 2u && host.StaminaPercent() < kStaminaTired &&
        tps * host.MoveConstantsView().dist3 < static_cast<float>(host.RecordTicks(slot)))
      row = kRowStamina;  // 0x1406d093a..0x1406d0963
  }
  // FUN_1406abb30 answers 0 for a missing cell (its walk argument is 0) and FUN_1406abd60 reads the mirror table regardless.
  int cellMotion = 0;
  bool cellMirror = false;
  const auto pick = [&](int pickRow) {
    const MoveCell c = host.Cell(pickRow, t.bucketColumn[req.U(kLookBucket)], false);
    cellMotion = c.found ? c.motionNo : 0;
    cellMirror = c.mirror;
  };
  {
    const MoveCell c = host.Cell(row, column, false);
    cellMotion = c.found ? c.motionNo : 0;
    cellMirror = c.mirror;
  }
  bool lockArm = false;

  const bool lock3d = req.FlagAtByte(0x3d), lock3e = req.FlagAtByte(0x3e);
  if ((lock3d || lock3e) && 1 < static_cast<int>(req.U(kCommandSpeedClass))) {  // 0x1406d09ac
    if (lock3d && !swingArm) {
      pick(kRowTurnA);
    } else {
      const uint32_t bucket = req.U(kLookBucket);
      if (!forced && (bucket == kBucketA || bucket == kBucketB)) {
        const float facingError = std::fabs(req.F(kFacingError));
        bool flipNow;
        float turn = 0.0f;
        if (facingError <= k.deg1125 || k.deg1575 <= facingError) {
          // The extreme facing error: the ball arm decides (0x1406d0ac3..0x1406d0b40).
          const float ballDeg = ballHeading();
          const int carrier = host.CarrierSlot();
          turn = turnFrom(lookOverride(carrier) ? FoldFull(req.F(kLookHeading), k) : ballDeg);
          const float mag = std::fabs(turn);
          flipNow = k.deg225 < mag && mag < k.deg1575 && Sign(req.F(kLookDelta)) != Sign(turn);
        } else {
          flipNow = Sign(req.F(kLookDelta)) != Sign(req.F(kFacingError));
        }
        if (flipNow) {
          const uint32_t to = (bucket - 3u) < 2u ? kBucketB : kBucketA;  // 0x1406d0be9: 1 < bucket - 3 gives 4
          req.SetU(kLookBucket, to);
          req.SetF(kLookDelta, to == kBucketA ? k.wrapHalf : k.half_turn);
        }
      }
      pick(kRowTurnB);
    }
    lockArm = true;
  } else if (host.MatchPhase() == 1 && 0 < static_cast<int>(req.U(kCommandSpeedClass)) &&
             static_cast<uint32_t>(host.ActionIndex()) < kSlots && !swingArm) {  // 0x1406d0c6c
    const bool trainer = host.MatchObjectFlag2c0();  // cVar4
    bool contest = false;
    const int carrier = host.CarrierSlot();
    if (static_cast<uint32_t>(carrier) < kSlots) {
      float own[3], other[3];
      host.OwnPosition(own);
      host.RecordPosition(carrier, other);
      const float dz = own[2] - other[2];
      const float dx = own[0] - other[0];
      const float dist = std::sqrt(dz * dz + dx * dx);  // FUN_1415a1780
      const int slot = host.ActionIndex();
      const bool ownSide = 0 <= slot && 10 < slot && slot < static_cast<int>(kSlots);
      if (!(k.kmh5 <= dist) && ownSide != (10 < carrier)) {
        const bool keeper = host.AnimeRecordClass(carrier) == kAnimeClassKeeper;
        const int pct = trainer ? (keeper ? kPercentTrainerKeeper : kPercentTrainer)
                                : (keeper ? kPercentTiredKeeper : kPercentTired);
        contest = host.Percent(pct);
      }
    }
    bool enter = true;
    if (!contest) {
      // 0x1406d0dd4: the class word below 1, or a request turn within 135 degrees, ends the arm.
      if (static_cast<int>(req.U(kClassWord)) < 1 || std::fabs(req.F(kTurn)) <= k.deg135) {
        enter = false;
      } else {
        const bool three = req.U(kCommandSpeedClass) == 3u;
        const int pct = trainer ? (three ? kPercentClose3 : kPercentClose) : (three ? 1 : 0);
        enter = host.Percent(pct);
      }
    }
    if (enter) {
      pick(kRowTurnC);
      host.CountAnalyserEvent(trainer, cellMotion);  // FUN_1406cba60(P, trainer, motion)
    }
  }
  // 0x1406d0e90 / 0x1406d0ea8: the lock arm reads the turn table, every other path the plain one, at the final bucket.
  req.SetByte(kFootFlag, lockArm ? tt.footFlagTurn[req.U(kLookBucket)] : tt.footFlag[req.U(kLookBucket)]);
  float duration = k.blend1667;  // DAT_1425ddf50
  if (host.AnimeState() == kReplayAnimeState && (host.CurrentState() == kStateStop || host.CurrentState() == kStateGoto) &&
      static_cast<int>(req.U(kCommandSpeedClass)) < 1 && req.U(kKind) == 3u)
    duration = k.blend04;  // DAT_14259b89c
  const int frames = host.CurrentClipFrames();
  const MotionRequest issue{cellMotion, cellMirror, 0, duration, playback, static_cast<float>(frames + 1) / tps};
  WriteIssue(host, req, issue);
  CopyHitWords(req, cellMotion);

  // The root-facing rate: the facing plus the look bucket's centre, folded, against the heading, over the hit seconds.
  float rootFacing = FoldFull(req.F(kFacing), k);
  rootFacing = FoldFull(rootFacing + host.AngleBuckets().centre[req.U(kLookBucket)], k);
  const float bearing = PesAngleDiff(rootFacing, req.F(kHeading), k);
  req.SetF(kRecordStampA, req.F(kDwellLimit) == 0.0f ? 0.0f : bearing / req.F(kDwellLimit));

  // The class and speed words (0x1406d1060..).
  const int cmdClass = static_cast<int>(req.U(kCommandSpeedClass));
  req.SetU(kPrevClassWord, static_cast<uint32_t>(cmdClass < 0 ? 0 : (cmdClass > 3 ? 3 : cmdClass)));
  const float speed = req.F(kDesiredSpeedKmh);
  float band = speed <= k.kmh28 ? (k.kmh5 <= speed ? speed : k.kmh5) : k.kmh28;
  req.SetF(kSpeedBand, band);  // req[0x22]
  const int bandIndex = cmdClass - 1;
  req.SetU(kClassWord, static_cast<uint32_t>(bandIndex < 3 ? (bandIndex < 0 ? 0 : bandIndex) : 2));
  float word = speed <= k.kmh16 ? (k.kmh5 <= speed ? speed : k.kmh5) : k.kmh16;
  req.SetF(kStopSpeedWord, word);  // req[0x1d]
  req.SetU(kLiveBucket, req.U(kLookBucket));
  req.SetU(kRecordStampB, req.U(kHeading));
  RequestState(req, MoveState::kGotoRun);
  req.SetU(kLiveKind, req.U(kKind));
  req.SetByte(kTurnLockFlag, 0);
  // P+0x4d0 vt+8 (the AdjustAngle adjuster's apply): a no-op in this port (AdjustAngleApply, movestep_turn.cpp).
  return true;
}

}  // namespace pes
