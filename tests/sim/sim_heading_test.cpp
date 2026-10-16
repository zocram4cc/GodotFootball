// A player's heading is flat. GetDirectionVec() is the fallback every controller
// normalises against, and Humanoid::SelectAnim asserts a movement command's
// desiredDirection has no height, so one height in either tilts the whole chain.
#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "pes_match_constants.hpp"

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/player/controller/playercontroller.hpp"
#include "onthepitch/player/humanoid/humanoid.hpp"
#include "onthepitch/velocitystate.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 1000;
constexpr int MATCH_MINUTES = 5;
constexpr float kPaceMps = walkVelocity;  // a walking movement across the pitch
constexpr float kLiftMps = 1.0f;          // a height in the movement

const pesrig::Rig* SharedRig() {
  static std::optional<pesrig::Rig> rig;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return nullptr;
    pes::PesVfs vfs;
    std::string err;
    if (vfs.Mount(root, &err)) rig = pesrig::BuildRig(vfs, &err);
  }
  return rig ? &*rig : nullptr;
}

// The fixture match, with the player skeleton and the match constants read from the
// PES install. `match` stays null without one, and the test skips.
struct FixtureMatch {
  blunted::Database db;
  std::unique_ptr<Match> match;

  FixtureMatch() {
    if (!db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return;
    SetDB(&db);
    MatchSetup setup;
    setup.homeTeamDbId = HOME_TEAM;
    setup.awayTeamDbId = AWAY_TEAM;
    setup.seed = SEED;
    setup.durationMinutes = MATCH_MINUTES;
    setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
    setup.rig = SharedRig();
    if (!setup.rig || !pestest::FillMissingPesConstants(&setup)) return;
    match.reset(new Match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties()));
  }
  ~FixtureMatch() {
    match.reset();
    SetDB(nullptr);
  }
};

// A humanoid whose movement is set by hand. A movement smuggle is the part of an anim's
// displacement that the physics movement subtracts, so a height in it becomes a height in
// spatialState.movement.
class HeadingProbe : public Humanoid {
public:
  HeadingProbe(Player* player, boost::intrusive_ptr<Node> source,
               std::shared_ptr<AnimCollection> anims)
      : Humanoid(player, source, anims) {
    currentAnim->anim = anims->GetAnim(GetIdleMovementAnimID());
  }
  void SetMovementSmuggle(const Vector3& smuggle) { spatialState.movementSmuggleMovement = smuggle; }
  void Recompute() { CalculateSpatialState(); }
};

// A controller's movement command, asked for by hand. The controller's per-tick state is
// read from the match's mental image, so the match must have ticked once.
class MovementProbe : public PlayerController {
public:
  explicit MovementProbe(Match* match) : PlayerController(match) {}
  void RequestCommand(PlayerCommandQueue&) override {}
  Vector3 GetDirection() override { return inputDirection; }
  float GetFloatVelocity() override { return inputVelocityFloat; }
  void Attach(PlayerBase* newPlayer) {
    SetPlayer(newPlayer);
    _mentalImage = match->GetMentalImage(0);
    _Preprocess();
  }
  PlayerCommandQueue MoveToward(const Vector3& input) {
    inputDirection = input;
    inputVelocityFloat = walkVelocity;
    PlayerCommandQueue queue;
    _MovementCommand(queue);
    return queue;
  }
};

}  // namespace

TEST(SimHeading, DirectionVecIsFlatWhenTheMovementCarriesHeight) {
  FixtureMatch fixture;
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES";
  Player* player = fixture.match->GetPlayer(1);
  HeadingProbe probe(player, player->GetHumanoidNode(), fixture.match->GetAnimCollection());

  probe.Recompute();  // the first derivation settles the previous position
  probe.SetMovementSmuggle(Vector3(0, kPaceMps, -kLiftMps));  // movement: pace forward, lift
  probe.Recompute();

  ASSERT_NE(probe.GetEnumVelocity(), e_Velocity_Idle);  // the heading branch, not the body fallback
  EXPECT_EQ(probe.GetDirectionVec().coords[2], 0.0f);
  EXPECT_NEAR(probe.GetDirectionVec().coords[1], -1.0f, 1e-5f);
}

TEST(SimHeading, DesiredDirectionIsFlatWhenTheInputCarriesHeight) {
  FixtureMatch fixture;
  if (!fixture.match) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES";
  fixture.match->Process();
  MovementProbe probe(fixture.match.get());
  probe.Attach(fixture.match->GetPlayer(1));

  PlayerCommandQueue queue = probe.MoveToward(Vector3(1, 0, kLiftMps));

  ASSERT_EQ(queue.size(), 1u);
  EXPECT_EQ(queue.front().desiredDirection.coords[2], 0.0f);
}
