# PES Fox engine services for cutscene and presentation: ranking, interfaces, Godot mapping, proof design, build plan

Read-only static RE (owner ruling 24-10-26; no repo edits; no snapshot taken this pass). Image
`~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000). Ledger
`tools/dev/ledger/universe.json` (37,855 rows). Scratch: `<workdrive>/gf-FoxCutscene-scratch/`
(scripts `fox_rank3.py`, `fox_rank4.py`, `v5.py`, `cutunlock.py`, `iface.py`, `vtdump.py`; data in `v3/`).
Labels: CONFIRMED = ledger row or image bytes read by script; INF = inferred, reason given; OPEN = what would settle it.
No PES bytes are quoted; VAs and behaviour only.

Sources not present in this session (re-derived, not copied): `<workdrive>/gf-FoxSurvey-scratch/systems.json`
and `<workdrive>/gf-CutsceneImpure-scratch/` do not exist. System labels here come from `universe.rtti_vtables`
(RTTI class per vtable slot, image-read) and namespace rules. The Ghidra bridge on 127.0.0.1:8080 did not answer,
so every body was read with capstone from the image.

## 0. Summary

- **Reconciled counts (CONFIRMED):** 8,300 pure rows universe-wide; cutscene 751 pure of 3,846 rows, so **3,095
  impure** = 2,639 walked/unwalked non-boundary + 456 boundary rows. This matches `pes-re-cutscene-impure.md` §1.
- **Ranking by blocks (presentation population P = cutscene, replay, camera, HUD, audio, crowd, render;
  4,093 impure non-boundary rows):** heap/object lifetime 665 · render/device handles 525 · resource/VFS and
  draw::load 425 · entity/scene store 315 · animation runtime 172 · threading/jobs 169 · UI framework 29 ·
  physics/collision 3 (§2).
- **The main finding is that Fox services are not the main blocker.** Two residual classes dominate:
  DLL-import absolute slots (2,467 P-impure functions reach one; 1,766 of them cutscene) and unwalked
  bodies (1,296; 1,012 cutscene). Only **748 of 2,639 cutscene impure functions are hard-free** (no DLL
  slot or unwalked body in their closure), and of those **83 need a Fox service**; **665 need only
  translate/CRT models** (§3). This matches the impure doc's tiers A+B+C = 718 within 4%.
- **Heap ranks first on blocks but unlocks little.** Its functions are mostly also hard-blocked (34 of 665
  hard-free). The heap free primitive is a virtual dispatch on an allocator object, not a DLL call (§4.1).
- **Pure leaves are counted as services.** Several boundary rows that are only accessors or value copies
  (`1414C1300`, `141F28F60`, `1416FF9F0`, `141878870`) were classified as services; the value-copy
  refinement moved 356 ObjectMemoryOperatorT rows to translate, but the leaf rule did not fire (boundary rows
  have no `leaf` flag). Known small error, ≤ 10 rows (§7).

## 1. Method and population

- **Pure set:** `tools/dev/ledger.py pure_rows` logic, copied verbatim (greatest fixed point over walked,
  non-boundary, no-indirect, no-vtable, no-vtable-call rows). Reproduces 8,300 and 751 (CONFIRMED).
- **Blocking edges:** a function is blocked by a service when its reverse closure (through walked, non-boundary
  callers) reaches a boundary row of that service. Counts are per impure function, not per edge.
- **Service families:** each boundary row's RTTI class (vtable-slot match from `rtti_vtables`), mapped by
  ordered namespace rules; CRT span (`14159EF7C`–`1415B49B0`) by address. Rows with no vtable are reported as
  X groups, not services.
- **Residual classes** (reverse-reach from own signals): **A_dll** = indirect site through an absolute slot
  whose qword is outside the code range (DLL import); **A_code** = absolute slot holding a code VA; **D** =
  other indirect (dispatch) sites; **T** = jump table; **U** = unwalked body; **V** = own vtable install.
  HARD = A_dll ∪ U; SOFT = D ∪ T ∪ V ∪ A_code (dispatchable with receiver narrowing and table models).
- **Greedy unlock:** repeatedly pick the service that completes the most hard-free functions whose Fox-service
  set is fully picked.

## 2. Ranked services (CONFIRMED counts; "blocks" = impure P functions whose closure reaches the service)

Source: `v3/v5_summary.json`, `v3/rows.json`. "Hard-free" = excludes HARD-blocked functions.

| rank | service (family) | boundary rows | rows reached | blocks P | blocks cutscene | sole blocker P | blocks hard-free | sole hard-free |
|---|---|---|---|---|---|---|---|---|
| 1 | B heap / object lifetime | 23 | 22 | 665 | 437 | 216 | 34 | 3 |
| 2 | E render / device handles (draw, gr, cfg, rc) | 179 | 122 | 525 | 332 | 138 | 66 | 31 |
| 3 | A resource/VFS + draw::load preloads | 467 | 437 | 425 | 336 | 53 | 33 | 5 |
| 4 | I entity / scene store (fox entity, match registry) | 90 | 79 | 315 | 209 | 64 | 60 | 44 |
| 5 | D animation runtime (cobra::And, fox::anim) | 73 | 63 | 172 | 166 | 15 | 12 | 8 |
| 6 | G threading / jobs | 22 | 14 | 169 | 163 | 0 | 4 | 0 |
| 7 | F UI framework (ncl, menusystem) | 89 | 13 | 29 | 7 | 1 | 1 | 1 |
| 8 | H physics / collision | 10 | 8 | 3 | 3 | 0 | 0 | 0 |

Notes on the table:
- Menu impure (2,744 non-boundary) is not in P. Pre-refinement counts give UI 311 and heap 960 menu blocks
  (`v3/summary`), so UI is the in-match-menu service (CONFIRMED on that pre-refinement run; menu column not
  re-run after the value-copy refinement).
- Non-service groups (not ranked): **translate** = value/containers 319 rows, value-copy ObjectMemoryOperatorT
  356 rows (no free call; CONFIRMED by callees), CRT/libc 65 rows (all in the CRT span). **X** groups:
  outside-universe 143 callee VAs (see §7), unlinked `FUN_` boundary rows 3 large bodies (140 P blocks),
  and 2 editor-namespace callbacks (57 cutscene blocks each).
- Greedy unlock, all P: I +44, E +42, A +9, B +28, D +11, G +4, F +1 → **139** hard-free functions need a service.
  Cutscene only: **83** (I +17, E +20, D +8, A +5, B +28, G +4, F +1) of 748 hard-free (CONFIRMED,
  `v3/cut_unlock.json`, which holds the VA list per step).

## 3. Co-blockers that bound what any service can unlock (CONFIRMED)

| class | P impure reached | cutscene impure reached | note |
|---|---|---|---|
| HARD: A_dll (DLL-import absolute slot) | 2,467 | 1,766 | 293 distinct DLL slots; top `142530380` (14,280 sites, → runtime DLL VA), `14252FA28` (1,303), `14252FA30` (802), `14252F878` (553) |
| HARD: U (unwalked body) | 1,296 | 1,012 | ledger rows with no body |
| SOFT: D (dispatch) | 3,194 | 2,167 | receiver narrowing needed (INF it is the same objects as §4) |
| SOFT: T (jump table) | 1,078 | 825 | table model |
| SOFT: V (own vtable install) | 1,443 | 950 | translatable |
| SOFT: A_code (abs slot → code) | 826 | 497 | dispatch to known code |
| HARD union | 2,639 | 1,891 | |

Only 42 abs slots hold code; 293 hold DLL addresses (CONFIRMED, qword read from the image). The shared render
stub target `14252FD80` is a DLL address (CONFIRMED), so the render-handle service bottoms out in an unnamed
import (see §4.2). **Names of the DLL slots are OPEN** (not statically nameable; `pes-re-cutscene-impure.md` §5.3).
This is the largest single gate, and it is not a Fox service.

## 4. Interface specs for the top services

Conventions (CONFIRMED where the body was read): a deleting destructor takes `rcx = this`, `dl` bit 0 = free
after destruction, `edx` = size; MSVC vtable install = store of a vtable address into `[rcx]`. Vtable slot
lists are from `v3/vtables.txt` (`B` = boundary row, `w` = walked, `u` = unwalked).

### 4.1 B — heap / object lifetime (rank 1)

- **Entry VAs:** `140332440` (ObjectMemoryOperatorT@C/@D, size 13; callers out-of-scope 347, cutscene 25);
  `140229610` (SharedObject@fox ctor/dtor, size 21, vtable `14253EDA8` 2 slots); `14027B030`-class
  deleting dtors of entity holders (family I, same convention).
- **Delete chain (CONFIRMED by body reads):** `140227930` (1 insn, tail to `1403FB2E0`) → `1403FB2E0`
  (delete(ptr,size), 7 insns, direct to `1403FB5B0`) → `1403FB5B0` (tail) → `1403FB4F0` (60 insns, **virtual
  dispatch**: `CALL [RAX+0x8]` and `CALL [RAX+0x30]` on an allocator object, 5 indirect sites).
- **Free-if-nonnull:** `1403CF770` = if `rcx != 0` tail to `14037FDB0` (CONFIRMED). The survey and impure doc
  label this "alloc path"; **that label is wrong** (corrected here). It is a free. `14037FDB0` → thunk to
  `14038E6F0` (walked, 17 insns; tails to `14038DF60`, `1403B0050`) (CONFIRMED tail edges; the body is the
  core deallocator INF).
- **Allocator vtables (RTTI, CONFIRMED):** `DlAllocator@heap@cobra` `14258E390` (11 slots), `Allocator@heap@cobra`
  `14258E218` (11), `StaticDlAllocator@heap@cobra` `142AC0930` (11), `UnsafeStaticDlAllocator` `142AC0A38` (11),
  `ScopeAllocatorImpl@impl@fox` `14258BCA8` (3, all B). For `[RAX+0x8]` = DlAllocator slot 1 = `1403FBCB0` (B) and
  `[RAX+0x30]` = slot 6 = `1403FBD00` (B). The receiver class is **INF** (not proven; the global holding the allocator is OPEN).
- **Observable state:** the destructor writes the vtable into `[this]`; the free path writes nothing
  observable to the caller beyond heap bookkeeping (CONFIRMED by the stores in the bodies read; allocator
  internal state OPEN).
- **Value copies (not heap):** `1416FF9F0` and `141878870` are `ObjectMemoryOperatorT@N` copy operators (null
  check, one 8-byte load and store, return the destination or null). No allocation (CONFIRMED). Survey label
  "paired alloc/free" is wrong; corrected.
- **Godot counterpart:** none. Lifetime maps to Godot object ownership (`RefCounted`/`Node`/`Resource`); the
  free chokepoint maps to `queue_free`/unreference. DESIGNED (ponytail: no allocator model in GF).

### 4.2 E — render / device handles (rank 2)

- **Shared stub `1415A171A`:** 1 insn, `JMP [0x14252FD80]`; the target is a DLL address (CONFIRMED). It is the
  slot of many vtables: `Dg@dg@gr@fox` `142A09E60` (166 slots, 2,104 vtables in the survey), `CameraPluginInterface@draw`
  `142609108` (8 slots, 7 of them this stub), `FileCallback@fox` `1425542A8` (2 of 3), `EntityLoadCreateFunc@fox`
  `142548820` (1 of 2), `LoadCallback@rc@cobra` `1426A2CB0` (1 of 5). The per-call service identity is **OPEN**
  (`pes-re-fox-layer.md` §4): the stub cannot be named statically, so its contract must come from recorded calls.
- **LoadCallback@rc@cobra** `140B492A0` (size 25, vtable `1426A2CB0` 5 slots: `140B492A0` B, `141B8A5C0` w,
  `1415A171A` B, `140421F40` B ×2). The "resource finished loading" callback into cutscene and menu code (survey
  CONFIRMED label; the body was not read in this pass).
- **CameraPluginInterface@draw** `1408797B0` (size 25; vtable `142609108`): installs the vtable, and an
  init depends on a flag (CONFIRMED install; init body INF).
- **CameraTargetInterface@draw** `14217C770` (size 25, replay); **Draw2D@gr@fox** vtable `1429FFDC8` (13 slots);
  **TextureBase@texture@rc@cobra** vtable `142A85900` (11 slots: 6 B, 3 w, 1 unknown); **Screen2dInfoRef@registry@match@@**
  `140429410` (size 18, vtable `142599B58`, 3 slots: 1 B, 2 w).
- **Observable state:** device queries return values through the shared slot; the query contract is OPEN.
- **Godot counterpart:** `RenderingServer` for draw objects; `godot/shaders/` and `src/pes/{camera,lighting,stadium,fmdl}.hpp`
  per the survey. Camera: `src/pes/camera.hpp`. LoadCallback → Godot signal or `await`.

### 4.3 A — resource / VFS and draw::load preloads (rank 3)

- **File primitives:** `1402B75C0` FileCallback@fox ctor/dtor (size 13; vtable `1425542A8`; flag → free size 0x10).
  `1403BB4F0` File@fs@fox (size 35; vtable `142584C28`, 1 slot B; installs sub-objects at `+0x20` (via `1403B8FB0`)
  and `+0x10`). `1402AFE40` RawFile/EvbFile/BoundingVolumeFile (size 67; state byte at `[rdi+0x40]`, 4 branches,
  sub-object at `+0x80`; calls `1403DB2C0`, `1403DB410`).
- **`1403DB410`** (5 insns) = tail to `1403DB280` (3 insns), which is an **atomic exchange** on `[rcx]` returning
  the old value (CONFIRMED). The real I/O lives under `1403DB2C0` (32 insns; direct to `1403DB590`, `1415D1980`; OPEN
  whether `1415D1980` is an import). So the open/read "primitive" is a flag exchange; state is kept in the object.
- **draw::load lists:** `AnimationDataList@load@draw` `142B61CD0` (18 slots), `EffectList@load@draw` `142B626F0` (18);
  `141E96950` (slot 2, size 28) = list constructor for `Data<AnimationDataLoadId>`; it constructs the node at
  `this+0x48` and calls `1403DC850` (8 insns → `1403F3AF0` → `1403F7DC0`). Each of the 22 LoadId kinds has
  one such list (survey; CONFIRMED vtables for two).
- **FileDB (UniformLoadManager Impl):** `141EA0E40` (size 18, ctor with `0xA0`-byte dtor path) and `141EA1180`
  (size 59; branches on the byte at `[this+0x98]`, a load-state enum; state values OPEN beyond 0–2).
- **Status getter:** `1414C1300` = return the dword at `[this+0x48]` (size 2, pure; CONFIRMED). It is a
  translate-only leaf, currently counted as a service (§7).
- **Observable state:** request objects and states live in the object; completion is signalled to callbacks
  (LoadCallback, §4.2). Read order and buffer addresses OPEN.
- **Godot counterpart:** `ResourceLoader` (threaded requests) over GF's VFS: `src/pes/vfs.hpp` (per the survey),
  `cpk.hpp`, `fpk.hpp`, `ftex.hpp`, `fox2.hpp`, `inflate.hpp` (CONFIRMED present under `src/pes/`).
  `tools/pes21_import/` builds the image-derived code (`dump_image.py`, `verify_image.py`, `image_hashes.json`).

### 4.4 I — entity / scene store (rank 4)

- **EntityLoadCreateFunc@fox** `14027B030` (size 21; vtable `142548820` 2 slots: `14027B030` B, `1415A171A` B). Its
  bit-0 flag path calls `1403CF770` (free-if-nonnull). **The survey calls this a "create path"; the body is a
  dtor with flags** (CONFIRMED). Templated `EntityLoadCreateFuncT<Entity>` `142548888` (2 slots: `14027AF10`,
  `14027ACE0`, both B); `TransformData` variant `142556850` (2 slots, `1402BE5B0`, `1402BE500`).
- **Object@rc@cobra** `14195A880` (size 32; vtable `142A82298`, 1 slot): installs the vtable, calls `14195AEA0`
  on the child at `[this+8]` (destroy, INF), flag → free.
- **Holders:** `140418060` `Reference<Copy<StepupTutorial>>` (size 16; release through `1404131A0`, conditional
  free), `140443C50` `ReferenceCopy<PkMatch>` (size 18). These are match-registry copy-on-write handles.
- **Accessor:** `141F28F60` `FixDemoParamListUtil` (size 7) = return the field at `[this+0x20]`; pure (CONFIRMED).
  Counted as a service here (§7).
- **Observable state:** the entity DB is not modelled; objects hold vtable + child pointers. OPEN: whether any
  entity record is read by the translated game logic (match registry layout per `pes-re-s7-records.md`).
- **Godot counterpart:** no Fox entity DB in GF. Use Godot `Node` tree for scene objects and `Resource` for
  parameter lists; match registry stays in GF's PES record layout (ponytail: thin holders translate mechanically).

### 4.5 D — animation runtime (rank 5)

- **Vtables (RTTI, CONFIRMED):** `BodyAnimControl@And@cobra` `142ABA1A8` (8 slots: `141AEE130`, `141AF2780`, `141AF4480`,
  `141AF4360`, `141AF4650`, `141794410` w, `141AEEE50`, `141AFC4A0` — all B except `141794410`); `GenAnimControl@And@cobra`
  `142ABA840` (8 slots, all B except 3 `141794410` w); `AnimMovePlugin@And@cobra` `142ABA3B8` (7 slots);
  `ActMotionBigJob@And@cobra` `142ABDF78` (6 slots: `141B0EE90` B, `1402297E0` w, `141B0F010` B, `1423A73D0` B,
  `141794410` w, `1403ED9C0` B).
- **Common walked slots:** `141794410` appears in ≥ 20 vtables and `1402297E0` in ≥ 10 (CONFIRMED by the vtable
  dump). Their semantics are OPEN (filler/default/no-op candidates).
- **Doc-cited bodies walked as engine (decision needed):** `141AF61C0` (size 722; boundary by
  `pes-re-animselect.md`; 40 P blocks), `141AFB090` (size 1181; `pes-re-anim-blend.md`; 52 P blocks, cutscene),
  `141EB6840` (size 407; `pes-re-animselect.md`; 48 P blocks, cutscene). The rule is `doc`: the game's own
  animation-select logic is treated as engine. **Un-boundarying them is an owner decision (OPEN).** Their bodies
  are available, so they can be translated.
- **Observable state:** the control objects hold blend state; the pose output feeds the skeleton (INF).
- **Godot counterpart:** GF's own evaluator (no Godot equivalent of `cobra::And`): `src/pes/gani.hpp`,
  `ask.hpp`, `animetable.hpp`, `clips.hpp`, `handseq.hpp`, `skl.hpp`, `mtar.hpp`, `locomotion.*`. `Skeleton3D`
  for pose application only (DESIGNED). `AnimationPlayer` is insufficient for PES blend semantics (INF).

### 4.6 G — threading / jobs (rank 6)

- **Named job construction:** `1403ED9C0` (size 36, string-keyed, `CALL 14022E4C0` with fallback `14258B560`) is slot 5 of
  `ActMotionBigJob` and slot 4 of `DaemonJob@geo@fox` `1429EC4D8` (6 slots). It is the common schedule entry (CONFIRMED).
- **Job graph:** `140AF4400` (replay; `CategoryTS` jobgraph). `DaemonJob@impl@grx@fox` `142A40260`,
  `PreDaemonJob@ph@fox` `142A5A940` (10 slots, several B) are the same pattern.
- **Godot counterpart:** `WorkerThreadPool` or sequential execution in the translated order. Sequential is the
  determinism-safe default (DESIGNED; owner choice OPEN).

### 4.7 F — UI framework (rank 7)

- `Screen2dInfoRef` (§4.2); `NclJsonMessage`/`NclMsgPackMessage` (survey: 4 sites each, menu/cutscene);
  `menusystem::Flash` (one site). In-match menus are 311 blocks pre-refinement (§2). **OPEN:** the ncl transport
  needs a body read; none was done in this pass.
- **Godot counterpart:** `Control` nodes plus a small message bus; translated game-plan logic keeps semantics (survey).

### 4.8 H — physics / collision (rank 8)

- `141B97080` RequestData@collision, `140418310` Object@collision, nine `fox::ph` shapes (animation-side). 3 P blocks.
  Stub last (survey). **Godot:** GodotPhysics, when needed.

## 5. Proof design

**Principle (owner ruling 26-10-26):** a function is PROVEN when every case is bit-exact (return value and every
written byte, from the same inputs and memory); coverage is reported, never a gate.

**Layer 1 — service contract diff (proves the service itself).** For each service entry in §4, record real calls
from a cutscene-time snapshot with CallDelta-style lazy memory mapping (cited in `pes-re-cutscene-impure.md` §3;
the team-tick precedent). Per call record: entry VA; arguments (`rcx`, `rdx`, `r8`, `r9`, `xmm0`–`xmm3`); memory
windows read before the call (lazy map); memory windows written; `rax`/`xmm0` on return. The GF reimplementation
(or translated C++) runs on the recorded pre-state; return and written bytes are diffed bit-exact. Pointer values
are compared through a per-record allocation map (pointers are not portable across processes; INF, required for
heap/VFS/entity handles).

**Layer 2 — replay stubs (makes callers provable).** New stub kind `fox-replay` in `tools/dev/translate/stubs.py`
(`DO_BY_KIND`, `KNOWN_DO`), implemented in `diffharness/run.py` and `xlate.py`. On a call, the stub matches the
argument key and the pre-state hash against the recorded record; on match it applies the recorded writes and
returns the recorded value; **on mismatch it aborts loudly** (same philosophy as `abort-trap`). Callers of a
service then run under the harness and are diffed against PES. Their proof is conditional on the service contract
of Layer 1 (per function: "replay-dependent"; coverage = matched records / reachable calls).

**Layer 3 — DLL and shared-stub replay.** The HARD slots (§3) and shared stub `1415A171A` use the same replay
keyed by slot VA, since their names are unknown. Without this, every HARD-blocked function remains blocked
(2,639 P / 1,891 cutscene).

**Layer 4 — dispatch.** For D sites (virtual calls), receiver narrowing is needed before dispatch (`gen_dispatch.py`).
Replay keys include the resolved target, so a wrong receiver aborts instead of silently diverging.

**Data handling:** recordings are PES-derived (snapshots, call records) and stay in scratch (`brief-rules`: no PES
bytes in deliverables). Tests that need them read them through `PES_EXE` at test time, as the diff suite does.
Snapshot coverage today: four cutscene-time stoppages (goal, goal kick, corner, period end); no kick-off, celebration
or half-time scene (`pes-re-cutscene-snap.md` §1, OPEN).

## 6. Build plan (ordered; each step lists what it unblocks)

Prerequisites (gate the rest; not Fox services):
- **P0a** Decide whether `141AF61C0`, `141AFB090`, `141EB6840` are engine or game code (rule `doc`). Unblocks
  140 P-impure functions (cutscene 100) if un-boundaried. OWNER DECISION.
- **P0b** Verify `1403A72E0` (823 P blocks) and `14038E7F1` (730) — ledger tail targets, not rows, bytes not a
  clear function start (INF: decode artifact). They dominate the outside "real" group (843 blocks). OPEN.
- **P0c** The two `Callback@?A0x55ae6fab@editor` rows (`141B55940`, `141B55A80`; 57 cutscene blocks each): an
  anonymous-namespace class boundary-marked by the `?` rule. Likely game code (INF). OPEN.
- **P0d** Name or model the DLL slots (293; top `142530380`, `14252FA28`, `14252FA30`): the largest gate (2,467 P).
  OPEN.
- **P0e** Add the `fox-replay` stub kind (Layer 2) and its harness/translator support.

Services, in the greedy order for cutscene (`v3/cut_unlock.json` lists the VAs each step unblocks):
1. **S1 Entity / scene store (I)** — unblocks **17** cutscene functions. First: `EntityLoadCreateFunc`
   family (dtor flags, `1403CF770` chokepoint), `Object@rc@cobra` (`14195A880`), holders (`140418060`,
   `140443C50`). Examples of unlocked fns: `14027A6F0`, `140412ED0`, `140412FF0`, `1404180E0`, `140418250`, `1408CE270`.
   Proof: Layer 1 on dtors and holders (registry records via `pes-re-s7-records.md`).
2. **S2 Render / device handles (E)** — unblocks **20**. First: the shared stub `1415A171A` (replay only; its
   target is DLL), `LoadCallback` `140B492A0`, `CameraPluginInterface` `1408797B0`. Examples: `140412300`,
   `14046EB70`, `14195CB50`, `1419F8EE0`, `141A162A0`, `141EBE1D0`. Godot: RenderingServer. Proof: Layer 3 on the shared stub.
3. **S3 Animation runtime (D)** — unblocks **8**. First: decide P0a; then `BodyAnimControl` / `GenAnimControl` /
   `ActMotionBigJob` slots. Examples: `141AFE190`, `141AFE2C0`, `141AFE440`, `1423BE9A0`, `1423BEF60`, `1423C0690`.
   Godot: GF evaluator + `Skeleton3D`.
4. **S4 Resource/VFS and draw::load (A)** — unblocks **5** directly (336 cutscene blocks; most hard-blocked, so
   it is a prerequisite for the preload fan-out). First: `FileCallback`, `File@fs`, `RawFile`, the `draw::load`
   list constructors, `FileDB`. Examples: `141B42CB0`, `141B90190`, `141B937D0`, `141B93860`, `141EC34D0`.
   Godot: `ResourceLoader` over GF VFS.
5. **S5 Heap / object lifetime (B)** — unblocks **28** (the largest cutscene blocker, 437). First: the free
   chokepoint `1403FB2E0`, then the allocator virtual receiver (`[RAX+0x8]`, `[RAX+0x30]` on `DlAllocator` slots
   1 and 6, INF; receiver global OPEN). Examples: `14026E8A0`, `1403EFDF0`, `141B8A470`, `141E82A50`, `141E82B10`,
   `141E82BD0`. Godot: none (object ownership). Built after S1–S2 because its functions need those first.
6. **S6 Threading / jobs (G)** — unblocks **4**. `1403ED9C0` string-keyed job construction. Examples: `1403EC8A0`,
   `14184D4C0`, `141B49EA0`, `142397400`. Godot: sequential or `WorkerThreadPool`.
7. **S7 UI framework (F)** — unblocks **1** (`14100F120`). Menu-side (311 pre-refinement menu blocks). Needs a
   body read of the ncl transport. Godot: `Control`.
8. **S8 Physics (H)** — unblocks 0 cutscene functions (3 blocks). Last.

Expected result after P0 and S1–S7 (INF, from the counts): the cutscene hard-free set grows from 748 by the 83
service-blocked functions only. The remaining cutscene blockers are the HARD set (1,891, dominated by DLL
imports and unwalked bodies), which P0a/P0d must resolve first.

## 7. Discrepancies, corrections and open items

Corrections to existing docs (CONFIRMED by body reads):
1. `1403CF770` is a free-if-nonnull tail to `14037FDB0`, not an allocator (survey §1, impure doc §2 "alloc path").
2. `1416FF9F0` / `141878870` are value copies, not a paired alloc/free (survey §1).
3. `14027B030` is a dtor with flags, not a "create path" (survey §1, scene/entity).
4. `1414C1300` (status getter) and `141F28F60` (accessor) are pure; they are counted as services here (known
   error: the leaf rule did not fire because boundary rows have no `leaf` flag).
5. `1403DB410` is a tail to an atomic exchange, not an I/O primitive; the I/O sits under `1403DB2C0`.

Open items (each with the check that settles it):
- **Allocator receiver** (B): identify the global that holds the allocator object; check `[RAX+0x8]` target on the
  real object in a snapshot (OPEN).
- **Shared stub `1415A171A` service identity** (E): record calls (Layer 3); the DLL target is unnamed (OPEN).
- **`141794410` and `1402297E0`** (common walked slots, ≥ 10–20 vtables): read the bodies (OPEN).
- **Outside tails** `1403A72E0`, `14038E7F1` (P0b) (OPEN).
- **Decode artifacts:** 115 of the 143 outside callee VAs have only `table` edges and look like jump-table words
  misread as code (INF; 51 P blocks). Fix the table rule before using outside counts.
- **Universe rule:** the `?` anonymous-namespace boundary rule marks game code as engine (P0c) (INF).
- **Snapshot coverage:** no kick-off, celebration, half-time or full-time scene captured (OPEN).
- **Heuristic families:** the family table (`v3/rows.json`) is regex-based over RTTI names. Spot checks above
  were by body reads; the rest is INF until each row is read.
- **Population:** "presentation" = cutscene, replay, camera, HUD, audio, crowd, render (the pipeline plan's list).
  In-match menu is reported separately (2,744 impure). If the owner wants menus in P, add them to `PRES`.

## 8. Files (scratch only; no repo edits)

- `<workdrive>/gf-FoxCutscene-scratch/v3/rows.json` — every boundary row with family, P/cut/menu reach.
- `v3/v5_summary.json` — refined service table, chokepoints, greedy order.
- `v3/v4_summary.json` — HARD/SOFT split, slot kinds, outside split, unresolved rows.
- `v3/cut_unlock.json` — cutscene greedy order and the VA list each step unblocks.
- `v3/iface.txt` — disassembly-level evidence for §4 (behaviour only; not for deliverables).
- `v3/vtables.txt` — vtable slot lists for §4 classes.
- Scripts: `fox_rank3.py` (pure set, residual, families), `fox_rank4.py` (HARD/SOFT), `v5.py` (refinements,
  chokepoints), `cutunlock.py`, `iface.py`, `vtdump.py`, `inspect1.py`.
