# Seed-11 "hang": no PES loop; the seed-11 two-minute PES-loop match stalls on the hasIt tree and completes on f2458223

Branch `StallHang` (worktree `<workdrive>/gf-StallHang`), pushed: `origin/StallHang` = `9a32c6abf157cf5326e97fd57e6e0656f1424379`, parent `f2458223`. Scope: test only (`tests/sim/pes_hang_test.cpp` + one CMake target). No layer, translator, anime-engine or `pesworldrecord.cpp` change: nothing in my lane was wrong.

## Verdict

1. **The seed-11 two-minute PES-loop match does not hang on f2458223.** It reaches full time in 38,867 sim ticks, determinism hash `3b7cc25daa988248`, reproduced in two plain runs and once under gdb. CONFIRMED.
2. **On `d9ed268c` (the PES-path hasIt rule, the tree the day file recorded as hanging), the match never finishes, but it is a stall, not an in-tick loop.** A stoppage ends in GF's own sliding idle clip beside a resting ball, and the match runs on until the cap. CONFIRMED.
3. **Nothing in `FUN_1423AB2F0` / `FUN_14239B440` was observed looping forever.** Every tick returned in the run to the cap, so the gdb stack in the brief is a normal per-tick path through the body-anime Exec. The count-based loop theory was tested and not seen (section 3). CONFIRMED that it was not seen; INF that it cannot occur.
4. **Acceptance:** seed 11 reaches full time on the branch (`f2458223`, and the new test is green there). The fix that makes it so is the hasIt revert already in `f2458223` (StallLive's file, `elizacontroller.cpp`), not an anime-side change.

## 1. Reproduction

Command (the brief's, plus the env it needs): `PES_EXE=<home> tools/dev/simslot.sh timeout 1800 nice -n 19 build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 11 --minutes 2 --pes21-root <workdrive>/PES21 --pes-loop` (without `PES_EXE` the binary exits 1 at startup).

| tree | result | ticks / wall | note |
|---|---|---|---|
| `f2458223` (branch tip), run 1 under gdb | `Full match complete: HOM 0 - 3 AWA`, exit 0 | 38,867 / 571 s | determinism `3b7cc25daa988248`, restarts longest 1,277 ticks |
| `f2458223`, run 2 (plain) | same | 38,867 / 570 s | same hash |
| `d9ed268c` (only `elizacontroller.cpp` restored, rebuilt) | `[restart] stalled: no restart within 1758 ticks (limit 1757)`, exit 1 | ~340 s | the restart watch aborts |
| `d9ed268c`, watch disabled (env-gated diagnostic, reverted) | `[cap] no full time after 194400 sim ticks`, exit 1 | 1,510 s under gdb | every tick returned |

The two trees differ only in `src/sim/onthepitch/player/controller/elizacontroller.cpp` (`git diff --stat d9ed268c f2458223`: one file). The hasIt line (elizacontroller.cpp:657): `d9ed268c` = `GetBallRetainer()==player || (PesLoop ? GetPesHolder()==CastPlayer() : HasPossession())`; `f2458223` = `GetBallRetainer()==player || HasPossession()`.

## 2. The stall on `d9ed268c` (CONFIRMED from the run log)

- The stoppage that ends the probe in the harness test (`pes_hang_test`, frame 23,341, longest stoppage 1,758 ticks) started near frame 21,583 (23,341 − 1,758, INF from the arithmetic).
- Breaker lines, repeated through the rest of the run: `[stalemate-who] H11 (10): movestep 7<-1 | fn 13 clip assets/gf_anims/sliding/idle/000.anim frame 14/47 | 1.4 m from the ball, 1.8 m/s`, with the ball at 0.08 m/s at (-28.7, -3.0), and `forcing H11 (10) from 1.4 m` on every stall tick. A GF stand-in clip (fn 13, `assets/gf_anims/sliding/idle/000.anim`), not an anime-chain clip.
- So the ball lies still, one man is designated and forced, and nobody's trap contact is ever granted. This is the stall class of `ALiveBallNeverLiesStillLongEnoughForTheBreaker` (liveFirings) and `ATrapTouchIsTheBodyScansGrantAndNothingElse`. The cause is the walker/trap-contact path for a non-holder beside a resting ball (`docs/re/pes-re-walker-touch.md`: GF over-refuses at `humanoid.cpp:584-597`). That path is owned by StallContact/StallLive, so I did not change it. INF.

## 3. The loop theory: tested, not seen

Code (translated, `src/pes/translated/closure/pes_fn_14239B440.cpp`, labels are PES VAs):
- Outer loop `L_14239B492`..`L_14239B532`: index `i` (`gpr[6]`, u32) runs while `i < count`, with `count = u32[r8+0xC]` (line 65), and `r8` is the node passed down from `FUN_14239A870` (its line 58 call).
- Inner loop `L_14239B650`..`L_14239B6BC`: runs while `i < gpr[14]`, where `gpr[14]` is `4·k` after `k` outer steps. It calls `FUN_1423AB2F0` at line 388 on every step.
- `FUN_1423AB2F0` itself has no backward branch (CONFIRMED from its file): it is a leaf that writes its frame only.
- So a huge `count` (near `0xFFFFFFFF`) would make the outer loop run that many steps, each calling `FUN_1423AB2F0`. That is the only shape of loop in the chain, and it is what the brief's gdb stack suggests.

Test of it: a conditional breakpoint at closure line 66 with `s.gpr[12] > 256`, on the `d9ed268c` tree with the watch disabled. It hit zero times in the ~2,400 s it ran before its timeout (`GDB_RC=124`, the run did not reach the cap, so this is partial). CONFIRMED: no oversized count in that window. INF: the count is never huge on this path, since every tick returned.

Why the brief's gdb stack misled: `FUN_1423AB2F0` is the leaf every man's body Exec calls every tick, so a sample taken mid-tick lands there. A single gdb stack cannot tell a loop from a normal tick. The 45-minute sample was taken in a stalled match, which runs ticks at full speed.

## 4. What is out of my lane (for Main)

- The seed-11 stall is GF's H11 staying in a stand-in clip beside a resting ball, with no PES trap issued. The fix belongs to StallContact/StallLive (walker/trap path), not to the anime engine or the translator. Need: decide whether that port is the seed-11 fix, or whether the revert alone is acceptable.
- No anime-side change is warranted: no ledger row, no layer change, no `gen_translated --check` needed.
- `src/sim/onthepitch/pesfoxworld*` does not exist in this tree; `RunAdjusters` and `BindAnimeChain` are in `pesworldrecord.cpp` (read only).

## 5. Test

`tests/sim/pes_hang_test.cpp` (new), target `pes_hang_test` in `tests/sim/CMakeLists.txt`, `TIMEOUT 1800`. One shared `pestest::Probe` on `{pesLoop=true, minutes=2, seed=11}`, `stallStops=true`, `judgeEnd=true`. Asserts full time (`stop.gameOver`), and the shared `Settle` also checks `!stalled`.

- Red on `d9ed268c`: `[  FAILED  ] PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime (320411 ms)`, `stopped at frame 23341, longest stoppage 1758 ticks`, exit 1. Bounded: 320 s.
- Green on `f2458223` (= `9a32c6abf1` tree): `[       OK ] (570496 ms)`, `PASSED 1 test`, exit 0.
- Mutation: re-applying the PES-holder hasIt rule on `f2458223` is the `d9ed268c` tree; that run is the red above. Reverted.

## 6. Not done, and why

- **Gate:** `GATE_TIER=fast` not run. This is a test-only change (one new binary, ~10 min). The fast-gate line I would put here is `GATE fast: NOT RUN (budget)`; the stall reds on `pes_restart_delay_test` stay as on `f2458223`. Main gates.
- **Ledger / layer:** unchanged. `gen_translated --check` not run (no layer change).
- **Commit path:** `commit.draft({repo: "<workdrive>/gf-StallHang"})` with the submodule symlink swapped out and back; SHA `9a32c6abf1`, `ls-remote` confirms `9a32c6abf157cf5326e97fd57e6e0656f1424379`.
- **Scratch:** `<workdrive>/gf-StallHang-scratch/{s11,bin}` (logs, gdb command files). Remove with the worktree.

## Open items

1. OPEN: why the non-holder beside a resting ball gets no trap in the `d9ed268c` state (walker/trap path; StallContact/StallLive).
2. OPEN: whether seed 11 needs its own census seed after the revert (the seed list was re-staged on `d9ed268c`).
3. INF: the `pes_hang_test` runtime (570 s) adds to the sim budget of the fast gate.
