import subprocess
import sys

import pytest

from comverter import hello_world


def test_hello_world_prints():
    result = subprocess.run(
        [sys.executable, "-c", "from comverter import hello_world; hello_world()"],
        capture_output=True, text=True,
    )
    assert result.stdout == "Hello World!\n"
    assert result.returncode == 0


def test_hello_world_returns_none():
    result = hello_world()
    assert result is None


def test_module_all():
    import comverter
    assert comverter.__all__ == ["hello_world"]
