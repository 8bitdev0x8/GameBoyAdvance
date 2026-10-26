"""Build/verify a fixed-size private PC-to-function map from the exact ARM ELF."""
import argparse,json,re,subprocess
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--elf',type=Path)
parser.add_argument('--nm',type=Path)
parser.add_argument('--empty',action='store_true')
parser.add_argument('--check',action='store_true')
args=parser.parse_args()
project=Path(__file__).resolve().parents[1]
header=project/'GBAFeather/src/generated/PcProfileRanges.h'
manifest=project/'GBAFeather/src/generated/pc-profile-map.json'
capacity=1152

def short(name):
    fixed=[('Arm7tdmi::try_run_thumb_block','Thumb block execute'),
           ('Arm7tdmi::cpu_run_until','CPU dispatch'),('Arm7tdmi::execute_thumb','CPU Thumb'),
           ('Arm7tdmi::execute_arm','CPU ARM'),('Arm7tdmi::fetch_thumb','CPU fetch Thumb'),
           ('Arm7tdmi::fetch_arm','CPU fetch ARM'),('apply_shift','CPU shift'),
           ('Arm7tdmi::account_data_access','CPU bus account'),('Arm7tdmi::try_skip','CPU idle recognition'),
           ('Bus::read_gamepak_rom','Bus ROM'),
           ('read_code_specialized<(gba::BusWidth)2>','Fetch Thumb fixed'),
           ('read_code_specialized<(gba::BusWidth)4>','Fetch ARM fixed'),
           ('Bus::read_code','Bus code fetch'),
           ('Bus::read_io','Bus IO read'),('Bus::write_io','Bus IO write'),('Bus::read(','Bus read'),
           ('Bus::write(','Bus write'),('ScanlineWorker::wait','Render wait'),
           ('Cartridge::read_rom','Cartridge ROM'),('CompressedRom::read_hot','ROM hot cache'),
           ('Emulator::run_until','Event dispatch'),('gfx_dma_handler','HDMI handler'),
           ('countVideoIrq','HDMI IRQ wrapper'),('prefetch','ROM prefetch')]
    for key,label in fixed:
        if key in name:return label
    name=name.replace('gba::','').replace('(anonymous namespace)::','').split('(')[0]
    return name[:23]

records=[]
if not args.empty:
    if not args.elf or not args.nm:parser.error('--elf and --nm required')
    def nm(demangle):
        return subprocess.check_output([str(args.nm),'-S','-n']+(['-C'] if demangle else [])+[str(args.elf)],text=True)
    def parse(text):
        for line in text.splitlines():
            match=re.match(r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([tTwW])\s+(.+)$',line)
            if match:
                address,size=int(match[1],16),int(match[2],16)
                if size and (0x10000000<=address<0x10800000 or 0x20000000<=address<0x20082000):
                    yield address,size,match[4]
    names={(address,size):name for address,size,name in parse(nm(True))}
    unique={}
    for address,size,symbol in parse(nm(False)):
        if address not in unique or size>unique[address]['end']-address:
            name=names.get((address,size),symbol)
            unique[address]=dict(begin=address,end=address+size,label=short(name),name=name,symbol=symbol)
    records=sorted(unique.values(),key=lambda value:value['begin'])
    if len(records)>capacity:raise SystemExit(f'{len(records)} functions exceed {capacity} slots')
    for previous,current in zip(records,records[1:]):
        if previous['end']>current['begin']:raise SystemExit('Overlapping function symbols; cannot safely classify')

if args.check:
    old=json.loads(manifest.read_text(encoding='utf8'))
    if records!=old['functions']:raise SystemExit('PC map does not match final ELF; rebuild the map and firmware')
    print(f'PASS: {len(records)} PC function ranges match the final ELF exactly')
else:
    header.parent.mkdir(parents=True,exist_ok=True)
    lines=['// Generated private symbol map; fixed capacity keeps link addresses stable.',
           'const GbaPcRange gbaPcRanges[GBA_PC_RANGE_CAPACITY] = {']
    lines += ['  {0x%08xu,0x%08xu,%s},'%(item['begin'],item['end'],json.dumps(item['label'])) for item in records]
    lines += ['};',f'const uint32_t gbaPcRangeCount={len(records)}u;','']
    header.write_text('\n'.join(lines),encoding='utf8')
    manifest.write_text(json.dumps(dict(capacity=capacity,functions=records),indent=2)+'\n',encoding='utf8')
    print(f'Generated {len(records)} PC ranges in a fixed {capacity*32}-byte flash table')
