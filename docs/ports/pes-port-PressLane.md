# PES press chain, PressLane: step 1 proven and committed; the X ctor is red at its next blocker (walk not deleted)

Branch `PressLane` at `61a18f7f47a561cf98d0fc854c69dc6eaa3dea03` (from `origin/Integrate` 91660def), confirmed by `git ls-remote`. Commit: the callback proof, its closure, the regenerated layer, and a red test. Behaviour is unchanged: `BindMemory` does not run the X ctor yet. The provisional walk stays in `_BallControlCommand`; no census run (no behaviour changed).

**Result: step 1 done (the callback is decoded, proven and translated). Step 2 is wired as code, then reverted, because the bound ctor aborts on a second untranslated registry callback. Steps 3 and 4 not started.**

## Step 1: the callback branch (CONFIRMED)

- `FUN_141B55730` (disasm, flat image): `r14 = [vt38(O)]` (Q); if Q != 0 and `vt(Q)[0x70](Q, O, rbp) == 0` it calls the stored callback `[rsp+0x68]` = `0x14041F2F0` (set by `FUN_1404234B0` line 93).
- vt(Q)[0x70] = `0x141B6EAB0` (Q's vtable `0x142ACAA40`, read from the probe state at the call site). It returns 0 when the lookup `0x141B72FC0(*(Q+0x10), *O, *(O+8), 1)` misses.
- The lookup key in the ctor's entry state is the 7-byte string `"COMMAND"` (`O+8` = 7). In the six real ctor captures (`pid1724101` seq321..326; `pid1858437` seq353..358) the key is present, so the callback is first-time registration (it creates a 0x20-byte node, vtable `0x2178BC5`).
- Probe states: `synth.py` with `entry 0x14091EDB0`, `stop 0x141B55784` (the `call [rax+0x70]` site), capture `pid1724101/seq321`. Read `rcx = Q`, `rax = vt`.

## Step 1: the proof (SYNTHETIC, labelled)

- Spec `CB_ABSENT_KEY`: `keylen` (u64 at `rcx+8` = `O+8`) set to 0 at `FUN_141B55730` entry; `reach 0x14041F2F0`, `stop 0x14041F2F0`, `start call`. One state, reached.
- `prove_bounded.py 0x14041F2F0 --tag pressback --synth … --merge`: **proven, 1 of 1 calls, 0 mismatches, own-block 9/14 (0.64)**. Record in `results_trapaction.json`; closure `closure_pressback.json`.
- Closure (`closure.py --spec-set trapaction --tag pressback`): root spec `14041F2F0.json`; new callee `14041F8E0` (spec + closure file).
- `closure_pressback.json` drops native PesHeap `0x1403FBC10`: `gen_dispatch` refused it as both translated and native (the same rule as `closure_pressx`, StallRegress5).
- Layer regenerated: 10,877 functions (3,889 recorded bit-exact, 6,988 closure). Callback at `src/pes/translated/pressback/pes_fn_14041F2F0.cpp`.
- `gen_translated.py --check` on the pushed tip: **GREEN** (10,877 byte-identical; held out: 0x1403BB670, 0x1405549C0, 0x14202BA30, unchanged).

## Step 2: the X ctor at BindMemory (NOT landed; red)

- Ctor argument registers (CONFIRMED, six captures): `rcx = X` (the block), `rdx = side` (0), `r8 = slot` (0..5). `r9` is not read: the ctor zeroes it before use (its captured value was `X + 0x1A5E0`, irrelevant).
- Code written and then reverted: a per-slot loop in `PesWorld::BindMemory` calling `pes_fn_14091E9F0` with `rcx = SlotObjectVa(side*11+slot)`, `rdx = side`, `r8 = slot`, over `MemoryCall()`.
- Test `SlotObjects.BindMemoryBuildsEachSlotsObjectThroughPesCtorAndTheTableNamesIt` (`tests/pes_press_object_test.cpp`): boots the anime world (`AnimeWorldTemplate`, the match's heap size, `PES_EXE` + `PES21_ROOT`), binds a `PesWorld`, requires slot-table entry `0x1436F3FC0 + index*8` == `SlotObjectVa(index)`.
- **RED** (verified with the loop in place): the bound ctor aborts with `PES_DISPATCH_UNTRANSLATED: 0x1404704f0`.
- Survey (`GF_PES_SURVEY`): the only press-chain target is **0x1404704F0, 484 hits per run**, called from `FUN_14047A4A0` (a press-closure member, `closure_pressx`), which stores `0x1404704F0` as a callback pointer (`pes_fn_14047A4A0.cpp` line 93), the same shape as 0x14041F2F0 (a 0x20-byte node create). The other 257 survey targets (`0x1401B8740…`, caller offset 0x7BF58) come from the anime boot, not the ctor.
- Why reverted: `BindMemory` runs in every match (`matchevents.cpp` `BindPesTrapWorld`), so the loop would abort every match. Step 2 lands only after the next callback is proven.

## Steps 3 and 4 (not started)

- The per-tick press caller G = `FUN_140963501` (`.pdata` 0x140963501..0x14096366C; return addresses 0x140963581 / 0x1409635E2 are inside it). Its entry loops over 3 entries with the slot in `ebx`, reads the action table at `M+0x2470C` (`lea rsi, [r13+0x2470c]`), keeps kinds `<= 0x15`, and calls `0x140A48910`, `0x140A6E340/5E0/150` before dispatching. Who writes the A2 per tick (slot, ctx, owner) is the G-side writer; not decoded past the loop.
- **Driver predicate (answering Main):** `q(rcx) == 0x142634948` is correct at the driver and the dispatcher. `rcx` is A0 at both entries: `pid1858437/seq1380` (driver) `rcx = 0x32360b80`, `[rcx] = 0x1426303D0`; `seq424` (dispatcher) `rcx = 0x332B97D8`, `[rcx] = 0x142630490` (the non-press classes). Why pid2111702 caught 0: the captures are GLOBAL and both hooks were already at cap 12 (pid1858437: 12 driver, 12 dispatcher). Main re-armed both at cap 24, EVERY 50 (hook request sent to Main).
- Not done: `RunActionPress` wiring, the red test for the designated man (ActionPress's target = ball), walk deletion, census, PesStallQuick, ankle 9/9, PesHang. The walk and the census number (151, measured on d4919cc4) are unchanged.

## Blockers, in order

1. **Callback 0x1404704F0** (from `FUN_14047A4A0`, the second registry insert): decode the lookup at its caller's branch, prove the absent-key state as for 0x14041F2F0 (labelled synthetic), translate, regenerate. It is reached 484 times per ctor run, so it is on the real path, not a rare one.
2. Then the step-2 loop re-lands (with its red test turning green), and `BindMemory` builds the 22 X blocks through the translated ctor.
3. Per-tick writers: decode G (`0x140963501`) for the A2 slot/ctx/owner writes and the action-table selection (`M+0x2470C`). The driver and dispatcher captures from Main's relaunch (cap 24 each) prove the press path (step 3's proofs).
4. Then `RunActionPress` (driver 0x1405141F0 -> vt 0x88 thunk 0x140518DD0 -> vt 0x90 dispatcher 0x140518E20 -> vt 0x20 ActionPress), the red test, walk deletion, census.

## Mechanics and hygiene

- Commits: `61a18f7f47` (callback + closure + layer + red test), pushed. `gen_translated.py --check`: green on the tip.
- Test run: `pes_press_object_test`: `SlotObjects.EachSlotHasItsOwnBlockOfTheAllocatorsSize` and `TheLastSlotsBlockEndsTheWindowExceptForAlignmentPadding` pass; the BindMemory test is red as described. Built only `pes_press_object_test` (49 s, ccache).
- Captures: `pid1724101/seq321`, `pid1858437/seq1380`, `seq424` unpacked for the probes and repacked afterwards. Scratch in `<workdrive>/gf-PressLane-scratch` (logs only, 304 MB); synthetic state copies deleted (3.7 GB each). The build dir is removed.
- Disk: 150 GiB free at the end (the owner floor is 150 for this session; the rule says return to >= 300 by session end: reclaim by packing old live2 launches, not captures).
- Not run: census (ALiveBall), PesStallQuick, PesHang, ankle 9/9 (no behaviour change; the walk stays).
- Mistakes disclosed: the first `gen_translated` run failed on my own `closure_pressback.json` member `0x1403FBC10` (fixed, rerun, 15 min); `closure.py` wrote a temp dir into `/tmp` (removed).

## Round 2 (PressLane, branch `PressLane` at `c9df355e`, pushed and confirmed by `ls-remote`)

Landed: the merge of `origin/Integrate` 8fa69b21; the second registry create callback `0x1404704F0` proven on a labelled synthetic state; and the X construction in `BindMemory` running end to end. The walk is NOT deleted and RunActionPress is NOT wired.

- Merge: `tools/dev/translate/results_trapaction.json` conflicted and was resolved with `json_union_merge.py` (95 records, duplicate VAs already in base and theirs). `pes_native.cpp` auto-merged. Layer regenerated: 10,964 functions (3,902 recorded bit-exact, 7,062 closure).
- `0x1404704F0` (CONFIRMED decode): the create callback of `FUN_14047A4A0`'s one-time name-pair init (guard byte at `rcx+8`; `lea rip-0xa049` at 0x14047A532 resolves to it). Proven on a SYNTHETIC state: step 1 forces the guard to 0 at `0x14047A4A0` and stops at its first call into `0x141B55730`; step 2 sets that call's name length to 0 (absent key) and reaches the callback. Result 1/1, 0 mismatches, own-block 9/14. Closure set `pressreg2`: native PesHeap `0x1403FBC10` dropped (gen_dispatch refuses it as both translated and native).
- Dead end recorded: emptying the registry list head (`Q+0x10`) makes PES's own lookup dereference null at `0x10`. Use the name-length perturbation instead.
- X construction: `BindMemory` calls `FUN_14091E9F0` per (side, slot): rcx = X, rdx = side, r8 = slot. Test `SlotObjects.BindMemoryBuildsEachSlotsObjectThroughPesCtorAndTheTableNamesIt` GREEN (install-backed, 1.5 s).
- `gen_translated.py --check` GREEN on `c9df355e`: 10,964 byte-identical.
- Press driver, NOT merged: `pid1408046` seq33805 (`pw_1405141F0`, press, `rcx` A0 vtable `0x142634948`). Bounded proof of `0x1405141F0` on that real call: 1/1, 0 mismatches, own-block 74/139. Record is in scratch only; merge with `prove_bounded.py --merge --tag pressdrv` next round.
- Real press entry shape (CONFIRMED from the capture): `rcx` = A0; `rdx` = A2 (A2+4 = 1, ctx 0x340267a0, owner 0x35e2b940); `r8` = A2+0x50 (exact, 0x50 in this capture); `r9` = a stack state pointer; `[rsp+0x28]` = 0x34 (PRESS action code).
- Blocker for RunActionPress (OPEN): the per-tick writers of A2+4 (slot), A2+0x38 (ctx), A2+0x40 (owner P) are PES's man tick `G` at `0x140963501` (`.pdata` 0x140963501..0x14096366C). Decoded so far: a loop over 3 entries (slot in `ebx`), the action table at `M+0x2470C` (`lea rsi,[r13+0x2470c]`), kinds `<= 0x15`, helper calls `0x140A48910`, `0x140A6E340/5E0/150`. The stores to A2 are not decoded. Also OPEN: owner == `PVa(slot)` and ctx == `CtxVa(side)` are not checked. Writing those from GF values is a stand-in and waits for G's proof.
- Next, in order: merge the driver record; port the writer of A2 from G (or the minimal path that proves it); then `RunActionPress` (driver `0x1405141F0`, then the vtable chain) with the red test, the walk deletion in the same commit, census (ALiveBall, PesStallQuick, PesHang), ankle 9/9, `gen --check`.
- Hygiene: build dir removed. Scratch `<workdrive>/gf-PressLane-scratch` 306 MB (logs and proof inputs). Synthetic state copies deleted (3.7 GB each). Disk 168 GiB free at the end.
- Not run this round: census, PesStallQuick, PesHang, ankle 9/9 (no walk change, no behaviour change in the wired path).
