# RefReplay3: closure stubs, hook sweep over all captures, SSE4.1 re-apply, slim captures (28-10-26)

Branch `RvReplay`, pushed head `a9946f5d` (`git ls-remote origin RvReplay` = `a9946f5d`). Commits: `d8a61a62` (closure stub_refs, referee universe rows, red/green check), `c1e590a8` (cherry-pick of `02ecfd72`, SSE4.1 emulator), `a9946f5d` (`tools/dev/rendezvous/slim.py` + README). Scratch, PES-derived, not committed: `<workdrive>/gf-RefReplay3-scratch/`. Gate not run (Main gates). Labels: CONFIRMED = read in tool output this session; INF = inferred, reason given; OPEN = not settled.

## 0. Summary

- **Closure stubs (task 1): fixed generically.** Same empty-row specs: 0/6 bit-exact (abort) before, 6/6 proven after. Committed check `check_closure_stubs.py`: RED on HEAD, GREEN here.
- **Hook sweep (task 2):** 74 captures (live + live2), 10 hooks, 148 snapshot pairs. 7 hooks PROVEN; `480910` 7/10 bit-exact (3 aborts, import thunk); `44C880` 0/11 and `445E90` 0/4 (unmodelled callees). Table §2.
- **SSE4.1 re-applied (task 3):** strict impure check `driver.py --check` rc 0 (34 of 34, bank `banks_union9l`); `gen_translated.py --check` rc 0. Both run on the final tree.
- **Slim captures (Main's priority task, done first):** 105 captures trimmed, 382.70 GB to 0.85 GB, 0 restored. 76 live2 captures were written after that pass and remain full (§5).
- **Callees (task 4):** decoded and classified; none modelled (§4).

## 1. Generic stub fix (task 1)

- **Red (committed tooling, callee rows empty, the state the RefReplay2 doc reproduced).** Specs `hooks/gen2/specs/140A39A80.json` (`stub_refs: []`). `snapdiff.py --only 0x140A39A80` over `seq058,060,062,064,065,066`: 0 of 6 bit-exact, every replay aborts with `PES_DISPATCH_UNTRANSLATED: 0x14159f3e0` (CONFIRMED, `snap_red_gen2_emptyrows.json`).
- **Cause (CONFIRMED).** `specgen/impure.py harness_fields` took `stub_refs` from the row's direct callees only. The five referee rows (`140A38F20`, `140A39A80`, `140A3A690`, `140A3A870`, `140A3A930`) are absent from `universe.json` (the bridge crawl never reached them). With empty callees the cookie check `0x14159F3E0` (`__security_check_cookie`) and `sqrtf` `0x1415A1780` never reached the stub map. The translated closure called the cookie check through the dispatch table and aborted.
- **Fix.** `classify.reach_stubs(va)` walks every direct CALL/JMP target of the closure, pilot bodies included, and stops at named models. `harness_fields` takes `stub_refs` from it (no per-hook list). Universe rows: `tools/dev/ledger/extend_rows.py` adds rows from the image (sizes from the `.pdata` rows in FoulOffside's `stoppage_rows.json`, copied to `extend_rows.json`; callees from the image's direct calls). The Ghidra bridge answered HTTP 000 (CONFIRMED), so the bridge crawl was not usable.
- **After.** Same empty-callee specs regenerated (`regen.py emptyrows`): `snap_fix_emptyrows.json`, 6 of 6 bit-exact, proven. Specs with the new rows (`regen.py committed`): `snap_fix_rows.json`, 6 of 6, proven.
- **Committed red/green check.** `tools/dev/diffharness/check_closure_stubs.py` (`harness_fields(0x140A39A80, [])` must carry `14159F3E0` and `1415A1780`): HEAD tree RED (`stub_refs []`, both missing); this tree GREEN. The HEAD run is the revert of the fix.
- **Committed impure specs.** `stub_refs` identical for 50 of 50 (`cmp_specs.py`, `specs_impure_new` vs `specs/impure`). Three specs (`14051B8B0`, `14065C2A0`, `140978E70`) differ from the committed copies in `regions`, `inputs`, `closure_fix`. The HEAD generator gives the same output for them (`specs_three_HEAD` = `specs_three_new`), so this drift predates the change (CONFIRMED); not regenerated here.
- **Note (INF).** With real rows, `A39A80` was already green through `classify`'s recursion over row callees. The fix matters when rows are sparse. The pilot walk adds no stub to any of the 10 hook specs (`reach_gap.py`: ok for all 10).

## 2. Verdict table: every foul/offside/restart hook over all captures in live/ and live2/

Captures: every `live/seqNNN` and `live2/seqNNN` with an `index.json` whose `rv.hook` is one of the 10 (74 total). Method: `frame_trace.py recorded` per capture with `hooks_live2.cfg` and the 10 hook specs (`specs_fix_rows`), then one `snapdiff.py` over all 148 snapshot pairs (`sweep.json`, 957 s, rc 0). PROVEN = every replayed call bit-exact (rax, xmm0, every written byte), none excluded (owner ruling 26-10-26). Counts are per call; captures are per snapshot.

| hook | VA | captures (live / live2) | calls | bit-exact | excluded | mismatched | verdict |
|---|---|---|---|---|---|---|---|
| foul_sanction_47F170 | 0x14047F170 | 10 (6 / 4) | 20 | 20 | 0 | 0 | PROVEN |
| foul_sanction_caller_480910 | 0x140480910 | 10 (6 / 4) | 10 | 7 | 0 | 3 | NOT PROVEN |
| ref_involve_A3A690 | 0x140A3A690 | 7 (6 / 1) | 26 | 26 | 0 | 0 | PROVEN |
| ref_judge_A3A930 | 0x140A3A930 | 7 (6 / 1) | 14 | 14 | 0 | 0 | PROVEN |
| ref_latch_A3A870 | 0x140A3A870 | 7 (6 / 1) | 7 | 7 | 0 | 0 | PROVEN |
| ref_restart_cmd_A39A80 | 0x140A39A80 | 7 (6 / 1) | 7 | 7 | 0 | 0 | PROVEN |
| linesman_lookat_A38F20 | 0x140A38F20 | 7 (6 / 1) | 7 | 7 | 0 | 0 | PROVEN |
| restart_446390 | 0x140446390 | 8 (7 / 1) | 8 | 8 | 0 | 0 | PROVEN |
| restart_44C880 | 0x14044C880 | 7 (6 / 1) | 11 | 0 | 11 | 0 | NOT PROVEN |
| restart_driver_445E90 | 0x140445E90 | 4 (3 / 1) | 4 | 0 | 4 | 0 | NOT PROVEN |

- **480910, 3 mismatches (CONFIRMED):** `PES_DISPATCH_UNTRANSLATED: 0x6FFFFEC77E00` (an import-page thunk, no named model). The two cases the previous run excluded (`seq085`, `seq089`) now run to this abort. The remaining 7 calls are bit-exact.
- **44C880 excluded (CONFIRMED):** `unmodellable callee 0x1404488E0` (7), `0x140449B00` (3), `0x1415A1780` (1, the sqrt model; why the harness refuses it is OPEN).
- **445E90 excluded (CONFIRMED):** `unmodellable callee 0x140440FA0` (4 of 4).
- **Not a regression (INF):** the sweep reads captures both before and after the slim pass; the trimmed captures replay identically (§5).

## 3. SSE4.1 emulator re-apply (task 3)

- `c1e590a8`: cherry-pick of `02ecfd72` onto `d8a61a62`, clean apply (BROADWELL CPU model in `run.new_x86_64()`).
- `driver.py --check --spec-dir specs/impure` with `GF_IMPURE_BANKS=banks_union9l` (per-VA corpus, `97901447`): rc 0 (`driver_check_final.log`). Earlier run on `c1e590a8`: "all 34 recorded-diffed functions still diff bit-exact".
- `gen_translated.py --image PES --check`: rc 0 (`gen_check_final.log`).
- **480910's two excluded cases:** no longer excluded; they abort at the import thunk (§2). Whether `pmovsxbd`/`pmulld` run in the 7 passing calls is not checked (OPEN). The closure built with no translator error (CONFIRMED).

## 4. Callees of the restart hooks (task 4): decoded, not modelled

Image reads with capstone (the bridge is down). Classification with `specgen.classify` as the generator uses it.
- **`0x1404488E0`** (45 B, `restart_44C880` caller): a six-case switch. `add edx,-0xc; cmp edx,5; ja`, table at `0x1404489AC`, `r8` = image base (`lea r8,[rip-0x448900]`). Table entries (CONFIRMED decode): cases 12..17 go to `0x14044892C, 0x14044892C, 0x14044890D, 0x14044897F, 0x140448948, 0x14044896F`. The blocker is the computed jump at `0x14044890B` ("switch case bodies not enumerated"). Its case bodies make indirect calls (`[rax+0x168]` in `0x1404489D0`; `[rax+0x10]`, `[rdx+8]` via `0x140448C90` → `0x140422F20`). Needs: the case bodies in the universe with their callees, then their indirect targets (vtables) from a live trace.
- **`0x140449B00`** (80 B, 16 universe callees): unmodellable. Indirect calls at `0x1404513B7`, `0x1404513C3` (in `1404513A0`), `0x14047D638` (in `14047D5D0`, via `140451FD0`), `0x14044595A` (in `140445910`), `0x141B5583F`; a computed jump in `14050D280` (via `14050C490`). Needs: indirect targets from a live trace. A static closure cannot be built from the bytes. OPEN.
- **`0x140440FA0`** (9 B, leaf): `test edx,edx; js; cmp edx,0x16; jae`. `classify` says run-real. The replay still excludes it as "unmodellable callee entered" (4 of 4), so the refusal is in the row or the harness, not the bytes. Why: OPEN (not traced further).

## 5. Slim captures (Main's priority task, done first)

- **Tool:** `tools/dev/rendezvous/slim.py` (`a9946f5d`, README paragraph). Replays each capture's recorded call with every `pread` of `mem.bin` logged (4 KiB file pages). The kept set is the read set plus one page either side. `mem.bin` is rewritten as a sparse file of the same size and offsets. The slim capture is replayed; it must read only kept pages, return cleanly, and produce byte-identical output (`tree_hash`) before the full file is deleted. Otherwise the full file is restored.
- **First pass** (every seq dir with `mem.bin`, `index.json`, `call.bin` and mtime older than 10 min at start): 105 trimmed, 0 restored, **382.70 GB to 0.85 GB** (`slim_all.log`). The 3.6 GB test capture `seq058` trimmed to 8.6 MB first (verified replay).
- **Measured now** (`st_blocks`): `live/` 98 dirs: 96 slim (0.76 GB), 2 full (`seq017`, `seq018`: no `index.json`/`call.bin`, not replayable). `live2/` 86 dirs: 10 slim (0.10 GB), **76 full (278.7 GB)**. The 76 were written by the watcher after the first pass and are still being written. Main's 10-minute rule kept them untouched. Disk now: `live` 6.5 GB, `live2` 260 GB. A second `slim.py` pass is needed once the watcher stops.
- **Mistake, disclosed:** at 08:40 a script was written into the main checkout by mistake (`<local>/Code/GameplayFootball/tools/dev/ledger/extend_rows.py`, untracked). It was removed at once; nothing was committed from there.

## 6. Checks and pushes

- `driver.py --check` (strict impure, bank): rc 0. `gen_translated.py --check`: rc 0. Both run at the final code (`c1e590a8` and later; `a9946f5d` only adds the rendezvous tool and README, which neither check reads).
- Red/green: `check_closure_stubs.py` (HEAD RED, worktree GREEN); hook replays (§1: 0/6 red, 6/6 green twice).
- Pushed: `d8a61a62`, `c1e590a8`, `a9946f5d`. Gate not run (Main gates). Smoke not run (not in scope).
- Tests run: `check_closure_stubs.py` (1), `driver.py --check` (34 functions), `gen_translated.py --check` (3951 functions), `snapdiff` (10 hooks, 74 captures).

## 7. Open items (numbered)

1. **Import thunk `0x6FFFFEC77E00`** (480910: 3 aborts). Needs a named import model; `import_slots.json` names image slots `0x14252Fxxx` only (OPEN).
2. **44C880:** `1404488E0` switch case bodies and their indirect targets; `140449B00` indirect dispatch (live trace of targets needed); harness refusal of `0x1415A1780` (OPEN).
3. **445E90:** harness refusal of `140440FA0`, a leaf (OPEN).
4. **Second slim pass** on the 76 live2 captures written after the first pass, when the watcher stops (needs Main's go).
5. **Impure spec drift:** `14051B8B0`, `14065C2A0`, `140978E70` differ from the generator at HEAD in `regions`/`inputs`; pre-existing; not regenerated.
6. **Universe rows:** only the five referee rows added (from the image). The bridge-driven `--extend` mode (RefReplay2 §5) is still open; the bridge is down.
