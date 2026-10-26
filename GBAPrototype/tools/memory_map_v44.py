import json
from pathlib import Path
root=Path(__file__).resolve().parents[2];project=root/'GBAPrototype'
source=project/'output/pc-profile-map-v44.json'
if not source.exists():source=root/'docs/evidence/pc-profile-map-v44.json'
mapping=json.loads(source.read_text())
def code_size(low,high):
    spans=sorted((r['begin'],r['end']) for r in mapping['functions'] if low<=r['begin']<high)
    previous=low;total=0
    for begin,end in spans:
        assert begin>=previous,('Overlapping function ranges',begin,previous)
        total+=end-begin;previous=end
    return total
flash_code=code_size(0x10000000,0x10800000)
main_code=code_size(0x20000000,0x20080000)
scratch_code=code_size(0x20080000,0x20082000)
def leaf(name,value,note='',kind='',short=None):
    return dict(name=name,value=value,note=note,kind=kind,short=short or name)
def group(name,children,note=''):
    return dict(name=name,children=children,note=note)
data=[
 group('PSRAM · external',[
    leaf('Decoded ROM cache',7208960,'6.875 MiB; 440 decoded 16 KiB blocks'),
    group('Game data and buffers',[
        leaf('GBA EWRAM',262144),leaf('Cartridge save data',131072,'Volatile; lost on reset'),
        leaf('GBA RGB555 framebuffer',76800,'Retained allocation; direct output bypasses full-frame copying'),
        leaf('HDMI recovery backup',153600,'Backup only; live HDMI framebuffer is in internal SRAM'),
        leaf('Binary debug logs',24576)]),
    leaf('Arena spare',7168,'Known remaining project-arena capacity','free'),
    leaf('Outside project arena',524288,'Reserved for SDK; actual usage/free capacity not measured','unknown')]),
 group('QSPI flash · external',[
    leaf('Compressed Sonic ROM',4753067,'Original cartridge: 16 MiB'),
    group('Native code',[
        leaf('Code executing from flash',flash_code,'Union of exact ELF function ranges'),
        leaf('SRAM code load image',main_code+scratch_code,'Stored in flash; copied into SRAM at startup')]),
    leaf('GBA BIOS',16384),
    leaf('Tables, data and padding',5079296-4753067-16384-flash_code-main_code-scratch_code,'Includes ROM index, PC map, initialized data, vectors and gaps'),
    leaf('Unused image address space',8380416-5079296,'Within configured application flash region','free'),
    leaf('Reserved flash tail',8192,'Outside configured application region','unknown')]),
 group('SRAM · internal',[
    leaf('Live HDMI framebuffer',153600,'320 × 240 × 2 bytes; display driver uses ordinary malloc'),
    group('Emulator state and workspace',[
        leaf('Tile-index caches',49152,'Two 2,048-entry caches, 12 bytes per entry'),
        leaf('Sprite line lists',20640,'160 × 128 indices plus per-line counts'),
        leaf('Scanline pixel workspace',3120,'BG/OBJ rows, priorities, transparency and OBJ-window bits'),
        leaf('Optional decoded blocks',3328,'Allocated even with CPU blocks OFF'),
        leaf('CPU/device state and metadata',10672,'Remainder of the 86,912-byte emulator object')]),
    group('Guest fast memory',[
        leaf('GBA VRAM',98304),leaf('GBA IWRAM',32768),leaf('Palette + OAM',2048),leaf('Bus wait tables',64)]),
    leaf('Hot ROM line cache',32768),
    leaf('Native executable code',main_code+scratch_code,'Includes 2,236 bytes of HDMI/DMA code in scratch X'),
    group('Stacks',[
        leaf('Core 0 stack reserve',16384),leaf('Core 1 stack + canary',8256)]),
    leaf('ROM metadata + code window',5124,'CompressedRom object; includes stable 256-byte instruction window'),
    leaf('Other static data',244252-main_code-133184-32768-8256,'Includes render worker, profiler, UART/USB control and SDK globals'),
    leaf('Other heap use + alignment',6928,'Residual accounting; includes driver scanline buffers and allocator overhead'),
    leaf('Scratch reserve / SDK stack area',8192-scratch_code,'Not part of the reported free heap','unknown'),
    leaf('Reported free heap',11088,'Cached measurement in latest V44 logs; not peak stack headroom','free')]),
 group('OTP · internal',[
    leaf('Provisioning occupancy unknown',8192,'8 KiB capacity; project does not provision OTP. Factory/configuration usage was not read.','unknown')])]
def total(node):return sum(total(n) for n in node['children']) if 'children'in node else node['value']
assert [total(n) for n in data]==[8388608,8388608,532480,8192]
def categories(node):
    if 'children' in node:
        for child in node['children']:categories(child)
        kinds={child['kind'] for child in node['children']};node['kind']=next(iter(kinds)) if len(kinds)==1 else 'data'
    elif not node['kind']:
        name=node['name'].lower()
        if name in ['decoded rom cache','compressed sonic rom','gba bios','hot rom line cache']:node['kind']='rom'
        elif name in ['code executing from flash','sram code load image','native executable code']:node['kind']='code'
        elif 'framebuffer' in name or 'hdmi recovery' in name:node['kind']='video'
        else:node['kind']='data'
for node in data:categories(node)
path=root/'docs/assets/rp2350-memory.fragment.html'
text=path.read_text(encoding='utf-8');start=text.index('<script type="application/json" id="rp2350-memory-data">');end=text.index('</script>',start)
text=text[:start]+'<script type="application/json" id="rp2350-memory-data">'+json.dumps(data,ensure_ascii=False)+text[end:]
path.write_text(text,encoding='utf-8')
(root/'docs/evidence/memory-map-v44.json').write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print('PASS exact capacities and non-overlapping code symbol ranges; internal live HDMI buffer confirmed from driver malloc and core allocator source')
