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

// The registry singleton DAT_1437F0AC8 (the object PES's string-assign FUN_141B55730 dispatches through: the DemoInfoRef writer's player names, and
// FUN_141B55F40's other readers). PES creates it in the engine init FUN_140401420 -> FUN_141B50400 -> FUN_141B55E70(config); that init also
// calls two protection thunks (thunk_EXT_FUN_15b23bce0, thunk_EXT_FUN_15b532330) and is not run. The creator is run (translated, proven on the
// real call live2/pid3319119 seq001) on a config built by meaning from the init's stores; returns the config block's VA.
// ponytail: the config is rebuilt from FUN_140401420's stores, not captured. Its two machine queries are fixed: the thread count (FUN_141B348C0
// (2, 0), clamped to 4: 4 on the capture) and the option flag (FUN_141B34850(0x14258ED10, 0): 0 on the capture, so flags bit 2 stays clear).
// Removed by running those two queries.
inline constexpr uint64_t kRegistryCellVa = 0x1437F0AC8;
inline constexpr uint64_t kRegistryConfigBytes = 0x88;   // FUN_140401420's local block (rsp+0x40 .. rsp+0xC8)
inline constexpr uint64_t kRegistryConfigCopyFrom = 0x28, kRegistryConfigCopyTo = 0x80;  // the creator's FUN_141B56170 copies these bytes to registry+0x50
uint64_t CreateRegistry(PesMemory& memory);

}  // namespace pes
