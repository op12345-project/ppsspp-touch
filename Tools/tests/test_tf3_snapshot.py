"""Synthetic checks for the read-only Tag Force 3 snapshot workflow."""

import base64
from argparse import Namespace
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tf3_snapshot as tf3


PROCESS = {"pid": 123, "path": "C:/scratch/Tag Force 3.iso"}
ADDRESS = 0x08800000
PNG = tf3.PNG_MAGIC + b"synthetic"


class FakeDebugger:
    def __init__(self, data, short_read=False):
        self.data = data
        self.short_read = short_read
        self.stepping = False
        self.events = []

    def request(self, event, **params):
        self.events.append(event)
        if event == "game.status":
            return {"game": {"id": tf3.GAME_ID, "version": "1.00"}, "paused": False}
        if event == "cpu.status":
            return {"stepping": self.stepping}
        if event == "gpu.buffer.screenshot":
            return {"uri": "data:image/png;base64," + base64.b64encode(PNG).decode()}
        if event == "memory.read":
            data = self.data[params["address"] - ADDRESS:][:params["size"]]
            if self.short_read:
                data = data[:-1]
            return {"base64": base64.b64encode(data).decode()}
        raise AssertionError(event)

    def send(self, event, **params):
        self.events.append(event)
        if event == "cpu.stepping":
            self.stepping = True
        elif event == "cpu.resume":
            self.stepping = False
        else:
            raise AssertionError(event)

    def wait_event(self, event):
        self.events.append(f"wait:{event}")
        return {"event": event}

    def close(self):
        pass


class SnapshotTests(unittest.TestCase):
    def test_capture_reads_paused_ram_and_resumes(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            data = bytes(range(32))
            debugger = FakeDebugger(data)
            target = root / "01_A"
            metadata = tf3.capture_state(debugger, target, "A", 1, "1.00", PROCESS, ADDRESS, len(data))
            self.assertEqual((target / "user_ram.bin").read_bytes(), data)
            self.assertEqual((target / "screen.png").read_bytes(), PNG)
            self.assertEqual(metadata["sha256"], hashlib.sha256(data).hexdigest())
            self.assertFalse(debugger.stepping)
            self.assertLess(debugger.events.index("cpu.stepping"), debugger.events.index("memory.read"))
            self.assertLess(debugger.events.index("memory.read"), debugger.events.index("cpu.resume"))
            self.assertFalse(any("write" in event or event.startswith("input.") for event in debugger.events))

    def test_failed_read_cleans_partial_capture_and_resumes(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            debugger = FakeDebugger(bytes(range(32)), short_read=True)
            with self.assertRaisesRegex(tf3.CaptureError, "Short memory.read"):
                tf3.capture_state(debugger, root / "01_A", "A", 1, "1.00", PROCESS, ADDRESS, 32)
            self.assertEqual(list(root.iterdir()), [])
            self.assertFalse(debugger.stepping)
            self.assertNotIn("memory.write", debugger.events)

    def test_wrong_game_and_invalid_range_are_rejected(self):
        with self.assertRaisesRegex(tf3.CaptureError, "ULES01183"):
            tf3.check_game({"game": {"id": "ULES00600", "version": "1.01"}, "paused": False}, PROCESS)
        with self.assertRaisesRegex(tf3.CaptureError, "range"):
            tf3.user_memory_range({"ranges": [{"name": "User Memory", "type": "ram",
                                                 "subtype": "primary", "address": ADDRESS, "size": 0}]}, 64)
        with self.assertRaisesRegex(tf3.CaptureError, "Disallowed"):
            tf3.Debugger.send(object.__new__(tf3.Debugger), "memory.write", address=ADDRESS, size=1)

    def test_wrong_image_failure_does_not_touch_iso_or_psp_memory(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            iso = root / "game.iso"
            iso.write_bytes(b"unchanged game image")
            debugger = FakeDebugger(b"")
            debugger.request = lambda event, **params: {**PROCESS, "path": str(root / "other.iso")}
            args = Namespace(iso=iso, out=root / "captures", port=34567, max_bytes=64)
            with patch.object(tf3, "Debugger", return_value=debugger):
                with self.assertRaisesRegex(tf3.CaptureError, "different game"):
                    tf3.capture(args)
            self.assertEqual(iso.read_bytes(), b"unchanged game image")
            self.assertFalse(any("write" in event or event.startswith("input.") for event in debugger.events))

    def test_compare_finds_aligned_widths_and_rejects_nonreturn(self):
        snapshots = [bytearray([0xAA] * 64) for _ in range(5)]
        for index, value in enumerate((0, 1, 2, 1, 0)):
            snapshots[index][3] = value
            snapshots[index][8:10] = value.to_bytes(2, "little")
            snapshots[index][16:20] = value.to_bytes(4, "little")
            snapshots[index][24] = value if index != 4 else 1
        rows, totals = tf3.find_candidates(ADDRESS, snapshots)
        self.assertIn((ADDRESS + 3, 1, 0), rows)
        self.assertIn((ADDRESS + 8, 2, 0), rows)
        self.assertIn((ADDRESS + 16, 4, 0), rows)
        self.assertNotIn((ADDRESS + 24, 1, 0), rows)
        self.assertGreaterEqual(totals[1], 1)
        self.assertGreaterEqual(totals[2], 1)
        self.assertGreaterEqual(totals[4], 1)

    def test_snapshot_identity_and_hash_validation(self):
        with tempfile.TemporaryDirectory() as temp:
            session = Path(temp)
            identity = {"game_id": tf3.GAME_ID, "disc_version": "1.00", "pid": 123,
                        "iso": PROCESS["path"], "address": ADDRESS, "size": 4}
            (session / "session.json").write_text(json.dumps(identity), encoding="utf-8")
            for step, label in enumerate(tf3.LABELS, 1):
                folder = session / f"{step:02d}_{label}"
                folder.mkdir()
                data = bytes([step] * 4)
                (folder / "user_ram.bin").write_bytes(data)
                (folder / "meta.json").write_text(json.dumps({
                    **identity, "label": label, "step": step,
                    "sha256": hashlib.sha256(data).hexdigest(),
                }), encoding="utf-8")
            self.assertEqual(tf3.load_snapshots(session)[0], ADDRESS)
            (session / "03_C" / "user_ram.bin").write_bytes(b"bad!")
            with self.assertRaisesRegex(tf3.CaptureError, "Corrupt"):
                tf3.load_snapshots(session)


if __name__ == "__main__":
    unittest.main()
