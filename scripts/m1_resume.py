#!/usr/bin/env python3
"""Resumable per-destination checkpoints for recursive HIL audits."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import m1_ux_audit

_VERSION = "1"
_AUDIT_SOURCES = {
    "wifi-menu-audit": "m1_wifi_menu_audit.py",
    "bluetooth-menu-audit": "m1_bt_menu_audit.py",
    "rfid-menu-audit": "m1_rfid_menu_audit.py",
    "nfc-menu-audit": "m1_nfc_menu_audit.py",
    "settings-menu-audit": "m1_settings_menu_audit.py",
    "misc-menu-audit": "m1_misc_menu_audit.py",
    "subghz-menu-audit": "m1_subghz_menu_audit.py",
    "tx-menu-audit": "m1_tx_menu_audit.py",
}
_SHARED_SOURCES = (
    "m1_resume.py",
    "m1_saved_item_audit.py",
    "m1_bt_menu_audit.py",
    "m1_wifi_menu_audit.py",
    "m1_hil.py",
    "m1_hil_test.py",
    "m1_ux_audit.py",
)
_state: dict[str, object] = {}


def _source_paths(test_id: str) -> list[Path]:
    root = Path(__file__).parent
    names = set(_SHARED_SOURCES)
    if test_id in _AUDIT_SOURCES:
        names.add(_AUDIT_SOURCES[test_id])
        if test_id == "tx-menu-audit":
            names.update((
                "m1_misc_menu_audit.py",
                "m1_nfc_menu_audit.py",
                "m1_subghz_menu_audit.py",
            ))
    else:
        names.update(path.name for path in root.glob("m1_*menu_audit.py"))
    return [root / name for name in sorted(names)]


def _version(test_id: str) -> str:
    digest = hashlib.sha256()
    for path in _source_paths(test_id):
        digest.update(path.read_bytes())
    return digest.hexdigest()


def start(test_id: str, directory: Path, fresh: bool = False) -> None:
    global _state
    path = directory / f"{test_id}-checkpoint.json"
    passed: set[str] = set()
    version = _version(test_id)
    if fresh:
        path.unlink(missing_ok=True)
    if not fresh and path.exists():
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            if data.get("version") == version and data.get("format") == _VERSION:
                passed = set(data.get("passed", []))
        except (OSError, ValueError, TypeError):
            pass
    _state = {"path": path, "version": version, "passed": passed}
    if passed:
        print(f"RESUME {test_id}: {len(passed)} destinations already passed", flush=True)


def should_run(label: str) -> bool:
    if not _state or label not in _state["passed"]:
        return True
    if m1_ux_audit.needs_result(label):
        print(f"RESUME RERUN {label}: UX result missing", flush=True)
        return True
    print(f"RESUME SKIP {label}", flush=True)
    return False


def resume_targets(targets):
    return [
        target for target in targets
        if should_run(target if isinstance(target, str) else target[0])
    ]


def log_pass(label: str) -> None:
    print(f"PASS {label}", flush=True)
    m1_ux_audit.evaluate(label)
    if not _state:
        return
    path = _state["path"]
    passed = _state["passed"]
    if label in passed:
        return
    passed.add(label)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(
        json.dumps({"format": _VERSION, "version": _state["version"],
                    "passed": sorted(passed)}, indent=2) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def finish(success: bool) -> None:
    if success:
        _state["path"].unlink(missing_ok=True)
    _state.clear()
