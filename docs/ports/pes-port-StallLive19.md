# pes-port-StallLive19: offline pass over the captured trap calls; code census of PES; resting-ball captures still missing (StallLive, 21-11-26)

Status: NOT LANDED (no sim change, tree equals HEAD `1b56b7264df7b8984707f3936fb245ae45d5e2e1`, ls-remote confirmed; build dir deleted). Data pass only.

## 1. Captures on disk
- `live2/pid1267009` (the relaunch): 2 folders, neither a trapopen capture (rec_140A8ED70, scorebug_1420F0EB0). No trapopen capture from the EVERY-5 arm had landed when this was written.
- Trapopen captures on disk (`index.json` hook `trapopen_14079A470`): 72, under `live2/pid*`. Of these, 30 GB of packed `mem.bin.zst` (32 oracle-linked captures); the rest were deleted or have dead links.

## 2. PES refusal codes in the usable oracle captures (MEASURED from corpus rows, all 127 trap-oracle rows)
| issued | out (PES) | count |
|---|---|---|
| 0 | 1 (reach, ball far) | 113 |
| 0 | 4 (height class) | 6 |
| 0 | 10 | 6 |
| 0 | 6 | 2 |
| 0 | unknown (no write for out) | 6 |
| 1 | 0 (success) | 2 |
| 1 | unknown (kick chooser rows, not trap) | 8 |

- Codes 7, 0 (race) and 3 do NOT occur in any PES capture on disk. GF's live census (round 16) has 7, 0, 3, 2, 10 as the dominant refusals. So either those codes are rare in PES's real calls, or the disk set is narrow (it is dominated by reach refusals, code 1, at one hook cadence).
- Code 10 occurs in PES (6 captures) with the ball moving fast and the man 4 m away.

## 3. Fields of the code-10 and code-4 captures (MEASURED, oracle probe on decompressed `pid3727195` seq029 and seq027)
| capture | out | ball speed (xz, m/s) | track step (xz, m/tick) | man-to-ball distance (m) | caller AC8 |
|---|---|---|---|---|---|
| seq029 | 10 | 10.43 | 0.1923 | 3.94 | 3 (MOVE) |
| seq027 | 4 | 11.20 | 0.1941 | 4.32 | 3 (MOVE) |

The ball velocity field (B+0x2948, +0x2950) and the sample track agree (track step × 54 = speed), so the ball fields are consistent with each other in these captures.

## 4. What this means for the brief
- No capture on disk is a resting-ball trap call, so no GF forced case has a PES counterpart to diff against. The diff for codes 7, 0 and 3 still needs Main's EVERY-5 captures in play (predicate: ball speed < 0.3 m/s).
- The probe pipeline is ready: `o/` oracle dirs with decompressed memory, and the `ZZR19Fields` probe (temporary, reverted) prints code, issued flag, caller slot, AC8, anime class, ball speed, track step and man-to-ball distance per capture. Re-run it on each new capture set.
- Predicate advice (already sent): use the track step (sample 1 − sample 0, xz, < 0.3/54 m) rather than the velocity field alone, since the velocity field is refreshed only in the team tick.

## 5. Open items
1. Resting-ball captures (Main's hook): decompress, run the probe, classify by code; the first code-7/0/3 captures are the comparison set.
2. Decide whether the code census gap (PES: mostly code 1; GF live: mostly 7/10/2/0/3) reflects real PES call populations or a capture-cadence artefact. Check against the same situation in the live stream, not the trapopen cadence.
3. Ring open item (D/(10(k+1)) structure and the contradiction with `pes-re-ball-track.md` section 3) carried from round 17b.
