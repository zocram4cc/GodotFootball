# pes-port-RecordMap: chain record layouts from live captures; four slices still blocked (28-10-26)

Branch `RecordMap` at `3091f83f` (from origin/main `d9b80ea2`); `git ls-remote origin RecordMap` = `3091f83f0ae4b14a041ec3dd4185f1c130d6ce8b`. Worktree `<workdrive>/gf-RecordMap`. Layouts, field→GF table and open items: `pes-re-record-layouts-chains.md` (Main archives it as `docs/re/pes-re-record-layouts-chains.md`, see §5).

## 1. Verdict

The layouts are mapped and partly pinned. The four slices are **not** unblocked, because each still needs a record that has no capture or a field with no writer:

- **KickInputs:** the kicker `ap` is not a G block start (refuted 0/12). The kick inputs are readable at their PES offsets, but their writers and meanings are still open. No GF change.
- **TrapClipPort:** no capture of the trap chain exists (no hook). Offsets are mapped from the image only. No GF change.
- **EndClear2:** the Clear unit, the carrier id, the imageA danger byte, the angle ctx `+0x18`/`+0xE0`, and the E0 ring domain are pinned. The Clear carrier record's owner and the G+0x180 stride are not. No GF change.
- **SanctionSwitch:** no sanction capture (`foul_sanction_47F170` has 0 captures). The E0 ring domain is pinned through the angle ctx; A1 and MI are mapped only. No GF change.

No GF behaviour changed. No test binary was built or run, because the ticket is a layout and tooling change.

## 2. Acceptance, per record

| Record | Size (lower bound) | Fields CONFIRMED from captures | Header |
|---|---|---|---|
| G anime/man/slot blocks (G tables) | block 0x3F40 for anime 0..20; man +0x32C (0..20); slot +0x10F0 (0..19) | yes (12/12 kick captures) | `pesrecordmap.hpp` asserts the constants against `pesworldrecord.hpp` |
| kicker `ap` (rcx of 0x14083CC30) | 0x32E8 | offsets readable 12/12; `+0x2752` in [-1,150]; `+0xAB8` image-resident | `PesKickerRecord`, 14 offsets asserted |
| action interface (`ap+0xAB8`) | 8 (vtable) | vtable in the image 12/12 | `PesActionInterface` |
| trap anime/action/query | not pinned | no | none (OPEN) |
| angle ctx (A0 / unit+8 / sanction A1) | 0xE8 | `+0x18`, `+0xE0` readable 6/6 | `PesCtxRecord` |
| E0 ring header (ctx+0xE0) | 0x1904 + 4 | count 8 (saturated) and head 0..7, 6/6 | `PesE0RingHeader` |
| Clear unit | 0x30 | `+0`, `+8`, `+0x28` pointers 5/5 | `PesClearUnit` |
| carrier record (unit+0) | 4 | `+0` u32 id 0..21, 5/5; not a G table entry (0/5) | `PesCarrierRecord` |
| imageA | 0x31C2 | `+0x31C1` in {0,1}, 5/5 | `PesImageA` |
| G+0x180 entry | 0x6E (stride 0x74 not pinned) | readable 6/6 | `PesCarrierTableEntry` |
| sanction A1, MI (G+0x270) | 0xE8 / 0x15B9 | no | none (OPEN) |

Field→GF table (the work list for cutover): layouts doc §7. Summary: GF has the man facing, pos x and vel x (written at `teamAIcontroller.cpp:3418-3425`), the anime class, the slot anchor x, and the E0 ring (`pesevents.hpp:20`, 800-byte slots, count +0x1900, head +0x1904). GF has none for the kicker inputs, the Clear unit or carrier, the trap records, the sanction A1, or G+0x40 and G+0x180. The kicker's `+0xAB8` and `+0xAC8` are stand-ins (`pesmovestep.cpp:176,272,380`, `humanoidbase.hpp:253`).

## 3. Findings that change the plan

1. **GF's 22-block assumption is wrong for the tail.** Live G tables: anime entries 0..20 are 0x3F40 blocks; slot entries 0..19 sit at anime+0x10F0; entries from 21 (anime) and 20 (slot) do not follow the block (slot 20 minus anime 20 = 0xB2EF0). `pesworldrecord.hpp` (`kSlotCount = 22`) indexes 22 blocks. OPEN: which objects are the tails.
2. **The kicker `ap` is not a block.** P (rcx of 0x14083CC30) is not a G anime entry in 12/12 captures and sits outside every block. The GF plan of putting `ap` in the slot block is refuted for the kick chain.
3. **The Clear carrier record is not a G table entry** (0/5 across anime, man, slot, side A and B). Its owner is OPEN.
4. **Anime `+0x1A`/`+0x1C` conflict:** the live reads are on the anime entry, but GF names those offsets `kSlotLoOffset`/`kSlotHiOffset` in the slot record (`pesworldrecord.hpp`). One of the two is wrong. INF, OPEN.
5. **Kick doc gate:** the kick doc tests `ap+0x2764 < 360`; live value 720.0 in seq017. Its meaning needs a decode, not the gate as written.
6. **G+0x180 stride is not pinned** (mutation to 0x78 still passes; the check only proves readability).

## 4. Verification run

- Access map: `python3 tools/dev/translate/recordmap.py analyze --image ~/ghidra-projects/scratch/Unpacker/pes_full.bin --out <scratch>/analysis.json`: closure 357 functions, 48 top levels, 3146 unresolved memory operands, 522 indirect calls (not covered by the map).
- Merge: `recordmap.py merge ...`: 19 records, 204 fields written under `fieldmap.json` key `recordmap`. The other keys are unchanged (checked against the copy taken before the merge).
- Pins: `recordmap_pin.py --live .../live2/pid407792 --max 6`: kick 6+6 captures, Clear 5, angle 6. Result: every check passes except the two refuted ones (ap block start 0/12; carrier table entry 0/5).
- Mutation checks, run in-process with the module constant overridden and no file edit (all reverted): `MAN_OFF` 0x32C→0x330 fails "man table"; `SLOT_OFF` 0x10F0→0x10F4 fails "slot table"; `RING_HEAD_OFF` 0x1904→0x1905 fails "ring head"; `CARRIER_TABLE_STRIDE` 0x74→0x78 does **not** fail (stride not pinned).
- Headers: `g++ -std=c++17 -fsyntax-only -Wall -Wextra -Wpedantic -x c++ src/pes/pesrecordmap.hpp` OK; an include-and-link program built and ran with `-I.`. No warnings except `#pragma once` when the header is compiled as the main file (expected).
- PII: `tools/check_pii.sh` not run (fails in a worktree because of the godot-cpp symlink). I grepped the four committed files for machine paths: none. Main should run the script from the main checkout.
- Gate: not run (Main gates). Test binaries run: none (no GF code or test changed).

## 5. Deviations and notes

- **File name:** the ticket asked for `docs/re/pes-re-record-layouts.md`, which already exists (348 lines, RecordLayoutRE: A3E190 ring, slot record, 6E150 class byte, need chain). It was not overwritten. The new doc is `pes-re-record-layouts-chains.md`, so Main's archive will not collide with it.
- **Misplaced write (disclosed):** my first `recordmap.py` write used a relative path and landed in `<local>/Code/GameplayFootball`, a different checkout from `<workdrive>/GodotFootball`. I moved that one file into the worktree (`mv`) and changed nothing else there. Main's checkout and its `.git/config` were not touched.
- **Submodule:** `git status` in the worktree errors on the godot-cpp symlink (`expected submodule path ... not to be a symbolic link`). I staged by explicit path. Commit identity as the brief says.
- Scratch (PES-derived, not committed): `<workdrive>/gf-RecordMap-scratch/` (`analysis.json`, `pin.json`, `pin.txt`, `records.txt`, `fieldmap.orig.json`).

## 6. Open items (numbered; each names what settles it)

1. Tails of G's tables (anime 21..25, slot 20..21): decode which objects they are. Settles the 22-block assumption.
2. Kicker `ap` owner: decode what its +0 pointer names and who allocates P. Needed before any GF kick field can go in a PES record.
3. Trap chain: hooks at `0x14079A470` and `0x14079AFD0` (windows in the layouts doc §8.3). Settles the action and anime layouts.
4. Sanction: hook `0x140A81230` plus a foul in the testbot match to fire `foul_sanction_47F170` (0 captures so far). Settles A1, MI and the E0 identity.
5. G+0x180 stride: a second pin with ids whose table bytes differ.
6. ANALYZE identity (angle A0 = unit+8 = sanction A1): a Clear call and an angle call in the same frame with equal ctx pointers.
7. Clear carrier record owner (`unit+0`, not a G table entry).
8. Anime `+0x1A`/`+0x1C` conflict with `kSlotLo/HiOffset`.
9. Kick-doc gate `ap+0x2764 < 360`: meaning, given 720.0 in a live capture.

## 7. Hooks (Main owns `hooks.cfg`; not edited here)

Candidate windows, read from `pes_full.bin` (`hook_plan.py` must check each window: no branch, no RIP-relative operand):
- `hook trap_kind0_79A470 0x14079a470 14 1 3000 488bc44c89482055415441554156`
- `hook trap_commit_79AFD0 0x14079afd0 14 1 3000 488bc4554154415541564157488d`
- `hook sanction_card_kind_A81230 0x140a81230 14 1 3 40534883ec20418bc183fa020f84`
- Existing `foul_sanction_47F170` and `foul_sanction_caller_480910` need a foul in the match to fire (0 captures in `live2/pid407792` and `live2/pid161465`).

## 8. Blockers for Main

- Hooks in section 7, or a foul in the testbot match, to unblock the trap and sanction slices.
- Confirmation whether the 22-block assumption is used by anything that will cut over (GF code indexing slots 20 and 21 through `pesworldrecord.hpp`).
