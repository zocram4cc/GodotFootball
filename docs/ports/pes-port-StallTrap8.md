# pes-port-StallTrap8: cmove rule fixed and pushed (8e782040); the --pes-loop match starts and finishes; counter-B feed measured in the real match: the writer stores nothing (framesSince 172), so the feed is reverted and not landed (StallTrap, 22-11-26)

Status: PART 1 LANDED (8e782040, ls-remote confirmed). PART 2 NOT LANDED: the feed zeros counter B in the real match, so it was reverted (land-or-revert).

## 1. Landed: the cmove rule (CONFIRMED)

Commit 8e782040 on origin/StallTrap (parent 5b67bc56). Files: tools/dev/translate/xlate.py, ro_tables.json, src/pes/translated/closure/pes_fn_1403B8170.cpp, src/sim/onthepitch/pesimagetables.cpp/.hpp.
- image_origins: a cmove whose candidates are image-derived and runtime-derived alike adds MIXED. bake_window refuses that base, so it is an ordinary memory read. A cmove with only image candidates keeps the window/bake.
- The empty-string NUL 0x142583E32 (the one live read left) is a runtime segment: LoadPesImageTables reads one byte from the install (kEmptyStringVa), and its ro_tables entry is marked bound. No value is baked.
- gen_translated --image: 10211 functions written; --check: 10211 byte-identical (rc 0). Only pes_fn_1403B8170.cpp changed in the layer.
- Proof the --pes-loop match starts and runs: `gfsim_run --seed 7 --minutes 2 --pes-loop` on 8e782040 (cmove only, feed off): rc 0, full time, `[determinism] dfaf58f02d1c7dc5`, 33,896 sim ticks, wall 663 s. The old boot abort (PES_RO_OUT_OF_WINDOW in pes_fn_1403B8170) is gone.
- Build: gfsim_run rebuilt; the translated lib rebuilt for the one changed file.
- Tests not run on this commit (cmove only, gate is Main's): pes_events_test not rebuilt for it.
- StallRegress has the SHA (sent).

## 2. The feed, measured in the real --pes-loop match (CONFIRMED, probe output)

Feed under test: RunCounterB per on-pitch slot in Team::RunPesPassTarget (gated to IsInPlay), BindSlot binds man+0x18 = slot and man+0x7E0 = side ctx, the stand-in store removed. Two --pes-loop 2-min seed-7 matches:
- Ungated feed, probe on every call: 1,857,988 CBLOG lines in 900 s (timeout, rc 124). Most calls were out of play (playState 3). Cost, not a crash.
- Gated feed (IsInPlay), probe on the first 60 in-play calls: match finishes (rc 0, 33,k ticks). All 60 calls: rax 0, counterB 0, framesSince 172 (44) or 173 (16).

Reading: the writer's store gate is frames-since the newest kind-7 event == 0 (or 2 with a team flag clear) (StallTrap5 probe, CONFIRMED there). In GF's match the newest kind-7 event is ~172 frames old, so the writer exits before its store and counter B stays 0. A feed would write 0 into counter B for every man. Not landed.

- EV+0x3208 (the first control divergence in the unit trace): INF-to-CONFIRMED that this is GF's event history, not a missing ring. In the match the newest kind-7 touch is 172 frames old, so GF is not routing kind-7 events when PES does. Writer of that event: the GF touch producer in pesevents.cpp (~line 705-720, kind 7 action 0xf) is gated (`if (!gate) return kNone`, and it skips a repeat of the newest key). OPEN: which gate blocks the touch route in play.

## 3. man+0x18 (player key)

- PES reads man+0x18 as a 4-byte key (value 18 in seq392) and forwards it to the event search (0x140A3E190 path).
- GF's touch events carry the same key: pesevents.cpp:715 `touch.SetU32(0x18, in.player)` ("payload+4 key"), and in.player is the PES slot (PesSlotOf). So the natural binding is man+0x18 = slot. Bound in the feed (BindSlot), not landed.
- Writer-of-record in PES: NOT FOUND. I did not run the constructor/initialiser scan Main asked for; I did not verify the value in the match (no probe read man+0x18). INF: slot key. OPEN: find the store to [man+0x18] in the man constructor (pes-image-field-scan), and read man+0x18 in the match.

## 4. Race test (red, not green)

PesCounterB.ANearRestingBallsManHasTheSmallestCounterBAndLeadsTheArrivalList (unit world, ball 1.2 m from place 10, ctx and key bound): counter B 0 for every man, arrival list lead 0 (not 10). RED. The unit world does not reproduce the match gate (no kind-7 event at framesSince 0). The FUN_140799440 race test was not written.

## 5. Not run

PesStallQuick, full ALiveBall, PesHang, PesPassTarget (no feed to measure; the cmove commit is only proven by the --pes-loop match above). gen --check on the feed: n/a (feed is sim-only).

## 6. Artifacts

- Feed (ungated, probe) saved: scratch/feed8.patch (pre-gating); scratch/feed9-gated-probe.patch (159 lines, gated feed and bounded probe). Both reverted from the worktree.
- Worktree: 8e782040, clean, build/ removed. Scratch: 142 MB.

## 7. Next steps (exact)

1. Find the touch-route gate for kind-7 (pesevents.cpp ~705-720): why GF's newest kind-7 is 172 frames old in play. That is the feed's blocker. The writer result rax 0 is the consequence.
2. Find the store to man+0x18 (static, pes-image-field-scan, man constructor) and confirm slot key in the match.
3. Then re-apply scratch/feed9-gated-probe.patch (minus the probe), run the pes-loop match, and only land if counter B is nonzero and PES-shaped.

## 8. Confidence

- cmove commit fixes the abort; gen --check byte-identical; pes-loop match finishes: CONFIRMED.
- Writer stores only at framesSince 0/2: CONFIRMED (earlier probe + these 60 calls showing 172/173).
- GF's newest kind-7 is stale in play: CONFIRMED by framesSince 172 (INF on the route gate that causes it).
- man+0x18 = slot key: INF (event key evidence; no PES writer found).
