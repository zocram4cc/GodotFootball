# Dribble proposal 6: slot ability bases from Player.bin for PES-id players, block byte 0x34 (+0xA4) written, angle oracle green; the 12 per-function replay proofs are NOT done (DribbleProposal6, 31-10-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-DribbleProposal-scratch`, PES-derived, never committed). Image `pes_full.bin`. CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Continues `pes-port-DribbleProposal5.md`.

## 0. Verdict

| Step | State |
|---|---|
| 1 `git merge origin/main`, push | DONE (fast-forward to `121ab548`, pushed). |
| 2 per-function replay proofs of the 8 closure members (DribbleProposal3) and 4 seeds (DribbleProposal4), `results_dribble.json` | **NOT DONE.** Finding in section 1: no captured state ever calls any of the 12. |
| 3 `+0xA4` production writer, `pes_angle_oracle_test` with its env | **PARTLY.** GF now writes block byte 0x34 for every slot through the store step; the angle oracle is regenerated and 4/4 green. PES's own copy of the roster row into the AI record is still unlocated (section 3). |
| 4 Player.bin ability path | DONE for Speed, Jump, Height (+ the foot byte). Not verifiable against the captured rows (section 4). |
| 5 fast gate | Section 6. |

## 1. Replay proofs of the 12 members: why none exists (CONFIRMED)

Members: `0x14063E830, 0x14063E9F0, 0x142119D90, 0x142119E00, 0x142119E60, 0x142119E90, 0x142119EB0, 0x142130EE0` (DribbleProposal3) and `0x140A57450, 0x14063D2F0, 0x14211CA50, 0x14211CB00` (DribbleProposal4).

- `frame_trace.py recorded --spec-dir tools/dev/diffharness/specs/dribble` records the entry of every spec in the dir (267 specs, all 12 members have one) while PES's bytes run the entry state's call. So a per-function replay needs an entry state whose call enters the member. The earlier `rv_d710` traces held only `14211D710` because the dribble spec dir then held only the three roots.
- Tooling, regenerated this session (scratch `t12.sh`, `t12loop.sh`): `synth_walk.py --entry 0x1421102D0` (the proposal writer, whose kind step `0x14212FE70` calls the members) from the 10 `d710` danger-builder captures and the 12 `pid483669` carrier_clear captures; trace each with specs/dribble; keep a state only when a member was entered. Each state takes 35 s and 2 GB (deleted when it hit nothing).
- Why: the members sit on branches of the kind step that no captured state takes. `0x14212FE70`'s tail (`fe70.dec.txt` lines 490-535) picks the record filler by `iVar10`: 3 -> `0x142119EB0`, 4/5 -> `0x142119E20`, 6 -> `0x142119E60`, anything else -> `0x142119E90` (the "none" record); the captured states all end in `0x142119E20`. `0x142119D90` is in the kind-4 stand-down arms, `0x14063E830` in the arm gated on `FUN_141052E30 == 3`, `0x142130EE0` in the branch gated on `param_3[7]+0xC4`/`+0xA0`, `0x14211CB00` in the `0xC4 == 4` arm.
- What a proof needs (not built): synthetic call states, i.e. a tool that takes a forward-emulated state (e.g. seq4891 at `0x1421102D0`), keeps its memory (a symlinked `mem.bin`), and writes a `call.bin` with chosen registers for the member, then `frame_trace recorded` + `snapdiff --only VA --bounded` as for the roots. The members are small: `0x14063E830(rcx, edx = slot < 0x16, r8d = bit)` reads G; `0x14211CB00(rcx, edx)` is a switch; the fillers write a record. These cases would be synthetic inputs over PES's real memory (PES's bytes run as the reference), to be labelled so in `results_dribble.json`. Not written: `results_dribble.json` is unchanged and the 12 stay without a replay proof. DribbleProposal5's "ran bit-equal inside the chain tests" cannot hold for them: no captured chain state enters them (above), so no test has run them yet.
- Result: the 7 builder captures that reach the entry (`pid1792161` seq16184, seq16456; `pid1965411` seq495; `pid1981120` seq4046, seq4444, seq4891; the first run of seq4891 listed all 50 traced callees) enter `0x1421102D0`, `0x142119DF0`, `0x142119E20`, `0x14212FE70` and `0x142131810` and **none of the 12**. The other sources (`pid1965411` seq033, `pid1981120` seq5314/5769/6161, all 12 carrier_clear captures) fail the forward run ("not reached"). The `0x14211D710`-entry pass was still running when the budget ended (its log: `gf-DribbleProposal-scratch/t12loop.log`).

## 2. What changed in GF (CONFIRMED run)

- `src/pes/playerdb.{hpp,cpp}`: `PlayerTraits` gains `speed`, `jump`, `heightCm` read from the record: Speed = id 0x1C (accessor `0x141F0F7C0`, bit 306, 6 bits, +40), Jump = id 0x1F (`0x141F0F760`, bit 294, 6 bits, +40), Height = id 0x10 (`0x141F0F590`, byte +0x1B, +100). They are the base bytes of block 0x27, 0x2C and 0x31 (the ids Main named). The foot (`leftFooted`, id 0x0F, bit 514) was already there.
- `Match::PesTraitsOf(data)`: the record by the player's PES id, nullopt for a player with none.
- `WriteSlotAbilities` (elizacontroller.cpp): base = `PlayerDatabase` byte for a player with a PES id, GF's own stat/height only for one without (the named `ponytail:` stand-in); new write of block byte 0x34 (`kAbilityStrongerFootId`, `+0xA4`) = `Match::LeftFootedOf(data)` through `StoreLiveAbility`. `kSlotByteA4Offset` is `static_assert`ed to `kSlotAbilityOffset + 0x34`; its comment no longer says "no GF writer".
- Tests (`tests/pes_playerdb_test.cpp`, 10/10 green): `ReadsSpeedJumpAndHeightWithTheirBiasesAndNothingBeside` (red on `origin/main`: the fields do not exist; neighbour bits set so an off-by-one read fails) and `AbilityOffsetsEqualTheExesAccessors` (decodes the four accessors out of `PES2021.exe` with `test edx,edx; jnz; mov eax,[rcx+d]; shr; and; add` / `movzx; add al` and compares bit, width and bias with the header constants; PASS: Speed 306/6/+40, Jump 294/6/+40, Height 216/8/+100, foot 514/1/0).
- Not run: mutations of the new tests (header constant changed -> `AbilityOffsetsEqualTheExesAccessors` / `ReadsSpeed...` RED), and no test covers the `WriteSlotAbilities` branch choice (it lives in the controller; guarded by the gate's smokes only). Say so.

## 3. `+0xA4` (OPEN part)

- CONFIRMED: `+0xA4` = record `+0x70` + 0x34 = ability block byte 0x34 (`0..1` in the clamp table `0x142613C60`); the store step `0x14092AF10` multiplies the base by cells of 1.0 for it and, since `v == block[id]`, never writes it: the byte is whatever the block held when the step ran. Captured rows (3 captures, 22 slots, `scr/rows.py`): byte 0x34 varies per player, 0x33 is 0 on all.
- INF: block 0x34 is the stronger foot (1 = left; the corridor angle reads it). Block 0x33 stayed 0 for all 22 players (the other 0..1 byte).
- OPEN: PES's writer of the initial block (the `0x50`-byte copy roster row -> AI record `+0x70`; `scr/blk2.py` shows the block equals the row for bytes 0x00..0x4F except the rebuilt ids, and the row's player id at block `+0xE0` is zero in the record). Scans for a 0x3C/0x50/0x128-byte copy beside the 324 `0x14124DAD0` call sites found nothing; the copy is probably vector moves. `0x14092AF10`'s base pointer is `FUN_141b59e20(...)+8` (`+0x30` of its argument). GF's write goes through the store step like the other three ids, with the Player.bin foot as the base.
- Angle oracle: `pes_angle_oracle_test` **4/4** with `GF_PES_ANGLE_ORACLE` = a dir regenerated this session from `pid407792/seq021` (`frame_trace recorded`, `snapdiff --only 0x142108630 --bounded` 1/1 bit-exact, own-block 0.418, then `trap_oracle.py`; `angleor.sh` in scratch used `danger_oracle.py` first, which writes no `ok_calls.bin` and fails the loader). The test copies `+0xA4` from the capture into its window, so it does not exercise the new production writer.

## 4. Player.bin versus the captured rows (CONFIRMED negative)

- The install's winning `Player.bin` (4cc placeholders) does not hold the captured players: `PlayerDatabase::Find` of ids 72101-72112, 74902-74921 gives flat Speed/Jump 40, height 180, while PES's roster rows have Speed/Jump 77/88/99 and heights 175-199 (a throwaway test run, removed). A 312-byte record carrying those ids is not in PES's memory either (`scr/findrec.py`: the id's 4 bytes occur 9-15 times, none is a record). So these players' rows come from another source (the edit/roster data), and the id map **Speed 0x1C -> 0x27, Jump 0x1F -> 0x2C, Height 0x10 -> 0x31, foot -> 0x34 is not checkable against captured rows**; it rests on the accessor labels (`pes-re-matchlogic.md` 1.2/1.3: Speed/Jump pair INF, as DribbleProposal5 said). The roster rows of those captures are flat (77 / 88 / 99 on every ability), which is why the Speed/Jump pair cannot be told apart by values either.
- Consequence for the owner's install: a player imported with a PES id whose Player.bin record is a placeholder now takes the placeholder's Speed/Jump/height (40/40/180), not GF's stat. That is PES's rule applied to this file; whether PES's row for such a player is the placeholder is not shown (the captured players' rows differ from it).
- The id map beyond these four ids ("every id the store step writes") was not extended: GF writes only 0x27, 0x2C, 0x31 and 0x34. The packed-edit setter `0x1412E4C60` / field setter `0x1414C6E90` use an id space of block index - 6 (ids 0x0E..0x27 minus 0x20 for the 25 abilities); the Player.bin-to-edit-object id table was not decoded (`FUN_141EEDAD0(db record, dest, field id)` reads by the accessor table `0x143529930` and writes through a second descriptor table via `FUN_140BFC5D0`).

## 5. Left, numbered

1. The 12 replay proofs: synthetic call-state tool (section 1), cases per member, `results_dribble.json` records, regenerate and `--check`.
2. The initial block copy into the AI record (section 3) and the Player.bin -> row id table (section 4).
3. `TurnReferenceOracle.EveryCapturedEntry...` and `...TheProposalChain...` FAIL on the scratch `oracle_d710` dirs for seq4891 and seq6161 (`BP+0x3328` GF 0x11, PES 0x15): the identical symptom DribbleProposal3 fixed with `which()`, so the dirs are probably stale manifests from before that fix (INF); none of my changes touch `RunTurnReference`. Regenerate with `walkdir.sh`/`oracle.sh` and re-run.
4. Kind 0x13 and flag-class captures (DribbleProposal5 section 5), A4 super-sub cells, breath-penalty ids.
5. 20-minute smoke (Main's batch).

## 6. Checks run

- `pes_playerdb_test` 10/10; `pes_angle_oracle_test` 4/4 (oracle env set); `pes_playerdb_wiring_test` 4/4; `pes_dribble_oracle_test` (`GF_PES_DRIBBLE_ORACLE` = `oracle_d710`): `CandidateScores...`, `AbilityStore.*`, `CarrierConstruct` pass, the two `TurnReferenceOracle` tests fail (item 3 above). Whole tree rebuilt (342 targets).
- `gen_translated.py --check`: not re-run (no translated file changed). `ledger.py --check`: "LEDGER.md current".
- `GATE fast`: see the final message.
- Pushed: `fcbd5c12` (`git ls-remote origin DribbleProposal` confirms); merge was a fast-forward, nothing else pushed.
