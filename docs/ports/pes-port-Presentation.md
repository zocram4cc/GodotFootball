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
