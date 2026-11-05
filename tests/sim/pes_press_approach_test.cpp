#include <gtest/gtest.h>

#include "pes_match_constants.hpp"
#include "pes_shared_match.hpp"

#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "gamedefines.hpp"
#include "gametypes.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
// The two-minute match with its stoppages, counted in the 54 Hz logic frames the sim ticks in, with headroom.
constexpr unsigned long kMaxTicks = 6ul * 60ul * Match::kFramesPerSecond;

// The seed-7 match, shared with the holder and restart tests (pes_shared_match.hpp). The callbacks only observe.
struct PressApproachState {
  unsigned long steered = 0, unnamed = 0;
};
PressApproachState pressApproach;
pestest::CallbackProbe pressApproachProbe(
    "ALoosePresserCommandNamesTheActionPressTarget", {true, kMinutes, kSeed}, kMaxTicks, /*stallStops=*/false,
    /*judgeEnd=*/false,
    [](Match& match) {
      const bool loose = match.IsInPlay() && !match.IsInSetPiece() && !match.GetBallRetainer() &&
                         match.GetBall()->GetMovement().GetLength() <= GameplayTuning::kStalemateBallSpeed_ms;
      if (!loose) return;
      for (int t = 0; t < 2; ++t) {
        Team* team = match.GetTeam(t);
        Player* presser = team->GetController()->GetLoosePressMan();
        if (!presser || presser->HasPossession() || team->PassGetReceiver() == presser) continue;
        const std::optional<PlayerCommand>& command = presser->GetPesDecision().command;
        if (!command || command->desiredFunctionType != e_FunctionType_Movement) continue;
        ++pressApproach.steered;
        if (!command->useDesiredTarget) ++pressApproach.unnamed;
      }
    },
    nullptr, /*shared=*/true);

}  // namespace

// PES's ActionPress steers the loose-ball presser to its target, the ball now or his own base (docs/re/pes-re-press-life.md
// section 8, step 6). His movement command must name that target: a command without one projects its direction a second
// ahead (pesmovestep FillCommandWords), which overshoots a ball a few metres away. Scope: a presser who does not hold the ball
// and is not the PASS_GET receiver (ActionPassGet's approach is another PES action, not this port's), with a movement decision.
TEST(PesPressApproach, ALoosePresserCommandNamesTheActionPressTarget) {
  pestest::Run(pressApproachProbe);
  if (pressApproachProbe.skipped) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(pressApproachProbe.setupError.empty()) << pressApproachProbe.setupError;
  pestest::Settle(pressApproachProbe);
  EXPECT_GT(pressApproach.steered, 0ul) << "no loose presser was steered in the match";
  EXPECT_EQ(pressApproach.unnamed, 0ul) << pressApproach.unnamed << " of " << pressApproach.steered
                                        << " loose-presser movement commands named no target";
}
