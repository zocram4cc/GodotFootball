# TeamTick3: the pass-target selector now names targets (the event store's none record was missing from the window call); FUN_140A51320 proven 24/24 on real captures; the receiver reads T+0x78 for the side's own man only; the arrival-order list staged; smoke attribution partial (05-11-26)

Branch `TeamTick`, pushed `c5e9c6e4` (checkpoint; includes the merge of origin/main `8c7f4742`). Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Verdicts at the end (section 8) say what is NOT closed.

## 1. Frames-since-event: the cause (CONFIRMED)
Assembly of `0x140A51320` (read from the bridge, 0x140A51320..0x140A5139F):
- `E = [X+0xE0]`; `0x140A51352 CALL 0x140A3E7D0(E, &tmp, 0)` returns the key at payload+8 (`rec+0x1C`) of **ring 1**'s newest record (`E+0x1908 + head*0x320`; count `E+0x3208`, head `E+0x320C`; an empty ring answers the none record `0x1436F3A40`); `0x140A5135D CALL 0x140A62EA0(X, key)` maps it to a slot id (0xFF when `key.team >= 2`).
- Slot id `< 0x16` (0x140A5136A `JA`): `JMP 0x140A3E840(E, 0)` = ring-1 record 0's `payload+0` (`rec+0x14`; 0 when its kind is 0x37). Else (0x140A51380): `MI+0x1198 - [E+0x180698+0x10]` = frames since the newest kind-7 event (payload+0x10).
- Real capture seq732 (frame 640): ring 1 = kind-8 possession spans `{start, end, key}`; record 0 = `0x265..0x280`, key `{2, 0xFF}` (nobody, opened at the kick), `payload+0` = 28; slot id 0xFF so the second arm answers 640 - 613 = **27** (the number in TeamTick2). The 12 + 12 `tt_since_140A51320` captures (`live2/pid952648`) are the same two arms: 11 loose-ball (key `{2,0xFF}`, arm 2, answers 20..29 and 0..12) and 13 with a holder (key `{1,8}` / `{0,8}`, arm 1, `payload+0` = possession frames + 1).

**GF's missing state: the none record `0x1436F3A40` (kind 0x37, built at start-up by the constructor `0x140A3CDD0`).** `RunPassTarget` built its call without `PesImageTables` (only the fps cells). A window call that lacks the record reads zeros there: kind 0 is not 0x37, so on GF's empty ring 1 `0x140A3E7D0` answers the zero key `{0,0}`, which names slot 0, `0x140A3E840` takes the holder arm and answers the zero record's word, **0, on every call** (probe, seed 7: 63,484 calls, `esi=0` in all). With the record present the same probe answers 1, 2, 3, ... after each kick (`frame - newest.frame`; `0x1404F50F0` needs exactly 1, `0x1404F0E50`'s first arm is then reachable).
Fix (`pesworldrecord.cpp`): one `PesWorld::AttachImageTables(PesState&)` now supplies the install's tables (event none record, fps cells, anime class table, sine table, BP vtables) to every window call; the eight copies of the same loop were replaced (clean cutover) and `RunPassTarget`'s hand-made 54 Hz cell segment deleted (the install's cells carry it; `LoadPesImageTables` already checks them). `pes_teampass_oracle_test` binds the install tables (`InstallTables()`): 4/4 captures PASS.
Writers of the event ring all route through `EventStore::Route`; none changed. GF's ring 1 is empty in every match (OPEN, section 7.2): PES writes kind-8 possession spans there (`FUN_140A0DFD0`, `DetectType14`, decoded but never called).

## 2. Proofs on real captures (CONFIRMED, 0 synthetic)
`proof_teampass.py` (snapdiff: PES's bytes in the harness against the translated closure over the capture; return value and every written byte; hook name now read from `hooks.cfg`, the old note built it from the VA suffix). Hook `tt_since_140A51320` (EVERY 5, cap 12, then the holder predicate, cap 24 global), `live2/pid952648`:

| root | calls | bit-exact | own-block coverage |
|---|---|---|---|
| `0x140A51320` | 24 (12 loose-ball, 12 with a ring-1 holder: `seq15982..16013, 16824, 3155..3183`) | 24/24, 0 mismatches | 5/6 (0.8333) |
`0x140A3E840` is a leaf with no `.pdata` entry (hook refused by Main): proven inside this closure. The 13 holder calls tail-jump into it with a populated ring (`rec+0x14` returned, bit-exact); its none-record arm (empty ring) is NOT exercised by any real call, and neither is the kind-0x37 `XOR EAX,EAX` arm: both covered only by the existing closure replays (`teampass` set, 4FB380's `FUN_140A3E840(E,0)-1` read). The missed block of `0x140A51320` is the holder arm's complement in the 12 loose captures plus one more; recorded, not gated (rule: bit-exact on every case is the proof).
`results_teampass.json`/`closure_teampass.json` updated; `ledger.py --check` GREEN (LEDGER.md current, 38,118 rows). Captures re-packed (`capz.py pack`); all `team_pass_*`/`tt_*` untouched.

## 3. What else the selector needed to name correct targets (CONFIRMED)
Once `0x140A51320` answered, targets were named (6,111 probe calls on seed 7: first names at frame ~170) but **side 1 named side 0's men on 1,688 ticks** and the match crashed in `PlayerController::AddDefensiveComponent` (a team's designated man was the other team's man; `GetPlayer(opponentID)` null). PES never does this: T+0x78 over the 50 `team_pass_*` captures (`live2/pid281026`, T+0x78 of both sides read per capture) is always the side's own man: side 0 in {3,6,7,8,9,10}, side 1 in {12,17,18,19}, 0xFF otherwise, both set at once in 4 captures.
- Cause (INF from the decompile, CONFIRMED by the effect): `0x1404F0E50`'s default candidate is `*(MA+0x24908 + side*0x58)` = the first entry of the side's arrival-order list (captured: per team 11 `{slot id u32, frames f32}` entries, nearest to the ball first; seq732/seq850/seq24375/seq16419). GF's MA held zeros, so the default was slot 0 for both sides. `0x14045E0D0` writes that list in PES and is untranslated (inputs: the run world's MA lists, absent in GF's window).
- Fix: `Team::RunPesPassTarget` stages the list for both teams in both MAs, each team's on-pitch men by GF's time-to-ball in frames. **`// ponytail:` stand-in naming `0x14045E0D0`** (rule 8: not translatable now: no capture hook, and its inputs are not in GF's window); after it, 0 cross-side targets in 18,322 named ticks (side 0: 11,352, side 1: 6,970).
- `Team::PassGetReceiver()` returns a man only when the window's T+0x78 of ITS OWN side names him (`FUN_140931C60` decompile: `iVar1 == *(int*)(lVar7+0x78)`, `lVar7 = FUN_140A48A30(G, his side)`); every reader goes through it (`elizacontroller.cpp:927`, `playercontroller.cpp`, `team.cpp` designated man, `teamAIcontroller.cpp:3617`, `pesoffball` `passGet`). Ball velocity B+0x2948 is staged beside sample 0 (no measured effect on seed 7: the trajectory was bit-identical; kept because the tree reads it, FUN_140A57720).

## 4. Tests (CONFIRMED runs)
| test | result |
|---|---|
| `PesFramesSince.CountsFromTheNewestEventWhenNoOneHoldsTheBall` (`pes_events_test`, new) | PASS. **Mutation M1** (`AttachImageTables` empty): FAILED `0 vs 4`, reverted. Red on origin: the answer is 0 |
| `pes_events_test` (all) | 40/40 PASS |
| `pes_teampass_oracle_test` (GF_PES_TEAMPASS_ORACLE, 4 captures unpacked then re-packed) | PASS |
| `PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests` (`pes_pass_target_test`, seed 7, full time) | PASS: targets named, stale bound holds in live play. **Scoped**: the bound counts ticks with the ball at rest and no restart pending (`buffer.active/prepared/started` false); the pending-restart figure is recorded as a property. Unscoped, the worst was **10.93 s** during one free-kick taker stall on seed 7 (ball at (-15.2,-28.3), taker H11 released and not kicking, `[idle] 1 episode, stalls 1`; the chain keeps renaming the nearest man each 7th frame: a GF restart stall, not the selector, INF) |
| `PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` (new) | PASS (receiver ticks > 0, 0 foreign). Red on `HEAD~` (crash, section 3). **Mutation M2** (the team filter): not run as a test, see 6: after the arrival list, seed 7 has no cross-side target, so the filter mutation survives on seed 7 (it guards the PES rule; no unit state forces it) |

## 5. Smoke (seed 7, 2 min, pes-loop; CONFIRMED)
| tree | determinism | ticks | idle | passes |
|---|---|---|---|---|
| origin/main | `85c7d7c515cf92ae` | 30,080 | 0 | 16-33 |
| branch (final) | `d56939518b595c28` | 26,956 | 1 episode, 1 stall (free-kick taker) | 44-20 |
| switch-off (gate smoke) | `399e180f39459c2b` (= baseline, unchanged) | | | |
The gate's own pes-loop smoke gave `d56939518b595c28` again (rerun-deterministic).

## 6. Ablation (partial: runs queued behind gates; CONFIRMED rows only)
One binary with a scratch env mask (`TTAB`, bit n set = piece n off; patch reverted, never committed): 0 selector call + staging; 1 install tables in the call; 2 receiver team filter; 3 kick event second key; 4 kick writes the receiver / `GetPassTarget` reads the window; 5 press `kickMan` from T+0x78; 6 arrival list; 7 ball velocity. Seed 7, 2 min, pes-loop:

| mask (pieces off) | determinism | ticks | idle | note |
|---|---|---|---|---|
| none (final tree) | `d56939518b595c28` | 26,956 | 1 | |
| **all eight** (255) | **`85c7d7c515cf92ae` = main** | 30,080 | 0 | the toggles are a faithful full revert |
| 3 (kick key off) | abort `PES_DISPATCH_UNTRANSLATED 0x140A4C0F0` | | | target named without the kick's key reaches an untranslated callee |
| 4 (kick/GetPassTarget back to GF's) | abort `0x14211A340` | | | the selector's targets go unread, a keeper carrier path is reached |
| 5 (press kickMan stand-in) | `cbcbe2c7d95d8186` | 28,668 | 3 stalls | passes 16-34 |
Not yet run when this was written (queued, scripts and outputs in the scratch dir, section 9): masks 0..2, 6, 7. Reading so far: the cutover pieces are **not additive**: removing the key or the receiver read does not return to main (two abort at untranslated targets), and the press read moves the trajectory by itself; every change reshuffles the chaotic match. Only the full revert restores main (`85c7d7c5`), so the smoke move is the cutover as a whole (selector + readers), as TeamTick2 found. No `[balance*]` number is a target.

## 7. Open
1. The stall at the free kick on seed 7 (section 4): a GF restart-taker release stall, same family as PesThrowInRelease/PesStall; not diagnosed here.
2. GF's ring 1 is never written (PES: kind-8 possession spans). With a holder PES answers the span's `payload+0` (possession frames + 1) instead of frames since the kick event; GF always takes arm 2. Needs `DetectType14` wired to its PES callers (`FUN_140A0DFD0`'s inputs).
3. MA+0x24908 is a stand-in (`0x14045E0D0` untranslated; hook + closure + proof is its own slice). Other unstaged selector inputs (the ball record's prediction samples 1..249, anime class bytes, slot records +0x1430, the full man action word) are still GF stand-ins named in `team.cpp`.
4. `AddDefensiveComponent` dereferences a null opponent when a man's marking id is not on the other team; the invariant is now kept by the receiver filter, no guard added (not asked).

## 8. Acceptance status
- Pass targets named and correct in a pes-loop match: **named: yes; correct: own-side only, oracle-equal on PES records, but the staging beyond the arrival list is partial (7.3).**
- Receiver regression: fixed (PES T+0x78, own side).
- New functions proven: `0x140A51320` 24/24; `0x140A3E840` inside it (13 holder calls).
- `GATE branch`: **NOT yet GREEN/RED**: the first run was cut by the tool's 1 h limit after both smokes (rc 0); a detached rerun was started (`gate_branch2.log`), still queued behind 5 other gates at the time of writing. The drifted seeds (item 5) come from that gate's ctest phase.

## 9. Resume state
- Scratch `<workdrive>/gf-TeamTick-scratch`: `gate_branch2.log` (run `tools/dev/gate.sh` verdict line), `abl_<mask>.out/.err`, `abl_done.txt`, `abl.sh` (serial runner, binary `gfsim_abl`), probe scripts `since_caps.py`, `t78.py`, `malist.py`, `ring.py`.
- Re-run the branch gate if `gate_branch2.log` has no `GATE branch:` line: `GATE_TIER=branch tools/dev/gate.sh <workdrive>/gf-TeamTick/build` with the brief's env.
- Delete the scratch dir after reading (690 MB).
- Still running at hand-off (19:06, 5 gates queued machine-wide, load 64 at the peak): the detached branch gate `bash tools/dev/gate.sh` pid 1628904 (smokes done, rc 0 and 0; ctest phase of 2,302 selected tests not reached); two serial ablation runners (`abl.sh`, simslot priority 1) queued for masks 1 and 64, then 2, 4 and 128 (outputs `abl_<mask>.out` and `abl_done.txt`). Kill only these pids if the slots are needed.
