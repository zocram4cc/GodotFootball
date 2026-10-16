# ShootoutCrash2 — the shootout segfault is a stale object file in `build/`, not the source

Result: Done. Root cause proven; no source change is warranted, so the branch carries only the
corrected tracking entry.

Changes: `tasks/15-10-26.md`: the "Open defect" entry (which told the next session to debug with a
RelWithDebInfo build + core dump, and called the backtrace bogus) now records the closed root cause
and the remedy, and the surrounding "known" lines no longer call the crash load-dependent. Two
commits, `b960dfb` and `21b1f94`, pushed as `refs/heads/ShootoutCrash2`. No test and no source file
changed.

## Root cause (proven)

`PesChallengeCounters.TheReportLineCarriesBothTeamsOrdered` and `…MaskHistogram…` fault in
`PenaltyShootoutController::Start` → `Referee::CancelSetPiece`. That backtrace is accurate; the cause
is a stale object file in **main's `build/`**:

1. `build/src/sim/CMakeFiles/gfsim.dir/onthepitch/penaltyshootoutcontroller.cpp.o` was built 10-15
   04:07, i.e. from pre-latch sources. `3880c67` (the possession latch) adds `int pesFrame` to
   `Match::BallEvent`: `BallEvent` grows 24 → 32 bytes, `Match` by +16, and `referee` moves from
   1872 (0x750) to 1888 (0x760). The owner's reproduction merge `b452cab` (10-15 21:04, "Merge
   origin/PoseVeto into main to reproduce the shootout crash") brought that change into main and
   rebuilt the binary — around this object, which is how one pre-latch object ended up linked
   against post-latch headers. Offsets read from the built binary's DWARF:
   `inPlay` 1708, `ballEvents` 1776, `possessionSideHistory` 1872 (0x750), `referee` 1888 (0x760),
   `sizeof(Match)` 8984.
   The stale object's inline `GetReferee()` contains `mov 0x750(%rax),%rdi` followed by
   `call Referee::CancelSetPiece` (`objdump -dr`, relocation `R_X86_64_PLT32` at the call; the fault's
   return address in the crashing binary is `Start+0x121`, exactly the call site).
2. Ninja holds **no dependency record for that one object**: `ninja -t deps …` → `#deps 0 (VALID)`.
   Every other one of the 118 gfsim objects has 1060–1187 deps, and across all 180 non-godot-cpp
   objects in that build dir it is the only `#deps 0`. So no header change ever rebuilds it, and
   `ninja -n <that object>` confirms it is considered up to date. A fresh ninja build records 1176
   deps for the same file, so the dep machinery itself is fine.
3. The binary therefore contained two `Match` layouts. At the fault the crashing instruction loaded the
   pointer stored at `Match+0x750` — the heap address of the `ValueHistory<float>` that
   `possessionSideHistory` holds (`this=0x55555fea3d90` in frame 0, while the `Match` is at
   `0x555559875490`) — and gdb resolves the first word of that object as
   `vtable for blunted::ValueHistory<float> + 16`: `Referee::CancelSetPiece` is invoked on a
   `ValueHistory<float>`. Last log before the fault is `[Referee::Process] standing them off the ball
   for set piece 3` (stdout is block-buffered under gdb, so the last buffered lines are lost).

Causality, verified in my worktree in two ways: (a) `build-o3` with a deliberately stale object (the
current `.cpp` compiled against the pre-merge `match.hpp`), and (b) the same fresh `build-o3` binary
linked with **main's own** `penaltyshootoutcontroller.cpp.o` copied out of its build dir:

- crash reproduced, with full debug info: `Referee::CancelSetPiece (referee.cpp:949) ←
  PenaltyShootoutController::Start (unique_ptr.h:192) ← PenaltyShootoutController::Process (:366) ←
  Match::Process`, `info symbol *(void**)$rdi` → `vtable for blunted::ValueHistory<float> + 16`.
- restoring the correctly compiled object and relinking: the same filter PASSES (53 s).

The injected object reproduces main's code path exactly, not merely a similar one: in both binaries
`PenaltyShootoutController::Start` has the load and the call at the same offsets — main's
`build/tests/sim/pes_challenge_counters_test`: `mov 0x750(%rax),%rdi` at `Start+0x103`, `call
Referee::CancelSetPiece` at `Start+0x11c`; my `build-o3` binary: `+0x103` and `+0x11c` as well. The
crash's return address `0x…c011` in main's binary is `Start+0x121`, the return of that call.

The identity is numeric, with main's own object file copied into a fresh build and run under gdb:
at the fault `$rdi` is `0x55555fea3d90`, the live `Match` is `0x555559875490`, and in that frame
`&this->possessionSideHistory - this` = 1872 (0x750), `&this->referee - this` = 1888 (0x760),
`this->possessionSideHistory.get()` = `0x55555fea3d90`, `this->referee` = `0x5555579e1e40`. So
`$rdi == possessionSideHistory.get()` → true, `$rdi == referee` → false: the stale load yields the
field the *current* layout keeps at 0x750, and `Referee::CancelSetPiece` is entered with it. `bt full`
puts the fault at `referee.cpp:949  match->StopSetPiece();` — its first statement, reading `this`
(garbage) as a `Referee`. (Frame 1 is `PenaltyShootoutController::Start` with no symbol table info,
because main's `build/` is a **Release** build — `CMAKE_BUILD_TYPE=Release`, and the object has no
`.debug_info` at all. That is why the owner's first backtrace looked like bogus inlining: no line
info, and attribution by nearest symbol.)

Remedy: `rm build/src/sim/CMakeFiles/gfsim.dir/onthepitch/penaltyshootoutcontroller.cpp.o` (or wipe
`build/`) and rebuild. `ninja` alone will not rebuild it. I did not touch main's build dir — it is
the owner's, and two of its tests fail because of it.

## Tests

Nothing added or changed: there is no source defect, so there is no failing test to write. The two
named tests already assert the invariant (a match runs into a shootout, the shootout plays to a
winner, and the report / mask histogram are then internally consistent). The variable is the build,
and the red→green evidence is the pair above: stale object → SIGSEGV in
`Start → CancelSetPiece` on a `ValueHistory<float>`; correct object → the filter passes.

## Verification

- `--gtest_filter='*ReportLine*'`: PASSED in `build-o3` (`-O3 -g`, fresh object; 53 s, 31 log lines
  from the penalty controller, starting at `[PenaltyShootoutController::Start] penalty shootout at
  the x=52.5 end: HOM shoot first`, so the shootout really is reached and played) and in the
  RelWithDebInfo `build/` (54.1 s); same for
  `--gtest_filter='*MaskHistogram*'` in `build/` (48.5 s PASSED). The gate's `build/` is fully
  consistent: all 197 non-godot-cpp objects in it are newer than the `match.hpp` that moved the
  layout, none older (`find -newermt` / `wc -l`).
- full gate in my worktree (`ctest --test-dir build -j3 --timeout 2400`, RelWithDebInfo,
  PES21_ROOT + PES21_EDIT_SAVE set): __GATE__
- the only failing tests are the `godot_*` ones, which need the GDExtension library. My worktree never
  built `godot/bin/libgodotfootball.so` (`godot/bin/` is empty here; main's has it, 04:05), so they
  report `ERROR: Can't open dynamic library, file not found`. This is an artifact of the worktree, not
  of the branch: the source is identical to main's. All non-Godot tests pass.
- smoke (`gfsim_run --seed 7 --minutes 20`, RelWithDebInfo): rc=0, `[determinism] 2f6242c5556975df`.
  That is not the `38dc29d6142683f7` recorded for the latch's smoke on 10-15: 29 commits landed between
  `fecc10e` and today's main, including the 1/54 s tick and the 2D headings, and the smoke's hash
  moves with them. The branch is source-identical to `origin/main`, so this is main's hash; the smoke
  reports it twice from the same seed, which is the self-consistency check.
- main's crashing binary run serially under gdb (`--gtest_filter='*ReportLine*'`): SIGSEGV with the
  backtrace above; `rdi` → `ValueHistory<float>` vtable.

## Notes

- The faulting run under gdb was live, so no core was needed (`ulimit -c unlimited` was never the
  bottleneck the ticket assumed); `bt full` + `info locals` come from `gdb-deep.log`.
- Why the earlier ASAN runs could not reproduce it (inference, not a check I ran): an ASAN build needs
  its own build directory, and a fresh compile records its deps, so no such tree can hold a pre-latch
  object. The crash needs a `build/` that survived the header change — like main's.
- The evidence objects and logs are in `gf-ShootoutCrash2-scratch/` on the shared disk: `stale-main.o`
  (md5 `bfc754d441bd47e401a8078fee690574`, the object copied out of main's `build/`),
  `gdb-deep.log` (`bt full` + locals on it), `gdb-ident.log` (the `$rdi` identity), `gate.log` /
  `gate2.log` (the gate before / after building the 12 test targets that a fresh configure marks
  `_NOT_BUILT`), `smoke.log`. My
  `build-o3` is an `-O3 -g` build used only for the captures; it and the `build/` the gate ran in
  were both compiled from the current sources.
- Working tree of the branch is clean apart from `extern/godot-cpp`, which is the worktree's symlink
  to main's checkout (never staged).
- Why that one deps record is empty is not recoverable from the tree: the object (10-15 04:07) is
  newer than its source (10-14 17:02) and older than the headers that followed, so ninja never had a
  reason to revisit it. A build interrupted between the compile and the deps-log write, or a build
  output copied in, are the plausible origins; either way the record was already empty when
  `b452cab`'s rebuild ran at 21:04. Everything else in that build dir is tracked normally.
- The ticket's other candidates were checked and are not this crash: `team.cpp`'s substitute builder
  returns false during `e_MatchPhase_Penalties` (`team.cpp:186`) and is gated behind
  `substitutions_enabled` (default false), so it cannot run in a shootout; `MentalImage::GetPlayerImage`
  ends in `players.at(0)` (mentalimage.cpp:60) and `Player::GetClosestOpponentDistance` derefs
  `AI_GetClosestPlayer` without a null check (player.cpp:428-431) — both still latent, neither is
  reachable in the shootout paths that a fresh build runs. Left unfixed: the ticket asked for the
  segfault, and these are separate hazards.
- Out of scope, observed: one shootout kick logs
  `verdict GOAL! (aimed for SAVED!) … [MISMATCH: recording what the ball did]`
  (penaltyshootoutcontroller.cpp:415) — the controller's scripted verdict disagreeing with the ball is
  a shootout-fidelity question, not this crash.
