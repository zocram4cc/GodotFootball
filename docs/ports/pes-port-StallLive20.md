# pes-port-StallLive20: StallThink merged into StallLive (ALiveBall 2,401 -> 118, still red); code-7 scan inconclusive; StallRun not merged (StallLive, 21-11-26)

Status: PARTIAL. ALiveBall still RED at 118 forced firings. Branch `StallLive` pushed at **43e96723bc76** (ls-remote confirmed): a no-ff merge of StallThink e49318d3 onto 1b56b726. Build dir deleted. Worktree clean.

## 1. Merge (done, pushed)
- `e49318d3` (StallThink, "a fired reaction replaces an earlier movement; holder decides the dribble") merged with `--no-ff` (merge base 1b56b726, no conflicts). Touches `pesmovestep.cpp/.hpp` (MovementDecision: a non-movement queued command wins, the first movement stands otherwise; the holder test uses `GetBallRetainer()==self || GetPesHolder()==self`), `tests/sim/pes_movestep_wire_test.cpp`, `tests/sim/pes_press_approach_test.cpp`.
- Commit subject for the merge names StallThink; its body is the merge message.

## 2. Acceptance on 43e96723 (run by me; all processes exited)
| Check | Result |
|---|---|
| PesStall ALiveBall | RED: **118** forced live-ball ticks (was 2,401 on 1b56b726) |
| PesStall (other 7) | PASS |
| PesHang seed 11 | PASS (782,590 ms) |
| PesPassTarget | PASS |
| Throw-in 9/9 | 8 PASS (ceremony test NOT run in this set; it takes ~30 min; run it next session) |
| PesPressApproach: ALoosePresser... | PASS |
| PesPressApproach: ANoManWhoDoesNotHoldTheBallIsDecidedByTheDribble | PASS (StallThink's test) |
| PesPressApproach: ADesignatedManWithNoHolder... | RED: 4 of 259 (was 6 of 563) |
| Seed-7 2-min smoke | 29,069 ticks, determinism c549549988d6c0fb (close to the ~30k target) |
| ledger --check | GREEN (47,642 rows) |
| gen_translated --check | not re-run on the merged tree (StallThink touches no translated code; last green: 10,150 functions on 1b56b726) |
| kick-clear mutation | not re-run on the merged tree (last red on 1b56b726) |

## 3. StallRun: not merged (by rule)
- `origin/StallRun` = **8ef8ae8747b4abe04f63110c09ef37b7ba91bb6b** (one commit on 1b56b726: issued clips start at their SetMotion start time; `humanoidbase.{cpp,hpp}`, `tests/sim/pes_movestep_wire_test.cpp`). StallRun reports ALiveBall 2,401 -> **607** on its own tree.
- Not merged: its measurement is red and it was not measured on top of 43e96723. Asked StallRun (agent://StallRun) to re-measure on 43e96723 and send the count with acceptance results. Next session: merge it into StallLive with `--no-ff` once that is green, then re-run the acceptance set. Expect a textual overlap risk in `humanoidbase` only if StallThink touched it (it did not).

## 4. Code-7 scan (round 20 static/empirical attempt): inconclusive
- Static: the candidate query `FUN_14084DF20` (translated, `src/pes/translated/closure/pes_fn_14084DF20.cpp`, 322 lines) calls `FUN_14084C020`, `FUN_14084BCA0` (height class), `FUN_1406E4230`, `FUN_1406EB100`, `FUN_1406B0ED0`, `FUN_1406AF8E0`, `FUN_14084BD70`, `FUN_14084CEE0`, `FUN_140856F40`, `FUN_140854D40`, `FUN_1406ABBA0/ABDD0`, `FUN_1406EA840`, `FUN_1406E6640` and `FUN_1406E8320`. It writes the query record at `+0x1C`, `+0x64`, `+0x70`, `+0x88`, `+0x90`, `+0xC0`. The anime-object reads are inside the callees, not in this function, so a per-field decode needs the callees (not done).
- Captured anime record (PES, seq025/seq027 caller): all zeros over the bytes the chain reads (`+0x1` class and `+0x1A..+0x1D`). GF's window copy matches. So the anime record is NOT the code-7 feed gap for those captures.
- Live block scan (GF, seed 7): zeroing single 8-byte words of the chain's input regions (ball, MA, events, carrier, caller's trap image, caller's player block, MI) on the first code-7 call: **0 outcome flips** before the run was stopped. Naive whole-window zeroing crashed the translated code (null dereference from zeroed pointers), so pointer-valued words (>= 2^32) were skipped. This scan is partial and inconclusive: the run was stopped at a clean point (diagnostic reverted; `src/` equals the merge).
- Live sensitivity (round 17, unchanged): anime class bytes, P+0x377C: 0 flips; P+0x2FC ring: 162 of 5,071.

## 5. Next step (exact)
1. Re-run the code-7 block scan with the pointer-skip rule only on the MA arrival list (`MA+0x22E4C`, the 11-entry NearestList rows) and the event store (`kEventsOffset`), at 8-byte granularity, on the first 3 code-7 calls (env `GF_SCAN7`, the probe in round 20 is recorded in this report's history; re-add it, it is ~30 lines around `playercontroller.cpp:522`). Let it run to completion (~40 min); do not kill it.
2. A flip identifies the field the candidate set needs. Compare the GF value at that offset with the same record in one trapopen capture from `live2/pid1267009` (the EVERY-5 hook; none had landed when this was written).
3. Red test: a resting-ball man in reach gets a non-empty candidate set (the state the flipped field should carry).

## 6. Open items
- ALiveBall 118 (red). The remaining forced firings are the target of the next session.
- StallRun 8ef8ae87: merge after re-measure on 43e96723 (section 3).
- Ceremony test and gen_translated/kick-clear on 43e96723: run before the next landing.
- Code 0 (race) and code 3 (bearing/commit): not started.
- Ring open item (D/(10(k+1)) structure; contradicts `pes-re-ball-track.md` section 3): carried from round 17b.
- Trapopen hook at EVERY 5 on pid1267009: no resting-ball captures yet.
- StallBreaker 09a5f7e2 is held, not merged: it is blocked by the untranslated slide entry 0x140789d00 (via 0x1425E1908, the SLIDING vtable). Needs its own translation.
