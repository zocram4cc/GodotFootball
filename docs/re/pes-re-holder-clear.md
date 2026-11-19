# PES holder MA+0x2601C: when it is set, kept and cleared on a loose ball, and what the holder does beside a resting ball (HolderClearRE, 19-11-26)

Status: static decode + verify script (43 checks, 1 planted mutation caught) + one GF probe (single seed, probe worktree only). No engine code landed; no commit; no gate. Branch under study: r18 (`<workdrive>/gf-AnimeLayer`, uncommitted on base `8a65de72`; snapshot taken 19-11-26, Main keeps editing r18). Probe worktree: `<workdrive>/gf-HolderClearRE` (detached `8a65de72` + the r18 snapshot patch + probe-only test and an env-gated mutant; removed at the end).
Scratch: `<workdrive>/gf-HolderClearRE-scratch/` (`verify_holder.py`, `scan_disp.py`, `r18.patch`, decompiles `dec1.c dec2.c dec3.c`, run logs). Ghidra bridge (8080) was DOWN; all decompiles came from `dec.sh` (headless PESLive, one at a time). Image: `pes_full.bin`, flat, file offset = VA − 0x140000000; control `0x14256AE1C` = 1.0f.
Tags: **CONFIRMED** = read in the image (verify script or decompile/translated file generated from the image) or measured; **INF** = inference (reason given); **OPEN** = not settled (blocker named).

## 0. Verdict (3 lines)

1. **Clear (CONFIRMED):** PES has no clear of MA+0x2601C on distance, time, touch or stoppage. The only writers are W1 (`0x140457259`, value = actor of the newest ring-1 key, else `0xFF`) and the object reset W2 (`0x140A71DF9`). A loose ball keeps the last valid key's actor (INF high); it becomes `0xFF` only if the newest ring-1 key is invalid or ring 1 is emptied (writer of the count reset OPEN).
2. **Holder action (CONFIRMED):** a MOVE(2) holder is refused by the gate-3 carrier clause (`0x1409FA4AD..0x1409FA4D2`) and his MOVE tick admits no TRAP (`0x1407824A0`: only demo 0x52/0x53). A holder touches a resting ball only from DRIBBLE(4), on the DRIBBLE tick (`0x1407B0D42`, `0x1407B35C9`, EDX=7). DRIBBLE is entered only from the trap-end family (`0x140796C1B` in `FUN_140796B30`, `0x14079F17D` in `FUN_14079EE70`), never from MOVE.
3. **GF rule to change:** `teamAIcontroller.cpp:3575` writes MOVE for every man (`TrapFactsOf`), so GF has no DRIBBLE man and never runs the DRIBBLE-tick admission; `playercontroller.cpp:416` and `elizacontroller.cpp:657-658` then refuse and hold the holder in the carrier brain. Probe: worst holder-stall run 1.04 s (bound 1.0 s, harness) with 439 stalled ticks in one seed-7 match on r18; dropping the carrier refusal (M1, PES-divergent) made it 4.37 s.

## 1. Constants (script output, `verify_holder.py`, pasted; read from the image)

```
1.0            0x14256AE1C  f32 1.0            flat control (CONFIRMED)
100.0          0x14253EE30  f32 100.0          gate-3 reach, C1 in m^2 (10 m)  [pesreceive / 0x1409FA480]
0.71           0x1425EB848  f32 0.7099999785   DRIBBLE-tick kind threshold     [0x1407B3525 family]
25.0           0x14259BE20  f32 25.0           ebf90 frame factor (not re-used here)
sentinel kind  0x1436F3A40  u32 0x37           empty ring record: key -> FUN_140A60500 = {team 2, idx 0xFF} (invalid)
```
Operand-VA pairs (instruction address → operand VA, as Main re-checks): `0x1409FA480 COMISS XMM0,[0x14253EE30]` = 100.0; `0x140A3E7D9 CMP [RCX+0x3208],R8D` (ring-1 count); `0x140A3E7EB MOV EAX,[RCX+0x320C]` (ring-1 head).

**Correction (brief):** the W1 store is at **`0x140457259`** (`mov dword ptr [r13+0x2601c], edi`, 7 bytes). `0x14045725A` is inside that instruction; it is not an instruction start. ApproachRE §3 and first-touch §5 quote the same wrong VA. Also, the instruction is a W1 write of a key actor, not a clear.

## 2. Q1: writers of MA+0x2601C and the ring-1 newest key

**Scan (CONFIRMED, `scan_disp.py`, byte-anchored, immediate-tolerant):** 21 memory operands with disp 0x2601C: 2 writes, `0x140457259 mov [r13+0x2601c],edi` (W1) and `0x140A71DF9 mov [rbx+0x2601c],0xff` (W2); 19 reads. Same count as HolderRE §1. Ring-1 count disp 0x3208: 5 operands, one write `0x14091FF06 mov [rcx+0x3208],eax`; head disp 0x320C: 4 reads, no disp32 writes.

**W1 = FUN_140457070 (decompile `dec1.c`, CONFIRMED):**
- Early return: `if (*(char*)(param_1+0x11) == 0) return;` (TA+0x11 flag; its setter callers OPEN, HolderRE §8).
- Key: `FUN_140A3E7D0(E, out, n=0)` = newest ring-1 record. If `n < E+0x3208` (count) the record is `E+0x1908 + ((E+0x320C − n) mod 8)·800`; else the static sentinel `0x1436F3A40`. Sentinel/kind `0x37` → `FUN_140A60500` = `{2, 0xFF}`.
- Valid: `FUN_140A60530`: `team < 2 && idx < 0x28` (`cmp [rcx],1; ja; cmp [rcx+4],0x27`, verified).
- Actor: `FUN_140A62E40(team_rec, key)`: needs `team_rec.i32[0] == key.team`, then `FUN_140A3B320(rec, idx)` = `team·11 + position` if the key's man is in the first 11 of the team's 40-list, else 0xFF.
- Store: `MA.u32[+0x2601C] = actor` at `0x140457259` (CONFIRMED). Invalid key → `0xFF`.
- Between the key read and the store there is **no ball or time read** (decompile lines 113–127, CONFIRMED). The frame test `5 < MI.frame − ring0_newest.frame` (`FUN_140A3E190`) gates only the fallback loop, whose result goes to MA+0x26010 (not 0x2601C).

**The clear rules:**
| Path | Writes | Status |
|---|---|---|
| W1, newest ring-1 key valid and on pitch | actor | CONFIRMED |
| W1, newest key invalid (count 0, sentinel, off-pitch, team mismatch) | `0xFF` | CONFIRMED |
| W1 not run (TA+0x11 clear) | nothing (value kept) | CONFIRMED path; caller OPEN |
| W2 object reset `0x140A71DF9` | `0xFF` | CONFIRMED; caller OPEN (HolderRE §1) |
| Ring-1 count reset `0x14091FF06` (`mov [rcx+0x3208],eax`; the same EAX goes to 9 counters at E+0x8·{0x641,0x590,0x4DF,0x42D,0x37C,0x2CB,0x217,0x164,0xB1,0} ) | with EAX = 0 → ring 1 empty → `0xFF` on the next W1 | CONFIRMED write; containing function has no .pdata (entry OPEN); no E8/E9/qword refs (OPEN) |
| Ball distance or time | none | CONFIRMED: no read in W1's key→store path; no other writer of 0x2601C |

**Ring-1 producers (K=8), `FUN_140A0DFD0` (decompile `dec2.c`, CONFIRMED structure):**
- Input record kind must be `0x14`. Branch A (`rec+0x18`) and branch B (`rec+0x19`) both push a K=8 record through `FUN_140A3DC40`, and only when `FUN_140A3E7D0(E,·,0)` is valid (`FUN_140A60530`). So a valid newest key is never replaced by an invalid K=8 push.
- The K=8 record is built by `FUN_140A3D060`: kind 8, frame (`MI+0x1198`), frame, byte at +0xC (from `FUN_140A6BD60/6BE70`), state (`MI+0x1168`) at +0x10. **No team/idx field is set** in the record.
- Branch B also pushes K=7 (touch) when probe `G+0x2A80` has `+0x64 == 1` and `+0x18 < 22`; K=7 is ring 0 (the doc routing), which W1 does not read.
- **OPEN:** the actor stored in the new ring-1 slot. It is written in `FUN_140A3DC40` / `FUN_140A3DAC0` (slot copy), which I did not finish decoding. INF (high): the K=8 push keeps the newest key's actor, because the producer pushes only while that key is valid and writes no actor of its own. So a loose ball keeps the last valid holder, which is what PES does.

**Verdict Q1 (CONFIRMED / INF):** the holder value changes on a loose ball only by (a) a newer valid ring-1 key with another actor (not produced by the K=8 builder as far as decoded; INF no), (b) ring-1 emptying (count reset `0x14091FF06` with EAX 0, caller OPEN), or (c) W1 not running (TA+0x11 clear, caller OPEN). No distance or time clear exists.

**GF side (r18):** `Match::RecomputePesHolder` (`match.hpp:278-300`) adopts `pesKeyActor` if non-null and never nulls the holder (`:294`). `pesKeyActor` is written only by touch writers (`humanoid.cpp:1401` keeper catch, `:1509` CarrierFromTouch). So GF lacks path (b) (and (c) is replaced by the restart-step switch, `match.cpp:906-910`). The persistence matches PES; the clear does not exist on either side.

## 3. Q2: what the holder does beside a resting ball

**Gate 3 `FUN_1409FA400` (decompile `dec2.c`/asm, CONFIRMED):**
1. Reach: `0x1409FA480 COMISS XMM0,[0x14253EE30]` (100.0); `ja 0x1409FA4D2` refuses if C1 > 100 (10 m, from the row table `MA+0x24710`, index = row byte `+0x247BE` + 11·side).
2. `carrier = FUN_140A627E0(ctx, 0)` (= MA+0x2601C, `0x140A627EC`).
3. **Carrier is the man** (`carrier == id`): `r14 = FUN_140A48910(G, id)` (`G+0x19C8+40·id`, translated `ai/pes_fn_140A48910.cpp`), and the action byte is `[r14+1]` (`0x1409FA4AD movzx ecx, byte [r14+1]`). Admitted iff:
   - `0x44` (ANIME_KP_DROP_BALL) and `MA+0x251FA[side] == 0` (`0x1409FA4B2`, `0x1409FA4B9`), or
   - `0x10` (STAGGER, `0x1409FA4C8`), or `0x15` (DODGE, `0x1409FA4CD`).
   - Otherwise refused at `0x1409FA4D2`. **So a MOVE(2) or DRIBBLE(4) holder is refused.** CONFIRMED.
4. **Carrier is a teammate** (same side, `carrier ≤ 0x15`): refused if the carrier's row byte `[carrier+0x247BE] != 0` (`0x1409FA4DF`, `0x1409FA4E9`); else the sub-gate. **Not ported in GF** (INF on the meaning of `+0x247BE`: a per-man table row, OPEN).
5. Other side / no carrier: sub-gate `FUN_1409FA520` (`0x1409FA4F7`), which holds the designated-owner refusal `MA+0x2600C == me` (`0x1409FA57B`, verified).

**MOVE tick `FUN_1407824A0` (decompile `dec3.c`, CONFIRMED):** calls `FUN_1406B4580(anime, 0x52, …)` (`0x1407824F3`), then `0x53` (`0x140782524`), then `FUN_1406B8340` (movement continuation). It never admits TRAP (7) and never commits DRIBBLE (4). **A MOVE holder cannot touch from his own tick.**

**DRIBBLE tick (action 4), CONFIRMED sites (verify script):** `FUN_1407B05F0` (`0x1407B0D42` EDX=7 admit, `0x1407B0D57` commit `FUN_1406EEBB0(7,0x5E)`) and `FUN_1407B3470` (`0x1407B35C9` admit, `0x1407B35E6` commit). Gates in helper A: ball-present flag `[ball+0x5C3]` (`0x1407B0C76`), plan byte `[plan+0x35]` (`0x1407B0C8D`), the action's `+0x70` handler. Kind from `0.71` (`0x1425EB848`). **No distance test and no gate 3.** (R2 values re-read; translated `closure/pes_fn_1407B3470.cpp` agrees.)

**How a man enters DRIBBLE (CONFIRMED structure, `dec3.c`):**
- `FUN_140796B30` (`0x140796C1B` = `FUN_1406B4580(anime, 4, …)`, then fallback `FUN_1406EEBB0(anime, 2, 0x5E)` = MOVE 2 at the end). Entry gates: anime class test (`0xBE5/0xBE7`), `FUN_1406E6CB0` vs a threshold, `FUN_14079D4B0`, then `FUN_14079F440` / `FUN_1406EC5F0`.
- `FUN_14079EE70` (`0x14079F17D` EDX=4 admit, returns 1 on admit).
- **Correction (R2):** R2 §3 attributes `0x140796C1B` to the trap tick `FUN_140797CD0`. The containing function is `FUN_140796B30` (its .pdata range `0x140796B30–0x140796CDF`). `FUN_140797CD0` is a different function.

**Answer (CONFIRMED structure):** a man goes DRIBBLE only at a trap end (the trap-end family), then admits TRAP 7 on each DRIBBLE tick while the ball flags hold. A man who has not trapped stays MOVE and cannot touch by any admission found. So the PES touch loop is TRAP → DRIBBLE → TRAP on the same man, and the touch needs no distance gate.

**GF side (r18):** `teamAIcontroller.cpp:3575` `f.action = f.sub = f.prevAction = f.prevSub = pes::kAnimeActionMove;` for every man (`TrapFactsOf`, comment at `:3568-3574` says so). So GF's P action byte (`kPByteAC9Offset`, read at `playercontroller.cpp:415`) is MOVE for everyone: the carrier clause refuses every holder, and no DRIBBLE state exists to run the DRIBBLE-tick admission.

**OPEN:** the PES action of a restart taker or a pass receiver who holds a resting ball (MOVE or DRIBBLE). Needs a hook on `FUN_140796B30` entry or the anime action switch (see §7).

## 4. GF rules that diverge (r18 snapshot, file:line)

| # | r18 file:line | Rule | PES fact | Divergence |
|---|---|---|---|---|
| D1 | `teamAIcontroller.cpp:3575` | `TrapFactsOf` writes MOVE (2) for every man | the man's real anime action; DRIBBLE after trap end (§3) | No DRIBBLE state, so the DRIBBLE-tick admission never runs. **Rule to change.** |
| D2 | `playercontroller.cpp:413-416` | `if (GetPesHolder()==me && !CarrierMayTouch(actionType,false)) return false` | gate-3 carrier clause (§3.3) | Holder rule matches for carrier==me, but the action byte is D1's MOVE. The teammate clause (§3.4) is missing. |
| D3 | `elizacontroller.cpp:657-658` | `hasIt = GetBallRetainer()==player \|\| GetPesHolder()==CastPlayer()` | the holder's touch comes from the DRIBBLE tick, not from the carrier brain | With `hasIt` true the step onto the ball (`!hasIt`) is skipped and the carrier brain runs; it queues nothing for a man 1 m off a resting ball. |
| D4 | `match.hpp:278-300` | `RecomputePesHolder`: adopts `pesKeyActor`, never nulls | W1: newest ring-1 key or `0xFF` (§2) | Missing the `0xFF` path (invalid key / ring emptied). Persistence itself matches PES. |
| D5 | `pesreceive.cpp:321-325` | `CarrierMayTouch` = {0x44 w/ flag clear, 0x10, 0x15} | same set (CONFIRMED) | The set is right; the teammate branch of gate 3 is not in `_ReceiveCandidate`. |

Not a divergence: `humanoid.cpp:1387` (retain clear on ball leave) is GF's loose-ball predicate with no PES counterpart (HolderRE §2); it is GF's own and separate from the holder.

## 5. Red-test spec (PES facts) and the r18 measurement

**Probe test (`tests/sim/pes_holder_stall_test.cpp`, probe only):** PES loop, seed 7, 2 min (as `pes_pass_target_test`).
- **Scenario:** each tick: in play (`PesPlayState()==1`), ball speed < `kStalemateBallSpeed_ms` (0.5 m/s, GF's rest bound), a PES holder `GetPesHolder()` 0.4–1.6 m from the ball (2D).
- **Run:** consecutive stall ticks; a hand-over inside the run does not restart it; the ball being touched or moved does.
- **Assertion:** longest run ≤ `kHolderStallMax_s` = 1.0 s.
- **Bound provenance:** the 1.0 s bound is a **harness choice (DESIGNED)**: 54 ticks, one dribble cycle. The 0.4–1.6 m band is measured in the GF probe (ApproachRE), not a PES constant. PES does not decode a holder time bound; the spec does not claim one.
- **PES fact behind it:** a DRIBBLE man admits TRAP on his DRIBBLE tick with no distance gate (§3), and a MOVE man is refused by gate 3 (§3.3); so a resting ball beside a DRIBBLE holder is touched on the next admitted tick.

**Measurement (probe worktree, detached `8a65de72` + r18 snapshot, Release build, `pes_holder_stall_test`, seed 7, 2 min, `PES_EXE=pes_full.bin`, one sim slot):**
| run | worst run (s) | runs over 1.0 s | stalled ticks | holder ticks (in play) |
|---|---|---|---|---|
| r18 snapshot (base) | **1.037** | 3 | 439 | 8808 |
| M1: env-gated removal of `playercontroller.cpp:416` carrier refusal (PES-divergent) | **4.370** | 10 | 1522 | 12028 |

- Base is red, by a small margin (1.04 s vs 1.0 s, single seed). M1 is red by more. Both are one seed; the match trajectories differ, so the counts are not a clean A/B. Noise is not measured (one run per arm).
- **Mutation spec:** the spec must turn red when the stall returns. Mutant M1 (drop the holder refusal) is the one measured: it went redder (4.37 s), so the test is sensitive to the carrier rule, but M1 is PES-divergent (PES refuses MOVE holders), so it is not the fix. The green-direction mutant (a DRIBBLE state with the PES admission) is not built; OPEN.
- Not run: `GATE_TIER=fast` (agents do not gate), other seeds.

## 6. Verification

- `verify_holder.py`: **CHECKS 43, FAILURES 0**. Covers the flat control; W1 store `0x140457259` (mov [r13+0x2601c],edi, .pdata begin `0x140457070`); W2 `0x140A71DF9`; ring-1 count compare `0x140A3E7D9` and head read `0x140A3E7EB`; leaves `0x140A60530` (valid key) and `0x140A60500` (sentinel `mov [rsp+8],2`) (no .pdata, decoded from the VA); carrier accessor `0x140A627EC`; sentinel kind `0x1436F3A40` = 0x37; gate 3 `0x1409FA400` begin, `0x1409FA480` vs 100.0 (rip target), `0x1409FA4AD` byte load, `0x1409FA4B2/4B9/4C8/4CD/4D2/4DF/4F7` opcodes, `0x1409FA57B` designated owner; EDX=7 sites `0x1409F878D/0x1409F880D`, `0x1407B0D42/0x1407B0D57/0x1407B35C9/0x1407B35E6`; `0x1407B0C76` (0x5C3), `0x1407B0C8D` (0x35); MOVE tick `0x1407824A0` begin, `0x1407824F3` and `0x140782524` (0x52/0x53); `0x140796C1B` (EDX=4); `0x14079F17D` (EDX=4); ring counter write `0x14091FF06` (leaf, no .pdata); 25.0 constant.
- `--mutate` (W1 store moved by one byte): **CAUGHT**, only `W1 store` fails.
- Every VA above was decoded from its .pdata range or as a leaf from its own VA; all are instruction starts.
- Ghidra decompiles: `dec.sh` (headless). Bridge down (port 8080 refused).

## 7. OPEN items and hooks wanted (Main owns the rig)

1. **Ring-1 slot actor** written by the K=8 push (`FUN_140A3DC40` → `FUN_140A3DAC0`). Decode the slot copy; then the loose-ball holder question closes (INF → CONFIRMED).
2. **Ring-1 count reset** `0x14091FF06`: containing function entry and its caller (EAX value). Decide whether a reset (EAX 0) occurs during play.
3. **W1 caller / TA+0x11 setter** (HolderRE §8, OPEN): decides whether W1 runs on a loose ball at all.
4. **Teammate row byte** `+0x247BE` meaning (gate 3, teammate branch).
5. **Holder's PES action** after a restart kick and after a pass (MOVE or DRIBBLE): the decisive open fact for the stall.

Hooks wanted (none started):
- `holder_w1_140457070` (entry of W1, VA `0x140457070`): predicate `in play and ball speed < 0.1 m/s and a PES holder 0.4–1.6 m from the ball` (Main to write the predicate on the holder's record), cap 24, FULL. Answers OPEN 3 and whether W1 runs on a loose ball.
- `ring1_count_reset_14091FF06` (VA `0x14091FF06`, entry): log EAX and the caller (return address), cap 12, FULL. Answers OPEN 2.
- `dribble_enter_140796B30` (entry `0x140796B30`): log the anime action before/after, cap 12. Answers OPEN 5 (which man goes DRIBBLE beside a resting ball).

## 8. Notes

- The 8.5 s figure in ApproachRE/brief is the pass-target stale time (`pes_pass_target_test`), not a holder time. The holder run is 1.04 s worst on r18 (single seed).
- The r18 snapshot is the uncommitted working tree at this session (`git diff HEAD` on `8a65de72`, excluding the submodule). It changes under Main; re-run the verify script and the probe test after any r18 change.
