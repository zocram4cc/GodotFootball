# pes-port-TrapOpen2: the trap chain runs on 24 captures with only per-call world records captured; G ported and wired (29-10-26)

Branch `StoppageFlow` (worktree `<workdrive>/gf-StoppageFlow`, `origin/main` merged first; `git ls-remote origin StoppageFlow` is the head after the final push). Tags: **CONFIRMED** = a tool run this session (test, decompile, capture bytes); **INF** = inference; **OPEN** = not settled (blocker named). Image `pes_full.bin`, base `0x140000000`.

## 0. Verdict against the acceptance

| Item | State |
|---|---|
| 1. worktree, merge `origin/main`, push | DONE |
| 2. oracle dirs for the 18 new kind-0 captures; test widened | **DONE: 24 oracle dirs (18 new + the old six), `pes_trap_oracle_test` 7/7 on all 24**. Every capture is slim; the oracle needed PM (the `match::Player`): not in any slim capture (see 2) |
| 3. G port; retire `PhysiqueG() = 1.0`; P+0x3AC0 analyser fields; event record carrier; P+0x2FC ring | G: DONE and wired (section 3). P+0x3AC0: DONE (ctx record in the window + MA cells written by `TeamAIController`). Event record: carrier found = GF's existing `EventStore` newest copy (section 5). Ring: formula decoded from data, writer OPEN (section 6) |
| 4. S3: chain from P of GF state AND loader data from `PesTrapData`; image once | DONE in the oracle: **no `H`/`I` line is replayed; the loader segment is `BuildTrapLoaderData` built once per process**; remaining captured inputs named in section 7 |
| 5. mutations, fast-tier interim line | section 8 |

## 1. Oracle dirs (CONFIRMED)

Scratch `<workdrive>/gf-TrapOpen-scratch/` (`tr.sh`, `sd2.sh`, `or_all.sh`, `oracle/`, `scan_all.tsv`; Main removes it). Per capture: `frame_trace.py recorded` -> `snapdiff.py --only 0x14079A470` -> `trap_oracle.py`. snapdiff: pid2580963 6/6 proven, pid2621613 6/6, **pid2670420 5/6: seq1491 `mismatches=1` (field `w` only, rax and xmm0 equal; first write diff at index 1137147, a stack-local spill: the same class as seq23906 in TrapP section 1)**. Pitfalls met: a `sd` run re-using a case dir leaves no `corpus.jsonl` for the old calls (regenerate into a fresh out dir), and a failed snapdiff empties `tr/<cap>/pool` (rerun `frame_trace`).

`trap_oracle.py` changes: (a) **slim captures have no PM**: the height `A height_bits` is now recovered from P+0x3B18 (`cm = round(P+0x3B18*180)`, accepted only when `f32(cm/180) == P+0x3B18`; an exit otherwise); (b) new `N name rel len` lines classify the heap ranges the chain reads: `An` (P+0x3AC0 object), `MA` (`*(An+0x18)`, 0x26040 bytes), `E` (`*(An+0xE0)`), `GX`/`GY` (the physique chain), `SB0/SB1` (the table-B squad records, `G+0x1978+0x28 s`); only the loader ranges stay `H`. `physique_scan.py` (new) lists every `match::Player` of a full capture with PM+0x30, the 0x44-byte physique block (PM+0x784) and PES's stored G.

Captures that the oracle needs and a slim capture lacks: **PM** (`match::Player`: PM+0x30, PM+0x784 block, PM+0x7B4 slider) for every kind-0 capture. Workarounds: height from P+0x3B18; slider recovered from PES's own G (section 3). If the owner wants the real block in the oracle, Main needs a **full** kind-0 capture.

## 2. Oracle result (CONFIRMED)

`EveryCapturedCallOnGfsWindowGivesPesOutput`: issued flag, out word and every byte written to the action object equal PES on **24/24**. `DerivedPEqualsPesPOnEveryDerivedField` 24/24 (incl. rate ceiling), `RateCeilingEqualsPesBitForBit` 24/24, new `DerivedGEqualsPesChainGOnEveryCapture` 24/24, `EachWindowBindingChangesTheCall` (now 20 bindings, forked) green. Build/test: `PES_EXE` flat image, `PES21_ROOT`, `GF_PES_TRAP_ORACLE=<24 dirs>`.

## 3. G (CONFIRMED)

`src/pes/physique.{hpp,cpp}` (new, in `pesfmt`): `PhysiqueG(k, {heightCm, legSlider, unscaled})` = `FUN_141EB5810`'s root-bone arm: `BlockScale` (`FUN_14195CA70`: `(legSlider*legGain + 1) * (tallGain*dev + 1)`, 1 for an unscaled block, `dev` = `FUN_14195C4B0`: `(h-180)/40` for `h >= 180`, else `(h-132)/48 - 1`) clamped to `[0.9166, 1.0833]` (`0x1425DDF78/80`; values printed by the loader, never typed). The `root_main` joint-scale factor is 1 (no table entry): **proven by equality, below**. All 14 constants are read from the exe behind each load instruction's opcode (`LoadPhysiqueConstants`; sites `0x14195C4B7/4D3/4DB/4F5/4FD/50D`, `0x14195CA7F/A9A/AA4/AAC/AC6/ADC`, `0x141EB588E/89C`, each checked to name the VA the decompile names). Loaded in `LoadMoveStepConstants` (`MoveStepConstants::physique`).

Proof, `tests/pes_physique_test.cpp` 3/3:
- port == the **translated** `FUN_14195CA70` bit for bit on 481 heights (60..300 cm) x 5 sliders x unscaled byte (4,810 cases; both height segments, the floor, the ceiling, both slider and gain signs);
- port == PES's stored G (`P+0xA00` and the chain's G) on **every** player of 9 full captures (176 players: 8 pids/launches + the 3-tick set; heights 175/180/185/189/199; the clamp is reached by the 199 cm players). **Finding:** at match start every block holds the default 180 (all G = 1.0 whatever PM+0x30 says, e.g. `pid2580963/seq040`): G follows the block, not PM+0x30;
- on the slim kind-0 captures: P+0x3B18-recovered height + a slider recovered from PES's G; **24/24 bit-equal**. `pid2670420/seq043` is a 191 cm player with G = 1.0412: that is **slider = -1** (`(1-0.019)*1.0613`), the only capture with a non-zero slider; the slider candidates are the decoded values `2n/14-1` (`FUN_141AD98B0`, 15 -> 0) and exactly one reproduces PES's G. The slider's own source (the EDIT record's appearance nibble 0x11:0) is not in GF: **ponytail stand-in 0 (neutral), naming `FUN_141AD98B0`**, carried in `PesActorFacts::legSlider` and the movestep host.

Wired: `PesMovestepHost::PhysiqueG()` (the ten movestep sites + the rate ceiling) now = `pes::PhysiqueG(c_.physique, {round(physical_height*100), 0, false})`; the `= 1.0` stand-in is deleted. Fixture players are all 1.80 m so every committed smoke sees G = 1.0 (no hash move expected on the fixture squads; real squads move). The movestep wiring itself has **no direct test** (the adapter is private to `pesmovestep.cpp`, the fixture heights are all 180): guarded by `pes_movestep_wire_test` 28/28 (unchanged) and by the equality proofs above. In the window: `WriteActorP` now derives `P+0xA00`, `P+0xA08 -> A, A+0xF0 -> B, B+0xC0 = G` (two 0x100-byte objects after P: `PesTrapImage::physiqueA/B`), the rate ceiling from the same G; `PesActorOpenInputs` lost `g`, `physiquePtr`, `analyzePtr`.

## 4. P+0x3AC0 (CONFIRMED structure, derived)

`P+0x3AC0 = CtxVa(side)`: the window already held a ctx record per side (`+0x18 -> MA`, `+0xE0 -> E`, the same two fields An has), so `WriteActorP` points P at it (`PesActorFacts::side` = slot / 11). MA grew from 0x24900 to 0x26040 (the chain reads `MA+0x2601C`). New writers in `TeamAIController::WriteMaRecord` (every tick): the **holder** `MA+0x2601C` (`world.SetMaHolder`, 0xFF none) and the **NearestList** `MA+0x22E4C + 200*slot + 0x58*team + 8*rank` from `ScoreRows::nearest` (`SetMaNearestEntry`/`ClearMaNearestEntry`); test `PesA1Sim.TheTeamAisRecordMaCarriesTheHolderAndTheNearestList` (red on main: the writers did not exist), `pes_tick_a1_test` 26/26. In the oracle MA and E are filled from the capture's own bytes at the named offsets (`N` lines), because the oracle has no match: that is the named capture input of section 7.

## 5. The event record (CONFIRMED carrier, INF semantics)

The chain's `Y+0x180698` is `E.newest` = payload+0 of the newest **kind-7 routed** event: written by the router `FUN_140A3DC40` on its kind-7 arm (`pesevents.hpp`), whose payload `FUN_140A18940`/`BuildInPlayBallEvent` fills: `+0` action kind (1 short kick, 2 high kick, 5 shot, 7 touch), `+4` the last toucher's side, `+8` his slot, `+0xC` his roster index, `+0x10` the frame. **GF already carries it in PES layout** (`EventStoreImage::newest` in the window; `PesWorld::EventsVa()` is what `ctx+0xE0` names). Nothing new to write; `EachWindowBindingChangesTheCall` now claims `E+0x180698` kind/side/slot/tick: each flip changes the call in at least one capture (so all four words are read and matter). The capture still supplies these 20 bytes in the oracle (no match).

## 6. The P+0x2FC ring (decoded from data; writer OPEN)

Three full snapshots (`live2/ticks_pid91171/snap2-224747`, `snap3-224751`, frames 15716 and 15723): slot 0 (a standing keeper): ring k=0..5 = `(0.0144,0,-0.00239) / 2 / 3 / 4 / 5 / 6` at frame 15716 and `(0.02681,0,-0.00446), 0.01668, 0.01112, 0.00834, 0.00667, 0.00556` at 15723; the player moved 0.19 m in 7 ticks = 0.0272 m/tick = ring0. So **ring[k] = (predicted root position after 10(k+1) ticks - current) / (10(k+1))**: ring0 is the present velocity, the later entries average the player's own braking prediction (the keeper's displacement stops growing after about 20 ticks: `X_k = ring[k]*(k+1)*10` = 0.268, 0.334, 0.334...). INF (two ticks, one player). The writer is the movement prediction (`FUN_1406E9800` family, `pes-re-gaps-anime.md` round 3) writing per tick: **not decoded**; still the one `PesActorOpenInputs::velocityRing` capture input. Next: decompile the per-tick predictor's store into `P+0x2FC` (xref the 60-tick loop), then port it; the three snapshots answer "does it change per tick" (yes).

## 7. What the oracle still captures (named; CONFIRMED by the run)

1. the **ring** 12 bytes (section 6);
2. the **event record** 20 bytes and the MA holder/NearestList cells: GF writes both in a real match (sections 4, 5), the oracle has no match;
3. **SBn**, the table-B squad record (12 dwords at `G+0x1978 + 0x28*side`, read by `FUN_140A3B320`, 48 bytes in 9 of 24 captures): writer and meaning OPEN; the oracle binds table B to a captured segment;
4. the leg-length slider: recovered from G (section 3);
5. per-call world records (ball, man, slot, anime, MI, caller frame): allowed;
6. nothing from the loader (`H`/`I` lines are no longer replayed; the 24 calls pass with `PesTrapData`).

## 8. Verification

- Ran (all PES21_ROOT/PES_EXE set): `pes_trap_oracle_test` 7/7 (24 oracle dirs), `pes_physique_test` 3/3 (scan = 9 full captures), `pes_stamina_test` 16/16, `pes_tick_a1_test` 26/26, `pes_movestep_wire_test` 28/28, `pes_trap_data_test` 3 skipped (needs `GF_PES_TRAP_READS`; layout-only change here), `pes_angle_oracle_test` skipped (needs its own capture dir; it includes the changed `pesworldrecord.hpp`). `gen_translated.py --check`: see the final message. **GATE_TIER**: not run (interim line).
- Mutations (run on scratch copies, compiled and linked by hand, source untouched):
  - physique: tall-gain sign swap (killed: translated-diff + scan), leg-gain swap (both), unscaled byte ignored (diff + relations), no hi clamp (scan + relations), no lo clamp (relations), lower segment without `-1` (diff + scan + relations); **survivors, all equivalent**: dropping the floor select (`x2` only feeds a test the floor never changes), `anchor <= x2` -> `<` (both segments are 0 at 180), the ceiling test (implied by `anchor <= x2`), `leg*tall` -> `tall*leg` (commutative).
  - `WriteActorP`: chain A->B pointer to A (killed by the oracle on all captures and the unit test), `G+0.5` in B (killed), rate ceiling from 1.0 not G (killed: `DerivedPEquals`), `P+0xA00` store dropped (killed by the unit test only: the slim captures do not hold that page), slider ignored (killed), height +1 (killed). **Survivor: `CtxVa(0)` instead of `CtxVa(side)`**: MA content only affects no capture's path (the `MA+0x2601C` flip changed no call: that binding is not claimed). So the actor-side ctx choice and the MA cells' effect on the result are **not proven by the 24 captures** (read, but not path-changing).
- Not covered by a test: the movestep adapter's use of `PhysiqueG()` (section 3).

## 9. Open items (numbered)

1. P+0x2FC ring writer (section 6); port the predictor and delete `velocityRing`.
2. SBn (section 7.3): decompile `FUN_140A3B320` and the table-B record writer.
3. The leg-length slider source (`FUN_141AD98B0`, EDIT appearance nibble 0x11:0); a capture with a **full** kind-0 snapshot would also give the PM block (needs Main).
4. A capture where MA's holder/NearestList changes the chain's path (to claim `MA+0x2601C` and the ctx side).
5. `snapdiff` write-log mismatch on pid2670420/seq1491 and pid1981120/seq23906 (stack spill class).
6. `PesTrapLoaderData::AddTo` still copies the 4.9 MB segment into `PesMem::Seg` per call (`RunTrapKind0`): built once, copied per call; a shared view needs a `PesMem` change (`pes_state.hpp`, a translated-layer file).
7. Wire `WriteActorP` into `Match` (needs the MoveStep phase map; unchanged from TrapP section 4).
