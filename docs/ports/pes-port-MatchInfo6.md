# Pressure lane lists bit-exact on GF's window (install tables bound); carrier walk 0x14210F110 proven 8/8; forced clear NOT ported (no capture reaches its block) (MatchInfo6, 29-10-26)

Branch `EndClear` (worktree `gf-EndClear`), head `a2fa4c2e` (confirmed by `git ls-remote origin EndClear`). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = blocker named. Fast-tier gate: `GATE_TIER` is not on this branch, so the interim line: pes_* binaries run (below) + `gen_translated.py --check` NOT rerun after the regen (the commit is the regen's own output; run it on merge).

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/main`, push | DONE (`724097c4`; the plain `git merge` trap: use `git -c submodule.recurse=false -c status.submodule=none -c diff.ignoreSubmodules=all`). |
| 2 unbound input behind the lane-list difference | **FOUND and BOUND.** Three install tables the window runs read at register-held addresses (§1). `DangerOracle.EveryCapturedCallLeavesPesImageARecords` (the old `DISABLED_` lane test, widened to every byte PES wrote in imageA below the danger byte) is bit-exact on **all 10 captures**; red without the binding on seq4046 and seq4891 (233 bytes each shown), green with it; 3 mutations. |
| 3 translate the carrier walk | **DONE, PROVEN 8/8** bit-exact (rax, xmm0, every written byte), own-block coverage **0.1092** (39 of 357 blocks): §2. |
| 4 forced clear from PES code in GF, delete `forcedClear=false` | **NOT DONE (OPEN 1).** No captured walk call reaches the Clear block, so there is no oracle for a red-first test. |
| 5 NativeSeam over-decode | §3: the short decodes are correct; **origin/main currently carries the wrong (over-long) files**. |
| 6 gate line / smokes | §4. |

## 1. The unbound input (CONFIRMED)

Method: diffed the ten captures' written imageA bytes (GF window run vs the harness's write log) instead of a read trace (a read trace needs a `pes_state.hpp` change = a full layer rebuild). The first divergence: seq4046's first lane list holds man 2 at squared distance 10.85 in GF, 156.1 in PES, and the lane records `P+0x4C+0x1C*k` carry man 2's position in GF where PES's hold zeros. `0x14069A500` (`dec_14069A500.c` L~110/L~250) builds a lane record only if `0x140A6E3C0(anime record)` is 0 and `anime+1 != 'W'`. `0x140A6E3C0` (MOVSX `0x140A6E3CE`, `CMP EAX,0x5E` `0x140A6E3D3`, `JL` `0x140A6E3D6`, `LEA RCX,[0x1426484F0]` `0x140A6E3DB`, `MOV EAX,[RCX+RAX*4]` `0x140A6E3E2`) reads the anime class table at a register-held address with an index the translator cannot bound (signed byte), so it is not baked, and the window run mapped no image: GF read zeros, so man 2's class gate never fired. Every capture's input bytes copied from the manifest were equal (checked per byte), which is why the manifest showed "no other record".

The full list of image bytes the danger closure reads that are not baked (rconsts: unicorn replay of the ten captures, every data read in the image range; translated sources grepped for data VAs materialised in registers): 

| Bytes | Reader | Role |
|---|---|---|
| `0x1426484F0` anime class table, 94 u32 entries, plus 128 entries before it (the signed index) | `0x140A6E3C0`, `0x140A6E450` | class gates |
| `0x142655950` quarter sine table, 513 floats | `0x140A92C80` (via `0x142138220` writing imageA+0x2B4..) | corridor tables |
| `0x14351AE50` (8 bytes: int 54 at +0, float 54.0 at +4) | `0x141E532E0`, `0x141E532D0` (initialised by `0x141E532F0`: `DAT_14351AE54 = DAT_14259D624; DAT_14351AE50 = 0x36`) | prediction frames (list kinds at imageA+0xB6C) |

Not bound, by design: allocator globals (`0x1436EF*`, `0x14258E27x/E39x`, `0x1437F0110`: PES's allocator, replaced by GF's native heap in production), the GS cookie `0x14350B340` (stack-protector word, cancels out), G's cell (the window's own).

Binding (clean, no stand-in): `src/sim/onthepitch/pesimagetables.{hpp,cpp}` `LoadPesImageTables(PeImage)` reads the three ranges from the install's `PES2021.exe` at load (the cells are checked against `0x14259D624`, the float the initialiser copies); `MatchSetup::pesImageTables` (required, thrown if missing, like every install constant), `Match` hands it to `PesWorld::BindImageTables`, `RunDangerBuilder` maps the segments per call. Plumbed in `gfsim_run`, `PesLibrary`/`GfMatch`, and `tests/sim/pes_match_constants.hpp`.

- Red (CONFIRMED, run): bind line commented in the test's `Build`: `EveryCapturedCallLeavesPesImageARecords` FAILED. Green: 5/5 `pes_danger_oracle_test`.
- Mutations (run, rebuilt, reverted): M1 class-table length 0 -> FAILED, exactly seq4046 and seq4891 (233 bytes each: the original 467-byte difference); M2 sine table 1 entry -> FAILED; M3 frame cells not mapped -> FAILED.
- The widened test also proved the list kinds at imageA+0xB6C and the corridor tables: **every written byte of imageA below +0x31C1 equals PES's on all ten**, which the forced clear reads (N1 over `imageA+0xB6C`, P at `imageA+0x2124`).
- Other window runs (`RunClearUnit`, `Involved`) do not map these tables yet; the Clear unit's own predicates read the same class table (`0x140A6E450`). INF: bind the segments in `NewCall` when the next consumer needs them.

## 2. The carrier walk 0x14210F110 (CONFIRMED)

- `closure.py --spec-set core --tag walk 0x14210F110`: 87 members (5 new specs: 0x14063DE20, 0x14063E3A0, 0x140A603C0, 0x140A604D0, 0x14210F110), set file `closure-walk.txt`.
- `frame_trace.py recorded` on the 8 captures (`pid1965411` seq035, seq627; `pid1981120` seq3914, 4518, 5130, 5878, 6294, 6695; all exit 0), `snapdiff.py --hooks hooks.cfg --only 0x14210F110`: `calls=8 replayed=8 mismatches=0 own_block=0.1092 proven=True`. Returns: seven 0x19 (no kind), seq5878 returns 3. Counts `tools/dev/translate/results_walk.json`; record in `results_core.json` (`proof: snapdiff`); layer regenerated: 4346 functions (3694 recorded, 652 closure), `pes_fn_14210F110.cpp` in `ball/`; `pestranslated` builds.
- **Coverage (CONFIRMED by unicorn PC trace, `wpc.py`)**: none of the 8 captures reaches the Clear block (`0x14210F4F2`, the `*piVar20 == 5` arm), the skip gate's reads, or the replacement chain (`0x14210023A`); each takes the same 113 PCs (seq5878: 172, the kind-1 -> 3 return). The three new walk captures of `pid2580963` (seq066, seq714, seq1288) take the same path (told to Main: delete them).

## 3. NativeSeam's over-decode (CONFIRMED)

`0x141FEE390` is ONE instruction (`JMP 0x141FFB9E0`, bridge disassembly); `0x1403D2E60` is six (`... RET`, then `JMP 0x1403DB250`). The correct committed bodies are 16 and 30 lines (tail calls); the extent rule (decode to the `.pdata` RUNTIME_FUNCTION end / the terminal `RET` or tail `JMP`, never into the next function) is what the generator applies: my regeneration re-emitted exactly 16 and 30 lines. **Branch state (CONFIRMED by `git show`):** `origin/NativeSeam` carries 15,532 and 5,529 lines (wrong); **`origin/main` today carries 15,399 and 5,516 lines (wrong; drift: `gen_translated.py --check` on main should flag it)**; `origin/StoppageFlow` and `EndClear` carry 16 and 30 (right). Main must take the EndClear/StoppageFlow side of these two files on merge (or regenerate).

## 4. Verification

- Binaries run (sim slot 4 / direct): `pes_danger_oracle_test` 5/5, `pes_world_test` 6/6, `pes_match_info_test` 2/2, `pes_tick_a1_test` 26/26, `pes_carrier_test` 189/189, `pes_movestep_wire_test` 28/28.
- **Red from Main: `MoveStepWire.TheKickoffTakerClosesToTheBall` PASSES on EndClear `a2fa4c2e`** (28/28, `[movestep-approach] first 4.38 closest 0.16`). So EndClear alone does not reproduce it; the failure Main saw needs StoppageFlow's tree merged with it. Not bisected (owner handover): INF the kickoff taker's carrier decision reaches `RunDangerBuilder`, so the ten-capture-exact builder now moves a seeded kickoff relative to the zeroed-table run Main's gate saw; if it reproduces on the merged branch, re-stage by seed search (never weaken the 0.65 m limit), or check that the merge keeps `MatchSetup::pesImageTables` set (a missing table is a thrown error, not a silent zero).
- Smokes (2 min, seed 7, both paths, each run twice: identical): switch-off `[determinism] 11c83698afc83d13` (27703 ticks), pes-loop `81a8586df698e5bf` (28906 ticks); passes/shots non-zero. Not compared with main's hash (Main compares).
- `gen_translated.py --check`: not rerun (see header).

## 5. Open items

1. **Forced clear (rule 7 chain gate) not ported; `WalkContext::forcedClear` (pescarrier.hpp:898, set false by the caller) stays.** Blocker: no capture reaches `0x14210F4F2` (§2), so a red-first test on a call where forced clear differs from GF's old behaviour does not exist. Next action: replay each new `carrier_walk_210F110` capture (`live2/pid2621613/`, cap 30, EVERY 2000) with `wpc.py`-style PC trace for `0x14210F4F2` until one hits (also keep the one that reaches `0x14210023A`), run `frame_trace.py recorded` + `snapdiff.py --hooks ... --only 0x14210F110` on it to raise coverage, then either (a) call the translated walk from GF on a PES BP (needs the priority list at BP+0x3E20 and the answer records `0x14211EF20`/validity `0x142119DF0`, BP+0xE4/+0x41E4/+0xAB8/+0x3380/+0x4204, all of which GF's `UnitSlot` table has no PES-layout writer for) or (b) port MatchInfo4 §4 (N1 over the lists at `imageA+0xB6C`, cls over `P+0x4C+0x1C*k`, the `F997` guard) as a pure function over the window's now-exact lists and diff it against the translated walk's chain-entry decision on that capture.
2. Walk own-block coverage is 0.1092; the proof holds under the correctness rule but 318 blocks (the whole Clear arm, replacement chain, set-piece tail) are unexercised.
3. Window runs other than the danger builder map no install tables (§1).
4. `pes_movestep_wire_test` red (Main's): not reproducible here; see §4.
5. Cleanup: `gf-EndClear-scratch` (traces `tw_*`, `sdw`, `dorc`, `rc_*.json`) once archived. Scratch scripts (not committed): `rconsts.py`, `rpc.py`, `wpc.py`, `walk_trace.sh`, `walk_sd.sh`, `cap_men.py`.
