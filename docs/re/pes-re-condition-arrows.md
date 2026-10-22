# PES condition arrows: what the bytes confirm (22-10-26)

> **Source.** The owner relayed a separate RE agent's answer to "does a higher spot on the player ID list give better odds
> of a green condition arrow?". Main checked each receipt against `pes_full.bin`; results below. Verdict: list position
> is not an input anywhere that was checked. The arrow's writer and band thresholds are still open.

## Confirmed at the bytes

- **The consumer reads the arrow from record+0x64:** `0x14092B4D6 mov ebx, dword ptr [rax+0x64]`.
- **Green writes nothing:** `0x14092B59B cmp ebx, 5; jge` and `0x14092B5A0 cmp ebx, 2; je` both jump past the
  multiplier loop. Index 2 (green, ×1.0) and out-of-range indices skip the stat write.
- **The multiplier table is at `0x142632D70`**, read at `0x14092B5D4 mov eax, [rbx + rcx*4 + 0x2632D70]` (rbx = image
  base), indexed `5·tier + arrow` (`0x14092B5C7 lea rax, [r11 + rcx*4]; add rcx, rax`). The tier byte and the target stat
  index come from an 8-byte list walked from `0x142632DB4`; the multiplier is written to `stats + 0xC888 + 4·index`.
  Rows, script-read:
  - tier 1: 1.12, 1.06, 1.00, **0.92**, 0.84 (the relayed 0.93 is wrong)
  - tier 2: 1.09, 1.05, 1.00, 0.94, 0.88
  - tier 3: **1.06**, 1.03, 1.00, 0.94, 0.88 (the relayed 1.07 is wrong)
- **The roll shape (`FUN_14089BAE0`):** `value = base · (100 − r + RNG(100) · r · 0.02) / 100`, with `0.02` at
  `0x1425C4894` and `100.0` at `0x14253EE30`. `RNG(100)` is `FUN_140A924E0` (an LCG,
  `x = x·(−0x2D22B2E3) + 0x4513 & 0x7FFFFFFF`, stream 2, scaled to [0, 100)). The randomness byte `r` is at
  **param_2+0x44** (the relayed `+0x68` is wrong). No list index or player ID enters.

## Not confirmed

- `FUN_14089BAE0` rolls three floats (`+0x328/+0x32C/+0x330` from bases at `+0x20/+0x24/+0x28`). It writes neither
  record+0x64 nor any band. That it is the arrow's roll is the relayed claim, not something these bytes show.
- The match-prep writer of record+0x64 and the band thresholds (where form N places the base) are open, as the relayed
  answer itself says.
- The apply-time team-state shift (own team forces index 0; the opponent pushes one step worse, capped at 4) in
  `0x14092B4CE`–`0x14092B5DB` was not re-traced here. The `cmp eax, 3` after `call 0x140A40BC0` at `0x14092B4EF` is
  the first gate in that block.

## For GF

GF rolls condition arrows at random (`Team::InitPlayers`: "condition arrows (random)"). PES derives them from form,
a per-player randomness byte and one RNG draw, and the multiplier table above applies per stat tier. Porting it needs the
arrow writer and band thresholds (open) and the PES player data (the player-database loader item).
