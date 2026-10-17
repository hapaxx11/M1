<!-- See COPYING.txt for license details. -->

GitHub Repository Guidelines
1. Branch Management
Main/Master branch is protected and requires pull request reviews
Feature branches should follow naming convention: feature/descriptive-name
Bug fix branches should follow: fix/issue-description
Hotfix branches: hotfix/urgent-fix-description
Delete branches after merging

2. Commit Messages
Follow conventional commits format:
feat: for new features
fix: for bug fixes
docs: for documentation changes
style: for formatting changes
refactor: for code restructuring
test: for adding tests
chore: for maintenance tasks
Write clear, descriptive messages in present tense
Reference issue numbers when applicable: "feat: add user authentication (#123)"

3. Pull Requests
Create detailed PR descriptions using the provided template
Include:
What changes were made
Why changes were needed
How to test the changes
Screenshots/videos for UI changes
Require at least one approval before merging
Link related issues
Keep PRs focused and reasonably sized

4. Code Review
Review PRs within 24 business hours
Be constructive and respectful in comments
Use GitHub's suggestion feature for small changes
Check for:
Code quality and standards
Test coverage
Documentation
Performance implications
Security concerns

5. CI/CD
Hapax uses GitHub Actions for builds, tests, releases, and documentation deploys:
Required checks must pass; host tests run when their source/test path filters match
Maintain test coverage above agreed threshold (e.g., 80%)
Automated linting must pass
Firmware build must succeed for matching firmware-input changes (docs-only changes are excluded by the `paths` filters in `ci.yml`)
No security vulnerabilities in dependencies
Merges that change firmware inputs trigger a GitHub Release with firmware artifacts;
docs-only and other excluded changes do not create a release
The Web Updater (GitHub Pages) and device OTA both pull from GitHub Releases

6. Documentation
Keep README.md updated with accurate feature descriptions and protocol counts
For each meaningful change, create a fragment under `.changelog/` instead of editing
`CHANGELOG.md` directly. Use `<short-description>.<added|changed|fixed|removed>.md`;
CI assembles fragments for qualifying firmware-input PRs. See
`.changelog/README.md` for format and assembly details.
  - Never write the literal string `## [Unreleased]` in changelog body text — the CI stamper matches it by simple text replace and will corrupt the file (see CLAUDE.md changelog rules)
  - Automated "changelog: stamp [Unreleased] as ..." PRs are infrastructure housekeeping — do NOT mention them in changelog entries, release notes, or PR descriptions. They are excluded from auto-generated release notes via `.github/release.yml` and the `changelog-stamp` label.
Update `.github/skills/flipper-import/SKILL.md` inventory tables when Flipper-derived files are added or removed
Document setup instructions
Maintain API documentation
Comment complex code sections
Include JSDoc/documentation comments for public APIs

7. Issue Management
Use issue templates for:
Bug reports
Feature requests
Documentation updates
Label issues appropriately
Use project boards for tracking
Regular backlog grooming

8. Security
No secrets/credentials in repository
Use environment variables for sensitive data
Regular dependency updates
Security scanning enabled
Follow security best practices

9. Version Control
Follow semantic versioning (MAJOR.MINOR.PATCH)
Maintain a changelog
Tag releases appropriately
Create release notes for significant versions

10. Code Standards
Follow agreed coding style guide
Use consistent formatting (preferably automated)
Write self-documenting code
Follow DRY (Don't Repeat Yourself) principles
Write unit tests for new features
Bug fixes MUST include regression tests — add one or more host-side unit tests (under
tests/) that fail before the fix and pass after it. A bug fix without a corresponding
regression test is incomplete.
All unit tests must follow the stub-based extraction pattern: identify pure-logic
functions, create minimal stubs in tests/stubs/ for HAL/RTOS/FatFS headers, and test
with Unity.  Do NOT attempt to unit test AT command construction, GPIO manipulation,
or RTOS task orchestration — those need hardware integration testing.
See CLAUDE.md § "Preferred Unit Testing Pattern" for the full specification.
When a source file mixes pure logic with hardware-coupled code, extract the pure
logic into a standalone .c/.h module with a clean interface (use callbacks for
hardware decoupling).  This improves both testability and maintainability.
See CLAUDE.md § "Preferred Modularization Pattern" for the full specification.

11. UX Pattern Standards
Any module that loads saved files from SD card MUST implement the Saved Item Actions
pattern (Emulate/Send, Info, Rename, Delete as core verbs).  This is the canonical
UX standard for the project, modelled on Flipper Zero's *_scene_saved_menu.c
architecture.  It supersedes any previously defined UX preferences when they conflict.
See CLAUDE.md § "Saved Item Actions Pattern" for the full specification.
Other Monstatek-derived UX rules (button bar guidelines, display layout, keypad mapping)
still apply when not superseded by this pattern.

12. UI / Button Bar Rules
Button bar columns MUST correspond to physical buttons: LEFT column = LEFT button,
CENTER column = OK button, RIGHT column = RIGHT button.  Never place a DOWN action in
the CENTER column or an OK action in the RIGHT column.
Never add "Back" as a menu item, selectable action, or button bar label — the hardware
BACK button handles navigation.  Selection lists MUST NOT have a button bar with "OK" —
use a scrollbar as a position indicator instead.  Button bars are appropriate only when
they convey non-obvious functionality (e.g. "OK:Download", "Send", "↓ Config").
See DEVELOPMENT.md § "Button Model" and "UI / Button Bar Rules" for the full specification.

13. Font-Aware Menu Implementation
All scrollable lists MUST use the font-aware helpers from m1_scene.h instead of
hardcoded visible item counts, row heights, or font constants.  The user-configurable
text size setting (m1_menu_style) drives m1_menu_item_h(), m1_menu_max_visible(), and
m1_menu_font().  A small number of justified exceptions exist for compact data displays
that are not standard selectable menus.
See DEVELOPMENT.md § "Font-Aware Menu Implementation" for helpers, layout constants,
rules, and the exception table.

14. Hardware State Management
Every module that uses the SI4463 radio, ESP32 coprocessor, NFC worker, or IR
transmitter MUST follow the corresponding init/deinit lifecycle pattern.  Failing to
deinit on all exit paths (including errors) leaves hardware resources active and
causes bugs in subsequent operations.
See DEVELOPMENT.md § "Hardware State Management" for per-subsystem rules.

15. Repository Hygiene
Regular dependency updates
Periodic cleanup of old branches
Archive unused repositories
Maintain reasonable repository size
Regular backup procedures

16. Communication
Use GitHub Discussions for technical discussions
Keep relevant conversations in PR comments
Use issue comments for status updates
Tag relevant team members when needed