#include "pes/cutscene_timeline.hpp"

#include <utility>

namespace pes {
namespace {

// ponytail: the manager's not-ready calls before a step starts. PES's step machine 0x141F59380 asks `0x98` (0x141F5A0B0, the
// screen-transition effect object 0x141F15A50 through the slot objects at 0x1437F5CA8) before it starts a step; the object
// finishes after rendering passes this port does not run, and the manager counts the refusals in +0x254 (+1.0 per call,
// 0x141F59380 case 6). Measured on the clean live goal of 30-10-26 (poll rows with the frame counter read twice): +0x254
// read 4.0 at both step switches, and step 0 started 6 frames after DemoInfo state 4, i.e. the go frame (DemoInfo state 8)
// is refused once. A render-pass-free GF needs those frames to look like PES's.
// Removed when the transition objects (0x141F15FC0 and the state objects' vslot 0x28 at 0x141F17B70..) are translated.
constexpr unsigned kFirstStepNotReady = 1;
constexpr unsigned kTransitionNotReady = 4;
// The scene's last step is asked the same way before the machine moves to teardown: phase 7 came 5 frames after the clock's
// last update on both clean logs that cover it (live goal of 30-10-26, and g1/g2 within the poll's slack): 3 refusals.
constexpr unsigned kLastStepNotReady = 3;
// ponytail: DemoInfo state 0xB (0x14047C830) waits until cue 1 (FUN_140A67AA0(1), the scene's sound, started at state 8) is no
// longer pending; the loader thread decides when, so the frames are wall dependent in PES. Measured: DemoInfo state 0 came
// 9 frames after the clock's last update on the clean live goal (state 0xB visible for the last 2 of them); removed with the
// cue objects (0x1436F4ED0 table, 0x140A67AA0).
constexpr unsigned kEndCueFrames = 2;

}  // namespace

float CutLength(float minTicks, float maxTicks, int draw, float ninetyNine) {
  if (minTicks <= 0.0f || maxTicks <= 0.0f) return maxTicks;
  return (float(draw) * (maxTicks - minTicks)) / ninetyNine + minTicks;
}

uint32_t GoalRunTimerTicks(uint32_t scene, float fps, float scale) {
  const uint32_t seconds = scene - 0x3013Du < 3 ? 10 : 4;  // 0x14047A720..0x14047A72B; 0x14047A7F7
  return uint32_t(float(uint32_t(fps) * seconds) / scale);  // 0x14047A7FD CVTSI2SS, 0x14047A802 DIVSS, 0x14047A807 CVTTSS2SI
}

CutsceneTimeline::CutsceneTimeline(const TimelineConfig& config, std::vector<TimelineStep> steps)
    : config_(config), steps_(std::move(steps)) {
  state_.stepPhase = 4;  // the manager's loading phases (0x141F57690, 0x141F5A760, 0x141F5B810) are done long before the gate
}

unsigned CutsceneTimeline::Refusals(size_t step, unsigned fallback) const {
  return steps_[step].notReadyBefore == TimelineStep::kDefaultNotReady ? fallback : steps_[step].notReadyBefore;
}

void CutsceneTimeline::StartStep(size_t index) {
  const TimelineStep& s = steps_[index];
  state_.step = int(index);
  state_.stepPhase = 5;
  state_.playerState = 4;
  state_.clock = 0;
  length_ = s.lengthTicks;
  start_ = s.startTicks;
  speed_ = s.speed;
  playing_ = false;
  notReady_ = 0;
  active_ = true;
}

// Player update 0x141F2F6B0 (states 4 and 5; 6 returns before the switch body). The finished test reads the clock the previous
// update left, so the update that clamps the clock to the length is followed by one that sets state 6.
void CutsceneTimeline::PlayerUpdate() {
  secondsThisTick_ = 0;
  if (state_.playerState == 4) state_.playerState = 5;  // cut start 0x141F2DD80 (length, start and speed were set by StartStep)
  if (state_.playerState != 5) return;
  float& clock = state_.clock;
  if (speed_ > 0.0f && length_ > 0.0f && length_ <= clock) state_.playerState = 6;
  float next = (config_.sixty / config_.fps) * speed_ + clock;
  if (clock < start_) next = start_;
  if (length_ > 0.0f && length_ < next) next = length_;
  const float delta = next > clock ? next - clock : 0.0f;
  secondsThisTick_ = delta / config_.sixty;
  // INF: +0xADE (the clock advances only with it, or while the clock is before the start) is set after the first update; the
  // writer is not in the decoded closure, the captured clocks (0 after the first update, then +1.1111 per update) say so.
  if (playing_ || clock < start_) clock += delta;
  start_ = 0;
  playing_ = true;
}

// Manager update 0x141F59050: the step machine 0x141F59380, then the player update in its tail, then the seconds accumulate.
void CutsceneTimeline::Manager() {
  TimelineState& s = state_;
  switch (s.stepPhase) {
    case 4:  // waits for the go flag (+0x54), then for the transition object
      if (!go_) break;
      if (notReady_ < Refusals(0, kFirstStepNotReady)) {
        ++notReady_;
        break;
      }
      StartStep(0);
      break;
    case 5: s.stepPhase = 6; [[fallthrough]];  // queues the next player, falls into 6
    case 6:
      if (s.playerState != 6) break;  // 0x141F572F0: over when the player is finished
      if (size_t(s.step) + 1 < steps_.size()) {
        if (notReady_ < Refusals(size_t(s.step) + 1, kTransitionNotReady)) ++notReady_;  // 0x141F5B6E0 refuses: not ready
        else StartStep(size_t(s.step) + 1);                 // swaps the players, scene start 0x141F5CD00
      } else if (notReady_ < kLastStepNotReady) {
        ++notReady_;  // the scene's end transition, asked the same way
      } else {
        s.stepPhase = 7;  // no next player: the machine moves to teardown
      }
      break;
    case 7:  // teardown, returns 1: the manager phase +0x1D4 becomes 4 and the DemoInfo sees it next frame
      teardownDone_ = true;
      return;
  }
  const int before = s.playerState;
  if (active_) PlayerUpdate();
  if (before == 4 && active_ && s.playerState >= 5) s.stepStarted = s.step;
  s.totalSeconds += secondsThisTick_;
  secondsThisTick_ = 0;
}

void CutsceneTimeline::Tick() {
  TimelineState& s = state_;
  s.anchorWrite = false;
  s.stepStarted = -1;
  s.ended = false;
  if (s.demoState == 0) return;
  ++s.tick;
  switch (s.demoState) {
    case 2:  // waiting for +0x1D4 (the pre-scene gate 0x140447290) and the manager's loads
      if (s.tick >= config_.gateTicks) s.demoState = 4;
      break;
    case 4: s.demoState = 5; break;                          // the cue is ready (stand-in: see TimelineConfig)
    case 5: s.anchorWrite = true; s.demoState = 6; break;    // 0x14047D280
    case 6: s.demoState = 7; break;
    case 7: s.demoState = 8; break;
    case 8:                                                  // go flag, one manager update
      go_ = true;
      s.demoState = 10;
      Manager();
      break;
    case 10:
      if (teardownDone_) {
        s.demoState = 11;
        endWait_ = 0;
        [[fallthrough]];
      } else {
        Manager();
        break;
      }
    case 11:
      if (endWait_++ >= kEndCueFrames) {
        s.demoState = 0;
        s.ended = true;
      }
      break;
  }
}

}  // namespace pes
