# M1 RPC Test Harness

Hardware-in-the-loop integration tests for the M1 firmware using the
built-in RPC protocol over USB CDC.  This is **"Playwright for embedded"**
— inject button presses, capture the 128×64 framebuffer, and assert on
screen content, all driven from Python.

## Requirements

- A physical M1 device connected via USB
- Python 3.10+
- Dependencies: `pip install -r requirements.txt`

## Quick Start

```bash
# Install dependencies
pip install -r rpc_tests/requirements.txt

# Run tests (Linux — adjust port for your system)
pytest rpc_tests/ -v --port /dev/ttyACM0

# Run tests (Windows)
pytest rpc_tests/ -v --port COM3

# Run tests (using environment variable)
export M1_PORT=/dev/ttyACM0
pytest rpc_tests/ -v
```

## Architecture

```
rpc_tests/
├── __init__.py          # Package marker
├── conftest.py          # pytest fixtures (--port, m1 client, screenshot_dir)
├── m1_client.py         # M1Client class — full RPC protocol implementation
├── test_basic.py        # Basic integration tests (ping, buttons, screen)
├── requirements.txt     # Python dependencies
└── README.md            # This file
```

### M1Client API

```python
from rpc_tests.m1_client import M1Client, Button

m1 = M1Client("/dev/ttyACM0")
m1.connect()

# Connectivity
m1.ping()
info = m1.device_info()
print(info.fw_version)           # "1.2.3"
print(info.battery_level)        # 85
print(info.hapax_revision)       # 14

# Button injection
m1.click(Button.OK)
m1.click(Button.DOWN)
m1.click(Button.BACK)

# Screen capture
img = m1.screenshot()            # PIL.Image (128×64, mode "1")
img.save("screen.png")

# File operations
files = m1.file_list("/SUBGHZ")
data = m1.file_read("/SUBGHZ/test.sub")
m1.file_delete("/SUBGHZ/old.sub")
m1.file_mkdir("/SUBGHZ/backups")

# Screen streaming
m1.screen_start(fps=10)
# ... receive SCREEN_FRAME packets ...
m1.screen_stop()

# CLI
output = m1.cli_exec("help")

m1.disconnect()
```

### Writing Tests

Tests use standard pytest with the `m1` fixture (auto-connected M1Client):

```python
from rpc_tests.m1_client import Button

def test_subghz_menu(m1):
    """Navigate to Sub-GHz and verify screen changes."""
    m1.click(Button.OK)          # Enter main menu
    m1.click(Button.DOWN)        # Navigate down
    m1.click(Button.OK)          # Enter module

    img = m1.screenshot()
    assert img.size == (128, 64)
    # Check that screen is not blank
    assert sum(1 for p in img.getdata() if p) > 100

    m1.click(Button.BACK)        # Return
    m1.click(Button.BACK)        # Return to home
```

## RPC Protocol

The client implements the full M1 RPC binary protocol:

```
Frame: [0xAA] [CMD:1] [SEQ:1] [LEN:2 LE] [PAYLOAD:0-8192] [CRC16:2]
```

- **SYNC** (0xAA): Frame start marker
- **CMD**: Command ID (see `Cmd` enum in `m1_client.py`)
- **SEQ**: Sequence number (0-255, wraps)
- **LEN**: Payload length (little-endian uint16)
- **PAYLOAD**: Command-specific data
- **CRC16**: CRC-16 over CMD+SEQ+LEN+PAYLOAD (same table as `m1_rpc.c`)

## CI Integration

These tests require physical hardware and **cannot run on GitHub Actions
cloud runners**.  Options for CI:

1. **Self-hosted runner** with M1 connected via USB
2. **Manual test gate** — run before releases
3. **Dedicated test bench** — Raspberry Pi + M1 with auto-trigger

The host-side unit tests (in `tests/`) run in CI without hardware.
