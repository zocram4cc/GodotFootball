// PES 2021's match clock: the period record at MI+0x1168..0x1194, its decode,
// the stoppage rule and the time-up gate that ends a period
// (docs/re/pes-re-clock-live.md section 8). Every constant is read from the
// user's own PES2021.exe through pes::PeImage::ReadVa; nothing is transcribed.
//
// The record: period (+0x1168), play state (+0x116c), LIMIT (+0x1170), F
// (+0x1174), an FNV-1a 64 checksum (+0x1178), a key (+0x1180), a ring index
// (+0x1184) and four ring slots (+0x1188..+0x1194). The current slot is
// ring[idx & 3]. FUN_140A3C070 decodes it: byte i of the slot is L_i, K_i is byte
// i of (key*3 + 4i), and the decoded byte is ((L_i + K_i) mod 256) XOR
// ((K_i*63) mod 256). The four decoded bytes are the stamp as an f32, the
// checksum is FNV-1a 64 over them, and PES traps when the checksum fails.
// FUN_140A3C050 adds F to the stamp: that is the period clock.
//
// Units: 60 per game-second, so the gate's 162000 is 45 game-minutes. The clock counts
// logic frames: in play the stamp advances PesUnitsPerFrame a frame, 90 * E60 * E5C * (60 / 54)
// / E58 (A6Writers 6): 10 at E58 = 10, the live match. In stoppage (play state 2 or 3) the
// stamp is frozen and F rises 0.5 per frame.

#ifndef _HPP_ONTHEPITCH_PESCLOCK
#define _HPP_ONTHEPITCH_PESCLOCK

#include <cstdint>
#include <optional>
#include <string>

#include "gametypes.hpp"
#include "pes/exe.hpp"

namespace pes {

// docs/re/pes-re-clock-live.md 8.1 and section 2 of pes-re-tactics-clock.md:
// the image addresses the clock reads (.data and .rdata).
constexpr uint64_t GAME_SPEED_VA = 0x14351ae54;           // .data float, 54.0
constexpr uint64_t ADDED_TIER_3_6_2ND_VA = 0x142632a68;   // .rdata 7200.0
constexpr uint64_t ADDED_TIER_1_2_2ND_VA = 0x142598590;   // .rdata 3600.0
constexpr uint64_t ADDED_TIER_1_2_ET_VA = 0x142632a60;    // .rdata 1800.0
constexpr uint64_t ADDED_TIER_3_6_ET_VA = 0x142598590;    // .rdata 3600.0 (the same float)
constexpr uint64_t TIME_UP_BASE_VA = 0x142647c68;         // .rdata 162000.0 (periods 1 and 3)
constexpr uint64_t TIME_UP_FLAG_VA = 0x142647c64;         // .rdata 54000.0 (periods 5 and 7)
constexpr uint64_t MINUTE_UNITS_VA = 0x142598590;         // .rdata 3600.0 (units per minute)
constexpr uint64_t LIMIT_CAP_VA = 0x142647c60;           // .rdata 21600.0 (LIMIT's clamp)
constexpr uint64_t SECOND_UNITS_VA = 0x14259a764;         // .rdata 60.0 (seconds per minute, units per second)

// Report 2.2: the HUD wraps the displayed minute at 100.
constexpr int kDisplayMinuteWrap = 100;
// Report 3.1: the HUD shows at most nine minutes of added time.
constexpr int kMaxAnnouncedAddedMinutes = 9;
// The added-time formula's own clock: 960 ticks per game-minute and 1000 units
// per tick (report 2.2 and 3.2), which the board's whole minutes come from.
constexpr uint32_t kTicksPerGameMinute = 960;
constexpr float kClockUnitsPerTick = 1000.0f;

// Stoppage: F takes this much per logic frame while the state is stopped (live
// OBS: 19.5 over 39 frames in state 2).
constexpr float kStoppageUnitsPerFrame = 0.5f;

struct PesClockConstants {
  float gameSpeed = 0.0f;          // 54.0: logic frames per game-second (.data)
  float addedTier_3_6_2nd = 0.0f;  // 7200.0, perf tiers 3..6, 2nd half
  float addedTier_1_2_2nd = 0.0f;  // 3600.0, perf tiers 1..2, 2nd half
  float addedTier_1_2_ET = 0.0f;   // 1800.0, perf tiers 1..2, extra time
  float addedTier_3_6_ET = 0.0f;   // 3600.0, perf tiers 3..6, extra time
  float timeUpBase = 0.0f;         // 162000.0: the length of periods 1 and 3
  float timeUpFlag = 0.0f;         // 54000.0: the length of periods 5 and 7
  float minuteUnits = 0.0f;        // 3600.0: LIMIT's units per whole minute
  float secondsUnit = 0.0f;        // 60.0: minutes to seconds, and seconds to units
  float limitCap = 0.0f;          // 21600.0: LIMIT's clamp in the stoppage writer
  float matchSpeedBase = 0.0f;     // 90.0: the match-speed numerator (.rdata 0x14259be5c)
};

bool ReadFloat(const PeImage& exe, uint64_t va, float* out);

// Reads every clock constant out of the install's PES2021.exe.
std::optional<PesClockConstants> LoadPesClockConstants(const PeImage& exe, std::string* error);

// The period number PES keeps at MI+0x1168 for a GF phase. The breaks (2, 4, 6, 8)
// are the walk-off set pieces in GF, not phases of their own.
uint32_t PeriodNumber(e_MatchPhase phase);

// Periods that run a clock; PES's judge skips the time-up in the others (2, 6, 9, 10).
bool PeriodHasClock(uint32_t period);

// The period a clock period hands to when its whistle blows (FUN_1404494E0, A6Writers 5): 1 to
// 2, 3 to 4 when level and to 10 otherwise, 5 to 6, 7 to 8 when level and to 10 otherwise. Any
// other period is unchanged.
uint32_t PeriodAfterWhistle(uint32_t period, bool scoresLevel);
// The stamp's units per logic frame: PES's match speed, 90 * E60 * E5C * (60 / 54) / E58
// (FUN_140A6C050, A6Writers 6), with E58 the match-time setting in minutes.
float PesUnitsPerFrame(const PesClockConstants& c, float matchMinutes);

// The period record, MI+0x1168..0x1194.
struct PeriodRecord {
  uint32_t period = 0;     // +0x1168
  uint32_t playState = 0;  // +0x116C: 1 in play, 2 or 3 stopped (OBS)
  float limit = 0.0f;      // +0x1170: LIMIT, added minutes times minuteUnits
  float f = 0.0f;          // +0x1174: F, the stoppage accrual
  uint64_t checksum = 0;   // +0x1178: FNV-1a 64 over the decoded stamp bytes
  uint32_t key = 0;        // +0x1180
  uint32_t ringIdx = 0;    // +0x1184
  uint32_t ring[4] = {};   // +0x1188..+0x1194
};

// FUN_140A3C070: the stamp the current slot decodes to. Throws when the checksum
// does not match, where PES traps (FUN_1415A1714).
float PeriodStamp(const PeriodRecord& r);
// FUN_140A3C050: the stamp plus F.
float PeriodClockUnits(const PeriodRecord& r);
// The inverse of PeriodStamp: writes the stamp into the next slot and its checksum.
void StoreStamp(PeriodRecord& r, float stamp);

// FUN_14050B9B0's gate, FUN_140A6BEF0: the period's length plus floor(LIMIT /
// minuteUnits) whole minutes, as the time-up threshold. The gate opens when the
// clock reaches it.
bool TimeUpReached(const PesClockConstants& c, uint32_t period, float limit, float clock);

// --- the post-gate judge (FUN_14050B9B0, the part after the gate opens) ---
//
// With the gate open, PES's judge runs on every tick, in play or not (FUN_14050C3E0 skips
// only periods 2, 6, 9 and 10). It counts the ticks the gate has been open (R+0xd0, the 60 s
// count), the ticks the ball has spent in the zone of the side in possession (R+0xd8), and,
// on a kind-0 touch, the ticks the ball has been on the ground with nobody holding it (R+0xd4,
// the 30 s count). A blow clears every count (FUN_14050C3E0). Not ported, each marked where it
// stands: the penalty-taker hold (G+0x2920), the R+8 flag (no writer traced), the zone's 0x255
// sub-branch, the ball-speed and zone-crossing tests of a kind-0 touch (FUN_14050B9B0 lines
// 141-200: B+0x2954 and MS+0x24844/0x2489c), and kind 2's x test on payload+8.
struct TimeUpCounters {
  int sixtySeconds = 0;   // R+0xd0: ticks with the gate open, cleared on a blow
  int zoneTicks = 0;      // R+0xd8: the zone count, signed by the side in possession
  int thirtySeconds = 0;  // R+0xd4: ground-ball ticks of a kind-0 touch
};

// The action kind of an empty touch record (the 0x37 sentinel of FUN_140a3e190).
constexpr uint32_t kTimeUpNoKind = 0x37;
// The player id of no player (FUN_140a62ee0's 0xff).
constexpr uint32_t kTimeUpNoPlayer = 0xff;

// The judge's view of one event record: payload+0 (kind), +0x10 (frame), +0x1c (restart
// kind), +0x20 (holder flag) and the actor's player id.
struct TimeUpEvent {
  uint32_t kind = kTimeUpNoKind;
  int frame = 0;
  uint32_t restartKind = 0;
  uint32_t flag = 0;
  uint32_t player = kTimeUpNoPlayer;
};

struct TimeUpInput {
  int zoneSign = 1;                 // the attack sign of the side in possession: +1 or -1
  bool carrier = false;             // a player has the ball (FUN_140a627e0 names one)
  float ballX_m = 0.0f;             // the ball's x along the pitch (B+0x88), PES metres
  float ballHeight_m = 0.0f;        // its height, PES y (B+0x8c)
  float ballZ_m = 0.0f;             // its z across the pitch (B+0x90)
  float speed_mps = 0.0f;           // |velocity| across the pitch, m/s (B+0x2948, B+0x2950)
  float halfLength_m = 0.0f;        // F[0]: the zone line is a third of it
  float penaltyLineX_m = 0.0f;      // F[6]: the penalty-area line
  float penaltyHalfWidth_m = 0.0f;  // F[7]: the penalty-area half-width
  int frame = 0;                    // MI+0x1198: the frame the judge runs on
  uint32_t ring1Word = 0;           // FUN_140a3e840: ring 1's newest payload+0, 0 when empty
  TimeUpEvent newest;               // R+0x3c0: the newest touch copy (FUN_140a3e190(E,0))
  TimeUpEvent second;               // FUN_140a3e190(E,1): the touch before it
};

// One judge tick with the gate open, at `rate` logic frames a game-second. Returns true when
// the judge blows; every count is then cleared.
bool TimeUpWhistle(TimeUpCounters& counters, const TimeUpInput& in, int rate);

// --- the added-time formula (report 3.2) ---

// Which of the two paths the added-time formula runs: PES evaluates it only at
// the end of the second half and of extra time.
enum e_AddedTimePhase { e_AddedTimePhase_2ndHalf, e_AddedTimePhase_ExtraTime };

// Added time in clock units: the tier bonus only, `elapsedSeconds * rate`. The
// 9000.0 / 5400.0 per-second term is the clock's own value at the end of the
// period, so this engine's added time is the bonus over it. `tier` is the
// on-pitch performer's perf tier (PES's TeamSlice + 0x4C00, which has no GF
// equivalent); 0 means no tier is known.
double AddedTimeUnits(const PesClockConstants& c, e_AddedTimePhase phase, float elapsedSeconds,
                      int tier);
uint32_t AddedTimeTicks(const PesClockConstants& c, e_AddedTimePhase phase, float elapsedSeconds,
                        int tier);
// The fourth official's board: whole game-minutes of added time, never zero
// once any time at all was lost, capped at the nine the HUD shows (report 3.1).
int AnnouncedAddedMinutes(const PesClockConstants& c, e_AddedTimePhase phase, float elapsedSeconds,
                          int tier);

// --- the display (report 2.6) ---

// Minute and second from a seconds count, as FUN_140A6BD80 and the HUD block.
void ClockDisplay(float totalSeconds, int* minute_db_pitch, int* second);

// The match clock: one period record per period, advanced by the logic tick.
class PesClock {
 public:
  // matchMinutes is the match-time setting, E58: the menu's duration, 5 to 90 minutes.
  explicit PesClock(const PesClockConstants& constants, float matchMinutes);

  // Starts the period's own record: the PES period number, stamp 0, F 0, no LIMIT.
  void SetPhase(e_MatchPhase phase);
  e_MatchPhase Phase() const { return phase; }

  // `frames` logic frames of play: the stamp advances PesUnitsPerFrame a frame. While frozen (a
  // stoppage) the stamp holds and F accrues half a unit a frame. A break has no clock.
  void Advance(uint32_t frames);
  // FUN_14044CE80 step 4: while stopped in a clock period, LIMIT takes the same half unit
  // per frame as F, clamped at limitCap. Called every tick; it does nothing in play.
  void AccrueStoppageLimit(uint32_t frames);
  // Freezes the clock for the duration of a stoppage (report 3.3).
  void SetFrozen(bool frozen) { this->frozen = frozen; }
  bool IsFrozen() const { return frozen; }

  const PeriodRecord& Record() const { return record; }
  float PeriodClock() const { return PeriodClockUnits(record); }
  // The clock has reached the period's length, with no added time counted.
  bool PastPeriodLength() const;
  // The time-up gate: the length plus the whole minutes LIMIT holds.
  bool TimeUpGateOpen() const;
  // Adds the added-time formula's minutes to LIMIT, once per period (report 3.2).
  void AnnounceAddedTime(int tier);
  // The period writer at a whistle (FUN_1404494E0): the record moves to the break or to full time,
  // as PeriodAfterWhistle names.
  void EndPeriod(bool scoresLevel);
  bool AddedTimeAnnounced() const { return addedTimeAnnounced; }

  // GF's display clock: the game-seconds the match has run, stoppages included.
  // The HUD's own source is not decoded (docs/re/pes-re-clock-live.md 8.2).
  float ElapsedSeconds() const { return elapsedSeconds; }
  void Display(int* minute_db_pitch, int* second) const;

  // The added-time formula's whole minutes for the running phase, and the board's
  // clamp to the nine the HUD shows.
  uint32_t AddedTicks(int tier) const;
  int AddedMinutes(int tier) const;
  int AddedMinutesUnclamped(int tier) const;

 private:
  e_AddedTimePhase AddedPhase() const;
  PesClockConstants constants;
  e_MatchPhase phase = e_MatchPhase_PreMatch;
  PeriodRecord record;
  bool frozen = false;
  bool addedTimeAnnounced = false;
  float elapsedSeconds = 0.0f;
};

}  // namespace pes

#endif
