# pes-port-Presentation: HUD clock minute from the translated HMS (FUN_140A6BD80), B2 round 1

Branch `Presentation`, pushed `e835e2cb` (`git ls-remote origin Presentation` = `e835e2cb69d76a11ac4826181585841764e2ac30`). Base: origin/main `b3c24920` (the origin/Presentation branch was already an ancestor of main: its translated layer had landed). Worktree `<workdrive>/gf-Presentation` (detached, from origin/main), scratch `<workdrive>/gf-Presentation-scratch`.
Labels: CONFIRMED = read in tool output, a live read, a probe run, or the Ghidra decompile. INF = inferred, reason given. OPEN = blocked, blocker named.

## 0. Summary

- Subsystem: **HUD clock** (scorebug clock minute). Chosen over replay, audio and crowd: its helpers are translated and bit-exact, while replay/audio/crowd entry points are not in the layer (section 2).
- Ported: GF's `ClockDisplay` minute now comes from the translated `pes_fn_140A6BD80` run on PES-layout memory (`pes::SplitHms`). The GF arithmetic and a dead `>= 100` wrap are deleted (clean cutover).
- **Not visible in the game yet.** `PesClock::Display`/`ClockDisplay` has no caller in the game. No frame is claimed for this change. A readout needs a new godot node and a match.gd hook (Main), see section 6.
- The HUD clock builder `0x1420EFEC0` is NOT ported. Its outputs disagree with the HMS slot reading (section 3), so the displayed-second source is OPEN.
- Tests: `pes_clock_test` 43 passed / 9 skipped without an install; 52 passed with `PES21_ROOT=<workdrive>/PES21`. Red-first and three mutations run (section 5).

## 1. Repo changes (commit e835e2cb)

| file | change |
|---|---|
| `src/pes/hud_clock.hpp`, `hud_clock.cpp` (new) | `pes::HmsSplit {hours, minute}`, `pes::SplitHms(float)`. Runs `pes_fn_140A6BD80(PesState&)`: float in xmm0, rdx = hours slot, r8 = minute slot, both in one `PesMem` host window at PES VA `0x20000000` (`+4` for minute). |
| `src/sim/onthepitch/pesclock.cpp` | `ClockDisplay`: minute = `SplitHms(...).minute`; second = `int(total) % 60` with a `ponytail:` comment naming the open second source. |
| `src/sim/onthepitch/pesclock.hpp` | Deleted `kDisplayMinuteWrap` (only use was the dead wrap). |
| `CMakeLists.txt` | `src/pes/hud_clock.cpp` added to `pesfmt` (which links `pestranslated`). |
| `tests/pes_clock_test.cpp` | Include; comment on the 60:00 test corrected (minute is mod 60, no wrap); new `HmsSplitsHoursAndMinutesFromASecondsCount` (3599 s, 3600 s, 5400 s). |

Commit path: the `commit` namespace failed on the worktree's `extern/godot-cpp` symlink (git: "expected submodule path ... not to be a symbolic link"). I committed with path-limited git (`-c status.submodule=none`, explicit paths, brief identity). The worktree is private to this round.

## 2. Decode gaps and coverage (HUD clock chain)

| VA | what | status | evidence |
|---|---|---|---|
| `0x140A6BD80` | HMS split (`hours`, `(s/60)%60`) | **translated, bit-exact** (64 cases, own-block 1.00) | `tools/dev/translate/results_presentation.json` (`va` 0x140A6BD80, diffed true); layer `src/pes/translated/hud/pes_fn_140A6BD80.cpp` |
| `0x1420BDFD0`, `0x1420BDF50`, `0x1420BE050`, `0x1420BE140`, `0x1420BE780` | half-length, minute offset, live-half test | translated, bit-exact (results rows, diffed true) | ledger rows 10983–10988 |
| `0x1420CE3D0`, `0x1420CE460` | builder callees (scoreboard attention) | translated, bit-exact | ledger rows 10991–10992 |
| `0x1420CD6C0` | builder callee (attention test) | **undecoded** in ledger; a results row exists, not in layer | ledger row 10990; results (1 row, not in layer) |
| `0x1420EFA10` | builder callee (103 instructions) | **undecoded** | ledger row 10998 |
| `0x1420EFEC0` | HUD clock-state builder (301 instructions) | **decoded only**, not translated | Ghidra decompile (live, CONFIRMED), `pes-re-clock-record.md` 2.2 |
| `0x1420F1750` | clock digit renderer | decoded, not translated; writes AFP sprite objects | `pes-re-hud-ui.md` 3.1 |
| `0x1420F0EB0` | scorebug update (per frame) | not in layer | results: no row |
| `0x14217DEF0` | ReplayHuman frame read | not in layer, not in presentation results | grep of results and layer: 0 |
| `0x1414406A0` | chant config loader | not in layer, not in results | grep: 0 |
| `0x14048FDAE` | ball-hit tier picker (not hookable, mid-function) | not in layer | grep: 0 |

Builder dependency that blocks a full port (CONFIRMED from the decompile): the tail calls `(**(code **)(*param_1 + 0x48))(param_1)` (vtable slot 9 on the builder's first argument, not identified) and reads the global `DAT_1437f968a` (replay flag).

## 3. The clock slot question (OPEN)

Facts (CONFIRMED):
- Translated `0x140A6BD80`, run by a probe (`<workdrive>/gf-Presentation-scratch/probe/probe.cpp`) on PES-layout memory:

  | seconds | rdx result (hours) | r8 result (minute) |
  |---|---|---|
  | 0, 59 | 0 | 0, 0 |
  | 60 | 0 | 1 |
  | 2699 | 0 | 44 |
  | 2700 | 0 | 45 |
  | 3599 | 0 | 59 |
  | 3600 | 1 | 0 |
  | 5400 | 1 | 30 |

- Decompile of `0x1420EFEC0`, live-half branch (`E+0xa4 == 0`): `FUN_140a6bd80(*(f32*)(E+0xa0), &local_88, &iStack_84)`, then `param_2[3] = local_80 + local_88` (the displayed minute) and `param_2[4] = ... + iStack_84`. So the builder reads the **rdx result (hours)** as the minute and the **r8 result (minute)** as the second. The other branch does the same with `E+0x11c`.
- Read-only live sample (pid 2024691, 10:19:23–10:19:50): `E = *(G+0x2C0)`, `G = q(0x1436F3DC8)` (CONFIRMED: E+0x58 = 10.0, the match-minutes setting, as in `pes-re-clock-record.md`). `E+0xa4 = 2` (so the `E+0x11c` branch), `E+0xa0 = 0.0`, `E+0x11c = 0.0`. The frame counter `MI+0x1198` advanced 2300 → 2378.
- Screenshot at 10:19:38 and 10:19:44 (`<workdrive>/gf-Presentation-scratch/clock/clkboth.png`): the on-screen clock reads **5:20**, unchanged across six seconds, score 0–0. So in this state the displayed clock is neither `E+0xa0` nor `E+0x11c`.

Inference (INF): either the displayed clock comes from another field/object, or the builder's `E` is not the object the HUD reads. The hours-as-minute reading of the decompile cannot be right for a 45-minute display, so the decompile's slot naming is unresolved. A capture of the builder during live play (E+0xa4 == 0, in-play) and the matching frame would settle it (section 6). Until then the minute port (r8 = `(s/60)%60`) is CONFIRMED by execution; the seconds source is OPEN.

## 4. Captures (live2/pid2024691, read only)

- `seq001`: `chant_load_1414406A0`, 1 call, paused 8.45 s (`watch.log` 10:14:41). Read but not usable yet: the chant loader is not translated.
- `seq002`: `scorebug_1420F0EB0`, 1 call, paused 24.1 s (10:15:52). Its snapshot holds the same `E` (E+0x58 = 10.0, E+0xa4 = 2, E+0xa0 = 0.0) and MI (period 1, in-play 1, frame 256). It is the scorebug's own state at kick-off, not a live-play clock.
- No replay capture yet (Main: replay needs a replay, likely after a goal).
- The snapshot reader I used is scratch (`<workdrive>/gf-Presentation-scratch/clock/snapread.py`, `livepread.py`, `scan.py`), not committed.

## 5. Verification

- **Red first (CONFIRMED):** before the port, `tests/pes_clock_test.cpp` failed to compile on origin/main: `fatal error: pes/hud_clock.hpp: No such file or directory` (ninja, object `pes_clock_test.cpp.o`).
- **Green:** `build/tests/pes_clock_test`: 43 passed, 9 skipped (install-dependent; no PES21_ROOT in the shell). With `PES21_ROOT=<workdrive>/PES21`: **52 passed**, 0 failed.
- **Mutations** (each applied with the edit tool, rebuilt, run, reverted; tree then equals the commit, `git status` clean in `src`, `tests`, `CMakeLists.txt`):
  - M1: HMS result slots swapped (minute read from `out`, hours from `out+4`): FAILED `HmsSplitsHoursAndMinutesFromASecondsCount`, `DisplayMinuteAndSecondFromASecondsCount`, `DisplayWrapsTheMinuteAtSixty` (3 of 3 relevant).
  - M2: minute slot = hours slot (`kMinuteVa = kHoursVa`): FAILED the same 3.
  - M3: `ClockDisplay` minute read from `.hours`: FAILED `DisplayMinuteAndSecondFromASecondsCount`, `DisplayWrapsTheMinuteAtSixty` (2).
- **Gate:** `GATE_TIER=branch tools/dev/gate.sh` launched in the background (log `<workdrive>/gf-Presentation-scratch/gate_branch.log`, 2400 s timeout). **Result pending at the time of writing** (not observed to finish). Main should gate the merge.
- **PII:** grep of the changed files for machine paths: no hits. `tools/check_pii.sh` not run (worktree symlink).
- **Visual:** none claimed. The new clock is not drawn anywhere. Frames taken for context only (section 3).

## 6. Hook and wiring requests for Main

1. **Clock-state builder** `0x1420EFEC0` (the open slot question): entry hook, `--when 'b(q(q(0x1436F3DC8)+0x2C0)+0xA4)==0 and b(q(q(0x1436F3DC8)+0x270)+0x116C)==1'` (live half, in play), EVERY 1000, cap 6, FULL. The output struct is the builder's `param_2`, so a replay gives PES's outputs. Pair it with a frame at the same time.
2. Match.gd hook: a readout node for the clock (new file `godot/match/hud_clock_readout.gd` once the slot question is settled). Needs one hook point in match.gd (Main to add or approve).

## 7. Next subsystem plan

- **Round 2 (HUD clock, builder slice):** (a) translate `0x1420CD6C0` and `0x1420EFA10` (undecoded; synthetic-case pipeline, no capture needed), (b) name the object behind vtable slot `+0x48` on the builder's first argument, (c) when capture 1 lands, settle the minute/second slot against a frame, (d) then translate `0x1420EFEC0` on GF-filled E state (E+0x58, E+0x9E/E+0xB0 limits, E+0xA0/E+0x11C floats, E+0xA4, E+0x114/E+0x118, E+0x2DA8) and retire `ClockDisplay` in favour of the builder output.
- **Alternative, bigger:** replay (ReplayHuman `0x14217DEF0`). Its frame layout is decoded (`pes-re-cutscenes.md` section 6: 0x264 header + 25 × 300-byte human records). It needs a replay capture (Main) and a GF recorder that does not exist (INF: a new feature, not a port).
- Crowd and audio: no entry point is live-located yet (`pes-port-PresReplay.md` open items 4); audio tier picker is not hookable.

## 8. Open items

1. Displayed clock source (section 3): OPEN, needs capture 1 in section 6.
2. Seconds: GF's `total % 60` stays as a `ponytail:` until (1) is settled.
3. `0x1420CD6C0`, `0x1420EFA10`, vtable slot `+0x48` (section 2): OPEN.
4. Gate `GATE_TIER=branch`: pending (section 5).
5. No visual proof for any change in this round.

# pes-port-Presentation round 2: clock-chain callees translated, builder slice open (18-11-26)

Branch `Presentation` (round 1 head `e835e2cb`, pushed). Round 2 adds the tooling and record changes below. Labels: CONFIRMED = read in tool output or probe; INF = inferred; OPEN = blocked.

## 0. Summary

- **`0x1420CD6C0` (HUD clock-chain accessor): translated and bit-exact**, 64 of 64 cases, own-block coverage 1.00 (driver output, CONFIRMED).
- **`0x1420EFA10` (builder callee, matrix/projection): translated and bit-exact**, 64 of 64 cases, own-block 1.00, provisional: it reads three image globals at their on-disk values (CONFIRMED from the driver's provisional note).
- **Builder `0x1420EFEC0`: NOT translated.** Its spec cannot run: its controller pointer chain is not modelled by the pilot spec generator, and it makes an indirect call through the controller's vtable slot `+0x48` whose target is not yet identified (section 3).
- **Round 1 gate (`GATE_TIER=branch`, log `<workdrive>/gf-Presentation-scratch/gate_branch.log`): still running** at the time of writing. Its ctest phase had not finished. No GREEN/RED line exists yet, so no gate result is claimed.
- **Replay captures:** four real calls of `0x14217DEF0` landed (seq205 to seq208 in `live2/pid2024691`, all one call each). Their first qword is `0x142BF7D58`, the ReplayHuman vtable (matches `pes-re-cutscenes.md` section 6). `0x14217DEF0` is not in the translated layer; its closure is 12 functions (section 4).

## 1. Repo changes

| file | change |
|---|---|
| `tools/dev/diffharness/run.py` | New input kind `const`: `case[name] = int(rule["value"], 16)` (a fixed value, never drawn). Needed to bind a stack-passed pointer argument to a region. |
| `tools/dev/diffharness/specs/presentation/1420CD6C0.json` | Hand fix (provenance: the generated spec drew the 5th and 6th arguments, which are stack-passed pointers, as random u8 scalars, so every case wrote through a small address; 69 of 69 cases failed "unmodelled write"). Regions `A4` (`0x20A00000`) and `A5` (`0x20A01000`) added; `stk0`/`stk1` are `const` inputs at those bases; outputs `w_A4_0`, `w_A5_0` added. After the fix: 69 valid, 0 failed (run.py). |
| `tools/dev/diffharness/specs/presentation/1420EFA10.json`, `1420EFEC0.json`, `140A6BD60.json`, `140A91270.json` | New, generated by `closure.py --spec-set presentation --tag clockbuild`. Closure members: 15 from 3 roots, 0 new universe rows, 11 committed specs kept. |
| `tools/dev/diffharness/specgen/sets/closure-clockbuild.txt` | New set file (closure output). |
| `tools/dev/ledger/universe.json` | Modified by closure.py (universe rows for the closure). |
| `tools/dev/translate/results_presentation.json` | Two records appended/replaced from a driver run: `0x1420CD6C0` and `0x1420EFA10` (diffed true). Merged with jq; the other 638 rows untouched. |
| `src/pes/translated/hud/pes_fn_1420CD6C0.cpp`, `pes_fn_1420EFA10.cpp` | Generated by `gen_translated.py`. |

Round-1 files (`src/pes/hud_clock.*`, `pesclock.*`, `pes_clock_test.cpp`) are unchanged in round 2.

## 2. The two translated functions

| VA | role | cases | own-block | proof class | what it does (CONFIRMED from the Ghidra decompile) |
|---|---|---|---|---|---|
| `0x1420CD6C0` | callee of the builder's attention test | 64/64 bit-exact | 1.00 | diffed, not provisional | `if (idx < 0x11)`: copies four u32 from `this+0x440+idx*8`, `+0x444+idx*8`, `+0x4C8+idx*8`, `+0x4CC+idx*8` to four out pointers; otherwise writes four zeros. |
| `0x1420EFA10` | callee of the builder (103+ instructions) | 64/64 bit-exact | 1.00 | diffed, provisional | Reads `FUN_140A91270(this->+0x38 + 0xB54)` into a local 4x4 matrix, multiplies a 4-vector by it, divides by w (`divps`), calls `FUN_14149A470` and writes two f32 scaled by `DAT_142565600` divided by two floats of the record at `this->+0x38` (`+0xCF4`, `+0xCF8`). Provisional: `fRam000000014255269c`, `DAT_14256AE1C`, `DAT_142565600` are read as on-disk image values. |

Provisional means the translated code uses the image's on-disk values for three globals. Whether PES's live values differ is OPEN; the ledger's convention for such globals applies.

## 3. The builder `0x1420EFEC0` (OPEN, round-2 remainder)

Runs: `pes-spec` closure produced `specs/presentation/1420EFEC0.json`. The harness fails every case: `unmodelled read at 0x102 (size 8)` and similar. Cause (CONFIRMED from the decompile): the builder's first argument `param_1` is a controller object; it reads `param_1[7]` (`+0x38`, the match-env pointer `E`), `param_1[6]` (`+0x30`, the attention record object passed to `0x1420CD6C0`), and `[param_1]` (vtable). The pilot spec generator does not follow these pointer chains (the known struct-map gap, `pes-port-Presentation.md` round 1, `specgen` class "pointer origin through add/sub").

What a hand spec needs (INF until a capture confirms each field): region `C` for the controller with `+0x30` and `+0x38` pointing at regions `R` and `E`; `E` fields used: `+0x9E/+0x9F` (yellow limits), `+0xA0` (live-half float), `+0xA4` (live-half flag), `+0xA5`, `+0xA8`, `+0xAC`, `+0xB0/+0xB1` (red limits), `+0xB9`, `+0xBA`, `+0x114`, `+0x118`, `+0x11C`, `+0x1B0` (23 records of 0x20 bytes), `+0xAC4`, `+0x2DA8`; globals `0x1436F3DC8` (G), `DAT_1437F968A` (replay flag), `G+0x270` (MI) through the phase getter.

Virtual call: `(**(code **)(*param_1 + 0x48))(param_1)` returns an int that is passed to `0x1420CD6C0` as the attention index. The vtable slot identity is OPEN. Method to identify it: read `[param_1]` from the scorebug capture `live2/pid2024691/seq002` (the scorebug `0x1420F0EB0` calls the builder through `0x1420F10D0`, so the controller is the scorebug's `this`), then the slot at `+0x48`. Not done in this round.

Builder outputs are a 17-u32 struct (`param_2`). The clock-slot question (`pes-port-Presentation.md` section 3) is answered by a live-play capture of this builder, which Main has armed as `clockbuild_1420EFEC0`.

## 4. Replay frame `0x14217DEF0` (captures landed, port not started)

- Captures: `live2/pid2024691/seq205`, `seq206`, `seq207`, `seq208` (one call each, pauses 17 to 23 s; all `full`, not yet unpacked by me). `call.bin` qword 0 = `0x142BF7D58` (ReplayHuman vtable), qword 1 = `0x37774EA0`-range pointer (the `this` reaches the human record), qword 2 = `0x31DF39C0`-range, `r8`/index dword `0xAE` in one record.
- Closure (`closure.py --dry-run`, tag `replayframe`): 12 members: `0x14217DEF0` (seed, replay), `0x1408BDF70`, `0x1420B6A30`, `0x14217DE20` (replay callees), `0x140227930`, `0x1403FB710`, `0x1403FB730`, `0x141EB6FF0`, `0x1403FB2E0`, `0x141978300` (cutscene callees), `0x1403FB5B0`, `0x1403FB4F0` (HUD callees).
- None of the 12 is in the layer for `0x14217DEF0`. The next step is a spec for `0x14217DEF0` (its record layout is in `pes-re-cutscenes.md` section 6: `frame + i*300 + 0x30c`, palette byte `+0x36B`), then the replay proof on the four captures with `snapdiff`. Not started in this round.

## 5. Verification so far

- Harness: `run.py` on `1420CD6C0.json` (69 valid, 0 failed) and `1420EFA10.json` (64 valid, 0 failed, icount max 139).
- Driver (`tools/dev/translate/driver.py --only 1420CD6C0 1420EFA10`): translated 2, diffed 2, bit-exact on 64 cases each (`<workdrive>/gf-Presentation-scratch/drv/run.log`).
- Layer: `gen_translated.py --image` regenerated the layer (files dated 10:33). `gen_translated.py --check` (drift check) was running at the time of writing; its result is pending. Do not count the layer as checked until that line says it passed.
- Gate: `GATE_TIER=branch` from round 1 still running (log above). Not counted.
- Mutations for the round-2 functions: none run. The translated functions are covered by the recorded diff (bit-exact against PES's own bytes), not by GF-side tests; the GF builder port (section 3) is where a mutation test applies.
- PII grep: run on the changed Python and JSON files before the commit (pending).

## 6. Open items

1. `gen_translated.py --check` result (pending, section 5).
2. Round-1 gate result (pending, section 5).
3. Builder: hand spec with the pointer chain (section 3); identify vtable slot `+0x48` from `seq002`.
4. Clock slot: settled by the armed `clockbuild_1420EFEC0` capture (Main).
5. `0x1420EFA10` provisional globals: check whether PES's live values differ (ledger convention).
6. Replay: spec for `0x14217DEF0`, then snapdiff on `seq205`–`seq208` (section 4).
7. Hook and wiring: the GF-side readout still needs a match.gd hook point (Main).

# pes-port-Presentation round 3: HUD clock builder translated and run on PES-layout state; replay not proven (18-11-26)

Branch `Presentation`: `b28f8504` (translation: spec, record, layer), `e4508380` (GF port and test). Both pushed; `git ls-remote origin Presentation` = `e4508380`. Labels: CONFIRMED = read in tool output, a live read, or a probe; INF = inference; OPEN = blocked.

## 0. Summary

- **Builder `0x1420EFEC0` translated, bit-exact on 104 cases** (own-block coverage 0.75). It is run from the translated layer on PES-layout state by `pes::ClockStateBuilder` (`src/pes/hud_clock_build.*`). Tests: 4 of 4 pass with the install (`tests/pes_hud_clock_test.cpp`).
- **Slot +0x48 identified (CONFIRMED):** the controller's vtable slot 9 is `FUN_1420F04B0`, which returns the constant 6. Its only caller is `FUN_1420F10D0` (unconditional, decompile xref), which passes the scorebug `this` as the builder's `param_1`. Read from the live process (read-only pread): the scorebug object's vtable is `0x142BE70B8` (the same as `rax` in `seq002`'s call record), slot 9 at `+0x48` = `0x1420F04B0`.
- **Replay `0x14217DEF0`: NOT proven.** Spec not written; the four captures are packed (one decompressed into scratch). Details and the next steps are in section 4.
- **Drift check (`gen_translated --check`):** the round-2 run finished `rc=0` (5654 functions byte-identical) on the round-2 layer. The current layer (5656 functions) has a new check running now (log `<workdrive>/gf-Presentation-scratch/gen_check3.log`, last line `gen_check rc=`). Reported at the end of this message.
- **Round-1 gate (`GATE_TIER=branch`)** is not reported: its log never reached a verdict, and its tree changed mid-run (round 2 regenerated the layer). Treat as not run.
- **Visual:** none. The clock is still not drawn; the port is not wired to a node.

## 1. Repo changes (round 3)

| commit | files | change |
|---|---|---|
| `b28f8504` | `tools/dev/diffharness/run.py` | Stop the guided-case solver crashing on rip-relative memory keys (`mk[3]` on a 2-tuple; such keys never match a region input, so `None` is the right answer). Unmodelled-access messages name the faulting RIP. |
| | `specs/presentation/1420EFEC0.json` | Hand spec for the builder (replaces the closure's pilot spec, which could not run). Regions C, X, E, GO, M, V, O, DS; `patch_addrs` for G (`0x1436F3DC8`) and DS (`0x143704E38`); inputs for the fields the builder branches on. Runs: 122 valid, 0 failed. |
| | `specs/presentation/1420F04B0.json`, `specgen/sets/closure-clockslot.txt`, `tools/dev/ledger/universe.json` | Closure output for the slot-9 target. |
| | `tools/dev/translate/results_presentation.json` | Two records (1420EFEC0 bit-exact 104 cases, coverage 0.75; 1420F04B0 bit-exact 64 cases, coverage 1.00). Original formatting kept. |
| | `src/pes/translated/hud/pes_fn_1420EFEC0.cpp`, `in_match_menu/pes_fn_1420F04B0.cpp`, `pes_dispatch_gen.cpp`, `translated.hpp` | Generated. `gen_translated.py` wrote 5656 functions (3762 recorded bit-exact, 1894 closure; was 3760). |
| `e4508380` | `src/pes/hud_clock_build.{hpp,cpp}` (new), `tests/pes_hud_clock_test.cpp` (new), `CMakeLists.txt`, `tests/CMakeLists.txt` | GF port and tests (section 2). |

Scratch (not committed): `<workdrive>/gf-Presentation-scratch/` (drivers' logs, the clock probes, `replay/seq205/mem.bin`, 3.9 GB, to delete after the replay proof).

## 2. The builder port

`pes::ClockStateBuilder::Create(PeImage)` reads the install's `.rdata` (`0x14252F000`, 0xE6C000) and `.data` (`0x14339B000`, 0x72A000), and keeps them as PES image runs. `Build(ClockEnvInputs)` zeroes eight host regions at PES VAs (`0x20000000`..`0x20070000`, outside the image and the stack), writes the controller (vtable, attention, E), the vtable slot 9 (`0x1420F04B0`), G (MI, E), MI (period `+0x1168`, in-play `+0x116C`), E (`+0x9E/9F`, `+0xA0`, `+0xA4`, `+0xB0/B1`, `+0x114`, `+0x11C`, `+0x2DA8`), and patches G, DS and the replay flag in the `.data` run. It enters `pes_fn_1420EFEC0` as the dispatch does (return address at `[rsp]`, `rcx` = C, `rdx` = O) and returns the 17 output words.

Tests (CONFIRMED with `PES21_ROOT=<workdrive>/PES21`, 4 of 4 pass):

| test | input | asserted |
|---|---|---|
| `LiveHalfMinuteIsTheMinuteOfTheSecondsCount` | live 1350 s, period 1 | out[4] (minute) = 22, out[3] (hours) = 0 |
| `LiveHalfHoursFromTheSecondsCount` | live 3600 s | out[3] = 1, out[4] = 0 |
| `FrozenPathReadsItsOwnFloat` | flag 1, frozen 2700 s, live 100 s | out[4] = 45 (the live float does not reach this path) |
| `DigitVariantFollowsTheHoursWord` | live 1350 s | out[5] = 2 (`1 + (out[3] < 10)`, decompile) |

Expectations come from the PES code's structure (decompile of `0x1420EFEC0`: the live branch writes the HMS hours into the hours word and the minute into the minute word; `FUN_1420BE140` adds 0 for period 1, decompile). They are not copied from GF's output.

Mutations (each applied with the edit tool, built, run, reverted; tree restored):
- M1: live seconds written to `E+0x11C` (the frozen slot): `LiveHalfMinuteIsTheMinuteOfTheSecondsCount`, `LiveHalfHoursFromTheSecondsCount` FAILED (2 of 4).
- M2: live flag forced to 0: `FrozenPathReadsItsOwnFloat` FAILED (1 of 4).
- After revert: `pes_hud_clock_test` 4 passed; `pes_clock_test` 52 passed.

Clock slot (CONFIRMED vs OPEN): the decompile names the hours word as the displayed minute. A 45-minute display cannot come from that reading, so the naming is wrong or the displayed minute comes from another word. The test pins the words the translated code writes, not which one the HUD shows. Settling it needs the armed capture `clockbuild_1420EFEC0` (Main, next PES launch).

Not wired to any node. The port has no visible effect.

## 3. Hand spec: what it varies and what it does not

- Varies: MI period (0..10) and in-play (0..3); the E fields listed above (bytes 0..255 or 0..1; floats -10..200); DS `+0x94/+0x98`; the replay flag.
- Fixed: the controller pointers, the vtable slot, G's MI and E pointers, DS's pointer. The attention table (`X+0x440..`) and the E table at `+0x1B0` are zero (the first table entry is varied: `E+0x1B8`, `E+0x1BD`). These are the coverage gaps (own-block 0.75).
- Per owner ruling, coverage is recorded, not gated: a case set is proven when every case is bit-exact.

## 4. Replay `0x14217DEF0`: NOT proven (state for the next round)

What is confirmed:
- Captures `live2/pid2024691/seq205`–`seq208`: one call each. `call.bin` is `CALL_REC` (`tools/dev/translate/frame_trace.py`: 16 GPRs in `rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8..r15` order, then 16 XMM lo/hi, then the return address). `rax` = `0x142BF7D58` (the ReplayHuman vtable, matches `pes-re-cutscenes.md` section 6), `rcx` (`this`) = `0x37774EA0` in seq205.
- The decompile (`FUN_14217def0`, CONFIRMED): `param_1` = ReplayHuman (`+0x18` flag, `+0x28` human index, copies the record `+0x50..+0xD0`), `param_2` = a unit list (`+0x10` frame pointer; `DAT_1436F5118 + 0x6A8` holds the replay manager), `param_3` = a `ushort[2]` (unit index, frame index). It calls `FUN_1420B6A30`, `FUN_1408BDF70` (a compare/copy), and reads a palette byte at `+0x36B`.
- Closure (`closure.py --dry-run --tag replayframe`): 12 functions. None is in the layer for `0x14217DEF0`.
- seq205 is decompressed to `replay/seq205/` in scratch (`zstd -d`, 5 s). seq206–208 are still packed.
- `capz.py unpack` in place refused: `PermissionError` creating `.capz.lock` in the capture directory (CONFIRMED, two captures). Scratch decompression is the workaround.

What is not done:
- No spec for `0x14217DEF0` or its 12 closure members; no `snapdiff` proof (`snapdiff.py --hooks <cfg> --snapshot SNAP=TRACE`, with `frame_trace.py recorded` producing the trace; the hook line is `hook replay_frame_14217DEF0 0x14217def0 15 1 1 …` in `<workdrive>/PES21/rendezvous/hooks.cfg`).
- The replay's inputs include a global manager and a unit list with pointer chains (the same class as the builder's), so a hand spec is needed, as for the builder. That is the next step.

## 5. Gaps and what is OPEN

1. Clock slot (hours word vs minute word): OPEN until `clockbuild_1420EFEC0` lands.
2. `0x1420EFA10`'s provisional globals (on-disk values): OPEN (ledger convention).
3. Builder coverage 0.75 (attention table and E table are zero): not gated.
4. Replay: spec and proof (section 4).
5. `gen_translated --check` on the current layer: pending at the time of writing (section 0).
6. GF wiring: none. A readout node and a match.gd hook point are needed (Main).
7. `EngineFixture` (branch `aebe3181`): the commit on that ref is the capsets tooling, not the fixture; the fixture was not used.
