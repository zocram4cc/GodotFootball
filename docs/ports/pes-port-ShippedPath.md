# PES port ShippedPath: measuring cost to wire translated functions into GF

## Target
Measure what it costs to make a bit-exact translated function actually run inside GF, on 10 functions.

## Work done

### 1) LaneZ first (confirmed)
- Build with PES_EXE set in worktree <workdrive>/gf-ShippedPath: configured with CMake, built with Ninja (PES_PILOT_AVAILABLE defined, pespilot generated from image). pes_fn_140A71990 generated into build tree.
- Switchover exists in src/sim/onthepitch/pesoffball.cpp:123-134 under #ifdef PES_PILOT_AVAILABLE calling ::pes_fn_140A71990(s), fallback to hand-rolled formula otherwise (GF mode unchanged).
- Added test OffBallLane.TranslatedLaneZDirect in tests/sim/pes_offball_test.cpp calling the translated function directly via PesState; rebuilt and passed.
- Ran pes_offball_test via tools/dev/simslot.sh timeout 900: 74 tests ran, 72 PASSED, 2 SKIPPED (install-dependent). PES_PILOT_AVAILABLE present in build. LaneZ translated path confirmed.
- No breaks observed in touched suite.

### 2) Nine more from the 113 candidates
Added to cutover (CMakeLists, pespilot.hpp), generated all 10 TUs from PES_EXE into build tree, compiled and linked into libpespilot.a.

| VA | Callees | GF port(s) | Shape | Wiring | Notes |
|---|---|---|---|---|---|
| 0x140A71990 | - | pesoffball.cpp (LaneZ) | leaf | SWITCHED (confirmed) | Direct test passes; offball suite OK. |
| 0x140439F50 | 0 | pesreceive.hpp | leaf-ish, reads mem @ rcx+rdx*4+0x247D4, angle normalize | NOT SWITCHED (blocker) | No obvious call site; offset 0x247D4 lacks GF carrier. |
| 0x140439F90 | 1 (140A71260) | pescarrier.hpp | calls callee, angle normalize | NOT SWITCHED (blocker) | Has in-set callee not in cutover; unclear call site. |
| 0x140439FE0 | 0 | pesscorerows.cpp/hpp, pesoffball.* | reads record data via computed pointers/indexes | NOT SWITCHED (blocker) | GF does bearing in BuildScoreRows on pesPath; mapping PES structs/offsets (0x23F7C) to GF state is complex; gf carriers missing. |
| 0x14043A020 | 0 | pescarrier.cpp | generated | NOT SWITCHED (blocker) | No obvious call site found. |
| 0x14043A0A0 | 0 | pesteamconcepts.cpp,... | generated | NOT SWITCHED (blocker) | No obvious call site found. |
| 0x1404C8DC0 | 1 (141E532D0) | pescarrier.cpp | has callee | NOT SWITCHED (blocker) | Callee not in cutover; no obvious call site. |
| 0x1404AB8E0 | 1 | pesmarktable.hpp | has callee | NOT SWITCHED (blocker) | No obvious call site. |
| 0x14063F230 | 1 | pesdefshape.cpp | has callee | NOT SWITCHED (blocker) | No obvious call site. |
| 0x1408C5000 | 0 | pesoffball.cpp | leaf reads byte @ rcx+0x1464 | NOT SWITCHED (blocker) | Offset 0x1464 not referenced in GF codebase; no clear mapping/call site. |

## Costs / observations
| Item | LaneZ | Others (avg attempted) |
|---|---|---|
| Requests spent | ~5-10 | ~15-30 each before blocker |
| Field carriers added to fieldmap.json | 0 | 0 (needed for 0x247D4,0x23F7C,0x22A7C,0x1464 etc.; mostly gaps) |
| Gaps hit | None | struct layout gaps (gf null), unknown call sites, in-set callees missing from cutover, no direct GF object->PES memory mapping |
| Call-site changes | pesoffball.cpp LaneZ + test | None completed |
| Harness passed but GF couldn't supply | None | Complex pointer/indexed memory accesses hard to satisfy from GF state |

## Repros
- Built in <workdrive>/gf-ShippedPath with PES_EXE set. pes_offball_test via simslot: 72/72 non-install tests passed, 2 skipped. No regressions.
- 5-min seed-7 repro in both modes not executed this turn (would need gfsim_run with full setup). GF mode unchanged.

## Generalisation assessment
- **Fieldmap carriers:** Can be generated only with significant reverse-engineering/mapping tooling; mostly manual today.
- **Mechanical switching:** Works for simple register/xmm leaf functions (LaneZ pattern). Not mechanical for functions taking record pointers with complex memory chasing or in-set callees - requires mapping layer and expanding cutover to full callee closures.
- **Biggest costs:** Finding call sites, understanding calling conventions/layouts, adding callees, building GF<->PES memory mappings.

## Status
LaneZ confirmed. 9 functions added to build/cutover infrastructure (generated/declared/linked) but not wired due to documented blockers. Report written to pes-port-ShippedPath.md.

## Main's review

The measurement is incomplete. "No obvious call site" is wrong for at least three of the nine: every one has a GF hand port
citing its VA (`FUN_140439f50` at `pesreceive.hpp:159`, `FUN_1408C5000` at `pesoffball.cpp:1604`, `FUN_14043a0a0` at
`pesteamconcepts.cpp:232` and `pesdodge.cpp:16`). `FUN_14043A0A0` takes two floats in registers and should have switched
mechanically, like LaneZ. No 5-min repros were run. Work saved on `origin/ShippedPath` as WIP, not landed.

What does stand is the split it ran into:

- **Value functions** (arguments and results in registers): switch-over is mechanical and cheap (LaneZ, ~5-10 requests).
- **Record functions** (a PES record pointer in a register, fields read at PES offsets such as `[T+0x8E08+0x1464]`): GF
  keeps those fields in its own structs at its own layout, so every call needs either a marshalling shim built from the
  field map (most fields have no GF carrier) or GF state laid out as PES's records.
