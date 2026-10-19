# Doc propagation round 4: community-qa, role-writer, carrier-helpers (PropAudit4, 19-10-26)

Branch `PropAudit4` (worktree `gf-PropAudit4`, base origin/main `df30230`). Docs only: no build, no test, no gate.

Commits (all pushed, `git ls-remote origin PropAudit4` = `232fdfe`):
- `4393658` community-qa C1-C10 (C5 superseded, C6 n/a — 7 files)
- `04361f2` role-writer §6 corrections incl. community-qa self-correction (8 files)
- `bb6cddf` carrier-helpers items 1-15 (6 files)
- `232fdfe` round-4 table in `docs/ports/pes-doc-propagation.md` (G-r4)

## 1. Community-qa C1-C10

- C1 EDITED (extra-frontman:107): +0.2 now gated on style bit 15 AND `DAT_142613D20[B[slot]]` bit 15 (CB-only). Source evidence visible (F3 + §7 log).
- C2 NOTE (tactics:244): auditor census not re-verified → note, claim kept.
- C3 EDITED (tactics:235,239): sentinel OBS.
- C4 EDITED + NOTE (matchlogic §1.2 notes + :656): index spaces separated (Player.bin ids vs in-match bytes); PK index OPEN. Evidence rows all visible (contact §3.3, attack-leftovers:844, F4 dec).
- C5 PARKED (superseded): role-writer §6 proves both branches use the same map. C5 bullet struck in community-qa; nothing added to stamina-subs.
- C6 PARKED (not our doc): auditor's +7.0 wrong; our three 8.0 rows confirmed present, raw bytes `00 00 00 41` per §7.
- C7 EDITED (team-concepts-port:58): sheet writer named, in-match path OPEN.
- C8 NOTE then EDITED (lane-core §5.1 + O1): C8 note added first, then role-writer closed O1 on static.
- C9 EDITED (defensive-shape:168): chaser gate + %11 rule.
- C10 EDITED (anim-select-2:117-120, :348): 36 m gate + Track Back skip.

## 2. Role-writer §6 (12 rows → 8 edits)

- community-qa:25-27 F1 self-correction EDITED (same-value R8D, S-base, single map); C5 bullet struck as superseded; C8 marked closed-on-static.
- tactics-clock (5 sites), positioning-phases (§1 + §5), foundation-teamplayer (98-99), OffBallSelectors:15, foundation-matchball D2, lane-core §5.1 + O1 (closed), extra-frontman:61 + :344 — all EDITED.
- C8 conflict resolved per brief: role-writer's close-on-static wins (live `T1+0x4C00[0]=0` already matches the dumped map).

## 3. Carrier-helpers items 1-15

- §0 float table re-verified mechanically against `pes_full.bin` (all 40 .rdata + 3 immediates agree with the doc; the two §1.1 corrections already in place).
- Items 1-4 EDITED in A4Lanes (§2.1/§2.2); 5-6 in A4Cont; 6-7 in A4Carrier:113 (kept 6 columns); 8-13 in attack-decisions (§2.3/§2.5/§2.6); 14 NOTE in lane-structs (current-DB state, older readings kept); 15 NOTE in carrier-ai §4.3 (cross-checked, offsets match exactly).
- Item 13 caveat: gate-1-fail falls into gate 2 (not 0xFF) — carried over from §2.3 as written.

## 4. Parked (2)

- R4-C5: superseded by role-writer (see §1).
- R4-C6: auditor error, not our doc (see §1).

## 5. Checks

- `tools/check_pii.sh` exit 0 from the main checkout; no machine paths in added lines.
- No build/tests (docs only). 20 files under `docs/`, +111/−67 vs `df30230`.
