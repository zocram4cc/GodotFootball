#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>
#include <optional>
#include <vector>

#include "gdext/pes_library.hpp"
#include "onthepitch/pesrandom.hpp"
#include "pes/cutscene_timeline.hpp"
#include "pes/fixdemo_select.hpp"
#include "pes/fixdemo_table.hpp"

class Match;
class Player;
namespace blunted {
class Database;
}

namespace godot {

// The GameplayFootball match engine (src/sim), ticked from Godot. Positions
// are reported in Fox/Godot space: Y up, metres (GF is Z up: (x, y, z) ->
// (x, z, -y)).
class GfMatch : public RefCounted {
  GDCLASS(GfMatch, RefCounted)

 public:
  GfMatch();
  ~GfMatch() override;

  Error start(const Ref<PesLibrary>& library, const String& db_sql, const String& anims_dir, int home, int away,
              int seed, int minutes, bool pes_loop);
  void step();  // one 1/54 s PES logic frame
  bool is_over() const;

  int player_count() const;
  Vector3 player_position(int index) const;
  // The positions at the previous logic frame, so the render side interpolates
  // between the two frames a render frame straddles (Milestone A1: the sim steps
  // 1/54 s, which is no longer every render frame's worth of time).
  Vector3 player_position_previous(int index) const;
  Vector3 ball_position_previous() const;
  Vector3 player_facing(int index) const;    // body direction, horizontal unit vector
  Vector3 player_velocity(int index) const;  // m/s
  int player_team(int index) const;          // 0 home, 1 away
  int player_database_id(int index) const;
  // The clip the MOVESTEP machine last issued for a player: its motion, group (PES blend group,
  // -1 for a single clip), mirror, start time and rate, blend duration, playback, and serial (each
  // issue is a new clip). Empty for an index out of range.
  Dictionary player_clip(int index) const;
  Vector3 ball_position() const;
  int match_time_ms() const;
  int score(int team) const;
  double field_half_width() const;   // to the touchline's outer edge (PES FieldInfo +0x08)
  double field_half_length() const;  // to the goal line's outer edge (PES FieldInfo +0x04)

  // The FixDemo environment words of the ctx (stadium id, time of day, weather); call after start().
  void set_cutscene_environment(int stadium, int time_of_day, int weather);
  // The cutscene PES's selection makes for the goal or foul stop GF just played, then cleared: {scene, category, steps:
  // [{cut, row, fdc: PackedStringArray}], anchor: {pos: Vector3, rot_y: degrees, flip: bool}} (anchor: goals only, the
  // placement PES's scene anchor gives the scene's player: PesCutsceneCamera.set_stage_default). Empty when none is pending.
  Dictionary take_cutscene();
  // The scene in force on PES's timeline (pes::CutsceneTimeline, one Tick per logic frame): {scene, step: the step whose
  // player has started (-1 before the first, the last stays in force until the scene ends), step_started: the step whose first
  // update was this frame (-1 otherwise), clock_prev_s and clock_s: that step's player clock before and after this frame, in
  // seconds (cameras play on it), ended: the scene's list ran out this frame (the category's rotation word advanced,
  // 0x141F58FC0)}. Empty when no scene is in force.
  Dictionary cutscene_state() const;

 protected:
  static void _bind_methods();

 private:
  void RefreshPlayers();
  void PollCutscenes();
  std::optional<pes::GoalAnchor> PlaceGoalScene(const Vector3& event);
  void StartTimeline(const pes::FixDemoRequest& request);
  // PES's gate function for the scene in force (goal 0x140472640, foul 0x14047EB00) on this frame's players; true once open.
  bool RunGate();
  // PES's slot of a player on the pitch (side * 11 + his index among his side's players, the squad key GF's arena uses); -1 when
  // he is not in the active set.
  int PesSlot(Player* p) const;
  Ref<PesLibrary> library_;  // keeps the rig the match points at alive
  std::unique_ptr<blunted::Database> db_;
  std::unique_ptr<Match> match_;
  std::vector<Player*> players_;  // home then away, active players only
  std::vector<int> teamOf_;
  // The previous logic frame's pose, for the render side's interpolation.
  std::vector<blunted::Vector3> previousPlayers_;
  blunted::Vector3 previousBall_;
  bool previousValid_ = false;
  // PES's FixDemo selection on GF memory, fed by the goal and foul stops the poll sees.
  std::unique_ptr<pes::FixDemoSelector> fixdemo_;
  uint32_t previousGoalPick_ = 0;  // A0+0x13C: the scene the previous goal's DemoInfo picked (the builder's repeat filter)
  std::unique_ptr<pes::Random> goalRng_;
  pes::FixDemoFacts cutsceneEnvironment_;
  std::optional<pes::FixDemoRequest> pendingCutscene_;
  std::optional<pes::GoalAnchor> pendingAnchor_;  // the goal request's scene anchor (PES's stage default of the first step)
  int lastScore_[2] = {0, 0};
  unsigned long lastFoulStop_ms_ = 0;
  bool foulStopSeen_ = false;
  // The scene's timeline: built when the request is made, ticked from the frame DemoInfo exists.
  std::optional<pes::CutsceneTimeline> timeline_;
  uint32_t timelineScene_ = 0;
  float timelineSixty_ = 60.0f;  // PoseConstants::sixty, to turn clock ticks into seconds
  float clockPrevTicks_ = 0;
  uint32_t tick_ = 0, timelineStartTick_ = 0;
  // The gate of DemoInfo state 2: whose facts feed PES's gate function, and whether it has opened (it latches).
  Player* gateScorer_ = nullptr;                 // goal: the scorer the run object tracks
  Player* gateFouler_ = nullptr;                 // foul: the fouler and the fouled man
  Player* gateVictim_ = nullptr;
  bool gateIsGoal_ = false, gateOpen_ = false;
};

}  // namespace godot
