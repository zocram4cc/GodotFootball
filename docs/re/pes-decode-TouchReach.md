# TouchReach: PES decode of three GF stand-ins (read-only)

Base: `origin/main` `a47f5f2`, read in `<workdrive>/GodotFootball` (no edits). Image: `pes_full.bin` (Ghidra bridge, base 0x140000000). Constants read from `<workdrive>/PES21/PES2021.exe` with `~/ghidra-projects/scratch/va.py`. Scratch: `<workdrive>/gf-TouchReach-scratch/` (decompiles saved as `d_*.c`).

Verdicts: CONFIRMED = decompile or file read; INF = inference; OPEN = blocked, with the blocker named.

## 1. Touch reach near the touchline: OPEN (no PES touch-reach constant found)

GF stand-in: `humanoid.cpp:3156-3157`, `pitchHalfH + lineHalfW + 0.11` = 34.17 m on z, and `pitchHalfW + lineHalfW + 0.11` = 52.67 m on x.

What PES does decode (CONFIRMED):
- **Pitch classifier `FUN_140a72e70` (VA 0x140a72e70), decompiled.** It takes the FieldInfo float array `F` (`F[0]` = half-length 52.56, `F[1]` = half-width 34.06; FieldInfo at `G+0x2aa8`) and a point `(x, z)`. Bit `0x1` is set exactly when `|x| <= F[0]` and `|z| <= F[1]`. There is no margin. Bit `0x6` means off in z, `0x10` means off in x. The only margin in the function is a centre-circle test (`F[0x11]`).
- So the only pitch-edge test PES has in this classifier is at 52.56 / 34.06, with no 0.11 term. The 34.17 figure is not a PES classifier boundary.

Where I looked for the touch-reach refusal (the PES side of `GetBestCheatableAnimID`'s out-of-bounds `continue`):
- `FUN_14081ff70` (clip picker; loops a 0x1c-stride table `DAT_14348dac0`, scores each clip's hit record against the ball). It reads no FieldInfo and does not call the classifier. No edge test. Its only ball-side input is the distance from the predicted ball to the hit point (`FUN_1406aff90`/`FUN_1406b05e0`).
- `FUN_140769fe0` (kick dispatcher; 100 KB; calls the picker about 60 times). It reads FieldInfo at `0x819` and `0x2431`, but the reads are aim/target geometry (`pfVar25[1]`, `[2]`, `[0x10]`), not a reach test. Not decoded further.
- `FUN_140838ca0` (header band scan, `0x140838ca0`). Angle gates only. No edge test.
- `FUN_1406b5c20`. Two-body contact solver. Not a reach test.

Verdict: OPEN. The PES function that refuses a touch because the predicted ball is off the pitch is not found. The chain to check next: the caller of `FUN_14081ff70` that feeds it the predicted ball, starting from `FUN_140769fe0`'s entry (the function that passes `param_2` into the picker). Blocker: the call graph is large and Ghidra xrefs are the only tool here. [INF] PES may have no such refusal at all, and rely on the restart spot being clamped in-bounds (see section 2).

Also relevant: GF uses the same stand-in `+ 0.11` at `referee.cpp:224` (x), `referee.cpp:292` (z), and `match.cpp:1057-1064` (goal plane). The PES classifier says the on-pitch edge is exactly F0/F1, so those are INF stand-ins too. The goal-plane value is a separate FieldInfo question (`pes-re-fieldinfo-consolidation.md` §4).

## 2. Fouled player / free-kick spot inside the pitch: CONFIRMED for the spot; the player is OPEN

**Whistle writer `FUN_14050e770` (VA 0x14050e770, decompiled).** Every whistle goes through it (`pes-re-duels-fouls.md` §2.1). Its args: `(judge, side, restart, reason, spot*, foulKind, fouler, victim)`. Restart kinds (`pes-re-foul-choreography.md` §85, INF(hi)): 1 kick-off, 2 throw-in, 3 goal kick, 4 corner, 5 free kick, 6 penalty.

`F` = FieldInfo pointer at `*(G+0x2aa8)` (`G = DAT_1436f3dc8`). Constants (read from the PE image):

| VA | value | use |
|---|---|---|
| `0x142565604` | 2.0 | restart-5 z inset |
| `0x14257a194` | 0.7 | goal-area push-out offset |
| `0x1425ab138` | 2.6 | goal-area push-out z |
| `0x14256ae1c` | 1.0 | sign +1 |
| `0x142588218` | −1.0 | sign −1 |
| `0x142565d30` | abs mask | |
| `0x14256ae18` | 0.01 | cm→m (used in the dispatcher, not here) |

FieldInfo rows used (`pes-re-fieldinfo.md`, OBS): F0 `+0x04` = 52.56 (outer goal-line edge), F1 `+0x08` = 34.06 (outer touchline edge), F4 `+0x14` = 47.06 (goal-area line), F5 `+0x18` = 9.16 (goal-area half-width).

**Explicit spot (`param_5 != NULL`), the path a foul or ball-out whistle takes.** Let `s = sign` of the restart team's attack direction (`T+0x254`), `(x, z)` = the spot:
1. If `|z| < F5` and `s·x < −F4` (spot inside the restart team's own goal area): `x ← sign(x)·(F4 + 0.7) = 47.76`, `z ← sign(z)·2.6`.
2. Write the spot to `JR+0x08` (through `FUN_140a3cd10`, which copies `JR+0x14` to `+0x08`).
3. If `|x| > F0`: `x ← sign(x)·F0` = 52.56. **Clamp, not a fold, no margin.**
4. If `|z| > F1`: `z ← sign(z)·F1` = 34.06.
5. If restart = 5 (free kick) and `|z| > F1 − 2.0`: `z ← sign(z)·(F1 − 2.0)` = 32.06.

**No-spot path (`param_5 == NULL`), the stored `JR+0x08` spot:**
- restart 2 (throw-in): if `|x| > F0 − 1.0`, `x ← sign(x)·51.56`.
- restart 5 (free kick): if `|z| > F1 − 2.0`, `z ← sign(z)·32.06`.

Verdict for the spot (CONFIRMED on the writer; INF on the argument): PES does clamp the restart spot inside the outer lines. A free kick from a foul that stands at `z = 34.2` is written at `z = 32.06`, not 34.06, and not the mirror 33.8 that GF's `RestartPosition` produces. `x` is clamped at 52.56 for every restart except a null-spot throw-in, where the limit is 51.56.

Caveat on the argument (INF(high)): the foul-commit caller is `0x143f546f0` (reason 5), in Denuvo-protected code. Its spot argument is not decoded. `pes-re-duels-fouls.md` says the spot is the offence point `MI+0x155c/0x1564`, which the foul choreography uses as the fouled player's point.

Verdict for the player (OPEN): `FUN_14050e770` clamps the **spot** (`JR+0x08`), not the fouled player's position. I found no clamp of the player's own position in this writer or in the foul-choreography code. Bystanders are the only ones moved (`FUN_14047d7f0`, `pes-re-foul-choreography.md` §2.1: a point off the pitch turns the bearing 180° and doubles the distance). Whether a tripee can stand past the line at the whistle is not decoded.

GF comparison (CONFIRMED from source):
- `offsiderule.cpp` `RestartPosition` (lines 141-161) folds x about `pitchHalfW − 0.5` = 52.0, and folds y about `pitchHalfH` = 34.0 (a mirror, `2·limit − v`). It does not know the restart kind. The new y fold gives 33.8 for the stalled case.
- PES clamps to F0 (52.56) and F1 (34.06), with an extra 2.0 m inset for free kicks (32.06). Those numbers differ from GF's 52.0, 34.0 and 33.8. The GF comment ("clamping it flat onto it aims along it") is a design choice PES does not share.

## 3. Goal stoppage of 1758 ticks against a 1757-tick bound: OPEN (arithmetic CONFIRMED; overrun cause not found)

**Bound arithmetic (CONFIRMED):**
- `restartwatch.cpp:31-33`: `goal_ms = GoalSequence::RestartKickOffAt_ms(0, kLongestCelebration_ms)`. `kLongestCelebration_ms = 29000` (`goalsequence.hpp:43`). `RestartPrepareAt_ms = 29000 + kRestartPrepareAfterReplay_ms (1500)` = 30500. `RestartKickOffAt_ms = 30500 + kKickOffAfterPrepare_ms (2000)` = **32500 ms**.
- `kTick_ms = 1000 · kPesTick_s = 1000/54` (`match.hpp:51`, `pesballphysics.hpp:27`). Float: `kTick_ms` = 18.518518656 (float `1/54` then ×1000). `32500 / kTick_ms` = 1754.99998692, so `ceil` = 1755. Bound = `1755 + kGateSlackTicks (2)` = **1757**. Exact double `1000/54` gives 1755.0 and the same 1757.

**What the goal schedule actually uses (CONFIRMED):**
- `referee.cpp:251-252`: `prepareTime = RestartPrepareAt_ms(now)`, `startTime = RestartKickOffAt_ms(now)`, both with the **default** celebration `kCelebration_ms = 19000` (`goalsequence.hpp`). So prepare = 20500 ms and start = **22500 ms** after the goal. 22500 / kTick = **1215.0 ticks**.
- The 29-s term is used by nothing that schedules a goal. `CelebrationLength_ms`, `ReplayFiresAt_ms` and `RestartKickOffAt_ms` have no caller in `match.cpp` or `goalcelebration.cpp`. The `match.cpp:UpdateIngameCamera` push-back that the `goalsequence.hpp` comment describes does not exist in this tree. So the restart watch's 32.5-s bound is about 10 s looser than any goal schedule this build runs.

**How a stoppage ends (CONFIRMED):**
- `RestartWatch::Observe`: `restarted = IsInPlay() && !IsInSetPiece()`. For a kick-off, `referee.cpp` `StartPlay(); StartSetPiece();` runs at `startTime` (`referee.cpp:~535-552`). The stoppage therefore runs until `StopSetPiece()`, which happens only when `buffer.taker->TouchAnim() && !TouchPending()` (`referee.cpp:~578-583`). The bound does **not** include that taker-touch latency. Only the 2-tick slack covers it.
- `Match::Process` runs `referee->Process()` (`match.cpp:717`) before `actualTime_ms += kTick_ms` (`match.cpp:843`). So the referee compares `startTime` with the time at the start of the tick. The match does not pause (no `pause` in `match.cpp`; the `goalsequence.hpp` comment about a paused clock is stale).
- The longest goal stoppage in the fixed run was 1251 ticks (A2Stall doc). That fits 1215 + about 36 ticks of kick-off latency (INF).

**Why 1758 is about 10 s longer than the schedule (OPEN):** The 1758-tick run was about 543 ticks (≈10.1 s) beyond `1215 + typical latency`. The in-tree paths that can add that time:
1. `AlterSetPiecePrepareTime` (`referee.cpp:681-716`, called only from `match.cpp:957`, the yellow/red card special hold). It pushes prepare to `now + 1000` each tick while the card's special function runs, capped at `stopTime + kMaxRestartDeferral_ms` = 30000 (`referee.cpp:40`). This fits a goal stoppage only if a yellow or red foul type was still current (`GetCurrentFoulType`). Unverified.
2. A taker that never produces `TouchAnim()`. The kick is refused, the stoppage holds, and the stalemate forcer re-picks at `kStalemateForceAction_s = 5.0` (`gameplaytuning.hpp:357`). This is the same failure class as the touchline freeze in `pes-re-touchline-restart-stall.md`, so Section 1 matters here. Unverified.

Discriminating experiment (for the implementer; not run here): a read-only per-tick trace from the goal tick to the stoppage end, logging `buffer.startTime`, `buffer.prepareTime`, `buffer.started`, `buffer.prepared`, `GetCurrentFoulType()`, `buffer.taker->TouchAnim()`, `TouchPending()`, and the taker id. Path 1 shows up as a moving `prepareTime`; path 2 shows up as a fixed `startTime` with `started = 1` and no touch.

## Open items
1. Section 1: find the PES caller chain that refuses a touch for an off-pitch predicted ball. Start at the `FUN_140769fe0` callers of `FUN_14081ff70`.
2. Section 2: decode the foul-commit caller `0x143f546f0` (protected). Confirm the spot argument is the fouled player's point. Check whether any player-position clamp exists.
3. Section 3: run the trace above on the pre-fix seed-7 trajectory. Decide whether the restart watch's bound should use the goal schedule the referee actually runs (22.5 s, 1215 ticks) plus measured taker latency, not the 29-s constant.
4. Stale comments to fix with the implementer: `goalsequence.hpp` (push-back that does not exist), `restartwatch.hpp` (goal bound described as the longest celebration).
