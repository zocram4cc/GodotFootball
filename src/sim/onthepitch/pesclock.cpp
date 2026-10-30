#include "onthepitch/pesclock.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace pes {

namespace {

// FUN_140A3C070's checksum: FNV-1a 64 over the four decoded bytes, LSB first.
constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
constexpr uint64_t kFnvPrime = 0x100000001b3ULL;
// The slot decode: K_i is byte i of (key * 3 + 4 * i), and the mask is K_i * 63.
constexpr uint32_t kKeyMultiplier = 3;
constexpr uint32_t kKeyStep = 4;
constexpr uint32_t kMaskMultiplier = 63;
constexpr int kSlotBytes = 4;
constexpr uint32_t kRingSlots = 4;

uint8_t KeyByte(uint32_t key, int i) {
  return static_cast<uint8_t>((key * kKeyMultiplier + kKeyStep * static_cast<uint32_t>(i)) >> (8 * i));
}

uint8_t MaskByte(uint8_t k) { return static_cast<uint8_t>(k * kMaskMultiplier); }

// Decodes one slot to the stamp's bits: ((L_i + K_i) mod 256) XOR ((K_i * 63) mod 256).
uint32_t DecodeSlot(uint32_t slot, uint32_t key) {
  uint32_t bits = 0;
  for (int i = 0; i < kSlotBytes; ++i) {
    const uint8_t k = KeyByte(key, i);
    const uint8_t l = static_cast<uint8_t>(slot >> (8 * i));
    const uint8_t b = static_cast<uint8_t>(static_cast<uint8_t>(l + k) ^ MaskByte(k));
    bits |= static_cast<uint32_t>(b) << (8 * i);
  }
  return bits;
}

// The inverse of DecodeSlot: L_i = ((b_i XOR mask) - K_i) mod 256.
uint32_t EncodeSlot(uint32_t bits, uint32_t key) {
  uint32_t slot = 0;
  for (int i = 0; i < kSlotBytes; ++i) {
    const uint8_t k = KeyByte(key, i);
    const uint8_t b = static_cast<uint8_t>(bits >> (8 * i));
    const uint8_t l = static_cast<uint8_t>(static_cast<uint8_t>(b ^ MaskByte(k)) - k);
    slot |= static_cast<uint32_t>(l) << (8 * i);
  }
  return slot;
}

uint64_t StampChecksum(uint32_t bits) {
  uint64_t h = kFnvOffsetBasis;
  for (int i = 0; i < kSlotBytes; ++i)
    h = (h ^ static_cast<uint8_t>(bits >> (8 * i))) * kFnvPrime;
  return h;
}

}  // namespace

bool ReadFloat(const PeImage& exe, uint64_t va, float* out) {
  return exe.ReadVa(va, sizeof(float), reinterpret_cast<uint8_t*>(out));
}

std::optional<PesClockConstants> LoadPesClockConstants(const PeImage& exe, std::string* error) {
  PesClockConstants c;
  bool ok = true;
  ok &= ReadFloat(exe, GAME_SPEED_VA, &c.gameSpeed);
  ok &= ReadFloat(exe, ADDED_TIER_3_6_2ND_VA, &c.addedTier_3_6_2nd);
  ok &= ReadFloat(exe, ADDED_TIER_1_2_2ND_VA, &c.addedTier_1_2_2nd);
  ok &= ReadFloat(exe, ADDED_TIER_1_2_ET_VA, &c.addedTier_1_2_ET);
  ok &= ReadFloat(exe, ADDED_TIER_3_6_ET_VA, &c.addedTier_3_6_ET);
  ok &= ReadFloat(exe, TIME_UP_BASE_VA, &c.timeUpBase);
  ok &= ReadFloat(exe, TIME_UP_FLAG_VA, &c.timeUpFlag);
  ok &= ReadFloat(exe, MINUTE_UNITS_VA, &c.minuteUnits);
  ok &= ReadFloat(exe, SECOND_UNITS_VA, &c.secondsUnit);
  ok &= ReadFloat(exe, LIMIT_CAP_VA, &c.limitCap);
  ok &= ReadFloat(exe, MATCH_SPEED_BASE_VA, &c.matchSpeedBase);
  if (!ok) {
    *error = "PES2021.exe: match clock constants unmapped";
    return std::nullopt;
  }
  return c;
}

uint32_t PeriodNumber(e_MatchPhase phase) {
  switch (phase) {
    case e_MatchPhase_1stHalf: return 1;
    case e_MatchPhase_2ndHalf: return 3;
    case e_MatchPhase_1stExtraTime: return 5;
    case e_MatchPhase_2ndExtraTime: return 7;
    case e_MatchPhase_Penalties: return 9;
    default: return 0;
  }
}

bool PeriodHasClock(uint32_t period) {
  return period == 1 || period == 3 || period == 5 || period == 7;
}

// FUN_1404494E0 writes the period through FUN_14102CAD0. The level cases are FUN_1404493D0 (period
// 3, VA 0x1404493D0) and FUN_1404499D0 (period 7, VA 0x1404499D0). ponytail: PES's knockout flags
// (0x66 extra time, 0x67 shoot-out) have no GF setting. GF plays extra time and a shoot-out after a
// draw (MatchProgression::GetNext), so a level match always takes the break that leads to them.
uint32_t PeriodAfterWhistle(uint32_t period, bool scoresLevel) {
  switch (period) {
    case 1: return 2;                     // the half-time break, fixed
    case 3: return scoresLevel ? 4 : 10;  // the extra-time break, or full time
    case 5: return 6;                     // the extra-time break, fixed
    case 7: return scoresLevel ? 8 : 10;  // the shoot-out break, or full time
    default: return period;
  }
}

// FUN_140A6C050 (the stamp, VA 0x140A6C050) and FUN_140A6BFC0 (LIMIT's product, VA 0x140A6BFC0):
// 90 * E60 * E5C * (60 / 54) / E58 units a frame (A6Writers 6). ponytail: E60 and E5C are 1.0 in the
// live match and GF has no setting for either; add them when a match at another value is read.
// E58 is the match-time setting, which GF's match duration stands for (INF high: one live point, 10).
float PesUnitsPerFrame(const PesClockConstants& c, float matchMinutes) {
  constexpr float kE60 = 1.0f;
  constexpr float kE5C = 1.0f;
  return c.matchSpeedBase * kE60 * kE5C * c.secondsUnit / (c.gameSpeed * matchMinutes);
}

float PeriodStamp(const PeriodRecord& r) {
  const uint32_t bits = DecodeSlot(r.ring[r.ringIdx % kRingSlots], r.key);
  if (StampChecksum(bits) != r.checksum)
    throw std::runtime_error("PES period record: the current slot fails its checksum");
  float stamp;
  std::memcpy(&stamp, &bits, sizeof stamp);
  return stamp;
}

float PeriodClockUnits(const PeriodRecord& r) { return PeriodStamp(r) + r.f; }

void StoreStamp(PeriodRecord& r, float stamp) {
  uint32_t bits;
  std::memcpy(&bits, &stamp, sizeof bits);
  r.ringIdx += 1;
  r.ring[r.ringIdx % kRingSlots] = EncodeSlot(bits, r.key);
  r.checksum = StampChecksum(bits);
}

bool TimeUpReached(const PesClockConstants& c, uint32_t period, float limit, float clock) {
  // FUN_140A6BEF0: the whole minutes of LIMIT, kept to 16 bits as PES keeps them,
  // times 60 for seconds and 60 for units.
  const auto minutes = static_cast<uint16_t>(static_cast<int64_t>(limit / c.minuteUnits));
  const float added = static_cast<float>(minutes) * c.secondsUnit * c.secondsUnit;
  const bool extraTime = period == 5 || period == 7;  // FUN_14050B9B0's flag
  const float length = extraTime ? c.timeUpFlag : c.timeUpBase;
  return length + added <= clock;
}

namespace {

// FUN_14050B9B0's post-gate constants. The zone line is a third of the half length (DAT_14258c264
// = 3.0). The touch timeouts run only under the holder flag 4 (payload+0x20).
constexpr int kCountSeconds = 60;
constexpr int kZoneSeconds = 4;
constexpr int kGroundSeconds = 30;     // R+0xd4: rate * 0x1e, line 297
constexpr int kRingSeconds = 6;        // line 272: the ring-1 touch within rate * 6
constexpr int kTouchFiveSeconds = 5;
constexpr int kTouchTenSeconds = 10;
constexpr int kOutOfZoneFrames = 30;   // line 227: the newest touch's age, 0x1e frames
constexpr float kZoneDivisor = 3.0f;
constexpr float kGroundHeight_m = 1.0f;  // DAT_14256ae1c: the ball is on the ground below this
constexpr uint32_t kHolderFlag = 4;
constexpr uint32_t kKindTwo = 2;
constexpr uint32_t kKindThree = 3;
constexpr uint32_t kKindFour = 4;
constexpr uint32_t kRestartKindOne = 1;
constexpr uint32_t kKindElevenBase = 0xb;  // kinds 11..13 are `kind - 0xb < 3` (line 224)
constexpr uint32_t kPlayerCount = 0x16;    // player ids below this are on the pitch (line 222)
constexpr uint32_t kSideSplitId = 10;      // ids above this are the away side (line 223)

// The out-of-zone pattern of a kind-0 touch (lines 202-231): the ball in the near half, the
// newest and the touch before it by players of opposite sides, and either the touch before a
// kind 11..13 or the newest touch more than 30 frames old.
bool OutOfZonePattern(const TimeUpInput& in) {
  if (!(in.ballX_m < in.halfLength_m / kZoneDivisor)) return false;
  const uint32_t newest = in.newest.player;
  const uint32_t second = in.second.player;
  if (newest >= kPlayerCount || second >= kPlayerCount) return false;
  if ((newest > kSideSplitId) == (second > kSideSplitId)) return false;
  if (in.newest.kind - kKindElevenBase <= 2u) return false;
  if (in.second.kind - kKindElevenBase < 3u) return true;
  return static_cast<uint32_t>(in.frame - in.newest.frame) > kOutOfZoneFrames;
}

}  // namespace

bool TimeUpWhistle(TimeUpCounters& c, const TimeUpInput& in, int rate) {
  // Blowing clears every count, as FUN_14050C3E0 does after a true return.
  const auto blow = [&c] {
    c = TimeUpCounters{};
    return true;
  };

  // ponytail: the penalty-taker hold (FUN_14050B9B0 lines 52-66, G+0x2920) and the R+8 flag
  // (line 67, no writer traced) are not ported; neither can return early here.
  // R+0xd0: the 60 s count, tested before the zone and the touch.
  c.sixtySeconds += 1;
  if (c.sixtySeconds > rate * kCountSeconds) return blow();

  // R+0xd8: a count running the other way from the side in possession is cleared, and the zone
  // is not tested on that tick. PES tests the signed count, so a count running the other way
  // never passes +4 s.
  bool zoneTested = true;
  if (c.zoneTicks != 0 && (c.zoneTicks > 0 ? 1 : -1) != in.zoneSign) {
    c.zoneTicks = 0;
    zoneTested = false;
  }
  if (zoneTested && in.zoneSign * in.ballX_m < in.halfLength_m / kZoneDivisor) {
    // ponytail: the zone's 0x255 sub-branch (FUN_14050B9B0 lines 88-119) is not ported: its team
    // flag and MI+0x11a0 have no GF source, so the zone count stands alone.
    c.zoneTicks += in.zoneSign;
    if (c.zoneTicks > rate * kZoneSeconds) return blow();
  }

  if (in.newest.kind == 0) {
    // The kind-0 ball-state branch (lines 140-232).
    if (in.second.restartKind == kRestartKindOne) return blow();
    // ponytail: the ball-speed and zone-crossing tests (lines 145-200: B+0x2954, MS+0x24844 and
    // MS+0x2489c) are not ported: they read match state with no GF source.
    if (OutOfZonePattern(in)) return blow();
  } else if (in.newest.flag == kHolderFlag) {
    // The holder-flagged touch timeouts (lines 233-252).
    if (in.newest.kind == kKindThree) return blow();
    // ponytail: kind 2's x test on payload+8 (lines 239-240) is not ported: that float is written
    // by the kind 1/2 tail (FUN_140a17e10), which the store does not hold.
    if (in.newest.kind == kKindTwo && c.sixtySeconds > rate * kTouchFiveSeconds) return blow();
    if (in.newest.kind == kKindFour && c.sixtySeconds > rate * kTouchTenSeconds) return blow();
  }

  // The final block's gate (lines 253-260): a ball in the attacking third and inside the
  // penalty-area half-width ends the tick with no count.
  if (in.halfLength_m / kZoneDivisor < in.zoneSign * in.ballX_m &&
      in.ballZ_m < in.penaltyHalfWidth_m) {
    return false;
  }
  if (in.newest.kind != 0) return rate * kGroundSeconds < c.thirtySeconds ? blow() : false;

  // The kind-0 ground count, R+0xd4 (lines 262-297). Line 286's kind 1..3 test cannot hold here:
  // FUN_140a3e190(E,0) is the touch this block already read, and that touch is kind 0.
  if (in.carrier) {
    if (in.speed_mps != 0.0f && 0.0f <= in.zoneSign * in.ballX_m &&
        in.ring1Word < static_cast<uint32_t>(rate * kRingSeconds)) {
      c.thirtySeconds = 0;
      return false;
    }
  } else {
    if (in.penaltyLineX_m <= in.ballX_m && in.ballZ_m <= in.penaltyHalfWidth_m) {
      c.thirtySeconds += 1;
    }
    if (kGroundHeight_m <= in.ballHeight_m) {
      c.thirtySeconds = 0;
      return false;
    }
  }
  c.thirtySeconds += 1;
  return rate * kGroundSeconds < c.thirtySeconds ? blow() : false;
}

void ClockDisplay(float totalSeconds, int* minute_db_pitch, int* second) {
  // FUN_140A6BD80: h = total/3600, m = (total/60)%60, and the HUD block's
  // `if minute >= 100: minute = 0`.
  const int total = static_cast<int>(std::floor(totalSeconds));
  int minute = (total / 60) % 60;
  if (minute >= kDisplayMinuteWrap) minute = 0;
  if (minute_db_pitch) *minute_db_pitch = minute;
  if (second) *second = total % 60;
}

double AddedTimeUnits(const PesClockConstants& c, e_AddedTimePhase phase, float elapsedSeconds,
                      int tier) {
  // Report 3.2: the tier bonus only. The 9000.0 / 5400.0 per-second term is the
  // clock's own value at the end of the period, which this engine's clock
  // already holds.
  if (phase == e_AddedTimePhase_2ndHalf) {
    if (tier >= 3 && tier <= 6)
      return static_cast<double>(elapsedSeconds) * c.addedTier_3_6_2nd;
    if (tier >= 1 && tier <= 2)
      return static_cast<double>(elapsedSeconds) * c.addedTier_1_2_2nd;
    return 0.0;
  }
  if (tier >= 1 && tier <= 2) return static_cast<double>(elapsedSeconds) * c.addedTier_1_2_ET;
  if (tier >= 3 && tier <= 6) return static_cast<double>(elapsedSeconds) * c.addedTier_3_6_ET;
  return 0.0;
}

uint32_t AddedTimeTicks(const PesClockConstants& c, e_AddedTimePhase phase, float elapsedSeconds,
                        int tier) {
  return static_cast<uint32_t>(AddedTimeUnits(c, phase, elapsedSeconds, tier) / kClockUnitsPerTick);
}

int AnnouncedAddedMinutes(const PesClockConstants& c, e_AddedTimePhase phase, float elapsedSeconds,
                          int tier) {
  const uint32_t ticks = AddedTimeTicks(c, phase, elapsedSeconds, tier);
  if (ticks == 0) return 0;
  const float minutes = static_cast<float>(ticks) / static_cast<float>(kTicksPerGameMinute);
  const int announced = static_cast<int>(std::ceil(minutes));
  return std::min(announced, kMaxAnnouncedAddedMinutes);  // report 3.1: at most 9
}

PesClock::PesClock(const PesClockConstants& constants, float matchMinutes, PeriodRecord& periodRecord)
    : constants(constants),
      record(periodRecord),
      unitsPerFrame(PesUnitsPerFrame(constants, matchMinutes)),
      // The match's game time per logic frame, the law MatchDurationGameTimeFromRealMilliseconds uses.
      gameSecondsPerFrame(constants.matchSpeedBase / (constants.gameSpeed * matchMinutes)) {}

void PesClock::SetPhase(e_MatchPhase newPhase) {
  phase = newPhase;
  record = PeriodRecord{};
  // ponytail: the break-end writers (2 -> 3, 4 -> 5, 6 -> 7, 8 -> 9) are OPEN (A6Writers 5). GF
  // starts the next period at the phase change, which the whistle makes, so no break lasts a frame.
  record.period = PeriodNumber(newPhase);
  StoreStamp(record, 0.0f);
  addedTimeAnnounced = false;
}

void PesClock::Advance(uint32_t frames) {
  const float n = static_cast<float>(frames);
  elapsedSeconds += gameSecondsPerFrame * n;
  // A break, the shoot-out and full time have no clock: the stamp and F hold.
  if (!PeriodHasClock(record.period)) return;
  if (frozen) {
    // A stoppage holds the stamp and adds F: half a unit a frame.
    record.f += kStoppageUnitsPerFrame * n;
    return;
  }
  StoreStamp(record, PeriodStamp(record) + unitsPerFrame * n);
}

void PesClock::AccrueStoppageLimit(uint32_t frames) {
  if (!frozen || !PeriodHasClock(record.period)) return;
  // FUN_14044CE80 step 4: LIMIT takes the half unit a frame that F takes, clamped at the cap.
  record.limit = std::min(record.limit + kStoppageUnitsPerFrame * static_cast<float>(frames),
                          constants.limitCap);
}

bool PesClock::PastPeriodLength() const {
  return PeriodHasClock(record.period) && TimeUpReached(constants, record.period, 0.0f, PeriodClock());
}

bool PesClock::TimeUpGateOpen() const {
  return PeriodHasClock(record.period) &&
         TimeUpReached(constants, record.period, record.limit, PeriodClock());
}

void PesClock::AnnounceAddedTime(int tier) {
  if (addedTimeAnnounced) return;
  addedTimeAnnounced = true;
  record.limit += static_cast<float>(AddedMinutesUnclamped(tier)) * constants.minuteUnits;
}

// FUN_1404494E0 writes the period at the whistle (A6Writers 5).
void PesClock::EndPeriod(bool scoresLevel) {
  record.period = PeriodAfterWhistle(record.period, scoresLevel);
}

void PesClock::Display(int* minute_db_pitch, int* second) const {
  ClockDisplay(elapsedSeconds, minute_db_pitch, second);
}

e_AddedTimePhase PesClock::AddedPhase() const {
  return (phase == e_MatchPhase_1stExtraTime || phase == e_MatchPhase_2ndExtraTime)
             ? e_AddedTimePhase_ExtraTime
             : e_AddedTimePhase_2ndHalf;
}

uint32_t PesClock::AddedTicks(int tier) const {
  return AddedTimeTicks(constants, AddedPhase(), elapsedSeconds, tier);
}

int PesClock::AddedMinutes(int tier) const {
  // Report 3.1: the board shows at most nine minutes of added time.
  return std::min(AddedMinutesUnclamped(tier), kMaxAnnouncedAddedMinutes);
}

int PesClock::AddedMinutesUnclamped(int tier) const {
  const uint32_t ticks = AddedTicks(tier);
  return static_cast<int>(static_cast<float>(ticks) / static_cast<float>(kTicksPerGameMinute));
}

}  // namespace pes
