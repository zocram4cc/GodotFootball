# PES cutscene roles: what the 3,095 impure functions are, the scene data, and the manager birth

Read-only RE (owner ruling 24-10-26). Labels: CONFIRMED (read in image/live/snapshot bytes,
decompile with verified xrefs, or reproducible tool output), INF (inferred, level stated),
OPEN (what would settle it). No PES bytes quoted; VAs, names and behaviour only.
Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA - 0x140000000).
Ledger `tools/dev/ledger/universe.json`. Scratch `<workdrive>/gf-CutsceneRoles-scratch/`
(per-group row dumps `role_*.json`, catcher `birth_catch2.py`, its log `birth2/birth2.log`).
Decompiles cited from `~/ghidra-projects/scratch/FoulChoreographyRE/dec/`.

Prior docs: `pes-re-cutscenes.md` (formats, selection, replays), `pes-re-cutscene-slots.md`
(manager/slot layout), `pes-re-scene-birth.md` (birth snapshots), `pes-re-restart-step.md`
(restart-step container), `pes-re-A5Judge.md` (Flow chain). This doc adds the role map of the
impure set, the data-vs-code split with file paths, the manager-birth chain + catch result,
the Flow provenance, and the foul/offside mechanism map (§6).

---

## 1. The impure set, reproduced

`subsystem == "cutscene"` rows: 3,846. `pure_rows` (tools/dev/ledger.py:93, greatest fixed
point: walked, not boundary, no indirect site, no vtable ref/call, all callees pure) gives
751 pure here. 3,846 - 751 = **3,095 impure** — the set in the task. Reproduced exactly from
the committed ledger in this session (CONFIRMED).

Method for the role map (all static, from the image + ledger; no bridge needed):

1. **RTTI class per function.** Rebuilt the vtable map from the image with the same
   `rtti_vtables` the crawler used (13,749 vtables); a function's VA equal to a vtable code
   slot ⇒ that slot's RTTI class. 88 distinct FixDemo classes exist (full list in scratch).
2. **Call-graph position.** Each row's `path[0]` is the root function it was crawled under
   (the doc seed or the RTTI vtable slot that opened the walk). The 14 cutscene doc seeds
   (universe.py:177-190) are the event builders and the demo engine; from them I computed the
   full callee subtrees: match-event subtree 6,055 rows, demo-engine subtree 1,006 rows.
3. **Anchor identification.** The ~20 biggest `path[0]` anchors that carried no RTTI class
   were identified by decoding their first ~80 instructions from the image (capstone):
   string refs (`.fdc`, `GenAnimControl`), call patterns (FixDemoCutFile load tasks, table
   column streams, fox anim control), and the vtable stored at each ctor.

## 2. Role map of the 3,095

Counts (partition of 3,095; every row counted exactly once; row dumps in scratch):

| # | Role | n | % | In match-event subtree | In demo-engine subtree | Boundary (engine) rows |
|---|---|---:|---:|---:|---:|---:|
| 1 | Scene selection / sequencing (what plays, when, in what order) | 718 | 23% | 518 | 75 | 30 |
| 2 | Scene data parsing (`.fdc`, `table_*.bin`, cut libraries) | 653 | 21% | 626 | 60 | 10 |
| 3 | Asset loading + entity/actor creation | 563 | 18% | 386 | 1 | 27 |
| 4 | Render / camera / effect / playback plumbing | 193 | 6% | 193 | 114 | 26 |
| 5 | Other (engine infra: cobra/fox property, job, string, list, alloc) | 968 | 31% | 850 | 148 | 363 |
| | **Total** | **3,095** | | | | 456 |

Subtree numbers overlap (a row can be under both); 497 rows are under neither seed subtree
(they hang off the RTTI-vtable roots, mostly demo-engine internals).

### 1. Scene selection / sequencing — 718

Decides **which scene, which variant, and when**. This is the layer that determines what the
player sees, and it is the layer Godot must re-implement faithfully (it is *not* data):

- **Event builders** (one per trigger): foul/card `FUN_140480910`, substitution
  `FUN_140484A10`, goal variant `FUN_140471A80`, goal-celebration selection chain
  `FUN_140478190`, period-end `FUN_140488470` (+ full-time table `FUN_140486B10`),
  end-of-match `FUN_14048AB10` (+ scene-id state machine `FUN_1404A3FA0`). All doc-cited,
  CONFIRMED. Their subtrees hold the per-event scene/variant/level selection, the 31-bit
  LCG celebration pick, the player-signature celebration map lookups, and the retries
  (all in `pes-re-cutscenes.md` §4.3).
- **FixDemo DB + slot machinery**: `FUN_141F500D0` (the fixdemo::DB accessor — the single
  most-called function in the whole impure set, 57 in-edges), `FUN_141F5F330` (slot-list
  build per category), `FUN_141F57840` (iterates mgr+0x78 slot entries → FixDemoHuman),
  `FUN_141F5E930` (FixDemoMatch vtable idx 16 → tail), the factory `FUN_141F5E730` and the
  creation event `FUN_14047C500` (see §4).
- **Manager/mode/entrance/result/replay objects** (RTTI class slots, CONFIRMED by vtable):
  `FixDemoMain` (vt 0x142BA51B8), `FixDemoManager` (0x142BA6800), `FixDemoMatch`
  (0x142BA6BC0), `FixDemoMode` (0x142B9EDE0), `FixDemoResult` (0x142BA7018),
  `FixDemoEntrance` (0x142C701F8), `FixDemoReplay` (0x142C70458) — the update/tick,
  state machine and step execution of the demo itself (55 rows sit directly on these
  classes; the rest of this group is their free-function helpers).
- **Condition evaluator + variant matcher**: `FUN_141F4D2F0` (2,081 insns, the single
  biggest function in the set), `FUN_141F4B1D0` (decision-tree matcher). These *read the
  data* but *compute the choice*; the weights/keys live in `table_<cat>.bin`.

**Visible in game: YES** — this is the "what is shown and when" logic: which celebration,
which card presentation, half-time, full-time ceremony, entrance, penalty routine.
Godot must reproduce the *decisions* (scene id, variant, timing); the data side is §3.

### 2. Scene data parsing — 653

Reads the shipped `.fdc` / `table_*.bin` / `.gani` bytes into memory (all formats fully
decoded in `pes-re-cutscenes.md` §2-5):

- **`table_<cat>.bin` readers**: `FUN_141F40C00` (parse order T1/T2/T3, 994 insns) and the
  column-stream getters `FUN_1423DCA90`/`1423DCBC0`/`1423DCAC0`/`1423DCAE0`/`1423DCB00`/
  `1423DCDF0` (f32/u32/u16/u8/string/end-of-section). `tbl.py` already re-implements the
  parse and consumes 100% of all nine tables (CONFIRMED, prior doc).
- **`.fdc` readers**: `FUN_141F36200` (type switch: Cut/Env/Timing/Effect/Human/Object/
  Camera/Function/HumanControl), `FUN_141F2F6B0` (the big `.fdc` block reader — 126 rows
  hang under it, the largest demo-subtree anchor), `FUN_141F34650` (`.fdc` file open,
  string `".fdc"` CONFIRMED in its body), `FUN_141F33280` (table open), `FUN_141F236F0`
  (record field reader, under goal-celebration selection).
- **FixDemoCutFile load tasks** (RTTI `LoadCutParamTask/…AnimeTask/LoadLightTask/
  LoadObjectTask`, vt 0x142BA5398 et al.): the async load of cut files, embedded `.gani`,
  `.canm` camera streams, lights, objects.

**Visible in game: NO directly** — it is plumbing. But it is *not* skippable: the data is
the content (§3). A Godot reimplementation parses the same `.fdc`/`.bin` files; these 653
rows are the reference parse order, not behaviour to translate.

### 3. Asset loading + entity/actor creation — 563

Turns parsed records into live scene objects:

- **Entity creation**: `FUN_141F5B030` (69 rows; FixDemoHuman/Object creation from
  Human/Object records, calls the actor-registry `FUN_1408D8810`/`1408D8020`),
  `FUN_141F571D0` + `FUN_141F5B9E0` (human control + camera object build),
  `FUN_141F5A920` (list plumbing, `list<T> too long` string), `FUN_141F642E0`
  (Fox entity registration, data 0x142BAB340).
- **Actor/camera/object runtime**: RTTI slots of `FixDemoHuman` (vt 0x142BB1048),
  `FixDemoCamera` (0x142BB0D90), `FixDemoObject` (0x142BB13F0), `FixDemoEmptyCamera`
  (16 rows directly on these classes, CONFIRMED by vtable). These execute the per-frame
  actor motion/camera paths driven by the `.fdc` records (slot-number binding, §3 below).
- **Fox asset load plumbing**: `FUN_141F5CAA0`, `FUN_1408D02D0` (the 1408Dxxxx fox
  asset/entity cluster, 31 rows).

**Visible in game: indirectly** — this is where the demo actors (players, ball, props,
camera) actually get created and move. Godot produces the visible result from the same
records with its own entities; the 563 rows are the reference for *what* is created per
scene (counts, types, slot bindings, times), not for how Godot renders it.

### 4. Render / camera / effect / playback plumbing — 193

- `FUN_141F7ECC0` (69 rows; calls the effect/particle cluster `FUN_141EB6FF0` and fox job
  `FUN_1403FB730`), `FUN_141F7E510` + `FUN_141F7EFD0` (fox effect/anim playback, call the
  141AFxxxx effect system), `FUN_141F7BFE0` (801 insns; stadium camera + crowd effects,
  calls `FUN_14087D830`/`140891A70` — the stadium-camera cluster) and the 141F7Bxxx camera
  path rows.
- Fox job/effect boundary rows (`?`, `cobra`, `fox` boundary tags): 26.

**Visible in game: YES, but Godot produces it differently** — crowd reactions, smoke,
fireworks, camera shakes, light flashes are visible; but per owner's rule for menus
(and now allowed for cutscenes), Godot can render these from the scene's effect records
without translating the fox effect engine.

### 5. Other — 968 (the bulk is engine infra)

- **cobra/fox engine boundary rows**: 363 (property system `?$PropertyInfoT` 291,
  `?$Data` 87, `?$ObjectMemoryOperatorT` 73, `?$List`/`?$HashMap` 51,
  `?$FixDemoParamListUtil` 18, job `Job` 9, …). Under `FUN_14029D510` alone: 194 rows
  (the cobra property/Job hub). These are where the cutscene code calls *into* the engine;
  Godot has no equivalent engine — its GodObject/Resource system is the stand-in.
- **Free functions on engine areas**: 187 cobra (1402xxxxx), 119 fox (141Exxxxxx),
  91 fox (1423xxxxx), 48 core util, 31 (1408xxxxx), plus small clusters (fx 141A, match
  1404, audio 1414, …). These are helpers the cutscene code shares with other subsystems
  (string, list, container, math, alloc).

**Visible in game: NO** — pure plumbing; Godot's own engine produces the effect.

**Implication for the owner's decision.** Of the 3,095: ~1,371 rows (44%) are engine
boundary or engine-area plumbing (group 5's 363 boundary rows + the free cobra/fox/util
rows) whose effect Godot produces by construction; ~1,213 rows (39%) are data parsing +
asset/entity creation (groups 2+3) that Godot re-implements as *parsers + entity builders
over the same files*; the behaviour Godot must **reproduce faithfully** is group 1's 718
rows (selection/sequencing) — but their *content* (which scenes exist, their conditions,
variants, timings, actor scripts) is in the data files, not in the code (§3). The net
"must-translate-to-match-PES" surface is the selection/sequencing logic (718 rows, of
which 75 sit on RTTI classes and the rest are free functions), **not** 3,095.

## 3. Where the scenes are defined (data), and the data-vs-code split

Files in the VFS (paths from `pes-re-cutscenes.md` §1, OBS, `pesfmt_dump ls ''` over the
mounted VFS incl. cpks; `PES21_ROOT`-relative):

| path | ext | count | holds |
|---|---|---:|---|
| `common/demo/fixdemo/<cat>/table_<cat>.bin` | .bin | 9 (cat ∈ ent,foul,goal,pk,change,timeup,end,result,mode) | Scenes, cut library, condition keys, celebration maps — **what plays and when** |
| `common/demo/fixdemo/<cat>/cut_data/*.fdc` | .fdc | 4,925 (ent 1,083 / goal 1,691 / mode 923 / foul 78 / pk 128 / change 192 / timeup 198 / end 526 / result 106) | One file = one layer of a cut: actor motions, camera, crowd, effects, props, timings |
| `common/demo/anime/FoxAnim/FixDemo/Animations/*.gani` | .gani | 5,483 (`dml_*` demo motions) | the motion data the Human records reference |
| `common/demo/anime/FoxAnim/FixDemo/CharacterAssets/` | .ask/.frig | 116/58 | demo skeletons |
| `common/demo/fixdemoobj/**` | .fpk/.ftex/.xml | ~5,700 | tunnel, locker room, banners, cup stands, press/staff models |
| `common/demo/prop/**`, `common/demo/mob/**` | — | 790/382 | trophies/props, crowd |

(The two loose `.fdc` under `content/trophy-server/uefa_supercup/.../fixdemo/end/cut_data/`
are the same format from a cpk, not a new system.)

**Data-vs-code split (CONFIRMED by the decoded tables + format docs):**

- **In the data (the content):** scene existence and order (steps), cut-group → variant
  maps, the per-variant condition keys (up to 10, `condType<<8|value`), variant weight
  rows, every actor/camera/effect record with slot bindings and times (1/60 s ticks),
  player-specific celebration signature maps, mood-dependent full-time variants.
  The nine tables are **fully decoded** (`tbl.py` consumes 100% of every stream on all
  nine — prior doc §2). The `.fdc` record layout is a byte-level dictionary (prior doc §3).
- **In the code (the behaviour):** the trigger → event-builder dispatch; the
  selection/level/weight logic and the 31-bit LCG (group 1); the decision-tree matcher
  and condition-evaluator *semantics* (group 1, the 2,081-instruction evaluator); the
  variant cache; the parse order (group 2); the per-frame execution of the chosen cut
  (groups 3+4); the match-context fields the conditions read (score, side, period, player
  ids, mood) — these come from the live match, not the files.

So: **the code picks; the data prescribes.** Godot can load the shipped `.bin`/`.fdc`/
`.gani` as-is (they are user's-install data, used as test inputs) and must reproduce the
picking logic. The scene *content* needs no translation at all.

## 4. The FixDemo manager birth: creation chain (CONFIRMED) and the catch (OPEN)

### 4.1 The creation chain (decompiles, xref-verified)

The manager is created **by a match-side event builder**, not by the demo engine:

```
event builder (e.g. foul FUN_140480910 / goal / sub / period-end)
  -> FUN_14047C500(owner, param2, param3, param4, param5)        [dec/14047c500.c]
       owner = param_1, type tag [0]==0x80001, [1] in/out flag, [2]=param4, [3]=n-entries
       -> (param4 != 4 path) builds a local entry list, then:
          rax = FUN_141F5E730(local_entries, *param_3)            # factory
          mov [rsi+0x1e0], rax        ; 14047C64D — the OWNING POINTER STORE
       -> (param4 == 4)  FUN_141F5E7E0(*param_1,*param_3) -> [rsi+0x1e0]
       -> (retry, tag 0x80001, [1]==1) FUN_141F620D0(0x80001,*param_3) -> [rsi+0x1e0]
  factory FUN_141F5E730: alloc 0x290 (tag 0x19) via 0x1403FB410 (Fox alloc), calls
  FUN_141F5E2E0(obj, param2, param3, param4) and returns obj
  FUN_141F5E2E0: the FixDemoMatch ctor — calls the shared mgr ctor FUN_141F55110,
  stores vtable &PTR_FUN_142BA6BC0 (FixDemoMatch), sets [0x28c]=0, copies param2,
  clamps param3 to [0,1] into [0x38]            [dec/141f5e2e0.c]
```

- The **owning pointer** is `*(owner+0x1e0)` where `owner` is the `0x80001`-tagged
  event/DB object that is `param_1` of `FUN_14047C500`. This is the pointer to poll
  (null -> non-null) per the task. It is per-event, not a stable global — the owner is
  allocated on the match heap, so a *fresh* birth stores into a *fresh* owner address.
  (The earlier scene-birth docs' "FixDemo::DB" global is a separate singleton:
  `fixdemo::DB` vt 0x14264CF28, accessor `FUN_141F500D0`; it is not the manager.)
- **CREATE/LOAD** (the task's `0x141F5F330` slot-build and `0x141F57840` human-create)
  are **virtual-only** — each has zero direct callers (E8 scan of the whole code
  segment); `0x141F5F330` is FixDemoMatch vtable slot 14 (vt 0x142BA6BC0, CONFIRMED by
  vtable read), so it runs when the manager's own update dispatches slot 14 —
  "about to run" right after construction, not on the creation stack. The fdc loader
  `0x141F36200` is called by the load path.

### 4.2 The catch attempt (this session)

Method (scratch `birth_catch2.py`, two stages): stage 1 scans the running heap
(0x0F000000-0x3D400000, ~775 MB writable, read-only pvr, ~17 s/scan) for a live manager
(vtable needle + 4-slot code-range validation); on hit, a backref scan finds the owning
pointer (qword == manager, owner tag 0x80001 check), then a ~200 ms SIGSTOP captures the
object + main-thread stack. Stage 2 polls that pointer at 2 ms for a birth edge
(null->non-null or value change to a valid manager) and stop-dumps the stack at the edge.
All heap work runs on the **running** process; the pause is only the dump window
(CONT in `finally`).

**Result: 0 live managers in 32 scans (2 runs: 15 + 17, ~9 min total), 0 births caught,
0 pauses (no manager was ever found, so no SIGSTOP was ever issued).** Hit rate 0/32.
Root cause, CONFIRMED by direct probing: the game was **not in a match** for the
whole window — `MI = *(G+0x270)` = `0x1C821D60` but `MI+0x116C (play) = 0`,
`MI+0x1168 (period) = 0`, frame frozen at 16,776,961 (the bot is between matches; the
manager only exists during a match, and is created at the first cutscene trigger).

Validation of the method (so the 0 is a true negative, not a broken needle):
- The same needle + validation found the live `FixDemoMatch` in the prior session's
  SceneBirth snapshot `birth1-214956` at `0x1A7E6D60` (first qword
  `0x142BA6BC0`, slots in code range) — that snapshot was taken **during a match**
  (`play=2, per=10`). CONFIRMED.
- The current live heap genuinely contains no FixDemo vtable instances in the scanned
  range while `play=0`. CONFIRMED (the scans are the read).

The second run (job bg_403) completed: 17 more scans, still 0 managers, so the total is
32 scans / 0 births / 0 pauses. The catcher script stays in scratch, armed for an
in-match window: rerun `sudo -n python3 birth_catch2.py --out <dir>` while the bot is
mid-match and it will record the first live manager, the owning-pointer address, the
object dump, and any birth edge. **If a match is wanted for this, Main should ensure the
bot is mid-match** — without it the live part of §4/§5 degrades to "no births observed".

CREATE/LOAD-on-stack: not observable in this session (no birth); the chain above says
they run right after the store, on the manager's first update tick.

## 5. The Flow object (the restart builders' runner `rcx`): narrowed to a single field

SceneBirth's suggestion was to emulate frame-sync `0x140422030` per referee-vtable
instance and watch for `0x140445E90(Flow, ...)`. The chain is now pinned statically,
so emulation is only needed to get the *live instance address*:

- **`0x140422030` is `match::MatchControl` vtable slot 19** (vt `0x1425984C0`, 20 slots;
  slot 19 = `0x140422030`). CONFIRMED by vtable read from the image. So its `this`
  (`param_1` in the dec) is the live `MatchControl` — call it **O**.
- Frame-sync body, line 108 (dec `140422030.c`, 478 lines):
  `FUN_140445E90(*(param_1 + 0x38), param_2, *(param_1 + 0x220));` — so
  **Flow = `*(O + 0x38)`**, a qword field of MatchControl. CONFIRMED.
  (Line 46 and 172 use the same field with other drivers; line 109
  `FUN_140445E70(*(param_1+0x38))`.)
- The driver `FUN_140445E90` (284 insns) does one indirect call, `call [rax+0x88]` =
  **Flow->vtable slot 17** — Flow is a heap object with a 20+-slot vtable. The runner
  `FUN_14044C880` (261 insns) is its step-dispatch (calls the 14044xxxxx step family).
  CONFIRMED by decode.
- Prior docs agree: restart-step §1 puts the step container at the same `O+0x38`
  (step array `Flow+0x878`, index `Flow+0x8f0`), A5Judge §3.1 the runner/driver pair.
  No contradiction.

**Narrowed to: Flow is the single object pointer at `MatchControl+0x38`** — one field,
set once per match when MatchControl is built. The live instance was not read (game not
in a match this session); reading `*(live_MatchControl+0x38)` in the next in-match
snapshot finishes the item (the SceneBirth catcher's `MI` probe already lands inside
match objects, so one more qword read does it). The referee-vtable emulation is no
longer needed for *provenance*; it would only be needed to watch the Flow *state
machine* execute, which is a different (playback) question.

## 6. Fouls and offsides: who does what, data vs code, what's already reachable

For the owner's decision on stoppage choreography. "Group" = the role-map group of
§2. "In the 3,095" = member of the impure set; "reached" = in the 32-function real-call
replay set (`pes-re-scene-birth.md` §2-3: 32 reached, 17 PROVEN) or exercised live
(frame-level captures / sim test). Most of this section is DOC: `pes-re-foul-choreography.md`
(all § cited), `pes-re-A5Judge.md`, `pes-re-offside.md`, `pes-re-officials-pick.md`,
`pes-re-restart-step.md`, `pes-re-setpiece-referee.md`. New work this session: group
mapping (this doc's `role_*.json`) and the reachability column; the underlying claims
carry their docs' OBS/CONFIRMED evidence.

### 6.1 Per mechanism

| mechanism | function(s) (VA) | class / owner | group | data it reads | decided in code | reached via real calls? |
|---|---|---|---|---|---|---|
| **Foul judgment** (when to whistle, card level) | Judge `Flow+0x908`, update `0x14050b490`; pending `0x14050ee40`; advantage/commit state machine in `FUN_1404a56c0`; whistle writer `0x14050e770` | `match::ai::Judge` (free, not FixDemo) | not in 3,095 (match subsystem) | none (no RNG, no tuning file) | contact gate, 1–4 levels, advantage window, card level 1–4 | NOT in the 32; static-only |
| **Offside detection + judge** | rule `0x140a7b7a0`, update `0x1404621d0`, line `0x14045cf30`, AI readers `0x14045e0d0/0x14050f580/0x142117c30`; judge call `0x1405b6490/0x1405b6660` | free match functions (ANALYZE component) | not in 3,095 | none — nine `.rdata` floats (margins 0.15/0.06/0.10, tol 0.30, bands 1.0/4.0 m) read from the exe at runtime | line = min(0, 2nd-rearmost defender surface, ball); categories 0–3 by distance band | NOT in the 32; detection rule itself verified live (~220 logged evaluations, 0 contradict — `pes-re-offside.md`) |
| **(a) Sanction → scene selection** | `0x140480910` → decision tree `0x14047f170`; request `0x14047feb0`; step 5 `0x140446960`, late-card step 6 `0x140446520`, offside step 0xb `0x140447690` | free functions under the event-builder subtree | **group 1** (in the 3,095: `140480910`, `14047f170`) | `table_foul.bin` (33 scenes 0x20001..20021, cut-group ids, variant weights 0x43xx) — the *content*; the choice of PK / INJURY / PINCH / EXCITE / WARN / CMN / NONE(0x20022) is the code decision tree (card colour, booking count, pinch zone, goal diff, period) | code picks one of 30 outcomes (29 scene ids + no-scene); data only prescribes what each scene contains | NOT in the 32 (match-side, Flow-gated; `14044c880` real-call attempt stopped on `rcx=0` — scene-birth §2). Static: decompile + raw bytes, disasm-verified |
| **(a) Scene → variant** | variant pick `0x141f4fda0` (cache `mgr+0x30`), matcher `0x141f4b1d0`, condition evaluator `0x141f4d2f0` (2,081 insns) | free, FixDemo DB subtree | group 1 (all three in the 3,095) | T1 variant rows: weighted cumulative-% keys, category-parity keys, stadium fallback | the roll (31-bit LCG) and matcher semantics | **YES — `141f4fda0` in the 32 and PROVEN bit-exact** (150/150 calls replayed); matcher/evaluator reached but cache-hot on the steady-state path |
| **(b) Referee + assistant placement during stoppage** | involvement `0x140a3a690` (foul: same half ∧ same touchline ∧ \|spot.z−a.z\| < 14 m; offside: half of the offence); at-stop action `RefereeActionOutofPlay` (vt `0x142642738`) slot 6 `0x140a3a930` (approach until 4 m, then JUDGE), slot 18 `0x140a3a870` (one gesture per stop: 1 restart / 2 foul / 3 offside); at-restart command `RefereeActionBasePosition` (vt `0x142642558`) Update `0x140a39a80`; linesman look-at `0x140a38f20` (2nd-last defender in play; offence spot when stopped) | `referee@match` action classes; entities `RefereeChief`/`RefereeLinesman` built by `0x140a35a00` from the setup-time pick (`pes-re-officials-pick.md`: one uniform draw of a 25/35-id pool, global xorshift128 — *which official* is data-pool + RNG; *where they go at the stop* is code) | not in 3,095 (match referee subsystem) | none (thresholds are code: 4 m, 14 m, 45° angle, pitch-third `halfWidth/3` = 11.35 m) | all of it: who is involved, approach distance, flag angle (near/middle/far from the offence z vs the pitch thirds), one-signal latch, indirect arm held until first touch | NOT in the 32; static-only |
| **(b) Card-scene actor placement** | slot build `0x141f5f330` → foul branch `0x141f5fa90` (slot mask OR of T1 `b0` bits: 1 = carded, 2–5 mates, 11–14 opponents, 22 referee); official model `0x141f58440` via `0x141f577b0`; match-player displacement `0x14047d7f0` (clear a 10 m disc, push to 7 m) | FixDemoMatch creation | group 1 (`141f5f330/141f5fa90/141f577b0/141f58440` all in the 3,095) | per-variant Human records: slot, position (x, z), rotY, gani, times — every authored cut (78 foul `.fdc`, decoded 100%) | the slot→match-player remap (OPEN, INF rec: slot 1 = fouler, 2–5 nearest mates, 11–14 nearest opponents) | `141f5f330` reached at birth (in the 32 set's +8), replayed; `141f57840`/`141f36200` CREATE/LOAD, not in the 32 |
| **(c) Broadcast camera + replay during the stoppage** | **none** — no fixdemo scene, no replay, no `CameraOffside` selection in any foul/offside programme (foul-choreo §5.3.6: the `CameraOffside@match` vt 0x142bd3df8 and `OffsideReplay` string exist but selection not traced; foul §9.8: "replay between whistle and restart: none") | — | — | the scene camera (cards) is authored per cut in the `.fdc` (DmCamera records, `targetKind 6`, `targetNo [22,1]`/`[1,22]`; referee 2.0–7.4 m from the carded player) | the stoppage presentation = hold + 0.1 s wipe (type 4) + 1.0 s fade-in only; in-play broadcast camera (`InplayCamera`, `pes-re-stadium-camera.md` §4) parks dead-ball | NOT in the 32; absence CONFIRMED by every programme read + live captures |
| **(d) Restart hold and release** | step program `Flow+0x878` (30 × int32), index `+0x8f0`, runner `0x14044c880`, driver `0x140445e90` (Flow = `*(MatchControl+0x38)`, §5), frame sync `0x140422030` | `match::MatchControl` + Flow (free, match side) | not in 3,095 (ledger subsystem "restart"; `140422030/140446960/140447690/140444060` are in the ledger under it) | `freekick.o` `suspendTime` 3.0 s (the CPU FK taker's wait after armed — the one data constant in the hold); scene start gate distances 5.0 m / 2.0 m / 4.0 m are code floats | the whole programme: foul hold 5.0 s no-scene (table `0x14259ce60`) / gate + scene; ball-out 2.5 s (table `0x14259ce54`); offside base also 2.5 s (the 3.0 s immediate @0x140447a25 is overwritten per frame unless a player is down); +2.5 s player down (→ 5.0/5.5 s); wipe 5 fr, teleport 20+20 fr, fade-in 59 fr, tail 2 fr (+16 caption), armed = 117 fr after teleport start; late card = next stop within 16 game-minutes (step 6, `FOUL_CMN_CARD_Y_B`, max(2.5 s, 340 ticks)); taker release gates per restart (kick-off receiver test; throw-in ≈81+receiver; corner 189; FK 162; PK 54 — A5Judge §4.7, live-measured where noted) | NOT in the 32 (Flow-gated; the one real-call attempt stopped on `rcx=0`). Live: per-step frame counts captured (FoulChoreographyRE-2 `steps.py` over `live.jsonl`/`seq*.jsonl`); GF sim re-implements it and is tested |

### 6.2 Data vs code, fouls/offsides specifically

- **Data:** the 33 foul scenes and their 78 cut files (`common/demo/fixdemo/foul/cut_data/*.fdc`):
  variant weights, actor slot/position/rotY/gani per cut, card/note/pen prop tick ranges on
  slot 22 (1/60 s), per-variant `maxTime`. Decoded 100% (`FoulChoreographyRE/table_foul.json`,
  `fdc_decoded.json` — in scratch, not repo). The Animetable rows 179–193 (referee `judge_*`)
  and 213–238 (linesman `assistant_*`) are animation data with the clip names fully decoded.
- **Code:** everything that decides *whether* a scene plays at all (the decision tree of
  `0x14047f170` — foul kind, card colour/count, pinch, EXCITE-once, injured), *who* is
  placed where (referee approach, assistant involvement + flag angle, slot remap), and the
  entire restart timing (the step programme). The offside line, its categories, and the
  AI offside flags are pure code (nine exe floats, no file).
- So for stoppages the split is **sharper than for celebrations**: the scene *content* is
  data, but the sanction→scene map, the placement, and the hold are all code. Godot must
  translate the decision tree + step programme (≈ group 1 + the restart subsystem), and can
  load the 78 `.fdc` + tables directly for the card scenes. Offside has **no scene content
  at all** — it is entirely code (flag, arm, 2.5/3.0 s hold, wipe).

### 6.3 Already reachable via real calls (what the 32 covers here)

- **In the 32 (demo-engine EXEC side of a card scene):** `141f4fda0` variant pick
  (PROVEN bit-exact, 150/150), the step-expansion/slot-state/DB family, and at birth
  `141f5f330` slot build + the matcher `141f4b1d0`. The scene side is replayable today.
- **Not in the 32 (match side):** the foul selector `140480910/14047f170`, the restart
  programme `14044c880`/driver/step handlers, the judge, the offside rule, the official
  placement. The single real-call attempt at `14044c880` stopped on `rcx=0` (Flow unknown
  at the time — now §5: one qword read of a live `MatchControl+0x38` unblocks it).
- **Already exercised live/sim (not the 32-set, but observed on real calls):** per-step
  frame counts across whole stoppages (foul-choreo §9.6: stop → armed 407 fr, armed → FK
  164 fr), the 272-frame no-scene foul hold, the 138–140-frame offside-class holds, and
  the stoppage timings ported into GF are tested in `tests/sim/pes_stoppage_timing_test.cpp`
  (throw-in/FK/corner/PK stops at the measured durations, seed 1000).

## 7. Open items

1. **Live manager + owning-pointer capture** — needs the bot in-match. Catcher finished
   32 scans / 0 births / 0 pauses (true negative, no match running); it stays armed in
   scratch for an in-match window.
2. **Flow live instance address** — one qword read at `*(MatchControl+0x38)` in an
   in-match snapshot (the SceneBirth snapshots predate MatchControl capture; a fresh
   one suffices).
3. The 497 rows under neither seed subtree: mostly demo-engine internals (load tasks,
   data bodies) reached via the RTTI vtable roots; no role assigned beyond groups 2/3.
   Not needed for the owner's translate-vs-reimplement decision.
4. **Stoppage camera**: the `CameraOffside@match` (vt 0x142bd3df8) / `CameraReplay`
   selection was never traced; the CONFIRMED "no scene, no replay" covers the step
   programmes read, but the dead-ball pose of the in-play camera itself (its switch to
   the parked position) is not in any doc. Settled by reading the `InplayCamera`
   dead-ball branch (`0x14087f280`, `pes-re-stadium-camera.md` §4.3) or one live
   stoppage sample.
5. **Foul slot → match-player remap** (foul-choreo §7.1): the code binds slots 1/2-5/
   11-14 to literal (side, idx) pairs, not to the carded player; the remap is not found.
   INF recommendation in §6.1; a live card-scene capture (needs the bot in a match)
   settles it.

## 8. Files (scratch only, never committed)

- `<workdrive>/gf-CutsceneRoles-scratch/impure_set.json` — the 3,095 VA list.
- `role_{select,parse,asset,render,other}.json` — per-group rows (va, name, size,
  role, path0 anchor, boundary tag).
- `anchor_ids.json` — the anchor identification evidence (calls/strings per anchor).
- `birth_catch2.py`, `birth2/birth2.log`, `birth2/birth2_result.json` — the catcher
  and its results.
