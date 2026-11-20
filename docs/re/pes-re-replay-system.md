# PES 2021 replay system: recorder, ring buffers, playback, and what starts a replay (static decode, round 3)

Labels: CONFIRMED = a headless decompile (`dec.sh`), an E8/E9 or absolute-pointer scan of the flat image `pes_full.bin` (file offset = VA − 0x140000000), an RTTI name read from the image, or a live capture; INF = inferred from names or structure; OPEN = not settled, with the probe that would settle it. Earlier layout facts that were OBS in `pes-re-cutscenes.md` §6 are repeated here only where this round confirmed a caller or a writer.

Scope: this decodes the recorder (what is written per tick), the ring it is written into, the playback (what the ReplayHuman frame restores), and the FixDemo demo chain that starts a replay. It does not decode the goal root that triggers the demo (OPEN, section 6).

## 1. Classes (CONFIRMED: RTTI names read from the image, vtable base from the `.rdata` slot)

| class (RTTI) | vtable base | role here |
|---|---|---|
| `match::Replay` | `0x142BD3E50` | the replay object |
| `match::ReplayHuman` | `0x142BF7D58` | the replayed human (frame reader: `0x14217DEF0` is its slot `+0x38`, at `0x142BF7D90`) |
| `match::DataManagerReplay` | `0x142654040` | owns the 21 units |
| `match::UnitReplay` | `0x142653F58` | one ring buffer (stride 0x50 in the manager) |
| `match::Human` | `0x142612FB8` (slots from `0x142613020`) | per-player object that owns the body/replay records |
| `match::Player` | `0x1426329A8` (slots from `0x142632A10`) | per-player match object |
| `mob::PropMain` | `0x142614DA0` (slot 6 = `0x142614DD0`) | the ball/prop object |
| `referee::RefereeLinesman` | `0x142642348` (slot 13 = `0x1426423B0`) | official; calls the same Human update |
| `referee::Referee` / `RefereeChief` | `0x142641E78` / `0x1426420F0` | officials |
| `game_mode::MatchListener` | `0x142BBA650` (slot 2 = `0x142BBA660`) | the match listener; `0x141FBF060` is its slot 2 |

## 2. Who writes the replay record each match tick (CONFIRMED callers; the tick root is OPEN)

Frame writers (the functions that fill a unit frame, 0x1FB0 bytes = 0x264 header + 25 × 300-byte human records):

| writer | what it writes (OBS layout, `pes-re-cutscenes.md` §6) | direct caller (CONFIRMED) | the caller's own caller |
|---|---|---|---|
| `0x140A89F20` | header (called from `140423098`) | `0x1404230D7` in `140423098` | indirect only |
| `0x140A88E00` | header/ball block (called from `140871E15`, `141F7E7C6`-side) | `0x140871F49` in `140871E15`; `0x141F830C9` in `141F83050` | `141F83050` ← `141F363DE` (in `141F36383`) |
| `0x140A89E00` | header tail | `0x1408D18D2` in `1408D1880` | `1408D1880` ← `1408CCBD0` (**PropMain slot 6**, `0x142614DD0`) |
| `0x140A88CE0` | ball block | `0x1408D08CE` in `1408D0814` | indirect only |
| `0x140A89030` | human record fields (+0xA8..+0xB4 family) | `0x1408BF5B7` in `1408BF135`; `0x141F7EA2C` in `141F7E7C6` | `1408BF135` indirect only |
| `0x140A893F0` | human record fields (+0x78 .. +0x110 family) | `0x1408BF82D` and `0x1408BF8C4` in `1408BF724` | `1408BF724` indirect only |
| `0x140A898D0` | human record motion fields | `0x1408BF8C4` in `1408BF724` | as above |

- `1408CCBD0` (PropMain slot 6) → `1408D1880` → `140A89E50`, `140A89EA0`, `140A89E00` writing the prop/ball fields at `prop+0x4CC/0x4D0`, `+0x4D8/0x4DC`, `+0x4E4/0x4E8` (CONFIRMED, decompile of `1408D1880`).
- `1408BF724` writes a human record from the anime/body accessors (`0x141EB8500..0x141EB8610`, `0x14195C420`) and then `140A893F0` (CONFIRMED, decompile).
- `1408BF135` is the per-player recorder (~1.2 KB). It reads the anime state through `*(P+0xAB8)` vtable `+0x28` (the anime state; `0x4A` is the replay action, from `movestep.hpp`), the motion ids through `1406B0ED0` / `1406AFBC0` / `1406AF830`, and the match clock `_DAT_142565D30`. It calls `1408BF5B7` → `140A89030` and `1408BF82D` → `140A893F0` (CONFIRMED decompile of the call sites; the field-by-field meaning of each argument is INF).
- `140871E15` writes the unit header through `140A88E00` with the unit's cursor and the Human at `param_1+100` (CONFIRMED call; INF on the field meaning).

Who calls the writers per tick:

- `Player` slot 13 (`0x140927450`, at `0x142632A10`) calls `1408BED00` (the Human update, CONFIRMED by the caller relation) and then `140922FF0` / `140921170` on `P+0x1A488`.
- `1408BED00` (Human slot 13, `0x142613020`) calls `1408BE3C0` only while the ReplayHuman record `P+0x938` is zero (create), then the Human's own update (`*(this+0x98)`), and stores the result at `this+0x94C` (CONFIRMED decompile).
- `RefereeLinesman` slot 13 (`0x140A38F00`) calls `1408BED00` too (CONFIRMED caller relation).
- `1408CCBD0` (PropMain slot 6) is the per-tick prop/ball writer above.
- The recorder is therefore driven by the per-tick virtual update of the players, the linesman, and the ball, not by a single match-tick function. **OPEN:** the single unit-cursor advance (the write cursor `+0x10` of the unit; wrap at 660). It is not in the direct-caller set of any writer above; it is probably in `140A8EE20` / `140A8ED70` (the unit's begin/end ops), which are referenced by the manager's vtable region only (`0x142653F18`, `0x142653F90`, `0x142653EE8`, `0x142653F60`). Probe: a hook on `140A8EE20` at EVERY 20 with `--when` on the unit cursor, or a write watch on the unit `+0x10`.

## 3. The ring (OBS, consistent with the captures; cursor advance OPEN)

- 21 units (`DataManagerReplay`). Unit fields (`pes-re-cutscenes.md` §6): `+0x10` u16 write cursor, `+0x12` capacity 660, `+0x14` total frames, `+0x20` appearance palette (count at `+0x7C`, 0xA0-byte entries from `+0x80`), `+0x40` frame array (660 × 0x1FB0), `+0x48` parallel 2000-byte-per-frame array (not decoded).
- Units 0 and 1 are the live rolling buffers (INF, medium); the others hold saved highlight or gallery clips.
- Copy between units: `140A8B6A0(mgr, dst, src, …)` (CONFIRMED caller relation only; gated by `param_2 < 2`).
- Live rate: about 57 frames/s at the 60 Hz game tick (OBS).

## 4. Playback: the ReplayHuman frame (CONFIRMED mechanics; the caller of the frame is INF)

- `14217DEF0` (translated on Presentation, `src/pes/replay_human.*`) reads a frame's record for its human index: `frame + i*300 + 0x30C` (the absent marker `0x7FFF`), then the palette byte at `+0x36B`, the 0xA0-byte palette entry, and copies it to the actor's `+0x50` (PresReplay21/22; CONFIRMED by the copy suite on 8 real captures).
- The actor record `R` is created per human by `141EB6840(…, 1)` in `1423E90E0` and by `141F7F480` (`+0x58`, arg 2) (CONFIRMED decompile).
- The pose is rebuilt from motion ids and times; no simulation runs (OBS).

## 5. The FixDemo demo chain that starts a replay (CONFIRMED chain; the trigger is OPEN)

Creation of a replay record for a demo:

- `1423E90E0` (CONFIRMED decompile) creates the 0x418-byte ReplayHuman record at `+0x28` through `141EB6840(…,1)` only when the demo event id satisfies `param_2 - 0x2C < 0xA2`, i.e. ids `0x2C..0xCD`. Its callers: `1423E9050` (← `1423E0298` in `1423E0170`) and `1423E9384` (no direct E8/E9 caller).
- `1423E9050` → `1423E90E0` after the check `141EC65E0`. `1423E0170` is the mode-1 path (its body calls `1423E9050`).
- `1423E0170` ← `141EDF57B` in `141EDF450` (CONFIRMED).

The demo dispatcher and its state machine (CONFIRMED decompile of `141EDE9F0`, `141EE03E0`):

- `141EDE9F0` is the per-tick update of one FixDemo demo object (`this+0x308` is the state; `this+0x31C` flags; `this+0x2F8` the current demo request). Its state handler: state 0 waits; state 1 calls `141EE03E0` (the demo's start/teardown tick: releases `this+0x348`, drives `this+0xD0`, `this+0x330`/`+0x336` flags); states 2..8 call `141EE0240`, `141EDF690`, `141EDFF10`, `141EDFA80`, `141EDF720`, **`141EDF450` (state 7)**, `141EDF5A0`; state 9 is the finished state. After a handler returns true the next state is `func_0x141EDF3B0(this)`.
- `141EDF450` (state 7, CONFIRMED decompile): if `this+0x370` is neither 0 nor 3, it sets flags and returns 1; otherwise it dispatches on `this+0x3C0`: 1 → `1423E0170` (the record-creating path above), 2 → `1423E0550`, else → `1423E0360`. The flags it clears/sets are `this+0x31C`/`+0x320`.
- So a replay record is created on the state-7 step of a demo whose mode is 1 and whose event id is in `0x2C..0xCD`.
- `1423E14D0` (called at the top of `141EDE9F0`) fetches the next demo request into `this+0x2F8` (CONFIRMED call; body not decoded).
- Who calls `141EDE9F0`: no direct E8/E9 and no absolute 8-byte pointer in the flat image (CONFIRMED scan). It is reached through a table or a register call. OPEN.
- `141EDEB55` (inside `141EDE9F0`) calls `141EDF450` directly; that is the state-7 call above, not a caller of `141EDE9F0`.

The FixDemo step machine, one level up (CONFIRMED caller):

- `141F59380` (the step machine, per docs `pes-re-cutscenes.md`/CutsceneTiming) is called from `141F59214` in the function `141F590E4..141F592AF` (CONFIRMED by pdata). `pes-port-CutsceneTiming*.md` names `141F59050` as the manager update; `141F590E4` is inside that manager's body or a sibling (INF).
- `14047C830` (DemoInfo state machine, docs) is called from `140446490` (function `140446390..140446514`) and `14044A259` (function `14044A180..14044A29C`) (CONFIRMED).

## 6. What starts a replay after a goal (OPEN; named probes)

Static facts:
- The replay record exists only for demo mode 1 with an event id in `0x2C..0xCD` (section 5). The goal category is 3 (`pes-re-cutscenes.md` §1: category ids; goal = 3), and goal scene ids are `GOAL_CELEBRATE_*` in `table_goal.bin`.
- `pes-re-foul-choreography.md` §1.5 lists Timing type `0xD` = `REPLAY_RECORD`, carried on slot 22 of a cut. That is the cut-level request for a replay record (CONFIRMED in the doc; the goal-cut instance is INF).
- GF's `goalsequence.cpp` has the replay timing only (`ReplayFiresAt_ms`, `kReplayPlayback_ms`) and no caller that starts PES's replay (CONFIRMED on main).

What is missing: the caller of `141EDE9F0` (the demo object's tick) and the goal-side call that queues a demo request with event id `0x2C..0xCD`. Probes, in order:
1. A hook on `1423E14D0` (the request fetch) at FIRST 1 EVERY 1 with `--when` on the request's category field = 3 (goal), cap 6, full capture. Its capture gives the request id (`param_2`) and the demo object. This names the goal root's event id.
2. A hook on `141EDE9F0` at EVERY 40 with `--when 'd(rcx+0x308)==7'` (state 7 only), cap 12. Its capture gives `this` and the caller's return address (the `--when ret==…` form, if the watcher allows the return address; `ret` is available per the watcher notes).
3. The data-table caller of `141EDE9F0`: a read watch on the FixDemo step table, or a hook on `141F59380` at FIRST 1 EVERY 1 with `--when 'd(rcx+0x1d8)==5'` (already staged as `fixdemo_step_machine_141F59380`).

## 7. The 141EB8E80 byte-scan spin (resolved as a harness-mode artifact; cause of the trace-mode divergence OPEN)

- The routine that appeared to spin is PES code at `0x14252E700..0x14252E7A0` (CONFIRMED disassembly, capstone). It is a name matcher: the outer loop `0x14252E72C..0x14252E748` walks the NUL-terminated name at `r8` (the name is a path, for example `common\anime\FoxAnim\Face\Animations\Base\angr_brwn…`, a stack buffer in the replay), and for each position calls the inner pattern matcher `0x14252E758` with a pattern in `rcx` (CONFIRMED by a read of the replayed memory at entry: the name is a real NUL-terminated string, terminated at outer iteration 69 for the first name).
- The run is therefore not a spin on bad data: 141EB8E80's name loop (`0x143527410..0x143527E50`, about 300 names, each `FUN_141B021A0` / `1414A8D60` / `141B00220`) matches the file names of the FoxAnim face directory. Total outer iterations are of the order of 33,000 before the first return.
- Without the trace layer the boot replay returns: `frame_trace.run_recorded(seq001, budget=4e8, models=load_import_models())` returns `err None` in 4.5 s (CONFIRMED). With the trace layer (`frame_trace.py recorded … --hooks … --out …`, which calls `run.install_hooks`) it does not return within 50M instructions, and it stops in the DLL's own code at `0x6F769F9A19AC` (a `ucrtbase.dll` loop, 171k steps per 2M). Bisecting by stub kind shows the same result with **zero** stubs installed (`kinds []`, `n_stubs 0`), so the divergence is in the trace layer itself, not in a `stubs.json` model. Cause: OPEN. The next probe: run the trace layer with `install_hooks` but without the impure set (`trace["impure"]`), and with the `_ret_sentinel` hook removed, to find which one makes the import resolve to the DLL's code.
- Consequence: the boot replay's result is not yet checked against PES's recorded post-state (the proof harness needs the trace layer). Nothing is translated or proven for `141EB8E80` / `1408BE3C0`. This is a named gap.

## 8. Recorder slice (what a port needs, from this decode)

To port the recorder bit-exactly, the translated set is: the per-tick entries `Player` slot 13 (`140927450`), `Human` update (`1408BED00`), `Human` record (`1408BE3C0`, the create), `1408BF135` / `1408BF724` (the per-player writers), `PropMain` slot 6 (`1408CCBD0`) and `1408D1880`/`1408D0814`, the header writers `140A89F20`, `140A88E00`, `140A89E00`, `140A88CE0`, the human writers `140A89030`, `140A893F0`, `140A898D0`, and the unit ops (cursor advance: OPEN). Each needs a capture of its call (the rig's hooks), a closure, and a proof, as in PresReplay and AnimeLayer rounds.

## 9. Open items (numbered; each names its probe)

1. Unit cursor advance and the unit ops `140A8EE20` / `140A8ED70` (section 2): hook at EVERY 20 with `--when` on the unit cursor.
2. Caller of `141EDE9F0` (section 5): a read watch or the `141F59380` hook (section 6, item 3).
3. The goal-side request with event id `0x2C..0xCD` (section 6, item 1): hook `1423E14D0` with a category predicate.
4. `1408BF135` and `1408D0814`: no direct, no absolute-pointer reference (the scans in this round); find the register-call site with a `lea reg,[rip+…]` scan, or hook them by their call from `1408BED00`'s update (INF).
5. Trace-layer divergence (section 7): the `install_hooks` / impure / sentinel bisect.
6. The `141B01500` (ResManager destructor) capture: teardown only (Main has the hook armed).
