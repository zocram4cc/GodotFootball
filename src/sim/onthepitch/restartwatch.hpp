#pragma once

// Watches a running match, tick by tick, for stoppages that do not restart in time.
//
// A stoppage is any tick on which play is off or a set piece is armed. The match
// clock only moves on `IsInPlay() && !IsInSetPiece()` (match.cpp, time), so a
// restart that never comes freezes game time and the match never reaches full
// time. The penalty shootout runs its own sequence and is not measured here.

#include "gametypes.hpp"

class Match;
namespace pes {
struct StoppageConstants;
struct SetPieceTakerConstants;
}  // namespace pes

// The longest a measured stoppage may last before its restart is taken, in sim
// ticks: the largest of the whistle, goal and period-break bounds.
//   - whistle: the foul bracket with a player down (FoulSequence::RestartTakeAt_ms),
//     the longest hold the referee arms, the wipe, the UI window, the fade-in and the
//     restart tail, all from PES2021.exe (pesstoppage.hpp); plus the area clearing
//     wait, bounded by SetPieceLaws::kMaxWait_ms (a GF bound); plus the longest taker
//     gate of corner, free kick and penalty, from the exe (pessetpiecetaker.hpp).
//   - goal: the kick-off at the end of the longest celebration the sequence allows
//     (pes::RunGoalProgramme from GoalSequence::kLongestCelebration_ms, plus the kick-off's hold; a GF bound).
//   - period break: the half-time kick-off (SetPieceLaws::HalfTimeTakeAt_ms, GF).
// Plus one tick per gate of slack for the referee's once-per-tick checks.
unsigned long RestartLimitTicks(const pes::StoppageConstants& stoppage,
                                const pes::SetPieceTakerConstants& taker);

class RestartWatch {
 public:
  explicit RestartWatch(unsigned long limitTicks) : limitTicks_(limitTicks) {}

  // Call once after every Match::Process().
  void Observe(const Match& match);

  // True once a measured stoppage has run past the limit. It stays true.
  bool Stalled() const { return stalled_; }
  // Ticks observed so far.
  unsigned long Ticks() const { return ticks_; }
  // The longest measured stoppage so far, including one still open.
  unsigned long LongestTicks() const { return longestTicks_; }
  unsigned long LimitTicks() const { return limitTicks_; }
  // Measured stoppages that have ended in a restart.
  unsigned long MeasuredStoppages() const { return measuredStoppages_; }
  // Maximal runs of ticks on which the match stood idle and the stalemate
  // recovery forced the ball to a player.
  unsigned long IdleEpisodes() const { return idleEpisodes_; }
  // The idle episodes that began while play was live (the ball lay still in open play).
  // The rest began in a stoppage, where the restart is still being held.
  unsigned long IdleStallEpisodes() const { return idleStallEpisodes_; }

 private:
  unsigned long limitTicks_;
  unsigned long ticks_ = 0;
  bool inStoppage_ = false;
  bool measured_ = false;
  unsigned long stoppageStart_ = 0;
  bool stalled_ = false;
  unsigned long longestTicks_ = 0;
  unsigned long measuredStoppages_ = 0;
  unsigned long lastStalemateCount_ = 0;
  bool idleLastTick_ = false;
  unsigned long idleEpisodes_ = 0;
  unsigned long idleStallEpisodes_ = 0;
};
