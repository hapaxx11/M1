#!/usr/bin/env python3
"""Privacy-preserving visual-geometry checks for recursive HIL audits."""

from __future__ import annotations

import json
from pathlib import Path

_state: dict[str, object] = {}


def start(test_id: str, directory: Path, enabled: bool) -> None:
    global _state
    _state = {
        "enabled": enabled,
        "path": directory / f"{test_id}-ux.json",
        "frame": None,
        "results": [],
    }


def observe(framebuffer: bytes) -> None:
    if _state.get("enabled") and len(framebuffer) == 1024:
        _state["frame"] = bytes(framebuffer)


def _dark_count(framebuffer: bytes, y: int) -> int:
    return sum(
        (framebuffer[x + (y // 8) * 128] >> (y % 8)) & 1
        for x in range(128)
    )


def analyze(framebuffer: bytes) -> list[str]:
    if len(framebuffer) != 1024:
        return ["invalid-frame"]
    findings: list[str] = []
    top = [_dark_count(framebuffer, y) for y in range(10)]
    body = [_dark_count(framebuffer, y) for y in range(12, 32)]
    if sum(top) > 10 * 128 * 85 // 100 and sum(body) < len(body) * 128 // 2:
        findings.append("inverted-header")
    if 0 < sum(top) < 10 * 128 * 85 // 100:
        if not any(_dark_count(framebuffer, y) >= 100 for y in range(9, 15)):
            findings.append("header-no-separator")
    solid_rows = sum(_dark_count(framebuffer, y) >= 120 for y in range(52, 64))
    has_button_gap = any(
        sum((framebuffer[x + (y // 8) * 128] >> (y % 8)) & 1 for y in range(52, 64)) <= 2
        for x in (42, 43, 63, 64, 85, 86)
    )
    if solid_rows >= 8 and not has_button_gap:
        findings.append("solid-bottom-bar")
    return findings


def evaluate(label: str) -> None:
    if not _state.get("enabled"):
        return
    frame = _state.get("frame")
    findings = ["no-destination-frame"] if frame is None else analyze(frame)
    _state["results"].append({"label": label, "findings": findings})
    _state["frame"] = None
    if findings:
        print(f"UX REVIEW {label}: {', '.join(findings)}", flush=True)


def finish() -> None:
    if _state.get("enabled"):
        path = _state["path"]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(_state["results"], indent=2) + "\n", encoding="utf-8")
        count = sum(bool(item["findings"]) for item in _state["results"])
        print(f"UX REVIEW: {count} destinations need source review", flush=True)
    _state.clear()
