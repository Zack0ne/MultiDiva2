import sys, re
sys.path.insert(0, __import__("os").path.dirname(__file__))
from dre import *
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

def make_sig(s, n):
    code = IMG[rva(s):rva(s) + n]
    mask = ["x"] * n
    for ins in md.disasm(code, s):
        o = ins.address - s
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                for k in range(4): mask[o + ins.disp_offset + k] = "?"
        if ins.mnemonic in ("call", "jmp") and ins.size == 5:
            for k in range(1, 5): mask[o + k] = "?"
    mask = "".join(mask)
    pat = b"".join(re.escape(bytes([c])) if m == "x" else b"." for c, m in zip(code, mask))
    hits = [va(m.start()) for m in re.finditer(pat, IMG, re.S)]
    bs = "".join(chr(92) + "x%02X" % c for c in code)
    return bs, mask, hits

if __name__ == "__main__":
    bs, mask, hits = make_sig(int(sys.argv[1], 16), int(sys.argv[2], 16))
    print(bs); print(mask); print("hits", [hex(h) for h in hits])
