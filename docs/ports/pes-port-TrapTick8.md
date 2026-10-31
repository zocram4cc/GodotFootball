# pes-port-TrapTick8: 0x1406E85B0 proven (1/1) and both PesForfeitMatch tests green; the switch-off hash move is one gate (commit 41f83d6f); LoaderData is mod captures vs the No-Mod mount; pes_stall_test still RED (31-10-26)

Branch `StoppageFlow`. Image `pes_full.bin` (base `0x140000000`). CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Verdict

| Item | State |
|---|---|
| 1. `git merge origin/main`, push | done (`4fa50e23`; docs-only merge) |
| 2. `0x1406E85B0` | **PROVEN**: `snapdiff.py --bounded --hooks hooks.cfg --only 0x1406E85B0` over `live2/pid3439759/seq10956` (the only `anime_displace_6E85B0` capture that landed; FULL): `calls=1 replayed=1 mismatches=0 own_block=1.0 (4/4) proven=True` (CONFIRMED). `closure_trapanime.json` now 8 members (`0x1406AF6D0, 0x1406AFF90, 0x1406B05E0, 0x1406B0ED0, 0x1406E72A0, 0x1406E85B0, 0x1406E8B70, 0x140A92C80`), record in `results_trapaction.json`, spec `specs/trapaction/1406E85B0.json`, layer regenerated 4555 functions, `gen_translated.py --check` rc 0 (CONFIRMED). Commit `aa6b34df` |
| 2. `PesForfeitMatch` | **GREEN**: all six tests ran OK (five in one run, `AYellowThenRedStillOnThePitchAtFullTimeForfeitsThreeNil` rerun alone after my 1800 s timeout cut the first run), incl. `ASideAtTheMinimumKeepsPlaying` (944 s) and the yellow-then-red one (934 s) (CONFIRMED) |
| 3. switch-off hash `e663d1e917d6c4f4` -> `4a350d095284288e` | **explained** (section 2): one hunk, commit `41f83d6f`, a PES-fidelity move; allowed. New switch-off baseline `4a350d095284288e` |
| 4. `pes_stall_test.ALiveBallNeverLiesStillLongEnoughForTheBreaker` | **STILL RED** (2 stalls, deterministic); trace in section 3; no fix (root not established) |
| 5. `LoaderDataTheTickReadsEqualsTheCapture` | **decided** (section 4): mod captures vs the No-Mod default mount, GF is right; oracle re-take needs FULL No-Mod captures and a Mbinfo kind-4 loader (first full one shows the gap) |
| 6. `GATE fast` | see section 6 |

## 1. 0x1406E85B0 (CONFIRMED)

`FUN_1406E85B0(iface, out, motion, pos, yaw, mirror, 0, 0)` (called by the trap tick at `0x14079C5EC`, ret `0x14079C5F1`, on the `*(char*)(action+0x26) == 0` branch): calls `FUN_1406E8B70` and rotates the result by yaw. The replay needed the hook's displaced bytes (`snapdiff --hooks /path/to/hooks.cfg`, line 57 of the install's `rendezvous/hooks.cfg`): without it the replay fetches the hook trampoline (`0x25a3700`) and excludes the call.
Commands (all under `GF_DIFF_PYTHON`, from the worktree): `closure.py --spec-set trapaction --tag trapanime2 --subsystem restart --members-file root.json 0x1406E85B0` (spec only; the set file removed again, as for the switch and `0x1406E72A0`), `frame_trace.py recorded <snap> --hooks hooks.cfg --out <trace> --spec-dir specs/trapaction`, `snapdiff.py --bounded --spec-dir specs/trapaction --hooks hooks.cfg --only 0x1406E85B0 --snapshot S=T --results res.json`, then the closure and results records, `gen_translated.py`.
**Layer regeneration rewrites every file's mtime** (4555 files, identical bytes), which makes ninja recompile all 4572 objects: after regenerating, `git ls-files src/pes/translated | grep -vxFf <changed-files> | xargs touch -d <older than the objects>` keeps the build incremental (done here; 21 objects rebuilt).

## 2. Switch-off hash: one hunk (CONFIRMED by mutation, 2-min seed 7, `gfsim_run` without `--pes-loop`)

| Build | `[determinism]` |
|---|---|
| origin/main (batch AR line) | `e663d1e917d6c4f4` (33628 ticks) |
| StoppageFlow head | `4a350d095284288e` (24839 ticks; TrapTick7's run, reproduced by the `PhysiqueG` = 1.0 row; my own head run timed out waiting for a sim slot) |
| head, `PhysiqueG()` = 1.0 | `4a350d095284288e` (G is not it) |
| head, old `TopRate` arithmetic | `5fddb5ee63de550e` |
| head, old arithmetic and G = 1.0 | `5fddb5ee63de550e` |
| head, old arithmetic, G = 1.0, `BallDodgeEntryGate` off | `e663d1e917d6c4f4` |
| head, `BallDodgeEntryGate` off only (new arithmetic, derived G) | **`e663d1e917d6c4f4`** (33628 ticks) |

- Cause: commit `41f83d6f` ("ball dodge entry gated by the ball's step over the lead time, FUN_140a070e0"): `ElizaController::_DodgeBallCandidate` returns false when `pes::BallDodgeEntryGate` fails, so a man never dodges a ball at rest. `_DodgeBallCandidate` is not `pes_loop`-gated (`_ReactionCommands` runs in both modes), so the switch-off path takes it. It is a PES-fidelity move (PES's entry 0x140A070E0 head), not an accident: allowed; the baseline moves to `4a350d095284288e`.
- The `TopRate` operation-order change (commit `5b4f4506`: PES's order at `0x1406EAE00` / `0x1406F9B4C`) does not move the hash by itself (gate off + new arithmetic = main's hash); with the gate on it changes the later trajectory (`5fdd...` vs `4a35...`), so it is a second PES-fidelity move that only shows once play has diverged.
- No bisect by checkout was needed: the single-hunk mutation lands exactly on main's hash.

## 3. The two live-ball stalls (CONFIRMED trace; NOT fixed)

Temporary per-tick prints (`_ReceiveCandidate` stage that refused, the Trap chain's refusal status, a presser census at the breaker; patch kept in scratch, reverted) on `pes_stall_test` (seed 7, 2 min, deterministic, same 2 stalls):
- Stall 1 (5.0 s, ball at rest 0.08 m/s at -7.7,17.9): H11 and A6 are both loose-ball pressers (`IsLoosePresser`), 2.4 m and 1.5 m off, converging. For ~5 s (ticks 24675..25275) every man in reach is **refused by the Trap chain** (`FUN_14079A470`), statuses 1 (the ball out of the reach test at `0x14079A4F1`..), then 2 and 10 (candidate search `FUN_140857220` found no clip; the status is its `local_98`). The receive entry itself says yes (`recv=1 why=yes`).
- Stall 2 (7.1 s, ball at rest at -0.3,-14.8): no presser anywhere. A11 stands **0.26-0.30 m** from the ball in an idle Movement clip (`movestep 1<-6`), not designated (the PES designated owner is A3, the last toucher, refused by gate 4c). A11's receive entry says yes, `_TrapCommand` runs as kind 4 (planned touch: the untranslated kind-4 chain is replaced by the `kKindFourUntranslated` refusal and PES's retry as kind 0) and the control retry is refused with status 2 every tick for 7 s. The breaker's `designatedPossessionPlayer = A11` then makes him `HasPossession` and the carrier brain moves the ball at once (hasP=1, d 0.25 m).
- So the stall is the Trap chain refusing a man standing at or near a ball at rest: INF: either PES refuses him too and a different PES entry moves him (the carrier / holder brain, which GF only runs for `HasPossession` men: the `hasIt` stand-in, radius 1.0 m, `AI_HasPossession`), or GF's derived inputs of P (ball relative position at rest, speed class) make the candidate search fail where PES's would not. OPEN: no PES capture of `trap_kind0_79A470` with a ball at rest within 0.5 m exists to tell which.
- Next step: a `trap_kind0_79A470` capture with `--when` the ball speed below 0.3 m/s and the man within 1.5 m (Main), then replay it on GF's window (the kind-0 oracle) and compare the candidate search; or decode `FUN_140857220`'s status-2 reason (`local_94 == 2` branch at `0x14079A9xx`). Not a translation gap.

## 4. LoaderData: stock vs mod (CONFIRMED)

- constant::Trap payload `+0x8` read from the captures through the registry (`0x1437F4808 -> entry[0x3E] -> +8`): `live2/pid483669/seq6634` (an oracle capture, launch before the pin) = `0x0a`; `live2/pid3061186/seq2016, seq2028, seq2035` (No-Mod launch, after the 30-10-26 14:35 pin) = `0x07`, byte-identical to the 0x40 bytes GF reads. The old 14 oracle dirs are mod captures; GF's default mount is "No Mod" since VfsOrder (the loader's answer is mounted by `VfsOptions{gameplayMod}`), so the GF value is stock and right. The test's comment now says so.
- Re-take: trap_oracle.py needs FULL captures (a slim capture lacks the actor-fact pages: 8 of the 12 No-Mod slim ticks replay PROVEN but `trap_oracle.py` fails reading the slot record). `pid3439759/seq3244` is the only full No-Mod tick that existed: its oracle dir built (181 manifest lines). On it `LoaderData` shows the Trap constant equal (the old failure is gone) but a new loader gap: `motion 0xda9 counts +0x4: capture 01, GF 00` (Mbinfo **kind 4**: `kTrapMbinfoKinds` has kinds 0, 1, 5, 6, 13 only). And `EveryCapturedTickOnGfsWindowEqualsPes` differs on that capture's derived P (`+0x47e` GF ff / capture 09, `+0x73e/f`): a P derivation gap for that man's state.
- Main restarted PES at 15:25 (`live2/pid3741448/`) with the tick captures kept FULL: 12 `trap_action_tick_797CD0` (seq1877..1982, 4762..4803) and 6 `trap_tick_callee_84F140` landed. Next: rebuild the oracle from them (`mk.sh` shape in scratch: `replay_verdicts.py --hook 0x140797CD0` then `trap_oracle.py`), add Mbinfo kind 4, then the LoaderData test is a relation over No-Mod captures.

## 5. New untranslated call: 0x14084F140 (reported to Main early)

`replay_verdicts.py` (unbounded snapdiff) over the 12 No-Mod slim tick captures of `pid3061186`: 8 PROVEN (`seq12099, 12607, 2016, 5902, 5927, 5940, 9794, 9798`), 2 `FAIL w` (`seq2028, seq2035`: a written-byte mismatch of the unbounded build, not yet analysed) and 2 `PES_DISPATCH_UNTRANSLATED: 0x14084f140` (`seq5907, seq5921`). `hook_plan.py`: PASS, window 14 bytes. Main added `trap_tick_callee_84F140` (cap 12, FULL): 6 captures in `live2/pid3741448/` (seq4762, 9095, 9107, 9541, 9978, 10028). Not proven here: same recipe as section 1.

## 6. Gate

See the final message of this run (`GATE fast` line or the interim). Sims ran directly at `nice -n 19` when slots 2/3 were held by CardLevel7 and ProcCamera5 (the gate held 0/1): the stall census, the five switch-off smokes (about 3 min each) and the tick oracle run.

## 7. Unfinished (numbered)

1. `pes_stall_test.ALiveBall...` (section 3): needs a PES reference for a loose ball at rest within 0.5 m (or the status-2 decode); no fix landed.
2. `0x14084F140` proof (6 FULL captures on disk), then the layer regeneration (with the mtime restore of section 1).
3. LoaderData oracle re-take from the 12 FULL No-Mod ticks of `pid3741448`; Mbinfo kind 4 in `kTrapMbinfoKinds`; the derived-P differences of seq3244; the two `FAIL w` replays (`pid3061186` seq2028, seq2035).
4. More `anime_displace_6E85B0` captures: none landed besides seq10956 (the launch hook predicate `ret==0x14079C5F1` is rare); the one proof stands (own block 4/4).
5. The `[stalemate]` print in `PesForfeitMatch.ARedCard...` shows a 56-80 s live-ball stall (A5 taker at x=-51.5, A1 forced from 10 m, in a jostle clip): not asserted by that test, same family as item 1.

## 8. Tests and mutations

- `0x1406E85B0`: no code mutation (generated unit, bit-exact replay). Hash mutations of section 2 (each rebuilt, run, reverted): `PhysiqueG` = 1.0; old `TopRate`; both; `BallDodgeEntryGate` off (with and without the old arithmetic). No engine code changed on this branch in this run besides the test comment.
- Ran: `pes_forfeit_test` (6 tests, all OK), `pes_stall_test` (1 OK, 1 FAILED as before), `pes_trap_oracle_test` on the new oracle dir (`EveryCapturedTickOnGfsWindowEqualsPes` and `LoaderDataTheTickReadsEqualsTheCapture` red on seq3244 as section 4; the other two TrapTickOracle tests not read), `snapdiff`/`gen_translated.py --check` rc 0.
