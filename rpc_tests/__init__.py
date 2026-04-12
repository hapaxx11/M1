# M1 RPC Test Harness — Python client and pytest integration tests
#
# This package provides:
#   - m1_client.py  — M1Client class for RPC communication over USB CDC
#   - conftest.py   — pytest fixtures (auto-connect, screenshot dir)
#   - test_basic.py — example integration tests
#
# Requirements: pyserial, pytest, Pillow
# Install:      pip install -r requirements.txt
# Run:          pytest rpc_tests/ -v --port /dev/ttyACM0
