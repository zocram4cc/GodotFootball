# PES port: KickBranch2: survey run (one untranslated callee: 0x140A907C0), request builder widened over it and over 0x14080EBA0 (172/172 SYNTHETIC modified calls bit-exact); the pes-loop smoke finishes; every kick still answers "none" (the cutover is inert); GATE result below (05-11-26)

Branch `KickMotion` (worktree `gf-KickMotion`), pushed (`git ls-remote origin KickMotion`: see section 7). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessors: `pes-port-KickBranch.md` (sections 5 and 8), `pes-port-KickRoot.md`, `pes-port-KickHeader.md`, `pes-port-KickKind.md`, `pes-port-KickChooser.md`.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. survey mode (scratch, reverted) | **DONE.** One pes-loop smoke in survey mode, then three more. The only untranslated target reached in a match is **`0x140A907C0`**, once, from the request builder. `git diff` shows no trace of the survey (patch kept as `gf-KickMotion-scratch/survey_debug.patch`). Section 1. |
| 2. revert | **DONE** (`git checkout` of the three files before the first commit). |
| 3. translate and prove each surveyed callee | **DONE for 0x140A907C0** (86 SYNTHETIC modified builder calls reach it) and for **0x14080EBA0 + 0x140481B00** (51 states, a static gap of the same builder): 172/172 bit-exact, set `kickleaf`, ONE regeneration. Section 2. Four static gaps remain loud aborts, none reached in any surveyed match (section 2.3). |
| 4. smoke without abort, `[kickmotion]` with clips, ablation | smoke **finishes**, `[determinism] a2f78ca1b76a66e4` = the anime-chain-alone reference (`b1d39978`), so **the full cutover is inert: 79 of 79 kicks answer none, no clip**. NOT MET for "kicks with clips"; the cause is found (section 3): PES answers none on all 15 real builder captures too. |
| 5. merge main, census, `GATE fast` | main (BE `c45fbd57`) merged clean; `gen_translated.py --check` and `ledger.py --check` current; gate: section 6. |

## 1. Survey (CONFIRMED)

Scratch-only patch to `PesWorld::RunKickRequest` / `RunKickChooser` (env `GF_KICK_SURVEY`): `pes_dispatch_missing_handler` throws, the call is caught, the target, its phase and the C++ call-site frames are recorded, a builder miss falls through to the chooser on the partial request, a chooser miss answers none. Results (seed 7, `--pes-loop`, capped serial run, Main's grant, `survey_a/b.log`):

| run | builder | chooser | untranslated targets |
|---|---|---|---|
| 5 min (ticket) | 55 ok, 0 miss | 55 ok, 0 miss | **none** (hash `e0d8b03934fd6807`) |
| 2 min | 78 ok, **1 miss** | 79 ok, 0 miss | **`0x140A907C0`** x1, phase builder, at the 5.0 s mark (hash `a2f78ca1b76a66e4`) |

(`--minutes` changes the match: the 5-minute match never reaches the call.) The only call site is `0x140834FFC` in `FUN_140833CD0` (shot, request +0 == 10: the defender-lane loop at `0x140834E50`, `FUN_140A907C0` = the segment-circle intersection `FUN_140A907C0(out1, out2, p, q, centre, radius)`).

Static gaps of the same two functions (the translated closure's `OUT-OF-SET CALL` markers, CONFIRMED by grep): builder `0x1406E4230` (`0x140833ECD`, request subtype 2), `0x14080EBA0` (`0x14083419F`, `0x1408341FD`, `0x14083486C`); chooser `0x14082DD20` (`0x140836B...`, when the motion interface answers 0x12) and `0x1408212D0` (end of the 0x97 fallback loop). Not reached by the 2-min or 5-min matches.

## 2. Proof (CONFIRMED, SYNTHETIC)

### 2.1 0x140A907C0
Tool `hdrsyn2.py` (scratch): PES's own bytes (`frame_trace.run_recorded`) on the shot capture `live2/pid802552/seq4358`, with at the builder's entry the ball (x, z, 40 track entries, speed) at rest, the kicker's P (+8, +0x10, +0x20, +0x28) and man position (the loop's first gate clears the lane flag +0x5E unless P's position equals the man's), the order's target, and the kicker's opposing nearest list (`MA+0x22E4C + 200*slot`, five pairs) drawn at random (seeds 11, 12, 13). 86 of 240 states enter `0x140A907C0`.

### 2.2 0x14080EBA0 (+ 0x140481B00)
Tool `hdrsyn3.py` (scratch): same on `seq4331` (receiver pass) with the order's action, kind, record bytes `+0x149/+0x14A/+0x14C` and receiver drawn too. 51 of 316 states enter `0x14080EBA0` (sites `0x14083419F` x6, `0x1408341FD` x45).

### 2.3 Record
`snapdiff --bounded --only 0x140833CD0` over the 172 cases (`sdhdr.sh`, 101 s): `calls=172 replayed=172 mismatches=0 own_block=0.7002 (418/597) proven=True`. New closure members: **`0x140481B00`, `0x14080EBA0`, `0x140A907C0`** (`closure_kick.json` 85 -> 88). New record `results_kickleaf.json` (SETS `kick`). One regeneration: `gen_translated.py` wrote 5,397 functions (3,750 recorded, 1,647 closure; +1 file `closure/pes_fn_14080EBA0.cpp`), `--check` byte-identical, `ledger.py --check` current. NOT reached by any of 556 draws: subtype 2 (`0x1406E4230`) and `0x14083486C`: both stay loud aborts; the chooser's `0x14082DD20` and `0x1408212D0` need chooser states (not attempted: no match reaches them). Coverage is recorded (418/597 builder blocks), not a gate.

## 3. Why every kick answers none (CONFIRMED; the INF in `pes-port-KickHeader.md` section 5 is wrong)

1. **GF's builder equals PES's.** The two tests `pes_kick_header_oracle_test` (were `DISABLED_`, red cause INF "the window's MA holds ball and carrier only") PASS on all 15 real kick_header captures once the 15 builder oracle dirs are built (`hdr_oracle.sh`: snapdiff + `trap_oracle.py`, in 4 capture batches): the builder's request equals PES's on every byte the reference wrote, and the chooser answers the same on the built request, on PES's own request and on `KickOrderFor`'s order. The INF was stale: `Build` copies the manifest's MA ranges since the KickKind work. Both tests are now enabled (they skip without `GF_PES_KICK_HEADER_ORACLE`).
2. **PES's own answer is none on all 15.** `want` (the proven chooser on PES's own request) is kind 0x97, band 12 on h030, h4331..h4772 (15/15; `[answers]` print of a scratch edit). `pes-port-KickKind.md` already recorded "16 of 19 chooser calls answer none in retry bursts (consecutive-tick re-picks of one request)". The Kick start `FUN_140724CC0` (0x140724DEF..) runs the builder and chooser every tick of the run-up; with kind none it finds no clip cell (`FUN_1406ABDD0` false, `*param_5 = ...`) and starts nothing, so the AI retries next tick. A real kind appears only at the strike tick. GF calls the pair once, at its own strike decision, so it samples PES's none-dominated distribution at the wrong moment (INF on the strike timing: the 15+24 captures all sit in the run-up).
3. **The MA arrival list is also missing (INF on its effect).** GF's window MA (`MA+0x23600..0x26040`, printed per kick) is zero over `0x24740..0x247C0` and has 8 non-zero bytes in `0x24700` where the captures hold 29-43 per 64-byte block (the ball-distance / arrival list `MA+0x2470C`: pairs at `+0x2470C + 8*(11*side+rank)`, counts `+0x247BC/D`, ranks `+0x247BE`); `pesdodge.cpp`, `elizacontroller.cpp`, `teamAIcontroller.cpp` already carry `ponytail: MA+0x2470c not carried`. No image instruction stores into `0x24700..0x247D0` by a direct displacement (capstone scan of `0x140001000..0x14252E800`), so the writer is not decoded. Zeroing it in the oracle changes nothing (PES answers none anyway), so this does NOT explain the smoke's none; it matters once kicks answer kinds.

So "kicks with clips" needs the Kick action's per-tick driver (the Kick object `0x1425E2358`, `FUN_140724CC0`'s caller chain `FUN_1406E5FC0`) and the AI order path, not more translated callees. That is the next piece of work.

## 4. Smoke and ablation (CONFIRMED)

Seed 7, 2 min, `--pes-loop`, `simslot.sh`, merged tree (main `c45fbd57`): `[determinism] a2f78ca1b76a66e4`, 46,254 ticks, `[kickmotion] 79 kicks, no clip 79, descriptor 1 0, in-flight Q 0`, no abort (`smoke_final.log`).
- Ablation: that hash equals the anime-chain-alone reference (`a2f78ca1b76a66e4` at `b1d39978`, wiring reverted), and the 2-min survey run (builder fall-through, chooser) gave the same hash: **the cutover adds no move; the whole move from main's `85c7d7c515cf92ae` is the KickRoot anime chain** (already reported in `pes-port-KickRoot.md`: bound into the window, run by no kick).
- Switch-off: see section 6 line.

## 5. Tests run

`pes_kick_header_oracle_test` 2/2 on the 15 captures (4 batches, `GF_PES_KICK_HEADER_ORACLE`), `pes_kick_chooser_oracle_test` and `pes_kick_root_test` built on the merged tree (not re-run: untouched). Mutations: none (no engine-code change; the diff is generated layer, closure/record JSON, one test file). `gen_translated.py --check`, `ledger.py --check`: current.

## 6. Gate

`GATE_TIER=fast tools/dev/gate.sh <workdrive>/gf-KickMotion/build` on the merged tree (main `c45fbd57` + `d65fb532`, 7,382 s wall, log `gate_final.log`): **`GATE fast: RED pes_restart_delay_test pes_throwin_release_test`** (2 of 2342 ctest red; the recorded re-check passes on 3,763 of 3,763 selected rows, "no unrecorded status regression vs HEAD (18760 rows compared)", ledger current, off-path ports all allow-listed).

- `PesRestartDelay.AThrowInIsReleasedFromItsSpotAndTaken` (399,751 ms) and `...AThrowInIsCarriedToItsSpotAndReleasedThere`: `pes_restart_delay_test.cpp:324` / `:361`, "throw-in released with the taker's root 1.3981834650039673 m from its spot", limit `throwInCarryStop_m` 0.65. The probes run seed `SEAMLESS_SEED = 3`, 20 minutes, path A. `pes_throwin_release_test` is the same family (`AThrowInIsReleasedFromInsideTheLine` passed in 468 s; the failing case there was not extracted). Both are seeded-match drift, not an abort and not a diff regression. INF: seed 3's match moved because the pes-loop hash moved (`85c7d7c515cf92ae` -> `a2f78ca1b76a66e4`, the KickRoot anime chain bound into the window; the kick wiring adds nothing, section 4), so its first throw-in is now released 1.40 m from its spot.
- **Not re-staged:** the census of replacement seeds (20-minute matches, serial through `simslot.sh`, then the `SEAMLESS_SEED` / `kThrowInSeeds` edits and a second gate) did not fit this run's request budget (spent on the survey, the 556-state proofs and the none diagnosis). Switch-off smoke on the merged tree: `[determinism] 399e180f39459c2b` = main's baseline.

## 7. Left (numbered)

1. **Re-stage the throw-in families, then `GATE fast`** (this branch is RED only on them): census `SEAMLESS_SEED` (now 3) in `pes_restart_delay_test.cpp` and the throw-in seed lists of `pes_throwin_release_test.cpp` over candidate seeds with the path-A probe (`tests/sim/pes_shared_match.hpp` structure), serial through `simslot.sh`; the property to hold is the taker's root within `throwInCarryStop_m` at the release.
2. **The Kick action's per-tick driver** (section 3): port `Kick` (vtable `0x1425E2358`, its tick `0x14071A250`, `FUN_140724CC0` and its callers) so GF re-picks every tick like PES and takes the first real kind; until then `[kickmotion]` stays all none and the cutover is inert. The order record's initialiser and the AI order path are still undecoded.
3. Decode the writer of the arrival list `MA+0x2470C..0x247D0` (section 3.3).
4. Static gaps with no state yet: builder `0x1406E4230` (subtype 2) and `0x14083486C`; chooser `0x14082DD20`, `0x1408212D0`. They abort loudly if a match ever reaches them.
5. Hook for Main only when item 2 needs real strike-tick calls: a consecutive-tick capture series of one kicker's `kick_chooser_8364D0` calls (the strike tick is the call whose descriptor kind is not 0x97; that is not expressible as an entry predicate, so sample EVERY 1 around a kick-header call).
6. Scratch `gf-KickMotion-scratch` (~46 GB: `syn/`, `oracle*`, `rs/ hs/ cs/ ys/ zs/`, scripts `hdrsyn2.py`, `hdrsyn3.py`, `hdr_oracle.sh`, `survey.sh`, `scan_ma.py`, `survey_debug.patch`) is kept until Main reads this; every capture is packed again.
