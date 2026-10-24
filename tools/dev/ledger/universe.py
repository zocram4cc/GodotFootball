#!/usr/bin/env python3
"""Match universe: a BFS over callees from the match's roots, via the Ghidra bridge.

One request at a time. Writes tools/dev/ledger/universe.json (committed data).
Re-crawl only when a root, a rule or the image changes; the gate does NOT run this (no bridge in gate).

Usage: python3 tools/dev/ledger/universe.py --image PES_IMAGE [--out PATH] [--max N]   (run it with a python that has capstone)
  --image  the PES image dump (file offset = VA - 0x140000000). Read for the RTTI vtable scan, the
           vtable and jump-table rules, and the capstone decode of bodies the bridge does not list
           (that venv has capstone). Without it those rules are off (meta.image = false).

Scope (owner ruling 24-10-26): in scope is everything a player goes through to play a match, from the
pre-match select flow through the match, its presentation and its in-match menus. Out of scope is only
edit mode, career modes (mode::offline), save/load and online (OUT_NS, out_rule). Two crawls share the image:
  in   roots: SEEDS (doc-cited functions) and the vtables vt_root_tag() names by RTTI class name. A call
       through an out-of-scope vtable is not expanded here (out_rule() classes are out-of-scope roots).
  out  roots: the code slots of every vtable out_rule() names. Walked the same way.
A row is in scope if the in crawl reaches it, out of scope if only the out crawl does. Rows that neither
crawl reaches are not in the universe.

Subsystem tag (row): the tag of the nearest root (BFS distance, ties by root order). A root's tag comes
from its doc cite (how = doc) or from its RTTI class name (how = rtti). A boundary row whose RTTI owner is
draw or gr is tagged render (how = owner). Out-of-scope rows carry "out: <category>".

Per-row rules (meta.rules counts them; docs/re/LEDGER.md states them):
  body      the body is the instructions reachable from the entry by control flow (fall-through,
            direct jumps and branches; calls return). The bridge lists Ghidra's whole body range,
            which runs over neighbouring functions (FUN_1403B9780: 12,705 listed lines, a getter).
  capstone  a body the bridge does not list ("No function found") is decoded from the image bytes
            with capstone, up to the next known function start (IMAGE_CAP at most), in the bridge's format.
  library   a Ghidra symbol that is not FUN_/thunk_/caseD_/... names CRT, STL, zlib, libxml or Winsock code. -> engine
  doc       a VA that a report names as the engine side of a call (phrase, then the VA on the same
            line, ENGINE_PHRASE over docs/re/*.md).                                              -> engine
  rtti      a vtable a walked function references has an RTTI owner (COL -> TypeDescriptor) in an
            engine namespace (ENGINE_ROOTS) or a template. Its code slots are engine.           -> engine
  crt       a FUN_ function inside the address span of the CRT's named symbols.                 -> engine
  table     an indirect jump through a relative table (byte-index map and RVA table, read from the
            image at the VAs in the code) takes the table's targets.
  vtable    every code slot of a vtable a walked function references is a callee (in the in crawl,
            not when the vtable is out of scope).
  leaf      <= LEAF_MAX_INSNS instructions, no call, no jump out, no indirect or table jump, no
            vtable reference.                                                                  -> leaf accessor
Boundary rows are recorded, not walked: their callees are not expanded.
Engine namespaces are ENGINE_ROOTS. menu and flow are not among them (owner ruling 24-10-26: in-match menus and HUD are in scope).
"""
import functools
import json
import mmap
import os
import re
import struct
import sys
import time
import urllib.request
from bisect import bisect_right
from collections import Counter, deque

BRIDGE = "http://127.0.0.1:8080"
IMAGE_BASE = 0x140000000
CODE_LO, CODE_HI = 0x140001000, 0x14252E800      # .trace: code
RDATA_LO, RDATA_HI = 0x14252F000, 0x14339B000    # .rdata: vtables, COLs, TypeDescriptors
IMPDATA_LO = 0x143D14000                         # protected import area: no body on the bridge
LEAF_MAX_INSNS = 10
MAX_INSNS = 20000
IMAGE_CAP = 0x2000                               # capstone decode limit for a body the bridge does not list
ENGINE_ROOTS = {"fox", "cobra", "KS", "gr", "draw", "std", "collision", "Concurrency"}
NOT_LIBRARY = ("FUN_", "thunk_", "caseD_", "switchD_", "sub_", "LAB_", "DAT_")
ENGINE_PHRASE = re.compile(r"engine call is|\*\*Engine\*\*|Engine side|engine side|Engine plugin|"
                           r"Engine \(OBS|engine-side|BodyAnimControl::|AnimMovePlugin|AnimSyncPlugin|"
                           r"cobra::|fox::")
VA_TOKEN = re.compile(r"(?:0x|FUN_|sub_)(14[0-9A-Fa-f]{7})(?![0-9A-Fa-f])")
INSN = re.compile(r"^([0-9a-fA-F]+):\s*(.*?)\s*$")
DIRECT = re.compile(r"^0x([0-9a-fA-F]+)$")
DREF = re.compile(r"(?<![0-9A-Fa-f])0x(14[0-9A-Fa-f]{7})(?![0-9A-Fa-f])")
IND_JMP = re.compile(r"^JMP\s+R\w+$")
LEA_BASE = re.compile(r"LEA\s+R\w+,\s*\[0x140000000\]")
TBL_BM = re.compile(r"MOVZX\s+\w+,\s*byte ptr \[R\w+ \+ R\w+\*0x1 \+ 0x([0-9a-fA-F]+)\]")
TBL_RT = re.compile(r"MOV\s+\w+,\s*dword ptr \[R\w+ \+ R\w+\*0x4 \+ 0x([0-9a-fA-F]+)\]")
CMP_IMM = re.compile(r"CMP\s+E\w+,\s*0x([0-9a-fA-F]+)")
RIP_REL = re.compile(r"\[rip ([+-]) 0x([0-9a-fA-F]+)\]")
SCALE = re.compile(r"\*(\d)\b")
REGS = ({"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "rip", "eax", "ebx", "ecx", "edx",
         "esi", "edi", "ebp", "esp", "ax", "bx", "cx", "dx", "si", "di", "bp", "sp", "al", "bl", "cl",
         "dl", "ah", "bh", "ch", "dh", "sil", "dil", "bpl", "spl"}
        | {"r%d%s" % (i, s) for i in range(8, 16) for s in ("", "d", "w", "b")}
        | {"xmm%d" % i for i in range(16)} | {"ymm%d" % i for i in range(16)})
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DOCS = os.path.join(REPO, "docs", "re")
DEFAULT_OUT = os.path.join(REPO, "tools", "dev", "ledger", "universe.json")

# Out of scope (owner ruling): RTTI namespaces whose classes are edit mode, save/load or online.
OUT_NS = {"edit": "edit mode", "savesystem": "save/load", "savesystembase": "save/load",
          "savedata": "save/load", "onlinesystem": "online", "onlinemode": "online",
          "onlinelogin": "online", "ntl": "online", "kps": "online",
          "commandautomake": "online", "multiplaycommandautomake": "online"}

# Function roots: (va, subsystem, doc cite). The cite is the report that names the VA; a VA marked
# "bridge lists no function" is decoded with capstone from the image.
SEEDS = [
    # AI: team tick, team plans, tactics, decisions
    (0x1404F6C30, "AI", "pes-re-teamtick.md §2 (team tick, CONFIRMED)"),
    (0x14041ECF0, "AI", "pes-re-teamtick.md §2.1 (team-tick owner H slot 2, DATA 0x142597590)"),
    (0x140500B90, "AI", "pes-re-plan-selector.md §1 (in-play plan selector, CONFIRMED)"),
    (0x14051E970, "AI", "pes-re-tactics.md §3 (CPU strategy decider)"),
    (0x1404F8350, "AI", "pes-re-tactics.md §3 (attack level writer, drift)"),
    (0x140579840, "AI", "pes-re-tactics.md §2.1 (concept selector)"),
    (0x1406424A0, "AI", "pes-re-tactics.md §2.1 (concept start)"),
    (0x140A62180, "AI", "pes-re-tactics.md §2 (defensive-shape style gate accessor)"),
    (0x1409F8030, "AI", "pes-re-dead-code.md §4.2 (action switch dispatcher)"),
    (0x1409F8220, "AI", "pes-re-receiving.md (receive reaction entry 3, live-data-root)"),
    (0x140A07E40, "AI", "pes-re-dodge-gates.md (dodge eligibility gate, ported)"),
    (0x140A08340, "AI", "pes-re-dodge-port.md (POST_DODGE predicate)"),
    (0x140A08EA0, "AI", "pes-re-run-targets.md (pass/shot block entry 8)"),
    # gameplay: the match tick, player driver, contacts
    (0x14042B9F0, "gameplay", "pes-re-foundation-matchball.md §H2 (match tick, 10 steps, OBS)"),
    (0x140418940, "gameplay", "pes-re-teamorder.md (listener registrar: MatchMain+MatchSub via FUN_14042fe20)"),
    (0x14042FE20, "gameplay", "pes-re-teamorder.md (listener builder, MatchMain then MatchSub)"),
    (0x140433760, "gameplay", "pes-re-teamorder.md (MatchSub listener, vtable 0x14259b1a8 slot 2)"),
    (0x1404334E0, "gameplay", "pes-re-teamorder.md (MatchSub path to team tick via FUN_140a8eff0)"),
    (0x140A8EFF0, "gameplay", "pes-re-teamorder.md (MatchSub to team-tick owner H)"),
    (0x140925750, "gameplay", "UNRESOLVED in bridge (slot-2 thumb only, DATA 0x1426329b8); seed kept, crawler records the gap"),
    (0x140925470, "gameplay", "pes-re-teamtick.md §5 correction (player driver, CONFIRMED raw)"),
    (0x1408BFCF0, "gameplay", "pes-re-a3helpers.md (per-player owner tick, called at 0x140925793)"),
    (0x1408C48C0, "gameplay", "pes-re-contact.md (PvP contacts, per player per tick)"),
    # ball and carrier
    (0x1421019D0, "ball", "pes-re-A4Carrier.md §11 (BallPlayer tick entry)"),
    (0x14092C9F0, "ball", "pes-re-ball-track.md §8 (carrier-sense, tick call L199)"),
    (0x140876360, "ball", "pes-re-officials-ball.md §0.7 (ball integrator)"),
    (0x140874470, "ball", "pes-re-officials-ball.md §0.7 (acceleration fn)"),
    (0x140874860, "ball", "pes-re-ball-track.md §8 (ball init/reset)"),
    (0x140873EE0, "ball", "pes-re-ball-contact.md (ground/air integrator driver)"),
    (0x140425E20, "ball", "pes-re-ball-collision.md (net in/out state)"),
    (0x140424F70, "ball", "pes-re-ball-collision.md (BallControl slot 4, DATA 0x1425988d0)"),
    # referee
    (0x1404A56C0, "referee", "pes-re-A5Judge.md (judge tick, step 0x3a)"),
    (0x140A38A00, "referee", "pes-re-officials-ball.md EV-SPC-0008 (referee chief, MODE 1)"),
    (0x140A38A70, "referee", "pes-re-officials-ball.md EV-SPC-0008 (referee PK placement)"),
    (0x140A38C40, "referee", "pes-re-dead-code.md §4.3 (referee dispatcher, DATA 0x142642190)"),
    # restart
    (0x14044C880, "restart", "pes-re-foul-choreography.md (situation-sequencer runner)"),
    (0x140445E90, "restart", "pes-re-foul-choreography.md (sequencer caller)"),
    (0x140422030, "restart", "pes-re-restart-step.md (restart-step container, caller-traced)"),
    (0x140423000, "restart", "pes-re-A5Judge.md (event-driven record slot 3, DATA 0x142598570)"),
    (0x140791EC0, "restart", "pes-re-throwin-choreography.md (SeamlessThrowin phases)"),
    (0x140792E10, "restart", "pes-re-restart-step.md (restart release phase)"),
    # animation
    (0x1406E5FC0, "animation", "pes-re-anim-select.md (AnimePlayer Exec, per-tick action virtual)"),
    (0x1407824A0, "animation", "pes-re-anim-select.md (action tick, MOVESTEP caller)"),
    (0x140789DD0, "animation", "pes-re-anim-select.md (class-0x53 gate, MOVESTEP caller)"),
    (0x1407D1E60, "animation", "pes-re-anim-select.md (velocity integrator, MOVESTEP caller)"),
    # camera: in-play (broadcast) camera and the stadium camera
    (0x14087E480, "camera", "pes-re-camera-settings.md §4 (InplayCamera ctor)"),
    (0x1408A1A70, "camera", "pes-re-stadium-camera.md §4.3 (record builder, modes != 3,6)"),
    (0x1408A57F0, "camera", "pes-re-stadium-camera.md §4.3 (stadium table builder, modes 3/6)"),
    (0x14087F4A0, "camera", "pes-re-stadium-camera.md §4.3 (mode-switch blend)"),
    (0x1408A2180, "camera", "pes-re-camera-target.md items 2/3 (target smoothing, raw focus, k spring)"),
    # replay
    (0x14217DEF0, "replay", "pes-re-cutscenes.md §6 (ReplayHuman::FUN_14217def0, frame record read)"),
    # audio and crowd
    (0x14048FDAE, "audio", "pes-re-audio.md §1.2 (ball-frame speed->tier picker, SE_PL_BALLHIT)"),
    (0x14143EB60, "audio", "pes-re-audio.md §4.1 (Se config loader)"),
    (0x14143F7F0, "audio", "pes-re-audio.md §4.1 (Announce config loader)"),
    (0x1414401C0, "audio", "pes-re-audio.md §4.1 (Sequence config loader)"),
    (0x14143F2B0, "crowd", "pes-re-audio.md §4.1 (Supporter config loader)"),
    (0x1414406A0, "crowd", "pes-re-audio.md §4.1 (Chant config loader)"),
    # HUD, scoreboard, overlays
    (0x1420F0EB0, "HUD", "pes-re-hud-ui.md §3.1 (scorebug main update)"),
    (0x1420E2910, "HUD", "pes-re-hud-ui.md §3.3 (radar per-frame update; bridge lists no function)"),
    (0x1420FBF20, "HUD", "pes-re-hud-ui.md §3.9 (tactics / attack-defence banner)"),
    (0x1420EB0C0, "HUD", "pes-re-hud-ui.md §3.11 (replay control bar handler)"),
    (0x1420EFEC0, "HUD", "pes-re-tactics-clock.md §2.6 (HUD clock block builder)"),
    (0x1420F1750, "HUD", "pes-re-hud-ui.md §3.1 (clock digits setter)"),
    (0x140A6BD80, "HUD", "pes-re-tactics-clock.md §2.6 (HMS conversion for the HUD clock)"),
    (0x1420F2F80, "HUD", "pes-re-hud-ui.md §3.7 (goal banner handler)"),
    (0x1420EA620, "HUD", "pes-re-hud-ui.md §3.7 (score demo handler)"),
    # cutscene (fixdemo)
    (0x141F40C00, "cutscene", "pes-re-cutscenes.md §2.2 (parse order T1/T2/T3)"),
    (0x141F4B1D0, "cutscene", "pes-re-cutscenes.md §3/4.2 (decision-tree matcher)"),
    (0x141F4D2F0, "cutscene", "pes-re-cutscenes.md §1/4.2 (condition evaluator)"),
    (0x141F57840, "cutscene", "pes-re-cutscene-slots.md §0 (iterates mgr+0x78 slot entries -> FixDemoHuman)"),
    (0x141F5E930, "cutscene", "pes-re-cutscene-slots.md §0 (FixDemoMatch vtbl idx16 -> tail FUN_141f57840; bridge lists no function)"),
    (0x141F5F330, "cutscene", "pes-re-cutscene-slots.md §3 (slot-list build per category)"),
    (0x140480910, "cutscene", "pes-re-cutscenes.md (foul/card cutscene)"),
    (0x140484A10, "cutscene", "pes-re-cutscenes.md / pes-re-A5Judge.md §0x08 (substitution demo)"),
    (0x140471A80, "cutscene", "pes-re-cutscenes.md (goal sequence variant)"),
    (0x140478190, "cutscene", "pes-re-cutscenes.md §4.3 (goal celebration selection chain)"),
    (0x140488470, "cutscene", "pes-re-cutscenes.md (period-end cutscene)"),
    (0x140486B10, "cutscene", "pes-re-missing-systems.md (period-end full-time cutscene table)"),
    (0x14048AB10, "cutscene", "pes-re-cutscenes.md (end-of-match cutscene)"),
    (0x1404A3FA0, "cutscene", "pes-re-missing-systems.md §2.9a (end-of-match scene-id state machine)"),
    # in-match menus: pause, team, substitution windows; game-plan UI
    (0x14044AF70, "in-match menu", "pes-re-A5Judge.md §0x20 (team-management window)"),
    (0x14044B1D0, "in-match menu", "pes-re-A5Judge.md §0x1d (substitution window)"),
    (0x14044B380, "in-match menu", "pes-re-A5Judge.md §0x1e/0x1f (substitution / throw-in no-kicker sub-windows)"),
    (0x14044BB40, "in-match menu", "pes-re-A5Judge.md §0x16 (substitution / UI window, 10 sub-states)"),
    (0x14044D2E0, "in-match menu", "pes-re-A5Judge.md §0x1a (armed set-play counter, UI)"),
    (0x1408C5030, "in-match menu", "pes-re-tactics.md §1 (binary toggles accessor, game plan)"),
    (0x1408C52E0, "in-match menu", "pes-re-tactics.md §1 (numeric sliders accessor, game plan)"),
    (0x1408C5E20, "in-match menu", "pes-re-plan-selector.md §2 (sheet plan writer, SetPlan)"),
    # pre-match: start plan, kits, settings before kick-off
    (0x141FC1590, "pre-match", "pes-re-plan-selector.md §3 (setup builder, reads the start plan)"),
    (0x1408E16D0, "pre-match", "pes-re-plan-selector.md §3 (start-plan reader)"),
    (0x141EA3190, "pre-match", "pes-kit-assembly.md §0/1/2 (kit config loader)"),
    (0x141EA29C0, "pre-match", "pes-kit-assembly.md §2.1 (missing-kit fallback)"),
    (0x141EA1320, "pre-match", "pes-kit-assembly.md §2 (per-kit loader realUni -> .bin -> edit; bridge lists no function)"),
    (0x141EA54A0, "pre-match", "pes-kit-assembly.md §2 (per-kit loader; bridge lists no function)"),
    (0x141A89120, "pre-match", "pes-kit-assembly.md §3 (assembly function, 15-slot table 0x143517b30; bridge lists no function)"),
    (0x140EA93C0, "pre-match", "pes-re-dead-code.md §5.1 (Match General Settings ball-list query, menu wrapper)"),
]


@functools.lru_cache(maxsize=None)
def owner(td):
    """(class, outermost namespace, next namespace) of a TypeDescriptor name; None for a template or anonymous name."""
    body = td[4:]
    if body.startswith("?"):
        return None
    p = [x for x in body.split("@") if x]
    if not p:
        return None
    return p[0], (p[-1] if len(p) > 1 else ""), (p[-2] if len(p) > 2 else "")


@functools.lru_cache(maxsize=None)
def out_rule(td):
    """(category, why) if the class is out of scope (edit mode, career, save/load, online), else None."""
    o = owner(td)
    if o is None:
        return None
    cls, ns, sub2 = o
    if ns in OUT_NS:
        return OUT_NS[ns], "RTTI %s:: namespace (%s)" % (ns, OUT_NS[ns])
    if ns == "mode" and sub2 == "offline":
        return "career", "RTTI mode::offline:: (offline career modes)"
    if ns == "matchPlan" and cls.startswith("DataManage"):
        return "save/load", "RTTI matchPlan::%s (game-plan preset save/load)" % cls
    return None


def vt_root_tag(td):
    """(subsystem, why) if the class's vtable is an in-scope root by its class name, else None."""
    if td.startswith(".?AVFixDemo"):
        return "cutscene", "RTTI FixDemo* (cutscene demo classes, no namespace)"
    o = owner(td)
    if o is None:
        return None
    cls, ns, sub2 = o
    if ns == "match" and sub2 == "camera":
        return "camera", "RTTI match::camera:: (in-play camera plugin)"
    if ns == "match" and cls.startswith(("Replay", "DataManagerReplay")):
        return "replay", "RTTI match::%s (recorded-state replay)" % cls
    if ns == "matchPlan" and not cls.startswith("DataManage"):
        return "in-match menu", "RTTI matchPlan::%s (game-plan UI)" % cls
    if ns == "menu" and cls.startswith("MatchPause"):
        return "in-match menu", "RTTI menu::%s (pause menu)" % cls
    if ns == "menu" and cls in ("AfterTeamSelect", "MatchGeneralSettings"):
        return "pre-match", "RTTI menu::%s (pre-match flow)" % cls
    if ns == "sound" and "chant" in td.lower():
        return "crowd", "RTTI sound::...chant (crowd chants)"
    return None


def get(path, tries=3, timeout=120):
    for i in range(tries):
        try:
            with urllib.request.urlopen(BRIDGE + path, timeout=timeout) as r:
                return r.read().decode("utf-8", "replace")
        except Exception:
            if i == tries - 1:
                raise
            time.sleep(2)


def disassemble(va):
    return get("/disassemble_function?address=0x%08X" % va, timeout=900)


def load_names():
    """Every function start the bridge knows, named: FUN_ and library symbols alike."""
    names = {}
    for line in get("/list_functions?limit=1000000", timeout=900).splitlines():
        name, sep, addr = line.rpartition(" at ")
        if sep and re.fullmatch(r"[0-9a-fA-F]+", addr):
            names.setdefault(int(addr, 16), name.strip())
    return names


def is_library(name):
    return bool(name) and not name.startswith(NOT_LIBRARY)


def doc_engine_vas():
    """VA -> doc, for VAs that a report names as the engine side of a call (same line, after the phrase)."""
    out = {}
    for fn in sorted(os.listdir(DOCS)):
        if not fn.endswith(".md") or fn == "LEDGER.md":
            continue
        with open(os.path.join(DOCS, fn), encoding="utf-8", errors="replace") as f:
            for line in f:
                for pm in ENGINE_PHRASE.finditer(line):
                    for vm in VA_TOKEN.finditer(line, pm.end()):
                        out.setdefault(int(vm.group(1), 16), fn)
    return out


def crt_span(names):
    lib = sorted(a for a, n in names.items() if is_library(n) and 0x141400000 <= a < 0x141800000)
    return (lib[0], lib[-1]) if lib else None


def parse_listing(txt):
    insns = {}
    for line in txt.splitlines():
        m = INSN.match(line)
        if m:
            insns.setdefault(int(m.group(1), 16), m.group(2))
    return insns


def image_listing(img, va, end):
    """Capstone decode of the image bytes [va, min(end, va + IMAGE_CAP)), in the bridge's listing format."""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    img.seek(va - IMAGE_BASE)
    code = img.read(min(end, va + IMAGE_CAP) - va)
    out = []
    for ins in md.disasm(code, va):
        ops = RIP_REL.sub(lambda mm: "[0x%X]" % (ins.address + ins.size + (
            int(mm.group(2), 16) if mm.group(1) == "+" else -int(mm.group(2), 16))), ins.op_str)
        ops = SCALE.sub(r"*0x\1", ops)
        ops = re.sub(r"[a-z][a-z0-9]*", lambda mm: mm.group(0).upper() if mm.group(0) in REGS else mm.group(0), ops)
        out.append("%x: %s %s" % (ins.address, ins.mnemonic.upper(), ops))
    return "\n".join(out)


def read(img, va, n):
    img.seek(va - IMAGE_BASE)
    return img.read(n)


def jump_tables(insns, order, img):
    """Indirect jump -> table targets, for the relative table shapes read from the image.

    Both shapes take the image base in a register (`LEA Rb,[0x140000000]`), load a dword RVA from
    `[Rb+Ri*4+RT]`, then `ADD R?,Rb; JMP R?`. The index is bounded by the nearest `CMP E?X,imm`
    before the load, so the table has imm+1 entries. Two index forms:
      byte-indexed: `MOVZX E?,byte ptr [Rb+Ri*1+BM]` maps the value to an index into RT
                    (docs/re/pes-re-dead-code.md §4.2, the action switch, 0x1409F8030);
      direct:       the switch value is the index (docs/re/pes-re-runloop-arms.md §4, the
                    MOVESTEP state dispatch at 0x1406BA900, table 0x1406BB50C).
    """
    out = {}
    if img is None or not any(LEA_BASE.search(t) for t in insns.values()):
        return out
    for i, a in enumerate(order):
        if not IND_JMP.match(insns[a]):
            continue
        win = [(order[j], insns[order[j]]) for j in range(max(0, i - 16), i)]
        rt = bm = rt_j = bm_j = None
        for j, (_, t) in enumerate(win):
            m = TBL_RT.search(t)
            if m:
                rt, rt_j = int(m.group(1), 16), j
            m = TBL_BM.search(t)
            if m:
                bm, bm_j = int(m.group(1), 16), j
        if rt is None:
            continue
        cmp_imm = None
        for _, t in reversed(win[:(bm_j if bm is not None else rt_j)]):
            m = CMP_IMM.search(t)
            if m:
                cmp_imm = int(m.group(1), 16)
                break
        if cmp_imm is None or cmp_imm > 0xFFF:
            continue
        if bm is None:
            span = cmp_imm + 1
        else:
            span = max(read(img, IMAGE_BASE + bm, cmp_imm + 1)) + 1
        table = read(img, IMAGE_BASE + rt, 4 * span)
        targets = {IMAGE_BASE + struct.unpack_from("<I", table, 4 * k)[0] for k in range(span)}
        out[a] = sorted(t for t in targets if CODE_LO <= t < CODE_HI)
    return out


def closure(entry, insns, order, pos, entries, tbl):
    """Body = instructions reachable from the entry by control flow inside the listing.

    Returns (body, direct calls, tail jumps out, table targets out, indirect sites, data VAs,
    resolved table jumps)."""
    body, work = set(), [entry]
    calls, tails, tbl_ext, indirect, drefs, tbl_hits = [], [], [], [], set(), 0
    while work and len(body) < MAX_INSNS:
        a = work.pop()
        if a in body or a not in insns:
            continue
        body.add(a)
        text = insns[a]
        mn, _, ops = text.partition(" ")
        ops = ops.strip()
        drefs.update(int(m.group(1), 16) for m in DREF.finditer(text))
        i = pos[a]
        nxt = order[i + 1] if i + 1 < len(order) else None
        if nxt is not None and nxt != entry and nxt in entries:
            nxt = None  # falls into another function's entry
        if mn.startswith("RET") or mn in ("HLT", "INT3", "UD2"):
            continue
        d = DIRECT.match(ops)
        if mn == "CALL":
            if d:
                calls.append(int(d.group(1), 16))
            else:
                indirect.append((a, text))
            if nxt is not None:
                work.append(nxt)
            continue
        if mn.startswith("J"):
            if a in tbl:
                tbl_hits += 1
                for t in tbl[a]:
                    if t in insns:
                        work.append(t)
                    else:
                        tbl_ext.append(t)
                continue
            if not d:
                indirect.append((a, text))
                continue
            t = int(d.group(1), 16)
            if t not in insns or (t in entries and t != entry):
                tails.append(t)
            else:
                work.append(t)
            if mn == "JMP":
                continue
        if nxt is not None:
            work.append(nxt)
    return body, calls, tails, tbl_ext, indirect, drefs, tbl_hits


def rtti_vtables(img):
    """{vtable VA: (TypeDescriptor name, code slots)} for every MSVC vtable in .rdata whose COL and self RVA check out."""
    m = mmap.mmap(img.fileno(), 0, access=mmap.ACCESS_READ)
    tds = {}
    for pref in (b".?AV", b".?AU", b".?AT", b".?AW"):
        i = m.find(pref)
        while i != -1:
            e = m.find(b"\0", i, i + 200)
            if e != -1:
                tds[IMAGE_BASE + i - 0x10] = m[i:e].decode("ascii", "replace")
            i = m.find(pref, i + 1)
    by_rva = {v - IMAGE_BASE: v for v in tds}
    rd = m[RDATA_LO - IMAGE_BASE:RDATA_HI - IMAGE_BASE]
    cand = {}
    for j in range(0, len(rd) - 3, 4):
        t = by_rva.get(struct.unpack_from("<I", rd, j)[0])
        if t is not None:
            cand.setdefault(t, []).append(RDATA_LO + j - 0xC)
    cols = {}
    for t, cs in cand.items():
        for c in cs:
            if RDATA_LO <= c and c + 24 <= RDATA_HI:
                sig, _, _, tr, _, selfr = struct.unpack("<6I", m[c - IMAGE_BASE:c - IMAGE_BASE + 24])
                if sig == 1 and IMAGE_BASE + selfr == c and IMAGE_BASE + tr == t:
                    cols[c] = t
    out = {}
    for j in range(0, len(rd) - 7, 8):
        p = struct.unpack_from("<Q", rd, j)[0]
        if p in cols:
            v = RDATA_LO + j + 8
            slots = []
            while len(slots) < 512:
                at = v - IMAGE_BASE + 8 * len(slots)
                q = struct.unpack("<Q", m[at:at + 8])[0]
                if not CODE_LO <= q < CODE_HI:
                    break
                slots.append(q)
            out[v] = (tds[cols[p]], slots)
    m.close()
    return out


def rtti_root(td):
    """Outermost namespace of a TypeDescriptor name ('?' for a template or anonymous name); None if not a class."""
    if not td.startswith((".?AV", ".?AU", ".?AT", ".?AW")):
        return None
    o = owner(td)
    return "?" if o is None else (o[1] or o[0])


def rtti_engine(root):
    return root is not None and (root == "?" or root in ENGINE_ROOTS)


def boundary(entry, name, owner_of, doc_engine, crt):
    """(rule, reason) if the function is the engine boundary, else None (walked)."""
    own = owner_of.get(entry)
    if own is not None and rtti_engine(own):
        return ("rtti", own)
    if is_library(name):
        return ("library", name)
    if entry in doc_engine:
        return ("doc", doc_engine[entry])
    if crt and name.startswith("FUN_") and crt[0] <= entry <= crt[1]:
        return ("crt", "%X-%X" % crt)
    return None


def crawl(seeds, scope, ctx):
    """BFS from the seeds [(va, subsystem, how)]. Returns (rows keyed by entry VA, out-vtable edges cut)."""
    names, img, VT, entries, limit = ctx["names"], ctx["img"], ctx["VT"], ctx["entries"], ctx["limit"]
    funcs, alias, owner_of, visited, cut = {}, {}, {}, set(), Counter()
    q = deque((va, sub, how, None, "root") for va, sub, how in seeds)
    done = 0
    while q:
        if limit and done >= limit:
            break
        va, sub, how, parent, _edge = q.popleft()
        va = alias.get(va, va)
        key = "%08X" % va
        if va in visited:
            if key in funcs and sub not in funcs[key]["roots"]:
                funcs[key]["roots"].append(sub)
            continue
        done += 1
        if done % 500 == 0:
            print("%s crawl: %d crawled, %d queued" % (scope, done, len(q)), file=sys.stderr, flush=True)
        base = funcs["%08X" % parent]["path"] if parent is not None and "%08X" % parent in funcs else []
        source = "bridge"
        try:
            txt = disassemble(va)
        except Exception as e:  # bridge error: the row stays, unresolved
            txt = "error: %s" % str(e)[:120]
        bridge_msg = txt.strip()[:120]
        if txt.startswith("No function found") and img is not None and CODE_LO <= va < CODE_HI:
            i = bisect_right(ctx["sorted"], va)
            end = ctx["sorted"][i] if i < len(ctx["sorted"]) else va + IMAGE_CAP
            txt = image_listing(img, va, end)
            source = "capstone"
        insns = {} if not txt or txt.startswith(("No function found", "error:")) else parse_listing(txt)
        if not insns:
            visited.add(va)
            funcs[key] = {"name": names.get(va, "FUN_%08x" % va), "size": 0, "subsystem": sub, "how": how,
                          "roots": [sub], "path": (base + [key])[-4:], "walked": False,
                          "boundary": None, "leaf": False, "unresolved": True, "source": source,
                          "protected": va >= IMPDATA_LO, "error": bridge_msg, "callees": {},
                          "indirect": [], "tables": 0, "vtables": [], "scope": scope}
            continue
        order = sorted(insns)
        entry = order[0]  # the listing starts at the containing function's entry
        if entry != va:
            alias[va] = entry
        ekey = "%08X" % entry
        if entry in visited:
            if sub not in funcs[ekey]["roots"]:
                funcs[ekey]["roots"].append(sub)
            continue
        visited.add(entry)
        pos = {a: i for i, a in enumerate(order)}
        tbl = jump_tables(insns, order, img)
        body, calls, tails, tbl_ext, indirect, drefs, tbl_hits = closure(
            entry, insns, order, pos, entries, tbl)
        name = names.get(entry, "FUN_%08x" % entry)
        vt_hits = [(v, VT[v]) for v in sorted(drefs) if v in VT]
        bnd = boundary(entry, name, owner_of, ctx["doc_engine"], ctx["crt"])
        callees = {}
        if bnd is None:
            for t in calls:
                callees.setdefault(t, "direct")
            for t in tails:
                callees.setdefault(t, "tail")
            for t in tbl_ext:
                callees.setdefault(t, "table")
            for _v, (td, slots) in vt_hits:
                o = out_rule(td)
                if scope == "in" and o:
                    cut[o[0]] += 1
                    continue
                root = rtti_root(td)
                for s in slots:
                    callees.setdefault(s, "vtable")
                    owner_of.setdefault(s, root)
        leaf = (bnd is None and len(body) <= LEAF_MAX_INSNS and not calls and not tails
                and not tbl_ext and not indirect and not tbl_hits and not vt_hits)
        funcs[ekey] = {
            "name": name, "size": len(body), "subsystem": sub, "how": how, "roots": [sub],
            "path": (base + [ekey])[-4:], "walked": bnd is None,
            "boundary": {"rule": bnd[0], "why": bnd[1]} if bnd else None,
            "leaf": leaf, "source": source, "scope": scope,
            "callees": {"%08X" % t: k for t, k in sorted(callees.items())},
            "indirect": [{"site": "%X" % a, "text": t[:80]} for a, t in indirect],
            "tables": tbl_hits,
            "vtables": [{"va": "%X" % v, "root": rtti_root(td)} for v, (td, _s) in vt_hits],
            "overspill": True if len(body) >= MAX_INSNS else None,
        }
        for t, kind in callees.items():
            q.append((t, sub, how, entry, kind))
    return funcs, cut


def opt(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    out = opt("--out", DEFAULT_OUT)
    image = opt("--image")
    limit = int(opt("--max", "0"))
    img = open(image, "rb") if image else None
    names = load_names()
    entries = set(names)
    VT = rtti_vtables(img) if img is not None else {}
    ctx = {"img": img, "names": names, "entries": entries, "sorted": sorted(entries), "VT": VT,
           "doc_engine": doc_engine_vas(), "crt": crt_span(names), "limit": limit}
    in_seeds = [(va, sub, "doc") for va, sub, _cite in SEEDS]
    out_seeds, vt_in, vt_out = [], [], []
    for v, (td, slots) in sorted(VT.items()):
        o = out_rule(td)
        if o:
            vt_out.append((v, td, o, len(slots)))
            out_seeds.extend((s, "out: " + o[0], "rtti") for s in slots)
            continue
        r = vt_root_tag(td)
        if r:
            vt_in.append((v, td, r, len(slots)))
            in_seeds.extend((s, r[0], "rtti") for s in slots)
    fin, cut_in = crawl(in_seeds, "in", ctx)
    fout, _cut_out = crawl(out_seeds, "out", ctx)
    for k, row in fout.items():
        if k not in fin:
            fin[k] = row
    funcs = fin
    for f in funcs.values():
        b = f.get("boundary")
        if b and b["rule"] == "rtti" and b["why"] in ("draw", "gr"):
            f["subsystem"], f["how"] = "render", "owner"
    rules = Counter()
    for f in funcs.values():
        if f.get("boundary"):
            rules["boundary:" + f["boundary"]["rule"]] += 1
        if f.get("leaf"):
            rules["leaf"] += 1
        if f.get("unresolved"):
            rules["unresolved"] += 1
            rules["unresolved:protected"] += 1 if f.get("protected") else 0
        if f.get("source") == "capstone":
            rules["capstone decoded"] += 1
        rules["table jumps resolved"] += f.get("tables", 0)
        rules["vtable rows"] += 1 if f.get("vtables") else 0
        rules["scope:" + f.get("scope", "?")] += 1
    for k, v in sorted(cut_in.items()):
        rules["in crawl: out-of-scope vtable edges not expanded (%s)" % k] = v
    scope_rules = []
    for name, group in (("in", vt_in), ("out", vt_out)):
        c, s = Counter(), Counter()
        for _v, _td, (_sub, why), n in group:
            c[why] += 1
            s[why] += n
        for why in sorted(c):
            scope_rules.append({"scope": name, "why": why, "vtables": c[why], "slots": s[why]})
    meta = {"bridge": BRIDGE, "image": bool(image), "named_symbols": len(names),
            "doc_engine_vas": len(ctx["doc_engine"]), "rtti_vtables": len(VT),
            "crt_span": ["%X" % a for a in ctx["crt"]] if ctx["crt"] else None,
            "leaf_max_insns": LEAF_MAX_INSNS, "max_insns": MAX_INSNS, "image_cap": IMAGE_CAP,
            "roots": [{"va": "0x%08X" % va, "subsystem": s, "how": "doc", "cite": c} for va, s, c in SEEDS],
            "vtable_roots": [{"vtable": "0x%08X" % v, "class": td[4:], "subsystem": r[0], "why": r[1],
                              "slots": n} for v, td, r, n in vt_in],
            "out_vtables": [{"vtable": "0x%08X" % v, "class": td[4:], "category": o[0], "why": o[1],
                             "slots": n} for v, td, o, n in vt_out],
            "scope_rules": scope_rules,
            "n_functions": len(funcs), "n_in": sum(1 for f in funcs.values() if f["scope"] == "in"),
            "n_out": sum(1 for f in funcs.values() if f["scope"] == "out"),
            "rules": dict(sorted(rules.items()))}
    with open(out, "w") as f:
        json.dump({"meta": meta, "functions": funcs}, f, indent=1, sort_keys=True)
    print("wrote %s: %d functions (in %d, out %d), rules %s" % (
        out, len(funcs), meta["n_in"], meta["n_out"], dict(sorted(rules.items()))))


if __name__ == "__main__":
    main()
