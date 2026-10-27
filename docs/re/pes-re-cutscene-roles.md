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
and the Flow provenance.

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

## 6. Open items

1. **Live manager + owning-pointer capture** — needs the bot in-match. Catcher finished
   32 scans / 0 births / 0 pauses (true negative, no match running); it stays armed in
   scratch for an in-match window.
2. **Flow live instance address** — one qword read at `*(MatchControl+0x38)` in an
   in-match snapshot (the SceneBirth snapshots predate MatchControl capture; a fresh
   one suffices).
3. The 497 rows under neither seed subtree: mostly demo-engine internals (load tasks,
   data bodies) reached via the RTTI vtable roots; no role assigned beyond groups 2/3.
   Not needed for the owner's translate-vs-reimplement decision.

## 7. Files (scratch only, never committed)

- `<workdrive>/gf-CutsceneRoles-scratch/impure_set.json` — the 3,095 VA list.
- `role_{select,parse,asset,render,other}.json` — per-group rows (va, name, size,
  role, path0 anchor, boundary tag).
- `anchor_ids.json` — the anchor identification evidence (calls/strings per anchor).
- `birth_catch2.py`, `birth2/birth2.log`, `birth2/birth2_result.json` — the catcher
  and its results.
