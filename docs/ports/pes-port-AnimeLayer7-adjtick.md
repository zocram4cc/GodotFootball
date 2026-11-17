# FUN_141EB5E70 proof and chain gap (AnimeLayer7)

## Proof status
- Walk-out run (done): pid3778393 seq061, seq062. calls 2, replayed 2, mismatches 0, own-block 1.0. Yaw branch only.
- In-play run (tag adjB, `proof_adjB.log`): seq1128 and seq1171. Strike running in both (counts 6 and 16), yaw running. Pending when this note was written.
- Height branch (P+0x498 state 2): not observed in any of the 19 captured calls. Named gap until a capture shows it (watcher bg_252, remaining cap).

## Captures (pid3778393); states are (state, count)
- Walk-out, seq061-070, 074, 075 (12): strike (0,0), height (0,0), yaw (2, 2..20, then 1 and 3 after a restart). Mode byte 0.
- In play, seq1128, 1129, 1141, 1166, 1168, 1169, 1171 (7): strike (2, 4..16), height (0,0), yaw (2, 4..16). Mode byte 1.
- Owner PM+0x10 (per-man tick counter): 176..204 in walk-out, 2310..2396 in play. Counter rises 2 per 50 hook calls, so about 25 calls per tick: one call per man per tick.

## p2
- float at [rdx] and P+0x41C = float(1/54) = 0x3C97B426 in every capture. GF passes p2 = 1/kPesTickHz once per tick per man.

## Gate (FUN_1408BFCF0)
- P = [man+0x7D8]. FUN_141EB5E70(P, p2) runs if (P+0xA07 == 0 or P+0xA06 != 0) and P != 0 and man+0x7C8 != 0. Captures: P+0xA06 = 1, P+0xA07 = 1, man+0x7C8 = 1.

## Second chain X (named gap, no stand-in)
- Owner PM = 0x3D54D580 (vtable 0x1426329A8, +0x7D8 = P).
- S = PM+0x940 = 0x3AF6C8E0 in all 19 captures. X = *S = 0x2FA967D0, vtable 0x142ABA840 (vt+0x18 = FUN_141794410, vt+0x20 = FUN_141AFF1F0, vt+0x38 = FUN_141B00910).
- FUN_141EBC820(S, p2) runs FUN_141AF7A90(X, p2) and then tail-jumps X->vt[+0x38](X, p2) only if DAT_1437F4CB0 != 0, X != 0 and S+0x14 <= 255 (signed compare; 0x100 exits).
- DAT_1437F4CB0 = 1 in all 12 walk-out captures.
- S+0x14 = 256 in all 19 captures (12 walk-out, 7 in play). The gate is closed in every captured tick, so X never ran in the captured set.
- GF: no coverage (docs/re/LEDGER.md:5371 undecoded; FUN_141B00910 untranslated).

## Chain A
- [P+0xA08] = 0x2CA7CB90, vtable 0x142ABA1A8 (vt+0x18 = 0x141AF4360, vt+0x20 = 0x141AF4650 driver, vt+0x38 = 0x141AFC4A0 runner).
- GF's RunFrame (pesfoxworld.cpp:255) runs the driver on a = Read64(engineP + kAnimeOffset), not on the player block. So the chain for P+0xA08 is that `a`. Confirm before the cut.

## Chain X: the earlier "born closed, dead" claim was WRONG (17-11, byte scan)

The xref-graph argument above (ctor, dtor, tick only) missed a second handle, found by disassembling the whole code
section (`capstone`, 10,514,410 instructions, every memory operand with `disp == 0x940`: 128 sites).

|fact|evidence|
|---|---|
|The wrapper has a **second handle**: the anime object's `P+0x3AE8`|`FUN_1406f0d50` (`0x1406f0d50: mov [rcx+0x3AE8], rdx; ret`) is called from the model paths `0x1408bf224` and `0x1408bf9f1` with rcx = `[model+0x7d8]`, rdx = `[model+0x940]`. Six sites touch `+0x3AE8`.|
|A fourth wrapper user exists, reached through that handle|`0x1406f1b36: mov rcx, [rsi+0x3AE8]; ... call 0x141ebced0` in the parametric-request path|
|**`FUN_141ebced0` writes the gate**|`0x141ebcef0: cmp edx, 0x100` → `jge` path stores `mov dword [rbx+0x14], 0x100`; the `jl` path calls `FUN_141aff150` and `FUN_141b00170` on X, zeroes `S+0x8` and `S+0x10`, then `0x141ebcf4d: mov dword ptr [rbx+0x14], edi`|

So `0x100` (256) is **"no X chain selected"**, not a permanent lock: a value below 256 is stored by
`FUN_141ebced0` from the parametric request (its `edx` comes from `FUN_1406b5870`'s answer at `0x1406f1b31`). All 19
captures show 256, i.e. X was unselected at every captured tick; whether it is ever selected in a match is **open
again**, and the cut still carries no stand-in. The concrete thing to capture next, when the rig is back: a hook on
`FUN_141ebced0` (or read `[P+0x3AE8]+0x14` after a parametric request).

Method note, because it cost the wrong claim: an immediate-value scan finds nothing here — the arming store is
`mov [rbx+0x14], edi`, a register. The image has 16 sites storing the immediate 2 into `[reg+0x14]` and 562 storing
any immediate, none of them the wrapper. Only the handle scan (`disp == 0x940`, then `disp == 0x3AE8`) found the writer.

## At-rest vtable slots are obfuscated (17-11)

Read the same VA in the unpacked image and in a live capture:

|slot|at rest (`pes_full.bin`)|at runtime (capture)|
|---|---|---|
|adjuster strike vtable `0x142b678f0` +0..0x18|`8000000000000000 8000000000000000 0000000080000000 0000000080000000`|`141eb45e0 141eb33e0 141eb1a20 141eb1bf0`|
|chain-A vtable `0x142aba1a8` +0..0x10|`119930522 3281480 300000000`|`141af4360 141af4650 141afc4a0`|

That is why `closure_check` misses `.rdata`-slot thunks, and why Ghidra has **no function** for `0x141afc4a0`,
`0x141ebcd00` or `0x141ebced0`: their addresses never appear as static operands, so its analysis never creates them.
Anything read out of `.rdata` in the file must be treated as encoded; the capture's runtime copy is the authority.

## The adjuster wrapper (`FUN_141eb3af0`), the shape the cut needs

Each of the three adjusters at `A+0x428` (strike), `A+0x498` (height), `A+0x4D0` (yaw) is this
struct, stepped once per tick with the chain pointer as context:

|field|role|
|---|---|
|`+0x0`|vtable: `+0x10` reset, `+0x18` step, `+0x20` finish|
|`+0x8`|state; **the body runs only when it equals 2**|
|`+0xa`|start frame, `+0xc` current frame, `+0xe` end frame|
|`+0x14`,`+0x1c`|the two accumulators `FUN_141af3ca0` folds each tick into|
|`+0x20` (int at `+0x10`),`+0x24`|the folded value and the active flag|

At `current >= end` it clears state/counters/accumulator/flag, calls vtable `+0x10` (reset) and
`+0x20` (finish) with the accumulators; while `current < end` it calls `+0x18` (step). So an
adjuster's whole run is bounded by its frame window, and the state field is set to 2 by the
clip/anime side — **who sets it to 2 is the remaining open item** for the height adjuster
(strike and yaw are seen at 2 live; height never was).

## The three adjusters and the B block, read out of a real in-play capture (17-11, static)

`live2/pid3778393/seq1128`, unpacked with `capz.py unpack` (packed again after) and read from
`mem.bin`, which is concatenated in `index.json` mapping order, not VA-indexed (its size equals the
sum of the mapping sizes). `P` = `rcx` at the hook = the anime object.

|adjuster|struct at|vtable|state `+0x8`|`+0xc` / `+0xe`|vtable `+0x10` reset / `+0x18` step / `+0x20` finish|
|---|---|---|---|---|---|
|strike|`P+0x428`|`0x142b678f0`|2 (running)|6 / 0|`0x141eb1a20` / `0x141eb1bf0` / `0x141794410`|
|height|`P+0x498`|`0x142b67920`|0 (never armed)|0 / 0|`0x141eb1a40` / `0x141eb2450` / `0x141794410`|
|yaw|`P+0x4D0`|`0x142b67950`|2 (running)|5 / 0|`0x141eb1a10` / `0x141eb1a50` / `0x141794410`|

The vtables are installed by the anime-object constructor `FUN_141eb42b0`, which leaves every
adjuster's state at 0; the stop/reset routines `FUN_141eb33e0` and `FUN_141eb3440` set the state
back to 0. No arming site turned up in `FUN_141eb4650`, `FUN_141eb3bd0`, `FUN_141af52b0` or
`FUN_1406f14e0`, so the open item stands, narrowed to: who stores 2 into `+0x8` of these three.

**Height is never armed, not armed late:** its state, counters and accumulator are all zero in this
capture, so the missing height branch is an arming decision upstream, not a timing difference.

`FUN_1406EEDF0`'s writes into the chain's B block, checked against the same capture
(`A = [P+0xA08] = 0x2ca7cb90`, `B = [A+0xF0] = 0x2f33d310`):

|B field|observed|source|
|---|---|---|
|`+0x80` any-active|1|set by any active adjuster|
|`+0x81` strike-active|1|`FUN_141eb3320(P+0x428)`|
|`+0x82` yaw-active|1|`FUN_141eb3320(P+0x4D0)`|
|`+0x90`, `+0x98`|−15.5029, −15.7650|the strike accumulators `P+0x43C`, `P+0x444` (they match exactly)|
|`+0xA4`, `+0xAC`|0.9758, −0.2185|the yaw trig polynomial on `P+0x4F0` = 3.5822|
|`+0xB4` height value|0|height inactive, so the `+0xB0`/`+0xB4` branch|
|`P+0x41C` p2|0.018519 = 1/54|confirms the per-tick dt|

These are the values the one-chain cut has to reproduce from the adjuster state: a checked
expectation for `FUN_1406EEDF0` without replay.
