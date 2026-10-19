# Out-of-play side: writer of `*(G+0x40)+0x2798` (S1Watch, round 8)

> **Main audit 19-10-26:** `0x140872CC7` `mov r14d,[rsi+0x2798]` (read), stores `0x140872D3E` `mov [rsi+0x2798],ebx` and `0x140872D8B` `mov [rsi+0x2798],eax`, the call `0x140425D97 -> 0x140872C10`, and `0x142598958`=0.1086859 (ball radius), `0x14259BE04`=12.0, `0x14259BDF4`=8.0 re-read: all match. Port note: with the restart gate (p+0x298==3 -> code 0) and K=10 emitting on code change, a ball resting outside at restart end produces a change (0 -> outside) and so an event; no extra "spot" rule exists in PES.

Status: **CONFIRMED** (writer, classifier, constants, live exact match). The name "out-of-play side" is only partly right: the int is a **pitch-zone code** (bit field), and the out-of-play bits are 3 of its 14 bits. Live test: 4,447 of 4,447 nonzero observations reproduced exactly; 1,175 of 1,175 frames agree on the zero gate.

## 1. Which object holds `+0x2798`

- `*(G+0x40)` = BallInfo-side object, value `0x0A3E8B04` in this match (`G = *(0x1436F3DC8)`). The int `*(G+0x40)+0x2798` is written **every tick** (2,957 changes in 7,755 frames). The position at `*(G+0x40)+0` is **stale** (changes rarely).
- Correction to the earlier note: the 7-frame cadence came from the `pes_state` signature cache (`ball_addr.txt`, `0x1D5C2E20`), not from `G+0x40`. The `+0x2798` int on `G+0x40` is fresh per tick; its position is not.
- The ball position used by the writer is `B = 0x3562E904`, which is `p+0x64` for `p = B-0x64 = 0x3562E8A0` (CONFIRMED: `p+0x64` equals the candidate's xyz in every sample; INF: `p` is the writer's `param_1`, see §2).
- `p+0x298` is the gate (§3). `p+0x530` is the FieldInfo handle (§4).

## 2. Writer and caller

| item | VA | evidence |
|---|---|---|
| writer `FUN_140872C10(p)` | `0x140872C10` | decompile; per-tick ball update |
| sole caller `FUN_140425A90(q)` | call at `0x140425D97` | `q+0x20` = p, gated by `p != 0` |
| read of old value (line 74) | `0x140872CC7` `mov r14d,[rsi+0x2798]` | `rsi = lVar11` |
| store, register value | `0x140872D3E` `mov [rsi+0x2798],ebx` | conditional (`jne 0x140872D46`) |
| store, register value (second path) | `0x140872D8B` `mov [rsi+0x2798],eax` | |
| read of bit 0x2 next tick | `0x1408731B4` `test byte [rsi+0x2798],r12b` | decompile line 261: `if (old&1 && old&2)` |
| BallInfo init (zeroes it) | `FUN_140A571F0`, store at `0x140A572BB` | `+0x2798 = 0` in reset |
| K=10 detector read | `0x140A1783B` `mov ecx,[rax+0x2798]` | `FUN_140A17810` compares with stored copy `[rsi+8]` |

`lVar11 = FUN_1404335E0(p+0x4E0)` returns `*(owner+0x10)+0x2970`; live, its `+0x2798` equals the `*(G+0x40)+0x2798` value (same int, matched exactly), so `*(G+0x40)` is that object (INF on the pointer chain, CONFIRMED on the value).

## 3. The value

```
if p.+0x298 == 3:  int = 0                           # gate, CONFIRMED live, 1175/1175 frames, lag 0
else:              int = FUN_140A72E70(F, x', z', &s) # s = 1 (char)
x' = x - sgn(x)*K ;  z' = z - sgn(z)*K              # x = p+0x64, z = p+0x6C
sgn(v) = -1 if v < 0 else +1                          # operands below
```
The call is at `0x140872D20..D3E`: `movss xmm8,[0x142588218]` (-1.0, VA `0x140872D22`), `movss xmm7,[0x14256AE1C]` (+1.0, `0x140872D2B`), `movss xmm12,[0x142598958]` (K, `0x140872D33`).

**K = 0.1086859330534935** (`0x142598958`, bytes `bb96de3d`→ float `0x3DDE96BB`). Equals the ball's resting height (0.10868), so K is the **ball radius**: the point is pulled toward the centre by one radius, so a zone bit fires when the **whole ball** has crossed the line.

## 4. Field record F (pfVar12 = FUN_140871D30(p+0x530))

`FUN_140871D30(h)` returns `*(*(h+0x10)+0x10)+4`; here `h = p+0x530`, `*(h+0x10) = 0x0A3EFF80`, block `0x1D45F020`. `F[i] = block + 4 + 4i`.

| index | value | role (INF from value; geometry consistent) |
|---|---|---|
| F0 | 52.56 | goal line, half-length |
| F1 | 34.06 | touchline, half-width |
| F4 | 47.06 | six-yard box, x |
| F5 | 9.16 | six-yard box, half-width |
| F6 | 36.14 | penalty box, x (52.5-16.5 ≈ 36) |
| F7 | 20.12 | penalty box, half-width |
| F17 | 9.15 | centre circle radius (9.15 m) |

Constants read in `FUN_140A72E70`: `0x142565D30` = abs mask (`0x7FFFFFFF`, operands `0x140A72E8E`, `0x140A72EA3`); `12.0` (`0x14259BE04`, `comiss` at `0x140A72F0C`); `8.0` (`0x14259BDF4`, `movss` at `0x140A72F00`).

## 5. Bit map of `FUN_140A72E70(F, x', z', s)` (CONFIRMED by exact match)

Let `ax=|x'|`, `az=|z'|`.

| bit | condition | meaning |
|---|---|---|
| 0x1 | `ax ≤ F0` and `az ≤ F1` | inside the field rectangle |
| 0x2 | `ax > F0` or `az > F1` | **out of play** (not inside) |
| 0x4 | `az > F1` and `ax ≤ F0` | outside by touchline |
| 0x8 | `ax > F0` | outside by goal line (end line) |
| 0x10 / 0x20 | `x' ≤ 0` / `x' > 0` | x side |
| 0x40 / 0x80 | `z' > 0` / `z' < 0` | z side |
| 0x100 | `az ≥ F7` | wide band (beyond penalty-box width) |
| 0x2000 | `12 < ax < F6-8` and `az ≥ F7` | (with 0x100) |
| 0x1000 | `F6-8 < ax < F0` and `az ≥ F7` | goal-line band beside penalty box |
| 0x400 / 0x800 | `ax ≥ F6` and `az < F7` | penalty box end, 0x800 if `x' > 0` |
| 0x200 | `ax ≥ F4` and `az < F5` | six-yard box |
| 0x4000 | `ax < F17` and `az < F17` | centre circle |

Equality corner cases (`ax ≠ F6`, `ax ≠ F4`) are in the code; the test set uses them exactly as written.

**Judge link.** The judge (`FUN_14050E4A0` case 10, `docs/re/pes-re-A5Judge.md` §1.4) tests the raw `|ball.x| <= F0` and `|ball.z| > F1` (no K margin). The zone code flips on `|x| − K > F0` (ball fully out). Live, the code leads the judge state by one frame: at `fr 3162698` the ball is at `x = −52.72` (`> F0 + K = 52.67`), the code is `1690 = 0x69A` (`0x8|0x2` set, out by end line), and `116C` becomes 2 at `fr 3162699` (`x = −53.09`). Touchline example: `fr 3164924` `z = −34.17` (`> F1 + K`), code `406 = 0x196` (`0x4|0x2` set), `116C` = 2 at `fr 3164925`.

**K=10 trigger.** `FUN_140A17810` emits on **any** change of the 14-bit code, not only out-of-play bits. Interior changes (penalty-box, six-yard box, centre circle, band bits) also emit, gated by `MI+0x11B4 == 0`.

## 6. Live verification

- Set: 60 s + 45 s in play (`out1.json`, `gate.json`), ball `B` read each frame, `p+0x298` and `*(G+0x40)+0x2798` read each frame, `116C` read each frame.
- Nonzero observations: 4,447 / 4,447 exact match with the formula in §3–§5.
- Gate: `p+0x298 == 3` ⇔ int `== 0` in 1,175 / 1,175 frames (lag 0).
- Position-sign test on the same frames: `B` agrees with the sign bits (0x10/0x20, 0x40/0x80) in 98.1% of play frames; swapping x and z gives ~50%, so `x = p+0x64`, `z = p+0x6C`.

## 7. Not established

1. That `p = *(q+0x20)` in `FUN_140425A90` (the ball record is found by value, not by pointer chain). The identity of `p` as the BallInfo or the physics ball is INF.
2. Which of the two stores (`0x140872D3E`, `0x140872D8B`) is the zero (gate) store: the compiler merged the branches. The gate behaviour is CONFIRMED live; the branch-to-store mapping is INF.
3. The meaning of `p+0x298 == 3` (named "dead ball" in the notes is INF). It toggles with `116C` but not identically (e.g. `116C = 3` with a nonzero code in some frames).
4. The exact F indices for F2, F3, F8–F16 are not used by this writer.

## 8. Rule for FirstTouchPort2 (sent)

`int(G+0x40 → +0x2798) = 0 if p+0x298 == 3 else code(x', z')`, with `x' = x − sgn(x)·0.1086859`, `z' = z − sgn(z)·0.1086859`, `F0 = 52.56`, `F1 = 34.06`, `F17 = 9.15`, and the bit table §5. Out of play ⇔ bit 0x2 set (`0x8` end line, `0x4` touchline).
