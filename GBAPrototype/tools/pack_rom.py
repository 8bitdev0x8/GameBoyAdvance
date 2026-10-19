"""Pack independent 16 KiB DEFLATE pages; retain every original ROM byte."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import zlib

def pack(source: Path, sketch: Path):
    rom = source.read_bytes()
    page_size = 16384
    if not rom or len(rom) > 32*1024*1024:
        raise ValueError('Expected a nonempty GBA ROM no larger than 32 MiB')
    blob = bytearray()
    entries = []
    for start in range(0, len(rom), page_size):
        page = rom[start:start+page_size]
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
        compressed = c.compress(page)+c.flush()
        stored_raw = len(compressed) >= len(page)
        data = page if stored_raw else compressed
        entries.append((len(blob), len(data), len(page), zlib.crc32(page), int(stored_raw)))
        blob.extend(data)
    def unpack(index):
        offset, size, rawsize, crc, raw = entries[index]
        data = blob[offset:offset+size]
        page = bytes(data) if raw else zlib.decompress(data, -15)
        assert len(page) == rawsize and zlib.crc32(page) == crc
        return page
    restored = b''.join(unpack(i) for i in range(len(entries)))
    assert restored == rom
    rng = random.Random(2350)
    for _ in range(1000):
        address = rng.randrange(len(rom)-4)
        assert restored[address:address+4] == rom[address:address+4]
    sketch.mkdir(parents=True, exist_ok=True)
    generated = sketch/'src/generated'
    generated.mkdir(parents=True, exist_ok=True)
    data_path = generated/'romblocks.bin'
    data_path.write_bytes(blob)
    (generated/'rom_blob.S').write_text(
        '.section .rodata.gba_rom,"a"\n.balign 4\n.global gba_rom_packed\n'
        'gba_rom_packed:\n.incbin "'+data_path.resolve().as_posix()+'"\n', encoding='utf-8')
    header = '#pragma once\n#include <stdint.h>\n'
    header += '#ifdef GBA_HOST_TEST\nextern const uint8_t* gba_rom_packed;\n#else\nextern "C" const uint8_t gba_rom_packed[];\n#endif\n'
    header += 'struct RomBlock { uint32_t offset,size,unpacked,crc,raw; };\n'
    header += f'constexpr uint32_t ROM_SIZE={len(rom)}, ROM_PAGE_SIZE={page_size}, ROM_BLOCK_COUNT={len(entries)}, ROM_PACKED_SIZE={len(blob)};\n'
    header += 'const RomBlock ROM_BLOCKS[]={\n'
    header += ''.join('{'+','.join(str(x)+'u' for x in e)+'},\n' for e in entries)
    header += '};\n'
    (generated/'RomIndex.h').write_text(header, encoding='utf-8')
    manifest = {'source': source.name, 'rom_bytes': len(rom), 'packed_bytes': len(blob),
                'pages': len(entries), 'page_size': page_size,
                'sha256': hashlib.sha256(rom).hexdigest(), 'round_trip': 'PASS'}
    (generated/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest))

if __name__ == '__main__':
    p=argparse.ArgumentParser()
    p.add_argument('rom',type=Path)
    p.add_argument('--sketch',type=Path,default=Path(__file__).resolve().parents[1]/'GBAFeather')
    a=p.parse_args()
    pack(a.rom,a.sketch)
