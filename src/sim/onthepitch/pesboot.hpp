// PES's boot on a PesMemory (OneWorld cut 1): the first frame of the game's start-up, run on the dump's image. The image GF maps is a dump of
// the running game, so its singletons hold that game's heap pointers. The boot clears them and builds the same objects in the memory's own
// heap with PES's own translated constructors.
#pragma once

#include <cstdint>
#include <string>

#include "onthepitch/pesmemory.hpp"

namespace pes {

// Runs the boot on `memory`, in PES's order: the zeroed data and the initialised data (the install's exe at installRoot supplies the
// bytes), the static guards, the scratch arena, malloc_init_hard, the C++ static initializers, the file layer's managers (the file manager
// FUN_14149A620, the file system FUN_1414A3550, the request managers FUN_1414B08A0 / FUN_1414B0E90, the pump's lists FUN_1402223C0 /
// FUN_140222390), the scripting init and the anime engine init FUN_141AE77B0 (the ResManager FUN_141B02800 and the type loaders), called as
// the game's boot does (seq001: rcx 0, rdx 1, r8 0, r9 0). Returns how many static initializers ran.
uint64_t PesBoot(PesMemory& memory, const std::string& installRoot);

// Two of the boot's steps, exposed for the tests. PesBoot runs them first, in this order (ZeroInitializedData is the first step too).
// RestoreInitialData: the constant-initialized statics whose dump bytes are runtime state come back from the install's own exe.
void RestoreInitialData(PesMemory& memory, const std::string& installRoot);
// ResetStaticInitState: every thread-safe static's guard is 0 and the global epoch is INT_MIN, as at process start.
void ResetStaticInitState(PesMemory& memory);

}  // namespace pes
