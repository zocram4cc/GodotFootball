# Batch BH: SurveyTool (merged to main)

**Contents:** branch `SurveyTool` (`51cc4bed`), merged onto main `55096617` in `gf-MergeLane` as `MergeBatchBH` (`3ba99f9b`). Report: `pes-tool-SurveyTool.md`.

- **Survey mode** (`GF_PES_SURVEY=<file>`): off by default, so an untranslated target aborts as before. Set, every untranslated dispatch target is counted with its first frame and caller offset, the run continues (the caller's recovery first, else the call has no effect), and the process exits 79, so a survey run can never pass as a smoke.
- **`tools/dev/translate/untranslated_callees.py`:** lists the untranslated callees reachable from translated roots.
- **`--hooks`:** prints `hook_plan.py` rows and `hooks.cfg` lines at FIRST 1 EVERY 20, and refuses targets that fail the prologue check.
- `pes_dispatch_missing` is no longer `[[noreturn]]`, so a survey run can continue past a miss.

## Conflicts

Additive only:
- `tests/CMakeLists.txt`: GateQuick's queue and selector tests, and SurveyTool's two tests. Both sides kept.
- `tools/dev/README.md`: the branch-tier and synth sections, and the survey section. Both sides kept.

## Proof

- **On the branch:** `GATE fast: GREEN` (2,323 of 2,323), smokes `399e180f39459c2b` / `85c7d7c515cf92ae`. A survey-mode pes-loop match exits 79 with the same hash.
- **Mutations:** exit code 79→0, the lister's walk removed, and survey aborting instead of continuing are each caught.
- **On the merged tree** (main gained batch BG after the branch point): built, then smokes re-run: switch-off `399e180f39459c2b`, pes-loop `85c7d7c515cf92ae`, unmoved. `translate_pes_survey_test` and `translate_untranslated_callees_test` pass.

## Left

- No survey run on the kick chain yet (KickMotion carries the cutover).
- The lister ignores `OUT-OF-RANGE JMP/JCC` markers; none is reachable from the trap or kick chains.
- A survey is a lower bound: a skipped call changes the run after it, so survey several seeds.
