# PES21 rendezvous, part 2: recorded-call emulator entry, PES hook list (static), sider module and install plan

Status: branch `Rendezvous` pushed (commit `0bd096d7`, confirmed by `git ls-remote origin Rendezvous`). **Nothing was loaded into PES21, no file in the install was written, and no live read of PES (pid <pid>) was made.** The hook list is static, from `pes_full.bin`. Labels: CONFIRMED, INF, OPEN.

## 1. Emulator entry from a recorded call (frame_trace, branch `Rendezvous`)

What it does. `run_recorded(snap_dir, orig, budget)` in `tools/dev/translate/frame_trace.py` emulates one rendezvous snapshot's recorded call at its captured entry, with:
- the 16 captured GPRs, including the caller's rsp (the real stack is the snapshot's, not a synthetic one);
- the 16 captured XMM values (128-bit);
- RFLAGS and MXCSR (unicorn 2.1 round-trips MXCSR; this closes the earlier OPEN item);
- the game's gs base (the captured TEB; `map_teb`'s synthetic TEB is not used, so StackBase/TLS stay the game's);
- the return address, read from the snapshot at [rsp] and checked against the record.

It returns `(unicorn, error)`. It refuses:
- an entry that starts with `jmp [rip+0]` (a rendezvous hook patch) unless the displaced bytes are given (`hook_bytes` parses the rv.dll config);
- a record whose return address differs from the snapshot at rsp;
- an unmapped or unreadable entry, stack or TEB page;
- a return that is not at the recorded address (`did not return`, with the budget);
- a return with rsp not at rsp+8 (`stack not balanced`).

CLI: `python3 frame_trace.py recorded SNAP_DIR [--hooks CFG] [--budget N]` prints RAX, XMM0 (low lane), rip and rsp as JSON; exit 1 on error. The existing frame-trace CLI is unchanged (`recorded` dispatches before `main()`).

**Tests** (`tools/dev/translate/test_recorded_call.py`, 11 tests, synthetic snapshots built in the test, no game bytes; registered as `translate_recorded_call_test` in `tests/CMakeLists.txt`, skip 77 without `GF_DIFF_PYTHON`):
- GPRs and return (RAX=19 from rcx/rdx; rsp+8 after ret); XMM (addsd of recorded operands); gs base (TEB self pointer read via `gs:[0x30]`); RFLAGS (pushfq); MXCSR (stmxcsr); a hook patch replaced by its original bytes; a patched entry without original refused; a patch to an unmapped target is an error; budget overrun; unbalanced return; hook_bytes parsing.
- Result: 11/11 pass (venv Python with unicorn 2.1.0). Run directly; ctest was not run (no cmake build, per the agents' verification split).

**Mutation checks** (each reverted and re-run; all caught):
| mutant | result |
|---|---|
| XMM inputs not loaded | FAIL |
| gs base not set | FAIL |
| RFLAGS not loaded | FAIL |
| MXCSR not loaded | FAIL |
| hook original bytes ignored | FAIL |
| rsp balance check removed | FAIL (after adding `test_unbalanced_return_is_an_error`; the first version of the suite missed this one) |
| patched-entry guard removed | FAIL |

**Real captures (CONFIRMED):** all 82 recorded calls from the Wine test-exe run (scratch `run/rdv/snaps`, `sim_tick`, hooked by rv.dll) replayed through `run_recorded` with `run/hooks_sim.cfg`: **82/82 RAX match the game's own result** (exit ring). Replay median 3.9 ms, max 7.4 ms.

**Finding fixed during this work (CONFIRMED):** without the hook config, the same snapshots *also* returned the right RAX, with no error. The snapshot holds the rv.dll patch at the entry, and its jump target (the trampoline pool) is mapped in the snapshot, so the replay ran the hook trampoline. The match was a coincidence of the hook being transparent (it changes only counters and the shared page). That is a silent wrong run, so the guard above now refuses it, with a test. The first synthetic test passed only because its patch jumped to an unmapped address; the mapped-stub test (`test_patched_entry_without_original_is_refused`) reproduced the silent run and then failed as intended before the fix.

**Not checked:** whether the captured MXCSR and RFLAGS affect a real PES call (none of the test code uses FP rounding modes); PES captures do not exist yet. The external side that writes these snapshots (`rv_take`, in scratch) is not in the repo: it is a copy of `full_snapshot.py` plus the rendezvous, and would be merged into `tools/dev/translate/full_snapshot.py` when the rendezvous is approved (OPEN).

## 2. PES hook list (static, from pes_full.bin)

Method (`scratch/src/pes_hook_plan.py`, `rv_hooks.py` v2):
- Flat image: file offset = VA − 0x140000000 (the harness's `image_kind` = flat; the same mapping the diff harness uses).
- Function containment through `.pdata` (158,648 functions).
- Window check: the shortest run of whole instructions at the function entry covering ≥14 bytes; REFUSED if it contains a branch, call, return, or rip-relative operand, or does not decode. **Refusal path tested** on six synthetic windows (call, rip-relative lea, ret, short jmp, undecodable, and a clean one): each refused or passed with the right reason.
- Callers: a raw `E8` scan of the code section, each confirmed by a linear decode of the caller's function.
- Branches into a window: raw-byte branch candidates (rel32 call/jmp/jcc, rel8 jmp/jcc), each confirmed or dismissed by a linear decode of the function holding its source byte. **Positive control: 100/100** real direct branches inside four hook functions were confirmed by the same check.

Results (17 hook lines in the config; every entry passes the window check):

| name | brief VA | hook entry | window (bytes) | confirmed branches into window | note |
|---|---|---|---|---|---|
| cut_create_141F57840 | 0x141F57840 | 0x141F57840 (entry) | 15 | 0 | cutscene CREATE/LOAD |
| cut_load_141F36200 | 0x141F36200 | 0x141F36200 (entry) | 20 | 0 (2 raw candidates dismissed: not instruction starts in the source's function) | cutscene LOAD |
| cut_load_141F5F330 | 0x141F5F330 | 0x141F5F330 (entry) | 17 | 0 | cutscene slot-list build (ledger) |
| fixdemo_eval_141F4D2F0 | 0x141F4D2F0 | 0x141F4D2F0 (entry) | 14 | 0 (1 raw dismissed) | FixDemo evaluator |
| fixdemo_match_141F4B1D0 | (scene-birth doc) | 0x141F4B1D0 (entry) | 15 | 0 | FixDemo matcher; not in the brief, added from docs/re/pes-re-scene-birth.md |
| restart_44C880 | 0x14044C880 | 0x14044C880 (entry) | 15 | 0 | restart builder |
| restart_446390 | 0x140446390 | 0x140446390 (entry) | 15 | 0 | restart builder (fan-in) |
| camera_update_87F280 | 0x14087F280 | 0x14087F280 (entry) | 16 | 0 (2 raw dismissed) | in-play camera update, vtable slot 4 |
| camera_caller_886780 | 0x140886780 | 0x140886780 (entry) | 18 | 0 | its caller (brief) |
| hud_radar_update_E2910 | (hud-ui doc) | 0x1420E2910 (entry) | 15 | 0 (1 raw dismissed) | HUD radar update (`docs/re/pes-re-hud-ui.md` §5) |
| mi_refresh_113B280 | 0x14113B27F | 0x14113B280 (entry) | 14 | 0 | see note A |
| mi_refresh_113B520 | 0x14113B51F | 0x14113B520 (entry) | 18 | 0 | see note A |
| type13_body_A115E9 | 0x140A115E9 | 0x140A115A0 (function entry) | 17 | 0 | see note B |
| fixdemo_caller_141F41CE0 | (caller of eval) | 0x141F41CE0 (entry) | 15 | 0 | direct caller of the evaluator |
| fixdemo_caller_141F4ACE0 | (caller of eval) | 0x141F4ACE0 (entry) | 15 | 0 (1 raw dismissed) | direct caller |
| fixdemo_caller_141F4B6C0 | (caller of eval) | 0x141F4B6C0 (entry) | 15 | 0 | direct caller |
| fixdemo_caller_141F50FC0 | (caller of eval) | 0x141F50FC0 (entry) | 15 | 0 (1 raw dismissed) | direct caller |

Notes:
- **A. The two MI VAs are int3 padding (CONFIRMED).** 0x14113B27F and 0x14113B51F hold 0xCC; the next `.pdata` function starts one byte later, at 0x14113B280 and 0x14113B520. The docs (`docs/re/pes-re-open-items.md` §EV-OPEN-0005) list the same addresses as "fn", so the docs' VAs are the padding bytes too. The hooks use the function starts.
- **B. The type-13 VA is inside its function, not at it (CONFIRMED).** 0x140A115E9 is in the function that starts at 0x140A115A0; the hook is at 0x140A115A0 (the window check passed there).
- **C. The camera update has no direct caller (CONFIRMED by the E8 scan)**, consistent with the docs. The brief's caller 0x140886780 contains `call qword ptr [rax + 0x20]` (vtable slot 4, the same slot as the camera update) and `call qword ptr [rax + 0x10]`. That the call dispatches to 0x14087F280 is INF: rax is not identified as the InplayCamera object.
- **D. FixDemo evaluator callers (CONFIRMED by E8 scan and decode):** five direct callers, 0x141F41CE0, 0x141F4ACE0, 0x141F4B1D0 (the matcher, already a target), 0x141F4B6C0, 0x141F50FC0. The four non-matcher callers are in the config.
- **E. Raw branch scan caveat:** the "dismissed" candidates were not instruction starts in a linear decode of their function. That is INF that they are data or padding; a linear decode can desync in code with embedded data. A real branch would have been caught by the positive control, so the risk is limited to a branch in a function whose linear decode desyncs (OPEN: a whole-function check by a second decoder, e.g. ghidra's, would close it).
- **F. Sampling is a placeholder.** The config writes `FIRST=0 EVERY=0`: the hooks are installed but never sample. The rate is the owner's call (section 5).

**What the config is and is not.** `run/plan/pes_hooks.cfg` (scratch only, never committed) carries the expected bytes of these 17 entries, which are PES code. The report carries no bytes: VAs, lengths and mnemonics only. rv_init compares the expected bytes with the live process before patching and refuses on mismatch, so a dump that differs from the running game is refused rather than patched.

**Precondition not yet met (OPEN, needs the owner):** `pes_full.bin` is the unpacked dump, not the live process. Before any install, the expected bytes must be confirmed against the live process with a read-only `/proc/<pid>/mem` read (no signal, no write). Until that is done, the hook list is static evidence only.

## 3. sider module and install/undo (branch `Rendezvous`, `tools/dev/rendezvous/`)

Files:
- `rendezvous.lua`: sider module. In `init(ctx)` it checks for `<sider_dir>rendezvous\hooks.cfg` (absent: logs and returns), then `ffi.load`s `rv.dll` and calls `rv_init(cfg, run\, 5000)`. It uses `io.open` and `ffi.load` (INF: both available in sider's LuaJIT; not run in sider).
- `install_rendezvous.py`: plan by default. `--apply` writes; `--undo` reverses. It writes `rendezvous\rv.dll`, `rendezvous\hooks.cfg`, `rendezvous\run\`, `modules\rendezvous.lua`, one `lua.module = "rendezvous.lua"` line after the last `lua.module` line (the file's line ending is kept), a backup `sider.ini.rendezvous-bak`, and `rendezvous\install.json` (pre/post sha256 and what was inserted).
- Undo: if `sider.ini` still has the post-install hash, it is restored byte-for-byte from the backup and checked against the pre-install hash. If the owner edited it since, only the inserted line is removed (refused unless it occurs exactly once); the backup is kept, and the message says so.

**Plan on the real install (CONFIRMED, no writes):** `--root <workdrive>/PES21 --dll build/rv.dll --cfg run/plan/pes_hooks.cfg`. It prints 7 steps with the input hashes. `sider.ini` sha256 before `fb23c854…`, after `c7753876…` (the planned insert at line 36). The script printed, and this report confirms: no `rendezvous*` entry in the install, no `sider.ini.*` backup, `sider.ini` hash unchanged after the run. The planned dll sha256 is `614b33b6…`; the planned module sha256 is `db4e0dcf…`.

**Apply and undo on scratch copies of `sider.ini` (CONFIRMED):**
- LF copy of the owner's `sider.ini`: apply, undo: sha256 identical to the pre-install hash; no leftover files; the `modules/` directory created by apply is removed.
- A second apply on an installed copy is refused (the line already exists).
- CRLF copy, with a `; owner edit` line appended after install, then undo: the inserted line is removed, the owner's line and the rest of the file are kept byte-for-byte, and the backup is kept with a message.
- Undo with nothing installed: refused.

The apply path was exercised only on scratch copies. Main asked for plan-only; the apply/undo code exists so the reversibility of the plan can be shown, and it must not be run on the owner's install without approval.

## 4. Risks (update to part 1)

- **Live bytes must match the dump** (section 2): unverified until a read-only live check. rv_init refuses a mismatch, so an unverified dump fails closed, not into a wrong patch.
- **Stall per sample:** a sampled call parks its thread until the snapshot is taken (about 1.5–2 s on PES, INF from the existing snapshot log). Hooks on per-frame functions (HUD, camera) would stall every frame they are sampled in.
- **Silent wrong run in replays:** closed for patched entries by the guard (section 1). Other displaced-code hazards are covered only by the window check and the branch scan (E above).
- **sider's Lua runtime** (`io.open`, `ffi.load` from a module): not exercised. Settling it needs an owner-approved load of a no-op rv.dll.
- **Install race:** a 14-byte patch written while another thread may execute the function (OPEN, untested on a multi-threaded target; the plan is to install with the process stopped or before the function can run).
- **Hook sampling of dispatch-only functions** (the MI refresh functions are vtable methods, per the docs): a sampled call stalls the dispatcher; the rate matters more there (INF).

## 5. Owner decisions needed (before any PES action)

1. Approve a read-only live check of the expected bytes (`/proc/<pid>/mem`, no signal, no write), which closes the precondition in section 2.
2. Approve the sider change (the plan in section 3), and a no-op rv.dll load first if wanted.
3. Choose the sampling rate per hook (the config has FIRST=0, EVERY=0 placeholders).
4. Choose which of the 17 hooks to install first. The per-frame candidates (HUD radar, camera) stall the game each time they are sampled; the cutscene and restart entries are lower-rate candidates (INF from the docs).

## 6. Files and what to run

Branch `Rendezvous` (commit `0bd096d7`, from `origin/FrameRoot`), five files:
- `tools/dev/translate/frame_trace.py`: `run_recorded`, `load_recorded_call`, `hook_bytes`, `recorded_main`, the `recorded` subcommand (+118 lines, no deletions).
- `tools/dev/translate/test_recorded_call.py`: 11 tests.
- `tests/CMakeLists.txt`: `translate_recorded_call_test`.
- `tools/dev/rendezvous/rendezvous.lua`, `tools/dev/rendezvous/install_rendezvous.py`.

Worktree: `<workdrive>/gf-Rendezvous` (`extern/godot-cpp` symlinked as the brief says; `git status` fails on that symlink, so staging was by explicit path). `check_pii` was not run from the main checkout; the changed files were scanned by grep for machine paths, PES extensions and emails (none).

Scratch (not committed; Main removes once archived): `<workdrive>/gf-Rendezvous-scratch/` — `src/rv_hooks.py` (v2: flat images and VAs), `src/pes_hook_plan.py`, `src/check_recorded_on_wine.py`, `run/plan/` (plan.json, pes_hooks.cfg with PES bytes), `fake/`, `fake2/` (install test copies).

Suggested checks for Main:
- `GF_DIFF_PYTHON=<venv python> ctest -R translate_recorded_call_test` (expect Passed; 11 tests).
- `python3 tools/dev/rendezvous/install_rendezvous.py --root <PES install> --dll <rv.dll> --cfg <cfg>` (plan only; prints what it would write).


## Main's live-byte check (26-10-26)

Read-only pread of the running game's code: all 17 hook windows match the expected bytes (17/17, 0 mismatches), so the unpacked dump is the running image at every planned hook.
