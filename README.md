# FrameWatch

FrameWatch is a C11 command-line decoder for fixed-size binary telemetry frames. It verifies CRC-32, checks sequence gaps and timestamp regressions, and flags sensor values outside a chosen range. Parsing is streaming: it reads one 28-byte frame at a time rather than loading the file into memory.

## Build and run

Requires a C11 compiler, Make, and Python 3 for the sample generator and tests.

```bash
make
python3 examples/make_sample.py
./framewatch examples/sample.bin 0 100
make test
```

The sample has one missing sequence and one threshold alert, so the command exits with status 2. Status 0 means all frames passed; status 3 means invalid arguments or an I/O error. Diagnostics include record numbers for damaged frames.

## Wire format

Each frame is exactly 28 bytes. Multi-byte integers and the IEEE 754 float are stored in big-endian order.

| Offset | Bytes | Field |
| ---: | ---: | --- |
| 0 | 2 | Magic `FW` |
| 2 | 1 | Version `1` |
| 3 | 1 | Frame length `28` |
| 4 | 4 | Sequence number |
| 8 | 8 | Timestamp in milliseconds |
| 16 | 2 | Sensor ID |
| 18 | 4 | Float32 reading |
| 22 | 2 | Reserved, zero |
| 24 | 4 | CRC-32 of bytes 0–23 |

This is an original synthetic format for demonstrating binary parsing; it does not claim compatibility with a spacecraft protocol. Invalid frames are excluded from the sequence and timestamp baseline. A skipped sequence after an invalid frame is reported as missing. The decoder assumes one globally ordered stream; separate sensors are identified in alerts but do not maintain independent sequence counters.

## Tests

`make test` generates frames in memory and checks nominal processing, CRC damage, truncated input, invalid float values, sequence gaps, timestamp regressions, threshold alerts, argument validation, and missing files. The tests use Python's standard library and do not require external services.
