#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>
#include <optional>
#include <vector>

#include "gdext/pes_library.hpp"
#include "onthepitch/pesrandom.hpp"
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
  // The render side finished the scene (0x141F58FC0): the category's rotation word advances.
  void cutscene_finished(int scene_id);

 protected:
  static void _bind_methods();

 private:
  void RefreshPlayers();
  void PollCutscenes();
  std::optional<pes::GoalAnchor> PlaceGoalScene(uint32_t sceneId, const Vector3& event);
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
  std::optional<pes::FixDemoTable> goalTable_;
  std::unique_ptr<pes::Random> goalRng_;
  pes::FixDemoFacts cutsceneEnvironment_;
  std::optional<pes::FixDemoRequest> pendingCutscene_;
  std::optional<pes::GoalAnchor> pendingAnchor_;  // the goal request's scene anchor (PES's stage default of the first step)
  int lastScore_[2] = {0, 0};
  unsigned long lastFoulStop_ms_ = 0;
  bool foulStopSeen_ = false;
};

}  // namespace godot
