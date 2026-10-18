# Doc propagation round 3: carrier plan order, supporter sides, stamina-subs model (PropAudit3, 18-10-26)

Branch `PropAudit3` (worktree `gf-PropAudit3`, detached base origin/main `5cf43c3`). Docs only: no build, no test, no gate.

## 1. Carrier plan order (CONFIRMED)

- `docs/re/pes-re-carrier-ai.md` l.372–375 (§4.4) and `docs/re/pes-re-carrier-port.md` l.265–267 (§3.6) now give the admission order **9, 8, 2, 3, 1, 4, 5, 6, 7**.
- The table @0x142bf02c0 holds 8 dwords: 9, 8, 2, 3, 1, 4, 5, 6. The dword at 0x142bf02e0 is 0. Read statically from the install (`PES2021.exe`, `.rdata`), not live.
- Entry 7 (SideChange) is written inline in the dispatcher 0x14211e2d0 (`local_38 = 7`). Source: `docs/re/pes-re-com-styles.md` §4.12 and Q3.
- Both fixes begin "Corrected 18-10-26".

## 2. Supporter sides, FUN_140541010 (CONFIRMED by decompile; no side change)

Decompile of body 0x140541010–0x140541579 (Ghidra `dec.sh`).

- Points: #0 = ball + 15 m along A (0x14259be0c = 15.0). #1 = ball + 12 m at `TurnToward(180°, A, 22°)`. #2 = ball + 12 m at `TurnToward(0°, A, 22°)`. Constants: 0x142598580 = 180.0, 0x1425c48c0 = 22.0, 0x14259be04 = 12.0, 0x14259bdf4 = 8.0, 0x14259be7c = −5.0.
- For A = +x (90°): #1 heading 158°, #2 heading 22°. Heading h gives (sin h, cos h) on (x, z). So 158° lies on −z and 22° on +z.
- Side test. Man lateral (PES z) is `+0x564`. Ball z is `MA+0x24834`. Ball x is `MA+0x2482c`.
  - #1 accepts when man z ≤ ball z.
  - #2 accepts when man z ≥ ball z.
  - These match the current text in `pes-re-transitions.md` §3.1 (l.224–227).
- `docs/re/pes-re-team-concepts-port.md` l.20 gives the same sides. The reversal named in the brief is not present in the current files, so nothing was copied from it.
- Edit: the claim now reads "lateral (PES z)" instead of "y", and a decode note with VAs was added (l.225). The 17-10-26 note is kept (l.226).
- INF: the output array (`+0x1c/+0x20/+0x24`) is not cleared inside the function. Slot 0 therefore also excludes the previous call's `+0x20/+0x24`. The caller's clearing is not decoded.
- INF: the runner index at `+0x18` comes from the caller and is not confirmed.
- INF: the "state priority > 2 = not run/press/MATCH_UP" gloss is the port's, not a decode of `FUN_140a40110`.

## 3. Stamina subs model text, match-management §2.3 (A3 completion; source visible)

- Source: `docs/re/pes-re-stamina-subs.md` §2 (pseudocode and the "Corrections to match-management §2.3" paragraph, OBS). Propagation row 20.
- `docs/re/pes-re-match-management.md` l.209–249: the 17-10-26 pre-correction code is replaced by the corrected reading.
  - Cap: per-mode schedule, gated in period 3. Mode 2 never reaches full cap.
  - Classes 0 (GK) and 1 (CB) are excluded.
  - User player gets +500 and form-close men +100, each halving the threshold.
  - Goal and assist protection is applied.
  - Candidates are sorted ascending by base + pct.
- OPEN inside the block (kept as the source says): `m` (clock minute, INF low) and `obj+0x1c` (readers only).

## 4. Parked (source row not visible; no edit)

- `docs/re/pes-re-anim-select.md` l.542 (§8 killed-hypotheses row, Freemove). OPEN. No propagation row cites l.542. The A6 rows (11–16) cite A6Period, A6Writers and clock docs only.
- `docs/re/pes-re-anim-select.md` l.625 (follow-up note). OPEN. No propagation row cites l.625. The note already carries the anim-select-2 content (Freemove never read; breath %), so there is nothing new to add.

## 5. Open items for Main

- Brief premise on R3-3 (team-concepts l.20 "reversed") does not hold in the current files. Check whether a reversed copy exists on another branch.
- Caller clearing of the supporter output array, the runner index `+0x18`, and `FUN_140a40110` are not decoded.
- Checks: `tools/check_pii.sh` exit 0 (run from the main checkout). No machine paths in added lines. No build or tests run (docs only).
