import re, sys, struct

BS = chr(92)
src = open(sys.argv[1], encoding="utf-8").read()
exe = open(sys.argv[2], "rb").read()

pe = struct.unpack_from("<I", exe, 0x3C)[0]
nsec = struct.unpack_from("<H", exe, pe + 6)[0]
optsz = struct.unpack_from("<H", exe, pe + 20)[0]
base = struct.unpack_from("<Q", exe, pe + 24 + 24)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    vsz, va, rsz, rptr = struct.unpack_from("<IIII", exe, o + 8)
    secs.append((va, rptr, rsz))

def to_va(off):
    for va, rp, rs in secs:
        if rp <= off < rp + rs:
            return base + va + off - rp
    return None

hexbyte = BS + BS + "x[0-9A-Fa-f]{2}"
pattern = r'\{ "(\w+)", (0x[0-9A-Fa-f]+), \{\s*\{ "((?:' + hexbyte + r')+)",\s*"([x?]+)" \}'
defs = re.findall(pattern, src)
ok = 0
for name, known, sig, mask in defs:
    b = bytes.fromhex(sig.replace(BS + "x", ""))
    assert len(b) == len(mask), name
    pat = b"".join(re.escape(bytes([c])) if m == "x" else b"." for c, m in zip(b, mask))
    hits = [to_va(m.start()) for m in re.finditer(pat, exe, re.S)]
    good = len(hits) == 1 and hits[0] == int(known, 16)
    ok += good
    print(f"{name:18} hits={len(hits)} at={[hex(h) for h in hits][:3]} expected={known} {'OK' if good else 'MISMATCH'}")
print(f"{ok}/{len(defs)} unique + matching")
