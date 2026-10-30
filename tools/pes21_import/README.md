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

## A match needs the image

`PES_EXE` is a required input of every GF match, not only of the translation tests. The Trap action's kind-0 chain
(`0x14079A470`, run from `ElizaController::_ReactionCommands`) is translated code that reads the game's code, `.rdata`, `.data`
and the data around them as the running game holds them: `PES2021.exe` on disk is packed and its sections differ from the running
game's in thousands of 8-byte words, so the install's file cannot stand in. `Match` maps the dump copy-on-write per match
(`PesTrapMemory`) and refuses a setup without it: `gfsim_run` exits with "PES_EXE ... is not set", the gdext library fails to
load, and the `Match` tests skip. Without a dump, make one with `dump_image.py` above, then run:

```bash
export PES_EXE=/path/to/pes_full.bin PES21_ROOT=/path/to/PES2021
build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --pes21-root "$PES21_ROOT"
```

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
