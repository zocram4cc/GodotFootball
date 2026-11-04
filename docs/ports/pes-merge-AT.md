# Batch AT merge

Lane `MergeBatchAT`, cut from `origin/main` after batch AS (`c38d2743`).

## Branch merged

|branch|head|what it brings|reports|
|---|---|---|---|
|`DribbleProposal`|`b9e67ab6`|Ability bytes for PES-id players come from the EDIT save through PES's own save→row map, which was decoded from the code: descriptor table `0x1434F2B20`, copy `FUN_141F10CE0`, build and clamp `FUN_141FE9B00`. Speed is bit 174, Jump bit 192, Form bit 252, injury tier bit 327; 58 row bytes plus the skill mask equal all 34 captured rows. `+0xA4` is the stronger foot. `gfsim_run --edit-save`. The 12 unproven dribble members are proven bit-exact with `synth_call.py`: 11 on synthetic cases, labelled `"synthetic": true` in `results_dribble.json`, and `0x14063D2F0` on 12 real calls. The stale `TurnReferenceOracle` dirs were regenerated.|`pes-port-DribbleProposal6.md`, `-7.md`, `pes-port-EditSaveFields.md`|

The merge was clean; main had no newer commits.

## Checks

- `gen_translated.py --check`: 4478 functions (3708 recorded bit-exact, 770 closure) regenerate byte-identical. Eight members moved from `closure/` to `dribble/`.
- `ledger.py --check`: current (38068 rows).
- `GATE_TIER=fast tools/dev/gate.sh` (slots 1/0): **GATE fast: GREEN**.
  - ctest: 2205 of 2205 passed.
  - The selective re-check picked 0 rows in the five fast-tier sets: the change reaches none of their functions. The dribble set's proofs are the synth/snapdiff runs above.
  - No unrecorded status regression (18587 rows compared).
  - Off-path ports: 125, all allow-listed.
- 2-minute smokes, seed 7: switch-off `2802089ed1b4da32`, pes-loop `e7c1462426cef558`. Both are unchanged from main after batch AS. The smokes load the fixture teams, not an EDIT save, so the new ability source does not run in them.

## Open (from the reports)

- `0x142130EE0` is proven on one synthetic case covering 22.5% of its blocks; more cases are needed.
- The playing-style masks at `+0x48/+0x4C` and row bytes `0x3C..0x3F` are not decoded.
- The writer of row bytes `0x00..0x4F` into the AI record is not located.
- The `gfsim_run --edit-save` full-time smoke is still queued for a sim slot (EditSaveFields).
