"""Tiny static-analysis helper for DivaMegaMix.exe (no ASLR, base 0x140000000)."""
import bisect, re, struct, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

EXE = r"C:\Program Files (x86)\Steam\steamapps\common\Hatsune Miku Project DIVA Mega Mix Plus\DivaMegaMix.exe"

pe = pefile.PE(EXE, fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"]])
BASE = pe.OPTIONAL_HEADER.ImageBase
IMG = pe.get_memory_mapped_image()  # bytes indexed by RVA

# Section names are scrambled by the protector; the real game code is the one holding the entry of
# task_pv_game_init_pv (0x1404414B0, see docs/re-notes.md "Song launch").
text = next(s for s in pe.sections if s.VirtualAddress <= 0x4414B0 < s.VirtualAddress + s.Misc_VirtualSize)
TEXT_LO = text.VirtualAddress
TEXT_HI = text.VirtualAddress + text.Misc_VirtualSize

# Function table from .pdata
FUNCS = sorted((e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION)
FSTARTS = [f[0] for f in FUNCS]

md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True


def va(rva): return BASE + rva
def rva(v): return v - BASE


def func_of(v):
    """(start_va, end_va) of the .pdata function containing VA v (chained unwind entries may split one function)."""
    r = rva(v)
    i = bisect.bisect_right(FSTARTS, r) - 1
    if i >= 0 and FUNCS[i][0] <= r < FUNCS[i][1]:
        return va(FUNCS[i][0]), va(FUNCS[i][1])
    return None


def find_bytes(pattern: bytes, lo=0, hi=None):
    hi = hi or len(IMG)
    out, i = [], IMG.find(pattern, lo, hi)
    while i != -1:
        out.append(va(i))
        i = IMG.find(pattern, i + 1, hi)
    return out


def find_str(s: str):
    return find_bytes(s.encode() + b"\0")


_rip_cache = None


def rip_refs_index():
    """Map target VA -> [instruction VA] for every disp32 that, interpreted as rip-relative, lands in the image.
    Cheap heuristic: decode at every offset where a ModRM with mod=00 rm=101 could be."""
    global _rip_cache
    if _rip_cache is not None:
        return _rip_cache
    idx = {}
    data = IMG
    size = len(IMG)
    # candidate: byte b where (b & 0xC7) == 0x05 is a ModRM rip-rel; instruction end = modrm+5 (+imm, ignored -> verified later)
    for m in re.finditer(rb"[\x05\x0D\x15\x1D\x25\x2D\x35\x3D]", data[TEXT_LO:TEXT_HI]):
        p = TEXT_LO + m.start()
        disp = struct.unpack_from("<i", data, p + 1)[0]
        for imm in (0, 1, 4):  # common immediate sizes after disp32
            tgt = p + 5 + imm + disp
            if 0 <= tgt < size:
                idx.setdefault(va(tgt), []).append(va(p))
    _rip_cache = idx
    return idx


def xrefs_data(target_va):
    """Instructions that reference target_va via rip-relative addressing (verified with capstone)."""
    out = []
    for modrm_va in set(rip_refs_index().get(target_va, [])):
        # instruction start is 1-4 bytes before modrm (prefix/REX/opcode)
        for back in range(1, 6):
            start = modrm_va - back
            code = IMG[rva(start):rva(start) + 16]
            ins = next(md.disasm(code, start, 1), None)
            if not ins or ins.address + ins.size <= modrm_va:
                continue
            for op in ins.operands:
                if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP and ins.address + ins.size + op.mem.disp == target_va:
                    out.append(ins)
                    break
            else:
                continue
            break
    return sorted(out, key=lambda i: i.address)


def xrefs_call(target_va):
    """E8/E9 rel32 calls/jumps to target_va."""
    out = []
    t = rva(target_va)
    for m in re.finditer(rb"[\xE8\xE9]", IMG[TEXT_LO:TEXT_HI]):
        p = TEXT_LO + m.start()
        if p + 5 + struct.unpack_from("<i", IMG, p + 1)[0] == t:
            out.append(va(p))
    return out


def dis(start_va, count=60, end_va=None):
    code = IMG[rva(start_va):rva(end_va or start_va + count * 15)]
    lines = []
    for n, ins in enumerate(md.disasm(code, start_va)):
        if end_va is None and n >= count:
            break
        lines.append(f"{ins.address:X}: {ins.mnemonic:6} {ins.op_str}")
    return "\n".join(lines)


def dis_func(v):
    f = func_of(v)
    return dis(f[0], end_va=f[1]) if f else dis(v)


def calls_in(start_va, end_va):
    out = []
    code = IMG[rva(start_va):rva(end_va)]
    for ins in md.disasm(code, start_va):
        if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
            out.append((ins.address, int(ins.op_str, 16), ins.mnemonic))
    return out


def rip_targets_in(start_va, end_va):
    out = []
    code = IMG[rva(start_va):rva(end_va)]
    for ins in md.disasm(code, start_va):
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                out.append((ins.address, ins.address + ins.size + op.mem.disp, f"{ins.mnemonic} {ins.op_str}"))
    return out


def cstr(v, n=80):
    b = IMG[rva(v):rva(v) + n]
    return b.split(b"\0")[0].decode("latin1")


def vtables_for(mangled: str):
    """MSVC x64 RTTI: TypeDescriptor name -> CompleteObjectLocators -> vtable VAs (+ COL offset)."""
    out = []
    for name_va in find_bytes(mangled.encode() + b"\0"):
        td = name_va - 0x10
        td_rva = struct.pack("<I", rva(td))
        for col_field in find_bytes(td_rva):
            col = col_field - 0x0C
            sig, off = struct.unpack_from("<II", IMG, rva(col))
            if sig != 1:
                continue
            for vt_ref in find_bytes(struct.pack("<Q", col)):
                out.append((vt_ref + 8, off))
    return out


def vtable_entries(vt, maxn=64):
    ents = []
    for i in range(maxn):
        p = struct.unpack_from("<Q", IMG, rva(vt) + i * 8)[0]
        if not (BASE + TEXT_LO <= p < BASE + 0x160000000 - 0x140000000 + BASE - BASE + 0x20000000):
            break
        ents.append(p)
    return ents
