#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <cstdio>
#include <cstdlib>

#include "base/log.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/player/humanoid/humanoid.hpp"
#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "onthepitch/pesmovestep.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/team.hpp"
#include "pes_match_constants.hpp"
#include "pes_shared_match.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

// The stalemate breaker is GF's band-aid: once nobody has moved the ball for five seconds it designates the
// nearest man, whatever the referee has armed. PES never needs it. So a firing is a defect in one of two places:
// the breaker counted idle time over a restart the referee had armed, or a live ball held by a man who never
// released it. Both are counted over one whole PES match (pes_loop on), tick by tick.
namespace {

constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
// The cap sim_match_test.cpp uses.
constexpr unsigned long kMaxTicks = 30ul * 60ul * 100ul;
// The quick target plays the same seed-7 match and stops after its first kQuickMinutes of sim time. A match's durationMinutes is its
// real-time length on the clock (match.cpp builds the PES clock from it), but matchduration.hpp clamps it to 5..90 minutes, so the
// full match cannot be shortened by its setting: kMinutes = 2 plays the five-minute clock. The window is therefore a prefix of that
// match: kQuickMinutes * 60 seconds of logic ticks at the sim's own rate, the same minutes-times-seconds-times-rate rule kMaxTicks uses.
// Four minutes (240 s, 12960 ticks) holds every forced tick before 240 s on 43e96723 (64 of 118, all from 218.8 s); the next is at 373.8 s.
constexpr unsigned long kQuickMinutes = 4;
constexpr unsigned long kSecondsPerMinute = 60ul;
constexpr unsigned long kQuickMaxTicks = kQuickMinutes * kSecondsPerMinute * Match::kFramesPerSecond;

// One whole PES match, with each breaker firing filed by the state of play on its tick. The match is the seed-7 match the
// restart-delay tests play, so the two families share one run (pes_shared_match.hpp).
class BreakerCensusProbe : public pestest::Probe {
 public:
  BreakerCensusProbe(const char* name = "PesStall", unsigned long maxSimTicks = kMaxTicks, bool shared = true)
      : Probe(name, {true, kMinutes, kSeed}, maxSimTicks, /*stallStops=*/false, /*judgeEnd=*/false, shared) {}
  unsigned long ticks = 0;  // ticks the probe saw
  long armedFirings = 0;    // the referee had a restart armed (its buffer is active)
  long liveFirings = 0;     // play is live, nothing is armed: a man on a ball that nobody plays
  // Attribution of the live firings by the man the breaker forces (the series freeze is AnimeLayer's SetParametric port, a separate defect): a PES-owned
  // dribbler whose body sits on the last frame of a clip with a series held in P is the freeze; every other firing, by the forced man's state.
  long liveFiringsSeriesFrozen = 0;
  // A PES dribbler frozen on the last frame of a series clip holds the ball for PES (MA+0x2601C) while it lies still beside him: the other men's firings
  // while one has been frozen for a second are downstream of the same defect (the defenders challenge a holder who never plays on).
  long liveFiringsDuringFreeze = 0;
  std::map<const Player*, int> frozenTicks_;
  std::vector<std::pair<long, std::string>> liveEpisodes;  // (ticks, start tick / man / state at the start): one per run of consecutive firings on one man
  long episodeStart_ = -1, episodeLast_ = -1, tick_ = 0;
  const Player* episodeMan_ = nullptr;
  std::string episodeLabel_;
  std::map<std::string, long> liveFiringsOther;
  long unarmedFirings = 0;  // play is off and nothing is armed
  // The walker (docs/re/pes-re-walker-touch.md round 2, R-RED): a man in MOVE, not the designated owner, within kWalkerReach_m of a ball at rest with
  // no PES holder, starts a trap within kWalkerWindowTicks or the walk is counted once. The reach and the window are the decode's harness choices.
  static constexpr float kWalkerReach_m = 0.5f;
  static constexpr long kWalkerWindowTicks = static_cast<long>(pes::kPesTickHz);  // one second of PES ticks
  std::map<const Player*, long> walkStart_;  // the tick each walk began, -1 once it was counted
  long walkUntouched = 0;
  void OnStart(Match& match) override { lastCount_ = match.GetStalemateCount(); }
  void OnTick(Match& match) override {
    ticks++;
    ++tick_;
    bool freezeActive = false;
    for (int team = 0; team < 2; ++team) {
      for (const auto& pl : match.GetTeam(team)->GetAllPlayers()) {
        if (!pl->IsActive()) continue;
        uint32_t seriesCount;
        std::memcpy(&seriesCount, match.PesWorldOf().P(match.PesSlotOf(pl.get())) + pes::kPSeriesCountOffset, sizeof seriesCount);
        const Anim* clip = pl->GetCurrentAnim();
        const bool frozen = match.PesOwnsDribble(*pl) && seriesCount > 0 && clip && clip->anim && clip->frameNum >= clip->anim->GetFrameCount() - 1 && pl->GetMovement().GetLength() < 0.1f;
        int& n = frozenTicks_[pl.get()];
        n = frozen ? n + 1 : 0;
        constexpr int kFrozenSecond = 100;  // ticks of 10 ms
        freezeActive = freezeActive || n >= kFrozenSecond;
      }
    }
    const bool ballAtRest = match.GetBall()->GetMovement().GetLength() <= GameplayTuning::kStalemateBallSpeed_ms;
    const Vector3 ball = match.GetBall()->Predict(0);
    for (int team = 0; team < 2; ++team) {
      for (const auto& pl : match.GetTeam(team)->GetAllPlayers()) {
        if (!pl->IsActive()) continue;
        const bool walking = match.IsInPlay() && pl->GetCurrentFunctionType() == e_FunctionType_Movement &&
                             pl.get() != match.GetDesignatedPossessionPlayer() && match.GetPesHolder() == nullptr && ballAtRest &&
                             (pl->GetPosition() - ball).Get2D().GetLength() <= kWalkerReach_m;
        const auto it = walkStart_.find(pl.get());
        if (!walking) {
          if (it != walkStart_.end()) walkStart_.erase(it);
          continue;
        }
        if (it == walkStart_.end()) {
          walkStart_[pl.get()] = tick_;
        } else if (it->second >= 0 && tick_ - it->second >= kWalkerWindowTicks) {
          walkUntouched++;
          it->second = -1;
        }
      }
    }
    const unsigned long count = match.GetStalemateCount();
    const bool fired = count != lastCount_;
    lastCount_ = count;
    if (!fired) return;
    if (match.GetRefereeBuffer().active)
      armedFirings++;
    else if (match.IsInPlay() && !match.IsInSetPiece()) {
      liveFirings++;
      if (std::getenv("GF_STALL_DUMP")) Dump(match);
      Player* who = match.GetDesignatedPossessionPlayer();
      bool seriesFrozenMan = false;
      if (who != nullptr) {
        uint32_t sc;
        std::memcpy(&sc, match.PesWorldOf().P(match.PesSlotOf(who)) + pes::kPSeriesCountOffset, sizeof sc);
        const Anim* cc = who->GetCurrentAnim();
        seriesFrozenMan = match.PesOwnsDribble(*who) && sc > 0 && cc && cc->anim && cc->frameNum >= cc->anim->GetFrameCount() - 1;
      }
      const bool downstreamOfFreeze = freezeActive || seriesFrozenMan;  // the episode list below holds the firings the series freeze does not explain
      if (!downstreamOfFreeze && (who != episodeMan_ || tick_ != episodeLast_ + 1)) {
        if (episodeStart_ >= 0) liveEpisodes.push_back({episodeLast_ - episodeStart_ + 1, episodeLabel_});
        episodeStart_ = tick_;
        episodeMan_ = who;
        const uint8_t* ep = who ? match.PesWorldOf().P(match.PesSlotOf(who)) : nullptr;
        const Anim* ec = who ? who->GetCurrentAnim() : nullptr;
        episodeLabel_ = "t" + std::to_string(tick_) + " " + (who ? std::string(who->GetPlayerData()->GetLastName()) : std::string("-")) + " ac8 " + (ep ? std::to_string(ep[pes::kPByteAC8Offset]) : "-") +
                        (who && match.PesOwnsDribble(*who) ? " PES dribbler" : "") + " clip " + (ec && ec->anim ? ec->anim->GetName() : std::string("-")) + " ball " +
                        std::to_string((who ? (who->GetPosition() - match.GetBall()->Predict(0)).Get2D().GetLength() : 0.0f)).substr(0, 4) + "m";
      }
      if (!downstreamOfFreeze) episodeLast_ = tick_;
      if (who != nullptr) {
        const uint8_t* p = match.PesWorldOf().P(match.PesSlotOf(who));
        uint32_t seriesCount;
        std::memcpy(&seriesCount, p + pes::kPSeriesCountOffset, sizeof seriesCount);
        const Anim* clip = who->GetCurrentAnim();
        const bool lastFrame = clip && clip->anim && clip->frameNum >= clip->anim->GetFrameCount() - 1;
        if (match.PesOwnsDribble(*who) && seriesCount > 0 && lastFrame) {
          liveFiringsSeriesFrozen++;
        } else if (freezeActive) {
          liveFiringsDuringFreeze++;
        } else {
          const int family = clip && clip->anim ? std::atoi(clip->anim->GetVariable("specialvar1").c_str()) : -1;
          if (std::getenv("GF_STALL_DUMP")) {
            const std::string name = clip && clip->anim ? clip->anim->GetName() : std::string("-");
            std::fprintf(stderr, "STALLDUMP t=%ld man=%s clip=%s family=%d ac8=%d owned=%d fn=%d ball=%.3f series=%u\n", tick_, std::string(who->GetPlayerData()->GetLastName()).c_str(), name.c_str(), family, int(p[pes::kPByteAC8Offset]), match.PesOwnsDribble(*who) ? 1 : 0, int(who->GetCurrentFunctionType()), double((who->GetPosition() - match.GetBall()->Predict(0)).Get2D().GetLength()), seriesCount);
          }
          liveFiringsOther["ac8 " + std::to_string(p[pes::kPByteAC8Offset]) + (match.PesOwnsDribble(*who) ? ", PES-owned dribbler" : ", not a PES dribbler") + ", body family " + std::to_string(family) + ", function " + std::to_string(int(who->GetCurrentFunctionType()))]++;
        }
      }
      else if (std::getenv("GF_STALL_DUMP")) std::fprintf(stderr, "STALLDUMP t=%ld man=none\n", tick_);
    }
    else
      unarmedFirings++;
  }

  // The summary the quick target prints: the forced-tick count, and with GF_STALL_DUMP the census by bucket.
  void Print(const char* label) const {
    std::printf("%s ticks=%lu of %lu forced_live=%ld\n", label, ticks, maxSimTicks, liveFirings);
    if (std::getenv("GF_STALL_DUMP"))
      std::printf("%s buckets armed=%ld live=%ld unarmed=%ld\n", label, armedFirings, liveFirings, unarmedFirings);
  }

  // GF_STALL_DUMP=1: one line per live firing, the forced man with his clip, body action bytes, holder state and distance.
  static void Dump(Match& match) {
    Player* who = match.GetDesignatedPossessionPlayer();
    if (!who) {
      std::printf("STALLDUMP t=%lu man=none\n", match.GetActualTime_ms());
      return;
    }
    const uint8_t* p = match.PesWorldOf().P(match.PesSlotOf(who));
    const Anim* clip = who->GetCurrentAnim();
    const auto& ms = who->GetMoveStep();
    Player* holder = match.GetPesHolder();
    std::printf("STALLDUMP t=%lu man=%d team=%d clip=%s fn=%d cmdfn=%d frame=%d/%d ac8=%d ac9=%d holder=%d retainer=%d ms=%d<-%d dist=%.2f ballv=%.2f owner=%d pesHolder=%d\n",
                match.GetActualTime_ms(), who->GetID(), who->GetTeamID(), clip->anim ? clip->anim->GetName().c_str() : "-",
                int(who->GetCurrentFunctionType()), int(clip->originatingCommand.desiredFunctionType), clip->frameNum,
                clip->anim ? clip->anim->GetFrameCount() : 0,
                int(p[pes::kPByteAC8Offset]), int(p[pes::kPByteAC9Offset]), match.GetPesHolder() == who ? 1 : 0,
                match.GetBallRetainer() == who ? 1 : 0, int(ms.state), int(ms.previous),
                double((who->GetPosition() - match.GetBall()->Predict(0)).Get2D().GetLength()),
                double(match.GetBall()->GetMovement().GetLength()),
                match.GetPesDesignatedOwner() == match.PesActorKeyOf(who) ? 1 : 0,
                holder ? holder->GetTeamID() * 100 + holder->GetID() : -1);
    // The movement fields for the 145 locomotion firings: the command's speed and target, the body's speed, the motion MOVESTEP last
    // issued (-1: the AI's clip), and the target's distance from the forced man.
    const PlayerCommand& cmd = clip->originatingCommand;
    std::printf("STALLMOVE t=%lu man=%d cmdv=%.2f usetgt=%d tgtdist=%.2f bodyv=%.2f motion=%d ms=%d\n", match.GetActualTime_ms(), who->GetID(),
                double(cmd.desiredVelocityFloat), cmd.useDesiredTarget ? 1 : 0,
                double((cmd.desiredTarget - who->GetPosition()).Get2D().GetLength()), double(who->GetMovement().GetLength()),
                pesmovestep::CurrentMotionOf(*who), int(ms.state));
    // The press state of the forced man's side: is he the loose PRESS man, and is there any PRESS man at all.
    TeamAIController* ctl = who->GetTeam()->GetController();
    const Player* pressMan = ctl->GetLoosePressMan();
    std::printf("STALLPRESS t=%lu forcedIsPresser=%d sidePress=%d sidePressId=%d sideHasPressMan=%d\n", match.GetActualTime_ms(),
                ctl->IsLoosePresser(who) ? 1 : 0, pressMan ? 1 : 0, pressMan ? pressMan->GetID() : -1,
                match.GetTeam(1 - who->GetTeamID())->GetController()->GetLoosePressMan() ? 1 : 0);
    Player* pressOwn = nullptr;
    for (const auto& p : who->GetTeam()->GetAllPlayers())
      if (p.get() == pressMan) pressOwn = p.get();
    if (pressOwn) {
      const PesDecision& decision = pressOwn->GetPesDecision();
      const PlayerCommand pcmd = decision.command ? *decision.command : PlayerCommand();
      const Vector3 ballNow = match.GetBall()->Predict(0);
      std::printf("STALLPRESSER t=%lu id=%d team=%d dist=%.2f cmdv=%.2f usetgt=%d tgtdist=%.2f bodyv=%.2f fn=%d ms=%d motion=%d play=%.2f pg=%d des=%d grp=%d rate=%.3f nat=%.2f\n", match.GetActualTime_ms(),
                  pressOwn->GetID(), pressOwn->GetTeamID(), double((pressOwn->GetPosition() - ballNow).Get2D().GetLength()),
                  double(pcmd.desiredVelocityFloat), pcmd.useDesiredTarget ? 1 : 0,
                  double((pcmd.desiredTarget - ballNow).Get2D().GetLength()), double(pressOwn->GetMovement().GetLength()),
                  int(pressOwn->GetCurrentFunctionType()), int(pressOwn->GetMoveStep().state), pesmovestep::CurrentMotionOf(*pressOwn),
                  pressOwn->CastHumanoid()->GetCurrentAnim() ? double(pressOwn->CastHumanoid()->GetCurrentAnim()->playbackRate) : 0.0,
                  pressOwn->GetTeam()->PassGetReceiver() == pressOwn ? 1 : 0, match.GetDesignatedPossessionPlayer() == pressOwn ? 1 : 0,
                  pressOwn->GetIssuedClip().request.group, double(pressOwn->GetIssuedClip().request.rate),
                  double(pressOwn->CastHumanoid()->PesClipNaturalSpeed(pesmovestep::CurrentMotionOf(*pressOwn))));
    }
    if (holder) {
      const uint8_t* hp = match.PesWorldOf().P(match.PesSlotOf(holder));
      const Anim* hc = holder->GetCurrentAnim();
      std::printf("STALLHOLDER t=%lu holder=%d team=%d dist=%.2f clip=%s fn=%d ac8=%d ac9=%d ms=%d\n", match.GetActualTime_ms(),
                  holder->GetID(), holder->GetTeamID(), double((holder->GetPosition() - match.GetBall()->Predict(0)).Get2D().GetLength()),
                  hc->anim ? hc->anim->GetName().c_str() : "-", int(holder->GetCurrentFunctionType()), int(hp[pes::kPByteAC8Offset]),
                  int(hp[pes::kPByteAC9Offset]), int(holder->GetMoveStep().state));
    }
  }

 private:
  unsigned long lastCount_ = 0;
};

BreakerCensusProbe census;

// The quick target: the same probe over the window of the same match, run alone (not shared with the family's run), so an agent
// measures the stall in a minute of sim time. Its count is the full census's count restricted to the window's ticks.
BreakerCensusProbe quickCensus("PesStallQuick", kQuickMaxTicks, /*shared=*/false);

// A man who has the ball at rest at his feet and does not play it is a trap the reaction keeps starting over: the trap clip
// is issued, the next tick the reaction starts the trap again, and the clip restarts from its first frame, so it never reaches
// its contact frame (PressAI's seed-7 goal-kick stall: the designated man 0.1 m from a resting ball for 86 s). PES's receive
// reaction holds the man in his trap while the clip plays (the action tick's end handler 0x14079E5C0 leaves at its first
// statement while the playing motion is in the anime's motion table: docs/ports/pes-port-TrapTick4.md), so a trap clip is
// never restarted before the contact frame it was heading for.
class TrapRestartProbe : public pestest::Probe {
 public:
  TrapRestartProbe()
      : Probe("PesStallTrap", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long trapClips = 0;        // trap clips that started (a tick on which a man's trap clip began)
  long restartedTraps = 0;   // those that began over a trap clip of his that had not reached its contact frame
  std::string holderSamples;
  long holderTraps = 0;      // those that began while he was PES's holder (MA+0x2601C == him)
  long trapStarts = 0;       // PES trap starts: ticks on which a man's P+0xAC8 became the Trap action
  long refusedStarts = 0;    // those whose clip the body did not take on the next tick (the action tick's PlayIssuedClip refused it)
  long gateRefusedStarts = 0;  // those that left an action gate 4a (0x14079BBE0's admitted ids) does not admit
  void OnTick(Match& match) override {
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        Humanoid* body = player->CastHumanoid();
        Seen& seen = seen_[player.get()];
        const bool onTrap = body->OnPesTrap();
        const Anim* clip = body->GetCurrentAnim();
        const bool began = onTrap && (!seen.onTrap || clip->frameNum < seen.frame);
        if (began) {
          trapClips++;
          if (seen.onTrap && seen.frame < seen.touchFrame) restartedTraps++;
          // PES's reaction entry 3 refuses the carrier unless he is in stagger, dodge or the keeper's drop (0x1409FA4AD..0x1409FA4D0): a holder who
          // left the Dodge action (P+0xACA) was admitted by PES's own rule and is not a defect. Stagger and the keeper's drop are not exempted
          // here (their action ids are not named in pestrapactor.hpp): any such case stays counted.
          const uint8_t leftAction = match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPPrevActionOffset];
          if (match.GetPesHolder() == player.get() && leftAction != pes::kAnimeActionDodge) {
            holderTraps++;
            const uint8_t* hp = match.PesWorldOf().P(match.PesSlotOf(player.get()));
            holderSamples += " [man left action " + std::to_string(hp[pes::kPPrevActionOffset]) + " sub " + std::to_string(hp[pes::kPPrevActionOffset + 1]) +
                             ", was on trap clip " + std::to_string(seen.onTrap) + ", dribble-owned " + std::to_string(match.PesOwnsDribble(*player)) + "]";
          }
        }
        const bool pesTrap = match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPByteAC8Offset] == pes::kAnimeActionTrap;
        if (seen.startedLastTick && !onTrap) refusedStarts++;
        const bool started = pesTrap && !seen.pesTrap;
        if (started) {
          trapStarts++;
          // StartTrapAction moved the action the man left into P+0xACA. PES admits a trap from the ids of 0x14079BBE0's list only; a tackle,
          // a keeper's catch or deflect and a man in no action (GF's own cards and celebrations) are not in it.
          const uint8_t left = match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPPrevActionOffset];
          if (left == pes::kAnimeActionTackle || left == pes::kAnimeActionNothing || left == pes::kAnimeActionKeeperCatch ||
              left == pes::kAnimeActionKeeperDeflect)
            gateRefusedStarts++;
        }
        seen = {onTrap, clip->frameNum, clip->touchFrame, pesTrap, started};
      }
    }
  }

 private:
  struct Seen {
    bool onTrap = false;
    int frame = 0;
    int touchFrame = -1;
    bool pesTrap = false;
    bool startedLastTick = false;
  };
  std::map<const Player*, Seen> seen_;
};

TrapRestartProbe trapCensus;

// A PES trap's touch is the body contact scan's grant (FUN_1406EDCE0, docs/ports/pes-port-TrapContact.md): the trap tick sets the
// contact-pending bit on the clip's contact frame, the scan's reach test passes or refuses it, and only a pass touches the ball. Every
// tick, each man in a PES trap clip is filed by whether the scan granted him a touch this tick (the grant counter moved) and whether he
// touched the ball this tick (his last-touch time is the tick's start: Match::Process has moved the clock on by the time the probe runs).
class TrapContactProbe : public pestest::Probe {
 public:
  TrapContactProbe()
      : Probe("PesStallContact", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long grants = 0;               // ticks a man in a trap clip took a grant
  long restingGrants = 0;        // those with the ball at rest (under a metre per second) at the tick's start
  long touchesWithoutGrant = 0;  // a man in a trap clip touched the ball with no grant: a touch with no PES source
  long grantsWithoutTouch = 0;   // a grant that did not touch the ball
  // DribbleEngage (docs/ports/pes-port-TrapContact3.md §4.1): a granted touch that leaves the ball at rest must hand the man to
  // PES's dribble: within kDribbleEngageTicks of the grant his P+0xAC8 is the Dribble action (4). A trapper stranded in Trap beside a
  // resting ball is the post-touch chain never running (end handler 0x14079E5C0 -> switch 0x1406EEBB0 -> act4 start 0x1407A38E0).
  static constexpr long kDribbleEngageTicks = 150;  // ~2.8 s: the trap clip's tail past its contact frame plus the end chain
  long strandedTrappers = 0;  // granted resting touches with no dribble inside the window
  std::string strandedTrapperSamples;  // each: the action at the window's end, whether he reached the dribble, whether he still holds the ball, the clip, the series
  // A man in PES's Dribble action whose P+0xAE0 names a motion (the one the dribble tick's SetMotion issued) must have that motion on the
  // anime player's list (the table P+0x354, rebuilt each tick from the clip the body plays): the dribble's frame tests (iface +0x38 =
  // FUN_1406E68C0, the contact test FUN_1406EC260, the leave test FUN_1407B9280) all read the motion's frame from that table and answer 0
  // for a motion that is not on it. A tick of such a man with his motion off the table is the body not playing what PES issued.
  long pesDribbleTicks = 0;
  long worstLastFrameHold = 0;  // the longest run of ticks a PES-owned dribbler's body sat on the last frame of its clip
  unsigned dribbleScans = 0, dribbleGrants = 0, dribblePending = 0;  // body scans of PES-owned dribblers, granted ones, ones that ran with contact pending
  unsigned dribbleKicks = 0;                         // dribbler touches the ball executor applied to the ball (Match::PesDribbleKicks)
  unsigned dribbleIssues = 0;                        // the dribble tick's SetMotion issues (Match::PesDribbleIssues)
  std::map<int, unsigned> unplayedDribbleIssues;   // those the body could not play, by motion
  long dribbleMotionNotOnTable = 0;
  // Attribution of the off-the-list ticks (the series freeze is AnimeLayer's SetParametric port, a separate defect): a tick counts as the series freeze when
  // the man's P holds a series (count > 0) and his body sits on the last frame of a clip the dribble family owns. The rest, by what the body plays.
  long notOnTableSeriesFrozen = 0;
  std::map<std::string, long> notOnTableOther;
  std::vector<std::pair<long, std::string>> ownershipStreaks;  // (length, how it began and ended), closed streaks
  // A PES-owned dribbler and a ball that lies still more than a touch away: PES's Dribble action steers the man onto the ball (the run-to point of every
  // real dribble frame is the ball) or hands the body back to MOVE; one that stays in the action, with the ball at rest out of reach, for five seconds
  // is the dribble holding a man who does nothing with it (the breaker's live firings).
  long strandedDribblers = 0;            // stretches of 5 s of that
  long strandedSeriesFrozen = 0;         // of them, the man's body on the last frame of a clip with a series held in P (the series freeze)
  long strandedTicks = 0;                // ticks in them
  std::string strandedSamples;
  long ownedTicksOffDribbleClip = 0;  // ticks of a PES-owned dribbler whose body clip is not a dribble-family clip
  std::map<std::string, long> dribbleClipExits;  // how a dribble clip left the body, and what took it
  // The first link of the Dribble action's chain: on the tick a man's P+0xAC8 becomes Dribble (the trap end's switch 0x1406EEBB0 ran the
  // act4 start 0x1407A38E0, whose SetMotion issued his first motion into P+0xAE0), the body must play that motion at the end of the tick.
  long dribbleEntries = 0;
  long dribbleEntriesNotPlayed = 0;
  std::string dribbleEntrySamples;
  long neverEngagedTrappers = 0;  // granted resting touches whose man was in PES's dribble at no tick of the window (the chain never ran)
  // PES's per-man counter block (FUN_1406F6790, 0x1406F67C4..) counts P+0x2814 one tick at a time up to 255; PES's own writers reset it
  // (0x1406EB639, 0x1406EB83E, 0x1406EE75F, in the switch / reset), so a man's byte is either 255, or one more than last tick's, or a reset
  // to a smaller value. A byte below 255 that did not move, or that moved by anything but one, is the counter not counting.
  long counterNotCounting = 0;
  // The strike adjuster A = P+0x428 that a Trap start arms (FUN_141EB39A0: mode byte A+0x6D = 1, state A+8 = 1; FUN_141EB3AE0 moves it to 2) walks the root
  // onto the ball's contact point one step a tick until its last tick A+0x10, when FUN_141EB3AF0's finish zeroes A+8..+0x1F. A man in the Trap action whose
  // adjuster has its mode set and steps left (A+0x10 > count) but whose state word reads 0 had it killed mid-run: the strike's walk stopped after its first step.
  long adjusterKilled = 0;
  std::map<int, uint32_t> adjusterPrevState_;
  std::string adjusterSamples;
  std::string counterSamples;  // the first violations: last->now@slot/action
  // Match::Process advances the clock at its end (match.cpp, `actualTime_ms += kTick_ms`), after the bodies stamped this tick's
  // touches, so a touch made on the tick that just ran carries the clock as it stood when the tick began.
  void OnStart(Match& match) override { tickStart_ms_ = match.GetActualTime_ms(); }
  void OnTick(Match& match) override {
    ++tickCount_;
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        const uint8_t byte = match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPTickCounterFirstOffset];
        uint16_t& last = lastCounter_.emplace(player.get(), kNoCounter).first->second;
        const bool upByOther = last != kNoCounter && byte != 0xFF && byte != last + 1 && byte > last;  // moved up by other than one
        // Below 255 and not moving. A man PES's switch resets every tick (the writers above) reads reset-then-count = 1 each tick, which is
        // the counter counting: only a byte stuck at another value (0 = the counter block never ran) is not.
        const bool stuck = last != kNoCounter && byte != 0xFF && byte == last && byte != 1;
        if (upByOther || stuck) {
          ++counterNotCounting;
          if (counterSamples.size() < 12) counterSamples += (upByOther ? " up " : " stuck ") + std::to_string(last) + "->" + std::to_string(byte) + "@" + std::to_string(match.PesSlotOf(player.get())) + "/ac8=" + std::to_string(match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPByteAC8Offset]);
        }
        last = byte;
      }
    }
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        const uint8_t* p = match.PesWorldOf().P(match.PesSlotOf(player.get()));
        const uint8_t* adj = p + pes::kPAdjusterOffset;
        int16_t count, ticks;
        std::memcpy(&count, adj + pes::kAdjCountOffset, sizeof count);
        std::memcpy(&ticks, adj + pes::kAdjTicksOffset, sizeof ticks);
        uint32_t state;
        std::memcpy(&state, adj + pes::kAdjStateOffset, sizeof state);
        // A running adjuster (state 2) is stepped by PES's anime tick every frame whatever the man's action, and ends through its own finish, which clears the mode
        // byte. A state word that reads 0 the next tick with the mode byte still 1 was zeroed by someone else mid-run. (A man admitted to the Trap action this
        // tick has not run his start yet: his adjuster is the previous trap's leftover, not a running one, so only a state seen running counts.)
        uint32_t& before = adjusterPrevState_[match.PesSlotOf(player.get())];
        if (before == pes::kAdjRunning && state == 0 && adj[pes::kAdjModeOffset] == 1) {
          ++adjusterKilled;
          if (adjusterSamples.size() < 160) adjusterSamples += " t" + std::to_string(tickCount_) + " " + std::to_string(match.PesSlotOf(player.get())) + (p[pes::kPByteAC8Offset] == pes::kAnimeActionTrap ? ":trap " : ":left the trap ") + std::to_string(count) + "/" + std::to_string(ticks);
        }
        before = state;
      }
    }
    {
      constexpr float kRestingBall_mps = 0.3f, kTouchReach_m = 0.6f;
      constexpr int kStrandedTicks = 500;  // 5 s at the 10 ms tick
      const bool resting = match.GetBall()->GetMovement().GetLength() < kRestingBall_mps;
      for (int team = 0; team < 2; ++team) {
        for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
          if (!player->IsActive()) continue;
          int& away = awayTicks_[player.get()];
          const float dist = (player->GetPosition() - match.GetBall()->Predict(0)).Get2D().GetLength();
          // A PES dribbler's body on the last frame of its clip: PES's layer loops the member, so GF's body must leave that frame on the next
          // tick. Held longer, it is the series freeze.
          if (match.PesOwnsDribble(*player)) {
            const Anim* clip = player->GetCurrentAnim();
            const bool lastFrame = clip && clip->anim && clip->frameNum >= clip->anim->GetFrameCount() - 1;
            long& held = lastFrameHold_[player.get()];
            held = lastFrame ? held + 1 : 0;
            worstLastFrameHold = std::max(worstLastFrameHold, held);
          }
          if (match.PesOwnsDribble(*player) && resting && dist > kTouchReach_m) {
            ++strandedTicks;
            if (++away == kStrandedTicks) {
              ++strandedDribblers;
              {
                uint32_t seriesCount;
                std::memcpy(&seriesCount, match.PesWorldOf().P(match.PesSlotOf(player.get())) + pes::kPSeriesCountOffset, sizeof seriesCount);
                const Anim* clip = player->GetCurrentAnim();
                if (seriesCount > 0 && clip && clip->anim && clip->frameNum >= clip->anim->GetFrameCount() - 1) ++strandedSeriesFrozen;
              }
              if (strandedSamples.size() < 160) strandedSamples += " " + std::string(player->GetPlayerData()->GetLastName()) + "@" + std::to_string(tickCount_) + "/" + std::to_string(dist).substr(0, 4) + "m";
            }
          } else {
            away = 0;
          }
        }
      }
    }
    const float restingSpeed_mps = 1.0f;
    const float speed = match.GetBall()->GetMovement().GetLength();
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        const int slot = match.PesSlotOf(player.get());
        const unsigned taken = match.PesTouchesTaken(slot);
        const bool granted = taken != taken_[slot];
        taken_[slot] = taken;
        const bool touched = player->GetLastTouchTime_ms() == tickStart_ms_;
        if (granted) {
          grants++;
          if (previousSpeed_ < restingSpeed_mps) {
            restingGrants++;
            waiting_[player.get()].restLeft = kDribbleEngageTicks;
          }
          if (!touched) grantsWithoutTouch++;
        } else if (touched && player->CastHumanoid()->OnPesTrap()) {
          touchesWithoutGrant++;
        }
      }
    }
    previousSpeed_ = speed;
    tickStart_ms_ = match.GetActualTime_ms();
    dribbleIssues = match.PesDribbleIssues();
    dribbleKicks = match.PesDribbleKicks();
    dribbleScans = match.PesDribbleScans();
    dribbleGrants = match.PesDribbleGrants();
    dribblePending = match.PesDribbleContactPending();
    unplayedDribbleIssues = match.PesDribbleIssuesUnplayed();
    for (int team = 0; team < 2; ++team) {
      for (const auto& player : match.GetTeam(team)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        {
          const uint8_t* p = match.PesWorldOf().P(match.PesSlotOf(player.get()));
          const uint16_t motion = match.PesDribbleIssuedMotion(match.PesSlotOf(player.get()));
          uint16_t& before = lastMotion_[player.get()];
          // A motion issued this tick reaches the list in the next tick's rebuild (the anime update follows the action tick): only a
          // motion that has been the man's for a tick is held to it.
          const bool settled = before == motion;
          before = motion;
          // Only a man PES owns in the Dribble action is a PES dribbler: one whose P reads Dribble because GF's clip derives it (a stand-up clip after a fall)
          // carries the issue of his earlier PES dribble, which no clip of his now plays (52 such ticks in the seed-7 match, all one man).
          if (settled && match.PesOwnsDribble(*player) && p[pes::kPByteAC8Offset] == pes::kAnimeActionDribble && motion != 0) {
            ++pesDribbleTicks;
            uint32_t count;
            std::memcpy(&count, p + pes::kPMotionCountOffset, sizeof count);
            bool listed = false;
            for (uint32_t k = 0; k < count && k < pes::kPMotionTableMax; ++k) {
              uint32_t id;
              std::memcpy(&id, p + pes::kPMotionTableOffset + k * pes::kPMotionEntryBytes, sizeof id);
              listed = listed || id == motion;
            }
            if (!listed) {
              ++dribbleMotionNotOnTable;
              const Anim* clip = player->GetCurrentAnim();
              uint32_t seriesCount;
              std::memcpy(&seriesCount, p + pes::kPSeriesCountOffset, sizeof seriesCount);
              const bool lastFrame = clip && clip->anim && clip->frameNum >= clip->anim->GetFrameCount() - 1;
              if (seriesCount > 0 && lastFrame) {
                ++notOnTableSeriesFrozen;
              } else {
                const int family = clip && clip->anim ? std::atoi(clip->anim->GetVariable("specialvar1").c_str()) : -1;
                const int bodyMotion = clip && clip->anim ? std::atoi(clip->anim->GetVariable("specialvar2").c_str()) : -1;
                ++notOnTableOther[std::string(match.PesOwnsDribble(*player) ? "PES-owned" : "NOT PES-owned (P says Dribble from GF's clip, the issue is stale)") + ", series " + (seriesCount > 0 ? "held" : "empty") + ", body family " + std::to_string(family) + (bodyMotion == int(motion) ? " plays the issued motion" : " plays another motion") + (lastFrame ? " (last frame)" : "")];
              }
            }
          }
        }
        {
          // Where the issued dribble clip goes: each change of a PES-owned dribbler's body clip is filed by how the old clip left (it ran to its
          // last frame, or something took the body earlier) and what took the body (family / function type).
          Player* pl = player.get();
          const Anim* clip = pl->GetCurrentAnim();
          {
            // The streaks of PES ownership: how each began (the body's clip and the trap tick's issue kind at the first owned tick) and how long it ran.
            Streak& st = streak_[pl];
            const bool owned = match.PesOwnsDribble(*pl);
            if (owned && st.start < 0) {
              st = {tickCount_, 0, clip && clip->anim ? std::atoi(clip->anim->GetVariable("specialvar1").c_str()) : -1,
                    clip && clip->anim ? std::atoi(clip->anim->GetVariable("specialvar2").c_str()) : -1,
                    int(match.PesTrapTickIssue(match.PesSlotOf(pl))), int(match.PesDribbleIssuedMotion(match.PesSlotOf(pl)))};
            } else if (!owned && st.start >= 0) {
              char line[200];
              std::snprintf(line, sizeof line, "%ld ticks (start %ld) began on clip %d/%d, trap issue %d, issued motion %d, ended with P action %d", tickCount_ - st.start,
                            st.start, st.family, st.motion, st.issue, st.issued, match.PesWorldOf().P(match.PesSlotOf(pl))[pes::kPByteAC8Offset]);
              ownershipStreaks.push_back({tickCount_ - st.start, line});
              st.start = -1;
            }
          }
          if (clip && clip->anim && match.PesOwnsDribble(*pl)) {
            const int family = std::atoi(clip->anim->GetVariable("specialvar1").c_str());
            auto& last = lastClip_[pl];
            if (last.id != clip->id || last.anim != clip->anim) {
              if (last.family == int(pes::PesClipFamily::kDribble)) {
                const bool ended = last.frame >= last.frames - 1;
                char key[96];
                std::snprintf(key, sizeof key, "dribble clip %s -> family %d fn %d", ended ? "ENDED" : "PREEMPTED", family, int(pl->GetCurrentFunctionType()));
                ++dribbleClipExits[key];
              }
            }
            last = {clip->id, clip->anim, family, clip->frameNum, clip->anim->GetFrameCount()};
            if (family != int(pes::PesClipFamily::kDribble)) {
              ++ownedTicksOffDribbleClip;
              const uint8_t* pp = match.PesWorldOf().P(match.PesSlotOf(pl));
              char key[120];
              std::snprintf(key, sizeof key, "off-dribble-clip tick: body family %d fn %d | P sub-state %02x | holder %d", family,
                            int(pl->GetCurrentFunctionType()), pp[pes::kPPhaseOffset], match.GetPesHolder() == pl);
              ++dribbleClipExits[key];
            }
          } else {
            lastClip_[pl] = {};
          }
        }
        {
          const uint8_t* pp = match.PesWorldOf().P(match.PesSlotOf(player.get()));
          uint8_t& before = lastAction_.emplace(player.get(), pp[pes::kPByteAC8Offset]).first->second;
          // Only the trap end's switch is PES's entry into Dribble here: a man GF derives into it (a BallControl clip of the carrier) runs no
          // act4 start.
          if (before == pes::kAnimeActionTrap && pp[pes::kPByteAC8Offset] == pes::kAnimeActionDribble) {
            ++dribbleEntries;
            const uint16_t motion = match.PesDribbleIssuedMotion(match.PesSlotOf(player.get()));  // the issue (series: its first entry)
            const Anim* clip = player->GetCurrentAnim();
            const bool played = clip && clip->anim &&
                                std::atoi(clip->anim->GetVariable("specialvar1").c_str()) == int(pes::PesClipFamily::kDribble) &&
                                std::atoi(clip->anim->GetVariable("specialvar2").c_str()) == motion;
            if (!played) {
              ++dribbleEntriesNotPlayed;
              if (dribbleEntrySamples.size() < 400)
                dribbleEntrySamples += " issue " + std::to_string(match.PesTrapTickIssue(match.PesSlotOf(player.get()))) + " seriesCount " +
                                       std::to_string(*reinterpret_cast<const uint32_t*>(pp + pes::kPSeriesCountOffset)) + " ae0 " +
                                       std::to_string(*reinterpret_cast<const uint16_t*>(pp + pes::kPWordAE0Offset)) + " motion " + std::to_string(motion) + " body " +
                                       (clip && clip->anim ? clip->anim->GetVariable("specialvar1") + "/" + clip->anim->GetVariable("specialvar2") : std::string("none")) + ";";
            }
          }
          before = pp[pes::kPByteAC8Offset];
        }
        auto& w = waiting_[player.get()];
        if (w.restLeft > 0 && match.PesWorldOf().P(match.PesSlotOf(player.get()))[pes::kPByteAC8Offset] == pes::kAnimeActionDribble)
          w.sawDribble = true;
        if (w.restLeft > 0 && --w.restLeft == 0) {
          const int slot = match.PesSlotOf(player.get());
          if (match.PesWorldOf().P(slot)[pes::kPByteAC8Offset] != pes::kAnimeActionDribble) {
            strandedTrappers++;
            const uint8_t* pp = match.PesWorldOf().P(slot);
            uint32_t seriesCount;
            std::memcpy(&seriesCount, pp + pes::kPSeriesCountOffset, sizeof seriesCount);
            const Anim* clip = player->GetCurrentAnim();
            strandedTrapperSamples += " [t" + std::to_string(tickCount_) + " " + std::string(player->GetPlayerData()->GetLastName()) + " ac8 " + std::to_string(pp[pes::kPByteAC8Offset]) + (w.sawDribble ? " left the dribble" : " never dribbled") +
                               (match.GetPesHolder() == player.get() ? ", holder" : ", not the holder") + ", series " + std::to_string(seriesCount) + ", clip " +
                               (clip && clip->anim ? clip->anim->GetName() : std::string("-")) + "]";
          }
          if (!w.sawDribble) neverEngagedTrappers++;
          w.sawDribble = false;
        }
      }
    }
  }

 private:
  struct Wait {
    long restLeft = 0;
    bool sawDribble = false;
  };
  std::map<const Player*, int> awayTicks_;
  std::map<const Player*, long> lastFrameHold_;
  std::map<const Player*, Wait> waiting_;
  static constexpr uint16_t kNoCounter = 0x100;
  std::map<const Player*, uint16_t> lastCounter_;
  std::map<const Player*, uint16_t> lastMotion_;
  std::map<const Player*, uint8_t> lastAction_;
  struct ClipSeen {
    int id = -1;
    const void* anim = nullptr;
    int family = -1, frame = 0, frames = 0;
  };
  std::map<const Player*, ClipSeen> lastClip_;
  struct Streak {
    long start = -1;
    long pad = 0;
    int family = -1, motion = -1, issue = 0, issued = 0;
  };
  std::map<const Player*, Streak> streak_;
  long tickCount_ = 0;
  std::map<int, unsigned> taken_;
  float previousSpeed_ = 0.0f;
  unsigned long tickStart_ms_ = 0;
};

TrapContactProbe contactCensus;

// The body's last-resort fallback (humanoid.cpp: "NO APPLICABLE ANIM FOUND", then ResetPosition) means the AI handed a man whose clip
// ended no command at all. It is never meant to happen in a match. The goal kick's strike (the keeper's root is carried onto the contact
// by the referee's strike, pesgoalkick.hpp) used to leave the keeper's queue empty from the release to the contact tick, so any
// keeper whose clip ended in that window hit the fallback.
class FallbackProbe : public pestest::Probe {
 public:
  FallbackProbe()
      : Probe("PesStallFallback", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long fallbacks = 0;
  void OnStart(Match&) override {
    connection_ = blunted::BindLog([this](blunted::e_LogType, std::string, std::string, std::string message) {
      if (message.find("NO APPLICABLE ANIM FOUND") != std::string::npos) fallbacks++;
    });
  }

 private:
  boost::signals2::scoped_connection connection_;
};

FallbackProbe fallbackCensus;

}  // namespace

TEST(PesStall, ATrapClipIsNeverRestartedBeforeItsContactFrame) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapClips, 0) << "no trap clip was played";
  EXPECT_EQ(trapCensus.restartedTraps, 0) << trapCensus.restartedTraps << " of " << trapCensus.trapClips
                                          << " trap clips restarted before their contact frame";
}

// PES's reaction entry 3 refuses the carrier (MA+0x2601C) unless he is in stagger, dodge or the keeper's drop (FUN_1409fa400 at
// 0x1409FA4AD..0x1409FA4D0), so a man who holds the ball is never handed a new trap: after his trap ends the action tick starts his
// dribble (0x1407A38E0) and he plays on. GF re-issued the trap for a holder every two or three ticks while the ball lay still.
TEST(PesStall, AHolderIsNeverHandedANewTrap) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapClips, 0) << "no trap clip was played";
  EXPECT_EQ(trapCensus.holderTraps, 0) << trapCensus.holderTraps << " of " << trapCensus.trapClips
                                       << " trap clips began for the man who already held the ball:" << trapCensus.holderSamples;
}

// PES's reaction gate 4a (the Trap action's slot +0x10, 0x14079BBE0) decides from the man's current action whether he may take a trap, and
// PES's action switch then replaces whatever he was doing: a trap PES starts is a trap the body plays. GF's body used to refuse 84 of 113
// starts (it was in a ball-control, slide or pass clip PlayIssuedClip would not interrupt), so the action tick ended the trap unplayed
// and the reaction re-issued it two ticks later.
TEST(PesStall, NoPesTrapStartIsRefusedByTheBody) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapStarts, 0) << "no PES trap started";
  EXPECT_EQ(trapCensus.refusedStarts, 0) << trapCensus.refusedStarts << " of " << trapCensus.trapStarts
                                         << " PES trap starts were refused by the body";
}

// Gate 4a refuses a man whose current action is not in 0x14079BBE0's list (a tackle, the keeper's catch and deflect, no action at all): the
// reaction never hands him a trap, so no trap start leaves one of those actions.
TEST(PesStall, NoTrapStartsFromAnActionGate4aRefuses) {
  pestest::Run(trapCensus);
  if (trapCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(trapCensus.setupError.empty()) << trapCensus.setupError;
  pestest::Settle(trapCensus);
  ASSERT_TRUE(trapCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(trapCensus.trapStarts, 0) << "no PES trap started";
  EXPECT_EQ(trapCensus.gateRefusedStarts, 0) << trapCensus.gateRefusedStarts << " of " << trapCensus.trapStarts
                                             << " PES trap starts left an action gate 4a does not admit";
}

// The referee arms a restart and releases it when the taker is ready. While it is armed the referee owns the
// ball; a breaker that forces a man over it is counting a hold PES does not stall on.
TEST(PesStall, TheBreakerNeverForcesABallUnderAnArmedRestart) {
  pestest::Run(census);
  if (census.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(census.setupError.empty()) << census.setupError;
  pestest::Settle(census);
  ASSERT_TRUE(census.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(census.armedFirings, 0) << "the breaker forced a man over a restart the referee had armed, "
                                       "for " << census.armedFirings << " ticks";
}

// OneWorld gate RATCHET (owner, 22-11-26): the live-ball breaker count lands at no more than the best measured on Integrate
// and only ever goes down; the goal stays 0. Measured on Integrate 39898dc3 (throw-in taker owns his body; first tip whose
// seed-7 match reaches full time): 2229 ticks in 160 episodes (full match), 53 in the quick window. Lower these in the same
// commit as any gain.
constexpr long kBestMeasuredLiveFirings = 2229;
constexpr long kBestMeasuredQuickLiveFirings = 53;

// A live ball that lies still for five seconds is a man who holds it and does nothing with it. PES's holder keeps
// the ball until he releases it, so the breaker must not have to take it from him.
TEST(PesStall, ALiveBallNeverLiesStillLongEnoughForTheBreaker) {
  pestest::Run(census);
  if (census.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(census.setupError.empty()) << census.setupError;
  pestest::Settle(census);
  ASSERT_TRUE(census.stop.gameOver) << "the match did not reach full time";
  if (census.episodeStart_ >= 0) census.liveEpisodes.push_back({census.episodeLast_ - census.episodeStart_ + 1, census.episodeLabel_});
  std::sort(census.liveEpisodes.rbegin(), census.liveEpisodes.rend());
  std::string episodes;
  for (std::size_t i = 0; i < census.liveEpisodes.size() && i < 25; ++i) episodes += "\n    " + std::to_string(census.liveEpisodes[i].first) + " ticks from " + census.liveEpisodes[i].second;
  // Printed on every run, pass or fail: the ratchet's number is the measurement every landing is judged by, so it is logged even
  // when it holds (a pass that printed nothing left lanes unable to say how much headroom a change used).
  std::printf("ALIVEBALL liveFirings=%ld episodes=%zu seriesFrozen=%ld duringFreeze=%ld bound=%ld\n", static_cast<long>(census.liveFirings),
              census.liveEpisodes.size(), static_cast<long>(census.liveFiringsSeriesFrozen), static_cast<long>(census.liveFiringsDuringFreeze),
              kBestMeasuredLiveFirings);
  EXPECT_LE(census.liveFirings, kBestMeasuredLiveFirings) << "the breaker forced a live ball for " << census.liveFirings << " ticks in " << census.liveEpisodes.size() << " episodes the series freeze does not explain (longest:" << episodes << "); " << census.liveFiringsSeriesFrozen
                                   << " of them on a PES dribbler whose body sat on the last frame of a clip with a series held (the series freeze), " << census.liveFiringsDuringFreeze
                                   << " more while another PES dribbler had been frozen so for a second; the rest, by the forced man:" << [&] {
                                        std::string o;
                                        for (const auto& [k, v] : census.liveFiringsOther) o += "\n    " + std::to_string(v) + " x " + k;
                                        return o;
                                      }();
}

// docs/re/pes-re-walker-touch.md round 2 (R-RED): PES's reaction entry 3 admits TRAP for a man in MOVE who is not the designated owner, 0.5 m from a
// ball at rest with no holder, with no approach-direction gate. A walk that ends with no trap start inside one second is that touch not made.
TEST(PesStall, ANonOwnerWalkerBesideARestingBallStartsATrapWithinASecond) {
  pestest::Run(census);
  if (census.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(census.setupError.empty()) << census.setupError;
  pestest::Settle(census);
  ASSERT_TRUE(census.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(census.walkUntouched, 0) << census.walkUntouched << " walks beside a resting ball ended with no trap start inside one second";
}

TEST(PesStallQuick, ALiveBallNeverLiesStillInTheQuickWindow) {
  pestest::Run(quickCensus);
  if (quickCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(quickCensus.setupError.empty()) << quickCensus.setupError;
  pestest::Settle(quickCensus);
  quickCensus.Print("PESSTALLQUICK");
  ASSERT_EQ(quickCensus.ticks, kQuickMaxTicks) << "the match ended before its quick window";
  EXPECT_LE(quickCensus.liveFirings, kBestMeasuredQuickLiveFirings) << "the breaker forced a live ball for " << quickCensus.liveFirings << " ticks in the first "
                                        << kQuickMinutes << " minute(s) of sim time";
}

TEST(PesStall, ANoManIsEverHandedNoCommandAtAClipEnd) {
  pestest::Run(fallbackCensus);
  if (fallbackCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(fallbackCensus.setupError.empty()) << fallbackCensus.setupError;
  pestest::Settle(fallbackCensus);
  ASSERT_TRUE(fallbackCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(fallbackCensus.fallbacks, 0) << fallbackCensus.fallbacks << " clip ends found no applicable anim";
}

// The trap's touch is PES's body contact scan, not the clip's contact frame: a man in a PES trap clip touches the ball on the tick the
// scan grants it and on no other, and a grant always touches. GF's own gate at the clip's frame (touchframe) has no PES source.
TEST(PesStall, ATrapTouchIsTheBodyScansGrantAndNothingElse) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.grants, 0) << "the body scan granted no touch in the whole match";
  EXPECT_EQ(contactCensus.touchesWithoutGrant, 0)
      << contactCensus.touchesWithoutGrant << " touches by a man in a trap clip with no grant of the body scan";
  EXPECT_EQ(contactCensus.grantsWithoutTouch, 0)
      << contactCensus.grantsWithoutTouch << " of " << contactCensus.grants << " grants did not touch the ball";
}

// PES's per-man counter block counts P+0x2814 up to 255 (FUN_1406F6790, 0x1406F67C4..0x1406F6806) and PES's switch resets it: every tick
// of every man on the pitch it is 255, one more than last tick, or a reset. The dribble chain reads the sibling P+0x2818 the same way.
TEST(PesStall, EveryManOnThePitchHasPesTickCountersCounting) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(contactCensus.counterNotCounting, 0) << contactCensus.counterNotCounting << " man-ticks with P+0x2814 below 255 that did not count by one:" << contactCensus.counterSamples;
}

// A resting ball a trapping man reaches is the case PES's reach test decides and GF's distance gate used to refuse (the loose-ball
// stalls: men 0.4 to 2.7 m from a ball at rest who never play it): at least one grant is a touch of a ball at rest.
// A Trap start's strike adjuster must run its steps to the contact point: it is PES's own state in P (P+0x428), written by the start and stepped by the
// anime tick, and GF's per-tick write of P must leave it alone while the tick owns it.
TEST(PesStall, ATrapStrikeAdjusterRunsItsStepsToTheContactFrame) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(contactCensus.adjusterKilled, 0) << contactCensus.adjusterKilled << " ticks of a running strike adjuster had its state word zeroed mid-run, by a man leaving the Trap action or otherwise (tick, slot, count/last tick:"
                                              << contactCensus.adjusterSamples << ")";
}

TEST(PesStall, APesDribblerGoesToABallAtRestOrLeavesTheDribble) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(contactCensus.strandedDribblers, 0) << contactCensus.strandedDribblers << " PES dribblers stayed in the Dribble action 5 s with the ball at rest out of reach, " << contactCensus.strandedSeriesFrozen << " of them frozen on a series clip's last frame (" << contactCensus.strandedTicks
                                                 << " ticks):" << contactCensus.strandedSamples;
}

TEST(PesStall, ATrapReachesABallAtRest) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_GT(contactCensus.restingGrants, 0) << "no trap touched a ball at rest in the whole match";
}
// A granted touch that leaves the ball at rest hands the man to PES's dribble: the post-touch chain (end handler 0x14079E5C0 ->
// switch 0x1406EEBB0 -> act4 start 0x1407A38E0) puts him in the Dribble action, which carries the ball. A trapper still in Trap 150
// ticks after his granted resting touch is stranded: the chain never ran and the ball lies still until the breaker fires.
TEST(PesStall, AGrantedRestingTouchStartsPesDribble) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.restingGrants, 0) << "no trap touched a ball at rest in the whole match";
  EXPECT_EQ(contactCensus.strandedTrappers, 0)
      << contactCensus.strandedTrappers << " of " << contactCensus.restingGrants
      << " granted resting touches left the trapper out of PES's dribble:" << contactCensus.strandedTrapperSamples;
}

// The cause side of the stranded count: a granted resting touch whose man was in PES's dribble at NO tick of the window is the post-touch
// chain never running. A man who reached the dribble and left it again (the holder gate FUN_1406EC5F0 went false because another man
// touched the ball, or the dribble's own exit) is PES's outcome, not a stall.
TEST(PesStall, AGrantedRestingTouchReachesPesDribbleWithinTheWindow) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.restingGrants, 0) << "no trap touched a ball at rest in the whole match";
  EXPECT_EQ(contactCensus.neverEngagedTrappers, 0)
      << contactCensus.neverEngagedTrappers << " of " << contactCensus.restingGrants
      << " granted resting touches never reached PES's dribble in the window";
}

// PES's dribble member loops: its record carries the loop flag (TrapContact10 section 14), so a PES dribbler's body is never left on the last
// frame of his clip for longer than the tick that reaches it. GF held that frame for 900+ ticks (the series freeze, WalkLane2 section 2).
TEST(PesStall, APesDribblerNeverHoldsHisClipsLastFrame) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.pesDribbleTicks, 0) << "no man was in PES's Dribble action with a motion issued in the whole match";
  EXPECT_LE(contactCensus.worstLastFrameHold, 1) << "a PES dribbler's body sat on the last frame of his clip for " << contactCensus.worstLastFrameHold << " ticks in a row";
}

// The first link of a PES dribbler's touch chain: the body plays the motion the Dribble action issued (SetMotion, P+0xAE0). Without it
// the dribble's frame tests read 0 and the contact-pending bit (0x1407A7085, FUN_1407A6F30) is never raised: no scan, no touch.
TEST(PesStall, APesDribblerPlaysTheMotionItIssued) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.pesDribbleTicks, 0) << "no man was in PES's Dribble action with a motion issued in the whole match";
  EXPECT_EQ(contactCensus.dribbleMotionNotOnTable, 0)
      << contactCensus.dribbleMotionNotOnTable << " of " << contactCensus.pesDribbleTicks
      << " PES-dribbler ticks had the issued motion off the anime player's list; ticks owned but on a non-dribble clip: "
      << contactCensus.ownedTicksOffDribbleClip << "; of them " << contactCensus.notOnTableSeriesFrozen << " with his body on the last frame of a series' clip (the series freeze), the others:" << [&] {
        std::string o;
        for (const auto& [k, v] : contactCensus.notOnTableOther) o += "\n    " + std::to_string(v) + " x " + k;
        return o;
      }() << "; streaks: " << contactCensus.ownershipStreaks.size() << [&] {
        auto v = contactCensus.ownershipStreaks;
        std::sort(v.rbegin(), v.rend());
        std::string o;
        for (std::size_t i = 0; i < v.size() && i < 12; ++i) o += "\n    " + v[i].second;
        return o;
      }() << "; " << [&] { std::string o; for (const auto& [k, v] : contactCensus.dribbleClipExits) o += "\n    " + std::to_string(v) + " x " + k; return o; }();
}

// Every SetMotion the Dribble action's tick makes reaches the body: PES's issue is the clip the man plays (RunPesDribbleTicks), so a motion
// the body could not play (no clip of the dribble family, or the body refused it) is PES's behaviour not reproduced. Killing the dribble
// runner leaves no issue at all, which the first assertion catches.
TEST(PesStall, EveryDribbleIssueIsPlayedOnTheBody) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.dribbleIssues, 0u) << "the Dribble action's tick issued no motion in the whole match";
  std::string unplayed;
  for (const auto& [motion, count] : contactCensus.unplayedDribbleIssues) unplayed += " " + std::to_string(motion) + "x" + std::to_string(count);
  EXPECT_TRUE(contactCensus.unplayedDribbleIssues.empty()) << "unplayable dribble issues (motion x count):" << unplayed;
}

// A man entering PES's Dribble action plays the motion the act4 start issued (the first SetMotion of the chain, run inside the trap end's
// switch), on the tick he enters it.
TEST(PesStall, AManEnteringPesDribbleBodyPlaysHisFirstMotion) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  ASSERT_GT(contactCensus.dribbleEntries, 0) << "no man entered PES's Dribble action in the whole match";
  EXPECT_EQ(contactCensus.dribbleEntriesNotPlayed, 0)
      << contactCensus.dribbleEntriesNotPlayed << " of " << contactCensus.dribbleEntries
      << " dribble entries left the body off the issued motion:" << contactCensus.dribbleEntrySamples;
}

// A PES dribbler's touch reaches the ball: the dribble's reach test grants it (the body scan), the ball executor's aggregator and the kick
// application's candidate builder answer the ball's new motion, and GF's ball takes it. Red while the touch stops at the scan's record.
TEST(PesStall, APesDribblerTouchMovesTheBall) {
  pestest::Run(contactCensus);
  if (contactCensus.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(contactCensus.setupError.empty()) << contactCensus.setupError;
  pestest::Settle(contactCensus);
  ASSERT_TRUE(contactCensus.stop.gameOver) << "the match did not reach full time";
  EXPECT_GT(contactCensus.dribbleKicks, 0u) << "no PES dribbler touch reached the ball in the whole match (scans " << contactCensus.dribbleScans << ", with contact pending "
      << contactCensus.dribblePending << ", granted " << contactCensus.dribbleGrants << ")";
}
namespace {
// PES's holder field MA+0x2601C is the W1 holder: the actor of the newest touch key, and nothing clears it on a loose ball (HolderRE
// section 2; 0x140457070 writes it, 0x140A627E0 reads it as the ball owner). GF writes this field from the ball retainer, which clears
// as soon as the ball is loose, so the trap-end admission (0x14079EE70 / 0x1406EC5F0) compares a trapper with "no owner". Every tick,
// both sides' field must hold the PES holder's slot (or the no-slot value when no holder exists).
class HolderWorldProbe : public pestest::Probe {
 public:
  HolderWorldProbe() : Probe("PesStallHolderWorld", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long ticks = 0;
  long mismatches = 0;  // side-ticks whose MA+0x2601C is not the PES holder's slot
  void OnTick(Match& match) override {
    ++ticks;
    const Player* holder = match.GetPesHolder();
    const uint32_t want = holder ? static_cast<uint32_t>(match.PesSlotOf(holder)) : pes::kMiNoSlot;
    for (int side = 0; side < 2; ++side) {
      if (pes::LoadU32(match.PesWorldOf().Ma(side), pes::kMaOwnerOffset) != want) mismatches++;
    }
  }
};
HolderWorldProbe holderWorld;
}  // namespace

TEST(PesStall, TheWorldHolderFieldIsThePesHolderEveryTick) {
  pestest::Run(holderWorld);
  if (holderWorld.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(holderWorld.setupError.empty()) << holderWorld.setupError;
  pestest::Settle(holderWorld);
  ASSERT_TRUE(holderWorld.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(holderWorld.mismatches, 0) << holderWorld.mismatches << " of " << 2 * holderWorld.ticks
                                       << " side-ticks had MA+0x2601C other than the PES holder";
}
