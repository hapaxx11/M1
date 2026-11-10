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
import m1_resume  # noqa: E402
import m1_saved_item_audit as saved_audit  # noqa: E402
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

    def test_active_ble_spam_is_opt_in_only(self):
        default_ids = {case.test_id for case in hil.select_cases(hil.CASES, None)}
        self.assertNotIn("ble-spam", default_ids)
        self.assertEqual(len(hil.CASES) - 1, len(default_ids))
        explicit_ids = {case.test_id for case in hil.select_cases(hil.CASES, ["ble-spam"])}
        self.assertEqual({"ble-spam"}, explicit_ids)

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
        self.assertEqual(48, len(wifi_audit.TOP_LEVEL_TARGETS))
        self.assertEqual(2, len(wifi_audit.TARGET_ACTIONS))
        self.assertEqual(5, len(wifi_audit.NETSCAN_ACTIONS))
        labels = [label for label, _path in wifi_audit.TOP_LEVEL_TARGETS]
        self.assertEqual(len(labels), len(set(labels)))
        for _label, path in wifi_audit.TOP_LEVEL_TARGETS:
            self.assertIn(path[0], range(7))

    def test_bluetooth_audit_covers_the_static_scene_tree(self):
        self.assertEqual(22, len(bt_audit.TOP_LEVEL_TARGETS))
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
        self.assertEqual(7, len(settings_audit.SKIPPED_DESTRUCTIVE))
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
        self.assertEqual(29, len(subghz_audit.TOP_LEVEL_TARGETS))
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
            mock.patch.object(misc_audit, "_reboot_main",
                              side_effect=lambda current: reopen(current, 9)),
            mock.patch.object(misc_audit, "_open_apps_menu", return_value=False),
            mock.patch.object(misc_audit, "_audit_path", side_effect=audit),
            mock.patch.object(misc_audit, "_audit_power_toggle"),
            mock.patch.object(misc_audit, "_wait_title", return_value=b"frame"),
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


class TestAuditSafety(unittest.TestCase):
    def test_navigation_rejects_unchanged_destination(self):
        for module in (bt_audit, wifi_audit):
            with (
                mock.patch.object(module, "_frame", return_value=b"same"),
                mock.patch.object(module, "_wait_change", return_value=b"same"),
                mock.patch.object(module, "_title_crc", return_value=1),
                mock.patch.object(module, "_click"),
            ):
                with self.assertRaisesRegex(AssertionError, "did not change"):
                    module._audit_path(mock.MagicMock(), (0,), False, 0.1) \
                        if module is bt_audit else module._audit_path(
                            mock.MagicMock(), (0,), 0.1)

    def test_parent_title_wait_allows_async_redraw(self):
        with (
            mock.patch.object(wifi_audit, "_frame", side_effect=(b"stale", b"parent")),
            mock.patch.object(wifi_audit, "_title_crc",
                              side_effect=lambda frame: {b"stale": 1, b"parent": 2}[frame]),
            mock.patch.object(wifi_audit.time, "monotonic", side_effect=(0.0, 0.0)),
            mock.patch.object(wifi_audit.time, "sleep"),
        ):
            self.assertEqual(b"parent", wifi_audit._wait_title(mock.MagicMock(), 2, 1.0))

    def test_apps_absence_never_launches_settings_as_an_app(self):
        client = mock.MagicMock()
        with (
            mock.patch.object(bt_audit, "_click") as click,
            mock.patch.object(bt_audit, "_frame", return_value=b"frame"),
            mock.patch.object(bt_audit, "_title_crc", return_value=0xB103D59A),
            mock.patch.object(bt_audit, "_wait_change", return_value=b"frame"),
        ):
            self.assertFalse(bt_audit._open_apps_menu(client))
        click.assert_called_once_with(client, "down", 10)

    def test_settings_skips_conditional_apps_entry(self):
        client = mock.MagicMock()
        with (
            mock.patch.object(bt_audit, "_click") as click,
            mock.patch.object(bt_audit, "_frame", return_value=b"frame"),
            mock.patch.object(bt_audit, "_title_crc",
                              side_effect=(1234, 0xB103D59A, 0xB103D59A)),
            mock.patch.object(bt_audit, "_wait_change",
                              side_effect=(b"frame", b"settings-open")),
        ):
            bt_audit._open_settings_menu(client)
        self.assertEqual(
            [mock.call(client, "down", 10), mock.call(client, "ok")],
            click.call_args_list,
        )

    def test_file_icon_detection_supports_all_text_sizes(self):
        for row, row_height in ((0, 8), (2, 10), (3, 13)):
            framebuffer = bytearray(1024)
            for dy, value in enumerate(saved_audit._FILE_ICON):
                for dx in range(10):
                    if (value >> dx) & 1:
                        y = 12 + row * row_height + dy
                        framebuffer[1 + dx + (y // 8) * 128] |= 1 << (y % 8)
            self.assertTrue(saved_audit._is_file_icon(framebuffer, row, row_height))

    def test_file_icon_must_be_on_selected_row(self):
        framebuffer = bytearray(1024)
        for dy, value in enumerate(saved_audit._FILE_ICON):
            for dx in range(10):
                if (value >> dx) & 1:
                    y = 12 + 8 + dy
                    framebuffer[1 + dx + (y // 8) * 128] |= 1 << (y % 8)
        self.assertFalse(saved_audit._has_file_icon(framebuffer))
        inverted = bytearray(1024)
        for dy, value in enumerate(saved_audit._FILE_ICON):
            for dx in range(10):
                if not ((value >> dx) & 1):
                    y = 12 + 8 + dy
                    inverted[1 + dx + (y // 8) * 128] |= 1 << (y % 8)
        self.assertTrue(saved_audit._has_file_icon(inverted))

    def test_nfc_saved_selection_skips_unsupported_files(self):
        client = mock.MagicMock()
        with (
            mock.patch.object(nfc_audit, "select_first_file", return_value=True),
            mock.patch.object(nfc_audit, "_has_file_icon", return_value=True),
            mock.patch.object(nfc_audit, "_frame", return_value=b"browser-first"),
            mock.patch.object(nfc_audit, "_title_crc", return_value=42),
            mock.patch.object(nfc_audit, "_wait_change", side_effect=(
                b"unsupported", b"unsupported", b"browser-first", b"browser-second",
                b"actions", b"actions-next", b"actions",
            )),
            mock.patch.object(nfc_audit, "_click") as click,
        ):
            self.assertEqual(42, nfc_audit._select_saved(client))
        self.assertEqual(3, click.call_args_list.count(mock.call(client, "ok")))
        self.assertEqual(1, click.call_args_list.count(mock.call(client, "back")))
        self.assertEqual(1, click.call_args_list.count(mock.call(client, "up")))

    def test_reboot_audit_resets_settings_cursor_first(self):
        client = mock.MagicMock()
        with (
            mock.patch.object(settings_audit, "_reboot_settings", return_value=client) as reopen,
            mock.patch.object(settings_audit, "_frame", return_value=b"frame"),
            mock.patch.object(settings_audit, "_click"),
            mock.patch.object(settings_audit, "_wait_change"),
            mock.patch.object(settings_audit, "connect", return_value=client),
            mock.patch.object(settings_audit.time, "sleep"),
        ):
            settings_audit._audit_power_reboot(client)
        reopen.assert_called_once_with(client)

    def test_power_toggle_restores_original_frame(self):
        original, changed = b"original", b"changed"
        with (
            mock.patch.object(misc_audit, "_frame", return_value=original),
            mock.patch.object(misc_audit, "_wait_change",
                              side_effect=(changed, original, original)),
            mock.patch.object(misc_audit, "_click"),
        ):
            misc_audit._audit_power_toggle(mock.MagicMock(), 1)
        with (
            mock.patch.object(misc_audit, "_frame", return_value=original),
            mock.patch.object(misc_audit, "_wait_change", return_value=changed),
            mock.patch.object(misc_audit, "_click"),
        ):
            with self.assertRaisesRegex(AssertionError, "not restored"):
                misc_audit._audit_power_toggle(mock.MagicMock(), 1)

    def test_focused_ble_spam_starts_active_mode_before_dwell(self):
        client = mock.MagicMock()
        with (
            mock.patch.object(hil, "open_bluetooth"),
            mock.patch.object(hil, "click") as click,
            mock.patch.object(hil, "capture", side_effect=(b"menu", b"picker", b"active")),
            mock.patch.object(hil, "capture_until_change",
                              side_effect=(b"active", b"stopped", b"picker",
                                           b"active", b"cleanup")) as transition,
            mock.patch.object(hil, "assert_title"),
            mock.patch.object(hil.time, "sleep") as sleep,
        ):
            result = hil.run_ble_spam(client, mock.MagicMock())
        self.assertIn("active stop/restart passed", result)
        self.assertEqual(5, transition.call_count)
        sleep.assert_called_once_with(17.0)
        self.assertEqual(
            [mock.call(client, "down", 5), mock.call(client, "ok"),
             mock.call(client, "ok")],
            click.call_args_list[:3],
        )

    def test_subghz_saved_layout_is_verified_before_actions(self):
        client = mock.MagicMock()
        with (
            mock.patch.object(subghz_audit, "select_first_file", return_value=True),
            mock.patch.object(subghz_audit, "_click"),
            mock.patch.object(subghz_audit, "_frame", return_value=b"raw"),
            mock.patch.object(subghz_audit, "_title_crc", return_value=1234),
        ):
            self.assertEqual("raw", subghz_audit._select_saved(client, 1234))
        with (
            mock.patch.object(subghz_audit, "select_first_file", return_value=True),
            mock.patch.object(subghz_audit, "_click"),
            mock.patch.object(subghz_audit, "_frame", return_value=b"parsed"),
            mock.patch.object(subghz_audit, "_title_crc", return_value=5678),
        ):
            self.assertEqual("parsed", subghz_audit._select_saved(client, 1234))

    def test_recursive_audits_exclude_auto_start_rf_actions(self):
        self.assertFalse(any(label.startswith("Attacks/") or "Flood" in label
                             for label, _path in wifi_audit.TOP_LEVEL_TARGETS))
        self.assertEqual({"Target/Handshake", "Target/Cycle AP"},
                         {label for label, _selection in wifi_audit.TARGET_ACTIONS})
        self.assertFalse(any(label == "BLE Advertise" or
                             (label.startswith("BLE Spam/") and
                              label != "BLE Spam/Unified picker")
                             for label, _path, _edit in bt_audit.TOP_LEVEL_TARGETS))

    def test_delete_actions_navigate_to_their_actual_indices(self):
        for module, expected in ((rfid_audit, 1), (nfc_audit, 1), (subghz_audit, 2)):
            source = pathlib.Path(module.__file__).read_text(encoding="utf-8")
            self.assertGreaterEqual(source.count('_click(client, "down", selection)'),
                                    expected)
        source = pathlib.Path(subghz_audit.__file__).read_text(encoding="utf-8")
        self.assertIn('_click(client, "down", 17)', source)


class TestResume(unittest.TestCase):
    def test_checkpoint_skips_passed_destinations_and_clears_on_success(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            m1_resume.start("demo", root, fresh=True)
            self.assertTrue(m1_resume.should_run("first"))
            m1_resume.log_pass("first")
            self.assertFalse(m1_resume.should_run("first"))
            m1_resume.start("demo", root)
            self.assertFalse(m1_resume.should_run("first"))
            self.assertTrue(m1_resume.should_run("second"))
            m1_resume.log_pass("second")
            m1_resume.finish(True)
            self.assertFalse((root / "demo-checkpoint.json").exists())

    def test_fresh_and_version_change_invalidate_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            m1_resume.start("demo", root, fresh=True)
            m1_resume.log_pass("first")
            path = root / "demo-checkpoint.json"
            m1_resume.start("demo", root, fresh=True)
            self.assertFalse(path.exists())
            self.assertTrue(m1_resume.should_run("first"))
            path.write_text('{"format":"1","version":"stale","passed":["first"]}',
                            encoding="utf-8")
            m1_resume.start("demo", root)
            self.assertTrue(m1_resume.should_run("first"))
            m1_resume.finish(False)

    def test_unstarted_logging_is_safe_for_host_tests(self):
        m1_resume.finish(False)
        self.assertTrue(m1_resume.should_run("anything"))
        m1_resume.log_pass("anything")

    def test_failure_preserves_only_passed_destinations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            m1_resume.start("demo", root, fresh=True)
            m1_resume.log_pass("first")
            m1_resume.finish(False)
            self.assertTrue((root / "demo-checkpoint.json").exists())
            m1_resume.start("demo", root)
            self.assertEqual(["second"], m1_resume.resume_targets(["first", "second"]))
            m1_resume.finish(False)

    def test_sequential_editors_advance_without_retesting_passed_steps(self):
        client = mock.MagicMock()
        with (
            mock.patch.object(subghz_audit, "_reboot_menu", return_value=client),
            mock.patch.object(subghz_audit, "_click") as click,
            mock.patch.object(subghz_audit, "_frame"),
            mock.patch.object(subghz_audit, "should_run",
                              side_effect=(False, True, False)) as should_run,
            mock.patch.object(subghz_audit, "log_pass") as log_pass,
        ):
            self.assertIs(client, subghz_audit._run_keeloq_editor_chain(client))
        self.assertEqual(
            [mock.call(f"Add Manually/{label} (cancel)")
             for label in subghz_audit.KEELOQ_EDITOR_CHAIN],
            should_run.call_args_list,
        )
        log_pass.assert_called_once_with("Add Manually/Set Counter (cancel)")
        self.assertEqual(7, click.call_count)

    def test_unrelated_audit_changes_do_not_invalidate_checkpoint(self):
        names = {path.name for path in m1_resume._source_paths("nfc-menu-audit")}
        self.assertIn("m1_nfc_menu_audit.py", names)
        self.assertIn("m1_saved_item_audit.py", names)
        self.assertIn("m1_hil.py", names)
        self.assertIn("m1_hil_test.py", names)
        self.assertNotIn("m1_subghz_menu_audit.py", names)

    def test_actual_misc_labels_survive_checkpoint_reload(self):
        groups = (
            misc_audit.IR_TARGETS,
            misc_audit.IR_SAVED_ACTIONS,
            misc_audit.CUSTOM_REMOTE_ACTIONS,
            misc_audit.CUSTOM_BUTTON_ACTIONS,
            misc_audit.GPIO_TARGETS,
            misc_audit.GAMES_TARGETS,
        )
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            for targets in groups:
                with self.subTest(label=targets[0][0]):
                    m1_resume.start("misc-menu-audit", root, fresh=True)
                    m1_resume.log_pass(targets[0][0])
                    m1_resume.finish(False)
                    m1_resume.start("misc-menu-audit", root)
                    remaining = m1_resume.resume_targets(targets)
                    self.assertNotIn(targets[0][0], {target[0] for target in remaining})
                    self.assertIn(targets[1][0], {target[0] for target in remaining})
                    m1_resume.finish(False)

    def test_completed_manual_editor_is_skipped_after_checkpoint_reload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            targets = subghz_audit.ADD_MANUALLY_TARGETS
            self.assertEqual("Add Manually/CAME Atomo 433 editor (cancel)", targets[0][0])
            m1_resume.start("subghz-menu-audit", root, fresh=True)
            m1_resume.log_pass(targets[0][0])
            m1_resume.finish(False)
            m1_resume.start("subghz-menu-audit", root)
            remaining = m1_resume.resume_targets(targets)
            self.assertNotIn(targets[0][0], {target[0] for target in remaining})
            self.assertIn(targets[1][0], {target[0] for target in remaining})
            m1_resume.finish(False)

    def test_moreraw_skip_matches_saved_file_type(self):
        client = mock.MagicMock()
        for kind, expected, unexpected in (
            ("raw", "Saved actions", "MoreRAW actions"),
            ("parsed", "MoreRAW actions", "Saved actions"),
        ):
            with self.subTest(kind=kind):
                with (
                    mock.patch.object(subghz_audit, "_read_raw_title", return_value=(client, 1)),
                    mock.patch.object(subghz_audit, "_reboot_menu", return_value=client),
                    mock.patch.object(subghz_audit, "_select_saved", return_value=kind),
                    mock.patch.object(subghz_audit, "resume_targets", return_value=[]),
                ):
                    _client, skipped = subghz_audit._run_saved_actions(client)
            self.assertTrue(any(item.startswith(expected) for item in skipped))
            self.assertFalse(any(item.startswith(unexpected) for item in skipped))


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
