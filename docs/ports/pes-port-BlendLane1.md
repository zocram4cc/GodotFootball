# pes-port-BlendLane1: PES's clip-start blend, decode and GF port attempt (branch BlendLane, base e154ed687)

## 0. Verdict
- NOTHING LANDED. No commit, no push (`git ls-remote origin BlendLane` empty). Scenario claimed: none. Both candidate
  ports break the ratchet or the M2 cap:
  - Crossfade of the clip-start placement over SetMotion's duration, alone: PesStallQuick **124** (bound 49, M2 cap 98). Walker RED.
  - Crossfade plus WalkLane11's walker patch (yaw and anchor): walker GREEN, PesStallQuick **1444**. Far over the cap.
- The decode is done (section 1). What blocks a faithful port is the untranslated evaluator that sums the layer records (section 3),
  and the per-man frame that GF never runs (section 2). Both need a rig capture (section 6).
- ALiveBall was not run: nothing reached a landing candidate.

## 1. Decode: what PES does at a clip start (VA-cited)
Image `pes_full.bin` (flat, VA = offset + 0x140000000). Decompile: headless dump `gf-WalkLane-scratch/slide/dec_blend.txt`
(FUN_141AF61C0, FUN_141AF52B0). Call sites: rel32 scan of the flat image (`gf-BlendLane-scratch/callers.py`). The Ghidra bridge
returned 404 for these VAs this session, so the dump is the decompile source.

| step | VA | what it does | verdict |
|---|---|---|---|
| SetMotion flush: record push | FUN_141AF61C0 (0x141AF61C0) | appends one 0x30-byte record to the layer host's vector (host+0xf8; layer 0 = `*(P+0x3E)`, layer 1 = `*(P+0x40)`), after FUN_141AF3FD0 (reset) and FUN_141AF3D00 (room check; grow via FUN_14022C690) | CONFIRMED (decompile) |
| record fields at push | 0x141AF61C0 body | +0x00 = layer (0 base, 0x100 upper); +0x04 = clip end (clip byte +0x10 × int +0x0C, scaled, per doc §2); **+0x08 = 1.0f** (literal `0x3F800000`, the weight); +0x0C and +0x10 = startTime × 300 (`fVar23 = *(param+0xC) * DAT_1425A266C`, DAT_1425A266C = 300.0); +0x14 = rate (`*(param+0x14)`, the unit rate); +0x18 = 1/rate (`DAT_14256AE1C / rate`, DAT_14256AE1C = 1.0) | CONFIRMED (decompile) |
| duration | 0x141AF61C0 param block | SetMotion's duration is the param block's +0x08 (`piStack_338[2]`, read after the push, selects the transition branch). It is NOT stored in the 0x30 record | CONFIRMED that it is not in the record; its use after the push is OPEN |
| series push | FUN_141AF52B0 (0x141AF52B0) | one record per member (the parametric/series blend); called from the parametric flush 0x1406E6325 | CONFIRMED (decompile) |
| rate for all records | FUN_141AF6E40 (0x141AF6E40) | writes rate and 1/rate into every record of the vector (`gpr[9] = kAllRecords`) | CONFIRMED (translated; `pesfoxworld.cpp:283-288`) |
| per-man anim tick | FUN_1406E5FC0 (0x1406E5FC0) | flush at 0x1406E6268 (base), 0x1406E6283 (rate), 0x1406E6325 (series); locomotion driver FUN_1406F19F0 at 0x1406E6529 | CONFIRMED (translated; call sites by rel32 scan) |
| its callers | 0x1409F1DCF, 0x1409F23E2 (in FUN_1409F20C0), 0x140A3739E | the per-man tick runs inside PES's AI/team tick | CONFIRMED (rel32 scan). GF never calls 0x1406E5FC0, 0x1409F20C0 or 0x140928FC0 (grep of `src/sim`) |
| root motion apply | FUN_141AFD400 (0x141AFD400) = AnimMovePlugin::apply | translation += R(model)·(pluginScale · rootDelta); the delta is read from the pose object (+0x0E / +0x74 / +0x0F, doc §7.1). Its only direct caller is 0x141AFD3F0 inside FUN_141AFD3C0, which is reached indirectly | CONFIRMED (1 caller). Delta source INF: the pose object is written by the untranslated evaluator (section 3) |
| pose read | PesFoxWorld::ReadPose (pesfoxworld.cpp:339) | reads the joint block the frame left in L1+0x50 / A+0x20 | CONFIRMED: no GF caller (grep). The frame that fills it is not run by GF |
| GF body placement | humanoidbase.cpp:248-282; humanoid.cpp:1698-1721 | body = startPos + positions[frameNum] (+ smuggles); orientation = startAngle + rotation smuggle. Clip start = snap: `startPos = spatialState.position`; `startAngle = clipAngle` (travel direction, humanoidbase.cpp:587-594) | CONFIRMED (read) |

### Weight curve and who advances it
- The record weight is 1.0 at push (+0x08). Doc §9 item 7 says every live record read had weight 1.0. The per-tick change is in
  the untranslated sync/evaluator: FUN_141B11420 (AnimSyncPlugin; mode 1 writes the stored weight, mode 2 writes `(1-w)*k`,
  doc §4.2), and the layer clock FUN_1423989F0 (`t += dt; if (t < 1) t = dt`, doc §4). The clock per tick is FUN_1406F19F0
  (`animAdvance = playbackSpeed / 54` s).
- Crossfade law: a linear ramp of each layer's progress, `w = clamp(tau/duration, 0, 1)` on the incoming record and `1 - w` on
  the outgoing ones. INF, high (doc §4.2). Not read from the evaluator (doc §9 items 4 and 7).
- P+0x3B80 (the series blend position): FUN_1406F14E0 zeroes it at issue (CONFIRMED, WalkLane11 §3). Who advances it after issue:
  OPEN. The single-motion clip start does not use it.

### Where it sits relative to the root-motion apply
Per tick in PES: action fills the request (+0x3B50..0x3B88) → flush (0x141AF61C0) → SetSpeed (0x141AF6E40) → clip progress
(FUN_1406E6C40 / FUN_1406E6CB0) → locomotion driver (FUN_1406F19F0) → sync (FUN_141B11420) → pose (evaluator, OPEN) → root apply
(FUN_141AFD400). The root apply runs after the pose, so the blend acts on root DELTAS (summed over records), not on absolute
positions. A PES-faithful port must integrate blended deltas.

## 2. What GF does instead (CONFIRMED by reading)
- `PesFoxWorld::IssueMotion` (pesfoxworld.cpp:258-289) runs the translated record push and SetSpeed per MotionRequest (PES mode
  only, `Match::PesAnimeIssue`). The per-man frame never runs for the body, and ReadPose has no caller.
- The clip start is a GF snap in `PlayIssuedClip` (humanoidbase.cpp:520-599). It swaps the clip, sets `startAngle = clipAngle`
  (travel direction) and `startPos = spatialState.position`. The placement jumps by the new clip's first-frame offset and the yaw
  turns to the travel direction in one tick.
- GF's `previousAnim` is only a copy for GF's own movement logic. Nothing in GF sums records.

## 3. Open (what blocks the faithful port)
1. The evaluator's weight write per tick (AnimSyncPlugin mode 1 / FUN_1423989F0), and how the outgoing records' weights fall.
   Blocks the blend law from being OBS rather than INF.
2. The per-man frame (0x1406E5FC0's body path) is never run by GF. A faithful port runs it per man per tick (action, flush, driver,
   sync, pose, root apply) and takes the root delta from the pose object. That is a multi-round port: the frame's action side is
   the PES MOVESTEP/action layer GF replaces with its own FSM.
3. Pose root delta: the pose object's +0x0E/+0x74/+0x0F layout is from doc §7.1 (OBS), not decoded in the evaluator.

## 4. The port attempt (measured; NOT landed)
Two stages, on e154ed687 (locked builds, `-j8`, `nice -n 19`; sims through `tools/dev/simslot.sh timeout`).
Stage 1: a placement crossfade in `HumanoidBase` (new `ClipBlend` state, `BlendClipPlacement` called at both placement sites,
humanoidbase.cpp:282 and humanoid.cpp:1720). A PES issue with SetMotion's duration (request.duration) ramps the body's placement
linearly from the outgoing placement (its track continues from the shown placement) to the issued clip's. Cleared on ResetPosition and on
the non-PES switches (humanoidbase.cpp:1353, humanoid.cpp:3055).
Stage 2: stage 1 plus the walker patch (`gf-WalkLane-scratch/walker-humanoidbase-only.patch`: clip yaw = current facing, anchor
at the start frame).

| tree | SetKickStand walker (far ticks commanded wrong) | PesAnimeAction | PesStallQuick (bound 49, M2 cap 98) | census (STALLDUMP clips, top) |
|---|---|---|---|---|
| base e154ed687 | RED (WalkLane11: 62 of 71) | 3/4 (ABall red, pre-existing) | 49 (bound; not re-run here) | not re-run |
| S1 crossfade | RED, 62 of 71 | 3/4 (same) | **124** (over cap by 26) | trip/idle 50, trap_0_0 in_fast_mirror 29, tackle 15 |
| S2 crossfade + walker | **GREEN** | 3/4 (same) | **1444** (over cap by 14x) | trap_0_0 in_mirror 308, tackle 123, sliding/idle 91 (GF clip, fn 13), trap sole 75 |
| walker only (WalkLane11, not re-measured) | GREEN | not run | 567 | sliding, stand_up_from_back, dribble, trap |
| yaw only / anchor only (WalkLane11) | GREEN / RED | - | 444 / 343 | - |

The receive test's one red (`PesAnimeAction.ABallActionIsNeverTakenForMovementOrForTheTrapItself`) is red on the base as recorded in
WalkLane11 §1, so it is not caused by this change. Not run: ALiveBall (20 min), PesHang, the census per-bucket delta against a
re-measured base, gen_translated --check, check_pii (no translated code changed and nothing pushed).

Reading: a placement crossfade of the outgoing track makes the rise worse, not better. The S2 rise is on trap men (ac8 7) and tackle men
(ac8 13), i.e. the PES actions GF stands in for, and on GF-owned sliding clips (fn 13). It is not on named stand-ins for the clip start.
The rise is not attributable to a named stand-in, so no M2 raise applies, and the crossfade is not PES's law (its outgoing track
keeps the old clip's position at the new clip's start, which PES does not do: PES blends deltas, which I did not implement).

Stage-2 WIP patch (both stages, 162 lines): `gf-BlendLane-scratch/blend-crossfade-plus-walker.wip.patch`. The source tree is reverted
to e154ed687; the build dir was removed. Scratch logs: `gf-BlendLane-scratch/s1_*.log`, `s2_*.log`.

## 5. Scenario and ratchet
- No scenario is claimed. M1: fails (PesStallQuick above 49 in both stages). M2: fails (S1 124 > 98; S2 1444 > 98; census not on named
  stand-ins). Nothing merges to Integrate from this branch.

## 6. Requests and next step
Rig: none this session (no captures read, no hooks). Requested for the next round, as Main's to arm:
1. A walker clip-start capture, EVERY 1, cap 12, predicate `--when` on the walker's action with `b(rdx+0xac8)` on the issue: the record
   push (FUN_141AF61C0) entry and return, and the pose object (+0x0E/+0x74/+0x0F) at the next 6 ticks. This settles the weight law (open 1).
2. FUN_141B11420 (AnimSyncPlugin) entry, EVERY 1, cap 24, with the predicate that the layer has more than one record. This gives the per-tick
   weights (open 1 and 3).
Next highest-value step: decode the evaluator's weight write from the capture above, then port the per-man frame's root-delta path for
the walker's clip start (one blended delta integrator in place of the placement snap). Do not re-run the placement crossfade.

## 7. Repo state
- Branch `BlendLane` created locally at e154ed687 (`<workdrive>/gf-BlendLane`). No commit, not pushed. Build dir removed.
- Worktree `extern/godot-cpp` is the symlink (for builds). The git status shows it, which is expected, and it is not staged.
- Lock: `/tmp/gf-build.lock` was held by Main's leaked fd (per Main); all builds ran under `flock` after that.
