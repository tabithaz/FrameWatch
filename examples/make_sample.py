"""Generate a repeatable binary sample for the FrameWatch CLI."""

import struct
import zlib
from pathlib import Path


def frame(sequence, timestamp_ms, sensor, value):
    payload = struct.pack(">2sBBIQHfH", b"FW", 1, 28, sequence, timestamp_ms, sensor, value, 0)
    return payload + struct.pack(">I", zlib.crc32(payload))


sample = b"".join([
    frame(1, 1_000, 4, 72.5),
    frame(2, 1_100, 4, 74.0),
    frame(4, 1_200, 4, 130.0),
])
destination = Path(__file__).with_name("sample.bin")
destination.write_bytes(sample)
print(destination)
