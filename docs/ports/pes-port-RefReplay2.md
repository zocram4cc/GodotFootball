# RefReplay2: strict impure check, CFG cookie stub, referee verdicts (28-10-26)

Branch `RvReplay`, pushed `97901447` (on `0b6c7622`; `git ls-remote origin RvReplay` = `97901447`). Worktree `<workdrive>/gf-RvReplay`. Scratch `<workdrive>/gf-RefReplay2-scratch-out` (PES-derived outputs, not committed). The `origin/main` scratch worktree was removed. Gate not run (Main gates). Labels: CONFIRMED = read in tool output this session; INF = inferred, reason given; OPEN = not settled.

## 0. Summary

- **Strict impure check: GREEN** (`driver.py --check --spec-dir specs/impure --seed-bank banks_union9l`: attempted 34, translated 34, diffed 34, rc 0). Fixed and pushed (§1).
- **Red was not a branch regression.** `origin/main` (2cbadbd2) fails the same way without a bank (§1). The BROADWELL emulator change was not the cause.
- **CFG cookie stub `0x14159F3E0`:** identified as MSVC `__security_check_cookie` (§2). Modelled in a scratch spec, `ref_restart_cmd_A39A80` goes from 0/6 (abort) to **6/6 bit-exact**. The generic fix (transitive `stub_refs`, universe rows) is **OPEN** (§2, §5).
- **SSE4.1 emulator redo: NOT DONE** (OPEN). `foul_sanction_caller_480910` 2 excluded cases: not re-run (§3).
- **`restart_44C880` / `restart_driver_445E90` callees: partly analysed** (§4). Not modelled.
- **Ledger `universe.json` rows: NOT ADDED** (OPEN, §5).

## 1. Strict impure check (task 1)

Baseline on a clean `origin/main` scratch worktree (`--spec-dir specs/impure --record results_impure.json`, no bank): 33 of 34 diffed. **Red:** `0x1404386D0`: `corpus empty (0 cases); 0 valid of 64 requested; 128 failed skipped`. Every case fails with `unmodelled read at 0x212 (size 1)`; the others fail at small addresses that change per case (0x129, 0x144, 0x21a, 0x2c9…) (CONFIRMED, direct `run.py` probe).

Regression check: the same probe fails identically at the record commit `8e2cb407` (Oct 27), so no commit since then caused it. The record itself was measured with a bank (`banks_union9l`, the 9-capture union in `gf-Captures-scratch`).

Cause (CONFIRMED): the spec's u64 fields (`A0_8_18_68`, `A0_8_18_E0_18`, `A0_8_18_E0_18069C`, `IMG_1436F3DC8_0`) carry the generator's generic `u8 0..255` domain (`input_schema`, "width-typed"). Pointer and handle values drawn from it dereference small addresses. The bank holds their real values: the same probe with `--seed-bank banks_union9l` gives 8 valid, 0 failed (CONFIRMED). No generator domain can supply those values without a bank (INF: they are heap/handle values, not region bases).

Bank for all 34: not correct. `1404FB2A0` drops to 18 valid of 64 with the bank (CONFIRMED), and is 78 valid without it (CONFIRMED, main run).

Fix (commit `97901447`): each result names its corpus (`results_impure.json` `"corpus": "bank"`, set on `1404386D0` only). `driver.py` replays a bank-recorded VA with `--seed-bank`, every other VA with the random corpus (`seed_bank_for`). A bank result without `--seed-bank` is refused (rc 2). CMake `diff_impure_recorded` passes `GF_IMPURE_BANKS` when set. Mutation: check run with the bank withheld gives `--check: 0X1404386D0 record corpus 'bank': give --seed-bank`, rc 2 (CONFIRMED).

Results on RvReplay, bank run before provenance: 33 diffed (`1404FB2A0` red). After: 34 of 34 (CONFIRMED, `rv-green`). Not re-run: `gen_translated.py --check` (no translated file changed; the doc's earlier run was green).

## 2. CFG guard `0x14159F3E0` (task 2)

- Bytes (CONFIRMED, capstone): `cmp rcx,[rip+0x1F6BF59]` (global `0x14350B340`); `jne` to slow path; `rol rcx,16; test cx,0xffff; jne`; `ret`; slow path `jmp 0x14159FBF8`. This is MSVC's `__security_check_cookie` (INF from shape; name agrees with `stubs.json`'s `engine-noop` entry, which models the success path as no effect).
- The abort (CONFIRMED in generated code): `pes_fn_*.cpp` of the closure contain `/*OUT-OF-SET CALL 0x14159F3E0*/ pes_dispatch_missing(0x14159F3E0)`. Closure members are translated with the root spec's `stub_refs` only (`snapdiff.build_closure`), and the root spec for `A39A80` has `stub_refs: []`.
- Why `stub_refs` is empty (CONFIRMED): `specgen/impure.py harness_fields` takes stub refs from the **direct** callees of each row, and `A39A80`'s direct callees do not include the cookie check (it is called from `140517CF0` and others). The universe rows for the referee functions do not exist (`A39A80`, `A3A690`, `A38F20`, `A3A930`, `A3A870`), so the scratch spec had no callees at all.
- Experiment (scratch only): copy of `A39A80`'s spec with `stub_refs = [14159F3E0, 1415A1780]` (the cookie check and the `sqrtf` thunk), same six captures (`seq058,060,062,064,065,066`), `snapdiff --only 0x140A39A80`: **calls 6, replayed 6, mismatches 0, own-block 8/39 = 0.21** (CONFIRMED, `snapdiff_guard.json`). Verdict **PROVEN** on this model.
- Model caveat (INF): the stub models the success path only. A case whose cookie check fails would go to `__report_gsfailure` (0x14159FBF8); none of the six captures does (they replay bit-exact).
- Generic fix **OPEN**: (a) make `harness_fields` compute `stub_refs` over the transitive closure of out-of-set callees, and (b) add the referee rows to the universe with their callees. Changing (a) re-derives every committed impure spec's `stub_refs`, so it needs a full impure re-record.

## 3. `foul_sanction_caller_480910` (task 3): not done

- Not re-run this session. Recorded verdict from the doc (RvReplay run on `0b6c7622`): 6 calls, **4 bit-exact, 2 excluded** at `0x141B700CC` (`pmovsxbd`, SSE4.1), verdict NOT PROVEN.
- The BROADWELL emulator change (`02ecfd72`, reverted by `0b6c7622`) was not reapplied. On the BROADWELL run the two calls reach `PES_DISPATCH_UNTRANSLATED: 0x6FFFFEC77E00`, an import-page thunk (doc §2a).
- Also needed (OPEN): the translator has no SSE4 mnemonics (`xlate.py emit_sse` whitelist), so `pmovsxbd`/`pmulld` must be translated, and the import thunk needs a named model (`import_slots.json`).
- The BROADWELL change is not the cause of the strict red (§1), so it can come back without touching `1404386D0`; its strict check must be re-run with the bank after reapplying.

## 4. Unmodellable callees (task 4)

| callee | caller | what the record says | status |
|---|---|---|---|
| `0x140A79640` | (`restart_44C880`) | `stubs.json` unmodellable: indirect calls at `141E535F9`, `141E5362F` (in `141E53540`); import slot `14252F6E8` has no name in the image; global stores at `1403FB8A9`, `1403FB622` | CONFIRMED (stub `why`). Needs: the dispatch targets in the closure, a named import model for `14252F6E8`, and the globals it writes. |
| `0x1404488E0` | `restart_44C880` (x7) | no `stubs.json` entry: the harness runs its image bytes; blocker in the spec's `harness` field | OPEN: not decoded this session |
| `0x140449B00` | `restart_44C880` (x1) | as above | OPEN |
| `0x140440FA0` | `restart_driver_445E90` (x3) | as above | OPEN |

The doc's "vtable, TLS, import" guesses are not verified. What is settled: none of these callees is in the stub table except `140A79640`, so the harness runs them as real bytes, and their closures are what block the replay.

## 5. Ledger universe rows (task 5): not added

- `specgen/sets.py` and `impure.py` read `tools/dev/ledger/universe.json` `functions` (CONFIRMED). The referee VAs are absent (CONFIRMED: `A39A80`, `A3A690`, `A38F20`, `A3A930`, `A3A870` are not keys; `14047F170`, `140445E90`, `14044C880`, `141E53520`, `140480910` are).
- `FoulOffside`'s `stoppage_rows.json` carries sizes and subsystems for them, but `callees: {}` for every row (CONFIRMED), so it cannot produce `stub_refs`.
- A full crawl (`universe.py`, bridge-driven, 37,855 functions, one request at a time) is not viable here. OPEN: a `--extend` mode that crawls only added seeds and merges their rows, with the callees taken from the image.

## 6. Verdict table (per hook)

Counts are per capture (snapshot). Rows marked "doc" are from the RvReplay run on `0b6c7622` (`docs/ports/pes-port-RefReplay.md` §2), not re-run here. Row marked "scratch" is this session's experiment.

| hook | VA | captures | calls | bit-exact | excluded | verdict | source |
|---|---|---|---|---|---|---|---|
| foul_sanction_47F170 | 0x14047F170 | 12 | 12 | 12 | 0 | PROVEN | doc |
| foul_sanction_caller_480910 | 0x140480910 | 6 | 6 | 4 | 2 (SSE4.1) | NOT PROVEN | doc (§3) |
| ref_involve_A3A690 | 0x140A3A690 | 22 | 22 | 22 | 0 | PROVEN | doc |
| ref_judge_A3A930 | 0x140A3A930 | 12 | 12 | 12 | 0 | PROVEN | doc |
| ref_latch_A3A870 | 0x140A3A870 | 6 | 6 | 6 | 0 | PROVEN | doc |
| ref_restart_cmd_A39A80 | 0x140A39A80 | 6 | 6 | **6** | 0 | **PROVEN** (was FAIL 0/6) | scratch (§2) |
| linesman_lookat_A38F20 | 0x140A38F20 | 6 | 6 | 6 | 0 | PROVEN | doc |
| restart_446390 | 0x140446390 | 7 | 7 | 7 | 0 | PROVEN | doc |
| restart_44C880 | 0x14044C880 | 6 | 9 | 0 | 9 | NOT VERIFIED (callees, §4) | doc |
| restart_driver_445E90 | 0x140445E90 | 3 | 3 | 0 | 3 | NOT VERIFIED (callee, §4) | doc |

Proven: 7 of 10 hooks (8 counting the scratch A39A80 model as proven only under that model).

## 7. Open items (numbered)

1. Generic `stub_refs`: transitive over the out-of-set closure in `specgen/impure.py`, plus a universe rows path (§2, §5). Re-record impure after.
2. SSE4.1: reapply `02ecfd72`'s BROADWELL constructor, re-run the strict check with the bank, and translate `pmovsxbd`/`pmulld` (§3).
3. `480910`: re-run the two excluded calls after (2), then name the `0x6FFFFEC77E00` import model (§3).
4. Callees `1404488E0`, `140449B00`, `140440FA0`, `140A79640`: decode and say what each needs (§4).
5. Universe rows for the referee functions with callees, via an extend mode (§5).
6. `gen_translated.py --check` not re-run in this session (no translated file changed).
7. Scratch-only PES-derived specs and verdict JSON stay in `gf-RefReplay2-scratch-out` (not committed).
