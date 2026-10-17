# A2Wiring report

Result: Done for the wiring scope. `Match::BallEvent` is deleted; every reader reads the store. The smoke has moved and is not judged here.

## Commits (refs/heads/A2Wiring, head 7b35f1e)
- `15051e4` the match owns PES's event store; the free-file consumers read it
- `15de805` the gdext glue loads the event constants (ClipSwap's files, separate commit)
- `8ec07ff` deletes `Match::PushBallEvent` (no callers)
- `a71e5c9` merge of origin/ComStyleGaps (`3526b1b`): the elizacontroller reads (dodge window, `_Carrier`) moved to the store
- `7b35f1e` deletes `Match::BallEvent`, `GetBallEvent`, `ballEvents`, its reset, and the remap in `ReplacePlayerReferences`

## What changed
- `match.{hpp,cpp}`: `pes::EventStore pesEvents`, routing table and constants (required in MatchSetup), touch queue, detector words, `GetPesEvents`, `QueuePesTouch`, `GetPesActorPlayer`, `PesActorKeyOf`, `GetPesEventAge_s`. `Process()` runs `ProcessPesEventStage()` after the ball tick and before the team tick.
- `matchevents.cpp` (new): ball-event family per queued touch (+K=11), then K=30, K=1, K=16, K=12, K=13, K=10, K=9.
- `team.cpp`: touch writer queues the touch; latch step 2 and `PassGetReceiver` read the store. `PesEventKindOf` and `BallEventKindOf` removed.
- `playercontroller.cpp`: re-touch lockout, shot-through, first-touch input read the store.
- `elizacontroller.cpp` (ComStyleGaps): human dodge window and `_Carrier` read the store.
- `pesevents`: `EventAt`, `kAction*`. `pesreceive`: `ReceiveEventKindOf`.
- Setup: `matchsetup.hpp`, `apps/gfsim_run.cpp`, `tests/sim/pes_match_constants.hpp`, gdext `pes_library`, `gf_match.cpp`.

## Substitution (corrects the earlier report)
My earlier note said the store keeps the outgoing player after a substitution. That was wrong. `Team::SubstitutePlayer` swaps the two players in the squad vector, so the incoming player takes the outgoing player's squad index. The store key is that squad index (PES keys by roster slot), so old events already name the incoming player, which is what the deleted remap did. The remap was redundant, not a behaviour loss.

## Tests (red first)
- `tests/sim/pes_event_wiring_test.cpp` (new, 4): PlayerKeysRoundTripThroughTheSquad; AQueuedTouchIsRoutedIntoTheStoreByTheNextEventStage; ThePossessionLatchFollowsTheNewestEventsSide (red while the latch still read BallEvent, green after the cutover); ALatchFlipIsRecordedAsKindTwelve.
- `tests/pes_events_test.cpp`: EventAtReadsTheNthNewestRingZeroEvent (red: `EventAt` missing).
- `tests/sim/pes_receive_test.cpp`: StoreActionKindsMapOntoTheReceiveKinds (red: `ReceiveEventKindOf` missing).
- Test bug I fixed in my own test: the EventAt test set the header frame, not payload+0x10.

## Verification (direct binaries; no gate, no lock, per main's rule)
- pes_events_test 40/40; pes_receive_test 21/21; pes_event_wiring_test 4/4; sim_match_test 8/8; pes_tick_a1_test 18/18; pes_clock_match_test 9/9; pes_carrier_test 38/38; pes_image_test 18/18; pes_dodge_test 45/45.
- `ctest -R "^godot_match"`: godot_match_test and godot_match_scene_test pass (merged tree).
- Full build clean on the merged tree.
- NOT run by me: full ctest gate, gfsim_run smoke (main's per-batch job).

## Smoke, as reported by ComStyleGaps (not re-run here, not tuned)
- main: 45-41 / `63286486ec6c84de`
- 15051e4 with ComStyleGaps' Long commits only: 40-37 / `975e0c6627598d66`
- with both edits: 38-31 / `adec20bd1d955fa4`
The event store and the cutover move the smoke. I have not judged whether the new numbers are correct.

## Open items (each named in matchevents.cpp)
- Tick order: listener vs MatchSub is OPEN (gaps-a2 a4). Event listener taken first.
- Referee stays at the head of `Process` (a3). MA stand-in built once at the head of the team tick (a1).
- Phase and state words wait on A6 (period record). Phase = play, state = 0.
- K=9/K=10 gate `gateOpen = true` (C8). GF has no closer for MI+0x11b4.
- K=10 side is a stand-in for the OPEN writer at G+0x2798 (gate C4): restart team + 1.
- K=9 sample height is INF (+4 word as height). K=12/K=13 axis mapping is INF (GF x as PES x, GF y as PES z); K=13 direction word fixed at 1; stoppage flag false.
- Ball-family inputs: state word = in play; PES id = team*11 + min(squad index, 10); roster, kick tail and holder words zero.
- K=30 never fires (GF has no stoppage state 2). K=14, type-14 and kind-27 are not run (no GF producer or consumer).
- `ballSumRaw` stand-in 0.

## Not resolved
- `tests/pes_possession_test.cpp` is not built by any CMake target, so its latch tests never run. I removed only the test of the deleted `PesEventKindOf`. Main should wire it or delete it.
