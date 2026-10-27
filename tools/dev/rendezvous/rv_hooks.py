#!/usr/bin/env python3
"""Hook-config maker for rv.dll (rendezvous prototype).

usage: rv_hooks.py IMAGE OUT.cfg [--flat] [--allow-refused] SPEC [SPEC ...]

  SPEC: NAME=0xVA:FIRST:EVERY   a hook at a virtual address (FIRST, EVERY: sampling, see rv.c)
        NAME=SYMBOL:FIRST:EVERY a hook at a symbol (PE with a symbol table, via nm)
  --flat  IMAGE is an unpacked flat image: file offset = VA - ImageBase (the pes_full.bin layout).
          Without it, IMAGE is a PE file and VAs map through its section table.

For each SPEC the entry must be a function start the patch can cover: the shortest run of whole
instructions at the entry that is >= 14 bytes (the jmp [rip+0] patch). A run containing a branch,
call, return or rip-relative operand is REFUSED: the displaced bytes are copied, not relocated. Each
hook is reported PASS or REFUSE with the reason. Only PASS hooks go into OUT.cfg:
  hook NAME VA LEN FIRST EVERY EXPECTED_HEX
Exit status 1 when any hook is refused (unless --allow-refused).
"""
import mmap
import struct
import subprocess
import sys

import capstone

NM = "x86_64-w64-mingw32-nm"


class Image:
    """Reads bytes at virtual addresses of a PE image: flat (offset = VA - base) or PE-sectioned."""

    def __init__(self, path, flat):
        self.f = open(path, "rb")
        self.data = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        e_lfanew = struct.unpack_from("<I", self.data, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.data, e_lfanew + 6)[0]
        opt_size = struct.unpack_from("<H", self.data, e_lfanew + 20)[0]
        opt = e_lfanew + 24
        if struct.unpack_from("<H", self.data, opt)[0] != 0x20B:
            raise SystemExit("not PE32+: %s" % path)
        self.base = struct.unpack_from("<Q", self.data, opt + 24)[0]
        self.flat = flat
        self.secs = []
        sec0 = opt + opt_size
        for i in range(nsec):
            s = sec0 + 40 * i
            vsize, va, rsize, roff = struct.unpack_from("<IIII", self.data, s + 8)
            self.secs.append((self.base + va, roff, min(rsize, vsize) if vsize else rsize))
        self.exc = struct.unpack_from("<II", self.data, opt + 112 + 3 * 8)

    def read(self, va, n):
        if self.flat:
            off = va - self.base
            if off < 0 or off + n > len(self.data):
                raise ValueError("VA %#x is outside the flat image" % va)
            return bytes(self.data[off:off + n])
        for sva, roff, size in self.secs:
            if sva <= va and va + n <= sva + size:
                return bytes(self.data[roff + (va - sva): roff + (va - sva) + n])
        raise ValueError("VA %#x is not in a section" % va)


def symbol_va(exe, name):
    out = subprocess.check_output([NM, exe]).decode()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    raise SystemExit("symbol not found: %s" % name)


def displaced_window(code, va, min_len=14):
    """(length, None) for the run of whole instructions that covers min_len bytes, else (None, reason)."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    total = 0
    for ins in md.disasm(code, va):
        if ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL) \
                or ins.group(capstone.CS_GRP_RET):
            return None, "branch at %#x (%s)" % (ins.address, ins.mnemonic)
        if "rip" in ins.op_str:
            return None, "rip-relative operand at %#x (%s)" % (ins.address, ins.mnemonic)
        total += ins.size
        if total >= min_len:
            return total, None
    return None, "cannot cover %d bytes with whole decodable instructions" % min_len


def main(argv):
    flat = "--flat" in argv
    allow = "--allow-refused" in argv
    args = [a for a in argv[1:] if not a.startswith("--")]
    if len(args) < 3:
        raise SystemExit(__doc__)
    image_path, out = args[0], args[1]
    img = Image(image_path, flat)
    lines, refused = ["# rv.dll config from %s (flat=%s)" % (image_path, flat)], 0
    for spec in args[2:]:
        name, rest = spec.split("=", 1)
        target, first, every = rest.split(":")
        va = int(target, 16) if target.lower().startswith("0x") else symbol_va(image_path, target)
        code = img.read(va, 64)
        length, why = displaced_window(code, va)
        if why:
            refused += 1
            print("REFUSE %-22s %#x  %s" % (name, va, why))
            if not allow:
                continue
            continue
        lines.append("hook %s %#x %d %s %s %s" % (name, va, length, first, every, code[:length].hex()))
        print("PASS   %-22s %#x  len=%d" % (name, va, length))
    open(out, "w").write("\n".join(lines) + "\n")
    print("wrote %s: %d hook(s), %d refused" % (out, len(lines) - 1, refused))
    return 1 if refused and not allow else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
