// pespilot.hpp: GF-side declarations for cut-over translated functions.
//
// Each pes_fn_<va> is GENERATED at build time from the user's image by
// tools/dev/translate/xlate.py into the build tree (never committed) and
// linked into gfsim. The declarations here name VAs only (addresses, not
// PES bytes), so this file is safe to commit. PesState comes from the
// committed tools/dev/translate/pes_state.hpp (the machine definition).
#pragma once

#include <cstdint>

struct PesState;

// 0x140A71990: lane z: XMM0 = XMM0 - 2 - 2*sext(DL). Entry XMM0 = F1
// (field half width), DL = lane k. Returns lane z in XMM0 lane 0.
// The TU defines ::pes_fn_<va>; GF calls the global name (the TU knows
// no C++ namespaces).
void pes_fn_140A71990(PesState& s);
