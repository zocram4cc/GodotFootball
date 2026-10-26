# PES match advance: live frame diff, time base, pump, writers (Advance, 26-10-26)

Read-only RE. Image `pes_full.bin` (VA offset 0x140000000). Snapshots: `<workdrive>/gf-Advance-scratch/snap/adv{0..2}` (open play, frames 15544→15715→15898, per=1, play=1, 60 frames apart) plus snapE (frame 24648, per=3, E count=1).

## (a) Objective & scope
Name what PES's real frame writes (MI frame counter, player positions, MA nearest list, time base, pump ring) via live frame-to-frame diff; decode full pump `0x141B3E060` xmm2/xmm3; run an advancing emulator path; name writers with call stacks (S4 first). No repo edits (read-only rule); all harnesses in scratch.

## (b) Findings by confidence

### Live frame-to-frame diff — CONFIRMED
- MI+0x1198 (frame): 15544,15715,15898 (+171,+183 over 60 frames ⇒ ~57 Hz).
- MI+0x119C: 30,201,384; MI+0x11A0: 588,40,223; MI+0x1178-0x1194 (4 dwords) change every frame (timestamp-ish). MI+0x1168 play/per constant.
- MA list span MA+0x22E4C (22×200): changes every frame (id rows + d² both change, e.g. row0 id 5→5→1, d² 3.4→52.3→66.9).
- MA ranks @+0x24710 (22 bytes): change every frame (e.g. 8e8d19… → 14566f… → 56536d…).
- MA ball cluster @+0x2482C: changes every frame (−47.96→−33.52→−17.25 X).
- MA carrier @+0x2601C: frame0==frame2 (changes only some frames).
- All 22 match::Player blocks (0x4000 each) change every frame (position floats at +0xD0/+0xD8 etc.).
- Pump ring: idx 12→23→15, count 3→3→2; all 32 slots rewritten; record timestamps spaced 17544 µs (= one frame). P+0x4190 (accumulated time) advances 3.001M/3.222M µs over the gaps (~17.5 ms/frame).
- Conclusion: the frame writes MI frame, positions, MA list/ranks/ball, time base, and pump records every frame. [CONFIRMED]

### Time base — CONFIRMED
- Full pump `0x141B3E060(rcx=pump, rdx, xmm2, xmm3)`:
  - xmm2 (frame dt): `0x141B3F0D0` = `(QPC − [pump+0x41A0]) − [pump+0x41A8]` then clamped, and callers do `addsd xmm2, [pump+0x4190]`; i.e. dt = (now_us − last_us) + accumulated.
  - xmm3 (time): `0x141B3F050` = QPC-derived microseconds (ticks/10, freq 10 MHz).
  - Constants: 2^63 double @0x14253EE40, clamp 9.223e18 @0x14253EE38, divisor 10.0 @0x1437F00E0, freq 10,000,000 @0x1437F00F0.
  - QPC getter: `0x141B34A30` (QueryPerformanceCounter import @IAT 0x14252F3F8 = WaitForSingleObject? no — the wait import; QPC import resolved by LazyMap to a model returning 0 in our harness).
  - `0x141B3F140` gate returns 1 when pump+0x4198 ≥ 3 pending slots.
- Live: P+0x4198 cycles 3,3,2; P+0x4190 advances ~17544 µs/frame.

### Emulated run — PARTIAL / NOT ADVANCED
- H step `0x14041ECF0(rcx=H, rdx=0)` and rdx=0x28: both run clean, idempotent, 0 writes to MI/MA/positions. CONFIRMED (Pump's report reproduced).
- Full pump with live/advancing QPC and P+4198=5: enters the record loop (0x141B3E180) only when `WaitForSingleObject` is modeled as signaled (return 0x102); otherwise spins in `0x141B3E146` (condvar wait). Even when signaled + advancing clock, pump returns clean but:
  - MI frame stays 15544 (no change),
  - idx 12→14, cnt 3→1 (consumed 2 records),
  - 0 writes to MI/MA/E/P0 regions,
  - full census shows 6.2M writes, all emulator-stack/pool noise; game-heap pages (0x2xx/0x3xx) get 192–528 writes each but none into the MI/MA/P0 objects.
- `0x141B3EEF0` drain: consumes ring (cnt 3→0) with no listener dispatch (reached none of WATCH) and no frame change.
- Record processor `0x140A115A0` with forged type-11 record: runs but faults (unmapped read 0x0) and never reaches the inc at `0x140A128E4` because forged record lacks sub-structure ([REC+0x31A]=0 gate).
- With a REAL snapE (E count=1, head=1) and no forging: `0x140A13060` runs ×2, `0x140A115A0` ×2, `0x140A11290` ×2, `0x140A128E4` inc NOT reached, frame frozen, UcError "unmapped read at 0x0".
- Conclusion: no emulated path advanced MI+0x1198 or positions. The pump consumes records but the frame advance lives in the producer-side record processing (0x140A115A0) which needs real detector-slot layout not yet decoded. [CONFIRMED reason: consumer side only; producer/record-processor not entered with valid data]

### Writers — PARTIAL
- S4 nearest-list writer: NOT found in any exercised path (H step, MatchSub type-2, forged analyze). CONFIRMED zero writes to MA+0x22E4C span and whole MA 0x27000 across 50 H steps. The builder is off-tick (pump/phase-change). Candidate entry: pump dispatch `0x141B3DD80→0x141B3EA30→0x141B3EDD0` with a non-type-2 record, or setup/phase-change path. [OPEN]
- Frame counter writer: `0x140A128E4` (`inc dword [rsi+0x1198]`), gated by `[r14+0x31A]`, inside the record processor `0x140A115A0` (type-11 body). The holder global at `0x1436F3DBA` is null in adv0 — needs a live setup capture where the detector system is active. [OPEN]
- Position writers: no direct writer identified; the 133 dispatches write game-heap pages 0x2a7/0x2c0-0x2c9/0x333/0x360-0x36b/0x3d5-0x3d6 but never the MI/MA/P0 objects directly. The player blocks change in live diff but the emulator's pump dispatch did not touch them. [OPEN]

## (c) Specs/interfaces defined
- Time base for `0x141B3E060`: xmm2 = `[pump+0x4190] + (QPC_us − [pump+0x41A0]) − [pump+0x41A8]`, clamped at 2^63; xmm3 = QPC_us; rdx=0 or 1 (flush flag). Gate `0x141B3F140` requires pump+0x4198 ≥ 3.
- Pump ring: 32 slots × 16 B at container+0x40000, idx @+0x40204, count @+0x40208; dispatch via `0x141B3CEB0` (hub walk: lo7 type, bits7-16 channel, stride `[rsi]>>0x11`).
- Record processor `0x140A115A0(D-holder, E+0x180C60, record, MI)` with switch on `[record]-1` (types 1–12 case bodies; types 13+ map to far addresses); type-11 body contains the frame inc.
- Live frame diff table (above) as the differential-test oracle.

## (d) Code changed
None in the repo (read-only rule). All harnesses in `<workdrive>/gf-Advance-scratch/`: framediff.py, objdiff.py, fielddiff.py, manpos*.py, pumpstate.py, fullpump.py, fullpump2.py, hstep.py, advance.py, ringwatch.py, drain.py, loop.py, gen4.py, signal/sigwork/sigloop.py, slot11.py, trace.py, census.py, snapE.py. Snapshots (PES-derived, never committed) in `snap/`.

## (e) Tests run / results
- 3-frame open-play diff (frames 15544/15715/15898): MI+MA+player blocks all change; per-frame hashes differ. CONFIRMED.
- Determinism: not re-tested here (Pump's det3.py covers H step 3× byte-identical); pump signaled runs reproduce the UcError deterministically.
- No build/test binaries (read-only RE task).

## (f) Fidelity metrics
- Live frame rate ~57 Hz (17544 µs/frame). Frame counter +171/+183 over 60 frames.
- Emulated: frame advance 0 (target not met). Ring consumption 3→0 (drain) and 3→1 (pump) confirmed; 133 listener dispatches confirmed with 0 game-object writes.

## (g) Unresolved / blockers
1. Producer-side frame advance (the `0x140A115A0` type-11 path) needs a real detector slot with valid sub-structure; snapE (E count=1) is available but the inc still skipped on the `[r14+0x31A]` gate and faults on a null sub-pointer. Need the slot record layout (E+0x26FF0/26FF4) decoded.
2. S4 nearest-list writer remains OPEN (off-tick builder).
3. Which hub/listener table serves match types 13–61 (t=3 junk-holder caveat).
4. Per-player dispatcher as a dispatched sequence (queue caller unproven).

## (h) Recommended next highest-value step
Decode the detector slot layout from snapE (E+0x26FF0/26FF4/0x271F4) and forge a complete type-11 record whose `[rec+0x31A]≠0` and sub-pointer is valid, then re-run `0x140A13060` to capture the inc firing and its `rsi` identity (MI vs shadow), naming the frame writer and its call stack. That is the gate to making MI+0x1198 advance in emulation.