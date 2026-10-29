# pes21_import: the PES 2021 image

GF's translated code is generated at build time from an image the user's own
install produces. The image is a read-only dump of the running game's memory
(`tools/pes21_import/dump_image.py`), not a decryption of the file on disk.

## Making the image

1. Start PES 2021 (the `PES2021.exe` process) and reach the main menu.
2. Dump it (needs read access to the game's memory):
   `sudo -n python3 tools/pes21_import/dump_image.py /path/to/pes_full.bin`
   This reads `/proc/<pid>/mem` read-only, never writes to the game, and
   writes the flat dump (file offset = VA − 0x140000000) plus a `.maps` file.
   It refuses to overwrite an existing dump without `--force`.
3. Point the build at it: `export PES_EXE=/path/to/pes_full.bin`
   (with `GF_DIFF_PYTHON` set, as `tools/dev/diffharness/README.md` says).

## Checking the image

`python3 tools/pes21_import/verify_image.py --image $PES_EXE` hashes every
stable section against `tools/pes21_import/image_hashes.json` (SHA-256 hashes
only, no PES bytes) and names any section that differs. The `diff_image_verify`
ctest runs it whenever `PES_EXE` is set, and the gate fails on a mismatch.

## Interoperability basis

GF reads the user's own running copy for interoperability: the format of the
dump (flat, offset = VA − 0x140000000) is GF's, and the hashes pin the image
the pilot specs were authored against. No PES bytes ship in this repo;
everything PES-derived regenerates from the user's install by script.

## `.impdata`: code at 0x143D14000..0x15B952000 is in the image

The dump carries the whole `.impdata` section (the protector's section, which holds 2,273 relocated game functions
reached through `jmp rel32` thunks in `.trace`). It is plain code, byte-identical in `PES2021.exe`, the dump and every
full capture except a few state words (31 pages in all). The Ghidra bridge's program ends at 0x143D13FFF, so the
bridge cannot decompile it (`thunk_EXT_FUN_144...`); the image can be decoded, and `closure.py` does so (a function
with no `.pdata` entry gets its extent from a linear decode; a VM entry or opaque-return body is refused by name).
A slim capture drops these pages; `full_snapshot.py` serves them from the image except `IMPDATA_VOLATILE_RVA`, the
31 pages that differ in any full capture. After a new dump or build, regenerate that constant:
`python3 tools/pes21_import/volatile_pages.py --image $PES_EXE FULL_CAPTURE_DIR [FULL_CAPTURE_DIR ...]`.
