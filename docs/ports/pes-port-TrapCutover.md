# pes-port-TrapCutover: GF's trap runs PES's kind-0 chain in a match; the match aborts on the slot record's checked stamina cells (29-10-26)

Branch `StoppageFlow`, head `b65e48dd` (`git ls-remote origin StoppageFlow` confirms; earlier pushes `6ccebe49` merge of `origin/main`, `744eab9a` WIP). Slice S4 of `docs/re/pes-re-trap-inputs.md` section 6. Tags: **CONFIRMED** = a tool run this session; **INF** = inference; **OPEN** = not settled (blocker named). Image `pes_full.bin`, base `0x140000000`.

## 0. Verdict against the acceptance

| Acceptance | State |
|---|---|
| Merge `origin/main`, push | DONE (`6ccebe49`) |
| `WriteActorP` wired into `Match`, P per player, written each tick from GF state | DONE in code (`WritePesTrapActors`, one `PesTrapImage` per slot in the window). **Not exercised green**: see item 1 |
| Trap image built once per match, no per-call copy | DONE: `PesTrapMemory` (a private copy-on-write mapping of the dump + the loader segment, built in the `Match` constructor); `RunTrapKind0` lends the loader segment to each call by move (no 4.9 MB copy) |
| `_TrapCommand` builds the request from `ChooseFirstTouch`, calls `RunTrapKind0`, retry rule, clip = action `u16 +8` | DONE in code (CONFIRMED to compile and link; the in-match path runs: the chain is reached from `_ReactionCommands`) |
| Delete `PickTrapClip`, `TrapBallClipPoint`, their tests, the G5 designation stand-in | DONE (picker, point builder, `ClipPoint`, the 4 picker/install tests; the designation gate now applies only off the PES path) |
| `pes_stall_test` live-ball case re-enabled and green | **Re-enabled; NOT green: the match ABORTS** (item 1) |
| Red-first clip test (seq4469 `0xB86`, seq23906 `0xA41`) | DONE and green: `TrapOracle.TheMatchsOwnMemoryCommitsPesClipOnTheIssuedCaptures` (CONFIRMED, 1 test run) |
| Fast-tier interim line, 2-min smoke determinism on both paths | **NOT RUN** (budget; the PES-path match aborts, so there is no PES-path hash) |

## 1. The blocker (OPEN, with the exact cause)

`pes_stall_test` (seed 7, 2 min, `pes_loop`) aborts in the first match: gdb backtrace `pes_fn_140A58550` (cold path) <- `pes_fn_140A581C0` <- `pes_fn_1407987F0` <- `pes_fn_14079A470` <- `PesWorld::RunTrapKind0` <- `ElizaController::_ReactionCommands`. `0x140A58550` is PES's checked-integer reader (decompile: a four-word ring `+0xF8 + 4 * (idx & 3)` and a counter `+0xF0 * 3`, hashed FNV-1a-64, compared with the stored hash at `+0xE8`; a mismatch calls the trap `FUN_1415A1714`). `0x140A581C0` (reserve percent, TrapP section 2.3) reads the slot record's cells at `+0xC8` (tank cur), `+0xE8` (max), `+0x108` (breath). In the oracle they come from the capture; **in a match nobody writes them**, so the hash check fails and PES's own code aborts. This is the same class as the `P+0x2458` abort noted in TrapGF.

Next step (one slice): a writer for the slot record's three checked cells from `Player::GetStamina()` (`StaminaState` cur / max / breath), each in PES's layout and with PES's hash (`tools/dev/translate/trap_oracle.py` `checked_int` decodes the layout: lines 90-110; the setter is the function that stores `+0xF0/+0xF4/+0xF8` and the hash, to be found by xref on `0x140A58550`'s callers' stores). Call it from `WritePesTrapActors` (it already runs each tick for every active man). Until then the in-match chain cannot be called without aborting; `_TrapCommand` should not be merged to a branch that runs a PES-path smoke before that writer exists.

## 2. What landed

- `pesworldrecord.{hpp,cpp}`: `std::array<PesTrapImage, kSlotCount> trap`; `PVa(slot)`, `P(slot)`, `PhysiqueA/B(slot)`; `RunTrapKind0(slot, request, PesTrapExternals&)` (segments lent by move, `imageBase` field).
- `pestrapactor.{hpp,cpp}`: `LoadActorConstants(const PeImage&)` (reads through the install's exe; the flat-image overload is gone); `WriteActorP` writes the slot's P; `kAnimeActionMove`, `kPNoMotion`; the velocity ring is six 12-byte entries (the oracle fills the first from the capture).
- `pestrapdata.{hpp,cpp}`: `PesTrapSource` (`LoadTrapSource(exe, vfs, dumpPath)`: actor constants, loader data, the dump's path) and `PesTrapMemory` (per match: a `MAP_PRIVATE` mapping of the dump, `imageBase` `0x140000000`, plus the loader segment).
- **Finding (CONFIRMED):** the install's `PES2021.exe` is not a usable runtime image. Its `.rdata`/`.data` differ from the running game's in 970 ranges (61,114 eight-byte words in `[0x14252F000, 0x143D14000)`; `slicediff.py`), and a slice of the dump alone ([`.rdata`, `.impdata`)) also fails (7 of 24 captures differ: the chain reads bytes beside the code). So a match needs the memory dump (`PES_EXE`, `tools/pes21_import/README.md`) as a required input, mapped whole and copy-on-write. `LoadTrapSource` takes its path; `gfsim_run`, the gdext library and the test helper read `PES_EXE` (the helper leaves the trap source empty without it, so Match tests skip). The `Match` constructor refuses a setup without `pesTrapSource`.
- `Match`: `PesTrapExternalsOf()`, `GetPesActorConstants()`; `Match::Process` calls `WritePesTrapActors` right after `WritePesMovementRecords`. `WriteManMovement` now keeps the previous position (`man+0x568`) before it writes the new one (P+0x14 is a copy).
- `TrapFactsOf` (teamAIcontroller.cpp): facts from the player: `slot`, `side = slot/11`, height (`pesmovestep::PhysiqueHeightCmOf`, shared with `PesMovestepHost::PhysiqueG`), phase = `MoveStepRecord::state`, `P+0x277C/+0x2780` = MOVESTEP's carried words `req[0x1a]/[0x1b]`, motion = `CurrentMotionOf` (no-motion sentinel `0xBE5`), Speed byte (`pesmovestep::SpeedByteOf`, shared with the rate-ceiling call), stamina.
- `pesreceive.{hpp,cpp}`: `TrapRequestFor(FirstTouch, yaw)` (word 0 heading, word 1 touch kind 0/3, word 2 dispatcher kind 0/4, word 3 zero), `RetryTrapAsControl(kind, out0, plan35)` (kind 4, `out[0]` in {3, 7, 8}, plan byte clear). `pestrapworld.hpp`: `CommittedTrapClip(P)` = u16 at `P+0xC80`.
- `_TrapCommand`: request -> chain -> retry -> clip = `CommittedTrapClip`; the command then names the clip by family `kTrap` and motion number **only if the match loaded that clip** (`AnimCollection::HasSpecial`).
- `_ReceiveCandidate`: the designation gate (G5) applies off the PES path only; PES's entry 3 has none.

## 3. Named stand-ins (each `ponytail:` in code, with the PES site that removes it)

1. Action ids `P+0xAC8..0xACB` = MOVE (2) for all four (GF has no body action id; the six captures show 2 and 3 with the same phases): `TrapFactsOf`.
2. `P+0x2FC` ring: each of the six entries = the man record's present velocity (decoded entry = displacement over `10(k+1)` ticks / `10(k+1)`; drops PES's braking prediction; INF from two snapshots): `WritePesTrapActors`, `PesActorOpenInputs`.
3. SBn (table B squad record, `G+0x1978`): table B stays unbound, reads answer zero: `WritePesTrapActors`. FUN_140A3B320's writer removes it.
4. Leg-length slider 0, kick approach 0, `P+0x277C`'s writers as MOVESTEP carries them.
5. Kind 4: the dispatcher's kind-4 chain `0x14079B3D0` is not translated, so a planned touch is given refusal status 3 and takes PES's own retry as kind 0 at once (`_TrapCommand`, `kKindFourUntranslated`). Its `R[0] = 3`, `R[1] = 4` come from `cmd+0x5c`, which GF has no source for; the plan flag bytes (`R+0x34/36/37/38/3A`) stay 0.

## 4. Verification run

- `pes_trap_oracle_test` (PES21_ROOT, PES_EXE, 24 oracle dirs): **7 of 7** passed before the dump change (per-slot P, `LoadActorConstants` on the install exe); after it the new test `TheMatchsOwnMemoryCommitsPesClipOnTheIssuedCaptures` **passes** (match memory = private mapping + loader: all 24 calls equal the flat-image run, seq4469 commits `0xB86`, seq23906 `0xA41`). The full 8 were not rerun after the final change.
- `pes_receive_test` 49 of 49 (4 picker/install tests deleted, 4 request/retry/clip-word tests added).
- `pes_stall_test`: aborts (section 1). The red-first run of the re-enabled live-ball case on the pre-change tree was lost (the binary was rebuilt before it ran); the inherited baseline is 147 live firings (TrapOpen2/TrapClipPort reports), not re-measured.
- NOT run: fast-tier interim line (every `pes_*` binary linking the changed files, `gen_translated.py --check`; no translated file was touched), 2-minute smokes on both paths, the other `pes_*` binaries that include `pesworldrecord.hpp` (layout change: check `ninja -C build -t deps` for a `#deps 0` object before relying on a partial rebuild).
- **Mutations:** none run (budget). Planned, per branch: request heading word 0 swapped with word 1 (`pes_receive_test`); `kind != kTrapKindDirectional` flipped in `_TrapCommand` (needs an in-match test: item 1); `CommittedTrapClip` offset `+8` -> `+0xA` (`TheCommittedClipIsTheActionObjectsWordAtPlusEight`, and the oracle clip test); `WritePesTrapActors` slot index (an in-match test of P's fields vs the man record).

## 5. Open items (numbered)

1. Slot-record checked cells (section 1): write tank cur / max / breath in PES's checked-int layout; then run `pes_stall_test` (red-first evidence, mutation list above), the PES-path 2-minute smoke twice (hash equal) and the GF-path smoke.
2. **The chain's committed clip may not be playable:** `pesclips.cpp`'s trap family loads only `gkcatch_*` clips, so a committed motion (`0xB86` `quick_restart_step90_near`, `0xA41` `kick_short_0_0_inside_y0_f045`, any Animetable cell) is refused by `HasSpecial` and no trap command is issued. Playing it needs either a load of the Animetable cells the chain can commit (names from the motion-name table) or the action's own tick (S5, `FUN_140797CD0`, with PES's touch frame). This is the likely reason the stall test cannot go green by item 1 alone.
3. `trapRows` (`ReceiveConstants`) and the `kTrap` gkcatch prefixes in `pesclips.cpp` now serve only clip loading (`pes_clips_test`, `pes_restart_delay_test` read `trapRows`); delete them with the row-picker clip family once item 2 settles what the trap family loads.
4. `PES_EXE` is now required to build a `Match` that traps (gfsim_run refuses without it; gdext's library load fails without it). Main's gate already exports it; document it in `AGENTS.md`/the gdext launch scripts.
5. The previous-position store in `WriteManMovement` also runs in GF mode (once per run build, so prev == pos there); harmless, but note it if a GF-path hash moves.
6. `P+0x3AC0`'s ctx side (`CtxVa(side)`) and the MA cells are not proven by the 24 captures (TrapOpen2 item 4).
