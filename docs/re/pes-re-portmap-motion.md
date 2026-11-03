# Motion-side port map (static RE only; no engine code changed)

Scope: physics and motion. Ball flight, bounce and spin; the kick and shot
execution (KickMotion is pending — GF takes the motion id but 21 kicks get no
clip because GF's stand-in kick kinds miss the table rows); first touch
beyond the trap chain (TRAP end handler → reaction walk; TrapTick11..14
landed, `TrapS5`/`TrapTick` translated); tackles and collisions; player
locomotion and animation selection outside what StoppageFlow translated;
the keeper's mover (`KP_MOVE`, anime id 55, null table entry — GF's
`_BallControlCommand` stands in, `elizacontroller.cpp:793`); the referee's
movement (`Judge::Update 0x1407A14E0`, `LinesmanAction 0x1407A2920`,
`LinesmanMove 0x1407A1F30` — GF's `officials.cpp` only parks officials at
fixed spots); restarts' placement (`SetKick` writers incl.
`0x140732D40` placer, corner body `0x140732E30`, corner loop
`0x140732EB0` — GF walks the keeper/taker by MOVESTEP instead of holding
the standby and warping by root motion,
`docs/re/pes-re-kicker-standby.md` §7).

Reports referenced below are in `docs/re/`. Every VA below was re-checked
against the Ghidra bridge on 03-11-26 (decompile + `xrefs_to`); the ledger
(`docs/re/LEDGER.md`) does not contain the 0x14073xxxx/0x14082xxxx/
0x14083xxxx kick-area functions, so ledger statuses are given only where a
row exists. Hook windows were verified with `tools/dev/rendezvous/hook_plan.py`
(`displaced_window` over `pes_full.bin`); prose proofs (register roles,
caller chains, records read/written, closure sizes) come from the bridge
decompiles fetched into `<scratch>/gf-PortMapMotion-scratch/caps/`.

## 1. What is already ported (no hook requested)

- Ball flight / bounce / spin: `0x140874470` (kick-velocity response),
  `0x140876360` (contact tick, 922 insns), `0x140875170/0x140875580/
  0x140875A10` (foot response + spin cap), `0x1408723D0` (mode writer) —
  `ported` in the ledger, GF `pesballphysics.cpp`/`pesballcollision.cpp`/
  `ball.cpp` (one remaining stand-in: touch spin, `FUN_140870A40` →
  `0x140874860`, `ball.cpp:94`). `FUN_140A92C80/FUN_140A92D40` (sin/atan2
  tables, `pesteamconcepts`/`carrier`/`dodge`/`mover` stand-ins) are already
  translated (`pes-port-KickTerms.md`/`KickTerms2.md`: table baked at
  generation time since the image reads 0 at GF runtime).
- Kick scatter `0x14083CC30` + post-scales `0x1408414A0` + chooser
  `0x1408364D0` + quality `0x1408413C0` + situation factor Q: decoded,
  translated, proven on live calls (`pes-port-Kick*`); GF runs
  `PesKickChooser`/`KickQuality` with an oracle.
- Trap chain through the S5 tick: translated and driven in GF
  (`pes-port-TrapTick*`).
- MOVESTEP groups A/B/D, run/step/turn families: largely ported
  (`src/pes/movestep_*.cpp`, ledger `ported` rows).
- Tackle sub-kind pick (readiness mask, first-ready-kind order, row→clip):
  ported in `pestackle.hpp`; only the per-clip reach test
  `FUN_14078A640` (physics prediction through the clip solver
  `FUN_1406ECBB0`) is open.
- Contact: level table, vetoes, jostle: ported (`pescontact.*`); open is
  the per-part writers (`P+0x508`, `FUN_140842BE0` family) and the
  window-tail tests.
- Referee/judge sanction path: translated and switched (`FoulSanction`,
  `CardProof`); open below is only the officials' *movement*.

## 2. Ranked map (visible impact / cost to prove)

Cost: `S` = hook directly + small body (≤ ~300 insns, deps already
proved/ported); `M` = hook + medium closure (named deps, some already
proven); `L` = hook + new record layouts or large closure. Rank is by
(in-match visibility × known GF symptom) / cost.

| # | GF-native code (file:line) | PES function(s) | Entry VA | Caller chain | Records read / written | Closure | Impact (visible symptom) | Cost |
|---|---|---|---|---|---|---|---|---|
| K1 | `humanoid.cpp:61-78` (kind/bucket/window/motion-id/Q-in-flight defaults; `ApplyPesKickError`) | kick request builder `FUN_1407324B0` (copies `P+0x2BC8`, reads `P+0xAB8` clip iface, `T-newest` via `0x140A489F0`, mirror via `0x1406E6660(ap,0x20)`; yields heading + spot + `ap+0x2754/0x2755` bucket) | 0x1407324B0 | `0x140731FFB`,`0x140732316` → feeds `0x1408364D0` (every kick) and all three `SetKick` placers via `0x140732D40` | reads: AnimePlayer+0x2BC8 ctx block, iface +0xAB8, G, match ctx; writes: heading/spot floats (params 3/4), `ap+0x2BC8` mirror flop | ~250-line body + deps already proven (`0x140A489F0` diffed, `0x1406E6660` S, `0x14124DAD0`) | Every kick's alignment misses: GF pins kind/bucket/window/motion-id, so scatter Q's geometry terms, post-scales and all bucket-keyed terms stay off; KickMotion's 21 clipless kicks | M |
| K2 | same site | geometry tests `FUN_140829E80` (kind candidacy over `FUN_1408336C0` req, `FUN_140833CD0` header builder, `FUN_140723DE0` angle bucket, `FUN_140837XX` builders; called from `0x140839270/0x140839530` launch path and 8 AI sites `0x140821660/0x1408217D0/0x140823570/0x140824550/0x140824A30…`) | 0x140829E80 | launch path + AI (13+ call sites) | reads: kick request struct, clip iface +0xAB8/+0x2752, facing/body geometry; writes: `ap+0x2754/0x2755`, candidacy out-params | ~1570-line decompile; leaf deps (`0x1407240F0` switch, `0x140724590/5B0/D0` side/mirror tests) are small; the four already-ported deps (`0x14046F230`,`0x140470D60`,`0x1406B00D0`,`0x1406B03D0`) are `translated`/`stand-in`/`decoded`/`ported` | The kind decision itself: all bucket-keyed scatter terms + GF's pinned kinds | M |
| K3a | `humanoid.cpp:100-130` (shot launch + Q) | shot builder `FUN_1408371A0` (request 10; called only from `0x1408364D0+0x…bcf`) | 0x1408371A0 | chooser → builder | reads: kicker ctx + kind/bucket; writes: ShotLaunch request | small (~100–300-line class) | Every shot's aim/lift/params are GF's, not PES's | S |
| K3b | same (`...GroundPass` path) | ground-pass builder `FUN_140837670` (request 9; only from chooser) | 0x140837670 | chooser → builder | same | small | Every ground pass alignment | S |
| K3c | same (HighPass path) | high-ball builder `FUN_1408360F0` (request 8; from chooser + from ground builder `0x140837866`) | 0x1408360F0 | chooser / ground builder → high builder | same | small | Every lofted ball alignment | S |
| K3d | same (`s.entry2755`/first-time) | first-time byte `FUN_140837FC0` (called from header builder `0x140833CD0+0x…f0f`) | 0x140837FC0 | header request → first-time byte | reads header ctx; writes `ap+0x2756` | small | First-time kicks (volleys) miss the window byte | S |
| H1 | `ball.cpp` prediction/track; `matchevents.cpp:94` (intended touch `FUN_140870A40` unhooked — hook refused: rip-relative op in window) | bounce/contact chain head `FUN_140876020` (reached from `0x140872570` trampoline at `140872A6D` and from `0x14046E1D0+0x…3FB`; leads into the `0x140876360` tick GF already ports at the exact landing) | 0x140876020 | contact chain → tick | reads: ball ctx `rcx`, `rdx` record, `0x1437F4808` asset registry; writes: contact record + `B+0x94` last-contact | 116 insns; deps named in body | Pins the still-unproven half of the bounce: `0x140872570` trampoline + `0x140731BB0` copier are `undecoded` (windows refuse); GF stands the intended touch in `ball.cpp:94` (`FUN_140870A40`) and the Window-Tail/clip-contact halves of `pescontact.cpp` | M |
| R1 | `officials.cpp:29-33` (fixed park spots) + `referee.cpp:934` (`ResetPosition` at restarts) | `Judge::Update 0x1407A14E0` (vtable `0x1425E20A0` slot 4; gesture chooser `0x1407A1AC0`, blend test `0x1407A1D40`, cmd `+0x85C` → `cmd+0x30` 1–9; `docs/re/pes-re-foul-choreography.md` §4.4) | 0x1407A14E0 | vtable slot (DATA xref `0x1425E20C0`) | reads: referee AnimePlayer, gesture cmd, `MI+0x1554/0x1588`; writes: referee clip chain | small (gesture machine only) | Referee never moves/gestures per PES in GF (parked at (10,-10,0)) | S |
| R2 | same | `LinesmanAction::Update 0x1407A2920` (vtable `0x1425E2270`; flag near/mid/far from `MI+0x1564`, §5.3) | 0x1407A2920 | vtable slot (DATA xref `0x1425E2290`) | reads: linesman AnimePlayer, `MI+0x1554/0x1564`; writes: flag clips | small | Linesmen never raise/wave/point per PES in GF (parked) | S |
| R3 | same | `LinesmanMove::Update 0x1407A1F30` (vtable `0x1425E2188`) | 0x1407A1F30 | vtable slot (DATA xref `0x1425E21A8`) | reads: linesman AnimePlayer + move target; writes: locomotion clips | small | Linesmen never track the line in GF | S |
| M1 | `pesmovestep.cpp:486+` (relax/aim + ability + clip predicates held at stub answers), `movestep/turn` | run-loop inputs `FUN_1406E6660(ap,idx)` (ability byte; 7 in-ledger callers incl. kick-stat `0x14071D770`, movestep entries, scatter `0x1408397A0`) and entry flag `FUN_1406BA7D0` (relax/aim; 4 callers, all in the run-loop groups `0x1406CE820/0x1406D38D0/0x1406D6780/0x1406CCD00`) | 0x1406E6660 / 0x1406BA7D0 | locomotion entries | reads: AnimePlayer fields + cmd speed class; writes: byte/flag | 18 + 84 insns | Turn/relax gating + kick-stat byte are stubbed (`50`, `false`) across every locomotion entry and every kick | S |
| M2 | `movestep_run`/`step` (contact-frame + cancel-frame + hit-frame stand-ins, 0.85 divisor; toe-off window; replay-gate MI cells) | clip length `FUN_1406B10D0` + tick length `FUN_1406B11E0` (`decoded` OBS, 46/45 insns; the start-time formula `clipTicks − rand − elapsed`, `sim-motion-mapping` §14 item 3); record readers `FUN_1406AF620` (blend/fixed, 44 insns, 33 callers) / `FUN_1406AF8E0` (cancel, 47 insns, 20 callers) / `FUN_1406B1D00` (RotData end, 47 insns, 21 callers) / `FUN_1406B1F00` (RotData signed offset, 49 insns, 15 callers) | 0x1406B10D0 … `0x1406B1F00` (6 VAs) | STEP family + entries | reads: clip record (Mbinfo/HitData), motion record; writes: req words `0x3D–0x3F`, `P+0x2468` divisor | each ≤ 50 insns | Every STEP-family contact frame, blend length, cancel and toe-off window is a stand-in: kick root alignment, trap touch frames, restart strike windows | S |
| T1 | `pestackle.hpp:144-178` (`solverOk` = always-yes) | clip reach test `FUN_14078A640` → physics prediction `FUN_1406ECBB0` (steps the tackle capsule over HitData/Animation records vs carrier prediction) | 0x14078A640 / 0x1406ECBB0 | tackle pick → reach test → clip solver | reads: tackle record, candidate clip HitData, carrier prediction; writes: solved/not | ~100–200-line solver + record reads | Every tackle kind's reach gate is answered "yes": tackles that should miss hit, incl. keeper claims | M |
| C1 | `pescontact.cpp:260-368/517` (one rigid offset for all four parts; head/feet 2D; window-tail tests absent; tracked-part writer `P+0x508` undecoded) | per-part distance `FUN_140842BE0` (hip 3D, rest horizontal — max `FUN_140843C50`, feet gate `FUN_140843E30` 0.23 m) | 0x140842BE0 / 0x140843C50 / (`0x140843E30` shares the max's function, same window) | contact solver | reads: bone block `P+0x538`, part table `0x1425EFBE0` (HIP/FOOT_L/FOOT_R/HEAD); writes: max distance | 3 small functions | Contact/shoulder outcomes use a rigid offset, not per-part tracks; header lift + aerial reach shift | S |

Not ranked (documented, no hook — windows refuse or already hooked):
- `FUN_140870A40` (intended-touch writer, stand-in `OBS`, `ball.cpp:94`, `matchevents.cpp`; 228 insns, ledger `ball/stand-in`): hook window REFUSED (rip-relative operand). Reach through K1/H1 instead; the already-hooked scatter entry `kick_scatter_83CC30` + `kick_postscale_8414A0` cover the launch.
- `FUN_140A93070` (candidate-builder callee, `pure`, 13 callers via `0x140874C28` and AI sites incl. `0x140829E80`): window REFUSED. Reach through K2/H1.
- `FUN_140731BB0` (candidate copier to simBall, `decoded OBS`, 118 insns, 14 callers): window REFUSED (branch at entry). Reach through H1 (`0x140876020`).
- `FUN_1408413C0` (descending-sort soft-min fold, called 6× from scatter entry `0x14083CC30`, already hooked): window REFUSED. No new hook.
- `FUN_140A638C0` (action-table ownership, `stand-in OBS`, 41 callers): window REFUSED (jbe in window). Already queued by StoppageFlow (`action_switch_6EEBB0` proved 31/31, `TrapTick6`).
- `FUN_1406B7560` (command-word fill `782` insns, `stand-in`), `FUN_1406ABB30` (anime-table lookup wrapper, 33 callers, `stand-in`): windows REFUSED (branch in window).GF's `MoveHostAdapter::FillCommandWords` stands in; needs translator/harness work, not a hook.
- `FUN_1406D6290` (HOME-based step handler, 287 insns, `stand-in CONFIRMED`): PASS but parked — its home position/angle inputs are not in GF; hook once the formation home record lands.
- `FUN_140A6E290/0x140A6E2E0` (clip predicates, `stand-in`): PASS at the shared entry `0x140A6DFD0` but that entry fans 4+ predicates; hook once `TrapTick12+` names the needed predicate state.
- Keeper `KP_PRESS` think/decider chain (`FUN_142115100/1421162C0`, rush race) and base-position core `0x1409E0390`: GF ports their formulas already (`peskeeper.cpp`); no hook.
- Corner body `0x140732E30` + placer `0x140732D40` ARE requested below (M3) — the corner loop `0x140732EB0` decompile is included as their caller evidence.

Ranked hook requests (12): K1, K2, K3a, K3b, K3c, K3d, H1, R1, R2, R3, M1 (as one `movestep_inputs` pair: 0x1406E6660 + 0x1406BA7D0 share the every-entry purpose — counted as one request, two VAs), M2 (one request: the six record-reader VAs; they are one proof unit: contact-frame placement). T1 and C1 round out the 12 lines below (each one request line; C1's two VAs share one function + window as well).

## 3. Hook requests (12; all PASS)

Conventions: cap 12 each (matches `rvwatch` practice for unproven chains); `--when` predicates use the entry registers the decompiles show (`rcx` = AnimePlayer `ap`, `rdx` = request/player ctx). Each names the GF code it replaces.

```
# --- PortMapMotion block (motion-side map; append-only; see docs/re/pes-re-portmap-motion.md) ---
# H1 kick request builder: every kick (any call). rcx=unused ctx, rdx=AnimePlayer ap.
TARGETS += [("mot_kick_req_7324B0", 0x1407324B0, "kick request builder (heading/spot/bucket; replaces humanoid.cpp PesKickSituation defaults)")]
# K2 geometry candidacy: run when a real kick request flows (called from launch path or AI): param_1 = kind.
TARGETS += [("mot_kick_geom_829E80", 0x140829E80, "kick geometry tests (replaces pinned kinds humanoid.cpp:64)")]
# K3a/b/c/d request builders: any call (each fires only on its request kind inside chooser/header paths).
TARGETS += [("mot_kick_shot_8371A0", 0x1408371A0, "shot builder req-10 (replaces GF shot params)")]
TARGETS += [("mot_kick_ground_837670", 0x140837670, "ground-pass builder req-9 (replaces GF ground params)")]
TARGETS += [("mot_kick_high_8360F0", 0x1408360F0, "high-ball builder req-8 (replaces GF lofted params); high-ball family writer 0x1408272A0 reached through it")]
TARGETS += [("mot_kick_first_837FC0", 0x140837FC0, "first-time byte ap+0x2756 (replaces first-time 0 in PesKickSituation)")]
# H1 contact-chain head: fires on the ball-contact path (any call; replaces ball.cpp touch-spin stand-in + pescontact window-tail gap).
TARGETS += [("mot_contact_head_876020", 0x140876020, "contact-chain head into the 876360 tick (ball/contact oracle)")]
# R1/R2/R3 officials' movement: any call (each fires only when its action Updates).
TARGETS += [("mot_judge_upd_7A14E0", 0x1407A14E0, "Judge::Update (replaces parked referee in officials.cpp)")]
TARGETS += [("mot_linesman_upd_7A2920", 0x1407A2920, "LinesmanAction::Update (replaces parked linesman)")]
TARGETS += [("mot_linesman_mov_7A1F30", 0x1407A1F30, "LinesmanMove::Update (replaces parked linesman)")]
# M1 locomotion entry inputs: any call (both fire on every locomotion entry / stat read).
TARGETS += [("mot_ability_6E6660", 0x1406E6660, "ability byte (replaces AbilityByte stub 50 in movestep_turn.cpp + kick-stat byte)")]
TARGETS += [("mot_entry_aim_BA7D0", 0x1406BA7D0, "entry relax/aim flag (replaces EntryAimFlag false in movestep_turn.cpp)")]
# M2 clip-record readers: any call (STEP family contact/cancel/rot readers + start-time pair; replaces movestep_run/step stand-ins).
TARGETS += [("mot_cliplen_6B10D0", 0x1406B10D0, "clip length ticks (start-time formula; replaces frame-count stand-in)")]
TARGETS += [("mot_ticklen_6B11E0", 0x1406B11E0, "tick length (start-time formula pair)")]
TARGETS += [("mot_blendrec_AF620", 0x1406AF620, "blend/fixed record (replaces 0.85-divisor stand-in)")]
TARGETS += [("mot_cancelrec_AF8E0", 0x1406AF8E0, "cancel-frame record (replaces no-record stand-in)")]
TARGETS += [("mot_rotend_6B1D00", 0x1406B1D00, "RotData end (replaces offset stand-ins)")]
TARGETS += [("mot_rotoff_6B1F00", 0x1406B1F00, "RotData signed offset (replaces offset stand-ins)")]
# M3 corner placement chain: SetKick placer (reached from all five standby SetKicks) + corner kick body + placer caller.
TARGETS += [("mot_setkick_place_732D40", 0x140732D40, "SetKick placer (replaces GF MOVESTEP walk-to-spot in pessetpieceteam/goalkick)")]
TARGETS += [("mot_corner_body_732E30", 0x140732E30, "CornerKick::Update phases (kick-off of restart placement proof)")]
# T1 tackle reach: any call (fires per tackle-kind row test; replaces solverOk=yes in pestackle.hpp).
TARGETS += [("mot_tackle_reach_78A640", 0x14078A640, "tackle reach test (replaces solverOk always-yes)")]
TARGETS += [("mot_clip_solver_6ECBB0", 0x1406ECBB0, "clip solver physics prediction (reach-test callee)")]
# C1 tracked parts: any call (fires per contact test; replaces rigid offset in pescontact.cpp).
TARGETS += [("mot_tracked_842BE0", 0x140842BE0, "per-part distance (replaces rigid offset; bone block P+0x538 + table 0x1425EFBE0)")]
TARGETS += [("mot_tracked_max_843C50", 0x140843C50, "max over parts (same window as feet gate 0x140843E30)")]
```

Window verdicts (from `hook_plan.py`'s `displaced_window`, re-run for this
report; all PASS):

| Target VA | entry | window |
|---|---|---|
| 0x1407324B0 | self | 20 |
| 0x140829E80 | self | 15 |
| 0x1408371A0 | self | 19 |
| 0x140837670 | self | 16 |
| 0x1408360F0 | self | 14 |
| 0x1408272A0 | self | 15 |
| 0x140837FC0 | self | 19 |
| 0x140876020 | self | 14 |
| 0x1407A14E0 | self | 16 |
| 0x1407A2920 | self | 14 |
| 0x1407A1F30 | self | 14 |
| 0x1406E6660 | self | 17 |
| 0x1406BA7D0 | self | 16 |
| 0x1406B10D0 | self | 14 |
| 0x1406B11E0 | self | 14 |
| 0x1406AF620 | self | 14 |
| 0x1406AF8E0 | self | 14 |
| 0x1406B1D00 | self | 14 |
| 0x1406B1F00 | self | 14 |
| 0x140732D40 | self | 15 |
| 0x140732E30 | self | 15 |
| 0x14078A640 | self | 15 |
| 0x1406ECBB0 | self | 21 |
| 0x140842BE0 | self | 15 |
| 0x140843C50 | self (= feet gate 0x140843E30's function) | 15 |

Suggested `--when` predicates (register evidence above; `ret` is
available in `--when`):
- kick builders + geom + request: `any call`, cap 12 (each fires only on
  its request kind; the request writer + chooser already sample every kick
  via `kick_chooser_8364D0`).
- contact head: `any call`, cap 12.
- judge/linesman: `any call` (vtable-dispatched; fires only when the
  action is current), cap 12.
- locomotion/record readers: `any call`, cap 12.
- tackle reach/solver, tracked parts: `any call`, cap 12.

GF code each replaces (clean cutover, no shims): `PesKickSituation` +
`ApplyPesKickError` kick-kind/bucket/window/motion-id/Q-in-flight defaults
(`humanoid.cpp:61-130`); `ball.cpp:94` touch-spin stand-in;
`officials.cpp:29-33` park spots (+ `referee.cpp:934`);
`EntryAimFlag`/`AbilityByte`/`ClipRunning/Halted/Fading` stubs
(`movestep_turn.cpp:119-139`); contact/cancel/RotData/toe-off stand-ins
(`movestep_step.cpp:1432-1448,1629-1712`, `movestep_run.cpp:1414-1469`);
`solverOk` (`pestackle.hpp:147-178`); rigid tracked-part offset
(`pescontact.cpp:260-368,517`).

## 4. Open / parked (no hook requested now)

1. Keeper mover `KP_MOVE` (anime id 55, **null table entry**:
   `GetAction` falls back to id 1; `docs/re/pes-re-fox-registry.md` §3):
   undecoded — the keeper rush is decided (`peskeeper.cpp` ports the race)
   but the rush *locomotion* driver has no code VA to hook (no table
   entry, no Update VA). Next: find what issues the keeper's MOVE clips
   during `KP_PRESS` (candidate: default locomotion driver
   `FUN_1406F0C20`, `kicker-standby` §1, which runs when slot 10 is false).
2. First touch beyond the trap chain: TRAP end handler
   `0x14079E5C0` is already hooked (diagnostic) and the reaction walk is
   landed; the remaining gap is the `+0x2FC` ring writer (TrapOpen2:
   decoded from tick snapshots, writer open).
3. `FUN_1406D6290` (HOME-based step, 287 insns, PASS window 15): parked
   until the formation home position/angle records reach GF.
4. Clip predicates `0x140A6E290/0x140A6E2E0` (PASS at shared entry
   `0x140A6DFD0`): parked until the needed predicate state is named.
5. `FUN_1406B7560` (cmd fill) / `FUN_1406ABB30` (anime lookup):
   windows REFUSE — translator/harness work, listed §2-bottom.
6. Ball spin magnitude live (`kick.md` §1.7: simBall not located) and
   velocity field (§2.4): H1's captures should settle both (contact
   record + `B+0x94` last-contact vs `B+0x20` spin).
7. Mode pitch-vs-weather (`kick.md` §4): H1 + live `cfg+0x78/0x7c` reads.
8. Kick spin branches A′/B′ callee names (`kick.md` §7 item 5):
   K1/K2 captures carry them.

## 5. Method note (for Main)

- `hook_plan.py` was run unmodified against `pes_full.bin` (baseline:
  all 61 existing targets PASS, `plan0/`); the 25 new VAs above were
  checked with the same `displaced_window` predicate (inline script,
  §3 table). Full `plan.json` run including the new block happens after
  the append lands (cheap, ~15 s).
- Bridge batching: single `decompile_function`/`xrefs_to` calls, one at
  a time, ~1–9 s each; total bridge use ≈ 40 requests.
- Ledger gap: the 0x14073xxxx/0x14082xxxx/0x14083xxxx kick-area functions
  are absent from `tools/dev/ledger/universe.json` (key ranges jump
  `0x140731BB0 → 0x1407418F0`, `0x140820D70 → 0x140841DC0`), so §2 cites
  RE reports + bridge output instead of ledger rows there. PortMapAI may
  want the same note if their slice hits that range.
