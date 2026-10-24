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

// 0x140439F50
void pes_fn_140439F50(PesState& s);
// 0x140439F90
void pes_fn_140439F90(PesState& s);
// 0x140439FE0
void pes_fn_140439FE0(PesState& s);
// 0x14043A020
void pes_fn_14043A020(PesState& s);
// 0x14043A0A0
void pes_fn_14043A0A0(PesState& s);
// 0x1404C8DC0
void pes_fn_1404C8DC0(PesState& s);
// 0x1404AB8E0
void pes_fn_1404AB8E0(PesState& s);
// 0x14063F230
void pes_fn_14063F230(PesState& s);
// 0x1408C5000
void pes_fn_1408C5000(PesState& s);
