# PES port: KickHeader: the kick request builder FUN_140833CD0 translated and proven 15/15; the chooser then leaves its bounded closure, so the PES-loop stays on the zero-geometry request (04-11-26)

Branch `KickMotion` (pushed). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. gate2.log final line | `gate2.log` is **empty (0 bytes)**: that run never wrote a line. `gate3.log` (pid 790790, started 05:47, still running in the worktree when I started and reading my edits) ends at `results_impure.json 34 of 34 recorded selected`: no verdict. I ran my own gate (section 6) in a scratch build dir. |
| 2. translate + prove `0x140833CD0`, `results_kick.json`, regenerate, `--check` | **DONE.** `snapdiff --bounded` on the 15 `kick_header_833CD0` captures (`live2/pid802552`: seq030, 4331, 4335, 4337, 4340, 4342, 4358, 4361, 4363, 4365, 4367, 4368, 4766, 4770, 4772): `calls=15 replayed=15 mismatches=0 own_block=0.6298 (376/597) proven=True`, bounded closure 80 functions. `results_kick.json`, `closure_kick.json`, `SETS` entry `("results_kick.json", "kick")`, 16 new universe rows, layer regenerated (5,110 functions, 3,732 recorded, 1,378 closure; no existing layer file changed). `--check`: see section 6. |
| 3. order-record fields, writers, GF-fed builder + chooser | **Fields decoded (CONFIRMED, section 3). Writers: OPEN (section 4). GF-fed builder + chooser: NOT DONE** (section 5: the chooser, not the builder, is the blocker). |
| 4. prove `0x1407324B0`, `0x140837FC0`; delete the default engine's kick stand-ins | NOT DONE (budget). `0x1407324B0` is the kick scatter (it reads P+0x2BC8 and scales the ball velocity; it is not an order writer). `0x140837FC0` is called by the builder and is in its proven 80-member closure. |
| 5. smokes, ablation, `GATE fast` | PES-loop behaviour is unchanged from the previous commit (section 5), so no hash move is expected from the kick path; smokes are Main's (verification split). Gate: section 6. |

## 1. The builder proof (CONFIRMED)

- Root spec `tools/dev/diffharness/specs/kick/140833CD0.json` (`closure.py --members-file` with the root only), traces `frame_trace.py recorded`, `snapdiff.py --bounded --only 0x140833CD0` over the 15, `closure.py --no-specs --members-file` (80 members), `gen_translated.py` (10.5 min).
- Arguments: rcx = request out (0x118 bytes), rdx = P, r8 = cmd, r9 = byte `*(param_4+0x70)` (0 in all 15), stack: +0x28 byte `*(param_4+0x75)` (0), +0x30 s32 Kick+0x40 (a frame stamp, 13425..14556), +0x38 f32 Kick+0x18 (the previous start's order +8: equals the order's +8 in all 15), +0x40 pointer to Kick+0x28.
- Callers: `0x140724DEF` (Kick start `FUN_140724CC0`, 13 captures) and `0x140725674` (`FUN_1407253D0`, the re-pick, 4 captures).
- The captures are 15 different orders: actions 9 (3), 8 (8), 10 (2); cmd[7] = 22, 19, 21, 25.

## 2. The order the builder reads (CONFIRMED: `readset.py` over the 15, plus the decompile)

cmd (= `param_4+0xB8`): `+0` action id (8 lofted, 9 ground, 10 shot), `+8` pointer to the order record, `+0x1C` the order kind (19 = 0x13 lofted to a receiver, 21 = 0x15 lofted without, 22 = 0x16 ground, 25 = 0x19 shot; `FUN_1407204E0` tests 0x15 and 0x18).
The record (0x160 bytes, a **stack copy** in the caller's frame, 0x98..0x2A8 above cmd): read words `+0x0` (only when the target height `+0x30 < 0`), `+0x8`, `+0x14` (receiver slot, >= 0x16 none = 0xFF), `+0x1C`, `+0x28`, `+0x2C/+0x30/+0x34` (target x, height, z), `+0x5C/+0x5D`, `+0xB4` and `+0xCC` (only for cmd[7] == 0x13), and `+0x149/+0x14A/+0x14C` through `FUN_1407204E0`. Default-initialised words in all 15: `+0x10 = 4`, `+0x20 = 0x20000`, `+0x44 = 0x300`, `+0x50 = 5`, `+0x54 = 2`, `+0x58 = 0x97`, `+0x64 = 9`, `+0x84 = 1`, `+0x148 = 0x200` (so `+0x149 = 2`). `src/sim/onthepitch/peskickchooser.hpp` names these (`PesKickOrder`, `kOrder*Offset`).

## 3. What else the builder reads (CONFIRMED by the oracle manifests)

Window records (P, ball, MI, man, slot, anime, side, pitch), image globals, and heap ranges: the AI side object's MA record (`MA+0x22E4C` lists, `MA+0x247D4`, `+0x24710`, `+0x247BE`) and **G+0x180, a roster table** of 24 entries x 0x74 bytes (actor id u32 at `+0x20`, flag byte `+0xD`, a record at `+0x68`). The window had no G+0x180: it now holds one (`PesWorldTables::roster`/`rosterTable`, G+0x400, ids = slot order, rest zero), named `ponytail:` at its constructor site (writer OPEN: match setup). The chooser oracle still passes 24/24 with it (2/2 tests).

## 4. Writers (OPEN)

The record is built in the AI order path above `FUN_140724CC0`: stack frames in the captures show `FUN_140724CC0` called from `FUN_1406E5FC0`'s action-manager chain (return addresses `0x1406E6A29`, `0x140726A30`, `0x14071AB30`, `0x1406E60CA`, `0x1409F44C9`, `0x1409F2060`, `0x1409F23E7`, `0x14092951B`); `0x14071A250` is Kick's own tick (not a writer), `0x1407266B0` a gate. The carrier answer -> command converter is `FUN_14210E790` (answer type 1 shot -> `FUN_14210F090`, 2/7 pass -> `FUN_14210EE00`, ...), which writes the BP command, not this record. I did not find the stores that fill `+0x2C..+0x34`, `+0x14`, `+0x14C`, `cmd[7]`. To finish: trace one capture's record bytes back with a write watch (the record lives at `rsp+0x310`..), or hook the ancestor frame.

## 5. Why the PES-loop stays on the zero-geometry request (CONFIRMED)

The builder runs on GF's window (`PesWorld::RunKickRequest`, `PesKickOrder`, `KickOrderFor`) but its request differs from PES's in the words computed from the side AI's MA record (INF: the window's MA holds ball and carrier only; the capture's MA bytes sit at the capture's heap VA), and, more important, **with PES's own request the chooser leaves its bounded closure on all 15 captured kicks**: `PES_DISPATCH_UNTRANSLATED` at `0x140830B50` (seq030, 4766), `0x1408387F0` (4331..4340), `0x1406E7A60` (4342), `0x1408371A0` (the shot list: 4358, 4361), `0x1408223F0` (4363..4368), `0x1406EA340` (4770, 4772). The 24 kick_chooser captures held none/42/100 only (zero-geometry answers returned before these paths). A production call would abort the match, so `humanoid.cpp` keeps the previous request (`KickRequestFor`, now a named `ponytail:` stand-in with this reason) and answers none, as in KickCutover. **The "kicks with clips again" smoke line cannot return until the chooser's closure is widened over fresh kick_chooser captures of shots, lofted and ground passes** (Main will arm `kick_chooser_8364D0` when disk allows; predicate `d(r8) in (8,9,10)`). Main suggested translating the six callees statically now: not done.

Tests: `tests/pes_kick_header_oracle_test.cpp` (builder request bytes vs PES's, and GF-fed order + chooser vs PES's) is committed **DISABLED_** with these two blockers in its comments; `GF_PES_KICK_HEADER_ORACLE=<15 oracle dirs from trap_oracle.py>`. `tests/pes_kick_oracle_common.hpp` is the shared window builder (the chooser test now uses it). Chooser oracle `pes_kick_chooser_oracle_test` 2/2 on the 24 (`GF_PES_KICK_ORACLE`; five captures had to be unpacked again: pid615629 seq15863, 15864, 16063-16065 need `capz.py pack`).

## 6. Gate

`GATE fast: RED` on `KickMotion` `59882c03`, NOT landable.

- **Ledger:** failed, then fixed by regenerating `LEDGER.md`; `ledger.py --check` passes alone. The gate was not re-run after the fix.
- **ctest:** `Loops/GoalSceneHold.TheKickOffWaitsForTheSceneAndThenKeepsItsSpacing/PesLoop` failed ("no goal in the match", 667 s). Inferred cause: KickCutover's all-none pes-loop kicks. Needs a seed re-stage by census.
- **Smokes:** switch-off `399e180f39459c2b` (= main); pes-loop `91ddee1c73385048` (main `38549be3476430fd`). The pes-loop move is the all-none chooser answers, the same hash as KickCutover; no ablation was run.

## 7. Left

1. Fresh kick_chooser captures (shots 10, lofted 8, ground 9); widen `closure_kickkind.json` (6 callees named) and re-prove.
2. Map the MA/ctx data into the window for the builder (then re-enable the two oracle tests); then wire `KickOrderFor` + `RunKickRequest` before `RunKickChooser` in `humanoid.cpp` and delete `KickRequestFor`.
3. Find the order record's writers (section 4); until then `KickOrderFor` is the stand-in.
4. Prove `0x1407324B0`; delete the default engine's kick stand-ins.
5. Pack the 15 kick_header captures and the five chooser captures; delete the scratch dirs `<workdrive>/gf-KickHeader-scratch` (build dir, oracle dirs) and `gf-KickCutover-scratch`.
