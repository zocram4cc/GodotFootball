# PES 2021 reverse-engineering reports

How PES 2021 does things, documented precisely enough to reimplement them
1:1: file formats, runtime assembly, selection logic. Every claim is tagged
**OBS** (observed, and how) or **INF** (inferred, with confidence). Killed
hypotheses are recorded with what killed them. Where the implementation later
proved a claim wrong, the report carries the correction and says so.

Sources: static analysis of the game executable, reads of a running game's
memory (never written), and the user's own install through `src/pes`.
Verbatim PES values appear here as evidence (owner decision, AGENTS.md); code
never takes a value from these reports, it reads it from the install at the
address given.

|Report|Covers|
|---|---|
|`pes-ai-map.md`|**the AI index**: every PES team and player AI system, its RE status, the report that holds it and the GF code it replaces; the documentation standard a port needs|
|`pes-re-protection.md`|the protection (Denuvo: moved plain code, ~99 mutated bodies, ~183 VM-entered Denuvo routines); live code equals disk; the full-image Ghidra project PESLive and the resolve/dec/annot/emu recipe; keeper deflect, referee set-piece positions and hidden constants unblocked|
|`pes-re-fox-registry.md`|there is no Fox method registry: the dispatch records are MSVC RTTI and every vtable is static (13,816 resolved); the action dispatch chain, the corner-kick state machine, the sequencer holds, and why post/net response is not virtual|
|`pes-re-dead-code.md`|dead-code census of all 165,185 functions by static reachability: 17 dead (library/serializer helpers, audio-middleware threads, one mutated thunk), nothing in player/team AI or the 0x140400000-0x140b00000 match range dead; 757 reachable only through Denuvo; the match-ball query does have a caller (Match General Settings menu), and the tactics-clock tick slot is 0x1426329b8, both correcting earlier reports; data-gated deadness out of reach of the method|
|`pes-re-tooling-ghidra.md`|the RE toolchain: GhidraMCP 1.4 on Ghidra 12.1.2 (two fixes), the headless postScript path, and how agents call both (batching by symbol, disassembly cross-checks on branches, instrumenting the port before deciding a fix)|
|`pes-kit-assembly.md`|kit configs (`UniformParameter.bin`, realUni), part selection, kit texture binding, the 4cc composition, live draw lists, the EDIT save cipher and appearance records, boots/gloves, skin tone|
|`pes-re-shaders.md`|shader packs, the deferred G-buffer and lighting model, per-shader texture roles and formulas, a Godot recipe|
|`pes-re-ftex.md`|`.ftex` arrays, cubes, volumes, chunk rules, a census of all 65,339 textures|
|`pes-re-anim-blend.md`|animation playback: linear crossfades with caller-set durations, the two-layer stack (base, officials-only upper), mask types 4-6, the startTime formula (no RNG), playbackSpeed per clip family, root motion with the AdjustTrans correction|
|`pes-re-anim-select.md`|how PES picks and blends player clips: the 24-state locomotion machine over Animetable.bin, turn buckets, the run blend, playback-speed formulas, Speed→top speed and Acceleration→ramp, crossfades, kick root alignment; live-checked on 25 bodies|
|`pes-re-anim-select-2.md`|animation follow-ups: Speed/Acceleration/Defensive Awareness bytes; blend styles 1–5, 9 and 11–14 dead; Freemove.bin unread; `FUN_140a581c0` = dash-reserve percentage|
|`pes-re-locomotion.md`|player movement: no speed formula - displacement is the clip root track times AnimMovePlugin scale and playbackSpeed; speed classes, member root speeds, turn buckets, the plugin scale clamp; playbackSpeed driver and the Speed coefficient open|
|`pes-re-animselect.md`|motion ids, 30 fps clips, 1/16384 root scale, situation tables, locomotion graph, blends, the hand rig|
|`pes-re-physique-helpers.md`|the physique helpers: slider decode and normalisers, the per-bone scale tables, the global scale and how it reaches the skeleton and animation|
|`pes-re-physique.md`|height/weight/sliders to per-bone scale, global scale and stride|
|`pes-re-marking-pixels.md`|texel-exact marking rasterisation: digit cells, name layout, the leg-number transform, armband and badges|
|`pes-re-markings.md`|numbers, names, captain armband, badges, GK and referee kits|
|`pes-re-collision.md`|`.frig` format (incl. the hand unit map), ball/player capsules, contact data|
|`pes-re-face-rig.md`|the face rig (face_anim_skel.ask and its repair), the 181 face clips and their channels, and the edit-face composition|
|`pes-re-face.md`|real-face and edit-face heads, `face_diff.bin`|
|`pes-re-camera-target.md`|Dynamic Wide camera internals: focus and its lead, target.y lift, the k spring, clamp blend, the zoom/height sliders, mode ids|
|`pes-re-pitch-grass.md`|the pitch, line and turf shaders as port-ready formulas, verified numerically against the compiled DXBC|
|`pes-re-stadium-camera.md`|nested stadium cpks, pitch dimensions, crowd, the in-play camera|
|`pes-re-fieldinfo.md`|PES's pitch geometry record: all 18 FieldInfo floats read live, 15 named (net roof/floor back, centre circle), none readable from the exe (named constants); GF's goal depth is 0.68–0.79 m too deep|
|`pes-re-misc-leftovers.md`|officials chain (the pick rule lives in fixture-VM code), FieldInfo F11 = goal-mouth back plane, the runtime net layout (0x60 stride), the injury registry; live items open|
|`pes-re-effects-particles.md`|the Tpp Effect system: the runtime path, the particle update rule, weather, the post chain, and the negative result on in-play emitters|
|`pes-re-audio.md`|the audio system (in-exe CRIWARE ADX2), the match sound inventory, the trigger-to-cue condition trees, and the crowd|
|`pes-re-lighting.md`|light rigs, sun, sky, fog, weather, the post chain|
|`pes-re-hud-ui.md`|the match HUD as a Flash/AS3 system: every overlay element with its region, trigger and data, plus the layout and scaling rules|
|`pes-re-duels-fouls.md`|duels (no stat roll; contact frames and capsules decide, abilities gate), the Judge foul points and 40/80/110 thresholds to card levels, the penalty-area rules, advantage (0.8 s hold, 4 s window), dives, offside involvement; the offside line evaluation still open|
|`pes-re-duel-sim.md`|the dive check `FUN_1405b83b0`: reached through a `jz` wrapper from the referee tick, runs only while the ball player is in DIVE (0xf), once per dive; its slot-0 draw from a fresh default-seeded Random is always 98, so the roll never decides - real contact (slide/tackle/keeper 0x38/0x3d, contact window, 1.25 m) is a level-1 foul for the diver, anything else books the diver and gives an indirect free kick; separate from the Judge foul points; GF has no outfield dive|
|`pes-re-offside.md`|offside detection: writer `FUN_140a7b7a0` per ball event, line = min(halfway, second-rearmost defender's rear surface, ball), body-point margins, 0.15/0.30 m tolerance, categories 0-3 (3 = onside near-level); live-checked 2,498/0|
|`pes-re-offside-port.md`|what of the offside detection is ported (the line, the attacker test, the categories, the pending-offside whistle), the ponytail approximations, and the untuned AI discipline|
|`pes-re-defensive-shape.md`|open-play defensive line height (ball-depth ramp, sliders, ≥10 m behind ball), team length, width/lateral shift, the action-state priority gate, MATCH_UP/cover pressing, marking order and hand-off; live-checked; corrects team-plans §2.1|
|`pes-re-defence-leftovers.md`|defensive cover follow-ups (lane start, feasibility, second presser, cover pick, the Delay/Press gates, fb6f0 rotation) and offside follow-ups (event ring, the kind-0 all-clear, gate fields, body points); static only, live items open|
|`pes-re-defender-decisions.md`|the engaging defender: a 17-entry first-true reaction table each frame (receive, tackle, slide, keep, dodges, block, ...), standing-tackle and slide gates with live JSON constants, the block radii, `ai+0x134` = human-controlled challenge (never set for CPU), no RNG|
|`pes-re-contact.md`|player-vs-player contact: the stagger/fall level table, contact.o thresholds, forced falls and vetoes, shielding/jostling, the side-by-side balance gauge, aerial reach and header lift, falls and recovery, Judge flags; live-checked; no RNG|
|`pes-re-contact-2.md`|contact follow-ups: the single jostle-result writer and the never-cleared field, contact.o stock vs mod (three fields differ), header kick category = ball-height band|
|`pes-re-contact-port.md`|what of player contact is ported (`pescontact.*`): the level table, forced falls, all nine vetoes, frontal/committed/push-away modifiers, direct triggers, the JOSTLE state with hold-up and shoulder-through, Balance playback rates, tracked-part displacement; the Judge fed through PES's contact gate; DODGE and tackle sub-kinds wait on PES clips|
|`pes-re-header.md`|headers: `ap+0x2754` is the ball-height band (writer `FUN_1407253d0`), and band 9 is the jump-only band; the AI makes no header decision (the clip selector forces a header from ball geometry); target and power are unchanged apart from the lift and Heading|
|`pes-re-header-contact-live.md`|the header live session: G/MA/MI/ball plumbing, `MI+0x116c` 1 = in-play, the kick request builder `FUN_140833cd0`, `req+0x5b` = reachability, the fast-accept gate (reachability AND header clip AND band 9), `req+0x5c` dead (initialiser-zeroed, never written); window NOT usable (~5% state-1 duty, 0 rows in 4 runs), the AI target/power hand-off and band-9 acceptance open with the trigger-mode sampler banked|
|`pes-re-kick-accuracy.md`|kick execution error: stat per kick type plus skill bonuses, the predictor and scatter constants, the pass assist; every draw is slot 2 of the per-player Random `ap+0x32d0`; weak foot = flat −5; requested-vs-actual comparison open|
|`pes-re-kick-scatter.md`|the physics scatter `FUN_1408398d5`: three {scale, sign, amp} records (yaw/pitch 22.5 deg x (1-q), speed 0.3 x (1-q), amp (1-q)^2 x 4.25 + 0.75) from `FUN_14083cc30`; `Normal` is a clipped Box-Muller giving a 0..1 spread, not an angle; per draw: elevation, launch speed, heading (with up to 6 redraws); no spin; `errScale` is the predictor's only, not this path's; the four inputs of each q and `FUN_1408414a0`'s branch conditions open; follow-up: each q is a soft-min of four inputs (launch-power penalty, the kick-type quality via A = 0.75a + 0.25 on the amps only, situation product Q by contact class, running speed and Physical Contact pressure) - pressure enters through Physical Contact, not Balance|
|`pes-re-kick-accuracy-port.md`|what of the kick execution error is ported (`peskickerror.*`): stat per kick type, the scatter records and soft-min q, Normal draw from the per-player generator; Q, F1/F2 and `FUN_1408414a0` pinned neutral (GF clips carry no PES kick kinds); throw-ins excluded|
|`pes-re-throwin-choreography.md`|throw-ins: the seamless pickup path vs the out-of-play scene path, restart point, taker rules, the throw action phase machines and release frames, the launch band, AI timing, team positioning; no foul throws; throwin.o table corrected|
|`pes-re-setpiece-choreography.md`|kick-off, corner, free kick and wall, goal kick, penalty: the post-wipe step program, the 20-tick teleport placement, kicker selection, positions (positionCK tables, wall at 9.15 m), holds and kick waits, validated on ten live restarts|
|`pes-re-setpiece-zones.md`|set-piece defence: corner and free-kick zone spots (live-exact), the man-marking pass-5 gate and pairing, all 10 positionCK variants with the N rule, the three candidate 0x31 kick-request sites|
|`pes-re-setpiece-zones-live.md`|live corners and free kicks across 3 matches (193 rows): per-corner V roll (V=2 long, V=1 short, exact at fixed N=6), the short/long pairing itself killed (CK-C ships V=1 with the long cross), the -46.06 unpaired fallback-mark stack, the FK gate (|z| <= 20.12, dir.R.x in [0,36.14]) and FK-G at d=29.06 still on the ordinary 20.55 m line, so the ~12 m guard is not a pure function of d; template writer, 0x31 attribution and the FK line still open|
|`pes-re-setpiece-kicker.md`|set-piece taker selection (corners/penalties = sheet taker, FK waterfall), preparation, run-up/strike timing, aim rules (corners 26.8 m/s, FK/goal kick 26.1–26.2 m/s, penalties 1 of 7 fixed points), wall 9.15 m and run timing|
|`pes-re-setpiece-port.md`|what of the set-piece team AI is ported (taker wait, taker waterfall, jog-to-marks, box push, post men, wall placement), the ponytail approximations, and the unwired wall jump|
|`pes-re-foul-choreography.md`|fouls, cards and offside on screen: scene choice (FUN_14047f170), table_foul.bin fully decoded, the stoppage step list and holds, per-player reaction roles, referee gestures, the linesman flag; slot-1 remap and offside detection open|
|`pes-re-cutscenes.md`|demo tables, the `.fdc` cut format, selection, replays|
|`pes-re-cutscene-slots.md`|cutscene actor slot -> player: the slot-list build `FUN_141f5f330` per category, entry bytes `[actor|kind|side|slot|flag]`, kind 2 = match player P(side,slot), 9 = preset/official, 10-16 special records, else side-swap remap; per-category producers (change, foul, goal, pk, timeup, end) with their (side,slot) pairs; the default-side writer open|
|`pes-re-scene-choice.md`|which cutscene plays: the entrance is a fixed config list (no choice; stadium/competition/weather only pick cut variants); the goal scene is a filtered pool and one Range0 draw on the goal generator (seeded with the match's kick-off triple), golazo a 50% gate; foul/sub/end have no live roll; fixdemo statics seeded from the clock; ENT list writer open|
|`pes-re-camera-live.md`|the live in-play camera: Dynamic Wide mode 5, slider values and offsets, camera distance (52.7 m) and vFOV (23.68°) for a known target, the PosY model check|
|`pes-re-camera-settings.md`|the camera settings record via `Copy<CameraSettings>` (copy blob `0x140450ff0`): zoom/height pair at +0x04, mode u32 at +0x0C, serialized mirror; mode map 1-6 in-play, 7-9 vertical; the mode writer one hop short (`[matchstruct+0x44]`)|
|`pes-re-stamina-drain.md`|kick-off tank from the Stamina ability (x1.1/1.2/1.3/1.4/95+5, 400/second + kind/condition terms), the per-tick drain (speed classes, +19/-5/-9), the breath reserve (+5/-3, capped at max(pct,20)*1920/100), the half-time +9000/+3600/+7200, the A5 fatigue effect (0.8+0.2*clamp) and the injury roll; live-verified 97.4% of single-tick steps|
|`pes-re-dash-reserve.md`|the dash reserve (breath) tank: one writer (+5/+3 refill, −3/−4 while dashing above 22 km/h), refills through every dead ball; caps top speed by up to 20 Speed points below 20 %; lists GF's ungated refill and its dead BreathPenalty|
|`pes-re-stamina-live.md`|live match structures (match context, roster and player blocks) and the stamina field search, partial; superseded by pes-re-stamina-drain for the drain/recovery formulas|
|`pes-re-injury-fatigue.md`|stamina tiers, the fatigue and injury clip families, the injury state machine and the constant archive layout that carries the values|
|`pes-re-setpiece-referee.md`|set-piece state machines and constants, the placement tables, the officials, and where every value lives|
|`pes-re-matchlogic.md`|player and team records, tuning files, ball parameters, formations, AI map|
|`pes-re-rng.md`|PES's RNG: math::Random/GameRandom layout, the 31-bit LCG and 15-bit slot-3 LCG literals, range/normal draws, the kick-off seed commit in the MatchControl ctor (clock-derived t, t+1, t+2), sub-generators, a consumer census, live step verification, the seed-pinning procedure|
|`pes-re-rng-slots.md`|which PES system draws from which random-number slot and generator: full call-site census, seeders, and the PES slot for each of GF's 37 draw sites|
|`pes-re-kick-launch.md`|kick launch: the launch writer FUN_1407324b0, ability/foot/ball-height terms, the logistic error scale and exponential scatter, the per-action spin tilt, the per-player LCG; partial (per-family ability index, live validation open)|
|`pes-re-ball-contact-2.md`|the bounce: a min() between a speed-derived impulse and vy gives reflection or restitution, which reproduces the live 0.37-2.26 band; the mode-byte writer (dry normal pitch = mode 2); corrections to part 1|
|`pes-re-ball-collision.md`|ball vs posts, crossbar, boards, stands, flags and player bodies: `FUN_14086d6f0` via PostCollisionListener; restitution/retention per object and bone, recent-hit softening, speed caps 70/50 km/h, analytic goal-frame cylinders, and the GoalnetFilm net model; nine live contacts reproduced|
|`pes-re-ball-contact.md`|ball ground contact, the tangential and spin response, and rolling, every constant mapped to a named ball.o field|
|`pes-re-ball-constants.md`|where PES ball physics constants live: ball.o in constant_match.bin, the named field map, and the runtime-written spin scale|
|`pes-re-officials-ball.md`|referees and linesmen, ball selection, the ball's flight model|
|`pes-re-leftovers.md`|small closures: the match-ball query (**not** dead - it has a caller, the Match General Settings menu, per `pes-re-dead-code.md` §5.1), late-card window = 16 game-minutes, `MI+0x158c == 1` = foul without a card, `injury.o` fields (`constant::Injury`, id 0x0b; reader open), officials' ids are setup reads (pick rule closed by pes-re-officials-pick)|
|`pes-re-leftovers-2.md`|follow-ups: injury.o's full field table (4 severities, 6 symptoms), why the getter xref cannot find its reader, injury vftable/factory; officials' pick-rule search narrowed (generic holder pattern), closed by pes-re-officials-pick|
|`pes-re-officials-pick.md`|the officials pick: a uniform `std::shuffle` of a pool chosen by mode and competition (25 ids in exhibition, 35 in competition, a region subset with one), first 5 ids taken, drawn from a session-wide clock-seeded xorshift128 — not the match RNG; the full writer chain from the picker to the ENV record|
|`pes-re-uniform-params.md`|uniform shader parameters the engine writes at run time: Repetition and pattern, number/name scales|
|`pes-re-cloth-nets.md`|shirt/pants cloth (`.fclo`, cloth skeletons), goal nets, corner flags, other physics props|
|`pes-re-fox2-stadium.md`|stadium assembly: Stadium.bin, the scene/pitch/goal/net/corner-flag placements, goal and corner-flag transforms|
|`pes-re-fox2-format.md`|the binary fox2 (FoxDataFile) layout: header, string table, entity and property blocks, containers, value types|
|`pes-re-goalkeeper.md`|goalkeeper body: positioning (live median 0.005 m defending, 0.000 m in possession), rush race and limits, one-on-one zone, pre-save set, all three deflect arms observed (0x44 catch, 0x46 deflect, 0x4a punch), BLOCK_LATE, pickup and throw-carry chains, style and penalty phases; the deflect point is ported; the release kind byte, the MA+0x251fa writer, the 0x81a1 enum and the Block-Late solvers open|
|`pes-re-keeper-port.md`|the keeper's deflect point (2 m at the requested angle inside 120 degrees of his heading, else 1 m at heading -/+60), the after-catch carry and step, the throw/kick request switch; ported in 703ad29|
|`pes-re-keeper-port-2.md`|second keeper port: the four distribution errors fixed, PES positioning and rush wired, the come-out bias deleted; one RNG draw (the positioning shuffle, per-keeper generator)|
|`pes-re-carrier-ai.md`|the ball carrier: plan choice, first-surviving-unit adoption with vetoes (no score competition), the 5-frame think cadence, difficulty reaction delays, shoot zone and shot choice, pass receivers and the openness formula (live-validated), pass choosers, pressure and clearance, the 72-direction dribble search, the skill/style bit order|
|`pes-re-carrier-port.md`|what of the carrier AI is ported and how: think cadence, shoot zone, openness ladder, PassForward tiers, plan roll; constants table with VAs; the not-ported list; open shoot-zone sample mismatch|
|`pes-re-carrier-units.md`|the rest of the carrier brain: the 72x15 dribble search with its threat term, shield/keep/turn, OneTwo/SideChange/GoalFront/PassTarget, RespondRequest, the full adopter walk, delay tables; live adoption mix; settles the shoot-zone mismatch|
|`pes-re-team-plans.md`|team AI: the team-brain class inventory, formation slot to target position, advanced-instruction enums, off-ball run selector gates, the press trigger/target/count, ZoneDefenceManager marking, pass/shoot/dribble scoring skeletons|
|`pes-re-tactics.md`|team tactics → behaviour: the team-sheet layout, slider formulas, toggles gating the team plans, the plan selector, the 17 advanced instructions and their timing, attack level/CPU strategy; live-checked|
|`pes-re-team-ai-leftovers.md`|team-AI follow-ups: the ReverseSideCounter body; PossessionMfJoinDefenceLine is dead; all 48 subConcepts are enabled; the defender aggression team terms; the six designated players; substitution settings provenance|
|`pes-re-constant-team.md`|`constant_team.bin` fully decoded: the 16 objects (registry 183-198), all 720 fields named and typed from each class's JSON reader and release loader (sorted keys, 4 bytes each, packed bool runs, offset lists), stock vs the installed mod (445 fields differ; the mod is a retuned Righetti 0.6.4: pressRate 0.5->5, slowDown* 1->0, transitionSec 5->2, all 21 off concepts on, pairAnime timings 0); the `T+0xa53c` setter `FUN_140535650` (losing or level-late with no active instruction, attack level >= 3 late or the ball held past the middle line 16/32/60 s, 5 m hysteresis, every 4th frame); code readers of five objects open|
|`pes-re-match-management.md`|how the CPU manages a match: the strategy decider and its inputs, strategy→attack level outside watch mode, substitution rules, reactive tactical changes; no late-game scripts|
|`pes-re-stamina-subs.md`|stamina substitutions `FUN_140523b60` port-ready: mode from `FUN_140520de0` (0 = none), cap byte `*(G+0x2c0)+0x18` with a per-mode minute schedule in the second half, role classes 0-9 (GK/CB never subbed), goal/assist protection, per-class thresholds 20/30/40 or 25/40/45 and base scores 1000/2000/0, ascending sort; the replacement picker and comparator (main/secondary position tiers, can-play, fit, super-sub bit 26); corrects match-management 2.3; the minute byte and form stat id INF|
|`pes-re-transitions.md`|possession and kick-off timers, counter-press/fore-check/regroup concepts with live timings, the concept id map, first-touch sample; corrects team-plans §4.1 and defensive-shape's timer trigger|
|`pes-re-team-concepts-port.md`|port of the team sheet (sliders, 7 toggles, ctor defaults), the concept selector with the VFS subConcept enables, TransitionChase and SideCounter; corrects transitions §2.1 (the trap point is 8 m from the chaser, not the carrier) and §3.1 (supporter sides); GF philosophy presets, zone pressure and counter roll deleted|
|`pes-re-receiving.md`|receiving the ball: the receive reaction, PASS_GET assignment and approach routes, first-touch kinds, interception touches; live 54 Hz capture; corrects defender-decisions entry 0|
|`pes-re-counter-trigger.md`|the counter-attack enable field T+0xa508 (writer, gate, values) and the mark-spot blend arithmetic|
|`pes-re-off-ball.md`|off-ball attacking movement: the run manager and its 11 selectors, caps per attack level, run timing, coordination, the 32-layer support grid and support cost; corrects attack-decisions §3|
|`pes-re-run-targets.md`|Chance run targets and the common stop test: 17 lanes across the pitch at z = F1 - 2 - 2k, X = the furthest of the ball and two opponents' line, lane masks and scores (base byte table `MA+0x25378`, writer open), the 20-step stop cascade with constants; corrects the spaceRun.o key map (PES19 order, 8-byte header: chanceScore 40 / 69 - the earlier 102/88 was real blob values under wrong names, per `pes-re-attack-leftovers.md` §1.5) and F[k] at +4k; ~65 leaf predicates still undecoded|
|`pes-re-attack-leftovers.md`|attack follow-ups live sections: Chance dispatch and scorers, the spaceRun.o key map, stop-test tail, support flags, carrier lane and receiver bodies, the corrected dribble ramp (30-spd)/15; O8 (the support-row capture) open, 15,771 atomic rounds with 0 wins, with the corrected S=P+0x2d4 hypothesis (`0x1405eb980` passes P itself as the shape argument) and a debugger-breakpoint fallback|
|`pes-re-receiving-port.md`|what of receiving is ported (`pesreceive.*`): reaction entry 3 and trap lockouts, `PASS_GET` with PES's own-target arbitration and the 10 m ball-distance gate, the first-touch chooser, the receiver's BallFollow receiving point, PRESS/DELAY plain control (team pressure man stands in), and the challenge-target rule (FUN_140a50e00) that fixed the pass-accuracy regression; open: the other six approach routes, route speed, the anime-layer trap push, the 22-player conflict loop|
|`pes-re-off-ball-port.md`|what of off-ball attacking movement is ported (`pesoffball.*`): the run manager, Chance runs with PES's stop cascade (about 40 checks), the slot mask and team-may-run gates, the designated supporter; open: the special lanes, the lane-commit checks, four of the six run types (Overlap, Counter, SecondLine, Diagonal, PostPlay), the 32-layer support grid; Chance starts still 5-6x PES (~93/team/20 min)|
|`pes-re-press-layer-port.md`|the press layer (`pespress.*`): the presser picker (role classes, speed and angle bands), pass-1 buckets, pass-2 driver, the excluded-man rule, the stamp ring, the action ring, the candidate fill; open: the writer FUN_140536fc0 (one input, the FUN_14044e0d0 hook, unresolved), the 2b wiring, event type 7 to GF touches|
|`pes-re-animtable-port.md`|the PES clip-selection tables read from the install (`pes::Animetable` rows x columns with motion number and mirror, `pes::Mbinfo` per-motion timing, hit and cancel records); needed by the DODGE and tackle-sub-kind ports; general enough for the locomotion port to reuse|
|`pes-re-carrier-units-port.md`|the throw-in taker stall (`1ace4b5`): during the restart hold the ball is bumped off the throw-in spot, past the line every touch anim is refused as out of bounds, the taker never throws, the referee never ends the restart; the referee returns the prepared, unstarted throw-in ball to its spot past 0.05 m (`SimMatch.AThrowInTakerTakesTheThrow` fails with the hold off, passes with it)|
|`pes-re-dodge-port.md`|what of the dodge is ported (`pesdodge.*`): the four selectors (ball, net, post, human) over `Animetable` + `Mbinfo`, reaction entries 4-7 between receive and block, the Dodge action's clips with `SetPesChallenge(kDodge)`; the motion-name table VA corrected to 0x14351e590, stage A's 8 clips (not 67); measured firing, the smoke shift (59520 -> 12662 forced, 71-29 -> 47-53 possession), and the named gaps|
|`pes-re-chance-lanes.md`|the Chance-run stop gates beyond pass 2: the time-to-reach `FUN_140654e60` + `FUN_140655660` (two angle arguments recovered from raw), the crowded-mate and pass-to-me stops, the mask's rank>1 angle sub-test, the undecoded remainder and the honest no-reduction reading (~600 runs against PES's live ~93)|
|`pes-re-post-dodge.md`|the post dodge selector `FUN_1407368d0` decoded and ported, the goal-sign and net-gate fixes that made it fire (post 0 -> 717), and the unresolved `>135` band column (17 vs the install's 16)|
|`pes-re-dodge-gates.md`|the dodge eligibility gates (`FUN_140a07bf0`, `FUN_140a07e40`, `FUN_140a0b350`) ported, their stand-ins, the GF pose veto on PES clips measured on and off, and why it stays on|
|`pes-re-livepoke.md`|Live reads: press hook `0x1425301f0` = ucrtbase `sqrtf` (flag A = distance < 4 m); Chance stop-leaf record fields; PES Chance runs last median 1.58 s (GF 8-9 s)|
|`pes-re-pose-veto.md`|5-seed veto on/off table; the veto stays until the human gate's roster-slot tail (`FUN_140a0b350` / `FUN_140a6ce10`, `team+0x6a4`) is ported|
|`pes-re-net-dodge.md`|Net dodge gate `FUN_140a07e40` ported: dodger-only inputs (no ball, no 12 m window); rate set by the 1 m goal-line edge test. Supersedes the net bullets of `pes-re-dodge-gates.md`|
|`pes-re-foundation.md`|**Foundation inventory**: 34 PES runtime-state structures (219 fields: 12 faithful / 94 stand-in / 111 absent), 80 gates mapped to the structures they need, dependency graph, and the eight-wave build order. Start here before porting any gate.|
|`pes-re-ball-track.md`|Wave 1 MB-F: PES 250-record ball track as the AI prediction; the floor-read defect (52% zero intercept movement) and its fix; tick mismatch (GF 10 ms vs PES 1/54 s) is the open decision|
|`pes-re-movestep-wave1.md`|Wave 1 A-MOV: MOVESTEP contract v0.11, record/playback writers, run-rate guard, speed class, yaw lock after nine states, L4 writer open; D1 slot resolved 17-10-26 (`vt+0x20` = `FUN_1406ea110`), D1 equality with GF speed open|
|`pes-re-tick-order.md`|Wave 1 MB-H: H1 setup rating; team-tick caller chain (FUN_140a8eff0 via the MatchSub listener); the 10-step record switch|
|`pes-re-live-teamplayer.md`|Live: 17 team/player rows; instruction table MA-relative; T+0x6a4 countdown; 48 possession flips graded, 41/41 exact-state agree|
|`pes-re-live-matchball.md`|Live: MI/event-ring/FieldInfo rows; the event store chain; F10-F12 values|
|`pes-re-possession-latch.md`|PoseVeto: the human dodge gate, the T+0x255 possession latch FUN_1404fe9c0 (restart rule R + cascade), event kinds, reaction countdown|
|`pes-re-fieldinfo-consolidation.md`|Wave 1 MB-G: the 18 FieldInfo values in one table at PES bits; duplicates removed; F10-F12 reader search; `G+0x2aa8` is the FieldInfoRef cache slot|
|`pes-re-attack-decisions.md`|Attacking decisions: offside detection, open carrier units, off-ball run scorers|
|`pes-re-ball-collision-netbody.md`|Net and player-body collisions: portable rebuild|
|`pes-re-defender-decisions-port.md`|Port: engaging-defender decisions|
|`pes-re-defensive-shape-port.md`|Port: open-play defensive line, team length/width and marking|
|`pes-re-run-targets-b1.md`|Chance-run lane commit leaves, batch b1|
|`pes-re-run-targets-b2.md`|Chance-run lane commit leaves, batch b2|
|`pes-re-run-targets-b3.md`|Chance-run lane commit leaves, batch b3|
|`pes-re-run-targets-b4.md`|Chance-run lane commit leaves, batch b4|
|`pes-re-run-targets-h1.md`|Chance-run shared helpers, batch h1|
|`pes-re-run-targets-s1.md`|Chance-run stop-test leaves, batch s1|
|`pes-re-run-targets-s2.md`|Chance-run stop-test leaves, batch s2|
|`pes-re-run-targets-s3.md`|Chance-run stop-test leaves, batch s3|
|`pes-re-gaps-anime.md`|Gaps: MOVESTEP group A helpers; motion record lists by kind (RotData/FootType/PivothitInfo/SubHitData), the loader chain; D1 vtable; the AnimePlayer owner (PM vtable 0x1426329a8); the P+0x428 adjuster per-tick chain; the bit-30 writer|
|`pes-re-gaps-match.md`|Gaps: instruction-table writer FUN_140a426f0; ball track rebuilt per tick (head FUN_14080fa80); the support pipeline (11 x 12-byte entries); F10-F12 have no reader; hub dispatch and tick order|
|`pes-re-com-styles.md`|**COM playing styles and the image/intention system, verified**: storage offsets, image scorer probabilities (per-evaluator re-seeded dice), the dispatcher `[9,8,2,3,1,4,5,6,7]`, the Intention ratchet; 4 corrections to the source write-up and 22 GF port mismatches. External source cited|
|`pes-re-shootout-stale-object.md`|The shootout segfault: a stale object with no ninja dependency record, two in-memory `Match` layouts, proof by address arithmetic; the fix and the build-hygiene rule|
|`pes-re-penalty-keeper.md`|PES penalty keeper: the gate `FUN_1409d5860`, the state machine `FUN_1409e5600`, the dive target (first ball record within 3.5 m, goal-axis bearing window) and its install constants, VA-cited; the GF port and why the shootout test stays red|
|`pes-re-stadium-cloth-vfx.md`|B6/B7 decode: team→stadium (Team.bin +0x3C is a Stadium.bin id, reader `FUN_141eeeea0`, fallback 29); cloth vs nocloth part objects (nocloth drawn; corrects cloth-nets §3); vfxdb header (reader not found)|
|`pes-re-gaps-a2.md`|A2/A6 gaps: MA build is inside the team tick (`FUN_140535c60` from `FUN_1404f6c30`), team-tick delivery chain, referee only via record handlers; HUD env source refuted; E5 step = frame delta; C8 = byte `MI+0x11b4`, keep `== 0`|
|`pes-re-taker-map.md`|`FUN_1404a8ad0` is the free-kick kicker chooser (MODE 5): designated table `T+0xa244`, the six-step waterfall, constants from the exe; corrects the 28 m and own-half notes; §9 = what `Wave1TSheet` still needs|
|`pes-re-stadium-cloth-vfx-opens.md`|Follow-up on the B6/B7 opens: match-side stadium resolver `FUN_1414f5120` (set at `owner+0x28`, else 29); cloth gate in `FUN_141a89120`; vfxdb loader `FUN_1415d3990` → parser `FUN_14161c8b0`|
|`pes-re-unbound-texture.md`|An unbound `Mask_Tex_SRGB` resolves to registry key K, else to `DAT_1437cb0e0`, a 4×4 opaque black texture (`FUN_141976ed0`), so the pitch `ao` = 0 is faithful. GB2.y = `(1 − sat(mask.b))·…`; the port drops `v7.zw` from specular and roughness (asm 166)|
|`pes-re-pitch-b4-corrections.md`|B4Grass: the weather-only rain/snow stadium members (the grey-pitch cause); a mask/alpha scan over 32 stadia; the pitch PS lanes; corrections to `pes-re-pitch-grass.md`|
|`pes-re-ball-track-live.md`|A1 live check: GF `BuildTrack` vs PES record 54 over 253 live samples, median 0.71 m (target 0.05 m); flight exact. T1 same-tick landing (verified, 0.159 m to 0.00000 m), T2 lift after a landing (unidentified), T3 live contact mode 1 vs GF mode 2, T4 spin blend; the track record starts at `B+0xB0`, not `+0xBC`|
|`pes-re-touchline-restart-stall.md`|A2Stall: the seed-7 freeze was a touchline free kick spotted at y = 34.2 past GF's 34.17 m touch reach (`humanoid.cpp:3156`); `OffsideRule::RestartPosition` now folds across the touchline too; the post-goal kickoff and latch theories are dead|
|`pes-re-restart-watch.md`|StallCap: `RestartWatch` stoppage-to-restart bound from install values, the sim-tick cap, `[restarts]`/`[idle]` metrics replacing the `[stalemate] forced` tick count; why a per-episode bound cannot see held A2's +2.5 s out-of-play holds|
|`pes-re-hitdata-index.md`|HitIndex: PES files HitData at `(w0&0x1fff)+0x11` (`FUN_1406ad300`, `ADD AX,0x11` at `0x1406ad438`); 2925/2925 install records confirm; GF's `mbinfo.cpp` was off by 0x11|
|`pes-re-ball-contact-port.md`|BallContact: T1 same-tick landing and T3 live contact mode 1 ported in `BallTick`; 1 s median 0.711 → 0.386 m recorded, 0.801 → 0.472 m live; T1 alone 0.349 m; T2/T4 open|
|`pes-re-exposure-same-shot.md`|ExposureShot: GF vs live PES at stadium 29, broadcast; GF pitch 1.3–2.0× brighter, white lines 0.69×, so not a global gain; B5's shirt clip does not transfer to the lit scene; camera and kits not matched|
|`pes-re-style-effects.md`|**What each of the 21 player playing styles changes in a match**: per-style table (sites, system, effect, constants by VA), quantitative before/after (Goal Poacher run threshold 30→15 and goal radius 11→16 m, Target Man +30, Box-to-Box/Hole +5, Extra Frontman +0.2, full-back blend, set-piece clamps, keeper range/angle/distribution), the dt18 selector table (`_BoxToBox` record 0xBC, `_dummyRunner` 0xBD), and the GF gap list. Constants checked by Main against live PES|
|`pes-re-style-census.md`|Every reader of the style word: 228 call sites of the gate `FUN_1408CB0B0` through `FUN_140A64030`/`FUN_140A63FE0`/`FUN_14063E890`, the bit at each site from raw disassembly, per-bit counts, and which earlier claims stand|
|`pes-re-playing-styles.md`|Playing styles: the bit list and engine names, storage (EDIT 0x3b → engine bit, `block+0x48`), compat masks `0x142613D20`, the dead per-style tables, dt18 keys, and the live write session. Its §2/§3/§10 reader claims are RETRACTED; use the two docs above|
|`pes-decode-TouchReach.md`|TouchReach: PES decode of three GF stand-ins (read-only)|
|`pes-re-A4Carrier.md`|A4Carrier: PES 2021 per-player brain (MainEval `FUN_142110D30`), its|
|`pes-re-A5Judge.md`|A5 PES referee decode: judge → whistle writer → sequencer → restart|
|`pes-re-A6Period.md`|A6Period: PES period record and time-up gate (RE + port report)|
|`pes-re-A6Writers.md`|A6Writers: PES period-clock writers, time-up post-gate, breaks, matc|
|`pes-re-Dribble-inventory.md`|Carrier and dribble fields: writers, readers, GF status (decode only|
|`pes-re-Dribble.md`|Dribble status (current): branch `refs/heads/Dribble` = `1a3ea8e`|
|`pes-re-OffBallSelectors-inventory.md`|OffBallSelectors: PES runtime fields the AI reads (decode-only, feed|
|`pes-re-OffBallSelectors-postplay-xmm0.md`|PostPlay lane 0 (FUN_14061fa40): what xmm0 holds on each exit (decis|
|`pes-re-OffBallSelectors.md`|OffBallSelectors: state (2026-10-15, end of pass)|
|`pes-re-a3helpers.md`|A3 MOVESTEP blockers: decode (A3Helpers)|
|`pes-re-foundation-areas.md`|Foundation inventory: areas round 1 did not cover (Areas)|
|`pes-re-foundation-matchball.md`|Foundation inventory: match-, analysis- and ball-owned PES state (Ma|
|`pes-re-foundation-teamplayer.md`|Foundation inventory: team-owned and player-owned PES runtime state|
|`pes-re-foundation-verify.md`|Foundation verification (round-1 tables, independent re-check)|
|`pes-re-foundation-widen.md`|Foundation inventory — widen pass (everything EXCEPT animation/anime|
|`pes-re-glove-hands.md`|PES glove-replaces-hands rule (GloveRule)|
|`pes-re-glove-keeper-2.md`|Keeper glove g1398: hollow render, round 3 (GloveRule)|
|`pes-re-glove-keeper.md`|Keeper gloves and the whole-body glove (GloveRule, round 2)|
|`pes-re-kick-align.md`|PES 2021 — open-play kick horizontal alignment (KickAlignRE)|
|`pes-re-kick.md`|PES kick and touch writers, spin pump, mode index, goal-frame accumu|
|`pes-re-tackle-kind.md`|PES tackle sub-kind (ContactPort stage C)|
|`pes-re-teamctx.md`|PES team-AI context read by AccelerationRamp (FUN_1406cbf70) and the|
|`pes-re-teamtick.md`|PES team tick, MA nearest lists, and the team/player interface (A4Te|
|`pes-re-wave1-MovestepStep.md`|Wave 1 A-MOV group C: the stepping states of the MOVESTEP machine|
|`pes-re-wave1-Wave1TSheet.md`|PES 2021 Wave 1: T-SHEET (team tactics sheet, T+0x8e08)|
|`pes-re-teamorder.md`|TeamOrder: logic frame order (bind, team tick `0x14041ecf0`, side ticks `0x1404f6c30`, hub, clear); the 22 player ticks live in an array at `M+0x10` (`M=*(0x1436f3f78)`) and interleave with side 0's tick, most running after it (live, ~10 µs); "players before H" refuted, caller still open; MI/roster/ball/E bind together; bit 27 read by the ramp at `0x1406cc3bd` (teamctx §4.2 confirmed) and by the veto `FUN_140676010`|
|`pes-re-extra-frontman.md`|Extra Frontman (bit 15) in set pieces: required gate for CPU aerial candidates (`F63C0`, `F7D00`), the corner candidate store (`14051B8B0`), and `P+0x3EF8` the designated aerial attacker chosen by jump reach (+0.2 m for the style, 0.05 tie window, `+0x25` tie-break, fallback filter). `+0x4C` bit 24 (card 56, not a COM card) clears the bit. dt18 key 0x28 dead. Constants checked by Main against live PES|
|`pes-re-style-reverter.md`|What rewrites a player's style word live: a ~59 Hz restore of the roster word and its copy (copy first, ~30 µs earlier), clearing only the written bit; the only record-to-block writer is `FUN_141FE9F40` (load chain listed); the per-frame reverter and its source are still open, with the eliminated candidates|
|`pes-carrier-A4Cont.md`|A4Cont: why the remaining carrier units are blocked: the lane core `0x14063CD90` (five undecoded helpers) gates BackSpace, GoalFront, Safety and the one-touch path; per-unit VA table with closure sizes and a six-step decode-first order|
|`pes-re-lane-structs.md`|LaneStruct: lane-struct channel map at the five unit callers (BackSpace/GoalFront/Safety via bool-only wrapper, DirectHighBall takes the teammate scan, OneTwo via one-touch), 13 direct core call sites with 0 indirect, five `FUN_` names corrected to interior VAs, port-ready wiring note for `LaneFree`/`ScanLane`|
|`pes-re-lane-core.md`|LaneCore: the pass-lane blocker detector `0x14063CD90` (predicted mates in an annulus-and-cone or crossing the lane; degrees, install constants), wrapper `0x14063DE20`/`0x14063DC10`, its five helpers, the one-touch predicate `0x142128C10`, the OneTwo receiver predicate `0x142124950`, and the b6c record A fields; a port spec|
|`pes-re-shootout-verdict-mismatch.md`|Shootout verdict: the recorded outcome is what the ball did (goal plane inside the frame, keeper touch, else miss); why a keeper touch was scored a miss (the 5 s timeout ignored it)|
|`pes-re-whole-body-uniform.md`|Whole-body uniform parts: PES builds uniform parts from kit and appearance only, markings bind only to UniformUnit parts, `pants_out_sub` hidden when the tail is in (`FUN_141a89120` +0xBC)|
|`pes-re-min-players.md`|The minimum-players (abandon) rule: not located statically; stop reason 10 is consumed at `0x14044eb4e` but never produced by an immediate store, and the abandon-mode byte `MI+0x1775` (0x0b–0x0f) has no immediate writer; probably inside the protected region; live trace plan|
|`pes-re-KeeperGlove.md`|Keeper glove g1398: the mesh itself has an enclosed see-through region (13% of its silhouette; 40/40 ray samples hit no triangle), while vanilla g0001 and W's g1578 have none; of 802 4cc glove meshes only g1398 and g0887 exceed 10%; `fox3DDF_Blin` has no no-cull variant, so PES culls like GF; likely hollow in PES too (data-level)|
|`pes-live-EFPin.md`|EFPin: live pin runs for Extra Frontman. Earlier side-0 runs void (wrong object; designated slot is `[brain+0xBB8]+0x3EF8`). Side 1: the CB is the designated aerial attacker 12.5% of recomputes without the style and 16.2% with it (not significant); FC860 runs for both sides; the +0.2 m effect is not separable without logging scores|
|`pes-live-EFPin-scores.md`|Extra Frontman measured live as a score: the aerial-reach base (mode 2, constants 1.935/0.435/2.335/−0.054343) gives the side-1 CB 2.3131, and exactly 2.5131 with bit 15 (+0.2000); the bit can only select a CB (compat row); whether it changes the winner is open (an unmodelled gate leaves the live slot at 255)|
|`pes-re-positioning-phases.md`|Positioning phase machine (external audit): directive `T+0x80A0` + per-slot `+0x80E0` copies; `SecondaryPositioning_11Styles` modes gated by role array C (mode 2 = role 2 + phase 20; mode 40 = Extra Frontman role 8/5 + phase 23); phase 23 = push-up-and-hold-line; hierarchy down to run-option selection; directive + role-array writers open|
|`pes-re-community-qa.md`|community Q&A audit (external auditor posts 2026-10-16 to 10-19 and the channel export 2024-09 to 2026-10): ranked findings against the open work list (role-array writer, keeper/designated slot label, Extra Frontman +0.2 narrowed to CB class, chip-shot and PK scatter term, Man Marking cadence, forward protection at −1/−2, instruction enum sentinel), full claim table, question/answer pairs, and the doc corrections proposed for review|
|`pes-re-role-writer.md`|Role arrays: A = `S+0x3F8C` (doc T+0x4BD4) and B = `S+0x3FB8` (doc T+0x4C00), S = T+0xC48, written as one pair with the same value by `0x140A425A0`/`0x140A42590` (bulk writers `FUN_140A59F90` at phase/setup, `FUN_1404F5F10` every team tick); only `FUN_140A422C0` swaps B; C = `P+0x3FB8` is the set-piece object aliasing S at init; array-B 0 = keeper; class map 0..9; corrections listed|
|`pes-re-carrier-helpers.md`|A4 carrier helpers for BackSpace and Safety: PassThrough validator `0x1421226C0` (lane 1 = len 5.0, cone 30, rad 2), cut-back `0x142125140`, track read `0x140A6CF70`, Safety gate and pickers `0x142126B70`/`0x142128490`; port specs and 15 doc corrections; every float audited against the image (§0)|
|`pes-re-first-touch.md`|Loose-ball pickup: the chaser is press state PRESS 0x34 (writer `FUN_140536FC0`, < 9 m, target `FUN_140639100`, movement `ActionPress` `FUN_140962440`), not MATCH_UP (needs MI+0x1775 in {3,8,9,10}); contact is reaction entry 3 `FUN_1409F8220` issuing TRAP (function 7) within 10 m, designated owner refused; `FUN_1404F4BE0` is a latch predicate; port spec and the GF trigger change (touchFrame from HitData)|
|`pes-re-lane-units2.md`|A4 carrier units OneTwo `0x14212AEB0` and DirectHighBall `0x142129BE0`: gates, one-touch `0x142128C10`, teammate lane branch (live, called at `0x14212A22D`), port specs; 35-row float table (Main re-audited against the image); 18 doc corrections; score/target/angle/curve row writers open|
|`pes-re-image-rows.md`|Carrier image rows: builder `0x142119390` writes image A only (per tick per carrier); curve table writer `0x142138220`, receiver score `IA+0x898` writer `0x142137F90` = (0.2 v_a + v_b + v_c)/2.2 (v_c open), opponent list `IA+0x000`, goal-mouth `IA+0x31C0` tests the carrier 18 frames ahead; team rows `MA+0x228D4..`, ball NearestEntry `MA+0x2470C`; score/list/angle blocks `MA+0x228EC..0x22CBC` and NearestList writers still open|
|`pes-re-press-life.md`|PRESS (0x34) lifecycle: setter `FUN_140A42940` (state + 4-deep history + target block), renewal by `FUN_140536FC0` each tick and reset by `FUN_140536690`, overwrites by MATCH_UP/SAND/GOAL_COVER/second presser/TRAP; ActionPress `FUN_140962440` target and 5 m step, ETA `FUN_1406788B0` = max(1, fps*dist), speed class `FUN_140A75BE0` cuts 0.05/8/13.5/22 km/h; DELAY = failed press-wins angle test; C3 unit = degrees; 55-row float table (Main re-audited)|
|`pes-re-team-layer.md`|A4 team layer port spec: 57-step team tick in PES order (FUN_1404F6C30) with GF status per step; verdicts on the auditor claims (B8A3 reader confirmed in FUN_1405F9EB0; 7.0 refuted; designation chain corrected, +0x8924 refuted; protected forward = LAST eligible in slot order, per-slot bonuses dead); slices A-G for implementers; 42-row float table (Main re-audited)|
|`pes-re-player-ai.md`|ball-carrier think units, off-ball runs, team plans and pressing, keepers, attribute/style inputs, AI tables|
|`pes-re-tactics-clock.md`|the match clock as a u32 percentage of minutes x 1920, the stoppage formula, the period state machine, and how GF differs|
|`pes-re-clock-record.md`|the real match clock: plain f32 seconds at `*(G+0x2c0)+0xa0`; the old percentage clock model is the stamina tank; `match_time` scales stamina; a freeze is a 4-byte float store; GF's `pesclock.*` mapping|

|`pes-re-clock-live.md`|live clock, 4 windows: `E+0xa0`/`E+0x11c` falsified, rate ~9.5 displayed-s/s; the `MI+0x1180` record layout falsified (20 Hz dump + the HUD reader's base walk ends at render-side widget lookups); poke protocol with owner-granted writes: the frame counter `MI+0x1198` is not the display source (both directions), `MI+0x119c` is re-derived every tick; the wall-clock reading of `MI+0x118c/0x1190/0x1194` is withdrawn 17-10-26 (those writers act on the MB-E block, `pes-re-A6Writers.md` §0); perf breakpoints unavailable; the live record and the freeze VA still open, next step: hold `MI+0x118c` in open play and watch the HUD|
|`pes-re-sim-motion-mapping.md`|GF simulation states to PES motion selection: the action layer, kick and trap families, keepers, set pieces, officials|

Paths in the reports are placeholders: `$PES21_ROOT` is your install,
`<scratch>` the analyst's scratch folder (its scripts are not in the repo),
`<repo>` this repository. Refresh from the analysts' report folder with
`tools/dev/archive_re.sh REPORT_DIR`.

## Implementation reports

Agent port and fix reports moved to `../ports/` (index: `../ports/README.md`).

## Render reports (`docs/render/`)

|Report|Covers|
|---|---|
|`../render/pes-pitch-PitchV7.md`|PitchV7: pitch VS fog terms (o7.z, o7.w) into specular and roughness|
|`../render/pes-render-B7Effects.md`|B7 effects (B7Effects): report|
|`../render/pes-render-EffectsBinding.md`|EffectsBinding: PesEffects binding over the effect parser|
|`../render/pes-render-b2lighting.md`|B2Lighting — match light rig (report §0-8)|
|`../render/pes-render-b3post.md`|B3Post: tonemap and post chain (Track B item B3)|
|`../render/pes-render-b5materials.md`|B5 Materials: report, update (branch `B5Materials`, head 8fd78e0)|
|`../render/pes-render-camera.md`|PES broadcast camera (B1, Dynamic Wide): target and framing port|
|`../render/pes-kit-compare-b5.md`|PES kit comparison: BNT (708) and W (760), B5 Materials|
|`../render/pes-exposure-ExposureMatch.md`|ExposureMatch: live PES is stadium 21 (not 29), fine, day; pitch origin mapped and a pose-aligned pair made; GF grass is 3.7× PES (dark turf R ×6.4, G ×2.9, B ×9) while painted logo is 1.4×, so not global exposure: the turf albedo path or a per-channel post stage; speckle, bloom, line specular rejected; mask, LUT, kits, hour open|

## Plans (`docs/plans/`)

|Plan|Covers|
|---|---|
|`../plans/engine-port-plan.md`|Port GameplayFootball to Godot 4 (GDExtension)|
|`../plans/godotfootball-foundation-plan.md`|GodotFootball — foundation|
