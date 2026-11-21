#!/usr/bin/env python3
"""boundary_guard.py: the proof harness's guard on the graphics device allocator boundary, 0x141957C50.

Owner ruling (Main, option A, 21-11-26): the boundary answers 0 (stubs.json gfx_device_alloc: GF has no graphics device, so the
allocation fails). A proof may take that answer only where PES's own code, on the null path, loses nothing a GF port keeps. At
every call site a proof's closure reaches, the arm PES skips on a null answer (the non-null arm of `test rax,rax; je skip`) must
write memory only through the returned pointer (the render-command packet: rax, or a register copied from it before the test),
and must not push, return, branch out of the arm, trap, or write anything else. A call inside the arm is allowed only to a pure
callee: one that writes no memory, pushes nothing, calls nothing, and jumps only inside itself (a getter). Any other site fails the
proof, loudly, with its address.

check_site(md, read, site, target, is_pure) -> None if the site passes, else the reason.
check_function / check_closure / closure_failures / closure_sites: the closure's sites over a flat image.
md: a capstone Cs (x86-64) with detail on. read(va, n) -> the image bytes at va.
"""
import functools

import capstone
from capstone.x86 import X86_OP_MEM, X86_OP_REG

GFX_DEVICE_ALLOC = 0x141957C50  # stubs.json gf-service gfx_device_alloc: a direct call from the render-command builders
RULING = "Main, option A (21-11-26): declared answer 0 plus this guard"

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
# String instructions write through rdi with an implicit count: never in an arm.
_STRING = {"movsb", "movsw", "movsq", "stosb", "stosw", "stosd", "stosq"}
# The registers a call clobbers (System V-free: Win64's volatile set), which a pure callee's return value overwrites.
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
    """True if the function at va writes no memory, pushes nothing, calls nothing and jumps only inside itself: its effect is
    its return value and the registers it writes. (A getter such as 0x1421F8690: mov rax, [rcx+8]; ret.)"""
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


def check_site(md, read, site, target=GFX_DEVICE_ALLOC, is_pure=None):
    """None if the call at site (a direct call to target) passes the guard; else the reason, naming the site. is_pure(callee VA)
    says whether a call inside the arm is to a pure callee; None refuses every call in an arm."""
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
        return "site %#x: the null test is not followed by je/jz (%s): the skipped arm is not delimited" % (site, nxt)
    j = seq[i + 1]
    join = int(j.op_str, 16)
    ft = j.address + j.size
    if join <= ft:
        return "site %#x: the skip target %#x is not forward" % (site, join)
    arm = []
    for ins in md.disasm(read(ft, join - ft + 0x10), ft):
        if ins.address >= join:
            break
        arm.append(ins)
    if not arm or arm[-1].address + arm[-1].size != join:
        return "site %#x: the skipped arm does not end exactly at %#x" % (site, join)
    for ins in arm:
        m = ins.mnemonic
        if m == "call":
            try:
                callee = int(ins.op_str, 16)
            except ValueError:
                return "site %#x: arm at %#x: indirect call %s" % (site, ins.address, ins.op_str)
            if is_pure is None or not is_pure(callee):
                return "site %#x: arm at %#x: call to %s (not a pure callee: its writes are unknown)" % (site, ins.address, ins.op_str)
            packet -= _VOLATILE
            continue
        if m in _LEAVES or m in _STRING or m.startswith("rep"):
            return "site %#x: arm at %#x: %s (a stack, return, trap or string effect outside the packet)" % (site, ins.address, m)
        if m.startswith("j"):
            try:
                t = int(ins.op_str, 16)
            except ValueError:
                return "site %#x: arm at %#x: indirect branch %s" % (site, ins.address, ins.op_str)
            if not ft <= t <= join:
                return "site %#x: arm at %#x: branch to %#x leaves the arm" % (site, ins.address, t)
            continue
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.access & capstone.CS_AC_WRITE:
                base = _canon(md.reg_name(op.mem.base)) if op.mem.base else None
                if base not in packet or op.mem.index != 0:
                    return "site %#x: arm at %#x: %s writes memory outside the packet (%s)" % (site, ins.address, m, ins.op_str)
        ops = ins.operands
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
