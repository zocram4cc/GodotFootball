# PES press chain, round 4: layout and thunk proven; RunActionPress NOT wired (PressWire)

Branch `StallApproach`, pushed: `bb52fa12e4eb0f28bf68f2dd7a5e7299b052ed95` (confirmed by `git ls-remote`). Commits this round:
- `ab15ca5b` the per-slot press region: `PesWorldImage::press[22]`, `kPressObjectBytes = 0x1A5E0`, `PressVa`/`Press`; layout test;
- `bb52fa12` the press thunk `0x140518DD0` proven 12/12 bit-exact on press captures, closure 141, layer 10445.

**Result: step 1 is only half done (the region is in; the construction is not), step 2 is half done (the thunk is proven; the dispatcher and driver press paths are proven only through it), step 3 is not started. The walk is still in `_BallControlCommand`. No red test for the wiring, no census. Target <=118 is not met (last measured 151).**

## Step 1: per-slot X region (partly done)

- Done: `PesWorldImage` ends with `press[kSlotCount]` (PesPressImage, 0x1A5E0 each), after the AI records. `kPressOffset`, `PesWorld::PressVa(index)`, `Press(index)`. Consumers of `kWorldBytes`/the window: `PesWorld::Bind` and `MemoryCall` take `kWorldBytes` and `sizeof(PesWorldImage)`, so they follow the new size; the trap loader VA (0x2100000000) is far above the window. No other consumer hard-codes the size.
- Test `pes_press_object_test` 2/2: each slot's block is kPressObjectBytes after the previous; the last block ends the window except the struct's alignment padding (4 bytes; the first draft asserted exact equality and failed on that padding).
- NOT done: building each X through the translated ctor `FUN_14091E9F0` at world build. What happened:
  1. Over the window alone the ctor aborted: the constructor's virtual calls read their vtables through the image, and the window alone maps none (`PES_DISPATCH_UNTRANSLATED: 0`). Fixed by running it on `MemoryCall()` (the match memory's image, heap and window). CONFIRMED.
  2. Then it aborts on an uncovered path: `FUN_141B55730` calls `0x14041F2F0` through a stored callback pointer (a `call [frame+0x68]` inside its branch). The six ctor captures never reached that call (own blocks 20/25), so the closure has no proof for it. This is the designed loud abort. CONFIRMED by gdb backtrace: `pes_fn_14091E9F0` -> `pes_fn_14091EDB0` -> `pes_fn_1409BD650` -> `pes_fn_1404234B0` -> `pes_fn_141B55730` -> dispatch missing.
  3. Reverted the construction, so `BindMemory` keeps working. Committed only the layout. An uncommitted intermediate with the construction would have aborted every match binding.
- Next for step 1: prove the callback path with a synthetic state over a real ctor capture (`synth.py`, `prove_bounded --synth`), or find the branch that decides the call and capture it (the `call [rsp+0x68]` at `pes_fn_141B55730.cpp` line ~84-85 is reached only on a path the captures did not take).

## Step 2: prove the thunk, dispatcher, driver; wire

- Done: thunk `0x140518DD0` proven on 12 real captures of `pw_140518DD0` (pid2111702). All 12 have A0's vtable `0x142634948` (the press action): 12/12 replayed, 0 mismatches, own block 2/2. Its closure (141 members) includes the dispatcher `0x140518E20` and ActionPress `0x140962440`, both entered by the replays, so the press path through the vtable chain 0x88 -> 0x90 -> 0x20 is covered for the thunk's inputs.
- Earlier finding (this round): none of the 12 dispatcher captures and none of the 12 driver captures in pid1858437 is a press action (A0 vtables 0x142630490, 0x142642558, 0x142631A30, 0x142634B38, 0x142630310, 0x1426301E0, 0x1426303D0 are other classes). So the driver proof of round 3 (88cc845b, on two non-press captures) does NOT cover the press path. The driver's press path is covered only through the thunk's closure; the driver itself still needs press captures (Main's `pw_1405141F0` with predicate `q(rcx)==0x142634948`; none in pid2068143 at the time, press captures to be checked in pid2111702).
- Not done: wiring `RunActionPress` (driver -> 0x88 -> 0x90 -> 0x20), which needs: A2's slot (A2+4), ctx (A2+0x38 = CtxVa(side)) and owner (A2+0x40 = PVa(slot)) written per tick; the driver's `param_3 = A2+0x50` (r8, confirmed in 12 of 12 driver/dispatcher captures) inside X; `param_4` = a 4-byte int the caller holds (its value at the press ticks is not yet known: non-press captures showed 0x1, 0x3, 0x11, 0x33, 0x36, so the press value must be read from a press capture). All of it depends on step 1 (X must exist).

## Step 3: walk, red test, census, PesStallQuick, ankle, PesHang, gen --check: NOT started

Nothing was wired, so the walk (`playercontroller.cpp`, `AI_GetBallControlMovement`) stays and no census number changed. The walk stays until the wiring is proven end to end.

## Mechanics and hygiene (CONFIRMED)

- `gen_translated.py --check`: green on `bb52fa12` (10445 functions byte-identical, 3831 recorded bit-exact, 6614 closure).
- A closure member list had two PesHeap native services (`0x1403FBC10`, `0x141B34CB0`); removed earlier (fd55fbda), as other closures do.
- Build: `build/` removed at the end (the test binary is 1.6 GB). The first build of this round ran without the shared lock because the wrapper exited early (Main noted it); later builds are foreground under `flock /tmp/gf-build.lock nice -n 19`.
- `pkill -f dec_alloc` once (round 3): noted by Main, no harm seen.
- `press_inputs.py` now reads an unpacked `mem.bin` directly (committed in `ab15ca5b`).
- Unpacked the three packed dispatcher captures in pid1858437 (seq424, seq439, seq463); they are not press captures, so they are not used. Scratch: `<local>/scratch-press`.

## Asks of Main

- The press predicate for the dispatcher and driver hooks, and the press captures for the driver (`pw_1405141F0`, `q(rcx)==0x142634948`), so step 2 can prove the driver on press.
- Captures of the X ctor where its callback path runs, or permission to build a synthetic state over an existing X capture (`synth.py`) for `0x14041F2F0`.
- Whether the walk should be deleted before the wiring is proven. I did not delete it.
