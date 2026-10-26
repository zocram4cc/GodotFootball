# PES cutscene-time snapshots, FixDemo per-frame trace, reach/coverage, stop classes

Read-only RE. PES2021.exe pid 53052 under Wine, bot churning matches. No repo edits.
Scratch: `<workdrive>/gf-CutsceneSnap-scratch/` (audit/trace scripts, snapshot dirs
`cut/snap*`, trace dirs `trace-*`). PES-derived bytes stay in scratch, never committed.
Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (base 0x140000000).
Ledger `tools/dev/ledger/universe.json`; Fox systems
`<workdrive>/gf-FoxSurvey-scratch/systems.json`; stubs/callee map/tiers in
`<workdrive>/gf-CutsceneImpure-scratch/pes-re-cutscene-impure.md` (§ references below
are to that doc unless noted). Labels: CONFIRMED = read in image/live/snapshot bytes
or ledger; INF = inferred with level; OPEN = what would settle it.

## 0. What was done

1. Polled live PES read-only for stoppages; took 4 complete SIGSTOP snapshots during
   stoppages (play=2), 1.7–6.8 s pauses, all CONT-guaranteed and logged (§1).
2. Audited each kept snapshot for the FixDemo active state: manager object, scene/step
   data, FixDemoEnv, history (§2).
3. Emulated the FixDemo manager's per-frame update from the snapshots (copied
   `frame_trace.py` + CallDelta lazy-map machinery) and traced calls into the
   cutscene set (§3): reach (how many of the cutscene functions) + own-block coverage.
4. Classified every stop (Fox service / import / indirect) with stub/reimpl needs (§4).

Owner ruling applied throughout: PROVEN = every run case input/output bit-exact,
whatever the coverage. Coverage is recorded per function as a confidence measure,
never a gate. No PROVEN claims are made here (no snapdiff replays run); this pass
reports reach + coverage only.

## 1. Snapshots taken (CONFIRMED)

Tool: copy of MainSnap `full_snapshot.py` (`cut_snapshot.py`), `--play 2 3`,
`--max-pause 10`, gap 20 s. Pauses logged in `cut/pauses.jsonl` (6 pauses total,
1.7–6.8 s; every pause resumed, game kept running).

Kept (4, with mem.bin): all play=2 stoppages.

| snap | pre (play/per/frame) | post restart/side/reason | pause | kind (CONFIRMED restart/reason map, A5Judge §1.5) |
|---|---|---|---|---|
| snap1-213436 | 2/1/17033 | 4/0/4 | 1.9 s | goal kick, side 0 (restart 4, reason 4 ball-out) |
| snap2-213532 | 2/1/18204 | 1/1/3 | 6.8 s | goal/kick-off, side 1 (restart 1, reason 3 goal) |
| snap3-213639 | 2/1/21606 | 4/1/4 | 1.7 s | corner, side 1 (restart 4, reason 4 ball-out) |
| snap4-213753 | 2/2/23049 | 7/1/1 | 1.8 s | period end, 2nd half (restart 7, reason 1; per 2) |

Rejected (no mem.bin): snap3-213608 (post MI null), snap3-213623 (post MI null
after a play=3 pre), snap4-213748 (post MI null). Two earlier MainSnap stop/
snapshots were also audited for comparison: snap1-162213 and snap2-162511
(both goal stoppages, restart 1/reason 3).

Kind counts: goal 1 (snap2) + 2 inherited goal stoppages in stop/; goal-kick 1;
corner 1; period-end (half-time path, per 2) 1. No kick-off scene, no goal
celebration replay, no half-time/full-time scene, no cards/substitutions seen.
OPEN: whether a playing celebration was captured vs a stoppage with the scene
list built — §2 shows snap2/snap1 carry live 4-step goal scene lists.

## 2. Manager active state per snapshot (CONFIRMED bytes, no PES bytes quoted)

Method: `audit_cutscene.py` scans writable snapshot pages for FixDemo vtable words,
reads `FixDemoEnv` singleton `*(0x1437f5d50)`, per-category history
`0x1437f5dc0+cat*0x40`, MI anchors. Manager objects verified by vtable qword
(`0x142ba6bc0` = FixDemoMatch), then step vector `mgr+0x20/+0x28` (0x1e0 stride,
CONFIRMED divisor: (end-begin)/0x1e0 integral), step kind/fade/cutRef decoded per
cutscenes §2.2, env `ctx+0x5204/0x5228/0x5208/0x5214` per Appendix A.

| snap | FixDemoMatch | steps (kind/fade/cutRef) | env subj/mode | evpos / side/slot |
|---|---|---|---|---|
| cut snap1 | none | — | 0/1 | — |
| cut snap2 | 0x2c8beb10 | 4: (4,0.05,0x300ad) (4,0.05,0x30136) (4,0.05,0x3011c) (5,0.5,0x0 term) | 0/2 | (49.56,0,−32.06)/(0,7) |
| cut snap3 | none | — | 0/1 | — |
| cut snap4 | none | — | 0/1 (evpos zeroed) | (0,0,0)/(0,0) |
| stop snap1 | 0x2c6c0cd0 | 4: (4,0.05,0x30110) (4,0.05,0x300a7) (4,0.05,0x30132) (5,0.5,0x0 term) | 0/2 | (−49.56,0,−31.52)/(0,9) |
| stop snap2 | 0x1bf39090 | 3: (4,0.05,0x300f4) (4,0.05,0x30132) (5,0.5,0x0 term) | 0/2 | (−33.85,0,14.54)/(0,10) |

- All snapshots: FixDemoEnv live (`0x22ea1880`, vtable `0x142ba5bc8` CONFIRMED),
  history non-empty in all 10 categories (a demo ran in every category this
  session, INF med). Random1000 statics + heap copies present where a manager is
  live (3 heap hits next to each live manager; mode 2 = match RNG per cutscenes
  §4.2; mode 1 in the non-manager snapshots, INF: idle/reset value, OPEN).
- The two traced managers' step lists are goal-scene shaped (CONFIRMED): kind-4
  steps with fade 0.05 (`0x3d4ccccd`, distinct from the T2 default 0.5) and
  goal-range cutRefs `0x300xx`, closed by a kind-5 cutRef-0 terminator.
  Scene ids resolve as `0x30000|(idx+1)` (cutscenes §2.3): e.g. snap2 step2
  cutRef `0x3011c` = goal scene index 0x11c. INF med: in-progress goal demos
  (scene list built, steps expanded); the EXEC/LOAD distinction is OPEN (§3).
- `mgr+0x30` per-manager `map<cutId,rowIndex>` cache (cutscenes §4.2): snap1
  `+0x30 == +0x28` (end pointer; empty/contiguous), snap2 similar — variant picks
  cached or none yet. The `mgr+0x60` map node holds an fdc path fragment
  (`o/goal/cut_data/…`, CONFIRMED ascii in snap1 node). `mgr+0x38` (subject side)
  reads 0 in both (slots doc §3: default side source; writer still OPEN per slots
  §4.1 — confirmed again here, no store found).
- Readability: every manager-pointed structure audited (step vector, map nodes,
  fdc path bytes) is fully inside readable snapshot pages. No holes touch the
  manager, its steps, env, or MI.

## 3. Per-frame update emulation + reach/coverage (CONFIRMED runs)

Mechanism: `frame_trace_cutscene.py` / `frame_trace_full.py` — snapshot-backed
LazyMap (pages mapped on first touch, same as CallDelta), manager vtable slot
invoked with the live manager as rcx, N frames × fixed instruction budget.
Each cutscene-ledger function's entry hooked (calls) + own-block leaders hooked
(`driver.block_bounds` live leaders; callee blocks never counted). No stubs were
installed for this measurement (image bytes run; import-model pages excluded the
way CallDelta excludes them — a stop, not a silent skip).

Slot survey on snap1 mgr (2 frames × 2M budget each, 833-set hooks): only vtable
slots 1,2,3,4,6,7,10,11,14 reach anything; slot 3 (`0x141f616f0`, FixDemoList
expansion, vtable idx3) dominates (32→63 calls over 2 frames vs ≤11 elsewhere),
so it is the per-frame update emulated below. CONFIRMED: slot 3 = `0x141f616f0`
(idx1/idx2 are the `0.05/0.5`-fade accessors per slots §5 OPEN item — now
CONFIRMED as step-field readers, not the writer).

Full-universe run (trace-full-snap2): snap2 mgr `0x2c8beb10`, slot 3,
50 frames × 50M budget, hooks on all 3,845 ledger cutscene rows with layouts
(3,846th has no layout), rc=0, no stop, 11,564 calls total:

- **Reach: 29 / 3,845 with layouts (0.75%); 25 / 3,095 impure (0.81%).**
  `pure_rows` (ledger.py:93) gives 5 of the 29 as pure
  (`140385820/140A7CB40/140A7CC60/140A7CC90/141F5E630`, CONFIRMED recompute);
  the other 25 are impure — tier split below. Reached (calls): `140401EE0` (2634),
  `140A7CC00` (750), `141F53E80` (739), `141F5D500` (669), `140385820`/`14159FADC`/
  `141F53A50`/`141F53DD0`/`141F54DA0`/`141F54FA0` (619 each), `141F5DC50` (569),
  `141F5DE00` (469), `140A7CB40`/`141F4FDA0`/`141F500D0`/`141F5E630` (300 each),
  `140A7CC60`/`140A7CC90`/`141F61D20` (150 each), rest ≤60.
- The emulated entries themselves are reached: `141F616F0` (50 calls, its own
  50 frames), `141F4FDA0` (300 = variant pick per step per frame), `141F500D0`
  (300 = env getter), `141F5E630` (300 = idx4 step-state). NOT reached:
  `141F4B1D0` (matcher — variant cache hit every time, INF hi: `mgr+0x30` map
  already resolved, `4FDA0` never misses), `141F4D2F0` (condition evaluator —
  same cause), `141F5F330` (slot build — runs at CREATE, not per-frame, INF med),
  `141F57840` (human creation — same), `141F36200` (fdc loader — LOAD phase,
  not EXEC), restart builders `14044C880/140446390` (match side, not manager
  side — expected absent, CONFIRMED separation).
- Own-block coverage (executed live leaders / live leaders, `block_bounds`):

| fn | live | exec | cov | fn | live | exec | cov |
|---|---|---|---|---|---|---|---|
| 140385820 | 1 | 1 | 1.00 | 141F54DA0 | 4 | 4 | 1.00 |
| 140401EE0 | 22 | 11 | 0.50 | 141F54ED0 | 5 | 5 | 1.00 |
| 140A7CB40 | 4 | 4 | 1.00 | 141F54FA0 | 6 | 5 | 0.83 |
| 140A7CC00 | 7 | 2 | 0.29 | 141F55030 | 5 | 4 | 0.80 |
| 140A7CC60 | 1 | 1 | 1.00 | 141F5D500 | 8 | 8 | 1.00 |
| 140A7CC90 | 4 | 3 | 0.75 | 141F5D620 | 8 | 8 | 1.00 |
| 14155E260 | 6 | 6 | 1.00 | 141F5DC50 | 6 | 6 | 1.00 |
| 14159FADC | 6 | 6 | 1.00 | 141F5DE00 | 20 | 4 | 0.20 |
| 141F4FDA0 | 28 | 15 | 0.54 | 141F5E630 | 31 | 5 | 0.16 |
| 141F500D0 | 7 | 2 | 0.29 | 141F60EF0 | 7 | 7 | 1.00 |
| 141F53A50 | 3 | 3 | 1.00 | 141F616F0 | 66 | 49 | 0.74 |
| 141F53C70 | 4 | 4 | 1.00 | 141F61D20 | 20 | 17 | 0.85 |
| 141F53DD0 | 9 | 9 | 1.00 | 141F61DF0 | 6 | 6 | 1.00 |
| 141F53E80 | 6 | 6 | 1.00 | 141F61EE0 | 5 | 3 | 0.60 |
| 141F53F20 | 13 | 12 | 0.92 | 141F61CC0 | 22 | 16 | 0.73 |

  30 rows (29 with calls + `141F61CC0`, 16 leaders hit, entry hook missed its
  0 calls — hooked range vs entry VA alias, OPEN, counted for coverage only).
  Full 1.00: 15 functions. Low (<0.6): `140A7CC00` (0.29), `141F500D0` (0.29),
  `141F5DE00` (0.20), `141F5E630` (0.16) — listed per the brief; the ruling
  keeps them reported, never gated.
- Tier split of the 29 (CONFIRMED direct-row signals; transitive `classify.py`
  is still the gate before translating, per impure §4): 1 boundary row
  (`14159FADC`, Fox CRT/libc); 25 with no own indirect site/vtable-ref/table
  (A/B candidates — includes `141F4FDA0` and `141F500D0`); 3 with one own
  indirect site each (`141F616F0` `CALL [RAX+0x20]` at `0x141F61A3C`,
  `141F61EE0`, `141F61CC0` — dispatch-shaped, tier B pending receiver
  narrowing). No dll-import, crt-init, or unwalked stop fired inside the
  reached set on this path.
- Repeat runs (833-set hooks): snap1 mgr 50 frames → same 5 functions
  (`140385820/140A7CB40/140A7CC60/140A7CC90/141F5E630`), 1,519 calls;
  old stop snap2 mgr 50 frames → same 5, 1,172 calls; snap2 mgr → same 5,
  1,519 calls. The full-universe run is a strict superset (+24). Coverage
  per function is identical across the three managers (deterministic given
  the same scene shape, INF hi).

## 4. Stop classification (CONFIRMED ledger + image; Fox labels where mapped)

Each entry: its own stop, then what a stub vs a Fox reimplementation must do.
"Needs snapshot" = Tier D only clearable by real calls (CallDelta replay).

| entry | size | own stop (CONFIRMED) | transitive closure | stub need | Fox reimpl need |
|---|---|---|---|---|---|
| 141F616F0 expansion | 346 | 1 indirect `CALL [RAX+0x20]` @0x141F61A3C; 2 CRT boundary (`14159F3E0` cookie, `14159FA0C` delete-vector); table jumps | 106 fns, 6 boundary (CRT), 32 indirect sites, 2 vtable-refs (FixDemoEnv root), 5 table jumps | cookie engine-noop + `14159FA0C` model; runtime dispatch for the `[RAX+0x20]` call | VFS/entity/anim per survey §3 if dispatch unresolvable |
| 141F4FDA0 variant pick | 109 | clean own (no ind/vt/tbl) | 44 fns, 1 boundary (`1415A1720` CRT thunk), 17 indirect (incl. DLL-import abs `[0x14252FA28]` via `14125A080`), 5 table jumps | crt-init model for `1415A1720` | import-model coverage (CallDelta excludes import pages — OPEN per impure §5.3) |
| 141F4B1D0 matcher | 356 | clean own | 34 fns, 1 boundary (same CRT thunk), 16 indirect, 5 table jumps | same CRT model | same import-model gap |
| 141F4D2F0 cond eval | 2081 | clean own, 3 table jumps | 15 fns, 0 boundary, 10 indirect, 5 table jumps | dispatch for reached table jumps | condition-ctx fields already live in env (Appendix A) — no reimpl |
| 141F5F330 slot build | 93 | clean own | 61 fns, 3 boundary (CRT cookie/delete-vector), 21 indirect, 2 vtable-refs, 6 table jumps | CRT models | receiver narrowing for slot-0/1/2 candidate sets (impure §5.2) |
| 141F57840 human create | 54 | clean own (1 direct callee) | 881 fns, 136 boundary (anim 58, math 33, CRT 18, render 15, jobs 9, entity 3), 307 indirect, 115 vtable-refs, 18 table jumps | nothing stubs this — needs Fox anim/entity/heap reimpls | animation + scene/entity + heap model (survey §3) |
| 141F36200 fdc loader | 12 | clean own (tail + direct) | 794 fns, 37 boundary (math 21, CRT 12, VFS 3, render 1 `1415A171A` draw), 388 indirect (DLL-import abs slots), 28 vtable-refs, 17 table jumps | VFS models for the 3 resource rows; import models for DLL slots | VFS/entity/anim full path (survey §3) |
| 141F500D0 env getter | 19 | clean own | 40 fns, 3 boundary, 14 indirect, 2 vtable-refs (FixDemoEnv) | `1403FB410` dispatch (`[RAX+0x10]` ×3) | receiver narrowing (single-class rule, impure §5.2) |
| 14044C880 restart | 261 | 1 indirect `[RAX+0x18]`; 1 table jump; 37 direct callees | 6,081 fns, 841 boundary (VFS 340, math 258, render 79, anim 70, entity 47, CRT 29, physics 9, jobs 9, UI 2) | out of scope for stubs — match-side program | full restart program (A5Judge) |
| 140446390 fan-in | 97 | 1 table jump; 13 direct callees | 6,023 fns, similar boundary spread | same | same |
| 140478190 goal builder | 530 | clean own, 4 table jumps, 36 direct callees | large (840 rtti + 817 dll-import + 59 unwalked per impure §3) | needs snapshot | needs snapshot + Fox |

Unreached-in-trace entries' stops are static (they never executed here), so
their classification is CONFIRMED-ledger, not CONFIRMED-trace. The reached
`141F4FDA0/141F500D0` executed with zero stops fired on-path (their closures'
DLL-import/vtable stops sit off the cache-hit path — cache-hot `mgr+0x30`
map, §2).

## 5. Open items / blockers

1. `mgr+0x38` writer still not found (slots §4.1 item 1 carries over; idx1/idx2
   decode now CONFIRMED them as step-field readers, not the writer).
2. `141F4B1D0/141F4D2F0` never execute while the variant cache is hot; a cold
   manager (fresh CREATE) or cache-busting scene is needed to trace the
   matcher/evaluator path. OPEN: which live moment is cache-cold.
3. `141F57840/141F36200/141F5F330` run at CREATE/LOAD, not per-frame; the
   per-frame slot only replays EXEC. Tracing CREATE needs the manager birth
   moment (entrance/kick-off scene start). OPEN.
4. No PROVEN claims yet: entry-state records + CallDelta deltas were not
   recorded in this pass (hooks counted calls/coverage only). Next: re-run
   with `--snapshot-calls` (frame_trace CallDelta path), then snapdiff replays
   per the NoFloor rule; Main re-measures.
5. Kick-off/celebration/replay/half-time/full-time scenes not captured (only
   goal stoppages + goal-kick/corner/period-end stoppages). The bot churns
   matches; keep the snapshot loop running into the next session.

## 6. Files (scratch only)

- `cut_snapshot.py`, `cut/` (4 kept snapshots + logs), `audit_cutscene.py`
  (+ per-snapshot JSON lines in §2 table).
- `frame_trace_cutscene.py` (833-set), `frame_trace_full.py` (full universe),
  `trace-idx*` (slot survey), `trace-idx3-full`, `trace-snap2-idx3`,
  `trace-oldsnap2-idx3`, `trace-full-snap2` (record.json files with calls,
  reach, executed leaders, lazy stats, import calls).
