#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/player.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

// Diagnostic probe (temporary): runs seed 7 / 5 min / pes_loop and prints the referee buffer,
// the taker's distance to the ball, and his command/function each tick once a throw-in is armed.
// Deleted before the fix lands.
TEST(RestartProbe, ThrowInTrace) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql"));
  SetDB(&db);

  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = 1;
  setup->awayTeamDbId = 2;
  setup->seed = 7;
  setup->durationMinutes = 5;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(1, 2), *setup, config);
  constexpr unsigned long kCap = 30ul * 60ul * Match::kFramesPerSecond;
  unsigned long tick = 0;
  bool traced = false;
  while (!match.IsGameOver() && tick < kCap) {
    match.Process();
    ++tick;
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (!b.active || b.desiredSetPiece != e_SetPiece_FreeKick || !b.taker) continue;
    if (tick < 27900 || tick > 28700) continue;
    const Vector3 ball = match.GetBall()->Predict(0);
    Player* tk = b.taker;
    const Vector3 pos = tk->GetPosition();
    const Vector3 toBall = (ball - pos).Get2D();
    const Anim* a = tk->GetCurrentAnim();
    const PlayerCommand& oc = a->originatingCommand;
    const std::string fam = a->anim ? a->anim->GetVariable("specialvar1") : "";
    std::printf(
        "[probe] t=%lu inPlay=%d setPiece=%d started=%d dist=%.2f pos=(%.2f,%.2f) "
        "toBall=(%.2f,%.2f) mv=(%.2f,%.2f) animId=%d fn=%d fam=%s frame=%d ocFn=%d "
        "ocDir=(%.2f,%.2f) ocVel=%.2f ocLook=%d(%.2f,%.2f) touchPend=%d\n",
        tick, (int)match.IsInPlay(), (int)match.IsInSetPiece(), (int)b.started,
        toBall.GetLength(), pos.coords[0], pos.coords[1], toBall.coords[0], toBall.coords[1],
        tk->GetMovement().coords[0], tk->GetMovement().coords[1], a->id, (int)a->functionType,
        fam.c_str(), a->frameNum, (int)oc.desiredFunctionType, oc.desiredDirection.coords[0],
        oc.desiredDirection.coords[1], oc.desiredVelocityFloat, (int)oc.useDesiredLookAt,
        oc.desiredLookAt.coords[0], oc.desiredLookAt.coords[1], (int)tk->TouchPending());
    if (tick > 28700) break;
  }
  SUCCEED();
}
