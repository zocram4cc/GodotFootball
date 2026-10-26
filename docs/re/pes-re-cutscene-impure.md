# PES cutscene impure set: callee map, stub models, entry points, rollout plan

Read-only RE (owner ruling 24-10-26; brief allows one SIGSTOP snapshot only if the game shows a cutscene).
Read `brief-rules.md` first. Scratch: `<workdrive>/gf-CutsceneImpure-scratch/`
(`cutscene-impure-stubs.json` = the stub file for the task agent). No repo edits made.
Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA − 0x140000000).
Ledger `tools/dev/ledger/universe.json` (37,855 rows; 18,374 in scope).
`tools/dev/ledger/indirect.py` output: `<workdrive>/gf-Indirect-scratch/indirect.json` (6,873 sites).
Fox survey: `docs/re/pes-re-fox-layer.md` (system grouping reused here).

Title line: cutscene impure closure map for the translation pipeline (3,095 in-scope non-pure rows).

## 0. Method (one paragraph)

`pure_rows` (`tools/dev/ledger.py`: walked, non-boundary, no indirect site, no vtable ref/call,
all callees pure — greatest fixed point) gives 8,300 pure rows universe-wide, 751 of the
3,846 cutscene rows. The other **3,095 cutscene rows are the impure set** (this task's scope).
Each row's impurity cause and its transitive closure blockers were computed from the universe
walk (direct/tail callees, indirect sites by shape, vtable refs, table jumps, unresolved,
boundary rows), with `indirect.py`'s rule/class per site and the Fox survey's per-VA system
labels (`<workdrive>/gf-FoxSurvey-scratch/systems.json`). Absolute `[0x…]` slots (611 sites,
skipped by `indirect.py collect_sites` by design) were resolved by hand from the image:
IAT-area slots hold runtime DLL addresses, `.data` slots hold dump-time code targets.
Every claim below is CONFIRMED (ledger/image-observed) unless marked INF or OPEN.

## §1 The set (CONFIRMED counts)

3,846 cutscene in-scope rows = 751 pure + **3,095 impure**. Impurity cause (direct row signal):

| cause | rows | note |
|---|---|---|
| closure-only (all direct callees walked, but reaches impurity) | 1,720 | modelling the callees unblocks these |
| own indirect site | 541 | §2 shapes |
| boundary row itself (Fox/CRT) | 456 | §2 Fox split |
| own vtable reference | 322 | entity/descriptor roots |
| unwalked (bridge lists no function) | 56 | capstone bodies in universe |
| own table jump | 59 | counted separately (overlap with above in row signals; 39 in rollout §6) |
| unresolved | 56 | no bridge/static resolution |

Own indirect sites in the set: 1,173. By shape: absolute `[0x…]` 611 (of which 59 distinct
slots; top `0x14252FA28` ×235, `0x14252FA30` ×132 — both IAT-area DLL slots, CONFIRMED by
image read: `0x6FFFF…` targets), vtable-shape `[reg+off]` 551, bare-reg `CALL RAX/RBX` 7,
callbacks 3, other 1. `indirect.json` covers 558/1,173 (the vtable-shape + reg sites);
the 611 absolute sites are resolved in §2 (DLL-import vs `.data`-fnptr-code split).
Vtable-shape coverage: 519 sites with 64-capped candidate lists (slot 0: 4,073 vtables;
slot 1: 2,757; slot 2: 1,907 — the lists are unusable as-is; the task agent needs the
receiver-class narrowing in `indirect.py`, currently only the global-store single-class rule).

Boundary rows reached from cutscene callers (Fox survey §2: 1,923 cutscene→boundary sites,
15.4% of cutscene callee edges): by system — resource/VFS 360 rows, math/containers 231,
animation 64, scene/entity 60, render/materials 58, threading/jobs 15, UI framework 12,
CRT/libc 28. The distinct **blocking** callees (callee not pure, counted per cutscene-impure
caller) number **3,828**: 2,948 walked-impure + 880 boundary/outside (360 resource/VFS,
231 math/containers, 64 animation, 60 scene/entity, 58 render/materials, 52 outside-universe,
28 CRT/libc, 15 threading, 12 UI). Top-30 blocking callees (block count = cutscene-impure
direct callers):

| # | VA | name (subsystem, size) | kind | Fox system |
|---|---|---|---|---|
| 397 | 1403FB5B0 (HUD, 13) | walked, closure-only | — (game logic) |
| 308 | 1403FB410 (gameplay, 62) | walked, 3× own vtable-shape `[RAX+0x10]` | — |
| 231 | 14159F3E0 __security_check_cookie (8) | boundary:library | CRT/libc |
| 106 | 1403CF7E0 (gameplay, 26) | walked, 1× `[RDX+0x8]` | — |
| 103 | 1402647D0 (pre-match, 32) | walked, 1× `[RAX+0x10]` | — |
| 84 | 140402370 (HUD, 94) | walked, 1× abs `[0x14252FA28]` DLL | — |
| 81 | 140421F40 (2) | boundary:rtti `?` | math/containers |
| 80 | 140402240 (HUD, 85) | walked, 3× abs DLL slots | — |
| 69 | 140227930 thunk (cutscene, 1) | walked, closure-only (→1403FB2E0) | — (fox free path) |
| 57 | 1415A171A (render, 1) | boundary:rtti | render/materials (draw) |
| 57 | 141F500D0 (cutscene, 19) | walked, closure-only (FixDemoEnv getter) | — |
| 55 | 1403FF430 (restart, 10) | walked, 1× abs `[0x14252F340]` crt-init | — |
| 55 | 1403FF490 (restart, 10) | walked, 1× abs `[0x14252F6E8]` crt-init | — |
| 49 | 1402297E0 (replay, 6) | walked, 1× `JMP [RAX]` | — |
| 43 | 14029D370 (cutscene, 27) | walked, closure-only | — |
| 43 | 1415A02B0 (camera, 1) | boundary:crt | CRT/libc |
| 43 | 1415A1720 (pre-match, 1) | boundary:crt (JMP [IAT]) | CRT/libc |
| 42 | 141978300 (cutscene, 18) | walked, 1× `CALL [RAX]` | — |
| 42 | 141957C50 thunk (camera, 1) | walked, 1× `JMP [0x143516830]` fnptr-code | — |
| 37 | 14000FF90 (in-match menu, 85) | walked, 1× abs `[0x14372D600]` fnptr-code | — |
| 35 | 1403D25A0 (pre-match, 26) | walked, closure-only | — |
| 35 | 140229610 (gameplay, 21) | boundary:rtti | math/containers (fox SharedObject) |
| 34 | 1415A1726 (1) | boundary:crt | CRT/libc |
| 33 | 14159F850 _Init_thread_footer (20) | boundary:library | CRT/libc |
| 33 | 14159F8B0 _Init_thread_header (24) | boundary:library | CRT/libc |
| 32 | 1403CF770 (cutscene, 3) | walked, closure-only (tail chain) | — (fox alloc path) |
| 32 | 14159F6C0 atexit (8) | boundary:library | CRT/libc |
| 31 | 14000CC50 (in-match menu, 510) | walked, 7× own (3 reg + 4 abs fnptr-code) | — |
| 31 | 14037FDB0 thunk (pre-match, 1) | walked, closure-only | — |
| 30 | 140F5A2B0 (cutscene, 284) | walked, closure-only | — |

Citations: ledger `universe.json` rows (walked/boundary/indirect/vtables/tables fields) for
every shape above; image reads for the slot targets (e.g. `0x14252FA28 → 0x6FFFFA6CAD50`
DLL, `0x143516830 → 0x141957C60` CODE, `0x14372D600 → 0x141F9B950` CODE); Fox survey
`docs/re/pes-re-fox-layer.md` §1 for 140227930/1403CF770 (free path), 140229610
(SharedObject), 140421F40 (shared null slot), 1415A171A (shared render/device stub, §4
UNKNOWN service); ImpureMap `docs/re/pes-re-impure-map.md` §2 for the cookie model and
the logf/powf precedent; doc cites 1403FB410 (alloc: `pes-re-s7-records.md:38,137`,
`pes-re-hud-ui.md:667`), 141F500D0 (FixDemoEnv getter: `pes-re-cutscenes.md` §4.1),
141978300 (`pes-re-fox-registry.md:331`), 1403FF430/490 (return-`0x18000003` models:
`pes-re-type13.md:72` — INF, harness-forged precedent, not a ledger cite).

## §2 Stub models for the top callees (in scratch `cutscene-impure-stubs.json`, 30 entries)

Contract matches `tools/dev/translate/stubs.json`/`stubs.py` (`stubs.load()` verified to
accept the file): per callee `va, kind, why (citations), model (null = unmodellable),
callers (cutscene-impure direct callers, capped at 50), used_by_closure`. Kinds used:
`engine-noop` (4), `run-real` (1), `unmodellable` (25). Prioritised by blocking count.

- **Modelled now (5):** `14159F3E0` engine-noop (cookie, ImpureMap precedent);
  `140421F40` run-real (2-insn return-0, Fox survey); `14159F850/14159F8B0/14159F6C0`
  engine-noop (thread-init/atexit, toolchain-emitted, no translated-path effect).
- **`unmodellable` with the reason (25):** each `why` names the exact blocker —
  DLL-import abs slots (`140402370/140402240`: `[0x14252FA28/30]` dump-time DLL targets,
  harness has no import models except abort traps); crt-init slots (`1403FF430/490`:
  KNOWN slots `0x14252F340/0x14252F6E8`, driver keeps non-abort import slots unmodellable);
  wide vtable candidate sets (`1403FB410` slot-2 ×1,907, `1403CF7E0` slot-1 ×2,757,
  `141978300/1402297E0` slot-0 ×4,073 — runtime dispatch needs receiver narrowing first);
  unnamed CRT thunks (`1415A1720/1415A02B0/1415A1726`: Fox survey §4 UNKNOWN, resolve by
  signature at translation); shared draw slot (`1415A171A`: Fox survey §4 UNKNOWN service);
  fox allocator (`140229610`: needs heap model); reg sites (`14000CC50`: `CALL RAX/RBX`
  input-dependent, `indirect.py` class unresolved); transitive chains (`1403FB5B0 →
  1403FB4F0` 5 sites; `140227930 → 1403FB2E0 → 1403FB5B0 → …`; `141F500D0 → 1403FB410 +
  141F4C950` vtable-ref; `1403CF770/14037FDB0` tail chains; `140F5A2B0/1403D25A0/14029D370`
  closures). Two are dispatch-plausible with extra work, noted in their `why`:
  `141957C50` (fnptr-code single target `0x141957C60`, uncorroborated store) and
  `14000FF90` (fnptr-code `0x141F9B950` + direct callee).
- **Correction to a plausible misread (CONFIRMED by re-measurement):** none of the top-30
  walked blockers is transitively `run-real`-clean — every one's closure reaches an
  indirect site, vtable ref, or unmodelled boundary row. An earlier greedy pass that treated
  own-signal-free callees as modellable (+230 cutscene fns over 30 picks) was wrong: it
  forced candidates into the runnable set without checking their own closures. The honest
  static number is §6 Tier A = 33. Do not mark any top-30 walked callee `run-real`
  without the transitive proof the task agent must run (`classify.py` per function, which
  also sees global stores the ledger does not record).

## §3 Entry points and the real-call option

Entry chains (CONFIRMED caller inversion over the universe; all in-scope):

- **Event builders (match side):** `FUN_14047C500` (DemoInfo writer: subject side
  `ctx+0x5204`, event pos, event player; `pes-re-cutscenes.md` §4.2) ← goal `140478190`
  (celebration selection, §4.3) ← `140446B20` ← `140446390` ← `14044C880` ← `140445E90`
  (restart, 284 insns, 15 direct callees) ← `140422030` (restart, 568); foul `140480910` ←
  `14047FEB0` ← `140446520/1404466F0/140446960` ← same `140446390` chain; substitution
  `140484A10` ← `140446F80` ← `140446390`; period-end `140488470` ← `140447290` ←
  `140446390` and ← `1404A3FA0`; end-of-match `14048AB10` ← `1404A3D00/1404A3EB0` ←
  `14044A180` ← `14044C880`. So all match-side cutscene entries route through the restart
  subsystem's `14044C880/140446390` fan-in — the frame hook for match-triggered cutscenes.
- **FixDemo runtime (manager side):** `FixDemoManager` vtable `0x142BA6800`
  (`STEP_IDLE/CREATE/LOAD/RESUME_WAIT/WAIT/NEXT_LOAD/EXEC/END`), `FixDemoMatch`
  `0x142BA6BC0`, `FixDemoEntrance` `0x142C701F8`, `FixDemoReplay` `0x142C70458`
  (`pes-re-cutscenes.md` §4.1); `FixDemoList` expansion `141F616F0` ← 15 callers incl.
  ctor `141F5E360`; variant pick `141F4FDA0` ← `141F616F0`; matcher `141F4B1D0`;
  condition evaluator `141F4D2F0` (2,081 insns); slot build `141F5F330` ← ctor path;
  human creation `141F57840` ← `141F5E930` (idx16) ← ctor; fdc loader `141F36200` ←
  `141F2FCF0` ← 4 loaders; singleton getter `141F500D0` (63 callers).
- **Every entry function is Tier D** (its transitive closure contains dll-import slots,
  rtti boundary rows, unwalked rows and outside-universe callees — table in the report
  data; e.g. `140478190` reaches 840 rtti-boundary + 817 dll-import + 59 unwalked refs).
  No entry runs under stubs+dispatch alone; they need snapshots (below) or the Fox
  reimplementations (VFS/entity/anim per the survey §3).

Real-call option (CONFIRMED status, not a new snapshot): the team-tick precedent
(`docs/ports/pes-port-CallDelta.md`: 27,948/29,298 calls replayed over seven snapshots
via `frame_trace.py` CallDelta + `snapcase.py` + `SnapView::over`) applies to cutscene
functions unchanged — the mechanism is per-function call-time memory, not team-tick
specific. What is missing is a **cutscene-time snapshot**: the 16:23 live snapshot in
`<workdrive>/gf-LiveState-scratch/live/` was taken mid-play with **no FixDemoManager
instance live** (`pes-re-cutscenes.md` §7: DB ×1, FixDemoHuman ×19, Replay ×21 units —
heap-stale copies only, no playing demo). A SIGSTOP snapshot during a goal celebration
or kickoff scene would supply real calls the way the team tick's do (OPEN: whether
`frame_trace.py` covers the FixDemo manager stepping + fdc-load paths; the tracer was
built for match-tick functions). **No snapshot was taken this run: MI read `0x0` on six
polls 5 s apart (game in pre-match, pid 53052, 21:0x UTC), so no cutscene was playing
and the brief's cutscene-time condition was not met.** The task agent's trigger: poll
`MI = *(G+0x270)` (`G = *(0x1436F3DC8)`) until non-null AND match-phase indicates a
stoppage, or watch the replay ring / FixDemo history state at `0x1437F5DC8 + cat*0x40`
(cutscenes doc §7: non-empty per category after a demo runs); then one
`kill -STOP <pid>`, dump every readable mapping (`tools/dev/translate/live_snapshot.py`),
`kill -CONT`, ≤10 s, CONT guaranteed, log the pause. Cost precedent: snapshot-1 delta
trace 205 s, pool 4.5 MB for 6,450 records (CallDelta §2).

## §4 Rollout plan for the task agent (cheapest first)

Tiers (CONFIRMED counts from the transitive analysis; ledger signals + image slot reads;
global stores NOT in the ledger — `classify.py` per function is the gate before translating):

| tier | runnable-with | cutscene fns | first action |
|---|---|---|---|
| A clean | stubs only (33 fully-clean closures) | 33 | translate smallest first |
| B dispatch-only | stubs + runtime dispatch (vtable-shape/reg/fnptr-code/table/vtable-ref/edge; NO dll/crt-init/boundary/unwalked) | 509 | receiver narrowing, then dispatch |
| C dispatch+CRT | B + crt-init/engine-noop models (cookie/thread-init/atexit/CRT thunks) | 176 | add the 5 §2 models, re-run |
| D blocked | needs snapshots (real calls) or Fox reimplementations | 2,377 | cutscene-time snapshot; VFS/entity/anim |

Expected share runnable with the §2 stubs + `gen_dispatch.py` runtime dispatch:
**(33 + 509 + 176) / 3,095 ≈ 23%** statically runnable; the remaining 77% is D-blocked
(top D causes: `site:dll-import` in 1,678 fns, `boundary:rtti` in 1,279, `unwalked` in
1,012, `outside-universe` in 751, callbacks in 216). Real calls from a cutscene-time
snapshot move D-functions the way CallDelta moved the impure sample (74 → 5,950/6,450
replayable on snapshot 1). Proven rule (owner ruling 26-10-26, supersedes the 50% floor
and the union-of-captures rule): a function is PROVEN when every case it runs is
input/output bit-exact (return value and every written byte, from the same inputs and
memory), whatever its coverage. Coverage is recorded and reported per function and per
subsystem as a confidence measure, never a gate. Prefer real-call cases (snapshots +
call-time deltas); report proven counts plus coverage and list low-coverage functions.

Order: (1) Tier A by size (cheapest 20 listed: `1400EC040` 9 insns … `141010000` 124);
(2) Tier B by size (thunks first: `140227930`, `1419F4CF0/1419F4DA0/1419F4EE0/1419F6A30/
1419F7A80`, then `141311C30`, `1402B8280`, `1408D9B80`, `140A92430` (RNG ctor — check
seeding OPEN in cutscenes §4.2), …); (3) Tier C after adding the CRT models; (4) Tier D
only after the cutscene snapshot lands, entry-adjacent functions first
(`141F4FDA0/141F4B1D0` matcher pair, `141F616F0` expansion, `141F5F330` slot build —
each Tier D with small dll/vtable footprints listed in §3).

Specgen settings: `python3 -m specgen generate --set specgen/sets/cutscene.txt --mode
impure --image pes_full.bin --out-dir specs/cutscene-impure` (set file = 833 committed
rows: the pure-closure seeds+callees, `tools/dev/diffharness/specgen/sets.py`; impure
mode = `classify.py`/`impure.py`: out-of-set callees, stub refs, harness class per
function). Then: extend the set file with the Tier A/B VAs (the full 3,095 minus the 833
already covered), per-function `--check-spec`, translate + build the `run-real` closure
VAs before any diff (ImpureMap §3: the C++ build treats them as engine boundaries
otherwise), and regenerate `pes_dispatch_gen.hpp` (`gen_dispatch.py`) with the dispatch
targets. Gate stays green per the pipeline plan; Main re-runs the gate + smoke.

## §5 Open items / blockers

1. `mgr+0x38` writer (subject side) — `pes-re-cutscene-slots.md` §4.1: reads everywhere,
   no store found; decode FixDemo vtable idx1/idx2 (`141F61210/141F61290`).
2. Vtable candidate narrowing (slot-0 ×4,073 / slot-1 ×2,757 / slot-2 ×1,907): receiver
   provenance per site is OPEN; `indirect.py` has only the global-store single-class rule.
3. DLL-import abs slots (~59 distinct; top `0x14252FA28/30`, `0x142530650`, `0x14252F6A8`):
   no names statically (Fox survey §4); harness import-model coverage for the cutscene
   set is OPEN (CallDelta excludes import-model pages from deltas).
4. Cutscene-time snapshot not taken (MI null, pre-match); tracer coverage of the FixDemo
   STEP_LOAD…EXEC path is OPEN.
5. Global stores invisible to this analysis (ledger has no field); `classify.py` may move
   Tier A/B functions to D.
6. Out-of-range scene constants (`0x20022/0x50010/0x7002b/0x3016b`) — sentinels INF-med,
   `pes-re-cutscenes.md` §4.4.

## §6 Data files (in scratch, for the task agent)

- `cutscene-impure-stubs.json` — 30 stub entries (loads via `stubs.load()`; kinds:
  4 engine-noop, 1 run-real, 25 unmodellable with reasons).
- `mk_stubs.py` — generator (re-run after a ledger re-crawl).
- Counts: 3,846 cutscene rows; 751 pure; 3,095 impure; 3,828 distinct blocking callees;
  tiers A 33 / B 509 / C 176 / D 2,377.
