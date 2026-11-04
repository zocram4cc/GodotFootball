# Batch AV merge

Lane `MergeBatchAV`: `origin/main` plus `DribbleProposal`.

## Branch merged

|branch|head|what it brings|reports|
|---|---|---|---|
|`DribbleProposal`|`d073ea15` (code `491ba167`)|**Style cutovers.** The run depth follows PES's style-word rule in `FUN_1405D8B70`: Goal Poacher and Fox in the Box each add 5 m, Creative Playmaker subtracts a constant 10 m, and roles 5-6 are gated on the forward-line band byte. The band rows come from `FUN_1405F9CD0`, ported as `WriteBandRows`. The keeper, Destroyer and Cross Specialist sites read the style word. GF's per-style depth table, width tuck, poacher spot and dribble style terms are deleted: PES has no counterpart for any of them. **Seeds.** `kCeremonySeeds` is re-staged to {5, 7, 9} by a census of seeds 1..40.|`pes-port-StyleCutover.md`, `-StyleCutover2.md`, `-CeremonySeeds.md`|

## Smoke hashes move (explained, PES fidelity)

|path (seed 7, 2 min)|before (main)|after|
|---|---|---|
|switch-off|`2802089ed1b4da32`|`3b73c968b4a377e0`|
|pes-loop|`e7c1462426cef558`|`0e480b158cf80bd4`|

The ablations in StyleCutover and StyleCutover2 show that the move comes from deleting GF-invented style terms. On the final tree, the new PES terms (bit 4 and the roles 5-6 arm) move neither hash. The same drift emptied the old ceremony seeds {2, 15, 16}. The census found 9 ceremony throw-ins in 79 throw-ins; the assertions are unchanged.

## Gate

- **Main's first AV gate (DribbleProposal `1be2ba8c`):** RED on the two ceremony throw-in tests, the seed drift above.
- **`GATE fast: GREEN` on `491ba167`** (CeremonySeeds, slots 1/0, absolute Release dir):
  - ctest: 2235 of 2235 passed, including both former reds;
  - `diff_gen_*` and `diff_pilot_recorded` passed;
  - no unrecorded status regression; ledger current;
  - smokes deterministic at the hashes above.
- **Why that result covers this lane:** apart from `docs/`, the lane's tree is identical to `491ba167` (`git diff --name-only 491ba167 HEAD` lists only docs files).
- `gen_translated.py --check`: 4478 functions regenerate byte-identical (Main's AV run).

## Open

- Ceremony-seed lists are census results: re-run the census whenever a cutover moves the smoke hashes.
- The COM-card terms in the dribble knobs are still `ponytail:` stand-ins.
- `PlayerData` still carries GF's style enum as a fallback for players without a save row.
