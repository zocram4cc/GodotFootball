# pes-port-StallBreaker8: the null key 0 (fixed), the sim now runs end to end, and the shootout loop

Branch `StallBreaker`, pushed `64426c53` (on d31435fd). One commit: `PES native heap: Attach replaces only the heap's own windows`. Files: `src/pes/translated/pes_native.cpp`, `tests/pilot/pes_native_heap_test.cpp`. Layer untouched; `gen --check` unaffected (the committed layer is unchanged since d31435fd, which passed byte-identical).

## 1. The null key 0 at pes_fn_141B52380: cause and fix (CONFIRMED)
- The global `0x1437F0A58` is the pad manager, an object built by the loader (`pestrapdata.cpp`). Its creator in PES is `FUN_141B51E80` (not in the layer); the reader is `FUN_141B52380`, which dispatches `[obj]+0x68` (the mask reader `FUN_141B52340`, in the layer).
- Diagnostics in the abort state (a temporary print, reverted): `obj=0x21003FE030` (the loader arena), `vt=0`, `reg0=0`, `services=2`, `find(obj)=nil`, `img=0`, `segs=1`. The loader's shared region was not mapped in that call.
- Cause: `PesHeap::Attach` cleared every service. `PesWorld::FillTemplate`, `RunFrameBuilder`, `RunTrapTick` and `RunDribbleTick` call `LendExternals` (which lends the trap windows, the loader arena included) and then `heap_->Attach(s)`, which dropped them. A translated read of the pad object then returned 0, and key 0 was dispatched.
- Refuted: my earlier hypothesis (the pad vtable word pointed at an unmapped image; fix = an arena vtable copy). With the trap windows kept, the vtable word is the image address and the flat image serves it, so the copy is unnecessary. That change was reverted (patch kept in `gf-StallBreaker-scratch/r7/pad-vtable.patch`).
- Fix: `Attach` removes only its own page/arena windows and pushes them again.
- Red first: `NativeHeap.AttachKeepsTheWindowsTheCallerLent` failed (read 0), then passed (`pes_native_heap_test` 10/10).
- Pad device: the AI-only answer. The pad object's mask at `+0xC8` is zero (no pad bound: `FUN_141B53060` copies `+0xC0`, which the loader leaves at zero). So `FUN_141B52340` answers 0 without any device, as in an AI-only match. No GF-native pad object or device boundary is needed: PES's own code runs on PES's object.

## 2. PesHang and ALiveBall now run to verdicts (MEASURED, fixed tree)
| test | result (seed 11 / seed 7) |
|---|---|
| PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime | no abort; FAIL: `stopped at frame 180000, longest stoppage 1276 ticks`, match not at full time |
| PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker | no abort; FAIL on the verdict: **the breaker forced a live ball for 2,510 ticks in 87 episodes the series freeze does not explain** (longest episode listed in the log) |
| PesRestartDelay (tests 2 and 3) | not re-run this round (test 3 was the key-0 abort before the fix); needs a run |

## 3. Why PesHang does not reach full time: a penalty-shootout loop (MEASURED from the log)
- The match ends 0–0 and goes to the penalty shootout (`PenaltyShootoutController`). Every kick is a MISS, and the log shows `taker none` with the ball at rest on the spot, `(x=-41.6)`, for hundreds of stalemate lines (276 in the run).
- `penaltyshootoutcontroller.cpp:299–306`: the ball counts as struck only once it leaves the spot. With no strike, the kick is abandoned at `runUpTimeout_ms = 12000` and scored MISS, so the shootout never finishes.
- So the loop is in the shootout's kick start (the taker never strikes), not in the handler code. The handlers are not on this path in the profile below.
- Not fixed here. Next step: why the shootout taker never kicks (the taker assignment after `GetPenaltySpot`, and whether PES's kick path is reached in the shootout).

## 4. Tick rate and the hot path (MEASURED)
- Rate: 180,000 frames in 1,929 s wall (PesHang, fixed tree): **≈ 93 frames per wall-second**.
- "Before" the 35 handlers cannot be measured: the tree before them aborts on `0x140789D00` early (no tick count). The question "did the handlers slow the sim" is answered by the profile instead.
- `perf record -e cpu-clock:u`, 300 s, PesHang, fixed tree (58,594 samples). Top:
  - libc (13.3%, unlabelled; string compares, likely from the animation lookups) and `blunted::Animation::GetVariable` (12.3%): the GF-side animation layer looks up variables by string per frame.
  - `PesMem::rd` (11.0%) and `PesMem::wr` (7.5%): the PES memory accessors.
  - `AnimCollection::CrudeSelection` (3.6%), `pes_fn_1415A1708` (2.9%, the eyes table loader), `PesState::cond` (1.8%).
  - **None of the 35 handlers (`0x14073DD50`, `0x140789D00`, `0x140791DD0`, `0x140770290`, …) appears at 0.3% or more.** The handlers are not the slow part.
- Hot function to attack if speed matters: `blunted::Animation::GetVariable` with the string-keyed lookups (GF side), then `PesMem::rd/wr`.

## 5. Open items
- The shootout loop (section 3): the blocker for full time, and so for the gameOver failures. This is the real answer to "why no full time", not the handlers.
- PesRestartDelay tests 2 and 3 not re-run after the heap fix.
- The handlers and the 35 targets remain closure members with no synthetic-state proof.
- `perf` needs `-e cpu-clock:u` (the kernel events are blocked by `perf_event_paranoid=2`).
- `build/` removed at the end of this round; the tree is clean.
