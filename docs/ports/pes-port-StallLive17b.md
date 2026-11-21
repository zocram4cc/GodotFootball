# pes-port-StallLive17b: the P+0x2FC ring is not the clip root travel, not an image constant, not the ball track; the writer is still unfound (StallLive, 21-11-26)

Status: NOT LANDED. Branch `StallLive`, HEAD **1b56b7264df7b8984707f3936fb245ae45d5e2e1** (ls-remote confirmed, no commit this round). Tree equals HEAD (verified). Build dir deleted. No sim change and no test change landed; the brief's red-first ring test was not written because no candidate source reproduces the captured ring, so there was nothing green to land.

## 1. The captured ring (CONFIRMED, two captures, same standing caller)
Captures: `pid3727195` seq025 and seq027 (oracle `r17/oracle`), caller slot 6, man at rest, AC8=3 (MOVE), ball moving at about 8 m/s.

Ring entries k=0..5 (m per frame, PES axes; x, z):
- seq025: (0.000954,−0.002059), (0.007885,−0.011162), (0.006608,−0.011367), (0.004956,−0.008525), (0.003965,−0.006820), (0.003304,−0.005684)
- seq027: (0.000240,−0.000513), (0.006199,−0.008851), (0.006635,−0.011196), (0.004956,−0.008525), (0.003965,−0.006820), (0.003304,−0.005684)

Entries k=3,4,5 are identical in both captures (to all printed digits) even though the playing motions differ (seq025 motion 0xB24, phase 1; seq027 motion 0x1F, phase 4).

## 2. Structure (CONFIRMED arithmetic)
For k ≥ 2, `entry_k × 10(k+1)` is one vector to 5 digits: (0.19824, −0.34100) for k=2,3,4,5 (both captures). So for k ≥ 2 the ring is D / (10(k+1)) for one fixed displacement D per 10 ticks. This matches the port's own comment in `pestrapactor.hpp` (the decoded entry is the predicted displacement over 10(k+1) ticks divided by 10(k+1)). Entries 0 and 1 do not fit that form (k=0 would give (0.00954, −0.02059); k=1 gives (0.1577, −0.2232) in seq025) — they look like a separate, earlier-written term.

## 3. Candidate sources ruled out (MEASURED)
- **Clip root travel** (`PesWorld::RunRootMotion`, PES's own root-motion reader FUN_141AFDF10 on the window, for motion 0xB24 and 0x1F at 10(k+1) frames): returns (0,0,0) for every k in both captures. Not the source.
- **Image constant**: no float with bits of 0.19824 exists in `pes_full.bin`; the bits of −0.341 occur in many places, so it is not a usable anchor. D is not a stored constant.
- **Ball velocity/track**: ball velocity is about (−8.4, −0.5, −7.6); ball displacement over 10 samples is about (−1.43, −0.01, −1.28). D has the opposite sign in x. Not the ball track.
- **Man velocity**: 0 in both (GF's current feed, which gives all-zero ring; the trap chain then refuses on code 7/0/3 in the live sensitivity, see round 17).

## 4. Static writer search (MEASURED, inconclusive)
- Named P writers (ctor `0x1406EB670`, action setter `0x1406EEBB0`, trap tick `0x140797CD0`, P tick `0x141EB5E70`, motion issue `0x1406F0F70`, `0x1406EE6A0`, `0x1406EB440`): none stores to a P+0x2FC..0x343 displacement directly. So the ring is written through a helper or a computed copy (the ball-track doc notes a copy chain `p+0x9B8 → p+0x2FC` through `0x140432AAD`, which that doc records as static in play).
- Whole-`.trace` displacement scan for stores to 0x2FC..0x343: one function, with no co-located P field writes (`r17/stores.out`). Too thin to name the writer.

## 5. Conflict with the RE doc (INF)
`docs/re/pes-re-ball-track.md` section 3 (round 13) reports the ring static in every window it measured, with different contents (slot 1 (0,0,−1)), and no writes at any kick or stoppage. The captured ring here is nonzero, decays with k, and differs from that doc's values. Either the ring is written only at trap/clip events (not measured there), or the doc's player was in a different action. Not resolved.

## 6. Next steps (numbered)
1. Find the ring's writer by a dynamic watch, not a static scan: the rig can hook the trap tick `0x140797CD0` (entry) with predicate `d(rdx+0x2fc+12)` nonzero and cap a few captures; then compare P+0x2FC before/after the call (the decoded entry k must be D/(10(k+1)) afterward). Hook line: `trapopen_14079A470` already exists; a new hook at `0x140797CD0` with the same predicate on the caller's P (`rdx` is P, see `pes-re-trap-inputs.md` section 3.2).
2. Once D is known as a function of an observable (e.g. the man's recent displacement or the action's predicted landing), port that and write the red test: entry k of GF's ring must equal D/(10(k+1)) on the two captures.
3. Entries 0 and 1 need their own term; decode with the same watch.
4. Until step 1-2, the GF feed (zeros from man velocity) is known to be wrong; trap refusal codes 7/0/3 are its visible symptom (round 17: zeroing the ring changes 162 of 5,071 live calls).
