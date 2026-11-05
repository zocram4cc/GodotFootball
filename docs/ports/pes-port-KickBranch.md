# PES port: KickBranch: the clips-that-turn-the-mover branch (0x1423BAEB0), the request builder's wide-pass gate (0x140A510F0) and the chooser's shot path (0x140A641D0, 0x140830380, 0x140831CA0) translated and proven bit-exact on SYNTHETIC modified calls; GF's order carries PES's own +0x149 / +0x8 / +0xB4; the pes-loop kick is cut over to KickOrderFor + RunKickRequest + RunKickChooser (04-11-26)

Branch `KickMotion` (worktree `gf-KickMotion`), pushed (confirm with `git ls-remote origin KickMotion`: `dbc76a6f` at the time of writing). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessors: `pes-port-KickRoot.md`, `pes-port-KickHeader.md`, `pes-port-KickWindow.md`, `pes-port-KickChooser.md`.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. root-motion rotation branch | **DONE**: 288/288 SYNTHETIC modified reader calls bit-exact (131 non-identity); one function added to the closure, `0x1423BAEB0`. Section 1. |
| 2. request-builder branch + should GF take it | **DONE**: `0x140A510F0` (and the 4 functions it reaches, 1 new file) proven 61/61 on SYNTHETIC modified builder calls. GF's order differs from PES's on three words, fixed; the branch itself is PES's (a wide-ball pass), not a GF artefact. Section 2. |
| 3. re-wire, delete `KickRequestFor`, smoke | wired and the old request + test deleted (section 3). Smoke 1 aborted at `0x140A641D0` (chooser shot path): translated and proven (section 4). Smoke 2 (capped serial run on `dbc76a6f`, Main's grant) aborted at **`0x140A907C0`**, called from the request builder's own translation: **NOT MET, no `[kickmotion]` line, no pes-loop hash**. Section 5. |
| 4. merge main, census, `GATE fast` | merged (main `3a9b9310` only added the KickRoot report). Census and `GATE fast` **NOT RUN** (batch BE not confirmed on main; no slot for the smoke either). Section 6. |

## 1. Item 1: the turning clips (CONFIRMED)

- Tool: `rootsyn2.py` (scratch): PES's own bytes (`frame_trace.run_recorded`, kick_chooser `live2/pid615629/seq15864`) with the reader's motion argument (`[rsp+0x28]`) and seconds (`xmm3`) replaced at its FIRST entry, the run stopped at the second entry; each run is a snapdiff trace (specs/kickroot), so the entry record and the call-time delta hold the modified arguments. 72 motions (every 61st of 4,389) x 0.2, 1/3, 0.5, 1.0 s = 288 rows; PES's vector for each equals `rootmotion_synthetic_all.tsv` (288/288); 131 are not (0, 0, 0, 1), 157 are.
- `snapdiff.py --bounded --only 0x141AFDF10` (`sdroot.sh`, one snapshot symlink per row so each is its own case): `calls=288 replayed=288 mismatches=0 own_block=1.0 (13/13) proven=True`. The closure grows by exactly one function: **`0x1423BAEB0`** (the segment evaluator `0x1423A5BC0`, `0x1423A5750`, `0x1423BB360` call). Record: `results_kickturn.json` (SETS `kickroot`), `closure_kickroot.json` 22 members. Real calls: none exist on disk (all 7 captured chooser runs return the identity vector); hook for Main in section 7.
- Test: `pes_kick_root_test` with the 288 rows installed beside seq15864 (`rootmotion_synthetic.tsv`) now asserts `turning > 0` (a file with no turning row would leave the branch unchecked); **PASSED** on the regenerated layer (the 131 turning vectors equal PES's on every byte). The 288 rows are SYNTHETIC (PES's reader asked other motions and times, never a real kick).
- Coverage: the root 13/13 blocks; the evaluator `0x1423BAEB0`'s own coverage is not measured (snapdiff reports the root's only).

## 2. Item 2: the builder's branch and GF's order (CONFIRMED)

- Path: builder `0x140833CD0` -> `0x1406D9A90` (call at `0x140835EE2`, when the order record's +0x1C is 0 or 5: always 0 in the 15 captures) -> `0x1406DA790` (only when the request subtype is 1: a ground pass, action 9) -> `0x1406DDA60` -> `0x140A510F0(side, target, 5)` (the side's pass-lane classifier, answers 0..4; calls `0x140A513A0`). `0x1406DDA60` returns false before it, unless the ball's |z| >= the penalty-area half width (pitch object +0x1C = 20.12 in the captures) and the target / angle gates hold (angle between the side's facing and ball -> target in [45, 135] degrees: `0x14259BE30`, `0x14259BE60`).
- Why no capture took it: the ground passes among the 15 (seq030, seq4766, seq4770, seq4772; the last three carry one order) have the ball at z = 0 and z = -13..-15, inside the 20.12 band (`hdrprobe.py`). **GF's order should take the branch exactly when its ball is wide: the gate is the situation, not an order byte** (INF from the decode, CONFIRMED by the modified states below: moving only the ball and the target opens it).
- Proof: `hdrsyn.py` (scratch): the 15 captures' own bytes, seq030's memory, the ball's (x, z) and the order target's (x, z) replaced at the builder's entry; 700 random draws (seed 7) of which 62 states got traced before I stopped the sweep, 49 entered `0x140A510F0` (8 also `0x140A513A0`); one partial trace removed. `snapdiff --bounded --only 0x140833CD0` over the 61 complete ones: `calls=61 replayed=61 mismatches=0 own_block=0.4422 (264/597) proven=True`. The closure grows by `0x140439EF0, 0x140439FE0, 0x14043A0A0, 0x140A510F0, 0x140A513A0` (3 were in the layer already; new files: `pes_fn_140A510F0.cpp`). Record: `results_kickbranch.json` (SETS `kick`), `closure_kick.json` 85 members. The branches of `0x140A510F0` that need `param_4 != 5` are dead from this caller (its other callers `0x1405379C0`, `0x1405A4B90`, `0x14060FE20` are outside the builder).
- GF's order vs PES's on the 15 (`hdrprobe.py`, CONFIRMED; units: record words as in `peskickchooser.hpp`):

| word | PES (15 of 15, or per kind) | GF before | GF now |
|---|---|---|---|
| +0x149 | 2 in all 15 (record default +0x148 = 0x200) | 0 | 2 (`kOrderByte149Default`): FUN_1407204E0 answers subtype **5** for a lofted receiver pass, GF's zero answered 7 |
| +0x8 | 1.0 in the 4 lofted passes without a receiver (kind 0x15); 0 receiver passes and ground passes; 0.7 shots | shots only | + 1.0 for kind 0x15 (`kOrderLoftedNoReceiverPower`) |
| +0xB4 | the pass speed in the 5 receiver passes (kind 0x13): 29.04, 24.98, 25.65, 24.16, 24.05; 0 elsewhere | 0 | the launch speed `|launch|` (stand-in; the builder scales it x 3.6 / 3.5, `0x14259CE38`, `0x14259BDE8`) |
| +0x1C, +0x28, +0x5C/5D, +0xCC, +0x14A, +0x14C | 0 | 0 | 0 |
| +0x10, +0x20, +0x44, +0x50, +0x54, +0x58, +0x64, +0x84 (record defaults) | 4, 0x20000, 0x300, 5, 2, 0x97, 9, 1 | 0 | 0 (the builder does not read them) |

The record's initialiser and the order writers are still not found (a byte scan for the immediates 0x148 = 0x200 and 0x58 = 0x97 finds no store; the record is built in the AI order path above `FUN_140724CC0`): the three stand-ins carry a `ponytail:` comment naming that.

## 3. Item 3: the cutover (CONFIRMED by build + tests)

`humanoid.cpp` (`PesKickSituation`): in a PES-loop match the Kick start now builds `pes::KickOrderFor(action, target, receiver, heading, shotPower, |launch|)`, runs `RunKickRequest`, then `RunKickChooser` on the request it built; the receiver is the team's pass target. `KickRequestFor`, its comment and `AGfBuiltRequestPutsTheFieldsWherePesDoesAndTheChooserAnswers` are deleted. Tests run (slot-held, `GF_PES_KICK_ORACLE` = the 22-dir oracle set): `pes_kick_root_test` 1/1, `pes_kick_chooser_oracle_test` 1/1 (descriptor equal on every byte of the 22 + chooser cases). `pes_kick_header_oracle_test` is still DISABLED_ (compile-fixed for the new `KickOrderFor` argument).

## 4. The smoke abort and the chooser's shot path (CONFIRMED)

The first run of the cutover (seed 7, 2 min, `--pes-loop`) aborted with `PES_DISPATCH_UNTRANSLATED: 0x140a641d0`: `FUN_14082E370` (the shot list's candidate evaluator, called from `0x1408371A0`) calls `FUN_140A641D0(actor, slot, 0x65F)` at `0x14082E625` when the kicker stands inside the goal-area window of the goal line (P+8 within (edge - 5, edge + 3) and |P+0x10| < pitch +0x14) with request +0x94 == 0 and +0x30 <= 5. The 24 real kick_chooser captures never took it.
Proof: `chsyn.py` (scratch): the synthetic chooser state h4358 (a shot) with P+8, P+0x10 and the request's +0x2C, +0x30, +0x5D, +0x80, +0x94, +0xEC, +0xFC drawn at random at the chooser's entry; 40 states, all entered `0x140A641D0`. `snapdiff --bounded --only 0x1408364D0`: `calls=40 replayed=40 mismatches=0 own_block=0.2837 proven=True`; the chooser closure grows by `0x140830380`, `0x140831CA0`, `0x140A641D0` (record `results_kickshot.json`, SETS `kickkind`, `closure_kickkind.json` 258). The layer was regenerated (`gen_translated.py`: 5,396 functions, 3,750 recorded) and `ledger.py --check` is current. SYNTHETIC again. **More untranslated paths may exist behind this one**: the smoke aborts at the first and the next is unknown until it runs.

Switch-off smoke on the cutover tree (CONFIRMED): `[determinism] 399e180f39459c2b` = main's baseline (30,213 ticks), so the default engine is untouched.

## 5. Smoke and ablation (OPEN: second abort)

Smoke 2 (seed 7, 2 min, `--pes-loop`, `systemd-run --user --scope -p MemoryMax=6G`, tree `dbc76a6f`): `PES_DISPATCH_UNTRANSLATED: 0x140a907c0`, printed after a `[stalemate] 5.0s idle` line, so the match had been running; no summary lines. `FUN_140A907C0(out1, out2, p, q, centre, radius)` is a segment-circle intersection helper (decompile: the quadratic of segment p -> q against a circle of `radius` around `centre`, roots written to `*param_1`, `*param_2`; callers include `0x1404A42E0`, `0x1405B5A00`, `0x140623750`, `0x140516740`, `0x14061ECE0`, `0x1406966A0`, `0x140776570`). The layer's `OUT-OF-SET CALL 0x140A907C0` sits in `pes_fn_140833CD0.cpp` (the request builder), so a builder path outside the 85-member closure calls it (which path: not traced, INF the lane tests `0x1406DAB80` / `0x1406DAFA0` family). Not obtained: pes-loop hash, `[kickmotion]`, the ablation. Switch-off on the earlier tree gave `399e180f39459c2b` (= main). Reference rows: the chain-bound, zero-request tree (`b1d39978`) gave `a2f78ca1b76a66e4` (46,254 ticks, `[kickmotion] 79 kicks, no clip 79`); main is `85c7d7c515cf92ae`. Chain-alone ablation: INF inert (the reader is only reached from a chooser run that gets geometry), not measured.

## 6. Merge, census, gate

Merged `origin/main` `3a9b9310` (docs only). Not run: census of the drifted seeds (the smoke has not run; the gate's kick-dependent families re-stage only after it does) and `GATE fast` (all slots held by other gates; batch BE not confirmed). `gen_translated.py` was run, not `--check`; `ledger.py --check` current.

## 7. Hooks for Main (real calls, EVERY >= 20, with caps)

1. `kick_root_141AFDF10` at `0x141AFDF10` (the reader), EVERY 20, cap 12, `--when 'd(rsp+0x28) in (0, 61, 427, 549, 610, 671, 793, 854, 915, 976, 1037, 1159, 1220, 1281, 1403, 1464, 1525, 1586, 1647, 1708, 2013, 2135, 2196, 2318, 2379, 2440, 2501, 2562, 2623, 2684, 2745, 2989, 3050, 3111, 3172, 3233, 3294, 3355, 3477, 3538, 3599, 3660, 3904, 3965, 4026, 4209, 4270, 4331)'` (the 48 motions whose clip returned a non-identity vector at some second: `[rsp+0x28]` is the motion argument, the `rsp` of an entry hook is the return slot). Keep FULL. A kick that picks one of those motions in live play is the first real turning call.
2. `kick_widepass_140A510F0` at `0x140A510F0`, EVERY 1, cap 12, `--when 'ret==0x1406DDBC0'` (only the call from `0x1406DDA60`: it fires when a ground pass's ball is outside the 20.12 band, rare). Keep FULL.
3. `kick_shotgoal_140A641D0` at `0x140A641D0`, EVERY 1, cap 12, `--when 'ret==0x14082E62A'` (the call from `0x14082E370`; `0x14082E625` + 5). Keep FULL.

## 8. Left (numbered)

1. Widen the request builder over `0x140A907C0` the way section 2 did: find the caller site in `pes_fn_140833CD0.cpp`, draw random states at the builder's entry (`hdrsyn.py` with the fields that open that gate), `sdhdr.sh`, `closure.py --no-specs --members-file` (tag kickheader), `gen_translated.py`, `ledger.py`, rebuild, rerun the capped smoke; repeat until the smoke finishes (each cycle ~40 min wall, ~12 requests). Then the ablation, census and `GATE fast`.
2. Ablation (section 5), then census + `GATE fast` after Main says BE is on main.
3. `pes_kick_header_oracle_test` stays DISABLED_ (MA data not mapped on GF's window).
4. Scratch `gf-KickMotion-scratch` (~46 GB: `syn/`, `oracle*`, `rs/` `hs/` `cs/` traces and the scripts `rootsyn2.py`, `hdrprobe.py`, `hdrsyn.py`, `chsyn.py`, `sdroot.sh`, `sdhdr.sh`, `sdch.sh`) is kept for item 1; remove it after the smoke. Captures unpacked this session and to pack again: `pid615629/seq15863, 15864, 16063, 16064`, `pid802552/seq030`.
