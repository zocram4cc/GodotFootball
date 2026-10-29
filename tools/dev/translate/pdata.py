#!/usr/bin/env python3
"""pdata.py: function starts and ends from the image's .pdata (the exception directory's RUNTIME_FUNCTION table).

The one place the tools learn where a function ends without the Ghidra bridge. A function with no .pdata entry (a
leaf or a thunk: x64 needs no unwind data for a frameless function) still has a start; its end is the next .pdata start
at most. A decode or a closure walk never crosses into the next .pdata function, and never admits an address that is
not a function boundary (an address inside a .pdata function other than its start, or one not preceded by int3 padding).
"""
import bisect
import struct

IMAGE_BASE = 0x140000000
INT3 = 0xCC


class Pdata:
    def __init__(self, img):
        """img: a binary file object over the flat image (file offset = VA - IMAGE_BASE)."""
        self.img = img
        img.seek(0x3C)
        lfa = struct.unpack("<I", img.read(4))[0]
        img.seek(lfa)
        if img.read(4) != b"PE\0\0":
            raise SystemExit("pdata: not a PE image")
        _, nsec, _, _, _, optsz, _ = struct.unpack("<HHIIIHH", img.read(20))
        opt = img.read(optsz)
        exc_rva, exc_size = struct.unpack_from("<II", opt, 112 + 8 * 3)
        self.ends = {}
        for i in range(0, exc_size - 11, 12):
            begin, end, _unwind = struct.unpack("<III", self.read(IMAGE_BASE + exc_rva + i, 12))
            self.ends.setdefault(IMAGE_BASE + begin, IMAGE_BASE + end)
        self.starts = sorted(self.ends)

    def read(self, va, n):
        self.img.seek(va - IMAGE_BASE)
        return self.img.read(n)

    def end_of(self, va):
        """The end of the .pdata function that starts at va (None if va is not a function start)."""
        return self.ends.get(va)

    def next_start(self, va):
        """The first .pdata function start strictly above va (None past the last)."""
        i = bisect.bisect_right(self.starts, va)
        return self.starts[i] if i < len(self.starts) else None

    def inside(self, va):
        """True if va lies strictly inside a .pdata function (after its start, before its end)."""
        i = bisect.bisect_right(self.starts, va) - 1
        return i >= 0 and self.starts[i] < va < self.ends[self.starts[i]]

    def is_boundary(self, va):
        """True if va can start a function: a .pdata start, or an address outside every .pdata function that follows
        int3 padding (a leaf or a thunk). Anything else (inside a function, or mid-instruction) is not a callee."""
        if va in self.ends:
            return True
        return not self.inside(va) and self.read(va - 1, 1)[0] == INT3
