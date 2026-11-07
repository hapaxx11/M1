<!-- See COPYING.txt for license details. -->

# Menu-to-HIL coverage and PR test selection

Every user-visible menu feature must be covered by at least one registered
single-device HIL case. A safe entry/exit/re-entry check in a recursive menu
audit is the minimum. Add a focused lifecycle case when the feature needs
specific state transitions or regression coverage beyond responsiveness.

The authoritative destinations are the target tables and saved-item actions in
`scripts/m1_*_menu_audit.py`. Whenever a change could have altered menu
structure, labels, ordering, navigation, available actions, or hardware
lifecycle, inspect and update the relevant audit in the same change, even when
no UI change was intended. This includes refactors and handler/lifecycle fixes:
do not assume existing coverage still matches. Adjust the ordinary Python tests
in `tests/test_m1_hil_test.py` at the same time.

## Test IDs to suggest in PRs and reviews

| Menu or feature area | Required recursive HIL case | Additional focused cases when relevant |
|---|---|---|
| Sub-GHz, including Analyzer, saved/MoreRAW, Add Manually, Remote, and Bind | `subghz-menu-audit` | Add a focused case for new radio state transitions |
| RFID, all manual protocols, utilities, and saved actions | `rfid-menu-audit` | — |
| NFC, Extra, NDEF, manual card types, saved and card actions | `nfc-menu-audit` | — |
| Infrared, GPIO/CAN, Games, BadUSB, and Apps | `misc-menu-audit` | — |
| WiFi, Target, Net Scan, General, 802.15.4, and Peer Link | `wifi-menu-audit` | `wifi-airtime`, `wifi-peer-link` |
| Bluetooth, sniffers, spam, detectors, GATT, Bad-BT, and saved devices | `bluetooth-menu-audit` | `ble-signal-finder`, `ble-gatt-discovery`, `ble-airtag-monitor`, `ble-spam` |
| Settings, LCD, Storage, Power, STM32/ESP32 update screens, Bluetooth Direct | `settings-menu-audit` | — |

Run `python scripts/m1_hil_test.py --list` for the registered case IDs and
`python scripts/m1_hil_test.py --test <id>` to run one case. Do not invent test
IDs in a PR or review.

## New menu-feature checklist

1. Add the destination to the appropriate recursive audit. Cover safe nested
   action menus as well as the top-level entry.
2. Exercise entry, BACK cleanup, and re-entry where the scene owns hardware.
   Use bounded waits and static log labels. Never retain ambient filenames,
   network identifiers, device identifiers, or screen captures in output.
3. Do not execute user scripts/apps, transmit RF, write or wipe tags, delete or
   rename files, install firmware, format storage, or power off as an audit side
   effect. Enter configuration screens and cancel without saving where safe.
4. If full functionality needs a peer, AP, live card/tag, decoded signal, or
   other external fixture, keep the safe HIL entry check and document the
   functional limitation in the PR. Do not report the external-dependent path
   as a functional pass.
5. If no safe single-device entry check is possible, document why in the PR.
   Destructive confirmation dialogs may be checked only when cancellation is
   deterministic; otherwise list the operation as an explicit skip.
6. Add a focused registered case when responsiveness alone would not catch the
   changed behavior. Keep ordinary host tests for reporting and coverage rules.

## PR author and reviewer rule

PR descriptions must name the relevant unchecked case IDs between the
`M1_HIL_RESULTS` markers. Reviewers, including agents performing review-only
work, must check whether the change could have altered audit coverage, verify
that the audit and its count tests were updated accordingly, and suggest the
exact existing case IDs from the table above. A review comment should name the
changed menu and the command to run, for example:

> The RFID manual-editor path changed. Please run
> `python scripts/m1_hil_test.py --test rfid-menu-audit` and include its
> unchecked result in the PR HIL section.

Only an observed on-device `PASS` may check a result box. `FAIL`, `SKIP`, and
unrun cases remain unchecked. Remote PR edits require explicit permission;
otherwise provide the test suggestion or generated Markdown without editing the
PR.
