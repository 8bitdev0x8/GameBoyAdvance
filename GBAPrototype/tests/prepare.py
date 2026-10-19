"""Expand GCC case ranges into equivalent labels for the MSVC host test."""
from pathlib import Path
import re
p=Path(__file__).resolve().parent
source=(p/'../GBAFeather/src/core/cpu.cpp').read_text()
def expand(m):
    return ' '.join(f'case {i}:' for i in range(int(m[1],0),int(m[2],0)+1))
source=re.sub(r'case\s+(0x[0-9a-fA-F]+|\d+)\s*\.\.\.\s*(0x[0-9a-fA-F]+|\d+)\s*:',expand,source)
(p/'portable_cpu.cpp').write_text('// Generated for MSVC tests; same case-label semantics.\n'+source)
