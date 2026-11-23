# CutLane3c (addendum to CutLane3b): static writer search for G+0x1978 and the run+0x18 writer. No code change.

Labels: CONFIRMED = read this session (scripts in <workdrive>/gf-CutLane-scratch: storescan.py, storescan2.py; outputs scan_out.txt, scan2_out.txt). INF = inference. OPEN = blocker.
Image: flat (file offset = RVA = VA - 0x140000000). Code = section `.trace` (RVA 0x1000..0x252E800). Functions from `.pdata` (156,556 entries). Scan = capstone x86-64 over each function body.

## 1. G+0x1978 (the table pointer 0x2DD5A0D8)

- CONFIRMED values: G = 0x2DB0C420 (1436F3DC8 -> ...). The block G+0x1958..0x19A0 is identical in seq203, seq204, seq205, seq206:
  - +0x1960 = 0x1425968E8 (a pointer to ASCII data, not a vtable; bytes at that VA read as text)
  - +0x1968 = 0x3F8000003C6AEC01 (floats 1.0 and 0.0092)
  - +0x1970 = 0x2DD5A0A0 (heap object; vtable 0x142596848 at +0, count 12 at +8, pointer 0x2DD5A0D0 at +0x10)
  - +0x1978 = 0x2DD5A0D8 (heap table: u32 array, values 6, 0xB, 4, 3, 2, 5, 7, 8, 9, 0xA, 0xF, 0x10, 0xD, 0xE, 1, 0x13, 0x11, 0x12, 0x16, 0x14, 0x15, 0xC, 0x17, 0x18, ... a permutation of small ids)
  - +0x1988 = 0x1425968E8, +0x1990 = -0xFF, +0x1998 = 0x2DD781B0, +0x19A0 = 0x2DD781E8.
  So the pointer is set once, before seq203, and never changes across the captures we have. It is not written by the goal tick.
- CONFIRMED static stores with displacement 0x1978 (scan over every .pdata function; exact disp or any store whose [disp, disp+size) covers 0x1978): 11 sites in 9 functions.
  - qword stores (pointer-sized): 0x14089B780 (`mov [rcx+0x1978], rax`, a zeroing init of the camera block 0x1958..0x1998; rax = 0), 0x14C2D60 (`mov [rdi+0x1978], rax`, ctor, decompile sets `param_1[0x32f]`... it zeroes at line 54 of dec_1978.txt), 0x14C3FA0 (`mov [rbx+0x1978], rax`, zeroes at lines 284/299 of dec_1978.txt), 0x18F22D0 (`mov [rdi+0x1978], rsi`, constructor, decompile shows `param_1[0x32f] = 0`).
  - dword stores (not the pointer): 0x5140E0, 0x91EDB0, 0x91FA80 (x3), 0xA35DE0.
  - compare: 0x928527 `cmp dword [r15+0x1978], 4`. LEA of 0x1978: 0x18F3071 (`lea r15, [rbx+0x1978]`), a reader.
- CONFIRMED: none of these stores 0x2DD5A0D8. All qword writers are zeroing constructors or the camera init. So the pointer is NOT written by a direct displacement store.
- INF: it is written by a block copy or an `add reg, 0x1978` followed by a store through that reg, neither of which the scan catches. A copy would have shown as a wide store covering 0x1978 (movups at 0x1970, etc.); the scan found none, so the likely route is the `add`/immediate-address form or a `rep movs` / memcpy of a struct (not inspected).
- OPEN: the writer. Next static step: scan for `add reg, 0x1978` / `sub reg, -0x1978` immediates and for `rep movs*` or memcpy calls whose destination is the G world block; or find who builds the G world (the boot constructor that the camera ctor 0x14089B780 runs under, DEMOINFO/match world ctor 0x140441490 per CutLane2-r2). The world ctor is the most likely writer of the pointer (it runs once and sets the block).

## 2. run+0x18 (the slot-19 run object 0x2DDCCFF4)

- CONFIRMED (CutLane2-r2): run+0x18 = (-37.56, 0, 40.56) at seq203, (-41.6211, 0, 35.56) at seq204, mirroring A0+0x30.
- CONFIRMED scan: functions that contain a load/store with displacement 0x21E8 (the G+0x21E8 run table) AND a store with displacement 0x18: one function, 0x142057E60 (4 sites `mov [rax+0x18], r8` at 0x142057EF9, 0x142057F50, 0x14205 80F9, 0x142058149). Its decompile (dec_1978.txt lines 348-501) is a large constructor that zeroes fields (`_DAT_1437f9580 = 10` and a loop of zero stores). It is NOT the run+0x18 writer by reading: its +0x18 stores are in the ctor's zeroing, not the celebration spot.
- INF: the run+0x18 writer is not a displacement-0x18 store through a register loaded from the 0x21E8 table in the same function. The write of A0+0x30 into run+0x18 likely happens in a function that gets the run pointer from a parameter (not from G+0x21E8), so the 0x21E8 filter misses it. Next: the store scan filtering on writes of 3 floats (movss/movlps/movq) with displacement 0x18 in functions whose callers pass the run object, or follow the run pointer from the tick 0x14047B460 (its r9 object, param_4, is the run-like object: it holds +0x1bc..+0x2230).
- OPEN.

## 3. What this gives the port
- The tick's r9 (param_4 in dec_7b460.txt) and the recompute's r9 are the objects the stand-in replaces with a zeroed object. The G+0x1978 table is read by 0x140A489D0 and the event-struct chain (CutLane3b section 2). The pointer's writer is still open; the table 0x2DD5A0D8 is the first thing to dump for its layout (u32 ids, count not yet decoded).
- No translation, no commit this round. Tests unchanged (last green: select 8/8, table 7/7, anchor 25/26 with the wide test RED).
