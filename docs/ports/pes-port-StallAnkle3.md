# pes-port-StallAnkle3: the throw-in taker's lifted stand is the SetKick point (R + 0.773), not the carry spot (StallRun, 22-11-26)

Branch `StallRun` pushed: `810de3b5a9925000bcdf95164f5b47111f8415d9` (confirmed by `git ls-remote origin StallRun`; parent `a2439354`). One file: `src/sim/onthepitch/teamAIcontroller.cpp` (+16 −4). No test edited. `TheThrowInTakersAnklesStayBehindTheLine` red on `a2439354`, green on `810de3b5`.

## 1. Result table

| check | result |
|---|---|
| `PesThrowInRelease.TheThrowInTakersAnklesStayBehindTheLine`, `a2439354` (red) | FAILED: 1 of 61 taking ticks short, persistent; worst 0.00923 m (`pes_throwin_release_test`, log `gf-StallRun-scratch/throwin_suite` / bg_261) |
| same test, `810de3b5` | OK (767 s) |
| `PesThrowInRelease.*` full binary, `810de3b5` | 9 of 9 PASSED (rc 0): ThePlayStateIsSetPlayReady, TheMatchIsBuiltWithTheInstallsPlayerDatabase, ACarriedThrowUsesTheShortRow..., ACarriedThrowInIsReleased, TheThrowInTakersAnklesStayBehindTheLine, EveryManHasHisDemoInfoRef, ACeremonyThrowInIsReleasedFromTheHand..., ACarriedThrowInIsHeldAtItsRowsKeyOffset..., TheLiftTurnsACarriedThrowInTakerToFaceThePitch. (Three ran as 0 ms because their probes were already run by earlier tests in the same binary.) |
| `PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime`, `810de3b5` | PASSED (910 s, rc 0) |
| `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`, `810de3b5` | FAILED (989 s): `census.liveFirings = 5876`, limit 0. Known red on StallRun. The handoff's "92" is not reconciled here: I did not run `a2439354` for the same metric on this machine, so no delta is claimed. |
| `PesStallQuick` | not run: it is on StallLive (`95d4bede`), not on this branch. |
| `tools/dev/translate/gen_translated.py --check` | rc 0: 10,150 functions regenerate byte-identical (3,824 recorded, 6,326 closure). The change is GF-side; no translated code moved. |
| `GATE_TIER=fast tools/dev/gate.sh` | NOT RUN (out of budget). Main gates at merge. |
| `check_pii` | my diff has no work-drive or home-directory strings (grep of `git show 810de3b5`). Full `tools/check_pii.sh` not run (fails in a worktree). |

Mutation: the red baseline (`a2439354`, same test, same probe) is the "remove the lift-end warp" mutant; it was run and reverted. The fix is one warp line plus a shared helper.

## 2. What PES does (live, read-only)

Live sampler: `gf-StallRun-scratch/throwin_sampler.py`, `sudo -n` pread of `/proc/<pid>/mem` only, no writes, no signals, no pauses. The image was read at G = `*(0x1436F3DC8)`, MI = `*(G+0x270)`; restart kind MI+0x1554 (2 = throw-in), taker slot MI+0x173C, frame MI+0x1198, play state MI+0x116C; F = `*(G+0x2AA8)`, F[1] = touchline half-width (read live 34.06); ankle margin DAT 0x142588200 read live = 0.1 m; the taker's AnimePlayer found by vtable 0x1425E34D8 with the slot byte at P+0x2458; joint block P+0x538 (joint 0 = root/pelvis, joint 3 = left ankle, joint 6 = right ankle), motion P+0xAE0 (u16), action P+0xAC8, phase P+0x245C, ball B+0x27B8 (B = `*(G+0x40)`).

One full throw-in episode captured (pid 567716, slot 18, frames 2564–2907 in the sampler log, `run2.csv`):

| frames | act | motion | ps | phase | root z (j0) | ankles z (j3 / j6) | what it is |
|---|---|---|---|---|---|---|---|
| 2564–2706 | 79 | 321 (0x141) | 2 | 3 | 18.5 → 24.2 | 18.8/18.1 → 24.2/24.2 | taker walks to the spot |
| 2707 | 26 | 2201 (0x899) | 2 | 2 | 35.13 | 35.14 / 35.12 | ThrowinLoop; root snaps from 24.2 to 35.1 in two frames |
| 2832–2907 | 27 | 3322 (0xCFA) | 3 | 1 | 34.93–35.08 | 34.91 / 35.21 | release key (0xCFA); nearer ankle 34.91, margin 0.75 m beyond 34.16 |

Ball at the restart: (−29.58, 5.25, 34.18). F[1] = 34.06. The restart point R = F[1] + 0.25 = 34.31 (DAT 0x142592228, docs/re/pes-re-throwin-choreography.md). R + 0.773 = 35.08, which is the live root at the key (35.078–35.082). The doc's earlier OBS sample also gave R + 0.773 for four takes (`docs/re/pes-re-throwin-choreography.md` 4.1), and its path-A approach gave the taker root 34.74–34.76, which is the carry spot F[1] + 0.7 (DAT 0x14257A194).

So PES's taker, once lifted, is not on the carry spot. He stands at the SetKick point, the key's length outside the line, and his ankles sit about 0.8–1.0 m outside the line. PES never puts a foot near the line during the hold or the key.

## 3. What GF did wrong

Failing tick (`gf-StallRun-scratch/ankle/diag2.log`, prior round, re-measured on `a2439354`): taker H11 on `assets/gf_anims/pass/idle/000.anim_mirror` (anim 1203), `type=shortpass`, fn 4 = `e_FunctionType_ShortPass`, frame 10 of 73, nearer ankle 4 mm inside 34.16 (worst 9.2 mm). The clip is GF's ShortPass wind-up, which is GF's throw; it is not a Movement stand and it is not a SetMotion start time (StallAnkle2).

Why he is there at that tick: `AdvancePieceWalk` (teamAIcontroller.cpp, the lift) warps the lifted root onto `pieceCarrySpot` = `ThrowInCarrySpot` (pessequencer.cpp:61; z = sign(R.z)·(F[1] + 0.7), 34.76 m) and then holds him there. PES's ThrowinLoop, at the loop's start, puts him on the SetKick stand point R + 0.773 (35.08 m). GF never did that step for the seamless path. The ceremony path already did (`ThrowInStandLength_m` at PrepareSetPiece) but the seamless lift did not. So GF's throw wind-up was 0.3 m inside PES's hold, and the pass/idle frame-10 ankle reading (34.15) crossed the 34.16 line.

## 4. The change (`810de3b5`)

- The lift's end (`pieceWarpTicks == 0`, `pieceCarrying`) now warps the root onto `ThrowInStandPoint(*match, taker)` before the loop clip (0xCF4) is issued. Comment cites PES's ThrowinLoop SetKick (0x140794280; docs/re/pes-re-throwin-choreography.md 4.1).
- `ThrowInStandPoint` is the one helper for the stand point (restart point + the release key's length outward, from `ThrowInStandLength_m`). The ceremony placement (PrepareSetPiece) now uses it too; its numbers are unchanged. This removes the duplicated outward/standIn computation.
- Forward declaration of `ThrowInStandPoint` in the anonymous namespace above `AdvancePieceWalk`.

No magic numbers: the constants are the existing ones (0xCF4 loop motion, the key via `ThrowInKeyOffset`, the DAT-read margin).

## 5. Killed theories

- The taker is on a GF Movement stand (`stand` in elizacontroller's lifted-taker branch). Killed: the failing clip is fn 4, ShortPass, the wind-up of the throw.
- The fault is the start-time unit (StallAnkle). Still killed (StallAnkle2).
- The nearer ankle sits on the line by placement (the taker stands at the ball). Killed: PES's root at the key is 35.08, GF's was on the carry spot at 34.76.

## 6. Open items

1. The one-tick push lag is not fixed. `PushPieceTakerBehindLine` runs in `match.cpp:898` after `RunPesTrapTicks`, and the joint block the test reads is written after it, so a shortfall found on tick t lands on the root on tick t+1. The shove is still a one-tick correction, not an in-tick one. With the stand 0.3 m outside, the ankle no longer reaches the margin, so the push does not run in this test. Whether PES's in-tick shove and GF's one-tick lag can differ on a throw where the ankle does reach the margin is OPEN; a sample with a short reading is needed.
2. Only one PES throw-in episode was captured. The sampler's rate is low in this match (none in the 900 s run at pid 678718; run 3 at 567716 was lost to a null-MI state). More episodes would firm up the 0.773 value and the lift-to-hold step. The 0.773 is corroborated by the four OBS takes in the doc, not by this sample alone.
3. The lift's warp still targets the carry spot (34.76) for its 14 ticks; the stand is applied at the loop start. PES's exact per-tick root path during the 14-frame lift (throwin_seamless_liftupball) is not decoded here (OPEN).
4. `PesStall.ALiveBall…` is still red on this branch at `liveFirings = 5876`. The handoff's 92 needs the same metric on `a2439354` to compare. Not done.
5. `GATE_TIER=fast` not run; `PesStallQuick` not on this branch (it is on StallLive `95d4bede`).

## 7. Timing and reads (owner rule: record every read, report any PES change)

- PES pid 567716 logged by the rig watcher at 23:03:33 (watch.log). The sampler (run 4) started 23:52:18 and last read the process at 23:56:46; from then the process was gone. The rig watcher restarted at 23:56:52 and logged PES pid 678718 at 23:57:03. The gap is most likely Main's restart. No read of mine paused or wrote the process, but I did not confirm the cause.
- The sampler's heap rescan (`scan_players`, reads of the whole anonymous heap) ran at most every 2 s in runs 2–4 and every 30 s in run 5. These are reads only.
- Run 5 (pid 678718, 23:57–00:12) caught no episode before it was stopped.

## 8. Commit

`810de3b5` on StallRun, subject "PES throw-in: the lifted taker stands on his SetKick stand point when the loop starts", body lists the VAs and the red/green numbers. Committed with the eval `commit` namespace from the worktree (the `extern/godot-cpp` symlink was swapped for an empty dir for the commit and restored afterwards).
