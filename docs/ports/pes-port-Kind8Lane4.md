# pes-port-Kind8Lane4: the two holder reds were wrong PES premises in the tests; GF's holder already matches PES's writers (23-11-26)

## Rig needs (top)
- None. No live read, hook or capture was made this session. Evidence: static decode of `pes_full.bin` (file offset = VA - 0x140000000)
  and the existing live sample in `docs/ports/pes-port-StallLive6.md` (MA+0x2601C measured live, 6,926 flight frames).

## Landed
- Merge `origin/Integrate` 82ad8564 into Kind8Lane: a fast-forward (Kind8Lane was already an ancestor). `gen_translated.py
  --image pes_full.bin --check`: 10,982 functions byte-identical (rc 0). `tools/check_pii.sh` rc 0 (run from <workdrive>/GodotFootball).
  Pushed: `git ls-remote origin Kind8Lane` = `82ad85644019c466b446f69e53c5d61c3c0debff`.
- Test commit `7c2b571e` (tests only; `git ls-remote origin Kind8Lane` = `7c2b571e26f64f6a9611466d64342384370ce90a`): `tests/sim/pes_holder_latch_test.cpp`. The commit body carries the same evidence.

## Red before, green after (tests only)
| test | before (82ad8564) | after (same src) |
|---|---|---|
| PesHolderLatch.ALastToucherWithLivePossessionBecomesTheHolder | FAILED: 101 unheld ticks | OK (excludes a last toucher with a pass since his touch) |
| PesHolderPersist.ASetHolder... (renamed ASetHolderIsClearedOnlyByAPassWhileTheBallIsInPlay) | FAILED: 5,049 cleared ticks | OK (every in-play clear has a pass since the holder was last set; 0 unattributed) |

Red-first is satisfied in the sense Main asked: the old tests were red on the GF code; the corrected tests are green on
the same code, so the old assertions were the only defect. The GF code matches the PES rule on every bucket (census below).

## Census (scratch, reverted)
| bucket | count | GF path | PES |
|---|---|---|---|
| latch: unheld live last toucher, a pass since his touch | 101 / 101 | `RecordPassEvent` nulls `pesKeyActor` (match.hpp:130) | kick pushes the none key; W1 stores 0xFF (StallLive6 section 2 items 1, 3) |
| latch: unheld, no pass since his touch | 0 / 101 | - | - |
| persist: in-play clear | 5,049 / 5,049 | the only GF path that nulls the holder is `RecordPassEvent` | same |

Debug detail for the persist window: pass at t=3166 ms, holder last observed set at t=3185 ms (one tick later, the recompute
at the next tick head clears it on the following observation). The first persist window was one tick and still counted
1,452 flights; the window is two ticks.

## PES writers (CONFIRMED, byte read from `pes_full.bin`)
- W1 `0x140457259`: `41 89 bd 1c 60 02 00` = `MOV [R13+0x2601C],EDI` (FUN_140457070). EDI = actor of the newest valid ring-1 key,
  else 0xFF (`docs/re/pes-re-holder-clear.md` section 2).
- W2 `0x140A71DF9`: `c7 83 1c 60 02 00 ff 00 00 00` = `MOV [RBX+0x2601C],0xFF` (object reset, init).
- Correction: `docs/re/pes-re-holder.md` section 8 says "only W2 writes 0xFF". It came from a scan for literal 0xFF immediates and misses
  W1's EDI store. The test comment that repeated it was wrong and is corrected.

## Evidence type (Main's condition)
- DECODE: W1/W2 bytes above (CONFIRMED). Ring-1 none key -> 0xFF is the decode in `pes-re-holder-clear.md` section 2.
- LIVE SAMPLE (not a replay of these test scenarios): `docs/ports/pes-port-StallLive6.md` section 2: 600 s of live PES, 11,148 frames;
  MA+0x2601C = 255 from the kick frame through all 6,926 flight frames; carrier held on slow touches (items 4-5).
- NOT DONE: no new capture or replay this session. The persist rule's "no clear without a pass" is the decoded rule plus this
  sample, not a capture of the exact test match.

## Ported writers and stand-ins
- No new port. `Match::RecomputePesHolder` (W1 recompute) and its input `pesKeyActor` match PES on every census bucket.
- Stand-ins left in place (named in code): the touch-key push is `CarrierFromTouch` (pesreceive.cpp:350), gated by `AI_HasPossession || receiver`;
  PES's K=7 push is probe-gated (G+0x2A80, OPEN, `pes-re-first-touch.md` 5.4). The restart-step stand-in (match.cpp:913).
  None of these produced a bucket in the census.

## Mutation checks (each rebuilt, run, reverted; `git diff` clean after each)
- M1 (latch): delete the touch-path `SetPesKeyActor` (humanoid.cpp:1514). Latch test: **FAILED, 2,617 ticks** (was 0 on the corrected test).
- M2 (persist): `RecomputePesHolder` also clears when `ballRetainer` is null (a loose-ball clear). Persist test: **FAILED, 2,760 ticks**.

## Ratchet and neighbours (same tree as the commit)
- PesStallQuick: forced_live = **53** (ratchet <= 53): PASS.
- PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver: PASS.

## Kind8 queue item: bit 4 of S+0x294 (static, partial)
- Callback `0x1404335B0` = 16 bytes: `lea rdx,[rcx+0x2970]; add rcx,4; jmp 0x140430C30` (bytes read from the image). Front = job+4,
  back = job+0x2970, back-front = 0x296C (matches the brief).
- `0x140430C30` (dec.sh decompile) copies back to front field by field. Front-relative writes start at +0x2798 and reach +0x296C
  (the first callback decompile starts at +0x279C). Nothing writes front+0x294.
- So the copy does NOT clear bit 4 at S+0x294, for S = front (0x294 < 0x2798) or S = job base (rcx+0x294 = front+0x290).
  The copy is not the clearer. INF: the clearer is elsewhere (candidates behind `call [r10+0x88]`, `0x140872570`).
- OPEN: the S base (`FUN_140425A90`: S = *(O+0x20)) and the write set of the `[r10+0x88]` target.

## GF side of kind 8 (not started)
- `matchevents.cpp:189` `in.ballState = kBallStateInPlay` and `kickGate = true` for every queued touch (matchevents.cpp:13 comment);
  PES's ball-state word G+0x40+0x291C is unported. No holder bucket depends on it.

## Open items
1. Kind8 bit-4 clearer: decode S's base and the `[r10+0x88]` target's writes.
2. Probe G+0x2A80 writer (K=7 gate). Needed before a non-possession touch push can be ported.
3. Ball state word (matchevents) for kind 8.
4. The two-tick kick window in the persist test is a measured lag (one tick to the holder observation, one tick of rounding); no PES fact
   fixes its size. A capture of the holder after a kick at per-frame resolution would pin it.

---

# Round 2 (after Main's review of 7c2b571e): S base, bit-4 clearer, probe writer

## Rig needs (top; none are launched by me, Main batches them)
1. Hook `0x140871DE0` (entry; FUN_140871de0 = `*(u32*)(S+0x294)=0; *(u16*)(S+0x291)=0`, the only bit-4 clearer statically
   proven to clear S+0x294 as a whole word). Predicate: `d(rcx+0x294)&0x10` (bit 4 set at entry, so the clear is real). Cap 12, FULL.
   Shows: the return address (the clearer's caller, `ret` in --when) and whether rcx equals `*(O+0x20)` of the driver.
   No direct caller and no absolute/RIP-relative reference exists in the image (scanned), so the caller is indirect or inlined.
   The live capture is the only way to name it.
2. Hook `0x140945EEF` (entry, the +0x64 countdown writer of the probe block): log R14D, RBX, and the caller. Cap 12, FULL.
3. Hook `0x14094DB7E` (entry; sets probe +0x64 = -1): log RBX and the caller. Cap 12, FULL.

## S base (CONFIRMED, translated and decompiled)
- Driver `0x140425A90` (`src/pes/translated/ball/pes_fn_140425A90.cpp:48`, decompile line 1 of s294.txt): `S = *(u64*)(O+0x20)`
  with O = its rcx argument (the job object). S is a POINTER stored at O+0x20, not a field of O.
- The driver then calls on S (rcx = S): `0x1408722C0` (arm: S+0x298=3, S+0x294 |= 1), `0x140872570` (clear bit 0 when set, then
  `0x1404335E0(S+0x4E0)`), `0x140872200`, `0x140872C10` (sync).
- Front buffer question: the copy `0x140430C30` (via callback `0x1404335B0`) writes O+0x279C..O+0x296C only (the first front
  write is at front+0x2798 = O+0x279C). It never writes O+0x20. So S, read from O+0x20, is NOT the front buffer and the copy does
  not clear S+0x294 unless the pointer at O+0x20 points back into O's copied range, which nothing in the image shows (INF).
  Verdict: the copy is not the bit-4 clearer (CONFIRMED for the field write set; the aliasing case is INF, closed by hook 1's
  rcx/S check).

## Writers of S+0x294 (static scan, `scanwrites.py` in scratch, 378 operands, 173 writes; validated on 0x2601C: 21/2)
- Set bit 0: `0x1408722C0` (|=1), `0x140872240` (|=1).
- Set bit 2: `0x140872210` (|=4). Set 0x13 (bit 4 among them) on the kick path: `0x140870A40` (`|= 0x13`, pes_fn_140870A40.cpp; the
  `|=4` at lines 278/359 are the other arms).
- Clear bit 0: `0x140872570` (translated: `and dword [S+0x294], 0xFFFFFFFE`; decompile agrees, guarded by `(byte & 1)`).
- Clear bit 1: `0x14086F618` (`and dword [r14+0x294], 0xFFFFFFFD`, inside chunk 0x14086F608 = the call [r10+0x88] site).
- CLEAR BIT 4 (whole-word): `0x140871DE0` (`mov [S+0x294],0`; untranslated; no direct/RIP/qword caller: hook 1).
- CLEAR BIT 4 (byte store, candidates): `mov byte [rbx+0x294],1` at 0x142095A33 and 0x142095ADA; `mov dword [rax+0x294],0` at
  0x1422A9C86; `and byte [rax+0x294], al` at 0x14310833C. The base of each is not yet tied to S (OPEN).
- The 0x14086F608 chunk's clear is bit 1, not bit 4: the brief's "bit-4 clearer candidate" there is ruled out by the operand.

## G+0x2A80 probe (the touch-key push's missing input; NOT ported)
- Writers found: `0x140A44FD1` (+0x2A98 = the id byte from RDI, +0x2AA0..0x2AB0 = RDI, +0x2A90 = vtable `0x142608810`, +0x2AC0 =
  id byte): an init/reset of the block, not a per-touch writer. `0x140945EEF` (+0x2AE4 := R14D, then if >0 decrement, then if
  `func_0x140A57930(G+0x40)` set it to a float-scaled value): a countdown, not the K=7 contact. `0x14094DB7E` (+0x2AE4 := -1). Plus
  `0x140A4839D` (+0x2A80 := R10) and `0x156E9EC30` (`or byte [rsi+0x2A80], al`).
- The probe's +0x18 candidate id and +0x64 active flag, written on a touch contact, are NOT found: the +0x2AE4 writers above are
  timer code. The contact writer (the one that sets the id and makes the flag 1 on a real touch) is OPEN.
- Port status: NOT PORTED. Red-first not started (no failing test added; no src change). Next: find the contact writer by a
  disp scan on the probe's base (G+0x2A80 via `[reg+0x2A80]`, any reg) excluding the three above, or hook the callers of
  `0x140945EEF` with `ret` to see which touch path arms it.

## Status vs Main's asks
- Item 1 (S base and bit-4 clearer): S base CONFIRMED; clearer NARROWED to `0x140871DE0` (whole-word) and byte-store candidates; the live
  identity needs hook 1. Not fully closed.
- Item 2 (probe writer port): NOT done. Blocker: the contact writer is not yet found statically; I spent the request budget on items 1.
- Holder reds: still green at 7c2b571e (no src change in this round). PesStallQuick forced_live 53 (from round 1).
- Tip: 7c2b571e (no new commit this round).
