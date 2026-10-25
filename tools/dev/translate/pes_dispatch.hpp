// pes_dispatch.hpp: runtime dispatch for translated indirect calls (build
// tree only; never committed). The table maps PES code VAs to translated
// functions; it is generated from the set of translated functions in the
// build (gen_dispatch.py), with no hand edits. A target missing from the
// table fails loudly (PES_DISPATCH_UNTRANSLATED), never silently.
#pragma once
#include <cstdint>

struct PesState;

struct PesDispatch {
  void (*fn)(PesState&);
};

PesDispatch pes_dispatch_lookup(uint64_t t);
[[noreturn]] void pes_dispatch_missing(uint64_t t);

// The table itself, generated per build (gen_dispatch.py) into the same
// directory (on the include path in every build).
#include "pes_dispatch_gen.hpp"
