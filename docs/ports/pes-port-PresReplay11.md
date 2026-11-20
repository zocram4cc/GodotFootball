# pes-port-PresReplay11: no translated writer of the object's +8; the install's .data tail comes from the packer (round 10)

Branch `Presentation`, last pushed `09ff2a631e5a`; no code change this round (investigation only). Copy tests still red. Build dir removed; scratch 22 MB.

Labels: CONFIRMED = gdb watch/breakpoint hit, unicorn-free static sweep, or a file read; INF = inferred.

## 1. Result

- **Every callee of the chain is translated and compiled.** `EE80`, `F230`, `F320`, `AB660`, `AB470`, `E3E0` call only translated functions (`pre_match/` is in the glob, so `AB610` and `F0E0` are real code). No stub on the chain is a no-op (CONFIRMED by listing callees against the layer and `stubs.json`).
- **The object's writers in the port** (hardware watch on the first qwords of the object at boot-window `0x2601018000`, the first creation): only `F230` line 83 (`+8 = 1`, 8 bytes) and line 92 (`+0 = 1`, byte), both after `F320` returns. Nothing else writes the object during the run (CONFIRMED).
- **The value `+8 = 0x375A90` is not from `F320`.** `F320` copies `[0x1436DCED8]` into the new object's `+8`, but that global is zero in the unpacked image (CONFIRMED), and no code in `.text` references `0x1436DCED8` except `F320`'s read (CONFIRMED, static xref sweep). So `F320` writes 0 there in PES too.
- **The first creation's stores are not in the translated code.** No translated function stores `0x375A90`, `0x100000000` or the object addresses (grep of the layer, earlier round).

## 2. The loader-produced state (the real gap; INF on the mechanism, CONFIRMED on the data)

- The install is packed. Its section table: `.trace` has 0x252D800 virtual bytes for 0x600 raw; `.data` has 0x72A000 virtual bytes for 0x334800 raw (CONFIRMED from the PE headers).
- The entry point is `0x15B95F020` (`.sbss`), a page-touch stub that ends in `jmp 0x15B95E000` (CONFIRMED by disassembly), i.e. into the packer's real unpacker, which runs before any game code.
- The unpacked image holds **46,397 non-zero qwords in the `.data` tail** past the raw size (`0x1436CF800..0x143AC5000`) (CONFIRMED). GF reads that whole tail as zero: the port's `.data` segment is the raw section only (`ReadSection`). That is why every runtime-initialised static the copy path reads (`0x143AC28xx`, `0x143AC3D98`, the TLS index, `0x14350B340` cookie, ...) is zero in GF.
- INF: the unpacker writes that tail (its decompression target). The unpacked dump is the state after it.

## 3. What this means for the blocker

- The copy path's static reads (the size-class table `0x143AC28E0`, the object chain `0x143AC3D98`, the list anchors) come from the unpacker's output, not from any translated boot function. The static init the port runs (`EE80`, `AEF50`, `B07D0`) writes parts of the same tail, but it starts from zeros where PES starts from the unpacked values.
- Fixing it means running the packer's startup (its code at `0x15B95E000` and the entry stub) in the same way PES's own boot code is run: from the install's raw sections, to produce the `.data` tail, then the port's boot. That is a sub-project (the packer is not a plain loop; it has imports and a decompressor), and I have not started it.

## 4. Decision for Main

1. Ruling needed: emulate the packer startup (large; PES's own code; the faithful route), or
2. Generate the post-unpack `.data` tail at layer-generation time from the unpacked image (as the translator already bakes closure constants), which needs an exception to the "no PES number tables" rule, or
3. Stop the copy tests here and mark the copy path blocked on the packer.

I recommend (1); (2) is the fallback if the packer proves too complex. No hooks needed.
