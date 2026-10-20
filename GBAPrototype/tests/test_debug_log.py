"""Independent PC-side checks for the firmware's binary logging protocol."""
import binascii
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zlib
from datetime import datetime, timezone

spec=importlib.util.spec_from_file_location('debug_log',Path(__file__).resolve().parents[1]/'tools/debug_log.py')
debug=importlib.util.module_from_spec(spec);spec.loader.exec_module(debug)
fixture_path=Path(sys.argv.pop(1)) if len(sys.argv)>1 and not sys.argv[1].startswith('-') else None

def record(kind,values):
    mask=sum(1<<i for i,n in enumerate(values) if n)
    payload=bytearray([kind])+struct.pack('<I',mask)
    for n in values:
        if not n:continue
        while n>=128:payload.append((n&127)|128);n>>=7
        payload.append(n)
    prior=struct.pack('<H',len(payload)+4)+payload
    return prior+struct.pack('<H',binascii.crc_hqx(prior,0xffff))

def packet(records,fingerprint=0):
    payload=b''.join(records)
    header=debug.MAGIC+struct.pack('<4H11I',1,64,32,16,len(payload),len(records),0,
        240000000,30000000,fingerprint,21,767,1188,0,12345)
    return header+struct.pack('<I',zlib.crc32(header))+payload+debug.END

class ProtocolTests(unittest.TestCase):
    def test_block_mode_and_control_status(self):
        values=[0]*32;values[27]=32
        _,frames,_=debug.decode(packet([record(1,values)]))
        self.assertEqual(frames[0]['blocks_on'],1)
        values[27]=64
        _,frames,_=debug.decode(packet([record(1,values)]))
        self.assertEqual(frames[0]['alu_burst_on'],1)
        self.assertEqual(frames[0]['blocks_on'],0)
        values[27]=128
        _,frames,_=debug.decode(packet([record(1,values)]))
        self.assertEqual(frames[0]['leaf_fetch_on'],1)
        self.assertEqual(frames[0]['alu_burst_on'],0)
        values[27]=0
        _,frames,_=debug.decode(packet([record(1,values)]))
        self.assertEqual(frames[0]['blocks_on'],0)
        status_spec=importlib.util.spec_from_file_location('block_control',Path(__file__).resolve().parents[1]/'tools/block_control.py')
        control=importlib.util.module_from_spec(status_spec);status_spec.loader.exec_module(control)
        parsed=control.parse_status('BLK 1 00000002 ffffffff 00000003 00000004 00000005 00000000\n')
        self.assertTrue(parsed['enabled']);self.assertEqual(parsed['hits'],0xffffffff)
        self.assertEqual(parsed['instructions'],5)
        self.assertTrue(control.parse_burst_status('BST 1\n')['enabled'])
        self.assertFalse(control.parse_burst_status('BST 0\n')['enabled'])
        with self.assertRaises(ValueError):control.parse_burst_status('BST 2')
        self.assertTrue(control.parse_fetch_status('FET 1\n')['enabled'])
        self.assertFalse(control.parse_fetch_status('FET 0\n')['enabled'])
        with self.assertRaises(ValueError):control.parse_fetch_status('FET 2')
        for bad in ['BLK 2 0 0 0 0 0 0','BLK 0 0 0 0 0 0 100000000','BLK 0 0 0','dump']:
            with self.assertRaises(ValueError):control.parse_status(bad)

    def test_exact_map_discovery_across_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory)
            for version in (41,42):
                row=dict(begin=0x20000000+version*32,end=0x20000004+version*32,
                    label=f'version{version}',name=f'function_v{version}')
                (folder/f'pc-profile-map-v{version}.json').write_text(json.dumps(dict(functions=[row])))
            old=folder/'pc-profile-map-v41.json';new=folder/'pc-profile-map-v42.json'
            self.assertEqual(debug.matching_map(debug.map_records(old)[1],folder),old)
            self.assertEqual(debug.matching_map(debug.map_records(new)[1],folder),new)
            self.assertIsNone(debug.matching_map(0,folder))
            self.assertIsNone(debug.matching_map(1234,folder))

    def test_offline_chart_report_and_pause_alignment(self):
        chart_spec=importlib.util.spec_from_file_location('debug_charts',Path(__file__).resolve().parents[1]/'tools/debug_charts.py')
        charts=importlib.util.module_from_spec(chart_spec);chart_spec.loader.exec_module(charts)
        values=[0]*32;values[0]=7;values[1]=10000;values[2]=100000
        pc=[0]*16;pc[0]=7;pc[1]=10000;pc[9]=pc[11]=pc[13]=0xffff
        h,f,p=debug.decode(packet([record(1,values),record(2,pc)]))
        p[0]['function1']='<unsafe-label>'
        with tempfile.TemporaryDirectory() as directory:
            path=charts.write_report(Path(directory)/'chart.html',h,f,p,debug.summarize(h,f,p))
            text=path.read_text(encoding='utf-8')
            self.assertNotIn('__GBA_LOG_DATA__',text)
            self.assertNotIn('<unsafe-label>',text)
            self.assertEqual(text.count('<canvas '),4)
            payload=json.loads(text.split('<script type="application/json" id="capture-data">')[1].split('</script>')[0])
            self.assertEqual(payload['frames'][0]['t'],.1)
            self.assertEqual(payload['profiles'][0]['t'],.1)

    def test_timestamped_capture_folder_and_collision(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory)/'logs'
            when=datetime(2026,10,19,22,12,34,567000,tzinfo=timezone.utc)
            first=debug.save_capture(b'one',Path('gba-log.bin'),folder,when)
            second=debug.save_capture(b'two',Path('gba-log.bin'),folder,when)
            local=when.astimezone().strftime('%Y-%m-%d_%H-%M-%S')+'-567'
            self.assertEqual(first,folder/f'gba-log-{local}.bin')
            self.assertEqual(second,folder/f'gba-log-{local}-01.bin')
            self.assertEqual(first.read_bytes(),b'one')
            self.assertEqual(second.read_bytes(),b'two')

    def test_explicit_capture_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'chosen'/'sonic.bin'
            saved=debug.save_capture(b'test',target,Path(directory)/'unused')
            self.assertEqual(saved.parent,target.parent)
            self.assertTrue(saved.name.startswith('sonic-'))

    def test_cpp_fixture(self):
        if fixture_path is None:
            self.skipTest('Supply the synthetic gba_debug_log_test fixture for C++/Python round-trip')
        header,frames,profiles=debug.decode(fixture_path.read_bytes())
        self.assertEqual(header['dependent_psram256_ns'],1188)
        self.assertEqual((len(frames),len(profiles)),(1,1))
        self.assertEqual(frames[0]['run_ms'],53)
        self.assertEqual(frames[0]['draw_ms'],18.3)
        self.assertEqual(frames[0]['partition_valid'],1)
        self.assertEqual(profiles[0]['function1'],'none')

    def test_varint_boundaries_and_mask_bit31(self):
        values=[0,1,127,128,16383,16384,0x1fffff,0x200000,0xfffffff,0x10000000,0xffffffff]*3
        values=values[:32]
        header,frames,_=debug.decode(packet([record(1,values)]))
        self.assertEqual([frames[0][key] for key in debug.FRAME_FIELDS],values)

    def test_empty(self):
        self.assertEqual(debug.decode(packet([]))[1:],([],[]))

    def test_corruption_and_truncation(self):
        original=packet([record(1,[1]*32)])
        for offset in (0,9,16,60,64,66,70,len(original)-1):
            damaged=bytearray(original);damaged[offset]^=1
            with self.assertRaises(ValueError):debug.decode(bytes(damaged))
        for cut in (0,10,63,64,70,len(original)-1):
            with self.assertRaises(ValueError):debug.decode(original[:cut])

    def test_map_fingerprint_and_exact_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'map.json'
            path.write_text(json.dumps({'functions':[{'begin':0x20001000,'end':0x20001020,'label':'CPU dispatch','name':'gba::Arm7tdmi::cpu_run_until()'}]}))
            _,fingerprint=debug.map_records(path)
            values=[0]*16;values[8]=1;values[9]=0;values[10]=20;values[11]=values[13]=0xffff
            _,_,profiles=debug.decode(packet([record(2,values)],fingerprint),path)
            self.assertEqual(profiles[0]['function1'],'gba::Arm7tdmi::cpu_run_until()')
            with self.assertRaisesRegex(ValueError,'map does not match'):
                debug.decode(packet([record(2,values)],fingerprint^1),path)

    def test_malformed_valid_crc_record(self):
        # A valid envelope/checksum must not hide an overflowing u32 varint.
        payload=b'\x01'+struct.pack('<I',1)+b'\xff\xff\xff\xff\x10'
        prior=struct.pack('<H',len(payload)+4)+payload
        bad=prior+struct.pack('<H',binascii.crc_hqx(prior,0xffff))
        with self.assertRaisesRegex(ValueError,'exceeds uint32'):debug.decode(packet([bad]))

    def test_summary_weighted_fps(self):
        frame=[0]*32;frame[0]=1;frame[2]=100000
        second=list(frame);second[0]=2;second[2]=300000
        h,f,p=debug.decode(packet([record(1,frame),record(1,second)]))
        summary=debug.summarize(h,f,p)
        self.assertEqual(summary['weighted_fps'],5)
        self.assertEqual(summary['worst_frame_ms'],300)

    def test_usb_partial_reads(self):
        original=packet([record(1,[1]*32)])
        class Port:
            def __init__(self):self.offset=0;self.written=b''
            def __enter__(self):return self
            def __exit__(self,*args):pass
            def reset_input_buffer(self):pass
            def write(self,data):self.written+=data;return len(data)
            def read(self,n):
                part=original[self.offset:self.offset+min(n,7)];self.offset+=len(part);return part
        port=Port()
        with patch.dict(sys.modules,{'serial':types.SimpleNamespace(Serial=lambda *a,**k:port)}):
            self.assertEqual(debug.download('TEST',5),original)
        self.assertEqual(port.written,b'cancel\ndump\n')

if __name__=='__main__':unittest.main()
