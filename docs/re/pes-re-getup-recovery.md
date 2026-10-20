# Get-up recovery: when may a defender slide or tackle again? (S1Watch, rounds 19–20)

> **Main audit 19-10-26:** `0x14072EEFD` is the call inside FUN_14072EA00 (the rise blend set): matches. No fixed get-up timer: the next slide/tackle is gated by the current clip's +0x70 frame test (FallDown FUN_14072E360, Sliding FUN_140787EC0, Tackle FUN_140790160).
> **Main audit 19-10-26 (round 20):** `0x1406EADE0` `lea rax,[rcx+0x19a4]; ret` on the secondary (anime+0xAB8): the phase byte is anime+0x245C. The release at the stand-up end is not found statically (needs a live match); PARKED.

Status: static. **The slide predicate has no explicit get-up gate.** Its refusals are the state, class and geometry tests of defender-decisions §3. The get-up's timing is a rise-up gate in the contact layer (§3), and the transition answers depend on a frame test that reads one phase byte (§7). Tags: CONFIRMED = decompile or image read; INF = inference; OPEN = not resolved.

## 1. The slide predicate (CONFIRMED structure, defender-decisions §3)

Entry `FUN_140971B60 → FUN_140971BA0` (`pes-re-defender-decisions.md` §3; decompile not re-read in full). The refusals in the doc:
- open play (`MI+0x116C == 1`), own anime not in class `FUN_140A6E4B0`, `MI+0x1198` odd;
- no newest-4 event of type 0xB within `10·gs + 0.5` frames;
- team capability 8 (`FUN_140A604D0`), CPU team (`T+0x255 == 0`);
- own anime not in classes `FUN_140A6E0D0`, `FUN_140A6E3C0`, `FUN_140A6E5E0`; not `FUN_140A6E300`, and ball not > 0.5 m high 10 frames ahead;
- distance `d ≥ 4.0 + 0.8·L`, heading and bearing tests, teammates in SLIDING/SLIDING_KEEP, opponent capsule test, `FUN_140A539D0`, `FUN_140A53690`.

No term in this list reads the fall, stagger or stand-up state directly.

## 2. The class tests the slide predicate uses (CONFIRMED, image read)

Each is a bit test on the per-class word at `DAT_1426484F0 + 4·class` (class = the clip-class byte of the current clip; test `class ≤ 0x5D`):

| function | test | bit |
|---|---|---|
| `FUN_140A6E300` | `word >> 15 & 1` | 15 |
| `FUN_140A6E0D0` | `word & 0x2C0 != 0` | 6, 7 or 9 |
| `FUN_140A6E5E0` | `word >> 5 & 1` | 5 |
| `FUN_140A6E3C0` | `(anime+8) bit 13` (any class), else `word >> 13 & 1` | 13 |

Class words (CONFIRMED): class 4 `0x4008` (bits 3,14); 5/6 `0x400C`; 7 `0x4020` (bits 5,14); 11/12 `0x4080` (bits 7,14); 13 `0x4040` (6,14); 14 `0x4100` (8,14); 15 `0x2000` (13); 16 `0x3002` (1,12,13); 17 `0x3000` (12,13); 72 `0x8000` (15); 91 `0x2` (1).

Bit meanings and the stand-up class are not labelled in the docs (OPEN).

## 3. The get-up rise gate (CONFIRMED, contact doc §4.3, FUN_14072C5C0)

- Rise-up starts at the fall clip's end frame (window +4/60 s) only if the ball, predicted 1 s ahead, is within 10 m, or the fall kind is 4, or `FUN_140A638C0`. Never during a stoppage or a pending Judge decision (`FUN_140A53F90`).
- Rise-up rate `0.95·(1−b) + 1.1·b` with `b = clamp((Balance−40)/60)`, blend 0.3 s (`FUN_14072EA00` @0x14072EEFD).
- After the rise, the ball owner goes to DRIBBLE; others to MOVE; an injury check may switch to `DEMO_INPLAY_CONTACT` (0x50).

## 4. The transition answers for a man in a fall or stand (CONFIRMED structure; corrected in round 20)

- **FallDown** (action 17, vt `0x1425DF250`, `+0x70 = FUN_14072E360`), **Sliding** (action 11/12, `+0x70 = FUN_140787EC0`), **Tackle** (action 13, `+0x70 = FUN_140790160`).
- **Correction (round 20):** the byte the FallDown and Sliding tests branch on is **not** `anime+0x20`. The call `(**(code**)(*sec+0x20))(sec)` goes to the secondary vtable slot `+0x20` = `FUN_1406EADE0` = `lea rax,[rcx+0x19a4]`. Here `sec = anime+0xAB8`, so the byte is **`anime+0xAB8+0x19A4 = anime+0x245C`**: the per-action phase byte (§7). The `+0x20` in my earlier note was the vtable index.
- The frame test (CONFIRMED, decompile `FUN_14072E360`): `iVar1` = entry count (`rec+5`, via `FUN_1406AFBC0`); `iVar2` = whole-clip frames (`FUN_1406AF620`). If phase == 2: when `iVar1 < 1`, compare `iVar2`; else `iVar2` = frames of entry 0 (`FUN_1406AF8E0(…, idx 0)`). If phase ≠ 2: when `iVar1 < 1`, compare `iVar2`; else `iVar2` = frames of entry `iVar1−1` (the last entry). Allow iff `iVar2 ≤ sec+0x38`.
- `sec+0x38` = `FUN_1406E6960` → `FUN_1406E68C0(clip)`: the clip's entry in the anime's current motion table (count `+0x420`, clip id `+0x354+k·0x14`, duration `+0x35C+k·0x14`), returned as `trunc(54·dur + 0.5)` frames; 0 if absent.
- The other frame-count helpers (CONFIRMED decompile): `FUN_1406AF8E0(db, clip, speed, idx)` = `(2·f / speed)·(fps/60) + 0.5` with `f = (u32>>13) & 0x3FF` from entry `idx` (`rec+0xA0+8·idx`); `FUN_1406AF620(db, clip, speed)` = the same from the clip header (`rec+0x18`, bits 10..19).

So the refusal reason at the end switch is the frame test: the selected track's frame count must be ≤ `sec+0x38`. FirstTouchPort2 observes `sec+0x38 = 0` at the stand-up end. The test then passes only if the selected track has 0 frames.

## 5. Rule: which track a phase selects (CONFIRMED, §4)

- Phase 2 selects entry 0; any other phase selects the last entry (`count−1`). If the count is 0 both fall back to the whole-clip frame count.

## 6. Open (rounds 19–20)

1. The stand-up clip id, its class and its track counts (the data is in the motion record; not read this round).
2. The writers of the phase byte in the FallDown class. The raw store scan finds 24 `[reg+0x245C]` byte stores, all in other action handlers (`0x14071A250`, `0x14071AB70`, `0x14071B350`, `0x14077E2D0`–`0x1407AA4E0`, `0x1406B8E40`, `0x140726AD0`). None is in `0x14072xxxx` (FallDown/Stagger code). The secondary `+0x19A4` form has 5 stores, none in the stand-up area.
3. The Stagger `+0x70` (`FUN_14072E450`).
4. The live test: a defender on a stand-up clip, the phase byte and `sec+0x38` at each frame, and the first frame a slide is allowed.

## 7. The phase byte `anime+0x245C` (round 20)

**Identity (CONFIRMED).** `anime+0x245C` is the per-action phase byte. The vtable accessor is `FUN_1406EADE0` (`lea rax,[rcx+0x19a4]` on the secondary). It is read by the generic AI dispatch `FUN_14072C7A0` (raw asm, no Ghidra header):
- `call [rax+0x20]` → phase byte; `movzx r9d, byte [rax]`;
- phase 0 → `FUN_14072F3B0` (the three-branch chase/kick dispatcher, §1 of `pes-re-kick-approach.md`);
- phase 1 → `FUN_14072EA00` (the post-fall/rise handler; it issues the clip with `SetMotion` at `0x14072EE05` and reaches the TRAP admission `FUN_1406B4580`, and the rise-rate `0x14072EEFD`);
- phase 2 → `call [vt+0xC8]` of the action, then exit.

**Values (partly CONFIRMED).** The byte is shared by several action families, so its values depend on the current action:
- Kick and corner (`pes-re-fox-registry.md`, `pes-re-kick-align.md`): phase 0 → 1 → 2 sequences the strike (`FUN_1407302B0` / `FUN_140733A30`).
- MOVESTEP (`pes-re-anim-select.md` §2): states 0..23 (`switch(*(P+0x245c))` in `FUN_1406BA900`), where state 23 is excluded from one test.
- Live values in play (docs, `pes-re-gaps-anime.md` §5): {0, 1, 3, 9}.
- FallDown/Sliding: the test uses only the value 2 versus "not 2". So for a stand-up, "2" is the phase that selects entry 0.

**Writers (partly CONFIRMED).**
- Byte stores to `[reg+0x245C]` (24 sites, from `raw661.py 245c`): `0x1406B9142` (`FUN_1406B8E40`, MOVESTEP), `0x1406EC0C9` (`FUN_1406EBF90` — a dword store inside a ball-zone bool test, a false match for the setter), `0x14071A9C6` (`FUN_14071A250`), `0x14071B348` (`FUN_14071AB70`), `0x14071B846`, `0x14071BBCA` (`FUN_14071B350`), `0x1407265B8`, `0x140726B28` (`FUN_140726AD0`), `0x14077E34B` (`FUN_14077E2D0`), `0x14077E841` (`FUN_14077E770`), `0x14077EF79`, `0x14077F400` (`FUN_14077EA40`), `0x14077F75B` (`FUN_14077F480`), `0x14078020D` (`FUN_14077FFB0`), `0x140780965`, `0x140780CF1` (`FUN_140780870`), `0x140781759` (`FUN_1407816B0`), `0x140784808` (`FUN_140783FF0`), `0x14078A668` (`FUN_14078A640`), `0x1407A2074` (`FUN_1407A1F30`), `0x1407A2E18` (`FUN_1407A2D80`), `0x1407A3521` (`FUN_1407A3490`), `0x1407AB7D0`, `0x1407ABAE5` (`FUN_1407AA4E0`).
- Resets: `FUN_1406EB670` writes 0 (docs, `pes-re-gaps-anime.md` §4); `FUN_1406EB440` clears to 0 every tick (docs).
- **Not found:** a writer inside the FallDown/Stagger/Rise functions. So the release at the end of a stand-up is not a store to this byte in those functions.

**The release branch (INF, not proven).** At the end switch, `sec+0x38 = 0` (FirstTouchPort2). The FallDown test passes only when the selected track has 0 frames. A phase-2 man selects entry 0, so a release needs entry 0 to be zero-length; a non-2 phase selects the last entry. Which phase the stand-up holds at its end, and which track is zero, are open. The decisive live check: read `anime+0x245C`, the entry count, and the entry frame counts of the stand-up clip, on the frame of the switch.

## 8. Answer to Main's question

- The byte is `anime+0x245C` (accessor `FUN_1406EADE0`), not `anime+0x20`.
- Meaning: the per-action phase. For FallDown/Sliding the frame test uses only "== 2" versus "≠ 2", and "== 2" selects track 0 while the other values select the last track.
- Writers: 24 byte-store sites in other action handlers (listed above); none in the FallDown/Stagger code. The release writer is not found.
- The release is a frame test that compares the selected track's length with `sec+0x38`. At the stand-up end, `sec+0x38 = 0`, so only a zero-length selected track releases. That zero-length case is unverified.
