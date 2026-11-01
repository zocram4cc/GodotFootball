# Batch AU merge

Lane `MergeBatchAU`, cut from `origin/main` after batch AT and its report commits.

## Branches merged

|branch|head|what it brings|report|
|---|---|---|---|
|`FoulSanction` (= `CardLevel2Cutover`)|`2e0486ec`|Test-only change. It decodes what the judge `0x1405B7470` does with ball-event kinds 1-14 and adds 9 killing tests through `EventStore::Route` (17/17). Every kind and role mutation is killed except kind 9, which falls to the default anyway (equivalent mutant). Ring mutations M8/M9 now each kill two older tests.|`pes-port-JudgeEvents.md`|
|`DribbleProposal`|`fca2b517`|Row `+0x48` style word and `+0x4C` seven-flag mask decoded from save ids `0x40..0x47` and applied; `+0x3C` is 0 for save players. PES's row → AI-record copy `FUN_140A58F00` is ported as `PesWorld::LoadRosterRow`. Whole rows equal all 34 captures. `0x142130EE0` block coverage is 0.26 on 13 synthetic cases.|`pes-port-DribbleRow.md`|

Both merges were clean.

## Checks

- `gen_translated.py --check`: 4478 functions (3708 recorded bit-exact, 770 closure) regenerate byte-identical.
- `ledger.py --check`: current (38068 rows).
- `GATE_TIER=fast tools/dev/gate.sh` (slots 1/0): **GATE fast: GREEN**.
  - ctest: 2218 of 2218 passed.
  - The selective re-check picked 0 rows: the change reaches none of the five sets' functions.
  - No unrecorded status regression (18587 rows compared).
  - Off-path ports: 125, all allow-listed.
- 2-minute smokes, seed 7: switch-off `2802089ed1b4da32`, pes-loop `e7c1462426cef558`, both unchanged from main.

## Open

- The run-depth branch at `teamAIcontroller.cpp:869` still keys on GF's style enum.
- The writer of live `+0x150`.
- The other arms of `0x142130EE0`.
- The `gfsim_run --edit-save` full-time smoke.
