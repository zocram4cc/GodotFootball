#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <memory>
#include <vector>

#include "gdext/pes_library.hpp"

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
              int seed, int minutes);
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
  Vector3 ball_position() const;
  int match_time_ms() const;
  int score(int team) const;
  double field_half_width() const;   // to the touchline's outer edge (PES FieldInfo +0x08)
  double field_half_length() const;  // to the goal line's outer edge (PES FieldInfo +0x04)

 protected:
  static void _bind_methods();

 private:
  void RefreshPlayers();
  Ref<PesLibrary> library_;  // keeps the rig the match points at alive
  std::unique_ptr<blunted::Database> db_;
  std::unique_ptr<Match> match_;
  std::vector<Player*> players_;  // home then away, active players only
  std::vector<int> teamOf_;
  // The previous logic frame's pose, for the render side's interpolation.
  std::vector<blunted::Vector3> previousPlayers_;
  blunted::Vector3 previousBall_;
  bool previousValid_ = false;
};

}  // namespace godot
