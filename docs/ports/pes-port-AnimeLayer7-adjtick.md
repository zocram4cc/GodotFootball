# FUN_141EB5E70 proof and chain gap (AnimeLayer7)

Status (17-11): `pes_full.bin` is a flat image (file offset = VA − 0x140000000). Every instruction address below was re-checked by a `.pdata`-anchored decode (`/tmp/citecheck_flat.py`). Addresses from an earlier PE-table mapping were 0xA00 too high and are replaced here.

## Proof status
- Walk-out run (done): pid3778393 seq061, seq062. calls 2, replayed 2, mismatches 0, own-block 1.0. Yaw branch only.
- In-play run (tag adjB, `proof_adjB.log`): seq1128 and seq1171. Strike running in both (counts 6 and 16), yaw running. Proven: with the walk-out pair, 4 real calls bit-exact, strike and yaw branches (proof row `dee9d88c`).
- Height branch (P+0x498 state 2): not observed in any of the 19 captured calls. Named gap until a capture shows it (watcher bg_252, remaining cap).

## Captures (pid3778393); states are (state, count)
- Walk-out, seq061-070, 074, 075 (12): strike (0,0), height (0,0), yaw (2, 2..20, then 1 and 3 after a restart). Mode byte 0.
- In play, seq1128, 1129, 1141, 1166, 1168, 1169, 1171 (7): strike (2, 4..16), height (0,0), yaw (2, 4..16). Mode byte 1.
- Owner PM+0x10 (per-man tick counter): 176..204 in walk-out, 2310..2396 in play. Counter rises 2 per 50 hook calls, so about 25 calls per tick: one call per man per tick.

## p2
- float at [rdx] and P+0x41C = float(1/54) = 0x3C97B426 in every capture. GF passes p2 = 1/kPesTickHz once per tick per man.

## Gate (FUN_1408BFCF0)
- P = [man+0x7D8]. FUN_141EB5E70(P, p2) runs if (P+0xA07 == 0 or P+0xA06 != 0) and P != 0 and man+0x7C8 != 0. Captures: P+0xA06 = 1, P+0xA07 = 1, man+0x7C8 = 1.

## Second chain X (flat image, 17-11)

The wrapper S is 0x18 bytes at model+0x940, and the same handle is also stored at P+0x3AE8. S+0x0 holds the X object (0x180 bytes; vtable 0x142ABA840 in the captures). S+0x14 is the gate int.

|role|function|verified facts|
|---|---|---|
|constructor|`FUN_141EBC1C0`|stores `S+0x14 = 0x100`|
|gate writer (wrapper)|`FUN_141EBC4D0`|one caller, `0x1406F1147`. `cmp edx,0x100` at `0x141EBC4F0`; the `>=` path stores `0x100` at `0x141EBC4F8`; the `<` path stores the index (`edi`) at `0x141EBC54D`|
|handle installer|`FUN_1406F0350`|`mov [rcx+0x3AE8], rdx; ret`. Callers `0x1408BE832` (in FUN_1408BE740) and `0x1408BEFFF` (in FUN_1408BED84): rcx = `[model+0x7D8]` = P, rdx = `[model+0x940]` = S|
|handle reader|the key function `FUN_1406F0F70`'s separate `.pdata` chunk `0x1406F10E6–0x1406F1182` (key in `ebp`; 157 E8 callers)|`cmp [rsi+0x3AE8],0` at `0x1406F111D`; index call `FUN_1406B4E70` at `0x1406F1131` with edx = key; `mov rcx,[rsi+0x3AE8]` at `0x1406F1136`; wrapper call at `0x1406F1147` with edx = index|
|index|`FUN_1406B4E70`|`cmp edx,0x1125; jae` → returns 0x100. Else reads the word at `table[key*4]` and returns it `& 0x1FF`; a null table returns 0x100. The table is `FUN_1406B4D40`'s 0x1125-word copy of the stock `Eyestarget.bin`, held by `FUN_1406B4EA0` (see `docs/re/pes-re-chainx-select.md`)|
|gate|`FUN_141EBC820`|`cmp [rbx+0x14],0x100` at `0x141EBC83A` (returns at >= 0x100). Per-tick call at `0x1408BFD49` in FUN_1408BFCF0|
|query|`FUN_141EBC300`|`cmp [rdi+0x14],0x100` at `0x141EBC34A`; one caller, `0x1408BEBDD`|
|release|`FUN_141EBC2A0`|frees X, `S+0x0 = 0`|

Reading: 0x100 is the sentinel the index returns for a key ≥ 0x1125, a null table or an entry whose low 9 bits are ≥ 0x100, and the wrapper stores it for any index ≥ 0x100. An index below 0x100 reaches the wrapper's `<` path (`FUN_141AFE750`, `FUN_141AFF770`, both verified by ChainXSelect). The gate runs the chain only when the singleton flag is set, X is non-null and `S+0x14 <= 0xFF`. All 19 captures show 256, which means "no chain selected" in those ticks. 234 motion keys select an entry; whether a live request ever does is open.

Settling capture: the wrapper entry `0x141EBC4D0` is not hookable (RIP-relative operand inside the window), so hook `FUN_141AFE750`'s entry with the predicate "return address = `0x141EBC52F`" (its call from the wrapper's `<` path). One hit settles it, and the snapshot carries P, S, the index and the key (`rbp`).

## Retracted claims (17-11)
- "Chain X is born closed / dead for player models": wrong. A second handle (P+0x3AE8) reaches the gate writer, so 256 is a "no selection" value, not a lock.
- "At-rest vtable slots are obfuscated": wrong. The earlier reads mapped `pes_full.bin` through the PE section table, which is 0xA00 off for this flat file. Flat (file offset = VA − 0x140000000): the control VA `0x14256AE1C` holds `0x3F800000` (1.0f); the strike vtable's five slots (`0x141EB45E0, 0x141EB33E0, 0x141EB1A20, 0x141EB1BF0, 0x141794410`) sit at file offset `0x2B678F0`, the flat offset of VA `0x142B678F0`. They are plain pointers, identical to the capture.
- Code addresses from the table map were 0xA00 too high. For example, the store reached as `0x1406F0D50` is the instruction at `0x1406F0350`.

## The adjuster wrapper (FUN_141EB3AF0), flat-decoded
Each adjuster struct: vtable at +0x0 (reset +0x10, step +0x18, finish +0x20); state int at +0x8; counter short at +0xC; end short at +0xE; limit short at +0x10; accumulators at +0x14/+0x18/+0x1C; value int at +0x20; active byte at +0x24.

Stepper, decoded in full:
1. The body runs only if the state (+0x8) is 2.
2. If the counter is 1, fetch a value into +0x20 (call `FUN_141AF3AF0`).
3. Fold the accumulators (call `FUN_141AF3CA0`).
4. If counter < end: counter++ and return (a ramp).
5. Else if counter > limit (+0x10): clear the state fields, call reset (+0x10) and finish (+0x20, tail jump).
6. Else step (+0x18).

In both running captures end = 0, so the stepper takes branch 5 or 6, not the ramp.

## The three adjusters and the B block (seq1128)

|adjuster|struct at|vtable (runtime)|state|counter / end|reset / step / finish|
|---|---|---|---|---|---|
|strike|P+0x428|0x142B678F0|2 (running)|6 / 0|0x141EB1A20 / 0x141EB1BF0 / 0x141794410|
|height|P+0x498|0x142B67920|0 (unarmed)|0 / 0|0x141EB1A40 / 0x141EB2450 / 0x141794410|
|yaw|P+0x4D0|0x142B67950|2 (running)|5 / 0|0x141EB1A10 / 0x141EB1A50 / 0x141794410|

Constructor base, verified by decode: FUN_1406E03D0 calls FUN_141EB41B0 with rcx = P (call at 0x1406E0402). FUN_141EB41B0 does `add rcx, 8` and calls FUN_141EB42B0 (call at 0x141EB41C6). The constructor writes its vtables at its param + 0x420, which is P+0x428.

B block (written by FUN_1406EEDF0, checked against seq1128): flags +0x80 = 1, +0x81 = 1, +0x82 = 1. B+0x90 = −15.5029 is the strike accumulator P+0x43C, and B+0x98 = −15.7650 is P+0x444. B+0xA4 = 0.9758 and B+0xAC = −0.2185 are sin and cos of P+0x4F0/2: with P+0x4F0 = 3.5822, sin(1.7911) = 0.97583 and cos(1.7911) = −0.21853. The 0.5 factors are the floats at `0x1425658B4` and `0x1425658BC`. B+0xB4 = 0 (height inactive). P+0x41C = 0.018519 = 1/54.

Height (17-11): state 0 in seq1128 and in all 19 captured calls. The arm path is FUN_141EB3830 → FUN_141EB3AE0.
- `FUN_141EB3AE0` (`cmp [rcx+8],1; jne; mov [rcx+8],2`) is the only 1 → 2 step.
- `FUN_141EB3830` zeroes the state, calls reset, sets the counter to 1, and sets end and limit from its arguments, then stores state 1 (`mov [rdi+8], r8d` with `r8d = 1`, at `0x141EB38B6`).
- It has eight call sites: 0x140727BC9, 0x14072AE61, 0x14072FEB4, 0x1407301D6, 0x140731790, 0x14075C2E3, 0x1407664C9, 0x1407EE361.
- Site 0x140727BC9 is followed by the setter call at 0x140727BE1 (both verified).
- All eight call sites arm `P+0x498` and each is followed at once by `FUN_141EB3AE0` (state 1 → 2). The height adjuster arms nowhere else.
- Site A (`FUN_140727230`, decompiled 17-11): strike (`P+0x428`) is armed and stepped on every path that passes the motion check, then height is armed only if the request byte `req+0x1D == 0` and `FUN_140724620(motion)` is true. `FUN_140724620`: the motion record's count byte `+4` (JumpData, list `+0x90`) is nonzero, and jump start (`FUN_1406B11E0`) < first HitData frame (`FUN_1406B03D0`) < jump end (`FUN_1406B10D0`), all at rate 1.0. So the height adjuster is the **jump** adjuster: it runs only for a motion whose ball contact falls inside its jump window (headers, aerial traps). Its target comes from `FUN_14071C910`, which also takes ability byte `0x2C` through `FUN_1406E6660`.
- Why height is 0 in all 19 captures: none of the captured ticks had a jump-contact motion selected (INF, consistent with the decode). Settling capture: the existing height-running watcher during a header; no new hook needed.

## Chain A
- `[P+0xA08]` = 0x2CA7CB90 (vtable 0x142ABA1A8). Slots: +0x18 = 0x141AF4360, +0x20 = 0x141AF4650 (driver; `.pdata` 141AF4650–141AF4878), +0x38 = 0x141AFC4A0 (runner; `.pdata` 141AFC4A0–141AFC69D). Ghidra has no function at the runner, so that is a Ghidra analysis gap, not an absent function.
- GF's RunFrame (pesfoxworld.cpp:255) runs the driver on a = Read64(engineP + kAnimeOffset), not on the player block. So the chain for P+0xA08 is that `a`. Confirm before the cut.

## Next
- Proof row (`dee9d88c`): strike and yaw, in play and walk-out. Height pending: settle with the arm hook and the site A guards.
- Cut: one chain A, as Main decided; X recorded as open (above).
