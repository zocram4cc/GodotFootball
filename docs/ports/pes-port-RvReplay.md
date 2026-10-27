# RvReplay: live rendezvous captures replayed; translator TLS fix; PROVEN counts (27-10-26)

Branch `RvReplay` pushed (`git ls-remote origin RvReplay` = `0a0478a7`). Base `origin/Rendezvous` (`0bd096d7`) merged with `origin/NoFloor` (`a775adcc`). Commits: `bcf15e7f` (recorded trace `--out`), `fe9d61f7` (hook restore in the harness), `0a0478a7` (TEB as `argv[7]`). Gate not run (Main gates). Scratch: `<workdrive>/gf-RvReplay-scratch/` (PES-derived, not committed). Live inputs read only; nothing written to `live/` or `rendezvous/`; PES not signalled. Per-capture outputs: `scratch/proc/seqNNN/` (`recorded.json`, `trace/`, `snapdiff.json`, `snapdiff.log`, `build/`).

Labels: CONFIRMED = read in tool output; INF = inferred; OPEN = not settled.

## 1. Summary

- **Captured calls replay through the recorded path.** `frame_trace.py recorded --out` runs the captured call at its real rsp, return address, registers, XMM, flags, MXCSR and GS base over the capture's memory, traces every impure-spec callee with call-time deltas, and records the call's own written bytes. `snapdiff.py` replays every traced callee on the PES side and the translated side.
- **Translator fix (CONFIRMED):** the translated snapshot diff main compiled `gs_base` from `frame_trace.TEB_BASE`, a synthetic TEB no snapshot holds. It now takes the trace's TEB at run time (`argv[7]`). This closes FrameRoot3's open TLS mismatch.
- **Recorded cutscene set (`cut/snap1-213436`): 15 PROVEN** (was 13), 1,932 replayed calls (was 1,918), 0 mismatches. Verdict changes: `1403FB710` and `1403FB730`, MISMATCH (7/7 wrong) to PROVEN (7/7 bit-exact). The other 13 are unchanged.
- **Live captures: 11 of 28 processed** (`seq001`–`seq011`). **Seven function-replays mismatch (OPEN, §3).** Captures `seq012`–`seq028` were NOT processed (budget).
- **Type-13 and MI-refresh: not captured** in `seq001`–`seq011`. The G+0x180 check is not run (§5).

## 2. Code changes

- `frame_trace.py`: `recorded --out DIR --hooks CFG [--spec-dir D]`. `run_recorded(trace=…)` installs the stub and import models and traces each spec's entry into `DIR/calls/<tag>.bin` and `.delta` (held in memory, written at finish). `CallDelta.own_writes()` gives the byte runs the call wrote (`written.json`). `trace_teb(trace_dir, sm)`: the record's TEB, else the snapshot's main-thread TEB. `record.json` carries the TEB.
- `full_snapshot.py`: `SnapMem(snap_dir, restore={va: code})`. A read of a rendezvous hook entry returns the displaced original code (the snapshot holds `jmp [rip+0]` patches). Used by `LazyMap` (emulator memory) and by the harness.
- `diffharness/snapcase.py`, `run.py`: `--hooks CFG`; TEB via `trace_teb`.
- `translate/snapdiff.py`: `--hooks CFG`; passes the trace TEB to the translated binary.
- `translate/diffmain.py`: `gen_snap` takes `gs_base` from `argv[7]` (`strtoull`), `argc` check 8. The `TEB_BASE` import is gone.
- `test_recorded_call.py`: 12 pass (harness Python), incl. a trace test (one entry, one written byte at `rdx`).

## 3. Live captures (`seq001`–`seq011`)

Per-capture result (`recorded.json` = emulated call; `snapdiff.json` = replays):

| capture | hook (`va`) | recorded | traced funcs | PROVEN funcs | mismatched funcs | notes |
|---|---|---|---|---|---|---|
| seq001 | `hud_radar_update_E2910` | ok, RAX 1 | 14 | 14 | 0 | 172 calls |
| seq002 | `fixdemo_caller_141f4b6c0` | ok, RAX 1 | 2 | 1 | 1 | `141F4D2F0` mismatch |
| seq003 | `fixdemo_eval_141F4D2F0` | ok, RAX 0 | 1 | 1 | 0 | |
| seq004 | `fixdemo_eval_141F4D2F0` | ok, RAX 0x100 | 2 | 1 | 1 | `141F4D2F0` mismatch |
| seq005 | `fixdemo_caller_141f4b6c0` | ok, RAX 1 | 2 | 1 | 1 | `141F4D2F0` mismatch |
| seq006 | `fixdemo_match_141F4B1D0` | ok, RAX 0x29F | 4 | 2 | 2 | `141F4B1D0` (rax, w), `141F4D2F0` (w) |
| seq007 | `fixdemo_match_141F4B1D0` | ok, RAX 0x47 | 3 | 1 | 2 | same two as seq006 |
| seq008 | `cut_create_141F57840` | **recorded failed**: unmodelled import `kernelbase.dll!GetLastError` (`0x6FFFFF3A9151`) | 16 | - | - | OPEN: import model needed |
| seq009 | `cut_create_141F57840` | ok, RAX 0 | 0 | - | - | no traced callee |
| seq010 | `fixdemo_caller_141f41ce0` | ok, RAX 1 | 4 | - | - | **snapdiff crashed** (no `snapdiff.json`), log `snapdiff.log` |
| seq011 | `fixdemo_caller_141f41ce0` | ok, RAX 1 | 4 | - | - | **snapdiff crashed** (same) |

Totals over `seq001`–`seq011` (per-capture replays, from `snapdiff.json`): replayed calls HUD 116, gameplay 28, restart 14, replay 14, cutscene 46. PROVEN function-replays by subsystem (ledger join, `universe.json`): **HUD 11, cutscene 7, gameplay 1, restart 1, replay 1 = 21**. Mismatched function-replays: cutscene 7 (`141F4D2F0` in five, `141F4B1D0` in two).

Notes:
- `seq001` (HUD radar, `1420E2910`): the captured call itself has no spec in `specs-union`, so it is not replayed (OPEN: specgen impure spec for hook VAs).
- The mismatches are all in cutscene-tagged FixDemo functions (`141F4D2F0`, `141F4B1D0`), differing in written bytes (`w`), and in `rax` for `141F4B1D0`. Not investigated (lockstep to the first divergent instruction not done). OPEN.
- The `snapdiff` log warns `no translator model for do='qpc' (kernel32.dll!QueryPerformanceCounter)`: a stub gap on the FixDemo path; OPEN.
- The same function appears in several captures, so these counts are per capture, not unique functions. Unique-function totals are not computed.

## 4. Dry run (INF: plumbing only, not a capture)

A synthetic record (live return-address slot, real `rcx`) on `births/birth1-214956`, entry `mgr3` (`141F616F0`): 29 callee tags, 430 distinct deltas, 58 pages written. `snapdiff` on `141F53DD0`: 10/10 bit-exact, own-block 1.00.

## 5. Type-13 and MI-refresh (ticket item 4): OPEN

No type-13 or MI-refresh capture yet among `seq001`–`seq011`. The `G+0x180` read and the one-frame emulated advance (MI+0x1198) are not run. Captures `seq012`–`seq028` may include them; not processed.

## 6. Totals against the snapshot-only numbers

| set | before | now |
|---|---|---|
| cutscene, `cut/snap1-213436` (FrameRoot3, `docs/ports/pes-port-FrameRoot3.md`) | 13 PROVEN, 2 MISMATCH (TLS), 1,918 calls | **15 PROVEN, 0 MISMATCH, 1,932 calls** |
| scene-birth, `births/birth1-214956` (`docs/re/pes-re-scene-birth.md`: 17 PROVEN, 1 MISMATCH `141F55030`) | 17 PROVEN | **re-check not completed** (the TEB change; the run was killed at 300 s) |
| live rendezvous `seq001`–`seq011` | none | **21 PROVEN function-replays** (HUD 11, cutscene 7, gameplay 1, restart 1, replay 1); 7 mismatched function-replays (cutscene) |

## 7. Open items

1. Mismatches `141F4D2F0` (`w`) and `141F4B1D0` (`rax`, `w`) in `seq002`, `004`–`007`: lockstep to the first divergent instruction (translator), fix general bugs in `xlate.py`, then `driver.py --check` on the recorded sets.
2. `seq010` and `seq011`: `snapdiff` crashed (no `snapdiff.json`); read `proc/seq010/snapdiff.log` for the traceback first.
3. `seq008`: unmodelled `kernelbase.dll!GetLastError` in the recorded call; needs an import model.
4. `seq012`–`seq028`: not processed. Run `scratch/process/run_all.sh <seq dirs>` as a tracked job (a plain `nohup &` was killed when the shell returned).
5. Captured hook entries need specs (specgen impure) before `snapdiff` can replay them (`1420E2910` etc.).
6. Scene-birth check (`birth1-all`): re-run after the TEB change (the old trace has no `teb` in `record.json`; `trace_teb` falls back to the main-thread TEB). Not completed.
7. Type-13 and MI-refresh: wait for those captures in `seq012`+; then `G+0x180`, bindings, one-frame advance.
8. `driver.py --check` and `gen_translated.py --check` not run after the `diffmain` change (the recorded-set re-run in §1 is the verdict list). Gate: Main.
9. `extern/godot-cpp` reverted to an empty directory (gitlink; never staged).
