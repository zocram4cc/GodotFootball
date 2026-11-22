# pes-port-StallLive17: trap-chain feed diagnosis (one feed field found, not fixed); RUN entry and Think arbitration not started; nothing landed (StallLive)

Status: NOT MET. Branch `StallLive`, HEAD **1b56b7264df7b8984707f3936fb245ae45d5e2e1** (unchanged this round, no commit). `src/` and `tests/` equal HEAD (verified). Build dir to be deleted at the end. Experiment patches archived: `r17/r17-experiments.patch` (scratch), `r17/oracle-experiment.patch`.

Integration (Main's directive): no SHAs received from StallRun, StallThink or StallBreaker2 this round, so nothing was merged, and no combined acceptance run was done.

## 1. Trap-chain feed: where the live inputs differ (MEASURED)

Method. The chain 0x14079A470 is translated and reproduces PES on the captured calls (oracle, two captures from `pid3727195` seq025 and seq027, both issued=0; base equals PES: out 1 and out 4). Live check: an env-gated copy of the chain's window, rerun with one live-in mutated at a time, on every kind-0 call for a man within 3 m of a resting ball, seed 7, 2 minutes (5,071 calls).

| live-in mutated (window copy) | calls whose (issued, out) changed |
|---|---|
| teammates' anime class byte (+1) = 3 | 0 / 5,072 |
| teammates' anime class byte (+1) = 2 | 0 / 5,072 |
| caller's anime class byte (+1) = 0 | 0 / 5,071 |
| P+0x377C (rate ceiling) = 1.0 | 0 / 5,071 |
| P+0x2FC velocity ring zeroed (6 x 12 bytes) | **162 / 5,071** |

So the anime class byte (GF writes 0 for outfield men, `teamAIcontroller.cpp:3774`, the ponytail there says it is a stand-in) and the rate ceiling do not drive the refusals. The velocity ring does.

Oracle capture check of the ring (CONFIRMED from capture bytes, seq025 and seq027, same caller slot 6, standing at the ball):

- captured P+0x2FC entries (m per frame): k=0 (0.00095, 0, -0.00206); k=1 (0.00789, 0, -0.01116); k=2 (0.00661, 0, -0.01137); k=3 (0.00496, 0, -0.00853); k=4 (0.00396, 0, -0.00682); k=5 (0.00330, 0, -0.00568). seq027 is the same to 3 digits (k=0 (0.00024, 0, -0.00051)).
- the same man: position P+0x8 = man +0x55C = man +0x568 (previous) = (-9.3018, 0, -8.8410), so his position delta is exactly 0; man velocity +0x574 = 0.
- GF's feed for entry k is the man's present velocity (`teamAIcontroller.cpp:3611-3616`, `WritePesTrapActors`, every one of the six entries copies man +0x574). For this man that is all zeros, while PES holds a nonzero, decaying trail (entries shrink with k).

Reading (INF, one capture pair). PES's ring is not the position-derived velocity. For a standing man it keeps a decaying nonzero prediction, which fits the body's anime root-motion predictor (the ponytail in `pestrapactor.hpp` already says the ring is an open input with no GF source; `docs/re/pes-re-trap-inputs.md` 1.3 leaves the writer OPEN). The candidate GF-available source is the playing clip's root displacement per frame (GF already computes `CalculateMovementAtFrame` for the body). Not tested.

Not found this round: the per-code input dump for codes 7 (empty candidate query), 0 (race) and 3 (bearing/commit). The live sensitivity only tested the ring and the rest above, so their input fields are still unidentified. Codes 2, 4, 5, 10, 11 not decoded (not needed yet).

## 2. Not fixed, not landed (why)
- A ring fed from the clip root-motion is an unverified approximation. It needs (a) a reader of motion 0xBC2 and the other captured motion numbers, to get the clip's root travel for the captured frame, and (b) a check that it reproduces the captured ring entries in both captures. Neither was done, so landing it would be a guess. The rule in the brief is no unverified feed.
- So: no trap-feed fix with a test landed this round. This is the gap in Main's "land at least the trap-feed fix".

## 3. Items not started this round
- RUN entry (speed-class stand-in `pesmovestep.cpp:241` vs PES `pes-re-movestep-wave1.md`): not started (carry-over from round 16 section 4).
- Think arbitration (round 16 section 3): not started.
- Full acceptance: not run (nothing changed).

## 4. Next steps (numbered)
1. Ring source: for the seq025/seq027 callers, read the playing clip's motion number (P+0xAE0) and frame, compute its root displacement per frame from the clip (the same reader GF uses for `animMovement`), and compare with the captured ring entries 0..5. If they match, the feed fix is "ring from clip root motion" with that as the test. If not, decode the P+0x2FC writer (`FUN_141EB5E70`/`0x1406F0F70`, per `pes-re-trap-inputs.md` 1.3).
2. Rig: no new hook needed for step 1 (the existing `trapopen_14079A470` captures seq025, seq027 carry the ring and clip). A hook would help only for a resting-ball refusal with a moving teammate (code 0): `hook trapopen_14079A470`, predicate `b(rdx+0x2458)` = caller slot with a capture taken when `d(rdx+0x2fc+12)` is nonzero and `b(rdx+0xae0)` motion nonzero.
3. Race refusal (code 0, 162-ish of the sensitivity set): teammate arrival list is `MA+0x22E4C` / the ball samples; check GF's team-tick order (`team.cpp:440` writes ball sample 0 only, from `Predict(0)`) against PES.
4. RUN entry and Think arbitration: unchanged from round 16 (items 2 and 3 there).
5. Integration: merge StallRun, StallThink and StallBreaker2 SHAs when they arrive; re-run the acceptance set.
