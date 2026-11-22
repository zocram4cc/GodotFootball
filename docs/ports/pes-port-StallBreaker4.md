# pes-port-StallBreaker4: the 0x140789D00 abort is the SLIDING action's vtable slot +0x90; three untranslated action handlers reached from one call site; closure sized; NOT translated, NOT proven, no SHA

Status: PARTIAL. Step 1 diagnosis is complete and CONFIRMED. No translation, no proof and no new commit. Branch `StallBreaker` stays at **09a5f7e2** (pushed, unmerged, per StallLive13). Steps 2 (walker) and 3 (acceptance) were not run because step 1 is not closed.

## 1. What the abort is (CONFIRMED)

- The image holds exactly one pointer to 0x140789D00, at **0x1425E1908** (scan of `pes_full.bin`, offset = VA - 0x140000000). 0x1425E1878 is the SLIDING action vtable base (walker decode, R2), and its slots read: +0x20 0x140786D60, +0x50 0x140788C00, +0x70 0x140787EC0, **+0x90 0x140789D00**. So 0x140789D00 is SLIDING's +0x90 handler.
- Survey run (`GF_PES_SURVEY=<file>`, pes_hang_test, seed 11; the file is `gf-StallBreaker-scratch/survey_hits.txt`, run killed at its 1,600 s budget after the match had produced 262 target lines): three untranslated targets are reached from ONE caller site, return offset 0x443c8f in `pes_fn_1406F6CE0` (nm of the test binary: function at 0x4402D0, offset 0x39BF):
  | target | hits | first frame | image slot (CONFIRMED) | action |
  |---|---|---|---|---|
  | 0x14073DD50 | 182 | 239 | 0x1425DF8B0 | not named (INF) |
  | 0x140789D00 | 152 | 186 | 0x1425E1908 | SLIDING +0x90 (CONFIRMED) |
  | 0x140791DD0 | 1 | 397 | 0x1425E1AD8 | not named (INF) |
- The earlier "abort at 0x140789d00" in the census is the first of these to fire in the non-survey run. The other two are behind it and would abort next.
- The 257 entries with caller 0x1EA768 at frame 1 are boot-time constructor hits; they are not the match-time failure.
- `pes_fn_1406F6CE0` is translated (`kickjoints/pes_fn_1406F6CE0.cpp`, dispatch line 16046). Its indirect call through the action slot is the one the table cannot resolve.

## 2. Closure (CONFIRMED by `closure.py --dry-run`, nothing written)

`closure.py --image pes_full.bin --spec-set core --tag slide --dry-run 0x140789D00 0x14073DD50 0x140791DD0` gives **4 members from 3 roots**, all already in the universe: the three roots (AI, seed) plus `0x141E532D0` (callee). Small; the translation itself is not large. The hard part is the proof.

## 3. What is missing (OPEN)

1. **Proof.** No real capture covers 0x140789D00, 0x14073DD50 or 0x140791DD0 (no hook exists; `live2/` has none). Method per Main's ruling: a labelled synthetic set over a real trap/slide-adjacent capture's memory, as in `pes-port-RestingBall6` section 4 (`synth_trapvirt_specs.py` + `closure.py --spec-set ... --tag ...` + `prove_bounded.py --merge`). The synth generator for these three is not written.
2. **Layer entry.** These three are not in `src/pes/translated`, so `untranslated_callees.py --hooks` refuses them ("not translated"). A hook line for a real capture can only be produced after the translation lands. Hook request, to be sent to Main once translated: `EVERY 1` with an in-play predicate, cap 6 (slides are rare in COM play), for 0x140789D00, plus the two other slot targets if they are to be proven too.
3. **Registration.** Gen_translated reaches a closure member only through a recorded caller's closure (CALLEE regex or `INDIRECT_EDGES`). The caller here is reached through a vtable, so the edge `pes_fn_1406F6CE0 -> {0x140789D00, 0x14073DD50, 0x140791DD0}` probably needs an `INDIRECT_EDGES` entry (CONFIRMED mechanism, gen_translated.py:58-64), and `1406F6CE0` must be a recorded member of a results set for the closure to be committed. Not checked.
4. **Gen run.** `gen_translated.py --image` takes ~20 min and then `--check`. Not run.

## 4. Stray and clean-up
- A file `./1` (6,621 bytes, left by my first survey run with `GF_PES_SURVEY=1`) was moved to `gf-StallBreaker-scratch/stray1.txt`; the worktree has no untracked file from it.
- `build/` in the worktree is still present (step 1 needs it). `rm -rf build` once the round is closed.

## 5. Next steps (in order)
1. Write the synth spec generator for the three slot targets over a real trap/slide-adjacent capture's memory (RestingBall6 method), record a `closure_slide.json` and results entries.
2. Add the `INDIRECT_EDGES` entries for `1406F6CE0`, run `gen_translated.py --image`, check `--check` and the `PES_DISPATCH_UNTRANSLATED` abort at seed 11 and seed 7.
3. Message Main the hook line request (EVERY 1, in-play predicate, cap 6) for 0x140789D00 and the two other slot targets.
4. Then the walker red test and census (step 2), and the acceptance set (step 3).

## 6. Verification run this round
| Check | Result |
|---|---|
| survey (pes_hang_test seed 11, GF_PES_SURVEY) | 262 target lines, three match-time targets from caller 0x443c8f; run killed at 1,600 s |
| image slot scan | one pointer per target; 0x140789D00 = SLIDING +0x90 |
| `closure.py --dry-run` | 4 members from 3 roots |
| `untranslated_callees.py --hooks` | refused: targets not in the layer (expected until translated) |
| tests, builds, gen, ledger | not run this round (no source change) |
