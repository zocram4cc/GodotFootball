# PES scene-birth snapshots, birth traces, replay verdicts, stop classes

Read-only RE. PES2021.exe pid 53052 under Wine, bot churning matches. No repo edits.
Scratch: `<workdrive>/gf-SceneBirth-scratch/` (catcher/trace scripts, `births/`,
`traces/`, `specs-birth*`, `snapdiff-*`, `res-*.json`). PES-derived bytes stay in
scratch, never committed. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(base 0x140000000). Ledger `tools/dev/ledger/universe.json`.
Prior docs: `docs/re/pes-re-cutscene-impure.md` (the 3,095 impure set, tiers),
`docs/re/pes-re-cutscene-snap.md` (4 stoppage snapshots, 25/3,095 steady-state reach),
`docs/ports/pes-port-CallDelta.md` (CallDelta + snapdiff mechanism).
Labels: CONFIRMED = read in image/live/snapshot bytes or tool output; INF = inferred
with level; OPEN = what would settle it. No PES bytes quoted below.

Owner rule applied: PROVEN = every replay bit-exact (return + every written byte,
same inputs and memory), whatever the coverage. Coverage reported, never a gate.

## 0. Result in one paragraph

Caught 6 scene-birth snapshots of 6 distinct restart kinds (period end, throw-in,
corner, goal kick, foul/FK, goal) with a 1->2 edge poller + immediate SIGSTOP.
The period-end and goal births carry a live FixDemoMatch with goal-shaped step lists;
the four ball-out/foul births carry no live manager (env mode 1, idle) — the scene
list builds later in those stoppages or not at all. Emulating the manager update
(slot 3), the matcher/evaluator probes, and the restart builders from the births
reaches 37 unique cutscene functions (32/3,095 impure, up from 25). Snapdiff replay:
**17 PROVEN**, 1 MISMATCH (`141F55030`, written-byte 49 diverges), 19 NO-REPLAY
(all import-model exclusions, dominated by `RtlEnter/LeaveCriticalSection`).
The restart builders never ran at a real call site: the Flow object (runner's rcx)
was not found, slot-14 CREATE stops on a DLL-import fetch, and the evaluator probe
needs caller-computed args. Their stops are classified in §5.

## 1. Scene-birth snapshots (CONFIRMED)

Tool: `birth_catch.py` — 50 ms read-only pread poll of `G = *(0x1436F3DC8)`,
`MI = *(G+0x270)`, `play = MI+0x116C`, `restart = MI+0x1554`, `reason = MI+0x1574`;
fires on the 1->2 edge (or kind change, or MI-appears-stopped), then
`cut_snapshot.take()`: SIGSTOP, dump every readable mapping, SIGCONT (CONT in a
finally block). Disarms until play returns to 1 (no retrigger storms); rejects
repeats of kept (restart,reason) kinds. 26 triggers, 33 dumps, 76.0 s total paused
time, every pause logged in `births/pauses.jsonl`, game kept running (pid 53052
alive at end).

Kept (6, with mem.bin):

| birth | play/per/frame | restart/side/reason | pause | kind (A5Judge §1.5 map) |
|---|---|---|---|---|
| birth1-214956 | 2/10/45940 | 7/0/1 | 2.3 s | period end (per 10) |
| birth1-220754 | 2/1/2586 | 2/1/4 | 2.3 s | throw-in, side 1 |
| birth2-220826 | 2/1/4304 | 4/1/4 | 2.2 s | corner, side 1 |
| birth3-220922 | 2/1/7196 | 3/0/4 | 3.2 s | goal kick, side 0 |
| birth4-221229 | 2/1/16036 | 5/0/5 | 2.1 s | foul/FK, side 0 |
| birth5-222835 | 2/3/45902 | 1/1/3 | 2.4 s | goal/kick-off, side 1 |

Hit rate: 26 triggers -> 6 kept-new-kind, 13 DUP-kind rejects (correct repeats:
throw-in x5, corner x4, goal kick x1, FK x3), 7 post-MI-null rejects (match ended
mid-dump: the trigger read a live pre but the dump landed after MI went null).
New-kind hit rate 6/26 (23%); dump success (post MI live, requested play) 26/33
(79%). No kick-off scene, half-time/full-time scene, card, substitution, or
penalty seen. OPEN: the same gaps as the snap doc, plus whether a celebration
replay (vs kick-off setup) was captured in birth5.

Manager state per kept birth (CONFIRMED `audit_cutscene.py` + step-vector reads):

| birth | FixDemoMatch | steps | env mode | cold-field reading |
|---|---|---|---|---|
| period end | 0x1a7e6d60 | 3: (4,0.0,0x6000b)(0,0.0,0x6000c)(2,0.5,0 term) | 2 | scene list present, period-end shaped |
| throw-in | none | — | 1 | pre-CREATE: no manager yet |
| corner | none | — | 1 | pre-CREATE |
| goal kick | none | — | 1 | pre-CREATE |
| foul/FK | none | — | 1 | pre-CREATE |
| goal | 0x1be32330 | 4: kind-4 fade 0.05 cutRefs 0x3014b/c/d + kind-5 term | 2 | goal-scene shaped (0x30000\|(idx+1) per cutscenes §2.3) |

The cold marker INF-hi: `mgr+0x30 == mgr+0x28` (variant map empty) in both live
managers — the matcher never fires per-frame (see §3). The no-manager births are
the coldest possible state: CREATE has not run. `mgr+0x38` reads 0 in both live
managers (writer still OPEN, slots §4.1 carries over).

## 2. Emulation from each birth (CONFIRMED runs)

Tool: `birth_trace.py` — snapshot-backed LazyMap + CallDelta (same machinery as
CallDelta `frame_trace.py`), hooks on all 3,845 ledger cutscene rows with layouts:
entry hook records calls + `entry_record` + delta id (lazy file opens — opening
6,400 files upfront starves the first fetch under the 2 GB cap, fixed this run);
own-block leaders hooked for coverage. Entries: `mgr3` (vtable slot 3 =
`0x141F616F0` expansion, rcx=mgr), `mgr14` (slot 14 = `0x141F5F330`), `reboot880`
(`0x14044C880`), `fanin390` (`0x140446390`), `matchload` (matcher `0x141F4B1D0` +
evaluator `0x141F4D2F0` probes, rcx=mgr).

| trace | entries run | frames | calls | reached | stop |
|---|---|---|---|---|---|
| birth1-all (period end) | mgr3 x50 | 50, no stop | 11,027 | 36 | mgr14 frame 0: DLL-import fetch `0x6ffffa67146c` (import model, §5) |
| birth1-matchload | matcher x50, eval x1 | 51, then stop | 51 | 2 (matcher+eval) | eval: null read `0x0` (needs caller args, §5) |
| birth1-restart | reboot880 | 1, stop at once | 0 | 0 | rcx=0 read `0x8f0` (Flow object unknown, §5) |
| birth5-goal | mgr3 x50, matcher probe | 51, then stop | 9,669 | 30 (29 shared + matcher) | matcher probe: hole read `0x150` on this snapshot |

Reach union: 37 unique functions (all 29 steady-state + 8 new: `141F54D50`,
`141F56600`, `141F57540`, `141F5DA70`, `141F5DBC0`, `141F5F330`, `141F60590`,
`141F4B1D0`). Impure (doc pure list of 5): **32/3,095 (1.03%), up from 25/3,095
(0.81%)**. New impure: +7 (the 8 minus pure `141F54D50`... see verdict table: all
8 listed; pure recompute via ledger.py disagrees on 4 of the doc's 5 — the 32
uses the doc's list to stay comparable).

Not reached (as in the snap doc): `141F4D2F0` evaluator on-path (1 probed case,
null-arg stop), `141F57840` human creation, `141F36200` fdc loader, restart
builders `14044C880/140446390` (match side, Flow-gated).

## 3. Replay verdicts (CONFIRMED snapdiff runs)

Specs: cutscene set (833) + 32 generated impure-mode (`specgen generate --mode
impure --pilot-set specgen/sets/cutscene.txt`): all 32 classify "harness cannot
run" statically (transitive indirect/import blockers, mostly `1403FB410`
`[rax+0x10]`), but the snapdiff `blocked.json` exclusion replays the cases that
stay off those paths. 38 functions replayed over birth1 (+matchload trace):
11,055 + 3 calls.

PROVEN = 17 (every replay bit-exact):

| fn | calls | replayed | cov | fn | calls | replayed | cov |
|---|---|---|---|---|---|---|---|
| 140385820 | 572 | 572 | 1.00 | 141F53DD0 | 572 | 561 | 0.33 |
| 140A7CB40 | 150 | 150 | 1.00 | 141F53E80 | 1122 | 847 | 0.33 |
| 140A7CC60 | 100 | 100 | 1.00 | 141F56600 | 15 | 13 | 0.67 |
| 140A7CC90 | 50 | 50 | 0.75 | 141F57540 | 2 | 1 | 0.83 |
| 141F5E630 | 150 | 150 | 0.39 | 141F5DC50 | 572 | 561 | 0.33 |
| 141F54D50 | 2 | 2 | 1.00 | 141F5DE00 | 472 | 461 | 0.20 |
| 140A7CC00 | 350 | 350 | 0.29 | 141F500D0 | 151 | 151 | 0.29 |
| 14155E260 | 275 | 143 | 0.33 | 141F4B1D0 (matcher) | 2 | 2 | 0.05 |
| 141F4FDA0 | 150 | 150 | 0.54 | (+141F53DD0/53E80/56600/57540/5DC50/5DE00 above) |

in 4,264 replays.)

MISMATCH = 1: `141F55030` (50/50 replayed, all 50 mismatch, cov 0.80): return and
xmm0 exact, written bytes diverge at byte 49 (records truncate w at 120 chars;
divergence past it). Real translator divergence, same family as the CallDelta
`1404405D0` upper-lane class INF-med. OPEN: the diverging instruction (RealFix lane).

NO-REPLAY = 19 (6,643 traced calls, 0 replayable): every case excluded by the
"OS import model run" rule — 4,642 `RtlEnter/LeaveCriticalSection` alone, 1,999
+`msvcrt!memcpy`, 99 `did not return` (`14155E260` budget), 1 import-fetch +
1 null-read (the two probes). Functions: `140401EE0` (2,588), `141F5D500` (622),
`14159FADC`/`141F53A50`/`141F54DA0`/`141F54FA0` (572 each), `140A7CC00`-path
`141F5D620`/`141F53C70` (275), `141F5DE00`-adjacents, `141F616F0` (50),
`141F54ED0` (50), smalls (`141F53F20`/`141F60EF0`/`141F61DF0`/`141F61EE0` 11 each,
`141F5DA70`/`141F5DBC0` 2 each, `141F5F330`/`141F60590` 1 each), `141F61D20`
(100), evaluator `141F4D2F0` (1).

The 25 steady-state functions: all 29 old reached again here (superset); verdicts
above cover all of them (their cases now come from birth traces).

## 4. Coverage (reported, never a gate)

Own-block fractions in the table above (`driver.block_bounds` live leaders).
Full 1.00: `140385820/140A7CB40/140A7CC60/141F54D50`. Low (<0.3): `140A7CC00`
0.29, `141F500D0` 0.29, `141F5DE00` 0.20, `141F4B1D0` 0.05 (2 cases). The ruling
keeps them PROVEN regardless.

## 5. Stop classification (CONFIRMED ledger + trace)

| # | stop | where seen | what it needs | count |
|---|---|---|---|---|
| S1 | `RtlEnter/LeaveCriticalSection` import-model exclusion | 6,641 traced calls over 19 fns (every NO-REPLAY fn) | stub: translator import-call support (a `return 0` stub is trivially modellable; the pipeline has no import-call codegen) or Fox reimpl of the Fox-allocator lock path | 19 fns blocked |
| S2 | `msvcrt!memcpy` import-model exclusion | 1,999 calls (with S1) | stub: memcpy model codegen (contract exists in import_stubs.json) | co-blocker |
| S3 | Flow object unknown (runner rcx; `[rbx+0x8f0]` read at 0x8f0 with rcx=0) | reboot880/fanin390 at real sites | dispatch: find Flow (scans tried: referee-vtable instances — 0x142598558 has no live instance; G-child shape scan; step-array content scans — all inconclusive; candidate 0xa3ee8e0 falsified by null +0x17860 chain) or trace down from frame-sync `140422030` with the referee object | 2 entries |
| S4 | DLL-import fetch `0x6ffffa67146c` | mgr14 (slot-14 CREATE) frame 0 | stub: import model for the slot-14 path, or Fox reimpl of the CREATE service it calls | 1 entry |
| S5 | evaluator null-arg read | `141F4D2F0` probe (rcx=mgr only) | dispatch: real call-site args (it is called BY the matcher with computed args; the 1 traced entry is the matcher's own call) | 1 fn |
| S6 | `14155E260` budget/`did not return` (99/275) | replay only | INF-med: path-dependent long loop; 143/275 still replay bit-exact | partial |
| S7 | `141F55030` w-byte-49 divergence | 50/50 replays | Fox/translator bug hunt (RealFix lane), not a stop | 1 fn |

Counts: stub-needing 19+1 (S1/S2 import codegen), dispatch-needing 3 (S3 Flow,
S5 eval args, S6 partial), Fox-reimpl-needing 0 new (S4 CREATE service is the
only reimpl candidate, and only if import models stay out).

## 6. Open items / blockers

1. Flow object (S3): the restart builders' real call sites stay unreachable until
   the runner's rcx is resolved. Cheapest next: emulate frame-sync `140422030`
   (slot 0 of referee vtable) with each 0x142598xxx-vtable instance as rcx and
   watch for the `140445E90(Flow,…)` call — reads its rcx off directly.
2. Import-calls codegen (S1/S2): unblocks 19 functions / 6,641 calls at one
   stroke. The models exist (`import_stubs.json`: return-0, memcpy); only the
   translated-side call support is missing.
3. `141F55030` divergence at w-byte 49: trace to the instruction (compare
   harness vs translated addrs.json first).
4. Kick-off/half-time/full-time/card/sub/PK births not captured; matcher fired
   only twice in 100 manager frames (cache-hot); evaluator still needs its real
   args. Keep the catcher (`birth_catch.py --keep N`) running into the next session.
5. No PROVEN claim covers CREATE/LOAD (`141F57840/141F36200/141F5F330`): they run
   at manager birth, which no snapshot caught (all births land mid-stoppage).

## 7. Files (scratch only)

- `birth_catch.py`, `births/` (6 kept + logs + `hitrate.json`), `birth_trace.py`,
  `traces/birth1-all` (36 fns, 11,053 cases, pool 144 MB), `traces/birth1-matchload`,
  `traces/birth5-goal` (30 fns), `traces/birth1-restart` (stop record),
  `specs-birth/` (30) + `specs-birth2/` (matcher/eval), `specs-all/` (863),
  `snapdiff-birth1/`, `snapdiff-matchload/`, `res-pilot.json`, `res-birth1.json`,
  `res-matchload.json`.
