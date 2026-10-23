// Plays one headless AI-vs-AI match and prints GameplayFootball's report.
//   gfsim_run --db-sql FILE --home ID --away ID --pes21-root DIR
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchreport.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesball.hpp"
#include "onthepitch/pessetpiecetaker.hpp"
#include "onthepitch/pessetpieceteam.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pesstoppage.hpp"
#include "onthepitch/peskickerror.hpp"
#include "onthepitch/pesoffball.hpp"
#include "onthepitch/pesclock.hpp"
#include "onthepitch/restartwatch.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/vfs.hpp"
#include "pes/playerdb.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

namespace {

constexpr unsigned int DEFAULT_SEED = 1;
constexpr int DEFAULT_MINUTES = 20;    // GF's default match length in real minutes
constexpr float DEFAULT_TIMESCALE = 1.0f;
constexpr const char* DEFAULT_ANIMS = "assets/gf_anims";
// The sim-tick cap: a match still unfinished after 60 minutes of logic frames (54 Hz) is
// stuck. The clock stops in every stoppage, so a full match runs longer than its 20 real
// minutes: seed 7 reaches full time at 128912 ticks (about 40 minutes). The restart watch
// stops a stalled restart long before this, so the cap is the backstop.
constexpr unsigned long kMaxSimTicks = 60ul * 60ul * Match::kFramesPerSecond;
constexpr int EXIT_USAGE = 2;

int Usage(const char* why) {
  std::fprintf(stderr, "%s\nusage: gfsim_run --db-sql FILE --home ID --away ID --pes21-root DIR "
                       "[--seed N] [--minutes N] [--timescale X] [--anims DIR] [--set key=value]... [--pes-loop] [--parity-log]\n", why);
  return EXIT_USAGE;
}

// --parity-log: a read-only observer for tools/dev/parity.py. It reads the match after each
// Process() and writes nothing back, so the determinism hash is the same with and without it.
// Its lines follow the report and are named for the metric each feeds (docs/plans/pes-parity-metrics.md).
const char* ParityRestartName(e_SetPiece setPiece) {
  switch (setPiece) {
    case e_SetPiece_KickOff: return "kickoff";
    case e_SetPiece_ThrowIn: return "throwin";
    case e_SetPiece_GoalKick: return "goalkick";
    case e_SetPiece_Corner: return "corner";
    case e_SetPiece_FreeKick: return "freekick";
    default: return nullptr;  // penalties and stoppages with no set piece are not in the metric set
  }
}

class ParityLog {
 public:
  // Call once after every Match::Process().
  void Observe(Match& match) {
    ++ticks_;
    const bool live = match.IsInPlay() && !match.IsInSetPiece();
    if (live) ++liveTicks_;
    ObserveMoveSteps(match);
    ObserveRestart(match, live);
  }

  void Print(Match& match, FILE* out) {
    std::fprintf(out, "[parity-state] ticks %lu live %lu |", ticks_, liveTicks_);
    for (int s = 0; s < pes::kMoveStateCount; s++) std::fprintf(out, " %d:%lu", s, stateTicks_[s]);
    std::fprintf(out, "\n[parity-stop] ");
    for (int s = 0; s < pes::kMoveStateCount; s++)
      std::fprintf(out, "%s%d:%lu", s ? " " : "", s, stopFrom_[s]);
    std::fprintf(out, "\n");
    for (const Restart& r : restarts_)
      std::fprintf(out, "[parity-restart] %s whistle=%lu release=%lu frames=%lu\n", r.name, r.whistle,
                   r.release, r.release - r.whistle);
    MatchData* data = match.GetMatchData();
    std::fprintf(out, "[parity-actions] passes %d-%d crosses %d-%d shots %d-%d tackles %d-%d slides %d-%d\n",
                 data->GetPassAttempts(0), data->GetPassAttempts(1), data->GetCrosses(0), data->GetCrosses(1),
                 data->GetShots(0), data->GetShots(1), data->GetChallengesTackles(0),
                 data->GetChallengesTackles(1), data->GetChallengesSlides(0), data->GetChallengesSlides(1));
  }

 private:
  struct Restart {
    const char* name;
    unsigned long whistle;
    unsigned long release;
  };

  // Every active player's MOVESTEP state this tick (M1), and each entry into STOP by the state
  // before it in this per-tick stream (M2).
  void ObserveMoveSteps(Match& match) {
    for (int teamID = 0; teamID < 2; teamID++) {
      players_.clear();  // GetActivePlayers appends
      match.GetActiveTeamPlayers(teamID, players_);
      for (Player* player : players_) {
        const pes::MoveState state = player->GetMoveStep().state;
        ++stateTicks_[static_cast<int>(state)];
        const auto seen = lastState_.find(player);
        if (seen != lastState_.end() && state == pes::MoveState::kStop && seen->second != state)
          ++stopFrom_[static_cast<int>(seen->second)];
        lastState_[player] = state;
      }
    }
  }

  // A stoppage runs from the first tick the match is not live to the first live tick after it, as
  // RestartWatch measures it. Kept: a kick-off, throw-in, goal kick, corner or free kick, except the
  // opening kick-off (no play before it). A period start is kept too: the referee moves the match phase
  // at the whistle (referee.cpp, the period writer), before this observer sees the stoppage, so the phase
  // check below drops only a restart whose phase changes inside the stoppage.
  void ObserveRestart(Match& match, bool live) {
    if (!live) {
      if (!inStoppage_) {
        inStoppage_ = true;
        whistle_ = ticks_;
        phase_ = match.GetMatchPhase();
        opening_ = liveTicks_ == 0;
        setPiece_ = e_SetPiece_None;
      }
      for (int teamID = 0; teamID < 2; teamID++) {
        const e_SetPiece setPiece = match.GetTeam(teamID)->GetController()->GetSetPieceType();
        if (setPiece != e_SetPiece_None) setPiece_ = setPiece;  // the last set piece named in the stoppage
      }
      return;
    }
    if (!inStoppage_) return;
    inStoppage_ = false;
    const char* name = ParityRestartName(setPiece_);
    if (name && !opening_ && match.GetMatchPhase() == phase_)
      restarts_.push_back({name, whistle_, ticks_});
  }

  unsigned long ticks_ = 0, liveTicks_ = 0;
  std::array<unsigned long, pes::kMoveStateCount> stateTicks_{}, stopFrom_{};
  std::unordered_map<const Player*, pes::MoveState> lastState_;
  std::vector<Player*> players_;
  bool inStoppage_ = false;
  unsigned long whistle_ = 0;
  e_MatchPhase phase_ = e_MatchPhase_PreMatch;
  bool opening_ = false;
  e_SetPiece setPiece_ = e_SetPiece_None;
  std::vector<Restart> restarts_;
};

}  // namespace

int main(int argc, char** argv) {
  std::string dbSql, root, anims = DEFAULT_ANIMS;
  if (const char* env = std::getenv("PES21_ROOT")) root = env;
  int home = 0, away = 0, minutes = DEFAULT_MINUTES;
  unsigned int seed = DEFAULT_SEED;
  float timeScale = DEFAULT_TIMESCALE;
  bool parityLog = false;  // --parity-log: the read-only observer in ParityLog
  Properties config;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--pes-loop") {  // the PES tick order (SW): a flag, no value
      config.SetBool("pes_loop", true);
      continue;
    }
    if (a == "--parity-log") {
      parityLog = true;
      continue;
    }
    if (i + 1 >= argc) return Usage(("missing value for " + a).c_str());
    const std::string v = argv[++i];
    if (a == "--db-sql") dbSql = v;
    else if (a == "--home") home = std::atoi(v.c_str());
    else if (a == "--away") away = std::atoi(v.c_str());
    else if (a == "--pes21-root") root = v;
    else if (a == "--seed") seed = unsigned(std::strtoul(v.c_str(), nullptr, 10));
    else if (a == "--minutes") minutes = std::atoi(v.c_str());
    else if (a == "--timescale") timeScale = std::strtof(v.c_str(), nullptr);
    else if (a == "--anims") anims = v;
    else if (a == "--set") {
      const size_t eq = v.find('=');
      if (eq == std::string::npos) return Usage(("--set needs key=value: " + v).c_str());
      config.Set(v.substr(0, eq).c_str(), v.substr(eq + 1));
    } else {
      return Usage(("unknown argument " + a).c_str());
    }
  }
  if (root.empty()) return Usage("need --pes21-root (the player skeleton comes from your PES 2021 install)");
  if (dbSql.empty() || home <= 0 || away <= 0) return Usage("need --db-sql, --home and --away");

  std::string err;
  pes::PesVfs vfs;
  if (!vfs.Mount(root, &err)) return std::fprintf(stderr, "PES21 mount failed: %s\n", err.c_str()), 1;
  auto rig = pesrig::BuildRig(vfs, &err);
  if (!rig) return std::fprintf(stderr, "PES21 rig failed: %s\n", err.c_str()), 1;
  // PES 2021's match-clock constants, read from the install's own exe
  // (docs/re/pes-re-tactics-clock.md 7.2: every clock value lives in the image,
  // none in a WESYS object).
  auto exe = pes::PeImage::Open(root + "/PES2021.exe", &err);
  if (!exe) return std::fprintf(stderr, "PES2021.exe: %s\n", err.c_str()), 1;
  auto clockConstants = pes::LoadPesClockConstants(*exe, &err);
  if (!clockConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's ball physics: the `ball.o` record of constant_match.bin plus the
  // image values (docs/re/pes-re-ball-contact.md 6).
  auto ballConstants = pes::LoadPesBallConstants(*exe, vfs, &err);
  if (!ballConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto judgeConstants = pes::LoadJudgeConstants(*exe, &err);
  if (!judgeConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto rngConstants = pes::LoadRngConstants(*exe, &err);
  if (!rngConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's goalkeeper deflect point and post-catch distribution target
  // (docs/re/pes-re-keeper-port.md).
  auto keeperConstants = pes::LoadKeeperConstants(*exe, &err);
  if (!keeperConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's set-piece taker wait: how long the AI holds a restart before the
  // kicker takes it (docs/re/pes-re-setpiece-choreography.md 9 item 2).
  auto setPieceTakerConstants = pes::LoadSetPieceTakerConstants(vfs, *exe, &err);
  if (!setPieceTakerConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's stamina model (docs/re/pes-re-stamina-drain.md): the tank, the
  // drain, the breath, the break bonus and the injury roll's constants, all
  // read from the install's own exe.
  auto staminaConstants = pes::LoadStaminaConstants(*exe, &err);
  if (!staminaConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's player database (common/etc/pesdb/Player.bin): each player's injury tier and stronger foot.
  auto playerDatabase = pes::PlayerDatabase::Load(vfs, &err);
  if (!playerDatabase) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto stoppageConstants = pes::LoadStoppageConstants(*exe, &err);
  if (!stoppageConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto restartSpotConstants = pes::LoadRestartSpotConstants(*exe, &err);
  if (!restartSpotConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's kick execution error (docs/re/pes-re-kick-scatter.md).
  auto kickErrorConstants = pes::LoadKickErrorConstants(*exe, &err);
  if (!kickErrorConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's ball-carrier decision layer (docs/re/pes-re-carrier-ai.md).
  auto carrierConstants = pes::LoadCarrierConstants(*exe, &err);
  if (!carrierConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto setPieceTeamConstants = pes::LoadSetPieceTeamConstants(*exe, &err);
  if (!setPieceTeamConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's open-play defensive shape (docs/re/pes-re-defensive-shape.md 6, 7, 14).
  auto defensiveShapeConstants = pes::LoadDefensiveShapeConstants(*exe, &err);
  if (!defensiveShapeConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's engaging-defender decisions (docs/re/pes-re-defender-decisions.md):
  // the reaction table and its gates, .rdata plus two json records out of the
  // install's constant files.
  auto defenderDecisionConstants = pes::LoadDefenderDecisionConstants(*exe, vfs, &err);
  if (!defenderDecisionConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's contact reaction (docs/re/pes-re-contact.md): contact.o plus
  // the exe's level table, vetoes and balance gauge.
  auto contactConstants = pes::LoadContactConstants(*exe, vfs, &err);
  if (!contactConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's event routing table and detector constants (docs/re/pes-re-gaps-a2.md).
  auto eventRingTable = pes::LoadEventRingTable(*exe, &err);
  if (!eventRingTable) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto eventConstants = pes::LoadEventConstants(*exe, &err);
  if (!eventConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES body clips the engine plays (pesclips.hpp), converted at match start.
  auto pesClips = pes::LoadPesClips(*exe, vfs, &err);
  if (!pesClips) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's dodge selectors (docs/re/pes-re-contact-port.md stage B): the
  // exe's tables, the animetable and the per-motion metadata.
  auto dodgeConstants = pes::LoadDodgeConstants(*exe, &err);
  if (!dodgeConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto animetable = pes::LoadAnimetable(*exe, vfs, &err);
  if (!animetable) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  auto mbinfo = pes::LoadMbinfo(vfs, *exe, &err);
  if (!mbinfo) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's standing-tackle sub-kind (pes-re-contact-port.md stage C): the
  // per-kind group tables and the readiness mask's constants.
  auto tackleConstants = pes::LoadTackleConstants(*exe, &err);
  if (!tackleConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's CPU strategy decider (docs/re/pes-re-match-management.md 1).
  auto strategyConstants = pes::LoadStrategyConstants(*exe, &err);
  if (!strategyConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's team sheet defaults and transition concepts (docs/re/pes-re-tactics.md 2).
  auto teamConceptConstants = pes::LoadTeamConceptConstants(*exe, vfs, &err);
  if (!teamConceptConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's CPU substitutions (docs/re/pes-re-stamina-subs.md).
  auto substitutionConstants = pes::LoadSubstitutionConstants(*exe, &err);
  if (!substitutionConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's off-ball run manager (docs/re/pes-re-off-ball.md).
  auto offBallConstants = pes::offball::LoadOffBallConstants(*exe, vfs, &err);
  if (!offBallConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's MOVESTEP machine (docs/re/pes-re-anim-select.md): the install constants its
  // handlers read, handed to the Match through MatchSetup.
  auto moveStepConstants = pes::LoadMoveStepConstants(*exe, vfs, &err);
  if (!moveStepConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;
  // PES 2021's presser picker and loose-ball PRESS chase (docs/re/pes-re-press-life.md).
  auto pressConstants = pes::LoadPressConstants(*exe, &err);
  if (!pressConstants) return std::fprintf(stderr, "%s\n", err.c_str()), 1;

  blunted::Database db;
  if (!db.LoadSqlScript(dbSql)) return std::fprintf(stderr, "cannot load %s\n", dbSql.c_str()), 1;
  SetDB(&db);

  MatchSetup setup;
  setup.homeTeamDbId = home;
  setup.awayTeamDbId = away;
  setup.seed = seed;
  setup.durationMinutes = minutes;
  setup.timeScale = timeScale;
  setup.animationDir = anims;
  setup.rig = &*rig;
  setup.pesClockConstants = &*clockConstants;
  setup.pesBallConstants = &*ballConstants;
  setup.pesJudgeConstants = &*judgeConstants;
  setup.pesRngConstants = &*rngConstants;
  setup.pesKeeperConstants = &*keeperConstants;
  setup.pesSetPieceTakerConstants = &*setPieceTakerConstants;
  setup.pesSetPieceTeamConstants = &*setPieceTeamConstants;
  setup.pesStaminaConstants = &*staminaConstants;
  setup.pesPlayerDatabase = &*playerDatabase;
  setup.pesStoppageConstants = &*stoppageConstants;
  setup.pesRestartSpotConstants = &*restartSpotConstants;
  setup.pesKickErrorConstants = &*kickErrorConstants;
  setup.pesCarrierConstants = &*carrierConstants;
  setup.pesDefensiveShapeConstants = &*defensiveShapeConstants;
  setup.pesDefenderDecisionConstants = &*defenderDecisionConstants;
  setup.pesPressConstants = &*pressConstants;
  setup.pesContactConstants = &*contactConstants;
  setup.pesEventRingTable = &*eventRingTable;
  setup.pesEventConstants = &*eventConstants;
  setup.pesClips = &*pesClips;
  setup.pesDodgeConstants = &*dodgeConstants;
  setup.pesAnimetable = &*animetable;
  setup.pesMbinfo = &*mbinfo;
  setup.pesTackleConstants = &*tackleConstants;
  setup.pesStrategyConstants = &*strategyConstants;
  setup.pesTeamConceptConstants = &*teamConceptConstants;
  setup.pesSubstitutionConstants = &*substitutionConstants;
  setup.pesOffBallConstants = &*offBallConstants;
  setup.pesMoveStepConstants = &*moveStepConstants;

  Match match(new MatchData(home, away), setup, config);  // Match owns the MatchData
  // A stalled restart freezes the match clock (it only moves while play is on and no
  // set piece is armed), so the match would never reach full time. The watch stops
  // the run at the first stoppage that runs past the install's bound on its restart.
  RestartWatch watch(RestartLimitTicks(*stoppageConstants, *setPieceTakerConstants));
  // Wall-time split: PES's hold timing is the sim, so every tick runs; this
  // only records which ticks had live play. std::chrono around Process,
  // classified by the pre-tick state (no sim state touched).
  unsigned long inPlayTicks = 0, holdTicks = 0;
  double inPlaySeconds = 0.0, holdSeconds = 0.0;
  ParityLog parity;
  while (!match.IsGameOver()) {
    if (watch.Ticks() >= kMaxSimTicks)
      return std::fprintf(stderr, "[cap] no full time after %lu sim ticks\n", kMaxSimTicks), 1;
    const bool live = match.IsInPlay() && !match.IsInSetPiece();
    const auto tickStart = std::chrono::steady_clock::now();
    match.Process();
    const double tickSeconds_ =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - tickStart).count();
    if (live) {
      ++inPlayTicks;
      inPlaySeconds += tickSeconds_;
    } else {
      ++holdTicks;
      holdSeconds += tickSeconds_;
    }
    watch.Observe(match);
    if (parityLog) parity.Observe(match);
    if (watch.Stalled())
      return std::fprintf(stderr,
                          "[restart] stalled: no restart within %lu ticks (limit %lu ticks)\n",
                          watch.LongestTicks(), watch.LimitTicks()), 1;
  }
  PrintMatchReport(match, stdout);
  const double tickSeconds = Match::kTick_ms / 1000.0;
  std::printf("[restarts] %lu stoppages, longest %.1f s (%lu ticks), bound %.1f s (%lu ticks)\n",
              watch.MeasuredStoppages(), watch.LongestTicks() * tickSeconds, watch.LongestTicks(),
              watch.LimitTicks() * tickSeconds, watch.LimitTicks());
  std::printf("[profile-split] in-play %lu ticks (%.1f s) | hold %lu ticks (%.1f s)\n", inPlayTicks,
              inPlaySeconds, holdTicks, holdSeconds);
  std::printf("[ticks] %lu sim ticks to full time (cap %lu)\n", watch.Ticks(), kMaxSimTicks);
  std::printf("[idle] %lu episodes, stalls %lu\n", watch.IdleEpisodes(), watch.IdleStallEpisodes());
  if (parityLog) parity.Print(match, stdout);
  return 0;
}
