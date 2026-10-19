# PES positioning phases: directive word, phase state machine, consumers (A8)

Source: external auditor writeup, archived verbatim in structure (18-10-26).
Image VAs use base `0x140000000`. Verdicts are the auditor's.

## 1. State

Team directive word: `TeamData+0x80A0` (u16). Per-player phase copies:
`TeamData+0x80E0` (u16, slot-indexed, 11 slots). The selector layer reads the
copies, not the team word directly. Writer of the directive word: not pinned (OPEN; the role-array part of the hunt is now pinned — see §5). _Corrected 19-10-26 from pes-re-role-writer.md §6: old "(OPEN, related to the role-array writer hunt: no store pinned yet)"._

## 2. Consumers

`SecondaryPositioning_11Styles` (`0x1405DC350`, 0x253F bytes) is a per-player
SELECTOR whose modes are gated by the phase copies and role array C
(`TeamData+0x3FB8`, 11 dwords):
- mode 2 (anchor/destroy) = role value 2, phase 20.
- mode 40 -> style bit 15 (Extra Frontman) = role 8 OR role 5, phase 23.

The {5,8} pair also appears in the best-player picker `ApplyLineupPositions`
(`0x1404A7D60`) as a +15.0 score adjustment on array A (`TeamData+0x4BD4`).

## 3. Phase 23 = push-up-and-hold-line

The secondary selector bails (ret 255) on phase-23 players; the master
positioning layer (`0x1405E6E80` / `0x1405E9370`) takes over: advance-taper
skipped, position clamped into the formation zone, keep-behind-line via
`sub_140A53F90` (5 m margin), line-holding depth 5 units (10 for Goal Poacher
holders via CheckStyleAtPosition bit 0), halfwidth -> +1052,
`sub_1405D09C0` finalizes. Net effect: phase-23 players are pushed up and
held as a line rather than individually re-positioned.

## 4. Hierarchy (verified layering)

Phase state machine -> positioning layers (secondary selector + master) ->
run-option selector (`0x140703BA0` family) -> movement execution.

## 5. Open items

- The directive-word writer (who sets the phase, when).
- The role-array writers (A `+0x4BD4`, B `+0x4C00`, C `+0x3FB8`): CLOSED 19-10-26 — see pes-re-role-writer.md §1.
