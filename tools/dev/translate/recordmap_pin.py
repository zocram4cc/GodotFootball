#!/usr/bin/env python3
"""RecordMap pin: check record identities and offsets of the port chains against live rendezvous captures.

Usage: python3 tools/dev/translate/recordmap_pin.py --live DIR [--out RESULT.json] [--max N]
  DIR: a PES pid folder of rendezvous captures (seqNNN/{index.json,call.bin,mem.bin}).

Each check reads memory of one capture (the entry state of a real call, via full_snapshot_rv.SnapMem) and
records PASS/FAIL. A check is an oracle: a wrong record identity or a wrong offset fails against live bytes.
G is the match object: the image global GLOBAL_G holds its pointer (the analyzer's IMG.1436F3DC8 root).
Output contains PES-derived values: keep it in scratch, never commit it.
"""
import argparse
import glob
import json
import math
import os
import struct
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "rendezvous"))

from full_snapshot_rv import SnapMem as Snapshot  # noqa: E402

GPR_ORDER = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
             "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
CALL_REC = struct.Struct("<49Q")  # 16 GPRs, 16 XMM (lo, hi), return address (rendezvous entry_record)

GLOBAL_G = 0x1436F3DC8             # image global: pointer to the match object G
IMG_LO, IMG_HI = 0x140000000, 0x160000000  # PES image (code, rodata, data) virtual range
G_ANIME, G_MAN, G_SLOT = 0x19C8, 0x1DD8, 0x25F8   # G's record tables (GF pesworldrecord.hpp; 0x28 entries)
G_SIDE_A, G_SIDE_B = 0x1928, 0x1978
G_ENTRY = 0x28
MAN_OFF, SLOT_OFF = 0x32C, 0x10F0  # one slot block: anime +0, man +0x32C, slot +0x10F0
CARRIER_TABLE_STRIDE = 0x74        # G+0x180 table entry stride (EndClear2 port doc)
BLOCK = 0x3F40                    # one slot block (GF pesworldrecord.hpp kPlayerBlockBytes; PES stride)
RING_COUNT_OFF, RING_HEAD_OFF = 0x1900, 0x1904  # E0 ring: u32 count (saturates at 8), u32 head (0..7)
UNIFORM_BLOCKS = 21  # measured: G anime entries 0..20 are one 0x3F40 block each in every capture; 21..25 are not (OPEN)
UNIFORM_SLOTS = 20   # measured: G slot table entries 0..19 sit at anime+0x10F0; entry 20 and up do not (OPEN)


def load_call(snap_dir):
    raw = open(os.path.join(snap_dir, "call.bin"), "rb").read()
    vals = CALL_REC.unpack(raw)
    return dict(zip(GPR_ORDER, vals[:16]))


class Checks:
    def __init__(self):
        self.res = defaultdict(lambda: {"pass": 0, "fail": 0, "ex": []})

    def ok(self, name, cond, detail=""):
        r = self.res[name]
        r["pass" if cond else "fail"] += 1
        if len(r["ex"]) < 4:
            r["ex"].append(detail)
        return cond

    def note(self, name, detail):
        r = self.res[name]
        if len(r["ex"]) < 6:
            r["ex"].append(detail)


def num(S, va, fmt):
    n = struct.calcsize(fmt)
    b = S.read(va, n)
    return struct.unpack(fmt, b)[0] if b else None


def in_image(p):
    return IMG_LO <= p < IMG_HI


def table_entries(S, G, base, count):
    return [S.u64(G + base + G_ENTRY * k) for k in range(count)]


def run(live_dir, max_caps):
    caps = defaultdict(list)
    for d in sorted(glob.glob(os.path.join(live_dir, "seq*"))):
        if not os.path.exists(os.path.join(d, "call.bin")):
            continue
        hook = json.load(open(os.path.join(d, "index.json")))["rv"]["hook"]
        caps[hook].append(d)
    chk = Checks()
    summary = {"hooks": {h: len(v) for h, v in caps.items()}}

    for hook in ("kick_scatter_83CC30", "kick_postscale_8414A0"):
        for d in caps.get(hook, [])[:max_caps]:
            S = Snapshot(d)
            tag = os.path.basename(d)
            G = S.u64(GLOBAL_G)
            chk.ok("GLOBAL_G holds a readable G object", G != 0 and S.read(G, 0x2E58) is not None,
                   "%s G=%x" % (tag, G))
            P = load_call(d)["rcx"]
            anime = table_entries(S, G, G_ANIME, 26)
            man = table_entries(S, G, G_MAN, 26)
            slot = table_entries(S, G, G_SLOT, 22)
            ka = next((k for k, v in enumerate(anime) if v == P), None)
            # Block layout of the uniform prefix: entries 0..UNIFORM_BLOCKS-1 stride 0x3F40 (one slot block),
            # man = anime + 0x32C, slot = anime + 0x10F0. Entries from UNIFORM_BLOCKS on are irregular (OPEN).
            live_anime = [(k, v) for k, v in enumerate(anime[:UNIFORM_BLOCKS]) if v]
            chk.ok("G anime table 0..%d: consecutive entries differ by one block (0x3F40)" % (UNIFORM_BLOCKS - 1),
                   len(live_anime) == UNIFORM_BLOCKS
                   and all(b[1] - a[1] == BLOCK for a, b in zip(live_anime, live_anime[1:])),
                   "%s strides=%s" % (tag, sorted({b[1] - a[1] for a, b in zip(live_anime, live_anime[1:])})[:4]))
            chk.ok("G man table entry k = anime entry k + 0x32C (k < %d)" % UNIFORM_BLOCKS,
                   all(man[k] == v + MAN_OFF for k, v in live_anime), tag)
            chk.ok("G slot table entry k = anime entry k + 0x10F0 (k < %d)" % UNIFORM_SLOTS,
                   all(slot[k] == v + SLOT_OFF for k, v in live_anime if k < UNIFORM_SLOTS), tag)
            chk.note("G slot table entries from %d on (irregular: value minus anime entry)" % UNIFORM_SLOTS,
                     "%s %s" % (tag, [hex(slot[k] - anime[k]) for k in range(UNIFORM_SLOTS, 22)]))
            chk.note("G anime table entries from %d on (irregular: value, delta from entry 0)" % UNIFORM_BLOCKS,
                     "%s %s" % (tag, [(hex(v), hex(v - anime[0])) for v in anime[UNIFORM_BLOCKS:UNIFORM_BLOCKS + 3]]))
            chk.note("P vs G anime table (nearest entry, signed delta)",
                     "%s P-anime[k] for k=0..5: %s" % (tag, [hex(P - v) if v else None for v in anime[:6]]))
            chk.ok("P (rcx) is the anime record of an anime-table slot (block start)", ka is not None,
                   "%s P=%x k=%s" % (tag, P, ka))
            if ka is not None:
                chk.ok("P+0x32C is the man record of the same slot (G+0x1DD8[k])",
                       man[ka] == P + MAN_OFF, "%s k=%d" % (tag, ka))
                chk.ok("P+0x10F0 is the slot record of the same slot (G+0x25F8[k])",
                       ka < 22 and slot[ka] == P + SLOT_OFF, "%s k=%d" % (tag, ka))
            else:
                # Identify P by the record tables: which table entry, at which offset, sits nearest below P.
                near = []
                for name, tab in (("anime", anime), ("man", man), ("slot", slot)):
                    for k, v in enumerate(tab):
                        if v and 0 <= P - v < 0x4000:
                            near.append((name, k, P - v))
                chk.note("P inside a G-table record (table, slot, P-record)", "%s %s" % (tag, near[:6]))
            chk.ok("P readable to +0x32E8 (kick fields reach 0x32E4)", S.read(P, 0x32E8) is not None, tag)
            s16v = num(S, P + 0x2752, "<h")
            chk.ok("P+0x2752 s16 in [-1,150] (kick table index)", s16v is not None and -1 <= s16v <= 150,
                   "%s v=%s" % (tag, s16v))
            for off in (0x2758, 0x2764, 0x2768, 0x281C, 0x2BC8, 0x2BD0, 0x2BF8):
                v = num(S, P + off, "<f")
                chk.ok("P+%#x f32 finite (kick scatter)" % off, v is not None and math.isfinite(v),
                       "%s v=%s" % (tag, v))
            ab8 = S.u64(P + 0xAB8)
            chk.ok("P+0xAB8 is an image-resident interface object", in_image(ab8), "%s p=%x" % (tag, ab8))
            vt = S.u64(ab8) if ab8 else 0
            chk.ok("[P+0xAB8] vtable in PES image", in_image(vt), "%s vt=%x" % (tag, vt))
            chk.ok("interface object readable to +0x30", S.read(ab8, 0x30) is not None, tag)
            chk.ok("P+0x32E4 int readable", S.read(P + 0x32E4, 4) is not None, tag)

    for d in caps.get("carrier_clear_2109EB0", [])[:max_caps]:
        S = Snapshot(d)
        G = S.u64(GLOBAL_G)
        U = load_call(d)["r8"]
        tag = os.path.basename(d)
        cr = S.u64(U)
        chk.ok("unit+0 (carrier record) is a readable pointer", cr != 0 and S.read(cr, 4) is not None,
               "%s cr=%x" % (tag, cr))
        g40 = S.u64(G + 0x40)
        chk.note("G+0x40 pointee +0 u32 and unit+0 record (equal?)",
                 "%s g40=%x u32=%s equal=%s" % (tag, g40, num(S, g40, "<I"), g40 == cr))
        cid = num(S, cr, "<I") if cr else None
        chk.ok("carrier record +0 u32 is a carrier id in 0..21", cid is not None and 0 <= cid <= 21,
               "%s id=%s" % (tag, cid))
        if cid is not None and 0 <= cid <= 21:
            tabs = {"anime": table_entries(S, G, G_ANIME, 26), "man": table_entries(S, G, G_MAN, 26),
                    "slot": table_entries(S, G, G_SLOT, 22), "sideA": table_entries(S, G, G_SIDE_A, 2),
                    "sideB": table_entries(S, G, G_SIDE_B, 2)}
            hits = [name for name, t in tabs.items() if cid < len(t) and t[cid] == cr]
            chk.note("carrier record (+0 u32 = id) is a G table entry at [id]", "%s id=%d hits=%s cr-anime=%s"
                     % (tag, cid, hits, [hex(cr - v) if v else None for v in tabs["anime"][:3]]))
            chk.ok("carrier record is the G table entry [id] of some table", len(hits) > 0,
                   "%s id=%d hits=%s" % (tag, cid, hits))
        ctx = S.u64(U + 0x8)
        chk.ok("unit+8 (ctx) is a readable pointer", ctx != 0 and S.read(ctx, 8) is not None, tag)
        ia = S.u64(U + 0x28)
        chk.ok("unit+0x28 (imageA) is a readable pointer", ia != 0 and S.read(ia, 0x31C2) is not None,
               "%s ia=%x" % (tag, ia))
        dz = num(S, ia + 0x31C1, "<B") if ia else None
        chk.ok("imageA+0x31C1 danger byte is 0 or 1", dz in (0, 1), "%s v=%s" % (tag, dz))

    for d in caps.get("clear_angle_2108630", [])[:max_caps]:
        S = Snapshot(d)
        G = S.u64(GLOBAL_G)
        r = load_call(d)
        ctx = r["rcx"]
        cid = r["rdx"] & 0xFFFFFFFF
        side = r["r8"] & 0xFF
        tag = os.path.basename(d)
        chk.ok("angle edx carrier id in 0..21", cid <= 21, "%s id=%d" % (tag, cid))
        chk.ok("angle r8b side in 0..1", side in (0, 1), "%s side=%d" % (tag, side))
        a18 = S.u64(ctx + 0x18)
        chk.ok("ctx+0x18 readable to +0x2601C+4 (A0.18 extent)",
               a18 != 0 and S.read(a18, 0x26020) is not None, "%s a18=%x" % (tag, a18))
        e0 = S.u64(ctx + 0xE0)
        chk.ok("ctx+0xE0 readable to +0x180698+4 (A0.E0 extent)",
               e0 != 0 and S.read(e0, 0x18069C) is not None, "%s e0=%x" % (tag, e0))
        t180 = S.u64(G + 0x180)
        ent = t180 + CARRIER_TABLE_STRIDE * cid
        chk.ok("G+0x180 table readable at [id] (stride 0x74, bytes to +0x6E)",
               t180 != 0 and S.read(ent, 0x6E) is not None, "%s t=%x" % (tag, t180))
        chk.note("G+0x180[id] bytes +0x6B,+0x6D", "%s %s %s" % (tag, num(S, ent + 0x6B, "<B"),
                                                              num(S, ent + 0x6D, "<B")))
        p40 = S.u64(G + 0x40)
        chk.ok("G+0x40 carrier record pointer readable (to +0x2954)",
               p40 != 0 and S.read(p40, 0x2954) is not None, "%s p40=%x" % (tag, p40))
        ring_count = num(S, e0 + RING_COUNT_OFF, "<I") if e0 else None
        ring_head = num(S, e0 + RING_HEAD_OFF, "<I") if e0 else None
        chk.ok("E0 ring count (E0+0x1900) in 0..8 (saturating ring of 8)",
               ring_count is not None and 0 <= ring_count <= 8, "%s count=%s" % (tag, ring_count))
        chk.ok("E0 ring head (E0+0x1904) in 0..7", ring_head is not None and 0 <= ring_head <= 7,
               "%s head=%s" % (tag, ring_head))

    summary["checks"] = {k: v for k, v in sorted(chk.res.items())}
    return summary


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--live", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max", type=int, default=6)
    args = ap.parse_args(argv)
    summary = run(args.live, args.max)
    with open(args.out, "w", encoding="utf8") as f:
        json.dump(summary, f, indent=1)
        f.write("\n")
    print("hooks:", {k: v for k, v in summary["hooks"].items() if "kick" in k or "clear" in k or "carrier" in k})
    for name, v in summary["checks"].items():
        print("%-78s pass %3d fail %3d" % (name, v["pass"], v["fail"]))
        for ex in v["ex"][:2]:
            print("      ", ex)


if __name__ == "__main__":
    main()
