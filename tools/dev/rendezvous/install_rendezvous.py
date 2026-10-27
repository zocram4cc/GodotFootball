#!/usr/bin/env python3
"""Install or undo the rendezvous prototype in a game install, through sider's Lua module loader.

Default: PLAN ONLY. It prints every write it would make, with the sha256 of each input, and writes
nothing. --apply performs the plan; --undo reverses an applied install.

The plan:
  <root>/rendezvous/rv.dll          copied from --dll
  <root>/rendezvous/hooks.cfg       copied from --cfg
  <root>/rendezvous/run/            created (the DLL writes its capture/release/done files here)
  <root>/modules/rendezvous.lua     copied from --module
  <root>/sider.ini                  one line `lua.module = "rendezvous.lua"` inserted after the last
                                    existing lua.module line (same line ending as the file)
  <root>/sider.ini.rendezvous-bak   the sider.ini as it was before --apply (removed by --undo)
  <root>/rendezvous/install.json    pre/post sha256 and the inserted line (what --undo needs)

Undo: if sider.ini still has the post-install hash, it is restored byte-for-byte from the backup and
its sha256 is checked against the pre-install hash. If it was edited since, only the inserted line is
removed (refused if that line is not found exactly once). The installed files are then removed.

usage: install_rendezvous.py --root DIR --dll FILE --cfg FILE --module FILE [--apply | --undo]
"""
import argparse
import hashlib
import json
import os
import shutil
import sys

MODULE_NAME = "rendezvous.lua"
INSERT_LINE = 'lua.module = "%s"' % MODULE_NAME


def sha256(path=None, data=None):
    if data is None:
        with open(path, "rb") as f:
            data = f.read()
    return hashlib.sha256(data).hexdigest()


def split_lines(data):
    """Lines of an ini file with their terminators kept; also the line ending it uses."""
    eol = b"\r\n" if b"\r\n" in data else b"\n"
    return data.split(eol), eol


def insert_after_last_module(data):
    """(new bytes, line index inserted at): inserts INSERT_LINE after the last lua.module line."""
    lines, eol = split_lines(data)
    last = None
    for i, raw in enumerate(lines):
        if raw.strip().lower().startswith(b"lua.module"):
            last = i
    if last is None:
        raise SystemExit("sider.ini has no lua.module line to insert after")
    if any(raw.strip() == INSERT_LINE.encode() for raw in lines):
        raise SystemExit("sider.ini already has %r: refusing to add it twice" % INSERT_LINE)
    lines.insert(last + 1, INSERT_LINE.encode())
    return eol.join(lines), last + 1


def remove_inserted(data, expect_index=None):
    """(new bytes) with exactly one INSERT_LINE removed; refused unless it occurs exactly once."""
    lines, eol = split_lines(data)
    hits = [i for i, raw in enumerate(lines) if raw.strip() == INSERT_LINE.encode()]
    if len(hits) != 1:
        raise SystemExit("expected exactly one %r in sider.ini, found %d: not removing" % (INSERT_LINE, len(hits)))
    del lines[hits[0]]
    return eol.join(lines)


def plan(args):
    ini_path = os.path.join(args.root, "sider.ini")
    ini = open(ini_path, "rb").read()
    new_ini, idx = insert_after_last_module(ini)
    dest = os.path.join(args.root, "rendezvous")
    steps = [
        ("copy", args.dll, os.path.join(dest, "rv.dll"), sha256(args.dll)),
        ("copy", args.cfg, os.path.join(dest, "hooks.cfg"), sha256(args.cfg)),
        ("mkdir", None, os.path.join(dest, "run"), None),
        ("copy", args.module, os.path.join(args.root, "modules", MODULE_NAME), sha256(args.module)),
        ("backup", ini_path, ini_path + ".rendezvous-bak", sha256(data=ini)),
        ("edit", ini_path, "insert line %d: %s" % (idx, INSERT_LINE), sha256(data=new_ini)),
        ("write", None, os.path.join(dest, "install.json"), None),
    ]
    return steps, sha256(data=ini), sha256(data=new_ini), new_ini


def print_plan(steps, pre, post):
    print("PLAN (no writes unless --apply):")
    for kind, src, dst, digest in steps:
        extra = "  sha256=%s" % digest if digest else ""
        print("  %-6s %s%s" % (kind, dst, ("  <- %s" % src) if src else "") + extra)
    print("  sider.ini sha256 before: %s" % pre)
    print("  sider.ini sha256 after:  %s" % post)


def apply(args):
    steps, pre, post, new_ini = plan(args)
    dest = os.path.join(args.root, "rendezvous")
    if os.path.exists(dest):
        raise SystemExit("%s exists: already installed or left over; undo first" % dest)
    ini_path = os.path.join(args.root, "sider.ini")
    mod_dst = os.path.join(args.root, "modules", MODULE_NAME)
    if os.path.exists(mod_dst):
        raise SystemExit("%s exists: refusing to overwrite" % mod_dst)
    print_plan(steps, pre, post)
    shutil.copyfile(ini_path, ini_path + ".rendezvous-bak")
    if sha256(ini_path + ".rendezvous-bak") != pre:
        raise SystemExit("backup does not match the original sider.ini: stopping before any edit")
    os.makedirs(os.path.join(dest, "run"))
    shutil.copyfile(args.dll, os.path.join(dest, "rv.dll"))
    shutil.copyfile(args.cfg, os.path.join(dest, "hooks.cfg"))
    created_modules = not os.path.isdir(os.path.dirname(mod_dst))
    os.makedirs(os.path.dirname(mod_dst), exist_ok=True)
    shutil.copyfile(args.module, mod_dst)
    with open(ini_path, "wb") as f:
        f.write(new_ini)
    if sha256(ini_path) != post:
        raise SystemExit("sider.ini does not hash to the planned post-install value")
    record = {"pre_sha256": pre, "post_sha256": post, "inserted": INSERT_LINE,
              "created_modules_dir": created_modules}
    with open(os.path.join(dest, "install.json"), "w") as f:
        json.dump(record, f, indent=1)
    print("applied: %s" % json.dumps(record))
    return 0


def undo(args):
    dest = os.path.join(args.root, "rendezvous")
    rec_path = os.path.join(dest, "install.json")
    if not os.path.exists(rec_path):
        raise SystemExit("no %s: nothing installed by this script here" % rec_path)
    rec = json.load(open(rec_path))
    ini_path = os.path.join(args.root, "sider.ini")
    bak = ini_path + ".rendezvous-bak"
    cur = sha256(ini_path)
    if cur == rec["post_sha256"]:
        if not os.path.exists(bak) or sha256(bak) != rec["pre_sha256"]:
            raise SystemExit("backup missing or not the pre-install file: not restoring")
        shutil.copyfile(bak, ini_path)
        how = "restored from backup"
    else:
        with open(ini_path, "rb") as f:
            new = remove_inserted(f.read())
        with open(ini_path, "wb") as f:
            f.write(new)
        how = "removed the inserted line (sider.ini was edited since install)"
    if sha256(ini_path) != rec["pre_sha256"]:
        print("note: sider.ini sha256 %s differs from the pre-install %s: %s" % (
            sha256(ini_path), rec["pre_sha256"], how))
    for p in (os.path.join(dest, "rv.dll"), os.path.join(dest, "hooks.cfg"), rec_path,
              os.path.join(args.root, "modules", MODULE_NAME)):
        if os.path.exists(p):
            os.remove(p)
    if os.path.isdir(os.path.join(dest, "run")):
        os.rmdir(os.path.join(dest, "run"))
    mods = os.path.join(args.root, "modules")
    if rec.get("created_modules_dir") and os.path.isdir(mods) and not os.listdir(mods):
        os.rmdir(mods)
    if os.path.isdir(dest):
        os.rmdir(dest)
    if os.path.exists(bak) and sha256(ini_path) == rec["pre_sha256"]:
        os.remove(bak)
    kept = " (backup kept at %s: sider.ini was not restored)" % bak if os.path.exists(bak) else ""
    print("undone: sider.ini %s; sha256 now %s (pre-install %s)%s" % (
        how, sha256(ini_path), rec["pre_sha256"], kept))
    return 0


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, help="the game install directory (contains sider.ini)")
    ap.add_argument("--dll", help="the rv.dll to install (plan and apply)")
    ap.add_argument("--cfg", help="the hooks.cfg to install (plan and apply)")
    ap.add_argument("--module", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), MODULE_NAME))
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--apply", action="store_true")
    g.add_argument("--undo", action="store_true")
    args = ap.parse_args(argv[1:])
    if not args.undo and not (args.dll and args.cfg):
        ap.error("--dll and --cfg are required unless --undo")
    if args.undo:
        return undo(args)
    if args.apply:
        return apply(args)
    steps, pre, post, _ = plan(args)
    print_plan(steps, pre, post)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
