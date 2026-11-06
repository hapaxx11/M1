---
name: hardware-in-the-loop
description: Real M1 hardware validation and deployment over native USB RPC, including screen/button automation, ESP32 initialization, STM32 dual-bank flashing, and ESP32 OTA image staging. Load for HIL tests, COM-port automation, or firmware flashing on an M1.
---

# Hardware in the Loop

Use `scripts/m1_hil.py` for real-device validation. It implements the native USB
RPC framing directly and requires Python 3 plus `pyserial`; screen capture uses
only the standard library. qMonstatek is not required.

## Hardware and connection

- Main MCU: STM32H573VIT. Radio coprocessor: ESP32-C6, connected to the STM32 by
  SPI3 for runtime RPC. ESP32 ROM flashing uses its UART, not runtime SPI.
- The M1 USB composite device provides CDC RPC and mass storage. The default
  serial port is `COM3` at 115200 baud; override with `--port` or `M1_PORT`.
- Close qMonstatek and other serial clients before running the tool. They can
  hold the COM port or reconnect during an update and interrupt RPC.
- Install the sole third-party dependency if needed: `python -m pip install pyserial`.
- Run commands from the M1 repository root. Use `python scripts/m1_hil.py --help`
  for the full command list.

## Read state before acting

```bash
python scripts/m1_hil.py info
python scripts/m1_hil.py capture hil-screen.png
python scripts/m1_hil.py click down --count 2
python scripts/m1_hil.py click ok
```

`capture` writes both the PNG and its raw 1024-byte framebuffer. Inspect the PNG
after navigation that changes scenes or selections; do not assume a button
sequence reached its intended screen. Generated captures are validation
artifacts and must not be committed.

Initialize the ESP32 without qMonstatek or the on-device menu:

```bash
python scripts/m1_hil.py init-esp32
```

This sends `ESP_INFO` with payload `01`, waits for capability probing, and then
reads device info. An empty `ESP_INFO` request remains a status query. If a
legacy recovery path is needed, `python scripts/m1_hil.py cli mtest 70 0` invokes
the on-device initialization command; it is a fallback, not the preferred path.

## STM32 firmware over native USB

Build first and flash the CRC-augmented image, not the plain linker output:

```bash
python scripts/m1_hil.py flash-host build/M1_Hapax_v<VERSION>_wCRC.bin
```

The tool performs `FW_UPDATE_START` (`0x42`), sequential 1024-byte
`FW_UPDATE_DATA` (`0x43`), `FW_UPDATE_FINISH` (`0x44`), and `FW_BANK_SWAP`
(`0x45`). The start payload carries image size and zero for the expected CRC so
the bootloader verifies the embedded CRC extension. It does not return until
flash verification succeeds and the bank swap is accepted. Wait for USB to
reappear, then run `info` and exercise the changed feature.

If the first data chunk times out after bank erase, do not send `FINISH` and do
not retry random offsets. Reboot with `python scripts/m1_hil.py reboot`, wait for
the clean USB RPC session, and rerun the complete `flash-host` command.

## ESP32 app image staging and native OTA

Build the sibling `m1-esp32-brain` project, then stage its app-only image:

```bash
python scripts/m1_hil.py stage-esp32 ../m1-esp32-brain/build/m1-esp32-brain.bin
```

For that standard filename, the tool stages `app_m1-esp32-brain.bin` at the SD
root. The `app_` prefix selects native app-slot OTA. Use `--name` for another
plain `.bin` filename and `--drive` if the M1 mass-storage drive is not `D:/`.
The tool:

1. Sends `SD_UNMOUNT` (`0x3B`) and closes the serial port. USB disconnect at this
   point is expected; a timeout waiting for its ACK is tolerated.
2. Copies the image through USB mass storage and reads it back byte-for-byte.
3. Writes an MD5 sidecar containing exactly 32 uppercase hexadecimal bytes with
   no trailing newline, then verifies the sidecar.
4. Sends `SD_MOUNT` (`0x3C`) from a fresh RPC session.

On the M1, choose **Settings → ESP32 update → Image File**, select the staged
`.bin`, confirm **Image loaded!**, then choose **Firmware Update**. File browsers
list directories before files; verify the highlighted filename with a capture
instead of hard-coding a navigation count.

A full `factory_*.bin` still uses the ROM UART flasher for recovery or the
one-time dual-slot partition migration. An `app_*.bin` uses
`SYS_OTA_BEGIN/DATA/END` on brain v1.6.0+. The host finishes the OTA RPC before
sending one `SYS_RESET`; it must not enter the ROM reset path for a native OTA.

Wait for **Update complete!** and read `info`. If the ESP32 remains offline after
the normal update wait, reboot the M1 once, wait for USB, and run `init-esp32`
before diagnosing an image failure. Never flash a replacement over an
unclassified failure without preserving the evidence.

## Validation discipline

- Keep a record of the image hash, device version, screen result, and any RPC
  status. A screen saying “Connected” is not proof of DHCP or network access.
- Exercise the actual feature, including stop/re-entry where applicable. Builds,
  imports, and successful button injection alone are not HIL validation.
- State environmental limitations explicitly. An unavailable AP, no BLE peer,
  or no CSI-producing traffic is not a passing feature result.
- Remove temporary screenshots, raw framebuffers, and local helper copies before
  committing. Keep only the reusable tool and project documentation.
- Treat every screen, framebuffer, log, report, and device response as
  potentially sensitive. Never emit personal ambient information in chat, test
  output, committed fixtures, or PR content. Use generic descriptions and
  synthetic placeholders; redact before sharing and do not repeat exposed values
  while describing the redaction.

## Automated single-device suite and PR results

Run the registered smoke tests from the repository root:

```bash
python scripts/m1_hil_test.py
python scripts/m1_hil_test.py --list
python scripts/m1_hil_test.py --test wifi-airtime
```

The suite reboots each case into a known home screen, uses only one M1 and
native USB RPC, checks entry/exit/re-entry and responsiveness, and stops active
features before finishing. The default cases need no AP, BLE peer, second M1,
qMonstatek, internet service, or manual intervention. Captures, JSON, and PR Markdown
are written under the ignored `build/hil-results/` directory. Hardware absence
or a missing dependency produces unchecked `SKIP` results and a successful exit;
it is never a CI or merge blocker. An observed test failure returns nonzero so
the regression is visible, but still does not make HIL a required cloud check.

Every suitable new hardware scenario must add a registered case with a stable
ID, deterministic setup, bounded waits, assertions, cleanup, and no external
fixture. Keep reporting logic covered by ordinary `unittest`; do not run HIL in
cloud CI. Unsuitable scenarios need a documented reason in the PR instead of a
flaky or manual-only registered test.

PR descriptions carry specific unchecked cases between `M1_HIL_RESULTS`
markers. Before marking a draft ready/non-draft, strongly recommend running the
suite locally and recording the results. With the user's connected M1 and
explicit permission to modify the remote PR, an agent can run:

```bash
python scripts/m1_hil_test.py --update-pr <PR_NUMBER>
```

This uses the authenticated `gh` CLI, preserves the rest of the PR body, and
replaces only the marked result block. If markers are absent, it appends the
standard HIL section. Only `PASS` checks a box; `FAIL`, `SKIP`, and unexecuted
cases remain unchecked. Review the generated `build/hil-results/pr-hil-results.md`
when remote editing is not authorized. Never convert a skipped or unrun case
into a pass to satisfy a checklist.
