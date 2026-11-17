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

## Next
- Proof row: strike and yaw (in play and walk-out). Height pending.
- Cut: one chain A, as Main decided; X recorded as a named gap with the S+0x14 evidence above.
