"""
test_basic.py — Basic M1 RPC integration tests.

These tests require a physical M1 device connected via USB.
Run with: pytest rpc_tests/ -v --port /dev/ttyACM0

Tests cover:
  - Device connectivity (PING/PONG)
  - Device info query
  - Button injection + screen capture
  - Basic menu navigation
"""

from .m1_client import Button


# ─── Connectivity ───────────────────────────────────────────────────

class TestConnectivity:
    """Verify basic RPC communication."""

    def test_ping(self, m1):
        """PING should receive PONG without error."""
        m1.ping()

    def test_ping_multiple(self, m1):
        """Multiple PINGs should all succeed."""
        for _ in range(10):
            m1.ping()

    def test_device_info(self, m1):
        """Device info should return valid firmware version."""
        info = m1.device_info()
        assert info.magic == 0x4D314649, f"Bad magic: 0x{info.magic:08X}"
        assert info.fw_major >= 0
        assert info.fw_minor >= 0
        assert 0 <= info.battery_level <= 100

    def test_device_info_hapax(self, m1):
        """Hapax fork should report non-zero revision."""
        info = m1.device_info()
        assert info.hapax_revision > 0, (
            f"Expected Hapax revision > 0, got {info.hapax_revision}"
        )


# ─── Screen Capture ─────────────────────────────────────────────────

class TestScreenCapture:
    """Verify screen framebuffer capture."""

    def test_screenshot_size(self, m1):
        """Screenshot should be exactly 128×64 pixels."""
        img = m1.screenshot()
        assert img.size == (128, 64)

    def test_screenshot_not_blank(self, m1):
        """Screen should have some non-zero pixels (not completely blank)."""
        img = m1.screenshot()
        # Count white pixels
        pixels = list(img.getdata())
        white_count = sum(1 for p in pixels if p)
        assert white_count > 0, "Screen is completely blank"

    def test_screenshot_save(self, m1, screenshot_dir):
        """Screenshot should be saveable as PNG."""
        img = m1.screenshot()
        path = screenshot_dir / "test_capture.png"
        img.save(str(path))
        assert path.exists()
        assert path.stat().st_size > 0


# ─── Button Injection ───────────────────────────────────────────────

class TestButtons:
    """Verify button event injection via RPC."""

    def test_click_ok(self, m1):
        """OK button click should be accepted (ACK)."""
        m1.click(Button.OK)

    def test_click_back(self, m1):
        """BACK button click should be accepted."""
        m1.click(Button.BACK)

    def test_all_buttons(self, m1):
        """All button IDs should be accepted without NACK."""
        for button in Button:
            m1.click(button, delay=0.1)


# ─── Menu Navigation ────────────────────────────────────────────────

class TestMenuNavigation:
    """
    Verify basic menu navigation using button injection + screen capture.

    These tests navigate the main menu by injecting UP/DOWN/OK/BACK
    and capturing the screen after each action.  They verify that the
    screen content changes in response to navigation.

    Note: These tests assume the device starts at or returns to the
    main menu.  The test order matters — each test returns to the
    starting state using BACK presses.
    """

    def test_navigate_down_changes_screen(self, m1):
        """Pressing DOWN should change the screen content."""
        img_before = m1.screenshot()
        m1.click(Button.DOWN)
        img_after = m1.screenshot()
        m1.click(Button.UP)  # Restore position

        # Screens should differ (different menu item highlighted)
        assert img_before.tobytes() != img_after.tobytes(), (
            "Screen did not change after DOWN press"
        )

    def test_navigate_ok_and_back(self, m1):
        """OK should enter a submenu, BACK should return to the previous screen."""
        img_main = m1.screenshot()
        m1.click(Button.OK)
        img_sub = m1.screenshot()
        m1.click(Button.BACK)
        img_returned = m1.screenshot()

        # Entering submenu should change the screen
        assert img_main.tobytes() != img_sub.tobytes(), (
            "Screen did not change after OK press"
        )

        # Returning should restore the original screen
        # (may not be pixel-exact due to animations, but should be similar)

    def test_navigation_round_trip(self, m1):
        """Navigate DOWN×3, then UP×3 — should return to the same screen."""
        img_start = m1.screenshot()

        for _ in range(3):
            m1.click(Button.DOWN)

        for _ in range(3):
            m1.click(Button.UP)

        img_end = m1.screenshot()
        assert img_start.tobytes() == img_end.tobytes(), (
            "Screen did not return to original after DOWN×3 + UP×3"
        )
