"""Prepare the preserved v21 renderer for independent native pixel tests."""
import argparse
from pathlib import Path
import re
import zipfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('snapshot', type=Path)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(a.snapshot) as archive:
    for original, output in [('GBAFeather/src/core/ppu.cpp', 'ppu_reference.cpp'),
                             ('GBAFeather/src/gba/core/ppu.hpp', 'ppu_reference.hpp')]:
        source = archive.read(original).decode()
        source = re.sub(r'\bPpu\b', 'ReferencePpu', source)
        source = source.replace('gba/core/ppu.hpp', 'ppu_reference.hpp')
        source = source.replace('RP2350_GBA_PPU_HPP', 'GBA_REFERENCE_PPU_HPP')
        (a.out / output).write_text(source)
