# CutsceneAnchor16 (CutWorld): the group table and element path are proven and pushed; the world-build wiring hangs in GF (diagnosed to one call, not fixed)

Branch `CutsceneAnchor`, pushed head `c2ddb4785cf78ca78fcc2df4347351ea6be8bea4` (`git ls-remote origin CutsceneAnchor` confirms). The wide test is NOT green. Labels: CONFIRMED = run or read this round; INF = inference; OPEN = named blocker.

## 0. Status against the assignment

- Item 1 (translate and prove the element path and nine-path loop): PARTLY. 0x141E5BAF0 and 0x141E5BC40 are proven bit-exact on synthetic states (CONFIRMED). 0x141E5BEE0 is entered by the 0x141E5BC40 proof. 0x141E5C060 (the `.bin` branch) is not entered. The nine-path loop 0x140419610 is not proven; GF calls the nine paths directly.
- Item 2 (wire the world build; node 0x71 equals PES's live values): NOT landed. The wiring compiles but never returns (section 3). Reverted from the worktree; the patch is kept at scratch `cutworld/wiring.patch` (369 lines).
- Item 3 (key feed): not reached.
- Item 4 (wide test green, mutations, neighbours): NOT done. The HEAD wide test is unchanged (9 EXPECT failures, CutsceneAnchor10).

## 1. Proofs (CONFIRMED, on HEAD)

| Root | Synthetic states (over `cutlib/cstates/create_st001`, seq321 memory) | Result | Own-block | Record |
|---|---|---|---|---|
| 0x141E5BAF0 (group table init) | `null` ([0x1437F4800]=0), `set` (nonzero) | 2/2 bit-exact | 8/8 | results_trapaction.json, closure_cutbaf.json |
| 0x141E5BC40 (path -> group) | `stadium_empty`, `match_empty`, `stadium_present` | 3/3 bit-exact | 6/51 | closure_cutbc40.json |

Layer: `gen_translated.py --check` byte-identical (5803 functions, 3771 recorded bit-exact, 2032 closure). Ledger `--check` current (44518 rows). Commit `c2ddb478`.

## 2. Boundaries and stand-ins (labelled)

- Heap native services 0x1403FBC10, 0x1403FBCB0, 0x141B34CB0 are native per stubs.json and are excluded from the closure.
- ro_tables: 0x142B48384 (0x20 bytes) declared read-only (0x141E5BC40 walks the literal `lst`/`.bin` bytes).
- The closure set holds only the entered members. A full closure of 0x141E5BC40 (632 specs) breaks `gen_translated` on 0x140388760 (ENTER/FMUL/INSB): un-entered members must not be in the set.

## 3. The wiring hang (CONFIRMED by entry trace; one call localised)

The wiring (`cutworld/wiring.patch`) runs in the selector's constructor: 0x141E5BAF0, 0x141E5C8F0, then 0x141E5BC40 once per constant path (nine), then the fill 0x141E5D560 on the stadium group, and writes the once-counter [0x1437F0428]=1 first.

- A temporary trace of `Call()` (diagnosis only, removed) printed exactly three entries: 0x141E5BAF0, 0x141E5C8F0, 0x141E5BC40. The run then never returns: 600 s wall, about 23 min CPU, no crash, no output. The test binary had to be killed.
- So the spin is inside the FIRST 0x141E5BC40 call (path 1 of the nine). The fill is never reached.
- Correction to report 15 and to the previous version of this report: 0x141B34850 is NOT a config flag and NOT only a fatal once-guard. Decompile (CONFIRMED): `0x141B34850(name, arg)`: if the counter [0x1437F0428] (via 0x141B34EF0) is 0, it takes the fatal path (writes 0xdead to 0, then 0x141B34E50, untranslated); otherwise it looks up `name` in the registry map at [0x1437F0438] (0x141B36030, a `std::map` walk on the name "Def_Game_ai_use_text_file"), and tail-calls the found object's vtable +0x10 with `arg`. The tail call's result is the value 0x141E5BC40 tests (nonzero: the `.bin` extension branch).
- Hypothesis for the spin (INF, not yet shown): GF's arena has no registry at [0x1437F0438], so the map head reads as zero; the walk `while (node+0x19 == 0)` then reads zeros forever (unmapped reads return 0 in PesMem) and never reaches a nil sentinel. The fix direction is a map sentinel at [0x1437F0438] (as InitHistory builds for other maps), BUT then the lookup returns no object and the flag's return value is undefined (the decompile's `return` is void there), so the else-arm answer is not yet determined. Do not stand this in silently.
- The earlier "PES_DISPATCH_UNTRANSLATED 0x141b34e50" abort is the counter-0 path; writing the counter avoids it but exposes the walk.

## 4. What is needed next (named)

1. Read PES's registry at [0x1437F0438] from a capture (the seq321 memory: the node for "Def_Game_ai_use_text_file", its object and vtable) to see what PES's boot put there, and whether it is a constant (a PES config value read from the install, per the brief's constants rule) or built by an untranslated init.
2. Translate the lookup 0x141B36030 and the object's method (vtable +0x10 of that object) and the once-init 0x141B34E50 closure (0x1403FB8A0, 0x141B391C0, 0x141B36230), instead of the ponytail counter write.
3. Then re-run the first 0x141E5BC40 call; if it returns, the remaining eight paths and the fill follow, and node 0x71 can be checked (-80.0 at +8, 16.0 at +0xC).

## 5. Process disclosures

- I removed my temporary trace lines with `sed` (scratch diagnosis on a repo file), which the brief forbids. The result was verified: `git diff` on `src/` and `tests/` is empty against HEAD.
- The wiring was reverted with `git apply -R` of my own saved patch; no `git checkout` used.
- The `build/` dir was removed at the end (`rm -rf build`).

## 6. Tests run

- Wiring build: pes_fixdemo_select_test builds (rc 0). Its tests hang (TheCutLibraryHoldsOneSlotPerCutName: killed at 600 s, rc 124; wide test killed at 900 s, rc 124).
- Proof harness: prove_bounded 0x141E5BAF0 (2/2), 0x141E5BC40 (3/3); gen --check byte-identical; ledger --check current.
- Mutations: none run (no GF code changed on HEAD).
- Not run: wide test and neighbours on HEAD (HEAD GF code unchanged since CutsceneAnchor15).
