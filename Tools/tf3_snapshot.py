#!/usr/bin/env python3
"""Read-only Tag Force 3 menu snapshots through PPSSPP's WebSocket debugger."""

import argparse
import base64
import csv
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import struct
import tempfile
import time


GAME_ID = "ULES01183"
LABELS = ("A", "B", "C", "B", "A")
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
ALLOWED_EVENTS = {
    "version", "game.status", "memory.mapping", "memory.read", "cpu.status",
    "cpu.stepping", "cpu.resume", "gpu.buffer.screenshot",
}
REPO_ROOT = Path(__file__).resolve().parent.parent


class CaptureError(RuntimeError):
    pass


def read_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise CaptureError("Debugger connection closed")
        data.extend(chunk)
    return bytes(data)


class Debugger:
    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=30)
        self.sock.settimeout(30)
        key = base64.b64encode(secrets.token_bytes(16)).decode("ascii")
        request = (
            f"GET /debugger HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Protocol: debugger.ppsspp.org\r\n\r\n"
        )
        self.sock.sendall(request.encode("ascii"))
        response = bytearray()
        while b"\r\n\r\n" not in response:
            if len(response) > 16384:
                raise CaptureError("Oversized WebSocket handshake")
            response.extend(read_exact(self.sock, 1))
        header = response.decode("ascii", "replace")
        expected = base64.b64encode(hashlib.sha1(
            (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")
        ).digest()).decode("ascii")
        if not header.startswith("HTTP/1.1 101 ") or f"sec-websocket-accept: {expected.lower()}" not in header.lower():
            raise CaptureError("Debugger WebSocket handshake failed")
        if "sec-websocket-protocol: debugger.ppsspp.org" not in header.lower():
            raise CaptureError("Unexpected WebSocket subprotocol")
        self.ticket = 0

    def close(self):
        self.sock.close()

    def send_frame(self, opcode, payload):
        mask = secrets.token_bytes(4)
        size = len(payload)
        header = bytearray([0x80 | opcode])
        if size < 126:
            header.append(0x80 | size)
        elif size < 65536:
            header.extend([0x80 | 126])
            header.extend(struct.pack("!H", size))
        else:
            header.extend([0x80 | 127])
            header.extend(struct.pack("!Q", size))
        header.extend(mask)
        self.sock.sendall(header + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def receive(self):
        fragments = bytearray()
        while True:
            first, second = read_exact(self.sock, 2)
            opcode = first & 15
            size = second & 127
            if size == 126:
                size = struct.unpack("!H", read_exact(self.sock, 2))[0]
            elif size == 127:
                size = struct.unpack("!Q", read_exact(self.sock, 8))[0]
            if size > 8 * 1024 * 1024 or len(fragments) + size > 8 * 1024 * 1024:
                raise CaptureError("Oversized debugger message")
            mask = read_exact(self.sock, 4) if second & 0x80 else None
            payload = read_exact(self.sock, size)
            if mask:
                payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
            if opcode == 8:
                raise CaptureError("Debugger closed the WebSocket")
            if opcode == 9:
                self.send_frame(10, payload)
                continue
            if opcode == 10:
                continue
            if opcode not in (0, 1):
                raise CaptureError(f"Unexpected WebSocket opcode {opcode}")
            fragments.extend(payload)
            if first & 0x80:
                try:
                    return json.loads(fragments.decode("utf-8"))
                except (UnicodeError, ValueError) as exc:
                    raise CaptureError("Invalid debugger JSON") from exc

    def send(self, event, **params):
        if event not in ALLOWED_EVENTS:
            raise CaptureError(f"Disallowed debugger event: {event}")
        self.ticket += 1
        message = {"event": event, "ticket": self.ticket, **params}
        self.send_frame(1, json.dumps(message, separators=(",", ":")).encode("utf-8"))
        return self.ticket

    def request(self, event, **params):
        ticket = self.send(event, **params)
        while True:
            message = self.receive()
            if message.get("ticket") != ticket:
                continue
            if message.get("event") == "error":
                raise CaptureError(f"{event}: {message.get('message', 'debugger error')}")
            if message.get("event") != event:
                raise CaptureError(f"Unexpected response to {event}")
            return message

    def wait_event(self, event):
        while True:
            message = self.receive()
            if message.get("event") == event:
                return message
            if message.get("event") == "error":
                raise CaptureError(message.get("message", "debugger error"))


def check_game(status, version, expected_version=None):
    game = status.get("game")
    if not isinstance(game, dict) or game.get("id") != GAME_ID:
        raise CaptureError(f"Expected {GAME_ID}; debugger reports {game!r}")
    disc_version = game.get("version")
    if not isinstance(disc_version, str) or not disc_version:
        raise CaptureError("Missing disc version")
    if expected_version is not None and disc_version != expected_version:
        raise CaptureError("Disc version changed during capture")
    if status.get("paused") is not False:
        raise CaptureError("Leave the PPSSPP pause menu before capturing")
    if not isinstance(version.get("pid"), int) or not version.get("path"):
        raise CaptureError("Debugger did not identify its process and game path")
    return disc_version


def check_iso_path(reported, expected):
    if os.path.normcase(str(Path(reported).resolve())) != os.path.normcase(str(expected.resolve())):
        raise CaptureError(f"Debugger is running a different game: {reported}")


def user_memory_range(mapping, limit):
    matches = [r for r in mapping.get("ranges", []) if r.get("name") == "User Memory"
               and r.get("type") == "ram" and r.get("subtype") == "primary"]
    if len(matches) != 1:
        raise CaptureError("Debugger did not report one primary user RAM range")
    address, size = matches[0].get("address"), matches[0].get("size")
    if not isinstance(address, int) or not isinstance(size, int) or address < 0x08000000 or size <= 0 or size > limit:
        raise CaptureError("Invalid or oversized user RAM range")
    if address + size > 0x100000000:
        raise CaptureError("User RAM range overflows address space")
    return address, size


def decode_screenshot(reply):
    uri = reply.get("uri", "")
    prefix = "data:image/png;base64,"
    if not isinstance(uri, str) or not uri.startswith(prefix):
        raise CaptureError("Debugger screenshot was not a PNG data URI")
    try:
        png = base64.b64decode(uri[len(prefix):], validate=True)
    except ValueError as exc:
        raise CaptureError("Invalid screenshot base64") from exc
    if not png.startswith(PNG_MAGIC):
        raise CaptureError("Debugger screenshot is not PNG")
    return png


def capture_state(debugger, target, label, step, game_version, process, address, size):
    if target.exists():
        raise CaptureError(f"Refusing to overwrite {target}")
    stage = target.parent / f".capture-{secrets.token_hex(8)}"
    stage.mkdir()
    stopped = False
    try:
        status = debugger.request("game.status")
        check_game(status, process, game_version)
        if debugger.request("cpu.status").get("stepping") is True:
            raise CaptureError("CPU was already stopped before capture")
        stopped = True  # Resume even if the pause broadcast times out after the request was sent.
        debugger.send("cpu.stepping")
        debugger.wait_event("cpu.stepping")
        if debugger.request("cpu.status").get("stepping") is not True:
            raise CaptureError("CPU did not stop")
        png = decode_screenshot(debugger.request("gpu.buffer.screenshot"))
        (stage / "screen.png").write_bytes(png)
        digest = hashlib.sha256()
        with (stage / "user_ram.bin").open("wb") as output:
            for offset in range(0, size, 1024 * 1024):
                count = min(1024 * 1024, size - offset)
                reply = debugger.request("memory.read", address=address + offset, size=count)
                try:
                    data = base64.b64decode(reply["base64"], validate=True)
                except (KeyError, ValueError) as exc:
                    raise CaptureError("Invalid memory.read base64") from exc
                if len(data) != count:
                    raise CaptureError("Short memory.read response")
                output.write(data)
                digest.update(data)
        if debugger.request("cpu.status").get("stepping") is not True:
            raise CaptureError("CPU resumed during capture")
        check_game(debugger.request("game.status"), process, game_version)
        metadata = {
            "label": label, "step": step, "game_id": GAME_ID,
            "disc_version": game_version, "pid": process["pid"], "iso": process["path"],
            "address": address, "size": size, "sha256": digest.hexdigest(),
            "captured_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }
        (stage / "meta.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        stage.rename(target)
        return metadata
    finally:
        try:
            if stopped:
                debugger.send("cpu.resume")
                debugger.wait_event("cpu.resume")
        finally:
            if stage.exists():
                shutil.rmtree(stage)


def capture(args):
    iso = args.iso.resolve(strict=True)
    if not iso.is_file():
        raise CaptureError("ISO path is not a file")
    root = args.out.resolve()
    if root == REPO_ROOT or root.is_relative_to(REPO_ROOT):
        raise CaptureError("Capture output must be outside the Git workspace")
    root.mkdir(parents=True, exist_ok=True)
    session = root / time.strftime("TF3-%Y%m%d-%H%M%S", time.gmtime())
    session.mkdir()
    debugger = Debugger(args.port)
    try:
        process = debugger.request("version", name="tf3-snapshot", version="1")
        check_iso_path(process.get("path", ""), iso)
        game_version = check_game(debugger.request("game.status"), process)
        address, size = user_memory_range(debugger.request("memory.mapping"), args.max_bytes)
        (session / "session.json").write_text(json.dumps({
            "game_id": GAME_ID, "disc_version": game_version, "pid": process["pid"],
            "iso": process["path"], "address": address, "size": size,
        }, indent=2) + "\n", encoding="utf-8")
        print(f"Session: {session}\nDisc: {GAME_ID} v{game_version}; user RAM {size:,} bytes")
        print("Use one stable menu. Start on item A; move one item between prompts: A > B > C > B > A.")
        for step, label in enumerate(LABELS, 1):
            input(f"[{step}/5] Verify item {label} is highlighted and settled, then press Enter to capture...")
            target = session / f"{step:02d}_{label}"
            capture_state(debugger, target, label, step, game_version, process, address, size)
            print(f"Captured {target}")
        print(f"Capture complete. Compare with: python Tools/tf3_snapshot.py compare --session \"{session}\"")
    finally:
        debugger.close()


def load_snapshots(session):
    if not session.is_dir() or not (session / "session.json").is_file():
        raise CaptureError("Not a TF3 capture session")
    session_meta = json.loads((session / "session.json").read_text(encoding="utf-8"))
    if session_meta.get("game_id") != GAME_ID:
        raise CaptureError("Capture session has a different game ID")
    snapshots = []
    reference = None
    for step, label in enumerate(LABELS, 1):
        folder = session / f"{step:02d}_{label}"
        meta = json.loads((folder / "meta.json").read_text(encoding="utf-8"))
        data = (folder / "user_ram.bin").read_bytes()
        if meta.get("label") != label or meta.get("step") != step or meta.get("game_id") != GAME_ID:
            raise CaptureError(f"Invalid capture metadata in {folder}")
        if len(data) != meta.get("size") or hashlib.sha256(data).hexdigest() != meta.get("sha256"):
            raise CaptureError(f"Corrupt memory snapshot in {folder}")
        identity = (meta["disc_version"], meta["pid"], meta["iso"], meta["address"], meta["size"])
        if reference is None:
            reference = identity
        elif identity != reference:
            raise CaptureError("Snapshots have different game, process, or memory ranges")
        snapshots.append(data)
    if reference != (session_meta.get("disc_version"), session_meta.get("pid"),
                     session_meta.get("iso"), session_meta.get("address"), session_meta.get("size")):
        raise CaptureError("Capture session identity differs from snapshots")
    return reference[3], snapshots


def find_candidates(address, snapshots, bases=(0, 1), max_rows=1000):
    if len(snapshots) != 5 or len({len(s) for s in snapshots}) != 1:
        raise CaptureError("Expected five equal-size snapshots")
    rows = []
    totals = {1: 0, 2: 0, 4: 0}
    for width in (1, 2, 4):
        for base in bases:
            if base < 0 or base + 2 >= (1 << (8 * width)):
                continue
            patterns = [(base + delta).to_bytes(width, "little") for delta in (0, 1, 2, 1, 0)]
            start = 0
            while True:
                offset = snapshots[0].find(patterns[0], start)
                if offset < 0:
                    break
                start = offset + 1
                if offset % width:
                    continue
                if all(snapshots[i][offset:offset + width] == patterns[i] for i in range(1, 5)):
                    totals[width] += 1
                    if len(rows) < max_rows:
                        rows.append((address + offset, width, base))
    return rows, totals


def compare(args):
    session = args.session.resolve(strict=True)
    address, snapshots = load_snapshots(session)
    rows, totals = find_candidates(address, snapshots, tuple(args.base), args.max_rows)
    output = session / "candidates.csv"
    if output.exists():
        raise CaptureError(f"Refusing to overwrite {output}")
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("address", "width_bits", "A_value", "B_value", "C_value", "note"))
        for candidate_address, width, base in rows:
            writer.writerow((f"0x{candidate_address:08X}", width * 8, base, base + 1, base + 2,
                             "unverified pattern; test in new session and other menu"))
    print(f"Candidates: 8-bit={totals[1]}, 16-bit={totals[2]}, 32-bit={totals[4]}")
    print(f"Wrote {len(rows)} preliminary rows to {output}")
    if sum(totals.values()) > len(rows):
        print("Output was capped; narrow the base values or raise --max-rows")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    cap = sub.add_parser("capture", help="capture A > B > C > B > A from a running TF3 debugger")
    cap.add_argument("--port", type=int, required=True)
    cap.add_argument("--iso", type=Path, required=True)
    cap.add_argument("--out", type=Path, default=Path(tempfile.gettempdir()) / "PPSSPP-Touch-TF3")
    cap.add_argument("--max-bytes", type=int, default=64 * 1024 * 1024)
    cap.set_defaults(action=capture)
    comp = sub.add_parser("compare", help="find preliminary 0/1/2/1/0 or 1/2/3/2/1 indices")
    comp.add_argument("--session", type=Path, required=True)
    comp.add_argument("--base", type=int, action="append", default=[])
    comp.add_argument("--max-rows", type=int, default=1000)
    comp.set_defaults(action=compare)
    args = parser.parse_args()
    if args.command == "capture" and not 1 <= args.port <= 65535:
        parser.error("--port must be 1..65535")
    if args.command == "capture" and not 0 < args.max_bytes <= 64 * 1024 * 1024:
        parser.error("--max-bytes must be 1..67108864")
    if args.command == "compare" and not args.base:
        args.base = [0, 1]
    if args.command == "compare" and args.max_rows < 1:
        parser.error("--max-rows must be positive")
    try:
        args.action(args)
    except (CaptureError, OSError, ValueError, KeyError, socket.timeout) as exc:
        parser.exit(1, f"Error: {exc}\n")


if __name__ == "__main__":
    main()
