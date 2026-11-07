#!/usr/bin/env python3
"""Run self-contained single-M1 HIL tests and optionally update a PR checklist."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from typing import Callable
import zlib

from m1_hil import BUTTONS, M1Hil, connect, write_png
from m1_bt_menu_audit import run as run_bt_menu_tree
from m1_misc_menu_audit import run as run_misc_menu_tree
from m1_nfc_menu_audit import run as run_nfc_menu_tree
from m1_rfid_menu_audit import run as run_rfid_menu_tree
from m1_settings_menu_audit import run as run_settings_menu_tree
from m1_subghz_menu_audit import run as run_subghz_menu_tree
from m1_wifi_menu_audit import run as run_wifi_menu_tree

RESULT_START = "<!-- M1_HIL_RESULTS:START -->"
RESULT_END = "<!-- M1_HIL_RESULTS:END -->"
BOOT_WAIT_SECONDS = 15.0


@dataclass(frozen=True)
class HilCase:
    test_id: str
    title: str
    run: Callable[[M1Hil, Path], str]


def click(client: M1Hil, name: str, count: int = 1) -> None:
    client.click(BUTTONS[name], count=count)


def write_redacted_capture(path: Path, framebuffer: bytes) -> None:
    redacted = bytearray(framebuffer)
    for x in range(128):
        for y in range(10, 64):
            redacted[x + (y // 8) * 128] &= ~(1 << (y % 8)) & 0xFF
    path.parent.mkdir(parents=True, exist_ok=True)
    path.with_suffix(".fb").write_bytes(redacted)
    write_png(path, bytes(redacted), scale=4)


def capture(client: M1Hil, directory: Path, name: str) -> bytes:
    framebuffer = client.request(0x13, {0x12}, timeout=8)
    if len(framebuffer) != 1024 or len(set(framebuffer)) < 2:
        raise AssertionError(f"{name}: framebuffer is missing or blank")
    write_redacted_capture(directory / f"{name}.png", framebuffer)
    return framebuffer


def title_crc(framebuffer: bytes) -> int:
    if len(framebuffer) != 1024:
        raise ValueError("expected a 1024-byte framebuffer")
    region = bytes(
        (framebuffer[x + (y // 8) * 128] >> (y % 8)) & 1
        for y in range(10)
        for x in range(96)
    )
    return zlib.crc32(region)


def assert_title(framebuffer: bytes, expected: int | tuple[int, ...], label: str) -> None:
    accepted = (expected,) if isinstance(expected, int) else expected
    if title_crc(framebuffer) not in accepted:
        raise AssertionError(f"{label}: unexpected screen identity")


def capture_until_change(
    client: M1Hil,
    directory: Path,
    name: str,
    previous: bytes,
    timeout_s: float,
    started: float | None = None,
) -> bytes:
    deadline = time.monotonic() + timeout_s if started is None else started + timeout_s
    while True:
        if time.monotonic() >= deadline:
            raise AssertionError(f"{name}: screen did not change within {timeout_s:.1f} s")
        current = capture(client, directory, name)
        if current != previous:
            if time.monotonic() > deadline:
                raise AssertionError(f"{name}: screen changed after {timeout_s:.1f} s")
            return current
        time.sleep(0.1)


def open_wifi(client: M1Hil) -> None:
    click(client, "ok")
    click(client, "down", 5)
    click(client, "ok")


def open_bluetooth(client: M1Hil) -> None:
    click(client, "ok")
    click(client, "down", 6)
    click(client, "ok")


def exercise_scene(
    client: M1Hil,
    artifacts: Path,
    prefix: str,
    active_title: int,
    menu_title: int,
    enter: Callable[[], None],
    entry_timeout_s: float,
    exit_timeout_s: float = 2.0,
    active_dwell_s: float = 0.0,
) -> str:
    before = capture(client, artifacts, f"{prefix}-before")
    started = time.monotonic()
    enter()
    active = capture_until_change(
        client, artifacts, f"{prefix}-active", before, entry_timeout_s, started
    )
    entry_ms = (time.monotonic() - started) * 1000
    assert_title(active, active_title, f"{prefix} entry")
    if active_dwell_s > 0.0:
        time.sleep(active_dwell_s)
        active = capture(client, artifacts, f"{prefix}-dwell")
        assert_title(active, active_title, f"{prefix} watchdog dwell")

    started = time.monotonic()
    click(client, "back")
    menu = capture_until_change(
        client, artifacts, f"{prefix}-menu", active, exit_timeout_s, started
    )
    exit_ms = (time.monotonic() - started) * 1000
    assert_title(menu, menu_title, f"{prefix} exit")

    started = time.monotonic()
    click(client, "ok")
    reentry = capture_until_change(
        client, artifacts, f"{prefix}-reentry", menu, entry_timeout_s, started
    )
    assert_title(reentry, active_title, f"{prefix} re-entry")
    started = time.monotonic()
    click(client, "back")
    cleanup = capture_until_change(
        client, artifacts, f"{prefix}-cleanup", reentry, exit_timeout_s, started
    )
    assert_title(cleanup, menu_title, f"{prefix} cleanup")
    client.info()
    return f"entry {entry_ms:.0f} ms, exit {exit_ms:.0f} ms, re-entry passed"


def run_wifi_airtime(client: M1Hil, artifacts: Path) -> str:
    open_wifi(client)
    click(client, "down")
    click(client, "ok")
    click(client, "down", 2)
    return exercise_scene(
        client, artifacts, "airtime", 0x7AA840B8, 0x04E3CD9B,
        lambda: click(client, "ok"), 5.0
    )


def run_wifi_peer_link(client: M1Hil, artifacts: Path) -> str:
    open_wifi(client)
    click(client, "down", 5)
    return exercise_scene(
        client, artifacts, "peer-link", 0x711B2521, 0x95F7D22C,
        lambda: click(client, "ok"), 3.0
    )


def run_ble_signal_finder(client: M1Hil, artifacts: Path) -> str:
    open_bluetooth(client)
    click(client, "down")
    return exercise_scene(
        client, artifacts, "signal-finder", 0x61785B04, 0xCC15BF52,
        lambda: click(client, "ok"), 5.0
    )


def run_ble_gatt_discovery(client: M1Hil, artifacts: Path) -> str:
    open_bluetooth(client)
    click(client, "down", 10)
    return exercise_scene(
        client, artifacts, "gatt-discovery", (0x48976E86, 0xEECE126F), 0xCC15BF52,
        lambda: click(client, "ok"), 8.0
    )


def run_ble_airtag_monitor(client: M1Hil, artifacts: Path) -> str:
    open_bluetooth(client)
    click(client, "down", 4)
    click(client, "ok")
    click(client, "down", 4)
    return exercise_scene(
        client, artifacts, "airtag-monitor", 0xD5605F26, 0x2F13F597,
        lambda: click(client, "ok"), 10.0, 3.0
    )


def run_ble_spam(client: M1Hil, artifacts: Path) -> str:
    open_bluetooth(client)
    click(client, "down", 5)
    click(client, "ok")
    return exercise_scene(
        client, artifacts, "ble-spam", 0x58EA5A94, 0x62F2B3DB,
        lambda: click(client, "ok"), 10.0, 3.0, active_dwell_s=17.0
    )


def run_wifi_menu_audit(client: M1Hil, artifacts: Path) -> str:
    return run_wifi_menu_tree(client)


def run_bt_menu_audit(client: M1Hil, artifacts: Path) -> str:
    return run_bt_menu_tree(client)


def run_rfid_menu_audit(client: M1Hil, artifacts: Path) -> str:
    return run_rfid_menu_tree(client)


def run_nfc_menu_audit(client: M1Hil, artifacts: Path) -> str:
    return run_nfc_menu_tree(client)


def run_settings_menu_audit(client: M1Hil, artifacts: Path) -> str:
    return run_settings_menu_tree(client)


def run_misc_menu_audit(client: M1Hil, artifacts: Path) -> str:
    return run_misc_menu_tree(client)


def run_subghz_menu_audit(client: M1Hil, artifacts: Path) -> str:
    return run_subghz_menu_tree(client)


CASES = (
    HilCase("wifi-airtime", "WiFi Channel Airtime lifecycle", run_wifi_airtime),
    HilCase("wifi-peer-link", "WiFi Peer Link lifecycle", run_wifi_peer_link),
    HilCase("wifi-menu-audit", "Recursive WiFi menu responsiveness", run_wifi_menu_audit),
    HilCase("bluetooth-menu-audit", "Recursive Bluetooth menu responsiveness", run_bt_menu_audit),
    HilCase("rfid-menu-audit", "Recursive RFID menu responsiveness", run_rfid_menu_audit),
    HilCase("nfc-menu-audit", "Recursive NFC menu responsiveness", run_nfc_menu_audit),
    HilCase("settings-menu-audit", "Recursive Settings menu responsiveness", run_settings_menu_audit),
    HilCase("misc-menu-audit", "Remaining non-SubGhz menu responsiveness", run_misc_menu_audit),
    HilCase("subghz-menu-audit", "Recursive SubGhz menu responsiveness", run_subghz_menu_audit),
    HilCase("ble-signal-finder", "BLE Signal Finder lifecycle", run_ble_signal_finder),
    HilCase("ble-gatt-discovery", "BLE GATT Discovery lifecycle", run_ble_gatt_discovery),
    HilCase("ble-airtag-monitor", "BLE AirTag Monitor lifecycle", run_ble_airtag_monitor),
    HilCase("ble-spam", "BLE Spam watchdog and lifecycle", run_ble_spam),
)


def render_pr_results(results: list[dict]) -> str:
    lines = []
    for result in results:
        status = result["status"]
        checked = "x" if status == "PASS" else " "
        line = f"- [{checked}] `{result['id']}` — {status}"
        if result.get("detail"):
            line += f" — {result['detail']}"
        lines.append(line)
    return "\n".join(lines)


def merge_pr_results(block: str, results: list[dict]) -> str:
    updates = {result["id"]: result for result in results}
    lines = []
    seen = set()
    for line in block.splitlines():
        match = re.match(r"^- \[[ x]\] `([^`]+)`", line)
        if not match:
            continue
        test_id = match.group(1)
        seen.add(test_id)
        lines.append(render_pr_results([updates[test_id]]) if test_id in updates else line)
    for result in results:
        if result["id"] not in seen:
            lines.append(render_pr_results([result]))
    return "\n".join(lines)


def update_pr_body(body: str, results: list[dict]) -> str:
    if RESULT_START in body and RESULT_END in body:
        start = body.index(RESULT_START)
        end = body.index(RESULT_END, start) + len(RESULT_END)
        merged = merge_pr_results(body[start + len(RESULT_START):end - len(RESULT_END)], results)
        block = f"{RESULT_START}\n{merged}\n{RESULT_END}"
        return body[:start] + block + body[end:]
    block = f"{RESULT_START}\n{render_pr_results(results)}\n{RESULT_END}"
    section = (
        "## Single-device HIL\n"
        "Run `python scripts/m1_hil_test.py --update-pr <PR>` locally. "
        "HIL is recommended, never a merge blocker.\n"
        f"{block}"
    )
    return body.rstrip() + "\n\n" + section + "\n"


def write_reports(output_dir: Path, results: list[dict]) -> tuple[Path, Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    markdown = output_dir / "pr-hil-results.md"
    report = output_dir / "hil-results.json"
    markdown.write_text(
        "<!-- Paste between the M1_HIL_RESULTS markers in the PR description. -->\n"
        + render_pr_results(results)
        + "\n",
        encoding="utf-8",
    )
    report.write_text(
        json.dumps(
            {
                "generated_at": datetime.now(timezone.utc).isoformat(),
                "results": results,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return markdown, report


def publish_results(pr_number: int, results: list[dict]) -> None:
    endpoint = f"repos/{{owner}}/{{repo}}/pulls/{pr_number}"
    viewed = subprocess.run(
        ["gh", "api", endpoint, "--jq", ".body"],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    body = update_pr_body(viewed.stdout.rstrip("\r\n"), results)
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", suffix=".md", delete=False
    ) as stream:
        stream.write(body)
        body_path = Path(stream.name)
    try:
        subprocess.run(
            ["gh", "api", "--method", "PATCH", endpoint, "--field", f"body=@{body_path}"],
            check=True,
        )
    finally:
        body_path.unlink(missing_ok=True)


def recover_device(port: str, baud: int) -> None:
    with connect(port, baud, timeout=5) as client:
        try:
            client.request(0x05, timeout=5)
        except TimeoutError:
            pass


def public_error(stage: str, error: BaseException) -> str:
    prefix = "cleanup failed" if stage == "cleanup" else stage
    if isinstance(error, AssertionError):
        return str(error)
    if isinstance(error, TimeoutError):
        return f"{prefix}: response timed out"
    if isinstance(error, ValueError):
        return f"{prefix}: invalid response or value"
    if isinstance(error, OSError):
        return f"{prefix}: connection or I/O error"
    return f"{prefix}: {type(error).__name__}"


def execute_case(case: HilCase, port: str, baud: int, output_dir: Path) -> dict:
    started = time.monotonic()
    stage = "connect"
    status = "SKIP"
    detail = "not run"
    artifacts = output_dir / case.test_id
    artifacts.mkdir(parents=True, exist_ok=True)
    try:
        with connect(port, baud, timeout=10) as client:
            stage = "reboot"
            try:
                client.request(0x05, timeout=5)
            except TimeoutError:
                pass
        stage = "boot"
        time.sleep(BOOT_WAIT_SECONDS)
        with connect(port, baud, timeout=20) as client:
            stage = "test"
            detail = case.run(client, artifacts)
        status = "PASS"
    except AssertionError as error:
        status, detail = "FAIL", str(error)
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        status = "FAIL" if stage == "test" else "SKIP"
        detail = public_error(stage, error)
    finally:
        if status != "PASS":
            try:
                recover_device(port, baud)
                detail += "; recovery reboot issued"
            except (OSError, RuntimeError, TimeoutError, ValueError) as cleanup_error:
                detail += f"; {public_error('cleanup', cleanup_error)}"
    return {
        "id": case.test_id,
        "title": case.title,
        "status": status,
        "detail": detail,
        "duration_s": round(time.monotonic() - started, 3),
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=os.environ.get("M1_PORT", "COM3"))
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--test",
        action="append",
        choices=[case.test_id for case in CASES],
        help="run only this test; repeat to select multiple tests",
    )
    parser.add_argument("--list", action="store_true", help="list tests without hardware")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("build/hil-results"),
        help="artifact directory (default: build/hil-results)",
    )
    parser.add_argument(
        "--update-pr",
        type=int,
        metavar="NUMBER",
        help="explicitly update this PR's HIL checklist through gh",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.list:
        for case in CASES:
            print(f"{case.test_id}: {case.title}")
        return 0

    selected = [case for case in CASES if not args.test or case.test_id in args.test]
    results = [execute_case(case, args.port, args.baud, args.output_dir) for case in selected]
    markdown, report = write_reports(args.output_dir, results)
    for result in results:
        print(f"{result['status']:4} {result['id']}: {result['detail']}")
    print(f"PR checklist: {markdown}")
    print(f"JSON results: {report}")

    if args.update_pr is not None:
        publish_results(args.update_pr, results)
        print(f"Updated HIL checklist in PR #{args.update_pr}")
    return 1 if any(result["status"] == "FAIL" for result in results) else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(public_error("run", error), file=sys.stderr)
        sys.exit(1)
