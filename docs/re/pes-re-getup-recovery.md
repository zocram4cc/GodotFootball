

> **Main 19-10-26 (rounds 27-30):** the motion-table base is the PRIMARY anime pointer (FUN_1406E6960, longlong* -0x157); +0x35C+k*0x14 is the clip's ELAPSED time. Clip 881 reached 83 elapsed frames at the phase 1->2 switch; its total is not read (the live motion object is unlocated), so "remaining = 0 at the switch" is OPEN. PARKED. Note FUN_1406B11E0 reads n from (w>>0x15)&0xff, unlike FUN_1406B10D0.

> **Main audit 19-10-26:** `0x14072EEFD` is the call inside FUN_14072EA00 (the rise blend set): matches. No fixed get-up timer: the next slide/tackle is gated by the current clip's +0x70 frame test (FallDown FUN_14072E360, Sliding FUN_140787EC0, Tackle FUN_140790160).
> **Main audit 19-10-26 (round 20):** `0x1406EADE0` `lea rax,[rcx+0x19a4]; ret` on the secondary (anime+0xAB8): the phase byte is anime+0x245C. The release at the stand-up end is not found statically (needs a live match); PARKED.
> **Main 19-10-26 (live):** one fall captured: Stagger 16 -> FallDown 17 (phase 1 constant) -> action 80 InplayDemoContact -> Move 2; the release is NOT a phase-2 release. CORRECTION: the current action id is the byte at anime_primary+0xAC9 (secondary+0x11), not anime+0x11.
## 16. Clip 881 total ticks: NOT established (round 28, final step)

**Formula (CONFIRMED by decompile of FUN_1406B10D0 and its callers).** `FUN_1406B10D0(obj, clip, rate, track)`: record = `*obj + clip·0x390` for `clip < 0x1125` (else `obj[1]`); track header word `w = *(u32 *)(rec+0x90+8·track)`, `n = low8(w>>13) + 2`; ticks = `int(((2n)/rate)·(fps/60) + 0.5)` with `fps = FUN_141E532D0()` (54 per §14). Main's formula matches. Note: the sibling `FUN_1406B11E0` (used for the frame-count path at several callers) reads `n = (w>>0x15) & 0xff` instead, so the two readers do NOT use the same bit field. Do not mix them.

**Object reachability (NOT found).** The motion object `obj` is passed in `rcx` by the callers (for example 0x1406F6EFA, 0x1406CFF72); `*obj` is the record array base. A pointer scan of the match global G (first 0x4000 bytes), MI, and the AnimePlayer (first 0x2000 bytes) for `obj` with `*obj + 881·0x390` giving `n_tr` in 1..32 and a nonzero track word returned only implausible hits (base in image data, track words such as 0xCCC3C032, which are not headers). No valid motion object was confirmed, so clip 881's record was not read live.

**Install data (not tried).** The install motion-record format is not decoded, so I could not read clip 881 from the files.

**Consequence.** The remaining-at-switch test ("total − elapsed ≈ 0") cannot be evaluated from this data. Clip 881's total is unknown; elapsed reached 83 frames at the 1→2 phase switch (§15). §15's status stands: OPEN.

**To close:** find the motion object by the caller's register (the enclosing function's `r12`/`rsi` at 0x1406F6EFA, decoded statically), or read the record through the owning function. Then read `n` at clip 881 track 0 (`*(rec+0x90)` word) and apply the formula above.
