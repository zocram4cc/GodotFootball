# Batch AY merge

Lane `MergeBatchAY` = `origin/main` + `FoulSanction` (`ede2b1c9`).

## What lands (CardProof)

- **Proofs:** the judge commit `0x143F546F0` is proven 11/11 on live captures (levels 1-3) and the decide `0x1405B7470` 12/12, both `snapdiff --bounded` bit-exact; recorded in `results_judge.json` and `results_core.json`.
- **GF wiring:** GF's card level now comes from the translated commit: `PesWorld::CommitJudge`; `DecideJudge` reports `JR+0x38` (1 foul, 2 foul + warning, 3 yellow, 4 red). Booking, the stoppage mirror and `DecideSanction` take it.
- **Tests:** `pes_judge_decide_test` 19/19, with the mutations listed in the report.
- **Not yet captured:** no red (level 4); the watcher predicate is retargeted to `d(rdx+0x18)>=4`.

## Gate

`GATE fast: GREEN`, 4,366 s.
- `gen_translated --check` 4837 byte-identical.
- ctest all passed.
- The recorded re-check selected 0 rows: the change reaches none of the five fast-tier sets; the judge set's proofs are the snapdiff runs above.
- No unrecorded status regression; ledger current.
- Smokes unchanged: `3b73c968b4a377e0` / `0e480b158cf80bd4`. GF's judge produces only kind-2 fouls, for which the commit's level equals the pending level it replaced.
