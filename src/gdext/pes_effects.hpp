#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "gdext/pes_library.hpp"

namespace godot {

// One stadium's effect entries (docs/re/pes-re-effects-particles.md 1.3, 2.1), read from the install.
class PesEffects : public Object {
  GDCLASS(PesEffects, Object)

 public:
  // weather: 0 fine, 1 rain, 2 snow (snow loads the rain pack, as PesLighting does). time_zone: 0 day, 1 night.
  // Returns {placed: [{type, setting}], unplaced: [create], orphans: [create]}: placed creates are the ones the
  // scene draws, unplaced are the rest, orphans name a setting the pack lacks. A create is {type, setting (index),
  // attributes}. An empty Dictionary when the pack cannot be read (the error is pushed).
  static Dictionary placement(const Ref<PesLibrary>& library, int stadium_id, int weather, int time_zone);

 protected:
  static void _bind_methods();
};

}  // namespace godot
