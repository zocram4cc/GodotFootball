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

// Team sheet readers, run on S = T+0x8e08 (rcx = S, rdx = k, r8 = plan); the result is in eax.
void pes_fn_1408C5030(PesState& s);  // Toggle
void pes_fn_1408C5060(PesState& s);  // Taker
void pes_fn_1408C5080(PesState& s);  // Designated
void pes_fn_1408C52E0(PesState& s);  // Slider
