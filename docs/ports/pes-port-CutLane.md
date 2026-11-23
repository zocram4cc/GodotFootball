# CutLane: stadium cut node and the wide-channel goal anchor

Branch `CutLane` (worktree `<workdrive>/gf-CutLane`, from `origin/CutsceneAnchor` at `4a5c369e`). Pushed: `c10e57cf`.

## Landed (green)
- `f9a195bf` `FixDemoSelect.TheStadiumCutNodeHoldsPesLiveValues` (tests/pes_fixdemo_select_test.cpp). Passes on the current wiring. The earlier pending node test was wrong: it read the holder as the node. The chain is slot → holder → +8 = node. Live check (sudo, pid1408046): slot 0x71 holder 0x321337e0, node (-80.0, 16.0).
- `c10e57cf` wide test loads the capture's stadium cut key 0x71 as pre-state at `[[G+0x2C0]+0x20]`. The test is still RED (see below).

## Findings
- The fill 0x141E5D560 builds each cut through the per-object insert 0x141E5CB40 (CONFIRMED: 3 real calls at match load, live2/pid1408046 seq621-623). The loader 0x141E5D980 had 0 calls there, so it is not on this path. Its proof (assignment step 1) is not done; the path is not needed for slot 0x71 in this launch.
- Else-arm key: `0x140476C50` at `L_140476E58` looks up `id = u32 [[G+0x2C0]+0x20]` (G = `[0x1436F3DC8]`), then `0x141E5CA20(lib, id)` returns holder+8. Translated `ai/pes_fn_141E5D4C0` is a wrapper that tail-calls `0x141E5CA20`, so the earlier note "D4C0 returns *(slot+8)" holds through the wrapper.
- GF's time-scale object (`Zalloc(0x60)` at G+0x2C0) has +0x20 = 0, so the lookup misses. That is the cause of the missing node in the else arm.
- Live values (sudo -n, read-only pread, pid1408046): `[G+0x2C0]+0x20` = 161 (0xA1) at the sample. Main reports 0x9E for the launch and that the field toggles. Seq204 (goal anchor) holds 0x71.

## Wide test (`GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`): RED
- Before pre-state: T[0] -41.5502167 (want -41.6211472), pos[0] 0 (want -41.5502167), pos[2] 0 (want 34).
- With pre-state 0x71: pos[0] -57.5796 (the anchor now reads the node). T[0] is still -41.5502167 (the event spot) and pos[2] is still 0.
- Next blocker: the else arm's T. `0x140476C50` takes the celebration spot from the event, so the node value (-80/16) does not reach T in this path. Trace what writes A0+0x30 after the node lookup (the `L_140476E58` branch's float reads at +0x130/+0x134/+0x138 and the compare at 0x140476EBD).
- GF runtime feed (open): PES's writer of `G+0x2C0`'s +0x20 is not decoded. Main's decision is `112 + TeamStadiumId(home)` with a `// ponytail:` note. Not written.

## Verification
- `pes_fixdemo_select_test` 8/8.
- `pes_goal_anchor_test` 23/24; only the wide test fails.
- `gen_translated.py --image <pes_full.bin> --check`: 5803 functions regenerate byte-identical.
- `ledger.py --check`: current (44518 rows).
- `tools/check_pii.sh` in the lane: pass.
- Not run: mutation check (no fix landed; the wide test is still red), the rest of the test binaries.

## Not done
- Loader `0x141E5D980` proof (not on the live path; needs synthetic states over a real capture).
- Runtime key feed for `G+0x2C0`+0x20.
- Wide test green.

## Scratch
- `<workdrive>/gf-CutLane-scratch/livequery.py` (read-only, needs sudo -n), `capread.py` (capture mem.bin reader).
