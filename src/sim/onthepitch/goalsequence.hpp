// Timing for what happens between a goal and the kickoff that follows it.
//
// The order is goal, celebration, replay, kickoff. Two different places have to
// agree for that to hold: Match fires the replay once the celebration has run,
// and Referee schedules the kickoff restart - which calls ResetSituation and
// clears the goal state, and with it the very timer the replay trigger is
// waiting on. When the celebration was lengthened without moving the restart,
// the goal state was cleared first and the replay stopped firing entirely.
//
// Both read their timings from here so they cannot drift apart again, and
// tests/onthepitch/goal_sequence_test.cpp asserts the order they imply.

#ifndef _HPP_ONTHEPITCH_GOALSEQUENCE
#define _HPP_ONTHEPITCH_GOALSEQUENCE

namespace GoalSequence {

// How long the goal celebration runs when nothing else decides it: the goal scene PES plays. ponytail: PES's scene
// length is the sum of its cut lengths (pes-re-cutscenes.md 3.3: a goal cut's maxTime, 700 at 60 Hz = 11.67 s;
// pes-re-foul-choreography.md 9.2: scene length = sum of cut maxTime plus 0.5 s step fades). The engine does not read
// that table yet, so the scene stands in as a constant. 12.5 s with the 2.33 s of pes::RunGoalProgramme puts goal ->
// ready at 801 frames, the centre of PES's live 757-843 window (docs/re/pes-re-A5Judge.md 9.3a).
constexpr unsigned long kMinCelebration_ms = 12500;

// Kept as the plain default for callers with no clip to hand.
constexpr unsigned long kCelebration_ms = kMinCelebration_ms;

// The longest celebration the schedule will run: the montage plus room for a
// clip that outlasts the shot it plays under (the longest single celebration
// clip is 10.0 s, and one may chain an intro into a loop). Everything below
// depends on this, because the recorded buffer has to reach back past it.
constexpr unsigned long kLongestCelebration_ms = 29000;

// How far before the goal the replay opens, so it shows the build-up rather
// than the celebration it just interrupted.
constexpr unsigned long kReplayLeadIn_ms = 10000;

// The recorded window the replay is cut from. It has to cover the longest
// celebration plus the lead-in, or a replay fired after the celebration can no
// longer reach the goal and plays back the celebration instead of the action.
constexpr unsigned long kReplayBuffer_ms = kLongestCelebration_ms + kReplayLeadIn_ms;

// PES's goal sequence is a MONTAGE of SHORT cuts, not held shots. Read frame
// by frame off VGL 26 day 12 at 5:09:59 (docs/VGL26_DAY12_REFERENCE.md §4):
// the goal goes in on the live camera, then six cuts of two to four seconds
// each - the scorer from behind, tracking; a front medium as he turns; a
// two-shot as the first teammate arrives (the scorer's name ribbon rises);
// the mob close; the mob wide; and two cuts to the STANDS - and only then the
// replay. Goal to restart is about forty seconds. An earlier reading of an
// older PES (youtu.be/ns5C3zpD6Ig) had three long shots and a sixty-second
// sequence; the 2021 broadcast is the reference and it cuts fast.
//
// Each cut is filmed by its own goal camtrack (Match picks per shot by the
// distance PES authored it at) or, for the two stand shots, by this ground's
// end/audience pool. The length is the SHOTS, never a timer held over a
// finished performance - a cast actor past his clip is released to the
// animation machinery and jogs back (Match::UpdateCutsceneChoreo).
enum class Shot { Behind = 0, Front, TwoShot, MobClose, MobWide, Crowd, CrowdTwo };
constexpr int kShotCount = 7;
// The cut lengths sum to kMinCelebration_ms (static_assert in goalsequence.cpp): the reference's seven cuts re-timed to the
// scene. ShotAt has no live caller yet, and nothing here is checked against a captured frame.
constexpr unsigned long kShotLength_ms[kShotCount] = {
    1500,  // Behind: scorer from behind, low, arms up, tracking his run
    1500,  // Front: front medium, low, ad boards behind him
    2500,  // TwoShot: from behind as the first teammate arrives; the name ribbon
    1500,  // MobClose: three heads
    2000,  // MobWide: four players, low angle
    2000,  // Crowd: the stands react (audience pool)
    1500,  // CrowdTwo: a second stand
};
// How far out PES authored the camera for each of the player shots, in
// metres from the scorer - what Match matches a goal camtrack against. The
// stand shots take the audience pool instead and this reads 0.
constexpr float kShotDistance_m[kShotCount] = {5.0f, 6.0f, 8.0f, 4.0f, 10.0f, 0.0f, 0.0f};
// Whether the shot is filmed by a goal camtrack staged on the scorer (true) or
// by the ground's own stand camerawork (false).
constexpr bool kShotOnScorer[kShotCount] = {true, true, true, true, true, false, false};

// The scorer's name ribbon comes up on the two-shot, about four seconds after
// the goal, and stays through the mob.
// The two lower-thirds, timed off the reference (implying.fun "Summer 2026
// Day 1" at 32:09, blue-mask over the clip): the score bug comes up 1.75 s
// after the ball goes in and holds 2.25 s, then after a 0.75 s gap the
// scorer's plate comes up and holds while the celebration is still on air.
constexpr unsigned long kScoreBugIn_ms = 1750;
constexpr unsigned long kScoreBugOut_ms = 4000;
constexpr unsigned long kScorerBugIn_ms = 4750;
constexpr unsigned long kRibbonIn_ms = kScorerBugIn_ms;

// The replay PES cuts to shows the BUILD-UP and the finish, from two angles -
// and then hands back. Ours played the tape all the way to the present, so a
// goal replay replayed the celebration that had just been on screen. Each
// angle plays a window ending at the goal. Times are WALL time: the wide
// angle plays 9000 ms of tape at full speed, the close angle 3500 ms of tape
// at half speed - both 9000/7000 ms on screen, and the sum below is the replay's wall time.
constexpr unsigned long kReplayWideAngle_ms = 5500;
constexpr unsigned long kReplaySideAngle_ms = 5000;
constexpr unsigned long kReplayCloseAngle_ms = 5000;  // 2500 ms of tape at half speed
constexpr unsigned long kReplayPlayback_ms =
    kReplayWideAngle_ms + kReplaySideAngle_ms + kReplayCloseAngle_ms;

// The whole celebration: the seven cuts back to back. A clip longer than the
// montage extends the last player shot rather than being cut off.
unsigned long CelebrationLength_ms(unsigned long animLength_ms);

// Which shot is on air `celebration_ms` into a celebration of that length, and
// where the current shot began (so the camera can cut rather than drift).
Shot ShotAt(unsigned long celebration_ms, unsigned long celebrationLength_ms);
unsigned long ShotStartedAt_ms(unsigned long celebration_ms,
                               unsigned long celebrationLength_ms);

// When the replay should fire for a goal scored at `goalTime_ms`.
// `cutsceneEnd_ms` is the end of a goal cutscene if one is playing, 0 if not;
// a cutscene running past the celebration wins, a shorter one does not cut it short.
// `celebrationLength_ms` is what CelebrationLength_ms returned for the clip on screen.
unsigned long ReplayFiresAt_ms(unsigned long goalTime_ms, unsigned long cutsceneEnd_ms = 0,
                               unsigned long celebrationLength_ms = kCelebration_ms);

// How far back the replay should start, given how long the celebration
// actually ran (the elapsed goal timer at the moment it fires).
unsigned long ReplayStartOffset_ms(unsigned long celebrationElapsed_ms);

// Do the constants above still describe goal, celebration, replay, kickoff -
// with the replay inside the recorded buffer?
bool ScheduleIsConsistent();

}  // namespace GoalSequence

#endif
