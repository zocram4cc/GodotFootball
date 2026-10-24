#include "restartwatch.hpp"

#include <algorithm>
#include <cmath>

#include "foulsequence.hpp"
#include "goalsequence.hpp"
#include "match.hpp"
#include "pessequencer.hpp"
#include "pessetpiecetaker.hpp"
#include "pesstoppage.hpp"
#include "setpiecelaws.hpp"

namespace {

// The referee tests each taker gate once per tick, so a gate can run one tick
// past its frame count, and the clearing hold can end one tick after its wait.
constexpr unsigned long kGateSlackTicks = 2;

}  // namespace

unsigned long RestartLimitTicks(const pes::StoppageConstants& stoppage,
                                const pes::SetPieceTakerConstants& taker, unsigned long longestCelebration_ms) {
  // Holding table[1] with a player down is the longest whistle stop the referee arms.
  const double bracket_ms = static_cast<double>(FoulSequence::RestartTakeAt_ms(0, stoppage, true, 1));
  const float longestGate_s =
      std::max({taker.cornerGate_s, taker.freeKickGate_s, taker.penaltyGate_s});
  const double whistle_ms = bracket_ms + static_cast<double>(SetPieceLaws::kMaxWait_ms) +
                            static_cast<double>(longestGate_s) * 1000.0;
  // A goal restart is the kick-off at the end of the longest celebration the sequence allows: the goal programme from
  // the celebration's end, then the kick-off's hold (pes::ReleaseFrames).
  const pes::Timing goal = pes::RunGoalProgramme(
      GoalSequence::ReplayFiresAt_ms(0, 0, longestCelebration_ms), stoppage);
  const double goal_ms =
      static_cast<double>(goal.armedAt_ms) + pes::ReleaseFrames(e_SetPiece_KickOff) * Match::kTick_ms;
  // A period break is the half-time walk-off, then the kick-off of the next period.
  const double halfTime_ms = static_cast<double>(SetPieceLaws::HalfTimeTakeAt_ms(0));
  const double longest_ms = std::max({whistle_ms, goal_ms, halfTime_ms});
  return static_cast<unsigned long>(std::ceil(longest_ms / Match::kTick_ms)) + kGateSlackTicks;
}

void RestartWatch::Observe(const Match& match) {
  ticks_++;
  const bool restarted = match.IsInPlay() && !match.IsInSetPiece();
  if (!restarted) {
    if (!inStoppage_) {
      inStoppage_ = true;
      measured_ = true;
      stoppageStart_ = ticks_;
    }
    // The penalty shootout runs its own sequence (penaltyshootout.cpp), not a restart.
    if (match.GetMatchPhase() == e_MatchPhase_Penalties) measured_ = false;
    if (measured_) {
      const unsigned long elapsed = ticks_ - stoppageStart_;
      longestTicks_ = std::max(longestTicks_, elapsed);
      if (elapsed > limitTicks_) stalled_ = true;
    }
  } else if (inStoppage_) {
    if (measured_) measuredStoppages_++;
    inStoppage_ = false;
  }

  const unsigned long stalemates = match.GetStalemateCount();
  const bool idleNow = stalemates > lastStalemateCount_;
  if (idleNow && !idleLastTick_) {
    idleEpisodes_++;
    // An episode that starts with play live is a stall: the ball lay still while the match
    // was running. One that starts in a stoppage is a hold the restart is still waiting out.
    if (match.IsInPlay() && !match.IsInSetPiece()) idleStallEpisodes_++;
  }
  idleLastTick_ = idleNow;
  lastStalemateCount_ = stalemates;
}
