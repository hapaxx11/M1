"""
conftest.py — pytest fixtures for M1 RPC hardware-in-the-loop tests.

Provides:
  - --port CLI option for specifying the M1 USB CDC serial port
  - m1 fixture: auto-connected M1Client instance
  - screenshot_dir fixture: directory for saving captured screen frames

Usage:
    pytest rpc_tests/ -v --port /dev/ttyACM0
    pytest rpc_tests/ -v --port COM3          # Windows
"""

import os
import pytest

from .m1_client import M1Client


def pytest_addoption(parser):
    """Add --port option for M1 serial port."""
    parser.addoption(
        "--port",
        action="store",
        default=None,
        help="M1 USB CDC serial port (e.g., /dev/ttyACM0, COM3)",
    )
    parser.addoption(
        "--baudrate",
        action="store",
        default=115200,
        type=int,
        help="Serial baud rate (default: 115200)",
    )


@pytest.fixture(scope="session")
def m1_port(request):
    """Get the serial port from CLI options or environment."""
    port = request.config.getoption("--port") or os.environ.get("M1_PORT")
    if port is None:
        pytest.skip("No M1 device port specified (use --port or M1_PORT env var)")
    return port


@pytest.fixture(scope="session")
def m1_baudrate(request):
    """Get the baud rate from CLI options."""
    return request.config.getoption("--baudrate")


@pytest.fixture(scope="session")
def m1(m1_port, m1_baudrate):
    """
    Session-scoped M1Client fixture.

    Connects to the M1 device once for the entire test session.
    Automatically disconnects on teardown.
    """
    client = M1Client(m1_port, baudrate=m1_baudrate)
    try:
        client.connect()
    except Exception as e:
        pytest.skip(f"Cannot connect to M1 at {m1_port}: {e}")
    yield client
    client.disconnect()


@pytest.fixture
def screenshot_dir(tmp_path):
    """Temporary directory for saving screenshots during tests."""
    d = tmp_path / "screenshots"
    d.mkdir()
    return d
