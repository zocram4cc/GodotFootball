# Batch AS merge

Lane `MergeBatchAS`, cut from `origin/main` after batch AR.

## Branch merged

|branch|head|what it brings|reports|
|---|---|---|---|
|`FoulSanction` (= `CardLevel2Cutover`)|`7a989d3a`|The translated judge decides fouls (`Referee::JudgeContact` → `DecideContact`); the judge's A1+0xE0 event ring points at the match's own event store; whistle `0x14050E770` re-proven; `.impdata` bake test; ceremony throw-in seeds re-staged|`pes-port-CardLevel6.md`, `pes-port-CardLevel7.md`|

The merge was clean; `origin/main` was merged again before landing (docs only).

**Thunk resolution carried from the branch:** the CFG check thunk `0x1415A01DC` keeps the branch's `engine-noop` stub model. The judge's closure needs it; without it `pes_judge_decide_test` aborts with `PES_DISPATCH_UNTRANSLATED`. Main's stand-in spec of the same thunk is deleted. The thunk is a `ret` either way, and the spec had no record in `results_dribble.json`. `pes_dribble_oracle_test` ran in the fast tier.

## Smoke hashes move (explained, PES fidelity)

|path (seed 7, 2 min)|before (main)|after|
|---|---|---|
|switch-off|`e663d1e917d6c4f4`|`2802089ed1b4da32`|
|pes-loop|`980c54e27f93ced2`|`e7c1462426cef558`|

CardLevel7's ablation attributes the whole move to the translated judge deciding contacts:
- restoring main's hand-ported judge on the branch head reproduces main's hashes exactly;
- disabling the sanction call moves neither hash.

The same move shifted ceremony throw-ins to other seeds. A census of seeds 1..40 (9 ceremony throw-ins in 80) re-staged `kCeremonySeeds = {2, 15, 16}`; the assertions are unchanged.

## Checks

- `gen_translated.py --check`: 4478 functions (3700 recorded bit-exact, 778 closure) regenerate byte-identical.
- `ledger.py --check`: current (38068 rows).
- `GATE_TIER=fast tools/dev/gate.sh` (slots 1/0): **GATE fast: GREEN**.
  - ctest: 2190 of 2190 passed.
  - Recorded re-check: every row selected and re-checked: results 245, cutscene 685, presentation 546, core 2253, impure 34.
  - No unrecorded status regression (18587 rows compared, 160 scope moves recorded).
  - Off-path ports: 125, all allow-listed.
  - The 20-minute match ran 44687 sim ticks to full time, with 28 stoppages; the longest was 23.7 s against a bound of 32.5 s.

## Not in this batch

- `StoppageFlow`: one tick oracle (seq1917) and the live-ball stall are red. It carries `pdata.py`'s switch-after-`ret` fix.
- `CutsceneTiming`: gate in progress. It carries ProcCamera3's xlate SSE fix.
- `DribbleProposal`: carries the EDIT-save ability source, the decoded save→row map and 12 synthetic member proofs; not gated yet.
- `KickMotion`: deferred until StoppageFlow lands.
