# pes-port-Presentation: presentation subsystems through the pipeline, one translator fix round (25-10-26)

Branch `Presentation`, head `19a8e25` (ls-remote confirmed). Commits: `d3c15e9` (set, specs, driver `--set`, gate test, pre-fix full run), `6f7795e` (merge of origin/main: docs only, at that time), `dffb103` (fix round, post-fix record), `1be821b` (branch-target guard, final record), `07b795a` (merge of origin/main at the translated-layer commit), `19a8e25` (layer regenerated with the presentation record). Worktree `<workdrive>/gf-Presentation`, scratch `<workdrive>/gf-Presentation-scratch`. Base `origin/CutsceneFix` `e7d3414`. Image `pes_full.bin`; harness Python `DiffHarnessRE-scratch/venv` (unicorn, capstone). Gate not run (Main gates). `check_pii.sh` not run in the worktree (symlink); the changed files were grepped for machine paths: no hits.

## Result

| set | first pass (base translator) | final (1be821b) | lost | gained |
|---|---|---|---|---|
| presentation, all 639 rows | 456 | **472** | 0 | 16 (seeds) + 40 callees |
| presentation seeds, 594 | 416 (70.0%) | **432 (72.7%)** | 0 | 16 |
| pilot `--check` (183) | 183 | 183 | 0 | 0 |
| impure `--check` (8) | 8 | 8 | 0 | 0 |
| cutscene `--check` (549) | 549 | 549 | 0 | 0 |

The three older records were re-checked on the fixed translator; their records are unchanged. The fix round's gains in those sets are not recorded yet (no full run of those sets).

## Sets

- Seeds: pure in-scope rows tagged replay 182, camera 149, crowd 42, HUD 53, audio 168 (594). These match the brief's counts from `ledger.pure_rows`.
- Closure over pure callees, per subsystem: replay 202, camera 163, crowd 59, HUD 57, audio 181. The union is 639 rows (594 seeds + 45 callees).
- `specgen select --subsystem replay camera crowd HUD audio` writes `specgen/sets/presentation.txt` with columns VA, status, subsystem, role. The subsystem is the first subsystem to reach a row, and seeds keep their own tag. Single-subsystem selection gives the old closure. The driver's `--set` writes `subsystem` and `role` on each result; all 639 rows carry them.
- Specs: 639 pilot-mode specs from the image into `tools/dev/diffharness/specs/presentation/` (3.7 s).

## Commentary: excluded 0 rows, with evidence

Match commentary is out (owner ruling 25-10-26). No pure in-scope audio row, and no callee of one, is commentary code. The walked universe does not contain the commentary code at all.

- RTTI class `Commentary@ai@app@sound@@` (type name at VA `0x14366CB30`). Its vtable is at `0x14294D068`, with 4 code slots: `0x14143D020`, `0x14143D0A0`, `0x14143E0D0` (no universe row), and `0x141794410` (in-match menu, shared with the PA-announcement and Chant vtables). The two code sites that reference the vtable, `0x14143CEAB` and `0x14143D02A`, lie in functions with no universe row (0 rows in `0x14143CC00`-`0x14143D200`).
- Speech-bank loader classes: `Loader@TeamName@edit@sys@sound@@` (vtable `0x142962FE0`, site `0x1414846E9`), `Loader@PlayerName@edit@sys@sound@@` (vtable `0x142962D08`, site `0x141483FF9`), `Loader@TeamDirectionName@sys@sound@@` (vtable `0x1429657C8`, site `0x14148CA0E`). Their private slots (`0x141484140`, `0x141483AC0`, `0x14148C720`) have no universe row; the shared slot `0x1414901A0` is a crowd row.
- Speech-bank strings: `00_TEAM` `0x142963778`, `10_PLAYER` `0x142963780`, `20_MAIN` to `50_MAIN` `0x142963790`-`0x1429637A8`, `/sound/acb/` `0x1429637C0` and `0x142963968`, `CommentaryLanguage` `0x142959178`, `CommentatorKind` `0x142958DD8`, `SdCommandForCommentary` `0x142959190`, `CheckCommentaryLanguage` `0x142955640`. No universe function references them, directly (RIP-relative LEA) or through a pointer table within 0x400 bytes. Their references come from code outside the universe.
- The LEA scan covers 102,097 RIP-relative targets in code. It is live: 2,181 universe functions reference strings. None of the 168 pure audio seeds, and none of the 13 audio-closure callees, references a string.
- Kept, not commentary: `PAannouncement@ai@app@sound@@` (stadium announcements, `SA_*` banks; report `pes-re-audio.md` §2.8 lists them apart from commentary), two audio rows (`0x14143F880`, `0x14143F8C0`).
- Residual risk (INF): the audit uses RTTI, strings and LEA references. Commentary reached only through computed names or data tables would not show here. The pure rule still applies to any such row.

## Specs and the gate

- `tests/pilot/CMakeLists.txt`: `diff_presentation_recorded`, mirroring `diff_cutscene_recorded` (driver `--check`, `specs/presentation`, `results_presentation.json`). Run by hand with the same command: 472/472. Not run through ctest (Main gates).

## Measurements per subsystem (final record; first pass in brackets)

| subsystem | seeds | bit-exact at floor | seeds + callees (closure) | coverage median (p25 / p75) | seed failures by class |
|---|---|---|---|---|---|
| replay | 182 | 140 (76.9%) [131, 72.0%] | 159 / 202 | 1.00 (0.68 / 1.00) | struct map gap 35; harness cannot run 6; translator bug 1 |
| camera | 149 | 127 (85.2%) [126, 84.6%] | 139 / 163 | 1.00 (1.00 / 1.00) | struct map gap 16; harness cannot run 5; translator bug 1 |
| crowd | 42 | 33 (78.6%) [33, 78.6%] | 47 / 59 | 1.00 (0.53 / 1.00) | struct map gap 3; harness cannot run 6 |
| HUD | 53 | 50 (94.3%) [47, 88.7%] | 54 / 57 | 1.00 (0.74 / 1.00) | struct map gap 2; harness cannot run 1 |
| audio | 168 | 82 (48.8%) [79, 47.0%] | 95 / 181 | 0.43 (0.00 / 1.00) | struct map gap 81; harness cannot run 3; translator bug 2 |
| all seeds | 594 | 432 (72.7%) [416, 70.0%] | 472 / 639 (all rows) | 1.00 (0.34 / 1.00) | struct map gap 137; harness cannot run 21; translator bug 4 |

"Harness cannot run" here is every row bit-exact below the 50% own-block floor (21 seeds, all of them bit-exact on their cases). Callees: 40 of 45 bit-exact.

## First pass against the cutscene set

- Cutscene: first pass 59.5% (447 of 751, translator before CutsceneFix); current 64.3% (483 of 751, same translator as this set).
- Presentation first pass: 70.0% overall, on the translator after CutsceneFix (`e7d3414`). The like-for-like baseline is the cutscene's current 64.3%.
- Per subsystem, first pass: replay 72.0%, camera 84.6%, crowd 78.6%, HUD 88.7% (all above 64.3%); audio 47.0% (below both).
- Failure shares, both on the same translator, seeds: presentation (final) struct map gap 23.0%, translator bug 0.7%, harness cannot run 3.5%; cutscene (current) map gap 26.8%, translator bug 2.1%, harness cannot run 6.8%.
- Answer: translator fixes carry over. The translator-bug class is small in the new subsystems (22 of 594 seeds on the first pass). The fix round recovered 16 of those. The new subsystems exposed classes that the cutscene set did not hit (listed below). Subsystem spread is set by the struct-map gap, which concentrates in audio (81 of 168 seeds). That is generator work, not translator work.

## Fix round: before and after

Before: translator at `e7d3414`, presentation first pass 456 of 639, 22 translator-bug seeds. After: 472 of 639, 4 translator-bug seeds. Lost 0.

Changes in `tools/dev/translate/xlate.py` (general lifts; no spec was edited):
- DIV / IDIV (8 to 64 bit, unsigned and signed; #DE traps via `__builtin_trap`): bit-exact on `0x14195D0E0`, `0x1420B0580`, `0x1420B2630` (provisional: live-state globals at on-disk values), `0x1420B2770`, `0x1421738E0`, `0x1421761C0`.
- REP STOSB/W/D/Q, plus single STOS and MOVSB/W/Q (MOVSD only with REP, since SSE MOVSD is the common form): bit-exact on `0x14042D610`, `0x140A8A550`, `0x142321C00`, `0x142321D34`, `0x142321D6C`.
- SSE: CMPNLTSS bit-exact on `0x1400EEF10`; MOVQ between GPR and xmm, MOVHLPS, MOVLHPS, PUNPCKLQDQ, PUNPCKHQDQ, PADDQ, MINPS, MAXPS, HADDPS, SQRTPS, RCPPS, RSQRTPS. Bit-exact on `0x14002A5C0`, `0x140049DE0`, `0x142178CB0`.
- `0x141EAF270` (RSQRTPS) now matches on 92 cases but sits below the floor (own-block 0.32). It is no longer a translator bug.
- LOOP: lifted with a label. No bit-exact function in these sets uses it (its only site, in `0x141B50460`, has 0 executed coverage).
- A branch whose target lies outside the decoded body now traps when taken instead of naming an undefined label. That recovered `0x14149F5E0` (camera, bit-exact on 64 cases, provisional) and kept `0x141B50460` at its pre-fix below-floor class. The first version of the LOOP lift had turned `0x141B50460` into a compile error; the guard is the fix for that regression.
- `0x14088FE50`: timeout to struct map gap (UC_ERR_MAP). The translator now runs through.

Not fixed, translator bugs remaining (4 seeds): `0x14089C270` (camera; `ret_rax` 7FFFFFFFFFFFFFFF vs FFFFFFFFFFFFFFFF on 64 of 64 cases), `0x1414536C0` (audio; `ret_rax` on 25 of 64), `0x141B88200` (audio; timeout), `0x1420AF8E0` (replay; timeout). Not diagnosed beyond that.

## Verification (exercised)

- Pre-fix full run, presentation: 639 attempted, 639 translated, 456 bit-exact (1,581 s, `--jobs 3`).
- Baseline checks before the first commit, untouched translator: pilot 183/183, impure 8/8, cutscene 549/549.
- Targeted run of the 22 translator-bug functions after the lifts: 15 bit-exact, 2 reclassified (`0x141EAF270`, `0x14088FE50`), 5 still failing.
- Post-fix full run: 471 bit-exact; checks pilot 183/183, impure 8/8, cutscene 549/549; presentation check 471/471.
- Final full run (after the branch guard): 472 bit-exact (1,812 s). Checks: pilot 183/183, impure 8/8, cutscene 549/549, presentation 472/472. No row lost against either earlier pass.
- Mutations: none run. The 22 translator-bug functions served as the red set (all failing before the lifts), and the record checks as the green set.
- Unverified by any bit-exact function in these sets: LOOP, REP MOVS.
- RCPPS and RSQRTPS are emitted as IEEE single-precision `1/x` and `1/sqrt(x)`. They match the unicorn oracle on the sets. Silicon's table approximation is not modelled here.

## Open items

1. Struct map gap: 137 seeds (81 in audio). It is the largest class, and the cutscene report's classes 1 to 3 still apply: pointer origin through `sub`/`add`, table-indexed pointers, stack arguments modelled as scalars. Generator work (`specgen/analyze.py`), not in this round.
2. Translator bugs left: 4 seeds (listed above).
3. Below-floor bit-exact: 21 seeds (`harness cannot run`). These need input coverage from the generator.
4. Pilot, impure and cutscene records are unchanged (checks only). A full run of the cutscene set would record the fix round's gains there.
5. The ledger (`ledger.py`, `joins.py`) reads only `results.json`, so presentation statuses in `LEDGER.md` are unchanged.
6. `BSWAP` (3 functions in the presentation pass) and `PREFETCHT0` are still unhandled.
7. The gate has not run; `diff_presentation_recorded` is new and unrun there.

## Merged state and the translated layer (19a8e25)

- origin/main now carries the translated layer (`src/pes/translated/`, `gen_translated.py`, gate test `diff_translated_drift`). Main moved the translator runtime headers (`pes_state.hpp`, `pes_cond.inl`, `pes_dispatch.hpp`, `pes_stubs.hpp`) into `src/pes/translated/`; the driver reads them from there. The merge had no conflicts.
- `gen_translated.py` `SETS` gains `results_presentation.json` (spec subdirectory `presentation`). The layer's subsystem directories come from each spec's subsystem tag, so the presentation functions land in `replay/`, `camera/`, `crowd/`, `hud/` and `audio/`.
- Regenerated layer: 1189 functions (1140 recorded bit-exact, 49 closure members), 18.6 s. `gen_translated.py --check` is byte-identical.
- Against main's layer: 320 recorded functions are added (presentation). Four closure members (`14044E020`, `140A489A0`, `140A73E40`, `140A76240`) are now recorded bit-exact in the presentation set and moved from `closure/` to `camera/`. `cutscene/pes_fn_140005530.cpp` is re-translated by the fixed translator; the cutscene record's check (549/549) covers it.
- Checks on the merged tree (`19a8e25`), driver `--check`: pilot 183/183, impure 8/8, cutscene 549/549, presentation 472/472. `gen_translated.py --check`: byte-identical, 1189 functions. The `diff_translated_drift` gate test was run by hand with the same command; the gate itself has not run (Main gates).
