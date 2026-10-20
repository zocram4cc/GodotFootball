# How MOVESTEP gets its movement decision from the AI (S1Watch, round 26)

> **Main audit 19-10-26 (rev 2):** the target fallback threshold `0x142598578` = **0.2 m** (script read; the 0.4 stated below is wrong).
> **Main audit 19-10-26:** the think gate is `cmpb $0x16,[r13+0x11b0]` at `0x140929A51` (not 0x140929A4B). MOVESTEP steers by the command record move target cmd+0x20 (FUN_1406B7560); the ball/pass request is a separate channel (AI+0x874..0x8C4). Open: the code-to-cmd bridge and the caller's phase check.

Status: **static decode of the reader and filler CONFIRMED; the think's full case table is partly decoded.** Tags: CONFIRMED = decompile or instruction read this round; INF = inference; OPEN = not resolved.

## 1. Two channels, one record (CONFIRMED)

The AI writes two separate things per player, both in the AI record `A` (the `param_3` of the think and of `FUN_1409257B0`):
- **Decision code** `A+0x8D4` (u32): written by `FUN_140929A00` (the think) with codes 0x13, 0x14, 0x19, 0x1B, 0x1C, 0x1D (the writes at lines 93, 151, 156, 160, 189, 237–246 of its decompile). The think runs only when `MI+0x11B0 == 0x16` (CONFIRMED, `0x140929A4B`: `cmp byte [MI+0x11B0], 0x16`).
- **Ball/pass request** `A+0x874..0x8C4`: written by `FUN_1409257B0`. It sets `A+0x874` (`(cVar1 != 0) + 2` or `3 − (iVar2 != 0)`), `A+0x878` (the anime function id from the table `DAT_1434A2480[iVar2]`), `A+0x894` (`0x42C80000` = 100.0), `A+0x89C`, and a sub-record at `A+0x8C4` (via `FUN_140A85A70(A+0x8C4, class)`). These carry the **pending ball action** (the TRAP/PASS/SHOT class).

So the ball request and the movement intent are not in the same word. The ball request lives at `+0x874..0x8C4`; the decision code at `+0x8D4` names the movement case.

## 2. What MOVESTEP reads: the command record `cmd` (CONFIRMED, `FUN_1406B7560`)

`FUN_1406B7560(P, req, cmd)` fills the MOVESTEP request `req` from the **command record** `cmd` (`param_3`), not from `A+0x8D4` directly:

| `req` word | source in `cmd` | VA (filler) |
|---|---|---|
| `req[0]` | `cmd[0x16]` (8 → 2) | `local_dc` (`0x1406B7567..`) |
| `req[0xE]` | `cmd[0x15]` (2 → 1) | `local_e0` |
| `req[0x15]` | `cmd[0x17]` (speed class) | `fVar16` |
| `req[0x172]`-derived | `cmd[0x172]` (desired speed, km/h) | `local_118` |
| target point | `cmd+0x20` (`*(cmd+8)`, `*(cmd+0x10)`, `*(cmd+0x14)` as floats; `local_110` = `cmd+0x20` pair) | `FUN_140A8F860(&local_138, &local_110)` → `param_2[0x11]` (the distance) |
| look-at | `cmd+0x1A` block via `FUN_140414640(param_2+0x51, cmd+0x1A)` | `req+0x51` |
| heading out | `req[1] = atan2(target − root)` (`FUN_140A92D40`, ×57.2958, wrap ±360) | `fVar19` |
| facing | `req[2] = P+0x20` (`param_1+0x20`) | `param_2[2] = uVar23` |
| desired body | `req[3]` from the look-at heading `fVar20` | `param_2[3]` |

So MOVESTEP steers by the **move target `cmd+0x20`** (heading `req[1]`, distance `req[0x11]`), with the look-at `cmd+0x1A` for the body (`req[3]`). The move target is the same point the kick/approach planner uses (`pes-re-kick-approach.md` §1: receiver `cmd+0x20`). The filler also has a fallback: when the ball is the target (`fVar19 < 0.4`, `DAT_142598578`) it substitutes the BallInfo sample 0 (`FUN_140A6CF70(B, …)`, B = `*(G+0x40)`).

**Action gate inside the filler.** It reads the current anime action id `*pcVar13` (via the anime sec vtable slot `+0x20`, i.e. `anime+0x245C`'s byte or the action) and branches on a set of ids (0x04, 0x05, 0x06, 0x07, 0x08, 0x0F, 0x10, 0x12, 0x13, 0x0C...). Those branches choose the stance/clip family; they do not read `A+0x8D4`.

## 3. When the AI wants a ShortPass and also has a movement target (answer, INF-marked)

- The **pass** is a ball request in `A+0x874..0x8C4`, consumed by the **TRAP/kick dispatcher** (`FUN_14072C5C0` → `FUN_14072F3B0` for phase 0, and the kick class `FUN_1407302B0`), not by MOVESTEP.
- The **movement** is `cmd+0x20` plus the move kind `cmd+0x16`, consumed by `FUN_1406B7560` → MOVESTEP (`FUN_1406BA900`).
- So MOVESTEP steers by the movement target while the pass is pending: the two run in the same tick, and the pass runs its own clip. **INF:** the pass takes precedence on the body (the kick dispatcher sets the anime phase, and MOVESTEP only runs in the locomotion phase 0). The precedence is decided by the anime phase byte `anime+0x245C` (§4 of `pes-re-getup-recovery.md`): a kick/pass sets phase ≠ 0 and the MOVESTEP filler is not called for that tick (OBS; the caller check is open).

## 4. Open

1. The full think case table: which code (0x13/0x14/0x19/0x1B/0x1C/0x1D) maps to which `cmd` kind, and how `cmd+0x16` is set from `A+0x8D4` (the bridge from the think to the filler; the writer of `cmd` is not decoded here).
2. The precedence rule between a pending pass (`A+0x874`) and a movement target in the same tick (§3 INF).
3. The caller of `FUN_1406B7560` (the MOVESTEP entry `FUN_1406B8340`, per `pes-re-anim-select.md` §1) and whether it checks the anime phase before filling.

## 5. Summary for ClipWords

- Decision code: `A+0x8D4` (think, gated by `MI+0x11B0 == 0x16`).
- Ball/pass request: `A+0x874..0x8C4` (`FUN_1409257B0`).
- MOVESTEP inputs: the command record `cmd`: move kind `cmd+0x16`, speed `cmd+0x17`/`cmd+0x172`, move target `cmd+0x20`, look-at `cmd+0x1A`.
- Steering: heading from the move target to `req[1]`, body from the look-at to `req[3]`, distance `req[0x11]`.
- Pass vs movement: separate words; the pass runs through the kick dispatcher; MOVESTEP is not called in a kick phase (INF).
