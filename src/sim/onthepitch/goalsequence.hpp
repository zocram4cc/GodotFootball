// Timing for what happens between a goal and the kickoff that follows it.
//
// The order is goal, celebration, replay, kickoff. Two different places have to agree for that to hold:
// Match fires the replay once the celebration has run, and Referee schedules the kickoff restart - which
// calls ResetSituation and clears the goal state, and with it the very timer the replay trigger is waiting
// on. So the restart is scheduled from the end of the celebration (ReplayFiresAt_ms), never before it.
//
// The celebration is PES's own: a goal plays a primary scene drawn from the install's FixDemo pool
// (PickCelebration), then a hug drawn from the same generator (PickHug), and the length is their sum
// (pes::fixdemo). The replay itself is not played by GF yet.
// tests/sim/pes_goal_scene_test.cpp and tests/sim/pes_restart_delay_test.cpp check the order.

#ifndef _HPP_ONTHEPITCH_GOALSEQUENCE
#define _HPP_ONTHEPITCH_GOALSEQUENCE

#include "pes/fixdemo.hpp"
#include "pesrandom.hpp"

namespace GoalSequence {

// The goal level the primary celebration is filtered by (FUN_140477400's goal-ctx[0]; pes-re-scene-choice.md
// 2a). ponytail: PES's level source is not decoded (INF), so every goal is level 0 until it is.
constexpr int kGoalLevel = 0;

// Offset from the match seed for the goal generator. PES seeds it from the match's kick-off triple
// (pes-re-scene-choice.md 2a), which GF does not carry. ponytail: replace with the triple when it is ported.
constexpr unsigned int kGoalCelebrationSalt = 0x60a15ceu;

// A scene length in seconds, in milliseconds.
unsigned long CelebrationMs(float length_s);

// The primary celebration PES plays for a goal (FUN_140477400): a uniform draw over the generic pool's scenes
// of `level`, drawn again while it repeats `previousIndex`. After ten repeats it is the fallback, scene 0.
pes::fixdemo::GoalScene PickCelebration(const pes::fixdemo::GoalScenes& scenes, int level,
                                        int previousIndex, pes::Random& rng);

// The hug that follows the celebration (FUN_140474b20): a uniform draw over the scenes with bit 2 or 3 that
// carry the level bits 12 or 13. Index -1 when that pool is empty.
pes::fixdemo::GoalScene PickHug(const pes::fixdemo::GoalScenes& scenes, pes::Random& rng);

// The longest goal the pool can produce, in ms: the longest primary celebration plus the longest hug. The
// restart watch's bound for a goal.
unsigned long LongestCelebration_ms(const pes::fixdemo::GoalScenes& scenes);

// When the referee should prepare the restart for a goal scored at `goalTime_ms`: once the celebration has
// run. `cutsceneEnd_ms` is the end of a goal cutscene if one is playing, 0 if not; a longer one wins.
unsigned long ReplayFiresAt_ms(unsigned long goalTime_ms, unsigned long cutsceneEnd_ms,
                               unsigned long celebrationLength_ms);

// The score bug and the scorer's name ribbon, timed off the reference broadcast (implying.fun "Summer
// 2026 Day 1" at 32:09, blue-mask over the clip): the score bug comes up 1.75 s after the ball goes in and
// holds 2.25 s, then after a 0.75 s gap the scorer's plate comes up and holds while the celebration is on air.
constexpr unsigned long kScoreBugIn_ms = 1750;
constexpr unsigned long kScoreBugOut_ms = 4000;
constexpr unsigned long kScorerBugIn_ms = 4750;
constexpr unsigned long kRibbonIn_ms = kScorerBugIn_ms;

}  // namespace GoalSequence

#endif
