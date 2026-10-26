"""Expand GCC case ranges into equivalent labels for the MSVC host test."""
from pathlib import Path
import re
import argparse
import zipfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline-archive',type=Path,help='Optional private V50 source archive for CPU comparison')
args=parser.parse_args()
p=Path(__file__).resolve().parent
source=(p/'../GBAFeather/src/core/cpu.cpp').read_text(encoding='utf-8')
def expand(m):
    return ' '.join(f'case {i}:' for i in range(int(m[1],0),int(m[2],0)+1))
source=re.sub(r'case\s+(0x[0-9a-fA-F]+|\d+)\s*\.\.\.\s*(0x[0-9a-fA-F]+|\d+)\s*:',expand,source)
(p/'portable_cpu.cpp').write_text('// Generated for MSVC tests; same case-label semantics.\n'+source,encoding='utf-8')
if args.baseline_archive:
    with zipfile.ZipFile(args.baseline_archive) as archive:
        reference=archive.read('GBAFeather/src/core/cpu.cpp').decode('utf-8')
    reference=re.sub(r'case\s+(0x[0-9a-fA-F]+|\d+)\s*\.\.\.\s*(0x[0-9a-fA-F]+|\d+)\s*:',expand,reference)
    (p/'portable_v50_cpu.cpp').write_text('// Generated from private V50 archive; same case-label semantics.\n'+reference,encoding='utf-8')
