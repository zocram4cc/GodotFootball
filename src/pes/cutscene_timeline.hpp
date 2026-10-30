#pragma once

// PES's FixDemo scene timeline: when a goal or foul scene starts, how long each step plays, how steps hand over, when the
// scene ends. One Tick() is one PES update frame (1/54 s). Decode: docs/ports/pes-port-CutsceneTiming.md and
// docs/ports/pes-port-CutsceneTiming2.md; every rule below names the PES site it mirrors.
//
//   DemoInfo state machine 0x14047C830      states 2 (gate), 4, 5 (anchor write 0x14047D280), 6, 7, 8 (go flag), 10, 0xB
//   manager update 0x141F59050, step machine 0x141F59380 (phase 4 wait go, 5 started, 6 playing, 7 teardown)
//   player update 0x141F2F6B0 (state 4 waiting, 5 playing, 6 finished), cut start 0x141F2DD80 (length, start, speed)
//
// Frame semantics: Tick() number 1 is the frame after the request (the frame DemoInfo exists, tick 0 = its creation).
// State() is the state at the END of the last Tick, the way PES's objects read between two frames.

#include <cstdint>
#include <vector>

namespace pes {

// One step of the manager's step vector, as its player holds it after the cut start (0x141F2DD80 and the scene start
// 0x141F5CD00): the clock is in ticks of 1/`sixty` s.
struct TimelineStep {
  float lengthTicks = 0;  // player+0x64: the first Cut record's length (CutLength below)
  float startTicks = 0;   // player+0x70: the clock jumps here in the first update (record +0x04, node +0x1C8)
  float speed = 1;        // player+0xAA8: the Cut record's playSpeed times the manager's +0xEC (1.0)
  // Refusals of the transition object before this step starts (manager+0x254 at the switch): the default is the measured
  // one of a quiet frame. A render hitch costs more (a logged goal needed 6 for one switch): an input for replays.
  static constexpr unsigned kDefaultNotReady = ~0u;
  unsigned notReadyBefore = kDefaultNotReady;
};

// The step length of 0x141F2DD80 lines 195-199: `max` when min <= 0 or max <= 0, else min + draw * (max - min) / 99 for the
// raw draw 0x141F7A150(scene+0x128, 100) in [0, 99]. `ninetyNine` is the float at 0x1425C48CC.
float CutLength(float minTicks, float maxTicks, int draw, float ninetyNine);

struct TimelineConfig {
  float sixty = 0;  // 0x14259A764: ticks per second (the player update's `movss xmm4` at 0x141F2F93B)
  float fps = 0;    // manager+0xF0 / player+0xAB0: 54.0 (pes::offball::kPesFps)
  // Ticks from the request to DemoInfo state 4: the pre-scene gate 0x140447290 sets DemoInfo+0x1D4, the flag state 2 waits
  // for (a frame counter against fps x a scene-dependent time: pes-port-CutsceneTiming2 section 3). An input, not a constant.
  uint32_t gateTicks = 0;
};

struct TimelineState {
  uint32_t tick = 0;
  uint8_t demoState = 2;     // DemoInfo+0x1D8: 2, 4, 5, 6, 7, 8, 10, 11 (0xB), 0 once the scene is over
  uint8_t stepPhase = 0;     // manager+0x1D8: 4 waiting for the go flag, 5 started, 6 playing, 7 teardown
  int step = 0;              // manager+0xE8
  uint8_t playerState = 4;   // player+0x58: 4 prepared / waiting for its first update, 5 playing, 6 finished
  float clock = 0;           // player+0x60 (ticks)
  float totalSeconds = 0;    // manager+0x100: the sum of the players' per-update seconds
  bool anchorWrite = false;  // this Tick ran DemoInfo state 5, the scene anchor write 0x14047D280
  int stepStarted = -1;      // the step whose player made its first update this Tick
  bool ended = false;        // this Tick moved DemoInfo to 0
};

class CutsceneTimeline {
 public:
  CutsceneTimeline(const TimelineConfig& config, std::vector<TimelineStep> steps);
  void Tick();
  const TimelineState& State() const { return state_; }
  bool Done() const { return state_.demoState == 0; }

 private:
  void Manager();
  unsigned Refusals(size_t step, unsigned fallback) const;
  void StartStep(size_t index);
  void PlayerUpdate();

  TimelineConfig config_;
  std::vector<TimelineStep> steps_;
  TimelineState state_;
  bool go_ = false;
  bool active_ = false;       // a step has started: the player updates run
  unsigned notReady_ = 0;     // manager+0x254: not-ready calls since the last step start
  bool teardownDone_ = false; // manager+0x1D4 == 4
  unsigned endWait_ = 0;
  // The player
  float length_ = 0, start_ = 0, speed_ = 0;
  bool playing_ = false;      // player+0xADE: set after the first update
  float secondsThisTick_ = 0;
};

}  // namespace pes
