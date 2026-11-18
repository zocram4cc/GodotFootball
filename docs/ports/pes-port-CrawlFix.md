# CrawlFix report (branch `CrawlFix`, 18-11-26)

## Objective and scope
Implement the CrawlGap fixes as tooling on a new branch off origin/main:
dropped direct rows, the r13+rax*4 jump-table form, LEA-referenced pointer
runs (dtable), thunk-tail edges, 24+11 roots with evidence, walk-or-not the
three boundary rows (decided by what they do). Red tests first, regen with
`tools/dev/ledger.py` (joins.json untouched), --check green, history row,
before/after in-scope counts. Wrong cite 0x14071D760 fixed (comment only).

## Findings by class (107 UNCLASSIFIED → 0 remaining)
- **Missing edge, fixed (19):** W1 dropped direct row 0x14079B3D0 (fixpoint
  sweep restores it + 55 same-class siblings); W2 bare-hex CMP bound
  (`CMP EAX, 5`) hid the r13+rax*4 table at 0x141A89569-74, whose case 0 is the
  fallthrough chunk with the 0x141A8966F call to 0x141A874A0; W3 vptr-store
  LEA pattern (`LEA RAX,[table]; MOV [RBX],RAX`, e.g. 0x14210D455) proves
  RTTI-less dispatch tables the vtable rule can't see.
- **Missing edge, boundary-policy (6):** only in-scope callers are
  0x141EC4560/0x141ED4310 (rtti:draw) + 0x141AFB090 (doc:anim-blend).
  Decided WALK after decompile/decode: 0x141AFB090 is an in-match
  BodyAnimControl blend builder calling match fns incl 0x141B15F00 (CONFIRMED
  decompile); the draw pair are pre-match kit/uniform/handshake dispatchers
  with bounded fan-out (32+4 direct callees incl 0x1414E03C0, 0x141ED48F0).
  Walked via WALK_EXCEPTIONS with reasons; the draw ENGINE boundary itself is
  untouched (out-rule still cuts its vtables).
- **Wrong VA (1):** 0x14071D760 is a 5-byte `JMP 0x14071D770` thunk. Fixed the
  GF comment cite only (src/sim/onthepitch/peskickerror.hpp:93). joins.json
  NOT regenerated (per order); ledger --check is green on the committed joins.
- **Genuinely off-path (10, allow-listed with evidence):** edit/save builder
  0x14140EA40 (decompile: u__save_/u__mount_/u__KONAMI__, menu CPK-loader
  caller); edit sequencer cluster 0x141FE3830 + 4 + 2 enum leaves (edit::
  0x1414Cxxxx + save-adjacent calls per decompile; GF cites editsave-only);
  callback-only 0x140A57B00 (DATA refs only, as `lea r9` arg); dead helper
  0x14213A180 (zero refs anywhere, bridge xrefs_to empty).
- **Missing root (79 → all covered):** 24 strategic seeds (trap/eliza/kick/
  carrier/goalkick sets, each with doc/live-hook cites) + 11 direct seeds for
  match-cited VAs with verified callers (worldrecord/stamina, eliza, carrier,
  uniform, form/state leaf). 6 round-1 seeds needed a real fix first: no
  bridge function contains them, and the visited container swallowed them —
  `seed_decode` re-decodes from the seed VA so every seed gets a row.

## Crawl fixes (tools/dev/ledger/universe.py; docs in code)
1. CMP_IMM accepts bare-hex bounds (`(?:0x)?`) — resolves the r13 shape
   (+395 `table jumps resolved`: 836 → 1231).
2. `ptr_runs`/`runs_of`/`dtable_callees`/`lea_refs` (+MAX_DTABLE=64, VT-skip):
   RTTI-less dispatch tables expand like vtables (54 `dtable rows`).
   Zero moved out-of-scope rows via dtable (verified).
3. `missing_callees` + bounded sweep requeues (≤3): crawl lossless for
   direct/tail rows across bridge hiccups (`sweep requeues`: 6).
   Thunk-tail: tails already recorded; locked by test + covered by sweep.
4. WALK_EXCEPTIONS (3 rows with reasons); seeds walked regardless.
5. doc text: per-row rules, leaf rule, boundary paragraph, crawl docstring.

## tests: `tools/dev/ledger/test_ledger.py` — 26/26 green)

9 pre-existing pass unchanged; 17 new (JumpTable/Dtable/ThunkTail/
BoundaryWalk/SeedDecode). Mutations run, observed red, reverted, re-run
green: M1 CMP→0x-only (2 red), M2 run-length off-by-one (1 red), M3
exceptions off (1 red), M4 drop tail kind (1 red), M5 seed_decode inverted
(1 red), plus fixture-bug fixes (hex VA width, RDATA bound, dropped
accumulator line — the suite caught a real bug in new code).

## Regen (image pes_full.bin, bridge 127.0.0.1:8080, system python3/capstone)

- universe: 131 seeds (96+24+11), roots in meta, ptr_runs 8471, walk_exceptions 3.
- `ledger.py --check`: GREEN — no unrecorded regressions (18818 rows compared,
  163 scope moves incl. 3 new: 2 leaf→undecoded via dtable, 1 engine→undecoded
  for walked 0x141ED4310); off-path ports 73, all allow-listed.
- history.csv row appended (fb9feb233c... then 2029c733 state).
- offpath_allow.json: 269 → 183 (round-1 coverage) → 172 (11 direct seeds);
  0 UNCLASSIFIED remain.

## Before/after in-scope counts (committed LEDGER.md → regenerated)

| status | before | after | Δ |
|---|---|---|---|
| in_total | 18857 | 26120 | +7263 |
| out_total | 19481 | 18865 | -616 |
| ported | 244 | 405 | +161 |
| diffed | 3764 | 3767 | +3 |
| translated | 1759 | 1827 | +68 |
| stand-in | 35 | 59 | +24 |
| decoded | 957 | 1476 | +519 |
| leaf accessor | 496 | 1058 | +562 |
| engine | 1219 | 1631 | +412 |
| undecoded | 10382 | 15896 | +5514 |
| inert | 1 | 1 | +0 |

New rows: 5894 brand-new + 1013 out→in moves + 56. Moves ride verified
direct/vtable/tail edges from newly walked match code (0 dtable-driven);
subsystem tags follow the in-path (AI 3348, animation 1005, pre-match 428,
gameplay 311, cutscene 305). Out categories moved: online 663, career 290,
edit 55, save/load 5 — shared functions the old crawl reached only from out
roots. Owner call: keep (match code calls them) or hand-list individually.

## Files (branch CrawlFix, pushed, ls-remote confirms to 2029c733)

- tools/dev/ledger/universe.py (fixes + 35 seeds + docs)
- tools/dev/ledger/test_ledger.py (+17 tests)
- tools/dev/ledger.py (pure_rows + leaf text + scope paragraph)
- tools/dev/ledger/offpath_allow.json (86+11 pruned, 10 classified OFFPATH)
- tools/dev/ledger/scope_moves.json (+3 CrawlFix entries)
- tools/dev/ledger/universe.json + docs/re/LEDGER.md + history.csv (regen)
- src/sim/onthepitch/peskickerror.hpp (comment cite → 0x14071d770)

## Unresolved / for Main at land time

1. joins.json is stale re: the comment fix (ports still name 0x14071D760;
   harmless: the thunk has an in-scope row). Regen joins when landing.
2. Inter-run variance ±~250 rows (bridge hiccups; sweep covers drops but a
   full --max-less re-crawl is the stabilizer). Committed universe.json is the
   reference.
3. 0x14140EA40's caller chain (menu CPK loader) stays out: correct per scope
   (edit/save), flagged, not seeded.
4. Gate not run (Main gates). No builds (tooling-only; no C++ touched).

Recommended next: land CrawlFix, regen joins, continue with the 10 off-path
allow entries as documented non-goals.
