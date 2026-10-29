#!/usr/bin/env python3
"""test_closure_region.py: closure.py reaches the game code in `.impdata` (VA >= 0x143D14000), which has no bridge body.

A `.trace` function tail-jumps (a 5-byte JMP rel32 thunk) into `.impdata`. The bridge's program ends before it, so the
ledger holds those targets as unresolved rows (size 0, no callees), and many of them have no .pdata entry (leaves).
closure.py decodes them from the image: a clean no-.pdata leaf gets its extent and a row, an unresolved universe row is
replaced, a VM entry or an opaque-return (mutated) body is refused by name. A clean no-.pdata leaf below `.impdata` (a
`.trace` jmp thunk island) is taken the same way (Main ruling 29-10-26); the interior of a .pdata function never is.
Needs capstone (the harness Python). Exit 0 green, 1 red, 77 skip.

Optional oracle against the real image: PES_EXE=pes_full.bin, and PES_CAPTURE=<full capture dir> to compare the same
bytes with a live process's.
"""
import json
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
try:
    import closure  # noqa: E402
except ImportError:
    print("SKIP: capstone not importable (use the harness Python)")
    sys.exit(77)

IB = closure.IMAGE_BASE
LO = closure.IMPDATA_LO
# function RVA -> bytes
PDATA_FN = LO - IB + 0x000  # in .impdata, has a .pdata entry: a tail JMP to LEAF
LEAF = LO - IB + 0x100      # clean leaf: mov byte [rcx+0x44],7 ; ret
BRANCHY = LO - IB + 0x200   # test ecx,ecx ; je +1 ; ret ; xor eax,eax ; ret   (the first RET is inside the branch span)
VM = LO - IB + 0x300        # pushfq ; ret
MUTATED = LO - IB + 0x400   # xchg [rsp],rax ; ret
TRACE_LEAF = 0x1100         # below .impdata, no .pdata entry
BODIES = {
    LEAF: bytes.fromhex("c6414407c3"),
    BRANCHY: bytes.fromhex("85c97401c331c0c3"),
    VM: bytes.fromhex("9cc3"),
    MUTATED: bytes.fromhex("48870424c3"),
    TRACE_LEAF: bytes.fromhex("c3"),
}
PDATA_AT = 0x1000


def make_image(path):
    size = LO - IB + 0x1000
    with open(path, "wb") as f:
        f.truncate(size)
        f.seek(0)
        hdr = bytearray(0x400)
        e, opt = 0x80, 0x98
        struct.pack_into("<I", hdr, 0x3C, e)
        hdr[e:e + 4] = b"PE\0\0"
        struct.pack_into("<H", hdr, e + 20, 240)
        struct.pack_into("<II", hdr, opt + 112 + 8 * 3, PDATA_AT, 12)  # exception directory: one entry
        f.write(hdr)
        f.seek(PDATA_AT)
        f.write(struct.pack("<III", PDATA_FN, PDATA_FN + 5, 0))
        jmp = struct.pack("<Bi", 0xE9, LEAF - (PDATA_FN + 5))
        f.seek(PDATA_FN)
        f.write(jmp)
        for rva, code in BODIES.items():
            f.seek(rva)
            f.write(code)


def run(pd, universe, root):
    try:
        members, _ = closure.crawl([root], universe, pd, "t")
        return members, None
    except SystemExit as e:
        return None, str(e)


def main():
    d = tempfile.mkdtemp(dir=HERE, prefix=".closure_region_")
    path = os.path.join(d, "image.bin")
    make_image(path)
    results = []
    try:
        with open(path, "rb") as img:
            pd = closure.Pdata(img)
            va = lambda rva: IB + rva  # noqa: E731
            unresolved = {"%08X" % va(LEAF): {"unresolved": True, "protected": True, "size": 0, "callees": {}}}
            members, err = run(pd, unresolved, va(PDATA_FN))
            row = unresolved.get("%08X" % va(LEAF), {})
            results.append(("tail JMP from a .pdata function reaches the no-.pdata leaf",
                            members == [va(PDATA_FN), va(LEAF)] and err is None))
            results.append(("the bridge's unresolved row is replaced by the decoded one",
                            row.get("unresolved") is False and row.get("size") == 2 and row.get("source") == "image-leaf"))
            results.append(("the tail callee of the .pdata function is recorded as 'tail'",
                            unresolved["%08X" % va(PDATA_FN)]["callees"] == {"%08X" % va(LEAF): "tail"}))
            results.append(("a RET inside a forward branch's span does not end the leaf",
                            pd.end_of(va(BRANCHY)) == va(BRANCHY) + 8))
            _, err = run(pd, {}, va(VM))
            results.append(("a VM entry is refused by name", err is not None and "VM entry" in err))
            _, err = run(pd, {}, va(MUTATED))
            results.append(("an opaque-return (mutated) body is refused by name", err is not None and "opaque return" in err))
            members, err = run(pd, {}, va(TRACE_LEAF))
            results.append(("below .impdata a clean no-.pdata leaf is taken too (Main ruling 29-10-26: any code VA)",
                            members == [va(TRACE_LEAF)] and err is None))
            _, err = run(pd, {}, va(PDATA_FN) + 1)
            results.append(("the interior of a .pdata function is refused by name, not decoded as a leaf",
                            err is not None and "interior of the .pdata function" in err))
            # a resolved row is never replaced: only an unresolved one in .impdata is
            kept = {"%08X" % va(LEAF): {"unresolved": False, "size": 99, "callees": {}}}
            run(pd, kept, va(LEAF))
            results.append(("a resolved row is kept", kept["%08X" % va(LEAF)]["size"] == 99))
        results += real_image_checks()
    finally:
        for f in os.listdir(d):
            os.remove(os.path.join(d, f))
        os.rmdir(d)
    for name, ok in results:
        print(("ok   " if ok else "FAIL ") + name)
    return 0 if all(ok for _, ok in results) else 1


def real_image_checks():
    """The game image (PES_EXE): every no-.pdata thunk target in .impdata that closure.py takes decodes to a RET-ended
    body; with PES_CAPTURE, the body is byte-identical to the live process's."""
    exe = os.environ.get("PES_EXE")
    if not exe:
        return []
    with open(exe, "rb") as img:
        pd = closure.Pdata(img)
        thunks = []
        for p in range(0x1000, 0x252E800 - 5, 16):  # .trace: the 16-aligned `jmp rel32` thunks into .impdata
            img.seek(p)
            b = img.read(5)
            if b[0] == 0xE9:
                t = IB + p + 5 + struct.unpack("<i", b[1:])[0]
                if t >= LO:
                    thunks.append(t)
        leaves = [t for t in thunks if t not in pd.ends and pd.end_of(t) is not None]
        row = pd.decode_row(leaves[0], "t") if leaves else None
        out = [("the image has no-.pdata thunk targets in .impdata that decode as clean leaves", len(leaves) > 100),
               ("a decoded image leaf ends in RET and has a row",
                row is not None and row["size"] > 0 and pd._read(pd.end_of(leaves[0]) - 1, 1) == b"\xc3")]
    cap = os.environ.get("PES_CAPTURE")
    if cap and leaves:
        idx = json.load(open(os.path.join(cap, "index.json")))
        m = next(m for m in idx["mappings"] if m["va"] == LO)
        with open(exe, "rb") as img, open(os.path.join(cap, "mem.bin"), "rb") as mem:
            pd = closure.Pdata(img)
            same = 0
            for t in leaves:
                n = pd.end_of(t) - t
                same += os.pread(mem.fileno(), n, m["off"] + t - LO) == pd._read(t, n)
        out.append(("capture and image carry the same bytes for every clean leaf (%d of %d)" % (same, len(leaves)),
                    same == len(leaves)))
    return out


if __name__ == "__main__":
    sys.exit(main())
