#!/usr/bin/env python3
"""gen_oracle.py: write one registry set's oracle directory: manifest.json naming each capture by its real folder, mem file and
call file. No symlinks: a reader opens the real paths, and a packed capture is named as packed (capz.py unpack restores it first).

Usage: gen_oracle.py --set NAME --out DIR [--registry PATH]
Exit 1 when a capture folder, call.bin or index.json is missing; nothing is written then.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import registry  # noqa: E402


def manifest_of(reg, name):
    """The manifest dict for set `name`. Raises RegistryError for an unknown set or a missing capture."""
    matches = [s for s in reg["sets"] if s["name"] == name]
    if not matches:
        raise registry.RegistryError("no set named %r" % name)
    s = matches[0]
    if s["status"] == "gone":
        raise registry.RegistryError("set %r is marked gone: no captures on disk to generate from" % name)
    captures, missing = [], []
    for q in s["seqs"]:
        d = os.path.join(reg["live2"], s["launch"], q)
        call, index = os.path.join(d, "call.bin"), os.path.join(d, "index.json")
        mem = os.path.join(d, "mem.bin")
        if not (os.path.isdir(d) and os.path.exists(call) and os.path.exists(index)):
            missing.append(d)
            continue
        packed = not os.path.exists(mem) and os.path.exists(mem + ".zst")
        if not packed and not os.path.exists(mem):
            missing.append(d)
            continue
        captures.append({"seq": q, "dir": d, "mem": mem + ".zst" if packed else mem, "packed": packed,
                         "call": call, "index": index})
    if missing:
        raise registry.RegistryError("set %r: %d captures missing: %s" % (name, len(missing), ", ".join(missing)))
    return {"set": name, "hook": s["hook"], "launch": s["launch"], "status": s["status"], "owner": s["owner"],
            "doc": s["doc"], "captures": captures}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--set", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--registry", default=None)
    a = ap.parse_args(argv)
    try:
        manifest = manifest_of(registry.load(a.registry), a.set)
    except registry.RegistryError as e:
        print("gen_oracle: %s" % e, file=sys.stderr)
        return 1
    target = os.path.join(a.out, "manifest.json")
    if os.path.exists(target):
        print("gen_oracle: %s exists; remove it first" % target, file=sys.stderr)
        return 1
    os.makedirs(a.out, exist_ok=True)
    with open(target, "w") as f:
        json.dump(manifest, f, indent=1)
        f.write("\n")
    print("gen_oracle: %d captures -> %s" % (len(manifest["captures"]), target))
    return 0


if __name__ == "__main__":
    sys.exit(main())
