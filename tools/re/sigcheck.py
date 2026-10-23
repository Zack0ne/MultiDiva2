"""sigcheck.py: checks every pattern in client/src/game/sigs.cpp against DivaMegaMix.exe: how many places each one
matches and whether the first match is the known v1.03 address (patterns with a resolver report their raw matches)."""
import os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from dre import *

SIGS = os.path.join(os.path.dirname(__file__), "..", "..", "client", "src", "game", "sigs.cpp")
src = open(SIGS, encoding="utf-8").read()

defs = re.findall(r'\{ "(\w+)", (0x[0-9A-Fa-f]+),\s*((?:"[0-9A-F? ]+"\s*)+)', src)
for name, known, parts in defs:
    text = "".join(re.findall(r'"([^"]*)"', parts)).split()
    regex = b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in text)
    hits = [va(m.start()) for m in re.finditer(regex, IMG, re.S)]
    status = "OK" if hits and hits[0] == int(known, 16) else "check resolver" if hits else "NOT FOUND"
    print(f"{name:22} hits={len(hits)} {[hex(h) for h in hits][:3]} known={known} {status}")
