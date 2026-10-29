#!/usr/bin/env python3
"""fdc_parse.py: the FixDemo cut file (.fdc) container and records, for Godot playback.

Layout (docs/re/pes-re-cutscenes.md §3, the loader is FUN_141f36200; every .fdc in the install
parses under this layout, CONFIRMED by the run below):

  u32 n ; n x { u32 offset, u32 size, u32 nameOffset }     (offsets from the file start)
  entry 0 = the cut block (its name is the file's own path)
  entries 1..n-1 = resources, named by path. size == 4: external (read from the VFS, the name
    with its leading 'cpk_dat/' stripped); otherwise embedded bytes.
  cut block: u32 m ; m x { u32 offset, u32 size, u32 typeOffset }  (offsets from the block start)
    record type = the single byte at block + typeOffset.

Every byte of a file must belong to exactly one parsed structure: the header, one name string, one
resource, the cut block's header, one record, or one type byte. `check_coverage` reports the gaps
(bytes no structure claims) and the overlaps (bytes two structures claim). A file with either fails.

Records are decoded to the field layouts of the doc, bounds-checked: a field past the record's
size is left out, and a record whose size differs from its type's nominal size is counted in
`size_variants`. Types with no layout in the doc (Env flags past 0x82, Effect, Function,
HumanControl, the Camera DOF block) are kept as hex and marked `layout: "OPEN"`.

Usage:
  fdc_parse.py --pes21-root DIR [--pesfmt PATH] [--json OUT]   # every cut_data/*.fdc of the VFS
  fdc_parse.py --dir DIR [--json OUT]                          # *.fdc files under DIR
  fdc_parse.py --selftest                                      # demo of the consumed-every-byte check
The --json output holds PES-derived content: write it to scratch, never to the repo.
Exit status 1 if any file fails the coverage check or cannot be parsed.
"""
import argparse
import collections
import concurrent.futures
import glob
import json
import os
import struct
import subprocess
import sys
# pesfmt_dump reads are independent; three in flight keeps the shared machine's share.
CAT_WORKERS = 3

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_PESFMT = os.path.join(REPO, "build", "apps", "pesfmt_dump")
CATS = ("ent", "foul", "goal", "pk", "change", "timeup", "end", "result", "mode")
FIXDEMO = "common/demo/fixdemo"
VFS_PREFIX = "cpk_dat/"

# Record type -> (class name, nominal size in bytes, adder in FUN_141f36200 per docs §3.1).
RECORD_TYPES = {
    0: ("Cut", 64, "FUN_141f26d50"),
    1: ("Env", 156, "FUN_141f3e1f0"),
    2: ("Timing", 92, "FUN_141f3f330"),
    3: ("Effect", 188, "FUN_141f3e050"),
    4: ("Human", 364, "FUN_141f3e830"),
    5: ("Object", 444, "FUN_141f3ecd0"),
    6: ("Camera", 284, "FUN_141f3dd00"),
    7: ("Function", 324, "FUN_141f3e530"),
    8: ("HumanControl", 96, "FUN_141f3e6d0"),
    99: ("Disabled", 444, None),  # editor-disabled, not handled by the loader's switch
}
# Types whose field layout the doc gives. The rest stay raw.
DECODED = {0, 2, 4, 5, 6}
PARTIAL = {1: "light path only; flag bytes from 0x82 are OPEN"}


class FdcError(Exception):
    pass


def _cstr(b, off, n):
    """The string of at most n bytes at off, cut at its NUL. Returns (text, index of the NUL or the window length)."""
    if off < 0 or off > len(b):
        raise FdcError("string at 0x%x past the end" % off)
    raw = b[off:off + n]
    end = raw.find(b"\0")
    text = raw if end < 0 else raw[:end]
    return text.decode("latin1"), (len(raw) if end < 0 else end)


def _fields(r, spec):
    """Read named fields from record bytes r. spec = [(name, off, fmt)] with fmt a struct code, or
    ('str', n) for a char[n]. A field that runs past the record is skipped (bounds-checked)."""
    out = {}
    for name, off, fmt in spec:
        if isinstance(fmt, tuple):
            if off + fmt[1] <= len(r):
                out[name], _ = _cstr(r, off, fmt[1])
            continue
        size = struct.calcsize("<" + fmt)
        if off + size <= len(r):
            vals = [round(v, 4) if isinstance(v, float) else v for v in struct.unpack_from("<" + fmt, r, off)]
            out[name] = vals[0] if len(vals) == 1 else vals
    return out


def _bytes(r, off, n):
    return list(r[off:off + n]) if off + n <= len(r) else None


def decode_record(t, r):
    """Decode one record's bytes r by type. Returns a dict of named fields (layout: DECODED/PARTIAL/OPEN)."""
    if t == 0:
        d = _fields(r, [("id", 0, "I"), ("cutStartTime", 4, "f"), ("minTime", 8, "f"), ("maxTime", 0xC, "f"),
                        ("playSpeed", 0x10, "f"), ("movingDistance", 0x14, "f"), ("cameraPattern", 0x18, "B"),
                        ("offset", 0x19, "B"), ("offsetPos", 0x1C, "fff"), ("offsetRotY", 0x28, "f"),
                        ("offsetFlipX", 0x2C, "B"), ("humanHeightRange", 0x30, "f"),
                        ("humanHeightRangeCenter", 0x34, "f"), ("functionNo", 0x38, "I")])
        d["layout"] = "DECODED"
        return d
    if t == 2:
        d = _fields(r, [("word0", 0, "I"), ("startTime", 4, "f"), ("endTime", 8, "f"), ("targetNo", 0xC, "i"),
                        ("paramNo1", 0x10, "i"), ("paramNo2", 0x14, "i"), ("value", 0x18, "f"),
                        ("actionName", 0x1C, ("str", 64))])
        if "word0" in d:  # INF med in the doc: high 16 bits = type, low 16 = id/order
            d["timingType"], d["timingId"] = d["word0"] >> 16, d["word0"] & 0xFFFF
        d["layout"] = "DECODED"
        return d
    if t == 4:
        d = _fields(r, [("slot", 0, "H"), ("disp", 2, "H"), ("position", 4, "fff"), ("rotY", 0x10, "f"),
                        ("animePath", 0x14, ("str", 128)), ("animeFacePath", 0x94, ("str", 128)),
                        ("animeStartTime", 0x114, "f"), ("correctRateForHeight", 0x11C, "f"),
                        ("functionNo", 0x124, "I"), ("ikTargetPos1", 0x134, "fff"), ("ikTargetPos2", 0x140, "fff"),
                        ("ikOnStart", 0x14C, "ff"), ("ikOnEnd", 0x154, "ff"), ("ikOffStart", 0x15C, "ff"),
                        ("ikOffEnd", 0x164, "ff")])
        for name, off in (("animeFlip_loop_loopResetPos_pad", 0x118), ("offsetType_lod_misc", 0x120),
                          ("ikBone_kind_pairs", 0x128), ("ikNo_bone_pairs", 0x12C), ("ik_misc", 0x130)):
            v = _bytes(r, off, 4)
            if v is not None:
                d[name] = v
        d["layout"] = "DECODED"
        return d
    if t in (5, 99):
        d = _fields(r, [("slot", 0, "H"), ("disp", 2, "H"), ("position", 4, "fff"), ("rotation", 0x10, "ffff"),
                        ("modelPath", 0x20, ("str", 128)), ("skeletonPath", 0xA0, ("str", 128)),
                        ("animePath", 0x120, ("str", 128)), ("animeStartTime", 0x1A0, "f"),
                        ("functionNo_modelType", 0x1AC, "I")])
        for name, off in (("animeLoop_loopResetPos_planeShadow_referStaffTex", 0x1A4),
                          ("supporterTypeTeam_Shirt_Facial_offsetType", 0x1A8),
                          ("parentKind_parentNo_parentPosBone_parentRotBone", 0x1B4)):
            v = _bytes(r, off, 4)
            if v is not None:
                d[name] = v
        d["layout"] = "DECODED"
        return d
    if t == 6:
        d = _fields(r, [("key", 0, "I"), ("startCutTime", 4, "f"), ("originalCameraId", 8, "H"),
                        ("envDataId", 0xA, "H"), ("animePath", 0xC, ("str", 128)), ("animeStartTime", 0x8C, "f"),
                        ("nearClip", 0x90, "f"), ("farClip", 0x94, "f"), ("nearClipScale", 0x98, "f"),
                        ("correctRateForHeight", 0xA0, "f"), ("functionNo", 0xA8, "I"),
                        ("zoomingTimeRate", 0xB4, "I"), ("zoom8", 0xB8, "iiiiiiii")])
        for name, off, n in (("animeTimeEqCutTime_loop_returnTop_pad", 0x9C, 4),
                             ("lensDistortion_autoStandClip_offsetType_pad", 0xA4, 4),
                             ("targetKind_targetNo3", 0xB0, 4), ("selectByRate_flags", 0xD8, 23)):
            v = _bytes(r, off, n)
            if v is not None:
                d[name] = v
        d["layout"] = "DECODED"
        return d
    if t == 1:
        d = _fields(r, [("id", 0, "H"), ("lightPath", 2, ("str", 128))])
        d["flagBytes_from_0x82"] = _bytes(r, 0x82, 39)
        d["layout"] = "PARTIAL"
        return d
    return {"raw": r.hex(), "layout": "OPEN"}


def _claim(claims, lo, hi, who):
    if hi > lo:
        claims.append((lo, hi, who))


def parse(b):
    """Parse one .fdc file's bytes. Returns a dict with resources, records and the coverage report.
    Raises FdcError when a structure points outside the file."""
    if len(b) < 4:
        raise FdcError("shorter than the 4-byte entry count")
    claims = []
    n, = struct.unpack_from("<I", b, 0)
    hdr_end = 4 + 12 * n
    if hdr_end > len(b):
        raise FdcError("entry table (%d entries) past the end" % n)
    _claim(claims, 0, hdr_end, "container table")
    entries = []
    for i in range(n):
        off, size, name_off = struct.unpack_from("<III", b, 4 + 12 * i)
        if off + size > len(b):
            raise FdcError("entry %d region 0x%x+%d past the end" % (i, off, size))
        name, nlen = _cstr(b, name_off, 512)
        if name_off + nlen >= len(b) or b[name_off + nlen] != 0:
            raise FdcError("unterminated name at 0x%x" % name_off)
        _claim(claims, name_off, name_off + nlen + 1, "name %d" % i)
        entries.append((off, size, name))
    if not entries:
        raise FdcError("no entries")
    cut_off, cut_size, cut_name = entries[0]
    resources = []
    for i, (off, size, name) in enumerate(entries[1:], start=1):
        _claim(claims, off, off + size, "resource %d" % i)
        if size == 4:
            ext = struct.unpack_from("<I", b, off)[0]
            resources.append({"name": name, "storage": "external", "vfs_path": name[len(VFS_PREFIX):]
                              if name.startswith(VFS_PREFIX) else name, "flag": ext})
        else:
            resources.append({"name": name, "storage": "embedded", "size": size,
                              "magic": b[off:off + 4].hex()})
    # The cut block: its header, records and one type byte per record.
    base = cut_off
    m, = struct.unpack_from("<I", b, base)
    rec_hdr_end = base + 4 + 12 * m
    if rec_hdr_end > base + cut_size:
        raise FdcError("cut block table (%d records) past its size" % m)
    _claim(claims, base, rec_hdr_end, "cut block table")
    records, size_variants = [], collections.Counter()
    for i in range(m):
        off, size, to = struct.unpack_from("<III", b, base + 4 + 12 * i)
        if to + 1 > cut_size or off + size > cut_size:
            raise FdcError("record %d outside the cut block" % i)
        t = b[base + to]
        _claim(claims, base + to, base + to + 1, "record %d type byte" % i)
        _claim(claims, base + off, base + off + size, "record %d" % i)
        cls = RECORD_TYPES.get(t)
        if cls is None:
            raise FdcError("record %d has unknown type %d" % (i, t))
        nominal = cls[1]
        if size != nominal:
            size_variants["%s:%d" % (cls[0], size)] += 1
        rec = {"index": i, "type": t, "class": cls[0], "size": size}
        rec.update(decode_record(t, b[base + off:base + off + size]))
        records.append(rec)
    # Coverage: every byte of the file claimed by exactly one structure. An unclaimed run that ends at the
    # cut block's end and is all zero is the block's alignment padding (counted apart); any other unclaimed
    # run is a gap.
    owner = [None] * len(b)
    overlaps = []
    for lo, hi, who in claims:
        for k in range(lo, hi):
            if owner[k] is not None:
                overlaps.append((k, owner[k], who))
            owner[k] = who
    block_end = cut_off + cut_size
    # The type pool sits at the end of the block: each record's type byte, with zero bytes between them.
    # Those zero bytes belong to no record; the offsets the table points at are referenced, not padding.
    refs = {base + struct.unpack_from("<III", b, base + 4 + 12 * i)[2] for i in range(m)}
    pool_start = min(refs) if refs else block_end
    pad_bytes, gap_list = 0, []
    for k in range(len(b)):
        if owner[k] is not None:
            continue
        if k not in refs and pool_start <= k < block_end and b[k] == 0:
            pad_bytes += 1
        else:
            gap_list.append(k)
    counts = collections.Counter(r["class"] for r in records)
    return {
        "name": cut_name,
        "bytes": len(b),
        "entries": n,
        "resources": resources,
        "records": records,
        "record_counts": dict(counts),
        "size_variants": dict(size_variants),
        "coverage": {"gap_bytes": len(gap_list), "gap_offsets": gap_list[:8], "pad_bytes": pad_bytes,
                     "overlap_bytes": len(overlaps),
                     "first_overlap": overlaps[0] if overlaps else None},
        "ok": not gap_list and not overlaps,
    }


def _run(cmd):
    p = subprocess.run(cmd, capture_output=True)
    if p.returncode != 0:
        raise SystemExit("%s failed rc=%d: %s" % (" ".join(cmd[:2]), p.returncode,
                                                 p.stderr.decode(errors="replace")[:300]))
    return p.stdout


def vfs_fdcs(pesfmt, root):
    """(path, bytes) for every cut_data/*.fdc in the mounted VFS, every category."""
    listing = []
    for cat in CATS:
        for e in json.loads(_run([pesfmt, "ls", "%s/%s/cut_data/" % (FIXDEMO, cat), "--pes21-root", root])):
            if e["path"].endswith(".fdc"):
                listing.append(e["path"])
    with concurrent.futures.ThreadPoolExecutor(max_workers=CAT_WORKERS) as pool:
        for path, data in zip(listing, pool.map(lambda p: _run([pesfmt, "cat", p, "--pes21-root", root]), listing)):
            yield path, data


def dir_fdcs(d):
    for p in sorted(glob.glob(os.path.join(d, "**", "*.fdc"), recursive=True)):
        with open(p, "rb") as f:
            yield p, f.read()


def selftest():
    """A file built from the layout parses with zero gaps; three stray bytes appended are counted as a gap."""
    rec = bytes(64)
    blk = struct.pack("<I", 1) + struct.pack("<III", 17, 64, 16) + bytes([0]) + rec  # m, table, type byte, record
    head_len = 4 + 12 * 2
    res_off = head_len + len(blk)
    names_off = res_off + 4
    head = (struct.pack("<I", 2) + struct.pack("<III", head_len, len(blk), names_off)
            + struct.pack("<III", res_off, 4, names_off + len(b"a.fdc\0")))
    body = head + blk + b"\0\0\0\0" + b"a.fdc\0" + b"r.gani\0"
    good = parse(body)
    assert good["ok"] and good["coverage"]["gap_bytes"] == 0, good["coverage"]
    assert [r["class"] for r in good["records"]] == ["Cut"], good["records"]
    stray = parse(body + b"\xaa\xaa\xaa")
    assert not stray["ok"] and stray["coverage"]["gap_bytes"] == 3, stray["coverage"]
    print("selftest ok: %d record(s), gap check catches 3 stray bytes" % len(good["records"]))
    return good


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pes21-root")
    ap.add_argument("--pesfmt", default=DEFAULT_PESFMT)
    ap.add_argument("--dir")
    ap.add_argument("--json")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        selftest()
        return 0
    if a.dir:
        it = dir_fdcs(a.dir)
    elif a.pes21_root:
        it = vfs_fdcs(a.pesfmt, a.pes21_root)
    else:
        ap.error("--pes21-root or --dir is required")
    out, total, failed, errors = {}, 0, [], []
    types = collections.Counter()
    gaps = 0
    for path, data in it:
        total += 1
        try:
            r = parse(data)
        except FdcError as e:
            errors.append((path, str(e)))
            continue
        for rec in r["records"]:
            types[rec["class"]] += 1
        gaps += r["coverage"]["gap_bytes"]
        if not r["ok"]:
            failed.append(path)
        out[path] = r
    print("fdc files: %d; parsed: %d; coverage failures: %d; parse errors: %d" % (
        total, len(out), len(failed), len(errors)))
    print("records by class: %s" % dict(sorted(types.items())))
    print("gap bytes over all files: %d" % gaps)
    for p, e in errors[:10]:
        print("ERROR %s: %s" % (p, e))
    for p in failed[:10]:
        print("COVERAGE %s: %s" % (p, out[p]["coverage"]))
    if a.json:
        with open(a.json, "w") as f:
            json.dump(out, f)
    return 1 if (failed or errors) else 0


if __name__ == "__main__":
    sys.exit(main())
