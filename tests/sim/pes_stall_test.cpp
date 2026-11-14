#include <gtest/gtest.h>

#include <algorithm>
#include <map>

#include "base/log.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/player/humanoid/humanoid.hpp"
#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/player/player.hpp"
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

// One whole PES match, with each breaker firing filed by the state of play on its tick. The match is the seed-7 match the
// restart-delay tests play, so the two families share one run (pes_shared_match.hpp).
class BreakerCensusProbe : public pestest::Probe {
 public:
  BreakerCensusProbe()
      : Probe("PesStall", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false, /*judgeEnd=*/false) {}
  long armedFirings = 0;    // the referee had a restart armed (its buffer is active)
  long liveFirings = 0;     // play is live, nothing is armed: a man on a ball that nobody plays
  long unarmedFirings = 0;  // play is off and nothing is armed
  void OnStart(Match& match) override { lastCount_ = match.GetStalemateCount(); }
  void OnTick(Match& match) override {
    const unsigned long count = match.GetStalemateCount();
    const bool fired = count != lastCount_;
    lastCount_ = count;
    if (!fired) return;
    if (match.GetRefereeBuffer().active)
      armedFirings++;
    else if (match.IsInPlay() && !match.IsInSetPiece())
      liveFirings++;
    else
      unarmedFirings++;
  }

 private:
  unsigned long lastCount_ = 0;
};

BreakerCensusProbe census;

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
// touched the ball this tick (his last-touch time is now).
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
  // A man in PES's Dribble action whose P+0xAE0 names a motion (the one the dribble tick's SetMotion issued) must have that motion on the
  // anime player's list (the table P+0x354, rebuilt each tick from the clip the body plays): the dribble's frame tests (iface +0x38 =
  // FUN_1406E68C0, the contact test FUN_1406EC260, the leave test FUN_1407B9280) all read the motion's frame from that table and answer 0
  // for a motion that is not on it. A tick of such a man with his motion off the table is the body not playing what PES issued.
  long pesDribbleTicks = 0;
  unsigned dribbleScans = 0, dribbleGrants = 0, dribblePending = 0;  // body scans of PES-owned dribblers, granted ones, ones that ran with contact pending
  unsigned dribbleKicks = 0;                         // dribbler touches the ball executor applied to the ball (Match::PesDribbleKicks)
  unsigned dribbleIssues = 0;                        // the dribble tick's SetMotion issues (Match::PesDribbleIssues)
  std::map<int, unsigned> unplayedDribbleIssues;   // those the body could not play, by motion
  long dribbleMotionNotOnTable = 0;
  std::vector<std::pair<long, std::string>> ownershipStreaks;  // (length, how it began and ended), closed streaks
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
          if (settled && p[pes::kPByteAC8Offset] == pes::kAnimeActionDribble && motion != 0) {
            ++pesDribbleTicks;
            uint32_t count;
            std::memcpy(&count, p + pes::kPMotionCountOffset, sizeof count);
            bool listed = false;
            for (uint32_t k = 0; k < count && k < pes::kPMotionTableMax; ++k) {
              uint32_t id;
              std::memcpy(&id, p + pes::kPMotionTableOffset + k * pes::kPMotionEntryBytes, sizeof id);
              listed = listed || id == motion;
            }
            if (!listed) ++dribbleMotionNotOnTable;
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
          if (match.PesWorldOf().P(slot)[pes::kPByteAC8Offset] != pes::kAnimeActionDribble) strandedTrappers++;
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

// A live ball that lies still for five seconds is a man who holds it and does nothing with it. PES's holder keeps
// the ball until he releases it, so the breaker must not have to take it from him.
TEST(PesStall, ALiveBallNeverLiesStillLongEnoughForTheBreaker) {
  pestest::Run(census);
  if (census.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(census.setupError.empty()) << census.setupError;
  pestest::Settle(census);
  ASSERT_TRUE(census.stop.gameOver) << "the match did not reach full time";
  EXPECT_EQ(census.liveFirings, 0) << "the breaker forced a live ball for " << census.liveFirings << " ticks";
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
      << " granted resting touches left the trapper out of PES's dribble";
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
      << contactCensus.ownedTicksOffDribbleClip << "; streaks: " << contactCensus.ownershipStreaks.size() << [&] {
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
