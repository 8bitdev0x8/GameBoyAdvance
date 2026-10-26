"""Generate a private firmware header from a user-supplied 16 KiB GBA BIOS."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('bios', type=Path)
parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'GBAFeather/src/generated/LocalBios.h')
args = parser.parse_args()
data = args.bios.read_bytes()
if len(data) != 16384:
    parser.error(f'Expected a 16384-byte GBA BIOS, got {len(data)} bytes')
digest = hashlib.sha256(data).hexdigest()
lines = ['#pragma once', '// Private user-supplied BIOS. No redistribution licence asserted.',
         '// SHA256: ' + digest, 'static const unsigned char local_gba_bios_bin[] = {']
lines += ['  ' + ','.join(f'0x{value:02x}' for value in data[i:i+16]) + ',' for i in range(0, len(data), 16)]
lines += ['};', 'static_assert(sizeof(local_gba_bios_bin) == 16384);', '']
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text('\n'.join(lines), encoding='utf8')
print(f'Private BIOS: {len(data)} bytes, SHA256 {digest}')
print(f'Generated: {args.output}')
