import struct
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path


EXECUTABLE = Path(__file__).resolve().parents[1] / "framewatch"


def frame(sequence, timestamp, sensor, value):
    payload = struct.pack(">2sBBIQHfH", b"FW", 1, 28, sequence, timestamp, sensor, value, 0)
    return payload + struct.pack(">I", zlib.crc32(payload))


def run_frames(data, minimum="0", maximum="100"):
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "sample.bin"
        path.write_bytes(data)
        return subprocess.run(
            [str(EXECUTABLE), str(path), minimum, maximum],
            capture_output=True, text=True, check=False,
        )


class FrameWatchTests(unittest.TestCase):
    def test_nominal_frames(self):
        result = run_frames(frame(1, 100, 7, 25) + frame(2, 101, 7, 30))
        self.assertEqual(result.returncode, 0)
        self.assertIn("frames=2 valid=2 corrupt=0", result.stdout)

    def test_crc_failure_does_not_advance_sequence(self):
        damaged = bytearray(frame(2, 101, 7, 30))
        damaged[20] ^= 1
        result = run_frames(frame(1, 100, 7, 25) + damaged + frame(3, 102, 7, 30))
        self.assertEqual(result.returncode, 2)
        self.assertIn("corrupt=1 missing_sequences=1", result.stdout)

    def test_threshold_gap_and_timestamp_regression(self):
        result = run_frames(frame(1, 100, 7, 25) + frame(3, 99, 7, 120))
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing_sequences=1 regressions=1 threshold_alerts=1", result.stdout)

    def test_truncated_frame(self):
        result = run_frames(frame(1, 100, 7, 25) + b"FW")
        self.assertEqual(result.returncode, 2)
        self.assertIn("corrupt=1", result.stdout)
        self.assertIn("Truncated frame", result.stderr)

    def test_bad_payload_and_arguments(self):
        result = run_frames(frame(1, 100, 7, float("nan")))
        self.assertEqual(result.returncode, 2)
        self.assertIn("Invalid payload", result.stderr)
        self.assertEqual(run_frames(b"", "10", "0").returncode, 3)
        self.assertEqual(run_frames(b"", "nan", "100").returncode, 3)

    def test_missing_input(self):
        result = subprocess.run(
            [str(EXECUTABLE), "/path/that/does/not/exist", "0", "100"],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 3)


if __name__ == "__main__":
    unittest.main()
