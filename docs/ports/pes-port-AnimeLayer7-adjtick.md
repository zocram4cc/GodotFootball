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

## Chain X closed: the gate is written once, closed (17-11, static)

The wrapper `S` is a plain 0x18-byte struct: `+0x0` the X object (0x180 bytes, vtable
`0x142ABA840`), `+0x8`/`+0x10` ints, `+0x14` the gate int. Its entire xref graph in the image:

|role|fn|what it does to `S`|
|---|---|---|
|ctor `0x141ebc1c0`|(called from the model init `0x1408be3c0`, stores it at model+0x940)|`S+0x14 = 0x100`; builds X only when `DAT_1437f4cb0` (the anime singleton flag) and `DAT_1437f4ca8` are live **and** the ResManager lookup `0x141af3300` yields a resource|
|dtor `0x141ebc2a0`|(model re-init `0x1408be3c0`, teardown `0x1408be8d0`, model dtor `0x1408bdd80`)|frees X, `S+0x0 = 0`; never touches `+0x14`|
|tick `0x141ebc820`|(one caller, the per-tick `0x1408bfcf0`)|`return` unless singleton, `S+0x0 != 0`, and `S+0x14 <= 0xff`|

**No writer of `S+0x14` other than the constructor exists in the graph**, so the gate is
initialized to 256 and stays there: chain X's tick is dead for player models. That matches the
19 captures exactly (`S+0x14 = 256` in every one). Confidence High on the xref graph; the
offset-level scan is by graph traversal, not a byte-pattern sweep of every `+0x14` store
(INF on completeness). Consequence for the cut: no stand-in and no further capture effort for X.

The per-tick caller also gates chain A/X together (`0x1408bfcf0`): both run only when
`A+0xA07 == 0 || A+0xA06 != 0`, and X only when `model+0x940 != 0` and `model+0x7c8 != 0`.

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
