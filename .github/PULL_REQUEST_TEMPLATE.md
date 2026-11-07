<!-- See COPYING.txt for license details. -->

## Description
Please include a summary of the change and which issue is fixed. Please also include relevant motivation and context. List any dependencies that are required for this change.

## Type of Change
Please delete options that are not relevant.

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Code refactoring
- [ ] Test update

## Hardware Requirements
- M1 Device: [Specify version]
- NFC Module: [Specify version]
- Other peripherals: [List any]

## Testing Environment
- IDE: [e.g., STM32CubeIDE]
- Version: [e.g., 1.11.0]
- Compiler: [e.g., GCC]
- Debugger: [e.g., ST-Link]

## How Has This Been Tested?
Please describe the tests that you ran to verify your changes. Provide instructions so we can reproduce. Please also list any relevant details for your test configuration.

- [ ] Test A
- [ ] Test B

## Single-device HIL (recommended, never a merge blocker)
Agents must add an unchecked item for every new or changed hardware behavior
that can be tested deterministically on one M1. Document why any unsuitable
scenario was omitted. Do not check an item unless it actually passed on-device.

Local agents with a connected M1 and permission to edit this PR can run all
applicable tests and update this section automatically:

```bash
python scripts/m1_hil_test.py --update-pr <PR_NUMBER>
```

Cloud CI does not run HIL. Missing hardware produces an unchecked `SKIP` and
must not block this PR. Results are also written to `build/hil-results/`.

<!-- M1_HIL_RESULTS:START -->
- [ ] `wifi-airtime` — NOT RUN
- [ ] `wifi-peer-link` — NOT RUN
- [ ] `ble-signal-finder` — NOT RUN
- [ ] `ble-gatt-discovery` — NOT RUN
- [ ] `ble-airtag-monitor` — NOT RUN
<!-- M1_HIL_RESULTS:END -->

## Checklist:
- [ ] My code follows the style guidelines of this project
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have added tests that prove my fix is effective or that my feature works
- [ ] New and existing unit tests pass locally with my changes
- [ ] Any dependent changes have been merged and published in downstream modules

## Screenshots (if appropriate):

## Additional Notes:
Add any other information about the pull request here. 