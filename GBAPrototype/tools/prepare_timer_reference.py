"""Prepare a privately preserved v26 timer implementation for comparison."""
import argparse
from pathlib import Path
import re
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('snapshot', type=Path)
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.snapshot) as archive:
    for original, name in [('GBAFeather/src/core/timers.cpp', 'timers_reference.cpp'),
                           ('GBAFeather/src/gba/core/timers.hpp', 'timers_reference.hpp')]:
        source = archive.read(original).decode('utf8')
        source = re.sub(r'\bTimers\b', 'ReferenceTimers', source)
        source = re.sub(r'\bTimerChannel\b', 'ReferenceTimerChannel', source)
        source = source.replace('gba/core/timers.hpp', 'timers_reference.hpp')
        source = source.replace('RP2350_GBA_TIMERS_HPP', 'REFERENCE_V26_TIMERS_HPP')
        (args.out/name).write_bytes(('\n'.join(source.splitlines())+'\n').encode('utf8'))
