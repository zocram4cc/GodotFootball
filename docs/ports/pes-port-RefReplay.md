# RefReplay: foul/offside/restart hook replays, harness fixes, layer check (28-10-26)

Branch `RvReplay` (worktree `<workdrive>/gf-RvReplay`). Pushed: `3b24ae21` (merge with `origin/main` e9687d7c), `4a87554a` (snapdiff fixes), `02ecfd72` (SSE4.1 emulator attempt, later reverted), `0b6c7622` (revert). `git ls-remote origin RvReplay` = `0b6c7622`. Scratch (PES-derived, not committed): `<workdrive>/gf-RvReplay-scratch/hooks/`. Labels: CONFIRMED = read in tool output this session; INF = inferred, reason given; OPEN = not settled. Gate not run (Main gates).

## 0. Summary

- **Merge with `origin/main`: DONE.** One conflict (`tools/dev/translate/results_impure.json`, took main's). `rendezvous.lua` already equals main's (CONFIRMED). The RvReplay4 blocker (`0x141F56FD0`, prepass table read `0x13FFFFFFF`) no longer reproduces: `gen_translated.py --image PES --check` = "3951 functions (3672 recorded bit-exact, 279 closure) regenerate byte-identical", rc 0 (CONFIRMED).
- **Hook replays: 6 of 10 entries PROVEN** (was 3 of 10 under the old 50 % coverage floor; 5 of 10 under the owner rule before this pass, with `140446390` counted as a false pass). Table §2.
- **Two FAILs, two NOT VERIFIED, and the strict impure check is RED** (§4). I did not leave it green; the cause is open.
- **Fixes pushed with their red baselines** (§3): `4a87554a` (snapdiff: a closure that does not build compares nothing; per-unit parallel build; `proven` verdict). The SSE4.1 emulator fix (`02ecfd72`) was red on the strict impure check and was reverted (`0b6c7622`).

## 1. Method

- Captures: `<workdrive>/gf-Rendezvous-scratch/live/seqNNN` (index.json `rv.hook` names the hook; CONFIRMED). Hook config: `<workdrive>/gf-Rendezvous-scratch/run/hooks_live2.cfg`. `seq017`, `seq018` hold only `mem.bin` (no index, no call.bin): excluded.
- Per capture: `frame_trace.py recorded SNAP --hooks CFG --out TRACE --spec-dir SPECS` (writes `calls/<VA>.bin`), then one `snapdiff.py --snapshot SNAP=TRACE ... --only <10 VAs>`. Script: `hooks/gen2/run_foul.sh`.
- Specs: `specgen generate --mode impure` from the unpacked image. The ledger universe (`tools/dev/ledger/universe.json`) has no rows for `A3A690, A3A930, A3A870, A38F20, A39A80, A38F20 (sized), A3A500, 141E53520`; I patched a scratch copy only (`hooks/gen2/gen.py`, sizes and subsystems from `FoulOffside`'s `specgen/sets/stoppage_rows.json`). Committed universe untouched. 11 specs in `hooks/specs_all/` (scratch, PES-derived).
- Verdict (owner ruling 26-10-26): PROVEN = translated, every replayed call bit-exact (rax, xmm0, every written byte), no call excluded. Own-block coverage recorded, not gated.

## 2. Verdicts (final run on `0b6c7622`; `hooks/snapdiff_foul.json`)

Captures counted per hook: 6 each where the live dir has 6. Exceptions: `restart_driver_445E90` has 3 (seq031, 033, 035); `restart_446390` has 7 (seq019 plus seq067–072).

| hook | VA | captures (snapshots) | calls | replayed bit-exact | excluded | coverage | verdict |
|---|---|---|---|---|---|---|---|
| foul_sanction_47F170 | 0x14047F170 | seq083–098 (12 snaps, odd/even pairs) | 12 | 12 | 0 | 0.52 | **PROVEN** |
| foul_sanction_caller_480910 | 0x140480910 | seq083,085,087,089,095,097 | 6 | 4 | 2 | 0.23 | **NOT PROVEN** (4/4 bit-exact; 2 excluded, see §2a) |
| ref_involve_A3A690 | 0x140A3A690 | seq037–066 (22 snaps) | 22 | 22 | 0 | 0.40 | **PROVEN** |
| ref_judge_A3A930 | 0x140A3A930 | 12 snaps | 12 | 12 | 0 | 0.61 | **PROVEN** |
| ref_latch_A3A870 | 0x140A3A870 | 6 snaps (seq037,040,044,048,050,054) | 6 | 6 | 0 | 0.88 | **PROVEN** |
| ref_restart_cmd_A39A80 | 0x140A39A80 | seq058,060,062,064,065,066 | 6 | 0 | 0 | 0.21 | **FAIL** (6/6 abort, §2b) |
| linesman_lookat_A38F20 | 0x140A38F20 | seq043,047,053,057,061,063 | 6 | 6 | 0 | 0.21 | **PROVEN** |
| restart_446390 | 0x140446390 | seq019,067–072 | 7 | 7 | 0 | 0.39 | **PROVEN** (was a false pass before, §3) |
| restart_44C880 | 0x14044C880 | seq012,016,029,032,034,036 | 9 | 0 | 9 | – | **NOT VERIFIED** (§2c) |
| restart_driver_445E90 | 0x140445E90 | seq031,033,035 | 3 | 0 | 3 | – | **NOT VERIFIED** (§2c) |

Proven: **before** 5 (owner rule on the RvReplay4 harness; `446390` not built, so not proven); **after** 6 (`446390` now built and bit-exact). Old floor count: 3 before, 3 after (the floor was the wrong gate).

§2a. `480910`, calls excluded: the default unicorn model raises `UC_ERR_INSN_INVALID` at `0x141B700CC` (`pmovsxbd`, SSE4.1; seq085 and seq089 died there). The two calls are excluded in the verdict. Pinned with the SSE4.1-capable model they run, and they FAIL with `PES_DISPATCH_UNTRANSLATED: 0x6FFFFEC77E00` (an import-page thunk, no byte compare possible). CONFIRMED on the BROADWELL run (`snapdiff_foul_AFTER_BROADWELL.json`).

§2b. `A39A80` FAIL: every case aborts with `PES_DISPATCH_UNTRANSLATED: 0x14159F3E0`. That VA is a Control-Flow-Guard check stub (`cmp rcx,[rip+0x1F6BF59]; jne; rol rcx,0x10; test cx,0xffff; …; ret`, CONFIRMED by disassembly). The translated caller calls it through the dispatch table, which lacks it; the harness trace never recorded it as a reached target, so `build_closure` did not add it. **First diverging byte: none**: the run aborts before any compare. OPEN fix: put the check stub in the closure (or model it as a return for code-range `rcx`), and find why `reached` misses guard-pointer calls.

§2c. `44C880` and `445E90` NOT VERIFIED: no replayable case. Every case enters an unmodellable callee: `44C880` → `0x1404488E0` (x7), `0x140449B00` (x1), `0x140A79640` (x1); `445E90` → `0x140440FA0` (x3). These are PES functions, not imports, so the closure needs them translated or modelled. OPEN.

## 3. Fixes pushed, each with its red baseline

| commit | file(s) | red before (case) | after |
|---|---|---|---|
| `4a87554a` | `tools/dev/translate/snapdiff.py` | `snapdiff_foul_BEFORE.json`: `140446390` replayed 7, mismatches 0, `translated: false`, note `compile: ` (blank). The 1528-target closure was compiled in one `g++` call; `drv.sh` returned rc 124 `timeout after 300s`, and the note kept only lines containing `error`. A false pass. | per-unit compile, JOBS at a time (`min(4, cpus)`); a failed unit is named with rc and first error; a build failure sets `replayed=0`, counts `unverified`; verdict `proven` replaces `floor`. `140446390` builds and is 7/7 bit-exact. |
| `02ecfd72` → reverted `0b6c7622` | `diffharness/run.py` (+`new_x86_64()`), `snapcase.py`, `translate/frame_trace.py`, `translate/snapshot.py` | `snapdiff_foul_BEFORE.json`: `480910` excluded 2 of 6 with `emu error: Invalid instruction` at `0x141B700CC` (pmovsxbd). | BROADWELL CPU model made the calls run, but **red on the strict impure check**: `0x1404386D0` went from valid cases to 0 of 64 (`unmodelled read at 0x212 …; corpus empty`). Reverted. |

Also fixed in the merge: none (the merge needed no translator edits).

## 4. Strict checks

- `gen_translated.py --image PES --check`: **GREEN** (3951 byte-identical, rc 0). Run before the revert; no translator file changed by the revert, and this check does no emulation (INF).
- `driver.py --check --spec-dir specs/impure --record results_impure.json` (= `diff_impure_recorded`): **RED**. attempted 34, translated 34, diffed 33. `0x1404386D0`: `corpus empty (0 cases); 0 valid of 64 requested; 128 failed skipped`; the abort is `unmodelled read at 0x212 (size 1): no region or global covers it` (CONFIRMED in `corp_1404386D0.jsonl.meta.json`). It is red on the reverted tree too, so BROADWELL is not the only cause. The tree equals `origin/main` in `diffharness/` and `translate/driver.py` except RvReplay's own tools (`git diff --stat origin/main`: `driver.py` +10/-, `frame_trace.py`, `snapdiff.py`, `type13_advance.py`). Note: `tools/dev/rendezvous/rv_watch.py` also differs from `origin/main` by 18 lines; I did not touch it (Main owns the watcher). OPEN: whether `0x1404386D0` is red on `origin/main` too (not run; needs a clean worktree run, about 8 min).
- `gate.sh`: not run (Main gates).

## 5. Not done (named)

1. `0x141F57840` and `0x140886780` (RvReplay4 §2a): not re-run in this pass; the per-unit build may fix the blank-compile class, unverified.
2. Strict impure red (`0x1404386D0`, §4): cause OPEN. Next step: find which `unmodelled read at 0x212` input is produced by the case generator (the spec inputs `A0_8_18_E0_18069C` etc.), then compare with `origin/main`.
3. SSE4.1: the default unicorn model lacks pmovsxbd/pmulld, and the translator has no SSE4 mnemonics (`xlate.py` `emit_sse` whitelist). BROADWELL fixes the emulator but breaks `0x1404386D0`. OPEN: fix the translator's SSE4.1 coverage and the emulator model together.
4. Guard check stub `0x14159F3E0` (§2b) and import thunk `0x6FFFFEC77E00` (§2a): the import/guard models are OPEN.
5. `44C880`/`445E90` unmodellable callees (§2c): OPEN.
6. Ledger: `universe.json` lacks rows for the referee functions above; Main should add them, or the specgen impure path cannot run for them.
7. Hook specs and verdict JSON stay in scratch (PES-derived). Committing them is OPEN for Main, per the RvReplay4 note.
8. `traced_cov.py` still uses its own 0.50 floor; not changed (out of scope).
