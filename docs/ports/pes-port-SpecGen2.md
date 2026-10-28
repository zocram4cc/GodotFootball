# pes-port-SpecGen2: one global stub table, recorded seeds, the EndClear chain

Branch `SpecGen`, pushed head `70cacd7d` (`git ls-remote origin SpecGen` = `70cacd7d`). Commits: `d92149ca` (stub table, EndClear chain, VA seeds for new specs), `dd492f5d` (merge of origin/main AL2, which carries RvReplay's per-closure `stub_refs`), `8ced0887` (recorded seeds restored), `70cacd7d` (docs). Gate not run (Main gates). Labels: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled.

## 0. Result

- **One stub choice (CONFIRMED):** `tools/dev/translate/stubs.json` decides every callee for every spec set. A callee in `stubs.MODEL_DOS` (`sqrt_xmm0`, `noop`, `abort`) is always its model; any other callee is always its translated body (run-real) or blocked. The four models are `1415A1780` (sqrtf, return-input), `14159F3E0` (`__security_check_cookie`, engine-noop), `1415A1714` and `14159FCCC` (tamper-trap import thunks, abort). `libm` entries (`1415A17CE`, logf/powf) have no emitter and stay blocked, as before.
- **Per-spec `stub_refs` removed:** 50 impure and 3 core specs carried them; the entries were already table models (`14159F3E0`, `1415A1780`), so the merge is validation plus removal. `stubs.check_spec_refs` fails at spec load if a legacy `stub_refs` names anything that is not a model (CONFIRMED: `140421F40`, run-real, is rejected; `1415A1780` and `14159F3E0` are accepted).
- **Readers that now take the table:** `driver.MODEL_MAP` (replaces `stubmap_of` and `ABORTMAP`), `snapdiff.py`, `gen_translated.py`, `run.py resolve_stubs`, `specgen/classify.py` (its `STUB` dict is built from `stubs.json`), `closure.py` (a model is never a member). `driver.universe_of` raises if a model VA is also a spec (the ruling's "never both").
- **Seeds (changed from the plan; see §3):** recorded seeds are frozen (restored to the values committed at `d14dc42b`, 4,636 specs). A new spec with no record takes `1000 + VA offset` (`specgen/spec.py` `seed_of`), which does not depend on set order.
- **EndClear chain landed:** the five records are flipped in `results_core.json` to `diffed: true, proof: "snapdiff"` with counts read from `results_endclear_clear.json` and `results_endclear_angle.json` (all replayed bit-exact, no mismatches): `0x142109EB0` 5/5, `0x142108630` 6/6, `0x14063A290` 6/6, `0x14063A490` 6/6, `0x14063D2F0` 12/12. `driver --check` skips `proof: snapdiff` records (their corpus is captures, not the unicorn harness). `closure.py --tag endclear` ran: 84 members, 3 universe rows added, 74 committed specs kept, 10 new specs written; `specgen/sets/closure-endclear.txt` committed.

## 1. Checks (all run on the pushed tree, after the merge of origin/main)

| check | result |
|---|---|
| `gen_translated.py --check` (merged tree) | rc 0: 3961 functions (3679 recorded bit-exact, 282 closure), regenerate byte-identical; 0 conflicts; held out 5 (unchanged) |
| `driver.py --check` pilot (`results.json`) | 245/245 still bit-exact |
| `driver.py --check` impure (`results_impure.json`, `GF_IMPURE_BANKS=<workdrive>/gf-Captures-scratch/banks_union9l`) | 34/34 |
| `driver.py --check` presentation | 546/546 |
| `driver.py --check` cutscene | 685/685 |
| `driver.py --check` core | 2253/2253 (every recorded-diffed record except the five snapdiff ones) |
| `check_closure_stubs.py` (repointed at `reach_stubs`, since `harness_fields` no longer emits `stub_refs`) | GREEN: closure models `14159F3E0`, `1415A1714`, `1415A1780` |
| `stubs.check_spec_refs` | red/green as above (run-real ref rejected, model refs accepted) |
| `py_compile` on every changed module | OK |

Test binaries run: none (no C++ changed). Gate not run (Main gates).

## 2. Verdict changes from the stub change

- **None** in any set. Every recorded function that re-ran is still bit-exact under the global table (§1).
- **Layer text changes (CONFIRMED, `gen_translated --check` before regeneration: 27 drifts):** the new table gives the sqrt and cookie models to closures that used to carry an out-of-set gap. Changed bodies: `ai/pes_fn_1405A7FE0`, `ai/pes_fn_140A51D50`, `ball/pes_fn_140582CB0`, `cutscene/pes_fn_1403D2E60`, `gameplay/pes_fn_1405A6910`, `gameplay/pes_fn_140615180`, `gameplay/pes_fn_140663C50`, `gameplay/pes_fn_140667480`, `gameplay/pes_fn_140667B90`, `gameplay/pes_fn_140667CB0`, `gameplay/pes_fn_14066B980`, `gameplay/pes_fn_140A23380`, `gameplay/pes_fn_140A6FF50`, `gameplay/pes_fn_141FEE390`, `gameplay/pes_fn_141FF52B0`, `replay/pes_fn_141B50460`, `restart/pes_fn_140511020` (plus `translated.hpp` and `pes_dispatch_gen.hpp`, which add the five records). The regenerated layer is committed; the checks above run against it.
- **Impure, first run only (INF, then CONFIRMED):** `0x1404386D0` showed "no longer diff" in my first impure run because that run had no capture bank (its record has `corpus: bank`; Main's merged driver refuses such a check without `--seed-bank`). With the bank it is 34/34.

## 3. Seeds: VA-derived seeds broke recorded verdicts, so the seeds were restored

- **Trial (CONFIRMED):** I first set every spec's seed to `1000 + VA offset` (commit `d92149ca`). Under that rule, 11 recorded functions lost bit-exact status: presentation `0x1422EFAE8`, `0x142329604`, `0x1423298E0`, `0x1423317F8`; cutscene `0x140227D20`, `0x140485E30`, `0x1408CCF50`, `0x1408CE110`, `0x1408D46B0`, `0x1408DA0F0`, `0x141F21530`.
- **Attribution (CONFIRMED, `attr_seed.py`):** the same 11 functions re-run with their old seed and the new stub table are bit-exact again (all 11). So the seed change caused the losses; the stub change did not.
- **Decision:** the ticket allows "else document why not". The seed is part of a record's input, so committed seeds are frozen: `8ced0887` restored all 4,636 committed seeds from `d14dc42b`. New specs (the 10 closure additions) keep `seed_of(VA)`. Set order no longer changes a new spec's seed.
- **The "72 committed core specs differ by seed" figure (OPEN):** I could not reproduce it. The committed core seeds equal `1000 + position` in `sets/core.txt` for all 2856 listed specs (0 mismatches); the 6 unlisted core specs are closure additions. The drift therefore comes from regenerating a set under a different order (the closure set file), not from the committed layer. Regenerating a recorded set now would still change its seeds; do not do that without re-running its check.

## 4. Other facts

- **EndClear caveat (CONFIRMED from `pes-port-EndClear3.md` §0):** `0x142109EB0` is proven on its danger-0 arm only (5/5). The danger-1 arm (store answer type 5, angle call, float store) has no real call in the captures. The flip records what was proven; it does not claim the danger-1 arm.
- **Closure specs (CONFIRMED, `closure.py` keeps committed specs):** of the 84 members, 6 committed specs differ from the generator's output in regions, inputs, status and subsystem (`14063A290`, `14063A370`, `14063A490`, `14063C560`, `142108630`, `142109EB0`). They were hand-edited by EndClear and are kept, since their snapdiff proofs ran against them. None differs in `seed`.
- **Conflicts (INF on the old count):** the flipped regeneration under the new table has 0 conflicts (the build raises on any). I did not re-run the old per-spec regime to reproduce the 7 conflicts (the README records `14069CD40` and `140A760E0` as the callees that differed); the oracle "7 to 0" is therefore met on the outcome, not re-measured.
- **Git:** plain `git status`, `fetch`, `merge` fail on the `extern/godot-cpp` symlink. Used `git -c submodule.recurse=false -c diff.ignoreSubmodules=all -c status.submodule=none` and `-c push.recurseSubmodules=no`.
- **Scratch (not committed):** `<workdrive>/gf-SpecGen-scratch/` holds `migrate_specs.py`, `flip_endclear.py`, `restore_seeds.py`, `attr_seed.py`, the check logs (`check3-*.log`, `stream3.log`, `gen_check_merged.log`). The spec-file rewrites were data migrations done by script, not edits; the source edits used the edit tool.

## 5. Open items

1. **Impure harness metadata not regenerated (OPEN):** the committed impure specs' `harness` blockers and `out_of_set_callees` were written before the table gained the abort thunks (`1415A1714`, `14159FCCC`). They still say "import slot", so those specs stay "harness cannot run" as before. Regenerating them would change verdicts; not done.
2. **`libm` (OPEN):** `1415A17CE` (logf/powf) has `do: powf_xmm0`, which no emitter implements. It stays blocked. A model needs an emitter and a host-libm bit-exactness note.
3. **Bank dependency:** `0x1404386D0` needs `GF_IMPURE_BANKS` (PES-derived, not committed). The gate must set it.
4. **Seed drift (OPEN, §3):** the "72" count is not reproduced; recorded seeds are frozen instead.
5. **Clear danger-1 arm (OPEN, §4).**

## 6. Next highest-value step

Regenerate impure harness metadata under the table in a branch with its check (open item 1), since that is where abort-trap callees still block specs. Rationale: it is the only place where the table and the committed harness data still disagree.
