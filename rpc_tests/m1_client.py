"""
m1_client.py — M1 RPC Client for hardware-in-the-loop testing.

Communicates with a real M1 device over USB CDC using the binary RPC
protocol defined in m1_rpc.h.  Provides a high-level API for:

  - Injecting button events (click, press, release)
  - Capturing the 128×64 framebuffer as a PIL Image
  - Reading device info (firmware version, battery, SD card)
  - File operations (list, read, write, delete, mkdir)
  - Screen streaming start/stop

Frame format:
  [0xAA] [CMD:1] [SEQ:1] [LEN:2 LE] [PAYLOAD:0-8192] [CRC16:2]

CRC-16 is computed over CMD+SEQ+LEN+PAYLOAD (everything except SYNC
and the CRC itself).  Uses the same table-driven CRC-16 as m1_rpc.c.

Usage:
    from m1_client import M1Client, Button

    m1 = M1Client("/dev/ttyACM0")
    m1.connect()
    m1.ping()
    m1.click(Button.OK)
    img = m1.screenshot()
    img.save("screen.png")
    info = m1.device_info()
    m1.disconnect()
"""

import struct
import time
from enum import IntEnum
from typing import Optional

import serial
from PIL import Image


# ─── RPC Protocol Constants ─────────────────────────────────────────

SYNC_BYTE = 0xAA
HEADER_SIZE = 5       # SYNC + CMD + SEQ + LEN(2)
CRC_SIZE = 2
SCREEN_FB_SIZE = 1024  # 128×64 / 8
CRC16_INIT = 0xFFFF

# ─── Command IDs (from m1_rpc.h) ────────────────────────────────────

class Cmd(IntEnum):
    """RPC command identifiers."""
    PING            = 0x01
    PONG            = 0x02
    GET_DEVICE_INFO = 0x03
    DEVICE_INFO_RESP = 0x04
    REBOOT          = 0x05
    ACK             = 0x06
    NACK            = 0x07
    POWER_OFF       = 0x08
    SCREEN_START    = 0x10
    SCREEN_STOP     = 0x11
    SCREEN_FRAME    = 0x12
    SCREEN_CAPTURE  = 0x13
    BUTTON_PRESS    = 0x20
    BUTTON_RELEASE  = 0x21
    BUTTON_CLICK    = 0x22
    FILE_LIST       = 0x30
    FILE_LIST_RESP  = 0x31
    FILE_READ       = 0x32
    FILE_READ_DATA  = 0x33
    FILE_DELETE     = 0x37
    FILE_MKDIR      = 0x38
    CLI_EXEC        = 0x60
    CLI_RESP        = 0x61


class Button(IntEnum):
    """Button identifiers matching m1_system.h."""
    OK    = 0
    UP    = 1
    LEFT  = 2
    RIGHT = 3
    DOWN  = 4
    BACK  = 5


class NackError(IntEnum):
    """NACK error codes from m1_rpc.h."""
    UNKNOWN_CMD     = 0x01
    INVALID_PAYLOAD = 0x02
    BUSY            = 0x03
    SD_NOT_READY    = 0x04
    FILE_NOT_FOUND  = 0x05
    FLASH_ERROR     = 0x07
    CRC_MISMATCH    = 0x08
    SIZE_TOO_LARGE  = 0x09
    BANK_EMPTY      = 0x0A
    ESP_FLASH       = 0x0B


# ─── CRC-16 Table (identical to m1_rpc.c) ───────────────────────────

_CRC16_TABLE = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x4864, 0x5845, 0x6826, 0x7807, 0x08E0, 0x18C1, 0x28A2, 0x38C3,
    0xC92C, 0xD90D, 0xE96E, 0xF94F, 0x89A8, 0x9989, 0xA9EA, 0xB9CB,
    0x5A15, 0x4A34, 0x7A57, 0x6A76, 0x1A91, 0x0AB0, 0x3AD3, 0x2AF2,
    0xDB1D, 0xCB3C, 0xFB5F, 0xEB7E, 0x9B99, 0x8BB8, 0xBBDB, 0xABFA,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBBBA,
    0x4A55, 0x5A74, 0x6A17, 0x7A36, 0x0AD1, 0x1AF0, 0x2A93, 0x3AB2,
    0xFD0E, 0xED2F, 0xDD4C, 0xCD6D, 0xBD8A, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
]


def crc16(data: bytes) -> int:
    """Compute CRC-16 over a byte buffer (init=0xFFFF)."""
    crc = CRC16_INIT
    for b in data:
        idx = ((crc >> 8) ^ b) & 0xFF
        crc = ((crc << 8) ^ _CRC16_TABLE[idx]) & 0xFFFF
    return crc


# ─── Exceptions ─────────────────────────────────────────────────────

class M1Error(Exception):
    """Base exception for M1 client errors."""


class M1TimeoutError(M1Error):
    """Raised when a response is not received in time."""


class M1NackError(M1Error):
    """Raised when the device responds with NACK."""
    def __init__(self, error_code: int, sub_error: int = 0):
        self.error_code = error_code
        self.sub_error = sub_error
        try:
            name = NackError(error_code).name
        except ValueError:
            name = f"0x{error_code:02X}"
        super().__init__(f"NACK: {name} (sub=0x{sub_error:02X})")


class M1CrcError(M1Error):
    """Raised when a received frame has a CRC mismatch."""


# ─── Device Info Struct ──────────────────────────────────────────────

class DeviceInfo:
    """Parsed S_RPC_DeviceInfo response."""
    def __init__(self, payload: bytes):
        if len(payload) < 49:
            raise M1Error(f"DeviceInfo payload too short: {len(payload)} bytes")

        # Unpack fixed fields matching S_RPC_DeviceInfo (packed struct)
        (
            self.magic,
            self.fw_major, self.fw_minor, self.fw_build, self.fw_rc,
            self.active_bank,
            self.battery_level, self.battery_charging,
            self.sd_card_present,
            self.sd_total_kb, self.sd_free_kb,
            self.esp32_ready,
        ) = struct.unpack_from("<IBBBBHBBBIIBB", payload, 0)
        # Note: extra byte from esp32_ready position shift

        # Reparse with correct offsets based on packed struct
        offset = 0
        self.magic = struct.unpack_from("<I", payload, offset)[0]; offset += 4
        self.fw_major = payload[offset]; offset += 1
        self.fw_minor = payload[offset]; offset += 1
        self.fw_build = payload[offset]; offset += 1
        self.fw_rc = payload[offset]; offset += 1
        self.active_bank = struct.unpack_from("<H", payload, offset)[0]; offset += 2
        self.battery_level = payload[offset]; offset += 1
        self.battery_charging = payload[offset]; offset += 1
        self.sd_card_present = payload[offset]; offset += 1
        self.sd_total_kb = struct.unpack_from("<I", payload, offset)[0]; offset += 4
        self.sd_free_kb = struct.unpack_from("<I", payload, offset)[0]; offset += 4
        self.esp32_ready = payload[offset]; offset += 1

        # ESP32 version string (32 bytes, null-terminated)
        esp32_raw = payload[offset:offset + 32]
        self.esp32_version = esp32_raw.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        offset += 32

        self.ism_band_region = payload[offset] if offset < len(payload) else 0; offset += 1
        self.op_mode = payload[offset] if offset < len(payload) else 0; offset += 1
        self.southpaw_mode = payload[offset] if offset < len(payload) else 0; offset += 1
        self.hapax_revision = payload[offset] if offset < len(payload) else 0; offset += 1

    @property
    def fw_version(self) -> str:
        rc = f"-rc{self.fw_rc}" if self.fw_rc else ""
        return f"{self.fw_major}.{self.fw_minor}.{self.fw_build}{rc}"

    def __repr__(self) -> str:
        return (
            f"DeviceInfo(fw={self.fw_version}, bank={self.active_bank}, "
            f"batt={self.battery_level}%, sd={'OK' if self.sd_card_present else 'NONE'}, "
            f"esp32={'ready' if self.esp32_ready else 'off'}, "
            f"hapax_rev={self.hapax_revision})"
        )


# ─── M1Client ───────────────────────────────────────────────────────

class M1Client:
    """
    High-level client for M1 RPC communication over USB CDC.

    Example:
        m1 = M1Client("/dev/ttyACM0")
        m1.connect()
        m1.ping()
        m1.click(Button.OK)
        img = m1.screenshot()
        info = m1.device_info()
        m1.disconnect()
    """

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 2.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self._ser: Optional[serial.Serial] = None
        self._seq: int = 0

    # ── Connection ──────────────────────────────────────────────────

    def connect(self) -> None:
        """Open the serial port and verify RPC connectivity with a PING."""
        self._ser = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            timeout=self.timeout,
            write_timeout=self.timeout,
        )
        # Flush any stale data
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        # Verify connectivity
        self.ping()

    def disconnect(self) -> None:
        """Close the serial port."""
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    @property
    def connected(self) -> bool:
        return self._ser is not None and self._ser.is_open

    # ── Low-Level Frame I/O ─────────────────────────────────────────

    def _next_seq(self) -> int:
        seq = self._seq
        self._seq = (self._seq + 1) & 0xFF
        return seq

    def send_frame(self, cmd: int, payload: bytes = b"") -> int:
        """Build and send an RPC frame.  Returns the sequence number used."""
        assert self._ser is not None, "Not connected"

        seq = self._next_seq()
        plen = len(payload)

        # Build header: SYNC + CMD + SEQ + LEN_LO + LEN_HI
        header = bytes([SYNC_BYTE, cmd, seq, plen & 0xFF, (plen >> 8) & 0xFF])

        # CRC over CMD+SEQ+LEN+PAYLOAD (bytes after SYNC)
        crc_data = header[1:] + payload
        crc_val = crc16(crc_data)
        crc_bytes = struct.pack("<H", crc_val)

        frame = header + payload + crc_bytes
        self._ser.write(frame)
        self._ser.flush()
        return seq

    def recv_frame(self, timeout: Optional[float] = None) -> tuple[int, int, bytes]:
        """
        Receive and validate a single RPC frame.

        Returns:
            (cmd, seq, payload) tuple.

        Raises:
            M1TimeoutError: No sync byte received within timeout.
            M1CrcError: CRC validation failed.
        """
        assert self._ser is not None, "Not connected"

        old_timeout = self._ser.timeout
        if timeout is not None:
            self._ser.timeout = timeout

        try:
            # Wait for sync byte (skip any non-sync data)
            while True:
                b = self._ser.read(1)
                if len(b) == 0:
                    raise M1TimeoutError("Timeout waiting for sync byte")
                if b[0] == SYNC_BYTE:
                    break

            # Read header: CMD + SEQ + LEN(2) = 4 bytes
            hdr = self._ser.read(4)
            if len(hdr) < 4:
                raise M1TimeoutError("Timeout reading frame header")

            cmd = hdr[0]
            seq = hdr[1]
            plen = hdr[2] | (hdr[3] << 8)

            # Read payload
            payload = b""
            if plen > 0:
                payload = self._ser.read(plen)
                if len(payload) < plen:
                    raise M1TimeoutError(
                        f"Timeout reading payload: got {len(payload)}/{plen}"
                    )

            # Read CRC
            crc_raw = self._ser.read(2)
            if len(crc_raw) < 2:
                raise M1TimeoutError("Timeout reading CRC")

            rx_crc = crc_raw[0] | (crc_raw[1] << 8)

            # Validate CRC
            computed = crc16(hdr + payload)
            if rx_crc != computed:
                raise M1CrcError(
                    f"CRC mismatch: rx=0x{rx_crc:04X} computed=0x{computed:04X}"
                )

            return (cmd, seq, payload)

        finally:
            self._ser.timeout = old_timeout

    def send_and_expect(
        self,
        cmd: int,
        payload: bytes = b"",
        expect_cmd: Optional[int] = None,
        timeout: Optional[float] = None,
    ) -> bytes:
        """
        Send a command and wait for the expected response.

        If expect_cmd is None, expects ACK.  Raises M1NackError on NACK.
        Returns the response payload.
        """
        seq = self.send_frame(cmd, payload)
        if expect_cmd is None:
            expect_cmd = Cmd.ACK

        # Read frames until we get the expected response or NACK
        deadline = time.monotonic() + (timeout or self.timeout)
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise M1TimeoutError(
                    f"Timeout waiting for response cmd=0x{expect_cmd:02X}"
                )

            rx_cmd, rx_seq, rx_payload = self.recv_frame(timeout=remaining)

            if rx_cmd == Cmd.NACK:
                err = rx_payload[0] if len(rx_payload) > 0 else 0
                sub = rx_payload[1] if len(rx_payload) > 1 else 0
                raise M1NackError(err, sub)

            if rx_cmd == expect_cmd:
                return rx_payload

            # Ignore unexpected frames (e.g., screen stream frames)

    # ── High-Level API ──────────────────────────────────────────────

    def ping(self) -> None:
        """Send PING and verify PONG response."""
        self.send_and_expect(Cmd.PING, expect_cmd=Cmd.PONG)

    def device_info(self) -> DeviceInfo:
        """Query device info (firmware version, battery, SD card, etc.)."""
        payload = self.send_and_expect(
            Cmd.GET_DEVICE_INFO, expect_cmd=Cmd.DEVICE_INFO_RESP
        )
        return DeviceInfo(payload)

    def click(self, button: Button, delay: float = 0.15) -> None:
        """Inject a button click event and wait for UI to update."""
        self.send_and_expect(Cmd.BUTTON_CLICK, bytes([int(button)]))
        time.sleep(delay)

    def press(self, button: Button) -> None:
        """Inject a button press (without release)."""
        self.send_and_expect(Cmd.BUTTON_PRESS, bytes([int(button)]))

    def release(self, button: Button) -> None:
        """Inject a button release."""
        self.send_and_expect(Cmd.BUTTON_RELEASE, bytes([int(button)]))

    def screenshot(self) -> Image.Image:
        """
        Capture a single screen frame as a 128×64 monochrome PIL Image.

        The framebuffer is in u8g2 tile format: 8 rows of 128 bytes each.
        Each byte represents 8 vertical pixels (LSB = top pixel in tile).
        """
        payload = self.send_and_expect(
            Cmd.SCREEN_CAPTURE, expect_cmd=Cmd.SCREEN_FRAME, timeout=2.0
        )
        if len(payload) < SCREEN_FB_SIZE:
            raise M1Error(
                f"Screen frame too small: {len(payload)} bytes "
                f"(expected {SCREEN_FB_SIZE})"
            )

        return fb_to_image(payload[:SCREEN_FB_SIZE])

    def screen_start(self, fps: int = 10) -> None:
        """Start screen streaming at the given FPS (1-15)."""
        fps = max(1, min(15, fps))
        self.send_and_expect(Cmd.SCREEN_START, bytes([fps]))

    def screen_stop(self) -> None:
        """Stop screen streaming."""
        self.send_and_expect(Cmd.SCREEN_STOP)

    def file_list(self, path: str) -> list[dict]:
        """
        List files in an SD card directory.

        Returns a list of dicts with 'name', 'size', 'is_dir' keys.
        """
        payload = self.send_and_expect(
            Cmd.FILE_LIST,
            path.encode("ascii"),
            expect_cmd=Cmd.FILE_LIST_RESP,
            timeout=5.0,
        )
        return _parse_file_list(payload)

    def file_read(self, path: str) -> bytes:
        """Read a file from the SD card.  Returns file content as bytes."""
        chunks = []
        seq = self.send_frame(Cmd.FILE_READ, path.encode("ascii"))

        deadline = time.monotonic() + 10.0
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise M1TimeoutError("Timeout reading file")

            rx_cmd, rx_seq, rx_payload = self.recv_frame(timeout=remaining)

            if rx_cmd == Cmd.NACK:
                err = rx_payload[0] if len(rx_payload) > 0 else 0
                raise M1NackError(err)

            if rx_cmd == Cmd.FILE_READ_DATA:
                if len(rx_payload) == 0:
                    break  # End of file marker
                chunks.append(rx_payload)
            elif rx_cmd == Cmd.ACK:
                break  # Transfer complete

        return b"".join(chunks)

    def file_delete(self, path: str) -> None:
        """Delete a file on the SD card."""
        self.send_and_expect(Cmd.FILE_DELETE, path.encode("ascii"), timeout=5.0)

    def file_mkdir(self, path: str) -> None:
        """Create a directory on the SD card."""
        self.send_and_expect(Cmd.FILE_MKDIR, path.encode("ascii"), timeout=5.0)

    def cli_exec(self, command: str, timeout: float = 5.0) -> str:
        """Execute a CLI command and return the text response."""
        payload = self.send_and_expect(
            Cmd.CLI_EXEC,
            command.encode("ascii"),
            expect_cmd=Cmd.CLI_RESP,
            timeout=timeout,
        )
        return payload.decode("ascii", errors="replace")

    def reboot(self) -> None:
        """Reboot the device.  Connection will be lost."""
        self.send_frame(Cmd.REBOOT)


# ─── Framebuffer Utilities ──────────────────────────────────────────

def fb_to_image(fb: bytes) -> Image.Image:
    """
    Convert a 1024-byte u8g2 framebuffer to a 128×64 PIL Image.

    u8g2 tile format: 8 tile rows × 128 columns.
    Each byte = 8 vertical pixels (bit 0 = top of tile, bit 7 = bottom).
    """
    img = Image.new("1", (128, 64), 0)
    pixels = img.load()
    assert pixels is not None

    for tile_row in range(8):
        for col in range(128):
            byte = fb[tile_row * 128 + col]
            for bit in range(8):
                y = tile_row * 8 + bit
                x = col
                if byte & (1 << bit):
                    pixels[x, y] = 1

    return img


def image_to_fb(img: Image.Image) -> bytes:
    """Convert a 128×64 PIL Image to u8g2 framebuffer bytes."""
    assert img.size == (128, 64), f"Expected 128×64, got {img.size}"
    mono = img.convert("1")
    pixels = mono.load()
    assert pixels is not None

    fb = bytearray(SCREEN_FB_SIZE)
    for tile_row in range(8):
        for col in range(128):
            byte_val = 0
            for bit in range(8):
                y = tile_row * 8 + bit
                if pixels[col, y]:
                    byte_val |= (1 << bit)
            fb[tile_row * 128 + col] = byte_val

    return bytes(fb)


def _parse_file_list(payload: bytes) -> list[dict]:
    """Parse a FILE_LIST_RESP payload into a list of file entries."""
    entries = []
    if not payload:
        return entries

    # Format: each entry is [type:1][size:4 LE][name:null-terminated]
    offset = 0
    while offset < len(payload):
        if offset + 5 > len(payload):
            break

        entry_type = payload[offset]; offset += 1
        size = struct.unpack_from("<I", payload, offset)[0]; offset += 4

        # Find null terminator
        name_end = payload.index(0, offset) if 0 in payload[offset:] else len(payload)
        name = payload[offset:name_end].decode("ascii", errors="replace")
        offset = name_end + 1

        entries.append({
            "name": name,
            "size": size,
            "is_dir": bool(entry_type & 0x10),
        })

    return entries
