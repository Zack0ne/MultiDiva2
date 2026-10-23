"""mksig.py <va> <length>: a signature for the code at <va>, in the mod's text format ("48 8B 05 ?? ?? ?? ??"),
with rip-relative displacements and call/jmp targets wildcarded, plus how many places in the exe it matches."""
import os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from dre import *
from capstone.x86 import X86_OP_MEM, X86_REG_RIP


def make_sig(start, length):
    code = IMG[rva(start):rva(start) + length]
    wild = [False] * length
    for ins in md.disasm(code, start):
        o = ins.address - start
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                for k in range(4):
                    if o + ins.disp_offset + k < length:
                        wild[o + ins.disp_offset + k] = True
        if ins.mnemonic in ("call", "jmp") and ins.size == 5:
            for k in range(1, 5):
                if o + k < length:
                    wild[o + k] = True
    text = " ".join("??" if w else "%02X" % c for c, w in zip(code, wild))
    regex = b"".join(b"." if w else re.escape(bytes([c])) for c, w in zip(code, wild))
    hits = [va(m.start()) for m in re.finditer(regex, IMG, re.S)]
    return text, hits


if __name__ == "__main__":
    text, hits = make_sig(int(sys.argv[1], 16), int(sys.argv[2], 16))
    print(text)
    print("hits", [hex(h) for h in hits])
