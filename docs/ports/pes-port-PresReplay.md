# PES presentation: real-call replay of the presentation set (PresReplay, 26-10-26)

Read-only RE. No repo edits. PES pid 53052 was not touched (snapshots only). Scratch: `<workdrive>/gf-PresReplay-scratch/`.
Labels: CONFIRMED = read from the image, a snapshot, or a replay run; INF = inferred; OPEN = not settled.

## 0. Result

- Presentation set: 639 specs (`tools/dev/diffharness/specs/presentation`, = `results_presentation.json`).
- **Reach: 6 of 639** (0.9%). **PROVEN: 6 of 639**, all 6 reached functions replayed bit-exact.
- Replays: 42 (rax, xmm0, every written byte, against the translated function), **0 mismatches**.
- Own-block coverage over the reached set: 11 of 16 leaders (0.69). Per function below.
- Entries: only 2 presentation entries ran in a snapshot frame, and both stopped at frame 0 (section 2). **The coverage gap is the result, not a pass.** Crowd, audio-cue and replay-recording entries were not located live, so they were not traced.

## 1. Method

1. Entry objects, confirmed live in every snapshot with the vtable as the first qword (scan of `mem.bin`, chunked, 7 snapshots).
   - InplayCamera: vtable `0x142609C28` (CONFIRMED, image), slot 4 = `0x14087F280` = `InplayCamera::update` (CONFIRMED, image; doc `pes-re-camera-target.md`). Object at `0x34BE44E0` (snap1-142012, snap2-142339), `0x34BFC830` (snap3-142654), `0x33C77D30` (snap1-162213), `0x2BE8DCF0` (snap2-162511), `0x1BF6B4F0` (snap3-163043). None in snap4-163107. `[obj+8]` u32 = 5 (in-play mode) in every object found.
   - HUD widget: vtable `0x1427BC920` (CONFIRMED, image), object at `0x363650C0` (open play) and `0x2C136BA0` (stoppage) in all 7 snapshots. Slot 4 = `0x140E5FCF0`. **Whether slot 4 is the widget's per-frame update is OPEN** (not in the docs).
   - SeConfig: vtable `0x14294D300` live, but its slot 4 is null. Not an entry.
2. Emulation: `tools/dev/translate/frame_trace.py` (CallDelta worktree, read-only) machinery in `pres_trace.py` (scratch). Snapshot memory is lazy (LazyMap), TEB modelled, CallDelta records call-time deltas. Each entry runs once per frame with `rcx` = object, 50 frames requested. Every presentation spec entry is hooked (calls) and its own-block leaders counted.
3. Replay: `snapdiff.py` (CallDelta worktree), run over all 7 snapshots, with `--spec-dir specs/presentation`. The translated side is built from `xlate.py` output through the harness (`diffmain.gen_snap`). One function (140A57800) failed to link in the shared build dir; it was rebuilt in a clean dir (`build2/`), and the result is the one used here (section 5, item 1).

## 2. Entries: confirmed or not

| entry | va | object live | ran in a frame | outcome |
|---|---|---|---|---|
| InplayCamera::update (vt slot 4) | 0x14087F280 | 6 of 7 snapshots (CONFIRMED) | **stopped at frame 0** in all 6 (CONFIRMED by trace) | 7 calls before the stop, 6 presentation functions reached |
| HUD widget slot 4 (candidate) | 0x140E5FCF0 | 7 of 7 (CONFIRMED) | **stopped at frame 0** in all 7 (CONFIRMED by trace) | 0 calls. Slot identity OPEN |
| HUD radar update | 0x1420E2910 (doc `pes-re-hud-ui.md`) | not located | not traced | `this` object not found. OPEN |
| HUD scorebug / Time::update | 0x1420F0EB0 (doc) | not located | not traced | `this` object not found. OPEN |
| crowd (CrowdNoise / CrowdInstanceUpdateJob), audio cues (PitchSound / CrowdNoise controllers), replay recording | docs `pes-re-audio.md` | not located | not traced | OPEN |

Note: `InplayCamera::update` is not itself in the presentation set (not a spec); its callees in the set are what was measured.

### Camera stop (CONFIRMED by trace; cause OPEN)

- Fault: `0x1408A2169 movss dword ptr [rsi+0x30], xmm0` with `rsi = 0` (unmapped write at 0x30). Identical RIP and register state (rbp `0x1001FE49`) on every snapshot, so it is deterministic.
- The update (`0x14087F280`) saves the caller's rsi at its prologue (`mov [rsp+0x58], rsi`), uses rsi as a loop pointer over 0x28-byte records (`0x14087F320`..`0x14087F36F`), then restores the caller's rsi at `0x14087F37E`. The callee path that faults reads rsi as an argument. The entry's rsi is the caller context, which the trace does not capture.
- Experiment: rsi = rdi = `this` at entry (`entries_*_rsi.json`, snap1-142012). Same fault at the same RIP, same reach. So synthesising rsi alone does not clear it. **OPEN**: the first instruction of the function containing `0x1408A2169` and the value that should reach rsi there (needs a clean disassembly from the function start, not a linear sweep from mid-function).
- Stop class: **caller-context register, not Fox, DLL import, or indirect.**

### HUD stop (CONFIRMED by trace)

- Fault: `0x1403FB746 mov r8, [rax + r8*8]` with `rax = [gs:0x58] = 0` (unmapped read at 0x0). This is a TLS-array read through the TEB. The replay TEB (`map_teb`) leaves `TEB+0x58` zero, and the snapshot holds no TLS array.
- Stop class: **TLS block (gs:[0x58]) not captured.** Not a DLL import, Fox or indirect site. The stop is kept as a stop, not zero-filled: a zero TLS block would make reads look valid when they are not.

## 3. Per-function results (reached set)

| function | calls | replays | mismatches | own-block | PROVEN |
|---|---|---|---|---|---|
| 0x140A57800 | 6 | 6 | 0 | 1/1 (1.00) | yes |
| 0x140A6CF70 | 6 | 6 | 0 | 2/3 (0.67) | yes |
| 0x14149A470 | 12 | 12 | 0 | 1/1 (1.00) | yes |
| 0x14149F5E0 | 6 | 6 | 0 | 2/5 (0.40) | yes (below the 0.5 floor, reported only) |
| 0x141E5CA20 | 6 | 6 | 0 | 4/5 (0.80) | yes |
| 0x141E5DE80 | 6 | 6 | 0 | 1/1 (1.00) | yes |
| **total** | **42** | **42** | **0** | **11/16 (0.69)** | **6 of 639** |

Replays per function are per snapshot: six snapshots had camera calls (snap1-142012, snap2-142339, snap3-142654, snap1-162213, snap2-162511, snap3-163043), and snap4-163107 had none. 0x14149A470 is reached twice per snapshot. Calls are the camera update's first calls before its stop, so every replay is a real call with its call-time memory delta.

JSON per function (reach, calls, replays, mismatches, own-block, static stop counts, for all 639): `<workdrive>/gf-PresReplay-scratch/results_pres_per_function.json`.

## 4. Mismatches

**None.** Zero mismatches in 42 replays, so no first divergent instruction exists to classify, and no `xlate.py` instruction class is named for RealFix. Whether these 6 functions exercise the MOVSS/MOVD upper-lane forms was not checked.

## 5. Stops and tool findings

1. **Tool bug (snapdiff build closure, CallDelta `snapdiff.py`, `build_closure`).** When a function's `pes_fn_*.cpp` already exists in the build dir, the loop `continue`s before scanning its `pes_fn_` references. Its callees then drop out of `need`, and the link fails (`undefined reference to pes_fn_140A6CF70`). Seen on 140A57800 in the 7-snapshot run, and fixed by a clean build dir. Fix: scan the cached file's references as well. Listed for Main, not edited.
2. **snapdiff `--only` takes a bare hex tag**: `--only 0x...` becomes `0X...` (`.upper()`), which yields "no spec". Pass `140A57800`.
3. Dynamic stops (per entry, frame 0, deterministic): camera 6 (caller-context register); HUD 7 (TLS read; snap4 HUD had 0 calls, error class not separately recorded).
4. Static stops over the 639 presentation specs (`precheck`):

| class | count |
|---|---|
| DLL import sites (named from `pes-re-import-slots.md`) | 0 |
| indirect call/jump sites (non-import) | 0 |
| Fox services / direct callees outside the set | 0 (every direct callee is in the set) |
| unresolved memory operand (`unresolved_mem` or `unresolved-mem@` flags) | 116 |
| tail jumps | 39 |
| stack-passed args | 24 |
| live-data writes | 16 |
| clean (no flags above) | 463 |

The import slots from the P0d table (134 slots, `pes-re-import-slots.md` §1.4) do not block any function in this 639-set statically. They block P functions elsewhere in the 37,855-row universe, not here. The presentation set is closed: no spec calls outside it and none goes through an indirect site.

## 6. Open items (numbered, for Main)

1. Camera stop: first instruction of the function containing `0x1408A2169` and the caller value of rsi (section 2). Without it, the camera's remaining presentation callees are unreached, so reach stays at 6.
2. Identify the HUD widget's per-frame update: slot 4 (`0x140E5FCF0`) is unconfirmed, and its TLS dependency needs a captured TLS array (not in the snapshots as taken).
3. Locate the live `this` for the radar (`0x1420E2910`) and scorebug (`0x1420F0EB0`) updates, and their per-frame caller.
4. Crowd, audio-cue and replay-recording entries: no live object found yet (vtable scan for the audio/crowd/replay classes is the next step).
5. The snapdiff closure bug in section 5.1 (CallDelta `snapdiff.py`), for Main.

## 7. Files (scratch only)

- `results_pres_per_function.json` (all 639, per function), `results_pres_7.json` (7-snapshot snapdiff), `results_pres_140A57800.json` (clean rebuild), `stops_precheck.json`, `snapdiff_7.log`.
- `pres_trace.py` (entry tracer), `scan_vt2.py` (vtable scan), `vt.py`, `disasm_va.py`, `classify_stops.py`.
- Traces: `run/p-<snapshot>/`; entries: `entries_<snapshot>.json`.
- Build dirs: `build/` (7-snapshot run), `build2/` (140A57800 clean rebuild).
