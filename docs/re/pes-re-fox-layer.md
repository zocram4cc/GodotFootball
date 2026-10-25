# PES Fox engine layer survey — what the 1,242 in-scope boundary rows are

Static RE only (owner ruling 24-10-26). Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(offset = VA − 0x140000000). Ledger: `tools/dev/ledger/universe.json` (37,855 rows;
18,374 in scope; 1,242 in-scope boundary rows, all counted here). RTTI map re-extracted
from the image with `universe.py`'s own `rtti_vtables` logic (11,334 TypeDescriptors,
13,749 vtables, 3,806 engine-owned). Scratch: `<workdrive>/gf-FoxSurvey-scratch/`
(`vtables.json`, `systems.json` = per-row system label). Ghidra bridge sampled
one function at a time for the interface descriptions.

Method, in one paragraph: every boundary row's entry VA was matched against the
re-extracted vtable slots to get its RTTI class (1,188/1,242 = 95.5% hit; the 54
misses are 36 CRT-span rows + library imports + 6 doc-cited engine rows, none of them
vtable slots). Each row's in-scope callers come from `callees` inversion over in-scope
rows only: 6,230 call sites. Systems are grouped by RTTI namespace/class below.
"?" owners (template/anonymous names, 483 rows) were resolved by inner-namespace tokens
(`fox` 291, `cobra` 109, `draw` 30, `std` 18, `match` 17) from the full TypeDescriptor
string, not the outermost-namespace shorthand. Counts below use single attribution
(primary slot owner); 354 slots sit in 2+ vtables (shared stubs — §4).

## §0 Per-system table (the acceptance table)

Boundary rows = distinct entry VAs. Call sites = in-scope caller→boundary edges
(`direct` = CALL, `vtable` = resolved vtable slot, `tail` = JMP-out tail call).
Dependent subsystems = caller `subsystem` tags per system.

| System | Boundary fns | Call sites (d/v/t) | Dependent subsystems (sites) | Surface | Treatment |
|---|---|---|---|---|---|
| resource/VFS | 416 | 662 (4/658/0) | cutscene 552, gameplay 54, replay 45, pre-match 5, restart 2, AI 2, menu 2 | 22 LoadId kinds + FileDB/File managers + fox fs/file + cobra fileio/compression/texlib | **map to Godot** (ResourceLoader + GF VFS/cpk/fpk already in `src/pes/`) |
| math/containers | 313 | 1,351 (31/1,319/1) | cutscene 418, menu 302, pre-match 206, gameplay 169, animation 130, camera 36, replay 35, AI 20, audio 20, restart 9, HUD 3, ball 3 | shared ctor/dtor stubs + PropertyInfo/hash/String/Path/Ref-count/allocator ops | **translate** (pure; StrCode already in `src/pes/strcode.hpp`) |
| render/materials | 164 | 545 (5/540/0) | cutscene 163, pre-match 143, replay 76, menu 64, gameplay 56, camera 20, audio 14, HUD 6, ball 2, animation 1 | CameraPlugin/CameraTarget iface, Uniform/EditUniform cfg, Texture, LoadCallback, Stdm/Audience/Bill/Effect draw objects | **map to Godot** (shaders in `godot/shaders/`, readers in `src/pes/`, `docs/render/`) |
| animation | 124 | 171 (0/171/0) | cutscene 73, gameplay 55, animation 29, pre-match 14 | cobra::And BodyAnim/GenAnim controls, Motion/Rig/Skel descriptors, Anim jobs | **reimplement** (no Godot equivalent; GF gani/ask readers exist, blending is GF's) |
| scene/entity | 122 | 280 (2/278/0) | cutscene 127, gameplay 64, camera 23, replay 19, pre-match 15, animation 11, menu 8, restart 8, AI 3, HUD 2 | Entity create/destroy, Transform, FixDemo param lists, match-registry Reference/Copy holders | **reimplement** (Fox entity DB has no Godot equivalent; registry holders are thin) |
| CRT/libc | 55 | 3,112 (3,069/1/42) | gameplay 618, cutscene 558, menu 545, AI 359, pre-match 279, animation 179, replay 128, audio 126, camera 120, ball 92, restart 61, HUD 23, referee 14, crowd 10 | sqrt import thunk + ~30 unnamed IAT thunks + cookie/EH/thread-init | **translate** (pure libc; only `sqrtf` needs naming) |
| threading/jobs | 21 | 38 (0/38/0) | cutscene 20, gameplay 11, replay 6, pre-match 1 | Daemon/Update/BigJob, Thread/Node, JobPacket | **map to Godot** (WorkerThreadPool / task graph is scheduling, not logic) |
| UI framework | 15 | 57 (0/57/0) | menu 34, cutscene 12, gameplay 8, replay 3 | fox::ncl JSON/msgpack messages, Screen2dInfoRef, menusystem Flash slot | **reimplement** (Fox ncl/menusystem has no Godot equivalent; Godot has no UI yet for it) |
| physics/collision | 12 | 14 (0/14/0) | animation 9, gameplay 3, replay 2 | fox::ph GroupShape, collision RequestData/Object | **map to Godot** (GodotPhysics; 12 rows, 14 sites — smallest system) |
| sound | 0 rows | 0 sites | — (audio 556 + crowd rows are walked game logic) | CRI ADX2 statically linked (no imports), fox `app@sound` AI walked in scope | nothing to treat (game-side triggers already in scope; playback = Godot AudioServer) |
| input | 0 rows | 0 sites | — (match::pad logic walked; pad state arrives via records) | fox/cobra hid/pad vtables unreached by the walk | nothing to treat (Godot Input; wire when human control lands) |

Totals: 1,242 rows, 6,230 sites. Cross-check: 1,188 vtable-linked (incl. one library-named row that is also a vtable slot, `0x140E83730`) + 54 unlinked (36 CRT-span + 12 library + 6 doc-cited) = 1,242.

### Where the systems live in the binary (RTTI namespaces)

- **fox** (outermost): entity/property/stream core, `fox::fs` files, `fox::anim` gani/mtar descriptors, `fox::ph` shapes, `fox::gr` materials, `fox::ncl` messages, `fox::nio` net addresses.
- **cobra**: `cobra::And` animation runtime, `cobra::rc::scene` draw objects + `LoadCallback`, `cobra::rc::render2d` streamer, `cobra::rc::texture`, `cobra::cfg` uniform/effect configs, `cobra::rd` rig/uniform parts, `cobra::heap` allocators, `cobra::compression`/`fileio`/`texlib`, `cobra::game` jobs/listener.
- **draw**: `draw::load` LoadId/Data/List resource triples (22 LoadId kinds, §2), `draw::util` humans/textures, `draw::anime` sequence lists, camera plugin interfaces.
- **"?"** (483): MSVC template/anonymous names — `?$PropertyInfoT` (193, fox property reflection), `?$ObjectMemoryOperatorT` (49+ fox / 19 cobra::And), `?$Data`/`?$List` (cobra scene + draw::load payloads), `?$Reference*` (match-registry holders, game-side flavor but engine-owned), `?$HashMap`, `?$Ref_count` (std).
- **CRT/libc** (55): 36 in CRT span `14159EF7C–1415B49B0` (mostly 1-insn `JMP [IAT]` import thunks), 13 named library (`__security_check_cookie`, `??_M`, `??_L`, `_Init_thread_*`, `atexit`, `_alloca_probe`, `__report_rangecheckfailure`, `sprintf`, `WspiapiLoad`, …), 6 doc-cited engine rows.

## §1 The interface per system (top entry points, as callers observe them)

Conventions: every sample below is a vtable-slot constructor/destructor-shaped body
(`LEA RAX,[vtable]; MOV [RCX],RAX`, optional `CALL 0x140227930` free path) unless noted.
"Callers observe X" = what the call sites need from it. Sizes are ledger `size`
(reachable-body instruction counts).

### resource/VFS (416 rows, 662 sites, 99% vtable)

- `0x1402B75C0` FileCallback@fox — 11 sites (gameplay 10, cutscene 1), size 13: vtable install + free-on-flag (`CALL 0x140227930`). Observed: file-read callback lifetime.
- `0x1402AFE40` RawFile@fox (+StageBlockFile/EvbFile share the slot) — 7 sites, all gameplay, size 67: state machine over `[RDI+0x40]` (4 branches calling `0x1403DB410`/`0x1403DB2C0` open/read primitives). Observed: async file read progress polling.
- `0x1403BB4F0` File@fox::fs — 3 sites, size 35: installs `0x142584C28`, constructs +0x20/+0x10 subs, `CALL 0x14037CA20`. Observed: open-a-file-handle.
- `0x1414C1300` Deflate/InflateHandleBase — 8 sites (restart 2, AI 2, replay 2, cutscene 2), size 2, direct+table: decompression handle ops. Observed: cpk inflate path (GF has `src/pes/inflate.hpp`).
- `0x141E96950` `?$List` AnimationList/DataList — 19 sites, all cutscene, size 28: list-node constructor (bytes decode as prologue; bridge lists no function — shared slot, §4). Observed: animation-load-list construction during cutscene preload.
- `0x141EA0E40/0x141EA1180` FileDB (UniformLoadManager Impl) — 4 sites each, all cutscene: load-manager file tables. Observed: uniform/texture preload tables.
- The other ~390 rows: per-kind `?$Data`/`?$List` payloads for each of the 22 LoadId kinds (DemoCharacter/Parts/Uniform/Ball/Effect/Stadium/Texture/Mob2DCharacter/Object/CornerFlag/Light/DemoObject/Animation/Human/AnimationData/ModelViewAnimation/Audience/BigFlag/Bill/GoalNet/Prop/StandsObject), each 8–18 rows of 1–3 insn accessors/ctors. Observed: resource-load request/response records the draw layer fills in.
- GF has: `src/pes/vfs.hpp` (PesVfs), `cpk.hpp`, `fpk.hpp`, `ftex.hpp`, `fox2.hpp` (+ `docs/re/pes-re-fox2-format.md`), `inflate.hpp`. Godot side: `ResourceLoader` + GF VFS mount at runtime.

### math/containers (313 rows, 1,351 sites, 98% vtable)

- `0x140421F40` — 381 sites (menu 147, cutscene 81, gameplay 57, camera 33…), size 2: `XOR EAX,EAX; RET`. Shared by 2,269 vtables (§4): the shared null/empty slot (pure-virtual or no-op filler). Observed: returns 0/NULL; callers treat it as "not implemented / empty".
- `0x140332440` `?$ObjectMemoryOperatorT` — 89 sites (pre-match 31, cutscene 25, animation 17…), size 13: installs `0x14253EDC0` (+`0x8` free path via `CALL 0x140227930`). Observed: typed new/delete for Fox values (String/Path/Color/Quat/Vector…).
- `0x140229610` SharedObject@fox — 82 sites, size 21: installs `0x14253EDA8`, `0x10001`-flavor alloc via `0x1403CF770` else `0x10`-byte free. Observed: ref-counted object root lifetime.
- `0x1416FF9F0` + `0x141878870` N-placed `?$ObjectMemoryOperatorT` pair — 30 sites each, identical caller sets (pre-match 18, animation 7…): paired alloc/free for one value type. Observed: allocate then release.
- `0x1417AC090` HashValueTester@fox::impl — 29 sites (animation 14, pre-match 12), size 13: installs `0x1425497C0`. Observed: hash-map key comparison.
- `0x140408AA0` `?$basic_stringbuf` — 14 sites (menu 7, gameplay 3…): std stringbuf vtable slot. Observed: stringstream formatting (menu text building).
- The other ~300: `?$PropertyInfoT` per-type reflection slots (one per Fox value type), `?$KeyEqualityTester`, `?$Ref_count/Reference/ReferenceCopy` (incl. match-registry holders), `SharedPointer`, `DlAllocator/StaticDlAllocator`, `EffectConfig/UniformConfig/*Config` value bags. Observed: construction, comparison, hashing, ref-counting of engine values.
- GF has: `src/pes/strcode.hpp` (StrCode64 = CityHash, the Fox string hash). Everything else is fresh but trivial.

### render/materials (164 rows, 545 sites, 99% vtable)

- `0x1415A171A` — 199 sites (cutscene 57, gameplay 47, menu 40, replay 21, audio 14…), size 1 (capstone): `JMP [0x14252FD80]`, slot of 2,104 vtables headed by `Dg@dg@gr@fox` (166), `HidMan`, `Device@hid`. Observed: shared render/device-service stub; callers go through it for device-level queries.
- `0x140B492A0` LoadCallback@cobra::rc — 26 sites (cutscene 22, menu 2…), size 25. Observed: "resource finished loading" callback into cutscene/menu code.
- `0x1408797B0` CameraPluginInterface@draw — 20 sites (camera 9, replay 9, cutscene 2), size 25: installs `0x142609108`, flag-dependent init. Observed: camera plugin create/init (GF has `src/pes/camera.hpp` + `godot/` broadcast camera).
- `0x1402417F0` PropertyInfo/List reflection — 9 sites (cutscene 7, gameplay 2), size 2. Observed: tiny property accessors.
- `0x14217C770` CameraTargetInterface@draw — 6 sites, all replay, size 25. Observed: replay camera target binding.
- Uniform/EditUniform + UniformConfig rows (5×4 sites, all pre-match): kit/uniform material config application. Observed: pre-match kit selection writes into uniform configs (GF has `src/pes/uniform.hpp`).
- GF has: `src/pes/fmdl.hpp`, `ftex.hpp`, `lighting.hpp`, `stadium.hpp`, `camera.hpp`; Godot side `godot/shaders/` (pes_skin/hair/uniform/pitch/eye/post…), `godot/post/pes_post_effect.gd`, `docs/render/` (turf, sky, exposure, grading, B7 effects). No GF equivalent of the Fox `draw::` runtime itself — that is the map target (RenderingServer).

### animation (124 rows, 171 sites, 100% vtable)

- All vtable, no direct: the call surface is the `cobra::And` plugin interface — `BodyAnimControl`/`GenAnimControl` (+Resource/ListNode), `AnimMovePlugin`, `AnimConvertBoneSpacePlugin`, `AnimSyncPlugin` (doc-phrase), `ActMotionBigJob`/`AnimControlProcessJob`/`RigToSkelJob`/`PhPreProcessJob`, `SkelFile`/`Gani` descriptors (`fox::anim` GaniBaseFile/MtFile/MtarFile/RigFile), `MotionDesc/MotionFrameInfo/GaniInfo/GeomInfo/RigInfo/SkeletonInfo` memory operators.
- Top: `0x141B027C0` (10 sites, all gameplay), `0x1406E4700/4740/5F50` (4 each, animation+cutscene), `0x1423A73D0` ActMotion/RigToSkel/CoreBigJob (4, cutscene), `0x141AEE1F0` control-list nodes (3, cutscene). Sizes 7–35.
- GF has: `src/pes/gani.hpp`, `ask.hpp`, `animetable.hpp`, `clips.hpp`, `handseq.hpp`, `skl.hpp`, `mtar.hpp`, `locomotion.*`, `movestep_*` — the data readers and the GF-side blend/state. The Fox `And` runtime (jobified anim graph evaluation) has no Godot equivalent in the port: reimplement as GF's own anim evaluator driven by the translated game logic.

### scene/entity (122 rows, 280 sites, 99% vtable)

- `0x141F28F60` `?$FixDemoParamListUtil` — 21 sites, all cutscene, size 7: cutscene parameter-list utility. Observed: cutscene (FixDemo) parameter marshalling.
- `0x140418060/0x140417FA0/0x1404180A0` `?$Reference` holders — 12/7/6 sites (gameplay/camera/menu…), size 16: `CALL 0x1404131A0` + conditional free. Observed: acquire/release of match-registry records.
- `0x14027B030` EntityLoadCreateFunc@fox — 9 sites, all cutscene, size 21: installs `0x142548820`, `0x10001` create path via `0x1403CF770`. Observed: create entity.
- `0x140429320` ReferencePointer/CommandInfoRef — 7 sites: command-info reference holders. Observed: passing command records by ref.
- `0x140443C50` ReferenceCopy/PkMatchRef — 6 sites: match-record copy-on-write handles.
- `0x14195A880` Object@cobra::rc — 6 sites (cutscene 3, HUD 2, camera 1), size 32: installs `0x142A82298`, child+`0x1403FB5E0/5B0` create pair. Observed: scene-object creation.
- GF has: nothing for the Fox entity DB yet. The match-registry `Reference/Copy/Pointer` holders are thin (acquire/release + record pointer) and translate mechanically.

### CRT/libc (55 rows, 3,112 sites — the bulk is compiler-generated)

- `__security_check_cookie` — 1,368 sites in every subsystem (menu 386, gameplay 236, cutscene 231…), size 8, `how=doc`. Observed: stack-cookie check; the translator emits the equivalent guard, Godot build keeps its own.
- `0x1415A1780` — 633 sites (gameplay 231, AI 209, animation 57, ball 49…), size 1: `JMP [0x1425301F0]`. CONFIRMED `sqrtf` (docs `pes-re-carrier-helpers.md §3`, `pes-re-first-touch.md §144`, `pes-re-holder-tree.md §262`: "sqrtf through the IAT"). Observed: `sqrt(d2)` everywhere distances are compared.
- ~30 unnamed 1-insn `JMP [IAT]` thunks (`0x1415A1720` 136 sites, `0x1415A02B0` 106, `0x1415A1708` 102, `0x1415A1726` 81, …down to 1 site): CRT/libm imports (targets resolve to runtime DLL addresses `0x6FFFF…`, unidentifiable statically — no names committed here). Observed: pure scalar helpers called directly; all `direct` (+9 `tail` at sqrt).
- `_Init_thread_header/footer` (70 each), `atexit` (60), `??_M`/`??_L` array new/delete (81/69), `_alloca_probe` (13), `__report_rangecheckfailure` (9), `sprintf` (1, replay), `WspiapiLoad` (1, gameplay): thread/CRT startup + EH + formatting. Observed: process/thread init, array allocation, one replay format path.
- `0x1415A1A68` (7) + `0x1415A1AD4` (58): local CRT wrappers used only by audio rows (33+33 sites) — `CALL 0x1415A1BB0` formatted-output core with `0x143707040` buffer. Observed: audio-log/format path.
- 6 doc-cited rows (`pes-re-animselect.md` ×3, `pes-re-anim-blend.md`, `pes-re-fox-registry.md`, `pes-re-anim-select.md`): engine-side animation-select helpers kept as boundary by doc rule. Observed: anim selection services (see animation treatment).

### threading/jobs (21 rows, 38 sites, 100% vtable)

- `0x1403ED9C0` Daemon/Update/SmallJob/BigJob/LightCaptureJob — 11 sites (cutscene 6, gameplay 4), size 36: string-keyed (`CALL 0x14022E4C0` with `0x14258B560` fallback) job construction. Observed: schedule a named job.
- `0x140AF4400` Thread/Node/Pad/Hid — 6 sites, all replay, size 18: null-guarded virtual dispatch pair. Observed: thread/node service query.
- Rest: JobPacket/SyncJobPacket, BigJob/PreDaemonJob/DaemonJob, SmallJob (1–2 sites each). Observed: job-graph edges (`fox::me::jobgraph`).
- Treatment: map to Godot — job scheduling is not game logic; Godot `WorkerThreadPool` (or a small GF task graph) preserves order, determinism comes from the translated logic above it.

### UI framework (15 rows, 57 sites, 100% vtable)

- `0x140429410` Screen2dInfoRef — 13 sites (gameplay 8, replay 3, menu 2): 2D-screen info reference. Observed: HUD/overlay screen-info handles.
- 10× `fox::ncl` NclJsonMessage(Base)/NclMsgPackMessage — 4 sites each (menu 3, cutscene 1), sizes 2–246: JSON/msgpack message construction over shared vtables. Observed: menu↔engine setting/command messages (game-plan values ride these).
- `menusystem::Flash` (1 site, menu): the single reached Flash (Scaleform-style) slot. Observed: one menu Flash call — the rest of menusystem is out of scope or walked game logic.
- Treatment: reimplement — Fox ncl messaging + menusystem/Flash have no Godot equivalent; Godot `Control` nodes + a small message bus replace the transport, translated game-plan logic keeps the semantics. Note: the in-match menu logic itself (3,777 rows) is walked in-scope code, not boundary — only its Fox transport is.

### physics/collision (12 rows, 14 sites, 100% vtable)

- `0x141B97080` RequestData@collision — 2 sites, gameplay, size 25: full ctor shape. Observed: collision query setup.
- `0x140418310` Object@collision — 1 site, gameplay. Observed: collision object handle.
- 9× `fox::ph` GroupShape/MutualConnectionList/JobSetup — 1 site each, all animation. Observed: animation-side physics shape queries (ragdoll/cloth adjacency, not ball physics — ball physics is walked game logic).
- Treatment: map to Godot (GodotPhysics). 12 rows / 14 sites: smallest system; stub last.

### sound and input: no boundary (findings, not gaps)

- **Sound**: 0 boundary rows. CRI ADX2 is statically linked (no `criAtom*` imports; `docs/re/pes-re-audio.md` §1.1 OBS), and the Fox `app@sound` AI layer (Match/Sequence/Chant/CrowdNoise/Commentary…) is walked in-scope game logic (556 audio + 147 crowd rows, `how=doc`). In-scope audio rows call only 14 boundary targets (12 CRT + 1 containers + 1 render). Playback maps to Godot AudioServer; trigger semantics stay in the translated logic.
- **Input**: 0 boundary rows. fox/cobra hid/pad vtables (`HidThread`, `Pad@impl@hid@fox`, `HidMan@game@cobra`, per-device `Pad@…@impl@hid@me@cobra`) are never reached by the walk — pad state enters match logic through `match::pad` records (walked), not through engine calls. Maps to Godot Input when human control lands.

## §2 Presentation split (cutscenes 3,846 + in-match menus 3,777 = 41% of scope)

Cutscene and in-match-menu rows are overwhelmingly **game logic above Fox**, not Fox itself:
their engines are the walked roots (FixDemo* demo classes, matchPlan::*, menu::MatchPause,
match::camera/Replay vtables — all in-scope, §ledger scope rules). The Fox framework shows
only at the 1,242-row boundary, through two mechanisms:

| | cutscene (3,846 rows) | in-match menu (3,777 rows) |
|---|---|---|
| callee edges total | 12,474 | 19,724 |
| edges into boundary | 1,923 (15.4%) | 955 (4.8%) |
| …of which `direct` (own helpers/CRT) | 564 | 556 |
| …of which `vtable` (Fox services) | 1,357 | 397 |
| …of which `tail` | 2 | 2 |
| distinct boundary rows reached | ~830 (798 via vtable + 33 via direct, overlap ~1) | ~90 (72 via vtable + 22 via direct, overlap 4) |

- **Cutscene's Fox surface is resource loading**: 518 of its 1,923 boundary sites (27%) are
  `draw::load` LoadId/Data/List slots — cutscene preload construction (Animation 33,
  Effect 32, DemoCharacter 27, Parts/Uniform/Stadium 24 each, Texture 23, …). Next are
  fox-core lifetimes (517: EntityLoadCreateFunc 66, SharedObject/allocators) and
  cobra-core (134), draw facade (110), CRT (408: cookie 231 + thread-init/format).
  Only 6 sites touch `gr` directly. Translation implication: cutscene game logic
  translates; its 518 load sites become Godot `ResourceLoader` calls against the GF VFS.
- **In-match menu's Fox surface is lifetimes + messages**: 386 of 955 sites (40%) are
  `__security_check_cookie` (compiler-generated, vanishes in translation), then fox-core
  object lifetimes (191: ObjectMemoryOperator 147, stringbuf/filebuf streams), `?$Reference`
  holders (match-registry reads), and the 10× ncl message ctors. Only 40 sites touch the
  draw facade, none touch `gr`. Translation implication: menu logic translates almost
  cleanly; its Fox transport (ncl messages, Screen2d refs) is reimplemented on Godot UI.

Vtable sharing (affects the "surface size" reading): 48 boundary rows sit in ≥10 vtables
each — notably `0x140421F40` (`XOR EAX,EAX; RET`, 2,269 vtables: the shared null slot),
`0x1415A171A` (2,104 vtables: shared render/device stub), `0x140332440`
(435 vtables: ObjectMemoryOperatorT master slot). Distinct-VA count (1,242) overstates the
distinct-service count; the per-system tables above name the real services.

## §3 Treatment summary with reasons and sizes

1. **translate** — 368 rows, 4,463 sites (CRT/libc 55 + math/containers 313):
   pure computation with no engine state: sqrt/import thunks, cookie/EH/thread-init
   (emitted by the toolchain, not hand-written), string/hash/Path/Property reflection,
   ref-counts, allocators, config value bags. Runs through the pipeline unchanged; GF
   already has `src/pes/strcode.hpp` for the one named hash. Only `0x1415A1780`=sqrtf
   needs a name; the other ~30 IAT thunks translate as libc calls by signature.
2. **map to Godot** — 601 rows, 1,245 sites (resource/VFS 416 + render/materials 164 + threading/jobs 21):
   Fox services whose counterpart is a Godot facility: file/resource loading →
   `ResourceLoader` over GF's VFS (`src/pes/vfs.hpp`, `cpk.hpp`, `fpk.hpp`, `ftex.hpp`,
   `fox2.hpp`, `inflate.hpp` all exist); draw/gr/material/camera → RenderingServer +
   existing `godot/shaders/` + `src/pes/{fmdl,lighting,stadium,camera}.hpp` +
   `docs/render/`; jobs → task scheduling. Reasons: reimplementing a renderer or a VFS
   the port already has is waste; the game logic above stays translated either way.
3. **reimplement** — 261 rows, 508 sites (animation 124 + scene/entity 122 + UI framework 15):
   Fox subsystems with no Godot equivalent: the `cobra::And` anim runtime (jobified
   graph evaluation over gani/rig/skel descriptors), the Fox entity DB
   (create/destroy/Transform/scene queries), the ncl message bus + menusystem/Flash.
   GF already reads every data format involved (`gani/ask/animetable/clips/handseq/skl/mtar`
   in `src/pes/`, `.ask`/`.gani` in AGENTS.md) — what is missing is the runtimes, which
   are GF's own anim evaluator, entity store, and Godot-`Control` menu layer.
4. **no action** — sound + input (0 boundary rows): already on the right side of the cut.
   Sound triggers and pad records are walked game logic; only the playback/input sinks
   need Godot endpoints (AudioServer, Input) when those milestones land.
5. **physics/collision** (12 rows, 14 sites) maps to GodotPhysics but is stub-last: the
   reached surface is animation-adjacent shapes, not ball physics.

Net: of 6,230 boundary call sites, ~72% (CRT + containers) translate mechanically,
~20% map onto Godot facilities GF partly has, ~8% need genuine reimplementation
(anim runtime, entity store, menu transport) — and none of the 7,623 presentation rows
is Fox itself except through those counted sites.

## §4 Open items / confidence notes

- Unnamed CRT IAT thunks (~30 VAs, e.g. `0x1415A1720` 136 sites): behavior as callers
  observe it is "pure scalar helper" (all `direct` calls, sizes 1–8); identities are
  UNKNOWN (targets are runtime DLL addresses, not statically nameable). Resolves by
  signature during translation, not by further RE.
- `0x1415A171A` (199 sites, 2,104 vtables): single attribution says draw-owned, but the
  slot is shared across `Dg/HidMan/Device` hierarchies — its exact service per call
  site is UNKNOWN from static evidence alone. Treatment (map to Godot) is unaffected.
- Out-of-scope-flavored rows reached from in-scope callers (savedata `Handle` 4 sites,
  online SharedCountDeleters 2, nio IpPortAddress 2, SCENARIO_EVENT PostProcess 7,
  kps Holder 2): counted in scene/entity + math/containers; each is a `vtable` reach
  from gameplay/cutscene/replay/pre-match. They look like record-holder reuse across
  the mode boundary, not live save/online behavior — OPEN, needs one decode pass each
  before translation touches them.
- Sound/input "0 rows" is a walk-coverage claim (the walk never reaches those vtables),
  not a proof no path exists: an indirect call the walk did not resolve could hide one.
  Confidence High, not Confirmed.
