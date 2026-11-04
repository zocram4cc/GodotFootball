# PES port: KickWindow: the 6 of 25 chooser window cases that differed are closed on every byte (22/22 + 2/2); the wiring, the pes-loop hash ablation, the real-call check and the gate are NOT done (04-11-26)

Branch `KickMotion` (pushed). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessor: `pes-port-KickChooser.md` (six callees proven 64/64; 6 of 25 window cases differed), `pes-port-KickHeader.md`.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. close the 6 differing cases | **DONE** (section 2). `pes_kick_chooser_oracle_test` on `oracle2` (4 real pid615629 captures + 18 synthetic): 2/2 tests, every descriptor byte equal. On the two pid1513193 real orders (seq1151, seq1170; `oracle4`): `EveryCapturedCall...` OK. The 25th case of the earlier count is not in my dirs (the other five pid1513193 captures are slim and have no built oracle dir; see 5). |
| 2. wire `KickOrderFor` + `RunKickRequest` before `RunKickChooser`, delete `KickRequestFor` | **NOT DONE** (budget at 70%). Not safe to wire yet either: section 3. |
| 3. new real chooser calls in `live2/pid1513193/` reaching `0x1408223F0` / `0x1406EA340` | **NOT DONE** (not looked at). |
| 4. merge `origin/main`, census, `GATE fast` | **NOT DONE.** `GATE fast`: no verdict, not run. |

## 1. How the differences were found (CONFIRMED)

The oracle dirs are built from the reference run's live-in set (`manifest.tsv`: `T` window records, `N` named heap objects, `H` heap ranges, `I` globals, `S` caller frame). `Build` of `tests/pes_kick_oracle_common.hpp` dropped two classes of line, so GF's run read zeros where PES had bytes:

1. **`N` lines were ignored** (`An`, `MA`, `E`, `SB0`, `GX`, `GY`). A debugging tracer (page protection + single step over the window, `pes_kick_chooser_oracle_test.with_tracer.cpp` kept in the scratch dir, not committed) logged every window access of GF's run. Reads not in the manifest: only `ctx+0x18/+0xE0` (An), `ti[0]+0xA18`, `ti[0]+0xB00`, `P chain A+0xF0`, `B+0xC8`.
2. **Heap `H` ranges inside the squad record** (the table-B record `SB0`: `+0xA18` = the player-info key u64 at entry+0x30 (`0xFFFD` team key, id), `+0xB00` = entry+0x118 (id)) were served at the capture's heap VA while table B names the window's team-info record, so `FUN_140A3B2A0` / `FUN_140A62D90` read zeros.

Neither is P+0x2816: **P+0x2816 is not read by any of the 6 reference runs** (not in any `T P` line of h030 / 15864 / h4766); GF's window leaves it 0 and no failing case reads it. It matters only for the synthetic `p` cases (they poke it to 0 and pass).

## 2. The fix and the evidence (CONFIRMED)

`Build` now:
- puts `N MA` / `N E` into the window's MA / event store of the kicker's side, and `N SB` plus every `H` range inside table B's record into `PesWorld::TeamInfoRecord(side)`; P+0x3AC0 is set to the window's ctx (`CtxVa`), as `WriteActorP` does;
- binds the anime player's objects `GX` (`P+0xA08`) and `GY` (`GX+0xF0`) as segments at their own VAs, named `ponytail:` at `CopyNamed` (open input; PES site that removes it: the anime player construction/update `FUN_141AF61C0` / `FUN_141AF7A90`; the Trap tick's window chain holds G and the O1/O2 children only).

Bisect (GF_KICK_PESVA toggles, since removed): every window group put back at PES VAs passes 24/24; only the physique chain at PES VAs flips 15864 and h4766 (a window chain gives them a different descriptor). Cause: `FUN_140829E80` at `0x14082AFF3` loads `*(P+0xA08)+0xF0` and calls `FUN_141AFDF10(B, 0, &out, t, motion)` for the **root-motion displacement of the kick motion** (`*(B+0xC8)` = GX, `GX+0x180` the layer, `GX+0x88`, `FUN_141B034B0`/`FUN_141B03660` the clip database); its vector feeds the kick angle, so the answer depends on the anime player.

Runs: `build/tests/pes_kick_chooser_oracle_test` with `GF_PES_KICK_ORACLE=oracle2`: PASSED 2 tests (descriptor equal on every byte, 22 cases: h030, h4331(p), h4335, h4337, h4340, h4342, h4358, h4361, h4363..4368, h4766, h4770(p), h4772(p); real 15863, 15864, 16063, 16064). `oracle4` (pid1513193 seq1151, seq1170): `EveryCapturedCall...` OK; `AGfBuiltRequest...` FAILS there (it checks `KickRequestFor`'s zero-geometry request against PES's builder request and dies with the function in the cutover; the two orders are lofted with a receiver). `pes_kick_header_oracle_test` builds.

## 3. What stops the wiring (INF from section 2, OPEN to prove)

The oracle serves the anime player chain from the capture. In a match, `WriteActorP` binds a window chain (A=PhysiqueA, B=PhysiqueB with G) with no layer/clip links, so `FUN_141AFDF10` runs on zeros: in the oracle, that exact substitution changes the descriptor (kind equal, band/params differ in 15864 +0x20; h4766 +0x10..). A cutover now would put chooser answers in the pes-loop that PES would not give. Needed: either the anime player's layer and clip objects for the kick (`GX+0x180`, `GX+0x88`, `GY+0xC8` and the clip database behind `FUN_141B03660`, built from the install as `PesTrapLoaderData` is), or a named stand-in for the root-motion vector at `0x14082AFF3` with its PES writer cited. Nothing to wire before that: `RunKickChooser` on the window chain was not run in a match.

## 4. Files

- `tests/pes_kick_oracle_common.hpp`: `CopyNamed`, team-info mapping, GX/GY segments, ctx pointer (the only source change).
- `tests/pes_kick_chooser_oracle_test.cpp`: unchanged.
- Scratch `gf-KickMotion-scratch` (syn/ ~44 GB, `oracle2`, `oracle3`, `oracle4`, tracer copy, analysis scripts `an.py ctx.py fnset.py`) kept for the follow-up. All captures I unpacked are packed again (pid615629 seq15863, 15864, 16063, 16064; pid1513193 seq1151, 1170).

## 5. Left

1. Provide the kick's anime chain (layer, clip DB, root-motion read) or a cited stand-in; re-run the 24; only then wire `KickOrderFor` + `RunKickRequest`, delete `KickRequestFor` and the zero-geometry request (and the `AGfBuiltRequest` test), run the pes-loop smoke and explain its hash move by an ablation.
2. Real calls for `0x1408223F0` / `0x1406EA340` (watcher rvwatch61 predicate `d(r8) in (8,10) or b(rdx+0x2816) < 0xFF`; the pid1513193 folder was not inspected). The five slim pid1513193 captures (816, 881, 972, 1041, 1117) have no built oracle dir.
3. Writer of P+0x2816 (still OPEN; not read by any failing case).
4. Merge `origin/main`, census re-stage, `GATE fast` after Main says BE is on main.
5. Delete the scratch dir once this is read.
