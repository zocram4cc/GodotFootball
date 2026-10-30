// PES's scene timeline (src/pes/cutscene_timeline.hpp). The relation tests need nothing from the install. The oracle
// (CapturedGoalsReplay) replays goals logged from a running PES by tools/dev/cutscene/live_timeline_poll.py, turned into
// episodes by timeline_episodes.py; set GF_CUT_EPISODES to that file (PES-derived: never committed), else it skips.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "pes/cutscene_timeline.hpp"

namespace {

using pes::CutsceneTimeline;
using pes::TimelineConfig;
using pes::TimelineState;
using pes::TimelineStep;

// The PES constants every relation below is written against: the player clock is in 1/60 s ticks, one update is
// (60 / 54) * speed ticks. Values of the image (0x14259A764) and of the manager's fps (+0xF0).
constexpr float kSixty = 60.0f, kFps = 54.0f;

TimelineConfig Config(uint32_t gate) {
  TimelineConfig c;
  c.sixty = kSixty;
  c.fps = kFps;
  c.gateTicks = gate;
  return c;
}

// Ticks (1-based) at which each marker happens in a run to the end.
struct Marks {
  std::vector<uint32_t> started;      // first update of step i
  std::vector<uint32_t> finished;     // the player of step i reads state 6
  std::vector<float> totalAtFinish;   // manager total seconds when it did
  uint32_t anchor = 0, ended = 0, go = 0;
  std::vector<float> clockAtStart;
};

Marks Play(uint32_t gate, std::vector<TimelineStep> steps) {
  Marks m;
  m.started.assign(steps.size(), 0);
  m.finished.assign(steps.size(), 0);
  m.totalAtFinish.assign(steps.size(), 0);
  m.clockAtStart.assign(steps.size(), 0);
  CutsceneTimeline t(Config(gate), steps);
  for (int i = 0; i < 100000 && !t.Done(); ++i) {
    t.Tick();
    const TimelineState& s = t.State();
    if (s.anchorWrite) m.anchor = s.tick;
    if (s.stepStarted >= 0) {
      m.started[size_t(s.stepStarted)] = s.tick;
      m.clockAtStart[size_t(s.stepStarted)] = s.clock;
    }
    if (s.stepPhase >= 5 && s.playerState == 6 && s.step >= 0 && m.finished[size_t(s.step)] == 0) {
      m.finished[size_t(s.step)] = s.tick;
      m.totalAtFinish[size_t(s.step)] = s.totalSeconds;
    }
    if (s.ended) m.ended = s.tick;
  }
  return m;
}

uint32_t UpdatesForLength(float length) { return uint32_t(std::ceil(double(length) * kFps / kSixty)); }

TEST(CutsceneTimeline, FirstStepStartsSixTicksAfterTheGateAndTheAnchorIsWrittenTwoAfter) {
  const Marks m = Play(100, {{330, 0, 1}});
  EXPECT_EQ(m.anchor, 102u);   // DemoInfo state 5 runs two frames after state 4 (0x14047C830)
  EXPECT_EQ(m.started[0], 106u);  // 4 -> 5 -> 6 -> 7 -> 8 (go) -> 10, then one refusal of the transition object
}

TEST(CutsceneTimeline, StepFinishesOneUpdateAfterTheClockReachesItsLength) {
  // 330 ticks = 297 updates of 1.1111 ticks; the first update does not move the clock, so the clock reaches the length at
  // update 298 and the player reads finished at update 299.
  const Marks m = Play(100, {{330, 0, 1}});
  EXPECT_EQ(m.finished[0] - m.started[0], UpdatesForLength(330) + 1);
  // The manager's seconds are the clock's travel plus the first update's idle one.
  EXPECT_NEAR(m.totalAtFinish[0], (330.0f + kSixty / kFps) / kSixty, 1e-4);
}

TEST(CutsceneTimeline, StartTimeJumpsTheClockInTheFirstUpdateAndShortensTheStep) {
  const Marks m = Play(100, {{178, 15, 1}});
  EXPECT_EQ(m.clockAtStart[0], 15.0f);
  EXPECT_EQ(m.finished[0] - m.started[0], UpdatesForLength(178 - 15) + 1);
  // The jump counts as one update that adds start/60 s.
  EXPECT_NEAR(m.totalAtFinish[0], 178.0f / kSixty, 1e-4);
}

TEST(CutsceneTimeline, SecondStepStartsFiveTicksAfterTheFirstFinishedAndInTheSameFrameAsItsFirstUpdate) {
  const Marks m = Play(100, {{184, 0, 1}, {180, 0, 1}});
  EXPECT_EQ(m.started[1] - m.finished[0], 5u);  // four refusals of the next step (manager+0x254 = 4), then the switch
  // The manager's seconds keep adding across steps.
  EXPECT_NEAR(m.totalAtFinish[1], (184.0f + 180.0f + 2 * kSixty / kFps) / kSixty, 1e-4);
}

TEST(CutsceneTimeline, SceneEndsWhenTheStepListRunsOutNotWhenTheFirstStepEnds) {
  const Marks two = Play(100, {{184, 0, 1}, {180, 0, 1}});
  const Marks one = Play(100, {{184, 0, 1}});
  EXPECT_EQ(one.ended - one.finished[0], 8u);
  EXPECT_EQ(two.ended - two.finished[1], 8u);
  EXPECT_GT(two.ended, one.ended + UpdatesForLength(180));
}

TEST(CutsceneTimeline, FinishedStateIsAbsorbingAndTicksAreIgnored) {
  CutsceneTimeline t(Config(5), {{20, 0, 1}});
  for (int i = 0; i < 1000; ++i) t.Tick();
  ASSERT_TRUE(t.Done());
  const uint32_t tick = t.State().tick;
  t.Tick();
  EXPECT_EQ(t.State().tick, tick);
  EXPECT_FALSE(t.State().ended);
}

TEST(CutsceneTimeline, CutLengthIsMaxUnlessBothBoundsArePositive) {
  EXPECT_EQ(pes::CutLength(0, 300, 50, 99.0f), 300.0f);
  EXPECT_EQ(pes::CutLength(100, 0, 50, 99.0f), 0.0f);
  EXPECT_EQ(pes::CutLength(100, 300, 0, 99.0f), 100.0f);
  EXPECT_EQ(pes::CutLength(100, 300, 99, 99.0f), 300.0f);
  EXPECT_NEAR(pes::CutLength(100, 300, 33, 99.0f), 166.6667f, 1e-3);
}

// The goal run object's timer (A0+0x14, PES's `0x14047A710`, called by the goal builder `0x140478190`): 4 s of PES frames
// divided by the time scale [[G+0x2C0]+0x5C], truncated; the three special scenes 0x3013D..0x3013F wait 10 s. When it runs
// out the DemoInfo's gate opens (`0x140472989`: `A0+0x14 == 0`): the latest the gate can open for a goal.
TEST(GoalRunTimer, FourSecondsOfFramesDividedByTheTimeScaleTenForTheSpecialScenes) {
  EXPECT_EQ(pes::GoalRunTimerTicks(0x30034, kFps, 1.0f), uint32_t(4 * kFps));
  EXPECT_EQ(pes::GoalRunTimerTicks(0x30034, kFps, 0.75f), uint32_t(4 * kFps / 0.75f));
  EXPECT_EQ(pes::GoalRunTimerTicks(0x30034, 60.0f, 1.0f), 240u);  // frames, not seconds: it follows the frame rate
  for (uint32_t scene : {0x3013Du, 0x3013Eu, 0x3013Fu}) EXPECT_EQ(pes::GoalRunTimerTicks(scene, kFps, 1.0f), uint32_t(10 * kFps));
  EXPECT_EQ(pes::GoalRunTimerTicks(0x3013C, kFps, 1.0f), uint32_t(4 * kFps));
  EXPECT_EQ(pes::GoalRunTimerTicks(0x30140, kFps, 1.0f), uint32_t(4 * kFps));
}

// ---- The oracle: goals logged from a running PES ----

struct Row {
  int frame = 0, frame1 = -1, demo = 0;
  int phase = -1, step = -1, pstate = -1;
  std::optional<float> pt, tot;
};
struct Episode {
  std::string name;
  int pick = 0;
  std::vector<TimelineStep> steps;
  std::vector<Row> rows;
};

std::vector<Episode> LoadEpisodes(const char* path) {
  std::vector<Episode> out;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string tag;
    ls >> tag;
    if (tag == "episode") {
      out.emplace_back();
      ls >> out.back().name >> out.back().pick;
    } else if (tag == "step") {
      TimelineStep s;
      ls >> s.lengthTicks >> s.startTicks >> s.speed;
      out.back().steps.push_back(s);
    } else if (tag == "row") {
      Row r;
      std::string phase, step, ps, pt, tot;
      ls >> r.frame >> r.frame1 >> r.demo >> phase >> step >> ps >> pt >> tot;
      if (phase != "-") {
        r.phase = std::stoi(phase);
        r.step = std::stoi(step);
        if (ps != "-") r.pstate = std::stoi(ps);
        if (pt != "-") r.pt = std::stof(pt);
        r.tot = std::stof(tot);
      }
      out.back().rows.push_back(r);
    }
  }
  return out;
}

bool Matches(const TimelineState& s, const Row& r) {
  if (s.demoState != r.demo && !(r.demo == 1 && s.demoState == 2)) return false;  // state 1 is the frame before the gate wait
  if (r.phase < 0) return true;
  return s.stepPhase == r.phase && s.step == r.step && (r.pstate < 0 || s.playerState == r.pstate) &&
         (!r.pt || std::fabs(s.clock - *r.pt) < 0.002f) && std::fabs(s.totalSeconds - *r.tot) < 0.0002f;
}

// The poll can land between the machine and the player update of the frame a step switches: the manager already holds the
// new step's byte and a fresh player (state 4) but still reads phase 6 and the previous total (0x141F59380 case 6 swaps
// +0xE8 and the players, then the scene start sets phase 5, then the tail runs the first update).
bool MatchesMidSwitch(const std::vector<TimelineState>& h, int k, const Row& r) {
  return k >= 1 && size_t(k) < h.size() && r.phase == 6 && r.pstate == 4 && h[size_t(k)].stepStarted == r.step && r.step > 0 &&
         std::fabs(h[size_t(k - 1)].totalSeconds - *r.tot) < 0.0002f;
}

// Or between the player update and the manager's accumulation of its seconds (0x141F59050 adds +0x68 after the machine): the
// new clock with the previous total.
bool MatchesMidTotal(const std::vector<TimelineState>& h, int k, const Row& r) {
  if (k < 1 || size_t(k) >= h.size() || r.phase < 0) return false;
  TimelineState s = h[size_t(k)];
  s.totalSeconds = h[size_t(k - 1)].totalSeconds;
  return Matches(s, r);
}
// A row is a poll between frames: it shows the state at the end of its frame or of the frame before it (the poller reads
// the frame counter first, the objects after, and the counter moves at the start of a frame).
// The index of the first row the model does not reproduce (rows.size() when all), and what differed.
std::pair<size_t, std::string> FirstMismatch(const Episode& e, const std::vector<TimelineStep>& steps, uint32_t gate) {
  CutsceneTimeline t(Config(gate), steps);
  std::vector<TimelineState> history{t.State()};
  const int last = e.rows.back().frame - e.pick;
  for (int i = 0; i < last + 2; ++i) {
    t.Tick();
    history.push_back(t.State());
  }
  for (size_t i = 0; i < e.rows.size(); ++i) {
    const Row& r = e.rows[i];
    const int k = r.frame - e.pick;
    // The poll reads the frame counter, then the objects, then (clean logs) the counter again: the objects show the end of
    // the frame before the counter's first reading or any frame up to its second. A log without the second reading
    // (frame1 -1, the poller of 29-10-26 and before) can have been starved between the reads: it gets four frames of slack.
    const int lo = r.frame1 >= 0 ? k - 1 : k - 4, hi = r.frame1 >= 0 ? r.frame1 - e.pick : k;
    bool hit = false;
    for (int t = std::max(lo, 0); t <= hi && size_t(t) < history.size() && !hit; ++t)
      hit = Matches(history[size_t(t)], r) || MatchesMidSwitch(history, t, r) || MatchesMidTotal(history, t, r);
    if (!hit) {
      std::ostringstream o;
      o << "gate " << gate << " frame " << r.frame << " (tick " << k << "): row demo " << r.demo << " phase " << r.phase
        << " step " << r.step << " ps " << r.pstate << " pt " << (r.pt ? *r.pt : -1) << " tot " << (r.tot ? *r.tot : -1)
        << " vs model demo " << int(history[size_t(k)].demoState) << " phase " << int(history[size_t(k)].stepPhase)
        << " step " << history[size_t(k)].step << " ps " << int(history[size_t(k)].playerState) << " clock "
        << history[size_t(k)].clock << " tot " << history[size_t(k)].totalSeconds;
      return {i, o.str()};
    }
  }
  return {e.rows.size(), ""};
}

TEST(CutsceneTimeline, CapturedGoalsReplay) {
  const char* path = std::getenv("GF_CUT_EPISODES");
  if (!path) GTEST_SKIP() << "GF_CUT_EPISODES not set (timeline_episodes.py output of a live_timeline_poll.py log)";
  const std::vector<Episode> episodes = LoadEpisodes(path);
  ASSERT_FALSE(episodes.empty());
  for (const Episode& e : episodes) {
    // Two inputs the model does not derive: the gate (ticks from the first DemoInfo row to state 4, the pre-scene counter of
    // 0x140447290) and the refusals before each later step (the transition object: 4 on a quiet frame, more after a
    // render hitch). Some gate and refusals in 4..8 must reproduce every row (state, step, player state, clock, seconds)
    // within the poll's phase; the refusals a replay needed are printed, a default (4) means the model held as it is.
    constexpr unsigned kSpan = 5;
    size_t combos = 1;
    for (size_t i = 1; i < e.steps.size(); ++i) combos *= kSpan;
    std::pair<size_t, std::string> best{0, "no gate tried"};
    std::string chosen;
    for (uint32_t gate = 1; gate <= 800 && best.first < e.rows.size(); ++gate) {
      for (size_t c = 0; c < combos && best.first < e.rows.size(); ++c) {
        std::vector<TimelineStep> steps = e.steps;
        std::string refusals;
        for (size_t i = 1, x = c; i < steps.size(); ++i, x /= kSpan) {
          steps[i].notReadyBefore = 4 + unsigned(x % kSpan);
          refusals += " " + std::to_string(steps[i].notReadyBefore);
        }
        auto fit = FirstMismatch(e, steps, gate);
        if (fit.first >= best.first) {
          best = fit;
          chosen = "gate " + std::to_string(gate) + " refusals" + refusals;
        }
      }
    }
    std::cout << "[oracle] " << e.name << ": " << chosen << (best.first == e.rows.size() ? " reproduces all " : " FAILS after ")
              << best.first << " of " << e.rows.size() << " rows\n";
    EXPECT_EQ(best.first, e.rows.size()) << e.name << ": best fit (" << chosen << ") fails at " << best.second;
  }
}

}  // namespace
