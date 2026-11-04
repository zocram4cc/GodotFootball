# pes-port-TrapTick11: all 26 trap ticks equal PES (seq1917 closed: R's joint objects), No-Mod ruling applied, the stall is a free-kick taker that never kicks; gate not run (01-11-26)

Branch `StoppageFlow`, head `1d704f41` (`git ls-remote` confirmed). Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Sims ran at `nice -n 19`.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge `origin/main`, push | done twice: `ceddeb4e` (batch AR state), then batch AS (`c38d2743`) in `1d704f41`. Conflicts: 3 generated closure files (took the merged generator's comment format, kept this branch's extra constants), `pesworldrecord.hpp` (both sides appended after the stack: the sanction image first, then F and F+0x868's object, offsets re-derived), `teamAIcontroller.cpp/.hpp` (both kept; `TrapFactsOf` re-wrapped in the anonymous namespace main closed earlier), one duplicate `SlotRecord` accessor removed. `gen_translated.py --check` before regeneration: 75 drifts (main's xlate.py changes); layer regenerated (4618 functions) and committed |
| 2. ruling | `LoaderDataTheTickReadsEqualsTheCapture` skips every oracle dir whose name does not start with the No-Mod launch `pid3741448_` and prints it as mod-era; the comparison on No-Mod dirs is unchanged. The tick tests keep all 26. `TheCapturesCoverTheStartAndBothMirrors` now counts No-Mod dirs only (`starts > 0`, mirrored > 0, plain > 0) and is **RED**: the 12 No-Mod captures all have P+0x245C in {1, 3} and mirror byte 0 (script-read from the capture memory), so there is no start and no mirrored tick. Main armed `trap_action_tick_797CD0` with `--when 'b(rdx+0x245c)==0 or b(rdx+0x2472)==1'` (rvwatch37, cap 24); the new captures land in `live2/pid3741448/` and must be added to `all26`-style oracle dir (`trap_oracle.py`) |
| 3. seq1917 | **CLOSED**: `EveryCapturedTickOnGfsWindowEqualsPes` passes on all 26. Two causes, neither the side-B mode word (see 3.1) |
| 4. live-ball stall | **STILL RED**, re-diagnosed (section 4) |
| 5. unbounded seq2028/2035; `GATE fast` | not run (budget); `gen_translated.py --check` on the final tree: see the last line of section 5 |

## 3. seq1917 (CONFIRMED)

Method: PES side = unicorn replay of the capture (`wall.py`: every byte written to P and the caller stack, with pc); GF side = the window run's raw write log; both normalised to P-relative and entry-rsp-relative addresses. The two logs agree byte for byte in address sequence (except return-address pushes, which the translation does not model) until the first value divergence.

### 3.1 The side-B word `[entry+0x118]` is not the cause (CONFIRMED)
`FUN_14085E3C0` (0x14085EE4E `CMP EAX,0x928`): PES word 0x124A9 and GF word 0 are both != 0x928, so both take the same branch. TrapTick10's lead is refuted for the value, but table B IS read elsewhere in the same function (3.3).

### 3.2 Cause 1: the joint objects (R+0x90..) (CONFIRMED)
`FUN_1406E6D10` (called from 0x14085E765's path via `FUN_140860240`) normalises `joint[k] - pos` using `FUN_14081D8C0(Q, k)` = `*(R+0x10)[k]`, R = `*(Q+8)`. R's pointer array (+0x30, 11 entries, count at R+0x20) pointed at nothing in the window, so GF read zeros and `FUN_140A8F8F0` normalised a zero vector: **NaN** (`0xFFC00000` x3 at `0x1406E70E7`'s stores where PES stores a unit vector, e.g. -0.526, -0.083, -0.846). The capture's joints are 0x160-byte objects: in seq1917 `array[k] = R + 0x90 + 0x160*k`; in seq126 the allocator scattered them (joint 0 at R+0x30, joints 4.. elsewhere), so they are separate objects, not a fixed tail. The tick reads joint 7's `+0x48` (vec3) and `+0x94/+0xA0` (f32) (manifest H lines `0x4480EB38 +12`, `0x4480EB84 +16`).
Change: `kRBytes = 0x90 + 11 * 0x160` (`pestrapworld.hpp`: `kRJointsOffset/kRJointStride/kRJointCount`), `WriteActorP` fills R's pointer array. The pose cells themselves are the animation engine's output, which GF does not run: the oracle supplies the read cells from the capture (named open input); in a match they stay zero as before (no regression: the array read null before). The oracle test gives each joint its own span (`kObjJoint0 + k`, by the captured array), which fixed the one regression this created (seq126's scattered joints).

### 3.3 Cause 2: table-B player-info entry reads (CONFIRMED)
After 3.2 the first value divergence is `FUN_140862530` (called by `FUN_1408614C0`; returns the contact-point rating as xmm0): `bVar1 = *(FUN_141B59E20(FUN_140A3AF90(FUN_140A489D0(G, side), idx)) + 0x20)` = **byte at entry+0x50** of the player-info entry (`record + 0xA8 + idx*0x128`), averaged with live ability 0x18 (Finishing, `FUN_1406E6660`). PES 1.0, GF 0.1527. The manifest reads three cells of that entry: `+0x30` (8 bytes: 0xFFFE, id 0x124A9), `+0x50` (1 byte, 0x63 in the bot match), `+0x118` (the id again). The record is bound at its captured VA (`CopyNamed`, rows `N SB1`), but those reads are H lines the tick test ignored. Fix in the oracle: H lines inside a table-B record become captured segments (`BuildTick`). The GF match has no writer for these cells (only +0x34 `SetPlayerInfo` and +0x78 `SetCarrierProfile`): named stand-in in `teamAIcontroller.cpp` (`// ponytail:`), removed by the roster loader's writer of the entry (OPEN: which GF stat maps to ability byte +0x50, the ability-id table is only partly decoded: ids 0x18 Finishing, 0x27 Speed, 0x2C jump, 0x31 height).

## 4. Stall (OPEN, re-diagnosed on the merged tree)

`pes_stall_test` (merged tree + this change, 695 s): `TheBreakerNeverForcesABallUnderAnArmedRestart` OK; `ALiveBall...` FAIL, 346 live ticks, ONE episode (three report lines, 5 s / 8 s / 11 s):
`ball 0.08 m/s at (-46.3, 31.4, 0.1) | buf active 0 prepared 1 started 1 piece 3 (free kick) taker H2 start-in -1.0 s | forcing A2 from 20.3 -> 18.9 -> 17.4 m`; the forced man is 17-20 m away (not 0.2 m: the earlier near-ball stalls of TrapTick9 are gone on this tree). `[stalemate-who]`: taker H2 is `movestep 5<-4`, function 14 `walk_1_1_relax` (a walk), never kicking; A2 walks/idles. So the live stall is a **free-kick taker (piece 3) that has started, is in play, and never takes the kick**: the referee's `started` flag is set and `inPlay` is 1, yet the taker only walks. Not a trap-chain or kind-0 refusal defect: the `{reason, rank}` search (`FUN_140857220`... `FUN_140863C40` rank 2) is not on this path, and no capture of `trap_kind0_79A470` is needed. Next step (not done): trace why a started free kick leaves its taker in WALK: the taker's command source (kick chooser `FUN_1408364D0`, KickKind's 24 proven captures; the walker's approach end rule) with a temporary print of the taker's `GetPesDecision().command` at the stall, then compare with `pes-port-StoppageFlow*.md` set-piece walker notes.
Earlier decode (TrapTick9 section 3, still valid, unused): refusal struct `{reason +0, rank +4}`; rank 2 = near miss (`0x140863D1A, 0x140863DA3, 0x140863E40, 0x1408640AF, 0x1408641DB`), refusal only when rank == 2 and `FUN_140849400` is false.

## 5. Checks run
- `pes_trap_oracle_test`, `GF_PES_TRAP_TICK_ORACLE` = all 26 dirs: `EveryCapturedTickOnGfsWindowEqualsPes` OK, `ListWhatTheOverlaySupplies` OK, `LoaderDataTheTickReadsEqualsTheCapture` OK (12 No-Mod compared, 14 mod-era skipped, printed), `TheCapturesCoverTheStartAndBothMirrors` FAIL (starts 0 / mirrored 0 in the 12 No-Mod: needs the new captures). Red-first: seq1917 failed on the unmodified tree for these causes; the NaN and the 1.0-vs-0.1527 were reproduced before each change.
- Mutations: none run beyond the red states above (not done: revert the joint array fill, revert the table-B segment supply: each must re-fail seq1917).
- `pes_stall_test` (direct, slot 4): 1 passed, 1 failed (section 4).
- Not run: `GATE_TIER=fast`, unbounded seq2028/2035, the 11 `TrapOracle` kind-0 tests (they skip without `GF_PES_TRAP_ORACLE`).

## 6. Unfinished (numbered)
1. New No-Mod start/mirrored ticks (rvwatch37, cap 24) -> `trap_oracle.py` dirs -> run `TheCapturesCover...` and the full 26+ tick/LoaderData tests (a start tick may expose a new derived mismatch).
2. Stall: the free-kick taker that walks (section 4).
3. `GATE_TIER=fast GATE_SLOTS="3 2"` with the gate env on the final tree; unbounded replay of `pid3061186` seq2028/2035.
4. Roster writer of the table-B player-info entry (ability bytes from +0x50, ids), joint pose cells (animation engine).
5. Scratch `gf-StoppageFlow-scratch` (about 1 GB) can go once item 1 lands; the `all26` links point into it.

`gen_translated.py --image pes_full.bin --check` on `1d704f41`: **4618 functions (3706 recorded bit-exact, 912 closure) regenerate byte-identical**, exit 0 (three held-out roots as before).
