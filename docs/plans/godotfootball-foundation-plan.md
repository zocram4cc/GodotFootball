# GodotFootball — foundation

## Context

Set up a new public repository, `GodotFootball` (created at the path the owner gave; that path never appears in any tracked file). It is a Godot 4.7 + C++ GDExtension AI-vs-AI football match engine. Assets come straight from a user's own PES 2021 install with no conversion step. The tactical and simulation code comes from GameplayFootball (GF): the code and logic, not its assets.

This plan stops at the foundation:
- a native C++ reader for PES21 files (cpk / fpk / ftex / fmdl / ask / skl / mtar / gani, with PES's own download/sider override priority);
- a Godot viewer that shows one PES21 full-body model, with PBR materials, playing one PES21 animation, proven by examined screenshots;
- the GF match engine compiled as a library that plays a deterministic headless AI-vs-AI match.

Rendering the simulated match, the tactics data file and PES clip classification are the next plan.

Hard rules for every step:
- No PII and no machine paths in any tracked file. That includes no default install path anywhere: the PES21 root always arrives at runtime.
- No PES-derived bytes committed. That covers extracted files, PES-mined name lists and transcribed PES number tables. All of them are produced at runtime from the user's install.
- Every subagent output is untrusted. Whoever accepts a step reads the full diff and re-runs that step's Verification command; a claim nobody re-ran is not done.

### MCP / skill evaluation (decided)

- **No new MCP.** Godot MCP servers (Coding-Solo/godot-mcp, ee0pdt/Godot-MCP, …) drive the editor and GDScript. This project's loop is CMake + `godot` CLI + Movie Maker frames under gamescope, and the shell covers all of it. Adding one would mean running third-party code with editor write access for no gain. The Ghidra/REA MCPs already mounted cover any binary format RE (e.g. frig unit pairing, if it is ever needed).
- **One new managed skill** (`godotfootball-loop`), written in Step 7 once the loop exists. Existing `reverse-engineer-anything` covers format RE.

### Facts this plan stands on (verified this session)

- Toolchain present: cmake 4.4.4, g++ 16.2.1, clang++ 23.1.1, ninja 1.13.2, python 3.14 + numpy 2.5.3, gamescope 3.16.31, ffmpeg 9.0.2, git, zlib/sqlite3/boost headers in `/usr/include`, Vulkan RADV on an RX 6900 XT (llvmpipe also installed; it must never be picked). Missing: `godot`, `scons` (not needed — godot-cpp builds with CMake).
- Godot `4.7.2-stable` exists: asset `Godot_v4.7.2-stable_linux.x86_64.zip` (77,860,424 bytes) + `SHA512-SUMS.txt` in release `godotengine/godot-builds@4.7.2-stable`. `--headless` uses the dummy renderer (no pixels), so rendered proof runs under gamescope instead.
- godot-cpp `10.0.0-stable` (versioned independently of Godot; one release targets Godot 4.3–4.7). CMake cache vars: `GODOTCPP_API_VERSION` (allowed `4.3`–`4.7`), `GODOTCPP_TARGET` (`template_debug|template_release|editor`), `GODOTCPP_DISABLE_EXCEPTIONS` (default ON).
- PES21 install layout:
  - `Data/dt*.cpk`. Includes `dt00_x64.cpk`, `dt00_4K_x64.cpk`, `dt13_all.cpk`, …; also junk like `dt18_all - Copia.cpk` and `*.bak`.
  - `download/*.cpk` plus `download/DpFileList.bin`. Format: u32 0, u32 count (0x25 = 37), 8 pad, then 48-byte records whose first bytes are a NUL-terminated cpk name.
  - `sider.ini`: `livecpk.enabled = 1`, `cpk.root = ".\livecpk\ball-root"`. Sider docs: roots are checked in listed order and the first match wins; relative roots resolve against the folder holding `sider.exe` (= install root).
- `dt13_all.cpk`:
  - `common/anime/FoxAnim/Body/CharacterAssets/body_anim_skel.ask` (1104 B, uncompressed), `body_high_skel.ask`, `body_skel.frig`.
  - `hand_anim_skel.ask`.
  - `common/anime/Mbinfo/json/anim_infos.json` (15.7 MB).
  - `common/anime/FoxAnim/Body/body_anime_file0..9.mtar` (4,389 clips).
- `.ask` layout, decoded by hand this session:
  - u32 version=1, u32 boneCount=20.
  - Then boneCount records of 44 bytes: u32 pad `0xCDCDCDCD`, i32 nameOffset (relative to the field's own position), i32 parent (−1 root), f32 tx,ty,tz, f32 0, f32 qx,qy,qz,qw.
  - Then a NUL-separated name table.
  - Bone 0 `sk_root_hip` at (0, 1.096120, 0); all 20 rotations identity.
- `download/4cc_40_faces.cpk` and `4cc_60_midcup.cpk` hold `Asset/model/character/face/real/<id>/#Win/face.fpk` (+`.fpkd`) and `…/real/<id>/sourceimages/#windx11/*.ftex`. Face id `70405` exists, with `arm_bsm.ftex` (a full-body pack).
- StrCode32: `strcode32("MOTION") == 0x08908348`, `strcode32("RIG_ROOT") == 0xd3c3ff90`, `strcode32("sk_root_hip") == 0x3ca4c491`.
- fmdl: GF has no reader of its own. The 4cc `pes-fmdl` addon (`FmdlFile.py`) has **no licence** (github.com/the4chancup/pes-fmdl-blender shows none) and must not be ported or copied. `BobDoleOwndU/FMDL-Studio-v2` is **MIT**: it is the reference for the C++ reader, attributed in NOTICE.
- GF source pin: commit `efd50d91c71947ab85974ddccbf224514eeab32f`. `src/`, `tests/` and `tools/pes21_import/` are clean at that commit.
- GF tree contents: 293 tracked stock `.anim` clips (all already carry `<tpose_rig>`, i.e. on the PES animation rig); `match.cpp` is 7,653 lines.
- GF match-to-UI coupling is confined to: `match.hpp:27-35` (menu/ingame widgets + `menutask.hpp`), `match.hpp:59-60` (gui2 widgets), `match.cpp:1042-1150` (HUD construction), `:1237` (`LoadingMatchPage`).
- GF's AI-vs-AI path is menu-driven (`menu/controllerselect.cpp:119,205` sets every side CPU). `[balance]` result lines are printed from `menu/ingame/gameover.cpp:274-420`.
- GF seeds RNG at `main.cpp:409-413` (`random_seed`, `srand`, `randomseed`, `fastrandomseed`); 28 `EnvironmentManager::GetTime_ms()` sites live in `onthepitch/{match.cpp,cutsceneplayback.hpp,rigdiodirector.cpp,player/humanoid/humanoidbase.cpp,ball.hpp}`.
- The GF data closure (`data/*`, `utils/database`, `utils/xmlloader`, `base/*`) already builds with only GTest + SQLite (`tests/CMakeLists.txt:671-708`).

---

## Approach

Steps 1→2→3→4→5 are sequential. Step 6's copy-and-cut work (6.1–6.2) needs only Step 0 and can run in parallel with Steps 1–5; its runner and tests (6.3 onwards) need Step 3's rig. The tree builds and `ctest` is green after every step.

### Step 0 — Repository bootstrap and guards

1. `git init` the new directory; branch `main`. Commits use the owner's existing global git identity (approved); never run `git config`.
2. Files:
   - `LICENSE`: Apache-2.0 (required — GF fork and Google Research Football are Apache-2.0).
   - `NOTICE`: attributions —
     - GameplayFootball (original author's public-domain code + the Google Research Football Apache-2.0 lineage), copied from GF commit `efd50d91`;
     - FMDL-Studio-v2 (MIT) as the fmdl format reference;
     - Godot Engine (MIT) and godot-cpp (MIT).
     Upstream copyright headers inside copied files stay verbatim.
   - `README.md`: what it is, build commands, "point it at your own PES 2021 install via `PES21_ROOT` / `--pes21-root`".
   - `AGENTS.md`, carrying these rules:
     - PES wins;
     - two skeletons (anim = `.ask`, render = `.skl`), never calibrate binds;
     - no magic numbers;
     - visual claims need an examined frame;
     - no PII / no PES bytes;
     - agent output is reviewed and re-run before acceptance.
3. `.gitignore`:
   - build/tool outputs: `build*/`, `godot/.godot/`, `godot/bin/`, `*.so`, `*.import`;
   - PES files: `*.cpk`, `*.fpk`, `*.fpkd`, `*.fmdl`, `*.ftex`, `*.gani`, `*.mtar`, `*.ask`, `*.skl`, `*.frig`, `*.dds`;
   - mined names: `anim_names*.txt`;
   - captures: `captures/`.
4. `tools/check_pii.sh` (bash, no arguments; exit 1 on any hit), scanning `git ls-files -z`:
   - Path patterns: home and mount directories, removable-media and macOS user roots, Windows drive letters, and agent config directories (the exact list lives in `tools/check_pii.sh`).
   - Built at runtime, never written in the file: `/$USER/`, `-$USER-`, `$(hostname)`.
   - Email regex `[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}`, except lines matched by `tools/pii_allowlist.txt`. Each allowlist line is a `path<TAB>regex` pair; initially empty.
   - Forbidden tracked extensions (same list as `.gitignore` PES group).
   Register it as CTest test `repo_pii_scan` (Step 1 CMake).
5. Commit: "Bootstrap repository and PII/PES guards".

### Step 1 — Toolchain and an empty GDExtension that loads

1. `tools/fetch_godot.sh`:
   - downloads `Godot_v4.7.2-stable_linux.x86_64.zip` and `SHA512-SUMS.txt` from `https://github.com/godotengine/godot-builds/releases/download/4.7.2-stable/`;
   - verifies the zip's SHA-512 line;
   - unzips into `${XDG_CACHE_HOME:-$HOME/.cache}/godotfootball/godot-4.7.2/`;
   - prints the binary path.
   `tools/godot.sh` execs that binary with all arguments, after running `fetch_godot.sh` if it is missing. Every later command uses `tools/godot.sh`.
2. `git submodule add https://github.com/godotengine/godot-cpp extern/godot-cpp`, checkout tag `10.0.0-stable`.
3. Top-level `CMakeLists.txt` (CMake ≥ 3.24, C++17, Ninja):
   - `set(GODOTCPP_API_VERSION 4.7)`, `set(GODOTCPP_TARGET template_debug)`, `add_subdirectory(extern/godot-cpp)`.
   - GTest via `FetchContent` tag `v1.14.0` (same as GF).
   - Targets: `pesfmt` (STATIC, `src/pes`), `pesrig` (STATIC, `src/rig`, links pesfmt), `gfsim` (STATIC, `src/sim`, Step 6, links pesrig), `godotfootball` (SHARED, `src/gdext`, links pesrig + `godot-cpp`, the target name defined at `extern/godot-cpp/cmake/godotcpp.cmake:380`), apps `pesfmt_dump`, `gfsim_run`. Set `OUTPUT_NAME godotfootball` and `PREFIX lib` explicitly so the file name does not carry godot-cpp's `GODOTCPP_SUFFIX`.
   - `godotfootball` output: `godot/bin/libgodotfootball.so`.
   - `enable_testing()`, `add_subdirectory(tests)`.
4. `godot/project.godot`:
   - `config/name="GodotFootball"`, renderer `forward_plus`, `rendering/driver/threads/thread_model` default;
   - `physics/common/physics_ticks_per_second=100` (GF's 10 ms tick, `main.cpp:412`).
   `godot/godotfootball.gdextension`:
   ```
   [configuration]
   entry_symbol = "godotfootball_library_init"
   compatibility_minimum = "4.7"
   [libraries]
   linux.debug.x86_64 = "res://bin/libgodotfootball.so"
   ```
   If Godot reports "No GDExtension library found for current OS and architecture", add `linux.editor.x86_64` with the same value.
5. `src/gdext/register_types.cpp`: `godotfootball_library_init` registering nothing yet except a `GFInfo` class with `static String version()` returning `"0.1.0"`.
6. `godot/smoke/smoke.gd` (`extends SceneTree`): `_init()` prints `GFInfo.version()` and `quit()`.

### Step 2 — PES21 virtual file system (`src/pes/`, no Godot, no exceptions thrown)

Error style everywhere in `pesfmt`: functions return `bool` / `std::optional<T>` and write a reason to `std::string* error`. No throws, because godot-cpp builds with exceptions disabled.

1. `strcode.{hpp,cpp}`: port GF `tools/pes21_import/strcode.py` (CityHash64 1.0) verbatim in logic. Exports `uint64_t StrCode64(std::string_view)` and `uint32_t StrCode32(std::string_view)`; add `GzHash` only when a caller needs it (none in this plan).
2. `cpk.{hpp,cpp}`: port GF `cpk.py`:
   - `read_utf`, `read_value`, `strip_crypt` (XOR `x=0x5F`, `m=0x15`), `crilayla_decompress`;
   - drop the dead `if False else` expressions at `cpk.py:143,152`;
   - header read: 2048 bytes at offset 16;
   - TOC: `TocOffset+16`, size from the big-endian u32 at +4 of the de-obfuscated probe;
   - file base = `min(TocOffset, ContentOffset)` when `ContentOffset` is set.
   `class CpkArchive { bool Open(path, error); const std::vector<CpkEntry>& Entries(); std::optional<std::vector<uint8_t>> Read(const CpkEntry&, error); }` reads one entry by seeking in place; it never extracts to disk. `CpkEntry{ std::string path /* DirName + '/' + FileName, no leading slash */; uint64_t offset, size, extractSize; }`.
3. `fpk.{hpp,cpp}`: port GF `fpk.py`: `foxfpk`/`foxfpkd`, fileCount at 0x24, 48-byte entries; strip a leading drive prefix (a Windows drive letter); normalise `\` to `/`; reference entries parsed and kept unused. `FpkArchive::Parse(std::span<const uint8_t>)` → entries `{path, offset, size}` over the in-memory blob.
4. `vfs.{hpp,cpp}` — `class PesVfs` with `bool Mount(const std::string& root, error)`, `bool Exists(path)`, `std::optional<std::vector<uint8_t>> Read(path, error)`, `std::vector<std::string> List(prefix)`, `std::string MountReport()`.
   - Path keys are case-folded (`std::tolower` ASCII) with `/` separators and no leading `/`.
   - Mount order, later wins on equal path:
     1. `Data/` cpks matching regex `^dt\d\d_[A-Za-z0-9_]+\.cpk$`, sorted by (two-digit number, then `_4K_` variants after their base so 4K textures win, then name). This skips `*.bak` and `* - Copia.cpk`.
     2. `download/` cpks in `DpFileList.bin` order. Parse: count at offset 4, records of 48 bytes from offset 16, name = bytes up to first NUL. Missing file → skip download with a `MountReport` line; listed-but-absent cpk → report line, continue.
     3. sider live roots: if `sider.ini` exists with `livecpk.enabled = 1`, each `cpk.root = "…"` in file order. Replace `\` with `/`; resolve relative roots against the install root. Absolute Windows roots (`^[A-Za-z]:`) are unresolvable: report and skip. Roots mount so that the first-listed root wins (mount them in reverse listed order). Loose files are indexed by walking the directory.
   - Index = `std::unordered_map<std::string, Source>` built once at mount (TOC only, no reads). `MountReport()` lists every mounted archive/root with entry counts; it is printed by tools and never committed.
5. `apps/pesfmt_dump.cpp` — CLI `pesfmt_dump <command> --pes21-root DIR …`, JSON to stdout:
   - `mounts`;
   - `ls <prefix>`;
   - `cat <path> > file` (dev-only, never into the repo);
   - later steps add `ask`, `skl`, `ftex`, `fmdl`, `gani`.

### Step 3 — Skeletons and the rig (`src/pes/ask|skl`, `src/rig/`)

1. `ask.{hpp,cpp}`: parse the layout under **Facts** (44-byte records, self-relative name offsets). Validate `version == 1` and `recordStart + count*44 <= size`; on failure return `std::nullopt` with the reason.
2. `skl.{hpp,cpp}`: port GF `pes_skl.py` (`version=12`, recordSize 56 — reject any other, per bone nameOffset + parent + row-major 3×4 **global** bind matrix, string table).
3. Load sources at runtime (no tables in code):
   - anim bind = `body_anim_skel.ask`;
   - tips = `body_high_skel.ask` (`sk_toe_*`, `tip_sk_hand_l`, `tip_sk_head`);
   - render bind = `body.skl`, `hand_l.skl`, `hand_r.skl`, found inside the fpk at VFS path `Asset/model/character/#Win/common_package.fpk` (inner path ends with `common/body.skl` etc.; match by suffix `/body.skl`, `/hand_l.skl`, `/hand_r.skl`).
4. `src/rig/rig_names.hpp` — structural knowledge only, no PES numbers; port from GF `retarget.py` (`GF_BODY_NODES` :403-425, `_finger_nodes` :440-451, `HELPER_TO_GF` :569-606, `_HELPER_FAMILIES` :608-627, `PES_TRACK_MAP` :57-67, `PES_ROOT_UNIT=0`, `PES_MOTION_UNIT=1`, `_ALIGN_REF_CHILD` :247-256):
   - the 20 body nodes `(gfName, pesBone, gfParent)` and the 38 finger nodes generated with the same rule (fingers ordered thumb, index, middle, pinky, ring; thumb has mata/mcp/pip, others mata/mcp/pip/dip; `skh_<finger>_<seg>_<l|r>`);
   - helper-collapse tables;
   - the gani track map;
   - the align reference children.
   Joint ID order = this list order (same as GF `jointorder.cpp`).
5. `src/rig/rig.{hpp,cpp}` — `struct Rig { std::vector<RigBone> bones; /* in joint-ID order */ };` with `RigBone{ std::string gfName, pesBone; int parent; Vec3 animLocalPos; Vec3 animGlobalPos; Vec3 renderGlobalPos; Quat align /* W, Fox space */; }`. `std::optional<Rig> BuildRig(PesVfs&, std::string* error)`:
   - anim positions from the ask (globals = chain sum; rotations are identity);
   - render positions = translation column of each `.skl` global matrix;
   - `W` per bone = port of `_build_align` (`retarget.py:300-363`) incl. `_minarc`, `_frame_align`, the hand palm rule and the forearm twist inheritance. Inputs for the helper constants come from the files, not code: `dsk_toe_l/r` from `body.skl`; `sk_toe_l - sk_foot_l`, `tip_sk_hand_l - sk_hand_l`, `tip_sk_head - sk_head` from `body_high_skel.ask`;
   - finger anim positions = port of `_extend_bind_with_fingers` (`retarget.py:372-381`): `handAnim + W(hand)·(fingerRender − handRender)`. Every finger's `W` equals its hand's `W` (GF `ALIGN_GF` rule).
   - `std::optional<int> ResolveBone(const Rig&, std::string_view pesBone, const Vec3* renderPos)` = port of `resolve_bone` (`retarget.py:630-662`), nearest-bone fallback over render positions.
6. Coordinates: everything in this plan stays in **Fox space** (Y up, +Z forward, metres), which Godot renders directly (Y-up, right-handed). The Fox→GF basis `(x,y,z)→(x,−z,y)` is applied only inside `src/sim` adapters in the next plan.
7. `pesfmt_dump ask|skl|rig` outputs JSON for the parity check.

### Step 4 — Textures, models, and a static full-body character in Godot

1. `src/pes/ftex.{hpp,cpp}`: port GF `ftex.py` `parse` + `_read_mip`. Width before height. Chunks with `rel & 0x80000000` or `comp == uncomp` are stored raw; a zlib failure keeps the raw bytes. `ftexsCount > 0` is rejected; cube = `textureType & 4` (6 faces). Output `FtexImage{ int pixelFormat, width, height, mipCount; bool cube; std::vector<std::vector<uint8_t>> mips; }` — no DDS step.
2. `src/gdext/pes_texture.cpp` maps `pixelFormat` to `godot::Image::Format`:

   | pf | Godot format |
   |---|---|
   | 0 | `FORMAT_RGBA8`, after swapping B↔R (data is BGRA) |
   | 1 | `FORMAT_R8` |
   | 2 | `FORMAT_DXT1` |
   | 3 | `FORMAT_DXT3` |
   | 4 | `FORMAT_DXT5` |
   | 8 | `FORMAT_RGTC_R` |
   | 9 | `FORMAT_RGTC_RG` |
   | 10 | `FORMAT_BPTC_RGBFU` |
   | 11 | `FORMAT_BPTC_RGBA` |
   | 12 | `FORMAT_RGBAH` |
   | 13 | `FORMAT_RGBAF` |
   | 14 / 15 | CPU-convert to `FORMAT_RGBAH` |

   Mips are concatenated into one buffer → `Image::create_from_data(w, h, mipCount > 1, fmt, data)` → `ImageTexture::create_from_image`. A cube texture is skipped with a log line (not needed for characters).
3. `src/pes/fmdl.{hpp,cpp}`: clean-room reader of FMDL v2.03 (`magic "FMDL"`, version `0x4001eb85`), written from FMDL-Studio-v2's `Fmdl.cs` (MIT) layout knowledge. Never open or copy `FmdlFile.py`. Output:

   ```cpp
   struct FmdlBone { std::string name; int parent; Vec3 localPos, worldPos; };
   struct FmdlTexture { std::string directory, filename; };     // as stored
   struct FmdlMaterial { std::string name, technique, shader;
                         std::vector<std::pair<std::string,int>> textures; // role -> texture index
                         std::vector<std::pair<std::string,Vec4>> params; };
   struct FmdlMesh { int materialIndex; std::vector<int> boneGroup; // local -> fmdl bone index
                     std::vector<Vec3> pos, nrm; std::vector<Vec4> tan; std::vector<Vec2> uv0;
                     std::vector<std::array<uint16_t,4>> joints; std::vector<Vec4> weights;
                     std::vector<uint32_t> indices; /* LOD 0 only */ };
   struct Fmdl { std::vector<FmdlBone> bones; std::vector<FmdlTexture> textures;
                 std::vector<FmdlMaterial> materials; std::vector<FmdlMesh> meshes; };
   std::optional<Fmdl> ParseFmdl(std::span<const uint8_t>, std::string* error);
   ```

   Half-float vertex attributes are decoded to float. Faces use LOD 0 only.
4. `src/gdext/pes_character.cpp` — `class PesCharacterBuilder : RefCounted` with `Node3D* build_face_model(Ref<PesLibrary>, int face_id)`. `PesLibrary` is a `RefCounted` wrapping `PesVfs` + cached `Rig`, with `Error open(String root)`, `String mount_report()`, `PackedStringArray list(String prefix)`.
   - Assembly = every `.fmdl` inside VFS `Asset/model/character/face/real/<id>/#Win/face.fpk`. Missing fpk → `ERR_FILE_NOT_FOUND` with the path in the error string.
   - Output tree: `Node3D "Character"` → `Skeleton3D "Skeleton"` with one bone per `Rig` bone. Name = `gfName`, parent = rig parent, rest = translation `animLocalPos`, identity rotation. Then one `MeshInstance3D` per kept mesh, with `skeleton = ".."` and a `Skin`.
   - Skin binds: for each rig bone `b`, `invBind(b) = inverse( Transform3D(Basis(Quat(W(b)).inverse()), renderGlobalPos(b)) )`. At rest the mesh is carried from the render pose into the anim T-pose by exactly `W`, the same transform GF's base.anim.util bake applied.
   - Vertex joint remap: fmdl bone-group index → fmdl bone name → `ResolveBone(rig, name, &fmdlBone.worldPos)` → rig joint ID. Weights renormalised to sum 1 after merging duplicate joints; a vertex with no resolvable joint goes to `body` with weight 1, counted and logged.
   - Mesh drop rules, ported 1:1 from GF `fmdl_to_fullbody.py`:
     - `is_non_render_pass` (:1012-1023, words `antiblur`/`outline` in material name or base texture);
     - `is_effect_texture` / `is_placeholder_texture` (constants beside them);
     - the hider rule `texture_is_hider` (:940-963, `HIDER_MAX_ALPHA = 8`) and `mesh_faces_sample_opaque` (:890-915), evaluated on the decoded base texture via `Image::decompress()`.
     - The kit slot (`is_kit_slot_texture`, bare `u0XXXp0`) is kept and rendered with a neutral grey `StandardMaterial3D`, `albedo_color = Color(0.5, 0.5, 0.5)` (named constant `kKitPlaceholderAlbedo`). Real kits are the next plan.
   - Material = `StandardMaterial3D`:
     - role containing `Base_Tex` or ending `_SRGB` → `albedo_texture` (alpha scissor if the texture has alpha below 1 anywhere, threshold `0.5` as `kAlphaScissorThreshold`);
     - role containing `NormalMap` or texture stem ending `_nrm` → `normal_texture` + `normal_enabled`;
     - role containing `SpecularMap` or stem ending `_srm` → `roughness_texture` / `metallic_texture` from the SRM channels: R = specular → `metallic_specular`, G = roughness channel, B = metal channel. Exact channel semantics are `unverified — confirm against one ball/kit _srm by inspection`; if wrong, swap the channel constants only.
   - Texture lookup order for each `FmdlTexture`:
     1. `directory` mapped by replacing a leading `/Assets/pes16/` with `Asset/` and inserting `#windx11/` before the file name, extension replaced by `.ftex`;
     2. same stem in `Asset/model/character/face/real/<id>/sourceimages/#windx11/`;
     3. first VFS path under `Asset/model/character/` whose file stem equals the stem (case-folded).
     Unresolved textures are logged and that slot is left empty. If fmdl textures are referenced by hash only (no strings), stop and record it as a new format finding before inventing a hash index.
5. `godot/viewer/viewer.tscn` + `viewer.gd`. User args after `--`: `--pes21-root DIR` (required), `--face ID` (required), `--clip NAME|0xHASH` (optional; without it the skeleton stays at rest, which is the anim T-pose), `--in-place`. The scene:
   - `WorldEnvironment`: `ProceduralSkyMaterial`, tonemap `TONE_MAPPER_FILMIC`, `ssao_enabled = true`, `glow_enabled = false`;
   - `DirectionalLight3D` with shadows, rotation (−50°, 30°, 0);
   - `Camera3D` at (0, 1.0, 3.2) looking at (0, 1.0, 0), FOV 40. Each is a named constant in `viewer.gd`.
   - Missing args → `push_error` + `get_tree().quit(2)`.
6. `tools/capture_viewer.sh --face ID [--clip NAME] [--frames N=60] [--out DIR=captures/<timestamp>] [-- extra viewer args]`. It runs:

   ```
   GF_GODOT=$(tools/fetch_godot.sh)
   gamescope --backend headless -W 1280 -H 720 -- "$GF_GODOT" --path godot \
     --write-movie "$OUT/frame.png" --fixed-fps 30 --quit-after "$N" \
     res://viewer/viewer.tscn -- --pes21-root "$PES21_ROOT" --face "$ID" ...
   ```

   It writes the log to `$OUT/godot.log`; exits 1 unless the log has a line containing `Using Device` and no line containing `llvmpipe`; prints the frame file list. `PES21_ROOT` must be set in the environment, otherwise exit 2 with a usage line.

### Step 5 — PES21 animation playing on the character

1. `src/pes/mtar.{hpp,cpp}`: port GF `mtar.py` (magic `0x0BFFA89C`, count, 16-byte entries at 0x20 `{nameHash, index, offset, size}`).
2. `src/pes/gani.{hpp,cpp}`: port GF `gani.py` in full: TrackHeader, units, segment table, quantised quaternion bitstream, `AnimHalf` (sign<<16, (exp+0x1DC00)<<13, mant<<13), Hermite tangents, static units, QUAT_DIFF / VECTOR_DIFF, and `validation()`.
3. `src/pes/anim_names.{hpp,cpp}` — `AnimNameIndex::Build(PesVfs&, const std::string& installRoot)`. The name dictionary is produced at runtime (it is PES-mined and must never be committed):
   - source 1: every `"file_name"` value in `common/anime/Mbinfo/json/anim_infos.json` (string scan for `"file_name"\s*:\s*"([^"]+)"`, no JSON library);
   - source 2: `PES2021.exe` in the install root, scanned for runs of `[A-Za-z0-9_]{3,90}`;
   - each candidate kept only if `StrCode32(candidate)` is a hash present in the 10 mtars.
   Cache to `user://pes21_cache/anim_names.txt`, rebuilt when the exe/json size+mtime change (stamp in the first line). Names never resolved stay addressable as `0x%08x`.
4. Pose sampling: port GF `gani_to_anim.py` `build_samplers` (:124), `sample_root` (:149), `fk_pose` (:195) and the `PES_FRAME_MS = 1000/59.94` timing. Skip `map_vec`/`map_quat` (Fox→GF) — output stays in Fox space. Root position scale `kPesPosToMetresGameplay = 1.0 / 20480.0`, with GF's measured-provenance comment (`retarget.py:80-90`). Bones not in `PES_TRACK_MAP` keep rest pose.
5. `src/gdext/pes_animation.cpp` — `class PesAnimation : RefCounted` with `Error load(Ref<PesLibrary>, String name_or_hash)`, `float length_seconds()`, `void apply(Skeleton3D* skeleton, float time_seconds, bool in_place)`:
   - per-bone local rotations → `set_bone_pose_rotation`;
   - root: `body` pose position = rest + sampled root offset (XZ zeroed when `in_place`);
   - after applying, call `skeleton->force_update_all_bone_transforms()`. This is GF AGENTS' "Viewer trap": a pose applied without a forced update renders the previous pose.
   `viewer.gd` applies the clip every `_process` at `fmod(time, length)`.
6. `pesfmt_dump gani <name|0xhash> --pes21-root DIR` dumps frames as JSON.

### Step 6 — The GF match engine as a deterministic headless library (`src/sim/`)

1. Copy, from GF commit `efd50d91` via `git -C <GF checkout> archive efd50d91 src | tar -x` into a temp dir, then move into `src/sim/` keeping relative layout. The include root is `src/sim/`, so `#include "onthepitch/…"` keeps working.
   - Keep: `base/`, `types/`, `scene/` (minus `scene/scene2d/`, `scene/objects/image2d.*`, anything including SDL), `managers/`, `framework/`, `systems/isystem*.{hpp,cpp}` (interfaces only), `data/`, `league/`, `misc/`, `onthepitch/`, `utils/` (minus `gui2/`, `objectloader.*`, `rigdio*`, `cloth*`, `sdl_surface*`, `threadhud*`, `orbitcamera*`), `defines.hpp`, `gamedefines.{hpp,cpp}`, `gametypes.hpp`, `utils.{hpp,cpp}`, `blunted.{hpp,cpp}`.
   - Do not copy: `main.*`, `gametask.*`, `remotecontrolmode.*`, `menu/`, `hid/`, `loaders/`, `viewer/`, `systems/graphics|audio|physics/`.
   - Copy the 293 tracked `data/media/animations/**/*.anim` files (not the `.anim.util` files) into `assets/gf_anims/` with the same subdirectories, from the same commit.
2. Cut rules, applied until `gfsim` compiles with **no** SDL/OpenGL/OpenAL/ODE include path and `grep -rn '#include.*SDL' src/sim` prints nothing:
   1. **HUD**: delete `match.hpp:27-35,59-60` includes and every widget member (`radar`, `playerHUD`, `handoffWipe`, `tacticsDebug`, `scoreboard`, `statsOverlay`, `formationGraphic`, `goalBug`, `banner`), their construction in `Match::Init` (`match.cpp:1042-1150`), the `LoadingMatchPage` close (`:1237`), and every later use. No replacement — HUD is Godot's job.
   2. **Menu**: Match stops reading `MenuTask`. New `src/sim/onthepitch/matchsetup.hpp`:

      ```cpp
      struct MatchSetup {
        int homeTeamDbId, awayTeamDbId;
        unsigned int seed;
        int durationMinutes;
        float timeScale;
        std::string animationDir;
      };
      ```

      This, plus the `Properties` config, is passed to `Match`. The `MatchData` is constructed as `new MatchData(home, away)` by the runner. `UpdateControllerSetup` (`match.cpp:1544-1574`) and `SetMatchData(nullptr)` (`:1269`) are deleted. `AnimCollection::Load` takes its root directory from `MatchSetup::animationDir` instead of the hardcoded `media/animations` (`animcollection.cpp:422-534`); the `pes`/`experimental`/`luxury` subdirectory rules stay as they are.
   3. **Human control**: delete `humangamer.*`, `player/controller/humancontroller.*`, `penaltyshootoutcontroller` human branches. Replace `GetHumanGamerCount()` at its call sites with the AI branch, deleting the human branch (`grep -rn 'GetHumanGamerCount\|IsHumanControlled\|HumanController' src/sim` must print nothing). `gamedefines.hpp` loses the SDL keycode table.
   4. **Presentation** (stadium, goals, sky, crowd, staff, props, pennants, sun, entrance, cutscenes, replays, celebration staging, goal director, camera, lights, sounds, rigdio, procedural pitch, pitch overlays, team flags, wipes, model viewer, remote control): delete the files and their call sites. Cutscene queries that gate restarts (`IsPlaying`-style) become constant `false`, so a restart deferred for a cutscene is never deferred.
   5. **Body render**: delete `PrepareFullbodyModel`, `UpdateFullbodyNodes`, `UpdateFullbodyModel`, `SkinInto`, `GetActiveBodyGeometry`, `ChooseBodyLod`, `BuildBodyLod`, `UploadFullbodyModel`, `SetKit`, `skinning.*`, `skinweights.*`, `splitgeometry.*`, `facerig` geometry binding, the fullbody load + `LoadSkinWeights` in the `Match` ctor (`match.cpp:164-182`, `player_body` config), and the `fullbodyNode` / `SkinWeights` / kit parameters of `Player::Activate` and `Team` (`team.cpp:84,103,112`).
   6. **Skeleton**: the three `LoadObject(... "media/objects/players/player.object")` sites (`team.cpp:123`, `officials.cpp:24`, `animcollection.cpp:429`) call `src/sim/onthepitch/player/humanoid/pesplayernode.{hpp,cpp}` — `boost::intrusive_ptr<Node> BuildPlayerNode(const pesrig::Rig&)`. It creates `Node("player")`, `SetLocalMode(e_LocalMode_Absolute)`, then the body/finger tree with GF node names, identity rotations, and local positions = rig `animLocalPos` converted Fox→GF `(x,y,z)→(x,−z,y)`. The `Rig` is built once by `pesrig::BuildRig` from the user's PES install and handed to `Match` through `MatchSetup` (add field `const pesrig::Rig* rig`). There is no committed or cached fallback rig: without a PES install the runner exits 2 with "need --pes21-root", and the sim tests skip.
      `ball.cpp:43` builds `Node("ball")` in code. Every other `ObjectLoader` call site is presentation (rule 4).
   7. **Clock**: replace every `EnvironmentManager::GetTime_ms()` in `src/sim/onthepitch` and `src/sim/utils` with `Match::GetSimTime_ms()` (= ticks × 10). Match-clock advance uses ticks × 10 × `timeScale`. Afterwards `grep -rn 'GetTime_ms()' src/sim/onthepitch src/sim/utils src/sim/data` prints nothing.
   8. **RNG**: `Match` ctor seeds `randomseed(setup.seed)` and `fastrandomseed(setup.seed)`. Every `rand()`, `srand()`, `time(` in `src/sim/{onthepitch,data,base}` becomes `random(…)` / `fastrandom(…)` from `bluntmath`; `grep -rnE '\b(rand|srand|time)\(' src/sim/onthepitch src/sim/data src/sim/base` prints nothing. `playerdata.cpp:31` skin colour uses `random`. The sim runs single-threaded (no `std::thread` in `src/sim/onthepitch`). `randMutex` and the global engines in `bluntmath.cpp` stay; a second `Match` in the same process re-seeds through the ctor.
3. `src/sim/onthepitch/matchreport.{hpp,cpp}`:
   - `void PrintMatchReport(const MatchData&, FILE*)` moves the printf block from GF `menu/ingame/gameover.cpp:274-420` verbatim (same `[balance…]` line formats, so GF's simbatch regexes still parse);
   - plus `Full match complete` and `[determinism] %016llx`. That value is a 64-bit FNV-1a over `(tick, ball position, each player position)` folded in every 100 ticks (`kDeterminismSampleTicks = 100`).
4. `apps/gfsim_run.cpp`:
   - Arguments: `--db-sql FILE` (required), `--home ID --away ID` (required), `--pes21-root DIR` (required; builds the rig), `--seed N` (default 1), `--minutes N` (default 20), `--timescale X` (default 1), `--anims DIR` (default `assets/gf_anims` relative to the current working dir), `--set key=value` (repeatable, into `Properties`).
   - The database is in-memory: add `bool Database::LoadSqlScript(const std::string& path)` that opens `:memory:` and `sqlite3_exec`s the file.
   - Loop `match->Get(); match->Process();` until the match reports full time, then `PrintMatchReport`. Exit 0.
5. `assets/fixtures/teams.sql` (synthetic, no real names):
   - `CREATE TABLE` statements copied from the GF schema (`regions`, `countries`, `leagues`, `teams`, `players`, `tournaments`, `tournamentdata` exactly as GF has them).
   - Rows: region 1 "Region", country 1 "Country", league 1 "League"; teams 1 "Home XI" (`HOM`, `255, 50, 50` / `255, 255, 255`) and 2 "Away XI" (`AWA`, `50, 50, 255` / `255, 255, 255`).
   - Both teams' `formation_xml`: GF team 1's 4-4-2 string verbatim. Roles: `GK, LB, CB, CB, RB, LM, CM, AM, RM, CF, CF`.
   - `tactics_xml`: every slider at GF's `baseTeamTactics` default (`teamAIcontroller.cpp:69-91`).
   - `formation_factory_xml` / `tactics_factory_xml` equal to the above.
   - 16 players per team (11 starters in the role order above + subs `GK, CB, CM, CM, CF`), `formationorder` 1..16. Each has: firstname "Player", lastname `"H<n>"` / `"A<n>"`, age 25, `base_stat` 0.65, height 1.80, weight 75, skincolor 1, hairstyle `medium01`, haircolor `black`, nationalteam_id 0.
   - `profile_xml`: all 39 `STAT_KEYS` from GF `install_team.py:100-114` at 0.65. The five `gk_*` keys are 0.65 for GKs and 0.20 for outfielders (`OUTFIELD_GK_STAT` constant comment: "PES rates outfield goalkeeping at the floor").
6. Tests (`tests/sim/`): copy GF tests from `efd50d91` whose sources are all inside `src/sim`, adjusting include roots only. Any GF test referencing a cut unit is not copied; its file name and reason go into the commit message. Add:
   - `sim_determinism_test`: two in-process matches, same seed, 2 minutes, `timeScale` 1 → identical determinism hash and identical `[balance]` lines; seed+1 → different hash.
   - `sim_full_match_test`: one 5-minute match completes and reports goals ≥ 0 for both sides.
   Both use `teams.sql` and the rig built from `PES21_ROOT`; skipped (`GTEST_SKIP`) with a message when `PES21_ROOT` is unset.

### Step 7 — Managed skill

`manage_skill create godotfootball-loop`. Description: "Build, test, and visually verify GodotFootball (Godot 4.7 GDExtension + PES21 VFS + GF sim): cmake/ctest, PII scan, viewer capture under gamescope, frame review, headless gfsim determinism." Body:
- the exact commands from Verification;
- the llvmpipe check;
- the "examine the frame, a clean log is not evidence" rule;
- the agent-output review rule;
- the no-PII/no-PES-bytes rules.

---

## Critical files & anchors

| File (GF, read-only) | Region | Why |
|---|---|---|
| `tools/pes21_import/retarget.py` | `PES_TRACK_MAP` :57-67, `_build_align` :221-363, `_extend_bind_with_fingers` :372-381, `GF_BODY_NODES`/`_finger_nodes` :403-451, `HELPER_TO_GF`/`resolve_bone` :569-662 | The rig logic to port. Its hardcoded `PES_BIND` / `PES_RENDER_BIND` / `_A_TOE_*` / `_T_*` numbers are NOT ported; they come from `.ask`/`.skl` at runtime. |
| `tools/pes21_import/gani.py` + `gani_to_anim.py` | whole file; `build_samplers`/`sample_root`/`fk_pose` :124-222 | Exact curve decode and root composition; the 4,389-clip oracle. |
| `tools/pes21_import/cpk.py` | :13-202 | UTF table, XOR, CRILAYLA, TOC base rule. |
| `tools/pes21_import/fmdl_to_fullbody.py` | :782-1033 | 4cc pack conventions (kit slot, hiders, non-render passes) that decide which meshes draw. |
| `src/onthepitch/match.cpp` / `match.hpp` | :1042-1150, :1237, :1269, :1544-1574; includes :27-35,:59-60 | Exact UI/menu seams to cut in Step 6. |

---

## Verification

Prerequisites: `export PES21_ROOT=<your PES 2021 folder>` in the shell, never in a file. Working dir is the repo root. `GF=<GF checkout>` only for the dev parity scripts below.

| Step | Command | Must show |
|---|---|---|
| 0 | `bash tools/check_pii.sh` | exit 0; then `echo "<a machine path>" > t.txt && git add t.txt && bash tools/check_pii.sh` → exit 1 naming `t.txt` (undo afterwards) |
| 1 | `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure` | green; `repo_pii_scan` listed |
| 1 | `tools/godot.sh --headless --path godot -s res://smoke/smoke.gd` | prints `0.1.0`, exit 0 |
| 2 | `build/apps/pesfmt_dump mounts --pes21-root "$PES21_ROOT"` | every `Data/dt*.cpk` except `.bak`/`Copia`; 37 download entries in DpFileList order; the `livecpk/ball-root` root |
| 2 | `build/apps/pesfmt_dump ls common/anime/FoxAnim/Body/CharacterAssets --pes21-root "$PES21_ROOT"` | exactly the 4 files listed under **Facts** for that dir (`body_anim_skel.ask` 1104 B …) |
| 2 | ctest `pes_strcode_test` | `StrCode32("MOTION")==0x08908348`, `RIG_ROOT==0xd3c3ff90`, `sk_root_hip==0x3ca4c491` |
| 3 | ctest `pes_rig_test` (skips without `PES21_ROOT`) | 58 bones; `body` anim global (0, 1.096120, 0) ±1e-6; `hip` local (0, −0.104039, −0.018350) ±1e-6 (equals GF's tracked `player.object` after basis change); every `W` unit-norm; trunk/neck/clavicle `W` = identity within 1e-4 |
| 3 | `PYTHONDONTWRITEBYTECODE=1 python3 tools/dev/parity_rig.py --gf-tools "$GF/tools/pes21_import" --pes21-root "$PES21_ROOT"` (dev script: imports GF `retarget.py`, compares with `pesfmt_dump rig`) | every anim position and `W` within 1e-5 of GF's tables. This is the oracle, and it proves GF's transcribed constants equal the file values. |
| 4 | `tools/capture_viewer.sh --face 70405 --frames 8` (no clip = rest pose = the anim T-pose) | log passes the GPU check; **open the last frame and look**: one connected figure, arms dead horizontal, palms down, textured, lit, normal-mapped, no floating pieces, no doubled shell (AGENTS T-pose health check) |
| 5 | `PYTHONDONTWRITEBYTECODE=1 python3 tools/dev/parity_gani.py --gf-tools "$GF/tools/pes21_import" --pes21-root "$PES21_ROOT"` | every 88th mtar entry (by archive order): per-frame quats within 1e-6 of GF `gani.py`; all 4,389 pass `validation()`; name index resolves 4,389/4,389 and equals the set in GF's `anim_names.txt` |
| 5 | `tools/capture_viewer.sh --face 70405 --clip sliding_0_0_y01_000 --frames 90 --in-place` (if that name is absent, pick the first `sliding_` name from `pesfmt_dump` and record it) | frames 1, 45, 90 examined: the model visibly moves through a slide, feet meet the ground, arms not corkscrewed |
| 6 | `build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 1000 --minutes 20 --pes21-root "$PES21_ROOT"` twice | both runs print identical `[balance]` lines and identical `[determinism]` hash; `Full match complete`; exit 0 |
| 6 | same with `--seed 1001` | different `[determinism]` hash |
| 6 | `grep -rn '#include.*SDL\|GetTime_ms()\|HumanController' src/sim; grep -rnE '\b(rand\|srand\|time)\(' src/sim/onthepitch src/sim/data src/sim/base` | no output |
| all | `ctest --test-dir build --output-on-failure` and `bash tools/check_pii.sh` | green / exit 0 before every commit |

Every frame claim is checked by opening the PNG. A step whose frame was not examined is not done. Subagent work is accepted only after its diff is read in full and these commands are re-run by the reviewer.

---

## Assumptions & contingencies

- **Commit identity**: the owner's global git identity (owner-approved). Upstream copyright header lines in copied GF files (the original author's name) stay verbatim as licence attribution; if the owner wants those removed too, strip them in one commit and note it in NOTICE.
- **GF stock animations**: the 293 tracked GF clips are carried as the fallback library (owner choice). PES clips replace them family by family in the next plan.
- **Deterministic clock**: the owner chose it. If removing a wall-clock site changes a gameplay timing visibly (e.g. a restart or celebration hold that was paced in real ms), keep the duration in ms but count it in sim ticks — never reintroduce `GetTime_ms()`.
- **`hand_anim_skel.ask`** is not used; fingers come from the proven GF derivation (render-bind finger offsets rotated by `W(hand)`). If `parity_rig.py` shows the derivation and that file agree within 1 mm, switching later is a free simplification.
- **4K textures**: `_4K_` Data cpks mount after their base so they win. If any model renders with a mismatched normal/base pair (4K base, non-4K normal), mount 4K before base instead and note it.
- **fmdl texture references**: string paths are assumed. If a face.fpk's fmdl carries only hashed texture references, stop Step 4, record the format finding in the commit message, and add a `PathCode64` index (port GF `strcode.gz_hash`) as its own sub-step before continuing.
- **SRM channel layout** is unverified; only the channel-assignment constants change if the first rendered material looks wrong.
- **Face id 70405** is this install's test subject, passed on the command line only — never a default in code. If it renders broken after the T-pose check passes, try `70101` (also present in `4cc_40_faces.cpk`) to separate a model defect from a loader defect.
- **GDExtension library key**: if `linux.debug.x86_64` does not load under the editor binary, add `linux.editor.x86_64` → same file.
