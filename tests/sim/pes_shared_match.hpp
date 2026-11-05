#pragma once

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "data/matchdata.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/restartwatch.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

// Whole matches, each played once per test process.
//
// A test that needs a match declares a Probe: the match it plays (tick order, length, seed) and its own stop: game over,
// the stall watch (when the probe stops at one), its cap, or its Done(). Probes on the same match share one run of it.
// The run gives each probe the ticks it would see on its own, and stops it where it would have stopped, so a probe's
// observations and its whole-match checks are those of a run of its own. Only the simulation is shared: the tests keep
// their assertions.
namespace pestest {

constexpr int kHomeDbId = 1, kAwayDbId = 2;
constexpr unsigned long kNoTickCap = std::numeric_limits<unsigned long>::max();

// The match a probe plays.
struct MatchKey {
  bool pesLoop;
  int minutes;
  unsigned int seed;
  bool operator<(const MatchKey& other) const {
    return std::tie(pesLoop, minutes, seed) < std::tie(other.pesLoop, other.minutes, other.seed);
  }
};

// Where a probe stopped, and the restart watch's readings there.
struct ProbeStop {
  bool reached = false;
  unsigned long frame = 0;      // Match::GetSimFrame() at the stop
  unsigned long long hash = 0;  // Match::GetDeterminismHash() at the stop
  bool judged = false;          // the match's own end, not Done(): the whole-match checks apply
  bool gameOver = false;
  bool stalled = false;
  unsigned long longest = 0;
  unsigned long limit = 0;
  unsigned long measured = 0;
};

class Probe;

// Every shared probe, by the match it plays.
inline std::map<MatchKey, std::vector<Probe*>>& ProbeRegistry() {
  static std::map<MatchKey, std::vector<Probe*>> registry;
  return registry;
}

class Probe {
 public:
  struct Failure {
    std::string file;
    int line;
    std::string message;
  };

  // A shared probe joins the run of its match that the process makes once; any other probe runs alone.
  Probe(const char* name, MatchKey key, unsigned long maxSimTicks, bool stallStops, bool judgeEnd, bool shared = true)
      : name(name), key(key), maxSimTicks(maxSimTicks), stallStops(stallStops), judgeEnd(judgeEnd), shared(shared) {
    if (shared) ProbeRegistry()[key].push_back(this);
  }
  Probe(const Probe&) = delete;
  Probe& operator=(const Probe&) = delete;
  virtual ~Probe() = default;

  // Called once, before the match's first tick.
  virtual void OnStart(Match& match) { (void)match; }
  // Called after every tick the probe is live for.
  virtual void OnTick(Match& match) { (void)match; }
  // Ends the probe early when true, after OnTick; its whole-match checks are then skipped.
  virtual bool Done(Match& match) {
    (void)match;
    return false;
  }

  const char* name;
  const MatchKey key;
  const unsigned long maxSimTicks;
  const bool stallStops;  // the stall watch ends the probe at the first stoppage past its bound
  const bool judgeEnd;    // the whole-match checks apply when the probe reaches the match's own end
  const bool shared;

  // Set by Run.
  bool skipped = false;           // PES21_ROOT is not set
  std::string setupError;         // the match could not be built
  std::vector<Failure> failures;  // raised by the callbacks; replayed into the test by Settle
  ProbeStop stop;
};

// Plays the match once with `probes` attached, each stopping where it would have run alone.
inline void Execute(const MatchKey& key, const std::vector<Probe*>& probes) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) {
    for (Probe* probe : probes) probe->skipped = true;
    return;
  }
  const auto fail = [&probes](const std::string& why) {
    for (Probe* probe : probes) probe->setupError = why;
  };
  pes::PesVfs vfs;
  std::string err;
  if (!vfs.Mount(root, &err)) return fail(err);
  auto rig = pesrig::BuildRig(vfs, &err);
  if (!rig) return fail(err);
  blunted::Database db;
  if (!db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql")) return fail("teams.sql did not load");
  SetDB(&db);

  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = kHomeDbId;
  setup->awayTeamDbId = kAwayDbId;
  setup->seed = key.seed;
  setup->durationMinutes = key.minutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  if (!FillMissingPesConstants(setup.get())) return fail("install constants unavailable");

  Properties config;
  config.SetBool("pes_loop", key.pesLoop);
  Match match(new MatchData(kHomeDbId, kAwayDbId), *setup, config);
  RestartWatch watch(RestartLimitTicks(*setup->pesStoppageConstants, *setup->pesSetPieceTakerConstants));

  // Callbacks run under a reporter, so their assertions are kept per probe and replayed by Settle.
  ::testing::TestPartResultArray raised;
  ::testing::ScopedFakeTestPartResultReporter intercept(
      ::testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &raised);
  const auto record = [&raised](Probe& probe, int from) {
    for (int i = from; i < raised.size(); ++i) {
      const ::testing::TestPartResult& result = raised.GetTestPartResult(i);
      if (result.failed())
        probe.failures.push_back({result.file_name() ? result.file_name() : "", result.line_number(), result.message()});
    }
  };

  std::vector<char> live(probes.size(), 1);
  const auto finish = [&](size_t i, bool judged) {
    Probe& probe = *probes[i];
    probe.stop.reached = true;
    probe.stop.frame = static_cast<unsigned long>(match.GetSimFrame());
    probe.stop.hash = match.GetDeterminismHash();
    probe.stop.judged = judged;
    probe.stop.gameOver = match.IsGameOver();
    probe.stop.stalled = watch.Stalled();
    probe.stop.longest = watch.LongestTicks();
    probe.stop.limit = watch.LimitTicks();
    probe.stop.measured = watch.MeasuredStoppages();
    live[i] = 0;
  };

  for (size_t i = 0; i < probes.size(); ++i) {
    const int from = raised.size();
    probes[i]->OnStart(match);
    record(*probes[i], from);
  }
  unsigned long ticks = 0;
  for (;;) {
    bool anyLive = false;
    for (size_t i = 0; i < probes.size(); ++i) {
      if (!live[i]) continue;
      const Probe& probe = *probes[i];
      if (match.IsGameOver() || (probe.stallStops && watch.Stalled()) || ticks >= probe.maxSimTicks)
        finish(i, true);
      else
        anyLive = true;
    }
    if (!anyLive) break;
    match.Process();
    watch.Observe(match);
    ++ticks;
    for (size_t i = 0; i < probes.size(); ++i) {
      if (!live[i]) continue;
      Probe& probe = *probes[i];
      const int from = raised.size();
      probe.OnTick(match);
      const bool done = probe.Done(match);
      record(probe, from);
      if (done) finish(i, false);
    }
  }
}

// Runs the match of `probe`: a shared probe's match once per process, with every shared probe on it; any other probe alone.
// Afterwards the probe's stop, failures and setup state are set.
inline void Run(Probe& probe) {
  if (!probe.shared) {
    Execute(probe.key, {&probe});
    return;
  }
  static std::set<MatchKey> played;
  if (played.insert(probe.key).second) Execute(probe.key, ProbeRegistry()[probe.key]);
}

// The whole-match checks of a run that reached the match's own end.
inline void JudgeWithinTheBound(const ProbeStop& s) {
  EXPECT_FALSE(s.stalled) << "a stoppage ran past " << s.limit << " ticks without a restart (longest " << s.longest
                          << ")";
  EXPECT_TRUE(s.gameOver) << "the match did not reach full time";
  EXPECT_GT(s.measured, 0ul) << "no stoppage was measured";
  EXPECT_LE(s.longest, s.limit);
}

// Replays the failures a probe's callbacks raised into the calling test, then judges its stop when it judges its end.
inline void Settle(const Probe& probe) {
  for (const Probe::Failure& failure : probe.failures) ADD_FAILURE_AT(failure.file.c_str(), failure.line) << failure.message;
  if (probe.judgeEnd && probe.stop.judged) JudgeWithinTheBound(probe.stop);
}

// A probe written as callbacks, run alone: the tests that play a match alone keep their callbacks as they were.
class CallbackProbe : public Probe {
 public:
  CallbackProbe(const char* name, MatchKey key, unsigned long maxSimTicks, bool stallStops, bool judgeEnd,
                std::function<void(Match&)> onTick, std::function<bool(Match&)> done)
      : Probe(name, key, maxSimTicks, stallStops, judgeEnd, /*shared=*/false),
        onTick_(std::move(onTick)),
        done_(std::move(done)) {}
  void OnTick(Match& match) override {
    if (onTick_) onTick_(match);
  }
  bool Done(Match& match) override { return done_ && done_(match); }

 private:
  std::function<void(Match&)> onTick_;
  std::function<bool(Match&)> done_;
};

}  // namespace pestest
