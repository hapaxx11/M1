#!/usr/bin/env python3
"""Tests for single-device HIL reporting; no hardware is accessed."""

import argparse
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
import m1_hil as driver  # noqa: E402
import m1_hil_test as hil  # noqa: E402
import m1_bt_menu_audit as bt_audit  # noqa: E402
import m1_misc_menu_audit as misc_audit  # noqa: E402
import m1_nfc_menu_audit as nfc_audit  # noqa: E402
import m1_rfid_menu_audit as rfid_audit  # noqa: E402
import m1_settings_menu_audit as settings_audit  # noqa: E402
import m1_subghz_menu_audit as subghz_audit  # noqa: E402
import m1_wifi_menu_audit as wifi_audit  # noqa: E402


class TestRegistry(unittest.TestCase):
    def test_ids_are_unique_and_self_descriptive(self):
        ids = [case.test_id for case in hil.CASES]
        self.assertEqual(len(ids), len(set(ids)))
        for case in hil.CASES:
            self.assertRegex(case.test_id, r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
            self.assertTrue(case.title)

    def test_every_top_level_menu_has_registered_recursive_audit(self):
        expected = {
            "wifi-menu-audit", "bluetooth-menu-audit", "rfid-menu-audit",
            "nfc-menu-audit", "settings-menu-audit", "misc-menu-audit",
            "subghz-menu-audit",
        }
        registered = {case.test_id for case in hil.CASES}
        self.assertTrue(expected.issubset(registered))
        guide = (pathlib.Path(__file__).resolve().parents[1] /
                 "documentation" / "hil_menu_coverage.md").read_text(encoding="utf-8")
        for test_id in expected:
            self.assertIn(f"`{test_id}`", guide)
        for menu in ("Sub-GHz", "RFID", "NFC", "Infrared", "GPIO/CAN",
                     "Games", "BadUSB", "Apps", "WiFi", "Bluetooth", "Settings"):
            self.assertIn(menu, guide)

    def test_wifi_audit_covers_the_static_scene_tree(self):
        self.assertEqual(57, len(wifi_audit.TOP_LEVEL_TARGETS))
        self.assertEqual(6, len(wifi_audit.TARGET_ACTIONS))
        self.assertEqual(5, len(wifi_audit.NETSCAN_ACTIONS))
        labels = [label for label, _path in wifi_audit.TOP_LEVEL_TARGETS]
        self.assertEqual(len(labels), len(set(labels)))
        for _label, path in wifi_audit.TOP_LEVEL_TARGETS:
            self.assertIn(path[0], range(7))

    def test_bluetooth_audit_covers_the_static_scene_tree(self):
        self.assertEqual(33, len(bt_audit.TOP_LEVEL_TARGETS))
        labels = [label for label, _path, _edit in bt_audit.TOP_LEVEL_TARGETS]
        self.assertEqual(len(labels), len(set(labels)))
        for _label, path, _edit in bt_audit.TOP_LEVEL_TARGETS:
            self.assertIn(path[0], range(15))
        self.assertIn(("BLE Config/Edit", (3,), True), bt_audit.TOP_LEVEL_TARGETS)

    def test_rfid_and_nfc_audits_cover_the_static_scene_trees(self):
        self.assertEqual(37, len(rfid_audit.TOP_LEVEL_TARGETS))
        self.assertEqual(26, len(nfc_audit.TOP_LEVEL_TARGETS))
        for module, count in ((rfid_audit, 6), (nfc_audit, 6)):
            labels = [label for label, _path, _edit in module.TOP_LEVEL_TARGETS]
            self.assertEqual(len(labels), len(set(labels)))
            for _label, path, _edit in module.TOP_LEVEL_TARGETS:
                self.assertIn(path[0], range(count))

    def test_settings_audit_covers_safe_destinations(self):
        self.assertEqual(26, len(settings_audit.TOP_LEVEL_TARGETS))
        self.assertEqual(6, len(settings_audit.SKIPPED_DESTRUCTIVE))
        labels = [label for label, _path, _edit in settings_audit.TOP_LEVEL_TARGETS]
        self.assertEqual(len(labels), len(set(labels)))
        for _label, path, _edit in settings_audit.TOP_LEVEL_TARGETS:
            self.assertIn(path[0], range(7))

    def test_misc_audit_covers_remaining_non_subghz_menus(self):
        self.assertEqual(17, len(misc_audit.IR_TARGETS))
        self.assertEqual(8, len(misc_audit.GPIO_TARGETS))
        self.assertEqual(13, len(misc_audit.GAMES_TARGETS))
        self.assertEqual(4, len(misc_audit.IR_SAVED_ACTIONS))
        self.assertEqual(3, len(misc_audit.CUSTOM_REMOTE_ACTIONS))
        self.assertEqual(2, len(misc_audit.CUSTOM_BUTTON_ACTIONS))
        for module, count in ((misc_audit.IR_TARGETS, 5),
                              (misc_audit.GPIO_TARGETS, 6),
                              (misc_audit.GAMES_TARGETS, 13)):
            labels = [label for label, _path, _repeat in module]
            self.assertEqual(len(labels), len(set(labels)))
            for _label, path, _repeat in module:
                self.assertIn(path[0], range(count))

    def test_subghz_audit_covers_safe_destinations(self):
        self.assertEqual(32, len(subghz_audit.TOP_LEVEL_TARGETS))
        self.assertEqual(20, len(subghz_audit.ADD_MANUALLY_TARGETS))
        self.assertEqual(3, len(subghz_audit.KEELOQ_EDITOR_CHAIN))
        self.assertEqual(4, len(subghz_audit.SAVED_ACTIONS))
        self.assertEqual(3, len(subghz_audit.MORE_RAW_ACTIONS))
        for _label, path, _config in subghz_audit.TOP_LEVEL_TARGETS:
            self.assertIn(path[0], range(13))

    def test_misc_audit_never_selects_badusb_scripts_or_external_apps(self):
        client = mock.MagicMock()
        current_menu = []
        audited_menus = []

        def reopen(current, selection):
            current_menu.append(selection)
            return current

        def audit(_client, _path, _edit, _wait):
            audited_menus.append(current_menu[-1])

        with (
            mock.patch.object(misc_audit, "_reboot_menu", side_effect=reopen),
            mock.patch.object(misc_audit, "_audit_path", side_effect=audit),
            mock.patch.object(misc_audit, "_run_ir_saved_actions",
                              side_effect=lambda current: (current, 0, [])),
            mock.patch.object(misc_audit, "_run_custom_actions",
                              side_effect=lambda current: (current, 0, [])),
            mock.patch.object(misc_audit, "_frame", return_value=b"frame"),
            mock.patch.object(misc_audit, "_title_crc", return_value=0xB103D59A),
        ):
            misc_audit.run(client)
        self.assertTrue(all(selection not in (7, 9) for selection in audited_menus))

    def test_custom_remote_detection_ignores_keyboard_animation(self):
        client = mock.MagicMock()
        title_crcs = {
            b"keyboard-old": 1234,
            b"keyboard-animated": 1234,
            b"custom-buttons": 5678,
        }
        with (
            mock.patch.object(misc_audit, "_open_custom_root", return_value=client),
            mock.patch.object(misc_audit, "_custom_new_remote_vkb",
                              return_value=b"keyboard-old"),
            mock.patch.object(misc_audit, "_frame", return_value=b"root"),
            mock.patch.object(misc_audit, "_wait_change",
                              return_value=b"keyboard-animated"),
            mock.patch.object(misc_audit, "_title_crc",
                              side_effect=lambda frame: title_crcs[frame]),
            mock.patch.object(misc_audit, "_click"),
        ):
            _client, present = misc_audit._open_custom_remote(client)
        self.assertFalse(present)

    def test_delete_audits_never_confirm_after_opening_dialog(self):
        unsafe = ('_frame(client) _click(client, "down") '
                  '_click(client, "ok") _frame(client)')
        for module in (misc_audit, rfid_audit, nfc_audit, subghz_audit):
            source = pathlib.Path(module.__file__).read_text(encoding="utf-8")
            normalized = " ".join(source.split())
            self.assertNotIn(unsafe, normalized, module.__file__)


class TestScreenTransitions(unittest.TestCase):
    @mock.patch("m1_hil_test.time.sleep", return_value=None)
    @mock.patch("m1_hil_test.capture", side_effect=[b"old", b"old", b"new"])
    def test_stale_frames_are_ignored_until_transition(self, capture, _sleep):
        actual = hil.capture_until_change(None, pathlib.Path("."), "scene", b"old", 1.0)
        self.assertEqual(actual, b"new")
        self.assertEqual(capture.call_count, 3)

    @mock.patch("m1_hil_test.time.monotonic", side_effect=[0.0, 2.0])
    @mock.patch("m1_hil_test.capture", return_value=b"same")
    def test_unchanged_screen_times_out(self, _capture, _monotonic):
        with self.assertRaisesRegex(AssertionError, "did not change"):
            hil.capture_until_change(None, pathlib.Path("."), "scene", b"same", 1.0)

    def test_wrong_screen_identity_is_rejected(self):
        framebuffer = bytearray(1024)
        expected = hil.title_crc(framebuffer)
        framebuffer[0] = 1
        with self.assertRaisesRegex(AssertionError, "unexpected screen identity"):
            hil.assert_title(framebuffer, expected, "negative case")

    @mock.patch("m1_hil_test.capture", side_effect=[bytes(1024), bytes([1]) * 1024])
    def test_failure_dialog_is_not_a_successful_entry(self, _capture):
        with self.assertRaisesRegex(AssertionError, "entry"):
            hil.exercise_scene(
                None, pathlib.Path("."), "scene", 0, 0, lambda: None, 1.0
            )

    @mock.patch("m1_hil_test.click")
    @mock.patch("m1_hil_test.assert_title")
    @mock.patch("m1_hil_test.time.sleep", return_value=None)
    @mock.patch("m1_hil_test.time.monotonic", return_value=0.0)
    @mock.patch(
        "m1_hil_test.capture",
        side_effect=[b"before", b"active", b"active", b"menu", b"reentry", b"cleanup"],
    )
    def test_active_dwell_recaptures_before_exit(
        self, capture, _monotonic, sleep, assert_title, _click
    ):
        client = mock.MagicMock()
        hil.exercise_scene(
            client, pathlib.Path("."), "scene", 1, 2, lambda: None, 1.0,
            active_dwell_s=17.0,
        )
        sleep.assert_called_once_with(17.0)
        self.assertEqual(capture.call_count, 6)
        self.assertEqual(assert_title.call_count, 5)
        client.info.assert_called_once_with()


class TestRecovery(unittest.TestCase):
    @mock.patch("m1_hil_test.time.sleep", return_value=None)
    @mock.patch("m1_hil_test.recover_device")
    @mock.patch("m1_hil_test.connect")
    def test_failure_issues_bounded_recovery_reboot(self, connect, recover, _sleep):
        client = mock.MagicMock()
        client.__enter__.return_value = client
        connect.return_value = client

        def fail(_client, _artifacts):
            raise AssertionError("boom")

        case = hil.HilCase("negative", "negative", fail)
        with tempfile.TemporaryDirectory() as directory:
            result = hil.execute_case(case, "COM3", 115200, pathlib.Path(directory))
        self.assertEqual(result["status"], "FAIL")
        self.assertIn("boom", result["detail"])
        recover.assert_called_once_with("COM3", 115200)

    @mock.patch("m1_hil_test.time.sleep", return_value=None)
    @mock.patch("m1_hil_test.recover_device", side_effect=OSError("cleanup failed"))
    @mock.patch("m1_hil_test.connect")
    def test_cleanup_failure_does_not_hide_original_failure(self, connect, _recover, _sleep):
        client = mock.MagicMock()
        client.__enter__.return_value = client
        connect.return_value = client

        def fail(_client, _artifacts):
            raise AssertionError("original failure")

        case = hil.HilCase("negative", "negative", fail)
        with tempfile.TemporaryDirectory() as directory:
            result = hil.execute_case(case, "COM3", 115200, pathlib.Path(directory))
        self.assertEqual(result["status"], "FAIL")
        self.assertIn("original failure", result["detail"])
        self.assertIn("cleanup failed", result["detail"])


    @mock.patch("m1_hil_test.time.sleep", return_value=None)
    @mock.patch("m1_hil_test.recover_device", side_effect=ValueError("invalid CRC"))
    @mock.patch("m1_hil_test.connect")
    def test_value_error_during_recovery_preserves_original_failure(self, connect, _recover, _sleep):
        client = mock.MagicMock()
        client.__enter__.return_value = client
        connect.return_value = client

        def fail(_client, _artifacts):
            raise AssertionError("original failure")

        case = hil.HilCase("negative", "negative", fail)
        with tempfile.TemporaryDirectory() as directory:
            result = hil.execute_case(case, "COM3", 115200, pathlib.Path(directory))
        self.assertEqual(result["status"], "FAIL")
        self.assertIn("original failure", result["detail"])
        self.assertIn("invalid response", result["detail"])
        self.assertNotIn("CRC", result["detail"])


class TestTransitionBudgets(unittest.TestCase):
    @mock.patch("m1_hil_test.capture", return_value=b"new")
    @mock.patch("m1_hil_test.time.sleep", return_value=None)
    @mock.patch("m1_hil_test.time.monotonic", side_effect=[0.0, 0.0, 2.0])
    def test_capture_after_deadline_is_rejected(self, _monotonic, _sleep, _capture):
        with self.assertRaisesRegex(AssertionError, "after"):
            hil.capture_until_change(None, pathlib.Path("."), "scene", b"old", 1.0)


class TestArtifacts(unittest.TestCase):
    def test_capture_artifacts_retain_only_title_region(self):
        framebuffer = bytes([0xFF]) * 1024
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "capture.png"
            hil.write_redacted_capture(path, framebuffer)
            redacted = path.with_suffix(".fb").read_bytes()
            self.assertTrue(path.exists())
        for x in range(128):
            for y in range(64):
                bit = (redacted[x + (y // 8) * 128] >> (y % 8)) & 1
                self.assertEqual(bit, 1 if y < 10 else 0)

    def test_public_errors_do_not_include_private_paths(self):
        detail = hil.public_error("test", OSError("C:/private/example"))
        self.assertNotIn("private", detail)
        self.assertNotIn("example", detail)


class TestStaging(unittest.TestCase):
    def test_write_bytes_sync_flushes_file_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "image.bin"
            with mock.patch("m1_hil.os.fsync") as fsync:
                driver.write_bytes_sync(path, b"firmware")
            self.assertEqual(path.read_bytes(), b"firmware")
            fsync.assert_called_once()

    def test_stage_ejects_volume_before_remount(self):
        events = []
        client = mock.MagicMock()
        client.__enter__.return_value = client
        with tempfile.TemporaryDirectory() as directory:
            image = pathlib.Path(directory) / "input.bin"
            image.write_bytes(b"firmware")
            args = argparse.Namespace(
                image=image,
                name="app.bin",
                drive=pathlib.Path(directory),
                port="COM3",
                baud=115200,
            )
            with (
                mock.patch("m1_hil.M1Hil", return_value=client),
                mock.patch("m1_hil.eject_volume", side_effect=lambda _drive: events.append("eject")),
                mock.patch("m1_hil.connect", side_effect=lambda *_args, **_kwargs: events.append("mount") or client),
            ):
                driver.stage_esp32(args)
        self.assertEqual(events, ["eject", "mount"])


class TestPrReporting(unittest.TestCase):
    def test_only_pass_is_checked(self):
        results = [
            {"id": "pass-case", "status": "PASS", "detail": "entry 10 ms"},
            {"id": "fail-case", "status": "FAIL", "detail": "timeout"},
            {"id": "skip-case", "status": "SKIP", "detail": "connect: unavailable"},
        ]
        rendered = hil.render_pr_results(results)
        self.assertIn("- [x] `pass-case` — PASS — entry 10 ms", rendered)
        self.assertIn("- [ ] `fail-case` — FAIL — timeout", rendered)
        self.assertIn("- [ ] `skip-case` — SKIP — connect: unavailable", rendered)

    def test_marked_block_is_replaced_and_other_content_is_preserved(self):
        original = (
            "## Description\nKeep this text.\n"
            f"{hil.RESULT_START}\n- [ ] old\n{hil.RESULT_END}\n"
            "## Checklist\nKeep this too.\n"
        )
        updated = hil.update_pr_body(
            original, [{"id": "wifi-airtime", "status": "PASS", "detail": "ok"}]
        )
        self.assertIn("Keep this text.", updated)
        self.assertIn("Keep this too.", updated)
        self.assertIn("- [x] `wifi-airtime` — PASS — ok", updated)
        self.assertNotIn("- [ ] old", updated)
        self.assertEqual(updated.count(hil.RESULT_START), 1)
        self.assertEqual(updated.count(hil.RESULT_END), 1)

    def test_selected_result_preserves_unexecuted_cases(self):
        original = (
            f"{hil.RESULT_START}\n"
            "- [ ] `wifi-airtime` — SKIP\n"
            "- [ ] `wifi-peer-link` — SKIP\n"
            "- [ ] `ble-signal-finder` — SKIP\n"
            f"{hil.RESULT_END}"
        )
        updated = hil.update_pr_body(
            original, [{"id": "wifi-airtime", "status": "PASS", "detail": "ok"}]
        )
        self.assertIn("- [x] `wifi-airtime` — PASS — ok", updated)
        self.assertIn("- [ ] `wifi-peer-link` — SKIP", updated)
        self.assertIn("- [ ] `ble-signal-finder` — SKIP", updated)

    def test_missing_markers_append_nonblocking_hil_section(self):
        updated = hil.update_pr_body(
            "Existing PR body\n",
            [{"id": "wifi-peer-link", "status": "SKIP", "detail": "no hardware"}],
        )
        self.assertTrue(updated.startswith("Existing PR body\n\n## Single-device HIL"))
        self.assertIn("never a merge blocker", updated)
        self.assertIn("- [ ] `wifi-peer-link` — SKIP — no hardware", updated)

    def test_reports_include_json_and_pr_markdown(self):
        results = [{"id": "ble-signal-finder", "status": "PASS", "detail": "ok"}]
        with tempfile.TemporaryDirectory() as directory:
            markdown, report = hil.write_reports(pathlib.Path(directory), results)
            self.assertIn("- [x] `ble-signal-finder`", markdown.read_text())
            self.assertIn('"status": "PASS"', report.read_text())

    @mock.patch("m1_hil_test.connect", side_effect=OSError("hardware unavailable"))
    def test_missing_hardware_is_a_skip_not_a_failure(self, _connect):
        with tempfile.TemporaryDirectory() as directory:
            result = hil.execute_case(hil.CASES[0], "COM3", 115200, pathlib.Path(directory))
        self.assertEqual(result["status"], "SKIP")
        self.assertIn("connection or I/O error", result["detail"])

    @mock.patch("m1_hil_test.subprocess.run")
    def test_publish_results_uses_gh_and_preserves_body(self, run):
        edited = []

        def fake_gh(command, check, **kwargs):
            self.assertTrue(check)
            if "--jq" in command:
                return subprocess.CompletedProcess(command, 0, stdout="Existing body")
            edited.append(pathlib.Path(command[-1].removeprefix("body=@")).read_text(encoding="utf-8"))
            return subprocess.CompletedProcess(command, 0, stdout="")

        run.side_effect = fake_gh
        hil.publish_results(42, [{"id": "wifi-airtime", "status": "PASS", "detail": "ok"}])
        self.assertEqual(
            run.call_args_list[0].args[0][:3],
            ["gh", "api", "repos/{owner}/{repo}/pulls/42"],
        )
        self.assertEqual(run.call_args_list[1].args[0][:4], ["gh", "api", "--method", "PATCH"])
        self.assertEqual(len(edited), 1)
        self.assertIn("Existing body", edited[0])
        self.assertIn("- [x] `wifi-airtime` — PASS — ok", edited[0])


if __name__ == "__main__":
    unittest.main()
