"""Read HDMI hardware status or request a clean HSTX restart (V49 or newer)."""
import argparse
import json
import time

FIELDS = ("frames", "irq_per_second", "irq_ms_per_second", "restarts",
          "sys_hz", "hstx_hz", "hstx_csr", "fifo_status", "framebuffer", "pixel_4_4")


def parse_status(line):
    parts = line.strip().split()
    if len(parts) != len(FIELDS) + 1 or parts[0] != "VID":
        raise ValueError("Invalid HDMI status response")
    values = [int(v, 16) for v in parts[1:]]
    if any(v < 0 or v > 0xffffffff for v in values):
        raise ValueError("HDMI status value outside u32 range")
    return dict(zip(FIELDS, values))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--restart", action="store_true", help="Restart video before reading status")
    args = parser.parse_args()
    import serial
    with serial.Serial(args.port, 115200, timeout=0.25, write_timeout=1) as device:
        device.reset_input_buffer()
        device.write(b"cancel\n")
        if args.restart:
            device.write(b"video restart\n")
        deadline = time.monotonic() + 15
        next_query = 0
        while time.monotonic() < deadline:
            if time.monotonic() >= next_query:
                device.write(b"video status\n")
                next_query = time.monotonic() + 1
            line = device.readline().decode("ascii", errors="replace")
            if line.startswith("VID "):
                print(json.dumps(parse_status(line), indent=2))
                return
    raise TimeoutError("No HDMI status; use V49+, check port, and close other serial programs")


if __name__ == "__main__":
    main()
