"""Set/query CPU native translation, blocks, burst, fetch or memory paths over USB."""
import argparse
import json
from pathlib import Path
import time


def parse_status(line: str) -> dict:
    parts = line.strip().split()
    if len(parts) != 8 or parts[0] != "BLK" or parts[1] not in ("0", "1"):
        raise ValueError("Invalid block status response")
    values = [int(item, 16) for item in parts[2:]]
    if any(value < 0 or value > 0xFFFFFFFF for value in values):
        raise ValueError("Counter outside u32 range")
    return dict(enabled=parts[1] == "1", **dict(zip(
        ("lookups", "hits", "misses", "batches", "instructions", "mismatches"), values)))


def parse_burst_status(line: str) -> dict:
    parts=line.strip().split()
    if len(parts)!=2 or parts[0]!='BST' or parts[1] not in ('0','1'):
        raise ValueError('Invalid ALU burst status response')
    return dict(enabled=parts[1]=='1')


def parse_fetch_status(line: str) -> dict:
    parts=line.strip().split()
    if len(parts)!=2 or parts[0]!='FET' or parts[1] not in ('0','1'):
        raise ValueError('Invalid inline fetch status response')
    return dict(enabled=parts[1]=='1')


def parse_memory_status(line: str) -> dict:
    parts=line.strip().split()
    if len(parts)!=2 or parts[0]!='MEM' or parts[1] not in ('0','1'):
        raise ValueError('Invalid memory-path status response')
    return dict(enabled=parts[1]=='1')


def parse_native_status(line: str) -> dict:
    parts=line.strip().split()
    if len(parts)!=9 or parts[0]!='JIT' or parts[1] not in ('0','1'):
        raise ValueError('Invalid native translation status response')
    values=[int(p,16) for p in parts[2:]]
    if any(v<0 or v>0xffffffff for v in values):raise ValueError('Counter outside u32 range')
    return dict(enabled=parts[1]=='1',**dict(zip(
        ('lookups','compiled','batches','instructions','memory','exits','mismatches'),values)))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--mode", choices=("on", "off", "status"), default="status")
    parser.add_argument("--engine", choices=("blocks", "burst", "fetch", "memory", "native"), default="blocks")
    parser.add_argument("--output", type=Path, help="Optional new JSON status file")
    args = parser.parse_args()
    if args.output and args.output.exists():
        raise FileExistsError(args.output)
    import serial
    with serial.Serial(args.port, 115200, timeout=0.25, write_timeout=1) as device:
        device.reset_input_buffer()
        device.write(b"cancel\n")
        if args.mode != "status":
            device.write(f"{args.engine} {args.mode}\n".encode("ascii"))
        deadline = time.monotonic() + 15
        next_query = 0
        while time.monotonic() < deadline:
            if time.monotonic() >= next_query:
                device.write(f"{args.engine} status\n".encode("ascii"))
                next_query = time.monotonic() + 1
            line = device.readline().decode("ascii", errors="replace")
            prefix={"blocks":"BLK ","burst":"BST ","fetch":"FET ","memory":"MEM ","native":"JIT "}[args.engine]
            if not line.startswith(prefix):
                continue
            status = {"blocks":parse_status,"burst":parse_burst_status,"fetch":parse_fetch_status,"memory":parse_memory_status,"native":parse_native_status}[args.engine](line)
            if args.mode != "status" and status["enabled"] != (args.mode == "on"):
                raise RuntimeError("Firmware did not apply requested execution mode")
            text = json.dumps(status, indent=2) + "\n"
            print(text, end="")
            if args.output:
                with args.output.open("x", encoding="utf-8") as output:
                    output.write(text)
            return
    raise TimeoutError("No execution-mode status; close other serial programs and check firmware/port")


if __name__ == "__main__":
    main()
