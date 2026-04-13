# m1-hil-tests — Bootstrap Instructions

Agent instructions for initializing the `hapaxx11/m1-hil-tests` repository.

---

## What This Repo Is

Hardware-in-the-loop (HIL) integration tests for the **Monstatek M1 firmware
(Hapax fork)**.  A Python test harness connects to a real M1 device over USB
CDC and exercises the full firmware through its binary RPC protocol — button
injection, framebuffer capture, file operations, and CLI commands.

Think of it as **"Playwright for embedded firmware."**

---

## Step 1 — ✅ Done: Files Copied from M1 Repo

The HIL test files originally lived in `rpc_tests/` in `hapaxx11/M1`.
They have been copied into `hapaxx11/m1-hil-tests` and the `rpc_tests/`
directory has been removed from the firmware repo.

Current repo structure:

```
m1-hil-tests/
├── m1_client.py         # M1Client class — full RPC protocol implementation
├── conftest.py          # pytest fixtures (--port, m1 client, screenshot_dir)
├── test_basic.py        # Basic integration tests (ping, buttons, screen)
├── requirements.txt     # pyserial, pytest, Pillow
├── README.md            # Setup and API reference
├── __init__.py          # Package marker
├── CLAUDE.md            # Agent instructions
└── .github/
    └── workflows/       # (future) CI workflow for self-hosted runner
```

Imports use absolute form (`from m1_client import ...`) since the package
is at the repo root.

---

## Step 2 — ✅ Done: CLAUDE.md Created

The following patterns and rules were copied from `hapaxx11/M1`'s `CLAUDE.md`:

### Rules to copy verbatim

- **ABSOLUTE RULES** section (no co-author attribution, no unauthorized
  remote operations, no public exposure) — adapt the repo name from M1 to
  m1-hil-tests
- **Git Commit Rules** section (concise messages, no AI attribution, stage
  specific files)

### Rules to adapt

- **Workflow Rules** — replace firmware build rules with Python-specific ones:
  - Always run `pytest` after code changes
  - Always validate with `ruff check` (or `flake8`) before committing
  - Tests require a physical M1 device — skip gracefully when no device
    is available (`--port` option or `M1_PORT` env var)

### New rules to add

- **Test Design Principles**:
  - Each test must leave the device in a known state (return to home screen)
  - Use the `m1` session-scoped fixture for device connection
  - Group tests by functionality using pytest classes
  - Screenshots saved during tests go to temporary directories (use
    `screenshot_dir` fixture)
  - Navigation tests must be idempotent — undo any menu traversal with
    matching BACK presses

- **RPC Protocol Reference**:
  - Frame format: `[0xAA] [CMD:1] [SEQ:1] [LEN:2 LE] [PAYLOAD:0-8192] [CRC16:2]`
  - CRC-16: poly=0x1021, init=0xFFFF, table-driven, no reflection
  - Same CRC table as `m1_rpc.c` in the M1 firmware repo
  - Command IDs must stay in sync with `m1_rpc.h` in `hapaxx11/M1`

---

## Step 3 — Repo Configuration

### Python setup

```bash
# Recommended: use a virtual environment
python -m venv .venv
source .venv/bin/activate  # or .venv\Scripts\activate.bat on Windows

pip install -r requirements.txt
```

### Running tests

```bash
# Linux
pytest -v --port /dev/ttyACM0

# Windows
pytest -v --port COM3

# Using environment variable
export M1_PORT=/dev/ttyACM0
pytest -v
```

### Optional: add development dependencies

```
# dev-requirements.txt
ruff>=0.4.0
pytest-timeout>=2.0
```

---

## Step 4 — Planned Work

### Immediate priorities

1. **Module navigation tests** — navigate to each top-level module
   (Sub-GHz, NFC, RFID, IR, WiFi, BT, Settings) and verify the screen
   shows expected content
2. **File operation tests** — write/read/delete round-trip via RPC
3. **Visual regression framework** — golden screenshot comparison with
   configurable tolerance

### Future capabilities

- **Screen text recognition** — OCR or pixel-pattern matching for the
  ST7567 128×64 monochrome display font
- **CI with self-hosted runner** — Raspberry Pi + M1 device connected
  via USB, triggered on push to `hapaxx11/M1`
- **Firmware version gating** — skip tests that require features not
  present in the connected firmware version

---

## RPC Command Reference

These command IDs come from `m1_rpc.h` in `hapaxx11/M1` and must stay
in sync:

| Range | Category | Commands |
|-------|----------|----------|
| 0x01–0x08 | System | PING, PONG, GET_DEVICE_INFO, DEVICE_INFO_RESP, REBOOT, ACK, NACK, POWER_OFF |
| 0x10–0x13 | Screen | SCREEN_START, SCREEN_STOP, SCREEN_FRAME, SCREEN_CAPTURE |
| 0x20–0x22 | Input | BUTTON_PRESS, BUTTON_RELEASE, BUTTON_CLICK |
| 0x30–0x3C | File | FILE_LIST, FILE_LIST_RESP, FILE_READ, FILE_READ_DATA, FILE_WRITE_*, FILE_DELETE, FILE_MKDIR, SD_UNMOUNT, SD_MOUNT |
| 0x40–0x47 | Firmware | FW_INFO, FW_UPDATE_*, FW_BANK_SWAP, FW_DFU_ENTER, FW_BANK_ERASE |
| 0x50–0x54 | ESP32 | ESP_INFO, ESP_UPDATE_* |
| 0x60–0x63 | Debug/CLI | CLI_EXEC, CLI_RESP, ESP_UART_SNOOP, ESP_UART_SNOOP_RESP |

### Button IDs

| ID | Button |
|----|--------|
| 0 | OK |
| 1 | UP |
| 2 | LEFT |
| 3 | RIGHT |
| 4 | DOWN |
| 5 | BACK |

---

## Relationship to hapaxx11/M1

This repo is a **companion** to the main firmware repo.  It does not contain
firmware source code.  The RPC protocol is defined in `hapaxx11/M1` at
`m1_csrc/m1_rpc.h` and `m1_csrc/m1_rpc.c`.

When the firmware adds new RPC commands, the `Cmd` enum in `m1_client.py`
must be updated to match.
