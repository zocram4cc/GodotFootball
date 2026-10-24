# pes-port-ImageDump: reproducible PES 2021 image + verifier

Branch `ImageDump` (head `1f55457`, confirmed by `ls-remote`): the image is now
one script from the user's running install, with a hash check of the right image.

## What landed

1. **`tools/pes21_import/dump_image.py`** (port of the scratch script, cleaned):
   finds `PES2021.exe` via `pgrep -x`, reads the image range read-only through
   `/proc/<pid>/mem` (O_RDONLY; never writes/pauses the game), writes the flat
   dump (offset = VA − 0x140000000) plus `.maps`, refuses to overwrite without
   `--force`. End address from the PE section table in process memory
   (max section end, page-rounded → `0x15b97c000`, same as the hand dump);
   falls back to `SizeOfImage`. Also fixed a COFF parse bug while porting
   (`NumberOfSections` is at `coff+2`, not `coff+4` — the scratch script never
   parsed it). Live-tested: dumped pid 2510857 → `pes_new.bin`, 15 maps,
   0 unreadable, byte-identical size (462,929,920).
2. **Stability (§2 table below):** second dump into
   `<workdrive>/gf-ImageDump-scratch/` (same live process, 11 days and
   different game states apart from `pes_full.bin`), compared per section and
   per 4 KB page.
3. **Verification:** `tools/pes21_import/image_hashes.json` (SHA-256 of the 15
   stable sections; unstable 4 presence-only) + `tools/pes21_import/verify_image.py`
   (exit 77 skip without `--image`/`PES_EXE`; exit 1 naming differing sections).
   Wired as ctest `diff_image_verify` in `tests/pilot/CMakeLists.txt` (runs when
   `PES_EXE` set; skips 77 without — the gate already reds on skipped diff tests
   when `PES_EXE` is set). Mutation-tested: flipped one `.pdata` byte → FAIL
   names `.pdata`, rc=1. `verify_image: OK` on both dumps, rc=0.
4. **Writable pages (§4 table below).** 5. **Docs:** `tools/pes21_import/README.md`
   (make the image / check it / interoperability basis).
- PII: `tools/check_pii.sh` from the main checkout passes (rc=0); changed files
  carry no machine paths. Gate not run (Main gates). PilotFix coordination: they
  keep `tests/pilot/CMakeLists.txt` untouched — my one appended test block
  confirmed compatible.

## §2 per-section stability (pes_full.bin 13-10-26 vs pes_new.bin 24-10-26)

| section | VA range | diff pages | diff bytes | verdict |
|---|---|---|---|---|
| .trace (code) | 0x140001000+0x252d800 | 1/9518 | 3 | 1 self-patched site (0x140447e79: 3 code bytes) |
| .rdata | 0x14252f000+0xe6c000 | 1/3692 | 8 | 1 self-patched site (0x142647c64: 8 bytes, pointer-shaped) |
| .data | 0x14339b000+0x72a000 | 138/1834 | 51,142 | live globals (see §4) |
| .pdata | 0x143ac5000+0x1d1000 | 0/465 | 0 | STABLE |
| .xtext/.tls/.bss/.data1 | 0x143c96000..0x143d14000 | 0 | 0 | STABLE (all four) |
| .impdata | 0x143d14000+0x17c3d94f | 15/97342 | 40 | 15 one-byte self-patched sites scattered over the range |
| .reloc/.xcode/.rsrc/.text1/.xtls/.idata/.sxdata/.sbss/.tls$/.edata | 0x15b952000..0x15b97c000 | 0 | 0 | STABLE (all ten) |

So: **15/19 sections byte-identical**; `.data` live; `.trace`/`.rdata`/`.impdata`
carry a handful of self-modified bytes (3/8/40). Hashes cover the 15 stable ones.

## §4 .data-dependence of the pilot specs

Specs read image globals two ways: `patch_addrs` (patch one qword at the VA to
a synthetic region = the spec seeds that object from the image) and `IMG.*`
region inputs (141 distinct field VAs across the 252 specs).

- `patch_addrs` VAs (3, all in .data): `0x1436F3DC8` (66 specs),
  `0x1437F4808` (2 specs: 1409FBCE0, 141E5D4C0), `0x143704E38` (1 spec: 140A55CF0).
  **All three qwords differ between the dumps** (live pointers/handles).
- `IMG.*` fields: 141 distinct VAs; **131 identical, 10 differ**:
  `0x1436F3DC8` (+0x0), `0x1436F4038` (+0x270), `0x1436F5030` (+0x180+0x10E8),
  `0x1436F51D0` (+0x270+0x1198), `0x1436F9DDC` (+0x1928+0x46EC, f32),
  `0x1436FE4F8`/`0x1436FE520` (+0x1928+0x8E08/8E30), `0x1437023AA`/`0x1437023B2`
  (+0xE1EE+0x3F4/3FC), `0x1437066A2` (+0xE1EE+0x46EC, f32).
  The +0x270/+0x1928/+0xE1EE sub-objects are otherwise stable page-wise; the
  differing fields are single words (handles/counters), not bulk state.
- `.data` diff mass concentrates at `0x1436EB000–0x14375B000`,
  `0x1437CB000`, `0x1437EB000–0x14380B000` (up to 28.7% of a 64 KB zone);
  the pilot globals sit inside the first band (diff runs `0x1436ED000–0x143708000`).

## Input to the next decision

The pilot's `IMG.*` reads are **not** dump-moment-independent: 10/141 seeded
fields (plus all 3 patch qwords) differ between two dumps of the same install.
Options: (a) a defined dump moment (specs record the moment; verifier pins it),
or (b) specs that model those ~13 globals as inputs (they already do — the
harness overwrites them per-case with generated values, so the dump's bytes
there are only defaults). Note (b) is nearly the status quo: the differing
fields are all case-varied inputs, never fixed constants. Recommending (b) with
a dump-moment note in the README as backstop; PilotFix owns the specs.

## Left

- `diff_image_verify` runs in the gate only once Main merges (not run here).
- `.trace`/`.rdata`/`.impdata` self-patches are uncharacterized (which patcher);
  hashes treat them as unstable, no action needed unless the gate wants them pinned.
- Scratch kept at `<workdrive>/gf-ImageDump-scratch/` (pes_new.bin + compare scripts).


## Main's change at landing

The verifier skipped `.trace` (the code section) and `.rdata` entirely because each has one self-patched site, so it verified none of the bytes the translator reads. Main added per-entry masks (`0x140447e79`+3, `0x142647c64`+8, zeroed before hashing); both dumps verify with those two sections hashed. Only `.data` and `.impdata` remain presence-only.
