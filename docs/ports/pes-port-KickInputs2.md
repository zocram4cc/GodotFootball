# PES port: KickInputs2 (branch KickSoftMin): kick-scatter stand-ins, oracle built, NOT CLOSED

## Verdict

NOT CLOSED. No stand-in in `src/sim/onthepitch/peskickerror.cpp` was replaced by a PES-layout record read. The kick
record was not added to GF. What did land is the oracle the predecessor said was missing: PES's own kick_scatter
calls, replayed from the 14 kick captures, give PES's four soft-min factors per call and PES's result. GF's `SoftMin4`
reproduces all 48 of them bit-exactly. That closes one piece of the scatter. Q, I, R, F1/F2, the post-scale and the
yaw/pitch terms remain stand-ins, because the record fields that feed them have no GF producer.

## Commits (branch `KickSoftMin`, confirmed by `git ls-remote origin KickSoftMin`)

- `b37e69ff`: `git merge origin/main` (origin/main `8d79090a`). The merge printed the `extern/godot-cpp` symlink warning and
  succeeded with the symlink in place.
- `9907fb6d`: kick oracle (replay tool, `run_recorded` `models` argument, oracle test, Q comment with writer VAs).

## What was decoded or verified (VAs; scripts read every value from the replayed memory)

Replay: `tools/dev/rendezvous/kick_replay.py` runs each capture's recorded call with `frame_trace.run_recorded`
(added an optional `models=` argument; default unchanged) and reads PES memory before and after the call. Output is
PES-derived and lives in `<workdrive>/gf-KickSoftMin-scratch/` (not committed).

- **Layout CONFIRMED against the captures** (14 of 14 replays return cleanly, `err=None`):
  `ap` = rcx; `[ap+0]` = `0x1425E34D8` (vtable, record-ids §1); `[ap+0xAB8]` = `0x1425E3508` (vtable+0x30) in 14/14.
  `kind` `+0x2752` (s16) = 42, 97, 57, 14, 28, 98, 43, 1 (all in 1..0x96).
  `window` `+0x2764` (f32) = 720.0 on 6 of 14 (W1 value `0x44340000`, W1 `0x1406EB6FC`), otherwise a decreasing
  timer: 275.95, 273.69, 155.71, 236.6, 71.52, 167.29, 78.47, 312.99.
  `firstTime` `+0x2756` = 0 or 1. AI bit (`+0x2460` bit 29) = 0 in 14/14.
- **Entry value of `uVar14` (`+0x2755`) is not the doc's set.** Observed 9, 13, 11, 2, 4. `pes-re-kick-scatter-inputs.md`
  (a) says `0xf/0xd/0x11 or 0`. CONFIRMED (captures), so that doc line describes a post-switch value, not the entry byte.
- **PES's soft-min calls** (`FUN_1408413C0`, args xmm0..xmm3 = `(I, x, F, R)`, result xmm0): 6 per scatter, 48 rows
  over 8 kick_scatter captures (`softmin_oracle.tsv`, scratch). Per capture, the PES inputs:

  | seq | kind | firstTime | window | I | Q (call-1 x) | F (arg 3) | R |
  |---|---|---|---|---|---|---|---|
  | seq017 | 42 | 0 | 720.0 | 1.0 | 0.88 | 1.0 | 1.0 |
  | seq022 | 97 | 1 | 275.95 | 0.9223 | 0.86 | 1.0 | 1.0 |
  | seq031 | 57 | 1 | 273.69 | 1.0 | 0.8966 | 1.0 | 0.9981 |
  | seq036 | 14 | 0 | 720.0 | 0.8077 | 0.716 | 0.5539 | 0.4451 |
  | seq048 | 28 | 0 | 155.71 | 0.9125 | 0.86 | 1.0 | 1.0 |
  | seq067 | 98 | 1 | 236.6 | 0.9377 | 0.88 | 1.0 | 0.9998 |
  | seq4471 | 43 | 0 | 720.0 | 0.9836 | 0.88 | 1.0 / 0.915 | 0.9903 |
  | seq4774 | 97 | 0 | 312.99 | 0.8258 | 0.86 | 0.9793 / 0.9119 | 1.0 |

  Call 2 is `qk = 0.5Q + 0.5` in every capture (e.g. 0.94 for Q 0.88), and calls 5-6 are `qs` the same way. So the
  call order is the GF `BuildScatter` order: `soft(q)`, `soft(stat*q)`, `soft(qk)`, `soft(stat*qk)`, `soft(qs)`, `soft(stat*qs)`
  (argument order in C++ is unspecified, so the pairing is by value; CONFIRMED by the TSV values, not by the source).
- **Q is never 1.0 in PES.** GF's `kSituationNotPorted = 1.0` differs from PES's Q in 8 of 8 scatter captures
  (0.716 to 0.88). This is the largest single gap.
- **Post-scale changes a record.** seq017: the captured Y/P/S after the scatter read `(2.7, 0.5, 0.8112)`,
  `(1.35, 0.5, 0.7653)`, `(0.018, 0.31998, 0.7653)`. Hand arithmetic from GF's `BuildScatter` with Q=0.88, I=1, R=1,
  F=1, stat=1 gives the same scales and amps and `pSign = 0.5` for all three. The S `pSign` differs (0.31998 vs 0.5):
  INF, the post-scale `FUN_1408414A0` (absent in GF) alters S's sign. Not tested.

## Oracle test (committed, green on current code)

`tests/sim/pes_kick_error_test.cpp`: `PesKickErrorOracle.SoftMinOnPesArgumentsGivesPesResult`. Reads
`PES_KICK_SOFTMIN_TSV` (skip when unset; fails when the file has no rows). Asserts `SoftMin4(a,b,c,d) == out` exactly, for
each row. Result: 48/48 rows bit-exact, 15/15 tests in `pes_kick_error_test` green (run directly with `PES21_ROOT` and the TSV).

Mutations (each rebuilt, run, reverted; `git diff` of the source shows only the comment change afterwards):
- **M1** `std::sort(..., std::less<float>())` instead of `greater`: oracle test FAILED (28 failure lines).
- **M2** fold `x = (hi * lo + hi) * kFoldHalf` instead of `lo`: oracle test FAILED (48 failure lines, one per row).

## Record (step 2): NOT added

No GF site produces the kick kind (`ap+0x2752`, six PES writers `0x1406EB6D6` (W1, -1), `0x1406F6B6B` (W2),
`0x140719DCF` (W3), `0x14071BB69` (W4), `0x140724F26` (W5), `0x140725FFF` (W6), record-ids §1). GF's `e_FunctionType`
and `PesActionKindOf` (1, 2, 5, 7) are a different namespace; mapping them to PES's 151 kinds would be an invented table.
Window and first-time are written only by W1 (init), so a GF record would hold init values that nothing reads until Q
is ported. I did not add a dead record. It lands with the first reader (Q).

## Stand-ins remaining (each with its PES writer or predicate VA)

1. **Q** (`kSituationNotPorted = 1.0`; PES values above). Kind: W1-W6. Window and first-time: W1 `0x1406EB6FC`
   (720.0f), W2 `0x1406F6B6B`. AI bit `+0x2460` bit 29: writer OPEN. Gates `0x140724590`, `0x140726610`, `0x140726640`,
   `0x1407240F0`, `0x1407246E0`, `0x1407241A0`, `0x140724740`: untranslated. Class base (0.9/0.88/0.86 at `0x14258C260`,
   `0x1425DDF70`, `0x1425EB850`) and the subclass table are install-readable; not wired (needs the kind).
2. **I** (launch penalty): GF `PassLaunchPenalty` / `ShotLaunchPenalty` on GF's launch. PES I is 0.8077 to 1.0 in the
   captures. GF is not checked against it (the scatter args that carry the launch are not decoded in this report).
3. **R** (pressure): GF `PressureFactor` with run term 0. PES R is 0.4451 to 1.0; the run term (`FUN_140723320`,
   contact vector `+0x2BF4`, state id `vtbl+8`) has no GF producer; writer OPEN.
4. **F1/F2**: GF 1.0. PES F is 0.5539 to 1.0 and varies per call (descriptor `FUN_1406B0250`, motion table singleton
   `0x1436F23C0`, built at run time).
5. **Post-scale** `FUN_1408414A0`: absent. Changes S `pSign` on seq017 (INF above). `u` provenance OPEN.
6. **xmm8 contact factor** (anime `+0x2C`, writer OPEN): 1.0. **angRamp** (anime/P `+0x28`, writers OPEN): omitted.
7. **Yaw redraw** `FUN_1408398D5` loop: skipped (`[rbp-0x78]` provenance OPEN).
8. **Shot pitch g** (`-0.02 g` term; `F` getter `0x141E532D0`): omitted.
9. **Header predicate** (`IsKickFunction`, team.cpp:26): unchanged; possession change outside the scatter.

## Tests and counts

- `pes_kick_error_test`: 15/15 green (includes the oracle with `PES_KICK_SOFTMIN_TSV`, install test with `PES21_ROOT`).
- Mutations M1 (red, 28 failures) and M2 (red, 48 failures): run, rebuilt, reverted.
- Gate: not run (Main gates).

## Open items (each names what settles it)

1. Q: a GF producer for the PES kick kind. Settled by a decode of the six writers' callers (Ghidra xrefs on
   `0x1406EB6D6`..`0x140725FFF`), or a GF rule that is itself verified against these captures.
2. Post-scale on S `pSign`: decode the caller `u` (`[rsp+0x78]`) and the predicates `0x1407246E0`, `0x1407241A0`,
   `0x140726610`; check against seq017 (S `pSign` 0.31998) and seq018.
3. Launch penalty I and pressure R: which scatter args carry the launch vector and opponent contact (the replay can read
   the stack args of the scatter call; not done here).
4. The kick captures are at scatter entry only; Q's gates need a per-case table from more captures (seq4774 and seq4471
   have different F per call).

## pes_* sweep (step 5): every binary in tests/sim and tests/ that links gfsim, run directly with `PES21_ROOT` and the TSV

Each binary ran under `timeout 120`, from the build of this branch at `9907fb6d` (code identical to the commit's
source; the only source changes are comments). Counts are PASSED / FAILED from gtest's summary line.

- 0 failures in every binary that ran to completion. Examples: `pes_kick_error_test` 15/15, `pes_movestep_test` 114/114,
  `pes_movestep_run_test` 99/99, `pes_press_test` 95/95, `pes_mentality_test` 47/47, `pes_offball_test` 76/76,
  `pes_uniform_test` 36/36, `pes_team_concepts_test` 31/31, `pes_tick_a1_test` 18/18, `pes_image_test` 19/19.
- **Timed out at 120 s (rc=124, no summary):** `pes_holder_latch_test`, `pes_movestep_wire_test`, `pes_period_end_test`,
  `pes_press_approach_test`, `pes_restart_delay_test`, `pes_restart_taking_test`, `pes_stall_test`,
  `pes_throwin_release_test`. NOT diagnosed. `pes_stall_test` is the binary Main disabled on `main` (stall, per the
  brief's rule that a timeout is a finding). I did not establish whether the other seven are pre-existing on
  `origin/main`; that needs a run of the same binaries on `8d79090a`.
- **Not reported:** the 34 binaries that sort before `pes_handseq_test` (first sweep piped through `tail -60`, so their
  lines were lost; the rerun was cancelled on Main's stop order). Their counts are unknown.

## Numbered next steps

1. Re-run the 34 binaries before `pes_handseq_test`, and the 8 timeouts on `origin/main`, to settle the stall question.
2. Q: find a GF producer for the PES kick kind (or a verified rule from GF kick types), then add the kick record
   (W1 init, W2-W6 sites) and read Q's inputs from it; the oracle's Q per kind is in the table above.
3. Post-scale `FUN_1408414A0`: port it with its caller's `u`; check against seq017 S `pSign` (0.31998) and seq018.
4. Launch penalty I and pressure R: decode the scatter's stack args to get the launch vector and opponent contact;
   then compare GF's `PassLaunchPenalty` / `PressureFactor` with the oracle's I and R per capture.
5. F1/F2: the motion descriptor needs the runtime motion table; decide whether the replay can supply it, or name it a
   runtime-only input.
