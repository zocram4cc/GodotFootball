# PES minimum-players rule (threshold 7): VA NOT FOUND, patch NOT applied

> **Superseded 22-10-26:** located: docs/re/pes-re-missing-systems.md §2.13 (FUN_140447B50 case 6, fewer than E+0x17 = 7 on the pitch).

Status: **BLOCKED on the VA.** The check, its threshold and its writer were not located in the image, so nothing was ported against an unconfirmed VA (brief §Inputs). No live writes were made. Branch `RedCardRule` is pushed (`git ls-remote`: 989f9eeeb599e269b287f75a0c41e2049f22e2a9) at `origin/main`, with no commit of its own, because there is no script change to carry.

Verdict key: CONFIRMED = read this session (decompile, disassembly, or byte scan of the install image); INF = inference; OPEN = blocker named.

## 1. What was asked

- The function that counts a side's remaining players, and the threshold 7.
- What it triggers (abandon).
- A `--no-min-players` flag in `tools/dev/pes_endless_match.py`, with revert, idempotence and readback.
- Live application and verification.

Docs checked: `pes-re-A5Judge.md` (stop-reason table §0 and §1.1, whistle writer §2, dispatcher §3.3), `pes-re-A6Period.md`, `pes-re-A6Writers.md`, `pes-re-clock-live.md` §8, `pes-re-foul-choreography.md` §9, `pes-re-duel-sim.md` §4, `pes-re-duels-fouls.md` §2.1, `pes-re-cutscenes.md` 0x24. **No existing doc names a minimum-player rule, a threshold of 7, or an abandon writer.** The only abandon mention is the A5 table row for stop reason 10, which has no writer.

## 2. Image and method

- Install `PES2021.exe` (PID 2510857, `<workdrive>/PES21`). Ghidra bridge on `pes_full.bin`, base 0x140000000. Used `xrefs_to` and `decompile_function`. `get_xrefs_to` is not on the bridge.
- Image layout (`pes_full.bin.maps`): `.text` 0x140001000–0x14252f000; a large RWX region 0x143d14000–0x15b952000 holding the code the docs call "protected" (Denuvo, per `pes-re-dead-code.md`). Ghidra does not decompile that region. Its bytes are readable, so it was scanned with capstone.
- Scripts (scratch, `<workdrive>/gf-RedCardRule-scratch/`): `dispsearch.py` / `dispsearch_all.py` (every instruction with a memory operand at a given displacement, full image); `callsites.py` (every E8 rel32 call to a target, full image); `countscan.py` (count compares near indexed `+0x1748` reads); `disasm_at.py`.

## 3. Findings

### 3.1 Consumers of the abandon path (CONFIRMED)

| VA | Instruction | Meaning |
|---|---|---|
| 0x14044eb4e | `cmp dword [r13+0x1574], 0xa` | foul/offside builder (FUN_14044eb10) tests stop reason 10 (abandon) |
| 0x14044eb37 | `cmp byte [r13+0x1775], 0` | same builder, mode test |

Stop reason `+0x1574` is the field the A5 doc calls MI+0x1574 (`JR+0x20`). The 0xa test is the only place the value 10 is consumed.

### 3.2 Writers of the stop reason (CONFIRMED: no immediate 10)

- Setter `0x141b8b250` = `mov dword [rcx+0x20], edx; ret` (rcx = MI+0x1554 record, so +0x20 = 0x1574). All calls to it over the full image were enumerated (`callsites.py`): 40+ sites.
- Immediate values passed by the sites whose argument is set just before the call: 1 (0x141b90dea), 6 (0x140453a4a), 7 (0x1404a5936, 0x1404a6e74, 0x1404a7096, 0x140447f31, 0x140452e90), 8 (0x140454829, 0x140454e46), 2 (0x140447f31 pre-write chain).
- Values taken from records or computed (not immediate): 0x140d1d770 (`+0x1ac0`), 0x1410a7ace (`[rax+0x1ac0]`), 0x1413e7bbd (`[rax+0x1b0c]`), 0x1420bd7e5 (`[rbp+0xe0]`), 0x1413eeadf (`[rax] >> 0xe`), 0x1413d7555 (computed), 0x14050ebc3 (inside the whistle writer, `mov edx, r14d`).
- The whistle writer FUN_14050e770 takes reason as its 4th argument. Its direct callers in `.text` pass 0 (kick-off, FUN_14050c490 at 0x14050cba2, decompile line 262), 4 (FUN_14050e020, FUN_14050e320; A5 §2.1), and 9 (FUN_14050cc40, PK shoot-out; A5 §2.1). The two protected callers pass 1 (period end, 0x143f5530b) and 5 (foul commit, 0x143f546f0; both per A5 §2.1). The restart argument of FUN_14050e110 is 1; its reason argument was not read. **No caller found passes 10.**
- Direct stores to `+0x1574` (`mov ... [reg+0x1574], imm`): none in the full image.

**Result:** no stop-reason writer with an immediate 10 exists. Reason 10 is consumed at 0x14044eb4e and never produced by an immediate store. The record-sourced writers (0x1410a7ace, 0x1413e7bbd, 0x1420bd7e5, 0x1413eeadf) could in principle carry 10; their source records are not identified.

### 3.3 Mode byte `+0x1775` writers (CONFIRMED)

- Setter `0x140a3ccf0` = `mov byte [rcx+0x1775], dl`. Its only caller is `0x141fb1903` in FUN_141fb1880, which passes 3 (match-start reset).
- Direct store `0x1404144e2` = `mov byte [rbx+0x1775], al`, inside a block copy of the match record (`0x140414380–0x1404144a0` copies `+0x168c…+0x1744`). A snapshot copy, not a new value.
- No other store to `+0x1775` in the full image. Modes 0x0b–0x0f (the abandon builder FUN_14044ffd0 per the A5 table) are never written by an immediate.

### 3.4 On-pitch flags and counts (CONFIRMED structure; count check NOT found)

- On-pitch byte array `MI+0x1748[0..21]`. Setter `FUN_140a3cac0(rec, slot, bool)` (`mov byte [rax+rcx+0x1748]`). Callers: FUN_140422030 (per-frame sync), FUN_140a43dc0 (named-object toggle), FUN_140a56000 (match load), FUN_141fb9e20 and FUN_141fba600 (reset to all-on), FUN_141fd1b90 (scene event 0x1d). None is a sending-off site.
- Predicate `FUN_140a59ec0 → FUN_140a59ca0`: per-slot lineup test against an 11-entry list per side (`team+0x44`, `team+0x8204`). The list count is at +0 (capped at 11). A candidate for "players remaining", **not verified** as the rule's count.
- Keeper red-card path: FUN_140523a10 (MI+0x158c == 4, or 3 with booking ≥ 2; slot 0) → FUN_140523210 (replacement). A substitution, not an abandon.
- `cmp` against 6 or 7 near on-pitch loops (countscan): 64 candidates in `.text`, none tied to a stop write. Protected region: six indexed `cmp byte [..+0x1748], 0` sites (0x143f24143, 0x143f668ca, 0x143f6a857, 0x144866acd, 0x15b6ee50e, 0x15b6efba1). Two were read (0x143f668ca, 0x143f24143): player-selection predicates, no count.

### 3.5 Threshold 7 (OPEN)

No compare against 7 tied to a player count and an abandon write was found. `cmp ... , 7` on `+0x1574` exists at 0x1404a5671 and 0x14087a095; these are not player counts.

## 4. Why it is blocked

1. The abandon consumer (0x14044eb4e) has no immediate writer of value 10, and no mode writer of 0x0b–0x0f, in `.text`. The writer, if it exists, is in the protected region (not decompilable by Ghidra) or reached through record-sourced values not yet traced.
2. The protected region's bytes are readable (0x143f546f0 decodes as the judge commit), but are not analysed.
3. The threshold 7 has no source: not in the brief's docs, and not found as a compare.

## 5. Acceptance vs. outcome

| Item | Result |
|---|---|
| Check VA CONFIRMED | **No** (OPEN, §3.5, §4) |
| Threshold 7 CONFIRMED | **No** (OPEN) |
| Patch applied and verified live | **No** (no VA to patch; no live write made) |
| Script change pushed | **No** (nothing to write without the VA). Branch `RedCardRule` pushed at 989f9ee so the branch exists. |

No live PES write. No `/tmp` writes. Scratch in `<workdrive>/gf-RedCardRule-scratch/`.

## 6. What would settle it

1. **Live, read-only:** during any live red card or player-count drop, poll `MI+0x1574`, `MI+0x1775`, `MI+0x1554` and the per-side on-pitch flags (`MI+0x1748`, 11 per side) each frame. If the stop reason is 10 or the mode is 0x0b–0x0f at a stoppage, the writer is the one active at that tick. Read-only per brief.
2. **Static, protected region:** identify the source objects of the record-sourced writers at 0x1410a7ace, 0x1413e7bbd and 0x1420bd7e5, and check whether any holds a constant 10.
3. **Owner decision:** if the rule cannot be observed live in this build, the flag cannot be implemented honestly. Confirm that the goal is to remove an abandon that exists (live-observed), not one assumed from the brief.

## 7. Confidence

- CONFIRMED: the consumers and writers in §3.1–3.3 (VA and instruction).
- INF: the record-sourced writers may produce 10 (§3.2).
- OPEN: the threshold, the count, and the writer (§3.5, §4).
