#!/usr/bin/env python3
"""boundary_guard.py: the proof harness's guard on the graphics device allocator boundary, 0x141957C50.

Owner ruling (Main, option A, 21-11-26): the boundary answers 0 (stubs.json gfx_device_alloc: GF has no graphics device, so the
allocation fails). A proof may take that answer only where PES's own code, on the null path, loses nothing a GF port keeps. At
every call site a proof's closure reaches, the skipped region PES runs only on a non-null answer (the non-null path of
`test rax,rax; je skip`) must write memory only through the returned pointer (the render-command packet: rax, or a register
copied from it before the test), and must not push, return, trap, or write anything else.

Two calls are admitted inside that region, and no others:
  - a pure callee (is_pure): writes no memory, pushes nothing, calls nothing, jumps only inside itself, and writes only volatile
    registers (a getter such as 0x1421F8690: mov rax, [rcx+8]; ret);
  - a paired reference (owner ruling, paired, round 3): the AddRef callee (recognised by its bytes: mov eax,1; lock xadd [rcx],eax;
    ret, 0x1403DB250) increments the count of an object whose pointer the same region stored into the packet. The increment is
    the render queue's reference, released when it consumes the packet. An AddRef of an object the region did not store into the
    packet is a write outside the packet and fails.
A site that does not meet this fails the proof, loudly, with its address.

The non-null region is the set of instructions reachable from the skipped arm's first byte without passing its exit: the exit is
the target of the arm's last jump past the join (the common continuation after a null-only block), or the join when there is no
such jump. Instructions reached only from a null-only block are not part of it.
ponytail: the packet and object provenance is tracked in address order over that region, not per path; a store on one path and
its AddRef on another would be taken as paired. No real site needs that; the rule would need path-sensitive provenance to be exact.

check_site(md, read, site, target, is_pure) -> None if the site passes, else the reason.
check_function / check_closure / closure_failures / closure_sites: the closure's sites over a flat image.
md: a capstone Cs (x86-64) with detail on. read(va, n) -> the image bytes at va.
"""
import functools

import capstone
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

GFX_DEVICE_ALLOC = 0x141957C50  # stubs.json gf-service gfx_device_alloc: a direct call from the render-command builders
RULING = ("Main, option A (21-11-26): declared answer 0 plus this guard; paired-reference ruling (owner, round 3): a refcount "
          "increment in the non-null region is admitted only when the same object is stored into the packet in that region")
# AddRef: mov eax,1; lock xadd dword ptr [rcx],eax; ret (the object's count sits at +8 of the pointer the callee receives in rcx).
ADDREF_BYTES = bytes.fromhex("b801000000" "f00fc101" "c3")

# Names of the same 64-bit register (the packet pointer may be copied to a 32-bit or byte view and back).
_REG64 = {}
for _x in "abcd":
    for _f in ("e%sx", "%sx", "%sl", "%sh"):
        _REG64[_f % _x] = "r%sx" % _x
for _x in ("si", "di", "bp", "sp"):
    for _f in ("e%s", "%s", "%sl"):
        _REG64[_f % _x] = "r%s" % _x
for _n in range(8, 16):
    for _f in ("r%dd", "r%dw", "r%db"):
        _REG64[_f % _n] = "r%d" % _n

# Instructions whose effect leaves the packet: a call (its effect is unknown), a stack write, a return, a trap, a privileged read.
_LEAVES = {"call", "ret", "push", "pushf", "pushfq", "enter", "leave", "int", "int3", "into", "syscall", "sysenter", "cpuid",
           "rdtsc", "rdtscp", "rdrand", "rdseed", "xgetbv", "xsave", "xsavec", "xsaveopt", "fxsave", "in", "out", "hlt"}
# String instructions write through rdi with an implicit count: never in the region.
_STRING = {"movsb", "movsw", "movsq", "stosb", "stosw", "stosd", "stosq"}
# The registers a call clobbers (Win64's volatile set), which a callee's return value overwrites.
_VOLATILE = {"rax", "rcx", "rdx", "r8", "r9", "r10", "r11"}


def _canon(name):
    return _REG64.get(name, name)


def _reg_writes(md, ins):
    """The 64-bit registers an instruction writes: its explicit register operands with write access, and the implicit ones
    (capstone's regs_write lists only the implicit ones: `mov rax, rbx` writes rax as an operand)."""
    out = {_canon(md.reg_name(r)) for r in ins.regs_write}
    for op in ins.operands:
        if op.type == X86_OP_REG and op.access & capstone.CS_AC_WRITE:
            out.add(_canon(md.reg_name(op.reg)))
    return out


def _writes_memory(ins):
    return any(op.type == X86_OP_MEM and op.access & capstone.CS_AC_WRITE for op in ins.operands)


def is_pure(pd, md, va):
    """True if the function at va writes no memory, pushes nothing, calls nothing, jumps only inside itself, and writes only
    volatile registers: its effect is its return value and the volatile registers. (A getter such as 0x1421F8690.)"""
    body = pd.decode(md, va) or []
    if not body or not any(i.mnemonic == "ret" for i in body):
        return False
    inside = {i.address for i in body}
    for ins in body:
        m = ins.mnemonic
        if m == "ret":
            continue
        if m.startswith("j"):
            try:
                t = int(ins.op_str, 16)
            except ValueError:
                return False
            if t not in inside:
                return False
            continue
        if m in _LEAVES or m in _STRING or m.startswith("rep") or _writes_memory(ins):
            return False
        if _reg_writes(md, ins) - _VOLATILE:  # a callee must preserve the non-volatile registers (the packet may live there)
            return False
    return True


def _non_null_region(md, read, ft, join):
    """({address: instruction} of the non-null region, its exit) or (None, reason). The region starts at ft and stops at its exit."""
    linear = [i for i in md.disasm(read(ft, join - ft + 0x10), ft) if i.address < join]
    if not linear or linear[-1].address + linear[-1].size != join:
        return None, "the skipped arm does not end exactly at %#x" % join
    exit_at = join
    if linear[-1].mnemonic == "jmp":
        try:
            if int(linear[-1].op_str, 16) > join:
                exit_at = int(linear[-1].op_str, 16)
        except ValueError:
            return None, "the skipped arm ends in an indirect jump"
    region = {}
    todo = [ft]
    while todo:
        a = todo.pop()
        if a == exit_at or a in region:
            continue
        if a > exit_at or a < ft:
            return None, "the non-null region leaves [%#x, %#x) at %#x" % (ft, exit_at, a)
        ins = next(md.disasm(read(a, 16), a), None)
        if ins is None:
            return None, "the non-null region cannot be decoded at %#x" % a
        region[a] = ins
        m = ins.mnemonic
        if m == "ret":
            return None, "arm at %#x: ret (a return outside the packet)" % a
        if m.startswith("j"):
            try:
                todo.append(int(ins.op_str, 16))
            except ValueError:
                return None, "arm at %#x: indirect branch %s" % (a, ins.op_str)
            if m == "jmp":
                continue
        todo.append(a + ins.size)
    return region, exit_at


def check_site(md, read, site, target=GFX_DEVICE_ALLOC, is_pure=None):
    """None if the call at site (a direct call to target) passes the guard; else the reason, naming the site. is_pure(callee VA)
    says whether a call inside the region is to a pure callee; None refuses every call there other than a paired AddRef."""
    seq = list(md.disasm(read(site, 0x100), site))
    if not seq or seq[0].address != site or seq[0].mnemonic != "call" or seq[0].op_str.strip() != hex(target):
        return "site %#x: not a direct call to %#x" % (site, target)
    packet = {"rax"}
    i = 1
    while True:
        if i >= len(seq) or i > 8:
            return "site %#x: no test of the returned pointer after the call" % site
        ins = seq[i]
        if ins.mnemonic == "test":
            ops = ins.operands
            if (len(ops) == 2 and all(o.type == X86_OP_REG for o in ops)
                    and _canon(md.reg_name(ops[0].reg)) == _canon(md.reg_name(ops[1].reg))
                    and _canon(md.reg_name(ops[0].reg)) in packet):
                break
            return "site %#x: the test after the call is not on the returned pointer (%s %s)" % (site, ins.mnemonic, ins.op_str)
        # A copy of the pointer before the test runs on both paths: it extends the packet's registers; any other write drops them.
        ops = ins.operands
        if (ins.mnemonic == "mov" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_REG
                and _canon(md.reg_name(ops[1].reg)) in packet):
            packet.add(_canon(md.reg_name(ops[0].reg)))
        else:
            packet -= _reg_writes(md, ins)
        i += 1
    if i + 1 >= len(seq) or seq[i + 1].mnemonic not in ("je", "jz"):
        nxt = seq[i + 1].mnemonic if i + 1 < len(seq) else "end"
        return "site %#x: the null test is not followed by je/jz (%s): the skipped region is not delimited" % (site, nxt)
    j = seq[i + 1]
    join = int(j.op_str, 16)
    ft = j.address + j.size
    if join <= ft:
        return "site %#x: the skip target %#x is not forward" % (site, join)
    region, exit_at = _non_null_region(md, read, ft, join)
    if region is None:
        return "site %#x: %s" % (site, exit_at)
    # Provenance within the region (paired-reference rule). tok maps a register to the object it holds: a register the region has not
    # written holds the object it held on entry (("in", reg)); a write gives a token naming that write. slot maps a packet offset to the
    # object stored there; stored is every object the region stored into the packet. A field address (lea reg,[obj+disp], add reg,imm)
    # is the object itself: the count sits at a fixed offset of the object it is read from.
    tok, slot, stored = {}, {}, set()

    def held(reg):
        return tok.get(reg, ("in", reg))

    for a in sorted(region):
        ins = region[a]
        m = ins.mnemonic
        if m == "call":
            try:
                callee = int(ins.op_str, 16)
            except ValueError:
                return "site %#x: arm at %#x: indirect call %s" % (site, ins.address, ins.op_str)
            if bytes(read(callee, len(ADDREF_BYTES))) == ADDREF_BYTES:
                if held("rcx") not in stored:
                    return ("site %#x: arm at %#x: refcount increment of an object the arm did not store into the packet "
                            "(the write is outside the packet; unpaired)" % (site, ins.address))
                packet -= _VOLATILE
                tok.update({r: ("call", a, r) for r in _VOLATILE})
                continue
            if is_pure is None or not is_pure(callee):
                return "site %#x: arm at %#x: call to %s (not a pure callee: its writes are unknown)" % (site, ins.address, ins.op_str)
            packet -= _VOLATILE
            tok.update({r: ("call", a, r) for r in _VOLATILE})
            continue
        if m in _LEAVES or m in _STRING or m.startswith("rep"):
            return "site %#x: arm at %#x: %s (a stack, return, trap or string effect outside the packet)" % (site, ins.address, m)
        if m.startswith("j"):
            continue  # the region's branches were followed when it was built
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.access & capstone.CS_AC_WRITE:
                base = _canon(md.reg_name(op.mem.base)) if op.mem.base else None
                # Owner ruling (widen, round 4): a write into the paired object itself (an object the region stored into the packet, held
                # by the base register) is admitted, refcount or flag; any other write outside the packet fails.
                paired = base is not None and held(base) in stored
                if (base not in packet and not paired) or op.mem.index != 0:
                    return "site %#x: arm at %#x: %s writes memory outside the packet (%s)" % (site, ins.address, m, ins.op_str)
        ops = ins.operands
        if m == "mov" and len(ops) == 2 and ops[0].type == X86_OP_MEM:  # a store into the packet (checked above)
            value = held(_canon(md.reg_name(ops[1].reg))) if ops[1].type == X86_OP_REG else None
            slot[ops[0].mem.disp] = value
            if value is not None:
                stored.add(value)
        elif m == "mov" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_MEM:
            mem = ops[1].mem
            if mem.base and _canon(md.reg_name(mem.base)) in packet and mem.index == 0:
                tok[_canon(md.reg_name(ops[0].reg))] = slot.get(mem.disp, ("pktin", mem.disp))
            else:
                tok[_canon(md.reg_name(ops[0].reg))] = ("mem", a)
        elif m == "mov" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_REG:
            tok[_canon(md.reg_name(ops[0].reg))] = held(_canon(md.reg_name(ops[1].reg)))
        elif m == "lea" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_MEM:
            mem = ops[1].mem
            tok[_canon(md.reg_name(ops[0].reg))] = held(_canon(md.reg_name(mem.base))) if mem.base and mem.index == 0 else ("lea", a)
        elif m == "add" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_IMM:
            pass  # the same object at a field offset
        else:
            for r in _reg_writes(md, ins):
                tok[r] = ("w", a, r)
        if m == "mov" and len(ops) == 2 and ops[0].type == X86_OP_MEM:
            continue  # a store: writes no register
        if (m == "mov" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_REG
                and _canon(md.reg_name(ops[1].reg)) in packet):
            packet.add(_canon(md.reg_name(ops[0].reg)))
        else:
            packet -= _reg_writes(md, ins)
    return None


@functools.lru_cache(maxsize=2)
def _image(path):
    """(the Pdata of the flat image, a capstone with detail) for one image path, opened once per process."""
    import pdata
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    return pdata.Pdata(open(path, "rb")), md


def check_function(pdata, md, read, va, target=GFX_DEVICE_ALLOC):
    """The failures of every call to target in the function at va: [(site, reason)]. A tail jump to target is a failure too."""
    failures = []
    body = pdata.decode(md, va) or []
    pure = functools.lru_cache(maxsize=None)(lambda callee: is_pure(pdata, md, callee))
    for ins in body:
        if ins.mnemonic == "call" and ins.op_str.strip() == hex(target):
            reason = check_site(md, read, ins.address, target, pure)
            if reason is not None:
                failures.append((ins.address, reason))
        elif ins.mnemonic == "jmp" and ins.op_str.strip() == hex(target):
            failures.append((ins.address, "site %#x: a tail jump to the boundary (no arm to check)" % ins.address))
    return failures


def check_closure(pdata, md, read, vas, target=GFX_DEVICE_ALLOC):
    """Every failure over the closure's functions, as strings naming the function and the site. [] when the closure is clean."""
    out = []
    for va in sorted(set(vas)):
        for site, reason in check_function(pdata, md, read, va, target):
            out.append("function %#x: %s" % (va, reason))
    return out


def closure_failures(image, vas, target=GFX_DEVICE_ALLOC):
    """The guard over a proof's closure (snapdiff.build_binary): every allocator site in those functions, [] when all pass.
    Each failure string names its function and site."""
    pd, md = _image(image)
    return check_closure(pd, md, pd.read, vas, target)


def closure_sites(image, vas, target=GFX_DEVICE_ALLOC):
    """Every allocator call site in a closure's functions (the proof's record names them): [site VA]."""
    pd, md = _image(image)
    out = []
    for va in sorted(set(vas)):
        for ins in pd.decode(md, va) or []:
            if ins.mnemonic == "call" and ins.op_str.strip() == hex(target):
                out.append(ins.address)
    return out
