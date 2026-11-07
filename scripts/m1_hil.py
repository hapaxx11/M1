#!/usr/bin/env python3
"""Hardware-in-the-loop control and firmware deployment over M1 native USB RPC."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import zlib

try:
    import serial
except ImportError:
    serial = None

SYNC = 0xAA
ACK = 0x06
NACK = 0x07
BUTTONS = {"ok": 0, "up": 1, "left": 2, "right": 3, "down": 4, "back": 5}


class RpcNackError(RuntimeError):
    def __init__(self, command: int, response: bytes):
        super().__init__(f"NACK for RPC 0x{command:02X}: {response.hex(' ')}")
        self.response = response


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def make_frame(command: int, sequence: int, payload: bytes = b"") -> bytes:
    body = bytes((command, sequence)) + struct.pack("<H", len(payload)) + payload
    return bytes((SYNC,)) + body + struct.pack("<H", crc16(body))


def write_png(path: Path, framebuffer: bytes, scale: int = 4) -> None:
    if len(framebuffer) != 1024:
        raise ValueError(f"expected 1024 framebuffer bytes, got {len(framebuffer)}")
    rows = []
    for y in range(64):
        row = bytearray((0,))
        for x in range(128):
            on = (framebuffer[x + (y // 8) * 128] >> (y % 8)) & 1
            row.extend(bytes((255 if on else 0,)) * scale)
        rows.extend((bytes(row),) * scale)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + kind
            + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
        )

    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 128 * scale, 64 * scale, 8, 0, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(b"".join(rows)))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


class M1Hil:
    def __init__(self, port: str, baud: int = 115200, discover: bool = True):
        if serial is None:
            raise RuntimeError("pyserial is required: python -m pip install pyserial")
        self.port = serial.Serial(port, baud, timeout=1)
        self.sequence = 0
        time.sleep(1)
        if discover:
            for attempt in range(8):
                try:
                    self.request(0x01, {0x02, ACK}, timeout=2)
                    break
                except TimeoutError:
                    if attempt == 7:
                        raise
                    time.sleep(0.5)
        self.port.reset_input_buffer()

    def close(self) -> None:
        self.port.close()

    def __enter__(self) -> "M1Hil":
        return self

    def __exit__(self, *args) -> None:
        self.close()

    def _read_exact(self, length: int, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        data = bytearray()
        while len(data) < length:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"RPC read timed out after {len(data)}/{length} bytes")
            data.extend(self.port.read(length - len(data)))
        return bytes(data)

    def _read_frame(self, timeout: float) -> tuple[int, int, bytes]:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("RPC response timed out")
            if self.port.read(1) != bytes((SYNC,)):
                continue
            header = self._read_exact(4, remaining)
            command, sequence, length = struct.unpack("<BBH", header)
            body = self._read_exact(length + 2, deadline - time.monotonic())
            received = struct.unpack("<H", body[-2:])[0]
            if crc16(header + body[:-2]) != received:
                raise ValueError("RPC CRC mismatch")
            return command, sequence, body[:-2]

    def request(
        self,
        command: int,
        expected: set[int] = {ACK},
        payload: bytes = b"",
        timeout: float = 10,
    ) -> bytes:
        self.sequence = (self.sequence + 1) & 0xFF
        sequence = self.sequence
        self.port.write(make_frame(command, sequence, payload))
        self.port.flush()
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"RPC 0x{command:02X} timed out")
            response_command, response_sequence, response = self._read_frame(remaining)
            if response_sequence != sequence or response_command not in expected | {NACK}:
                continue
            if response_command == NACK:
                raise RpcNackError(command, response)
            return response

    def info(self) -> bytes:
        return self.request(0x03, {0x04})

    def click(self, button: int, count: int = 1, delay: float = 0.45) -> None:
        for _ in range(count):
            self.request(0x22, payload=bytes((button,)))
            time.sleep(delay)

    def capture(self, path: Path, scale: int = 4) -> None:
        framebuffer = self.request(0x13, {0x12}, timeout=8)
        path.with_suffix(".fb").write_bytes(framebuffer)
        write_png(path, framebuffer, scale)


def connect(port: str, baud: int, timeout: float = 30) -> M1Hil:
    deadline = time.monotonic() + timeout
    while True:
        client = None
        try:
            client = M1Hil(port, baud)
            return client
        except OSError:
            if client is not None:
                client.close()
            if time.monotonic() >= deadline:
                raise
            time.sleep(1)


def print_info(client: M1Hil) -> None:
    info = client.info()
    ready = info[21] if len(info) > 21 else 0
    version = info[22:54].split(b"\0", 1)[0].decode(errors="replace") if len(info) > 22 else ""
    print(f"ESP32 ready={ready} version={version or 'unknown'}")


def flash_host(client: M1Hil, image_path: Path) -> None:
    image = image_path.read_bytes()
    if not image:
        raise ValueError("firmware image is empty")
    print(f"Flashing {len(image)} bytes from {image_path}")
    client.request(0x42, payload=struct.pack("<II", len(image), 0), timeout=90)
    print("Inactive bank erased")
    started = time.monotonic()
    try:
        for offset in range(0, len(image), 1024):
            chunk = image[offset : offset + 1024]
            client.request(0x43, payload=struct.pack("<I", offset) + chunk, timeout=15)
            if offset % (64 * 1024) == 0 or offset + len(chunk) == len(image):
                percent = (offset + len(chunk)) * 100 // len(image)
                print(f"{percent:3d}% {offset + len(chunk)}/{len(image)}", flush=True)
    except TimeoutError as error:
        raise RuntimeError(
            "flash data timed out; reboot the M1 and rerun flash-host from a clean RPC session"
        ) from error
    client.request(0x44, timeout=90)
    print(f"Flash and embedded CRC verification passed in {time.monotonic() - started:.1f}s")
    client.request(0x45, timeout=15)
    print("Bank swap accepted; M1 is rebooting")


def write_bytes_sync(path: Path, data: bytes) -> None:
    with path.open("wb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def eject_volume(drive: Path) -> None:
    if os.name != "nt":
        raise RuntimeError("volume ejection is unsupported on this platform")
    text = str(drive)
    volume = text[:2] if len(text) >= 2 and text[1] == ":" else text
    script = (
        f"$volume = Get-WmiObject Win32_Volume -Filter \"DriveLetter='{volume}'\"; "
        "if ($null -eq $volume) { exit 2 }; "
        "$result = $volume.Dismount($false, $false); "
        "if ($result.ReturnValue -ne 0) { exit $result.ReturnValue }"
    )
    subprocess.run(
        ["powershell.exe", "-NoProfile", "-Command", script, volume],
        check=True,
        capture_output=True,
        text=True,
        timeout=30,
    )


def stage_esp32(args: argparse.Namespace) -> None:
    image = args.image.resolve()
    source = image.read_bytes()
    if not source:
        raise ValueError("firmware image is empty")
    name = args.name or (f"app_{image.name}" if image.name == "m1-esp32-brain.bin" else image.name)
    if Path(name).name != name or not name.lower().endswith(".bin"):
        raise ValueError("--name must be a plain .bin filename")
    drive = Path(args.drive)
    destination = drive / name
    sidecar = destination.with_suffix(".md5")

    with M1Hil(args.port, args.baud) as client:
        try:
            client.request(0x3B, timeout=15)
        except TimeoutError:
            print("USB disconnected for mass-storage mode (expected)")
        except RpcNackError as error:
            if error.response != b"\x04" or not drive.is_dir():
                raise
            print("SD card is already unmounted; resuming staging")

    write_bytes_sync(destination, source)
    digest = hashlib.md5(source).hexdigest().upper()
    write_bytes_sync(sidecar, digest.encode("ascii"))
    if destination.read_bytes() != source:
        raise RuntimeError("SD image verification failed")
    if sidecar.read_bytes() != digest.encode("ascii") or len(sidecar.read_bytes()) != 32:
        raise RuntimeError("SD MD5 sidecar verification failed")
    eject_volume(drive)
    print(f"Staged {name} ({len(source)} bytes)")
    print(f"MD5 {digest}")

    with connect(args.port, args.baud) as client:
        client.request(0x3C, timeout=15)
    print("SD card remounted for on-device OTA")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=os.environ.get("M1_PORT", "COM3"))
    parser.add_argument("--baud", type=int, default=115200)
    subparsers = parser.add_subparsers(dest="action", required=True)

    subparsers.add_parser("info", help="show ESP32 readiness and firmware version")

    subparsers.add_parser("init-esp32", help="initialize ESP32 through native RPC")

    capture = subparsers.add_parser("capture", help="capture the 128x64 screen to PNG and raw FB")
    capture.add_argument("path", type=Path)
    capture.add_argument("--scale", type=int, default=4, choices=range(1, 9))

    click = subparsers.add_parser("click", help="click device buttons")
    click.add_argument("button", choices=BUTTONS)
    click.add_argument("--count", type=int, default=1)
    click.add_argument("--delay", type=float, default=0.45)

    cli = subparsers.add_parser("cli", help="execute an on-device CLI command")
    cli.add_argument("command", nargs=argparse.REMAINDER)

    subparsers.add_parser("reboot", help="reboot the M1")

    flash = subparsers.add_parser("flash-host", help="flash and swap to a CRC-augmented STM32 image")
    flash.add_argument("image", type=Path)

    stage = subparsers.add_parser("stage-esp32", help="copy and verify an ESP32 image and MD5 over USB MSC")
    stage.add_argument("image", type=Path)
    stage.add_argument("--drive", default="D:/")
    stage.add_argument("--name", help="destination filename; app_ prefix selects native app OTA")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.action == "stage-esp32":
        stage_esp32(args)
        return 0
    if args.action == "cli" and not args.command:
        raise ValueError("cli requires a command")

    with M1Hil(args.port, args.baud) as client:
        if args.action == "info":
            print_info(client)
        elif args.action == "init-esp32":
            response = client.request(0x50, {0x51}, b"\x01", timeout=40)
            print(f"ESP_INFO ready={response[0] if response else 0}")
            print_info(client)
        elif args.action == "capture":
            client.capture(args.path, args.scale)
            print(args.path)
        elif args.action == "click":
            if args.count < 1 or args.delay < 0:
                raise ValueError("count must be positive and delay non-negative")
            client.click(BUTTONS[args.button], args.count, args.delay)
        elif args.action == "cli":
            command = " ".join(args.command).encode()
            response = client.request(0x60, {0x61}, command, timeout=40)
            print(response.decode(errors="replace"), end="")
        elif args.action == "reboot":
            try:
                client.request(0x05, timeout=5)
            except TimeoutError:
                pass
            print("M1 reboot requested")
        elif args.action == "flash-host":
            flash_host(client, args.image)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        print(f"error: {type(error).__name__}", file=sys.stderr)
        sys.exit(1)
